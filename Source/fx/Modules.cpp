#include "Modules.h"
#include "../common/Reverb.h"

namespace geminis::fx
{
using namespace geminis::dsp;

//==============================================================================
float ParamSpec::toReal (float norm) const
{
    norm = clampf (norm, 0.0f, 1.0f);
    if (isChoice())
        return (float) juce::roundToInt (norm * (float) (choices.size() - 1));
    if (skew == 1.0f)
        return min + (max - min) * norm;
    return min + (max - min) * std::pow (norm, 1.0f / skew);
}

float ParamSpec::toNorm (float real) const
{
    if (isChoice())
        return choices.size() > 1 ? clampf (real / (float) (choices.size() - 1), 0.0f, 1.0f) : 0.0f;
    const float p = clampf ((real - min) / (max - min), 0.0f, 1.0f);
    return skew == 1.0f ? p : std::pow (p, skew);
}

juce::String ParamSpec::format (float real) const
{
    if (isChoice())
    {
        const int i = juce::jlimit (0, (int) choices.size() - 1, juce::roundToInt (real));
        return juce::String::fromUTF8 (choices[(size_t) i]);
    }

    const juce::String u = juce::String::fromUTF8 (unit);
    if (u == "%")  return juce::String (juce::roundToInt (real)) + " %";
    if (u == "ms") return std::abs (real) >= 100.0f ? juce::String (juce::roundToInt (real)) + " ms" : juce::String (real, 1) + " ms";
    if (u == "Hz")
    {
        if (real >= 1000.0f) return juce::String (real / 1000.0f, 2) + " kHz";
        return real >= 100.0f ? juce::String (juce::roundToInt (real)) + " Hz" : juce::String (real, 2) + " Hz";
    }
    if (u == "dB") return (min < 0.0f && real > 0.05f ? "+" : "") + juce::String (real, 1) + " dB";
    if (u == ":1") return juce::String (real, 1) + ":1";
    if (u == "st") return (real > 0.0f ? "+" : "") + juce::String (juce::roundToInt (real));
    if (u == "s")  return juce::String (real, 2) + " s";
    if (u.isEmpty()) return juce::String (real * 10.0f, 1);   // controles 0..10
    return juce::String (real, 2) + " " + u;
}

double syncedSeconds (int division, double bpm)
{
    // 0 libre (no se usa), 1 1/2, 2 1/4, 3 1/4 con puntillo, 4 1/8, 5 1/8 con puntillo, 6 1/8 tresillo, 7 1/16
    static const double beats[] = { 1.0, 2.0, 1.0, 1.5, 0.5, 0.75, 1.0 / 3.0, 0.25 };
    const double b = beats[juce::jlimit (0, 7, division)];
    return b * 60.0 / juce::jmax (30.0, bpm);
}

//==============================================================================
namespace
{
/** Módulo que procesa cada canal por separado y, si la señal es mono, solo el izquierdo. */
class MonoModule : public Module
{
public:
    void process (float* L, float* R, int n, const float* v, Context& ctx) override
    {
        update (v, ctx);
        processChannel (0, L, n);
        if (ctx.stereo) processChannel (1, R, n);
        else std::copy (L, L + n, R);
    }

protected:
    virtual void update (const float* v, const Context& ctx) = 0;
    virtual void processChannel (int ch, float* x, int n) = 0;
    double sr = 44100.0;
};

/** Sobremuestreo x4 para las saturaciones (evita el sonido digital áspero). */
class Oversampler
{
public:
    void prepare (int maxBlock)
    {
        os = std::make_unique<juce::dsp::Oversampling<float>> (2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        os->initProcessing ((size_t) maxBlock);
    }
    void reset() { if (os) os->reset(); }
    int latency() const { return os ? (int) std::lround (os->getLatencyInSamples()) : 0; }

    template <typename Fn>
    void process (float* L, float* R, int n, bool stereo, Fn&& fn)
    {
        float* chans[] = { L, R };
        juce::dsp::AudioBlock<float> block (chans, stereo ? 2u : 1u, (size_t) n);
        auto up = os->processSamplesUp (block);
        for (size_t c = 0; c < up.getNumChannels(); ++c)
            fn ((int) c, up.getChannelPointer (c), (int) up.getNumSamples());
        os->processSamplesDown (block);
    }

private:
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
};

/** Base para saturaciones mono con sobremuestreo. */
class SaturationModule : public Module
{
public:
    void prepare (double sampleRate, int maxBlock) override
    {
        sr = sampleRate;
        osr = sampleRate * 4.0;
        oversampler.prepare (maxBlock);
        reset();
    }
    void reset() override { oversampler.reset(); resetState(); }
    int getLatency() const override { return oversampler.latency(); }

    void process (float* L, float* R, int n, const float* v, Context& ctx) override
    {
        update (v);
        oversampler.process (L, R, n, ctx.stereo, [this] (int ch, float* x, int m) { run (ch, x, m); });
        if (! ctx.stereo) std::copy (L, L + n, R);
    }

protected:
    virtual void update (const float* v) = 0;
    virtual void run (int ch, float* x, int n) = 0;
    virtual void resetState() {}
    double sr = 44100.0, osr = 176400.0;
    Oversampler oversampler;
};

//==============================================================================
class GateModule : public Module
{
public:
    void prepare (double sampleRate, int) override { sr = sampleRate; det.set (sr, 0.3, 40.0); reset(); }
    void reset() override { det.reset(); gain = 0.0f; open = false; }

    void process (float* L, float* R, int n, const float* v, Context& ctx) override
    {
        const float openThr = dbToGain (v[0]), closeThr = dbToGain (v[0] - 5.0f);
        const float relCoef = (float) std::exp (-1.0 / (v[1] * 0.001 * sr));
        const float attCoef = (float) std::exp (-1.0 / (0.0008 * sr));
        const float floor = dbToGain (-v[2]);

        for (int i = 0; i < n; ++i)
        {
            const float lvl = det.process (ctx.stereo ? std::max (std::abs (L[i]), std::abs (R[i])) : L[i]);
            if (open ? lvl < closeThr : lvl > openThr) open = ! open;
            const float target = open ? 1.0f : floor;
            gain = target > gain ? target + attCoef * (gain - target) : target + relCoef * (gain - target);
            L[i] *= gain;
            if (ctx.stereo) R[i] *= gain;
        }
        if (! ctx.stereo) std::copy (L, L + n, R);
    }

private:
    double sr = 44100.0;
    EnvelopeFollower det;
    float gain = 0.0f;
    bool open = false;
};

//==============================================================================
class CompressorModule : public Module
{
public:
    void prepare (double sampleRate, int) override { sr = sampleRate; reset(); }
    void reset() override { envDb = -120.0f; gainDb = 0.0f; }

    void process (float* L, float* R, int n, const float* v, Context& ctx) override
    {
        const float thr = v[0], ratio = v[1];
        const float att = (float) std::exp (-1.0 / (v[2] * 0.001 * sr));
        const float rel = (float) std::exp (-1.0 / (v[3] * 0.001 * sr));
        const float makeup = dbToGain (v[4]);
        const float mix = v[5] * 0.01f;
        const float knee = 6.0f;

        for (int i = 0; i < n; ++i)
        {
            const float in = ctx.stereo ? std::max (std::abs (L[i]), std::abs (R[i])) : std::abs (L[i]);
            const float lvl = gainToDb (in);
            const float over = lvl - thr;
            float target = 0.0f;
            if (over > knee * 0.5f)       target = over * (1.0f / ratio - 1.0f);
            else if (over > -knee * 0.5f) target = (1.0f / ratio - 1.0f) * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);
            gainDb = target < gainDb ? target + att * (gainDb - target) : target + rel * (gainDb - target);
            const float g = dbToGain (gainDb) * makeup;
            L[i] = L[i] * (1.0f - mix) + L[i] * g * mix;
            if (ctx.stereo) R[i] = R[i] * (1.0f - mix) + R[i] * g * mix;
        }
        reduction = -gainDb;
        if (! ctx.stereo) std::copy (L, L + n, R);
    }

    float reduction = 0.0f;

private:
    double sr = 44100.0;
    float envDb = -120.0f, gainDb = 0.0f;
};

//==============================================================================
class LimiterModule : public Module
{
public:
    void prepare (double sampleRate, int) override { sr = sampleRate; reset(); }
    void reset() override { gain = 1.0f; }

    void process (float* L, float* R, int n, const float* v, Context& ctx) override
    {
        const float drive = dbToGain (v[0]), ceiling = dbToGain (v[1]);
        const float rel = (float) std::exp (-1.0 / (v[2] * 0.001 * sr));
        for (int i = 0; i < n; ++i)
        {
            float l = L[i] * drive, r = ctx.stereo ? R[i] * drive : l;
            const float peak = std::max (std::abs (l), std::abs (r));
            const float target = peak > ceiling ? ceiling / peak : 1.0f;
            gain = target < gain ? target : target + rel * (gain - target);
            l *= gain; r *= gain;
            // Recorte suave final por si algún pico se cuela
            L[i] = softCeil (l, ceiling);
            if (ctx.stereo) R[i] = softCeil (r, ceiling);
        }
        if (! ctx.stereo) std::copy (L, L + n, R);
    }

private:
    static inline float softCeil (float x, float c) noexcept
    {
        const float a = std::abs (x), knee = 0.8f * c;
        if (a <= knee) return x;
        return std::copysign (knee + (c - knee) * fastTanh ((a - knee) / (c - knee)), x);
    }

    double sr = 44100.0;
    float gain = 1.0f;
};

//==============================================================================
class OverdriveModule : public SaturationModule
{
    struct Chan { Biquad hp, mid, tone; };
    Chan c[2];
    float drive = 1.0f, level = 1.0f;

    void resetState() override { for (auto& ch : c) { ch.hp.reset(); ch.mid.reset(); ch.tone.reset(); } }
    void update (const float* v) override
    {
        for (auto& ch : c)
        {
            ch.hp.setHighpass (osr, 220.0);
            ch.mid.setPeak (osr, 760.0, 0.7, 6.0);
            ch.tone.setLowpass (osr, 900.0 + 7500.0 * v[1] * v[1]);
        }
        drive = dbToGain (4.0f + 38.0f * v[0]);
        level = dbToGain (-30.0f + 30.0f * v[2]) * 3.0f;
    }
    void run (int ch, float* x, int n) override
    {
        auto& s = c[ch];
        for (int i = 0; i < n; ++i)
        {
            const float hp = s.hp.process (x[i]);
            const float lows = x[i] - hp;
            const float clipped = fastTanh (s.mid.process (hp) * drive);
            x[i] = (s.tone.process (clipped) * 0.7f + lows * 0.5f) * level;
        }
    }
};

class DistortionModule : public SaturationModule
{
    struct Chan { Biquad tight, emph, tone, deemph; };
    Chan c[2];
    float drive = 1.0f, level = 1.0f;

