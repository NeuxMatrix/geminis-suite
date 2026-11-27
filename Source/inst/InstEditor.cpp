#include "InstEditor.h"
#include <thread>
#include "../common/AppHooks.h"
#include "../common/Splash.h"
#include "../common/Widgets.h"

namespace geminis::inst
{
using APVTS = juce::AudioProcessorValueTreeState;

namespace
{
    juce::Colour modeColour (Mode m)
    {
        switch (m)
        {
            case Mode::synth:       return colours::violet;
            case Mode::drums:       return colours::rose;
            case Mode::instruments: return colours::cyan;
            case Mode::none:        break;
        }
        return colours::gold;
    }

    juce::String modeTitle (Mode m)
    {
        switch (m)
        {
            case Mode::synth:       return "SINTETIZADOR";
            case Mode::drums:       return U ("BATERÍA");
            case Mode::instruments: return "INSTRUMENTOS";
            case Mode::none:        break;
        }
        return {};
    }

    void setupKnob (ParamKnob& k, APVTS& s, const juce::String& id, const juce::String& label, juce::Colour c)
    {
        k.attach (s, id, label, c);
        if (auto* d = findDef (id))
        {
            k.setBipolar (d->min < 0.0f);
            k.slider.setDoubleClickReturnValue (true, d->def);
            k.slider.setTooltip (d->name);
        }
    }

    void layoutRow (juce::Rectangle<int> r, std::initializer_list<juce::Component*> comps, int gap = 4)
    {
        const int n = (int) comps.size();
        if (n == 0) return;
        const int w = (r.getWidth() - gap * (n - 1)) / n;
        for (auto* c : comps)
        {
            c->setBounds (r.removeFromLeft (w));
            r.removeFromLeft (gap);
        }
    }

    /** Color de un pad según el tipo de sonido. */
    juce::Colour soundColour (const juce::String& name)
    {
        auto has = [&name] (const char* w) { return name.containsIgnoreCase (juce::String::fromUTF8 (w)); };
        if (has ("Bombo") || has ("808")) return juce::Colour (0xffff6b6b);
        if (has ("Caja") || has ("Aro") || has ("Palmas") || has ("Cajón golpe")) return colours::gold;
        if (has ("Charles") || has ("Maraca") || has ("Pandereta") || has ("Ruido")) return colours::cyan;
        if (has ("Tom") || has ("Conga") || has ("Bongo") || has ("Tumbadora") || has ("Cajón")) return colours::violet;
        if (has ("Platillo") || has ("Triángulo") || has ("Campana")) return colours::green;
        if (has ("Láser") || has ("Glitch") || has ("Metal") || has ("Yunque") || has ("freno")) return juce::Colour (0xffff9f5a);
        if (has ("Nota pulsada") || has ("Acorde") || has ("Voz")) return juce::Colour (0xffb98cff);
        return colours::rose;
    }

    struct Family { juce::String name; juce::Colour colour; };
    Family instrumentFamily (int index)
    {
        if (index <= 2)  return { "Teclas", colours::gold };
        if (index <= 5)  return { "Cuerdas", colours::violet };
        if (index <= 7)  return { "Bajos", juce::Colour (0xffff6b6b) };
        if (index <= 11) return { "Guitarras", juce::Colour (0xffff9f5a) };
        if (index == 12) return { "Cuerdas", colours::violet };
        if (index <= 15) return { "Vientos", colours::cyan };
        return { U ("Percusión afinada"), colours::green };
    }
}

//==============================================================================
/** Combo unido a un parámetro de elección. */
class ChoiceBox : public juce::ComboBox
{
public:
    void attach (APVTS& s, const juce::String& id, bool keepItems = false)
    {
        attachment.reset();
        if (! keepItems)
        {
            clear (juce::dontSendNotification);
            if (auto* d = findDef (id)) addItemList (d->choices, 1);
        }
        if (auto* d = findDef (id)) setTooltip (d->name);
        attachment = std::make_unique<APVTS::ComboBoxAttachment> (s, id, *this);
    }

private:
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
};

class PowerToggle : public juce::ToggleButton
{
public:
    void attach (APVTS& s, const juce::String& id, juce::Colour c)
    {
        setColour (juce::ToggleButton::tickColourId, c);
        attachment = std::make_unique<APVTS::ButtonAttachment> (s, id, *this);
    }

private:
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

/** Panel de cristal con título. */
class Panel : public juce::Component, public juce::SettableTooltipClient
{
public:
    Panel (juce::String t, juce::Colour c) : title (std::move (t)), accent (c) {}

    void paint (juce::Graphics& g) override
    {
        draw::glassPanel (g, getLocalBounds().toFloat(), accent, 0.22f, 10.0f);
        if (title.isNotEmpty())
        {
            g.setColour (accent);
            draw::spacedText (g, upper (title), Fonts::ui (10.5f, 700), { 14.0f, 4.0f, (float) getWidth() - 28.0f, 22.0f },
                              juce::Justification::centredLeft, 0.22f);
        }
        paintExtra (g);
    }

    virtual void paintExtra (juce::Graphics&) {}
    virtual void refresh() {}

    juce::Rectangle<int> header() const { return getLocalBounds().removeFromTop (28).reduced (10, 3); }
    juce::Rectangle<int> body() const { return getLocalBounds().withTrimmedTop (28).reduced (10, 6); }

    juce::String title;
    juce::Colour accent;
};

void paintScreen (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (juce::Colour (0xff0a0a15));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (colours::edge.withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
}

//==============================================================================
/** Dibujo de la onda de un oscilador. Se puede dibujar encima con el ratón. */
class WaveView : public juce::Component, public juce::SettableTooltipClient
{
public:
    WaveView (GeminisInstrumentProcessor& p, int oscIndex, juce::Colour c) : proc (p), osc (oscIndex), colour (c)
    {
        const juce::String pre = "o" + juce::String (osc + 1) + "_";
        waveId = pre + "wave";
        curveId = pre + "curve";
        pWave = p.apvts.getRawParameterValue (pre + "wave");
        pType = p.apvts.getRawParameterValue (pre + "ctype");
        pCurve = p.apvts.getRawParameterValue (pre + "curve");
        setMouseCursor (juce::MouseCursor::CrosshairCursor);
        setTooltip (U ("Haz clic y arrastra para dibujar tu propia onda"));
        refresh();
    }

    void refresh()
    {
        const int w = juce::roundToInt (pWave->load()), t = juce::roundToInt (pType->load());
        const float c = pCurve->load();
        const auto d = proc.synth.getDrawnWave (osc);
        if (w == wave && t == type && std::abs (c - curve) < 0.0005f && d == drawn) return;
        wave = w; type = t; curve = c; drawn = d;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        paintScreen (g, r);
        const auto area = waveArea();
        g.setColour (colours::edge.withAlpha (0.45f));
        g.drawHorizontalLine (juce::roundToInt (area.getCentreY()), area.getX(), area.getRight());
        for (int k = 1; k < 4; ++k)
            g.drawVerticalLine (juce::roundToInt (area.getX() + area.getWidth() * (float) k / 4.0f), area.getY(), area.getBottom());

        // Recorrido de la curva en perspectiva: del fondo (curva 1) al frente (curva 0)
        for (int k = 5; k >= 1; --k)
        {
            const auto shifted = area.translated ((float) k * 4.0f, (float) -k * 2.5f).reduced ((float) k * 1.5f, (float) k * 1.0f);
            g.setColour (colour.withAlpha (0.05f + 0.025f * (float) (5 - k)));
            g.strokePath (makePath ((float) k / 5.0f, shifted), juce::PathStrokeType (1.0f));
        }
        const auto main = makePath (curve, area);
        g.setColour (colour.withAlpha (0.16f));
        g.strokePath (main, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colour);
        g.strokePath (main, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (wave == waveDrawn)
        {
            g.setColour (colours::text.withAlpha (0.7f));
            for (int i = 0; i < DrawnWave::numPoints; ++i)
            {
                const float x = area.getX() + area.getWidth() * ((float) i + 0.5f) / (float) DrawnWave::numPoints;
                g.fillEllipse (x - 1.5f, yFor (drawn.points[(size_t) i], area) - 1.5f, 3.0f, 3.0f);
            }
        }
        g.setColour (colours::textFaint);
        g.setFont (Fonts::ui (10.0f, 600));
        g.drawText (wave == waveDrawn ? U ("dibuja con el ratón") : U ("clic para dibujar"), r.reduced (8.0f, 3.0f), juce::Justification::bottomRight);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (wave != waveDrawn)
        {
            // Se empieza a dibujar desde la onda que está sonando
            DrawnWave d;
            for (int i = 0; i < DrawnWave::numPoints; ++i)
                d.points[(size_t) i] = juce::jlimit (-1.0f, 1.0f, curvedValue (wave, type, curve, ((float) i + 0.5f) / (float) DrawnWave::numPoints, drawn));
            d.smooth = 1.0f;
            proc.synth.setDrawnWave (osc, d);
            proc.setParam (waveId, (float) waveDrawn);
            proc.setParam (curveId, 0.0f);
            refresh();
        }
        lastIndex = -1;
        drawAt (e.position);
    }

    void mouseDrag (const juce::MouseEvent& e) override { drawAt (e.position); }

private:
    juce::Rectangle<float> waveArea() const
    {
        return getLocalBounds().toFloat().reduced (10.0f, 10.0f).withTrimmedRight (20.0f).withTrimmedTop (12.0f);
    }

    static float yFor (float v, juce::Rectangle<float> a) { return a.getCentreY() - v * a.getHeight() * 0.45f; }

    juce::Path makePath (float amount, juce::Rectangle<float> area) const
    {
        juce::Path p;
        constexpr int n = 140;
        for (int i = 0; i <= n; ++i)
        {
            const float ph = (float) i / (float) n;
            const float v = juce::jlimit (-1.15f, 1.15f, curvedValue (wave, type, amount, juce::jmin (ph, 0.9999f), drawn));
            const float x = area.getX() + area.getWidth() * ph;
            if (i == 0) p.startNewSubPath (x, yFor (v, area));
            else        p.lineTo (x, yFor (v, area));
        }
        return p;
    }

    void drawAt (juce::Point<float> pos)
    {
        const auto area = waveArea();
        const int idx = juce::jlimit (0, DrawnWave::numPoints - 1, (int) ((pos.x - area.getX()) / area.getWidth() * (float) DrawnWave::numPoints));
        const float v = juce::jlimit (-1.0f, 1.0f, (area.getCentreY() - pos.y) / (area.getHeight() * 0.45f));
        auto d = proc.synth.getDrawnWave (osc);
        if (lastIndex < 0 || lastIndex == idx)
            d.points[(size_t) idx] = v;
        else
        {
            const int step = idx > lastIndex ? 1 : -1;
            for (int i = lastIndex; i != idx + step; i += step)
            {
                const float t = (float) (i - lastIndex) / (float) (idx - lastIndex);
                d.points[(size_t) i] = lastValue + (v - lastValue) * t;
            }
        }
        lastIndex = idx;
        lastValue = v;
        proc.synth.setDrawnWave (osc, d);
        refresh();
    }

    GeminisInstrumentProcessor& proc;
    int osc;
    juce::Colour colour;
    juce::String waveId, curveId;
    std::atomic<float> *pWave, *pType, *pCurve;
    int wave = -1, type = -1;
    float curve = -1.0f;
    DrawnWave drawn;
    int lastIndex = -1;
    float lastValue = 0.0f;
};

/** Forma de una envolvente ADSR. */
class EnvView : public juce::Component
{
public:
    EnvView (APVTS& s, juce::Colour c) : state (s), colour (c) { setPrefix ("ea_"); }

    void setPrefix (const juce::String& p)
    {
        const char* names[] = { "a", "d", "s", "r" };
        for (int k = 0; k < 4; ++k) params[(size_t) k] = state.getRawParameterValue (p + names[k]);
        last.fill (-1.0f);
        refresh();
    }

    void refresh()
    {
        std::array<float, 4> v;
        for (int k = 0; k < 4; ++k) v[(size_t) k] = params[(size_t) k]->load();
        if (v == last) return;
        last = v;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        paintScreen (g, r);
        const auto a = r.reduced (10.0f, 12.0f);
        auto tw = [] (float s) { return std::sqrt (juce::jlimit (0.0f, 10.0f, s) / 10.0f); };
        const float unit = a.getWidth() / 3.3f;
        const float x0 = a.getX(), x1 = x0 + tw (last[0]) * unit, x2 = x1 + tw (last[1]) * unit, x3 = x2 + 0.3f * unit,
                    x4 = x3 + tw (last[3]) * unit;
        const float top = a.getY(), bottom = a.getBottom(), sy = bottom - last[2] * a.getHeight();

        juce::Path p;
        p.startNewSubPath (x0, bottom);
        p.lineTo (x1, top);
        p.quadraticTo (x1 + (x2 - x1) * 0.15f, sy, x2, sy);
        p.lineTo (x3, sy);
        p.quadraticTo (x3 + (x4 - x3) * 0.15f, bottom, x4, bottom);

        juce::Path fill (p);
        fill.lineTo (x0, bottom);
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.28f), 0.0f, top, colour.withAlpha (0.02f), 0.0f, bottom, false));
        g.fillPath (fill);
        g.setColour (colour);
        g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colours::text);
        for (auto pt : { juce::Point<float> (x1, top), juce::Point<float> (x2, sy), juce::Point<float> (x3, sy) })
            g.fillEllipse (pt.x - 3.0f, pt.y - 3.0f, 6.0f, 6.0f);
    }

