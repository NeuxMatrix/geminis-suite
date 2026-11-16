// Prueba sin interfaz de Geminis FX.
// 1) Pasa una guitarra sintética por cada módulo con sus valores por defecto, al mínimo y al máximo.
// 2) Monta cada preset de fábrica como una cadena de módulos y escribe un WAV por preset.
// Comprueba que la salida es finita, que no se dispara y que no queda en silencio.
// Con «--entrada guitarra.wav voz.wav» usa esas grabaciones (mono, 48 kHz) en lugar de las sintéticas
// y solo escribe los presets: así se hacen las demos de la web con guitarra y voz reales.
#include <juce_audio_formats/juce_audio_formats.h>
#include "fx/Modules.h"
#include "fx/FxPresets.h"
#include "common/Tuner.h"
#include <iostream>
#include <numeric>
#include <juce_dsp/juce_dsp.h>

using namespace geminis::fx;

static juce::String U (const char* s) { return juce::String::fromUTF8 (s); }

static void pluck (std::vector<float>& out, double sr, double hz, int start, float amp)
{
    const int period = (int) (sr / hz);
    std::vector<float> line ((size_t) period);
    juce::Random rng (1234 + (int) hz);
    for (auto& v : line) v = rng.nextFloat() * 2.0f - 1.0f;
    float prev = 0.0f;
    for (int i = start, k = 0; i < (int) out.size(); ++i, k = (k + 1) % period)
    {
        const float v = line[(size_t) k];
        out[(size_t) i] += v * amp;
        line[(size_t) k] = 0.996f * 0.5f * (v + prev);
        prev = v;
    }
}

static std::vector<float> makeGuitar (double sr, int total)
{
    std::vector<float> dry ((size_t) total, 0.0f);
    const double notes[] = { 82.41, 110.0, 146.83, 196.0, 246.94, 329.63 };
    for (int i = 0; i < 6; ++i) pluck (dry, sr, notes[i], (int) (sr * 0.25 * i), 0.18f);
    for (int i = 0; i < 3; ++i) pluck (dry, sr, notes[i * 2], (int) (sr * 2.0), 0.14f);
    return dry;
}

static std::vector<float> makeVoice (double sr, int total)
{
    // Vocal sintética: diente de sierra con vibrato y dos formantes, más una "s" de ruido.
    std::vector<float> out ((size_t) total, 0.0f);
    geminis::dsp::Biquad f1, f2;
    f1.setBandpass (sr, 700.0, 4.0);
    f2.setBandpass (sr, 1200.0, 5.0);
    double phase = 0.0;
    juce::Random rng (7);
    for (int i = 0; i < total; ++i)
    {
        const double t = i / sr;
        const double hz = 220.0 * std::pow (2.0, (std::floor (t * 2.0) * 2.0) / 12.0) * (1.0 + 0.004 * std::sin (t * 30.0));
        phase += hz / sr;
        phase -= std::floor (phase);
        const float saw = (float) (2.0 * phase - 1.0);
        float v = f1.process (saw) * 0.6f + f2.process (saw) * 0.4f;
        if (std::fmod (t, 1.0) > 0.85) v += (rng.nextFloat() * 2.0f - 1.0f) * 0.25f;
        out[(size_t) i] = v * 0.4f;
    }
    return out;
}

//==============================================================================
// Pruebas de funcionamiento: que cada efecto haga lo que dice y se note.

/** Voz cantada sintética: fuente glotal (diente de sierra limitado en banda y suavizado) por tres
    formantes de «a», con vibrato natural. Cada nota lleva su desafinación en cents. */
