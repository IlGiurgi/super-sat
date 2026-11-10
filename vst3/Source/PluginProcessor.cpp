#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout SuperSatProcessor::createLayout()
{
    using FP = juce::AudioParameterFloat;
    using NR = juce::NormalisableRange<float>;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    p.push_back (std::make_unique<FP> (juce::ParameterID { "driveLow", 1 },  "Low Drive",  NR (0.f, 100.f, 0.1f), 25.f));
    p.push_back (std::make_unique<FP> (juce::ParameterID { "driveMid", 1 },  "Mid Drive",  NR (0.f, 100.f, 0.1f), 35.f));
    p.push_back (std::make_unique<FP> (juce::ParameterID { "driveHigh", 1 }, "High Drive", NR (0.f, 100.f, 0.1f), 25.f));

    NR xl (60.f, 800.f, 1.f);   xl.setSkewForCentre (220.f);
    NR xh (1200.f, 9000.f, 1.f); xh.setSkewForCentre (3200.f);
    p.push_back (std::make_unique<FP> (juce::ParameterID { "xLow", 1 },  "Low Crossover",  xl, 200.f));
    p.push_back (std::make_unique<FP> (juce::ParameterID { "xHigh", 1 }, "High Crossover", xh, 2500.f));

    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "style", 1 }, "Style",
                 juce::StringArray { "TUBE", "TAPE", "CRUNCH", "FOLD", "FUZZ" }, 0));

    p.push_back (std::make_unique<FP> (juce::ParameterID { "tone", 1 }, "Tone",  NR (-100.f, 100.f, 0.1f), 0.f));
    p.push_back (std::make_unique<FP> (juce::ParameterID { "mix", 1 },  "Mix",   NR (0.f, 100.f, 0.1f), 100.f));
    p.push_back (std::make_unique<FP> (juce::ParameterID { "out", 1 },  "Level", NR (-18.f, 6.f, 0.1f), 0.f));

    return { std::make_move_iterator (p.begin()), std::make_move_iterator (p.end()) };
}

SuperSatProcessor::SuperSatProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

bool SuperSatProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == l.getMainInputChannelSet();
}

//==============================================================================
void SuperSatProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    const int maxBlock = juce::jmax (samplesPerBlock, 512);

    os.initProcessing ((size_t) maxBlock);
    os.reset();

    juce::dsp::ProcessSpec osSpec { sampleRate * 4.0, (juce::uint32) (maxBlock * 4), 2 };
    lowSplit.prepare (osSpec);
    midSplit.prepare (osSpec);
    lowSplit.reset();
    midSplit.reset();

    const float latency = os.getLatencyInSamples();
    setLatencySamples (juce::roundToInt (latency));

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlock, 2 };
    dryDelay.prepare (spec);
    dryDelay.setDelay (latency);
    dryDelay.reset();

    dryBuf.setSize (2, maxBlock);

    for (int c = 0; c < 2; ++c)
    {
        toneLo[c].reset();
        toneHi[c].reset();
        dcX[c] = dcY[c] = 0.0f;
    }
    lastTone = 9999.0f;

    for (auto& s : sPre)  s.reset (sampleRate * 4.0, 0.02);
    for (auto& s : sPost) s.reset (sampleRate * 4.0, 0.02);
    sMix.reset (sampleRate, 0.02);
    sOut.reset (sampleRate, 0.02);

    // porta i valori correnti subito a regime
    auto P = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    const char* ids[3] = { "driveLow", "driveMid", "driveHigh" };
    for (int i = 0; i < 3; ++i)
    {
        const float db = -6.0f + P (ids[i]) * 0.46f;
        sPre[i].setCurrentAndTargetValue (juce::Decibels::decibelsToGain (db));
        sPost[i].setCurrentAndTargetValue (0.62f / (1.0f + juce::jmax (0.0f, db) / 38.0f));
    }
    sMix.setCurrentAndTargetValue (P ("mix") / 100.0f);
    sOut.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (P ("out")));
}

//==============================================================================
float SuperSatProcessor::shapeSample (int style, float x)
{
    x = juce::jlimit (-1.0f, 1.0f, x);
    float u = x * 2.5f;
    switch (style)
    {
        case 0:  return std::tanh (u * 1.4f + 0.3f) - std::tanh (0.3f);          // TUBE
        case 1:  return std::tanh (u);                                           // TAPE
        case 2:  u = juce::jlimit (-1.5f, 1.5f, u); return u - u * u * u / 6.75f; // CRUNCH
        case 3:  return std::sin (u * 1.1f);                                     // FOLD
        default: return u > 0.0f ? 1.0f - std::exp (-u * 1.6f)                   // FUZZ
                                 : -(1.0f - std::exp (u * 0.8f)) * 0.85f;
    }
}

void SuperSatProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int nCh = juce::jmin (2, buffer.getNumChannels());
    const int n   = buffer.getNumSamples();
    for (int c = nCh; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);
    if (n == 0 || nCh == 0)
        return;

    auto P = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    // parametri
    const char* ids[3] = { "driveLow", "driveMid", "driveHigh" };
    for (int i = 0; i < 3; ++i)
    {
        const float db = -6.0f + P (ids[i]) * 0.46f;
        sPre[i].setTargetValue  (juce::Decibels::decibelsToGain (db));
        sPost[i].setTargetValue (0.62f / (1.0f + juce::jmax (0.0f, db) / 38.0f));
    }
    sMix.setTargetValue (P ("mix") / 100.0f);
    sOut.setTargetValue (juce::Decibels::decibelsToGain (P ("out")));
    const int style = juce::jlimit (0, 4, (int) P ("style"));
    lowSplit.setCutoffFrequency (P ("xLow"));
    midSplit.setCutoffFrequency (P ("xHigh"));

    const float tone = P ("tone");
    if (std::abs (tone - lastTone) > 1.0e-3f)
    {
        lastTone = tone;
        const float tiltDb = tone * 0.12f;
        *toneLo[0].coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowShelf  (sr, 600.0f,  0.7071f, juce::Decibels::decibelsToGain (-tiltDb));
        *toneHi[0].coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 2500.0f, 0.7071f, juce::Decibels::decibelsToGain ( tiltDb));
        *toneLo[1].coefficients = *toneLo[0].coefficients;
        *toneHi[1].coefficients = *toneHi[0].coefficients;
    }

    // segnale secco, ritardato come la latenza dell'oversampling
    if (dryBuf.getNumSamples() < n)
        dryBuf.setSize (2, n, false, false, true);
    for (int c = 0; c < nCh; ++c)
    {
        const float* in = buffer.getReadPointer (c);
        float* d = dryBuf.getWritePointer (c);
        for (int i = 0; i < n; ++i)
        {
            dryDelay.pushSample (c, in[i]);
            d[i] = dryDelay.popSample (c);
        }
    }

    // saturazione a 4x, tre bande
    juce::dsp::AudioBlock<float> block (buffer);
    auto sub = block.getSubsetChannelBlock (0, (size_t) nCh);
    auto up = os.processSamplesUp (sub);
    const size_t un = up.getNumSamples();

    for (size_t i = 0; i < un; ++i)
    {
        const float g0 = sPre[0].getNextValue(), g1 = sPre[1].getNextValue(), g2 = sPre[2].getNextValue();
        const float q0 = sPost[0].getNextValue(), q1 = sPost[1].getNextValue(), q2 = sPost[2].getNextValue();

        for (int c = 0; c < nCh; ++c)
        {
            const float x = up.getSample (c, (int) i);
            float low, rest, mid, high;
            lowSplit.processSample (c, x, low, rest);
            midSplit.processSample (c, rest, mid, high);

            const float y = shapeSample (style, low  * g0) * q0
                          + shapeSample (style, mid  * g1) * q1
                          + shapeSample (style, high * g2) * q2;
            up.setSample (c, (int) i, y);
        }
    }
    os.processSamplesDown (sub);

    // tono, mix, livello, limite di sicurezza
    float sumSq = 0.0f;
    int pos = ringPos.load (std::memory_order_relaxed);

    for (int i = 0; i < n; ++i)
    {
        const float mix = sMix.getNextValue();
        const float outG = sOut.getNextValue();
        const float wetG = std::sin (mix * juce::MathConstants<float>::halfPi);
        const float dryG = std::cos (mix * juce::MathConstants<float>::halfPi);
        float mono = 0.0f;

        for (int c = 0; c < nCh; ++c)
        {
            float w = buffer.getSample (c, i);
            // blocco DC (la saturazione asimmetrica ne genera)
            const float dcOut = w - dcX[c] + 0.995f * dcY[c];
            dcX[c] = w; dcY[c] = dcOut; w = dcOut;
            w = toneHi[c].processSample (toneLo[c].processSample (w));

            float y = (dryBuf.getSample (c, i) * dryG + w * wetG) * outG;
            const float a = std::abs (y);
            if (a > 0.95f)
                y = (y < 0.0f ? -1.0f : 1.0f) * (0.95f + 0.05f * std::tanh ((a - 0.95f) / 0.05f));

            buffer.setSample (c, i, y);
            sumSq += y * y;
            mono += y;
        }
        ring[(size_t) pos] = mono / (float) nCh;
        pos = (pos + 1) & (fftSize - 1);
    }
    ringPos.store (pos, std::memory_order_relaxed);
    outLevel.store (std::sqrt (sumSq / (float) (n * nCh)), std::memory_order_relaxed);
}

//==============================================================================
juce::AudioProcessorEditor* SuperSatProcessor::createEditor() { return new SuperSatEditor (*this); }

void SuperSatProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void SuperSatProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SuperSatProcessor(); }
