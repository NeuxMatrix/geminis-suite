#include "Rack.h"

namespace geminis::fx
{
void Rack::addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout, std::function<int (int)> typeOf)
{
    juce::StringArray names;
    for (const auto& m : catalog())
        names.add (juce::String::fromUTF8 (m.name));

    for (int s = 0; s < numSlots; ++s)
    {
        const juce::String prefix = "Ranura " + juce::String (s + 1);
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { typeId (s), 1 }, prefix + " tipo", names, 0));
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { onId (s), 1 }, prefix + " activa", true));

        for (int p = 0; p < paramsPerSlot; ++p)
        {
            auto toText = [typeOf, s, p] (float value, int) -> juce::String
            {
                const auto& inf = info (typeOf ? typeOf (s) : 0);
                if (p >= (int) inf.params.size()) return "-";
                const auto& spec = inf.params[(size_t) p];
                return spec.format (spec.toReal (value));
            };
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { paramId (s, p), 1 }, prefix + " control " + juce::String (p + 1),
                juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (toText)));
        }
    }
}

Rack::Rack (juce::AudioProcessorValueTreeState& s) : state (s)
{
    for (int i = 0; i < numSlots; ++i)
    {
        auto& slot = slots[(size_t) i];
        slot.typeParam = state.getRawParameterValue (typeId (i));
        slot.onParam = state.getRawParameterValue (onId (i));
        for (int p = 0; p < paramsPerSlot; ++p)
            slot.p[(size_t) p] = state.getRawParameterValue (paramId (i, p));
        slot.wet.setImmediate (1.0f);
        state.addParameterListener (typeId (i), this);
    }
}

Rack::~Rack()
{
    cancelPendingUpdate();
    for (int i = 0; i < numSlots; ++i)
        state.removeParameterListener (typeId (i), this);
}

void Rack::parameterChanged (const juce::String&, float) { triggerAsyncUpdate(); }
void Rack::handleAsyncUpdate() { syncModules(); }

void Rack::prepare (double sr, int maxBlock)
{
    sampleRate = sr;
    maxBlockSize = juce::jmax (16, maxBlock);
    dryL.assign ((size_t) maxBlockSize, 0.0f);
    dryR.assign ((size_t) maxBlockSize, 0.0f);

    for (int i = 0; i < numSlots; ++i)
    {
        auto& slot = slots[(size_t) i];
        const int t = juce::jlimit (0, numTypes - 1, juce::roundToInt (slot.typeParam->load()));
        auto m = info (t).create();
        m->prepare (sampleRate, maxBlockSize);
        if (t == cab) m->setUserIr (userIr);
        {
            const juce::SpinLock::ScopedLockType lock (slot.lock);
            std::swap (slot.module, m);
            slot.type = t;
            slot.wet.setImmediate (slot.onParam->load() > 0.5f ? 1.0f : 0.0f);
        }
        currentTypes[(size_t) i] = t;
    }
    updateLatency();
}

void Rack::syncModules()
{
    bool changed = false;
    for (int i = 0; i < numSlots; ++i)
    {
        auto& slot = slots[(size_t) i];
        const int t = juce::jlimit (0, numTypes - 1, juce::roundToInt (slot.typeParam->load()));
        if (t == slot.type && slot.module != nullptr)
            continue;

        auto m = info (t).create();
        if (sampleRate > 0.0)
            m->prepare (sampleRate, maxBlockSize);
        if (t == cab) m->setUserIr (userIr);
        {
            const juce::SpinLock::ScopedLockType lock (slot.lock);
            std::swap (slot.module, m);
            slot.type = t;
        }
        currentTypes[(size_t) i] = t;
        changed = true;
        // `m` (el módulo anterior) se destruye aquí, fuera del cerrojo
    }

    if (changed)
    {
        updateLatency();
        if (onStructureChanged) onStructureChanged();
    }
}

void Rack::updateLatency()
{
    int total = 0;
    for (auto& slot : slots)
    {
        const juce::SpinLock::ScopedLockType lock (slot.lock);
        if (slot.module != nullptr && slot.type != empty)
            total += slot.module->getLatency();
    }
    if (total != latency.exchange (total) && onLatencyChanged)
        onLatencyChanged();
}

