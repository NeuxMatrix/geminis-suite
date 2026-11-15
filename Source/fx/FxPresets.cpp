#include "FxPresets.h"

namespace geminis::fx
{
// Orden de los valores de cada módulo (ver Modules.cpp):
// gate {umbral, liberación, reducción} · compressor {umbral, ratio, ataque, liberación, nivel, mezcla}
// limiter {entrada, techo, liberación} · overdrive {ganancia, tono, nivel} · distortion {dist, tono, nivel, graves}
// fuzz {fuzz, tono, nivel, sesgo, octava} · booster {tipo, ganancia, brillo} · octaver {-1, -2, +1, directo, tono}
// wah {modo, pedal, velocidad, sensibilidad, resonancia, mezcla} · eq5 {90, 250, 800, 2k5, 6k, salida}
// amp {modelo, ganancia, graves, medios, agudos, presencia, master, profundidad} · cab {tipo, micro, graves, agudos, sala}
// chorus {vel, prof, retardo, mezcla, anchura} · flanger {vel, prof, retardo, realim, mezcla}
// phaser {etapas, vel, prof, centro, realim, mezcla, estéreo} · vibrato {vel, prof} · tremolo {forma, vel, sinc, prof, estéreo}
// rotary {vel, acel, bocina, sat, anchura} · delay {tipo, tiempo, sinc, realim, tono, mod, mezcla}
// reverb {tipo, decaimiento, pre, tono, mod, mezcla, anchura} · volume {vol, pan}
// vocalEq {filtro, cuerpo, nasal, presencia, aire} · deesser {freq, umbral, red} · tape {entrada, calidez, salida, mezcla}
// doubler {sep, var, anchura, mezcla} · autotune {tono, escala, vel, mezcla} · lofi {filtro, bits, muestreo, ruido, mezcla}

const std::vector<FxPreset>& fxPresets()
{
    static const std::vector<FxPreset> list = {
        //======================================================================  GUITARRA
        { "Limpio cristalino", "Limpios", Mode::guitar, {
            { gate, true, { -62, 120, 50 } },
            { compressor, true, { -24, 3, 10, 150, 5, 100 } },
            { amp, true, { 0, 0.35f, 0.5f, 0.45f, 0.6f, 0.55f, 0.5f, 0.3f } },
            { cab, true, { 0, 0.4f, 60, 12000, 0.1f } },
            { chorus, false, { 0.6f, 0.4f, 12, 35, 1 } },
            { reverb, true, { 1, 0.4f, 12, 0.6f, 0.3f, 18, 1 } } } },
        { "Funk rítmico", "Limpios", Mode::guitar, {
            { gate, true, { -60, 80, 50 } },
            { compressor, true, { -28, 6, 3, 80, 2, 100 } },
            { wah, true, { 2, 0.5f, 1.5f, 0.6f, 6, 100 } },
            { amp, true, { 0, 0.4f, 0.45f, 0.55f, 0.65f, 0.6f, 0.36f, 0.2f } },
            { cab, true, { 0, 0.35f, 70, 11000, 0.05f } },
            { reverb, true, { 0, 0.3f, 5, 0.6f, 0.2f, 12, 1 } } } },
        { "Jazz cálido", "Limpios", Mode::guitar, {
            { compressor, true, { -20, 2.5f, 15, 200, 3, 100 } },
            { amp, true, { 0, 0.3f, 0.6f, 0.45f, 0.35f, 0.4f, 0.5f, 0.3f } },
            { cab, true, { 1, 0.7f, 50, 6000, 0.15f } },
            { reverb, true, { 1, 0.35f, 10, 0.4f, 0.2f, 15, 1 } } } },
        { "Country brillante", "Limpios", Mode::guitar, {
            { compressor, true, { -26, 5, 5, 120, 2, 100 } },
            { booster, true, { 1, 4, 4 } },
            { amp, true, { 0, 0.45f, 0.45f, 0.5f, 0.7f, 0.6f, 0.38f, 0.2f } },
            { cab, true, { 0, 0.3f, 70, 12000, 0 } },
            { delay, true, { 0, 140, 0, 10, 0.7f, 0, 20 } },
            { reverb, true, { 0, 0.3f, 5, 0.6f, 0.2f, 10, 1 } } } },
        { "Surf con trémolo", "Limpios", Mode::guitar, {
            { amp, true, { 0, 0.5f, 0.4f, 0.5f, 0.7f, 0.6f, 0.5f, 0.2f } },
            { cab, true, { 0, 0.35f, 60, 12000, 0 } },
            { tremolo, true, { 0, 5.5f, 0, 60, 0 } },
            { reverb, true, { 3, 0.7f, 5, 0.55f, 0.2f, 40, 1 } } } },
        { "Indie con chorus", "Limpios", Mode::guitar, {
            { compressor, true, { -22, 3, 10, 150, 4, 100 } },
            { overdrive, true, { 0.25f, 0.6f, 0.55f } },
            { amp, true, { 1, 0.45f, 0.5f, 0.5f, 0.6f, 0.55f, 0.5f, 0.3f } },
            { cab, true, { 1, 0.45f, 60, 11000, 0.1f } },
            { chorus, true, { 0.9f, 0.5f, 10, 45, 1 } },
            { delay, true, { 0, 380, 4, 30, 0.6f, 0.1f, 25 } },
            { reverb, true, { 2, 0.45f, 15, 0.6f, 0.3f, 18, 1 } } } },

        { "Blues de bar", "Crunch", Mode::guitar, {
            { gate, true, { -64, 150, 40 } },
            { overdrive, true, { 0.35f, 0.55f, 0.6f } },
            { amp, true, { 1, 0.55f, 0.5f, 0.6f, 0.5f, 0.5f, 0.42f, 0.3f } },
            { cab, true, { 1, 0.5f, 60, 10000, 0.1f } },
            { reverb, true, { 3, 0.45f, 8, 0.5f, 0.2f, 22, 1 } } } },
        { "Crunch Britannia", "Crunch", Mode::guitar, {
            { gate, true, { -60, 120, 60 } },
            { amp, true, { 2, 0.45f, 0.5f, 0.65f, 0.55f, 0.55f, 0.5f, 0.3f } },
            { cab, true, { 2, 0.45f, 60, 12000, 0.05f } },
            { reverb, true, { 0, 0.35f, 8, 0.6f, 0.2f, 10, 1 } } } },
        { "Rock clásico", "Crunch", Mode::guitar, {
            { gate, true, { -58, 120, 60 } },
            { booster, true, { 1, 6, 2 } },
            { amp, true, { 2, 0.6f, 0.5f, 0.6f, 0.55f, 0.55f, 0.5f, 0.35f } },
            { cab, true, { 2, 0.45f, 70, 12000, 0.05f } },
            { delay, true, { 0, 330, 0, 20, 0.5f, 0.1f, 15 } },
            { reverb, true, { 1, 0.4f, 12, 0.6f, 0.3f, 12, 1 } } } },
        { "Psicodelia 60", "Crunch", Mode::guitar, {
            { fuzz, true, { 0.65f, 0.5f, 0.5f, 0.3f, 0 } },
            { amp, true, { 1, 0.5f, 0.5f, 0.55f, 0.55f, 0.5f, 0.5f, 0.3f } },
            { cab, true, { 1, 0.5f, 60, 10000, 0.1f } },
            { phaser, true, { 1, 0.4f, 0.8f, 600, 50, 50, 90 } },
            { reverb, true, { 3, 0.5f, 10, 0.5f, 0.3f, 25, 1 } } } },
        { "Grunge 90", "Crunch", Mode::guitar, {
            { gate, true, { -56, 100, 60 } },
            { distortion, true, { 0.6f, 0.45f, 0.55f, 100 } },
            { amp, true, { 2, 0.55f, 0.55f, 0.5f, 0.55f, 0.5f, 0.5f, 0.4f } },
            { cab, true, { 2, 0.5f, 70, 11000, 0.05f } },
            { chorus, true, { 0.5f, 0.4f, 14, 30, 1 } },
            { reverb, true, { 0, 0.4f, 8, 0.5f, 0.2f, 12, 1 } } } },
        { "Wah clásico", "Crunch", Mode::guitar, {
            { wah, true, { 0, 0.55f, 1.5f, 0.5f, 6, 100 } },
            { overdrive, true, { 0.4f, 0.5f, 0.6f } },
            { amp, true, { 2, 0.55f, 0.5f, 0.6f, 0.55f, 0.55f, 0.5f, 0.3f } },
            { cab, true, { 2, 0.45f, 60, 12000, 0.05f } },
            { reverb, true, { 1, 0.4f, 12, 0.6f, 0.3f, 15, 1 } } } },
        { "Rotativo órgano", "Crunch", Mode::guitar, {
            { overdrive, true, { 0.2f, 0.45f, 0.6f } },
            { amp, true, { 1, 0.45f, 0.55f, 0.55f, 0.5f, 0.5f, 0.4f, 0.3f } },
            { cab, true, { 1, 0.5f, 60, 10000, 0 } },
            { rotary, true, { 1, 1.2f, 0.5f, 0.3f, 0.8f } },
            { reverb, true, { 1, 0.45f, 15, 0.5f, 0.3f, 20, 1 } } } },

        { "Hard rock solista", "Alta ganancia", Mode::guitar, {
            { gate, true, { -56, 100, 60 } },
            { overdrive, true, { 0.3f, 0.55f, 0.65f } },
            { amp, true, { 3, 0.6f, 0.5f, 0.6f, 0.55f, 0.55f, 0.5f, 0.4f } },
            { cab, true, { 2, 0.45f, 70, 12000, 0.05f } },
            { delay, true, { 2, 420, 0, 35, 0.4f, 0.2f, 25 } },
            { reverb, true, { 2, 0.5f, 20, 0.6f, 0.3f, 15, 1 } } } },
        { "Metal moderno", "Alta ganancia", Mode::guitar, {
            { gate, true, { -52, 60, 70 } },
            { overdrive, true, { 0.05f, 0.6f, 0.75f } },
            { amp, true, { 4, 0.65f, 0.55f, 0.4f, 0.6f, 0.6f, 0.5f, 0.5f } },
            { cab, true, { 3, 0.4f, 80, 11000, 0 } },
            { eq5, true, { 0, -2, -1, 1, 1, 0 } },
            { reverb, false, { 0, 0.3f, 5, 0.5f, 0.2f, 8, 1 } } } },
        { "Djent", "Alta ganancia", Mode::guitar, {
            { gate, true, { -50, 40, 80 } },
            { booster, true, { 2, 6, 0 } },
            { amp, true, { 4, 0.75f, 0.5f, 0.35f, 0.65f, 0.65f, 0.5f, 0.6f } },
            { cab, true, { 3, 0.35f, 90, 9000, 0 } },
            { eq5, true, { 1, -3, 0, 2, 0, 0 } } } },
        { "Octava fuzz", "Alta ganancia", Mode::guitar, {
            { gate, true, { -56, 100, 60 } },
            { octaver, true, { 80, 30, 0, 70, 0.4f } },
            { fuzz, true, { 0.8f, 0.45f, 0.5f, 0.4f, 0 } },
            { amp, true, { 3, 0.4f, 0.55f, 0.5f, 0.5f, 0.5f, 0.5f, 0.4f } },
            { cab, true, { 2, 0.45f, 60, 10000, 0 } } } },

        { "Shoegaze", "Ambientes", Mode::guitar, {
            { fuzz, true, { 0.7f, 0.4f, 0.5f, 0.2f, 0.3f } },
            { amp, true, { 1, 0.5f, 0.5f, 0.5f, 0.55f, 0.5f, 0.5f, 0.3f } },
            { cab, true, { 1, 0.5f, 60, 10000, 0.1f } },
            { chorus, true, { 0.3f, 0.6f, 15, 40, 1 } },
            { delay, true, { 5, 500, 0, 40, 0.5f, 0, 35 } },
            { reverb, true, { 5, 0.8f, 30, 0.6f, 0.5f, 45, 1 } } } },
        { "Ambiente infinito", "Ambientes", Mode::guitar, {
            { compressor, true, { -24, 3, 10, 200, 4, 100 } },
            { amp, true, { 0, 0.3f, 0.5f, 0.5f, 0.6f, 0.55f, 0.5f, 0.3f } },
            { cab, true, { 0, 0.4f, 60, 12000, 0.1f } },
            { delay, true, { 1, 520, 0, 55, 0.5f, 0.4f, 35 } },
            { delay, true, { 3, 390, 0, 35, 0.6f, 0.1f, 25 } },
            { reverb, true, { 4, 0.85f, 40, 0.5f, 0.6f, 45, 1 } } } },
        { "Lo-fi de habitación", "Ambientes", Mode::guitar, {
            { compressor, true, { -22, 4, 10, 150, 5, 100 } },
            { amp, true, { 0, 0.4f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.3f } },
            { cab, true, { 6, 0.5f, 100, 8000, 0.2f } },
            { lofi, true, { 0.5f, 10, 2, 0.3f, 60 } },
            { reverb, true, { 0, 0.4f, 8, 0.4f, 0.2f, 15, 1 } } } },

        { "Bajo limpio", "Bajo", Mode::guitar, {
            { compressor, true, { -22, 4, 10, 120, 4, 100 } },
            { amp, true, { 5, 0.35f, 0.6f, 0.45f, 0.5f, 0.5f, 0.5f, 0.4f } },
            { cab, true, { 4, 0.4f, 30, 8000, 0 } } } },
        { "Bajo saturado", "Bajo", Mode::guitar, {
            { compressor, true, { -20, 4, 8, 120, 4, 100 } },
            { overdrive, true, { 0.45f, 0.4f, 0.6f } },
            { amp, true, { 5, 0.6f, 0.6f, 0.55f, 0.55f, 0.5f, 0.38f, 0.5f } },
            { cab, true, { 5, 0.45f, 35, 9000, 0 } } } },
        { "Bajo con octava", "Bajo", Mode::guitar, {
            { octaver, true, { 60, 0, 0, 100, 0.3f } },
            { compressor, true, { -22, 4, 10, 120, 4, 100 } },
            { amp, true, { 5, 0.4f, 0.6f, 0.5f, 0.5f, 0.5f, 0.5f, 0.4f } },
            { cab, true, { 4, 0.4f, 30, 8000, 0 } } } },

        //======================================================================  VOZ
        { "Voz pop brillante", "Mezcla", Mode::voice, {
            { vocalEq, true, { 90, -1, -1, 3, 4 } },
            { deesser, true, { 6500, -26, 8 } },
            { compressor, true, { -22, 4, 5, 120, 5, 100 } },
            { delay, true, { 0, 250, 4, 20, 0.6f, 0.1f, 12 } },
            { reverb, true, { 2, 0.45f, 25, 0.65f, 0.3f, 18, 1 } } } },
        { "Rap seco y presente", "Mezcla", Mode::voice, {
            { vocalEq, true, { 100, 1, -2, 4, 2 } },
            { compressor, true, { -26, 6, 3, 80, 7, 100 } },
            { deesser, true, { 6000, -24, 10 } },
            { tape, true, { 6, 0.4f, 0, 60 } },
            { limiter, true, { 2, -1, 60 } } } },
        { "Voz rock", "Mezcla", Mode::voice, {
            { vocalEq, true, { 110, 0, 0, 3, 2 } },
            { tape, true, { 10, 0.6f, -3, 100 } },
            { compressor, true, { -20, 5, 5, 100, 5, 100 } },
            { delay, true, { 1, 300, 2, 25, 0.5f, 0.2f, 15 } },
            { reverb, true, { 0, 0.35f, 10, 0.6f, 0.2f, 12, 1 } } } },
        { "Balada íntima", "Mezcla", Mode::voice, {
            { vocalEq, true, { 70, 1, -1, 2, 3 } },
            { compressor, true, { -24, 3, 15, 200, 4, 100 } },
            { deesser, true, { 6500, -28, 8 } },
            { delay, true, { 0, 450, 3, 30, 0.5f, 0.1f, 15 } },
            { reverb, true, { 1, 0.6f, 40, 0.6f, 0.3f, 28, 1 } } } },
        { "Coros dobles", "Mezcla", Mode::voice, {
            { vocalEq, true, { 120, -1, -1, 2, 4 } },
            { compressor, true, { -22, 4, 8, 150, 5, 100 } },
            { doubler, true, { 18, 0.5f, 0.9f, 60 } },
            { chorus, true, { 0.4f, 0.3f, 15, 20, 1 } },
            { reverb, true, { 1, 0.5f, 20, 0.6f, 0.3f, 22, 1 } } } },
        { "Voz etérea", "Mezcla", Mode::voice, {
            { vocalEq, true, { 120, -2, -1, 2, 5 } },
            { compressor, true, { -22, 3, 10, 150, 4, 100 } },
            { delay, true, { 3, 400, 4, 40, 0.5f, 0.2f, 30 } },
            { reverb, true, { 5, 0.8f, 40, 0.6f, 0.5f, 40, 1 } } } },

        { "Directo limpio", "Directo", Mode::voice, {
            { gate, true, { -55, 150, 20 } },
            { vocalEq, true, { 90, 0, 0, 2, 2 } },
            { compressor, true, { -22, 3, 8, 150, 4, 100 } },
            { reverb, true, { 0, 0.35f, 10, 0.6f, 0.2f, 14, 1 } } } },
        { "Locución y pódcast", "Directo", Mode::voice, {
            { gate, true, { -55, 150, 20 } },
            { vocalEq, true, { 80, 2, -2, 2, 1 } },
            { deesser, true, { 6000, -26, 8 } },
            { compressor, true, { -24, 4, 5, 150, 6, 100 } },
            { limiter, true, { 3, -1, 60 } } } },

        { "Afinación robótica", "Efectos", Mode::voice, {
            { autotune, true, { 0, 0, 1.0f, 100 } },
            { vocalEq, true, { 100, 0, -1, 3, 3 } },
            { compressor, true, { -22, 4, 5, 120, 5, 100 } },
            { reverb, true, { 2, 0.4f, 20, 0.6f, 0.3f, 15, 1 } } } },
        { "Afinación natural", "Efectos", Mode::voice, {
            { autotune, true, { 0, 0, 0.45f, 100 } },
            { vocalEq, true, { 90, 0, -1, 2, 3 } },
            { compressor, true, { -22, 3, 8, 150, 4, 100 } },
            { reverb, true, { 1, 0.45f, 20, 0.6f, 0.3f, 18, 1 } } } },
        { "Radio antigua", "Efectos", Mode::voice, {
            { vocalEq, true, { 200, 0, 3, 2, -6 } },
            { lofi, true, { 0.85f, 8, 3, 0.4f, 100 } },
            { compressor, true, { -20, 6, 5, 100, 6, 100 } } } },
        { "Megáfono", "Efectos", Mode::voice, {
            { vocalEq, true, { 300, -6, 6, 4, -12 } },
            { tape, true, { 22, 0.2f, -8, 100 } },
            { lofi, true, { 1.0f, 7, 2, 0.1f, 100 } } } },
    };
    return list;
}
} // namespace geminis::fx
