#include "InstParams.h"
#include "Wavetable.h"
#include "Drums.h"
#include "Sampler.h"

namespace geminis::inst
{
namespace
{
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }

    PDef knob (const juce::String& id, const char* name, float mn, float mx, float df, const char* unit = "", float skew = 1.0f)
    {
        PDef d; d.id = id; d.name = U (name); d.min = mn; d.max = mx; d.def = df; d.unit = U (unit); d.skew = skew; return d;
    }
    PDef integer (const juce::String& id, const char* name, int mn, int mx, int df, const char* unit = "")
    {
        PDef d = knob (id, name, (float) mn, (float) mx, (float) df, unit); d.integer = true; return d;
    }
    PDef choice (const juce::String& id, const char* name, const juce::StringArray& items, int df)
    {
        PDef d; d.id = id; d.name = U (name); d.choices = items; d.min = 0; d.max = (float) (items.size() - 1); d.def = (float) df; return d;
    }
    PDef toggle (const juce::String& id, const char* name, bool df)
    {
        PDef d; d.id = id; d.name = U (name); d.boolean = true; d.min = 0; d.max = 1; d.def = df ? 1.0f : 0.0f; return d;
    }
    juce::StringArray utf8List (std::initializer_list<const char*> items)
    {
        juce::StringArray a;
        for (auto* s : items) a.add (U (s));
        return a;
    }
}

const juce::StringArray& divisionNames()
{
    static const juce::StringArray n = utf8List ({ "Libre", "4 compases", "2 compases", "1 compás", "1/2", "1/4", "1/4 con puntillo",
                                                  "1/8", "1/8 con puntillo", "1/8 tresillo", "1/16", "1/16 tresillo", "1/32" });
    return n;
}

double divisionSeconds (int division, double bpm)
{
    const double beat = 60.0 / juce::jmax (20.0, bpm);
    switch (division)
    {
        case 1:  return beat * 16.0;
        case 2:  return beat * 8.0;
        case 3:  return beat * 4.0;
        case 4:  return beat * 2.0;
        case 5:  return beat;
        case 6:  return beat * 1.5;
        case 7:  return beat * 0.5;
        case 8:  return beat * 0.75;
        case 9:  return beat / 3.0;
        case 10: return beat * 0.25;
        case 11: return beat / 6.0;
        case 12: return beat * 0.125;
        default: return 0.0;
    }
}

juce::String padParamId (int pad, const char* what)
{
    return "p" + juce::String (pad + 1).paddedLeft ('0', 2) + "_" + what;
}

