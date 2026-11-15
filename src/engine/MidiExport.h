#pragma once
// Rendu d'une boucle et écriture d'un fichier MIDI standard (format 1, 480 ticks par noire).
#include <array>
#include <cstdint>
#include <vector>
#include "Capture.h"
#include "Types.h"
#include "Project.h"
#include "Unit.h"

namespace maq {

constexpr int kClocksPerBar = 96;  // 24 horloges par noire, mesure à 4 temps

// Durée d'un passage complet d'une rangée, en horloges (0 si la rangée est désactivée)
std::int64_t rowPeriodClocks(const RowConfig& row);
// La plus longue rangée active (une mesure si aucune rangée n'est active)
std::int64_t loopLengthClocks(const std::array<RowConfig, kRows>& rows);
// Cycle complet avant que toutes les rangées se retrouvent au départ ensemble (PPCM), plafonné
std::int64_t cycleLengthClocks(const std::array<RowConfig, kRows>& rows, std::int64_t capClocks);

// Fait tourner une copie de l'unité pendant « clocks » horloges, sans transport. Résultat reproductible.
std::vector<CapturedEvent> renderLoop(Unit unit, std::int64_t clocks);

// Durées d'un projet à plusieurs unités (en horloges). Sans chaînage : la plus longue rangée des unités actives ;
// en série par rangée : la plus longue somme d'une rangée sur la chaîne ; en série par unité : somme des unités.
std::int64_t projectLoopClocks(const Project& project);
std::int64_t projectCycleClocks(const Project& project, std::int64_t capClocks);

// Fait tourner une copie du projet (unités actives, chaînage, liens, canaux de sortie) pendant « clocks » horloges
std::vector<CapturedEvent> renderProject(Project project, std::int64_t clocks);

// Comme ci-dessous, pour plusieurs unités : une piste par unité du masque (bit 0 = A...) et par rangée du masque.
// Piste nommée « Row N » si seule l'unité A est exportée, sinon « A1 », « B2 »...
std::vector<std::uint8_t> buildMidiFile(const std::vector<CapturedEvent>& events, double bpm, int rowMask, int unitMask,
                                        std::int64_t lengthClocks);

// Piste 0 : tempo et mesure ; pistes suivantes : une par rangée du masque (bit 0 = rangée 1...), unité A.
// Les messages sans rangée sont ignorés, les Note Off sans Note On sont écartés, les notes encore
// ouvertes sont fermées à la fin (lengthClocks).
std::vector<std::uint8_t> buildMidiFile(const std::vector<CapturedEvent>& events, double bpm, int rowMask,
                                        std::int64_t lengthClocks);

}  // namespace maq
