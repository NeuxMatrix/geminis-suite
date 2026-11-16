#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>

namespace geminis::inst
{
/** Muestras CC0 incluidas en el plugin (ver tools/make_samples.py y Assets/samples). */
struct SampleEntry
{
    const char* group;      // instrumento ("piano", "cuerdas"...) o "drum"
    const char* resource;   // nombre del recurso binario
    int root;               // nota MIDI de la muestra
    int loopStart;          // inicio del bucle en muestras, -1 si no tiene
};

const std::vector<SampleEntry>& sampleManifest();

struct DecodedSample
{
    juce::AudioBuffer<float> data;   // mono
    double sampleRate = 44100.0;
    int root = 60;
    int loopStart = -1;
    juce::String name;
};

/** Decodifica una muestra (OGG) del binario. Hilo de la interfaz o de carga. */
std::shared_ptr<DecodedSample> decodeSample (const SampleEntry&);

/** Nombre corto de una muestra de batería para mostrar ("caja_acustica" -> "Caja acústica"). */
juce::String drumSampleDisplayName (const juce::String& resource);
} // namespace geminis::inst
