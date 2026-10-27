#pragma once
// Table des Events et des modes, dans l'ordre de défilement du menu EVENT / MODE.
#include <string>
#include "Types.h"

namespace maq {

constexpr int kEventCount = 60;
constexpr int kModeCount = 8;
constexpr int kFreeCcEventIndex = 1;  // l'entrée « contrôleur libre » (0..127)

// Index 0 : ---   1 : contrôleur libre   2..6 : nA1..nA5   7..11 : nr1..nr5   12 : dYn
// 13..44 : contrôleurs fixes (C0..C19, C64..C74, C84)   45..47 : PIt, PIP, PI-   48 : At   49 : Prg
// 50..54 : PA1..PA5   55..59 : Pr1..Pr5
EventConfig eventForIndex(int index);   // pour le contrôleur libre, ccNumber = 0 (à remplacer par l'appelant)
std::string eventLabel(int index);      // 3 caractères au plus : "nA1", "C64", "PI-"...

const char* modeLabel(int mode);        // "FOr", "bAC", "PE1", "rnd", "PE2", "OnS", "Onr", "OnE"

}  // namespace maq
