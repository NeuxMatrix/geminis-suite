#include "FxEditor.h"
#include "../common/AppHooks.h"

using namespace geminis;
using namespace geminis::fx;

namespace geminis::fx
{
//==============================================================================
SlotPanel::SlotPanel (GeminisFxProcessor& p, int s, int pos, juce::Colour)
    : processor (p), slot (s), position (pos)
{
    type = processor.rack.getType (slot);
    const auto& inf = info (type);
    colour = inf.getColour();
    auto& state = processor.apvts;

    power.setColour (juce::ToggleButton::tickColourId, colour);
    power.setTooltip (U ("Encender o apagar el módulo"));
    addAndMakeVisible (power);
    powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, Rack::onId (slot), power);
    power.onStateChange = [this] { repaint(); };

    for (auto* b : { &up, &down, &remove })
    {
        b->setColour (juce::TextButton::buttonOnColourId, colour);
        addAndMakeVisible (b);
    }
    up.setTooltip (U ("Subir en la cadena"));
    down.setTooltip (U ("Bajar en la cadena"));
    remove.setTooltip (U ("Quitar el módulo"));
    up.onClick = [this] { if (onMoveUp) onMoveUp (slot); };
    down.onClick = [this] { if (onMoveDown) onMoveDown (slot); };
    remove.onClick = [this] { if (onRemove) onRemove (slot); };

    for (size_t i = 0; i < inf.params.size() && i < (size_t) Rack::paramsPerSlot; ++i)
    {
        const auto& spec = inf.params[i];
        auto* param = state.getParameter (Rack::paramId (slot, (int) i));
        if (spec.isChoice())
        {
            auto* box = new juce::ComboBox();
            for (int c = 0; c < (int) spec.choices.size(); ++c)
                box->addItem (juce::String::fromUTF8 (spec.choices[(size_t) c]), c + 1);
            controls.add (box);
            addAndMakeVisible (box);
            choiceAttachments.add (new NormalisedChoiceAttachment (*param, *box, (int) spec.choices.size()));
            isChoice.push_back (true);
        }
        else
        {
            auto* knob = new juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
            knob->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 15);
            knob->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
            knob->setColour (juce::Slider::rotarySliderFillColourId, colour);
            knob->getProperties().set ("bipolar", spec.bipolar);
            controls.add (knob);
            addAndMakeVisible (knob);
            sliderAttachments.add (new juce::AudioProcessorValueTreeState::SliderAttachment (state, Rack::paramId (slot, (int) i), *knob));
            knob->setDoubleClickReturnValue (true, spec.toNorm (spec.def));
            isChoice.push_back (false);
        }
        auto* l = new juce::Label ({}, juce::String::fromUTF8 (spec.name));
        l->setFont (Fonts::ui (11.5f, 600));
        l->setJustificationType (juce::Justification::centred);
        l->setColour (juce::Label::textColourId, colours::textDim);
        l->setInterceptsMouseClicks (false, false);
        labels.add (l);
        addAndMakeVisible (l);
    }

    if (type == cab)
    {
        const auto ir = processor.rack.getUserIr();
        irButton.setButtonText (ir.existsAsFile() ? ir.getFileNameWithoutExtension() : U ("Cargar IR..."));
        irButton.setTooltip (U ("Carga un archivo WAV de respuesta a impulso y elige \"IR propia\""));
        irButton.onClick = [this]
        {
            chooser = std::make_unique<juce::FileChooser> (U ("Elige una respuesta a impulso"), juce::File(), "*.wav;*.aif;*.aiff");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this] (const juce::FileChooser& fc)
                                  {
                                      const auto f = fc.getResult();
                                      if (! f.existsAsFile()) return;
                                      processor.loadUserIr (f);
                                      irButton.setButtonText (f.getFileNameWithoutExtension());
                                      if (auto* tp = processor.apvts.getParameter (Rack::paramId (slot, 0)))
                                          tp->setValueNotifyingHost (1.0f);
                                  });
        };
        addAndMakeVisible (irButton);
    }
}