    void setAccent (juce::Colour c) { colour = c; repaint(); }

private:
    APVTS& state;
    juce::Colour colour;
    std::array<std::atomic<float>*, 4> params {};
    std::array<float, 4> last {};
};

/** Respuesta aproximada del filtro. */
class FilterView : public juce::Component
{
public:
    FilterView (APVTS& s, juce::Colour c) : colour (c)
    {
        pType = s.getRawParameterValue ("f_type");
        pCut = s.getRawParameterValue ("f_cut");
        pRes = s.getRawParameterValue ("f_res");
    }

    void refresh()
    {
        const int t = juce::roundToInt (pType->load());
        const float c = pCut->load(), r = pRes->load();
        if (t == type && c == cut && r == res) return;
        type = t; cut = c; res = r;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        paintScreen (g, r);
        const auto a = r.reduced (6.0f, 6.0f);
        const double q = 0.6 + res * res * 10.0;
        juce::Path p;
        for (int i = 0; i <= (int) a.getWidth(); i += 2)
        {
            const double f = 20.0 * std::pow (1000.0, (double) i / a.getWidth());
            const double w = f / juce::jmax (20.0f, cut);
            const std::complex<double> den (1.0 - w * w, w / q);
            std::complex<double> h;
            switch (type)
            {
                case 0:  h = 1.0 / (den * den); break;
                case 1:  h = 1.0 / den; break;
                case 2:  h = std::complex<double> (0.0, w / q) / den; break;
                case 3:  h = -w * w / den; break;
                default: h = (1.0 - w * w) / den; break;
            }
            const float db = (float) juce::jlimit (-36.0, 24.0, 20.0 * std::log10 (std::abs (h) + 1.0e-9));
            const float y = juce::jmap (db, 24.0f, -36.0f, a.getY(), a.getBottom());
            if (i == 0) p.startNewSubPath (a.getX() + (float) i, y);
            else        p.lineTo (a.getX() + (float) i, y);
        }
        juce::Path fill (p);
        fill.lineTo (a.getRight(), a.getBottom());
        fill.lineTo (a.getX(), a.getBottom());
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.25f), 0.0f, a.getY(), colour.withAlpha (0.0f), 0.0f, a.getBottom(), false));
        g.fillPath (fill);
        g.setColour (colour);
        g.strokePath (p, juce::PathStrokeType (1.8f));
    }

private:
    juce::Colour colour;
    std::atomic<float> *pType, *pCut, *pRes;
    int type = -1;
    float cut = -1.0f, res = -1.0f;
};

/** Osciloscopio de la salida. */
class ScopeView : public juce::Component
{
public:
    ScopeView (GeminisInstrumentProcessor& p, juce::Colour c) : proc (p), colour (c) {}

    void refresh()
    {
        constexpr int N = GeminisInstrumentProcessor::scopeSize;
        const int end = proc.scopeWrite.load();
        float peak = 0.0f;
        for (int i = 0; i < (int) buf.size(); ++i)
        {
            buf[(size_t) i] = proc.scope[(size_t) ((end - (int) buf.size() + i + N * 2) % N)];
            peak = juce::jmax (peak, std::abs (buf[(size_t) i]));
        }
        start = 0;
        for (int i = 1; i < 400; ++i)
            if (buf[(size_t) i - 1] <= 0.0f && buf[(size_t) i] > 0.0f) { start = i; break; }
        if (peak < 1.0e-4f && silent) return;
        silent = peak < 1.0e-4f;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        paintScreen (g, r);
        const auto a = r.reduced (8.0f, 8.0f);
        g.setColour (colours::edge.withAlpha (0.4f));
        g.drawHorizontalLine (juce::roundToInt (a.getCentreY()), a.getX(), a.getRight());
        juce::Path p;
        constexpr int shown = 600;
        for (int i = 0; i < shown; ++i)
        {
            const float v = juce::jlimit (-1.0f, 1.0f, buf[(size_t) (start + i)] * 1.6f);
            const float x = a.getX() + a.getWidth() * (float) i / (float) (shown - 1);
            const float y = a.getCentreY() - v * a.getHeight() * 0.48f;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (colour.withAlpha (0.2f));
        g.strokePath (p, juce::PathStrokeType (5.0f));
        g.setColour (colour);
        g.strokePath (p, juce::PathStrokeType (1.5f));
        g.setColour (colours::textFaint);
        g.setFont (Fonts::ui (10.0f, 600));
        g.drawText (U ("SALIDA"), r.reduced (8.0f, 4.0f), juce::Justification::topRight);
    }

private:
    GeminisInstrumentProcessor& proc;
    juce::Colour colour;
    std::array<float, 1024> buf {};
    int start = 0;
    bool silent = false;
};

/** Forma de un LFO. */
class LfoShapeView : public juce::Component
{
public:
    LfoShapeView (std::atomic<float>* shapeParam, juce::Colour c) : shape (shapeParam), colour (c) {}
    void refresh() { const int s = juce::roundToInt (shape->load()); if (s != last) { last = s; repaint(); } }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        paintScreen (g, r);
        const auto a = r.reduced (8.0f, 10.0f);
        juce::Path p;
        juce::Random rnd (7);
        float held = 0.0f, prev = 0.0f;
        for (int i = 0; i <= 64; ++i)
        {
            const float ph = (float) i / 64.0f * 2.0f;
            const float f = ph - std::floor (ph);
            float v = 0.0f;
            switch (last)
            {
                case 0: v = std::sin (ph * juce::MathConstants<float>::twoPi); break;
                case 1: v = 1.0f - 4.0f * std::abs (f - 0.5f); break;
                case 2: v = 2.0f * f - 1.0f; break;
                case 3: v = f < 0.5f ? 1.0f : -1.0f; break;
                case 4: if (i % 8 == 0) held = rnd.nextFloat() * 2.0f - 1.0f; v = held; break;
                default: if (i % 8 == 0) { prev = held; held = rnd.nextFloat() * 2.0f - 1.0f; }
                         v = prev + (held - prev) * (float) (i % 8) / 8.0f; break;
            }
            const float x = a.getX() + a.getWidth() * (float) i / 64.0f, y = a.getCentreY() - v * a.getHeight() * 0.45f;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (colour);
        g.strokePath (p, juce::PathStrokeType (1.6f));
    }

private:
    std::atomic<float>* shape;
    juce::Colour colour;
    int last = -1;
};

//==============================================================================
//  SINTETIZADOR
//==============================================================================
class OscPanel : public Panel
{
public:
    OscPanel (GeminisInstrumentProcessor& p, int o, juce::Colour c)
        : Panel (o == 0 ? "Oscilador 1" : "Oscilador 2", c), proc (p), osc (o), view (p, o, c)
    {
        const juce::String pre = "o" + juce::String (o + 1) + "_";
        auto& s = p.apvts;
        wave.attach (s, pre + "wave");
        type.attach (s, pre + "ctype");
        type.setTooltip (U ("Cómo se dobla la onda al girar CURVA: curvar, plegar, sincro, formante o bits"));
        setupKnob (curve, s, pre + "curve", "CURVA", c);
        curve.slider.setTooltip (U ("Curva la onda: el sonido cambia según la doblas. En muchos presets la mueve la rueda de modulación."));
        setupKnob (oct, s, pre + "oct", "Octava", c);
        setupKnob (semi, s, pre + "semi", "Semitono", c);
        setupKnob (fine, s, pre + "fine", "Fino", c);
        setupKnob (level, s, pre + "level", "Nivel", c);
        setupKnob (uni, s, pre + "uni", U ("Unísono"), c);
        setupKnob (det, s, pre + "det", "Desafinar", c);

        shapes.setTextWhenNothingSelected (U ("Formas para dibujar"));
        shapes.addItemList (DrawnWave::presetNames(), 1);
        shapes.setTooltip (U ("Carga una forma de partida en la onda dibujada"));
        shapes.onChange = [this]
        {
            const int i = shapes.getSelectedItemIndex();
            if (i < 0) return;
            auto d = DrawnWave::preset (i);
            d.smooth = proc.synth.getDrawnWave (osc).smooth;
            proc.synth.setDrawnWave (osc, d);
            proc.setParam ("o" + juce::String (osc + 1) + "_wave", (float) waveDrawn);
            shapes.setSelectedId (0, juce::dontSendNotification);
        };
        smooth.setTooltip (U ("Une los puntos de tu onda con curvas suaves o con escalones"));
        smooth.onClick = [this]
        {
            auto d = proc.synth.getDrawnWave (osc);
            d.smooth = d.smooth > 0.5f ? 0.0f : 1.0f;
            proc.synth.setDrawnWave (osc, d);
            refresh();
        };
        for (auto* comp : std::initializer_list<juce::Component*> { &view, &wave, &type, &shapes, &smooth, &curve, &oct, &semi, &fine, &level, &uni, &det })
            addAndMakeVisible (comp);
        refresh();
    }

    void refresh() override
    {
        view.refresh();
        smooth.setButtonText (proc.synth.getDrawnWave (osc).smooth > 0.5f ? juce::String ("Suave") : U ("Escalón"));
    }