    void resetState() override { for (auto& ch : c) { ch.tight.reset(); ch.emph.reset(); ch.tone.reset(); ch.deemph.reset(); } }
    void update (const float* v) override
    {
        for (auto& ch : c)
        {
            ch.tight.setHighpass (osr, v[3]);
            ch.emph.setPeak (osr, 1300.0, 0.6, 8.0);
            ch.tone.setLowpass (osr, 1500.0 + 8000.0 * v[1] * v[1], 0.6);
            ch.deemph.setPeak (osr, 400.0, 0.8, -3.0);
        }
        drive = dbToGain (10.0f + 42.0f * v[0]);
        level = dbToGain (-36.0f + 34.0f * v[2]) * 2.0f;
    }
    void run (int ch, float* x, int n) override
    {
        auto& s = c[ch];
        for (int i = 0; i < n; ++i)
        {
            float y = s.emph.process (s.tight.process (x[i])) * drive;
            const float a = std::abs (y);
            y = y / std::pow (1.0f + std::pow (a, 2.5f), 0.4f);
            y = clampf (y + 0.1f * y * y, -1.0f, 1.0f);
            x[i] = s.deemph.process (s.tone.process (y)) * level;
        }
    }
};

class FuzzModule : public SaturationModule
{
    struct Chan { Biquad in, lp, hp, dc; };
    Chan c[2];
    float drive = 1.0f, level = 1.0f, bias = 0.0f, tone = 0.5f, octave = 0.0f;

    void resetState() override { for (auto& ch : c) { ch.in.reset(); ch.lp.reset(); ch.hp.reset(); ch.dc.reset(); } }
    void update (const float* v) override
    {
        for (auto& ch : c)
        {
            ch.in.setHighpass (osr, 90.0);
            ch.lp.setLowpass (osr, 650.0);
            ch.hp.setHighpass (osr, 1100.0);
            ch.dc.setHighpass (osr, 25.0);
        }
        drive = dbToGain (18.0f + 42.0f * v[0]);
        tone = v[1];
        level = dbToGain (-40.0f + 36.0f * v[2]) * 2.0f;
        bias = 0.05f + 0.6f * v[3];
        octave = v[4];
    }
    void run (int ch, float* x, int n) override
    {
        auto& s = c[ch];
        const float tb = std::tanh (bias);
        for (int i = 0; i < n; ++i)
        {
            float in = s.in.process (x[i]);
            in = in * (1.0f - octave) + std::abs (in) * octave * 1.6f;
            float y = std::tanh (in * drive + bias) - tb;
            y = clampf (y * 1.4f, -1.0f, 1.0f);
            y = s.dc.process (y);
            const float shaped = s.lp.process (y) * (1.0f - tone) + s.hp.process (y) * tone;
            x[i] = shaped * level;
        }
    }
};

class BoosterModule : public MonoModule
{
public:
    void prepare (double sampleRate, int) override { sr = sampleRate; for (auto& ch : c) { ch.a.reset(); ch.b.reset(); } }

private:
    struct Chan { Biquad a, b; };
    Chan c[2];
    float gain = 1.0f;

    void update (const float* v, const Context&) override
    {
        const int type = (int) v[0];
        for (auto& ch : c)
        {
            if (type == 1)      ch.a.setHighShelf (sr, 1500.0, 9.0);
            else if (type == 2) ch.a.setPeak (sr, 900.0, 0.8, 8.0);
            else                ch.a.setPeak (sr, 1000.0, 1.0, 0.0);
            ch.b.setHighShelf (sr, 4500.0, v[2]);
        }
        gain = dbToGain (v[1]);
    }
    void processChannel (int ch, float* x, int n) override
    {
        auto& s = c[ch];
        for (int i = 0; i < n; ++i)
        {
            const float y = s.b.process (s.a.process (x[i])) * gain;
            x[i] = 1.2f * fastTanh (y / 1.2f);
        }
    }
};

//==============================================================================
class OctaverModule : public MonoModule
{
public:
    void prepare (double sampleRate, int) override
    {
        sr = sampleRate;
        for (auto& ch : c)
        {
            ch.track.setLowpass (sr, 280.0, 0.6);
            ch.track2.setLowpass (sr, 280.0, 0.6);
            ch.env.set (sr, 4.0, 60.0);
            ch.upHp.setHighpass (sr, 90.0);
            ch.ff1 = ch.ff2 = 1.0f; ch.last = 0.0f;
        }
    }

private:
    struct Chan { Biquad track, track2, subLp, subLp2, upHp; EnvelopeFollower env; float ff1 = 1.0f, ff2 = 1.0f, last = 0.0f; bool armed = true; };
    Chan c[2];
    float sub1 = 0.0f, sub2 = 0.0f, up = 0.0f, direct = 1.0f;

    void update (const float* v, const Context&) override
    {
        sub1 = v[0] * 0.01f; sub2 = v[1] * 0.01f; up = v[2] * 0.01f; direct = v[3] * 0.01f;
        for (auto& ch : c)
        {
            ch.subLp.setLowpass (sr, 200.0 + 1800.0 * v[4], 0.7);
            ch.subLp2.setLowpass (sr, 200.0 + 1800.0 * v[4], 0.7);
        }
    }
    void processChannel (int ch, float* x, int n) override
    {
        auto& s = c[ch];
        for (int i = 0; i < n; ++i)
        {
            const float in = x[i];
            const float tr = s.track2.process (s.track.process (in));
            const float e = s.env.process (in);
            // Divisor de octava: biestable que cambia en cada cruce positivo
            if (s.armed && tr > 0.02f * (e + 1.0e-4f) && s.last <= 0.0f)
            {
                s.ff1 = -s.ff1;
                if (s.ff1 > 0.0f) s.ff2 = -s.ff2;
                s.armed = false;
            }
            if (tr < 0.0f) s.armed = true;
            s.last = tr;
            const float o1 = s.subLp.process (s.ff1 * e * 1.4f);
            const float o2 = s.subLp2.process (s.ff2 * e * 1.4f);
            const float o3 = s.upHp.process (std::abs (in) * 1.5f);
            x[i] = in * direct + o1 * sub1 + o2 * sub2 + o3 * up;
        }
    }
};

//==============================================================================
class WahModule : public MonoModule
{
public:
    void prepare (double sampleRate, int maxBlock) override
    {
        sr = sampleRate;
        for (auto& ch : c) { ch.f.reset(); ch.env.set (sr, 5.0, 120.0); }
        posBuf.assign ((size_t) std::max (maxBlock, 64), 0.5f);
        lfo.reset();
        posRamp.setImmediate (0.5f);
    }

private:
    struct Chan { Svf f; EnvelopeFollower env; };
    Chan c[2];
    Lfo lfo;
    Ramp posRamp;
    int mode = 0;
    float pedal = 0.5f, rate = 1.0f, sens = 0.5f, q = 5.0f, mix = 1.0f;
    std::vector<float> posBuf = std::vector<float> (8192, 0.5f);

    void update (const float* v, const Context&) override
    {
        mode = (int) v[0]; pedal = v[1]; rate = v[2]; sens = v[3]; q = v[4]; mix = v[5] * 0.01f;
    }

    void processChannel (int ch, float* x, int n) override
    {
        auto& s = c[ch];
        if ((size_t) n > posBuf.size()) n = (int) posBuf.size();
        if (ch == 0)
        {
            // La posición del pedal se calcula una vez y la comparten ambos canales
            for (int i = 0; i < n; ++i)
            {
                float pos = pedal;
                if (mode == 1)      pos = 0.5f + 0.5f * lfo.next (rate / sr, Lfo::sine);
                else if (mode == 2) pos = clampf (s.env.process (x[i]) * (2.0f + 30.0f * sens), 0.0f, 1.0f);
                posRamp.setTarget (pos, 32);
                posBuf[(size_t) i] = posRamp.next();
            }
        }
        for (int i = 0; i < n; ++i)
        {
            const float pos = posBuf[(size_t) i];
            if ((i & 7) == 0) s.f.set (sr, 350.0 * std::pow (6.3, (double) pos), q);
            const auto o = s.f.process (x[i]);
            const float wet = o.bp * 2.2f + o.lp * 0.25f;
            x[i] = x[i] * (1.0f - mix) + wet * mix;
        }
    }
};

//==============================================================================
class Eq5Module : public Module
{
public:
    void prepare (double sampleRate, int) override { sr = sampleRate; for (auto& ch : f) for (auto& b : ch) b.reset(); }
    void process (float* L, float* R, int n, const float* v, Context& ctx) override
    {
        for (auto& ch : f)
        {
            ch[0].setLowShelf (sr, 90.0, v[0]);
            ch[1].setPeak (sr, 250.0, 0.9, v[1]);
            ch[2].setPeak (sr, 800.0, 0.9, v[2]);
            ch[3].setPeak (sr, 2500.0, 0.9, v[3]);
            ch[4].setHighShelf (sr, 6000.0, v[4]);
        }
        const float out = dbToGain (v[5]);
        auto run = [&] (std::array<Biquad, 5>& bands, float* x)
        {
            for (int i = 0; i < n; ++i)
            {
                float y = x[i];
                for (auto& b : bands) y = b.process (y);
                x[i] = y * out;
            }
        };
        run (f[0], L);
        if (ctx.stereo) run (f[1], R); else std::copy (L, L + n, R);
    }

private:
    double sr = 44100.0;
    std::array<std::array<Biquad, 5>, 2> f;
};

//==============================================================================
/** Carácter de cada amplificador. Son diseños propios inspirados en estilos, no réplicas. */
struct AmpVoicing
{
    float tightHz, preMidHz, preMidDb, preMidQ, gainMinDb, gainMaxDb, bias1, bias2, interHpHz, stage2Db;
    bool thirdStage; float stage3Db, fizzLpHz, midHz, powerDb, trimDb, bassHz;
};

const AmpVoicing& ampVoicing (int model)
{
    static const AmpVoicing v[] = {
        // Cristal: limpio americano con mucho margen
        {  60.0f, 1500.0f, 2.0f, 0.5f,  -6.0f, 18.0f, 0.10f, 0.05f,  30.0f,  0.0f, false, 0.0f, 9000.0f, 500.0f, 0.0f,  10.0f, 120.0f },
        // Dorado: combo de los 50, se rompe pronto y tiene medios gordos
        {  70.0f,  700.0f, 4.0f, 0.6f,   0.0f, 30.0f, 0.30f, 0.20f,  60.0f,  4.0f, false, 0.0f, 6500.0f, 650.0f, 5.0f,  -4.0f, 110.0f },
        // Britannia: stack británico crujiente
        { 110.0f,  900.0f, 5.0f, 0.7f,   6.0f, 36.0f, 0.25f, 0.15f, 120.0f,  8.0f, false, 0.0f, 7000.0f, 750.0f, 4.0f,  -8.0f, 120.0f },
        // Corona: británico modificado con más ganancia
        { 130.0f, 1100.0f, 6.0f, 0.7f,  10.0f, 42.0f, 0.22f, 0.12f, 150.0f, 10.0f, true,  4.0f, 6600.0f, 800.0f, 3.0f, -10.0f, 110.0f },
        // Titán: alta ganancia moderna
        { 160.0f, 1400.0f, 4.0f, 0.7f,  14.0f, 46.0f, 0.20f, 0.10f, 180.0f, 12.0f, true,  8.0f, 6200.0f, 650.0f, 3.0f, -10.0f, 100.0f },
        // Abismo: amplificador de bajo
        {  30.0f,  600.0f, 1.0f, 0.5f,  -4.0f, 26.0f, 0.15f, 0.05f,  25.0f,  2.0f, false, 0.0f, 5000.0f, 500.0f, 2.0f,   2.0f,  70.0f },
    };
    return v[juce::jlimit (0, 5, model)];
}

class AmpModule : public SaturationModule
{
    struct Chan { Biquad tight, preMid, inter, inter2, fizz, fizz2, bass, mid, treble, presence, depth, dc; };
    Chan c[2];
    int model = 2;
    float gain = 1.0f, st2 = 1.0f, st3 = 1.0f, power = 1.0f, master = 1.0f;

