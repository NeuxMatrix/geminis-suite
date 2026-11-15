#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace geminis
{
inline juce::String U (const char* utf8) { return juce::String::fromUTF8 (utf8); }

/** Mayúsculas que respetan tildes y eñes en cualquier sistema (toUpperCase depende del idioma del sistema). */
inline juce::String upper (const juce::String& s)
{
    juce::String out;
    for (auto c : s)
    {
        switch (c)
        {
            case 0xe1: c = 0xc1; break;  case 0xe9: c = 0xc9; break;  case 0xed: c = 0xcd; break;
            case 0xf3: c = 0xd3; break;  case 0xfa: c = 0xda; break;  case 0xf1: c = 0xd1; break;
            case 0xfc: c = 0xdc; break;
            default: c = juce::CharacterFunctions::toUpperCase (c); break;
        }
        out += juce::String::charToString (c);
    }
    return out;
}

namespace colours
{
    const juce::Colour bg          { 0xff08080f };
    const juce::Colour bg2         { 0xff0f0f1c };
    const juce::Colour panel       { 0xff151627 };
    const juce::Colour panelHi     { 0xff1d1f35 };
    const juce::Colour edge        { 0xff2a2c47 };
    const juce::Colour text        { 0xffecedf6 };
    const juce::Colour textDim     { 0xff8d91ad };
    const juce::Colour textFaint   { 0xff565a78 };
    const juce::Colour gold        { 0xfff3c96a };
    const juce::Colour goldDeep    { 0xffc88a28 };
    const juce::Colour cyan        { 0xff8fe9ff };
    const juce::Colour violet      { 0xffa78bfa };
    const juce::Colour rose        { 0xffff7aa8 };
    const juce::Colour green       { 0xff6ee7a8 };
    const juce::Colour red         { 0xffff5d6c };

    const juce::Colour guitar      = gold;
    const juce::Colour voice       = cyan;
    const juce::Colour instrument  = violet;
}

/** Tipografías incluidas en el plugin (Inter para la interfaz, Cinzel para la marca). */
struct Fonts
{
    Fonts();
    juce::Typeface::Ptr interRegular, interSemi, interBold, cinzelMedium, cinzelBold;

    static juce::Font ui (float height, int weight = 400);      // 400, 600, 700
    static juce::Font brand (float height, bool bold = true);
};

/** Aspecto común de Geminis: potenciómetros, interruptores, menús y botones. */
class GeminisLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GeminisLookAndFeel();

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                        int thumbStart, int thumbSize, bool over, bool down) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int w, int h) override;

private:
    juce::SharedResourcePointer<Fonts> fonts;
};

/** Utilidades de dibujo compartidas. */
namespace draw
{
    void glassPanel (juce::Graphics&, juce::Rectangle<float>, juce::Colour accent, float accentAlpha, float radius = 12.0f);
    void textWithGradient (juce::Graphics&, const juce::String&, const juce::Font&, juce::Rectangle<float>,
                           juce::Justification, juce::ColourGradient);
    void spacedText (juce::Graphics&, const juce::String&, const juce::Font&, juce::Rectangle<float>,
                     juce::Justification, float tracking);
    juce::Image emblem();                 // logo zodiacal
    juce::Image emblemGlow();             // versión desenfocada para el halo
}
} // namespace geminis
