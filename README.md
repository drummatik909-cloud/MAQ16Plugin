# Plugin MAQ 16/3 pour Bitwig – étape 2b, lot 3

Lot 2 validé (aide, langue FR/EN, manuel, Panic). Ce lot 3 ajoute :

## Gamme

- Barre, ligne 3 : fondamentale (Do…Si, noms français ou anglais selon « Do Ré Mi »), type de gamme (chromatique, majeure, mineures naturelle / harmonique / mélodique, dorien, phrygien, lydien, mixolydien, locrien, pentatoniques, blues, ton entier, diminuée) et mode.
- Mode **Verrouillage** : en tournant un potard de note absolue (nA, PA), il ne s'arrête que sur les notes de la gamme ; les notes apprises au clavier sont ramenées dans la gamme.
- Mode **Quantif. en sortie** : les valeurs des potards ne changent pas, mais chaque note envoyée est ramenée dans la gamme (le Pitch Bend n'est pas touché).
- Fondamentale, type et mode sont des paramètres automatisables dans Bitwig.

## Apprentissage au clavier

- Bouton **Apprentissage** : l'afficheur montre « LEA ». Clique un potard pour placer le curseur (LED vive), puis joue des notes : chaque note règle le potard du pas, le curseur avance de First à Last puis revient au début.
- Rangées de notes absolues : une note hors plage est repliée dans la plage (même note, autre octave). Rangées de notes relatives : la première note jouée sert de référence, les suivantes sont des intervalles.
- Une rangée de vélocité (dYn) enregistre la force de frappe sur le même pas.
- Boutons **Pause** (potard à fond à droite pour PA/Pr, sinon pas muet) et **Saut** (passe un pas sans le modifier). Une note jouée sur un pas muet le réactive.
- Pour que les notes du clavier arrivent au plugin dans Bitwig, la piste doit recevoir ton clavier (entrée « All ins » ou ton clavier, piste armée).

## Pas encore là

Gamme libre (cliquer les notes), suivi automatique de la tonalité de Bitwig, capture et export MIDI (lot 4), multi-unités et chaînage (lot 5), télécommande de la vraie MAQ, SysEx, moniteur MIDI (lot 6), morphing, liste de presets, skins, Strict/Étendu (lot 7).

## Fichiers à mettre dans GitHub

Dossiers `plugin`, `src`, `tests`, plus `CMakeLists.txt` (celui de la racine, qui cite Scale.cpp) et `README.md`. Ne touche ni à `assets` ni à `.github`.

## Tests du moteur (1392 vérifications)

`g++ -std=c++17 -I src src/engine/Unit.cpp src/engine/Events.cpp src/engine/Scale.cpp src/ui/Strings.cpp src/ui/Preferences.cpp src/presets/Bank.cpp src/presets/FolderSettings.cpp tests/test_main.cpp -o maq_tests`