    void resized() override
    {
        auto head = header();
        type.setBounds (head.removeFromRight (104));
        head.removeFromRight (6);
        wave.setBounds (head.removeFromRight (112));
        auto b = body();
        layoutRow (b.removeFromBottom (66), { &oct, &semi, &fine, &level, &uni, &det }, 2);
        b.removeFromBottom (4);
        auto left = b.removeFromLeft (204);
        auto row = left.removeFromBottom (22);
        smooth.setBounds (row.removeFromRight (70));
        row.removeFromRight (4);
        shapes.setBounds (row);
        left.removeFromBottom (4);
        view.setBounds (left);
        b.removeFromLeft (6);
        curve.setBounds (b);
    }

private:
    GeminisInstrumentProcessor& proc;
    int osc;
    WaveView view;
    ChoiceBox wave, type;
    juce::ComboBox shapes;
    juce::TextButton smooth;
    ParamKnob curve, oct, semi, fine, level, uni, det;
};

class MixPanel : public Panel
{
public:
    MixPanel (GeminisInstrumentProcessor& p, juce::Colour c) : Panel ("Fuentes", c), scope (p, c)
    {
        auto& s = p.apvts;
        subShape.attach (s, "sub_shape");
        setupKnob (sub, s, "sub", "Sub", c);
        setupKnob (noise, s, "noise", "Ruido", c);
        setupKnob (fm, s, "fm", "FM", c);
        setupKnob (ring, s, "ring", "Anillo", c);
        fm.slider.setTooltip (U ("El oscilador 2 modula la frecuencia del 1: sonidos metálicos y de campana"));
        ring.slider.setTooltip (U ("Multiplica los dos osciladores: timbres inarmónicos e industriales"));
        for (auto* comp : std::initializer_list<juce::Component*> { &subShape, &sub, &noise, &fm, &ring, &scope })
            addAndMakeVisible (comp);
    }

    void refresh() override { scope.refresh(); }

    void resized() override
    {
        subShape.setBounds (header().removeFromRight (110));
        auto b = body();
        layoutRow (b.removeFromTop (70), { &sub, &noise, &fm, &ring }, 4);
        b.removeFromTop (6);
        scope.setBounds (b);
    }

private:
    ChoiceBox subShape;
    ParamKnob sub, noise, fm, ring;
    ScopeView scope;
};

class FilterPanel : public Panel
{
public:
    FilterPanel (GeminisInstrumentProcessor& p, juce::Colour c) : Panel ("Filtro", c), view (p.apvts, c)
    {
        auto& s = p.apvts;
        type.attach (s, "f_type");
        setupKnob (cut, s, "f_cut", "Corte", c);
        setupKnob (res, s, "f_res", "Resonancia", c);
        setupKnob (drive, s, "f_drive", U ("Saturación"), c);
        setupKnob (env, s, "f_env", "Envolvente", c);
        setupKnob (key, s, "f_key", "Teclado", c);
        setupKnob (vel, s, "f_vel", "Velocidad", c);
        for (auto* comp : std::initializer_list<juce::Component*> { &type, &view, &cut, &res, &drive, &env, &key, &vel })
            addAndMakeVisible (comp);
    }

    void refresh() override { view.refresh(); }

    void resized() override
    {
        type.setBounds (header().removeFromRight (130));
        auto b = body();
        layoutRow (b.removeFromBottom (70), { &cut, &res, &drive, &env, &key, &vel }, 2);
        b.removeFromBottom (4);
        view.setBounds (b);
    }

private:
    ChoiceBox type;
    FilterView view;
    ParamKnob cut, res, drive, env, key, vel;
};

class EnvPanel : public Panel
{
public:
    EnvPanel (GeminisInstrumentProcessor& p, juce::Colour c) : Panel ("Envolventes", c), state (p.apvts), view (p.apvts, c)
    {
        const char* names[] = { "Volumen", "Filtro", "Mod" };
        for (int i = 0; i < 3; ++i)
        {
            tabs[(size_t) i].setButtonText (names[i]);
            tabs[(size_t) i].setColour (juce::TextButton::buttonOnColourId, c);
            tabs[(size_t) i].setColour (juce::TextButton::textColourOnId, colours::bg);
            tabs[(size_t) i].onClick = [this, i] { select (i); };
            addAndMakeVisible (tabs[(size_t) i]);
        }
        tabs[2].setTooltip (U ("La envolvente Mod se usa en la matriz de modulación (por ejemplo, para curvar la onda al pulsar)"));
        for (auto* comp : std::initializer_list<juce::Component*> { &view, &a, &d, &s, &r })
            addAndMakeVisible (comp);
        select (0);
    }

    void select (int i)
    {
        current = i;
        const char* prefixes[] = { "ea_", "ef_", "em_" };
        const juce::String p (prefixes[i]);
        setupKnob (a, state, p + "a", "Ataque", accent);
        setupKnob (d, state, p + "d", U ("Caída"), accent);
        setupKnob (s, state, p + "s", "Sostenido", accent);
        setupKnob (r, state, p + "r", U ("Liberación"), accent);
        view.setPrefix (p);
        for (int k = 0; k < 3; ++k) tabs[(size_t) k].setToggleState (k == i, juce::dontSendNotification);
    }

    void refresh() override { view.refresh(); }

    void resized() override
    {
        auto head = header();
        for (int k = 2; k >= 0; --k)
        {
            tabs[(size_t) k].setBounds (head.removeFromRight (72));
            head.removeFromRight (4);
        }
        auto b = body();
        view.setBounds (b.removeFromLeft (200));
        b.removeFromLeft (8);
        layoutRow (b.withSizeKeepingCentre (b.getWidth(), juce::jmin (b.getHeight(), 92)), { &a, &d, &s, &r }, 4);
    }

private:
    APVTS& state;
    EnvView view;
    std::array<juce::TextButton, 3> tabs;
    ParamKnob a, d, s, r;
    int current = 0;
};

class LfoPanel : public Panel
{
public:
    LfoPanel (GeminisInstrumentProcessor& p, juce::Colour c)
        : Panel ("LFO", c),
          view1 (p.apvts.getRawParameterValue ("l1_shape"), c), view2 (p.apvts.getRawParameterValue ("l2_shape"), c)
    {
        auto& s = p.apvts;
        for (int l = 0; l < 2; ++l)
        {
            const juce::String pre = "l" + juce::String (l + 1) + "_";
            shape[(size_t) l].attach (s, pre + "shape");
            sync[(size_t) l].attach (s, pre + "sync");
            setupKnob (rate[(size_t) l], s, pre + "rate", "Velocidad", c);
            rate[(size_t) l].setCompact (true);
            addAndMakeVisible (shape[(size_t) l]);
            addAndMakeVisible (sync[(size_t) l]);
            addAndMakeVisible (rate[(size_t) l]);
        }
        sync[0].setTooltip (U ("Sincroniza el LFO 1 con el tempo (Libre = usa la velocidad)"));
        sync[1].setTooltip (U ("El LFO 2 vuelve a empezar con cada nota"));
        addAndMakeVisible (view1);
        addAndMakeVisible (view2);
    }

    void refresh() override { view1.refresh(); view2.refresh(); }

    void paintExtra (juce::Graphics& g) override
    {
        g.setColour (colours::textDim);
        g.setFont (Fonts::ui (11.0f, 700));
        for (int l = 0; l < 2; ++l)
            g.drawText (l == 0 ? "1" : "2", labels[(size_t) l], juce::Justification::centred);
    }

    void resized() override
    {
        auto b = body();
        const int h = b.getHeight() / 2;
        for (int l = 0; l < 2; ++l)
        {
            auto row = b.removeFromTop (h).reduced (0, 3);
            labels[(size_t) l] = row.removeFromLeft (16);
            auto combos = row.removeFromLeft (112);
            shape[(size_t) l].setBounds (combos.removeFromTop (combos.getHeight() / 2).reduced (0, 2));
            sync[(size_t) l].setBounds (combos.reduced (0, 2));
            row.removeFromLeft (4);
            rate[(size_t) l].setBounds (row.removeFromLeft (58));
            row.removeFromLeft (4);
            (l == 0 ? view1 : view2).setBounds (row.reduced (0, 4));
        }
    }

private:
    std::array<ChoiceBox, 2> shape, sync;
    std::array<ParamKnob, 2> rate;
    LfoShapeView view1, view2;
    std::array<juce::Rectangle<int>, 2> labels;
};

class ModPanel : public Panel
{
public:
    ModPanel (GeminisInstrumentProcessor& p, juce::Colour c) : Panel (U ("Matriz de modulación"), c)
    {
        auto& s = p.apvts;
        for (int m = 0; m < numModSlots; ++m)
        {
            const juce::String pre = "m" + juce::String (m + 1) + "_";
            auto& slot = slots[(size_t) m];
            slot.src.attach (s, pre + "src");
            slot.dst.attach (s, pre + "dst");
            setupKnob (slot.amt, s, pre + "amt", {}, c);
            slot.amt.setShowName (false);
            addAndMakeVisible (slot.src);
            addAndMakeVisible (slot.dst);
            addAndMakeVisible (slot.amt);
        }
    }

    void paintExtra (juce::Graphics& g) override
    {
        g.setFont (Fonts::ui (11.0f, 700));
        for (int m = 0; m < numModSlots; ++m)
        {
            const auto b = badges[(size_t) m].toFloat();
            g.setColour (accent.withAlpha (0.18f));
            g.fillEllipse (b.withSizeKeepingCentre (18.0f, 18.0f));
            g.setColour (accent);
            g.drawText (juce::String (m + 1), b, juce::Justification::centred);
            g.setColour (colours::textFaint);
            g.drawText (U ("›"), arrows[(size_t) m], juce::Justification::centred);
        }
        g.setColour (colours::textFaint);
        g.setFont (Fonts::ui (10.0f, 600));
        g.drawText (U ("fuente  ›  destino  ·  cantidad"), header().toFloat(), juce::Justification::centredRight);
    }

    void resized() override
    {
        auto b = body();
        const int colW = (b.getWidth() - 24) / 3;
        const int rowH = b.getHeight() / 2;
        for (int m = 0; m < numModSlots; ++m)
        {
            const int col = m % 3, row = m / 3;
            auto cell = juce::Rectangle<int> (b.getX() + col * (colW + 12), b.getY() + row * rowH, colW, rowH).reduced (0, 2);
            auto& slot = slots[(size_t) m];
            badges[(size_t) m] = cell.removeFromLeft (22);
            slot.amt.setBounds (cell.removeFromRight (44).withSizeKeepingCentre (44, juce::jmin (44, cell.getHeight())));
            const int comboW = (cell.getWidth() - 14) / 2;
            const auto mid = cell.withSizeKeepingCentre (cell.getWidth(), 26);
            auto rest = mid;
            slot.src.setBounds (rest.removeFromLeft (comboW));
            arrows[(size_t) m] = rest.removeFromLeft (14);
            slot.dst.setBounds (rest.removeFromLeft (comboW));
        }
    }

private:
    struct Slot { ChoiceBox src, dst; ParamKnob amt; };
    std::array<Slot, numModSlots> slots;
    std::array<juce::Rectangle<int>, numModSlots> badges, arrows;
};

class VoicePanel : public Panel
{
public:
    VoicePanel (GeminisInstrumentProcessor& p, juce::Colour c) : Panel ("Voces", c)
    {
        auto& s = p.apvts;
        mode.attach (s, "voice_mode");
        setupKnob (glide, s, "glide", "Portamento", c);
        setupKnob (bend, s, "bend", "Pitch bend", c);
        setupKnob (vel, s, "vel_sens", "Velocidad", c);
        setupKnob (width, s, "width", "Anchura", c);
        setupKnob (vol, s, "sy_vol", "Volumen", c);
        for (auto* comp : std::initializer_list<juce::Component*> { &mode, &glide, &bend, &vel, &width, &vol })
            addAndMakeVisible (comp);
    }

    void resized() override
    {
        mode.setBounds (header().removeFromRight (120));
        auto b = body();
        layoutRow (b.withSizeKeepingCentre (b.getWidth(), juce::jmin (b.getHeight(), 80)), { &glide, &bend, &vel, &width, &vol }, 2);
    }

private:
    ChoiceBox mode;
    ParamKnob glide, bend, vel, width, vol;
};

//==============================================================================
/** Página: algo que se refresca con el temporizador de la ventana. */
class Page : public juce::Component
{
public:
    virtual void refresh() {}
    virtual void presetChanged() {}
};

class SynthPage : public Page
{
public:
    explicit SynthPage (GeminisInstrumentProcessor& p)
        : osc1 (p, 0, colours::violet), osc2 (p, 1, juce::Colour (0xffc4a8ff)), mix (p, colours::violet), filter (p, colours::rose),
          env (p, colours::cyan), lfo (p, colours::green), mods (p, colours::gold), voice (p, colours::violet)
    {
        for (auto* panel : panels()) addAndMakeVisible (panel);
    }

