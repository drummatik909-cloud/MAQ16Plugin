#pragma once
#include <vector>
#include "Types.h"

namespace maq {

// Une unité MAQ : 3 rangées de 16 pas pilotées par l'horloge MIDI (24 impulsions par noire).
class Unit {
public:
    Unit();

    std::array<RowConfig, kRows> rows;
    int relativeBase = 36;  // note de référence pour NoteRel (hypothèse : note reçue absente = 36)
    std::array<int, kRows> relativeBaseRow;   // référence propre à une rangée relative ; -1 = utiliser relativeBase
    void setRelativeBase(int row, int note);  // row = -1 : toutes les rangées

    // Chaînage (lot 5). Mode Onr : à la fin de sa passe, la rangée démarre la rangée Onr suivante.
    bool chainInsideUnit = true;  // chaînage Onr entre les rangées de l'unité
    bool wrapOnr = true;          // la dernière rangée Onr relance la première ; sinon la chaîne s'arrête
    const std::vector<int>& finishedRows() const { return finished_; }  // rangées One-Shot terminées à la dernière horloge
    bool anyRowRunning() const;
    void holdRow(int r);                                  // met la rangée en attente
    void triggerRow(int r, std::vector<MidiMsg>& out);    // démarre la rangée immédiatement
    void triggerUnit(std::vector<MidiMsg>& out);          // redémarre l'unité (la rangée Onr de tête démarre)

    // Gamme (lot 3) : en mode « quantification en sortie » (2), chaque note jouée est ramenée dans la gamme
    int scaleMode = 0;
    int scaleRoot = 0;
    int scaleType = 0;

    void start();                                   // départ avec remise au premier pas (MIDI Start)
    void continuePlaying();                         // reprise (MIDI Continue)
    void stop(std::vector<MidiMsg>& out);           // arrêt, envoie les Note Off en attente
    void panic(std::vector<MidiMsg>& out);          // Note Off + All Notes Off sur les canaux utilisés
    void tick(std::vector<MidiMsg>& out);           // à appeler à chaque horloge MIDI

    bool isPlaying() const { return playing_; }
    int currentStep(int row) const;                 // dernier pas joué (1..16), 0 si aucun
    void seedRandom(uint32_t seed);

private:
    struct RowState {
        int t = 0;            // compteur de pas joués depuis le départ
        int phase = 0;        // horloges écoulées dans le pas courant
        int lastStep = 0;     // 1..16
        bool running = true;  // faux après la fin d'un One-Shot
        bool noteActive = false;
        int note = 0;
        int noteChannel = 0;
        int noteLeft = 0;
        int currentPot = 0;   // pour les rangées Velocity
        bool hasCurrent = false;
        uint32_t rng = 1;
    };

    std::array<RowState, kRows> st_;
    std::vector<int> finished_;
    bool playing_ = false;
    uint32_t seed_ = 12345;

    void resetStates();
    void applyOnrHead();
    bool isOnr(int r) const;
    int nextOnrRow(int r) const;
    int baseFor(int r) const;
    void fireRow(int r, std::vector<MidiMsg>& out);
    int position(int r);  // 1..16 ou -1 si la rangée a fini
    void noteOffIfActive(int r, std::vector<MidiMsg>& out);
    static uint32_t nextRandom(uint32_t& s);
};

}  // namespace maq
