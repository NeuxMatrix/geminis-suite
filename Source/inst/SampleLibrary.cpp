#include "SampleLibrary.h"
#include "GeminisSamples.h"

namespace geminis::inst
{
const std::vector<SampleEntry>& sampleManifest()
{
    static const std::vector<SampleEntry> list = {
#include "SampleManifest.inc"
    };
    return list;
}

std::shared_ptr<DecodedSample> decodeSample (const SampleEntry& e)
{
    int size = 0;
    const char* data = GeminisSamples::getNamedResource (e.resource, size);
    if (data == nullptr || size <= 0) return nullptr;

    juce::OggVorbisAudioFormat ogg;
    std::unique_ptr<juce::AudioFormatReader> reader (ogg.createReaderFor (new juce::MemoryInputStream (data, (size_t) size, false), true));
    if (reader == nullptr) return nullptr;

    auto s = std::make_shared<DecodedSample>();
    const int len = (int) reader->lengthInSamples;
    s->data.setSize (1, len + 4);
    s->data.clear();
    reader->read (&s->data, 0, len, 0, true, false);
    s->data.setSize (1, len + 4, true, true, true);
    s->sampleRate = reader->sampleRate;
    s->root = e.root;
    s->loopStart = e.loopStart >= 0 && e.loopStart < len - 64 ? e.loopStart : -1;
    s->name = e.resource;
    // Últimas muestras de seguridad para la interpolación
    auto* w = s->data.getWritePointer (0);
    for (int i = len; i < len + 4; ++i) w[i] = s->loopStart >= 0 ? w[s->loopStart + (i - len)] : 0.0f;
    return s;
}

juce::String drumSampleDisplayName (const juce::String& resource)
{
    static const std::map<juce::String, const char*> names = {
        { "caja_acustica", "Caja acústica" }, { "caja_suave", "Caja suave" }, { "caja_aro", "Caja (aro)" },
        { "caja_vintage", "Caja vintage" }, { "bombo_concierto", "Bombo de concierto" },
        { "charles_cerrado", "Charles cerrado" }, { "charles_abierto", "Charles abierto" }, { "charles_pedal", "Charles de pedal" },
        { "platillo_crash", "Platillo crash" }, { "platillo_ride", "Platillo ride" }, { "platillo_campana", "Campana de ride" },
        { "tom_agudo", "Tom agudo" }, { "tom_grave", "Tom grave" }, { "palmas", "Palmas" }, { "cencerro", "Cencerro" },
        { "pandereta", "Pandereta" }, { "maraca", "Maraca" }, { "claves", "Claves" }, { "bloque_madera", "Bloque de madera" },
        { "conga", "Conga" }, { "conga_aguda", "Conga aguda" }, { "tumbadora", "Tumbadora" }, { "bongo_agudo", "Bongó agudo" },
        { "bongo_grave", "Bongó grave" }, { "cajon_grave", "Cajón grave" }, { "cajon_golpe", "Cajón golpe" },
        { "triangulo", "Triángulo" }, { "yunque", "Yunque" }, { "tambor_freno", "Tambor de freno" } };
    const auto key = resource.upToLastOccurrenceOf ("_ogg", false, false);
    auto it = names.find (key);
    return it != names.end() ? juce::String::fromUTF8 (it->second) : key.replaceCharacter ('_', ' ');
}
} // namespace geminis::inst