    void refresh() override { for (auto* panel : panels()) panel->refresh(); }

    void resized() override
    {
        auto r = getLocalBounds();
        auto rowA = r.removeFromTop (240);
        r.removeFromTop (8);
        auto rowB = r.removeFromTop (170);
        r.removeFromTop (8);
        auto rowC = r;
        osc1.setBounds (rowA.removeFromLeft (404));
        rowA.removeFromLeft (10);
        osc2.setBounds (rowA.removeFromLeft (404));
        rowA.removeFromLeft (10);
        mix.setBounds (rowA);
        filter.setBounds (rowB.removeFromLeft (404));
        rowB.removeFromLeft (10);
        env.setBounds (rowB.removeFromLeft (500));
        rowB.removeFromLeft (10);
        lfo.setBounds (rowB);
        mods.setBounds (rowC.removeFromLeft (820));
        rowC.removeFromLeft (10);
        voice.setBounds (rowC);
    }

private:
    std::array<Panel*, 8> panels() { return { &osc1, &osc2, &mix, &filter, &env, &lfo, &mods, &voice }; }

    OscPanel osc1, osc2;
    MixPanel mix;
    FilterPanel filter;
    EnvPanel env;
    LfoPanel lfo;
    ModPanel mods;
    VoicePanel voice;
};

//==============================================================================
//  BATERÍA
//==============================================================================
class PadGrid : public Panel
{
public:
    PadGrid (GeminisInstrumentProcessor& p, juce::Colour c) : Panel ("Pads", c), proc (p)
    {
        for (int i = 0; i < numPads; ++i)
            sounds[(size_t) i] = p.apvts.getRawParameterValue (padParamId (i, "snd"));
        map = p.apvts.getRawParameterValue ("midi_map");
        setTooltip (U ("Pulsa un pad para oírlo y editarlo. Cuanto más arriba pulses, más fuerte."));
    }

    std::function<void (int)> onSelect;
    int selected = 0;

    void refresh() override
    {
        bool dirty = false;
        for (int i = 0; i < numPads; ++i)
        {
            const auto h = proc.drums.hitCount[(size_t) i].load();
            if (h != lastHits[(size_t) i]) { lastHits[(size_t) i] = h; flash[(size_t) i] = 1.0f; dirty = true; }
            else if (flash[(size_t) i] > 0.01f) { flash[(size_t) i] *= 0.82f; dirty = true; }
            const int snd = juce::roundToInt (sounds[(size_t) i]->load());
            if (snd != lastSounds[(size_t) i]) { lastSounds[(size_t) i] = snd; dirty = true; }
        }
        const int m = juce::roundToInt (map->load());
        if (m != lastMap) { lastMap = m; dirty = true; }
        const int root = proc.drums.midiRoot.load();
        if (root != lastRoot) { lastRoot = root; dirty = true; }
        const auto notes = proc.drums.midiNotesReceived.load();
        if (notes != lastMidiNotes) { lastMidiNotes = notes; midiFlash = 1.0f; dirty = true; }
        else if (midiFlash > 0.01f) { midiFlash *= 0.85f; dirty = true; }
        if (dirty) repaint();
    }