const std::vector<PDef>& allParamDefs()
{
    static const std::vector<PDef> defs = []
    {
        std::vector<PDef> d;
        d.push_back (knob ("vol", "Volumen general", -36.0f, 6.0f, 0.0f, "dB"));

        //====================================================================== SINTETIZADOR
        for (int o = 1; o <= 2; ++o)
        {
            const juce::String p = "o" + juce::String (o) + "_";
            d.push_back (choice (p + "wave", o == 1 ? "Osc 1 onda" : "Osc 2 onda", baseWaveNames(), waveSaw));
            d.push_back (choice (p + "ctype", o == 1 ? "Osc 1 tipo de curva" : "Osc 2 tipo de curva", curveTypeNames(), curveBend));
            d.push_back (knob (p + "curve", o == 1 ? "Osc 1 curva" : "Osc 2 curva", 0.0f, 1.0f, 0.0f));
            d.push_back (integer (p + "oct", o == 1 ? "Osc 1 octava" : "Osc 2 octava", -3, 3, 0));
            d.push_back (integer (p + "semi", o == 1 ? "Osc 1 semitonos" : "Osc 2 semitonos", -12, 12, 0, "st"));
            d.push_back (knob (p + "fine", o == 1 ? "Osc 1 fino" : "Osc 2 fino", -100.0f, 100.0f, o == 1 ? 0.0f : 8.0f, "ct"));
            d.push_back (knob (p + "level", o == 1 ? "Osc 1 nivel" : "Osc 2 nivel", 0.0f, 1.0f, o == 1 ? 0.8f : 0.6f));
            d.push_back (integer (p + "uni", o == 1 ? "Osc 1 unísono" : "Osc 2 unísono", 1, 7, 1));
            d.push_back (knob (p + "det", o == 1 ? "Osc 1 desafinación" : "Osc 2 desafinación", 0.0f, 1.0f, 0.25f));
        }
        d.push_back (knob ("sub", "Sub oscilador", 0.0f, 1.0f, 0.0f));
        d.push_back (choice ("sub_shape", "Forma del sub", utf8List ({ "Seno", "Cuadrada" }), 0));
        d.push_back (knob ("noise", "Ruido", 0.0f, 1.0f, 0.0f));
        d.push_back (knob ("fm", "FM (osc 2 a osc 1)", 0.0f, 1.0f, 0.0f));
        d.push_back (knob ("ring", "Modulación en anillo", 0.0f, 1.0f, 0.0f));

        d.push_back (choice ("f_type", "Tipo de filtro", utf8List ({ "Paso bajo 24", "Paso bajo 12", "Paso banda", "Paso alto", "Muesca" }), 0));
        d.push_back (knob ("f_cut", "Corte", 20.0f, 20000.0f, 6000.0f, "Hz", 0.25f));
        d.push_back (knob ("f_res", "Resonancia", 0.0f, 1.0f, 0.15f));
        d.push_back (knob ("f_drive", "Saturación del filtro", 0.0f, 1.0f, 0.0f));
        d.push_back (knob ("f_env", "Envolvente al filtro", -1.0f, 1.0f, 0.3f));
        d.push_back (knob ("f_key", "Seguimiento de teclado", 0.0f, 1.0f, 0.4f));
        d.push_back (knob ("f_vel", "Velocidad al filtro", 0.0f, 1.0f, 0.2f));

        const char* envNames[] = { "ea", "ef", "em" };
        const char* envTitles[] = { "Volumen", "Filtro", "Mod" };
        const float envDefs[3][4] = { { 0.004f, 0.3f, 0.8f, 0.25f }, { 0.002f, 0.45f, 0.25f, 0.35f }, { 0.3f, 0.6f, 0.0f, 0.5f } };
        for (int e = 0; e < 3; ++e)
        {
            const juce::String p = juce::String (envNames[e]) + "_";
            const juce::String t = juce::String ("Env ") + envTitles[e] + " ";
            d.push_back (knob (p + "a", (t + "ataque").toRawUTF8(), 0.001f, 10.0f, envDefs[e][0], "s", 0.25f));
            d.push_back (knob (p + "d", (t + "caída").toRawUTF8(), 0.001f, 10.0f, envDefs[e][1], "s", 0.25f));
            d.push_back (knob (p + "s", (t + "sostenido").toRawUTF8(), 0.0f, 1.0f, envDefs[e][2]));
            d.push_back (knob (p + "r", (t + "liberación").toRawUTF8(), 0.001f, 10.0f, envDefs[e][3], "s", 0.25f));
        }

        const auto lfoShapes = utf8List ({ "Seno", "Triángulo", "Sierra", "Cuadrada", "Aleatorio", "Aleatorio suave" });
        for (int l = 1; l <= 2; ++l)
        {
            const juce::String p = "l" + juce::String (l) + "_";
            d.push_back (choice (p + "shape", l == 1 ? "LFO 1 forma" : "LFO 2 forma", lfoShapes, 0));
            d.push_back (knob (p + "rate", l == 1 ? "LFO 1 velocidad" : "LFO 2 velocidad", 0.02f, 20.0f, l == 1 ? 0.5f : 5.0f, "Hz", 0.3f));
            d.push_back (choice (p + "sync", l == 1 ? "LFO 1 sincronía" : "LFO 2 sincronía", divisionNames(), 0));
        }

        const auto sources = utf8List ({ "Ninguna", "LFO 1", "LFO 2", "Env Filtro", "Env Mod", "Velocidad", "Rueda de mod.", "Aftertouch", "Nota", "Aleatorio" });
        const auto dests = utf8List ({ "Ninguno", "Curva 1", "Curva 2", "Tono 1", "Tono 2", "Tono", "Nivel 1", "Nivel 2", "FM", "Anillo", "Corte",
                                       "Resonancia", "Volumen", "Panorama", "Desafinación", "Vel. LFO 2", "Sub", "Ruido" });
        for (int m = 1; m <= numModSlots; ++m)
        {
            const juce::String p = "m" + juce::String (m) + "_";
            const juce::String t = "Mod " + juce::String (m) + " ";
            d.push_back (choice (p + "src", (t + "fuente").toRawUTF8(), sources, 0));
            d.push_back (choice (p + "dst", (t + "destino").toRawUTF8(), dests, 0));
            d.push_back (knob (p + "amt", (t + "cantidad").toRawUTF8(), -1.0f, 1.0f, 0.0f));
        }

        d.push_back (choice ("voice_mode", "Voces", utf8List ({ "Polifónico", "Mono", "Legato" }), 0));
        d.push_back (knob ("glide", "Portamento", 0.0f, 2.0f, 0.0f, "s", 0.35f));
        d.push_back (integer ("bend", "Rango de pitch bend", 0, 24, 2, "st"));
        d.push_back (knob ("vel_sens", "Sensibilidad a la velocidad", 0.0f, 1.0f, 0.6f));
        d.push_back (knob ("width", "Anchura estéreo", 0.0f, 1.0f, 0.7f));
        d.push_back (knob ("sy_vol", "Volumen del sinte", -36.0f, 12.0f, -4.0f, "dB"));

        //====================================================================== BATERÍA
        const auto chokes = utf8List ({ "Ninguno", "A", "B", "C", "D" });
        for (int pad = 0; pad < numPads; ++pad)
        {
            const juce::String t = "Pad " + juce::String (pad + 1) + " ";
            d.push_back (choice (padParamId (pad, "snd"), (t + "sonido").toRawUTF8(), drumSoundNames(), 0));
            d.push_back (knob (padParamId (pad, "tune"), (t + "afinación").toRawUTF8(), -24.0f, 24.0f, 0.0f, "st"));
            d.push_back (knob (padParamId (pad, "decay"), (t + "caída").toRawUTF8(), 0.0f, 1.0f, 0.5f));
            d.push_back (knob (padParamId (pad, "tone"), (t + "tono").toRawUTF8(), 0.0f, 1.0f, 0.5f));
            d.push_back (knob (padParamId (pad, "snap"), (t + "golpe").toRawUTF8(), 0.0f, 1.0f, 0.5f));
            d.push_back (knob (padParamId (pad, "drive"), (t + "saturación").toRawUTF8(), 0.0f, 1.0f, 0.0f));
            d.push_back (knob (padParamId (pad, "level"), (t + "nivel").toRawUTF8(), 0.0f, 1.0f, 0.75f));
            d.push_back (knob (padParamId (pad, "pan"), (t + "panorama").toRawUTF8(), -1.0f, 1.0f, 0.0f));
            d.push_back (choice (padParamId (pad, "choke"), (t + "grupo de corte").toRawUTF8(), chokes, 0));
        }
        d.push_back (choice ("seq_mode", "Secuenciador", utf8List ({ "Apagado", "Con el DAW", "Siempre" }), 1));
        d.push_back (choice ("seq_pattern", "Patrón", utf8List ({ "A", "B", "C", "D" }), 0));
        d.push_back (knob ("seq_swing", "Swing", 0.0f, 60.0f, 0.0f, "%"));
        d.push_back (choice ("seq_len", "Pasos", utf8List ({ "16", "32" }), 0));
        d.push_back (knob ("seq_bpm", "Tempo propio", 60.0f, 200.0f, 100.0f, "bpm"));
        d.push_back (choice ("midi_map", "Mapa MIDI", utf8List ({ "Maschine (C1 = pad 1)", "General MIDI" }), 0));
        d.push_back (knob ("dr_vol", "Volumen de la batería", -36.0f, 6.0f, -3.0f, "dB"));

        //====================================================================== INSTRUMENTOS
        d.push_back (choice ("ins_type", "Instrumento", instrumentNames(), 0));
        d.push_back (knob ("ins_attack", "Ataque", 0.001f, 2.0f, 0.002f, "s", 0.3f));
        d.push_back (knob ("ins_release", "Liberación", 0.02f, 5.0f, 0.4f, "s", 0.35f));
        d.push_back (knob ("ins_tone", "Tono", 0.0f, 1.0f, 0.8f));
        d.push_back (knob ("ins_char", "Carácter", 0.0f, 1.0f, 0.5f));
        d.push_back (integer ("ins_tune", "Transposición", -24, 24, 0, "st"));
        d.push_back (knob ("ins_vel", "Sensibilidad a la velocidad", 0.0f, 1.0f, 0.7f));
        d.push_back (knob ("ins_vol", "Volumen del instrumento", -36.0f, 6.0f, -2.0f, "dB"));

        //====================================================================== EFECTOS (común)
        d.push_back (toggle ("fx_sat_on", "Saturación", false));
        d.push_back (choice ("fx_sat_type", "Tipo de saturación", utf8List ({ "Cinta", "Válvula", "Furia", "Lo-fi" }), 0));
        d.push_back (knob ("fx_sat_amt", "Saturación cantidad", 0.0f, 1.0f, 0.4f));
        d.push_back (knob ("fx_sat_mix", "Saturación mezcla", 0.0f, 100.0f, 100.0f, "%"));
        d.push_back (toggle ("fx_cmp_on", "Compresor", false));
        d.push_back (knob ("fx_cmp_amt", "Compresor cantidad", 0.0f, 1.0f, 0.4f));
        d.push_back (toggle ("fx_mod_on", "Modulación", false));
        d.push_back (choice ("fx_mod_type", "Tipo de modulación", utf8List ({ "Chorus", "Phaser", "Flanger" }), 0));
        d.push_back (knob ("fx_mod_rate", "Modulación velocidad", 0.05f, 5.0f, 0.6f, "Hz", 0.5f));
        d.push_back (knob ("fx_mod_depth", "Modulación profundidad", 0.0f, 1.0f, 0.5f));
        d.push_back (knob ("fx_mod_mix", "Modulación mezcla", 0.0f, 100.0f, 40.0f, "%"));
        d.push_back (toggle ("fx_dly_on", "Delay", false));
        d.push_back (choice ("fx_dly_type", "Tipo de delay", utf8List ({ "Digital", "Cinta", "Ping-pong" }), 2));
        d.push_back (choice ("fx_dly_div", "Delay tiempo", utf8List ({ "1/2", "1/4", "1/4 con puntillo", "1/8", "1/8 con puntillo", "1/8 tresillo", "1/16" }), 4));
        d.push_back (knob ("fx_dly_fb", "Delay realimentación", 0.0f, 95.0f, 35.0f, "%"));
        d.push_back (knob ("fx_dly_mix", "Delay mezcla", 0.0f, 100.0f, 25.0f, "%"));
        d.push_back (toggle ("fx_rev_on", "Reverb", true));
        d.push_back (choice ("fx_rev_type", "Tipo de reverb", utf8List ({ "Habitación", "Sala", "Placa", "Catedral", "Shimmer" }), 1));
        d.push_back (knob ("fx_rev_size", "Reverb tamaño", 0.0f, 1.0f, 0.45f));
        d.push_back (knob ("fx_rev_mix", "Reverb mezcla", 0.0f, 100.0f, 18.0f, "%"));
        return d;
    }();
    return defs;
}