void SlotPanel::paint (juce::Graphics& g)
{
    const auto& inf = info (type);
    const bool on = power.getToggleState();
    auto r = getLocalBounds().toFloat().reduced (1.0f);

    draw::glassPanel (g, r, colour, on ? 0.75f : 0.2f, 12.0f);

    // Franja de color con el número de posición
    auto stripe = r.removeFromLeft (34.0f);
    juce::Path sp;
    sp.addRoundedRectangle (stripe.getX(), stripe.getY(), stripe.getWidth(), stripe.getHeight(), 12.0f, 12.0f, true, false, true, false);
    juce::ColourGradient sg (colour.withAlpha (on ? 0.5f : 0.15f), stripe.getX(), stripe.getY(),
                             colour.withAlpha (on ? 0.18f : 0.05f), stripe.getX(), stripe.getBottom(), false);
    g.setGradientFill (sg);
    g.fillPath (sp);
    g.setColour (on ? colours::bg : colours::textDim);
    g.setFont (Fonts::ui (15.0f, 700));
    g.drawText (juce::String (position + 1), stripe.removeFromTop (40.0f), juce::Justification::centred);

    auto header = getLocalBounds().withTrimmedLeft (94).removeFromTop (34).toFloat().translated (0.0f, 4.0f);
    g.setColour (on ? colours::text : colours::textDim);
    g.setFont (Fonts::ui (15.0f, 700));
    const auto name = juce::String::fromUTF8 (inf.name);
    g.drawText (name, header, juce::Justification::centredLeft);
    const float nameW = Fonts::ui (15.0f, 700).getStringWidthFloat (name);
    g.setColour (colour.withAlpha (on ? 0.9f : 0.4f));
    g.setFont (Fonts::ui (10.5f, 700));
    g.drawText (upper (juce::String::fromUTF8 (inf.category)), header.withTrimmedLeft (nameW + 12.0f).withWidth (120.0f), juce::Justification::centredLeft);
    g.setColour (colours::textFaint);
    g.setFont (Fonts::ui (12.0f));
    g.drawText (juce::String::fromUTF8 (inf.description), header.withTrimmedLeft (nameW + 120.0f).withTrimmedRight (110.0f), juce::Justification::centredLeft);
}

void SlotPanel::resized()
{
    auto r = getLocalBounds().withTrimmedLeft (34).reduced (10, 6);
    auto header = r.removeFromTop (28);
    power.setBounds (header.removeFromLeft (36).withSizeKeepingCentre (36, 24));
    remove.setBounds (header.removeFromRight (28).withSizeKeepingCentre (26, 24));
    header.removeFromRight (4);
    down.setBounds (header.removeFromRight (28).withSizeKeepingCentre (26, 24));
    header.removeFromRight (4);
    up.setBounds (header.removeFromRight (28).withSizeKeepingCentre (26, 24));

    r.removeFromTop (4);
    if (irButton.isVisible())
        irButton.setBounds (r.removeFromRight (140).withSizeKeepingCentre (130, 28));

    int x = r.getX() + 4;
    for (int i = 0; i < controls.size(); ++i)
    {
        const int w = isChoice[(size_t) i] ? 156 : 78;
        auto cell = juce::Rectangle<int> (x, r.getY(), w, r.getHeight());
        labels[i]->setBounds (cell.removeFromTop (16));
        if (isChoice[(size_t) i])
            controls[i]->setBounds (cell.withSizeKeepingCentre (w - 12, 28).translated (0, -6));
        else
            controls[i]->setBounds (cell.reduced (2, 0));
        x += w + 4;
    }
}

//==============================================================================
ModuleBrowser::ModuleBrowser (Mode mode)
{
    const int mask = mode == Mode::voice ? forVoice : forGuitar;
    for (const auto& cat : categories())
    {
        bool headerAdded = false;
        for (int t = 1; t < numTypes; ++t)
        {
            const auto& inf = info (t);
            if ((inf.modes & mask) == 0 || juce::String::fromUTF8 (inf.category) != cat) continue;
            if (! headerAdded) { rows.push_back ({ true, cat, 0, inf.getColour() }); headerAdded = true; }
            rows.push_back ({ false, juce::String::fromUTF8 (inf.name), t, inf.getColour() });
        }
    }
    setRepaintsOnMouseActivity (true);
}

int ModuleBrowser::getIdealHeight() const
{
    int h = 8;
    for (const auto& r : rows) h += r.header ? headerH : rowH;
    return h;
}

int ModuleBrowser::rowAt (int y) const
{
    int pos = 8;
    for (int i = 0; i < (int) rows.size(); ++i)
    {
        const int h = rows[(size_t) i].header ? headerH : rowH;
        if (y >= pos && y < pos + h) return rows[(size_t) i].header ? -1 : i;
        pos += h;
    }
    return -1;
}

void ModuleBrowser::paint (juce::Graphics& g)
{
    int y = 8;
    for (int i = 0; i < (int) rows.size(); ++i)
    {
        const auto& row = rows[(size_t) i];
        if (row.header)
        {
            g.setColour (row.colour.withAlpha (0.85f));
            draw::spacedText (g, upper (row.text), Fonts::ui (10.5f, 700), juce::Rectangle<int> (14, y + 10, getWidth() - 20, 16).toFloat(),
                              juce::Justification::centredLeft, 0.14f);
            y += headerH;
            continue;
        }
        auto r = juce::Rectangle<int> (6, y, getWidth() - 12, rowH - 2).toFloat();
        if (i == hovered)
        {
            g.setColour (row.colour.withAlpha (0.14f));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (row.colour);
            g.setFont (Fonts::ui (16.0f, 700));
            g.drawText ("+", r.withTrimmedRight (8.0f), juce::Justification::centredRight);
        }
        g.setColour (row.colour);
        g.fillEllipse (r.getX() + 9.0f, r.getCentreY() - 3.0f, 6.0f, 6.0f);
        g.setColour (i == hovered ? colours::text : colours::text.withAlpha (0.85f));
        g.setFont (Fonts::ui (13.5f));
        g.drawText (row.text, r.withTrimmedLeft (24.0f), juce::Justification::centredLeft);
        y += rowH;
    }
}

