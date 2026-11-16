#include "Theme.h"
#include "BinaryData.h"

namespace geminis
{
//==============================================================================
Fonts::Fonts()
{
    auto load = [] (const void* data, int size) { return juce::Typeface::createSystemTypefaceFor (data, (size_t) size); };
    interRegular = load (BinaryData::InterRegular_otf, BinaryData::InterRegular_otfSize);
    interSemi    = load (BinaryData::InterSemiBold_otf, BinaryData::InterSemiBold_otfSize);
    interBold    = load (BinaryData::InterBold_otf, BinaryData::InterBold_otfSize);
    cinzelMedium = load (BinaryData::Cinzel500_ttf, BinaryData::Cinzel500_ttfSize);
    cinzelBold   = load (BinaryData::Cinzel700_ttf, BinaryData::Cinzel700_ttfSize);
}

juce::Font Fonts::ui (float height, int weight)
{
    juce::SharedResourcePointer<Fonts> f;
    auto tf = weight >= 700 ? f->interBold : weight >= 600 ? f->interSemi : f->interRegular;
    return juce::Font (juce::FontOptions (tf).withHeight (height));
}

juce::Font Fonts::brand (float height, bool bold)
{
    juce::SharedResourcePointer<Fonts> f;
    return juce::Font (juce::FontOptions (bold ? f->cinzelBold : f->cinzelMedium).withHeight (height));
}

//==============================================================================
GeminisLookAndFeel::GeminisLookAndFeel()
{
    using namespace colours;
    setColour (juce::ResizableWindow::backgroundColourId, bg);
    setColour (juce::Slider::textBoxTextColourId, textDim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, gold.withAlpha (0.3f));
    setColour (juce::Slider::rotarySliderFillColourId, gold);
    setColour (juce::Label::textColourId, text);
    setColour (juce::Label::textWhenEditingColourId, text);
    setColour (juce::TextEditor::backgroundColourId, panelHi);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::outlineColourId, edge);
    setColour (juce::TextEditor::focusedOutlineColourId, gold);
    setColour (juce::ComboBox::backgroundColourId, panelHi);
    setColour (juce::ComboBox::outlineColourId, edge);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, textDim);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::headerTextColourId, gold);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, gold.withAlpha (0.18f));
    setColour (juce::PopupMenu::highlightedTextColourId, text);
    setColour (juce::TextButton::buttonColourId, panelHi);
    setColour (juce::TextButton::buttonOnColourId, gold);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, bg);
    setColour (juce::ToggleButton::tickColourId, gold);
    setColour (juce::ScrollBar::thumbColourId, edge);
    setColour (juce::TooltipWindow::backgroundColourId, panelHi);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::AlertWindow::backgroundColourId, panel);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
}

juce::Typeface::Ptr GeminisLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    if (f.isBold())
        return fonts->interBold;
    return fonts->interRegular;
}

void GeminisLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    if (radius < 6.0f) return;

    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
    const float alpha = slider.isEnabled() ? 1.0f : 0.3f;
    const bool bipolar = (bool) slider.getProperties().getWithDefault ("bipolar", false);
    const float arcR = radius - 2.0f;

    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff23253b).withAlpha (alpha));
    g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.01f)
    {
        juce::Path value;
        value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (accent.withAlpha (0.22f * alpha));
        g.strokePath (value, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (accent.withAlpha (alpha));
        g.strokePath (value, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float kr = radius - 8.0f;
    if (kr > 3.0f)
    {
        juce::ColourGradient body (juce::Colour (0xff34375a).withAlpha (alpha), c.x - kr * 0.6f, c.y - kr * 0.8f,
                                   juce::Colour (0xff121322).withAlpha (alpha), c.x + kr * 0.5f, c.y + kr, true);
        g.setGradientFill (body);
        g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.10f * alpha));
        g.drawEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f, 1.0f);

        const juce::Point<float> tip (c.x + std::sin (angle) * (kr - 3.0f), c.y - std::cos (angle) * (kr - 3.0f));
        const juce::Point<float> base (c.x + std::sin (angle) * kr * 0.35f, c.y - std::cos (angle) * kr * 0.35f);
        g.setColour (colours::text.withAlpha (0.9f * alpha));
        g.drawLine ({ base, tip }, 2.2f);
        g.setColour (accent.withAlpha (alpha));
        g.fillEllipse (tip.x - 2.2f, tip.y - 2.2f, 4.4f, 4.4f);
    }
}

void GeminisLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                           juce::Slider::SliderStyle style, juce::Slider& slider)
{
    const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
    const bool vertical = style == juce::Slider::LinearVertical;
    auto r = juce::Rectangle<int> (x, y, w, h).toFloat();
    auto track = vertical ? r.withSizeKeepingCentre (4.0f, r.getHeight()) : r.withSizeKeepingCentre (r.getWidth(), 4.0f);
    g.setColour (juce::Colour (0xff23253b));
    g.fillRoundedRectangle (track, 2.0f);
    auto filled = vertical ? track.withTop (pos) : track.withRight (pos);
    g.setColour (accent);
    g.fillRoundedRectangle (filled, 2.0f);
    const auto thumb = vertical ? juce::Point<float> (r.getCentreX(), pos) : juce::Point<float> (pos, r.getCentreY());
    g.setColour (colours::text);
    g.fillEllipse (thumb.x - 6.0f, thumb.y - 6.0f, 12.0f, 12.0f);
}

void GeminisLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool)
{
    const auto area = button.getLocalBounds().toFloat();
    const auto pill = area.withSizeKeepingCentre (juce::jmin (32.0f, area.getWidth()), 17.0f).withX (area.getX() + 1.0f);
    const bool on = button.getToggleState();
    const auto accent = button.findColour (juce::ToggleButton::tickColourId);

    if (on)
    {
        g.setColour (accent.withAlpha (0.25f));
        g.fillRoundedRectangle (pill.expanded (2.0f), 10.0f);
    }
    g.setColour (on ? accent.withAlpha (highlighted ? 1.0f : 0.92f) : juce::Colour (0xff2a2c45));
    g.fillRoundedRectangle (pill, 8.5f);
    g.setColour (on ? colours::bg : colours::textDim);
    const float knobX = on ? pill.getRight() - 15.0f : pill.getX() + 2.0f;
    g.fillEllipse (knobX, pill.getY() + 2.0f, 13.0f, 13.0f);

    if (button.getButtonText().isNotEmpty() && area.getWidth() > 50.0f)
    {
        g.setColour (on ? colours::text : colours::textDim);
        g.setFont (Fonts::ui (13.0f, 600));
        g.drawText (button.getButtonText(), area.withTrimmedLeft (pill.getWidth() + 8.0f), juce::Justification::centredLeft);
    }
}

void GeminisLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const auto accent = b.findColour (juce::TextButton::buttonOnColourId);
    const float radius = juce::jmin (8.0f, r.getHeight() * 0.5f);

    if (on)
    {
        g.setColour (accent.withAlpha (down ? 0.75f : highlighted ? 0.95f : 0.85f));
        g.fillRoundedRectangle (r, radius);
        return;
    }

    juce::ColourGradient grad (colours::panelHi.brighter (highlighted ? 0.12f : 0.04f), 0.0f, r.getY(),
                               colours::panel, 0.0f, r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, radius);
    g.setColour (highlighted ? accent.withAlpha (0.6f) : colours::edge);
    g.drawRoundedRectangle (r, radius, 1.0f);
    if (down)
    {
        g.setColour (juce::Colours::black.withAlpha (0.2f));
        g.fillRoundedRectangle (r, radius);
    }
}

void GeminisLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (Fonts::ui (juce::jmin (14.0f, (float) b.getHeight() * 0.5f), 600));
    g.setColour (b.getToggleState() ? b.findColour (juce::TextButton::textColourOnId)
                                    : b.findColour (juce::TextButton::textColourOffId).withAlpha (b.isEnabled() ? 1.0f : 0.4f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (6, 0), juce::Justification::centred, 1);
}

void GeminisLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (box.isMouseOver (true) ? colours::gold.withAlpha (0.5f) : box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (r, 7.0f, 1.0f);

    juce::Path arrow;
    const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    arrow.startNewSubPath (ax - 4.0f, ay - 2.0f);
    arrow.lineTo (ax, ay + 2.5f);
    arrow.lineTo (ax + 4.0f, ay - 2.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId).withAlpha (box.isEnabled() ? 1.0f : 0.4f));
    g.strokePath (arrow, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font GeminisLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return Fonts::ui (juce::jmin (14.0f, (float) box.getHeight() * 0.55f), 400);
}

void GeminisLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font GeminisLookAndFeel::getPopupMenuFont() { return Fonts::ui (14.0f, 400); }

void GeminisLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (colours::panel);
    g.setColour (colours::edge);
    g.drawRect (0, 0, w, h, 1);
}

juce::Label* GeminisLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (slider);
    l->setFont (Fonts::ui (11.5f, 400));
    l->setJustificationType (juce::Justification::centred);
    l->setColour (juce::Label::outlineWhenEditingColourId, colours::gold);
    l->setColour (juce::Label::backgroundWhenEditingColourId, colours::panelHi);
    return l;
}

void GeminisLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                                        int thumbStart, int thumbSize, bool over, bool)
{
    auto thumb = vertical ? juce::Rectangle<int> (x + w / 2 - 3, thumbStart, 6, thumbSize)
                          : juce::Rectangle<int> (thumbStart, y + h / 2 - 3, thumbSize, 6);
    g.setColour (over ? colours::textFaint : colours::edge);
    g.fillRoundedRectangle (thumb.toFloat(), 3.0f);
}

void GeminisLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    g.fillAll (colours::panelHi);
    g.setColour (colours::edge);
    g.drawRect (0, 0, w, h);
    g.setColour (colours::text);
    g.setFont (Fonts::ui (13.0f));
    g.drawFittedText (text, juce::Rectangle<int> (w, h).reduced (6, 2), juce::Justification::centredLeft, 4);
}

//==============================================================================
namespace draw
{
void glassPanel (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour accent, float accentAlpha, float radius)
{
    juce::ColourGradient fill (juce::Colour (0xff1a1b30).withAlpha (0.92f), r.getX(), r.getY(),
                               juce::Colour (0xff0e0f1c).withAlpha (0.92f), r.getX(), r.getBottom(), false);
    g.setGradientFill (fill);
    g.fillRoundedRectangle (r, radius);

    juce::ColourGradient border (accent.withAlpha (accentAlpha), r.getX(), r.getY(),
                                 colours::edge.withAlpha (0.6f), r.getRight(), r.getBottom(), false);
    g.setGradientFill (border);
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);

    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + radius, r.getRight() - radius);
}

void textWithGradient (juce::Graphics& g, const juce::String& text, const juce::Font& font, juce::Rectangle<float> area,
                       juce::Justification just, juce::ColourGradient grad)
{
    juce::GlyphArrangement glyphs;
    glyphs.addFittedText (font, text, area.getX(), area.getY(), area.getWidth(), area.getHeight(), just, 1);
    juce::Path p;
    glyphs.createPath (p);
    g.setGradientFill (grad);
    g.fillPath (p);
}

void spacedText (juce::Graphics& g, const juce::String& text, const juce::Font& font, juce::Rectangle<float> area,
                 juce::Justification just, float tracking)
{
    g.setFont (font.withExtraKerningFactor (tracking));
    g.drawText (text, area, just, false);
}

juce::Image emblem()
{
    return juce::ImageCache::getFromMemory (BinaryData::logoemblema_png, BinaryData::logoemblema_pngSize);
}

juce::Image emblemGlow()
{
    auto small = emblem().rescaled (192, 192, juce::Graphics::mediumResamplingQuality);
    juce::Image glow (juce::Image::ARGB, 192, 192, true);
    juce::ImageConvolutionKernel kernel (17);
    kernel.createGaussianBlur (7.0f);
    kernel.applyToImage (glow, small, glow.getBounds());
    return glow;
}
} // namespace draw
} // namespace geminis
