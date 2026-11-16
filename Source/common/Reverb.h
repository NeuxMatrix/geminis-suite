#pragma once
#include "DspCommon.h"
#include <array>

namespace geminis::dsp
{
/** Reverb de red de retardos con realimentación (8 líneas) y varios tipos.
    Diseño propio: difusión de entrada, matriz de Householder, amortiguación
    por línea, modulación lenta, muelle por cadena de pasatodos y shimmer. */
class GeminisReverb
{
public:
    enum Type { room = 0, hall, plate, spring, cathedral, shimmer, numTypes };

    struct Params
    {
        int type = hall;
        float decay = 0.5f;      // 0..1
        float preDelayMs = 10.0f;
        float tone = 0.5f;       // 0 oscuro .. 1 brillante
        float mix = 0.25f;
        float width = 1.0f;
        float modulation = 0.3f;
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& l : lines) l.prepare ((int) (sr * 0.32) + 64);
        for (auto& d : diffusers) d.prepare ((int) (sr * 0.03) + 16);
        preDelay.prepare ((int) (sr * 0.25) + 16);
        springLine.prepare ((int) (sr * 0.08) + 16);
        shifter.prepare (sr, 200.0f);
        reset();
    }

    void reset()
    {
        for (auto& l : lines) l.clear();
        for (auto& d : diffusers) d.clear();
        preDelay.clear();
        springLine.clear();
        shifter.clear();
        for (auto& f : damp) f.reset();
        for (auto& a : springAp) a = {};
        springLp.reset();
        springHp.reset();
        shimmerFb = 0.0f;
        lastType = -1;
    }

    void process (float* L, float* R, int n, const Params& p)
    {
        if (p.type != lastType) configure (p.type);

        const auto& c = cfg;
        const float rt60 = c.minRt + (c.maxRt - c.minRt) * p.decay * p.decay;
        for (int i = 0; i < 8; ++i)
        {
            const float lenSec = c.size * baseMs[(size_t) i] * 0.001f;
            gains[(size_t) i] = std::pow (10.0f, -3.0f * lenSec / rt60);
            lens[(size_t) i] = lenSec * (float) sr;
        }
        const float dampHz = 1800.0f + 14000.0f * p.tone * p.tone * c.brightness;
        for (auto& f : damp) f.setCutoff (sr, dampHz);
        const float pre = clampf (p.preDelayMs, 0.0f, 240.0f) * 0.001f * (float) sr;
        const float modDepth = (6.0f + 18.0f * p.modulation) * c.modScale;
        const double modInc = 0.37 / sr;

        springLp.setCutoff (sr, 3800.0 + 2000.0 * p.tone);
        springHp.setCutoff (sr, 180.0);

        const float wet = p.mix, dry = 1.0f - 0.5f * p.mix * p.mix;
        const float w = clampf (p.width, 0.0f, 1.0f);

        for (int s = 0; s < n; ++s)
        {
            const float inL = L[s], inR = R[s];
            float x = 0.5f * (inL + inR);

            preDelay.push (x);
            x = pre >= 1.0f ? preDelay.read (pre) : x;

            if (cfg.isShimmer)
                x += shimmerFb;

            // Difusión de entrada
            for (int d = 0; d < 4; ++d)
                x = diffuse (diffusers[(size_t) d], x, diffLen[(size_t) d] * c.size, 0.62f * c.diffusion);

            if (cfg.isSpring)
                x = springStage (x, p);

            // Red de retardos
            const float m1 = (float) std::sin (twoPi * modPhase) * modDepth;
            const float m2 = (float) std::sin (twoPi * (modPhase + 0.25)) * modDepth;
            modPhase += modInc;
            if (modPhase >= 1.0) modPhase -= 1.0;

            std::array<float, 8> o {};
            for (int i = 0; i < 8; ++i)
            {
                float d = lens[(size_t) i];
                if (i == 1) d += m1;
                if (i == 6) d += m2;
                o[(size_t) i] = damp[(size_t) i].lowpass (lines[(size_t) i].read (d)) * gains[(size_t) i];
            }

            float sum = 0.0f;
            for (float v : o) sum += v;
            sum *= 0.25f; // 2/N

            for (int i = 0; i < 8; ++i)
                lines[(size_t) i].push (x + o[(size_t) i] - sum);

            float outL = o[0] - o[2] + o[4] - o[6] + 0.5f * (o[1] + o[5]);
            float outR = o[1] - o[3] + o[5] - o[7] + 0.5f * (o[2] + o[6]);
            outL *= 0.45f;
            outR *= 0.45f;

            if (cfg.isShimmer)
                shimmerFb = fastTanh (shifter.process (0.5f * (outL + outR), 2.0f, 120.0f) * 0.55f * (0.4f + 0.6f * p.decay));

            const float mid = 0.5f * (outL + outR), side = 0.5f * (outL - outR) * w;
            L[s] = inL * dry + (mid + side) * wet;
            R[s] = inR * dry + (mid - side) * wet;
        }
    }

private:
    struct Config
    {
        float size, minRt, maxRt, diffusion, brightness, modScale;
        bool isSpring, isShimmer;
    };