void ModuleBrowser::mouseMove (const juce::MouseEvent& e)
{
    const int now = rowAt (e.y);
    if (now != hovered)
    {
        hovered = now;
        setMouseCursor (now >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void ModuleBrowser::mouseExit (const juce::MouseEvent&) { hovered = -1; repaint(); }

void ModuleBrowser::mouseUp (const juce::MouseEvent& e)
{
    const int r = rowAt (e.y);
    if (r >= 0 && onAdd) onAdd (rows[(size_t) r].type);
}

//==============================================================================
namespace
{
    struct GuitarString { const char* name; int midi; };
    const GuitarString guitarStrings[] = { { "6 · Mi", 40 }, { "5 · La", 45 }, { "4 · Re", 50 },
                                           { "3 · Sol", 55 }, { "2 · Si", 59 }, { "1 · Mi", 64 } };
    constexpr int tunerHoldFrames = 18;      // 1,2 s a 15 lecturas por segundo
    constexpr float inTuneCents = 3.0f;
}

TunerPanel::TunerPanel (GeminisFxProcessor& p) : processor (p)
{
    muteToggle.setButtonText (U ("Silenciar la salida mientras afinas"));
    muteToggle.setToggleState (processor.tuner.muteOutput.load(), juce::dontSendNotification);
    muteToggle.setColour (juce::ToggleButton::tickColourId, colours::green);
    muteToggle.onClick = [this] { processor.tuner.muteOutput = muteToggle.getToggleState(); };
    addAndMakeVisible (muteToggle);

    closeButton.setButtonText (U ("Cerrar afinador"));
    closeButton.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (closeButton);
}

void TunerPanel::reset()
{
    hasReading = false; holdFrames = 0; silentFrames = 0; level = 0.0f; cents = 0.0f;
    repaint();
}

void TunerPanel::update (const Tuner::Reading& r, float inputPeak)
{
    level = juce::jmax (level * 0.8f, inputPeak);
    if (r.valid)
    {
        if (! hasReading || r.midiNote != shown.midiNote) cents = r.cents;
        else cents += 0.35f * (r.cents - cents);
        shown = r;
        hasReading = true;
        holdFrames = tunerHoldFrames;
    }
    else if (holdFrames > 0) --holdFrames;
    else hasReading = false;

    silentFrames = level < 0.002f ? silentFrames + 1 : 0;
    repaint();
}

void TunerPanel::resized()
{
    auto r = getLocalBounds().reduced (22, 18);
    auto top = r.removeFromTop (34);
    closeButton.setBounds (top.removeFromRight (150));
    top.removeFromRight (14);
    muteToggle.setBounds (top.removeFromRight (300));
}

void TunerPanel::paint (juce::Graphics& g)
{
    const auto acc = guitar ? colours::guitar : colours::voice;
    auto area = getLocalBounds().toFloat();
    g.setColour (colours::bg.withAlpha (0.94f));
    g.fillRoundedRectangle (area, 14.0f);
    draw::glassPanel (g, area.reduced (1.0f), acc, 0.35f, 14.0f);

    auto r = area.reduced (22.0f, 18.0f);
    auto top = r.removeFromTop (34.0f);
    g.setColour (acc);
    draw::spacedText (g, U ("AFINADOR"), Fonts::ui (13.0f, 700), top.removeFromLeft (160.0f), juce::Justification::centredLeft, 0.3f);
    g.setColour (colours::textFaint);
    g.setFont (Fonts::ui (12.0f));
    g.drawText (U ("La = 440 Hz"), top.removeFromLeft (120.0f), juce::Justification::centredLeft);

    const bool live = hasReading && holdFrames == tunerHoldFrames;
    const float absCents = std::abs (cents);
    const auto stateColour = ! hasReading ? colours::textFaint
                           : absCents < inTuneCents ? colours::green
                           : absCents < 15.0f ? colours::gold : colours::red;
    const auto noteColour = live ? stateColour : stateColour.withAlpha (0.45f);

    // Nota grande
    auto noteArea = r.removeFromTop (r.getHeight() * 0.42f);
    if (hasReading)
    {
        const auto name = Tuner::noteName (shown.midiNote);
        const auto letters = name.trimCharactersAtEnd ("-0123456789"), octave = name.substring (letters.length());
        auto big = Fonts::ui (juce::jmin (150.0f, noteArea.getHeight() * 0.9f), 700);
        const float w = juce::GlyphArrangement::getStringWidth (big, letters);
        auto line = noteArea.withSizeKeepingCentre (w + 60.0f, noteArea.getHeight());
        const auto c = noteArea.getCentre();
        juce::ColourGradient glow (noteColour.withAlpha (live ? 0.22f : 0.1f), c.x, c.y,
                                   noteColour.withAlpha (0.0f), c.x + noteArea.getHeight() * 0.85f, c.y, true);
        g.setGradientFill (glow);
        g.fillEllipse (noteArea.withSizeKeepingCentre (noteArea.getHeight() * 1.7f, noteArea.getHeight() * 1.1f));
        g.setColour (noteColour);
        g.setFont (big);
        g.drawText (letters, line.removeFromLeft (w + 4.0f), juce::Justification::centred);
        g.setFont (Fonts::ui (big.getHeight() * 0.3f, 600));
        g.drawText (octave, line.withTrimmedTop (big.getHeight() * 0.45f), juce::Justification::centredLeft);
    }
    else
    {
        g.setColour (colours::textDim);
        g.setFont (Fonts::ui (30.0f, 600));
        g.drawText (guitar ? U ("Toca una cuerda al aire") : U ("Canta una nota larga"), noteArea, juce::Justification::centred);
    }

    // Escala de cents con la aguja
    auto meter = r.removeFromTop (110.0f).reduced (juce::jmax (0.0f, (r.getWidth() - 760.0f) * 0.5f), 0.0f);
    auto scale = meter.removeFromTop (70.0f);
    const float cx = scale.getCentreX(), half = scale.getWidth() * 0.5f;
    auto xFor = [cx, half] (float c) { return cx + juce::jlimit (-50.0f, 50.0f, c) / 50.0f * half; };
    g.setColour (colours::green.withAlpha (0.16f));
    g.fillRoundedRectangle (juce::Rectangle<float> (xFor (-inTuneCents), scale.getY(), xFor (inTuneCents) - xFor (-inTuneCents), scale.getHeight()), 4.0f);
    for (int c = -50; c <= 50; c += 5)
    {
        const bool major = c % 25 == 0, mid = c % 10 == 0;
        const float h = major ? 34.0f : mid ? 22.0f : 12.0f;
        g.setColour (c == 0 ? colours::green : colours::edge.brighter (major ? 0.6f : 0.2f));
        g.fillRect (juce::Rectangle<float> (xFor ((float) c) - (c == 0 ? 1.5f : 0.75f), scale.getCentreY() - h * 0.5f, c == 0 ? 3.0f : 1.5f, h));
    }
    g.setFont (Fonts::ui (12.0f, 600));
    g.setColour (colours::textFaint);
    for (int c : { -50, -25, 25, 50 })
        g.drawText ((c > 0 ? "+" : "") + juce::String (c), juce::Rectangle<float> (xFor ((float) c) - 30.0f, scale.getBottom() - 12.0f, 60.0f, 16.0f),
                    juce::Justification::centred);
    g.setFont (Fonts::ui (22.0f, 700));
    g.drawText (U ("♭"), juce::Rectangle<float> (scale.getX() - 44.0f, scale.getY(), 30.0f, scale.getHeight()), juce::Justification::centred);
    g.drawText (U ("♯"), juce::Rectangle<float> (scale.getRight() + 14.0f, scale.getY(), 30.0f, scale.getHeight()), juce::Justification::centred);
    if (hasReading)
    {
        const float x = xFor (cents);
        g.setColour (noteColour.withAlpha (0.25f));
        g.fillRoundedRectangle (juce::Rectangle<float> (x - 9.0f, scale.getY() - 4.0f, 18.0f, scale.getHeight() + 8.0f), 9.0f);
        g.setColour (noteColour);
        g.fillRoundedRectangle (juce::Rectangle<float> (x - 3.5f, scale.getY() - 2.0f, 7.0f, scale.getHeight() + 4.0f), 3.5f);
    }

    // Qué hacer
    auto status = meter.removeFromTop (36.0f);
    if (hasReading)
    {
        juce::String advice;
        if (absCents < inTuneCents) advice = U ("Afinada");
        else if (cents < 0.0f)      advice = guitar ? U ("Baja: aprieta la clavija") : U ("Baja: sube un poco");
        else                        advice = guitar ? U ("Alta: afloja la clavija") : U ("Alta: baja un poco");
        g.setColour (stateColour);
        g.setFont (Fonts::ui (18.0f, 700));
        g.drawText (advice, status.removeFromTop (22.0f), juce::Justification::centred);
        g.setColour (colours::textDim);
        g.setFont (Fonts::ui (13.0f));
        g.drawText ((cents >= 0.0f ? "+" : "") + juce::String (cents, 1) + " cents  ·  " + juce::String (shown.hz, 1) + " Hz",
                    status, juce::Justification::centred);
    }

    // Cuerdas de la guitarra con afinación estándar
    r.removeFromTop (14.0f);
    if (guitar)
    {
        auto row = r.removeFromTop (64.0f);
        const float chipW = juce::jmin (130.0f, (row.getWidth() - 50.0f) / 6.0f);
        auto chips = row.withSizeKeepingCentre (chipW * 6.0f + 50.0f, row.getHeight());
        int nearest = -1;
        if (hasReading)
        {
            const float note = 69.0f + 12.0f * std::log2 (shown.hz / 440.0f);
            float bestD = 2.6f;
            for (int i = 0; i < 6; ++i)
                if (std::abs (note - (float) guitarStrings[i].midi) < bestD) { bestD = std::abs (note - (float) guitarStrings[i].midi); nearest = i; }
        }
        for (int i = 0; i < 6; ++i)
        {
            auto c = chips.removeFromLeft (chipW);
            chips.removeFromLeft (10.0f);
            const bool on = i == nearest;
            g.setColour (on ? stateColour.withAlpha (0.18f) : colours::panel);
            g.fillRoundedRectangle (c, 10.0f);
            g.setColour (on ? stateColour : colours::edge);
            g.drawRoundedRectangle (c.reduced (0.5f), 10.0f, on ? 2.0f : 1.0f);
            g.setColour (on ? colours::text : colours::textDim);
            g.setFont (Fonts::ui (17.0f, 700));
            g.drawText (U (guitarStrings[i].name), c.removeFromTop (c.getHeight() * 0.6f), juce::Justification::centredBottom);
            g.setColour (colours::textFaint);
            g.setFont (Fonts::ui (12.0f));
            const float hz = 440.0f * std::pow (2.0f, (float) (guitarStrings[i].midi - 69) / 12.0f);
            g.drawText (juce::String (hz, 1) + " Hz", c, juce::Justification::centred);
        }
    }

    // Nivel de entrada y ayuda si no llega sonido
    auto bottom = getLocalBounds().toFloat().reduced (22.0f, 18.0f).removeFromBottom (44.0f);
    auto levelRow = bottom.removeFromTop (16.0f).withSizeKeepingCentre (juce::jmin (520.0f, bottom.getWidth()), 16.0f);
    g.setColour (colours::textFaint);
    g.setFont (Fonts::ui (11.0f, 700));
    g.drawText (U ("ENTRADA"), levelRow.removeFromLeft (70.0f), juce::Justification::centredLeft);
    auto bar = levelRow.withSizeKeepingCentre (levelRow.getWidth(), 6.0f);
    g.setColour (juce::Colour (0xff1e2036));
    g.fillRoundedRectangle (bar, 3.0f);
    const float lv = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (level, -60.0f) + 60.0f) / 60.0f);
    g.setColour (colours::green);
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * lv), 3.0f);
    if (silentFrames > 45)
    {
        g.setColour (colours::gold);
        g.setFont (Fonts::ui (13.0f));
        g.drawFittedText (U ("No llega sonido. Si arriba pone que la entrada está silenciada, pulsa «Activar entrada», "
                             "y comprueba en «Ajustes de audio» la tarjeta y la entrada elegidas."),
                          bottom.toNearestInt(), juce::Justification::centred, 2);
    }
}
} // namespace geminis::fx

