#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    constexpr int kW = 1000, kH = 640, kArtW = 480;

    struct Stage { const char* name; float at; juce::uint32 aura; juce::uint32 bolt; };
    const Stage kStages[6] = {
        { "BASE FORM",         0.00f, 0xff8fa6ff, 0xffc8d4ff },
        { "SUPER SAIYAN",      0.18f, 0xffffe45c, 0xfffff6a8 },
        { "SUPER SAIYAN 2",    0.38f, 0xfffff07a, 0xff7fdcff },
        { "SUPER SAIYAN 3",    0.56f, 0xffffae00, 0xfffff2a0 },
        { "SAIYAN GOD",        0.74f, 0xffff2323, 0xffffa0a0 },
        { "SUPER SAIYAN BLUE", 0.90f, 0xff0fa6ff, 0xffe6fcff } };

    const float kAggr[5] = { 0.95f, 0.90f, 1.05f, 1.10f, 1.15f };

    float smooth01 (float t) { t = juce::jlimit (0.0f, 1.0f, t); return t * t * (3.0f - 2.0f * t); }

    juce::Font ui (float h, bool bold = false)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    }
}

//==============================================================================
SuperSatEditor::SuperSatEditor (SuperSatProcessor& pr)
    : AudioProcessorEditor (&pr), proc (pr)
{
    imgs[0] = juce::ImageFileFormat::loadFrom (BinaryData::goku0_png, BinaryData::goku0_pngSize);
    imgs[1] = juce::ImageFileFormat::loadFrom (BinaryData::goku1_png, BinaryData::goku1_pngSize);
    imgs[2] = juce::ImageFileFormat::loadFrom (BinaryData::goku2_png, BinaryData::goku2_pngSize);
    imgs[3] = juce::ImageFileFormat::loadFrom (BinaryData::goku3_png, BinaryData::goku3_pngSize);
    imgs[4] = juce::ImageFileFormat::loadFrom (BinaryData::goku4_png, BinaryData::goku4_pngSize);
    imgs[5] = juce::ImageFileFormat::loadFrom (BinaryData::goku5_png, BinaryData::goku5_pngSize);

    // sagome sfocate (bassa risoluzione) per l'alone di energia dietro Goku
    for (size_t i = 1; i < imgs.size(); ++i)
    {
        if (imgs[i].isNull()) continue;
        auto small = imgs[i].rescaled (180, 240, juce::Graphics::mediumResamplingQuality)
                            .convertedToFormat (juce::Image::ARGB);
        juce::Image dst (juce::Image::ARGB, 180, 240, true);
        juce::ImageConvolutionKernel k (13);
        k.createGaussianBlur (6.0f);
        k.applyToImage (dst, small, dst.getBounds());
        glow[i] = dst;
    }

    auto pctF = [] (double v) { return juce::String (juce::roundToInt (v)) + "%"; };
    addKnob (kDL, "driveLow",  "LOW DRIVE",  pctF);
    addKnob (kDM, "driveMid",  "MID DRIVE",  pctF);
    addKnob (kDH, "driveHigh", "HIGH DRIVE", pctF);
    addKnob (kXL, "xLow",  "LOW X",  [] (double v) { return juce::String (juce::roundToInt (v)) + " Hz"; });
    addKnob (kXH, "xHigh", "HIGH X", [] (double v) { return juce::String (v / 1000.0, 1) + " k"; });
    addKnob (kTone, "tone", "TONE",  [] (double v) { return (v > 0 ? "+" : "") + juce::String (juce::roundToInt (v)); });
    addKnob (kMix,  "mix",  "MIX",   pctF);
    addKnob (kOut,  "out",  "LEVEL", [] (double v) { return (v >= 0 ? "+" : "") + juce::String (v, 1) + " dB"; });

    const char* names[5] = { "TUBE", "TAPE", "CRUNCH", "FOLD", "FUZZ" };
    for (int i = 0; i < 5; ++i)
    {
        auto& b = styleBtns[(size_t) i];
        b.setButtonText (names[i]);
        b.setClickingTogglesState (false);
        b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff171722));
        b.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        b.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        b.onClick = [this, i]
        {
            if (auto* prm = proc.apvts.getParameter ("style"))
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i));
                prm->endChangeGesture();
            }
        };
        addAndMakeVisible (b);
    }

    parts.reserve (500);
    setSize (kW, kH);
    startTimerHz (30);
}