struct SungNote { double midi, cents, seconds; };
static std::vector<float> makeSinging (double sr, const std::vector<SungNote>& notes, double vibratoCents = 12.0)
{
    double total = 0.0;
    for (auto& n : notes) total += n.seconds;
    std::vector<float> out ((size_t) (sr * total), 0.0f);
    geminis::dsp::Biquad f1, f2, f3;
    f1.setBandpass (sr, 750.0, 5.0); f2.setBandpass (sr, 1150.0, 6.0); f3.setBandpass (sr, 2800.0, 8.0);
    float lp = 0.0f;
    double phase = 0.0, t0 = 0.0;
    size_t i = 0;
    for (auto& n : notes)
    {
        const size_t end = std::min (out.size(), i + (size_t) (sr * n.seconds));
        for (; i < end; ++i)
        {
            const double t = (double) i / sr, local = t - t0;
            const double vib = vibratoCents * std::sin (2.0 * juce::MathConstants<double>::pi * 5.3 * t) * std::min (1.0, local / 0.25);
            const double hz = 440.0 * std::pow (2.0, (n.midi - 69.0 + (n.cents + vib) / 100.0) / 12.0);
            const double dt = hz / sr;
            phase += dt;
            if (phase >= 1.0) phase -= 1.0;
            double saw = 2.0 * phase - 1.0;
            // polyBLEP
            if (phase < dt) { const double x = phase / dt; saw -= x + x - x * x - 1.0; }
            else if (phase > 1.0 - dt) { const double x = (phase - 1.0) / dt; saw -= x * x + x + x + 1.0; }
            lp += 0.25f * ((float) saw - lp);
            const float env = (float) std::min (1.0, std::min (local / 0.03, (n.seconds - local) / 0.03));
            out[i] = (f1.process (lp) + f2.process (lp) * 0.7f + f3.process (lp) * 0.3f) * 0.9f * env;
        }
        t0 += n.seconds;
    }
    return out;
}

/** Cuerda pulsada con la frecuencia exacta (retardo fraccional con un paso todo). */
static std::vector<float> exactString (double sr, double hz, double seconds)
{
    std::vector<float> out ((size_t) (sr * seconds));
    const double P = sr / hz;
    const int N = (int) std::floor (P - 0.5);
    const double frac = P - 0.5 - N;            // el filtro de pérdidas añade medio sample
    const float c = (float) ((1.0 - frac) / (1.0 + frac));
    std::vector<float> line ((size_t) N);
    juce::Random rng (11 + (int) hz);
    for (auto& v : line) v = rng.nextFloat() * 2.0f - 1.0f;
    float prev = 0.0f, apX = 0.0f, apY = 0.0f;
    int k = 0;
    for (auto& o : out)
    {
        const float x = line[(size_t) k];
        const float lp = 0.5f * (x + prev) * 0.998f; prev = x;
        const float y = c * lp + apX - c * apY; apX = lp; apY = y;
        line[(size_t) k] = y;
        k = (k + 1) % N;
        o = x * 0.3f;
    }
    return out;
}

/** Tono medio (Hz) de un tramo, con el afinador de Geminis. */
static float pitchOf (const std::vector<float>& x, double sr, double from, double to)
{
    geminis::Tuner t;
    t.setSampleRate (sr);
    t.enabled = true;
    const size_t a = (size_t) (from * sr), b = std::min (x.size(), (size_t) (to * sr));
    for (size_t p = a; p + 256 <= b; p += 256) t.push (x.data() + p, 256);
    const auto r = t.analyse();
    return r.valid ? r.hz : 0.0f;
}

static float centsBetween (float hz, double midi) { return hz > 0.0f ? (float) (1200.0 * std::log2 (hz / (440.0 * std::pow (2.0, (midi - 69.0) / 12.0)))) : 9999.0f; }

/** Pasa una señal mono por un módulo con sus valores; devuelve izquierda y derecha. */
static std::pair<std::vector<float>, std::vector<float>> runModule (int type, std::vector<float> vals, const std::vector<float>& dry, double sr, int* latencyOut = nullptr)
{
    const auto& inf = info (type);
    auto m = inf.create();
    m->prepare (sr, 256);
    vals.resize (8, 0.0f);
    std::vector<float> L (dry), R (dry);
    Context ctx;
    ctx.sampleRate = sr;
    for (size_t pos = 0; pos < dry.size(); pos += 256)
    {
        const int n = (int) std::min ((size_t) 256, dry.size() - pos);
        ctx.stereo = false;
        m->process (L.data() + pos, R.data() + pos, n, vals.data(), ctx);
    }
    if (latencyOut != nullptr) *latencyOut = m->getLatency();
    return { L, R };
}