//==============================================================================
GeminisFxEditor::GeminisFxEditor (GeminisFxProcessor& p)
    : AudioProcessorEditor (p), processor (p),
      splash (U ("¿Qué vas a tocar hoy?"),
              { Splash::Card { "Guitarra", U ("Amplis, pantallas y 27 efectos."), U ("En directo o sobre pistas grabadas."), "Entrar", colours::guitar, Icon::guitar },
                Splash::Card { "Voz", U ("Cadena vocal, afinación y efectos."), U ("En directo o sobre pistas grabadas."), "Entrar", colours::voice, Icon::microphone },
                Splash::Card { "Instrumentos MIDI", U ("Sintetizador, batería e instrumentos."), U ("Mapas para Maschine y General MIDI."), U ("Cómo abrirlo"), colours::instrument, Icon::pads } })
{
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);

    emblemSmall = draw::emblem().rescaled (88, 88, juce::Graphics::highResamplingQuality);

    homeButton.setTooltip (U ("Volver a la pantalla de inicio"));
    homeButton.onClick = [this]
    {
        if (auto& goHome = geminis::homeHandler()) goHome();
        else showSplash (true);
    };
    presetButton.setTooltip (U ("Presets de fábrica y tuyos"));
    presetButton.onClick = [this] { showPresetMenu(); };
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    tunerButton.setClickingTogglesState (true);
    tunerButton.setTooltip (U ("Afinador a pantalla completa. La salida se silencia mientras afinas."));
    tunerButton.onClick = [this] { showTuner (tunerButton.getToggleState()); };
    tunerPanel.onClose = [this] { showTuner (false); };
    for (auto* b : { &homeButton, &presetButton, &prevButton, &nextButton, &tunerButton })
        addAndMakeVisible (b);

    inputBox.addItemList ({ "Mono izquierda", "Mono derecha", "Mono suma", U ("Estéreo") }, 1);
    inputBox.setTooltip (U ("Qué entrada usar: guitarra en la entrada 1, 2, ambas, o pista estéreo"));
    addAndMakeVisible (inputBox);
    inputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, "inputSel", inputBox);

    inKnob.attach (processor.apvts, "inGain", "Entrada");
    outKnob.attach (processor.apvts, "outGain", "Salida");
    inKnob.setBipolar (true);
    outKnob.setBipolar (true);
    inKnob.setCompact (true);
    outKnob.setCompact (true);
    addAndMakeVisible (inKnob);
    addAndMakeVisible (outKnob);
    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);

    browserView.setScrollBarsShown (true, false);
    rackView.setScrollBarsShown (true, false);
    rackView.setViewedComponent (&rackContent, false);
    addAndMakeVisible (browserView);
    addAndMakeVisible (rackView);

    emptyHint.setText (U ("Añade módulos desde la lista de la izquierda. La señal va de arriba abajo."), juce::dontSendNotification);
    emptyHint.setJustificationType (juce::Justification::centred);
    emptyHint.setColour (juce::Label::textColourId, colours::textFaint);
    emptyHint.setFont (Fonts::ui (14.0f));
    rackContent.addAndMakeVisible (emptyHint);
    addChildComponent (tunerPanel);

    splash.onChoose = [this] (int index)
    {
        if (index == 2)
        {
            splash.showMessage (U ("Instrumentos MIDI"),
                                U ("Los instrumentos están en el plugin hermano Geminis Instrument. "
                                   "Créalo en una pista de instrumento o MIDI de tu DAW (o en un Sound de Maschine) "
                                   "y elige Sintetizador, Batería o Instrumentos."));
            return;
        }
        enterMode (index == 0 ? Mode::guitar : Mode::voice);
    };
    addChildComponent (splash);

    processor.rack.onStructureChanged = [this] { rebuildRack(); };
    processor.onPresetChanged = [this] { updatePresetLabel(); rebuildBrowser(); rebuildRack(); repaint(); };

    setSize (1200, 780);
    rebuildBrowser();
    rebuildRack();
    updatePresetLabel();
    showSplash (processor.getMode() == Mode::none);
    startTimerHz (30);
}

