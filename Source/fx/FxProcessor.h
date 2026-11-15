#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Rack.h"
#include "FxPresets.h"
#include "../common/Tuner.h"

class GeminisFxProcessor : public juce::AudioProcessor
{
public:
    GeminisFxProcessor();
    ~GeminisFxProcessor() override;

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
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==========================================================================
    geminis::fx::Mode getMode() const noexcept { return (geminis::fx::Mode) mode.load(); }
    void setMode (geminis::fx::Mode m, bool loadDefaultPreset);

    /** Aplica un preset de fábrica (índice en fxPresets()). */
    void applyPreset (int index);
    juce::String getPresetName() const { return presetName; }

    /** Guarda o carga un preset de usuario en un archivo. */
    bool saveUserPreset (const juce::File&);
    bool loadUserPreset (const juce::File&);
    static juce::File userPresetFolder();

    void loadUserIr (const juce::File&);

    juce::AudioProcessorValueTreeState apvts;
    geminis::fx::Rack rack;
    geminis::Tuner tuner;
    std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f };
    std::function<void()> onPresetChanged;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioBuffer<float> work;
    std::atomic<int> mode { 0 };
    int currentPreset = 0;
    juce::String presetName { "Sin preset" };
    std::atomic<float>* inputSel = nullptr;
    std::atomic<float>* inGain = nullptr;
    std::atomic<float>* outGain = nullptr;

    JUCE_DECLARE_WEAK_REFERENCEABLE (GeminisFxProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GeminisFxProcessor)
};