    void resetState() override
    {
        for (auto& ch : c)
            for (auto* b : { &ch.tight, &ch.preMid, &ch.inter, &ch.inter2, &ch.fizz, &ch.fizz2, &ch.bass, &ch.mid, &ch.treble, &ch.presence, &ch.depth, &ch.dc })
                b->reset();
    }

    void update (const float* v) override
    {
        model = (int) v[0];
        const auto& a = ampVoicing (model);
        const float g = v[1];
        for (auto& ch : c)
        {
            ch.tight.setHighpass (osr, a.tightHz);
            ch.preMid.setPeak (osr, a.preMidHz, a.preMidQ, a.preMidDb);
            ch.inter.setHighpass (osr, a.interHpHz);
            ch.inter2.setHighpass (osr, a.interHpHz * 0.6);
            ch.fizz.setLowpass (osr, a.fizzLpHz);
            ch.fizz2.setLowpass (osr, a.fizzLpHz * 1.3);
            ch.bass.setLowShelf (osr, a.bassHz, (v[2] - 0.5f) * 24.0f);
            ch.mid.setPeak (osr, a.midHz, 0.7, (v[3] - 0.5f) * 20.0f);
            ch.treble.setHighShelf (osr, 3000.0, (v[4] - 0.5f) * 24.0f);
            ch.presence.setPeak (osr, 4200.0, 0.8, (v[5] - 0.5f) * 16.0f);
            ch.depth.setPeak (osr, 85.0, 1.0, v[7] * 9.0f);
            ch.dc.setHighpass (osr, 20.0);
        }
        gain = dbToGain (a.gainMinDb + (a.gainMaxDb - a.gainMinDb) * g);
        st2 = dbToGain (a.stage2Db * (0.4f + 0.6f * g));
        st3 = dbToGain (a.stage3Db * (0.4f + 0.6f * g));
        power = dbToGain (a.powerDb);
        // Nivel automático: al subir la ganancia, el volumen sube menos (sobre todo en los limpios).
        static const float autoLevel[] = { 0.55f, 0.5f, 0.25f, 0.1f, 0.05f, 0.6f };
        const float gainDbFromMid = (a.gainMaxDb - a.gainMinDb) * (g - 0.5f);
        master = v[6] * v[6] * 4.0f * dbToGain (a.trimDb - autoLevel[juce::jlimit (0, 5, model)] * gainDbFromMid);
    }

    static inline float shape (float x, float bias) noexcept { return std::tanh (x + bias) - std::tanh (bias); }

    void run (int ch, float* x, int n) override
    {
        auto& s = c[ch];
        const auto& a = ampVoicing (model);
        for (int i = 0; i < n; ++i)
        {
            float y = s.preMid.process (s.tight.process (x[i]));
            y = shape (y * gain, a.bias1);
            y = shape (s.inter.process (y) * st2, a.bias2);
            if (a.thirdStage) y = shape (s.inter2.process (y) * st3, a.bias1 * 0.5f);
            y = s.fizz2.process (s.fizz.process (y)) * 0.5f;
            y = s.treble.process (s.mid.process (s.bass.process (y)));
            y = std::tanh (y * power);
            y = s.dc.process (s.depth.process (s.presence.process (y)));
            x[i] = y * master;
        }
    }
};

//==============================================================================
struct CabVoicing { float hpHz, lowHz, lowDb, midHz, midDb, midQ, presHz, presDb, notchHz, notchDb, lpHz; };

const CabVoicing& cabVoicing (int type)
{
    static const CabVoicing v[] = {
        {  95.0f, 140.0f, 2.0f, 450.0f, -2.0f, 1.0f, 2800.0f, 4.0f, 4200.0f, -5.0f, 5600.0f },  // 1x12 abierta
        {  85.0f, 120.0f, 3.0f, 400.0f, -3.0f, 1.0f, 2400.0f, 3.5f, 3900.0f, -6.0f, 5200.0f },  // 2x12 vintage
        {  75.0f, 110.0f, 4.0f, 500.0f, -2.5f, 0.9f, 2600.0f, 5.0f, 4400.0f, -7.0f, 5000.0f },  // 4x12 Britannia
        {  70.0f, 100.0f, 5.0f, 380.0f, -5.0f, 0.8f, 3000.0f, 5.5f, 4700.0f, -6.0f, 5400.0f },  // 4x12 Titán
        {  40.0f,  80.0f, 4.0f, 600.0f, -2.0f, 0.8f, 1800.0f, 2.0f, 3000.0f, -4.0f, 3800.0f },  // 1x15 bajo
        {  45.0f,  95.0f, 3.0f, 450.0f, -1.5f, 0.9f, 2500.0f, 3.0f, 4200.0f, -4.0f, 6000.0f },  // 4x10 bajo
        { 160.0f, 300.0f, 1.0f, 900.0f, 3.0f, 0.8f, 2200.0f, 4.0f, 3600.0f, -8.0f, 4200.0f },   // altavoz pequeño
    };
    return v[juce::jlimit (0, 6, type)];
}

class CabModule : public Module
{
public:
    static constexpr int userIrType = 7;

    void prepare (double sampleRate, int maxBlock) override
    {
        sr = sampleRate;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 1 };
        for (auto& cv : conv) cv.prepare (spec);
        for (auto& r : room) r.prepare ((int) (sr * 0.05) + 8);
        if (irFile.existsAsFile()) setUserIr (irFile);
        reset();
    }

    void reset() override
    {
        for (auto& ch : c) for (auto& b : ch.b) b.reset();
        for (auto& cv : conv) cv.reset();
        for (auto& r : room) r.clear();
    }

    void setUserIr (const juce::File& file) override
    {
        irFile = file;
        if (! file.existsAsFile()) return;
        for (auto& cv : conv)
            cv.loadImpulseResponse (file, juce::dsp::Convolution::Stereo::no, juce::dsp::Convolution::Trim::yes, 0,
                                    juce::dsp::Convolution::Normalise::yes);
    }

