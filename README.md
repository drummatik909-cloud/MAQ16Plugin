# Plugin MAQ 16/3 pour Bitwig – étape 1

Étape 1 du plan de construction de la V1 : moteur de séquence d'une unité, sortie MIDI, banque de 99 presets, réglage des dossiers d'import et d'export. Référence : la spécification technique (manuel officiel du MAQ16/3).

## État

| Élément | État |
|---|---|
| Moteur d'une unité (3 rangées, 16 pas, horloge 24 par noire) | fait et testé (111 vérifications) |
| Events : notes (nA, nr, PA, Pr), vélocité, CC libre et fixes, Pitch Bend, aftertouch, Program Change | fait et testé |
| Modes : avant, arrière, pendule 1 et 2, aléatoire, One-Shot, OnE | fait et testé |
| First/Last, Prescale g et n, mutes, pauses, Panic | fait et testé |
| Banque de 99 presets en fichier `.maqbank` | fait et testé |
| Dossiers d'import/export, option « garder le même dossier », mémorisation | fait et testé (logique) |
| Squelette du plugin JUCE/CLAP (horloge de Bitwig vers MIDI) | écrit, **non compilé** |
| Sélecteur de dossier à l'écran, façade, interface | étape 2 |
| Cn1, Ad1, Poly-AT, durée de pas (t.4 à t.32), modes déclenchés par notes, chaînage Onr | à faire |
| Test de lecture de la tonalité de Bitwig, glisser-déposer MIDI | à faire (fin d'étape 1 / étape 2) |

## Dossiers d'import et d'export

`src/presets/FolderSettings` gère cinq usages : import de presets, export de presets, sauvegarde de banque, sauvegarde SysEx, export MIDI.

- Option **« Garder le même dossier »** (activée par défaut) : un seul dossier pour tous les usages, le choisir pour l'un le change pour tous.
- Option désactivée : chaque usage mémorise son dossier.
- Le choix est conservé entre les sessions dans `%APPDATA%\MAQ16Plugin\settings.ini` (Windows) ou `~/Library/Application Support/MAQ16Plugin/settings.ini` (macOS).
- Dossier disparu : retour au dossier par défaut `Documents/MAQ16 Presets`, avec `usingFallback()` pour prévenir l'utilisateur.
- Le sélecteur de fichiers (JUCE FileChooser) sera branché sur `folderFor()` et `setFolder()` à l'étape 2.

## Compiler et tester le moteur

Sans CMake : `g++ -std=c++17 -I src src/engine/Unit.cpp src/presets/Bank.cpp src/presets/FolderSettings.cpp tests/test_main.cpp -o maq_tests`

Avec CMake (Windows, PowerShell) :

```
cmake -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## Compiler le plugin (non testé)

Nécessite un accès réseau (téléchargement de JUCE et de clap-juce-extensions) :

```
cmake -B build-plugin -DMAQ_BUILD_PLUGIN=ON
cmake --build build-plugin --config Release
```

Le CMake produit un VST3 et un CLAP (note effect). Si la configuration échoue, envoie-moi le message d'erreur : je corrigerai les noms d'options.

## Hypothèses à valider sur la vraie MAQ

- Potard stocké de 0 à 127 ; note = 36 + arrondi(potard × 12 × octaves / 127).
- Pause PA/Pr : potard à 127 exactement.
- Plateau central du Pitch Bend : ±3 autour de 64.
- Notes relatives : référence 36 quand aucune note n'est reçue.
- Rangées de vélocité jouées avant les rangées de notes à la même horloge.
- Pendule 1 : 1 2 3 4 4 3 2 1 1 2… ; pendule 2 : 1 2 3 4 3 2 1 2…