GeminisFxEditor::~GeminisFxEditor()
{
    stopTimer();
    processor.tuner.enabled = false;
    processor.rack.onStructureChanged = nullptr;
    processor.onPresetChanged = nullptr;
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

juce::Colour GeminisFxEditor::accent() const
{
    return processor.getMode() == Mode::voice ? colours::voice : colours::guitar;
}

void GeminisFxEditor::showTuner (bool show)
{
    tunerButton.setToggleState (show, juce::dontSendNotification);
    processor.tuner.enabled = show;
    tunerPanel.setGuitar (processor.getMode() != Mode::voice);
    tunerPanel.reset();
    tunerPanel.setVisible (show);
    if (show) tunerPanel.toFront (false);
    tunerCounter = 0;
}

void GeminisFxEditor::showSplash (bool show)
{
    if (show) showTuner (false);
    splash.setVisible (show);
    if (show) splash.toFront (false);
}

void GeminisFxEditor::enterMode (Mode m)
{
    processor.setMode (m, true);
    showSplash (false);
    updatePresetLabel();
    rebuildBrowser();
    rebuildRack();
    repaint();
}

void GeminisFxEditor::rebuildBrowser()
{
    browser = std::make_unique<ModuleBrowser> (processor.getMode());
    browser->onAdd = [this] (int type)
    {
        if (processor.rack.addModule (type) < 0)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, U ("Rack lleno"),
                                                    U ("El rack admite 10 módulos. Quita alguno para añadir otro."));
    };
    browserView.setViewedComponent (browser.get(), false);
    browser->setSize (browserView.getWidth() - 8, browser->getIdealHeight());
}

