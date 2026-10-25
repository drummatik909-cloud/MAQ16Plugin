#pragma once
// NON COMPILÉ dans l'environnement de rédaction : à valider avec JUCE 8 sur Windows puis macOS.
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include "engine/Unit.h"

class MaqProcessor : public juce::AudioProcessor {
public:
    MaqProcessor();
    ~MaqProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override { return true; }
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }  // façade : étape 2
    bool hasEditor() const override { return false; }

    const juce::String getName() const override { return "MAQ 16-3"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override {}      // état : étape 2
    void setStateInformation(const void*, int) override {}

private:
    void emit(const std::vector<maq::MidiMsg>& msgs, int samplePos, juce::MidiBuffer& out);

    maq::Unit unit_;
    double sampleRate_ = 44100.0;
    bool wasPlaying_ = false;
    double nextClock_ = 0.0;      // prochaine horloge MIDI (24 par noire) à traiter
    double expectedStart_ = 0.0;  // position attendue au début du bloc suivant, en horloges

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MaqProcessor)
};