static std::vector<float> defaults (int type)
{
    std::vector<float> v;
    for (const auto& p : info (type).params) v.push_back (p.def);
    return v;
}

/** Variación de la envolvente en tramos de 10 ms (desviación / media) entre dos instantes. */
static float envelopeRipple (const std::vector<float>& x, double sr, double from, double to)
{
    const int frame = (int) (sr * 0.01);
    std::vector<float> e;
    for (size_t p = (size_t) (from * sr); p + (size_t) frame <= std::min (x.size(), (size_t) (to * sr)); p += (size_t) frame)
    {
        double s = 0.0;
        for (int k = 0; k < frame; ++k) s += x[p + (size_t) k] * x[p + (size_t) k];
        e.push_back ((float) std::sqrt (s / frame));
    }
    double mean = 0.0, var = 0.0;
    for (float v : e) mean += v;
    mean /= (double) juce::jmax ((size_t) 1, e.size());
    for (float v : e) var += (v - mean) * (v - mean);
    var /= (double) juce::jmax ((size_t) 1, e.size());
    return mean > 0.0 ? (float) (std::sqrt (var) / mean) : 0.0f;
}

/** Correlación entre izquierda y derecha (1 = mono, 0 = nada en común). */
static float correlation (const std::vector<float>& a, const std::vector<float>& b, double sr, double from)
{
    double ab = 0.0, aa = 0.0, bb = 0.0;
    for (size_t i = (size_t) (from * sr); i < std::min (a.size(), b.size()); ++i) { ab += a[i] * b[i]; aa += a[i] * a[i]; bb += b[i] * b[i]; }
    return aa > 0.0 && bb > 0.0 ? (float) (ab / std::sqrt (aa * bb)) : 1.0f;
}

