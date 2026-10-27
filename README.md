# Plugin MAQ 16/3 pour Bitwig – étape 2a (façade)

Étape 1 (moteur MIDI d'une unité) validée dans Bitwig. Étape 2a : la façade.

## Ce que contient l'étape 2a

- Fenêtre redimensionnable avec la photo de la façade Dark Edition (assets/facade.png).
- 48 potards (glisser verticalement, molette, double-clic = retour à 0) reliés au moteur et automatisables dans Bitwig.
- 48 LED de pas (pas courant en lecture, rangée choisie en demi-intensité, pas muets éteints).
- Afficheur 3 chiffres à sept segments, 8 boutons de menu avec LED, encodeur DATA (glisser verticalement ou molette).
- Menus : EVENT (PRESET dans EVENT = numéro du contrôleur libre), CHN, FIRST/LAST, PRESCALE, MODE, SINGLE STEP (mute d'un pas avec START/STOP en mode une rangée).
- START/STOP : le transport est piloté par Bitwig ; le bouton ramène à l'affichage du tempo (hors SINGLE STEP). Sa LED indique la lecture.
- État sauvegardé avec le projet Bitwig.

## Pas encore là (étape 2b)

PRESET (99 slots, import/export, choix des dossiers : la logique existe déjà dans src/presets), interrupteur VAL/NOTE, mode apprentissage, gamme, capture, export MIDI par glisser-déposer. Aussi : Cn1, Ad1, Poly-AT, durée de pas, modes par notes, assignation des rangées de vélocité (pour l'instant une rangée de vélocité s'applique à toutes les autres).

## Fichiers à déposer dans GitHub

Dépose le contenu du dossier (src, plugin, tests, assets, CMakeLists.txt, README.md) : les fichiers existants sont remplacés. Ne touche pas à `.github/workflows/build.yml`.

## Tests du moteur

`g++ -std=c++17 -I src src/engine/Unit.cpp src/engine/Events.cpp src/presets/Bank.cpp src/presets/FolderSettings.cpp tests/test_main.cpp -o maq_tests`
