#include "PluginEditor.h"

using namespace geminis;

//==============================================================================
GeminisLookAndFeel::GeminisLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::background);
    setColour (juce::Slider::textBoxTextColourId, colours::textDim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::ComboBox::backgroundColourId, colours::background);
    setColour (juce::ComboBox::outlineColourId, colours::panelEdge);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::arrowColourId, colours::textDim);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::guitar.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, colours::text);
    setColour (juce::TextButton::buttonColourId, colours::background);
    setColour (juce::TextButton::textColourOffId, colours::text);
}

void GeminisLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
    const float alpha = slider.isEnabled() ? 1.0f : 0.35f;

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - 2.0f, radius - 2.0f, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::panelEdge.withAlpha (alpha));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius - 2.0f, radius - 2.0f, 0.0f, startAngle, angle, true);
    g.setColour (accent.withAlpha (alpha));
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float capRadius = radius - 8.0f;
    g.setColour (juce::Colour (0xff2a2f3b).withAlpha (alpha));
    g.fillEllipse (centre.x - capRadius, centre.y - capRadius, capRadius * 2.0f, capRadius * 2.0f);
    g.setColour (colours::panelEdge.brighter (0.2f).withAlpha (alpha));
    g.drawEllipse (centre.x - capRadius, centre.y - capRadius, capRadius * 2.0f, capRadius * 2.0f, 1.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -capRadius + 3.0f, 3.0f, capRadius * 0.55f, 1.5f);
    g.setColour (colours::text.withAlpha (alpha));
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

void GeminisLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool)
{
    const auto area = button.getLocalBounds().toFloat().withSizeKeepingCentre (30.0f, 16.0f);
    const bool on = button.getToggleState();
    const auto accent = button.findColour (juce::ToggleButton::tickColourId);

    g.setColour (on ? accent.withAlpha (highlighted ? 1.0f : 0.9f) : colours::panelEdge);
    g.fillRoundedRectangle (area, 8.0f);
    g.setColour (on ? colours::background : colours::textDim);
    const float knobX = on ? area.getRight() - 14.0f : area.getX() + 2.0f;
    g.fillEllipse (knobX, area.getY() + 2.0f, 12.0f, 12.0f);
}

//==============================================================================
ModulePanel::ModulePanel (APVTS& state, const char* titleText, const char* bypassId,
                          std::vector<Knob> knobs, const char* comboId)
    : title (U (titleText))
{
    if (bypassId != nullptr)
    {
        hasPower = true;
        power.setColour (juce::ToggleButton::tickColourId, colours::guitar);
        addAndMakeVisible (power);
        powerAttachment = std::make_unique<APVTS::ButtonAttachment> (state, bypassId, power);
        power.onStateChange = [this] { updateEnabled(); };
    }

    if (comboId != nullptr)
    {
        hasCombo = true;
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (comboId)))
            combo.addItemList (choice->choices, 1);
        addAndMakeVisible (combo);
        comboAttachment = std::make_unique<APVTS::ComboBoxAttachment> (state, comboId, combo);
    }

    for (const auto& k : knobs)
    {
        auto* slider = sliders.add (new juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow));
        slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        slider->setColour (juce::Slider::rotarySliderFillColourId, colours::guitar);
        slider->setColour (juce::Slider::textBoxTextColourId, colours::textDim);
        slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (slider);
        attachments.add (new APVTS::SliderAttachment (state, k.id, *slider));

        auto* label = labels.add (new juce::Label ({}, U (k.label)));
        label->setJustificationType (juce::Justification::centred);
        label->setFont (juce::FontOptions (12.0f));
        label->setColour (juce::Label::textColourId, colours::textDim);
        addAndMakeVisible (label);
    }

    updateEnabled();
}

void ModulePanel::addFooterButton (const juce::String& text, std::function<void()> onClick)
{
    footer = std::make_unique<juce::TextButton> (text);
    footer->onClick = std::move (onClick);
    addAndMakeVisible (*footer);
    resized();
}

void ModulePanel::setFooterText (const juce::String& text)
{
    if (footer != nullptr)
        footer->setButtonText (text);
}

void ModulePanel::updateEnabled()
{
    const bool on = ! hasPower || power.getToggleState();
    for (auto* s : sliders) s->setEnabled (on);
    for (auto* l : labels)  l->setAlpha (on ? 1.0f : 0.5f);
    combo.setEnabled (on);
    repaint();
}

