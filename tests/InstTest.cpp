// Prueba sin interfaz de Geminis Instrument: toca todos los presets del sinte, los kits de batería
// y los instrumentos, mide niveles y comprueba que nada se dispara ni se queda mudo.
// Uso: GeminisInstTest [carpeta_para_wav]
#include "inst/InstProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cstdio>
#include <complex>

namespace
{
using namespace geminis::inst;

struct Note { double start, length; int note; float vel; };
struct Result { float peak = 0.0f, rms = 0.0f; bool finite = true; };

constexpr double sr = 48000.0;
constexpr int block = 512;
const float testVolumeDb = -12.0f;       // así el limitador de salida no tapa los niveles reales

Result render (GeminisInstrumentProcessor& p, double seconds, const std::vector<Note>& notes, juce::AudioBuffer<float>* capture = nullptr,
               std::function<void (juce::MidiBuffer&, int start, int n)> extra = {})
{
    Result r;
    const int total = (int) (seconds * sr);
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    double sum = 0.0;
    if (capture != nullptr) capture->setSize (2, total);
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = juce::jmin (block, total - pos);
        midi.clear();
        for (const auto& note : notes)
        {
            const int on = (int) (note.start * sr), off = (int) ((note.start + note.length) * sr);
            if (on >= pos && on < pos + n) midi.addEvent (juce::MidiMessage::noteOn (1, note.note, note.vel), on - pos);
            if (off >= pos && off < pos + n) midi.addEvent (juce::MidiMessage::noteOff (1, note.note), off - pos);
        }
        if (extra) extra (midi, pos, n);
        buf.setSize (2, n, false, false, true);
        p.processBlock (buf, midi);
        for (int ch = 0; ch < 2; ++ch)
        {
            const float* d = buf.getReadPointer (ch);
            for (int i = 0; i < n; ++i)
            {
                if (! std::isfinite (d[i])) r.finite = false;
                r.peak = juce::jmax (r.peak, std::abs (d[i]));
                sum += (double) d[i] * d[i];
            }
            if (capture != nullptr) capture->copyFrom (ch, pos, d, n);
        }
    }
    r.rms = (float) std::sqrt (sum / (2.0 * total));
    return r;
}

float toDb (float g) { return juce::Decibels::gainToDecibels (g, -120.0f); }

void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    if (auto os = f.createOutputStream())
        if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (os.get(), sr, 2, 24, {}, 0)))
        {
            os.release();
            w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
        }
}

/** Centroide espectral aproximado (Hz) de un trozo mono. */
float centroid (const juce::AudioBuffer<float>& b, int start)
{
    constexpr int order = 13, size = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> data ((size_t) size * 2, 0.0f);
    for (int i = 0; i < size; ++i)
    {
        const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) size);
        data[(size_t) i] = b.getSample (0, start + i) * w;
    }
    fft.performFrequencyOnlyForwardTransform (data.data());
    double num = 0.0, den = 0.0;
    for (int k = 1; k < size / 2; ++k)
    {
        const double f = k * sr / size;
        num += f * data[(size_t) k];
        den += data[(size_t) k];
    }
    return den > 0.0 ? (float) (num / den) : 0.0f;
}

std::vector<Note> phraseFor (const juce::String& category)
{
    std::vector<Note> notes;
    if (category == "Bajos" || category == "Secuencias")
    {
        const int line[] = { 36, 36, 48, 36, 39, 36, 43, 46, 36, 36, 48, 36, 41, 39, 43, 34 };
        for (int i = 0; i < 16; ++i) notes.push_back ({ i * 0.25, 0.2, line[i] + (category == "Secuencias" ? 12 : 0), i % 4 == 0 ? 1.0f : 0.75f });
    }
    else if (category == "Leads")
    {
        const int line[] = { 60, 63, 65, 67, 70, 67, 65, 63, 62, 63, 60 };
        for (int i = 0; i < 11; ++i) notes.push_back ({ i * 0.33, i == 10 ? 1.0 : 0.31, line[i], 0.85f });
    }
    else
    {
        for (int n : { 48, 55, 60, 63, 67 }) notes.push_back ({ 0.0, 2.6, n, 0.8f });
        for (int n : { 46, 53, 58, 62, 65 }) notes.push_back ({ 2.8, 0.5, n, 0.7f });
    }
    return notes;
}
} // namespace

/** Diagnóstico: cuánto cambia el timbre (centroide en armónicos) con cada onda y tipo de curva. */
float harmonicCentroid (int w, int c, float amt, const DrawnWave& d)
{
    constexpr int N = 512;
    std::vector<float> x (N);
    for (int i = 0; i < N; ++i) x[(size_t) i] = curvedValue (w, c, amt, (float) i / N, d);
    double num = 0, den = 0;
    for (int k = 1; k < 64; ++k)
    {
        std::complex<double> sum;
        for (int i = 0; i < N; ++i) sum += (double) x[(size_t) i] * std::polar (1.0, -2.0 * juce::MathConstants<double>::pi * k * i / N);
        num += k * std::abs (sum);
        den += std::abs (sum);
    }
    return den > 0 ? (float) (num / den) : 0.0f;
}

