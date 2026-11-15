#include "Remote.h"
#include <algorithm>

namespace maq {

bool applyProgramChange(Unit& unit, int& selectedRow, int value) {
    selectedRow = std::max(0, std::min(kRows - 1, selectedRow));
    if (value < 0 || value > 127) return false;
    if (value <= 2) { selectedRow = value; return true; }  // sélection de rangée 1 à 3

    RowConfig& row = unit.rows[static_cast<size_t>(selectedRow)];
    switch (value) {
        case 3: row.rowMute = false; return true;   // rangée entière active
        case 4: row.rowMute = true; return true;    // rangée entière coupée
        case 5: row.mute.fill(false); return true;  // tous les pas actifs
        case 6: row.mute.fill(true); return true;   // tous les pas coupés
        case 8: row.mode = PlayMode::Forward; return true;
        case 9: row.mode = PlayMode::Backward; return true;
        case 10: row.mode = PlayMode::Pendulum1; return true;
        case 11: row.mode = PlayMode::Random; return true;
        default: break;
    }
    if (value >= 16 && value <= 31) { row.mute[static_cast<size_t>(value - 16)] = false; return true; }  // pas 1 à 16 actifs
    if (value >= 32 && value <= 47) { row.mute[static_cast<size_t>(value - 32)] = true; return true; }   // pas 1 à 16 coupés
    if (value >= 48 && value <= 63) { row.first = value - 48 + 1; return true; }                          // premier pas
    if (value >= 64 && value <= 79) { row.last = value - 64 + 1; return true; }                           // dernier pas
    if (value >= 80 && value <= 111) {                                                                    // contrôleur fixe 0 à 31
        row.event.kind = EventKind::FixedCC;
        row.event.ccNumber = value - 80;
        return true;
    }
    if (value >= 112 && value <= 116) { row.event.kind = EventKind::NoteAbs; row.event.octaves = value - 111; return true; }
    if (value >= 117 && value <= 121) { row.event.kind = EventKind::NoteRel; row.event.octaves = value - 116; return true; }
    if (value == 122) { row.event.kind = EventKind::PitchBend; return true; }
    if (value == 123) { row.event.kind = EventKind::PitchBendPos; return true; }
    if (value == 124) { row.event.kind = EventKind::PitchBendNeg; return true; }
    if (value == 125) { row.event.kind = EventKind::ProgramChange; return true; }
    return false;  // 126 = Cn1, 127 = Ad1 (pas encore émulés) et valeurs hors tableau
}

}  // namespace maq