void ModulePanel::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat().reduced (1.0f);
    const bool on = ! hasPower || power.getToggleState();

    g.setColour (colours::panel);
    g.fillRoundedRectangle (area, 10.0f);
    g.setColour (on && hasPower ? colours::guitar.withAlpha (0.45f) : colours::panelEdge);
    g.drawRoundedRectangle (area, 10.0f, 1.0f);

    g.setColour (on ? colours::text : colours::textDim);
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText (title, getLocalBounds().removeFromTop (36).reduced (14, 0).withTrimmedRight (hasPower ? 36 : 0),
                juce::Justification::centredLeft);
}

void ModulePanel::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto header = area.removeFromTop (26);
    if (hasPower)
        power.setBounds (header.removeFromRight (36));

    area.removeFromTop (4);
    if (hasCombo)
    {
        combo.setBounds (area.removeFromTop (26).reduced (2, 0));
        area.removeFromTop (4);
    }
    if (footer != nullptr)
        footer->setBounds (area.removeFromBottom (24).reduced (2, 0));

    const int count = sliders.size();
    if (count == 0)
        return;

    const int perRow = count > 4 ? (count + 1) / 2 : count;
    const int rows = (count + perRow - 1) / perRow;
    const int cellW = area.getWidth() / perRow;
    const int cellH = area.getHeight() / rows;

    for (int i = 0; i < count; ++i)
    {
        juce::Rectangle<int> cell (area.getX() + (i % perRow) * cellW, area.getY() + (i / perRow) * cellH, cellW, cellH);
        cell = cell.withSizeKeepingCentre (juce::jmin (cellW, 96), juce::jmin (cellH - 6, 104));
        labels[i]->setBounds (cell.removeFromTop (16));
        sliders[i]->setBounds (cell);
    }
}

//==============================================================================
void LevelMeter::push (float peak)
{
    const float target = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (peak, -60.0f) + 60.0f) / 60.0f);
    const float next = target > level ? target : level * 0.85f + target * 0.15f;
    if (std::abs (next - level) > 0.001f)
    {
        level = next;
        repaint();
    }
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();
    g.setColour (colours::textDim);
    g.setFont (juce::FontOptions (10.0f));
    g.drawText (label, area.removeFromLeft (26), juce::Justification::centredLeft);

    auto bar = area.toFloat().withSizeKeepingCentre ((float) area.getWidth(), 6.0f);
    g.setColour (colours::panelEdge);
    g.fillRoundedRectangle (bar, 3.0f);
    g.setColour (level > 0.97f ? juce::Colour (0xffef5350) : colours::guitar);
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * level), 3.0f);
}

//==============================================================================
namespace
{
struct Card { const char* title; const char* line1; const char* line2; const char* status; juce::Colour colour; Mode mode; bool available; };

const Card cards[] = {
    { "Guitarra", "Amplis, pantallas y pedales.", "En directo o sobre pistas grabadas.", "DISPONIBLE", colours::guitar, Mode::guitar, true },
    { "Voz", "Cadena vocal completa.", "En directo o sobre pistas grabadas.", "PRÓXIMAMENTE", colours::voice, Mode::voice, false },
    { "Instrumentos MIDI", "Baterías, bajo, cuerdas y sintes.", "Mapas Maschine y General MIDI.", "PRÓXIMAMENTE", colours::instrument, Mode::instrument, false },
};
}

juce::Rectangle<int> ModeChooser::cardBounds (int index) const
{
    const int gap = 24, w = 280, h = 250;
    const int total = 3 * w + 2 * gap;
    return { (getWidth() - total) / 2 + index * (w + gap), getHeight() / 2 - 70, w, h };
}

