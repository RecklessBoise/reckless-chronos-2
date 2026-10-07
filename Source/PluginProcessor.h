#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Params.h"
#include "dsp/Timbre.h"
#include "dsp/Effects.h"
#include "dsp/Arp.h"
#include "presets/PresetManager.h"

class RecklessChronosProcessor : public juce::AudioProcessor
{
public:
    RecklessChronosProcessor();
    ~RecklessChronosProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Reckless Chronos 2"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgramIndex; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    rc::PresetManager presets;
    juce::MidiKeyboardState keyboardState;

    std::atomic<float> uiBend { 0.0f }, uiMod { 0.0f };   // on-screen joystick
    std::atomic<int> mode { (int) rc::Mode::Program };
    std::atomic<int> voiceCount { 0 };
    std::atomic<float> meterL { 0.0f }, meterR { 0.0f };

    juce::String currentPresetName { "Init Program" };
    int currentProgramIndex = 0;   // index into factory program list (host program change)
    float uiScale = 0.8f;
    int editTimbre = 0;

    void loadProgram (int index);
    void loadCombi (int index);
    void setMode (rc::Mode m);
    void panic() { panicRequested.store (true); }

    std::function<void()> onPresetChanged; // message-thread notification for the editor

private:
    struct Event { int pos; int type; int a; float b; }; // type: 0 on, 1 off, 2 cc, 3 bend, 4 at
    void handleEvent (const Event& e);
    void noteOn (int note, float vel);
    void noteOff (int note);
    void renderSegment (int start, int n);
    float vectorGain (int t) const;

    std::array<rc::Timbre, rc::kNumTimbres> timbres;
    std::array<std::array<std::atomic<float>*, rc::tpCount>, rc::kNumTimbres> raw {};
    std::array<std::atomic<float>*, rc::gpCount> graw {};

    rc::MasterDelay delay;
    rc::MasterReverb reverb;
    rc::Limiter limiter;
    rc::Arpeggiator arp;
    bool arpWasOn = false;

    juce::AudioBuffer<float> mixBuf, dlySend, revSend, fxOut;
    std::vector<Event> events;
    std::vector<rc::Arpeggiator::Event> arpEvents;

    float midiMod = 0.0f, midiBend = 0.0f, midiAT = 0.0f;
    bool sustain = false;
    double sampleRate = 48000.0, timeSec = 0.0, bpm = 120.0;
    float smoothedGain[rc::kNumTimbres] { 1, 1, 1, 1 };
    std::atomic<bool> panicRequested { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RecklessChronosProcessor)
};
