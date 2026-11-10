#pragma once
#include "PluginProcessor.h"
#include <array>
#include <vector>

// Manopole con l'arco colorato come l'aura corrente
struct KnobLookAndFeel : public juce::LookAndFeel_V4
{
    juce::Colour aura { 0xffffd23f };

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider&) override
    {
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
        const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float ang = a0 + pos * (a1 - a0);
        const juce::PathStrokeType st (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        juce::Path track;
        track.addCentredArc (c.x, c.y, r, r, 0.0f, a0, a1, true);
        g.setColour (juce::Colour (0xff25253a));
        g.strokePath (track, st);

        juce::Path val;
        val.addCentredArc (c.x, c.y, r, r, 0.0f, a0, ang, true);
        g.setColour (aura.withAlpha (0.25f));
        g.strokePath (val, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (aura);
        g.strokePath (val, st);

        juce::Path ind;
        ind.startNewSubPath (c.x + std::sin (ang) * r * 0.30f, c.y - std::cos (ang) * r * 0.30f);
        ind.lineTo          (c.x + std::sin (ang) * r * 0.82f, c.y - std::cos (ang) * r * 0.82f);
        g.setColour (juce::Colours::white);
        g.strokePath (ind, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
};

class SuperSatEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SuperSatEditor (SuperSatProcessor&);
    ~SuperSatEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider s;
        juce::Label l;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    struct Particle { float x, y, vx, vy, life, maxLife, sz; };
    struct Bolt { std::vector<juce::Point<float>> pts; int ttl; };

    void timerCallback() override;
    void addKnob (Knob&, const juce::String& id, const juce::String& name,
                  std::function<juce::String (double)> fmt);
    void placeKnob (Knob&, int cx, int y, int size);
    void updateSpectrum();
    float targetPower() const;
    Bolt makeBolt (float x, float y, float len);
    void drawSpectrum (juce::Graphics&, juce::Rectangle<float>);
    void drawArtPanel (juce::Graphics&, juce::Rectangle<float>);

    SuperSatProcessor& proc;
    KnobLookAndFeel lnf;
    Knob kDL, kDM, kDH, kXL, kXH, kTone, kMix, kOut;
    std::array<juce::TextButton, 5> styleBtns;

    std::array<juce::Image, 6> imgs;

    // stato animazione
    float p = 0.0f, lvl = 0.0f, avgLvl = 0.0f, pulse = 0.0f, flash = 0.0f, scout = 0.0f;
    int lastStage = 0;
    juce::Colour aura { 0xff8fa6ff }, boltCol { 0xffc8d4ff };
    juce::String stageName { "BASE FORM" };
    int stageA = 0;
    float stageT = 0.0f;
    float shakeX = 0.0f, shakeY = 0.0f;
    juce::Random rng;
    std::vector<Particle> parts;
    std::vector<Bolt> bolts;

    // spettro
    juce::dsp::FFT fft { SuperSatProcessor::fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) SuperSatProcessor::fftSize,
                                                 juce::dsp::WindowingFunction<float>::hann };
    std::array<float, 2 * SuperSatProcessor::fftSize> fftData {};
    std::array<float, 64> spec {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuperSatEditor)
};
