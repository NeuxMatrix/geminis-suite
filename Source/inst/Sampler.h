#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "SampleLibrary.h"
#include "InstParams.h"
#include "../common/DspCommon.h"

namespace geminis::inst
{
/** Instrumentos: los de muestras CC0 y los modelados (bajo y guitarras con cuerda física). */
struct InstrumentInfo
{
    juce::String name, group, description;
    bool modeled = false;
    int model = 0;          // para los modelados
    float gain = 1.0f;
};

const std::vector<InstrumentInfo>& instrumentList();
const juce::StringArray& instrumentNames();

class SamplerEngine
{
public:
    static constexpr int maxVoices = 24;

    explicit SamplerEngine (juce::AudioProcessorValueTreeState&);
    ~SamplerEngine();

    void prepare (double sampleRate, int maxBlock);
    void reset();
    void render (float* L, float* R, int numSamples, const juce::MidiBuffer& midi);

    /** Hilo de la interfaz: carga las muestras del instrumento elegido si ha cambiado. */
    void updateInstrument();
    int getLoadedInstrument() const noexcept { return loaded.load(); }

private:
    struct Zone { std::shared_ptr<DecodedSample> sample; int lo = 0, hi = 127; };
    struct Set { int instrument = -1; std::vector<Zone> zones; };
    struct Voice;

    void noteOn (int note, float vel);
    void noteOff (int note);

    juce::AudioProcessorValueTreeState& state;
    double sr = 44100.0;
    std::vector<std::unique_ptr<Voice>> voices;

    juce::SpinLock setLock;
    std::shared_ptr<Set> liveSet, audioSet;
    std::vector<std::shared_ptr<Set>> retired;
    std::atomic<int> loaded { -1 };
    juce::uint32 counter = 0;
    bool sustainPedal = false;
    std::array<bool, 128> sustained {};
    float pitchBend = 0.0f;

    std::atomic<float> *pType, *pAttack, *pRelease, *pTone, *pChar, *pTune, *pVel, *pVol;
};
} // namespace geminis::inst
