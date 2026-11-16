#include "Wavetable.h"

namespace geminis::inst
{
namespace
{
    constexpr float twoPi = juce::MathConstants<float>::twoPi;
    inline float frac (float x) noexcept { return x - std::floor (x); }
}

const juce::StringArray& baseWaveNames()
{
    static const juce::StringArray names { juce::String::fromUTF8 ("Seno"), juce::String::fromUTF8 ("Triángulo"), "Sierra", "Cuadrada",
                                           "Pulso", juce::String::fromUTF8 ("Órgano"), "Coro", juce::String::fromUTF8 ("Metálica"),
                                           "Cristal", "Digital", "Dibujada" };
    return names;
}

const juce::StringArray& curveTypeNames()
{
    static const juce::StringArray names { "Curvar", "Plegar", "Sincro", "Formante", "Bits" };
    return names;
}

//==============================================================================
DrawnWave::DrawnWave()
{
    for (int i = 0; i < numPoints; ++i)
        points[(size_t) i] = std::sin (twoPi * (float) i / (float) numPoints);
}

float DrawnWave::valueAt (float phase) const noexcept
{
    const float pos = frac (phase) * (float) numPoints;
    const int i1 = (int) pos;
    const float t = pos - (float) i1;
    auto p = [this] (int i) { return points[(size_t) ((i + numPoints) % numPoints)]; };
    const float y0 = p (i1 - 1), y1 = p (i1), y2 = p (i1 + 1), y3 = p (i1 + 2);
    // Catmull-Rom cíclica
    const float curve = 0.5f * ((2.0f * y1) + (-y0 + y2) * t + (2.0f * y0 - 5.0f * y1 + 4.0f * y2 - y3) * t * t
                                + (-y0 + 3.0f * y1 - 3.0f * y2 + y3) * t * t * t);
    const float linear = y1 + (y2 - y1) * t;
    const float step = y1;
    const float s = juce::jlimit (0.0f, 1.0f, smooth);
    const float v = s < 0.5f ? step + (linear - step) * (s * 2.0f) : linear + (curve - linear) * ((s - 0.5f) * 2.0f);
    return juce::jlimit (-1.2f, 1.2f, v);
}

juce::String DrawnWave::toString() const
{
    juce::StringArray parts;
    for (auto v : points) parts.add (juce::String (v, 4));
    parts.add (juce::String (smooth, 3));
    return parts.joinIntoString (",");
}

DrawnWave DrawnWave::fromString (const juce::String& s)
{
    DrawnWave d;
    juce::StringArray parts;
    parts.addTokens (s, ",", {});
    if (parts.size() >= numPoints)
    {
        for (int i = 0; i < numPoints; ++i)
            d.points[(size_t) i] = juce::jlimit (-1.0f, 1.0f, parts[i].getFloatValue());
        if (parts.size() > numPoints) d.smooth = juce::jlimit (0.0f, 1.0f, parts[numPoints].getFloatValue());
    }
    return d;
}

const juce::StringArray& DrawnWave::presetNames()
{
    static const juce::StringArray names { "Seno", "Montañas", "Doble pico", "Escalera", "Aleta", "Latido", "Rizo", "Caos" };
    return names;
}

DrawnWave DrawnWave::preset (int index)
{
    DrawnWave d;
    juce::Random rng (77);
    for (int i = 0; i < numPoints; ++i)
    {
        const float p = (float) i / (float) numPoints;
        float v = 0.0f;
        switch (index)
        {
            case 1:  v = std::sin (twoPi * p) * 0.6f + std::sin (twoPi * 3.0f * p) * 0.4f; break;                 // montañas
            case 2:  v = std::sin (twoPi * p) * std::abs (std::sin (twoPi * 2.0f * p)) * 1.4f; break;            // doble pico
            case 3:  v = std::floor ((1.0f - 2.0f * p) * 4.0f + 0.5f) / 4.0f; d.smooth = 0.0f; break;            // escalera
            case 4:  v = p < 0.3f ? p / 0.3f : 1.0f - 2.0f * (p - 0.3f) / 0.7f; break;                          // aleta
            case 5:  v = std::exp (-p * 9.0f) * std::sin (twoPi * 6.0f * p) * 1.3f; break;                       // latido
            case 6:  v = std::sin (twoPi * p + 2.5f * std::sin (twoPi * 2.0f * p)); break;                      // rizo
            case 7:  v = rng.nextFloat() * 2.0f - 1.0f; d.smooth = 0.7f; break;                                 // caos
            default: v = std::sin (twoPi * p); break;
        }
        d.points[(size_t) i] = juce::jlimit (-1.0f, 1.0f, v);
    }
    return d;
}

//==============================================================================
float baseWaveValue (int wave, float p, const DrawnWave& drawn) noexcept
{
    p = frac (p);
    switch (wave)
    {
        case waveSine:     return std::sin (twoPi * p);
        case waveTriangle: { const float t = frac (p + 0.25f); return 1.0f - 4.0f * std::abs (t - 0.5f); }
        case waveSaw:      return 1.0f - 2.0f * p;
        case waveSquare:   return p < 0.5f ? 1.0f : -1.0f;
        case wavePulse:    return p < 0.18f ? 1.0f : -0.4f;
        case waveOrgan:
        {
            static const float amp[] = { 1.0f, 0.75f, 0.6f, 0.5f, 0.0f, 0.3f, 0.0f, 0.25f };
            float v = 0.0f;
            for (int h = 0; h < 8; ++h) v += amp[h] * std::sin (twoPi * (float) (h + 1) * p);
            return v * 0.45f;
        }
        case waveChoir:
        {
            // Armónicos con dos formantes, como una vocal "a"
            float v = 0.0f;
            for (int h = 1; h <= 40; ++h)
            {
                const float fh = (float) h;
                const float a = std::exp (-0.5f * std::pow ((fh - 5.5f) / 1.8f, 2.0f)) + 0.6f * std::exp (-0.5f * std::pow ((fh - 9.0f) / 2.0f, 2.0f))
                              + 0.25f * std::exp (-0.5f * std::pow ((fh - 19.0f) / 3.0f, 2.0f)) + 0.35f / fh;
                v += a * std::sin (twoPi * fh * p + (float) h * 0.7f);
            }
            return v * 0.35f;
        }
        case waveMetal:
        {
            static const int harm[] = { 1, 3, 7, 11, 13, 17, 23, 29 };
            float v = 0.0f;
            for (int k = 0; k < 8; ++k) v += std::sin (twoPi * (float) harm[k] * p + (float) k) / std::sqrt ((float) harm[k]);
            return v * 0.4f;
        }
        case waveGlass:
            return std::sin (twoPi * p) * 0.7f + std::sin (twoPi * 4.0f * p) * 0.35f + std::sin (twoPi * 7.0f * p) * 0.2f
                   + std::sin (twoPi * 11.0f * p) * 0.1f;
        case waveDigital:
            return std::floor ((1.0f - 2.0f * p) * 6.0f) / 6.0f * 0.7f + std::sin (twoPi * 2.0f * p) * 0.3f;
        case waveDrawn:
        default:
            return drawn.valueAt (p);
    }
}

float curvedValue (int wave, int curve, float a, float p, const DrawnWave& drawn) noexcept
{
    a = juce::jlimit (0.0f, 1.0f, a);
    p = frac (p);
    switch (curve)
    {
        case curveBend:
        {
            // Distorsión de fase: la onda se "dobla" hacia un lado
            // Recorrido exponencial: el cambio de timbre se reparte por todo el giro del mando
            const float d = 0.5f * std::pow (0.04f, a);
            const float q = p < d ? 0.5f * p / d : 0.5f + 0.5f * (p - d) / (1.0f - d);
            return baseWaveValue (wave, q, drawn);
        }
        case curveFold:
        {
            // Un poco de seno mezclado para que las ondas planas (cuadrada, pulso) también se plieguen
            const float y = (baseWaveValue (wave, p, drawn) * (1.0f - 0.3f * a) + 0.3f * a * std::sin (twoPi * p)) * (1.0f + 5.0f * a);
            const float t = frac ((y + 1.0f) * 0.25f);
            return 1.0f - 4.0f * std::abs (t - 0.5f);
        }
        case curveSync:
            return baseWaveValue (wave, frac (p * (1.0f + 7.0f * a)), drawn);
        case curveFormant:
        {
            const float w = juce::jmin (1.0f, a * 4.0f);
            const float hann = 0.5f - 0.5f * std::cos (twoPi * p);
            return baseWaveValue (wave, frac (p * (1.0f + 7.0f * a)), drawn) * (1.0f + (hann - 1.0f) * w);
        }
        case curveBits:
        default:
        {
            if (a < 0.01f) return baseWaveValue (wave, p, drawn);
            const float steps = 8.0f + std::pow (1.0f - a, 2.0f) * 248.0f;
            const float q = std::floor (p * steps) / steps;
            const float levels = 1.0f + std::pow (1.0f - a, 3.0f) * 31.0f;
            // Cuantización sin nivel cero: con 1 bit queda una onda cuadrada, nunca silencio
            return (std::floor (baseWaveValue (wave, q, drawn) * levels) + 0.5f) / levels;
        }
    }
}

//==============================================================================
std::shared_ptr<WaveSet> buildWaveSet (int wave, int curve, const DrawnWave& drawn)
{
    auto set = std::make_shared<WaveSet>();
    set->wave = wave;
    set->curve = curve;
    set->drawn = drawn;
    set->data.assign ((size_t) numFrames * numMips * (tableSize + 1), 0.0f);

    constexpr int overOrder = tableBits + 2;          // 4 veces más puntos para medir los armónicos
    constexpr int overSize = 1 << overOrder;
    juce::dsp::FFT forward (overOrder), inverse (tableBits);
    std::vector<juce::dsp::Complex<float>> timeBig ((size_t) overSize), specBig ((size_t) overSize);
    std::vector<juce::dsp::Complex<float>> spec ((size_t) tableSize), out ((size_t) tableSize);

    for (int f = 0; f < numFrames; ++f)
    {
        const float amount = (float) f / (float) (numFrames - 1);
        for (int i = 0; i < overSize; ++i)
            timeBig[(size_t) i] = { curvedValue (wave, curve, amount, (float) i / (float) overSize, drawn), 0.0f };
        forward.perform (timeBig.data(), specBig.data(), false);

        float scale = 1.0f;
        for (int m = 0; m < numMips; ++m)
        {
            const int maxHarm = juce::jmin (tableSize / 2 - 1, 1024 >> m);
            std::fill (spec.begin(), spec.end(), juce::dsp::Complex<float> { 0.0f, 0.0f });
            for (int k = 1; k <= maxHarm; ++k)
            {
                // Atenuación suave de los últimos armónicos para evitar el efecto Gibbs
                const float fade = k > maxHarm * 3 / 4 ? 0.5f + 0.5f * std::cos (juce::MathConstants<float>::pi * (float) (k - maxHarm * 3 / 4) / (float) (maxHarm / 4 + 1)) : 1.0f;
                spec[(size_t) k] = specBig[(size_t) k] * fade;
                spec[(size_t) (tableSize - k)] = std::conj (specBig[(size_t) k]) * fade;
            }
            inverse.perform (spec.data(), out.data(), true);

            float* t = set->data.data() + ((size_t) f * numMips + (size_t) m) * (size_t) (tableSize + 1);
            for (int i = 0; i < tableSize; ++i) t[i] = out[(size_t) i].real();
            if (m == 0)
            {
                float peak = 0.0f;
                for (int i = 0; i < tableSize; ++i) peak = std::max (peak, std::abs (t[i]));
                scale = peak > 1.0e-6f ? 0.9f / peak : 0.0f;
            }
            for (int i = 0; i < tableSize; ++i) t[i] *= scale;
            t[tableSize] = t[0];
        }
    }
    return set;
}

//==============================================================================
void WaveBank::publish (std::shared_ptr<WaveSet> s)
{
    const juce::SpinLock::ScopedLockType l (lock);
    if (live != nullptr)
        retired.emplace_back (live, audioBlocks.load());
    live = std::move (s);
    ++publishCount;
}

const WaveSet* WaveBank::acquire() noexcept
{
    const WaveSet* p = nullptr;
    {
        const juce::SpinLock::ScopedTryLockType l (lock);
        if (l.isLocked()) p = live.get();
    }
    ++audioBlocks;
    return p;
}

void WaveBank::collectGarbage()
{
    const juce::SpinLock::ScopedLockType l (lock);
    const auto now = audioBlocks.load();
    retired.erase (std::remove_if (retired.begin(), retired.end(),
                                   [now] (const auto& r) { return now >= r.second + 3; }),
                   retired.end());
    // Si el audio está parado mucho tiempo, no acumular: se conservan solo los últimos.
    while (retired.size() > 24) retired.erase (retired.begin());
}
} // namespace geminis::inst
