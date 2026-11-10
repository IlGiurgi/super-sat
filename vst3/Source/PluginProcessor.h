#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

class SuperSatProcessor : public juce::AudioProcessor
{
public:
    SuperSatProcessor();
    ~SuperSatProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Super Sat"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // dati per la grafica (letti dall'editor)
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    std::array<float, fftSize> ring {};
    std::atomic<int> ringPos { 0 };
    std::atomic<float> outLevel { 0.0f };

private:
    static float shapeSample (int style, float x);

    juce::dsp::Oversampling<float> os { 2, 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false };
    juce::dsp::LinkwitzRileyFilter<float> lowSplit, midSplit;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 8192 };
    juce::AudioBuffer<float> dryBuf;

    juce::dsp::IIR::Filter<float> toneLo[2], toneHi[2];
    float lastTone = 9999.0f;
    double sr = 44100.0;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> sPre[3], sPost[3], sMix, sOut;
    float dcX[2] { 0.0f, 0.0f }, dcY[2] { 0.0f, 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuperSatProcessor)
};
