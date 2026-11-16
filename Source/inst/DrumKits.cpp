#include "Drums.h"

/*  Los 16 kits siguen la distribución de pads de Maschine (pad 1 abajo a la izquierda):

    Fila 4 (arriba)  13-16  Melodía, acorde, voz y efecto (FX o muestra)
    Fila 3            9-12  Toms, cencerros, bloques o percusión secundaria (platillos en los acústicos)
    Fila 2            5-8   Bombo secundario, caja secundaria, shakers y percusión ligera
    Fila 1 (abajo)    1-4   1 bombo · 2 caja · 3 charles cerrado · 4 charles abierto (o palmas)
*/

namespace geminis::inst
{
namespace
{
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }

    PadPreset pad (const char* sound, float tune = 0, float decay = 0.5f, float tone = 0.5f, float snap = 0.5f, float drive = 0,
                   float level = 0.75f, float pan = 0, int choke = 0)
    {
        PadPreset p;
        p.sound = U (sound); p.tune = tune; p.decay = decay; p.tone = tone; p.snap = snap; p.drive = drive; p.level = level; p.pan = pan; p.choke = choke;
        return p;
    }

    /** Patrón escrito como texto: X acento, x normal, o suave, . silencio. Pads numerados del 1 al 16. */
    Pattern pat (std::initializer_list<std::pair<int, const char*>> rows)
    {
        Pattern p;
        for (const auto& r : rows)
        {
            const juce::String s (r.second);
            for (int k = 0; k < juce::jmin (numSteps, s.length()); ++k)
            {
                const auto c = s[k];
                p.steps[(size_t) (r.first - 1)][(size_t) k] = (juce::uint8) (c == 'X' ? 3 : c == 'x' ? 2 : c == 'o' ? 1 : 0);
            }
        }
        return p;
    }

    using Extra = std::vector<std::pair<juce::String, float>>;

    DrumKit kit (const char* name, const char* desc, double bpm, float swing, std::array<PadPreset, numPads> pads,
                 std::array<Pattern, numPatterns> patterns, Extra extra)
    {
        DrumKit k;
        k.name = U (name); k.description = U (desc); k.bpm = bpm; k.swing = swing; k.pads = pads; k.patterns = patterns; k.extra = std::move (extra);
        return k;
    }

    // Fila 4: melodía, acorde y voz, en do (la afinación de cada pad los transporta)
    PadPreset melodyPad() { return pad ("Nota pulsada", 0, 0.5f, 0.55f, 0.5f, 0, 0.6f, -0.15f); }
    PadPreset chordPad()  { return pad ("Acorde", 0, 0.5f, 0.5f, 0.5f, 0, 0.55f); }
    PadPreset voicePad()  { return pad ("Voz", 0, 0.5f, 0.5f, 0.5f, 0, 0.55f, 0.15f); }

    /** Kit acústico. Fila 2: bombo profundo, aro, maraca y palmas. Fila 3: dos toms, crash y ride. */
    std::array<PadPreset, numPads> acousticPads (float kickTune = -2.0f, const char* snare = "Caja acústica")
    {
        return { pad ("Bombo seco", kickTune, 0.45f, 0.45f, 0.55f, 0.1f, 0.85f),
                 pad (snare, 0, 0.5f, 0.5f, 0.55f, 0, 0.8f),
                 pad ("Charles cerrado", 0, 0.5f, 0.55f, 0.5f, 0, 0.55f, 0.25f, 1),
                 pad ("Charles abierto", 0, 0.5f, 0.55f, 0.5f, 0, 0.55f, 0.25f, 1),

                 pad ("Bombo profundo", kickTune - 1.0f, 0.3f, 0.45f, 0.5f, 0.0f, 0.7f),
                 pad ("Caja (aro)", 0, 0.5f, 0.5f, 0.5f, 0, 0.65f, 0.05f),
                 pad ("Maraca", 0, 0.5f, 0.5f, 0.5f, 0, 0.45f, -0.3f),
                 pad ("Palmas", 0, 0.5f, 0.5f, 0.5f, 0, 0.65f),

                 pad ("Tom grave", -2, 0.5f, 0.5f, 0.5f, 0, 0.75f, -0.3f),
                 pad ("Tom agudo", 0, 0.5f, 0.5f, 0.5f, 0, 0.7f, 0.15f),
                 pad ("Platillo crash", 0, 0.5f, 0.5f, 0.5f, 0, 0.6f, -0.3f),
                 pad ("Platillo ride", 0, 0.5f, 0.5f, 0.5f, 0, 0.55f, 0.35f),

                 melodyPad(), chordPad(), voicePad(),
                 pad ("Ruido", 0, 0.55f, 0.5f, 0.5f, 0, 0.45f) };
    }

