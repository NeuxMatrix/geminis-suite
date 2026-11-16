#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Wavetable.h"
#include "InstParams.h"
#include "../common/DspCommon.h"

namespace geminis::inst
{
/** Sintetizador de Geminis: 2 osciladores de onda curvable o dibujada con unísono, sub, ruido,
    FM y anillo; filtro multimodo con saturación; 3 envolventes, 2 LFO y matriz de 6 modulaciones. */
class SynthEngine
{
public:
    static constexpr int maxVoices = 20;
    static constexpr int maxPolyphony = 16;
    static constexpr int controlRate = 16;

    explicit SynthEngine (juce::AudioProcessorValueTreeState&);
    ~SynthEngine();

    void prepare (double sampleRate, int maxBlock);
    void reset();
    /** Mezcla (suma) el sinte en L y R. */
    void render (float* L, float* R, int numSamples, const juce::MidiBuffer& midi, double bpm);

    /** Hilo de la interfaz: reconstruye las tablas si ha cambiado la onda, la curva o el dibujo. */
    void updateTables();
    void setDrawnWave (int osc, const DrawnWave& w);
    DrawnWave getDrawnWave (int osc) const;
    std::shared_ptr<WaveSet> getWaveSet (int osc) const { return banks[(size_t) osc].current(); }

    void allNotesOff();
    int activeVoices() const noexcept { return numActive.load(); }

private:
    struct Snapshot;
    struct Voice;

    void readParams (Snapshot&) const;
    void handleMidi (const juce::MidiMessage&);
    void noteOn (int note, float velocity);
    void noteOff (int note);
    Voice* findVoiceToUse();

    juce::AudioProcessorValueTreeState& state;
    double sr = 44100.0;

    std::array<WaveBank, 2> banks;
    std::array<DrawnWave, 2> drawn;
    mutable juce::SpinLock drawnLock;
    std::array<int, 2> builtWave { -1, -1 }, builtCurve { -1, -1 };
    std::array<DrawnWave, 2> builtDrawn;

    std::vector<std::unique_ptr<Voice>> voices;
    std::unique_ptr<Snapshot> snap;
    std::array<const WaveSet*, 2> tables { nullptr, nullptr };

    std::vector<int> heldNotes;      // para mono / legato
    bool sustainPedal = false;
    std::array<bool, 128> sustained {};
    float pitchBend = 0.0f, modWheel = 0.0f, aftertouch = 0.0f;
    float lastNotePitch = 60.0f;
    juce::uint32 voiceCounter = 0;
    dsp::Lfo lfo1;
    float lfo1Value = 0.0f;
    std::atomic<int> numActive { 0 };
    juce::Random random;

    // Parámetros (valores reales)
    struct OscParams { std::atomic<float> *wave, *ctype, *curve, *oct, *semi, *fine, *level, *uni, *det; };
    std::array<OscParams, 2> oscP;
    std::atomic<float> *pSub, *pSubShape, *pNoise, *pFm, *pRing, *pFType, *pCut, *pRes, *pDrive, *pFEnv, *pFKey, *pFVel;
    std::array<std::atomic<float>*, 12> pEnv;
    std::array<std::atomic<float>*, 6> pLfo;
    std::array<std::atomic<float>*, numModSlots * 3> pMod;
    std::atomic<float> *pVoiceMode, *pGlide, *pBend, *pVelSens, *pWidth, *pVol;
};
} // namespace geminis::inst
