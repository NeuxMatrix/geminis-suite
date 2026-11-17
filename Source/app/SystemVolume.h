#pragma once
#include <juce_core/juce_core.h>

namespace geminis::app
{
/** Volumen y silencio de la salida de Windows: lo que cambian las teclas de volumen y de silencio.
    Con ASIO, Windows no toca el sonido del programa, así que el programa lo aplica él mismo. */
struct SystemVolume
{
    bool available = false;   // false fuera de Windows o si no hay salida por defecto
    float gain = 1.0f;        // ganancia lineal del volumen de Windows (0 con el volumen a cero)
    bool muted = false;
};

/** Lee el volumen de la salida por defecto de Windows. Barato: se puede llamar varias veces por segundo
    desde el hilo de la interfaz. */
SystemVolume readSystemVolume();

/** Tipos de dispositivo que se saltan el volumen de Windows y en los que el programa lo aplica. */
inline bool deviceTypeBypassesSystemVolume (const juce::String& typeName) { return typeName == "ASIO"; }

/** Ganancia de salida del programa: el volumen de Windows con ASIO (si está activado), si no 1. */
inline float outputGainFor (const juce::String& typeName, bool followSystem, const SystemVolume& v)
{
    if (! followSystem || ! v.available || ! deviceTypeBypassesSystemVolume (typeName)) return 1.0f;
    return v.muted ? 0.0f : juce::jlimit (0.0f, 1.0f, v.gain);
}
} // namespace geminis::app