SuperSatEditor::~SuperSatEditor()
{
    stopTimer();
}

void SuperSatEditor::addKnob (Knob& k, const juce::String& id, const juce::String& name,
                              std::function<juce::String (double)> fmt)
{
    k.s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    k.s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 18);
    k.s.setLookAndFeel (&lnf);
    k.s.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    k.s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.s.textFromValueFunction = [fmt] (double v) { return fmt (v); };
    k.s.valueFromTextFunction = [] (const juce::String& t) { return t.getDoubleValue(); };
    addAndMakeVisible (k.s);

    k.l.setText (name, juce::dontSendNotification);
    k.l.setJustificationType (juce::Justification::centred);
    k.l.setColour (juce::Label::textColourId, juce::Colour (0xff8a8aa0));
    k.l.setFont (ui (11.0f, true));
    addAndMakeVisible (k.l);

    k.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.s);
}

void SuperSatEditor::placeKnob (Knob& k, int cx, int y, int size)
{
    k.l.setBounds (cx - 55, y, 110, 16);
    k.s.setBounds (cx - size / 2 - 6, y + 16, size + 12, size + 20);
}

void SuperSatEditor::resized()
{
    const int px = kArtW + 14, pw = kW - kArtW - 28;   // area destra
    const int col3 = pw / 3;
    placeKnob (kDL, px + col3 / 2,           262, 84);
    placeKnob (kDM, px + col3 + col3 / 2,    262, 84);
    placeKnob (kDH, px + 2 * col3 + col3 / 2, 262, 84);

    const int col5 = pw / 5;
    Knob* row[5] = { &kXL, &kXH, &kTone, &kMix, &kOut };
    for (int i = 0; i < 5; ++i)
        placeKnob (*row[i], px + col5 * i + col5 / 2, 412, 64);

    const int bw = (pw - 4 * 8) / 5;
    for (int i = 0; i < 5; ++i)
        styleBtns[(size_t) i].setBounds (px + i * (bw + 8), 570, bw, 34);
}

//==============================================================================
float SuperSatEditor::targetPower() const
{
    auto P = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
    const float base = (P ("driveLow") * 0.3f + P ("driveMid") * 0.4f + P ("driveHigh") * 0.3f) / 100.0f;
    const int style = juce::jlimit (0, 4, (int) P ("style"));
    const float live = proc.outLevel.load();
    const float ln = live > 0.0005f ? juce::jmin (1.0f, lvl * 5.0f) : 0.5f;
    return juce::jlimit (0.0f, 1.0f, base * kAggr[style] * (0.3f + 0.7f * P ("mix") / 100.0f) * (0.85f + 0.3f * ln));
}

SuperSatEditor::Bolt SuperSatEditor::makeBolt (float x, float y, float len)
{
    Bolt b;
    b.pts.push_back ({ x, y });
    float a = rng.nextFloat() * juce::MathConstants<float>::twoPi;
    const int seg = 7 + rng.nextInt (5);
    for (int i = 0; i < seg; ++i)
    {
        a += (rng.nextFloat() - 0.5f) * 1.4f;
        x += std::cos (a) * len / (float) seg;
        y += std::sin (a) * len / (float) seg;
        b.pts.push_back ({ x, y });
    }
    b.ttl = 3 + rng.nextInt (3);
    return b;
}

