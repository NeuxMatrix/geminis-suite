#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../fx/Modules.h"
#include "InstParams.h"

namespace geminis::inst
{
/** Cadena de efectos de Geminis Instrument: saturación, compresor, modulación, delay y reverb.
    Reutiliza los módulos de Geminis FX con controles simplificados. */
class InstFx
{
public:
    explicit InstFx (juce::AudioProcessorValueTreeState& state)
    {
        auto raw = [&state] (const char* id) { return state.getRawParameterValue (id); };
        satOn = raw ("fx_sat_on"); satType = raw ("fx_sat_type"); satAmt = raw ("fx_sat_amt"); satMix = raw ("fx_sat_mix");
        cmpOn = raw ("fx_cmp_on"); cmpAmt = raw ("fx_cmp_amt");
        modOn = raw ("fx_mod_on"); modType = raw ("fx_mod_type"); modRate = raw ("fx_mod_rate"); modDepth = raw ("fx_mod_depth"); modMix = raw ("fx_mod_mix");
        dlyOn = raw ("fx_dly_on"); dlyType = raw ("fx_dly_type"); dlyDiv = raw ("fx_dly_div"); dlyFb = raw ("fx_dly_fb"); dlyMix = raw ("fx_dly_mix");
        revOn = raw ("fx_rev_on"); revType = raw ("fx_rev_type"); revSize = raw ("fx_rev_size"); revMix = raw ("fx_rev_mix");
    }

    void prepare (double sampleRate, int maxBlock)
    {
        using namespace geminis::fx;
        sr = sampleRate;
        const int types[] = { tape, overdrive, distortion, lofi, compressor, chorus, phaser, flanger, delay, reverb };
        for (int i = 0; i < 10; ++i)
        {
            mods[(size_t) i] = info (types[i]).create();
            mods[(size_t) i]->prepare (sampleRate, maxBlock);
        }
        dry.setSize (2, maxBlock);
        lastSat = lastMod = -1;
    }

    void reset() { for (auto& m : mods) if (m) m->reset(); }

    void process (float* L, float* R, int n, double bpm)
    {
        using namespace geminis::fx;
        Context ctx;
        ctx.sampleRate = sr;
        ctx.bpm = bpm;
        ctx.stereo = true;
        std::array<float, 8> v {};

        if (*satOn > 0.5f)
        {
            const int t = juce::roundToInt (satType->load());
            if (t != lastSat) { mods[(size_t) t]->reset(); lastSat = t; }
            const float a = satAmt->load();
            switch (t)
            {
                case 0: v = { 2.0f + a * 20.0f, 0.4f + a * 0.4f, -1.0f - a * 6.0f, 100.0f }; break;          // cinta
                case 1: v = { 0.15f + a * 0.8f, 0.55f, 0.40f - a * 0.2f }; break;                          // válvula
                case 2: v = { 0.2f + a * 0.8f, 0.55f, 0.38f - a * 0.04f, 90.0f }; break;                    // furia
                default: v = { 0.3f + a * 0.45f, 12.0f - a * 8.0f, 1.0f + a * 10.0f, 0.15f + a * 0.3f, 100.0f }; break; // lo-fi
            }
            processWet (*mods[(size_t) t], L, R, n, v.data(), ctx, satMix->load() * 0.01f);
        }
        if (*cmpOn > 0.5f)
        {
            const float a = cmpAmt->load();
            v = { -6.0f - a * 30.0f, 2.0f + a * 6.0f, 6.0f, 120.0f, a * 10.0f, 100.0f };
            mods[4]->process (L, R, n, v.data(), ctx);
        }
        if (*modOn > 0.5f)
        {
            const int t = juce::roundToInt (modType->load());
            if (t != lastMod) { mods[(size_t) (5 + t)]->reset(); lastMod = t; }
            const float rate = modRate->load(), depth = modDepth->load(), mix = modMix->load();
            if (t == 0)      v = { rate, depth, 12.0f, mix, 1.0f };
            else if (t == 1) v = { 2.0f, rate, depth, 800.0f, 45.0f, mix, 90.0f };
            else             v = { rate * 0.5f, depth, 2.0f, 55.0f, mix };
            mods[(size_t) (5 + t)]->process (L, R, n, v.data(), ctx);
        }
        if (*dlyOn > 0.5f)
        {
            static const int typeMap[] = { 0, 1, 3 };
            static const int divMap[] = { 1, 2, 3, 4, 5, 6, 7 };
            const int t = typeMap[juce::jlimit (0, 2, juce::roundToInt (dlyType->load()))];
            const int dv = divMap[juce::jlimit (0, 6, juce::roundToInt (dlyDiv->load()))];
            v = { (float) t, 380.0f, (float) dv, dlyFb->load(), 0.55f, 0.12f, dlyMix->load() };
            mods[8]->process (L, R, n, v.data(), ctx);
        }
        if (*revOn > 0.5f)
        {
            static const int typeMap[] = { 0, 1, 2, 4, 5 };
            const int t = typeMap[juce::jlimit (0, 4, juce::roundToInt (revType->load()))];
            v = { (float) t, revSize->load(), 12.0f, 0.55f, 0.3f, revMix->load(), 1.0f };
            mods[9]->process (L, R, n, v.data(), ctx);
        }
    }

    int getLatency() const
    {
        int lat = 0;
        if (*satOn > 0.5f && lastSat >= 0) lat += mods[(size_t) lastSat]->getLatency();
        return lat;
    }

private:
    void processWet (geminis::fx::Module& m, float* L, float* R, int n, const float* v, geminis::fx::Context& ctx, float mix)
    {
        if (mix >= 0.999f) { m.process (L, R, n, v, ctx); return; }
        dry.copyFrom (0, 0, L, n);
        dry.copyFrom (1, 0, R, n);
        m.process (L, R, n, v, ctx);
        const float* dl = dry.getReadPointer (0);
        const float* dr = dry.getReadPointer (1);
        for (int i = 0; i < n; ++i)
        {
            L[i] = dl[i] + (L[i] - dl[i]) * mix;
            R[i] = dr[i] + (R[i] - dr[i]) * mix;
        }
    }

    double sr = 44100.0;
    std::array<std::unique_ptr<geminis::fx::Module>, 10> mods;
    juce::AudioBuffer<float> dry;
    int lastSat = -1, lastMod = -1;
    std::atomic<float> *satOn, *satType, *satAmt, *satMix, *cmpOn, *cmpAmt, *modOn, *modType, *modRate, *modDepth, *modMix,
                       *dlyOn, *dlyType, *dlyDiv, *dlyFb, *dlyMix, *revOn, *revType, *revSize, *revMix;
};
} // namespace geminis::inst
