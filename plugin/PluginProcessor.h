#pragma once
// Étape 2a : paramètres automatisables (48 potards, 48 mutes, réglages de rangée) et façade.
// À valider avec JUCE 8 : la compilation se fait sur GitHub.
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <vector>
#include "engine/Events.h"
#include "engine/Unit.h"

class MaqProcessor : public juce::AudioProcessor {
public:
    MaqProcessor();
    ~MaqProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override { return true; }
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

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

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // Identifiants des paramètres (rangée 0..2, pas 0..15)
    static juce::String potId(int row, int step) { return "pot_" + juce::String(row) + "_" + juce::String(step); }
    static juce::String muteId(int row, int step) { return "mute_" + juce::String(row) + "_" + juce::String(step); }
    static juce::String rowId(const juce::String& name, int row) { return name + "_" + juce::String(row); }

    juce::AudioProcessorValueTreeState apvts;

    // Lus par l'interface (mis à jour dans processBlock)
    std::array<std::atomic<int>, maq::kRows> uiStep;
    std::atomic<double> uiBpm{0.0};
    std::atomic<bool> uiPlaying{false};

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void syncEngineFromParams();
    void emit(const std::vector<maq::MidiMsg>& msgs, int samplePos, juce::MidiBuffer& out);

    struct RowParams {
        std::atomic<float>* event = nullptr;
        std::atomic<float>* cc = nullptr;
        std::atomic<float>* chan = nullptr;
        std::atomic<float>* first = nullptr;
        std::atomic<float>* last = nullptr;
        std::atomic<float>* gate = nullptr;
        std::atomic<float>* noteLen = nullptr;
        std::atomic<float>* mode = nullptr;
        std::array<std::atomic<float>*, maq::kSteps> pot{};
        std::array<std::atomic<float>*, maq::kSteps> mute{};
    };
    std::array<RowParams, maq::kRows> rp_;

    maq::Unit unit_;
    double sampleRate_ = 44100.0;
    bool wasPlaying_ = false;
    double nextClock_ = 0.0;      // prochaine horloge MIDI (24 par noire) à traiter
    double expectedStart_ = 0.0;  // position attendue au début du bloc suivant, en horloges

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MaqProcessor)
};