void SuperSatEditor::updateSpectrum()
{
    const int pos = proc.ringPos.load (std::memory_order_relaxed);
    constexpr int N = SuperSatProcessor::fftSize;
    for (int i = 0; i < N; ++i)
        fftData[(size_t) i] = proc.ring[(size_t) ((pos + i) & (N - 1))];
    std::fill (fftData.begin() + N, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable (fftData.data(), (size_t) N);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    const double sr = proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0;
    for (int b = 0; b < 64; ++b)
    {
        const float f = 20.0f * std::pow (1000.0f, ((float) b + 0.5f) / 64.0f);
        const int bin = juce::jlimit (1, N / 2 - 1, (int) (f / (float) (sr * 0.5) * (float) (N / 2)));
        const float mag = fftData[(size_t) bin] / ((float) N * 0.25f);
        const float db = juce::Decibels::gainToDecibels (mag, -90.0f);
        const float v = juce::jlimit (0.0f, 1.0f, juce::jmap (db, -80.0f, -6.0f, 0.0f, 1.0f));
        auto& s = spec[(size_t) b];
        s += (v - s) * (v > s ? 0.5f : 0.15f);
    }
}

void SuperSatEditor::timerCallback()
{
    // livello audio e battiti
    const float live = proc.outLevel.load();
    lvl += (live - lvl) * (live > lvl ? 0.6f : 0.2f);
    const bool onset = lvl > avgLvl * 1.45f + 0.02f;
    avgLvl += (lvl - avgLvl) * 0.06f;
    pulse = juce::jmax (pulse * 0.88f, onset ? 1.0f : 0.0f);

    // potenza
    const float tg = targetPower();
    p += (tg - p) * (tg > p ? 0.2f : 0.06f);

    // stadio e colori
    int a = 0;
    while (a < 5 && p >= kStages[a + 1].at) ++a;
    float t = 0.0f;
    if (a < 5)
        t = smooth01 ((p - (kStages[a + 1].at - 0.07f)) / 0.07f);
    stageA = a; stageT = t;
    const int dom = (t > 0.5f && a < 5) ? a + 1 : a;
    stageName = kStages[dom].name;
    const auto a0 = juce::Colour (kStages[a].aura), b0 = juce::Colour (kStages[juce::jmin (5, a + 1)].aura);
    aura = a0.interpolatedWith (b0, t);
    boltCol = juce::Colour (kStages[a].bolt).interpolatedWith (juce::Colour (kStages[juce::jmin (5, a + 1)].bolt), t);
    lnf.aura = aura;

    if (dom > lastStage) flash = 1.0f;
    lastStage = dom;

    // shake
    const float sh = std::pow (p, 2.4f) * 9.0f;
    shakeX = (rng.nextFloat() - 0.5f) * sh;
    shakeY = (rng.nextFloat() - 0.5f) * sh;

    // particelle di fiamma lungo i bordi del pannello
    tt += 1.0f / 30.0f;
    rayAngle += 0.006f + 0.03f * p + 0.02f * pulse;

    const int spawn = (int) (p * 9.0f + lvl * 16.0f);
    for (int i = 0; i < spawn && parts.size() < 520; ++i)
    {
        Particle q;
        const float zone = rng.nextFloat();
        if (zone < 0.4f)        { q.x = rng.nextFloat() * 150.0f;                 q.y = 250.0f + rng.nextFloat() * 390.0f; }   // lato sinistro
        else if (zone < 0.8f)   { q.x = (float) kArtW - rng.nextFloat() * 150.0f; q.y = 250.0f + rng.nextFloat() * 390.0f; }   // lato destro
        else                    { q.x = 110.0f + rng.nextFloat() * 260.0f;        q.y = 120.0f + rng.nextFloat() * 140.0f; }   // sopra la testa
        q.vx = (rng.nextFloat() - 0.5f) * 1.6f;
        q.vy = -(2.5f + rng.nextFloat() * 5.0f) * (1.0f + p * 1.8f);
        q.life = 0.0f;
        q.maxLife = 20.0f + rng.nextFloat() * 28.0f;
        q.sz = (16.0f + rng.nextFloat() * 34.0f) * (0.6f + p);
        parts.push_back (q);
    }
    for (size_t i = parts.size(); i-- > 0;)
    {
        auto& q = parts[i];
        q.life += 1.0f; q.x += q.vx; q.y += q.vy;
        if (q.life > q.maxLife) parts.erase (parts.begin() + (long) i);
    }

    // fulmini
    if (p > 0.33f && rng.nextFloat() < 0.3f + p * 0.6f)
    {
        const int n = 1 + (int) (p * 3.0f);
        for (int i = 0; i < n; ++i)
        {
            const bool left = rng.nextBool();
            const float x = left ? rng.nextFloat() * 110.0f : (float) kArtW - rng.nextFloat() * 110.0f;
            bolts.push_back (makeBolt (x, rng.nextFloat() * (float) kH, 110.0f + rng.nextFloat() * 190.0f));
        }
    }
    for (size_t i = bolts.size(); i-- > 0;)
        if (--bolts[i].ttl <= 0) bolts.erase (bolts.begin() + (long) i);

    if (flash > 0.0f) flash -= 0.09f;

    // scintille
    if (p > 0.15f)
    {
        const int ns = (int) (p * 4.0f + lvl * 6.0f);
        for (int i = 0; i < ns && sparks.size() < 160; ++i)
        {
            Spark s;
            s.x = 60.0f + rng.nextFloat() * 360.0f;
            s.y = 120.0f + rng.nextFloat() * 420.0f;
            const float a = rng.nextFloat() * juce::MathConstants<float>::twoPi;
            const float v = 2.0f + rng.nextFloat() * 6.0f;
            s.vx = std::cos (a) * v; s.vy = std::sin (a) * v - 2.0f;
            s.life = 0.0f; s.maxLife = 10.0f + rng.nextFloat() * 18.0f;
            sparks.push_back (s);
        }
    }
    for (size_t i = sparks.size(); i-- > 0;)
    {
        auto& s = sparks[i];
        s.life += 1.0f; s.x += s.vx; s.y += s.vy; s.vy += 0.25f;
        if (s.life > s.maxLife) sparks.erase (sparks.begin() + (long) i);
    }

    // onde d'urto (a ogni colpo e a ogni cambio di stadio)
    if (pulse > 0.99f && p > 0.08f && rings.size() < 5) rings.push_back ({ 30.0f, 0.8f, false });
    if (flash > 0.98f) { rings.push_back ({ 20.0f, 1.0f, true }); rings.push_back ({ -30.0f, 1.0f, true }); }
    for (size_t i = rings.size(); i-- > 0;)
    {
        auto& r = rings[i];
        r.r += r.big ? 26.0f : 16.0f;
        r.a -= r.big ? 0.035f : 0.05f;
        if (r.a <= 0.0f) rings.erase (rings.begin() + (long) i);
    }

    updateSpectrum();

    // pulsanti di stile
    const int style = (int) proc.apvts.getRawParameterValue ("style")->load();
    for (int i = 0; i < 5; ++i)
    {
        auto& b = styleBtns[(size_t) i];
        b.setColour (juce::TextButton::buttonColourId,
                     i == style ? aura.withMultipliedBrightness (0.55f) : juce::Colour (0xff171722));
    }
    for (auto* k : { &kDL, &kDM, &kDH, &kXL, &kXH, &kTone, &kMix, &kOut })
        k->s.repaint();

    repaint();
}

//==============================================================================
void SuperSatEditor::drawSpectrum (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (juce::Colour (0xff0c0c14));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (juce::Colour (0xff23232f));
    g.drawRoundedRectangle (r, 8.0f, 1.0f);

    auto xOf = [&] (float f) { return r.getX() + r.getWidth() * std::log (f / 20.0f) / std::log (1000.0f); };
    const float xl = xOf (proc.apvts.getRawParameterValue ("xLow")->load());
    const float xh = xOf (proc.apvts.getRawParameterValue ("xHigh")->load());

    g.setColour (aura.withAlpha (0.05f));
    g.fillRect (xl, r.getY(), xh - xl, r.getHeight());
    g.setColour (aura.withAlpha (0.4f));
    for (float x : { xl, xh })
        for (float yy = r.getY(); yy < r.getBottom(); yy += 10.0f)
            g.drawVerticalLine ((int) x, yy, yy + 5.0f);

    g.setColour (aura.withAlpha (0.55f));
    g.setFont (ui (10.0f, true));
    g.drawText ("LOW",  (int) r.getX() + 6, (int) r.getBottom() - 16, 40, 12, juce::Justification::left);
    g.drawText ("MID",  (int) xl + 6,       (int) r.getBottom() - 16, 40, 12, juce::Justification::left);
    g.drawText ("HIGH", (int) xh + 6,       (int) r.getBottom() - 16, 40, 12, juce::Justification::left);

    const float bw = r.getWidth() / 64.0f;
    for (int b = 0; b < 64; ++b)
    {
        const float v = juce::jmax (spec[(size_t) b], 0.03f);
        const float h = std::pow (v, 1.5f) * r.getHeight() * (0.8f + 0.2f * p);
        juce::ColourGradient gr (aura.withAlpha (0.8f), 0.0f, r.getBottom(),
                                 aura.withAlpha (0.0f), 0.0f, r.getBottom() - h, false);
        g.setGradientFill (gr);
        g.fillRect (r.getX() + (float) b * bw + 1.0f, r.getBottom() - h, bw - 2.0f, h);
    }
}

void SuperSatEditor::drawArtPanel (juce::Graphics& g, juce::Rectangle<float> art)
{
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (art.toNearestInt());

    const float cx = art.getCentreX();
    const bool baseOpaque = (stageA == 0);   // il Goku base ha lo sfondo nero

    // ---- A. sfondo: gradiente + raggi rotanti
    g.fillAll (juce::Colours::black);
    g.setGradientFill (juce::ColourGradient (aura.withAlpha (0.0f), cx, 0.0f,
                                             aura.withAlpha (0.10f + 0.40f * p), cx, (float) kH, false));
    g.fillRect (art);

    if (p > 0.1f)
    {
        const int nb = 14;
        const float R = 800.0f;
        for (int k = 0; k < nb; ++k)
        {
            const float a0 = rayAngle + (float) k * juce::MathConstants<float>::twoPi / (float) nb;
            const float a1 = a0 + juce::MathConstants<float>::twoPi / (float) nb * (0.28f + 0.2f * p);
            juce::Path ray;
            ray.startNewSubPath (cx, 230.0f);
            ray.lineTo (cx + std::cos (a0) * R, 230.0f + std::sin (a0) * R);
            ray.lineTo (cx + std::cos (a1) * R, 230.0f + std::sin (a1) * R);
            ray.closeSubPath();
            g.setColour (aura.withAlpha (0.04f + 0.11f * p + 0.05f * pulse));
            g.fillPath (ray);
        }
    }

    // ---- B. alone grande
    {
        const float R = 260.0f + 240.0f * p;
        juce::ColourGradient hg (aura.withAlpha (juce::jlimit (0.0f, 1.0f, 0.18f + 0.6f * p + 0.25f * pulse)), cx, 240.0f,
                                 aura.withAlpha (0.0f), cx + R, 240.0f, true);
        g.setGradientFill (hg);
        g.fillRect (art);
    }

    // ---- C. fiamme dietro Goku (due livelli: esterno colorato, nucleo chiaro)
    for (auto& q : parts)
    {
        const float k = 1.0f - q.life / q.maxLife;
        g.setColour (aura.withAlpha (k * 0.28f));
        g.fillEllipse (q.x - q.sz * 0.5f, q.y - q.sz * 1.3f, q.sz, q.sz * 2.6f);
        g.setColour (aura.brighter (0.8f).withAlpha (k * 0.30f));
        g.fillEllipse (q.x - q.sz * 0.25f, q.y - q.sz * 0.7f, q.sz * 0.5f, q.sz * 1.4f);
    }

    // ---- D + E. Goku: bagliore sulla sagoma + immagine (zoom che respira, rotazione, tremolio)
    const float breathe = std::sin (tt * 2.4f) * 0.008f;
    const float zoom = 1.0f + breathe + 0.035f * p + 0.03f * pulse + 0.05f * lvl;
    const float rot = std::sin (tt * 7.0f) * 0.004f * p + (rng.nextFloat() - 0.5f) * 0.004f * p * p;
    const float s0 = (float) kArtW / 720.0f;
    auto figureTf = [&] (float extra)
    {
        return juce::AffineTransform::scale (s0)
                   .scaled (zoom * extra, zoom * extra, cx, 640.0f)
                   .rotated (rot, cx, 640.0f)
                   .translated (shakeX, shakeY);
    };
    auto glowTf = [&] (float extra)
    {
        return juce::AffineTransform::scale ((float) kArtW / 180.0f)
                   .scaled (zoom * extra, zoom * extra, cx, 600.0f)
                   .rotated (rot, cx, 640.0f)
                   .translated (shakeX, shakeY);
    };
    auto drawGlow = [&] (int idx, float op, float extra)
    {
        if (glow[(size_t) idx].isNull() || op <= 0.01f) return;
        g.setColour (aura.withAlpha (juce::jlimit (0.0f, 1.0f, op)));
        g.drawImageTransformed (glow[(size_t) idx], glowTf (extra), true);
    };
    {
        const float gop = (0.35f + 0.6f * p) * (0.85f + 0.3f * pulse);
        for (float extra : { 1.10f + 0.12f * p, 1.04f })
        {
            drawGlow (stageA, gop * (1.0f - stageT), extra);
            if (stageA < 5) drawGlow (stageA + 1, gop * stageT, extra);
        }
    }
    auto drawImg = [&] (int idx, float op)
    {
        auto& im = imgs[(size_t) idx];
        if (im.isNull()) return;
        g.setOpacity (op);
        g.drawImageTransformed (im, figureTf (1.0f));
    };
    drawImg (stageA, 1.0f);
    if (stageT > 0.001f && stageA < 5) drawImg (stageA + 1, stageT);
    g.setOpacity (1.0f);
    juce::ignoreUnused (baseOpaque);

    // ---- F. effetti davanti
    for (auto& r : rings)
    {
        if (r.r < 0.0f) continue;
        g.setColour (aura.withAlpha (juce::jlimit (0.0f, 1.0f, r.a) * 0.7f));
        g.drawEllipse (cx - r.r, 300.0f - r.r * 0.9f, r.r * 2.0f, r.r * 1.8f, r.big ? 5.0f : 3.0f);
    }

    for (auto& b : bolts)
    {
        juce::Path path;
        for (size_t i = 0; i < b.pts.size(); ++i)
            i == 0 ? path.startNewSubPath (b.pts[i]) : path.lineTo (b.pts[i]);
        g.setColour (boltCol.withAlpha (0.22f));
        g.strokePath (path, juce::PathStrokeType (12.0f, juce::PathStrokeType::curved));
        g.setColour (boltCol.withAlpha (0.55f));
        g.strokePath (path, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved));
        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved));
    }

    for (auto& s : sparks)
    {
        const float k = 1.0f - s.life / s.maxLife;
        g.setColour (aura.withAlpha (k * 0.5f));
        g.fillEllipse (s.x - 4.0f, s.y - 4.0f, 8.0f, 8.0f);
        g.setColour (juce::Colours::white.withAlpha (k));
        g.fillEllipse (s.x - 1.5f, s.y - 1.5f, 3.0f, 3.0f);
    }

    // linee di velocità ai bordi quando si è al massimo
    if (p > 0.55f)
    {
        const int n = 12 + (int) (p * 24.0f);
        for (int i = 0; i < n; ++i)
        {
            const float a = rng.nextFloat() * juce::MathConstants<float>::twoPi;
            const float r0 = 250.0f + rng.nextFloat() * 120.0f, r1 = r0 + 60.0f + rng.nextFloat() * 160.0f;
            g.setColour (juce::Colours::white.withAlpha (0.05f + rng.nextFloat() * 0.18f * p));
            g.drawLine (cx + std::cos (a) * r0, 280.0f + std::sin (a) * r0 * 1.1f,
                        cx + std::cos (a) * r1, 280.0f + std::sin (a) * r1 * 1.1f, 1.0f + rng.nextFloat() * 2.0f);
        }
    }

    // ---- G. interfaccia: nome dello stadio, barra potenza, scouter
    const auto name = stageName;
    g.setFont (ui (26.0f, true));
    g.setColour (aura.withAlpha (0.55f));
    g.drawText (name, 17, 15, 440, 34, juce::Justification::left);
    g.setColour (juce::Colours::white);
    g.drawText (name, 16, 14, 440, 34, juce::Justification::left);

    g.setColour (juce::Colours::white.withAlpha (0.14f));
    g.fillRect (16.0f, 52.0f, 220.0f, 7.0f);
    g.setColour (aura);
    g.fillRect (16.0f, 52.0f, 220.0f * p, 7.0f);

    const float targetPl = p * p * 14000.0f + lvl * 600.0f;
    scout += (targetPl - scout) * 0.15f;
    const int pl = juce::roundToInt (scout);
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::bold)));
    g.setColour (juce::Colour (0xff74ff9e));
    g.drawText ("POWER LEVEL " + juce::String (pl), 0, 66, kArtW - 14, 18, juce::Justification::right);
    if (pl > 9000 && (juce::Time::getMillisecondCounter() / 250) % 2 == 0)
    {
        g.setFont (ui (20.0f, true));
        g.drawText ("IT'S OVER 9000!!!", 0, 86, kArtW - 14, 26, juce::Justification::right);
    }

    if (flash > 0.0f)
    {
        g.setColour (juce::Colours::white.withAlpha (juce::jlimit (0.0f, 1.0f, flash) * 0.7f));
        g.fillRect (art);
    }
}

void SuperSatEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff101018));

    drawArtPanel (g, { 0.0f, 0.0f, (float) kArtW, (float) kH });

    // cornice che si accende con la potenza
    g.setColour (aura.withAlpha (0.25f + 0.75f * p));
    g.drawRect (0, 0, kW, kH, 2);
    g.drawVerticalLine (kArtW, 0.0f, (float) kH);

    // titolo
    g.setColour (juce::Colours::white);
    g.setFont (ui (22.0f, true));
    g.drawText ("SUPER", kArtW + 16, 8, 80, 30, juce::Justification::left);
    g.setColour (aura);
    g.drawText ("SAT", kArtW + 90, 8, 60, 30, juce::Justification::left);
    g.setColour (juce::Colour (0xff7c7c90));
    g.setFont (ui (12.0f));
    g.drawText ("multiband saturator - turn it up and power up", kArtW + 150, 8, 360, 30, juce::Justification::left);

    drawSpectrum (g, { (float) kArtW + 14.0f, 48.0f, (float) (kW - kArtW - 28), 190.0f });

    g.setColour (juce::Colour (0xff23232f));
    g.drawHorizontalLine (396, (float) kArtW + 14.0f, (float) kW - 14.0f);
    g.drawHorizontalLine (556, (float) kArtW + 14.0f, (float) kW - 14.0f);
}
