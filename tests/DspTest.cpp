// Prueba sin interfaz de Geminis FX.
// 1) Pasa una guitarra sintética por cada módulo con sus valores por defecto, al mínimo y al máximo.
// 2) Monta cada preset de fábrica como una cadena de módulos y escribe un WAV por preset.
// Comprueba que la salida es finita, que no se dispara y que no queda en silencio.
// Con «--entrada guitarra.wav voz.wav» usa esas grabaciones (mono, 48 kHz) en lugar de las sintéticas
// y solo escribe los presets: así se hacen las demos de la web con guitarra y voz reales.
#include <juce_audio_formats/juce_audio_formats.h>
#include "fx/Modules.h"
#include "fx/FxPresets.h"
#include <iostream>

using namespace geminis::fx;

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

    std::cout << "\n" << checks << " pruebas, resultado: " << (ok ? "TODO BIEN" : "HAY FALLOS") << "\n";
    return ok ? 0 : 1;
}