void GeminisFxEditor::rebuildRack()
{
    panels.clear();
    auto& rack = processor.rack;
    int position = 0;
    for (int s = 0; s < Rack::numSlots; ++s)
    {
        if (rack.getType (s) == empty) continue;
        auto* panel = panels.add (new SlotPanel (processor, s, position++, accent()));
        panel->onMoveUp = [this] (int slot)
        {
            for (int prev = slot - 1; prev >= 0; --prev)
                if (processor.rack.getType (prev) != empty) { processor.rack.swapSlots (slot, prev); break; }
        };
        panel->onMoveDown = [this] (int slot)
        {
            for (int next = slot + 1; next < Rack::numSlots; ++next)
                if (processor.rack.getType (next) != empty) { processor.rack.swapSlots (slot, next); break; }
        };
        panel->onRemove = [this] (int slot) { juce::MessageManager::callAsync ([this, slot] { processor.rack.removeSlot (slot); }); };
        rackContent.addAndMakeVisible (panel);
    }
    emptyHint.setVisible (panels.isEmpty());
    resized();
}

void GeminisFxEditor::updatePresetLabel()
{
    presetButton.setButtonText (processor.getPresetName());
}

void GeminisFxEditor::showPresetMenu()
{
    const auto& presets = fxPresets();
    const auto mode = processor.getMode();
    juce::PopupMenu menu;
    juce::StringArray cats;
    for (const auto& p : presets)
        if (p.mode == mode && ! cats.contains (juce::String::fromUTF8 (p.category)))
            cats.add (juce::String::fromUTF8 (p.category));

    for (const auto& cat : cats)
    {
        juce::PopupMenu sub;
        for (int i = 0; i < (int) presets.size(); ++i)
            if (presets[(size_t) i].mode == mode && juce::String::fromUTF8 (presets[(size_t) i].category) == cat)
                sub.addItem (i + 1, juce::String::fromUTF8 (presets[(size_t) i].name), true, processor.getCurrentProgram() == i);
        menu.addSubMenu (cat, sub);
    }

    juce::PopupMenu user;
    int userId = 10000;
    const auto files = GeminisFxProcessor::userPresetFolder().findChildFiles (juce::File::findFiles, false, "*.geminisfx");
    for (const auto& f : files) user.addItem (userId++, f.getFileNameWithoutExtension());
    if (files.isEmpty()) user.addItem (-1, U ("(todavía no hay presets tuyos)"), false);
    menu.addSeparator();
    menu.addSubMenu (U ("Mis presets"), user);
    menu.addItem (9001, U ("Guardar preset..."));
    menu.addItem (9002, U ("Cargar preset desde archivo..."));
    menu.addItem (9003, U ("Vaciar el rack"));

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetButton), [this, files] (int result)
    {
        if (result <= 0) return;
        if (result >= 10000) { processor.loadUserPreset (files[result - 10000]); return; }
        if (result == 9001)
        {
            fileChooser = std::make_unique<juce::FileChooser> (U ("Guardar preset"), GeminisFxProcessor::userPresetFolder().getChildFile ("Mi sonido.geminisfx"), "*.geminisfx");
            fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                                      [this] (const juce::FileChooser& fc)
                                      {
                                          auto f = fc.getResult();
                                          if (f == juce::File()) return;
                                          processor.saveUserPreset (f.withFileExtension ("geminisfx"));
                                      });
            return;
        }
        if (result == 9002)
        {
            fileChooser = std::make_unique<juce::FileChooser> (U ("Cargar preset"), GeminisFxProcessor::userPresetFolder(), "*.geminisfx");
            fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                      [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) processor.loadUserPreset (fc.getResult()); });
            return;
        }
        if (result == 9003) { processor.rack.clearAll(); return; }
        processor.applyPreset (result - 1);
    });
}

