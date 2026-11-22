#pragma once
// Quatre unités MAQ virtuelles (A à D) : lecture en parallèle, chaînage en série, liens entre unités,
// canal de sortie par unité (lot 5).
#include <array>
#include <vector>
#include "Remote.h"
#include "Unit.h"

namespace maq {

constexpr int kUnits = 4;

enum class ChannelMode { PerRow, Forced, Offset };

struct UnitOutput {
    ChannelMode mode = ChannelMode::PerRow;  // par rangée (origine), canal forcé, ou décalage de N canaux
    int forced = 1;                          // 1..16 (mode Forced)
    int offset = 0;                          // -15..15 (mode Offset, circulaire)
};

enum class ChainKind { None, Series };
enum class ChainScope { PerRow, WholeUnit };

struct ChainConfig {
    ChainKind kind = ChainKind::None;
    ChainScope scope = ChainScope::WholeUnit;  // par rangée (A1 -> B1 ...) ou par unité entière (A -> B ...)
    bool loopToFirst = true;                   // fin de chaîne : retour à la première unité, sinon arrêt
    std::array<bool, kUnits> member{{true, true, true, true}};  // unités de la chaîne, dans l'ordre A..D
};

// Une rangée de notes d'une unité fixe la note de référence des rangées relatives (nr, Pr) d'une autre
struct TransposeLink {
    bool enabled = false;
    int srcUnit = 0, srcRow = 0;
    int dstUnit = 1;
    int dstRow = -1;  // -1 : toutes les rangées de l'unité cible
};

// Une rangée Prg (Program Change) d'une unité pilote une autre comme la télécommande de la vraie MAQ
struct ControlLink {
    bool enabled = false;
    int srcUnit = 0, srcRow = 0;
    int dstUnit = 1;
};

struct ChannelCollision {
    int channel;   // 1..16
    int unitMask;  // bit u = unité u émet sur ce canal
};

class Project {
public:
    static constexpr int kMaxLinks = 8;

    std::array<Unit, kUnits> units;
    std::array<bool, kUnits> active{{true, false, false, false}};
    std::array<UnitOutput, kUnits> output;
    ChainConfig chain;
    std::array<TransposeLink, kMaxLinks> transposeLinks;
    std::array<ControlLink, kMaxLinks> controlLinks;

    Project();

    void start();
    void continuePlaying();
    void stop(std::vector<MidiMsg>& out);
    void panic(std::vector<MidiMsg>& out);
    void tick(std::vector<MidiMsg>& out);  // une horloge MIDI pour toutes les unités actives
    bool isPlaying() const;
    void configureChain();  // applique le réglage de chaînage aux unités (aussi appelé au départ)

    // Refuse (false) un lien vers soi-même ou qui créerait une boucle de pilotage
    bool setControlLink(int index, const ControlLink& link);

    int selectedRow(int unit) const { return selectedRow_[static_cast<size_t>(unit)]; }

    static int outputChannel(const UnitOutput& o, int channel0to15);
    // Canaux utilisés par plusieurs unités qui peuvent jouer en même temps. Les rangées d'une même unité peuvent
    // partager un canal ; en chaînage, les unités enchaînées ne jouent pas ensemble (par rangée : seules deux
    // rangées de numéros différents peuvent se chevaucher).
    std::vector<ChannelCollision> collisions() const;
    // Chaînage en série impossible à faire avancer : une rangée utilisée qui n'est pas en OnS ou Onr ne finit jamais.
    // Retourne true et la première rangée en cause (unité 0..3, rangée 0..2).
    bool chainProblem(int& unit, int& row) const;
    int activeMemberCount() const;

private:
    std::array<int, kUnits> selectedRow_{};
    std::vector<MidiMsg> tmp_;

    int memberList(std::array<int, kUnits>& list) const;
    int tickOrder(std::array<int, kUnits>& order) const;
    void holdForChain();
    void applyChain();
    void applyLinks(size_t from, int unit);
    void tag(size_t from, int unit);
    void flush(std::vector<MidiMsg>& out);
    bool wouldCycle(int index, const ControlLink& link) const;
    bool canOverlap(int unitA, int rowA, int unitB, int rowB) const;
};

}  // namespace maq
