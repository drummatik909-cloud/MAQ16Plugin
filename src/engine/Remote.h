#pragma once
// Commandes de télécommande (Appendix A du manuel) appliquées à une unité logicielle.
#include "Unit.h"

namespace maq {

// Program Change reçu (valeur = numéro du manuel - 1). Les numéros 0 à 2 choisissent la rangée courante
// (selectedRow) ; les autres s'appliquent à cette rangée. Retourne false si la valeur n'est pas prise en charge
// (valeurs hors tableau, Cn1 et Ad1 pas encore émulés).
bool applyProgramChange(Unit& unit, int& selectedRow, int value);

}  // namespace maq
