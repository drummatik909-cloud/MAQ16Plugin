#pragma once
// Étape 2b, lot 1 : paramètres automatisables, presets (99 slots), banque, dossiers d'import/export.
// À valider avec JUCE 8 : la compilation se fait sur GitHub.
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <vector>
#include "engine/Capture.h"
#include "engine/Events.h"
#include "engine/MidiExport.h"
#include "engine/Project.h"
#include "engine/Scale.h"
#include "engine/Unit.h"
#include "presets/Bank.h"
#include "presets/FolderSettings.h"
#include "ui/Preferences.h"
#include "ui/Strings.h"

// 1 : le plugin se déclare comme instrument avec une sortie audio stéréo silencieuse (compatible Ableton Live, Cubase)
// 0 : déclaration « effet MIDI » sans entrée ni sortie audio
#ifndef MAQ_AS_INSTRUMENT
#define MAQ_AS_INSTRUMENT 1
#endif

class MaqProcessor : public juce::AudioProcessor {
public:
    MaqProcessor();
    ~MaqProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
#if MAQ_AS_INSTRUMENT
    bool isBusesLayoutSupported(const BusesLayout& layout) const override {
        const auto& out = layout.getMainOutputChannelSet();
        return layout.getMainInputChannelSet().isDisabled() &&
               (out == juce::AudioChannelSet::stereo() || out.isDisabled());
    }
#else
    bool isBusesLayoutSupported(const BusesLayout&) const override { return true; }
#endif
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MAQ 16-3"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return MAQ_AS_INSTRUMENT == 0; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // Identifiants des paramètres (rangée 0..2, pas 0..15, unité 0..3). L'unité A garde les identifiants
    // d'origine (projets existants) ; les unités B à D ajoutent le préfixe « u1_ », « u2_ », « u3_ ».
    static juce::String unitPrefix(int unit) { return unit == 0 ? juce::String() : "u" + juce::String(unit) + "_"; }
    static juce::String potId(int row, int step, int unit = 0) {
        return unitPrefix(unit) + "pot_" + juce::String(row) + "_" + juce::String(step);
    }
    static juce::String muteId(int row, int step, int unit = 0) {
        return unitPrefix(unit) + "mute_" + juce::String(row) + "_" + juce::String(step);
    }
    static juce::String rowId(const juce::String& name, int row, int unit = 0) {
        return unitPrefix(unit) + name + "_" + juce::String(row);
    }
    // Réglages propres à une unité : "active", "outmode" (0 par rangée, 1 forcé, 2 décalage), "outchan", "outoff"
    static juce::String unitId(const juce::String& name, int unit) { return name + "_unit" + juce::String(unit); }
    // Paramètres globaux : "valnote" (noms de notes), "notefr" (Do Ré Mi), "preset" (slot choisi, 0..99),
    // "chainkind" (0 aucun, 1 série par rangée, 2 série par unité), "chainloop",
    // liens : "tr_on", "tr_su", "tr_sr", "tr_du", "tr_dr" (transposition) et "ct_on", "ct_su", "ct_sr", "ct_du" (pilotage)

    // ---- Presets (slots 1..99) ----
    void storePreset(int slot);        // enregistre les réglages actuels dans le slot
    bool recallPreset(int slot);       // charge le slot (faux s'il est vide)
    bool presetUsed(int slot) const;
    juce::String presetName(int slot) const;
    void setPresetName(int slot, const juce::String& name);
    int usedPresetCount() const;

    // ---- Banque en fichier .maqbank ----
    bool exportBank(const juce::File& file) const;
    bool importBank(const juce::File& file);

    // ---- Dossiers d'import / d'export (mémorisés entre les sessions) ----
    maq::FolderSettings folders;
    void saveFolderSettings();

    // ---- Préférences communes à tous les projets (langue, copie locale du manuel) ----
    maq::Preferences prefs;
    void savePreferences();

    // ---- Panic : coupe toutes les notes (traité au prochain bloc audio) ----
    void requestPanic() { panicRequested.store(true); }

