#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../common/DspCommon.h"

namespace geminis::fx
{
/** Información del host que algunos módulos necesitan (tempo para sincronizar). */
struct Context
{
    double sampleRate = 44100.0;
    double bpm = 120.0;
    bool stereo = false;   // false: L y R llevan la misma señal y basta con procesar L
};

enum ModeMask { forGuitar = 1, forVoice = 2, forBoth = 3 };

/** Descripción de un control de un módulo. Los valores viajan normalizados (0..1)
    en los parámetros del host y se convierten aquí a su unidad real. */
struct ParamSpec
{
    const char* name = "";
    float min = 0.0f, max = 1.0f, def = 0.5f, skew = 1.0f;
    const char* unit = "";
    std::vector<const char*> choices;
    bool bipolar = false;

    bool isChoice() const noexcept { return ! choices.empty(); }
    float toReal (float norm) const;
    float toNorm (float real) const;
    juce::String format (float real) const;
};

class Module
{
public:
    virtual ~Module() = default;
    virtual void prepare (double sampleRate, int maxBlock) = 0;
    virtual void reset() {}
    /** Procesa en el sitio. `v` son los valores reales de los controles del módulo. */
    virtual void process (float* L, float* R, int n, const float* v, Context& ctx) = 0;
    virtual int getLatency() const { return 0; }
    virtual void setUserIr (const juce::File&) {}
};

struct ModuleInfo
{
    const char* name;
    const char* category;
    int modes;
    juce::uint32 colour;
    const char* description;
    std::vector<ParamSpec> params;
    std::function<std::unique_ptr<Module>()> create;

    juce::Colour getColour() const { return juce::Colour (colour); }
};

/** Tipos de módulo. El orden es fijo: se guarda en las sesiones y presets. */
enum Type : int
{
    empty = 0,
    gate, compressor, limiter,
    overdrive, distortion, fuzz, booster, octaver,
    wah, eq5,
    amp, cab,
    chorus, flanger, phaser, vibrato, tremolo, rotary,
    delay, reverb,
    volume,
    vocalEq, deesser, tape, doubler, autotune, lofi,
    numTypes
};

const std::vector<ModuleInfo>& catalog();
const ModuleInfo& info (int type);

/** Categorías en el orden en que aparecen en el navegador. */
const juce::StringArray& categories();

/** Divisiones de tempo para delays y trémolos sincronizados. */
double syncedSeconds (int division, double bpm);
} // namespace geminis::fx