    void process (float* L, float* R, int n, const float* v, Context& ctx) override
    {
        const int type = (int) v[0];
        const float bright = 1.0f - v[1];
        const auto& cv = cabVoicing (type);
        for (auto& ch : c)
        {
            ch.b[0].setHighpass (sr, std::max (cv.hpHz, v[2]), 0.9);
            ch.b[1].setPeak (sr, cv.lowHz, 1.2, cv.lowDb);
            ch.b[2].setPeak (sr, cv.midHz, cv.midQ, cv.midDb);
            ch.b[3].setPeak (sr, cv.presHz, 1.4, cv.presDb * (0.3f + 1.2f * bright));
            ch.b[4].setPeak (sr, cv.notchHz, 3.0, cv.notchDb);
            const double lp = std::min ((double) v[3], cv.lpHz * (0.7 + 0.6 * bright));
            ch.b[5].setLowpass (sr, lp, 0.8);
            ch.b[6].setLowpass (sr, lp * 1.1, 0.6);
        }
        const float roomAmt = v[4];
        const bool useIr = type == userIrType && conv[0].getCurrentIRSize() > 0;

        auto run = [&] (int ch, float* x)
        {
            if (useIr)
            {
                float* chans[] = { x };
                juce::dsp::AudioBlock<float> block (chans, 1, (size_t) n);
                juce::dsp::ProcessContextReplacing<float> pc (block);
                conv[ch].process (pc);
                for (int i = 0; i < n; ++i) x[i] = c[ch].b[0].process (x[i]);
            }
            else
            {
                for (int i = 0; i < n; ++i)
                {
                    float y = x[i];
                    for (auto& b : c[ch].b) y = b.process (y);
                    x[i] = y;
                }
            }
            if (roomAmt > 0.001f)
            {
                auto& r = room[ch];
                const float d1 = (float) sr * (ch == 0 ? 0.0071f : 0.0089f), d2 = (float) sr * (ch == 0 ? 0.0153f : 0.0131f);
                for (int i = 0; i < n; ++i)
                {
                    const float y = x[i] + roomAmt * (0.45f * r.read (d1) + 0.3f * r.read (d2));
                    r.push (x[i]);
                    x[i] = y;
                }
            }
        };

        run (0, L);
        if (ctx.stereo) run (1, R);
        else if (roomAmt > 0.001f)
        {
            // La sala abre la imagen estéreo aunque la entrada sea mono
            std::copy (L, L + n, R);
            ctx.stereo = true;
        }
        else std::copy (L, L + n, R);
    }

private:
    struct Chan { std::array<Biquad, 7> b; };
    Chan c[2];
    juce::dsp::Convolution conv[2];
    DelayBuffer room[2];
    juce::File irFile;
    double sr = 44100.0;
};

//==============================================================================
/** Base para modulaciones que siempre entregan estéreo. */
class StereoModule : public Module
{
public:
    void process (float* L, float* R, int n, const float* v, Context& ctx) override
    {
        if (! ctx.stereo) std::copy (L, L + n, R);
        ctx.stereo = true;
        run (L, R, n, v, ctx);
    }

protected:
    virtual void run (float* L, float* R, int n, const float* v, const Context& ctx) = 0;
    double sr = 44100.0;
};

class ChorusModule : public StereoModule
{
public:
    void prepare (double sampleRate, int) override { sr = sampleRate; for (auto& d : dl) d.prepare ((int) (sr * 0.06)); lfo.reset(); }
    void reset() override { for (auto& d : dl) d.clear(); }

private:
    DelayBuffer dl[2];
    Lfo lfo;
    void run (float* L, float* R, int n, const float* v, const Context&) override
    {
        const double inc = v[0] / sr;
        const float depth = v[1] * 0.006f * (float) sr, base = v[2] * 0.001f * (float) sr;
        const float mix = v[3] * 0.01f, width = v[4];
        for (int i = 0; i < n; ++i)
        {
            const double ph = lfo.phase;
            const float m1 = std::sin (twoPi * (float) ph);
            const float m2 = std::sin (twoPi * (float) (ph + 0.25 * width));
            lfo.next (inc, Lfo::sine);
            dl[0].push (L[i]);
            dl[1].push (R[i]);
            const float wl = dl[0].read (base + depth * (0.5f + 0.5f * m1));
            const float wr = dl[1].read (base + depth * (0.5f + 0.5f * m2));
            L[i] = L[i] * (1.0f - 0.5f * mix) + wl * mix;
            R[i] = R[i] * (1.0f - 0.5f * mix) + wr * mix;
        }
    }
};

class FlangerModule : public StereoModule
{
public:
    void prepare (double sampleRate, int) override { sr = sampleRate; for (auto& d : dl) d.prepare ((int) (sr * 0.03)); reset(); }
    void reset() override { for (auto& d : dl) d.clear(); fb[0] = fb[1] = 0.0f; }

private:
    DelayBuffer dl[2];
    Lfo lfo;
    float fb[2] {};
    void run (float* L, float* R, int n, const float* v, const Context&) override
    {
        const double inc = v[0] / sr;
        const float depth = v[1], base = v[2] * 0.001f * (float) sr, feedback = v[3] * 0.0095f, mix = v[4] * 0.01f;
        // El retardo barre en escala exponencial: con 2 ms y profundidad 0,7 va de 0,8 a 5,3 ms,
        // así las muescas recorren varias octavas (el «avión a reacción»).
        const float sweep = depth * 2.0f;
        for (int i = 0; i < n; ++i)
        {
            const float ph = (float) lfo.phase;
            lfo.next (inc, Lfo::triangle);
            const float tri1 = 1.0f - 4.0f * std::abs (ph - 0.5f);
            float ph2 = ph + 0.25f; ph2 -= std::floor (ph2);
            const float tri2 = 1.0f - 4.0f * std::abs (ph2 - 0.5f);
            float* io[2] = { L, R };
            const float mods[2] = { tri1, tri2 };
            for (int c = 0; c < 2; ++c)
            {
                const float d = std::max (1.0f, base * std::exp2 (sweep * mods[c]));
                const float x = io[c][i];
                dl[c].push (x + fastTanh (fb[c] * feedback));
                const float w = dl[c].read (d);
                fb[c] = w;
                io[c][i] = x * (1.0f - mix) + w * mix;   // al 50 % directo y retardo igual: muescas profundas
            }
        }
    }
};

class PhaserModule : public StereoModule
{
public:
    void prepare (double sampleRate, int) override { sr = sampleRate; reset(); }
    void reset() override { for (auto& ch : z) ch.fill (0.0f); fb[0] = fb[1] = 0.0f; }

private:
    std::array<std::array<float, 12>, 2> z {};
    float fb[2] {};
    Lfo lfo;
    void run (float* L, float* R, int n, const float* v, const Context&) override
    {
        static const int stagesFor[] = { 2, 4, 6, 8, 10, 12 };
        const int stages = stagesFor[juce::jlimit (0, 5, (int) v[0])];
        const double inc = v[1] / sr;
        const float depth = v[2], centre = v[3], feedback = v[4] * 0.009f, mix = v[5] * 0.01f, spread = v[6] / 360.0f;
        float* io[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            const float ph = (float) lfo.phase;
            lfo.next (inc, Lfo::sine);
            for (int c = 0; c < 2; ++c)
            {
                const float m = std::sin (twoPi * (ph + (c == 1 ? spread : 0.0f)));
                const float f = clampf (centre * std::pow (8.0f, depth * m), 40.0f, (float) sr * 0.45f);
                const float t = std::tan (pi * f / (float) sr);
                const float a = (t - 1.0f) / (t + 1.0f);
                const float x = io[c][i];
                float y = x + fb[c] * feedback;
                for (int s = 0; s < stages; ++s)
                {
                    const float out = a * y + z[(size_t) c][(size_t) s];
                    z[(size_t) c][(size_t) s] = y - a * out;
                    y = out;
                }
                fb[c] = y;
                io[c][i] = x * (1.0f - mix) + y * mix;   // al 50 % las muescas son completas
            }
        }
    }
};

class VibratoModule : public MonoModule
{
public:
    void prepare (double sampleRate, int) override { sr = sampleRate; for (auto& d : dl) d.prepare ((int) (sr * 0.03)); lfo.reset(); }
    void reset() override { for (auto& d : dl) d.clear(); }

private:
    DelayBuffer dl[2];
    Lfo lfo;
    double savedPhase = 0.0;
    float rate = 5.0f, depth = 0.3f;
    void update (const float* v, const Context&) override { rate = v[0]; depth = v[1]; savedPhase = lfo.phase; }
    void processChannel (int ch, float* x, int n) override
    {
        lfo.phase = savedPhase;
        const float base = 0.006f * (float) sr, dep = depth * 0.004f * (float) sr;
        for (int i = 0; i < n; ++i)
        {
            const float m = lfo.next (rate / sr, Lfo::sine);
            dl[ch].push (x[i]);
            x[i] = dl[ch].read (base + dep * m);
        }
    }
};

class TremoloModule : public Module
{
public:
    void prepare (double sampleRate, int) override
    {
        sr = sampleRate;
        for (auto& ch : split) ch.setCutoff (sr, 800.0);
        lfo.reset();
    }

    void process (float* L, float* R, int n, const float* v, Context& ctx) override
    {
        const int shape = (int) v[0];
        double rate = v[1];
        if ((int) v[2] > 0) rate = 1.0 / syncedSeconds ((int) v[2] + 1, ctx.bpm);
        const float depth = v[3] * 0.01f, stereo = v[4];
        if (stereo > 0.01f && ! ctx.stereo) { std::copy (L, L + n, R); ctx.stereo = true; }

        for (int i = 0; i < n; ++i)
        {
            const float ph = (float) lfo.phase;
            lfo.next (rate / sr, Lfo::sine);
            auto wave = [shape] (float p)
            {
                p -= std::floor (p);
                switch (shape)
                {
                    case 1: return 1.0f - 2.0f * std::abs (2.0f * p - 1.0f);
                    case 2: return fastTanh (6.0f * std::sin (twoPi * p));
                    default: return std::sin (twoPi * p);
                }
            };
            const float m1 = wave (ph);
            const float m2 = wave (ph + 0.5f * stereo);
            if (shape == 3)
            {
                // Armónico: graves y agudos laten en contrafase
                float* io[2] = { L, R };
                for (int c = 0; c < (ctx.stereo ? 2 : 1); ++c)
                {
                    const float x = io[c][i];
                    const float lo = split[c].lowpass (x), hi = x - lo;
                    const float m = c == 0 ? m1 : m2;
                    io[c][i] = lo * (1.0f - depth * (0.5f + 0.5f * m)) + hi * (1.0f - depth * (0.5f - 0.5f * m));
                }
            }
            else
            {
                L[i] *= 1.0f - depth * (0.5f + 0.5f * m1);
                if (ctx.stereo) R[i] *= 1.0f - depth * (0.5f + 0.5f * m2);
            }
        }
        if (! ctx.stereo) std::copy (L, L + n, R);
    }

private:
    double sr = 44100.0;
    Lfo lfo;
    OnePole split[2];
};

class RotaryModule : public StereoModule
{
public:
    void prepare (double sampleRate, int) override
    {
        sr = sampleRate;
        for (auto& d : horn) d.prepare ((int) (sr * 0.01));
        xover.setLowpass (sr, 800.0, 0.6);
        xover2.setLowpass (sr, 800.0, 0.6);
        hornRate = 0.8f; drumRate = 0.7f;
    }

private:
    DelayBuffer horn[2];
    Biquad xover, xover2;
    double hornPh = 0.0, drumPh = 0.0;
    float hornRate = 0.8f, drumRate = 0.7f;

    void run (float* L, float* R, int n, const float* v, const Context&) override
    {
        const bool fast = (int) v[0] == 1;
        const float accel = std::max (0.2f, v[1]);
        const float balance = v[2], drive = v[3], width = v[4];
        const float hornTarget = fast ? 6.7f : 0.8f, drumTarget = fast ? 5.9f : 0.7f;
        const float k = 1.0f - std::exp (-1.0f / (accel * (float) sr));
        const float k2 = 1.0f - std::exp (-1.0f / (accel * 1.6f * (float) sr));
        for (int i = 0; i < n; ++i)
        {
            hornRate += (hornTarget - hornRate) * k;
            drumRate += (drumTarget - drumRate) * k2;
            hornPh += hornRate / sr; if (hornPh >= 1.0) hornPh -= 1.0;
            drumPh += drumRate / sr; if (drumPh >= 1.0) drumPh -= 1.0;

            float x = 0.5f * (L[i] + R[i]);
            x = drive > 0.01f ? fastTanh (x * (1.0f + 6.0f * drive)) / (1.0f + 2.0f * drive) : x;
            const float lo = xover2.process (xover.process (x)), hi = x - lo;
            const float hs = std::sin (twoPi * (float) hornPh), hc = std::cos (twoPi * (float) hornPh);
            const float ds = std::sin (twoPi * (float) drumPh);
            horn[0].push (hi); horn[1].push (hi);
            const float dBase = 0.0025f * (float) sr, dMod = 0.0009f * (float) sr;
            // La bocina apunta hacia un lado y luego hacia el otro: volumen y brillo cambian mucho
            const float hl = horn[0].read (dBase + dMod * hs) * (1.0f - 0.6f * (0.5f + 0.5f * hc));
            const float hr = horn[1].read (dBase - dMod * hs) * (1.0f - 0.6f * (0.5f - 0.5f * hc));
            const float dl = lo * (1.0f - 0.35f * (0.5f + 0.5f * ds));
            const float dr = lo * (1.0f - 0.35f * (0.5f - 0.5f * ds));
            const float hornGain = 0.5f + balance, drumGain = 1.5f - balance;
            const float outL = hl * hornGain + dl * drumGain, outR = hr * hornGain + dr * drumGain;
            const float mid = 0.5f * (outL + outR), side = 0.5f * (outL - outR) * width;
            L[i] = mid + side;
            R[i] = mid - side;
        }
    }
};

//==============================================================================
class DelayModule : public Module
{
public:
    void prepare (double sampleRate, int) override
    {
        sr = sampleRate;
        for (auto& d : line) d.prepare ((int) (sr * 2.2) + 64);
        for (auto& g : reverse) g.prepare (sr, 4300.0f);
        reset();
    }