    void paintExtra (juce::Graphics& g) override
    {
        const auto mapText = lastMap == 1 ? U ("MAPA GENERAL MIDI")
                                          : U ("MAPA MASCHINE · PAD 1 = ") + juce::MidiMessage::getMidiNoteName (lastRoot, true, true, 3);
        const auto headerFont = Fonts::ui (10.0f, 600);
        g.setColour (colours::textFaint);
        g.setFont (headerFont);
        g.drawText (mapText, header().toFloat(), juce::Justification::centredRight);

        // Piloto MIDI: se enciende con cada nota que llega (si no se enciende, no llega MIDI)
        {
            const auto h = header().toFloat();
            const float textW = juce::GlyphArrangement::getStringWidth (headerFont, mapText);
            auto midiArea = h.withRight (h.getRight() - textW - 12.0f).removeFromRight (44.0f);
            const auto dot = midiArea.removeFromLeft (10.0f).withSizeKeepingCentre (6.0f, 6.0f);
            g.setColour (lastMidiNotes == 0 ? colours::textFaint.withAlpha (0.4f)
                                            : colours::textFaint.interpolatedWith (juce::Colours::limegreen, midiFlash));
            g.fillEllipse (dot);
            g.setColour (colours::textFaint);
            g.drawText ("MIDI", midiArea, juce::Justification::centredLeft);
        }

        const auto& names = drumSoundNames();
        for (int i = 0; i < numPads; ++i)
        {
            const auto r = padBounds (i).toFloat();
            const auto name = names[juce::jlimit (0, names.size() - 1, lastSounds[(size_t) i])];
            const auto col = soundColour (name);
            const float f = flash[(size_t) i];

            g.setGradientFill (juce::ColourGradient (col.withAlpha (0.16f + 0.5f * f), r.getX(), r.getY(),
                                                     col.withAlpha (0.06f + 0.3f * f), r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle (r, 9.0f);
            if (f > 0.05f)
            {
                g.setColour (col.withAlpha (0.35f * f));
                g.drawRoundedRectangle (r.expanded (2.0f), 11.0f, 3.0f);
            }
            g.setColour (i == selected ? col : col.withAlpha (0.35f));
            g.drawRoundedRectangle (r.reduced (0.5f), 9.0f, i == selected ? 2.0f : 1.0f);

            g.setColour (col);
            g.setFont (Fonts::ui (11.0f, 700));
            g.drawText (juce::String (i + 1), r.reduced (8.0f, 6.0f), juce::Justification::topLeft);
            g.setColour (colours::textFaint);
            g.setFont (Fonts::ui (10.0f, 600));
            g.drawText (juce::MidiMessage::getMidiNoteName (lastMap == 1 ? padToNote (i, 1) : lastRoot + i, true, true, 3),
                        r.reduced (8.0f, 6.0f), juce::Justification::topRight);
            g.setColour (colours::text);
            g.setFont (Fonts::ui (12.0f, 600));
            g.drawFittedText (name, r.reduced (6.0f, 8.0f).toNearestInt(), juce::Justification::centredBottom, 2, 0.85f);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < numPads; ++i)
        {
            const auto r = padBounds (i);
            if (r.contains (e.getPosition()))
            {
                const float vel = juce::jlimit (0.35f, 1.0f, 1.0f - (float) (e.y - r.getY()) / (float) r.getHeight() * 0.65f);
                proc.drums.triggerFromUi (i, vel);
                selected = i;
                if (onSelect) onSelect (i);
                repaint();
                return;
            }
        }
    }

private:
    /** El pad 1 abajo a la izquierda, como en Maschine. */
    juce::Rectangle<int> padBounds (int pad) const
    {
        const auto b = body();
        const int gap = 7;
        const int size = juce::jmin ((b.getWidth() - gap * 3) / 4, (b.getHeight() - gap * 3) / 4);
        const int x0 = b.getX() + (b.getWidth() - (size * 4 + gap * 3)) / 2;
        const int col = pad % 4, row = 3 - pad / 4;
        return { x0 + col * (size + gap), b.getY() + row * (size + gap), size, size };
    }

    GeminisInstrumentProcessor& proc;
    std::array<std::atomic<float>*, numPads> sounds {};
    std::atomic<float>* map = nullptr;
    std::array<juce::uint32, numPads> lastHits {};
    std::array<float, numPads> flash {};
    std::array<int, numPads> lastSounds {};
    int lastMap = -1, lastRoot = 36;
    juce::uint32 lastMidiNotes = 0;
    float midiFlash = 0.0f;
};

class StepGrid : public juce::Component, public juce::SettableTooltipClient
{
public:
    StepGrid (GeminisInstrumentProcessor& p, juce::Colour c) : proc (p), accent (c)
    {
        for (int i = 0; i < numPads; ++i)
            sounds[(size_t) i] = p.apvts.getRawParameterValue (padParamId (i, "snd"));
        pattern = p.apvts.getRawParameterValue ("seq_pattern");
        length = p.apvts.getRawParameterValue ("seq_len");
        setTooltip (U ("Clic: poner o quitar golpe · Mayúsculas o botón derecho: cambiar la fuerza (suave, normal, acento)"));
    }

    std::function<void (int)> onSelect;
    int selected = 0;

    void refresh()
    {
        const int pat = juce::roundToInt (pattern->load());
        const int len = juce::roundToInt (length->load()) == 1 ? 32 : 16;
        const auto pt = proc.drums.getPattern (pat);
        const int step = proc.drums.currentStep.load();
        bool dirty = pat != lastPattern || len != steps || step != lastStep || ! (pt.steps == current.steps);
        for (int i = 0; i < numPads; ++i)
        {
            const int snd = juce::roundToInt (sounds[(size_t) i]->load());
            if (snd != lastSounds[(size_t) i]) { lastSounds[(size_t) i] = snd; dirty = true; }
        }
        lastPattern = pat; steps = len; lastStep = step; current = pt;
        if (dirty) repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto& names = drumSoundNames();
        const auto grid = gridArea();
        const float cw = (float) grid.getWidth() / (float) steps, rh = (float) grid.getHeight() / (float) numPads;

        for (int s = 0; s < steps; s += 4)
        {
            g.setColour (juce::Colour (0xff12132a).withAlpha ((s / 4) % 2 == 0 ? 0.9f : 0.45f));
            g.fillRect (juce::Rectangle<float> ((float) grid.getX() + cw * (float) s, (float) grid.getY(), cw * 4.0f, (float) grid.getHeight()));
        }
        if (lastStep >= 0 && lastStep < steps)
        {
            g.setColour (accent.withAlpha (0.14f));
            g.fillRect (juce::Rectangle<float> ((float) grid.getX() + cw * (float) lastStep, (float) grid.getY(), cw, (float) grid.getHeight()));
        }

        for (int row = 0; row < numPads; ++row)
        {
            const int pad = numPads - 1 - row;    // el pad 16 arriba, el 1 abajo
            const auto name = names[juce::jlimit (0, names.size() - 1, lastSounds[(size_t) pad])];
            const auto col = soundColour (name);
            const float y = (float) grid.getY() + rh * (float) row;
            auto label = juce::Rectangle<float> (0.0f, y, (float) grid.getX() - 6.0f, rh);
            if (pad == selected)
            {
                g.setColour (col.withAlpha (0.14f));
                g.fillRoundedRectangle (label.reduced (0.0f, 1.0f), 4.0f);
            }
            g.setColour (col);
            g.fillRoundedRectangle (label.getX() + 3.0f, label.getCentreY() - 3.0f, 6.0f, 6.0f, 2.0f);
            g.setColour (pad == selected ? colours::text : colours::textDim);
            g.setFont (Fonts::ui (juce::jmin (12.0f, rh * 0.62f), 600));
            g.drawText (juce::String (pad + 1) + "  " + name, label.withTrimmedLeft (14.0f), juce::Justification::centredLeft, true);

            for (int s = 0; s < steps; ++s)
            {
                const int v = current.steps[(size_t) pad][(size_t) s];
                auto cell = juce::Rectangle<float> ((float) grid.getX() + cw * (float) s, y, cw, rh).reduced (1.5f, 1.5f);
                if (v == 0)
                {
                    g.setColour (juce::Colour (0xff23253f));
                    g.fillRoundedRectangle (cell, 3.0f);
                }
                else
                {
                    const float alpha = v == 1 ? 0.45f : v == 2 ? 0.78f : 1.0f;
                    g.setColour (col.withAlpha (alpha * (s == lastStep ? 1.0f : 0.9f)));
                    g.fillRoundedRectangle (cell, 3.0f);
                    if (v == 3)
                    {
                        g.setColour (colours::text.withAlpha (0.85f));
                        g.drawRoundedRectangle (cell.reduced (1.0f), 2.5f, 1.2f);
                    }
                }
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        int pad, step;
        if (! cellAt (e.getPosition(), pad, step))
        {
            if (labelPadAt (e.getPosition(), pad))
            {
                proc.drums.triggerFromUi (pad, 0.85f);
                selected = pad;
                if (onSelect) onSelect (pad);
                repaint();
            }
            paintValue = -1;
            return;
        }
        const int v = current.steps[(size_t) pad][(size_t) step];
        if (e.mods.isShiftDown() || e.mods.isPopupMenu())
            paintValue = v == 0 ? 3 : v == 3 ? 1 : v + 1;     // acento -> suave -> normal -> acento
        else
            paintValue = v == 0 ? 2 : 0;
        set (pad, step, paintValue);
        lastCellPad = pad;
        lastCellStep = step;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        int pad, step;
        if (paintValue < 0 || ! cellAt (e.getPosition(), pad, step)) return;
        if (pad == lastCellPad && step == lastCellStep) return;
        set (pad, step, paintValue);
        lastCellPad = pad;
        lastCellStep = step;
    }

private:
    juce::Rectangle<int> gridArea() const { return getLocalBounds().withTrimmedLeft (132); }

    bool cellAt (juce::Point<int> p, int& pad, int& step) const
    {
        const auto grid = gridArea();
        if (! grid.contains (p)) return false;
        const int row = juce::jlimit (0, numPads - 1, (p.y - grid.getY()) * numPads / juce::jmax (1, grid.getHeight()));
        step = juce::jlimit (0, steps - 1, (p.x - grid.getX()) * steps / juce::jmax (1, grid.getWidth()));
        pad = numPads - 1 - row;
        return true;
    }

    bool labelPadAt (juce::Point<int> p, int& pad) const
    {
        const auto grid = gridArea();
        if (p.x >= grid.getX() || p.y < grid.getY() || p.y >= grid.getBottom()) return false;
        pad = numPads - 1 - juce::jlimit (0, numPads - 1, (p.y - grid.getY()) * numPads / juce::jmax (1, grid.getHeight()));
        return true;
    }

    void set (int pad, int step, int v)
    {
        proc.drums.setStep (lastPattern, pad, step, v);
        current.steps[(size_t) pad][(size_t) step] = (juce::uint8) v;
        if (v > 0 && proc.drums.currentStep.load() < 0) proc.drums.triggerFromUi (pad, v == 1 ? 0.45f : v == 2 ? 0.78f : 1.0f);
        repaint();
    }

    GeminisInstrumentProcessor& proc;
    juce::Colour accent;
    std::array<std::atomic<float>*, numPads> sounds {};
    std::atomic<float> *pattern, *length;
    std::array<int, numPads> lastSounds {};
    Pattern current;
    int lastPattern = 0, steps = 16, lastStep = -1;
    int paintValue = -1, lastCellPad = -1, lastCellStep = -1;
};

class SequencerPanel : public Panel
{
public:
    SequencerPanel (GeminisInstrumentProcessor& p, juce::Colour c) : Panel ("Secuenciador", c), proc (p), grid (p, c)
    {
        play.setColour (juce::TextButton::buttonOnColourId, c);
        play.setColour (juce::TextButton::textColourOnId, colours::bg);
        play.setTooltip (U ("Toca el patrón con el tempo propio. Con el DAW en marcha, el patrón sigue al DAW."));
        play.onClick = [this]
        {
            const bool running = juce::roundToInt (proc.getParam ("seq_mode")) == 2;
            proc.setParam ("seq_mode", running ? 1.0f : 2.0f);
        };
        addAndMakeVisible (play);
        const char* letters[] = { "A", "B", "C", "D" };
        for (int i = 0; i < numPatterns; ++i)
        {
            auto& b = patterns[(size_t) i];
            b.setButtonText (letters[i]);
            b.setColour (juce::TextButton::buttonOnColourId, c);
            b.setColour (juce::TextButton::textColourOnId, colours::bg);
            b.setTooltip (U ("Patrón ") + letters[i] + U (". Con el DAW, cambia de patrón en directo."));
            b.onClick = [this, i] { proc.setParam ("seq_pattern", (float) i); };
            addAndMakeVisible (b);
        }
        length.attach (p.apvts, "seq_len");
        length.setTooltip (U ("Largo del patrón en semicorcheas"));
        clear.setTooltip (U ("Borra todos los golpes de este patrón"));
        clear.onClick = [this]
        {
            proc.drums.setPattern (juce::roundToInt (proc.getParam ("seq_pattern")), Pattern());
        };
        addAndMakeVisible (length);
        addAndMakeVisible (clear);
        addAndMakeVisible (grid);
    }

    StepGrid grid;

    void refresh() override
    {
        grid.refresh();
        const int mode = juce::roundToInt (proc.getParam ("seq_mode"));
        const bool running = mode == 2;
        play.setToggleState (running, juce::dontSendNotification);
        play.setButtonText (running ? U ("■  Parar") : (mode == 1 && proc.drums.currentStep.load() >= 0 ? U ("Sigue al DAW") : U ("▶  Tocar")));
        const int pat = juce::roundToInt (proc.getParam ("seq_pattern"));
        for (int i = 0; i < numPatterns; ++i) patterns[(size_t) i].setToggleState (i == pat, juce::dontSendNotification);
    }

    void resized() override
    {
        auto head = header();
        head.removeFromLeft (110);
        play.setBounds (head.removeFromLeft (112));
        head.removeFromLeft (12);
        for (auto& b : patterns) { b.setBounds (head.removeFromLeft (30)); head.removeFromLeft (4); }
        clear.setBounds (head.removeFromRight (70));
        head.removeFromRight (6);
        length.setBounds (head.removeFromRight (64));
        grid.setBounds (body());
    }

private:
    GeminisInstrumentProcessor& proc;
    juce::TextButton play, clear { "Borrar" };
    std::array<juce::TextButton, numPatterns> patterns;
    ChoiceBox length;
};

class PadEditor : public Panel
{
public:
    PadEditor (GeminisInstrumentProcessor& p, juce::Colour c) : Panel ("Pad", c), proc (p)
    {
        for (auto* comp : std::initializer_list<juce::Component*> { &sound, &choke, &tune, &decay, &tone, &snap, &drive, &level, &pan })
            addAndMakeVisible (comp);
        choke.setTooltip (U ("Pads del mismo grupo se cortan entre sí (por ejemplo, charles cerrado y abierto)"));
        setPad (0);
    }

    void setPad (int pad)
    {
        current = pad;
        auto& s = proc.apvts;
        sound.clear (juce::dontSendNotification);
        const auto& names = drumSoundNames();
        sound.addSectionHeading (U ("Sintetizados"));
        for (int i = 0; i < numDrumModels; ++i) sound.addItem (names[i], i + 1);
        sound.addSectionHeading (U ("Acústicos (muestras libres CC0)"));
        for (int i = numDrumModels; i < firstLateDrumSound(); ++i) sound.addItem (names[i], i + 1);
        sound.addSectionHeading (U ("Melodía, acorde y voz (fila 4)"));
        for (int i = firstLateDrumSound(); i < names.size(); ++i) sound.addItem (names[i], i + 1);
        sound.attach (s, padParamId (pad, "snd"), true);
        choke.attach (s, padParamId (pad, "choke"));
        setupKnob (tune, s, padParamId (pad, "tune"), U ("Afinación"), accent);
        setupKnob (decay, s, padParamId (pad, "decay"), U ("Caída"), accent);
        setupKnob (tone, s, padParamId (pad, "tone"), "Tono", accent);
        setupKnob (snap, s, padParamId (pad, "snap"), "Golpe", accent);
        setupKnob (drive, s, padParamId (pad, "drive"), U ("Saturación"), accent);
        setupKnob (level, s, padParamId (pad, "level"), "Nivel", accent);
        setupKnob (pan, s, padParamId (pad, "pan"), "Panorama", accent);
        title = "Pad " + juce::String (pad + 1);
        repaint();
    }

    void paintExtra (juce::Graphics& g) override
    {
        g.setColour (colours::textFaint);
        g.setFont (Fonts::ui (10.5f, 600));
        g.drawText (U ("Grupo de corte"), chokeLabel, juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto head = header();
        head.removeFromLeft (70);
        sound.setBounds (head.removeFromLeft (230));
        auto b = body();
        auto top = b.removeFromTop (b.getHeight() / 2);
        layoutRow (top.reduced (0, 2), { &tune, &decay, &tone, &snap }, 6);
        auto bottom = b.reduced (0, 2);
        auto chokeArea = bottom.removeFromRight (bottom.getWidth() / 4);
        layoutRow (bottom, { &drive, &level, &pan }, 6);
        chokeArea = chokeArea.reduced (8, 0);
        chokeLabel = chokeArea.withHeight (20).withY (chokeArea.getCentreY() - 24);
        choke.setBounds (chokeArea.withHeight (26).withY (chokeArea.getCentreY() - 2));
    }

private:
    GeminisInstrumentProcessor& proc;
    int current = 0;
    ChoiceBox sound, choke;
    ParamKnob tune, decay, tone, snap, drive, level, pan;
    juce::Rectangle<int> chokeLabel;
};

/** Arrastra el patrón como archivo MIDI al DAW (o clic para guardarlo). */
class MidiDragTile : public juce::Component, public juce::SettableTooltipClient
{
public:
    MidiDragTile (GeminisInstrumentProcessor& p, juce::Colour c) : proc (p), accent (c)
    {
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        setTooltip (U ("Arrastra a una pista del DAW o de Maschine. Clic: guardar en Documentos/Geminis/MIDI"));
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (accent.withAlpha (isMouseOver() ? 0.16f : 0.08f));
        g.fillRoundedRectangle (r, 9.0f);
        juce::Path border;
        border.addRoundedRectangle (r, 9.0f);
        juce::Path dashed;
        const float dashes[] = { 5.0f, 4.0f };
        juce::PathStrokeType (1.2f).createDashedStroke (dashed, border, dashes, 2);
        g.setColour (accent.withAlpha (0.7f));
        g.fillPath (dashed);
        g.setColour (colours::text);
        g.setFont (Fonts::ui (12.5f, 700));
        g.drawText (U ("Arrastrar MIDI"), r.withTrimmedBottom (r.getHeight() * 0.45f), juce::Justification::centredBottom);
        g.setColour (colours::textDim);
        g.setFont (Fonts::ui (10.5f, 400));
        g.drawText (status, r.withTrimmedTop (r.getHeight() * 0.58f), juce::Justification::centredTop);
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging || e.getDistanceFromDragStart() < 6) return;
        dragging = true;
        const auto f = exportFile();
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (! dragging)
        {
            const auto f = exportFile();
            status = U ("Guardado: ") + f.getFileName();
            repaint();
        }
        dragging = false;
    }

private:
    juce::File exportFile()
    {
        const auto name = proc.getKitName().isNotEmpty() ? proc.getKitName() : U ("Patrón Geminis");
        return proc.drums.exportMidi (juce::roundToInt (proc.getParam ("seq_pattern")), name);
    }

    GeminisInstrumentProcessor& proc;
    juce::Colour accent;
    bool dragging = false;
    juce::String status { U ("al DAW o a Maschine") };
};

class KitPanel : public Panel
{
public:
    KitPanel (GeminisInstrumentProcessor& p, juce::Colour c) : Panel ("Kit y tempo", c), proc (p), drag (p, c)
    {
        auto& s = p.apvts;
        for (const auto& k : drumKits()) kits.addItem (k.name, kits.getNumItems() + 1);
        kits.onChange = [this] { const int i = kits.getSelectedItemIndex(); if (i >= 0 && i != proc.getKit()) proc.applyKit (i); };
        seqMode.attach (s, "seq_mode");
        map.attach (s, "midi_map");
        map.setTooltip (U ("Maschine: pad 1 = C1 (nota 36) en adelante. General MIDI: bombo 36, caja 38, charles 42..."));
        setupKnob (bpm, s, "seq_bpm", "Tempo propio", c);
        setupKnob (swing, s, "seq_swing", "Swing", c);
        setupKnob (vol, s, "dr_vol", "Volumen", c);
        exportButton.setTooltip (U ("Guarda los 16 pads como WAV numerados. En Maschine, arrástralos juntos al pad 1: "
                                    "cada sonido cae en su pad. Después guarda el grupo como Kit."));
        exportButton.onClick = [this] { showExportMenu(); };
        for (auto* comp : std::initializer_list<juce::Component*> { &kits, &seqMode, &map, &bpm, &swing, &vol, &drag, &exportButton })
            addAndMakeVisible (comp);
        presetChanged();
    }

    ~KitPanel() override
    {
        if (exportThread.joinable()) exportThread.join();
    }

    void showExportMenu()
    {
        if (exporting) return;
        juce::PopupMenu m;
        m.addItem (1, U ("Este kit (con tus cambios)"));
        m.addItem (2, U ("Los 16 kits de fábrica"));
        m.addSeparator();
        m.addItem (3, U ("Abrir la carpeta de kits"));
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&exportButton), [this] (int r)
        {
            if (r == 3) { auto d = GeminisInstrumentProcessor::maschineKitFolder(); d.createDirectory(); d.startAsProcess(); return; }
            if (r == 1 || r == 2) startExport (r == 2);
        });
    }

    void startExport (bool allKits)
    {
        juce::MemoryBlock state;
        proc.getStateInformation (state);
        exporting = true;
        exportButton.setEnabled (false);
        exportButton.setButtonText (U ("Exportando..."));
        if (exportThread.joinable()) exportThread.join();
        exportThread = std::thread ([safe = juce::Component::SafePointer<KitPanel> (this), state, allKits]
        {
            const auto parent = GeminisInstrumentProcessor::maschineKitFolder();
            parent.createDirectory();
            juce::File last;
            int done = 0;
            const int total = allKits ? (int) drumKits().size() : 1;
            for (int i = 0; i < total; ++i)
            {
                last = GeminisInstrumentProcessor::exportKitForMaschine (state, allKits ? i : -1, parent);
                if (last != juce::File()) ++done;
            }
            juce::MessageManager::callAsync ([safe, last, done, total, allKits, parent]
            {
                if (auto* self = safe.getComponent())
                {
                    self->exporting = false;
                    self->exportButton.setEnabled (true);
                    self->exportButton.setButtonText (U ("Exportar a Maschine"));
                }
                if (done < total)
                {
                    juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, U ("Exportar a Maschine"),
                        U ("No se pudieron guardar todos los kits en ") + parent.getFullPathName());
                    return;
                }
                (allKits ? parent : last).startAsProcess();
                juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, U ("Kit listo para Maschine"),
                    U ("Guardado en ") + (allKits ? parent : last).getFullPathName() + "\n\n"
                    + U ("En Maschine: elige un grupo vacío, selecciona los 16 archivos de un kit (01 a 16) y arrástralos "
                         "juntos al pad 1. Cada sonido cae en su pad. Para tenerlo en el navegador de Kits, "
                         "guárdalo con File > Save Group As."));
            });
        });
    }