void ModeChooser::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    g.setColour (colours::text);
    g.setFont (juce::FontOptions (40.0f, juce::Font::bold));
    g.drawText ("GEMINIS", getLocalBounds().removeFromTop (150).withTrimmedTop (60), juce::Justification::centred);
    g.setColour (colours::textDim);
    g.setFont (juce::FontOptions (18.0f));
    g.drawText (U ("¿Qué vas a hacer hoy?"), getLocalBounds().removeFromTop (200).withTrimmedTop (150), juce::Justification::centred);

    for (int i = 0; i < 3; ++i)
    {
        const auto& card = cards[i];
        const auto area = cardBounds (i).toFloat();
        const bool hot = hovered == i && card.available;

        g.setColour (hot ? colours::panel.brighter (0.08f) : colours::panel);
        g.fillRoundedRectangle (area, 14.0f);
        g.setColour (hot ? card.colour : colours::panelEdge);
        g.drawRoundedRectangle (area, 14.0f, hot ? 2.0f : 1.0f);

        auto inner = cardBounds (i).reduced (24);
        g.setColour (card.colour.withAlpha (card.available ? 1.0f : 0.5f));
        g.fillRoundedRectangle (inner.removeFromTop (6).removeFromLeft (48).toFloat(), 3.0f);
        inner.removeFromTop (22);

        g.setColour (card.available ? colours::text : colours::textDim);
        g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
        g.drawText (U (card.title), inner.removeFromTop (34), juce::Justification::centredLeft);
        inner.removeFromTop (12);

        g.setColour (colours::textDim);
        g.setFont (juce::FontOptions (14.0f));
        g.drawText (U (card.line1), inner.removeFromTop (22), juce::Justification::centredLeft);
        g.drawText (U (card.line2), inner.removeFromTop (22), juce::Justification::centredLeft);

        g.setColour (card.available ? card.colour : colours::textDim);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (U (card.status), inner.removeFromBottom (20), juce::Justification::centredLeft);
    }
}

void ModeChooser::mouseMove (const juce::MouseEvent& e)
{
    int now = -1;
    for (int i = 0; i < 3; ++i)
        if (cardBounds (i).contains (e.getPosition()))
            now = i;

    if (now != hovered)
    {
        hovered = now;
        setMouseCursor (now >= 0 && cards[now].available ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void ModeChooser::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}

void ModeChooser::mouseUp (const juce::MouseEvent& e)
{
    for (int i = 0; i < 3; ++i)
        if (cardBounds (i).contains (e.getPosition()) && cards[i].available && onChoose != nullptr)
            onChoose (cards[i].mode);
}

//==============================================================================
GeminisEditor::GeminisEditor (GeminisProcessor& owner)
    : AudioProcessorEditor (owner), processor (owner)
{
    setLookAndFeel (&lookAndFeel);
    auto& state = processor.apvts;

    input  = std::make_unique<ModulePanel> (state, "PUERTA DE RUIDO", "gateOn",
                 std::vector<ModulePanel::Knob> { { "inGain", "Entrada" }, { "gateThresh", "Umbral" } });
    comp   = std::make_unique<ModulePanel> (state, "COMPRESOR", "compOn",
                 std::vector<ModulePanel::Knob> { { "compThresh", "Umbral" }, { "compRatio", "Ratio" }, { "compMakeup", "Nivel" } });
    drive  = std::make_unique<ModulePanel> (state, "IMPULSO", "drvOn",
                 std::vector<ModulePanel::Knob> { { "drvGain", "Ganancia" }, { "drvTone", "Tono" }, { "drvLevel", "Nivel" } });
    amp    = std::make_unique<ModulePanel> (state, "AMPLIFICADOR", "ampOn",
                 std::vector<ModulePanel::Knob> { { "ampGain", "Ganancia" }, { "ampBass", "Graves" }, { "ampMid", "Medios" },
                                                  { "ampTreble", "Agudos" }, { "ampPresence", "Presencia" }, { "ampMaster", "Master" } },
                 "ampModel");
    cab    = std::make_unique<ModulePanel> (state, "PANTALLA", "cabOn",
                 std::vector<ModulePanel::Knob> { { "cabMic", "Micro" } }, "cabType");
    trem   = std::make_unique<ModulePanel> (state, "TRÉMOLO", "tremOn",
                 std::vector<ModulePanel::Knob> { { "tremRate", "Velocidad" }, { "tremDepth", "Profundidad" } });
    delay  = std::make_unique<ModulePanel> (state, "DELAY", "dlyOn",
                 std::vector<ModulePanel::Knob> { { "dlyTime", "Tiempo" }, { "dlyFeedback", "Repeticiones" }, { "dlyMix", "Mezcla" } });
    reverb = std::make_unique<ModulePanel> (state, "REVERB", "revOn",
                 std::vector<ModulePanel::Knob> { { "revSize", "Tamaño" }, { "revDamp", "Tono" }, { "revMix", "Mezcla" } });
    output = std::make_unique<ModulePanel> (state, "SALIDA", nullptr,
                 std::vector<ModulePanel::Knob> { { "outGain", "Volumen" } });

    for (auto* panel : { input.get(), comp.get(), drive.get(), amp.get(), cab.get(), trem.get(), delay.get(), reverb.get(), output.get() })
        addAndMakeVisible (panel);

    cab->addFooterButton (processor.getUserIrName().isNotEmpty() ? processor.getUserIrName() : U ("Cargar IR..."),
                          [this] { chooseIr(); });

    modeButton.setButtonText ("Cambiar modo");
    modeButton.onClick = [this] { showChooser (true); };
    addAndMakeVisible (modeButton);

    int itemId = 1;
    for (const auto& preset : getFactoryPresets())
        presetBox.addItem (U (preset.name), itemId++);
    presetBox.setSelectedId (processor.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { processor.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    inputBox.addItemList ({ "Entrada: izquierda", "Entrada: derecha", "Entrada: ambas" }, 1);
    addAndMakeVisible (inputBox);
    inputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, "inputSel", inputBox);

    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);

    chooser.onChoose = [this] (Mode m)
    {
        processor.setMode (m);
        showChooser (false);
    };
    addChildComponent (chooser);

    setSize (1100, 680);
    showChooser (processor.getMode() == Mode::none);
    startTimerHz (30);
}

GeminisEditor::~GeminisEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void GeminisEditor::showChooser (bool shouldShow)
{
    chooser.setVisible (shouldShow);
    if (shouldShow)
        chooser.toFront (false);
}

void GeminisEditor::chooseIr()
{
    fileChooser = std::make_unique<juce::FileChooser> (U ("Elige una respuesta a impulso"), juce::File(), "*.wav;*.aif;*.aiff");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto file = fc.getResult();
                                  if (! file.existsAsFile())
                                      return;

                                  processor.loadUserIr (file);
                                  cab->setFooterText (processor.getUserIrName());
                                  if (auto* type = processor.apvts.getParameter ("cabType"))
                                      type->setValueNotifyingHost (1.0f);
                              });
}

