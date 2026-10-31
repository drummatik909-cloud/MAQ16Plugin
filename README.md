# Plugin MAQ 16/3 pour Bitwig – étape 2b, lot 1

Étape 2a validée dans Bitwig (façade, potards, LED, afficheur, menus). Ce lot ajoute :

## Nouveautés

- **Noms de notes** : bouton « Notes » dans la barre sous la façade. Activé, la bulle de valeur des potards et l'afficheur montrent la note (C#3, Do#3…) au lieu du chiffre ; « Do Ré Mi » bascule entre noms français et anglais. Sur l'afficheur à sept segments, la lettre est C D E F G A b, le point décimal indique un dièse, puis le signe et le chiffre de l'octave. Convention de Bitwig : 60 = 3.
- **Valeur d'un potard** : tourner un potard affiche sa valeur sur l'afficheur pendant 1,5 s (menu non sélectionné), et dans une bulle pendant le glissement. Pause (PA, Pr à fond à droite) : « --- ».
- **Menu PRESET** (99 slots) :
  - bouton PRESET : l'afficheur montre le numéro du slot (un point = slot déjà enregistré), LED 7 et 8 allumées ; DATA choisit le slot.
  - START/STOP (= Enter) : rappelle le slot (« rEC »).
  - second appui sur PRESET : les 8 LED s'allument (enregistrement) ; START/STOP enregistre (« StO »).
- **Barre de contrôle** : choix du slot (< >), nom du preset, Enregistrer, Rappeler, Importer / Exporter une banque `.maqbank`, choix du dossier d'import et du dossier d'export, case « Garder le même dossier ».
- La banque (99 slots) est sauvegardée avec le projet Bitwig. Le dossier choisi est mémorisé entre les sessions.
- Nouveaux paramètres automatisables : « Preset slot », « Show note names », « Note names in French ».

## Pas encore là

Couronne de notes autour des potards (la bulle et l'afficheur la remplacent pour l'instant), mode apprentissage, gamme, capture, export MIDI par glisser-déposer. Aussi : Cn1, Ad1, Poly-AT, durée de pas, modes par notes, assignation des rangées de vélocité.

## Fichiers modifiés par rapport à l'étape 2a

plugin/Facade.cpp, plugin/Facade.h, plugin/PluginProcessor.cpp, plugin/PluginProcessor.h, src/engine/Events.cpp, src/engine/Events.h, src/presets/Bank.cpp, src/presets/Bank.h, src/presets/FolderSettings.cpp, src/presets/FolderSettings.h, tests/test_main.cpp, README.md.

## Tests du moteur (271 vérifications)

`g++ -std=c++17 -I src src/engine/Unit.cpp src/engine/Events.cpp src/presets/Bank.cpp src/presets/FolderSettings.cpp tests/test_main.cpp -o maq_tests`
