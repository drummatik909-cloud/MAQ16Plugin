# Plugin MAQ 16/3 pour Bitwig – étape 2b, lot 2

Lot 1 validé (noms de notes, presets, banque, dossiers). Ce lot 2 ajoute :

## Nouveautés

- **Aide contextuelle** : en bas de la fenêtre, deux lignes décrivent la fonction choisie (menu, Event, mode, réglage en cours), d'après le manuel du MAQ16/3. Ce sont des reformulations, pas une traduction officielle.
- **Langue FR / EN** : bouton FR/EN dans la barre ; change les boutons, les messages et l'aide ; mémorisé dans preferences.ini (commun à tous les projets). Les inscriptions imprimées sur la photo restent en anglais, comme sur la vraie machine.
- **Bouton Manuel** : ouvre la page officielle du manuel chez Doepfer, ou ta copie locale du PDF (à choisir une fois ; le chemin est mémorisé).
- **Bouton Panic** : coupe immédiatement toutes les notes (Note Off et All Notes Off).
- Nouveaux fichiers : src/ui/Strings.*, src/ui/Preferences.* (testés : 399 vérifications).

## Fichiers à mettre dans GitHub

Glisse les dossiers `plugin`, `src`, `tests` et les fichiers `CMakeLists.txt` (celui de la racine, qui cite les nouveaux fichiers) et `README.md`. Ne touche ni à `assets` ni à `.github`.

## Pas encore là

Apprentissage et gamme (lot 3), capture et export MIDI (lot 4), multi-unités et chaînage (lot 5), télécommande de la vraie MAQ, SysEx, moniteur MIDI (lot 6), automation avancée, morphing, liste de presets, skins, Strict/Étendu (lot 7).

## Tests du moteur

`g++ -std=c++17 -I src src/engine/Unit.cpp src/engine/Events.cpp src/ui/Strings.cpp src/ui/Preferences.cpp src/presets/Bank.cpp src/presets/FolderSettings.cpp tests/test_main.cpp -o maq_tests`
