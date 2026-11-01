#pragma once
// Gammes, quantification et apprentissage (lot 3).
#include <string>
#include "Types.h"
#include "ui/Strings.h"

namespace maq {

constexpr int kScaleCount = 15;

// Modes de la gamme : 0 = aucune, 1 = verrouillage (potards et notes apprises), 2 = quantification en sortie
constexpr int kScaleModeOff = 0;
constexpr int kScaleModeLock = 1;
constexpr int kScaleModeOutput = 2;

// type : 0 chromatique, 1 majeure, 2 mineure naturelle, 3 mineure harmonique, 4 mineure mélodique,
//        5 dorien, 6 phrygien, 7 lydien, 8 mixolydien, 9 locrien, 10 pentatonique majeure,
//        11 pentatonique mineure, 12 blues, 13 ton entier, 14 diminuée
unsigned scaleMask(int type);                          // 12 bits : intervalles autorisés à partir de la fondamentale
bool inScale(int midiNote, int root, int type);        // root : 0 = Do ... 11 = Si
int quantizeNote(int midiNote, int root, int type);    // note de la gamme la plus proche (à égalité : la plus grave)
int quantizeNoteInRange(int midiNote, int lo, int hi, int root, int type);  // idem, sans sortir de [lo, hi]

std::string scaleName(int type, Lang lang);
std::string pitchClassName(int pitchClass, bool french);  // "C#" ou "Do#"

// Note -> position de potard (inverse de noteFromPot / noteOffsetFromPot). -1 si hors plage.
// avoidPause : pour PA / Pr, le potard à fond à droite (127) est une pause : on reste en dessous.
int potForNote(int octaves, int note, bool avoidPause);
int potForOffset(int octaves, int offset, bool avoidPause);

// Verrouillage : ramène un potard de note absolue sur la note de la gamme la plus proche.
int snapPot(const EventConfig& e, int pot, int root, int type);

// Apprentissage : potard à enregistrer pour une note jouée au clavier, -1 si la rangée n'est pas une rangée de notes.
// reference : première note jouée (rangées de notes relatives), -1 tant qu'elle n'est pas connue.
int learnPot(const EventConfig& e, int note, int& reference, bool lockToScale, int root, int type);

}  // namespace maq