    void reset() override
    {
        for (auto& d : line) d.clear();
        for (auto& g : reverse) g.clear();
        for (auto& f : lp) f.reset();
        for (auto& f : hp) f.reset();
        fbL = fbR = 0.0f;
        timeRamp.setImmediate (-1.0f);
    }

    void process (float* L, float* R, int n, const float* v, Context& ctx) override
    {
        const int type = (int) v[0];
        double seconds = v[1] * 0.001;
        if ((int) v[2] > 0) seconds = syncedSeconds ((int) v[2], ctx.bpm);
        seconds = juce::jlimit (0.01, 2.0, seconds);
        const float target = (float) (seconds * sr);
        if (timeRamp.current < 0.0f) timeRamp.setImmediate (target);
        timeRamp.setTarget (target, (int) (sr * (type == 1 ? 0.35 : 0.08)));

        const float feedback = std::min (v[3] * 0.01f, 1.0f) * (type == 1 ? 1.05f : 0.98f);
        const float mod = v[5];
        const float mix = v[6] * 0.01f;
        const float tone = v[4];
        double lpHz = 1500.0 + 12500.0 * tone * tone;
        if (type == 2) lpHz = std::min (lpHz, 3400.0);
        if (type == 1) lpHz *= 0.65;
        for (auto& f : lp) f.setCutoff (sr, lpHz);
        for (auto& f : hp) f.setCutoff (sr, type == 1 ? 120.0 : 40.0);

        if (! ctx.stereo) std::copy (L, L + n, R);
        const bool wasStereo = ctx.stereo;
        ctx.stereo = true;

        const double wowInc = 0.55 / sr, flutterInc = 6.5 / sr, chorusInc = 0.4 / sr;
        for (int i = 0; i < n; ++i)
        {
            const float base = timeRamp.next();
            float m = 0.0f;
            if (type == 1)
                m = mod * ((float) std::sin (twoPi * (float) wow.phase) * 0.004f + (float) std::sin (twoPi * (float) flutter.phase) * 0.0004f) * (float) sr;
            else if (mod > 0.0f)
                m = mod * (float) std::sin (twoPi * (float) wow.phase) * 0.002f * (float) sr;
            wow.next (type == 1 ? wowInc : chorusInc, Lfo::sine);
            flutter.next (flutterInc, Lfo::sine);

            const float inL = L[i], inR = R[i];
            float wetL = 0.0f, wetR = 0.0f;

            switch (type)
            {
                case 3: // ping-pong
                {
                    const float mono = 0.5f * (inL + inR);
                    wetL = line[0].read (base + m);
                    wetR = line[1].read (base + m);
                    line[0].push (mono + loop (1, wetR) * feedback);
                    line[1].push (loop (0, wetL) * feedback);
                    break;
                }
                case 4: // multitap
                {
                    const float t1 = line[0].read (base * 0.25f + m), t2 = line[0].read (base * 0.5f + m);
                    const float t3 = line[0].read (base * 0.75f + m), t4 = line[0].read (base + m);
                    wetL = 0.45f * t1 + 0.7f * t3 + 0.3f * t4;
                    wetR = 0.6f * t2 + 0.9f * t4;
                    line[0].push (0.5f * (inL + inR) + loop (0, t4) * feedback);
                    break;
                }
                case 5: // inverso
                {
                    const float window = (float) (2.0 * seconds * 1000.0);
                    wetL = reverse[0].process (inL + fbL * feedback, -1.0f, window);
                    wetR = reverse[1].process (inR + fbR * feedback, -1.0f, window * 1.03f);
                    fbL = lp[0].lowpass (wetL);
                    fbR = lp[1].lowpass (wetR);
                    break;
                }
                default: // digital, cinta, analógico
                {
                    wetL = line[0].read (base + m);
                    wetR = line[1].read (base * (wasStereo ? 1.0f : 1.012f) + m);
                    line[0].push (inL + loop (0, wetL) * feedback);
                    line[1].push (inR + loop (1, wetR) * feedback);
                    break;
                }
            }

            L[i] = inL + wetL * mix;
            R[i] = inR + wetR * mix;
        }
    }

private:
    inline float loop (int ch, float x) noexcept
    {
        float y = hp[ch].highpass (lp[ch].lowpass (x));
        return fastTanh (y * 1.1f) / 1.1f;
    }

    double sr = 44100.0;
    DelayBuffer line[2];
    GrainShifter reverse[2];
    OnePole lp[2], hp[2];
    Lfo wow, flutter;
    Ramp timeRamp;
    float fbL = 0.0f, fbR = 0.0f;
};

class ReverbModule : public Module
{
public:
    void prepare (double sampleRate, int) override { reverb.prepare (sampleRate); }
    void reset() override { reverb.reset(); }
    void process (float* L, float* R, int n, const float* v, Context& ctx) override
    {
        if (! ctx.stereo) std::copy (L, L + n, R);
        ctx.stereo = true;
        GeminisReverb::Params p;
        p.type = (int) v[0];
        p.decay = v[1];
        p.preDelayMs = v[2];
        p.tone = v[3];
        p.modulation = v[4];
        p.mix = v[5] * 0.01f;
        p.width = v[6];
        reverb.process (L, R, n, p);
    }

private:
    GeminisReverb reverb;
};

class VolumeModule : public Module
{
public:
    void prepare (double, int) override {}
    void process (float* L, float* R, int n, const float* v, Context& ctx) override
    {
        const float g = dbToGain (v[0]);
        const float pan = v[1];
        if (std::abs (pan) > 0.001f && ! ctx.stereo) { std::copy (L, L + n, R); ctx.stereo = true; }
        const float gl = g * std::min (1.0f, 1.0f - pan), gr = g * std::min (1.0f, 1.0f + pan);
        for (int i = 0; i < n; ++i) { L[i] *= gl; R[i] *= ctx.stereo ? gr : gl; }
    }
};

//==============================================================================
class VocalEqModule : public Module
{
public:
    void prepare (double sampleRate, int) override { sr = sampleRate; for (auto& ch : f) for (auto& b : ch) b.reset(); }
    void process (float* L, float* R, int n, const float* v, Context& ctx) override
    {
        for (auto& ch : f)
        {
            ch[0].setHighpass (sr, v[0], 0.7);
            ch[1].setHighpass (sr, v[0], 0.7);
            ch[2].setPeak (sr, 200.0, 0.9, v[1]);
            ch[3].setPeak (sr, 1000.0, 1.2, v[2]);
            ch[4].setPeak (sr, 4000.0, 0.9, v[3]);
            ch[5].setHighShelf (sr, 11000.0, v[4]);
        }
        auto run = [&] (std::array<Biquad, 6>& bands, float* x)
        {
            for (int i = 0; i < n; ++i) { float y = x[i]; for (auto& b : bands) y = b.process (y); x[i] = y; }
        };
        run (f[0], L);
        if (ctx.stereo) run (f[1], R); else std::copy (L, L + n, R);
    }

private:
    double sr = 44100.0;
    std::array<std::array<Biquad, 6>, 2> f;
};

class DeesserModule : public Module
{
public:
    void prepare (double sampleRate, int) override { sr = sampleRate; det.set (sr, 0.4, 60.0); reset(); }
    void reset() override { for (auto& b : split) b.reset(); detHp.reset(); det.reset(); gain = 1.0f; }

    void process (float* L, float* R, int n, const float* v, Context& ctx) override
    {
        detHp.setHighpass (sr, v[0], 0.8);
        for (auto& b : split) b.setLowpass (sr, v[0] * 0.8, 0.6);
        const float thr = v[1], maxRed = v[2];
        const float k = (float) std::exp (-1.0 / (0.002 * sr));
        for (int i = 0; i < n; ++i)
        {
            const float mono = ctx.stereo ? 0.5f * (L[i] + R[i]) : L[i];
            const float lvl = gainToDb (det.process (detHp.process (mono)));
            const float red = std::min (maxRed, std::max (0.0f, (lvl - thr) * 0.8f));
            gain = dbToGain (-red) + k * (gain - dbToGain (-red));
            const float lo = split[0].process (L[i]);
            L[i] = lo + (L[i] - lo) * gain;
            if (ctx.stereo) { const float lo2 = split[1].process (R[i]); R[i] = lo2 + (R[i] - lo2) * gain; }
        }
        if (! ctx.stereo) std::copy (L, L + n, R);
    }

private:
    double sr = 44100.0;
    Biquad split[2], detHp;
    EnvelopeFollower det;
    float gain = 1.0f;
};

class TapeModule : public SaturationModule
{
    struct Chan { Biquad bump, roll, dc; };
    Chan c[2];
    float drive = 1.0f, out = 1.0f, mix = 1.0f;
    void resetState() override { for (auto& ch : c) { ch.bump.reset(); ch.roll.reset(); ch.dc.reset(); } }
    void update (const float* v) override
    {
        drive = dbToGain (v[0]);
        for (auto& ch : c)
        {
            ch.bump.setPeak (osr, 95.0, 0.9, 3.5 * v[1]);
            ch.roll.setHighShelf (osr, 9000.0, -7.0 * v[1]);
            ch.dc.setHighpass (osr, 15.0);
        }
        out = dbToGain (v[2]) / std::sqrt (drive);
        mix = v[3] * 0.01f;
    }
    void run (int ch, float* x, int n) override
    {
        auto& s = c[ch];
        for (int i = 0; i < n; ++i)
        {
            const float in = x[i];
            float y = s.bump.process (in * drive);
            y = fastTanh (y + 0.08f * y * y) ;
            y = s.dc.process (s.roll.process (y)) * out;
            x[i] = in * (1.0f - mix) + y * mix;
        }
    }
};

class DoublerModule : public StereoModule
{
public:
    void prepare (double sampleRate, int) override
    {
        sr = sampleRate;
        for (auto& d : dl) d.prepare ((int) (sr * 0.09));
        lfoA.seed = 111u; lfoB.seed = 999u; lfoC.seed = 4242u; lfoD.seed = 777u;
        darkA.setCutoff (sr, 6500.0); thinB.setCutoff (sr, 180.0);
    }
    void reset() override { for (auto& d : dl) d.clear(); darkA.reset(); thinB.reset(); }

private:
    DelayBuffer dl[2];
    Lfo lfoA, lfoB, lfoC, lfoD;
    OnePole darkA, thinB;