    void presetChanged()
    {
        kits.setSelectedItemIndex (proc.getKit(), juce::dontSendNotification);
        repaint();
    }

    void paintExtra (juce::Graphics& g) override
    {
        const auto& list = drumKits();
        const int k = proc.getKit();
        if (! juce::isPositiveAndBelow (k, (int) list.size())) return;
        g.setColour (colours::text);
        g.setFont (Fonts::brand (20.0f));
        g.drawText (upper (list[(size_t) k].name), info.removeFromTop (28).toFloat(), juce::Justification::centredLeft);
        g.setColour (colours::textDim);
        g.setFont (Fonts::ui (12.0f));
        g.drawFittedText (list[(size_t) k].description, info, juce::Justification::topLeft, 2, 1.0f);
        info = infoArea;
        g.setColour (colours::textFaint);
        g.setFont (Fonts::ui (10.5f, 600));
        g.drawText ("Secuenciador", modeLabel, juce::Justification::centredLeft);
        g.drawText ("Mapa MIDI", mapLabel, juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto head = header();
        kits.setBounds (head.removeFromRight (190));
        head.removeFromRight (8);
        exportButton.setBounds (head.removeFromRight (150));
        auto b = body();
        infoArea = info = b.removeFromTop (58);
        b.removeFromTop (4);
        drag.setBounds (b.removeFromRight (132).reduced (0, 6));
        b.removeFromRight (10);
        auto combos = b.removeFromRight (150);
        modeLabel = combos.removeFromTop (18);
        seqMode.setBounds (combos.removeFromTop (26));
        combos.removeFromTop (6);
        mapLabel = combos.removeFromTop (18);
        map.setBounds (combos.removeFromTop (26));
        b.removeFromRight (6);
        layoutRow (b.withSizeKeepingCentre (b.getWidth(), juce::jmin (b.getHeight(), 92)), { &bpm, &swing, &vol }, 4);
    }

private:
    GeminisInstrumentProcessor& proc;
    juce::ComboBox kits;
    ChoiceBox seqMode, map;
    ParamKnob bpm, swing, vol;
    MidiDragTile drag;
    juce::TextButton exportButton { juce::String::fromUTF8 ("Exportar a Maschine") };
    std::thread exportThread;
    bool exporting = false;
    juce::Rectangle<int> info, infoArea, modeLabel, mapLabel;
};

class DrumPage : public Page
{
public:
    explicit DrumPage (GeminisInstrumentProcessor& p)
        : pads (p, colours::rose), seq (p, colours::rose), editor (p, colours::rose), kit (p, colours::rose)
    {
        pads.onSelect = [this] (int pad) { select (pad); };
        seq.grid.onSelect = [this] (int pad) { select (pad); };
        for (auto* comp : std::initializer_list<juce::Component*> { &pads, &seq, &editor, &kit })
            addAndMakeVisible (comp);
    }

    void select (int pad)
    {
        pads.selected = pad;
        seq.grid.selected = pad;
        editor.setPad (pad);
        pads.repaint();
        seq.grid.repaint();
    }

    void refresh() override { pads.refresh(); seq.refresh(); }
    void presetChanged() override { kit.presetChanged(); editor.setPad (pads.selected); }

    void resized() override
    {
        auto r = getLocalBounds();
        auto top = r.removeFromTop (384);
        r.removeFromTop (8);
        pads.setBounds (top.removeFromLeft (384));
        top.removeFromLeft (10);
        seq.setBounds (top);
        editor.setBounds (r.removeFromLeft (560));
        r.removeFromLeft (10);
        kit.setBounds (r);
    }

private:
    PadGrid pads;
    SequencerPanel seq;
    PadEditor editor;
    KitPanel kit;
};

//==============================================================================
//  INSTRUMENTOS
//==============================================================================
class InstrumentGrid : public Panel
{
public:
    InstrumentGrid (GeminisInstrumentProcessor& p, juce::Colour c) : Panel ("Instrumentos", c), proc (p)
    {
        type = p.apvts.getRawParameterValue ("ins_type");
    }

    void refresh() override
    {
        const int t = juce::roundToInt (type->load());
        const int l = proc.sampler.getLoadedInstrument();
        if (t != lastType || l != lastLoaded) { lastType = t; lastLoaded = l; repaint(); }
    }

