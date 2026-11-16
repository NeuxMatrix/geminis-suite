#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

namespace geminis
{
/** Potenciómetro con su nombre encima y el valor debajo, unido a un parámetro. */
class ParamKnob : public juce::Component
{
public:
    ParamKnob() { init(); }
    ParamKnob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& label, juce::Colour c = colours::gold)
    {
        init();
        attach (s, id, label, c);
    }

    void attach (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& label, juce::Colour c = colours::gold)
    {
        attachment.reset();
        name.setText (label, juce::dontSendNotification);
        slider.setColour (juce::Slider::rotarySliderFillColourId, c);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    }

    void setAccent (juce::Colour c) { slider.setColour (juce::Slider::rotarySliderFillColourId, c); repaint(); }
    void setBipolar (bool b) { slider.getProperties().set ("bipolar", b); }
    void setCompact (bool c) { compact = c; resized(); }
    void setShowName (bool b) { showName = b; name.setVisible (b); resized(); }

    void resized() override
    {
        auto r = getLocalBounds();
        if (! showName)
        {
            slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            slider.setBounds (r);
            return;
        }
        if (compact)
        {
            name.setBounds (r.removeFromTop (13));
            slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            slider.setBounds (r);
            return;
        }
        name.setBounds (r.removeFromTop (15));
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, r.getWidth(), 15);
        slider.setBounds (r);
    }

    juce::Slider slider;
    juce::Label name;

private:
    void init()
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 15);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.setDoubleClickReturnValue (false, 0.0);
        addAndMakeVisible (slider);
        name.setJustificationType (juce::Justification::centred);
        name.setFont (Fonts::ui (11.5f, 600));
        name.setColour (juce::Label::textColourId, colours::textDim);
        name.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (name);
    }

    bool compact = false, showName = true;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

/** Une un ComboBox a un parámetro normalizado (0..1) repartido en N opciones. */
class NormalisedChoiceAttachment
{
public:
    NormalisedChoiceAttachment (juce::RangedAudioParameter& p, juce::ComboBox& box, int numChoices)
        : combo (box), n (juce::jmax (2, numChoices)),
          attachment (p, [this] (float v) { combo.setSelectedItemIndex (juce::roundToInt (v * (float) (n - 1)), juce::dontSendNotification); })
    {
        combo.onChange = [this]
        {
            const int idx = combo.getSelectedItemIndex();
            if (idx >= 0) attachment.setValueAsCompleteGesture ((float) idx / (float) (n - 1));
        };
        attachment.sendInitialUpdate();
    }
    ~NormalisedChoiceAttachment() { combo.onChange = nullptr; }

private:
    juce::ComboBox& combo;
    int n;
    juce::ParameterAttachment attachment;
};

/** Medidor de nivel horizontal. */
class LevelMeter : public juce::Component
{
public:
    explicit LevelMeter (const juce::String& labelText) : label (labelText) {}

    void push (float peak)
    {
        const float target = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (peak, -60.0f) + 60.0f) / 60.0f);
        const float next = target > level ? target : level * 0.86f + target * 0.14f;
        if (target > 0.97f) clipHold = 40;
        else if (clipHold > 0) --clipHold;
        if (std::abs (next - level) > 0.002f || clipHold > 0) { level = next; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        g.setColour (colours::textFaint);
        g.setFont (Fonts::ui (10.0f, 700));
        g.drawText (label, area.removeFromLeft (28.0f), juce::Justification::centredLeft);
        auto bar = area.withSizeKeepingCentre (area.getWidth(), 6.0f);
        g.setColour (juce::Colour (0xff1e2036));
        g.fillRoundedRectangle (bar, 3.0f);
        juce::ColourGradient grad (colours::green, bar.getX(), 0.0f, colours::red, bar.getRight(), 0.0f, false);
        grad.addColour (0.7, colours::gold);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * level), 3.0f);
        if (clipHold > 0)
        {
            g.setColour (colours::red);
            g.fillEllipse (bar.getRight() - 6.0f, bar.getCentreY() - 3.0f, 6.0f, 6.0f);
        }
    }

private:
    juce::String label;
    float level = 0.0f;
    int clipHold = 0;
};
} // namespace geminis
