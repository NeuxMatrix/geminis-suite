#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace geminis::inst
{
/** Definición de un parámetro de Geminis Instrument (valores reales). */
struct PDef
{
    juce::String id;
    juce::String name;
    float min = 0.0f, max = 1.0f, def = 0.0f, skew = 1.0f;
    juce::String unit;
    juce::StringArray choices;
    bool integer = false;
    bool boolean = false;

    bool isChoice() const { return choices.size() > 0; }
};

enum class Mode { none = 0, synth = 1, drums = 2, instruments = 3 };

constexpr int numPads = 16;
constexpr int numSteps = 32;
constexpr int numModSlots = 6;
constexpr int numPatterns = 4;

// Fuentes y destinos de la matriz de modulación
enum ModSource { srcNone = 0, srcLfo1, srcLfo2, srcEnvFilter, srcEnvMod, srcVelocity, srcModWheel, srcAftertouch, srcNote, srcRandom, numModSources };
enum ModDest { dstNone = 0, dstCurve1, dstCurve2, dstPitch1, dstPitch2, dstPitch, dstLevel1, dstLevel2, dstFm, dstRing, dstCutoff,
               dstResonance, dstVolume, dstPan, dstDetune, dstLfo2Rate, dstSub, dstNoise, numModDests };

const std::vector<PDef>& allParamDefs();
const PDef* findDef (const juce::String& id);
juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

juce::String formatValue (const PDef&, float real);
juce::String padParamId (int pad, const char* what);   // p03_tune ...

/** Divisiones de tiempo para LFO y delay sincronizados. Devuelve segundos (0 = libre). */
double divisionSeconds (int division, double bpm);
const juce::StringArray& divisionNames();
} // namespace geminis::inst