void Rack::process (float* L, float* R, int n, Context& ctx)
{
    std::array<float, paramsPerSlot> values {};

    for (auto& slot : slots)
    {
        const juce::SpinLock::ScopedTryLockType lock (slot.lock);
        if (! lock.isLocked() || slot.module == nullptr || slot.type == empty)
            continue;
        if (juce::roundToInt (slot.typeParam->load()) != slot.type)
            continue; // el cambio de módulo está en camino

        const bool on = slot.onParam->load() > 0.5f;
        slot.wet.setTarget (on ? 1.0f : 0.0f, (int) (ctx.sampleRate * 0.015));
        if (! on && slot.wet.current <= 0.0f && slot.wet.left == 0)
            continue;

        const auto& inf = info (slot.type);
        for (size_t p = 0; p < inf.params.size() && p < (size_t) paramsPerSlot; ++p)
            values[p] = inf.params[p].toReal (slot.p[p]->load());

        if (slot.wet.left == 0 && slot.wet.current >= 1.0f)
        {
            slot.module->process (L, R, n, values.data(), ctx);
            continue;
        }

        // Fundido al encender o apagar el módulo para evitar clics
        const int m = juce::jmin (n, (int) dryL.size());
        std::copy (L, L + m, dryL.data());
        std::copy (R, R + m, dryR.data());
        slot.module->process (L, R, n, values.data(), ctx);
        for (int i = 0; i < m; ++i)
        {
            const float g = slot.wet.next();
            L[i] = dryL[(size_t) i] + (L[i] - dryL[(size_t) i]) * g;
            R[i] = dryR[(size_t) i] + (R[i] - dryR[(size_t) i]) * g;
        }
    }
}

int Rack::getType (int slot) const { return currentTypes[(size_t) slot].load(); }
bool Rack::isOn (int slot) const { return slots[(size_t) slot].onParam->load() > 0.5f; }

int Rack::countUsed() const
{
    int used = 0;
    for (int i = 0; i < numSlots; ++i)
        if (juce::roundToInt (slots[(size_t) i].typeParam->load()) != empty) ++used;
    return used;
}

void Rack::setUserIr (const juce::File& file)
{
    userIr = file;
    for (auto& slot : slots)
    {
        const juce::SpinLock::ScopedLockType lock (slot.lock);
        if (slot.module != nullptr && slot.type == cab)
            slot.module->setUserIr (file);
    }
}

//==============================================================================
static void setParam (juce::AudioProcessorValueTreeState& state, const juce::String& id, float normalised)
{
    if (auto* p = state.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalised));
        p->endChangeGesture();
    }
}

void Rack::setSlot (int slot, int type, bool on, const std::vector<float>& realValues)
{
    if (! juce::isPositiveAndBelow (slot, numSlots)) return;
    type = juce::jlimit (0, numTypes - 1, type);
    const auto& inf = info (type);

    for (int p = 0; p < paramsPerSlot; ++p)
    {
        float norm = 0.5f;
        if (p < (int) inf.params.size())
        {
            const auto& spec = inf.params[(size_t) p];
            norm = spec.toNorm (p < (int) realValues.size() ? realValues[(size_t) p] : spec.def);
        }
        setParam (state, paramId (slot, p), norm);
    }
    setParam (state, onId (slot), on ? 1.0f : 0.0f);
    if (auto* tp = state.getParameter (typeId (slot)))
        setParam (state, typeId (slot), tp->convertTo0to1 ((float) type));
    syncModules();
}

Rack::RawSlot Rack::readRaw (int slot) const
{
    RawSlot r;
    r.type = state.getParameter (typeId (slot))->getValue();
    r.on = state.getParameter (onId (slot))->getValue();
    for (int p = 0; p < paramsPerSlot; ++p)
        r.p[(size_t) p] = state.getParameter (paramId (slot, p))->getValue();
    return r;
}

void Rack::writeRaw (int slot, const RawSlot& r)
{
    for (int p = 0; p < paramsPerSlot; ++p)
        setParam (state, paramId (slot, p), r.p[(size_t) p]);
    setParam (state, onId (slot), r.on);
    setParam (state, typeId (slot), r.type);
}

void Rack::swapSlots (int a, int b)
{
    if (! juce::isPositiveAndBelow (a, numSlots) || ! juce::isPositiveAndBelow (b, numSlots) || a == b) return;
    const auto ra = readRaw (a), rb = readRaw (b);
    writeRaw (a, rb);
    writeRaw (b, ra);
    syncModules();
    if (onStructureChanged) onStructureChanged();
}

void Rack::removeSlot (int slot)
{
    if (! juce::isPositiveAndBelow (slot, numSlots)) return;
    for (int i = slot; i < numSlots - 1; ++i)
        writeRaw (i, readRaw (i + 1));
    setSlot (numSlots - 1, empty, true);
    syncModules();
    if (onStructureChanged) onStructureChanged();
}

int Rack::addModule (int type)
{
    for (int i = 0; i < numSlots; ++i)
        if (juce::roundToInt (slots[(size_t) i].typeParam->load()) == empty)
        {
            setSlot (i, type, true);
            if (onStructureChanged) onStructureChanged();
            return i;
        }
    return -1;
}

void Rack::clearAll()
{
    for (int i = 0; i < numSlots; ++i)
        setSlot (i, empty, true);
}
} // namespace geminis::fx
