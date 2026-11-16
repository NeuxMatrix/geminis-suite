#pragma once
#include <juce_core/juce_core.h>
#include <vector>

namespace geminis::inst
{
struct SynthPreset
{
    juce::String name, category;
    std::vector<std::pair<juce::String, float>> values;   // solo lo que cambia respecto al sonido inicial
    int drawn1 = -1, drawn2 = -1;                          // forma dibujada de partida para cada oscilador
};

const std::vector<SynthPreset>& synthPresets();
/** ¿Pertenece este parámetro al sintetizador o a sus efectos? (se reinician al cargar un preset) */
bool isSynthParam (const juce::String& id);
} // namespace geminis::inst