    // ---- Apprentissage : notes jouées au clavier, mises de côté pour l'interface ----
    struct NoteIn {
        int note = 0;
        int velocity = 0;
    };
    std::atomic<bool> learnActive{false};
    bool popNote(NoteIn& out);

    // ---- Unités A à D ----
    std::atomic<int> selectedUnit{0};  // unité affichée par la façade (les presets et l'apprentissage s'y appliquent)
    int uiStepFor(int unit, int row) const { return uiStepAll[static_cast<size_t>(unit)][static_cast<size_t>(row)].load(); }
    maq::Project makeProjectSnapshot() const;                 // réglages actuels sous forme de projet
    std::vector<maq::ChannelCollision> channelCollisions() const;

    // ---- Export MIDI : boucle (rendu sans transport) ou capture (derniers passages joués) ----
    // source : 0 = boucle, 1 = capture ; lengthIndex : voir l'interface ;
    // scope : 0 = 3 rangées, 1..3 = une rangée (de l'unité choisie), 4 = toutes les unités.
    // Retourne un tableau vide s'il n'y a aucune note à exporter.
    std::vector<std::uint8_t> buildMidi(int source, int lengthIndex, int scope) const;

    // ---- Suivi de la tonalité par MIDI : fondamentale reçue sur le canal 16 (0..11), -1 si aucune ----
    std::atomic<int> followRoot{-1};

    juce::AudioProcessorValueTreeState apvts;

    // Lus par l'interface (mis à jour dans processBlock)
    std::array<std::array<std::atomic<int>, maq::kRows>, maq::kUnits> uiStepAll;
    std::atomic<double> uiBpm{0.0};
    std::atomic<bool> uiPlaying{false};
    std::atomic<bool> panicRequested{false};

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void syncEngineFromParams();
    void configureUnit(maq::Unit& u, int unit) const;
    void configureProject(maq::Project& p) const;
    static BusesProperties makeBuses();
    void emit(const std::vector<maq::MidiMsg>& msgs, int samplePos, juce::MidiBuffer& out);
    void setParamValue(const juce::String& id, int value);

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
    std::array<std::array<RowParams, maq::kRows>, maq::kUnits> rp_;

    struct UnitParams {
        std::atomic<float>* active = nullptr;
        std::atomic<float>* outMode = nullptr;
        std::atomic<float>* outChan = nullptr;
        std::atomic<float>* outOff = nullptr;
    };
    std::array<UnitParams, maq::kUnits> up_;
    std::atomic<float>* chainKind_ = nullptr;
    std::atomic<float>* chainLoop_ = nullptr;
    std::atomic<float>* trOn_ = nullptr; std::atomic<float>* trSu_ = nullptr; std::atomic<float>* trSr_ = nullptr;
    std::atomic<float>* trDu_ = nullptr; std::atomic<float>* trDr_ = nullptr;
    std::atomic<float>* ctOn_ = nullptr; std::atomic<float>* ctSu_ = nullptr; std::atomic<float>* ctSr_ = nullptr;
    std::atomic<float>* ctDu_ = nullptr;

    std::atomic<float>* followKey_ = nullptr;
    maq::CaptureBuffer capture_;
    std::int64_t captureClock_ = 0;  // compte les horloges jouées (fil audio uniquement)
    std::atomic<float>* scaleRoot_ = nullptr;
    std::atomic<float>* scaleType_ = nullptr;
    std::atomic<float>* scaleMode_ = nullptr;
    juce::AbstractFifo noteFifo_{64};
    std::array<NoteIn, 64> noteBuf_{};

    mutable juce::CriticalSection bankLock_;
    maq::Bank bank_;

    maq::Project project_;
    double sampleRate_ = 44100.0;
    bool wasPlaying_ = false;
    double nextClock_ = 0.0;      // prochaine horloge MIDI (24 par noire) à traiter
    double expectedStart_ = 0.0;  // position attendue au début du bloc suivant, en horloges

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MaqProcessor)
};
