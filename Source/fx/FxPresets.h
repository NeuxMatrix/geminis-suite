#pragma once
#include "Modules.h"

namespace geminis::fx
{
enum class Mode { none = 0, guitar = 1, voice = 2 };

struct SlotPreset
{
    int type;
    bool on;
    std::vector<float> values;   // en unidades reales, en el orden de los controles del módulo
};

struct FxPreset
{
    const char* name;
    const char* category;
    Mode mode;
    std::vector<SlotPreset> slots;
};

const std::vector<FxPreset>& fxPresets();
} // namespace geminis::fx
