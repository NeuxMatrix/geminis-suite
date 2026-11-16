#pragma once
#include <cmath>
#include <vector>
#include <algorithm>
#include <cstdint>

/** Piezas de audio reutilizables por todos los módulos de Geminis. */
namespace geminis::dsp
{
constexpr float pi = 3.14159265358979f;
constexpr float twoPi = 6.28318530717959f;

inline float dbToGain (float dB) noexcept { return std::pow (10.0f, dB * 0.05f); }
inline float gainToDb (float g) noexcept  { return 20.0f * std::log10 (std::max (g, 1.0e-6f)); }
inline float clampf (float x, float lo, float hi) noexcept { return std::min (hi, std::max (lo, x)); }
inline float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }

/** tanh rápida (aproximación racional), suficiente para saturación. */
inline float fastTanh (float x) noexcept
{
    x = clampf (x, -4.97f, 4.97f);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

inline float midiToHz (float note) noexcept { return 440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f); }

//==============================================================================
/** Filtro de segundo orden (fórmulas RBJ), forma transpuesta II. */
struct Biquad
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    void reset() noexcept { z1 = z2 = 0.0f; }

    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setLowpass (double sr, double hz, double q = 0.7071)   { set (sr, hz, q, 0.0, 0); }
    void setHighpass (double sr, double hz, double q = 0.7071)  { set (sr, hz, q, 0.0, 1); }
    void setPeak (double sr, double hz, double q, double dB)    { set (sr, hz, q, dB, 2); }
    void setLowShelf (double sr, double hz, double dB)          { set (sr, hz, 0.7071, dB, 3); }
    void setHighShelf (double sr, double hz, double dB)         { set (sr, hz, 0.7071, dB, 4); }
    void setBandpass (double sr, double hz, double q)           { set (sr, hz, q, 0.0, 5); }
    void setNotch (double sr, double hz, double q)              { set (sr, hz, q, 0.0, 6); }
    void setAllpass (double sr, double hz, double q)            { set (sr, hz, q, 0.0, 7); }

private:
    void set (double sr, double hz, double q, double dB, int type)
    {
        const double piD = 3.14159265358979323846;
        hz = std::fmin (std::fmax (hz, 10.0), sr * 0.45);
        q = std::fmax (q, 0.05);
        const double w = 2.0 * piD * hz / sr;
        const double cs = std::cos (w), sn = std::sin (w);
        const double alpha = sn / (2.0 * q);
        const double A = std::pow (10.0, dB / 40.0);
        double B0 = 1, B1 = 0, B2 = 0, A0 = 1, A1 = 0, A2 = 0;

        switch (type)
        {
            case 0: B0 = (1 - cs) / 2; B1 = 1 - cs; B2 = (1 - cs) / 2; A0 = 1 + alpha; A1 = -2 * cs; A2 = 1 - alpha; break;
            case 1: B0 = (1 + cs) / 2; B1 = -(1 + cs); B2 = (1 + cs) / 2; A0 = 1 + alpha; A1 = -2 * cs; A2 = 1 - alpha; break;
            case 2: B0 = 1 + alpha * A; B1 = -2 * cs; B2 = 1 - alpha * A; A0 = 1 + alpha / A; A1 = -2 * cs; A2 = 1 - alpha / A; break;
            case 3:
            {
                const double s = 2.0 * std::sqrt (A) * alpha;
                B0 = A * ((A + 1) - (A - 1) * cs + s); B1 = 2 * A * ((A - 1) - (A + 1) * cs); B2 = A * ((A + 1) - (A - 1) * cs - s);
                A0 = (A + 1) + (A - 1) * cs + s;       A1 = -2 * ((A - 1) + (A + 1) * cs);    A2 = (A + 1) + (A - 1) * cs - s;
                break;
            }
            case 4:
            {
                const double s = 2.0 * std::sqrt (A) * alpha;
                B0 = A * ((A + 1) + (A - 1) * cs + s); B1 = -2 * A * ((A - 1) + (A + 1) * cs); B2 = A * ((A + 1) + (A - 1) * cs - s);
                A0 = (A + 1) - (A - 1) * cs + s;       A1 = 2 * ((A - 1) - (A + 1) * cs);      A2 = (A + 1) - (A - 1) * cs - s;
                break;
            }
            case 5: B0 = alpha; B1 = 0; B2 = -alpha; A0 = 1 + alpha; A1 = -2 * cs; A2 = 1 - alpha; break;
            case 6: B0 = 1; B1 = -2 * cs; B2 = 1; A0 = 1 + alpha; A1 = -2 * cs; A2 = 1 - alpha; break;
            default: B0 = 1 - alpha; B1 = -2 * cs; B2 = 1 + alpha; A0 = 1 + alpha; A1 = -2 * cs; A2 = 1 - alpha; break;
        }

        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
        a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }
};

//==============================================================================
/** Filtro de un polo. */
struct OnePole
{
    float a = 0.0f, z = 0.0f;
    void setCutoff (double sr, double hz) noexcept { a = (float) std::exp (-2.0 * 3.14159265358979 * std::fmax (hz, 1.0) / sr); }
    void reset() noexcept { z = 0.0f; }
    inline float lowpass (float x) noexcept  { z = x + a * (z - x); return z; }
    inline float highpass (float x) noexcept { return x - lowpass (x); }
};

/** Filtro de estado variable con topología sin retardo (estable al modularlo). */
struct Svf
{
    float g = 0.0f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float ic1 = 0.0f, ic2 = 0.0f;

    struct Out { float lp, bp, hp; };

    void reset() noexcept { ic1 = ic2 = 0.0f; }

