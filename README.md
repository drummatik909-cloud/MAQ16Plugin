# Plugin MAQ 16/3 pour Bitwig – étape 2b, lot 4 (export MIDI, capture, suivi de tonalité)

Lot 3b validé (70 gammes). Ce lot 4 ajoute :

## Export MIDI (nouvelle ligne « MIDI » dans la barre)

- **Source** : « Boucle » (le plugin fait tourner ses réglages actuels sans transport : résultat reproductible, mode aléatoire compris) ou « Capture » (les derniers passages réellement joués, y compris tes manipulations en direct).
- **Longueur** : en boucle, 1 / 2 / 4 / 8 boucles ou cycle complet (PPCM des rangées, plafonné à 64 mesures) ; en capture, 1 à 32 mesures (4/4).
- **Portée** : les 3 rangées ou une seule.
- **Glisser le MIDI** : on attrape la zone bleue et on la glisse sur une piste ou une case de clip de Bitwig ; le fichier .mid est créé dans le dossier temporaire.
- **Exporter .mid** : enregistre le fichier dans le dossier d'export MIDI (mémorisé comme les autres dossiers).
- Le fichier : format 1, 480 ticks par noire, une piste de tempo et une piste par rangée (« Row 1 »…), canaux d'origine, notes avec vélocité et durée, contrôleurs, Pitch Bend, aftertouch et changements de programme. Les notes encore ouvertes sont fermées à la fin ; les Note Off sans Note On (début de fenêtre) sont écartés.

## Suivi de la tonalité par MIDI (bouton « Suivre Bitwig »)

- Activé, le plugin écoute les **notes MIDI du canal 16** : la note reçue donne la **fondamentale** de la gamme (Do = 0 … Si = 11). Le menu de fondamentale est grisé et montre la fondamentale suivie. Le type de gamme et le mode restent ceux du plugin.
- Les notes du canal 16 ne sont ni jouées ni apprises tant que le suivi est actif.
- Pour envoyer ces notes depuis Bitwig 6 (à essayer, non testé ici) : un Note Grid placé avant le plugin, avec le module « Root Key » relié à un « Note Out » réglé sur le canal 16.
- Sans toucher au plugin : un Note Grid placé après lui, avec « Note In » → « Quantize by Scale » → « Note Out », ramène la sortie dans la tonalité et la gamme du projet Bitwig.

## Fichiers à mettre dans GitHub

Dossiers `plugin`, `src`, `tests` + `CMakeLists.txt` (racine, cite Capture.cpp et MidiExport.cpp) + `README.md`. Ne touche ni à `assets` ni à `.github`.

## Tests du moteur (4145 vérifications)

`g++ -std=c++17 -I src src/engine/Unit.cpp src/engine/Events.cpp src/engine/Scale.cpp src/engine/Capture.cpp src/engine/MidiExport.cpp src/ui/Strings.cpp src/ui/Preferences.cpp src/presets/Bank.cpp src/presets/FolderSettings.cpp tests/test_main.cpp -o maq_tests`
