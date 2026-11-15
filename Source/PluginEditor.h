#pragma once
#include "PluginProcessor.h"

namespace geminis
{
inline juce::String U (const char* utf8) { return juce::String::fromUTF8 (utf8); }

namespace colours
{
    const juce::Colour background { 0xff12141a };
    const juce::Colour panel      { 0xff1c2029 };
    const juce::Colour panelEdge  { 0xff2c323f };
    const juce::Colour text       { 0xffe8eaf0 };
    const juce::Colour textDim    { 0xff8b93a5 };
    const juce::Colour guitar     { 0xffe9b44c };
    const juce::Colour voice      { 0xff4cc9c0 };
    const juce::Colour instrument { 0xffa78bfa };
}

//==============================================================================
class GeminisLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GeminisLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

//==============================================================================
/** Un módulo del rack: título, interruptor, selector opcional y potenciómetros. */
class ModulePanel : public juce::Component
{
public:
    struct Knob { const char* id; const char* label; };

    ModulePanel (juce::AudioProcessorValueTreeState&, const char* title, const char* bypassId,
                 std::vector<Knob> knobs, const char* comboId = nullptr);

    void addFooterButton (const juce::String& text, std::function<void()> onClick);
    void setFooterText (const juce::String& text);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    juce::String title;
    juce::ToggleButton power;
    std::unique_ptr<APVTS::ButtonAttachment> powerAttachment;
    juce::ComboBox combo;
    std::unique_ptr<APVTS::ComboBoxAttachment> comboAttachment;
    juce::OwnedArray<juce::Slider> sliders;
    juce::OwnedArray<juce::Label> labels;
    juce::OwnedArray<APVTS::SliderAttachment> attachments;
    std::unique_ptr<juce::TextButton> footer;
    bool hasPower = false, hasCombo = false;

    void updateEnabled();
};

//==============================================================================
class LevelMeter : public juce::Component
{
public:
    explicit LevelMeter (const juce::String& labelText) : label (labelText) {}
    void push (float peak);
    void paint (juce::Graphics&) override;

private:
    juce::String label;
    float level = 0.0f;
};

//==============================================================================
/** Pantalla de inicio: pregunta para qué se va a usar Geminis. */
class ModeChooser : public juce::Component
{
public:
    std::function<void (Mode)> onChoose;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::Rectangle<int> cardBounds (int index) const;
    int hovered = -1;
};
} // namespace geminis

//==============================================================================
class GeminisEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GeminisEditor (GeminisProcessor&);
    ~GeminisEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showChooser (bool shouldShow);
    void chooseIr();

    GeminisProcessor& processor;
    geminis::GeminisLookAndFeel lookAndFeel;

    juce::TextButton modeButton;
    juce::ComboBox presetBox, inputBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> inputAttachment;
    geminis::LevelMeter inMeter { "IN" }, outMeter { "OUT" };

    std::unique_ptr<geminis::ModulePanel> input, comp, drive, amp, cab, trem, delay, reverb, output;
    geminis::ModeChooser chooser;
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GeminisEditor)
};
