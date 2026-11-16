#include "SynthPresets.h"

namespace geminis::inst
{
namespace
{
    using V = std::vector<std::pair<juce::String, float>>;

    SynthPreset P (const char* name, const char* cat, V values, int d1 = -1, int d2 = -1)
    {
        SynthPreset p;
        p.name = juce::String::fromUTF8 (name);
        p.category = juce::String::fromUTF8 (cat);
        p.values = std::move (values);
        p.drawn1 = d1;
        p.drawn2 = d2;
        return p;
    }

    // Referencia rápida de valores de elección:
    // onda: 0 seno 1 triángulo 2 sierra 3 cuadrada 4 pulso 5 órgano 6 coro 7 metálica 8 cristal 9 digital 10 dibujada
    // curva: 0 curvar 1 plegar 2 sincro 3 formante 4 bits · filtro: 0 PB24 1 PB12 2 banda 3 PA 4 muesca
    // fuentes: 1 LFO1 2 LFO2 3 EnvF 4 EnvMod 5 vel 6 rueda 7 AT 8 nota 9 aleatorio
    // destinos: 1 curva1 2 curva2 3 tono1 4 tono2 5 tono 6 nivel1 7 nivel2 8 FM 9 anillo 10 corte 11 reso 12 volumen 13 pan 14 desafinación 15 velLFO2 16 sub 17 ruido
    // sincronía LFO: 0 libre 3 1 compás 4 1/2 5 1/4 7 1/8 8 1/8. 9 1/8T 10 1/16 · delay: 0 1/2 1 1/4 2 1/4. 3 1/8 4 1/8. 5 1/8T 6 1/16
    // reverb: 0 habitación 1 sala 2 placa 3 catedral 4 shimmer · saturación: 0 cinta 1 válvula 2 furia 3 lo-fi
}

bool isSynthParam (const juce::String& id)
{
    static const juce::StringArray prefixes { "o1_", "o2_", "f_", "ea_", "ef_", "em_", "l1_", "l2_", "m1_", "m2_", "m3_", "m4_", "m5_", "m6_", "fx_" };
    static const juce::StringArray exact { "sub", "sub_shape", "noise", "fm", "ring", "voice_mode", "glide", "bend", "vel_sens", "width", "sy_vol" };
    if (exact.contains (id)) return true;
    for (const auto& p : prefixes) if (id.startsWith (p)) return true;
    return false;
}

const std::vector<SynthPreset>& synthPresets()
{
    static const std::vector<SynthPreset> list = {
        P ("Inicial", "Inicial", { { "sy_vol", -3.0f }, { "o1_ctype", 2 }, { "o2_ctype", 2 }, { "m1_src", 6 }, { "m1_dst", 1 }, { "m1_amt", 0.8f },
            { "m2_src", 6 }, { "m2_dst", 2 }, { "m2_amt", 0.6f } }),

        //================================================================== BAJOS
        P ("Bajo analógico redondo", "Bajos", { { "sy_vol", -3.0f }, { "o2_wave", 3 }, { "o2_fine", -5 }, { "o2_level", 0.5f }, { "sub", 0.5f },
            { "f_cut", 320 }, { "f_res", 0.25f }, { "f_env", 0.45f }, { "f_key", 0.3f }, { "ef_a", 0.001f }, { "ef_d", 0.25f }, { "ef_s", 0.1f }, { "ef_r", 0.2f },
            { "ea_a", 0.002f }, { "ea_d", 0.4f }, { "ea_s", 1.0f }, { "ea_r", 0.12f }, { "voice_mode", 1 }, { "glide", 0.04f },
            { "fx_rev_on", 0 }, { "fx_cmp_on", 1 }, { "fx_cmp_amt", 0.3f }, { "m1_src", 6 }, { "m1_dst", 10 }, { "m1_amt", 0.5f } }),
        P ("Bajo curvado", "Bajos", { { "sy_vol", 4.5f }, { "o1_wave", 0 }, { "o1_curve", 0.55f }, { "o2_level", 0 }, { "sub", 0.4f }, { "f_type", 1 }, { "f_cut", 2500 },
            { "f_res", 0.1f }, { "f_env", 0.2f }, { "em_a", 0.001f }, { "em_d", 0.25f }, { "em_s", 0 }, { "m1_src", 4 }, { "m1_dst", 1 }, { "m1_amt", 0.4f },
            { "m2_src", 6 }, { "m2_dst", 1 }, { "m2_amt", 0.4f }, { "ea_d", 0.5f }, { "ea_s", 0.9f }, { "ea_r", 0.1f }, { "voice_mode", 1 }, { "fx_rev_on", 0 } }),
        P ("Bajo de goma", "Bajos", { { "sy_vol", 2.0f }, { "o1_wave", 1 }, { "o1_ctype", 1 }, { "o1_curve", 0.3f }, { "o2_level", 0 }, { "sub", 0.3f },
            { "m1_src", 3 }, { "m1_dst", 1 }, { "m1_amt", 0.5f }, { "f_cut", 600 }, { "f_res", 0.4f }, { "f_env", 0.5f }, { "ef_d", 0.18f }, { "ef_s", 0 },
            { "ea_s", 0.9f }, { "ea_r", 0.1f }, { "voice_mode", 2 }, { "glide", 0.08f }, { "fx_rev_on", 0 } }),
        P ("Sub 808", "Bajos", { { "sy_vol", 10.0f }, { "o1_wave", 0 }, { "o2_level", 0 }, { "o1_level", 1.0f }, { "f_cut", 900 }, { "f_res", 0 }, { "f_env", 0 },
            { "ea_a", 0.002f }, { "ea_d", 1.4f }, { "ea_s", 0 }, { "ea_r", 0.3f }, { "voice_mode", 1 }, { "glide", 0.06f },
            { "fx_sat_on", 1 }, { "fx_sat_type", 1 }, { "fx_sat_amt", 0.35f }, { "fx_sat_mix", 60 }, { "fx_rev_on", 0 }, { "m1_src", 6 }, { "m1_dst", 1 }, { "m1_amt", 0.4f } }),
        P ("Bajo ácido", "Bajos", { { "sy_vol", -0.5f }, { "o2_level", 0 }, { "f_cut", 350 }, { "f_res", 0.78f }, { "f_env", 0.7f }, { "f_drive", 0.4f }, { "ef_d", 0.2f },
            { "ef_s", 0 }, { "ea_s", 0.9f }, { "ea_r", 0.08f }, { "voice_mode", 2 }, { "glide", 0.07f }, { "m1_src", 5 }, { "m1_dst", 10 }, { "m1_amt", 0.3f },
            { "m2_src", 6 }, { "m2_dst", 11 }, { "m2_amt", 0.2f }, { "fx_dly_on", 1 }, { "fx_dly_div", 4 }, { "fx_dly_fb", 25 }, { "fx_dly_mix", 15 }, { "fx_rev_on", 0 } }),
        P ("Bajo Reese", "Bajos", { { "sy_vol", 2.5f }, { "o1_uni", 3 }, { "o1_det", 0.35f }, { "o2_uni", 3 }, { "o2_det", 0.45f }, { "o2_fine", -12 }, { "f_cut", 700 },
            { "f_res", 0.2f }, { "f_env", 0.1f }, { "sub", 0.3f }, { "voice_mode", 1 }, { "width", 0.5f }, { "l1_rate", 0.3f },
            { "m1_src", 1 }, { "m1_dst", 10 }, { "m1_amt", 0.15f }, { "fx_sat_on", 1 }, { "fx_sat_amt", 0.35f }, { "fx_rev_on", 0 } }),
        P ("Bajo FM", "Bajos", { { "sy_vol", 1.5f }, { "o1_wave", 0 }, { "o2_wave", 0 }, { "o2_oct", 1 }, { "o2_level", 0 }, { "o2_fine", 0 }, { "fm", 0.45f },
            { "em_a", 0.001f }, { "em_d", 0.3f }, { "em_s", 0 }, { "m1_src", 4 }, { "m1_dst", 8 }, { "m1_amt", 0.4f }, { "f_type", 1 }, { "f_cut", 4000 },
            { "f_env", 0 }, { "ea_s", 0.85f }, { "ea_r", 0.1f }, { "voice_mode", 1 }, { "fx_rev_on", 0 }, { "m2_src", 6 }, { "m2_dst", 8 }, { "m2_amt", 0.4f } }),
        P ("Bajo sincronizado", "Bajos", { { "sy_vol", 3.5f }, { "o1_ctype", 2 }, { "o1_curve", 0.3f }, { "m1_src", 3 }, { "m1_dst", 1 }, { "m1_amt", 0.5f },
            { "o2_wave", 3 }, { "o2_oct", -1 }, { "o2_level", 0.4f }, { "f_cut", 1200 }, { "f_res", 0.3f }, { "f_env", 0.3f }, { "ef_d", 0.3f }, { "ef_s", 0.1f },
            { "voice_mode", 1 }, { "fx_rev_on", 0 } }),
        P ("Bajo industrial", "Bajos", { { "sy_vol", 9.5f }, { "o1_wave", 3 }, { "o1_ctype", 4 }, { "o1_curve", 0.45f }, { "o2_fine", 15 }, { "f_cut", 900 }, { "f_res", 0.35f },
            { "f_env", 0.4f }, { "f_drive", 0.6f }, { "fx_sat_on", 1 }, { "fx_sat_type", 2 }, { "fx_sat_amt", 0.5f }, { "fx_sat_mix", 70 },
            { "voice_mode", 1 }, { "fx_rev_on", 0 }, { "m1_src", 6 }, { "m1_dst", 1 }, { "m1_amt", 0.5f } }),
        P ("Bajo de pulso", "Bajos", { { "sy_vol", 4.0f }, { "o1_wave", 4 }, { "o2_wave", 4 }, { "o2_fine", 6 }, { "l1_rate", 0.4f }, { "m1_src", 1 }, { "m1_dst", 1 },
            { "m1_amt", 0.25f }, { "f_cut", 500 }, { "f_res", 0.2f }, { "f_env", 0.35f }, { "voice_mode", 1 }, { "fx_rev_on", 0 } }),

        //================================================================== LEADS
        P ("Lead cósmico", "Leads", { { "sy_vol", 3.5f }, { "o1_uni", 5 }, { "o1_det", 0.3f }, { "o2_wave", 3 }, { "o2_oct", 1 }, { "o2_level", 0.3f }, { "f_cut", 3500 },
            { "f_res", 0.25f }, { "f_env", 0.2f }, { "voice_mode", 2 }, { "glide", 0.06f }, { "m1_src", 2 }, { "m1_dst", 5 }, { "m1_amt", 0.015f },
            { "m2_src", 6 }, { "m2_dst", 10 }, { "m2_amt", 0.4f }, { "fx_dly_on", 1 }, { "fx_dly_div", 4 }, { "fx_dly_fb", 35 }, { "fx_dly_mix", 22 },
            { "fx_rev_mix", 20 } }),
        P ("Lead curvo", "Leads", { { "sy_vol", 6.5f }, { "o1_wave", 0 }, { "o1_curve", 0.6f }, { "m1_src", 6 }, { "m1_dst", 1 }, { "m1_amt", 0.4f }, { "l1_rate", 0.2f },
            { "m2_src", 1 }, { "m2_dst", 1 }, { "m2_amt", 0.15f }, { "o2_wave", 0 }, { "o2_curve", 0.4f }, { "o2_oct", 1 }, { "o2_level", 0.3f },
            { "f_type", 1 }, { "f_cut", 8000 }, { "f_env", 0 }, { "voice_mode", 1 }, { "glide", 0.05f }, { "fx_dly_on", 1 }, { "fx_dly_mix", 18 } }),
        P ("Lead chillón", "Leads", { { "sy_vol", -7.5f }, { "o1_ctype", 2 }, { "o1_curve", 0.5f }, { "em_a", 0.001f }, { "em_d", 0.4f }, { "em_s", 0.2f },
            { "m1_src", 4 }, { "m1_dst", 1 }, { "m1_amt", 0.4f }, { "m2_src", 6 }, { "m2_dst", 1 }, { "m2_amt", 0.3f }, { "f_drive", 0.5f }, { "f_cut", 5000 },
            { "f_res", 0.3f }, { "fx_sat_on", 1 }, { "fx_sat_type", 1 }, { "fx_sat_amt", 0.5f }, { "voice_mode", 1 }, { "fx_dly_on", 1 }, { "fx_dly_mix", 15 } }),
        P ("Lead plegado", "Leads", { { "sy_vol", 2.5f }, { "o1_wave", 1 }, { "o1_ctype", 1 }, { "o1_curve", 0.45f }, { "l1_sync", 5 }, { "m1_src", 1 }, { "m1_dst", 1 },
            { "m1_amt", 0.3f }, { "o2_level", 0 }, { "f_type", 1 }, { "f_cut", 6000 }, { "voice_mode", 2 }, { "fx_dly_on", 1 }, { "fx_dly_div", 1 }, { "fx_dly_mix", 20 } }),
        P ("Lead cuadrado clásico", "Leads", { { "sy_vol", -1.0f }, { "o1_wave", 3 }, { "o2_wave", 3 }, { "o2_fine", 10 }, { "o2_oct", -1 }, { "o2_level", 0.5f },
            { "f_cut", 2800 }, { "f_res", 0.2f }, { "f_env", 0.25f }, { "voice_mode", 1 }, { "glide", 0.03f }, { "fx_mod_on", 1 },
            { "m1_src", 6 }, { "m1_dst", 10 }, { "m1_amt", 0.5f } }),
        P ("Lead de ocho bits", "Leads", { { "sy_vol", -1.0f }, { "o1_wave", 3 }, { "o1_ctype", 4 }, { "o1_curve", 0.6f }, { "o2_wave", 4 }, { "o2_ctype", 4 }, { "o2_curve", 0.6f },
            { "o2_oct", 1 }, { "o2_level", 0.35f }, { "o2_fine", 0 }, { "f_type", 1 }, { "f_cut", 12000 }, { "f_res", 0 }, { "f_env", 0 },
            { "voice_mode", 1 }, { "fx_rev_mix", 10 }, { "m1_src", 6 }, { "m1_dst", 1 }, { "m1_amt", 0.4f } }),
        P ("Lead vocal", "Leads", { { "sy_vol", 9.5f }, { "o1_wave", 6 }, { "o1_ctype", 3 }, { "o1_curve", 0.3f }, { "m1_src", 6 }, { "m1_dst", 1 }, { "m1_amt", 0.5f },
            { "m2_src", 1 }, { "m2_dst", 1 }, { "m2_amt", 0.1f }, { "o2_level", 0 }, { "f_type", 1 }, { "f_cut", 7000 }, { "f_env", 0 },
            { "voice_mode", 1 }, { "glide", 0.08f }, { "fx_rev_type", 2 }, { "fx_rev_mix", 25 } }),
        P ("Lead metálico", "Leads", { { "sy_vol", 5.5f }, { "o1_wave", 7 }, { "o1_curve", 0.2f }, { "o2_wave", 0 }, { "o2_oct", 1 }, { "o2_semi", 7 }, { "o2_level", 0 },
            { "ring", 0.4f }, { "f_cut", 4000 }, { "f_res", 0.2f }, { "voice_mode", 1 }, { "fx_dly_on", 1 }, { "fx_dly_mix", 18 },
            { "m1_src", 6 }, { "m1_dst", 9 }, { "m1_amt", 0.5f } }),
        P ("Silbido espacial", "Leads", { { "sy_vol", 0.0f }, { "o1_wave", 0 }, { "o2_wave", 0 }, { "o2_oct", 2 }, { "o2_level", 0.15f }, { "glide", 0.2f }, { "voice_mode", 2 },
            { "l2_rate", 5.5f }, { "m1_src", 2 }, { "m1_dst", 5 }, { "m1_amt", 0.02f }, { "f_type", 1 }, { "f_cut", 15000 }, { "f_env", 0 },
            { "fx_dly_on", 1 }, { "fx_dly_div", 2 }, { "fx_dly_fb", 45 }, { "fx_dly_mix", 30 }, { "fx_rev_type", 3 }, { "fx_rev_mix", 30 } }),

        //================================================================== PADS
        P ("Pad nebulosa", "Pads", { { "sy_vol", -2.5f }, { "o1_uni", 5 }, { "o1_det", 0.4f }, { "o2_uni", 5 }, { "o2_det", 0.5f }, { "o2_oct", 1 }, { "o2_level", 0.4f },
            { "f_cut", 1800 }, { "f_env", 0.2f }, { "ea_a", 1.2f }, { "ea_d", 1 }, { "ea_s", 0.9f }, { "ea_r", 2.5f }, { "ef_a", 1.5f }, { "ef_d", 2 },
            { "ef_s", 0.5f }, { "ef_r", 2 }, { "l1_rate", 0.15f }, { "m1_src", 1 }, { "m1_dst", 10 }, { "m1_amt", 0.2f }, { "fx_mod_on", 1 },
            { "fx_rev_type", 3 }, { "fx_rev_size", 0.8f }, { "fx_rev_mix", 35 } }),
        P ("Pad curvo que respira", "Pads", { { "sy_vol", -3.5f }, { "o1_wave", 1 }, { "l1_rate", 0.12f }, { "m1_src", 1 }, { "m1_dst", 1 }, { "m1_amt", 0.5f },
            { "o2_wave", 0 }, { "o2_curve", 0.3f }, { "o2_fine", 7 }, { "o2_level", 0.6f }, { "l2_rate", 0.2f }, { "m2_src", 2 }, { "m2_dst", 2 },
            { "m2_amt", 0.3f }, { "f_type", 1 }, { "f_cut", 5000 }, { "f_env", 0 }, { "ea_a", 1.5f }, { "ea_d", 1 }, { "ea_s", 0.9f }, { "ea_r", 3 },
            { "fx_rev_type", 4 }, { "fx_rev_mix", 30 } }),
        P ("Pad de cristal", "Pads", { { "sy_vol", -3.5f }, { "o1_wave", 8 }, { "o2_wave", 8 }, { "o2_oct", 1 }, { "o2_fine", 6 }, { "o2_level", 0.5f }, { "f_type", 1 },
            { "f_cut", 9000 }, { "f_env", 0 }, { "ea_a", 0.8f }, { "ea_d", 1.5f }, { "ea_s", 0.8f }, { "ea_r", 3 }, { "fx_mod_on", 1 },
            { "fx_rev_type", 4 }, { "fx_rev_size", 0.7f }, { "fx_rev_mix", 35 }, { "m1_src", 6 }, { "m1_dst", 1 }, { "m1_amt", 0.6f } }),
        P ("Coro celestial", "Pads", { { "sy_vol", 2.0f }, { "o1_wave", 6 }, { "o1_uni", 3 }, { "o1_det", 0.3f }, { "o1_ctype", 3 }, { "o1_curve", 0.2f }, { "o2_wave", 6 },
            { "o2_oct", -1 }, { "o2_level", 0.5f }, { "l1_rate", 0.25f }, { "m1_src", 1 }, { "m1_dst", 1 }, { "m1_amt", 0.15f }, { "f_type", 1 },
            { "f_cut", 6000 }, { "f_env", 0 }, { "ea_a", 1 }, { "ea_d", 1 }, { "ea_s", 0.9f }, { "ea_r", 2.5f }, { "fx_rev_type", 3 }, { "fx_rev_mix", 35 } }),
        P ("Pad oscuro", "Pads", { { "sy_vol", -4.5f }, { "o1_uni", 3 }, { "o2_wave", 3 }, { "o2_oct", -1 }, { "o2_level", 0.5f }, { "f_cut", 600 }, { "f_res", 0.3f },
            { "f_env", 0.25f }, { "ef_a", 2 }, { "ef_d", 3 }, { "ef_s", 0.4f }, { "ea_a", 2 }, { "ea_d", 2 }, { "ea_s", 0.9f }, { "ea_r", 3 },
            { "l1_rate", 0.07f }, { "m1_src", 1 }, { "m1_dst", 10 }, { "m1_amt", 0.15f }, { "fx_rev_type", 3 }, { "fx_rev_mix", 30 } }),
        P ("Cuerdas analógicas", "Pads", { { "sy_vol", -0.5f }, { "o1_uni", 7 }, { "o1_det", 0.35f }, { "o2_level", 0 }, { "f_cut", 3500 }, { "f_res", 0.05f }, { "f_env", 0 },
            { "ea_a", 0.6f }, { "ea_d", 1 }, { "ea_s", 0.85f }, { "ea_r", 1.5f }, { "fx_mod_on", 1 }, { "fx_mod_depth", 0.6f }, { "fx_mod_mix", 50 },
            { "fx_rev_mix", 25 } }),
        P ("Pad plegado lento", "Pads", { { "sy_vol", -6.5f }, { "o1_wave", 0 }, { "o1_ctype", 1 }, { "l1_sync", 2 }, { "m1_src", 1 }, { "m1_dst", 1 }, { "m1_amt", 0.4f },
            { "o2_wave", 1 }, { "o2_ctype", 1 }, { "o2_curve", 0.2f }, { "o2_oct", 1 }, { "o2_level", 0.4f }, { "f_type", 1 }, { "f_cut", 7000 },
            { "f_env", 0 }, { "ea_a", 1 }, { "ea_d", 1 }, { "ea_s", 0.9f }, { "ea_r", 2.5f }, { "fx_rev_type", 4 }, { "fx_rev_mix", 25 },
            { "fx_dly_on", 1 }, { "fx_dly_div", 2 }, { "fx_dly_mix", 15 } }),
        P ("Pad de ensueño", "Pads", { { "sy_vol", -2.5f }, { "o1_uni", 5 }, { "o2_wave", 0 }, { "o2_oct", 1 }, { "f_cut", 2400 }, { "f_res", 0.2f }, { "l2_shape", 5 },
            { "l2_rate", 0.5f }, { "m1_src", 2 }, { "m1_dst", 10 }, { "m1_amt", 0.2f }, { "ea_a", 1 }, { "ea_d", 1 }, { "ea_s", 0.8f }, { "ea_r", 3 },
            { "fx_mod_on", 1 }, { "fx_mod_type", 1 }, { "fx_rev_type", 4 }, { "fx_rev_mix", 30 } }),
        P ("Marea digital", "Pads", { { "sy_vol", -3.5f }, { "o1_wave", 9 }, { "o1_ctype", 2 }, { "o1_curve", 0.1f }, { "l1_rate", 0.1f }, { "m1_src", 1 }, { "m1_dst", 1 },
            { "m1_amt", 0.4f }, { "o2_wave", 9 }, { "o2_ctype", 4 }, { "o2_curve", 0.2f }, { "o2_fine", 9 }, { "o2_level", 0.5f }, { "f_cut", 4000 },
            { "f_env", 0 }, { "ea_a", 1.2f }, { "ea_d", 1 }, { "ea_s", 0.9f }, { "ea_r", 2.5f }, { "fx_rev_type", 3 }, { "fx_rev_mix", 30 } }),

        //================================================================== PULSADOS Y TECLAS
        P ("Pulsado brillante", "Pulsados y teclas", { { "sy_vol", 0.0f }, { "o2_wave", 3 }, { "o2_oct", 1 }, { "o2_level", 0.3f }, { "f_cut", 400 }, { "f_res", 0.25f },
            { "f_env", 0.75f }, { "ef_d", 0.25f }, { "ef_s", 0 }, { "ef_r", 0.2f }, { "ea_a", 0.001f }, { "ea_d", 0.5f }, { "ea_s", 0 }, { "ea_r", 0.3f },
            { "fx_dly_on", 1 }, { "fx_dly_div", 4 }, { "fx_dly_fb", 30 }, { "fx_dly_mix", 20 } }),
        P ("Pulsado curvado", "Pulsados y teclas", { { "sy_vol", 3.0f }, { "o1_wave", 0 }, { "o1_curve", 0.2f }, { "em_a", 0.001f }, { "em_d", 0.15f }, { "em_s", 0 },
            { "m1_src", 4 }, { "m1_dst", 1 }, { "m1_amt", 0.7f }, { "o2_level", 0 }, { "ea_a", 0.001f }, { "ea_d", 0.6f }, { "ea_s", 0 }, { "ea_r", 0.35f },
            { "f_type", 1 }, { "f_cut", 9000 }, { "f_env", 0 }, { "fx_dly_on", 1 }, { "fx_dly_div", 3 }, { "fx_dly_mix", 15 } }),
        P ("Teclado eléctrico FM", "Pulsados y teclas", { { "sy_vol", -3.5f }, { "o1_wave", 0 }, { "o2_wave", 0 }, { "o2_oct", 2 }, { "o2_level", 0 }, { "o2_fine", 0 },
            { "fm", 0.25f }, { "em_a", 0.001f }, { "em_d", 0.6f }, { "em_s", 0 }, { "em_r", 0.5f }, { "m1_src", 4 }, { "m1_dst", 8 }, { "m1_amt", 0.35f },
            { "m2_src", 5 }, { "m2_dst", 8 }, { "m2_amt", 0.3f }, { "ea_a", 0.001f }, { "ea_d", 1.8f }, { "ea_s", 0.2f }, { "ea_r", 0.6f },
            { "f_type", 1 }, { "f_cut", 12000 }, { "f_env", 0 }, { "fx_mod_on", 1 }, { "fx_mod_depth", 0.4f }, { "fx_rev_mix", 15 } }),
        P ("Campanas de cristal", "Pulsados y teclas", { { "sy_vol", -3.0f }, { "o1_wave", 8 }, { "o2_wave", 0 }, { "o2_semi", 7 }, { "o2_oct", 1 }, { "o2_level", 0.3f },
            { "ring", 0.25f }, { "ea_a", 0.001f }, { "ea_d", 2 }, { "ea_s", 0 }, { "ea_r", 2 }, { "f_type", 1 }, { "f_cut", 10000 }, { "f_env", 0 },
            { "fx_rev_type", 4 }, { "fx_rev_mix", 30 } }),
        P ("Órgano de ruedas", "Pulsados y teclas", { { "sy_vol", -4.0f }, { "o1_wave", 5 }, { "o2_level", 0 }, { "f_type", 1 }, { "f_cut", 12000 }, { "f_res", 0 }, { "f_env", 0 },
            { "ea_a", 0.005f }, { "ea_d", 0.1f }, { "ea_s", 1 }, { "ea_r", 0.06f }, { "fx_mod_on", 1 }, { "fx_mod_rate", 5.8f }, { "fx_mod_depth", 0.3f },
            { "fx_mod_mix", 40 }, { "fx_sat_on", 1 }, { "fx_sat_type", 1 }, { "fx_sat_amt", 0.25f }, { "m1_src", 6 }, { "m1_dst", 1 }, { "m1_amt", 0.5f } }),
        P ("Clavinet sintético", "Pulsados y teclas", { { "sy_vol", 12.0f }, { "o1_wave", 4 }, { "o1_curve", 0.3f }, { "o2_level", 0 }, { "f_type", 1 }, { "f_cut", 2400 },
            { "f_res", 0.45f }, { "f_env", 0.55f }, { "ef_d", 0.2f }, { "ef_s", 0.1f }, { "f_vel", 0.5f }, { "ea_a", 0.001f }, { "ea_d", 0.4f },
            { "ea_s", 0.3f }, { "ea_r", 0.1f }, { "fx_sat_on", 1 }, { "fx_sat_type", 1 }, { "fx_sat_amt", 0.2f }, { "fx_rev_mix", 10 } }),
        P ("Marimba digital", "Pulsados y teclas", { { "sy_vol", -1.0f }, { "o1_wave", 0 }, { "o1_ctype", 1 }, { "o1_curve", 0.2f }, { "em_d", 0.08f }, { "em_a", 0.001f },
            { "em_s", 0 }, { "m1_src", 4 }, { "m1_dst", 1 }, { "m1_amt", 0.5f }, { "o2_level", 0 }, { "ea_a", 0.001f }, { "ea_d", 0.5f }, { "ea_s", 0 },
            { "ea_r", 0.4f }, { "f_type", 1 }, { "f_cut", 8000 }, { "f_env", 0 }, { "fx_rev_type", 0 }, { "fx_rev_mix", 15 } }),
        P ("Arpa de luz", "Pulsados y teclas", { { "sy_vol", -2.5f }, { "o1_wave", 1 }, { "o2_wave", 0 }, { "o2_oct", 1 }, { "o2_level", 0.4f }, { "ea_a", 0.001f },
            { "ea_d", 1.2f }, { "ea_s", 0 }, { "ea_r", 1 }, { "f_type", 1 }, { "f_cut", 10000 }, { "f_env", 0 }, { "fx_dly_on", 1 }, { "fx_dly_div", 2 },
            { "fx_dly_mix", 25 }, { "fx_rev_type", 4 }, { "fx_rev_mix", 25 } }),

        //================================================================== SECUENCIAS
        P ("Secuencia sincronizada", "Secuencias", { { "sy_vol", 6.5f }, { "o1_ctype", 2 }, { "l1_shape", 2 }, { "l1_sync", 3 }, { "m1_src", 1 }, { "m1_dst", 1 },
            { "m1_amt", 0.6f }, { "o2_level", 0.3f }, { "f_cut", 2500 }, { "f_res", 0.3f }, { "f_env", 0.3f }, { "ea_a", 0.001f }, { "ea_d", 0.25f },
            { "ea_s", 0.3f }, { "ea_r", 0.15f }, { "fx_dly_on", 1 }, { "fx_dly_div", 4 }, { "fx_dly_mix", 20 } }),
        P ("Pulso con puerta", "Secuencias", { { "sy_vol", -2.5f }, { "o1_uni", 3 }, { "f_cut", 1500 }, { "f_res", 0.35f }, { "l1_shape", 3 }, { "l1_sync", 10 },
            { "m1_src", 1 }, { "m1_dst", 12 }, { "m1_amt", 0.9f }, { "ea_a", 0.01f }, { "ea_d", 0.3f }, { "ea_s", 0.9f }, { "ea_r", 0.3f }, { "fx_rev_mix", 18 } }),
        P ("Burbujas", "Secuencias", { { "sy_vol", 7.0f }, { "o1_wave", 3 }, { "o1_ctype", 3 }, { "o1_curve", 0.4f }, { "l2_shape", 4 }, { "l2_rate", 8 }, { "m1_src", 2 },
            { "m1_dst", 1 }, { "m1_amt", 0.5f }, { "o2_level", 0 }, { "f_type", 1 }, { "f_cut", 6000 }, { "ea_a", 0.001f }, { "ea_d", 0.2f }, { "ea_s", 0 },
            { "ea_r", 0.2f }, { "fx_dly_on", 1 }, { "fx_dly_div", 4 }, { "fx_dly_mix", 30 } }),
        P ("Arpegio de cristal", "Secuencias", { { "sy_vol", 8.5f }, { "o1_wave", 8 }, { "o1_ctype", 4 }, { "o1_curve", 0.2f }, { "o2_level", 0 }, { "ea_a", 0.001f },
            { "ea_d", 0.3f }, { "ea_s", 0 }, { "ea_r", 0.25f }, { "f_type", 1 }, { "f_cut", 11000 }, { "f_env", 0 }, { "fx_dly_on", 1 }, { "fx_dly_div", 4 },
            { "fx_dly_fb", 40 }, { "fx_dly_mix", 25 }, { "fx_rev_mix", 20 } }),
        P ("Bajo secuenciado", "Secuencias", { { "sy_vol", 4.5f }, { "o2_wave", 3 }, { "o2_oct", -1 }, { "o2_level", 0.5f }, { "f_cut", 500 }, { "f_res", 0.5f },
            { "f_env", 0.6f }, { "ef_d", 0.15f }, { "ef_s", 0 }, { "ea_a", 0.001f }, { "ea_d", 0.2f }, { "ea_s", 0 }, { "ea_r", 0.1f }, { "voice_mode", 1 },
            { "m1_src", 5 }, { "m1_dst", 10 }, { "m1_amt", 0.3f }, { "fx_rev_on", 0 } }),
        P ("Trance cortado", "Secuencias", { { "sy_vol", -2.5f }, { "o1_uni", 7 }, { "o1_det", 0.3f }, { "o2_oct", 1 }, { "o2_uni", 3 }, { "o2_level", 0.4f }, { "f_cut", 5000 },
            { "f_env", 0 }, { "l1_shape", 3 }, { "l1_sync", 10 }, { "m1_src", 1 }, { "m1_dst", 12 }, { "m1_amt", 0.9f }, { "fx_rev_mix", 20 },
            { "fx_dly_on", 1 }, { "fx_dly_div", 4 }, { "fx_dly_mix", 15 } }),

        //================================================================== INDUSTRIAL Y FX
        P ("Sirena industrial", "Industrial y FX", { { "sy_vol", 12.0f }, { "o1_ctype", 4 }, { "o1_curve", 0.3f }, { "l1_shape", 1 }, { "l1_rate", 0.5f }, { "m1_src", 1 },
            { "m1_dst", 5 }, { "m1_amt", 0.25f }, { "f_drive", 0.5f }, { "fx_sat_on", 1 }, { "fx_sat_type", 2 }, { "fx_sat_amt", 0.5f },
            { "fx_rev_type", 3 }, { "fx_rev_mix", 25 } }),
        P ("Motor oxidado", "Industrial y FX", { { "sy_vol", 12.0f }, { "o1_wave", 3 }, { "o1_ctype", 1 }, { "o1_curve", 0.6f }, { "o2_oct", -1 }, { "o2_semi", 1 },
            { "o2_fine", 30 }, { "ring", 0.5f }, { "noise", 0.15f }, { "f_cut", 1200 }, { "f_res", 0.4f }, { "f_drive", 0.7f }, { "l2_shape", 4 },
            { "l2_rate", 6 }, { "m1_src", 2 }, { "m1_dst", 10 }, { "m1_amt", 0.3f }, { "fx_sat_on", 1 }, { "fx_sat_type", 2 }, { "fx_sat_amt", 0.6f },
            { "fx_sat_mix", 80 }, { "voice_mode", 1 } }),
        P ("Choque metálico", "Industrial y FX", { { "sy_vol", -3.5f }, { "o1_wave", 7 }, { "o2_wave", 7 }, { "o2_semi", 6 }, { "o2_fine", 13 }, { "ring", 0.6f },
            { "ea_a", 0.001f }, { "ea_d", 0.8f }, { "ea_s", 0 }, { "ea_r", 0.8f }, { "f_cut", 6000 }, { "em_a", 0.001f }, { "em_d", 0.2f }, { "em_s", 0 },
            { "m1_src", 4 }, { "m1_dst", 5 }, { "m1_amt", -0.1f }, { "fx_rev_type", 3 }, { "fx_rev_mix", 30 } }),
        P ("Ruido de fábrica", "Industrial y FX", { { "sy_vol", 12.0f }, { "noise", 0.8f }, { "o1_wave", 3 }, { "o1_oct", -2 }, { "o1_level", 0.2f }, { "o2_level", 0 },
            { "f_type", 2 }, { "f_cut", 900 }, { "f_res", 0.6f }, { "f_env", 0 }, { "l1_rate", 0.3f }, { "m1_src", 1 }, { "m1_dst", 10 }, { "m1_amt", 0.5f },
            { "ea_a", 0.3f }, { "ea_d", 1 }, { "ea_s", 0.8f }, { "ea_r", 1 }, { "fx_sat_on", 1 }, { "fx_sat_type", 2 }, { "fx_sat_amt", 0.4f },
            { "fx_sat_mix", 60 }, { "fx_rev_type", 3 }, { "fx_rev_mix", 35 } }),
        P ("Láser espacial", "Industrial y FX", { { "sy_vol", -2.0f }, { "o1_wave", 0 }, { "o2_level", 0 }, { "em_a", 0.001f }, { "em_d", 0.3f }, { "em_s", 0 },
            { "m1_src", 4 }, { "m1_dst", 5 }, { "m1_amt", 0.8f }, { "ea_a", 0.001f }, { "ea_d", 0.4f }, { "ea_s", 0 }, { "ea_r", 0.3f },
            { "f_type", 1 }, { "f_cut", 15000 }, { "f_env", 0 }, { "fx_dly_on", 1 }, { "fx_dly_div", 4 }, { "fx_dly_mix", 30 } }),
        P ("Glitch digital", "Industrial y FX", { { "sy_vol", -2.5f }, { "o1_wave", 9 }, { "o1_ctype", 4 }, { "o1_curve", 0.7f }, { "l2_shape", 4 }, { "l2_rate", 12 },
            { "m1_src", 2 }, { "m1_dst", 1 }, { "m1_amt", 0.5f }, { "m2_src", 2 }, { "m2_dst", 5 }, { "m2_amt", 0.1f }, { "o2_level", 0 },
            { "f_type", 1 }, { "f_cut", 10000 }, { "ea_a", 0.001f }, { "ea_d", 0.3f }, { "ea_s", 0.5f }, { "ea_r", 0.2f },
            { "fx_dly_on", 1 }, { "fx_dly_div", 6 }, { "fx_dly_mix", 20 } }),
        P ("Tormenta", "Industrial y FX", { { "sy_vol", 9.0f }, { "noise", 1 }, { "o1_level", 0 }, { "o2_level", 0 }, { "f_type", 1 }, { "f_cut", 400 }, { "f_res", 0.3f },
            { "f_env", 0 }, { "f_key", 0 }, { "l1_shape", 5 }, { "l1_rate", 0.3f }, { "m1_src", 1 }, { "m1_dst", 10 }, { "m1_amt", 0.6f },
            { "ea_a", 2 }, { "ea_d", 2 }, { "ea_s", 1 }, { "ea_r", 3 }, { "fx_rev_type", 3 }, { "fx_rev_size", 0.9f }, { "fx_rev_mix", 45 } }),

        //================================================================== FUTURISTAS
        P ("Nave nodriza", "Futuristas", { { "sy_vol", -5.5f }, { "o1_oct", -1 }, { "o1_uni", 7 }, { "o1_det", 0.6f }, { "o2_wave", 7 }, { "o2_oct", -2 }, { "o2_level", 0.4f },
            { "f_cut", 900 }, { "f_res", 0.45f }, { "f_env", 0.2f }, { "l1_rate", 0.08f }, { "m1_src", 1 }, { "m1_dst", 10 }, { "m1_amt", 0.3f },
            { "ea_a", 2 }, { "ea_d", 2 }, { "ea_s", 0.9f }, { "ea_r", 4 }, { "fx_rev_type", 4 }, { "fx_rev_size", 0.9f }, { "fx_rev_mix", 40 },
            { "fx_dly_on", 1 }, { "fx_dly_div", 2 }, { "fx_dly_mix", 25 } }),
        P ("Holograma", "Futuristas", { { "sy_vol", -0.5f }, { "o1_wave", 8 }, { "o1_ctype", 3 }, { "l1_rate", 0.25f }, { "m1_src", 1 }, { "m1_dst", 1 }, { "m1_amt", 0.5f },
            { "o2_wave", 6 }, { "o2_oct", 1 }, { "o2_level", 0.3f }, { "ring", 0.2f }, { "f_type", 1 }, { "f_cut", 9000 }, { "f_env", 0 },
            { "ea_a", 0.5f }, { "ea_d", 1 }, { "ea_s", 0.8f }, { "ea_r", 2 }, { "fx_mod_on", 1 }, { "fx_mod_type", 1 }, { "fx_rev_type", 4 }, { "fx_rev_mix", 30 } }),
        P ("Pulso cuántico", "Futuristas", { { "sy_vol", -2.0f }, { "o1_wave", 4 }, { "o1_ctype", 2 }, { "l1_sync", 8 }, { "m1_src", 1 }, { "m1_dst", 1 }, { "m1_amt", 0.5f },
            { "m2_src", 1 }, { "m2_dst", 13 }, { "m2_amt", 0.5f }, { "f_cut", 4000 }, { "f_res", 0.3f }, { "ea_a", 0.01f }, { "ea_d", 0.4f },
            { "ea_s", 0.7f }, { "ea_r", 0.4f }, { "fx_dly_on", 1 }, { "fx_dly_div", 5 }, { "fx_dly_mix", 25 } }),
        P ("Voz del futuro", "Futuristas", { { "sy_vol", 11.0f }, { "o1_wave", 6 }, { "o1_ctype", 3 }, { "o1_curve", 0.5f }, { "m1_src", 6 }, { "m1_dst", 1 }, { "m1_amt", -0.5f },
            { "m2_src", 2 }, { "m2_dst", 1 }, { "m2_amt", 0.2f }, { "o2_oct", -1 }, { "o2_level", 0.3f }, { "f_cut", 3000 }, { "f_res", 0.35f },
            { "voice_mode", 1 }, { "glide", 0.1f }, { "fx_rev_type", 2 }, { "fx_rev_mix", 25 } }),
        P ("Ciudad de neón", "Futuristas", { { "sy_vol", 0.5f }, { "o1_uni", 5 }, { "o1_det", 0.25f }, { "o2_wave", 3 }, { "o2_oct", 1 }, { "o2_level", 0.3f },
            { "f_cut", 1400 }, { "f_res", 0.3f }, { "f_env", 0.45f }, { "ef_d", 0.6f }, { "ef_s", 0.3f }, { "ea_a", 0.01f }, { "ea_d", 0.8f },
            { "ea_s", 0.6f }, { "ea_r", 0.8f }, { "fx_mod_on", 1 }, { "fx_dly_on", 1 }, { "fx_dly_div", 2 }, { "fx_dly_mix", 25 }, { "fx_rev_mix", 25 } }),
        P ("Cristal líquido", "Futuristas", { { "sy_vol", -5.5f }, { "o1_wave", 10 }, { "l1_rate", 0.3f }, { "m1_src", 1 }, { "m1_dst", 1 }, { "m1_amt", 0.4f },
            { "o2_wave", 10 }, { "o2_oct", 1 }, { "o2_level", 0.3f }, { "f_type", 1 }, { "f_cut", 8000 }, { "f_env", 0 }, { "ea_a", 0.3f },
            { "ea_d", 1 }, { "ea_s", 0.8f }, { "ea_r", 1.5f }, { "fx_rev_type", 4 }, { "fx_rev_mix", 30 } }, 6, 2),

        //================================================================== CLÁSICOS ANALÓGICOS
        P ("Metales de sintetizador", "Clásicos analógicos", { { "sy_vol", -0.5f }, { "o2_fine", 10 }, { "f_cut", 800 }, { "f_res", 0.1f }, { "f_env", 0.5f },
            { "ef_a", 0.08f }, { "ef_d", 0.6f }, { "ef_s", 0.4f }, { "ea_a", 0.05f }, { "ea_d", 0.5f }, { "ea_s", 0.8f }, { "ea_r", 0.3f },
            { "fx_mod_on", 1 }, { "fx_rev_mix", 15 } }),
        P ("Cuerdas de máquina", "Clásicos analógicos", { { "sy_vol", -2.0f }, { "o2_oct", 1 }, { "o2_fine", 5 }, { "o2_level", 0.5f }, { "f_cut", 4000 }, { "f_env", 0 },
            { "ea_a", 0.4f }, { "ea_d", 1 }, { "ea_s", 0.9f }, { "ea_r", 1.2f }, { "fx_mod_on", 1 }, { "fx_mod_rate", 0.5f }, { "fx_mod_depth", 0.8f },
            { "fx_mod_mix", 60 }, { "fx_rev_mix", 22 } }),
        P ("Sinte de los setenta", "Clásicos analógicos", { { "sy_vol", 3.0f }, { "o2_oct", -1 }, { "o2_fine", 4 }, { "sub", 0.3f }, { "f_cut", 1200 }, { "f_res", 0.45f },
            { "f_env", 0.5f }, { "voice_mode", 1 }, { "glide", 0.05f }, { "ea_a", 0.005f }, { "ea_d", 0.4f }, { "ea_s", 0.7f }, { "ea_r", 0.2f },
            { "fx_dly_on", 1 }, { "fx_dly_div", 2 }, { "fx_dly_mix", 15 }, { "m1_src", 6 }, { "m1_dst", 10 }, { "m1_amt", 0.5f } }),
        P ("Campanas de anillo", "Clásicos analógicos", { { "sy_vol", -7.0f }, { "o1_wave", 3 }, { "o2_wave", 3 }, { "o2_semi", 7 }, { "o2_level", 0 }, { "ring", 0.8f },
            { "f_type", 1 }, { "f_cut", 6000 }, { "f_env", 0 }, { "ea_a", 0.001f }, { "ea_d", 1.4f }, { "ea_s", 0 }, { "ea_r", 1.2f }, { "fx_rev_mix", 25 } }),
        P ("Barrido de filtro", "Clásicos analógicos", { { "sy_vol", -6.0f }, { "o1_uni", 3 }, { "o2_oct", -1 }, { "f_cut", 300 }, { "f_res", 0.55f }, { "f_env", 0.6f },
            { "ef_a", 3 }, { "ef_d", 3 }, { "ef_s", 0.5f }, { "ef_r", 3 }, { "ea_a", 1 }, { "ea_d", 1 }, { "ea_s", 0.9f }, { "ea_r", 2.5f },
            { "fx_rev_type", 3 }, { "fx_rev_mix", 30 } }),
        P ("Polisinte ochentero", "Clásicos analógicos", { { "sy_vol", -4.5f }, { "o1_wave", 3 }, { "o2_fine", 7 }, { "f_cut", 2200 }, { "f_res", 0.2f }, { "f_env", 0.3f },
            { "ea_a", 0.01f }, { "ea_d", 0.6f }, { "ea_s", 0.7f }, { "ea_r", 0.5f }, { "fx_mod_on", 1 }, { "fx_rev_type", 2 }, { "fx_rev_mix", 20 } }),

        //================================================================== ONDAS DIBUJADAS
        P ("Dibujada: montañas", "Ondas dibujadas", { { "sy_vol", -4.5f }, { "o1_wave", 10 }, { "m1_src", 6 }, { "m1_dst", 1 }, { "m1_amt", 0.6f }, { "o2_level", 0 },
            { "f_type", 1 }, { "f_cut", 9000 }, { "f_env", 0 }, { "ea_a", 0.01f }, { "ea_d", 0.5f }, { "ea_s", 0.8f }, { "ea_r", 0.4f } }, 1),
        P ("Dibujada: latido", "Ondas dibujadas", { { "sy_vol", 1.0f }, { "o1_wave", 10 }, { "o1_ctype", 1 }, { "m1_src", 1 }, { "m1_dst", 1 }, { "m1_amt", 0.3f },
            { "o2_level", 0 }, { "f_cut", 5000 }, { "f_res", 0.2f }, { "ea_a", 0.3f }, { "ea_d", 1 }, { "ea_s", 0.8f }, { "ea_r", 1.5f },
            { "fx_rev_type", 4 }, { "fx_rev_mix", 25 } }, 5),
        P ("Dibujada: escalera", "Ondas dibujadas", { { "sy_vol", 4.0f }, { "o1_wave", 10 }, { "o1_ctype", 4 }, { "o1_curve", 0.2f }, { "o2_level", 0 }, { "voice_mode", 1 },
            { "f_type", 1 }, { "f_cut", 14000 }, { "f_env", 0 }, { "fx_dly_on", 1 }, { "fx_dly_div", 3 }, { "fx_dly_mix", 15 },
            { "m1_src", 6 }, { "m1_dst", 1 }, { "m1_amt", 0.5f } }, 3),
        P ("Dibujada: caos", "Ondas dibujadas", { { "sy_vol", 2.0f }, { "o1_wave", 10 }, { "o1_ctype", 2 }, { "o1_curve", 0.2f }, { "em_a", 0.001f }, { "em_d", 0.4f },
            { "em_s", 0 }, { "m1_src", 4 }, { "m1_dst", 1 }, { "m1_amt", 0.5f }, { "o2_level", 0 }, { "f_cut", 3000 }, { "f_res", 0.3f },
            { "ea_a", 0.001f }, { "ea_d", 0.4f }, { "ea_s", 0.3f }, { "ea_r", 0.3f }, { "fx_dly_on", 1 }, { "fx_dly_div", 4 }, { "fx_dly_mix", 20 } }, 7),
    };
    return list;
}
} // namespace geminis::inst