    /** Kit electrónico. Fila 2: bombo profundo, aro, maraca y palmas. Fila 3: dos toms, platillo y cencerro. */
    std::array<PadPreset, numPads> electronicPads (const char* kick, float kickDrive, const char* snare)
    {
        return { pad (kick, 0, 0.55f, 0.5f, 0.65f, kickDrive, 0.9f),
                 pad (snare, 0, 0.5f, 0.5f, 0.55f, 0, 0.75f),
                 pad ("Charles cerrado sintético", 0, 0.5f, 0.55f, 0.5f, 0, 0.5f, 0.2f, 1),
                 pad ("Charles abierto sintético", 0, 0.45f, 0.55f, 0.5f, 0, 0.5f, 0.2f, 1),

                 pad ("Bombo profundo", -2, 0.6f, 0.45f, 0.4f, 0.1f, 0.75f),
                 pad ("Aro sintético", 0, 0.5f, 0.55f, 0.5f, 0, 0.6f, 0.1f),
                 pad ("Maraca sintética", 0, 0.5f, 0.55f, 0.5f, 0, 0.45f, -0.25f),
                 pad ("Palmas sintéticas", 0, 0.5f, 0.5f, 0.5f, 0, 0.72f),

                 pad ("Tom sintético", -5, 0.5f, 0.5f, 0.5f, 0, 0.75f, -0.3f),
                 pad ("Tom sintético", 2, 0.5f, 0.5f, 0.5f, 0, 0.7f, 0.3f),
                 pad ("Platillo sintético", 0, 0.5f, 0.55f, 0.5f, 0, 0.5f, -0.3f),
                 pad ("Cencerro sintético", 0, 0.5f, 0.5f, 0.5f, 0, 0.5f, 0.2f),

                 melodyPad(), chordPad(), voicePad(),
                 pad ("Láser", 0, 0.5f, 0.5f, 0.5f, 0, 0.5f) };
    }
}