    void set (double sr, double hz, double q) noexcept
    {
        hz = std::fmin (std::fmax (hz, 15.0), sr * 0.48);
        g = (float) std::tan (3.14159265358979 * hz / sr);
        k = (float) (1.0 / std::fmax (q, 0.3));
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    inline Out process (float x) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return { v2, v1, x - k * v1 - v2 };
    }
};

//==============================================================================
/** Línea de retardo circular con lectura fraccional (interpolación cúbica). */
class DelayBuffer
{
public:
    void prepare (int maxSamples)
    {
        int size = 1;
        while (size < maxSamples + 4) size <<= 1;
        buf.assign ((size_t) size, 0.0f);
        mask = size - 1;
        w = 0;
    }

    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    int capacity() const noexcept { return mask - 3; }

    inline void push (float x) noexcept { buf[(size_t) w] = x; w = (w + 1) & mask; }

    /** Lee la muestra de hace `d` muestras (d >= 1). */
    inline float read (float d) const noexcept
    {
        d = clampf (d, 1.0f, (float) (mask - 3));
        const float pos = (float) w - d;
        const int i = (int) std::floor (pos);
        const float f = pos - (float) i;
        const float ym1 = buf[(size_t) ((i - 1) & mask)], y0 = buf[(size_t) (i & mask)];
        const float y1 = buf[(size_t) ((i + 1) & mask)], y2 = buf[(size_t) ((i + 2) & mask)];
        const float c1 = 0.5f * (y1 - ym1);
        const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * f + c2) * f + c1) * f + y0;
    }

    inline float readInt (int d) const noexcept { return buf[(size_t) ((w - d) & mask)]; }

private:
    std::vector<float> buf { std::vector<float> (8, 0.0f) };
    int mask = 7, w = 0;
};

//==============================================================================
/** Oscilador lento de modulación. */
struct Lfo
{
    enum Shape { sine = 0, triangle, saw, square, randomStep, randomSmooth };
    double phase = 0.0;
    float held = 0.0f, prevHeld = 0.0f;
    uint32_t seed = 12345u;

    void reset (double p = 0.0) noexcept { phase = p; }

    float nextRandom() noexcept
    {
        seed = seed * 1664525u + 1013904223u;
        return (float) (seed >> 8) / 8388608.0f - 1.0f;
    }

    /** Devuelve un valor en [-1, 1] y avanza `inc` ciclos. */
    inline float next (double inc, int shape) noexcept
    {
        const float p = (float) phase;
        float v = 0.0f;
        switch (shape)
        {
            case sine:     v = std::sin (twoPi * p); break;
            case triangle: v = 1.0f - 4.0f * std::abs (p - 0.5f); break;
            case saw:      v = 2.0f * p - 1.0f; break;
            case square:   v = p < 0.5f ? 1.0f : -1.0f; break;
            case randomStep: v = held; break;
            default:       v = prevHeld + (held - prevHeld) * (0.5f - 0.5f * std::cos (pi * p)); break;
        }
        phase += inc;
        if (phase >= 1.0)
        {
            phase -= std::floor (phase);
            prevHeld = held;
            held = nextRandom();
        }
        return v;
    }
};

//==============================================================================
/** Seguidor de envolvente con ataque y liberación independientes. */
struct EnvelopeFollower
{
    float att = 0.0f, rel = 0.0f, env = 0.0f;
    void set (double sr, double attackMs, double releaseMs) noexcept
    {
        att = (float) std::exp (-1.0 / (std::fmax (attackMs, 0.01) * 0.001 * sr));
        rel = (float) std::exp (-1.0 / (std::fmax (releaseMs, 0.01) * 0.001 * sr));
    }
    void reset() noexcept { env = 0.0f; }
    inline float process (float x) noexcept
    {
        const float a = std::abs (x);
        env = a > env ? a + att * (env - a) : a + rel * (env - a);
        return env;
    }
};

//==============================================================================
/** Desplazador de tono granular de dos cabezas. ratio = 2 sube una octava, -1 invierte. */
class GrainShifter
{
public:
    void prepare (double sampleRate, float maxWindowMs = 2100.0f)
    {
        sr = sampleRate;
        buf.prepare ((int) (sr * maxWindowMs * 0.001) + 16);
        phase = 0.0f;
    }

    void clear() { buf.clear(); phase = 0.0f; }

    inline float process (float x, float ratio, float windowMs) noexcept
    {
        buf.push (x);
        const float window = clampf (windowMs * 0.001f * (float) sr, 64.0f, (float) buf.capacity() - 8.0f);
        phase += (1.0f - ratio) / window;
        phase -= std::floor (phase);
        const float p2 = phase + 0.5f - (phase >= 0.5f ? 1.0f : 0.0f);
        const float g1 = std::sin (pi * phase), g2 = std::sin (pi * p2);
        return buf.read (1.0f + phase * window) * g1 * g1 + buf.read (1.0f + p2 * window) * g2 * g2;
    }

private:
    DelayBuffer buf;
    double sr = 44100.0;
    float phase = 0.0f;
};

//==============================================================================
/** Generador de ruido blanco barato. */
struct Noise
{
    uint32_t s = 22222u;
    inline float next() noexcept
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float) (int32_t) s * (1.0f / 2147483648.0f);
    }
};

/** Suavizado lineal por muestra hacia un objetivo. */
struct Ramp
{
    float current = 0.0f, target = 0.0f, step = 0.0f;
    int left = 0;
    void setImmediate (float v) noexcept { current = target = v; left = 0; }
    void setTarget (float v, int samples) noexcept
    {
        if (v == target) return;
        target = v;
        left = std::max (1, samples);
        step = (target - current) / (float) left;
    }
    inline float next() noexcept
    {
        if (left > 0) { current += step; if (--left == 0) current = target; }
        return current;
    }
};
} // namespace geminis::dsp
