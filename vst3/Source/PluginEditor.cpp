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
    imgs[0] = juce::ImageCache::getFromMemory (BinaryData::goku0_jpg, BinaryData::goku0_jpgSize);
    imgs[1] = juce::ImageCache::getFromMemory (BinaryData::goku1_jpg, BinaryData::goku1_jpgSize);
    imgs[2] = juce::ImageCache::getFromMemory (BinaryData::goku2_jpg, BinaryData::goku2_jpgSize);
    imgs[3] = juce::ImageCache::getFromMemory (BinaryData::goku3_jpg, BinaryData::goku3_jpgSize);
    imgs[4] = juce::ImageCache::getFromMemory (BinaryData::goku4_jpg, BinaryData::goku4_jpgSize);
    imgs[5] = juce::ImageCache::getFromMemory (BinaryData::goku5_jpg, BinaryData::goku5_jpgSize);

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
    const int spawn = (int) (p * 5.0f + lvl * 8.0f);
    for (int i = 0; i < spawn && parts.size() < 480; ++i)
    {
        Particle q;
        q.x = rng.nextFloat() * (float) kArtW;
        q.y = (float) kH - rng.nextFloat() * 90.0f;
        q.vx = (rng.nextFloat() - 0.5f) * 1.2f;
        q.vy = -(2.0f + rng.nextFloat() * 4.0f) * (1.0f + p * 1.8f);
        q.life = 0.0f;
        q.maxLife = 22.0f + rng.nextFloat() * 28.0f;
        q.sz = (12.0f + rng.nextFloat() * 26.0f) * (0.6f + p);
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
    g.setColour (juce::Colours::black);
    g.fillRect (art);

    // Goku: dissolvenza tra uno stadio e il successivo, leggero zoom con la potenza
    const float sc = 1.0f + 0.03f * p + 0.012f * pulse;
    const float w = art.getWidth() * sc, h = art.getHeight() * sc;
    const float ox = (art.getWidth() - w) * 0.5f + shakeX;
    const float oy = (art.getHeight() - h) * 0.35f + shakeY;
    auto drawImg = [&] (int idx, float op)
    {
        auto& im = imgs[(size_t) idx];
        if (im.isNull()) return;
        g.setOpacity (op);
        g.drawImage (im, ox, oy, w, h, 0, 0, im.getWidth(), im.getHeight());
    };
    drawImg (stageA, 1.0f);
    if (stageT > 0.001f && stageA < 5) drawImg (stageA + 1, stageT);
    g.setOpacity (1.0f);

    // alone di energia ai bordi
    {
        const auto c = art.getCentre();
        juce::ColourGradient vg (aura.withAlpha (0.0f), c.x, c.y,
                                 aura.withAlpha (0.55f * p + 0.1f * pulse), c.x + 330.0f, c.y + 120.0f, true);
        g.setGradientFill (vg);
        g.fillRect (art);
    }

    // fiamme
    for (auto& q : parts)
    {
        const float k = 1.0f - q.life / q.maxLife;
        g.setColour (aura.withAlpha (k * 0.30f));
        g.fillEllipse (q.x - q.sz * 0.5f, q.y - q.sz * 1.2f, q.sz, q.sz * 2.4f);
    }

    // fulmini
    for (auto& b : bolts)
    {
        juce::Path path;
        for (size_t i = 0; i < b.pts.size(); ++i)
            i == 0 ? path.startNewSubPath (b.pts[i]) : path.lineTo (b.pts[i]);
        g.setColour (boltCol.withAlpha (0.28f));
        g.strokePath (path, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved));
        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved));
    }

    // scritta dello stadio
    const auto name = stageName;
    g.setFont (ui (26.0f, true));
    g.setColour (aura.withAlpha (0.55f));
    g.drawText (name, 17, 15, 440, 34, juce::Justification::left);
    g.setColour (juce::Colours::white);
    g.drawText (name, 16, 14, 440, 34, juce::Justification::left);

    // barra potenza
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.fillRect (16.0f, 52.0f, 220.0f, 7.0f);
    g.setColour (aura);
    g.fillRect (16.0f, 52.0f, 220.0f * p, 7.0f);

    // scouter
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
        g.setColour (juce::Colours::white.withAlpha (juce::jlimit (0.0f, 1.0f, flash) * 0.6f));
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