void printCurveMatrix()
{
    const DrawnWave d = DrawnWave::preset (1);
    std::printf ("%-12s", "");
    for (int c = 0; c < numCurveTypes; ++c) std::printf ("%20s", curveTypeNames()[c].toRawUTF8());
    std::printf ("\n");
    for (int w = 0; w < numBaseWaves; ++w)
    {
        std::printf ("%-12s", baseWaveNames()[w].toRawUTF8());
        for (int c = 0; c < numCurveTypes; ++c)
        {
            // ¿Algún fotograma se queda sin señal?
            float minDev = 10.0f;
            for (int f = 0; f < numFrames; ++f)
            {
                double sum = 0, sq = 0;
                for (int i = 0; i < 256; ++i) { const double v = curvedValue (w, c, (float) f / (numFrames - 1), (float) i / 256.0f, d); sum += v; sq += v * v; }
                minDev = juce::jmin (minDev, (float) std::sqrt (juce::jmax (0.0, sq / 256 - (sum / 256) * (sum / 256))));
            }
            std::printf ("   %c%4.1f %4.1f %4.1f", minDev < 0.05f ? '!' : ' ',
                         harmonicCentroid (w, c, 0.0f, d), harmonicCentroid (w, c, 0.5f, d), harmonicCentroid (w, c, 1.0f, d));
        }
        std::printf ("\n");
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc > 1 && juce::String (argv[1]) == "--curvas") { printCurveMatrix(); return 0; }
    const juce::File wavDir = argc > 1 ? juce::File (juce::String::fromUTF8 (argv[1])) : juce::File();
    if (wavDir != juce::File()) wavDir.createDirectory();

    int failures = 0, tests = 0;
    auto check = [&] (bool ok, const juce::String& what)
    {
        ++tests;
        if (! ok) { ++failures; std::printf ("  FALLO: %s\n", what.toRawUTF8()); }
    };

    auto proc = std::make_unique<GeminisInstrumentProcessor>();
    proc->setPlayConfigDetails (0, 2, sr, block);
    proc->prepareToPlay (sr, block);
    proc->setParam ("vol", testVolumeDb);

    //==========================================================================
    std::printf ("\n== SINTETIZADOR: %d presets ==\n", (int) synthPresets().size());
    std::printf ("%-28s %-22s %8s %8s\n", "preset", "categoría", "pico dB", "rms dB");
    const auto& presets = synthPresets();
    std::vector<std::pair<float, juce::String>> loudness;
    for (int i = 0; i < (int) presets.size(); ++i)
    {
        const auto& pr = presets[(size_t) i];
        proc->applySynthPreset (i);
        proc->setParam ("vol", testVolumeDb);
        proc->reset();
        juce::AudioBuffer<float> cap;
        render (*proc, 0.05, {});      // aplica el modo
        const auto r = render (*proc, 5.0, phraseFor (pr.category), &cap);
        const float peakDb = toDb (r.peak) - testVolumeDb, rmsDb = toDb (r.rms) - testVolumeDb;
        const float current = proc->getParam ("sy_vol");
        const float suggested = juce::jlimit (-24.0f, 12.0f, current + juce::jmin (-4.5f - peakDb, -18.0f - rmsDb));
        std::printf ("%-28s %-22s %8.1f %8.1f   SUGERIDO|%s|%.1f\n", pr.name.toRawUTF8(), pr.category.toRawUTF8(), peakDb, rmsDb,
                     pr.name.toRawUTF8(), suggested);
        loudness.push_back ({ rmsDb, pr.name });
        check (r.finite, pr.name + ": valores no finitos");
        check (r.rms > 1.0e-4f, pr.name + ": silencio");
        check (peakDb < 3.0f, pr.name + ": demasiado fuerte (" + juce::String (peakDb, 1) + " dB)");
        check (rmsDb > -34.0f, pr.name + ": demasiado flojo (" + juce::String (rmsDb, 1) + " dB)");
        if (wavDir != juce::File())
            writeWav (wavDir.getChildFile ("sinte " + juce::File::createLegalFileName (pr.name) + ".wav"), cap);
    }

    // La rueda de modulación curva la onda en el sonido inicial: el timbre tiene que cambiar.
    // Se compara la misma nota en el mismo instante con la rueda abajo y arriba.
    {
        auto take = [&] (int wheel)
        {
            proc->applySynthPreset (0);
            proc->setParam ("vol", testVolumeDb);
            proc->setParam ("fx_rev_on", 0.0f);
            juce::AudioBuffer<float> cap;
            render (*proc, 0.05, {}, nullptr, [wheel] (juce::MidiBuffer& m, int, int) { m.addEvent (juce::MidiMessage::controllerEvent (1, 1, wheel), 0); });
            render (*proc, 1.5, { { 0.0, 1.4, 45, 0.8f } }, &cap);
            render (*proc, 0.5, {});
            float c = 0.0f;
            for (int w = 0; w < 6; ++w) c += centroid (cap, (int) ((0.3 + 0.15 * w) * sr)) / 6.0f;
            return std::make_pair (c, cap);
        };
        const auto [c0, cap0] = take (0);
        const auto [c1, cap1] = take (127);
        std::printf ("\nCurva con la rueda: centroide %.0f Hz (abajo) -> %.0f Hz (arriba)\n", c0, c1);
        check (std::abs (c1 - c0) > c0 * 0.15f, "la rueda de modulación no cambia el timbre");
        if (wavDir != juce::File())
        {
            writeWav (wavDir.getChildFile ("sinte rueda abajo.wav"), cap0);
            writeWav (wavDir.getChildFile ("sinte rueda arriba.wav"), cap1);
        }
        render (*proc, 0.05, {}, nullptr, [] (juce::MidiBuffer& m, int, int) { m.addEvent (juce::MidiMessage::controllerEvent (1, 1, 0), 0); });
    }

    // Saturación: encenderla no debe cambiar mucho el volumen
    {
        const char* names[] = { "Cinta", "Válvula", "Furia", "Lo-fi" };
        proc->applySynthPreset (0);
        proc->setParam ("vol", testVolumeDb);
        proc->setParam ("fx_rev_on", 0.0f);
        const std::vector<Note> chord { { 0.0, 1.4, 48, 0.8f }, { 0.0, 1.4, 55, 0.8f }, { 0.0, 1.4, 60, 0.8f } };
        proc->setParam ("fx_sat_on", 0.0f);
        const float dry = render (*proc, 1.5, chord).rms;
        std::printf ("\nSaturación (dB respecto a apagada):\n");
        for (int t = 0; t < 4; ++t)
        {
            std::printf ("  %-8s", names[t]);
            for (float amt : { 0.0f, 0.5f, 1.0f })
            {
                proc->setParam ("fx_sat_on", 1.0f);
                proc->setParam ("fx_sat_type", (float) t);
                proc->setParam ("fx_sat_amt", amt);
                proc->setParam ("fx_sat_mix", 100.0f);
                const float wet = render (*proc, 1.5, chord).rms;
                const float diff = toDb (wet) - toDb (dry);
                std::printf ("  %5.1f", diff);
                check (std::abs (diff) < 5.0f, juce::String ("saturación ") + names[t] + " cambia demasiado el volumen");
            }
            std::printf ("\n");
        }
        proc->setParam ("fx_sat_on", 0.0f);
    }

    // Onda dibujada
    {
        proc->applySynthPreset (0);
        proc->setParam ("vol", testVolumeDb);
        proc->synth.setDrawnWave (0, DrawnWave::preset (7));
        proc->setParam ("o1_wave", (float) waveDrawn);
        proc->synth.updateTables();
        const auto r = render (*proc, 1.5, { { 0.0, 1.0, 57, 0.8f } });
        check (r.finite && r.rms > 1.0e-4f, "onda dibujada muda");
        std::printf ("Onda dibujada: rms %.1f dB\n", toDb (r.rms) - testVolumeDb);
    }

    //==========================================================================
    std::printf ("\n== BATERÍA: %d kits ==\n", (int) drumKits().size());
    for (int i = 0; i < (int) drumKits().size(); ++i)
    {
        proc->applyKit (i);
        proc->setParam ("vol", testVolumeDb);
        proc->setParam ("seq_mode", 2.0f);
        float worstPeak = 0.0f, rmsSum = 0.0f;
        bool finite = true;
        juce::AudioBuffer<float> cap;
        for (int pat = 0; pat < numPatterns; ++pat)
        {
            proc->setParam ("seq_pattern", (float) pat);
            proc->setParam ("seq_mode", 1.0f);
            render (*proc, 0.02, {});              // reinicia el reloj interno
            proc->setParam ("seq_mode", 2.0f);
            const double bar = 240.0 / drumKits()[(size_t) i].bpm;
            const auto r = render (*proc, bar * 2.0, {}, pat == 0 ? &cap : nullptr);
            worstPeak = juce::jmax (worstPeak, r.peak);
            rmsSum += r.rms;
            finite = finite && r.finite;
            check (r.rms > 1.0e-4f, drumKits()[(size_t) i].name + " patrón " + juce::String (pat + 1) + ": silencio");
        }
        const float peakDb = toDb (worstPeak) - testVolumeDb, rmsDb = toDb (rmsSum / numPatterns) - testVolumeDb;
        std::printf ("%-16s %5.0f bpm   pico %6.1f dB   rms %6.1f dB\n", drumKits()[(size_t) i].name.toRawUTF8(), drumKits()[(size_t) i].bpm, peakDb, rmsDb);
        check (finite, drumKits()[(size_t) i].name + ": valores no finitos");
        check (peakDb < 3.0f, drumKits()[(size_t) i].name + ": demasiado fuerte");
        if (wavDir != juce::File())
            writeWav (wavDir.getChildFile ("kit " + juce::File::createLegalFileName (drumKits()[(size_t) i].name) + ".wav"), cap);
    }

    // Los 16 pads por MIDI con el mapa de Maschine y con General MIDI
    for (int map = 0; map < 2; ++map)
    {
        proc->applyKit (0);
        proc->setParam ("vol", testVolumeDb);
        proc->setParam ("seq_mode", 0.0f);
        proc->setParam ("midi_map", (float) map);
        int sounding = 0;
        for (int pad = 0; pad < numPads; ++pad)
        {
            const auto before = proc->drums.hitCount[(size_t) pad].load();
            render (*proc, 0.15, { { 0.0, 0.05, padToNote (pad, map), 0.9f } });
            if (proc->drums.hitCount[(size_t) pad].load() != before) ++sounding;
        }
        std::printf ("Mapa %s: %d de 16 pads responden\n", map == 0 ? "Maschine" : "General MIDI", sounding);
        check (sounding == numPads, "no responden todos los pads por MIDI");
    }

    //==========================================================================
    std::printf ("\n== INSTRUMENTOS: %d ==\n", (int) instrumentList().size());
    for (int i = 0; i < (int) instrumentList().size(); ++i)
    {
        proc->applyInstrument (i);
        proc->setParam ("vol", testVolumeDb);
        proc->sampler.updateInstrument();
        render (*proc, 0.05, {});
        std::vector<Note> notes;
        const bool bass = i == 5 || i == 6 || i == 7;
        const int root = bass ? 33 : 48;
        const int line[] = { 0, 4, 7, 12, 7, 4 };
        for (int k = 0; k < 6; ++k) notes.push_back ({ k * 0.3, 0.28, root + line[k], 0.8f });
        for (int n : { 0, 7, 12, 16 }) notes.push_back ({ 2.0, 1.5, root + 12 + n, 0.7f });
        juce::AudioBuffer<float> cap;
        const auto r = render (*proc, 4.5, notes, &cap);
        const float peakDb = toDb (r.peak) - testVolumeDb, rmsDb = toDb (r.rms) - testVolumeDb;
        std::printf ("%-28s pico %6.1f dB   rms %6.1f dB\n", instrumentList()[(size_t) i].name.toRawUTF8(), peakDb, rmsDb);
        check (r.finite, instrumentList()[(size_t) i].name + ": valores no finitos");
        check (r.rms > 1.0e-4f, instrumentList()[(size_t) i].name + ": silencio");
        check (peakDb < 3.0f, instrumentList()[(size_t) i].name + ": demasiado fuerte");
        if (wavDir != juce::File())
            writeWav (wavDir.getChildFile ("instrumento " + juce::File::createLegalFileName (instrumentList()[(size_t) i].name) + ".wav"), cap);
    }

    //==========================================================================
    // Guardar y recuperar el estado (proyecto del DAW)
    {
        proc->applySynthPreset (5);
        auto d = DrawnWave::preset (3);
        d.points[4] = 0.123f;
        proc->synth.setDrawnWave (1, d);
        proc->drums.setStep (2, 7, 9, 3);
        juce::MemoryBlock state;
        proc->getStateInformation (state);
        auto other = std::make_unique<GeminisInstrumentProcessor>();
        other->setStateInformation (state.getData(), (int) state.getSize());
        check (other->getMode() == Mode::synth, "el estado no recupera el modo");
        check (std::abs (other->getParam ("f_cut") - proc->getParam ("f_cut")) < 0.01f, "el estado no recupera el corte");
        check (other->synth.getDrawnWave (1) == proc->synth.getDrawnWave (1), "el estado no recupera la onda dibujada");
        check (other->drums.getPattern (2).steps[7][9] == 3, "el estado no recupera los patrones");
        check (other->getSynthPresetName() == presets[5].name, "el estado no recupera el nombre del preset");
        std::printf ("\nEstado guardado: %d bytes\n", (int) state.getSize());
    }

    std::printf ("\n%d pruebas, %d fallos\n", tests, failures);
    return failures == 0 ? 0 : 1;
}
