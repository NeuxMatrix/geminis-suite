#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "InstParams.h"
#include "SynthEngine.h"
#include "SynthPresets.h"
#include "Drums.h"
#include "Sampler.h"
#include "InstFx.h"

/** Geminis Instrument: sintetizador modular, batería por estilos e instrumentos con muestras. */
class GeminisInstrumentProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    using Mode = geminis::inst::Mode;

    GeminisInstrumentProcessor();
    ~GeminisInstrumentProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Geminis Instrument"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==========================================================================
    Mode getMode() const noexcept { return (Mode) mode.load(); }
    void setMode (Mode m);

    void applySynthPreset (int index);
    void applyKit (int index);
    void applyInstrument (int index);

    int getSynthPreset() const noexcept { return synthPreset; }
    int getKit() const noexcept { return kit; }
    juce::String getSynthPresetName() const { return synthPresetName; }
    juce::String getKitName() const { return kitName; }

    /** Sonidos propios del sinte (Documentos/Geminis/Sonidos). */
    bool saveUserSound (const juce::File&);
    bool loadUserSound (const juce::File&);
    static juce::File userSoundFolder();

    /** Pone un parámetro en su valor real avisando al DAW. */
    void setParam (const juce::String& id, float realValue);
    float getParam (const juce::String& id) const;

    juce::AudioProcessorValueTreeState apvts;
    geminis::inst::SynthEngine synth;
    geminis::inst::DrumEngine drums;
    geminis::inst::SamplerEngine sampler;
    geminis::inst::InstFx fx;
    juce::MidiKeyboardState keyboardState;
    std::atomic<float> outputPeak { 0.0f };

    /** Últimas muestras de la salida para el osciloscopio. */
    static constexpr int scopeSize = 2048;
    std::array<float, scopeSize> scope {};
    std::atomic<int> scopeWrite { 0 };
    std::function<void()> onPresetChanged;

private:
    void timerCallback() override;
    void notifyChanged();
    void resetParams (const std::function<bool (const juce::String&)>& which);
    void storeFxSnapshot (Mode);
    bool restoreFxSnapshot (Mode);

    std::atomic<int> mode { 0 };
    int lastAudioMode = 0;
    int synthPreset = -1, kit = -1, currentProgram = 0;
    juce::String synthPresetName, kitName;
    std::array<std::vector<std::pair<juce::String, float>>, 4> fxSnapshots;

    juce::AudioBuffer<float> work;
    juce::SmoothedValue<float> masterGain;
    std::atomic<float>* masterVol = nullptr;

    JUCE_DECLARE_WEAK_REFERENCEABLE (GeminisInstrumentProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GeminisInstrumentProcessor)
};