    void paintExtra (juce::Graphics& g) override
    {
        g.setColour (colours::textFaint);
        g.setFont (Fonts::ui (10.0f, 600));
        g.drawText (U ("MUESTRAS LIBRES CC0 Y CUERDAS MODELADAS"), header().toFloat(), juce::Justification::centredRight);

        const auto& list = instrumentList();
        for (int i = 0; i < (int) list.size(); ++i)
        {
            const auto r = cardBounds (i).toFloat();
            const auto fam = instrumentFamily (i);
            const bool sel = i == lastType;
            const bool hov = i == hovered;
            g.setGradientFill (juce::ColourGradient (fam.colour.withAlpha (sel ? 0.28f : hov ? 0.14f : 0.07f), r.getX(), r.getY(),
                                                     fam.colour.withAlpha (sel ? 0.1f : 0.03f), r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle (r, 9.0f);
            g.setColour (sel ? fam.colour : fam.colour.withAlpha (0.3f));
            g.drawRoundedRectangle (r.reduced (0.5f), 9.0f, sel ? 2.0f : 1.0f);

            auto inner = r.reduced (12.0f, 8.0f);
            g.setColour (fam.colour);
            draw::spacedText (g, upper (fam.name), Fonts::ui (9.5f, 700), inner.removeFromTop (16.0f),
                              juce::Justification::centredLeft, 0.18f);
            g.setColour (colours::text);
            g.setFont (Fonts::ui (14.0f, 700));
            g.drawFittedText (list[(size_t) i].name, inner.removeFromTop (38.0f).toNearestInt(), juce::Justification::centredLeft, 2, 0.9f);
            g.setColour (colours::textFaint);
            g.setFont (Fonts::ui (10.5f, 600));
            const bool loading = sel && lastLoaded != i;
            g.drawText (loading ? U ("cargando...") : list[(size_t) i].modeled ? U ("Modelado") : U ("Muestras"),
                        inner, juce::Justification::bottomLeft);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int h = cardAt (e.getPosition());
        if (h != hovered) { hovered = h; repaint(); }
    }
    void mouseExit (const juce::MouseEvent&) override { hovered = -1; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        const int i = cardAt (e.getPosition());
        if (i >= 0) proc.applyInstrument (i);
    }

private:
    juce::Rectangle<int> cardBounds (int i) const
    {
        const auto b = body();
        constexpr int cols = 4, rows = 5, gap = 8;
        const int w = (b.getWidth() - gap * (cols - 1)) / cols, h = (b.getHeight() - gap * (rows - 1)) / rows;
        return { b.getX() + (i % cols) * (w + gap), b.getY() + (i / cols) * (h + gap), w, h };
    }

    int cardAt (juce::Point<int> p) const
    {
        for (int i = 0; i < (int) instrumentList().size(); ++i)
            if (cardBounds (i).contains (p)) return i;
        return -1;
    }

    GeminisInstrumentProcessor& proc;
    std::atomic<float>* type = nullptr;
    int lastType = -1, lastLoaded = -1, hovered = -1;
};

class InstrumentControls : public Panel
{
public:
    InstrumentControls (GeminisInstrumentProcessor& p, juce::Colour c) : Panel ("Sonido", c), proc (p)
    {
        auto& s = p.apvts;
        setupKnob (attack, s, "ins_attack", "Ataque", c);
        setupKnob (release, s, "ins_release", U ("Liberación"), c);
        setupKnob (tone, s, "ins_tone", "Tono", c);
        setupKnob (character, s, "ins_char", U ("Carácter"), c);
        setupKnob (tune, s, "ins_tune", U ("Transponer"), c);
        setupKnob (vel, s, "ins_vel", "Velocidad", c);
        setupKnob (vol, s, "ins_vol", "Volumen", c);
        character.slider.setTooltip (U ("En las guitarras y bajos: posición de la púa y brillo de la cuerda. En las muestras: cuerpo y calidez."));
        for (auto* comp : std::initializer_list<juce::Component*> { &attack, &release, &tone, &character, &tune, &vel, &vol })
            addAndMakeVisible (comp);
        type = s.getRawParameterValue ("ins_type");
    }

    void refresh() override
    {
        const int t = juce::roundToInt (type->load());
        if (t != last) { last = t; repaint(); }
    }

    void paintExtra (juce::Graphics& g) override
    {
        const auto& list = instrumentList();
        if (! juce::isPositiveAndBelow (last, (int) list.size())) return;
        auto r = info;
        const auto fam = instrumentFamily (last);
        g.setColour (fam.colour);
        draw::spacedText (g, upper (fam.name), Fonts::ui (10.0f, 700), r.removeFromTop (18).toFloat(),
                          juce::Justification::centredLeft, 0.2f);
        g.setColour (colours::text);
        g.setFont (Fonts::brand (22.0f));
        g.drawFittedText (upper (list[(size_t) last].name), r.removeFromTop (32), juce::Justification::centredLeft, 1, 0.8f);
        g.setColour (colours::textDim);
        g.setFont (Fonts::ui (12.5f));
        g.drawFittedText (list[(size_t) last].description, r.removeFromTop (40), juce::Justification::topLeft, 2, 1.0f);
        g.setColour (colours::textFaint);
        g.setFont (Fonts::ui (11.0f));
        g.drawFittedText (U ("Toca con tu teclado MIDI, con el de la pantalla o con las teclas A S D F G H J K del ordenador (Z y X cambian de octava)."),
                          hint, juce::Justification::topLeft, 4, 1.0f);
    }

    void resized() override
    {
        auto b = body();
        info = b.removeFromTop (96);
        b.removeFromTop (6);
        auto row1 = b.removeFromTop (92);
        layoutRow (row1, { &attack, &release, &tone, &character }, 4);
        b.removeFromTop (6);
        auto row2 = b.removeFromTop (92);
        layoutRow (row2.withTrimmedRight (row2.getWidth() / 4), { &tune, &vel, &vol }, 4);
        b.removeFromTop (8);
        hint = b;
    }

private:
    GeminisInstrumentProcessor& proc;
    ParamKnob attack, release, tone, character, tune, vel, vol;
    std::atomic<float>* type = nullptr;
    int last = -1;
    juce::Rectangle<int> info, hint;
};

class InstrumentPage : public Page
{
public:
    explicit InstrumentPage (GeminisInstrumentProcessor& p) : grid (p, colours::cyan), controls (p, colours::cyan)
    {
        addAndMakeVisible (grid);
        addAndMakeVisible (controls);
    }

    void refresh() override { grid.refresh(); controls.refresh(); }

    void resized() override
    {
        auto r = getLocalBounds();
        grid.setBounds (r.removeFromLeft (800));
        r.removeFromLeft (10);
        controls.setBounds (r);
    }

private:
    InstrumentGrid grid;
    InstrumentControls controls;
};

//==============================================================================
/** Tira de efectos común a los tres modos. */
class FxStrip : public juce::Component
{
public:
    explicit FxStrip (GeminisInstrumentProcessor& p) : proc (p)
    {
        auto& s = p.apvts;
        satOn.setButtonText (U ("Saturación"));
        cmpOn.setButtonText ("Compresor");
        modOn.setButtonText (U ("Modulación"));
        dlyOn.setButtonText ("Delay");
        revOn.setButtonText ("Reverb");
        satType.attach (s, "fx_sat_type");
        modType.attach (s, "fx_mod_type");
        dlyType.attach (s, "fx_dly_type");
        dlyDiv.attach (s, "fx_dly_div");
        revType.attach (s, "fx_rev_type");
        knobs = { &satAmt, &satMix, &cmpAmt, &modRate, &modDepth, &modMix, &dlyFb, &dlyMix, &revSize, &revMix };
        for (auto* c : std::initializer_list<juce::Component*> { &satOn, &cmpOn, &modOn, &dlyOn, &revOn, &satType, &modType, &dlyType, &dlyDiv, &revType })
            addAndMakeVisible (c);
        for (auto* k : knobs) addAndMakeVisible (k);
        setAccent (colours::violet);
    }

    void setAccent (juce::Colour c)
    {
        accent = c;
        auto& s = proc.apvts;
        satOn.attach (s, "fx_sat_on", c);
        cmpOn.attach (s, "fx_cmp_on", c);
        modOn.attach (s, "fx_mod_on", c);
        dlyOn.attach (s, "fx_dly_on", c);
        revOn.attach (s, "fx_rev_on", c);
        setupKnob (satAmt, s, "fx_sat_amt", "Cantidad", c);
        setupKnob (satMix, s, "fx_sat_mix", "Mezcla", c);
        setupKnob (cmpAmt, s, "fx_cmp_amt", "Cantidad", c);
        setupKnob (modRate, s, "fx_mod_rate", "Velocidad", c);
        setupKnob (modDepth, s, "fx_mod_depth", "Profundidad", c);
        setupKnob (modMix, s, "fx_mod_mix", "Mezcla", c);
        setupKnob (dlyFb, s, "fx_dly_fb", U ("Repeticiones"), c);
        setupKnob (dlyMix, s, "fx_dly_mix", "Mezcla", c);
        setupKnob (revSize, s, "fx_rev_size", U ("Tamaño"), c);
        setupKnob (revMix, s, "fx_rev_mix", "Mezcla", c);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        for (const auto& r : sections)
            draw::glassPanel (g, r.toFloat(), accent, 0.18f, 10.0f);
        g.setColour (colours::textFaint);
        g.setFont (Fonts::ui (10.5f, 600));
        g.drawText ("Tiempo", divLabel, juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        const float units[] = { 2.55f, 1.6f, 3.0f, 3.3f, 2.8f };
        const float total = 13.25f;
        const int gap = 10;
        const int avail = r.getWidth() - gap * 4;
        for (int i = 0; i < 5; ++i)
        {
            const int w = i == 4 ? r.getWidth() : juce::roundToInt ((float) avail * units[i] / total);
            sections[(size_t) i] = r.removeFromLeft (w);
            r.removeFromLeft (gap);
        }
        auto head = [] (juce::Rectangle<int> s) { return s.removeFromTop (30).reduced (10, 5); };
        auto bodyOf = [] (juce::Rectangle<int> s) { return s.withTrimmedTop (30).reduced (8, 2); };

        { auto h = head (sections[0]); satType.setBounds (h.removeFromRight (90)); satOn.setBounds (h); layoutRow (bodyOf (sections[0]), { &satAmt, &satMix }); }
        { auto h = head (sections[1]); cmpOn.setBounds (h); layoutRow (bodyOf (sections[1]), { &cmpAmt }); }
        { auto h = head (sections[2]); modType.setBounds (h.removeFromRight (96)); modOn.setBounds (h); layoutRow (bodyOf (sections[2]), { &modRate, &modDepth, &modMix }); }
        {
            auto h = head (sections[3]); dlyType.setBounds (h.removeFromRight (104)); dlyOn.setBounds (h);
            auto b = bodyOf (sections[3]);
            auto div = b.removeFromLeft (122).reduced (4, 0);
            divLabel = div.withHeight (18).withY (div.getCentreY() - 22);
            dlyDiv.setBounds (div.withHeight (26).withY (div.getCentreY() - 3));
            layoutRow (b, { &dlyFb, &dlyMix });
        }
        { auto h = head (sections[4]); revType.setBounds (h.removeFromRight (110)); revOn.setBounds (h); layoutRow (bodyOf (sections[4]), { &revSize, &revMix }); }
    }

private:
    GeminisInstrumentProcessor& proc;
    juce::Colour accent;
    PowerToggle satOn, cmpOn, modOn, dlyOn, revOn;
    ChoiceBox satType, modType, dlyType, dlyDiv, revType;
    ParamKnob satAmt, satMix, cmpAmt, modRate, modDepth, modMix, dlyFb, dlyMix, revSize, revMix;
    std::vector<ParamKnob*> knobs;
    std::array<juce::Rectangle<int>, 5> sections;
    juce::Rectangle<int> divLabel;
};

//==============================================================================
/** Todo el contenido de la ventana, dibujado a tamaño de diseño. */
class InstContent : public juce::Component, private juce::Timer
{
public:
    explicit InstContent (GeminisInstrumentProcessor& p)
        : proc (p),
          splash (U ("¿Qué vas a crear hoy?"),
                  { Splash::Card { "Sintetizador", U ("Ondas que se curvan y se dibujan."), U ("Modular, con 66 presets y efectos."), "Entrar", colours::violet, Icon::synth },
                    Splash::Card { U ("Batería"), U ("16 kits por estilos y secuenciador."), U ("Pads para Maschine y General MIDI."), "Entrar", colours::rose, Icon::drums },
                    Splash::Card { "Instrumentos", U ("Piano, cuerdas, vientos, bajos..."), U ("Guitarras y bajos de cuerda física."), "Entrar", colours::cyan, Icon::keys } }),
          fxStrip (p),
          keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
    {
        emblemSmall = draw::emblem().rescaled (88, 88, juce::Graphics::highResamplingQuality);

        homeButton.setTooltip (U ("Volver a la pantalla de inicio"));
        homeButton.onClick = [this]
        {
            if (auto& goHome = geminis::homeHandler()) goHome();
            else showSplash (true);
        };
        presetButton.onClick = [this] { showPresetMenu(); };
        prevButton.onClick = [this] { stepPreset (-1); };
        nextButton.onClick = [this] { stepPreset (1); };
        for (auto* b : { &homeButton, &presetButton, &prevButton, &nextButton })
            addAndMakeVisible (b);

        volKnob.attach (p.apvts, "vol", "Volumen");
        volKnob.setCompact (true);
        volKnob.setBipolar (true);
        volKnob.slider.setDoubleClickReturnValue (true, 0.0);
        addAndMakeVisible (volKnob);
        addAndMakeVisible (outMeter);
        addAndMakeVisible (fxStrip);

        keyboard.setAvailableRange (24, 108);
        keyboard.setKeyPressBaseOctave (5);
        keyboard.setOctaveForMiddleC (3);
        keyboard.setScrollButtonsVisible (false);
        keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffe8e9f3));
        keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff15162a));
        keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff9a9cb5));
        keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::black.withAlpha (0.25f));
        keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, colours::panel);
        keyboard.setColour (juce::MidiKeyboardComponent::textLabelColourId, juce::Colour (0xff6a6d88));
        addAndMakeVisible (keyboard);

        splash.onChoose = [this] (int index)
        {
            const Mode modes[] = { Mode::synth, Mode::drums, Mode::instruments };
            enterMode (modes[juce::jlimit (0, 2, index)]);
        };
        addChildComponent (splash);

        proc.onPresetChanged = [this] { presetChanged(); };
        buildPage();
        showSplash (proc.getMode() == Mode::none);
        startTimerHz (30);
    }

    ~InstContent() override
    {
        stopTimer();
        proc.onPresetChanged = nullptr;
    }

    void paint (juce::Graphics& g) override
    {
        const auto acc = accent();
        juce::ColourGradient bgGrad (juce::Colour (0xff0d0c1d), 0.0f, 0.0f, colours::bg, 0.0f, (float) getHeight(), false);
        g.setGradientFill (bgGrad);
        g.fillAll();
        juce::ColourGradient glow (acc.withAlpha (0.07f), (float) getWidth() * 0.6f, 60.0f, acc.withAlpha (0.0f), (float) getWidth() * 0.6f, 520.0f, true);
        g.setGradientFill (glow);
        g.fillAll();

        auto bar = getLocalBounds().removeFromTop (68).toFloat();
        juce::ColourGradient barGrad (juce::Colour (0xff17172c), 0.0f, 0.0f, juce::Colour (0xff101021), 0.0f, bar.getBottom(), false);
        g.setGradientFill (barGrad);
        g.fillRect (bar);
        juce::ColourGradient line (acc.withAlpha (0.0f), 0.0f, bar.getBottom(), acc.withAlpha (0.0f), bar.getRight(), bar.getBottom(), false);
        line.addColour (0.5, acc.withAlpha (0.6f));
        g.setGradientFill (line);
        g.fillRect (bar.withTop (bar.getBottom() - 1.0f));

        g.drawImage (emblemSmall, juce::Rectangle<float> (14.0f, 10.0f, 48.0f, 48.0f));
        juce::ColourGradient gold (juce::Colour (0xfffff4d2), 0.0f, 18.0f, colours::goldDeep, 0.0f, 48.0f, false);
        draw::textWithGradient (g, U ("GÉMINIS"), Fonts::brand (24.0f).withExtraKerningFactor (0.12f), { 70.0f, 12.0f, 160.0f, 30.0f },
                                juce::Justification::centredLeft, gold);
        g.setColour (acc);
        draw::spacedText (g, modeTitle (proc.getMode()), Fonts::ui (11.0f, 700), { 71.0f, 40.0f, 160.0f, 16.0f }, juce::Justification::centredLeft, 0.25f);
    }

    void resized() override
    {
        splash.setBounds (getLocalBounds());
        auto bar = getLocalBounds().removeFromTop (68).reduced (14, 16);
        bar.removeFromLeft (220);
        homeButton.setBounds (bar.removeFromLeft (76));
        bar.removeFromLeft (14);
        prevButton.setBounds (bar.removeFromLeft (30));
        bar.removeFromLeft (4);
        presetButton.setBounds (bar.removeFromLeft (280));
        bar.removeFromLeft (4);
        nextButton.setBounds (bar.removeFromLeft (30));
        auto right = bar.removeFromRight (170);
        outMeter.setBounds (right.withSizeKeepingCentre (right.getWidth(), 18));
        bar.removeFromRight (10);
        volKnob.setBounds (bar.removeFromRight (56).withY (6).withHeight (58));

        auto body = getLocalBounds().withTrimmedTop (76).reduced (14, 0).withTrimmedBottom (14);
        const bool hasKeys = proc.getMode() != Mode::drums;
        keyboard.setVisible (hasKeys && proc.getMode() != Mode::none);
        if (hasKeys)
        {
            auto keys = body.removeFromBottom (54);
            keyboard.setBounds (keys);
            keyboard.setKeyWidth ((float) keys.getWidth() / 50.0f);
            body.removeFromBottom (8);
        }
        fxStrip.setBounds (body.removeFromBottom (104));
        body.removeFromBottom (8);
        if (page != nullptr) page->setBounds (body);
    }