    /** Simula dos tomas más, una a cada lado. Cada toma va un poco a destiempo y desafinada unos
        cents de forma cambiante (como un cantante que repite la frase), y con un timbre algo distinto. */
    void run (float* L, float* R, int n, const float* v, const Context&) override
    {
        const float sep = v[0] * 0.001f * (float) sr, var = v[1], width = v[2], mix = v[3] * 0.01f;
        const float drift = (0.002f + var * 0.006f) * (float) sr;      // destiempo cambiante de hasta 8 ms
        const float wobble = (0.0004f + var * 0.0012f) * (float) sr;   // vaivén rápido: unos cents de afinación
        const float gain = mix * 1.3f;
        for (int i = 0; i < n; ++i)
        {
            const float mono = 0.5f * (L[i] + R[i]);
            dl[0].push (mono); dl[1].push (mono);
            const float a1 = lfoA.next (0.35 / sr, Lfo::randomSmooth), b1 = lfoB.next (0.42 / sr, Lfo::randomSmooth);
            const float a2 = lfoC.next (2.1 / sr, Lfo::randomSmooth), b2 = lfoD.next (2.6 / sr, Lfo::randomSmooth);
            float a = dl[0].read (sep + drift * (0.5f + 0.5f * a1) + wobble * a2 + 2.0f);
            float b = dl[1].read (sep * 1.4f + drift * (0.5f + 0.5f * b1) + wobble * b2 + 2.0f);
            a = darkA.lowpass (a);                 // una toma algo más oscura
            b = b - thinB.lowpass (b);             // y la otra con menos graves
            const float vl = a * (0.5f + 0.5f * width) + b * (0.5f - 0.5f * width);
            const float vr = b * (0.5f + 0.5f * width) + a * (0.5f - 0.5f * width);
            L[i] = L[i] + vl * gain;
            R[i] = R[i] + vr * gain;
        }
    }
};

//==============================================================================
/** Afinación automática con PSOLA. Detecta el periodo de la voz (YIN sobre señal diezmada) y la
    vuelve a construir con granos de dos periodos colocados a la distancia del periodo corregido.
    Así cambia la afinación sin cambiar el timbre de la voz y sin la ondulación de un desplazador
    de granos fijo. Necesita unos 25 ms de anticipación, que se comunican al DAW como latencia. */
class AutotuneModule : public Module
{
public:
    void prepare (double sampleRate, int) override
    {
        sr = sampleRate;
        decim = sr > 60000.0 ? 4 : 2;
        dsr = sr / decim;
        aa.setLowpass (sr, dsr * 0.4, 0.7);
        aa2.setLowpass (sr, dsr * 0.4, 0.7);
        ring.assign (2048, 0.0f);
        diff.assign (600, 1.0f);
        maxPeriod = (int) std::ceil (sr / lowestHz);
        latency = 2 * maxPeriod + 8;
        int size = 1;
        while (size < 4 * latency + 4096) size <<= 1;
        mask = size - 1;
        in.assign ((size_t) size, 0.0f);
        out.assign ((size_t) size, 0.0f);
        reset();
    }

    int getLatency() const override { return latency; }

    void reset() override
    {
        std::fill (ring.begin(), ring.end(), 0.0f);
        std::fill (in.begin(), in.end(), 0.0f);
        std::fill (out.begin(), out.end(), 0.0f);
        aa.reset(); aa2.reset();
        ratio = 1.0f; targetRatio = 1.0f; targetNote = -1.0f;
        period = unvoicedPeriod(); voiced = false;
        recent[0] = recent[1] = recent[2] = 0.0f;
        writePos = 0; decimCount = 0; hopCount = 0;
        now = 0;
        nextMark = anaMark = (double) latency;
    }

    void process (float* L, float* R, int n, const float* v, Context& ctx) override
    {
        key = (int) v[0];
        scale = (int) v[1];
        const float speed = clampf (v[2], 0.0f, 1.0f);
        const float mix = v[3] * 0.01f;
        // Velocidad 1: corrección instantánea (efecto robótico); 0,6: firme pero natural; 0: muy lenta
        const float retuneMs = 1.0f + 400.0f * (1.0f - speed) * (1.0f - speed) * (1.0f - speed);
        const float k = 1.0f - std::exp (-1.0f / (retuneMs * 0.001f * (float) sr));

        for (int i = 0; i < n; ++i)
        {
            const float mono = ctx.stereo ? 0.5f * (L[i] + R[i]) : L[i];
            in[(size_t) (now & mask)] = mono;

            const float filtered = aa2.process (aa.process (mono));
            if (++decimCount >= decim)
            {
                decimCount = 0;
                ring[(size_t) writePos] = filtered;
                writePos = (writePos + 1) & 2047;
                if (++hopCount >= 128) { hopCount = 0; analyse(); }
            }
            ratio += (targetRatio - ratio) * k;

            // Coloca los granos que ya tienen toda su entrada disponible
            while (nextMark + (double) grainPeriod() + 1.0 <= (double) now)
                placeGrain();

            const std::int64_t readPos = now - latency;
            const float wet = out[(size_t) (readPos & mask)];
            out[(size_t) (readPos & mask)] = 0.0f;
            const float dry = in[(size_t) (readPos & mask)];
            const float y = dry * (1.0f - mix) + wet * mix;
            L[i] = y;
            if (ctx.stereo) R[i] = y;
            ++now;
        }
        if (! ctx.stereo) std::copy (L, L + n, R);
    }

private:
    static constexpr float lowestHz = 80.0f;
    float unvoicedPeriod() const { return (float) (sr * 0.005); }
    float grainPeriod() const { return voiced ? period : unvoicedPeriod(); }

    void placeGrain()
    {
        const float P = grainPeriod();
        const float r = voiced ? ratio : 1.0f;
        // Centro de análisis: el punto de la rejilla de periodos más cercano por detrás de la marca de síntesis
        const double steps = std::floor ((nextMark - anaMark) / (double) P + 1.0e-6);
        const double a = anaMark + steps * (double) P;
        const std::int64_t centre = (std::int64_t) std::llround (nextMark);
        const double offset = a - (double) centre;          // posición de entrada = salida + offset
        const int half = (int) std::ceil (P);
        const float gain = 1.0f / r;                        // los granos se solapan más al subir el tono
        for (int j = -half; j <= half; ++j)
        {
            const float w = std::abs ((float) j) >= P ? 0.0f : 0.5f + 0.5f * std::cos (pi * (float) j / P);
            if (w <= 0.0f) continue;
            const double ip = (double) (centre + j) + offset;
            const std::int64_t i0 = (std::int64_t) std::floor (ip);
            const float f = (float) (ip - (double) i0);
            const float x0 = in[(size_t) (i0 & mask)], x1 = in[(size_t) ((i0 + 1) & mask)];
            out[(size_t) ((centre + j) & mask)] += (x0 + (x1 - x0) * f) * w * gain;
        }
        anaMark = a;
        nextMark += (double) (P / r);
    }

    void analyse()
    {
        const int W = 384;
        const int tauMin = (int) (dsr / 1000.0), tauMax = std::min ((int) (dsr / (lowestHz - 5.0f)), (int) diff.size() - 2);
        const int start = (writePos - W - tauMax - 2 + 2048) & 2047;
        auto at = [this, start] (int i) { return ring[(size_t) ((start + i) & 2047)]; };

        float energy = 0.0f;
        for (int i = 0; i < W; ++i) energy += at (i) * at (i);
        if (energy < 1.0e-4f) { unvoice(); return; }

        float running = 0.0f;
        diff[0] = 1.0f;
        for (int tau = 1; tau <= tauMax + 1; ++tau)
        {
            float d = 0.0f;
            for (int i = 0; i < W; ++i) { const float e = at (i) - at (i + tau); d += e * e; }
            running += d;
            diff[(size_t) tau] = running > 0.0f ? d * (float) tau / running : 1.0f;
        }

        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
            if (diff[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < tauMax && diff[(size_t) tau + 1] < diff[(size_t) tau]) ++tau;
                best = tau;
                break;
            }
        if (best < 0)
        {
            int m = tauMin;
            for (int tau = tauMin; tau < tauMax; ++tau)
                if (diff[(size_t) tau] < diff[(size_t) m]) m = tau;
            if (diff[(size_t) m] < 0.25f) best = m;
        }
        if (best < 2) { unvoice(); return; }

        const float a = diff[(size_t) best - 1], b = diff[(size_t) best], c = diff[(size_t) best + 1];
        const float denom = a - 2.0f * b + c;
        const float refined = (float) best + (std::abs (denom) > 1.0e-6f ? 0.5f * (a - c) / denom : 0.0f);
        float hz = (float) dsr / refined;

        // Mediana de las tres últimas lecturas: quita saltos sueltos de octava
        recent[0] = recent[1]; recent[1] = recent[2]; recent[2] = hz;
        if (recent[0] > 0.0f && recent[1] > 0.0f)
            hz = std::max (std::min (recent[0], recent[1]), std::min (std::max (recent[0], recent[1]), recent[2]));

        period = clampf ((float) sr / hz, 8.0f, (float) maxPeriod);
        voiced = true;

        const float note = 69.0f + 12.0f * std::log2 (hz / 440.0f);
        // Histéresis: no cambia de nota objetivo hasta pasar claramente a la siguiente
        if (targetNote < 0.0f || std::abs (note - targetNote) > 0.65f)
            targetNote = nearestInScale (note);
        targetRatio = clampf (std::pow (2.0f, (targetNote - note) / 12.0f), 0.7f, 1.4f);
    }

    void unvoice()
    {
        voiced = false;
        targetRatio = 1.0f;
        targetNote = -1.0f;
        recent[0] = recent[1] = recent[2] = 0.0f;
    }