const std::vector<DrumKit>& drumKits()
{
    static const std::vector<DrumKit> kits = []
    {
        std::vector<DrumKit> k;

        k.push_back (kit ("Rock", "Batería acústica con pegada, para rock clásico y moderno.", 112, 0, acousticPads(), {
                pat ({ { 1, "x.......x.x....." }, { 2, "....X.......X..." }, { 3, "X.x.X.x.X.x.X.x." }, { 11, "X..............." } }),
                pat ({ { 1, "x.....x.x.x....." }, { 2, "....X.......X..o" }, { 3, "X.x.X.x.X.x.X..." }, { 4, "..............x." } }),
                pat ({ { 1, "x.......x......." }, { 2, "....X.........xX" }, { 9, "..........xxxx.." }, { 10, "........xx......" }, { 11, "X..............." } }),
                pat ({ { 1, "x.........x....." }, { 2, "........X......." }, { 7, "....x.......x..." }, { 12, "X.x.x.x.X.x.x.x." } }) },
            { { "dr_vol", -4.0f }, { "fx_rev_on", 1 }, { "fx_rev_type", 1 }, { "fx_rev_size", 0.4f }, { "fx_rev_mix", 14 }, { "fx_cmp_on", 1 }, { "fx_cmp_amt", 0.3f } }));
        k.back().pads[6] = pad ("Charles de pedal", 0, 0.5f, 0.5f, 0.5f, 0, 0.5f, 0.25f, 1);

        k.push_back (kit ("Metal", "Doble bombo, caja seca y platos al frente.", 150, 0, acousticPads (-1.0f), {
                pat ({ { 1, "xxxxxxxxxxxxxxxx" }, { 2, "....X.......X..." }, { 12, "X...x...X...x..." } }),
                pat ({ { 1, "x.x.x.x.x.x.x.x." }, { 2, ".x.x.x.x.x.x.x.x" }, { 11, "X.x.x.x.X.x.x.x." } }),
                pat ({ { 1, "x.xxx.xxx.xxx.xx" }, { 2, "....X.......X..." }, { 3, "x.x.x.x.x.x.x.x." } }),
                pat ({ { 1, "x..x..x...x..x.." }, { 2, "........X......." }, { 11, "X.......X......." } }) },
            { { "dr_vol", -4.5f }, { "fx_cmp_on", 1 }, { "fx_cmp_amt", 0.45f }, { "fx_rev_on", 1 }, { "fx_rev_type", 0 }, { "fx_rev_mix", 10 } }));
        k.back().pads[0] = pad ("Bombo duro", -1, 0.3f, 0.55f, 0.8f, 0.1f, 0.85f);

        {
            auto pads = acousticPads (-1.0f);
            pads[5] = pad ("Caja suave", 0, 0.45f, 0.5f, 0.5f, 0, 0.5f);
            pads[6] = pad ("Pandereta", 0, 0.5f, 0.5f, 0.5f, 0, 0.5f, -0.2f);
            pads[10] = pad ("Cencerro", 0, 0.5f, 0.5f, 0.5f, 0, 0.55f, 0.2f);
            k.push_back (kit ("Funk", "Caja con notas fantasma y charles abierto.", 100, 12, pads, {
                pat ({ { 1, "x.....x...x..x.." }, { 2, "....X..o.o..X.o." }, { 3, "xxxxxxxxxxxxxx.x" }, { 4, "..............x." } }),
                pat ({ { 1, "x..x..x...x....x" }, { 2, "....X..o.o.oX..." }, { 3, "x.x.x...x.x.x..." }, { 4, "......x.......x." } }),
                pat ({ { 1, "x.x...x..x.x...." }, { 2, "....X.oo..o.X.oo" }, { 3, "x.xxx.x.x.xxx.x." }, { 7, "....x.......x..." } }),
                pat ({ { 1, "x.........x....." }, { 2, "....X.......X..." }, { 11, "x...x...x...x..." }, { 12, "x.xxx.xxx.xxx.xx" } }) },
                { { "dr_vol", -4.0f }, { "fx_cmp_on", 1 }, { "fx_cmp_amt", 0.35f }, { "fx_rev_on", 1 }, { "fx_rev_type", 0 }, { "fx_rev_mix", 12 } }));
        }

        k.push_back (kit ("Pop", "Bombo y caja limpios con palmas y maraca.", 110, 0, acousticPads(), {
                pat ({ { 1, "x.......x.x....." }, { 2, "....x.......x..." }, { 3, "x.x.x.x.x.x.x.x." }, { 7, "oxoxoxoxoxoxoxox" }, { 8, "....x.......x..." } }),
                pat ({ { 1, "x..x....x.x....." }, { 2, "....x.......x..." }, { 3, "x.x.x...x.x.x..." }, { 4, "......x.......x." }, { 8, "....x.......x..." } }),
                pat ({ { 1, "x.......x......." }, { 2, "....x.......x.xx" }, { 8, "....x.......x..." }, { 9, "..........x.x.x." }, { 10, "........x.x....." }, { 11, "X..............." } }),
                pat ({ { 1, "x.......x......." }, { 2, "....x.......x..." }, { 7, "oxoxoxoxoxoxoxox" }, { 12, "x.x.x.x.x.x.x.x." } }) },
            { { "dr_vol", -4.0f }, { "fx_rev_on", 1 }, { "fx_rev_type", 2 }, { "fx_rev_mix", 18 }, { "fx_cmp_on", 1 }, { "fx_cmp_amt", 0.3f } }));

        {
            auto pads = electronicPads ("Bombo seco", 0.2f, "Caja eléctrica");
            pads[15] = pad ("Ruido", 0, 0.55f, 0.5f, 0.5f, 0, 0.45f);
            k.push_back (kit ("Electrónica", "Bombo a negras, palmas y charles abiertos: pista de baile.", 126, 0, pads, {
                pat ({ { 1, "x...x...x...x..." }, { 3, "xx.xxx.xxx.xxx.x" }, { 4, "..x...x...x...x." }, { 8, "....x.......x..." } }),
                pat ({ { 1, "x...x...x...x..." }, { 2, "....x.......x..." }, { 4, "..x...x...x...x." }, { 7, "oxoxoxoxoxoxoxox" }, { 8, "....x.......x..." } }),
                pat ({ { 1, "x...x...x...x..." }, { 2, "x.x.x.x.xxxxxxxx" }, { 11, "X..............." }, { 16, "x..............." } }),
                pat ({ { 3, "x.x.x.x.x.x.x.x." }, { 8, "....x.......x..." }, { 16, "x.........x....x" } }) },
                { { "dr_vol", -5.5f }, { "fx_rev_on", 1 }, { "fx_rev_type", 1 }, { "fx_rev_mix", 15 }, { "fx_cmp_on", 1 }, { "fx_cmp_amt", 0.4f } }));
        }

        {
            auto pads = acousticPads (-3.0f, "Caja vintage");
            pads[0] = pad ("Bombo seco", -3, 0.5f, 0.35f, 0.4f, 0.25f, 0.9f);
            pads[2] = pad ("Charles cerrado", 0, 0.5f, 0.4f, 0.5f, 0, 0.55f, 0.2f, 1);
            pads[6] = pad ("Pandereta", 0, 0.5f, 0.45f, 0.5f, 0, 0.45f, -0.2f);
            k.push_back (kit ("Hip hop", "Boom bap con swing, caja vintage y bombo gordo.", 90, 25, pads, {
                pat ({ { 1, "x......x..x....." }, { 2, "....x.......x..." }, { 3, "x.x.x.x.x.x.x.x." } }),
                pat ({ { 1, "x.x.......x..x.." }, { 2, "....x.......x..." }, { 3, "x.x.x.x.x.x.x.xx" }, { 6, "..........o....." } }),
                pat ({ { 1, "x......x.xx....." }, { 2, "....x.......x..x" }, { 3, "x.x.x.x.x.x.x..." }, { 4, "..............x." } }),
                pat ({ { 1, "x.........x....." }, { 2, "....x.......x..." }, { 3, "x...x...x...x..." }, { 7, "....x.......x..." } }) },
                { { "dr_vol", -1.5f }, { "fx_sat_on", 1 }, { "fx_sat_type", 0 }, { "fx_sat_amt", 0.35f }, { "fx_cmp_on", 1 }, { "fx_cmp_amt", 0.4f },
                  { "fx_rev_on", 1 }, { "fx_rev_type", 0 }, { "fx_rev_mix", 10 } }));
        }

        {
            auto pads = electronicPads ("Bombo seco", 0.15f, "Caja eléctrica");
            pads[4] = pad ("Bajo 808", 0, 0.7f, 0.5f, 0.4f, 0.3f, 0.85f);
            k.push_back (kit ("Trap", "808 largo, charles con redobles y palmas a medio tiempo.", 140, 0, pads, {
                pat ({ { 1, "x.........x....." }, { 2, "........x......." }, { 3, "x.x.x.x.x.x.x.xx" }, { 5, "x.........x....." }, { 8, "........x......." } }),
                pat ({ { 1, "x......x..x....." }, { 2, "........x......." }, { 3, "x.xxx.x.x.x.xxxx" }, { 5, "x......x..x....." }, { 8, "........x......." } }),
                pat ({ { 1, "x.....x...x.x..." }, { 2, "........x.....xx" }, { 3, "xoxoxoxoxoxoxo.." }, { 4, "..............x." }, { 5, "x.....x...x.x..." }, { 8, "........x......." } }),
                pat ({ { 3, "x.x.x.x.x.x.x.x." }, { 5, "x..............." }, { 8, "........x......." }, { 16, "....x.......x..." } }) },
                { { "dr_vol", -5.5f }, { "fx_cmp_on", 1 }, { "fx_cmp_amt", 0.35f }, { "fx_rev_on", 1 }, { "fx_rev_type", 2 }, { "fx_rev_mix", 12 } }));
        }

        {
            auto pads = electronicPads ("Bombo duro", 0.3f, "Caja rota");
            pads[4] = pad ("Bombo profundo", -3, 0.8f, 0.35f, 0.3f, 0.2f, 0.55f);
            pads[10] = pad ("Platillo ride", 0, 0.5f, 0.5f, 0.5f, 0, 0.5f, 0.3f);
            pads[15] = pad ("Metal industrial", 0, 0.4f, 0.55f, 0.5f, 0, 0.45f, 0.2f);
            k.push_back (kit ("Techno", "Bombo duro, ride y retumbo entre golpes.", 130, 0, pads, {
                pat ({ { 1, "x...x...x...x..." }, { 4, "..x...x...x...x." }, { 8, "....x.......x..." }, { 11, "x.x.x.x.x.x.x.x." } }),
                pat ({ { 1, "x...x...x...x..." }, { 5, "...o...o...o...o" }, { 6, "..x..x....x..x.." }, { 7, "oxoxoxoxoxoxoxox" } }),
                pat ({ { 1, "x...x...x...x..." }, { 2, "....x.......x..." }, { 4, "..x...x...x...x." }, { 16, "......x.......x." } }),
                pat ({ { 1, "x...x...x...x..." }, { 11, "x.x.x.x.x.x.x.x." }, { 12, "x..x..x..x..x.x." }, { 16, "x..............." } }) },
                { { "dr_vol", -5.0f }, { "fx_rev_on", 1 }, { "fx_rev_type", 1 }, { "fx_rev_mix", 15 }, { "fx_cmp_on", 1 }, { "fx_cmp_amt", 0.3f },
                  { "fx_dly_on", 1 }, { "fx_dly_div", 4 }, { "fx_dly_fb", 30 }, { "fx_dly_mix", 10 } }));
        }

        {
            auto pads = electronicPads ("Bombo seco", 0.1f, "Caja eléctrica");
            pads[2] = pad ("Charles cerrado", 0, 0.5f, 0.55f, 0.5f, 0, 0.5f, 0.2f, 1);
            pads[3] = pad ("Charles abierto", 0, 0.45f, 0.55f, 0.5f, 0, 0.5f, 0.2f, 1);
            pads[6] = pad ("Pandereta", 0, 0.5f, 0.5f, 0.5f, 0, 0.45f, -0.2f);
            pads[7] = pad ("Palmas", 0, 0.5f, 0.5f, 0.5f, 0, 0.7f);
            pads[8] = pad ("Conga", 0, 0.5f, 0.5f, 0.5f, 0, 0.6f, -0.3f);
            pads[9] = pad ("Conga aguda", 0, 0.5f, 0.5f, 0.5f, 0, 0.55f, 0.3f);
            pads[10] = pad ("Platillo crash", 0, 0.5f, 0.5f, 0.5f, 0, 0.55f, -0.3f);
            pads[11] = pad ("Platillo ride", 0, 0.5f, 0.5f, 0.5f, 0, 0.5f, 0.3f);
            pads[15] = pad ("Ruido", 0, 0.55f, 0.5f, 0.5f, 0, 0.45f);
            k.push_back (kit ("House", "Bombo a negras, palmas reales, charles abierto y congas.", 124, 15, pads, {
                pat ({ { 1, "x...x...x...x..." }, { 4, "..x...x...x...x." }, { 7, ".o.o.o.o.o.o.o.o" }, { 8, "....x.......x..." } }),
                pat ({ { 1, "x...x...x...x..." }, { 3, "x...x...x...x..." }, { 4, "..x...x...x...x." }, { 7, "....x.......x..." }, { 8, "....x.......x..." }, { 10, "......x..x....x." } }),
                pat ({ { 1, "x...x...x...x..." }, { 4, "..x...x...x...x." }, { 8, "....x.......x..." }, { 9, "x..x..x...x..x.." }, { 10, "..x...x..x...x.." }, { 12, "x.x.x.x.x.x.x.x." } }),
                pat ({ { 1, "x...x...x...x..." }, { 2, "....x.......x.xx" }, { 8, "....x.......x..." }, { 11, "X..............." }, { 16, "x..............." } }) },
                { { "dr_vol", -6.5f }, { "fx_rev_on", 1 }, { "fx_rev_type", 1 }, { "fx_rev_mix", 14 }, { "fx_cmp_on", 1 }, { "fx_cmp_amt", 0.3f } }));
        }

        k.push_back (kit ("Industrial", "Metal, yunque, ruido y bombos saturados.", 115, 0, {
                pad ("Bombo duro", -2, 0.5f, 0.45f, 0.7f, 0.6f, 0.85f),
                pad ("Caja rota", 0, 0.55f, 0.5f, 0.6f, 0.3f, 0.75f),
                pad ("Charles cerrado sintético", 0, 0.45f, 0.5f, 0.5f, 0.3f, 0.45f, 0.25f, 1),
                pad ("Charles abierto sintético", -3, 0.5f, 0.5f, 0.5f, 0.3f, 0.45f, 0.25f, 1),

                pad ("Bombo profundo", -4, 0.6f, 0.4f, 0.5f, 0.4f, 0.7f),
                pad ("Metal industrial", 0, 0.45f, 0.5f, 0.5f, 0.2f, 0.6f, 0.2f),
                pad ("Glitch", 0, 0.5f, 0.5f, 0.5f, 0, 0.45f, -0.3f),
                pad ("Palmas sintéticas", -2, 0.5f, 0.5f, 0.5f, 0.4f, 0.65f),

                pad ("Tambor de freno", 0, 0.5f, 0.5f, 0.5f, 0.2f, 0.65f, -0.2f),
                pad ("Yunque", 0, 0.5f, 0.5f, 0.5f, 0.1f, 0.55f, 0.2f),
                pad ("Metal industrial", -7, 0.6f, 0.45f, 0.5f, 0.3f, 0.55f, -0.2f),
                pad ("Yunque", -5, 0.5f, 0.45f, 0.5f, 0.2f, 0.5f, 0.3f),

                melodyPad(), chordPad(), voicePad(),
                pad ("Ruido", -5, 0.75f, 0.45f, 0.5f, 0.3f, 0.5f) }, {
                pat ({ { 1, "x..x..x...x..x.." }, { 2, "....x.......x..." }, { 3, "x.x.x.x.x.x.x.x." }, { 6, "..x.......x....." }, { 9, "......x.......x." } }),
                pat ({ { 1, "x...x...x...x..." }, { 2, "....x.......x..." }, { 7, "x..x..x..x..x..x" }, { 10, "..x...x...x...x." } }),
                pat ({ { 1, "xx..x..xx..x..x." }, { 2, "....x..x....x..." }, { 7, "............xxxx" }, { 11, "x.......x......." }, { 16, "x..............." } }),
                pat ({ { 1, "x.......x......." }, { 6, "x...x...x...x..." }, { 12, "..x...x...x...x." }, { 16, "....x.......x..." } }) },
            { { "dr_vol", 0.0f }, { "fx_sat_on", 1 }, { "fx_sat_type", 2 }, { "fx_sat_amt", 0.5f }, { "fx_sat_mix", 60 }, { "fx_cmp_on", 1 }, { "fx_cmp_amt", 0.5f },
              { "fx_rev_on", 1 }, { "fx_rev_type", 3 }, { "fx_rev_size", 0.6f }, { "fx_rev_mix", 20 } }));

        {
            auto pads = acousticPads (-4.0f, "Caja vintage");
            pads[0] = pad ("Bombo seco", -4, 0.45f, 0.3f, 0.3f, 0.2f, 0.9f);
            pads[1] = pad ("Caja vintage", -1, 0.5f, 0.35f, 0.45f, 0, 0.75f);
            pads[2] = pad ("Charles cerrado", 0, 0.5f, 0.35f, 0.4f, 0, 0.5f, 0.2f, 1);
            pads[5] = pad ("Caja (aro)", 0, 0.5f, 0.4f, 0.5f, 0, 0.55f);
            pads[6] = pad ("Maraca", 0, 0.5f, 0.4f, 0.5f, 0, 0.4f, -0.2f);
            pads[7] = pad ("Palmas", 0, 0.5f, 0.35f, 0.5f, 0, 0.55f);
            pads[10] = pad ("Cencerro", 0, 0.5f, 0.4f, 0.5f, 0, 0.45f, 0.2f);
            pads[11] = pad ("Platillo ride", 0, 0.5f, 0.35f, 0.5f, 0, 0.5f, 0.3f);
            k.push_back (kit ("Lo-fi", "Tempo lento, mucho swing y sonido de cinta gastada.", 80, 30, pads, {
                pat ({ { 1, "x.........x....." }, { 2, "....x.......x..." }, { 3, "x.o.x.o.x.o.x.o." } }),
                pat ({ { 1, "x......x..x....." }, { 2, "....x.......x..o" }, { 3, "x.o.x.o.x.o.x.ox" } }),
                pat ({ { 1, "x.x.......x....." }, { 2, "....x.......x..." }, { 6, "..o.....o......." }, { 12, "x.x.x.x.x.x.x.x." } }),
                pat ({ { 1, "x.........x..x.." }, { 2, "....x.......x..." }, { 3, "x...x...x...x..." }, { 11, "....x.......x..." } }) },
                { { "dr_vol", -2.5f }, { "fx_sat_on", 1 }, { "fx_sat_type", 3 }, { "fx_sat_amt", 0.45f }, { "fx_cmp_on", 1 }, { "fx_cmp_amt", 0.4f },
                  { "fx_rev_on", 1 }, { "fx_rev_type", 0 }, { "fx_rev_mix", 18 } }));
        }

        {
            auto pads = electronicPads ("Bombo seco", 0.15f, "Caja eléctrica");
            pads[4] = pad ("Bajo 808", -2, 0.55f, 0.5f, 0.4f, 0.2f, 0.75f);
            pads[6] = pad ("Maraca", 0, 0.5f, 0.5f, 0.5f, 0, 0.42f, -0.3f);
            pads[8] = pad ("Conga", 0, 0.5f, 0.5f, 0.5f, 0, 0.55f, -0.2f);
            pads[9] = pad ("Bongó agudo", 0, 0.5f, 0.5f, 0.5f, 0, 0.5f, 0.3f);
            pads[11] = pad ("Cencerro", 0, 0.5f, 0.5f, 0.5f, 0, 0.5f, 0.2f);
            k.push_back (kit ("Reguetón", "El ritmo dembow con bombo a negras.", 95, 0, pads, {
                pat ({ { 1, "x...x...x...x..." }, { 2, "...x..x....x..x." }, { 3, "x.x.x.x.x.x.x.x." } }),
                pat ({ { 1, "x...x...x...x..." }, { 6, "...x..x....x..x." }, { 7, "xoxoxoxoxoxoxoxo" }, { 8, "...x..x....x..x." } }),
                pat ({ { 1, "x...x...x...x..." }, { 2, "...x..x....x..x." }, { 9, "..x...x...x..x.." }, { 10, "x..x....x..x...." } }),
                pat ({ { 2, "...x..x....x..x." }, { 5, "x.......x......." }, { 16, "x..............." } }) },
                { { "dr_vol", -4.0f }, { "fx_cmp_on", 1 }, { "fx_cmp_amt", 0.4f }, { "fx_rev_on", 1 }, { "fx_rev_type", 2 }, { "fx_rev_mix", 12 } }));
        }

        {
            auto pads = acousticPads (0.0f);
            pads[0] = pad ("Bombo seco", 0, 0.4f, 0.5f, 0.7f, 0.1f, 0.85f);
            pads[1] = pad ("Caja acústica", 2, 0.45f, 0.55f, 0.6f, 0, 0.8f);
            pads[4] = pad ("Bajo 808", -5, 0.6f, 0.5f, 0.4f, 0.2f, 0.7f);
            pads[6] = pad ("Maraca", 0, 0.5f, 0.55f, 0.5f, 0, 0.42f, -0.3f);
            k.push_back (kit ("Drum and bass", "Breakbeat rápido a 172 con caja aguda.", 172, 0, pads, {
                pat ({ { 1, "x.........x....." }, { 2, "....X.......X..." }, { 3, "x.x.x.x.x.x.x.x." } }),
                pat ({ { 1, "x.........x....." }, { 2, "....X..o.o..X..o" }, { 3, "xxx.xxx.xxx.xxx." }, { 4, "...x.......x...." } }),
                pat ({ { 1, "x.x.......x.x..." }, { 2, "....X.......X.xx" }, { 7, "oxoxoxoxoxoxoxox" }, { 11, "X..............." } }),
                pat ({ { 1, "x.........x....." }, { 2, "....X.......X..." }, { 5, "x.........x....." }, { 12, "x.x.x.x.x.x.x.x." } }) },
                { { "dr_vol", -4.0f }, { "fx_cmp_on", 1 }, { "fx_cmp_amt", 0.5f }, { "fx_rev_on", 1 }, { "fx_rev_type", 0 }, { "fx_rev_mix", 10 } }));
        }

        {
            auto pads = acousticPads (3.0f, "Caja suave");
            pads[0] = pad ("Bombo seco", 3, 0.35f, 0.45f, 0.2f, 0, 0.55f);
            pads[1] = pad ("Caja suave", 0, 0.45f, 0.45f, 0.4f, 0, 0.6f);
            pads[6] = pad ("Charles de pedal", 0, 0.5f, 0.5f, 0.5f, 0, 0.5f, 0.25f, 1);
            pads[7] = pad ("Claves", 0, 0.5f, 0.5f, 0.5f, 0, 0.45f, 0.3f);
            pads[10] = pad ("Campana de ride", 0, 0.5f, 0.5f, 0.5f, 0, 0.5f, 0.35f);
            k.push_back (kit ("Jazz", "Ride con swing, charles de pie y caja suave.", 130, 55, pads, {
                pat ({ { 1, "o.......o......." }, { 2, "......o.......o." }, { 7, "....x.......x..." }, { 12, "x...x..xx...x..x" } }),
                pat ({ { 2, "..o...o..o...o.." }, { 6, "..........x....." }, { 7, "....x.......x..." }, { 12, "x...x..xx...x..x" } }),
                pat ({ { 2, "....o.o.o..oxoxo" }, { 7, "....x.......x..." }, { 9, "............x.x." }, { 10, "..........x....." }, { 12, "x...x..xx...x..x" } }),
                pat ({ { 1, "x.......x......." }, { 2, "....o.......o..." }, { 7, "....x.......x..." }, { 11, "x...x..xx...x..x" } }) },
                { { "dr_vol", -2.0f }, { "fx_rev_on", 1 }, { "fx_rev_type", 1 }, { "fx_rev_size", 0.5f }, { "fx_rev_mix", 20 } }));
        }

        k.push_back (kit ("Latina", "Cajón, congas, claves, cencerro y bloque.", 100, 0, {
                pad ("Cajón grave", 0, 0.5f, 0.5f, 0.5f, 0, 0.85f),
                pad ("Cajón golpe", 0, 0.5f, 0.5f, 0.5f, 0, 0.75f),
                pad ("Maraca", 0, 0.5f, 0.5f, 0.5f, 0, 0.5f, -0.3f),
                pad ("Palmas", 0, 0.5f, 0.5f, 0.5f, 0, 0.6f),

                pad ("Tumbadora", 0, 0.5f, 0.5f, 0.5f, 0, 0.75f, -0.3f),
                pad ("Claves", 0, 0.5f, 0.5f, 0.5f, 0, 0.6f, 0.3f),
                pad ("Pandereta", 0, 0.5f, 0.5f, 0.5f, 0, 0.5f, 0.2f),
                pad ("Triángulo", 0, 0.5f, 0.5f, 0.5f, 0, 0.45f, 0.4f),

                pad ("Conga", 0, 0.5f, 0.5f, 0.5f, 0, 0.7f, -0.1f),
                pad ("Conga aguda", 0, 0.5f, 0.5f, 0.5f, 0, 0.65f, 0.1f),
                pad ("Cencerro", 0, 0.5f, 0.5f, 0.5f, 0, 0.55f, 0.25f),
                pad ("Bloque de madera", 0, 0.5f, 0.5f, 0.5f, 0, 0.55f, -0.2f),

                melodyPad(), chordPad(), voicePad(),
                pad ("Platillo crash", 0, 0.5f, 0.5f, 0.5f, 0, 0.55f, -0.3f) }, {
                pat ({ { 1, "x.....x.x.....x." }, { 3, "x.x.x.x.x.x.x.x." }, { 6, "x..x..x...x.x..." }, { 9, "......xx......xx" }, { 10, "..x.......x....." }, { 11, "x...x...x...x..." } }),
                pat ({ { 1, "x.......x......." }, { 2, "....x.......x..." }, { 6, "x..x...x..x.x..." }, { 9, "...x.......x...." }, { 10, "x.x.x.x.x.x.x.x." } }),
                pat ({ { 1, "x..x..x...x....." }, { 2, "....x..x....x.xx" }, { 3, "xoxoxoxoxoxoxoxo" }, { 4, "x..x..x...x.x..." } }),
                pat ({ { 5, "x..x....x..x...." }, { 7, "x.xx.xx.x.xx.xx." }, { 8, "oxoxoxoxoxoxoxox" }, { 12, "..x..x....x..x.." } }) },
            { { "dr_vol", -2.5f }, { "fx_rev_on", 1 }, { "fx_rev_type", 1 }, { "fx_rev_mix", 16 } }));

        {
            auto pads = electronicPads ("Bombo seco", 0.1f, "Caja eléctrica");
            pads[0] = pad ("Bombo seco", -2, 0.5f, 0.45f, 0.5f, 0.1f, 0.85f);
            pads[1] = pad ("Caja eléctrica", -2, 0.6f, 0.5f, 0.55f, 0, 0.75f);
            k.push_back (kit ("Synthwave", "Ochentas: caja enorme con reverb y toms electrónicos.", 110, 0, pads, {
                pat ({ { 1, "x.......x......." }, { 2, "....x.......x..." }, { 3, "x.x.x.x.x.x.x.x." }, { 8, "....x.......x..." } }),
                pat ({ { 1, "x.....x.x......." }, { 2, "....x.......x..." }, { 3, "xoxoxoxoxoxoxoxo" }, { 8, "....x.......x..." } }),
                pat ({ { 1, "x.......x......." }, { 2, "....x..........." }, { 9, "..........x.x.xx" }, { 10, "........x.x....." }, { 11, "X..............." } }),
                pat ({ { 1, "x...x...x...x..." }, { 2, "....x.......x..." }, { 3, "..x...x...x...x." }, { 16, "x..............." } }) },
                { { "dr_vol", -7.5f }, { "fx_rev_on", 1 }, { "fx_rev_type", 1 }, { "fx_rev_size", 0.7f }, { "fx_rev_mix", 28 }, { "fx_cmp_on", 1 }, { "fx_cmp_amt", 0.3f } }));
        }
        return k;
    }();
    return kits;
}
} // namespace geminis::inst