const PDef* findDef (const juce::String& id)
{
    static const std::map<juce::String, const PDef*> index = []
    {
        std::map<juce::String, const PDef*> m;
        for (const auto& d : allParamDefs()) m[d.id] = &d;
        return m;
    }();
    auto it = index.find (id);
    return it != index.end() ? it->second : nullptr;
}

juce::String formatValue (const PDef& d, float v)
{
    if (d.boolean) return v > 0.5f ? U ("Sí") : "No";
    if (d.isChoice()) return d.choices[juce::jlimit (0, d.choices.size() - 1, juce::roundToInt (v))];
    const auto& u = d.unit;
    if (d.integer) return (u == "st" && v > 0 ? "+" : "") + juce::String (juce::roundToInt (v)) + (u.isNotEmpty() ? " " + u : juce::String());
    if (u == "Hz") return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (v, v < 10.0f ? 2 : 0) + " Hz";
    if (u == "s")  return v < 0.9995f ? juce::String (juce::roundToInt (v * 1000.0f)) + " ms" : juce::String (v, 2) + " s";
    if (u == "dB") return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB";
    if (u == "%")  return juce::String (juce::roundToInt (v)) + " %";
    if (u == "st") return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " st";
    if (u == "ct") return (v > 0.5f ? "+" : "") + juce::String (juce::roundToInt (v)) + " ct";
    if (u == "bpm") return juce::String (juce::roundToInt (v)) + " bpm";
    if (d.min < 0.0f) return (v > 0.005f ? "+" : "") + juce::String (juce::roundToInt (v * 100.0f)) + " %";
    return juce::String (v * 10.0f, 1);
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& def : allParamDefs())
    {
        const juce::ParameterID pid { def.id, 1 };
        if (def.boolean)
            layout.add (std::make_unique<juce::AudioParameterBool> (pid, def.name, def.def > 0.5f));
        else if (def.isChoice())
            layout.add (std::make_unique<juce::AudioParameterChoice> (pid, def.name, def.choices, (int) def.def));
        else if (def.integer)
            layout.add (std::make_unique<juce::AudioParameterInt> (pid, def.name, (int) def.min, (int) def.max, (int) def.def,
                juce::AudioParameterIntAttributes().withStringFromValueFunction ([&def] (int v, int) { return formatValue (def, (float) v); })));
        else
        {
            juce::NormalisableRange<float> range (def.min, def.max, 0.0f, def.skew);
            layout.add (std::make_unique<juce::AudioParameterFloat> (pid, def.name, range, def.def,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction ([&def] (float v, int) { return formatValue (def, v); })));
        }
    }
    return layout;
}
} // namespace geminis::inst