/** Espectro medio en 24 bandas (dB), para comparar timbres. */
static std::array<float, 24> averageSpectrum (const std::vector<float>& x, double sr, double from, double to)
{
    const int N = 2048;
    juce::dsp::FFT fft (11);
    std::array<double, 24> acc {};
    std::vector<float> buf (2 * N);
    for (size_t p = (size_t) (from * sr); p + N <= std::min (x.size(), (size_t) (to * sr)); p += N / 2)
    {
        for (int k = 0; k < N; ++k) buf[(size_t) k] = x[p + (size_t) k] * (0.5f - 0.5f * std::cos (6.2831853f * (float) k / (float) N));
        std::fill (buf.begin() + N, buf.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (buf.data());
        for (int b = 0; b < 24; ++b)
        {
            const double lo = 100.0 * std::pow (2.0, b * 7.0 / 24.0), hi = lo * std::pow (2.0, 7.0 / 24.0);
            for (int k = (int) (lo / sr * N); k <= (int) (hi / sr * N) && k < N / 2; ++k) acc[(size_t) b] += buf[(size_t) k] * buf[(size_t) k];
        }
    }
    std::array<float, 24> db {};
    for (int b = 0; b < 24; ++b) db[(size_t) b] = (float) (10.0 * std::log10 (acc[(size_t) b] + 1.0e-12));
    return db;
}

/** Diferencia media de timbre (dB por banda) entre dos espectros, sin contar el nivel global. */
static float timbreDifference (const std::array<float, 24>& a, const std::array<float, 24>& b)
{
    float offset = 0.0f;
    for (int i = 0; i < 24; ++i) offset += (a[(size_t) i] - b[(size_t) i]) / 24.0f;
    float d = 0.0f;
    for (int i = 0; i < 24; ++i) d += std::abs (a[(size_t) i] - b[(size_t) i] - offset) / 24.0f;
    return d;
}

/** Cuánto se mueve el espectro con el tiempo: para cada una de 24 bandas, la distancia (dB) entre su
    nivel más alto y más bajo (percentiles 90 y 10) a lo largo del tramo, y la media de las bandas.
    Una señal estable da 0; chorus, flanger, phaser o rotativo la hacen moverse. */
static float spectralMotion (const std::vector<float>& x, double sr, double from, double to)
{
    const int N = 1024;
    juce::dsp::FFT fft (10);
    std::vector<std::vector<float>> bands (24);
    std::vector<float> buf (2 * N);
    for (size_t p = (size_t) (from * sr); p + N <= std::min (x.size(), (size_t) (to * sr)); p += N)
    {
        for (int k = 0; k < N; ++k) buf[(size_t) k] = x[p + (size_t) k] * (0.5f - 0.5f * std::cos (6.2831853f * (float) k / (float) N));
        std::fill (buf.begin() + N, buf.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (buf.data());
        for (int b = 0; b < 24; ++b)
        {
            const double lo = 100.0 * std::pow (2.0, b * 7.0 / 24.0), hi = lo * std::pow (2.0, 7.0 / 24.0);
            double e = 1.0e-12;
            for (int k = (int) (lo / sr * N); k <= (int) (hi / sr * N) && k < N / 2; ++k) e += buf[(size_t) k] * buf[(size_t) k];
            bands[(size_t) b].push_back ((float) (10.0 * std::log10 (e)));
        }
    }
    double total = 0.0;
    for (auto& v : bands)
    {
        if (v.size() < 10) continue;
        std::sort (v.begin(), v.end());
        total += v[v.size() * 9 / 10] - v[v.size() / 10];
    }
    return (float) (total / 24.0);
}

struct Result { float peak = 0.0f, rms = 0.0f; bool finite = true; };

static Result runChain (std::vector<std::unique_ptr<Module>>& chain, const std::vector<std::vector<float>>& values,
                        const std::vector<float>& dry, juce::AudioBuffer<float>* outBuf, double sr, int block)
{
    const int total = (int) dry.size();
    std::vector<float> L ((size_t) block), R ((size_t) block);
    Context ctx;
    ctx.sampleRate = sr;
    ctx.bpm = 120.0;
    Result res;
    double sum = 0.0;
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = juce::jmin (block, total - pos);
        std::copy (dry.begin() + pos, dry.begin() + pos + n, L.begin());
        std::copy (dry.begin() + pos, dry.begin() + pos + n, R.begin());
        ctx.stereo = false;
        for (size_t m = 0; m < chain.size(); ++m)
            chain[m]->process (L.data(), R.data(), n, values[m].data(), ctx);
        for (int i = 0; i < n; ++i)
        {
            if (! std::isfinite (L[(size_t) i]) || ! std::isfinite (R[(size_t) i])) res.finite = false;
            res.peak = juce::jmax (res.peak, std::abs (L[(size_t) i]), std::abs (R[(size_t) i]));
            sum += L[(size_t) i] * L[(size_t) i];
        }
        if (outBuf != nullptr)
        {
            outBuf->copyFrom (0, pos, L.data(), n);
            outBuf->copyFrom (1, pos, R.data(), n);
        }
    }
    res.rms = (float) std::sqrt (sum / total);
    return res;
}

static std::vector<float> readWav (const juce::File& f)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
    if (r == nullptr) return {};
    juce::AudioBuffer<float> b (1, (int) r->lengthInSamples);
    r->read (&b, 0, b.getNumSamples(), 0, true, false);
    return { b.getReadPointer (0), b.getReadPointer (0) + b.getNumSamples() };
}

static void writeWav (const juce::File& f, const juce::AudioBuffer<float>& buf, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
    if (os == nullptr) return;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, 2, 24, {}, 0));
    if (w == nullptr) return;
    os.release();
    w->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
}

