#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "InstParams.h"
#include "SampleLibrary.h"
#include "../common/DspCommon.h"

namespace geminis::inst
{
/** Sonidos sintetizados de la caja de ritmos (después vienen las muestras). */
enum DrumModel { dmKick = 0, dmKickDeep, dmKickHard, dmSnare, dmSnareNoise, dmClap, dmHatClosed, dmHatOpen, dmTom, dmCymbal,
                 dmCowbell, dmRim, dmShaker, dmZap, dmBass808, dmMetal, dmNoise, dmGlitch, numDrumModels };

/** Sonidos sintetizados añadidos después de las muestras (así no cambia el sonido guardado
    en proyectos anteriores): nota pulsada para melodías, acorde y voz. Van en la fila 4. */
enum DrumLateModel { dlPluck = 0, dlChord, dlVoice, numLateDrumModels };

/** Nombres de todos los sonidos: modelos sintetizados, muestras y los sonidos añadidos después. */
const juce::StringArray& drumSoundNames();
/** Número de muestras de batería. */
int numDrumSamples();
/** Índice del primer sonido añadido después de las muestras. */
int firstLateDrumSound();
int drumSoundIndex (const juce::String& name);       // -1 si no existe

/** Patrón: velocidad por paso (0 apagado, 1 suave, 2 normal, 3 acento). */
struct Pattern
{
    std::array<std::array<juce::uint8, numSteps>, numPads> steps {};
    juce::String toString() const;
    static Pattern fromString (const juce::String&);
    bool isEmpty() const;
};

struct PadPreset
{
    juce::String sound;
    float tune = 0, decay = 0.5f, tone = 0.5f, snap = 0.5f, drive = 0, level = 0.75f, pan = 0;
    int choke = 0;
};

struct DrumKit
{
    juce::String name, description;
    double bpm = 100.0;
    float swing = 0.0f;
    std::array<PadPreset, numPads> pads;
    std::array<Pattern, numPatterns> patterns;
    std::vector<std::pair<juce::String, float>> extra;   // otros parámetros (efectos...)
};

const std::vector<DrumKit>& drumKits();
/** Nombre corto de cada pad según su papel en la distribución de Maschine:
    fila 1 bombo, caja y charles; fila 2 refuerzos y percusión ligera; fila 3 toms y
    percusión secundaria; fila 4 melodía, acorde, voz y efecto. */
const juce::StringArray& padRoleNames();
/** Nota MIDI de un pad en el mapa elegido (0 Maschine, 1 General MIDI). */
int padToNote (int pad, int map);
/** Pad que corresponde a una nota (o -1). */
int noteToPad (int note, int map);

class DrumEngine
{
public:
    explicit DrumEngine (juce::AudioProcessorValueTreeState&);
    ~DrumEngine();

    void prepare (double sampleRate, int maxBlock);
    void reset();
    void render (float* L, float* R, int numSamples, const juce::MidiBuffer& midi, juce::AudioPlayHead* playHead);

    /** Carga las muestras de batería (hilo de la interfaz, una vez). */
    void loadSamples();

    // Interfaz
    void triggerFromUi (int pad, float velocity);                 // cualquier hilo
    Pattern getPattern (int index) const;
    void setPattern (int index, const Pattern&);
    void setStep (int pattern, int pad, int step, int value);
    std::atomic<int> currentStep { -1 };
    std::array<std::atomic<juce::uint32>, numPads> hitCount {};

    /** Escribe el patrón como archivo MIDI (un compás o dos). */
    juce::File exportMidi (int pattern, const juce::String& kitName) const;

private:
    struct Voice;
    void trigger (int pad, float velocity, int offset);
    void renderVoices (float* L, float* R, int start, int n);

    juce::AudioProcessorValueTreeState& state;
    double sr = 44100.0;
    std::vector<std::unique_ptr<Voice>> voices;
    std::vector<std::shared_ptr<DecodedSample>> samples;     // índice = sonido - numDrumModels
    std::atomic<bool> samplesReady { false };

    mutable juce::SpinLock patternLock;
    std::array<Pattern, numPatterns> patterns;

    juce::AbstractFifo uiFifo { 64 };
    std::array<std::pair<int, float>, 64> uiEvents {};

    double internalPpq = 0.0;
    bool wasPlaying = false;
    juce::uint32 counter = 0;

    struct PadParams { std::atomic<float> *snd, *tune, *decay, *tone, *snap, *drive, *level, *pan, *choke; };
    std::array<PadParams, numPads> padP;
    std::atomic<float> *seqMode, *seqPattern, *seqSwing, *seqLen, *seqBpm, *midiMap, *volume;
};
} // namespace geminis::inst