void GeminisEditor::timerCallback()
{
    inMeter.push (processor.inputPeak.exchange (0.0f));
    outMeter.push (processor.outputPeak.exchange (0.0f));
}

void GeminisEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    auto bar = getLocalBounds().removeFromTop (60);
    g.setColour (colours::panel);
    g.fillRect (bar);
    g.setColour (colours::panelEdge);
    g.fillRect (bar.removeFromBottom (1));

    auto logo = bar.reduced (20, 0);
    g.setColour (colours::text);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("GEMINIS", logo.removeFromLeft (112), juce::Justification::centredLeft);
    g.setColour (colours::guitar);
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText ("GUITARRA", logo.removeFromLeft (90), juce::Justification::centredLeft);
}

void GeminisEditor::resized()
{
    chooser.setBounds (getLocalBounds());

    auto area = getLocalBounds();
    auto bar = area.removeFromTop (60).reduced (20, 15);
    bar.removeFromLeft (210);
    modeButton.setBounds (bar.removeFromLeft (120));
    bar.removeFromLeft (12);
    presetBox.setBounds (bar.removeFromLeft (200));
    bar.removeFromLeft (12);
    inputBox.setBounds (bar.removeFromLeft (170));
    bar.removeFromLeft (16);
    inMeter.setBounds (bar.removeFromLeft (bar.getWidth() / 2).reduced (4, 0));
    outMeter.setBounds (bar.reduced (4, 0));

    area.reduce (16, 16);
    const int gap = 12;
    auto top = area.removeFromTop ((area.getHeight() - gap) / 2);
    area.removeFromTop (gap);
    auto bottom = area;

    auto place = [gap] (juce::Rectangle<int>& row, juce::Component& c, int width, bool last = false)
    {
        c.setBounds (last ? row : row.removeFromLeft (width));
        if (! last) row.removeFromLeft (gap);
    };

    place (top, *input, 190);
    place (top, *comp, 250);
    place (top, *drive, 250);
    place (top, *cab, 0, true);

    const int fxWidth = (bottom.getWidth() - 320 - 160 - 100 - 4 * gap) / 2;
    place (bottom, *amp, 320);
    place (bottom, *trem, 160);
    place (bottom, *delay, fxWidth);
    place (bottom, *reverb, fxWidth);
    place (bottom, *output, 0, true);
}