private:
    juce::Colour accent() const { return modeColour (proc.getMode()); }

    void timerCallback() override
    {
        outMeter.push (proc.outputPeak.exchange (0.0f));
        if (page != nullptr && page->isVisible()) page->refresh();
        updatePresetLabel();
    }

    void showSplash (bool show)
    {
        splash.setVisible (show);
        if (show) splash.toFront (false);
    }

    void enterMode (Mode m)
    {
        proc.setMode (m);
        showSplash (false);
        buildPage();
        keyboard.grabKeyboardFocus();
    }

    void presetChanged()
    {
        if (proc.getMode() != shownMode) buildPage();
        else if (page != nullptr) page->presetChanged();
        updatePresetLabel();
    }

    void buildPage()
    {
        shownMode = proc.getMode();
        page.reset();
        switch (shownMode)
        {
            case Mode::synth:       page = std::make_unique<SynthPage> (proc); break;
            case Mode::drums:       page = std::make_unique<DrumPage> (proc); break;
            case Mode::instruments: page = std::make_unique<InstrumentPage> (proc); break;
            case Mode::none:        break;
        }
        const auto acc = accent();
        fxStrip.setAccent (acc);
        fxStrip.setVisible (shownMode != Mode::none);
        for (auto* b : { &prevButton, &presetButton, &nextButton })
        {
            b->setColour (juce::TextButton::buttonOnColourId, acc);
            b->setVisible (shownMode != Mode::none);
        }
        keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, acc.withAlpha (0.75f));
        keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, acc.withAlpha (0.25f));
        if (page != nullptr) addAndMakeVisible (*page);
        splash.toFront (false);
        resized();
        repaint();
    }

    juce::String currentPresetText() const
    {
        switch (proc.getMode())
        {
            case Mode::synth:
                return proc.getSynthPresetName().isNotEmpty() ? proc.getSynthPresetName() : U ("Sonido propio");
            case Mode::drums:
                return "Kit: " + (proc.getKitName().isNotEmpty() ? proc.getKitName() : U ("propio"));
            case Mode::instruments:
            {
                const int t = juce::roundToInt (proc.getParam ("ins_type"));
                return instrumentNames()[juce::jlimit (0, instrumentNames().size() - 1, t)];
            }
            case Mode::none: break;
        }
        return {};
    }

    void updatePresetLabel()
    {
        const auto text = currentPresetText();
        if (presetButton.getButtonText() != text) presetButton.setButtonText (text);
    }

    void showPresetMenu()
    {
        juce::PopupMenu menu;
        const auto m = proc.getMode();
        if (m == Mode::synth)
        {
            const auto& list = synthPresets();
            juce::StringArray cats;
            for (const auto& p : list) if (! cats.contains (p.category)) cats.add (p.category);
            for (const auto& cat : cats)
            {
                juce::PopupMenu sub;
                for (int i = 0; i < (int) list.size(); ++i)
                    if (list[(size_t) i].category == cat)
                        sub.addItem (i + 1, list[(size_t) i].name, true, proc.getSynthPreset() == i);
                if (cat == "Inicial") menu.addItem (1, U ("Empezar desde cero (sierra)"));
                else menu.addSubMenu (cat, sub);
            }
            const auto files = GeminisInstrumentProcessor::userSoundFolder().findChildFiles (juce::File::findFiles, false, "*.geminissonido");
            juce::PopupMenu user;
            for (int i = 0; i < files.size(); ++i) user.addItem (10000 + i, files[i].getFileNameWithoutExtension());
            if (files.isEmpty()) user.addItem (-1, U ("(todavía no hay sonidos tuyos)"), false);
            menu.addSeparator();
            menu.addSubMenu (U ("Mis sonidos"), user);
            menu.addItem (9001, U ("Guardar mi sonido..."));
            menu.addItem (9002, U ("Cargar sonido desde archivo..."));
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetButton), [this, files] (int r)
            {
                if (r <= 0) return;
                if (r >= 10000) { proc.loadUserSound (files[r - 10000]); return; }
                if (r == 9001) { saveSound(); return; }
                if (r == 9002) { loadSound(); return; }
                proc.applySynthPreset (r - 1);
            });
        }
        else if (m == Mode::drums)
        {
            const auto& kits = drumKits();
            for (int i = 0; i < (int) kits.size(); ++i)
                menu.addItem (i + 1, kits[(size_t) i].name + "  ·  " + juce::String (juce::roundToInt (kits[(size_t) i].bpm)) + " bpm", true, proc.getKit() == i);
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetButton), [this] (int r)
            {
                if (r > 0) proc.applyKit (r - 1);
            });
        }
        else if (m == Mode::instruments)
        {
            const auto& list = instrumentList();
            const int cur = juce::roundToInt (proc.getParam ("ins_type"));
            juce::String lastFamily;
            for (int i = 0; i < (int) list.size(); ++i)
            {
                const juce::String fam = instrumentFamily (i).name;
                if (fam != lastFamily) { menu.addSectionHeader (fam); lastFamily = fam; }
                menu.addItem (i + 1, list[(size_t) i].name, true, cur == i);
            }
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetButton), [this] (int r)
            {
                if (r > 0) proc.applyInstrument (r - 1);
            });
        }
    }

    void saveSound()
    {
        auto start = GeminisInstrumentProcessor::userSoundFolder().getChildFile (juce::File::createLegalFileName (currentPresetText()) + ".geminissonido");
        fileChooser = std::make_unique<juce::FileChooser> (U ("Guardar mi sonido"), start, "*.geminissonido");
        fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [this] (const juce::FileChooser& fc)
                                  {
                                      const auto f = fc.getResult();
                                      if (f != juce::File()) proc.saveUserSound (f.withFileExtension ("geminissonido"));
                                  });
    }

    void loadSound()
    {
        fileChooser = std::make_unique<juce::FileChooser> (U ("Cargar sonido"), GeminisInstrumentProcessor::userSoundFolder(), "*.geminissonido");
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this] (const juce::FileChooser& fc)
                                  {
                                      if (fc.getResult().existsAsFile()) proc.loadUserSound (fc.getResult());
                                  });
    }

    void stepPreset (int delta)
    {
        const auto m = proc.getMode();
        if (m == Mode::synth)
        {
            const int n = (int) synthPresets().size();
            proc.applySynthPreset (((juce::jmax (0, proc.getSynthPreset()) + delta) % n + n) % n);
        }
        else if (m == Mode::drums)
        {
            const int n = (int) drumKits().size();
            proc.applyKit (((juce::jmax (0, proc.getKit()) + delta) % n + n) % n);
        }
        else if (m == Mode::instruments)
        {
            const int n = (int) instrumentList().size();
            proc.applyInstrument (((juce::roundToInt (proc.getParam ("ins_type")) + delta) % n + n) % n);
        }
    }

    GeminisInstrumentProcessor& proc;
    juce::Image emblemSmall;
    juce::TextButton homeButton { "Inicio" }, presetButton, prevButton { "<" }, nextButton { ">" };
    ParamKnob volKnob;
    LevelMeter outMeter { "OUT" };
    Splash splash;
    FxStrip fxStrip;
    juce::MidiKeyboardComponent keyboard;
    std::unique_ptr<Page> page;
    Mode shownMode = Mode::none;
    std::unique_ptr<juce::FileChooser> fileChooser;
};
} // namespace geminis::inst

//==============================================================================
GeminisInstrumentEditor::GeminisInstrumentEditor (GeminisInstrumentProcessor& p)
    : AudioProcessorEditor (p), processor (p)
{
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
    content = std::make_unique<geminis::inst::InstContent> (p);
    addAndMakeVisible (*content);

    // Se escala entre el 60 % y el 140 % manteniendo la proporción; en pantallas bajas empieza más pequeño
    double scale = 1.0;
    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        scale = juce::jlimit (0.6, 1.0, (display->userArea.getHeight() - 90) / (double) designH);
    setResizable (true, true);
    setResizeLimits (juce::roundToInt (designW * 0.6), juce::roundToInt (designH * 0.6), juce::roundToInt (designW * 1.4), juce::roundToInt (designH * 1.4));
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) designW / (double) designH);
    setSize (juce::roundToInt (designW * scale), juce::roundToInt (designH * scale));
}

GeminisInstrumentEditor::~GeminisInstrumentEditor()
{
    content.reset();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void GeminisInstrumentEditor::paint (juce::Graphics& g)
{
    g.fillAll (geminis::colours::bg);
}

void GeminisInstrumentEditor::resized()
{
    const float scale = (float) getWidth() / (float) designW;
    content->setBounds (0, 0, designW, designH);
    content->setTransform (juce::AffineTransform::scale (scale));
}
