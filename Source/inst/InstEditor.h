#pragma once
#include "InstProcessor.h"
#include "../common/Theme.h"

namespace geminis::inst { class InstContent; }

/** Ventana de Geminis Instrument. Todo se dibuja a 1240 x 820 y se escala al tamaño de la ventana. */
class GeminisInstrumentEditor : public juce::AudioProcessorEditor
{
public:
    static constexpr int designW = 1240, designH = 820;

    explicit GeminisInstrumentEditor (GeminisInstrumentProcessor&);
    ~GeminisInstrumentEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    GeminisInstrumentProcessor& processor;
    geminis::GeminisLookAndFeel lnf;
    std::unique_ptr<geminis::inst::InstContent> content;
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GeminisInstrumentEditor)
};