    float nearestInScale (float note) const
    {
        static const int masks[] = {
            0xFFF,                                                   // cromática
            (1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 11),   // mayor
            (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 10),   // menor
            (1 << 0) | (1 << 2) | (1 << 4) | (1 << 7) | (1 << 9),    // pentatónica mayor
            (1 << 0) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 10),   // pentatónica menor
        };
        const int mask = masks[juce::jlimit (0, 4, scale)];
        float best = std::round (note), bestDist = 100.0f;
        for (int off = -6; off <= 6; ++off)
        {
            const int cand = (int) std::round (note) + off;
            const int degree = ((cand - key) % 12 + 12) % 12;
            if ((mask >> degree) & 1)
            {
                const float d = std::abs ((float) cand - note);
                if (d < bestDist) { bestDist = d; best = (float) cand; }
            }
        }
        return best;
    }

    double sr = 44100.0, dsr = 22050.0;
    int decim = 2, decimCount = 0, hopCount = 0, writePos = 0, key = 0, scale = 0;
    int maxPeriod = 552, latency = 1112, mask = 0;
    Biquad aa, aa2;
    std::vector<float> ring, diff, in, out;
    std::int64_t now = 0;
    double nextMark = 0.0, anaMark = 0.0;
    float period = 220.0f, ratio = 1.0f, targetRatio = 1.0f, targetNote = -1.0f;
    float recent[3] {};
    bool voiced = false;
};

class LofiModule : public Module
{
public:
    void prepare (double sampleRate, int) override { sr = sampleRate; reset(); }
    void reset() override { for (auto& b : bands) b.reset(); held[0] = held[1] = 0.0f; counter = 0; }

    void process (float* L, float* R, int n, const float* v, Context& ctx) override
    {
        const float filt = v[0];
        bands[0].setHighpass (sr, 20.0 + 380.0 * filt, 0.7);
        bands[1].setLowpass (sr, 20000.0 - 17200.0 * filt, 0.7);
        bands[2].setHighpass (sr, 20.0 + 380.0 * filt, 0.7);
        bands[3].setLowpass (sr, 20000.0 - 17200.0 * filt, 0.7);
        const float levels = std::pow (2.0f, v[1] - 1.0f);
        const int hold = std::max (1, (int) v[2]);
        const float noiseAmt = v[3], mix = v[4] * 0.01f;

        for (int i = 0; i < n; ++i)
        {
            if (++counter >= hold) { counter = 0; held[0] = L[i]; held[1] = ctx.stereo ? R[i] : L[i]; }
            for (int c = 0; c < (ctx.stereo ? 2 : 1); ++c)
            {
                float* x = c == 0 ? L : R;
                float y = std::round (held[c] * levels) / levels;
                y = bands[c * 2 + 1].process (bands[c * 2].process (y));
                float crackle = 0.0f;
                if (noiseAmt > 0.0f)
                {
                    crackle = noise.next() * 0.01f * noiseAmt;
                    if (noise.next() > 0.9995f - 0.002f * noiseAmt) crackle += noise.next() * 0.3f * noiseAmt;
                }
                x[i] = x[i] * (1.0f - mix) + (y + crackle) * mix;
            }
        }
        if (! ctx.stereo) std::copy (L, L + n, R);
    }

private:
    double sr = 44100.0;
    Biquad bands[4];
    float held[2] {};
    int counter = 0;
    Noise noise;
};

class EmptyModule : public Module
{
public:
    void prepare (double, int) override {}
    void process (float*, float*, int, const float*, Context&) override {}
};

//==============================================================================
template <typename T> std::function<std::unique_ptr<Module>()> make() { return [] { return std::make_unique<T>(); }; }

ParamSpec knob (const char* name, float mn, float mx, float def, const char* unit = "", float skew = 1.0f, bool bipolar = false)
{
    ParamSpec p;
    p.name = name; p.min = mn; p.max = mx; p.def = def; p.unit = unit; p.skew = skew; p.bipolar = bipolar;
    return p;
}

ParamSpec choice (const char* name, std::vector<const char*> names, int def)
{
    ParamSpec p;
    p.name = name; p.min = 0.0f; p.max = (float) (names.size() - 1); p.def = (float) def;
    p.choices = std::move (names);
    return p;
}

const std::vector<const char*> syncNames { "Libre", "1/2", "1/4", "1/4 con puntillo", "1/8", "1/8 con puntillo", "1/8 tresillo", "1/16" };

constexpr juce::uint32 cDyn = 0xff6ee7a8, cSat = 0xffff9f5a, cAmp = 0xfff3c96a, cFilt = 0xffff7aa8,
                       cMod = 0xffa78bfa, cDly = 0xff8fe9ff, cRev = 0xff7aa2ff, cVoz = 0xff4cc9c0, cUtil = 0xff9aa0b8;
} // namespace