int main (int argc, char** argv)
{
    const double sr = 48000.0;
    const int block = 256;
    int total = (int) (sr * 4.0);
    const juce::File outDir = argc > 1 ? juce::File (argv[1]) : juce::File::getCurrentWorkingDirectory();
    outDir.createDirectory();

    const bool realInput = argc > 4 && juce::String (argv[2]) == "--entrada";
    auto guitar = realInput ? readWav (juce::File (juce::String::fromUTF8 (argv[3]))) : makeGuitar (sr, total);
    auto voice = realInput ? readWav (juce::File (juce::String::fromUTF8 (argv[4]))) : makeVoice (sr, total);
    if (realInput)
    {
        if (guitar.empty() || voice.empty()) { std::cout << "No se pudieron leer las grabaciones\n"; return 1; }
        // Las dos con la misma longitud para que cada preset use un solo búfer de salida.
        total = (int) juce::jmax (guitar.size(), voice.size());
        guitar.resize ((size_t) total, 0.0f);
        voice.resize ((size_t) total, 0.0f);
    }
    bool ok = true;
    int checks = 0;

    if (argc > 2 && juce::String (argv[2]) == "--amp")
    {
        // Calibración: nivel de cada amplificador con ganancia baja, media y alta (master 0.5, tonos al centro).
        for (int model = 0; model < 6; ++model)
        {
            std::cout << "modelo " << model << ":";
            for (float g : { 0.15f, 0.5f, 0.85f })
            {
                std::vector<std::unique_ptr<Module>> chain;
                chain.push_back (info (amp).create());
                chain.back()->prepare (sr, block);
                std::vector<std::vector<float>> values { { (float) model, g, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.0f } };
                const auto r = runChain (chain, values, guitar, nullptr, sr, block);
                std::cout << "  g" << g << " rms " << juce::Decibels::gainToDecibels (r.rms) << " dB pico " << r.peak;
            }
            std::cout << "\n";
        }
        return 0;
    }

    // 1) Cada módulo por separado, en tres posiciones de sus controles.
    for (int t = 1; t < numTypes && ! realInput; ++t)
    {
        const auto& inf = info (t);
        for (int variant = 0; variant < 3; ++variant)
        {
            std::vector<float> vals;
            for (const auto& p : inf.params)
                vals.push_back (variant == 0 ? p.def : p.toReal (variant == 1 ? 0.0f : 1.0f));
            vals.resize (8, 0.0f);

            std::vector<std::unique_ptr<Module>> chain;
            chain.push_back (inf.create());
            chain.back()->prepare (sr, block);
            std::vector<std::vector<float>> values { vals };
            const auto& src = (inf.modes & forGuitar) ? guitar : voice;
            const auto r = runChain (chain, values, src, nullptr, sr, block);
            ++checks;
            const bool bad = ! r.finite || r.peak > 12.0f;   // +21 dB: todo al máximo es legítimo, el limitador final lo recoge
            if (bad) ok = false;
            if (bad || variant == 0)
                std::cout << (bad ? "FALLO  " : "ok     ") << juce::String::fromUTF8 (inf.name).paddedRight (' ', 22)
                          << (variant == 0 ? " defecto" : variant == 1 ? " mínimo " : " máximo ")
                          << "  pico " << juce::String (r.peak, 3) << "  rms " << juce::String (r.rms, 4)
                          << "  latencia " << chain.back()->getLatency() << "\n";
        }
    }

    // 2) Cada preset como cadena completa (sin el limitador final del plugin).
    const auto& presets = fxPresets();
    for (const auto& p : presets)
    {
        std::vector<std::unique_ptr<Module>> chain;
        std::vector<std::vector<float>> values;
        for (const auto& s : p.slots)
        {
            if (! s.on || s.type == empty) continue;
            const auto& inf = info (s.type);
            std::vector<float> vals;
            for (size_t i = 0; i < inf.params.size(); ++i)
                vals.push_back (i < s.values.size() ? juce::jlimit (inf.params[i].min, inf.params[i].max, s.values[i]) : inf.params[i].def);
            vals.resize (8, 0.0f);
            chain.push_back (inf.create());
            chain.back()->prepare (sr, block);
            values.push_back (vals);
        }
        juce::AudioBuffer<float> out (2, total);
        const auto r = runChain (chain, values, p.mode == Mode::voice ? voice : guitar, &out, sr, block);
        ++checks;
        const bool bad = ! r.finite || r.peak > 1.6f || r.rms < 1.0e-4f;
        if (bad) ok = false;
        const auto name = juce::String::fromUTF8 (p.name);
        std::cout << (bad ? "FALLO  " : "ok     ") << "preset " << name.paddedRight (' ', 24)
                  << "  pico " << juce::String (r.peak, 3) << "  rms " << juce::String (r.rms, 4) << "\n";
        writeWav (outDir.getChildFile ((p.mode == Mode::voice ? "voz_" : "guitarra_") + name.replaceCharacter (' ', '_') + ".wav"), out, sr);
    }

    // Con grabaciones reales solo se escriben los presets para las demos de la web.
    if (realInput)
    {
        std::cout << "\n" << checks << " presets con las grabaciones, resultado: " << (ok ? "TODO BIEN" : "HAY FALLOS") << "\n";
        return ok ? 0 : 1;
    }

    // 3) Que funcionen y se noten
    std::cout << "\n== Funcionamiento ==\n";
    auto expect = [&] (bool cond, const juce::String& what)
    {
        ++checks;
        if (! cond) ok = false;
        std::cout << (cond ? "ok     " : "FALLO  ") << what << "\n";
    };

    // Afinador: seis cuerdas, a 44,1, 48 y 96 kHz
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        float worst = 0.0f;
        for (double midi : { 40.0, 45.0, 50.0, 55.0, 59.0, 64.0 })
            for (double off : { -23.0, 0.0, 17.0 })
            {
                const auto str = exactString (rate, 440.0 * std::pow (2.0, (midi - 69.0 + off / 100.0) / 12.0), 0.6);
                const float hz = pitchOf (str, rate, 0.1, 0.6);
                worst = juce::jmax (worst, std::abs (centsBetween (hz, midi) - (float) off));
            }
        expect (worst < 1.0f, "afinador a " + juce::String (rate / 1000.0, 1) + U (" kHz: error máximo ") + juce::String (worst, 2) + " cents");
    }

    // Afinación (autotune): una voz desafinada acaba en la nota
    {
        const std::vector<SungNote> song { { 57, -38, 0.7 }, { 60, 32, 0.7 }, { 64, -22, 0.7 }, { 62, 41, 0.7 }, { 55, -30, 0.7 } };
        const auto voiceIn = makeSinging (sr, song);
        for (float speed : { 1.0f, 0.6f })
        {
            int lat = 0;
            auto [L, R] = runModule (autotune, { 0, 0, speed, 100 }, voiceIn, sr, &lat);
            const double latS = lat / sr;
            float worstIn = 0.0f, worstOut = 0.0f;
            double t = 0.0;
            for (auto& n : song)
            {
                worstIn = juce::jmax (worstIn, std::abs (centsBetween (pitchOf (voiceIn, sr, t + 0.3, t + n.seconds - 0.05), n.midi)));
                worstOut = juce::jmax (worstOut, std::abs (centsBetween (pitchOf (L, sr, t + 0.3 + latS, t + n.seconds - 0.05 + latS), n.midi)));
                t += n.seconds;
            }
            const float ripple = envelopeRipple (L, sr, 0.3 + latS, 0.65 + latS), rippleIn = envelopeRipple (voiceIn, sr, 0.3, 0.65);
            expect (worstOut < 6.0f, U ("afinación velocidad ") + juce::String (speed, 1) + ": de " + juce::String ((int) std::round (worstIn)) + " a "
                                     + juce::String (worstOut, 1) + " cents de la nota (latencia " + juce::String (latS * 1000.0, 1) + " ms)");
            expect (ripple < rippleIn + 0.06f, U ("afinación velocidad ") + juce::String (speed, 1) + U (": sin ondulación (envolvente ")
                                                + juce::String (ripple, 3) + ", entrada " + juce::String (rippleIn, 3) + ")");
        }

        // Escala: un Do# en Do mayor va a Do o a Re
        const std::vector<SungNote> sharp { { 61, 15, 0.8 } };
        const auto sIn = makeSinging (sr, sharp, 6.0);
        int lat = 0;
        auto [L, R] = runModule (autotune, { 0, 1, 1.0f, 100 }, sIn, sr, &lat);
        const float hz = pitchOf (L, sr, 0.35 + lat / sr, 0.75 + lat / sr);
        const float toC = std::abs (centsBetween (hz, 60)), toD = std::abs (centsBetween (hz, 62));
        expect (juce::jmin (toC, toD) < 6.0f, U ("afinación en Do mayor: un Do# va a ") + juce::String (toC < toD ? "Do" : "Re")
                                              + " (" + juce::String (juce::jmin (toC, toD), 1) + " cents)");

        // El timbre de la voz se conserva al corregir: se compara con la misma voz cantada afinada
        std::vector<SungNote> offPitch = song, inTune = song;
        for (auto& n : inTune) n.cents = 0.0;
        const auto offIn = makeSinging (sr, offPitch, 0.0), reference = makeSinging (sr, inTune, 0.0);
        auto [CL, CR] = runModule (autotune, { 0, 0, 1.0f, 100 }, offIn, sr, &lat);
        const float timbre = timbreDifference (averageSpectrum (reference, sr, 0.0, 3.5), averageSpectrum (CL, sr, lat / sr, 3.5 + lat / sr));
        expect (timbre < 1.5f, U ("afinación: conserva el timbre de la voz (diferencia media ") + juce::String (timbre, 2) + " dB por banda)");
    }

    // Doblador: abre la voz a los lados y se oye
    {
        const auto vIn = makeSinging (sr, { { 57, 0, 1.0 }, { 60, 0, 1.0 }, { 64, 0, 1.0 } });
        auto [L, R] = runModule (doubler, defaults (doubler), vIn, sr);
        const float corr = correlation (L, R, sr, 0.2);
        std::vector<float> side (L.size());
        for (size_t i = 0; i < L.size(); ++i) side[i] = L[i] - vIn[i];
        const float addedDb = juce::Decibels::gainToDecibels (std::sqrt (std::inner_product (side.begin(), side.end(), side.begin(), 0.0) / side.size())
                                                              / std::sqrt (std::inner_product (vIn.begin(), vIn.end(), vIn.begin(), 0.0) / vIn.size()));
        expect (corr < 0.8f && addedDb > -12.0f, U ("doblador: correlación L/R ") + juce::String (corr, 2) + U (", toma añadida ") + juce::String (addedDb, 1) + " dB");
    }

    // Modulaciones con sus valores por defecto: el sonido tiene que moverse claramente
    {
        std::vector<float> pad ((size_t) (sr * 3.0), 0.0f);
        {
            // Dos dientes de sierra sostenidos cuyo periodo cabe justo en cada tramo de análisis:
            // sin efecto, el espectro no se mueve nada.
            double ph[2] {};
            const double hz[2] { sr / 1024.0 * 2.0, sr / 1024.0 * 3.0 };
            for (size_t i = 0; i < pad.size(); ++i)
                for (int k = 0; k < 2; ++k) { ph[k] += hz[k] / sr; ph[k] -= std::floor (ph[k]); pad[i] += (float) (2.0 * ph[k] - 1.0) * 0.2f; }
        }
        const float still = spectralMotion (pad, sr, 0.2, 3.0);
        const struct { int type; const char* name; float minMotion; } mods[] = {
            { chorus, "chorus", 3.0f }, { flanger, "flanger", 5.0f }, { phaser, "phaser", 6.0f },
            { rotary, "rotativo", 4.0f }, { wah, "wah", 0.0f } };
        for (auto& m : mods)
        {
            auto vals = defaults (m.type);
            if (m.type == wah) { vals[0] = 1; }    // wah automático
            auto [L, R] = runModule (m.type, vals, pad, sr);
            const float motion = spectralMotion (L, sr, 0.2, 3.0);
            expect (motion > still + (m.type == wah ? 6.0f : m.minMotion),
                    juce::String (m.name) + (m.type == wah ? U (" automático") : "") + ": el espectro se mueve " + juce::String (motion, 1)
                        + " dB (sin efecto " + juce::String (still, 1) + ")");
        }
        // Vibrato y chorus: cuánto oscila la afinación de un tono puro (cents, de pico a pico)
        {
            std::vector<float> tone ((size_t) (sr * 2.0));
            for (size_t i = 0; i < tone.size(); ++i) tone[i] = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * (double) i / sr);
            for (int type : { (int) vibrato, (int) chorus })
            {
                auto [L, R] = runModule (type, defaults (type), tone, sr);
                if (type == chorus) for (size_t i = 0; i < L.size(); ++i) L[i] -= 0.5f * tone[i];   // solo la voz retardada
                std::vector<double> crossings;
                for (size_t i = (size_t) (0.3 * sr); i + 1 < L.size(); ++i)
                    if (L[i] <= 0.0f && L[i + 1] > 0.0f) crossings.push_back ((double) i + L[i] / (L[i] - L[i + 1]));
                double lo = 1.0e9, hi = 0.0;
                for (size_t k = 4; k < crossings.size(); ++k)
                {
                    const double hz = 4.0 * sr / (crossings[k] - crossings[k - 4]);
                    lo = std::min (lo, hz); hi = std::max (hi, hz);
                }
                const float cents = (float) (1200.0 * std::log2 (hi / lo));
                expect (cents > (type == vibrato ? 40.0f : 12.0f), juce::String (type == vibrato ? "vibrato" : "chorus")
                                                                       + U (": la afinación oscila ") + juce::String ((int) std::round (cents)) + " cents");
            }
        }

        // De-esser: baja las eses
        {
            const auto vs = makeVoice (sr, (int) (sr * 2.0));
            auto [L, R] = runModule (deesser, defaults (deesser), vs, sr);
            const float before = averageSpectrum (vs, sr, 0.86, 0.99)[22], after = averageSpectrum (L, sr, 0.86, 0.99)[22];
            expect (before - after > 4.0f, U ("de-esser: baja las eses ") + juce::String (before - after, 1) + " dB");
        }

        // Cada módulo con sus valores por defecto cambia el sonido de forma audible
        // (menos los que por defecto deben dejarlo igual: ecualizadores planos, volumen 0 dB, puerta y limitador)
        const auto gtr = makeGuitar (sr, (int) (sr * 3.0));
        const auto voc = makeSinging (sr, { { 57, 0, 1.0 }, { 60, 0, 1.0 }, { 64, 0, 1.0 } });
        juce::StringArray quiet;
        for (int t = 1; t < numTypes; ++t)
        {
            if (t == eq5 || t == volume || t == gate || t == limiter || t == deesser) continue;
            const auto& src = (info (t).modes & forGuitar) ? gtr : voc;
            int lat = 0;
            auto [L, R] = runModule (t, defaults (t), src, sr, &lat);
            double num = 0.0, den = 0.0;
            for (size_t i = (size_t) (0.1 * sr); i + (size_t) lat < src.size(); ++i)
            {
                const float l = L[i + (size_t) lat], r = R[i + (size_t) lat];
                const float d = 0.5f * (l + r) - src[i], dside = 0.5f * (l - r);
                num += d * d + dside * dside; den += src[i] * src[i];
            }
            const float db = (float) (10.0 * std::log10 (num / den + 1.0e-12));
            if (db < -15.0f) quiet.add (juce::String::fromUTF8 (info (t).name) + " (" + juce::String (db, 1) + " dB)");
        }
        expect (quiet.isEmpty(), U ("todos los efectos cambian el sonido con sus valores de fábrica")
                                     + (quiet.isEmpty() ? juce::String() : ": se notan poco " + quiet.joinIntoString (", ")));

        auto [TL, TR] = runModule (tremolo, defaults (tremolo), pad, sr);
        const float trem = envelopeRipple (TL, sr, 0.2, 3.0);
        expect (trem > 0.2f, U ("trémolo: variación de volumen ") + juce::String (trem, 2));
    }

    std::cout << "\n" << checks << " pruebas, resultado: " << (ok ? "TODO BIEN" : "HAY FALLOS") << "\n";
    return ok ? 0 : 1;
}
