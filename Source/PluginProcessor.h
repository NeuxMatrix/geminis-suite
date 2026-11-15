#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/GuitarChain.h"

namespace geminis
{
enum class Mode { none = 0, guitar = 1, voice = 2, instrument = 3 };

struct FactoryPreset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;
};

const std::vector<FactoryPreset>& getFactoryPresets();
}

class GeminisProcessor : public juce::AudioProcessor
{
public:
    GeminisProcessor();
    ~GeminisProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Geminis FX"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==========================================================================
    geminis::Mode getMode() const noexcept { return (geminis::Mode) mode.load(); }
    void setMode (geminis::Mode m);

    void loadUserIr (const juce::File&);
    juce::String getUserIrName() const { return juce::File (userIrPath).getFileNameWithoutExtension(); }

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    geminis::GuitarSettings readSettings() const;
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    geminis::GuitarChain guitar;
    juce::AudioBuffer<float> work;
    std::atomic<int> mode { 0 };
    int currentPreset = 0;
    juce::String userIrPath;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GeminisProcessor)
};