void GeminisFxEditor::stepPreset (int delta)
{
    const auto& presets = fxPresets();
    const int n = (int) presets.size();
    int i = processor.getCurrentProgram();
    for (int k = 0; k < n; ++k)
    {
        i = (i + delta + n) % n;
        if (presets[(size_t) i].mode == processor.getMode()) { processor.applyPreset (i); return; }
    }
}

void GeminisFxEditor::timerCallback()
{
    const float inPeak = processor.inputPeak.exchange (0.0f);
    inMeter.push (inPeak);
    outMeter.push (processor.outputPeak.exchange (0.0f));
    if (tunerPanel.isVisible())
    {
        tunerPeak = juce::jmax (tunerPeak, inPeak);
        if (++tunerCounter >= 2)
        {
            tunerCounter = 0;
            tunerPanel.update (processor.tuner.analyse(), tunerPeak);
            tunerPeak = 0.0f;
        }
    }
}

void GeminisFxEditor::paint (juce::Graphics& g)
{
    const auto acc = accent();
    juce::ColourGradient bgGrad (juce::Colour (0xff0d0c1d), 0.0f, 0.0f, colours::bg, 0.0f, (float) getHeight(), false);
    g.setGradientFill (bgGrad);
    g.fillAll();
    juce::ColourGradient glow (acc.withAlpha (0.07f), (float) getWidth() * 0.6f, 60.0f, acc.withAlpha (0.0f), (float) getWidth() * 0.6f, 500.0f, true);
    g.setGradientFill (glow);
    g.fillAll();

    // Barra superior
    auto bar = getLocalBounds().removeFromTop (68).toFloat();
    juce::ColourGradient barGrad (juce::Colour (0xff17172c), 0.0f, 0.0f, juce::Colour (0xff101021), 0.0f, bar.getBottom(), false);
    g.setGradientFill (barGrad);
    g.fillRect (bar);
    juce::ColourGradient line (acc.withAlpha (0.0f), 0.0f, bar.getBottom(), acc.withAlpha (0.0f), bar.getRight(), bar.getBottom(), false);
    line.addColour (0.5, acc.withAlpha (0.6f));
    g.setGradientFill (line);
    g.fillRect (bar.withTop (bar.getBottom() - 1.0f));

    g.drawImage (emblemSmall, juce::Rectangle<float> (14.0f, 10.0f, 48.0f, 48.0f));
    juce::ColourGradient gold (juce::Colour (0xfffff4d2), 0.0f, 18.0f, colours::goldDeep, 0.0f, 48.0f, false);
    draw::textWithGradient (g, U ("GÉMINIS"), Fonts::brand (24.0f).withExtraKerningFactor (0.12f), { 70.0f, 12.0f, 160.0f, 30.0f },
                            juce::Justification::centredLeft, gold);
    g.setColour (acc);
    draw::spacedText (g, processor.getMode() == Mode::voice ? "VOZ" : "GUITARRA", Fonts::ui (11.0f, 700),
                      { 71.0f, 40.0f, 140.0f, 16.0f }, juce::Justification::centredLeft, 0.25f);

    // Títulos de columnas
    g.setColour (colours::textFaint);
    draw::spacedText (g, U ("MÓDULOS"), Fonts::ui (11.0f, 700), { 18.0f, 78.0f, 200.0f, 18.0f }, juce::Justification::centredLeft, 0.2f);
    draw::spacedText (g, U ("CADENA DE SEÑAL  ·  ENTRADA ARRIBA, SALIDA ABAJO"), Fonts::ui (11.0f, 700),
                      { 246.0f, 78.0f, 600.0f, 18.0f }, juce::Justification::centredLeft, 0.2f);

    auto side = browserView.getBounds().toFloat().expanded (4.0f, 4.0f);
    draw::glassPanel (g, side, acc, 0.25f, 12.0f);
}

