#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <memory>

namespace geminis::inst
{
/** Tablas de onda con límite de banda. Cada oscilador tiene un juego de 16 "fotogramas"
    que recorren la curva (de 0 a 1) y 11 versiones por fotograma con menos armónicos
    para las notas agudas, así la onda se puede curvar sin que suene áspera. */
constexpr int tableBits = 11;
constexpr int tableSize = 1 << tableBits;       // 2048 muestras por ciclo
constexpr int numFrames = 16;
constexpr int numMips = 11;

enum BaseWave { waveSine = 0, waveTriangle, waveSaw, waveSquare, wavePulse, waveOrgan, waveChoir, waveMetal,
                waveGlass, waveDigital, waveDrawn, numBaseWaves };
enum CurveType { curveBend = 0, curveFold, curveSync, curveFormant, curveBits, numCurveTypes };

const juce::StringArray& baseWaveNames();
const juce::StringArray& curveTypeNames();

/** Onda dibujada por el usuario: 32 puntos (-1..1) unidos con curvas suaves o rectas. */
struct DrawnWave
{
    static constexpr int numPoints = 32;
    std::array<float, numPoints> points {};
    float smooth = 1.0f;        // 0 = escalones, 1 = curva suave

    DrawnWave();
    float valueAt (float phase) const noexcept;   // fase 0..1
    juce::String toString() const;
    static DrawnWave fromString (const juce::String&);
    bool operator== (const DrawnWave& o) const noexcept { return points == o.points && smooth == o.smooth; }

    // Formas de partida para el editor
    static DrawnWave preset (int index);
    static const juce::StringArray& presetNames();
};

/** Valor sin filtrar de la onda base en una fase (0..1); sirve también para dibujarla. */
float baseWaveValue (int wave, float phase, const DrawnWave& drawn) noexcept;
/** Aplica la curva a la onda: devuelve el valor en la fase para una cantidad 0..1. */
float curvedValue (int wave, int curve, float amount, float phase, const DrawnWave& drawn) noexcept;

struct WaveSet
{
    // [fotograma][mip][tableSize + 1]
    std::vector<float> data;
    int wave = 0, curve = 0;
    DrawnWave drawn;

    const float* table (int frame, int mip) const noexcept
    {
        return data.data() + ((size_t) frame * numMips + (size_t) mip) * (size_t) (tableSize + 1);
    }
};

std::shared_ptr<WaveSet> buildWaveSet (int wave, int curve, const DrawnWave& drawn);

/** Elige la versión con armónicos suficientes para una frecuencia sin pasar de Nyquist. */
inline int mipForIncrement (float phaseInc) noexcept
{
    // armónicos permitidos = 0.5 / inc ; el mip m conserva 1024 >> m armónicos
    const float maxHarm = 0.45f / std::max (phaseInc, 1.0e-7f);
    int m = 0;
    float h = 1024.0f;
    while (m < numMips - 1 && h > maxHarm) { h *= 0.5f; ++m; }
    return m;
}

/** Lectura de la tabla con interpolación lineal; `phase` en [0,1). */
inline float readTable (const float* t, float phase) noexcept
{
    const float pos = phase * (float) tableSize;
    const int i = (int) pos;
    const float f = pos - (float) i;
    return t[i] + (t[i + 1] - t[i]) * f;
}

/** Guarda los juegos de tablas en uso y libera los antiguos solo cuando el audio ya no los lee. */
class WaveBank
{
public:
    void publish (std::shared_ptr<WaveSet> s);        // hilo de la interfaz
    const WaveSet* acquire() noexcept;                // hilo de audio, al principio de cada bloque
    void collectGarbage();                            // hilo de la interfaz
    std::shared_ptr<WaveSet> current() const { const juce::SpinLock::ScopedLockType l (lock); return live; }

private:
    mutable juce::SpinLock lock;
    std::shared_ptr<WaveSet> live;
    std::vector<std::pair<std::shared_ptr<WaveSet>, juce::uint32>> retired;
    std::atomic<juce::uint32> audioBlocks { 0 }, publishCount { 0 };
};
} // namespace geminis::inst