    struct Allpass1 { float a = 0.0f, z = 0.0f; };

    void configure (int type)
    {
        lastType = type;
        switch (type)
        {
            case room:      cfg = { 0.45f, 0.25f, 1.6f, 0.8f, 0.9f, 0.4f, false, false }; break;
            case plate:     cfg = { 0.70f, 0.6f, 5.0f, 1.0f, 1.0f, 0.6f, false, false }; break;
            case spring:    cfg = { 0.55f, 0.6f, 3.0f, 0.6f, 0.75f, 0.3f, true, false }; break;
            case cathedral: cfg = { 1.60f, 2.5f, 12.0f, 0.9f, 0.8f, 1.0f, false, false }; break;
            case shimmer:   cfg = { 1.30f, 2.0f, 10.0f, 0.9f, 1.0f, 1.0f, false, true }; break;
            default:        cfg = { 1.00f, 1.0f, 7.0f, 0.85f, 0.9f, 0.8f, false, false }; break;
        }
        for (auto& l : lines) l.clear();
        shimmerFb = 0.0f;
    }

    inline float diffuse (DelayBuffer& d, float x, float lenMs, float g) noexcept
    {
        const float delayed = d.read (std::max (1.0f, lenMs * 0.001f * (float) sr));
        const float v = x + g * delayed;
        d.push (v);
        return delayed - g * v;
    }

    inline float springStage (float x, const Params& p) noexcept
    {
        // Cadena dispersiva dentro de un bucle corto: el "goteo" metálico del muelle
        const float loopLen = (0.028f + 0.01f * p.decay) * (float) sr;
        float y = x + springLine.read (loopLen) * (0.55f + 0.3f * p.decay);
        for (auto& ap : springAp)
        {
            ap.a = 0.62f;
            const float out = -ap.a * y + ap.z;
            ap.z = y + ap.a * out;
            y = out;
        }
        y = springHp.highpass (springLp.lowpass (y));
        springLine.push (fastTanh (y));
        return y * 0.8f + x * 0.4f;
    }

    static constexpr std::array<float, 8> baseMs { 29.7f, 37.1f, 41.1f, 43.7f, 47.9f, 53.3f, 59.9f, 67.1f };
    static constexpr std::array<float, 4> diffLen { 4.7f, 3.6f, 12.7f, 9.3f };

    double sr = 44100.0;
    Config cfg { 1.0f, 1.0f, 7.0f, 0.85f, 0.9f, 0.8f, false, false };
    int lastType = -1;
    std::array<DelayBuffer, 8> lines;
    std::array<DelayBuffer, 4> diffusers;
    std::array<OnePole, 8> damp;
    std::array<float, 8> gains {}, lens {};
    DelayBuffer preDelay, springLine;
    std::array<Allpass1, 12> springAp {};
    OnePole springLp, springHp;
    GrainShifter shifter;
    float shimmerFb = 0.0f;
    double modPhase = 0.0;
};
} // namespace geminis::dsp
