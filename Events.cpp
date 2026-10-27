#include "Events.h"
#include <algorithm>

namespace maq {

namespace {
EventConfig make(EventKind kind, int octaves = 1, int cc = 0) {
    EventConfig e;
    e.kind = kind;
    e.octaves = octaves;
    e.ccNumber = cc;
    return e;
}
}  // namespace

EventConfig eventForIndex(int index) {
    index = std::max(0, std::min(kEventCount - 1, index));
    if (index == 0) return make(EventKind::Off);
    if (index == 1) return make(EventKind::FreeCC);
    if (index <= 6) return make(EventKind::NoteAbs, index - 1);          // 2..6  -> 1..5 octaves
    if (index <= 11) return make(EventKind::NoteRel, index - 6);         // 7..11 -> 1..5 octaves
    if (index == 12) return make(EventKind::Velocity);
    if (index <= 44) return make(EventKind::FixedCC, 1, index - 13);     // 13..44 -> index 0..31
    if (index == 45) return make(EventKind::PitchBend);
    if (index == 46) return make(EventKind::PitchBendPos);
    if (index == 47) return make(EventKind::PitchBendNeg);
    if (index == 48) return make(EventKind::AfterTouch);
    if (index == 49) return make(EventKind::ProgramChange);
    if (index <= 54) return make(EventKind::NoteAbsPause, index - 49);   // 50..54 -> 1..5 octaves
    return make(EventKind::NoteRelPause, index - 54);                    // 55..59 -> 1..5 octaves
}

std::string eventLabel(int index) {
    index = std::max(0, std::min(kEventCount - 1, index));
    if (index == 0) return "---";
    if (index == 1) return "CC";
    if (index <= 6) return "nA" + std::to_string(index - 1);
    if (index <= 11) return "nr" + std::to_string(index - 6);
    if (index == 12) return "dYn";
    if (index <= 44) return "C" + std::to_string(fixedCcNumber(index - 13));
    if (index == 45) return "PIt";
    if (index == 46) return "PIP";
    if (index == 47) return "PI-";
    if (index == 48) return "At";
    if (index == 49) return "Prg";
    if (index <= 54) return "PA" + std::to_string(index - 49);
    return "Pr" + std::to_string(index - 54);
}

const char* modeLabel(int mode) {
    static const char* const labels[kModeCount] = {"FOr", "bAC", "PE1", "rnd", "PE2", "OnS", "Onr", "OnE"};
    mode = std::max(0, std::min(kModeCount - 1, mode));
    return labels[mode];
}

}  // namespace maq