void GeminisFxEditor::resized()
{
    splash.setBounds (getLocalBounds());

    auto bar = getLocalBounds().removeFromTop (68).reduced (14, 16);
    bar.removeFromLeft (220);
    homeButton.setBounds (bar.removeFromLeft (76));
    bar.removeFromLeft (14);
    prevButton.setBounds (bar.removeFromLeft (30));
    bar.removeFromLeft (4);
    presetButton.setBounds (bar.removeFromLeft (230));
    bar.removeFromLeft (4);
    nextButton.setBounds (bar.removeFromLeft (30));
    bar.removeFromLeft (14);

    auto right = bar.removeFromRight (170);
    inMeter.setBounds (right.removeFromTop (right.getHeight() / 2).reduced (0, 3));
    outMeter.setBounds (right.reduced (0, 3));
    bar.removeFromRight (8);
    outKnob.setBounds (bar.removeFromRight (52).withY (6).withHeight (58));
    inKnob.setBounds (bar.removeFromRight (52).withY (6).withHeight (58));
    bar.removeFromRight (8);
    inputBox.setBounds (bar.removeFromRight (140));
    bar.removeFromRight (8);
    tunerButton.setBounds (bar.removeFromLeft (84));
    tunerPanel.setBounds (getLocalBounds().withTrimmedTop (84).reduced (14, 0).withTrimmedBottom (14));

    auto body = getLocalBounds().withTrimmedTop (100).reduced (14, 0).withTrimmedBottom (14);
    browserView.setBounds (body.removeFromLeft (212).reduced (4, 4));
    if (browser != nullptr) browser->setSize (browserView.getWidth() - 10, browser->getIdealHeight());
    body.removeFromLeft (14);
    rackView.setBounds (body);

    const int w = rackView.getWidth() - 12;
    const int panelH = 132, gap = 10;
    int y = 0;
    for (auto* p : panels)
    {
        p->setBounds (0, y, w, panelH);
        y += panelH + gap;
    }
    emptyHint.setBounds (0, 0, w, 200);
    rackContent.setSize (w, juce::jmax (y, 200));
}