//==============================================================================
const std::vector<ModuleInfo>& catalog()
{
    static const std::vector<ModuleInfo> list = [] {
        std::vector<ModuleInfo> m;
        m.push_back ({ "Vacío", "", 0, cUtil, "", {}, make<EmptyModule>() });

        m.push_back ({ "Puerta de ruido", "Dinámica", forBoth, cDyn, "Corta el ruido cuando no tocas.",
                       { knob ("Umbral", -90.0f, -20.0f, -60.0f, "dB"), knob ("Liberación", 5.0f, 800.0f, 120.0f, "ms", 0.5f),
                         knob ("Reducción", 6.0f, 90.0f, 60.0f, "dB") }, make<GateModule>() });
        m.push_back ({ "Compresor", "Dinámica", forBoth, cDyn, "Iguala el volumen y da sustain.",
                       { knob ("Umbral", -50.0f, 0.0f, -20.0f, "dB"), knob ("Ratio", 1.0f, 20.0f, 4.0f, ":1", 0.45f),
                         knob ("Ataque", 0.1f, 100.0f, 8.0f, "ms", 0.4f), knob ("Liberación", 10.0f, 1000.0f, 150.0f, "ms", 0.4f),
                         knob ("Nivel", 0.0f, 24.0f, 4.0f, "dB"), knob ("Mezcla", 0.0f, 100.0f, 100.0f, "%") }, make<CompressorModule>() });
        m.push_back ({ "Limitador", "Dinámica", forBoth, cDyn, "Techo de volumen sin picos.",
                       { knob ("Entrada", 0.0f, 24.0f, 0.0f, "dB"), knob ("Techo", -12.0f, 0.0f, -1.0f, "dB"),
                         knob ("Liberación", 10.0f, 500.0f, 60.0f, "ms", 0.5f) }, make<LimiterModule>() });

        m.push_back ({ "Impulso", "Saturación", forGuitar, cSat, "Overdrive con realce de medios.",
                       { knob ("Ganancia", 0.0f, 1.0f, 0.4f), knob ("Tono", 0.0f, 1.0f, 0.5f), knob ("Nivel", 0.0f, 1.0f, 0.5f) },
                       make<OverdriveModule>() });
        m.push_back ({ "Furia", "Saturación", forGuitar, cSat, "Distorsión dura y cortante.",
                       { knob ("Distorsión", 0.0f, 1.0f, 0.5f), knob ("Tono", 0.0f, 1.0f, 0.5f), knob ("Nivel", 0.0f, 1.0f, 0.5f),
                         knob ("Graves", 60.0f, 400.0f, 120.0f, "Hz", 0.5f) }, make<DistortionModule>() });
        m.push_back ({ "Nébula", "Saturación", forGuitar, cSat, "Fuzz salvaje con octava.",
                       { knob ("Fuzz", 0.0f, 1.0f, 0.6f), knob ("Tono", 0.0f, 1.0f, 0.5f), knob ("Nivel", 0.0f, 1.0f, 0.5f),
                         knob ("Sesgo", 0.0f, 1.0f, 0.2f), knob ("Octava", 0.0f, 1.0f, 0.0f) }, make<FuzzModule>() });
        m.push_back ({ "Chispa", "Saturación", forGuitar, cSat, "Booster limpio, de agudos o de medios.",
                       { choice ("Tipo", { "Limpio", "Agudos", "Medios" }, 0), knob ("Ganancia", 0.0f, 24.0f, 8.0f, "dB"),
                         knob ("Brillo", -10.0f, 10.0f, 0.0f, "dB", 1.0f, true) }, make<BoosterModule>() });
        m.push_back ({ "Octavador", "Saturación", forGuitar, cSat, "Añade octavas graves y aguda.",
                       { knob ("Octava -1", 0.0f, 100.0f, 70.0f, "%"), knob ("Octava -2", 0.0f, 100.0f, 0.0f, "%"),
                         knob ("Octava +1", 0.0f, 100.0f, 0.0f, "%"), knob ("Directo", 0.0f, 100.0f, 100.0f, "%"),
                         knob ("Tono", 0.0f, 1.0f, 0.4f) }, make<OctaverModule>() });

        m.push_back ({ "Wah Lunar", "Filtro", forGuitar, cFilt, "Wah manual, automático o por envolvente.",
                       { choice ("Modo", { "Pedal", "Automático", "Envolvente" }, 0), knob ("Pedal", 0.0f, 1.0f, 0.5f),
                         knob ("Velocidad", 0.1f, 8.0f, 1.5f, "Hz", 0.5f), knob ("Sensibilidad", 0.0f, 1.0f, 0.5f),
                         knob ("Resonancia", 1.0f, 12.0f, 5.0f, "", 0.6f), knob ("Mezcla", 0.0f, 100.0f, 100.0f, "%") }, make<WahModule>() });
        m.push_back ({ "Ecualizador", "Filtro", forBoth, cFilt, "Cinco bandas para esculpir el tono.",
                       { knob ("90 Hz", -15.0f, 15.0f, 0.0f, "dB", 1.0f, true), knob ("250 Hz", -15.0f, 15.0f, 0.0f, "dB", 1.0f, true),
                         knob ("800 Hz", -15.0f, 15.0f, 0.0f, "dB", 1.0f, true), knob ("2,5 kHz", -15.0f, 15.0f, 0.0f, "dB", 1.0f, true),
                         knob ("6 kHz", -15.0f, 15.0f, 0.0f, "dB", 1.0f, true), knob ("Salida", -12.0f, 12.0f, 0.0f, "dB", 1.0f, true) },
                       make<Eq5Module>() });

        m.push_back ({ "Amplificador", "Amplificadores", forGuitar, cAmp, "Seis amplis propios, del limpio al bajo.",
                       { choice ("Modelo", { "Cristal (limpio)", "Dorado (combo 50)", "Britannia (crunch)", "Corona (británico caliente)",
                                             "Titán (alta ganancia)", "Abismo (bajo)" }, 2),
                         knob ("Ganancia", 0.0f, 1.0f, 0.5f), knob ("Graves", 0.0f, 1.0f, 0.5f), knob ("Medios", 0.0f, 1.0f, 0.5f),
                         knob ("Agudos", 0.0f, 1.0f, 0.5f), knob ("Presencia", 0.0f, 1.0f, 0.5f), knob ("Master", 0.0f, 1.0f, 0.5f),
                         knob ("Profundidad", 0.0f, 1.0f, 0.3f) }, make<AmpModule>() });
        m.push_back ({ "Pantalla", "Amplificadores", forGuitar, cAmp, "Altavoz y micrófono, o tu propia IR.",
                       { choice ("Tipo", { "1x12 abierta", "2x12 vintage", "4x12 Britannia", "4x12 Titán", "1x15 bajo", "4x10 bajo",
                                           "Altavoz pequeño", "IR propia" }, 2),
                         knob ("Micro", 0.0f, 1.0f, 0.45f), knob ("Corte graves", 20.0f, 250.0f, 40.0f, "Hz", 0.5f),
                         knob ("Corte agudos", 3000.0f, 16000.0f, 12000.0f, "Hz", 0.5f), knob ("Sala", 0.0f, 1.0f, 0.0f) }, make<CabModule>() });

        m.push_back ({ "Chorus", "Modulación", forBoth, cMod, "Engorda y abre el sonido.",
                       { knob ("Velocidad", 0.05f, 5.0f, 0.8f, "Hz", 0.5f), knob ("Profundidad", 0.0f, 1.0f, 0.5f),
                         knob ("Retardo", 5.0f, 25.0f, 12.0f, "ms"), knob ("Mezcla", 0.0f, 100.0f, 50.0f, "%"),
                         knob ("Anchura", 0.0f, 1.0f, 1.0f) }, make<ChorusModule>() });
        m.push_back ({ "Flanger", "Modulación", forBoth, cMod, "Barrido de avión a reacción.",
                       { knob ("Velocidad", 0.05f, 5.0f, 0.3f, "Hz", 0.5f), knob ("Profundidad", 0.0f, 1.0f, 0.7f),
                         knob ("Retardo", 0.3f, 6.0f, 2.0f, "ms"), knob ("Realimentación", -95.0f, 95.0f, 50.0f, "%", 1.0f, true),
                         knob ("Mezcla", 0.0f, 100.0f, 50.0f, "%") }, make<FlangerModule>() });
        m.push_back ({ "Phaser", "Modulación", forBoth, cMod, "Fase giratoria de 2 a 12 etapas.",
                       { choice ("Etapas", { "2", "4", "6", "8", "10", "12" }, 1), knob ("Velocidad", 0.05f, 8.0f, 0.5f, "Hz", 0.5f),
                         knob ("Profundidad", 0.0f, 1.0f, 0.7f), knob ("Centro", 200.0f, 2500.0f, 700.0f, "Hz", 0.5f),
                         knob ("Realimentación", 0.0f, 95.0f, 40.0f, "%"), knob ("Mezcla", 0.0f, 100.0f, 50.0f, "%"),
                         knob ("Estéreo", 0.0f, 180.0f, 90.0f, "º") }, make<PhaserModule>() });
        m.push_back ({ "Vibrato", "Modulación", forBoth, cMod, "Oscilación de afinación.",
                       { knob ("Velocidad", 0.5f, 12.0f, 5.0f, "Hz", 0.6f), knob ("Profundidad", 0.0f, 1.0f, 0.3f) }, make<VibratoModule>() });
        m.push_back ({ "Trémolo", "Modulación", forBoth, cMod, "Pulsación de volumen, también armónica.",
                       { choice ("Forma", { "Seno", "Triángulo", "Cuadrada", "Armónico" }, 0),
                         knob ("Velocidad", 0.5f, 15.0f, 4.0f, "Hz", 0.6f),
                         choice ("Sincronía", { "Libre", "1/4", "1/4 con puntillo", "1/8", "1/8 con puntillo", "1/8 tresillo", "1/16" }, 0),
                         knob ("Profundidad", 0.0f, 100.0f, 60.0f, "%"), knob ("Estéreo", 0.0f, 1.0f, 0.0f) }, make<TremoloModule>() });
        m.push_back ({ "Rotativo", "Modulación", forBoth, cMod, "Altavoz giratorio con bocina y tambor.",
                       { choice ("Velocidad", { "Lento", "Rápido" }, 1), knob ("Aceleración", 0.2f, 5.0f, 1.2f, "s", 0.6f),
                         knob ("Bocina", 0.0f, 1.0f, 0.5f), knob ("Saturación", 0.0f, 1.0f, 0.2f), knob ("Anchura", 0.0f, 1.0f, 0.8f) },
                       make<RotaryModule>() });

        m.push_back ({ "Delay", "Delay", forBoth, cDly, "Digital, cinta, analógico, ping-pong, multitap o inverso.",
                       { choice ("Tipo", { "Digital", "Cinta", "Analógico", "Ping-pong", "Multitap", "Inverso" }, 0),
                         knob ("Tiempo", 10.0f, 2000.0f, 380.0f, "ms", 0.4f), choice ("Sincronía", syncNames, 0),
                         knob ("Realimentación", 0.0f, 100.0f, 35.0f, "%"), knob ("Tono", 0.0f, 1.0f, 0.6f),
                         knob ("Modulación", 0.0f, 1.0f, 0.15f), knob ("Mezcla", 0.0f, 100.0f, 30.0f, "%") }, make<DelayModule>() });
        m.push_back ({ "Reverb", "Reverb", forBoth, cRev, "Habitación, sala, placa, muelle, catedral o shimmer.",
                       { choice ("Tipo", { "Habitación", "Sala", "Placa", "Muelle", "Catedral", "Shimmer" }, 1),
                         knob ("Decaimiento", 0.0f, 1.0f, 0.5f), knob ("Pre-retardo", 0.0f, 200.0f, 15.0f, "ms", 0.6f),
                         knob ("Tono", 0.0f, 1.0f, 0.6f), knob ("Modulación", 0.0f, 1.0f, 0.3f),
                         knob ("Mezcla", 0.0f, 100.0f, 25.0f, "%"), knob ("Anchura", 0.0f, 1.0f, 1.0f) }, make<ReverbModule>() });

        m.push_back ({ "Volumen", "Utilidad", forBoth, cUtil, "Nivel y panorama.",
                       { knob ("Volumen", -40.0f, 12.0f, 0.0f, "dB", 1.6f), knob ("Panorama", -1.0f, 1.0f, 0.0f, "", 1.0f, true) },
                       make<VolumeModule>() });

        m.push_back ({ "Ecualizador vocal", "Voz", forVoice, cVoz, "Filtro de graves, cuerpo, presencia y aire.",
                       { knob ("Filtro graves", 20.0f, 300.0f, 80.0f, "Hz", 0.5f), knob ("Cuerpo", -12.0f, 12.0f, 0.0f, "dB", 1.0f, true),
                         knob ("Nasal", -12.0f, 12.0f, 0.0f, "dB", 1.0f, true), knob ("Presencia", -12.0f, 12.0f, 2.0f, "dB", 1.0f, true),
                         knob ("Aire", -12.0f, 12.0f, 3.0f, "dB", 1.0f, true) }, make<VocalEqModule>() });
        m.push_back ({ "De-esser", "Voz", forVoice, cVoz, "Suaviza las eses y los silbidos.",
                       { knob ("Frecuencia", 3000.0f, 10000.0f, 6000.0f, "Hz", 0.6f), knob ("Umbral", -50.0f, 0.0f, -26.0f, "dB"),
                         knob ("Reducción", 0.0f, 24.0f, 10.0f, "dB") }, make<DeesserModule>() });
        m.push_back ({ "Saturación cinta", "Voz", forBoth, cVoz, "Calidez y compresión de cinta.",
                       { knob ("Entrada", 0.0f, 24.0f, 6.0f, "dB"), knob ("Calidez", 0.0f, 1.0f, 0.5f),
                         knob ("Salida", -24.0f, 6.0f, 0.0f, "dB"), knob ("Mezcla", 0.0f, 100.0f, 100.0f, "%") }, make<TapeModule>() });
        m.push_back ({ "Doblador", "Voz", forVoice, cVoz, "Simula una segunda toma a cada lado.",
                       { knob ("Separación", 8.0f, 40.0f, 18.0f, "ms"), knob ("Variación", 0.0f, 1.0f, 0.4f),
                         knob ("Anchura", 0.0f, 1.0f, 0.9f), knob ("Mezcla", 0.0f, 100.0f, 45.0f, "%") }, make<DoublerModule>() });
        m.push_back ({ "Afinación", "Voz", forVoice, cVoz, "Corrige la afinación a la escala elegida.",
                       { choice ("Tonalidad", { "Do", "Do#", "Re", "Re#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si" }, 0),
                         choice ("Escala", { "Cromática", "Mayor", "Menor", "Pentatónica mayor", "Pentatónica menor" }, 0),
                         knob ("Velocidad", 0.0f, 1.0f, 0.6f), knob ("Mezcla", 0.0f, 100.0f, 100.0f, "%") }, make<AutotuneModule>() });
        m.push_back ({ "Lo-fi", "Utilidad", forBoth, cUtil, "Radio, teléfono y sonido de vinilo.",
                       { knob ("Filtro", 0.0f, 1.0f, 0.6f), knob ("Bits", 3.0f, 16.0f, 10.0f, "bits"),
                         knob ("Muestreo", 1.0f, 24.0f, 3.0f, "x"), knob ("Ruido", 0.0f, 1.0f, 0.2f),
                         knob ("Mezcla", 0.0f, 100.0f, 100.0f, "%") }, make<LofiModule>() });

        jassert ((int) m.size() == numTypes);
        return m;
    }();
    return list;
}

const ModuleInfo& info (int type)
{
    const auto& c = catalog();
    return c[(size_t) juce::jlimit (0, (int) c.size() - 1, type)];
}

const juce::StringArray& categories()
{
    static const juce::StringArray cats = [] {
        juce::StringArray a;
        for (auto* c : { "Dinámica", "Saturación", "Amplificadores", "Filtro", "Modulación", "Delay", "Reverb", "Voz", "Utilidad" })
            a.add (juce::String::fromUTF8 (c));
        return a;
    }();
    return cats;
}
} // namespace geminis::fx
