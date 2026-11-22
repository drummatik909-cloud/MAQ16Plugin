# Plugin MAQ 16/3 – lot 5c (aide des unités et du chaînage)

- **Aide au survol** : en passant la souris sur les onglets A à D, « Actif », « Canaux », le chaînage, « Retour à A », « Liens... » ou « Aide unités », la ligne d'aide du bas explique le réglage.
- **Bouton « Aide unités »** : ouvre le guide complet (unités, démarrage, chaînage par rangée et par unité, retour à A, choix des unités, canaux MIDI, liens) avec trois exemples. Français et anglais.
- **Conseils orange** (ligne MIDI, à droite) : « active au moins deux unités » quand le chaînage est choisi avec une seule unité active, et « la rangée N de l'unité X n'est pas en OnS ou Onr » quand une rangée ne peut pas passer le relais.
- **Collisions de canal** : ne signalent plus que les unités qui peuvent jouer en même temps. Des unités enchaînées ne comptent pas (par rangée : seules deux rangées de numéros différents peuvent se chevaucher).

Fichiers modifiés : `plugin/Facade.h`, `plugin/Facade.cpp`, `src/engine/Project.h/.cpp`, `src/ui/Strings.h/.cpp`, `tests/test_main.cpp`, `README.md`. Tests du moteur : 4273 vérifications.

---

# Plugin MAQ 16/3 – étape 2b, lot 5b (4 unités A à D dans le plugin)

**Ce qui change dans Bitwig** (cinquième ligne de la barre, sous l'export MIDI) :
- **Onglets A B C D** : la façade règle l'unité choisie (la lettre s'affiche en rouge en haut à droite de la façade). Les presets (Enregistrer / Rappeler), l'apprentissage au clavier et les menus de la façade s'appliquent à l'unité affichée. Les unités B à D ont leurs propres paramètres (automatisables) ; l'unité A garde ses identifiants d'origine, les anciens projets restent valables.
- **Actif** : active ou coupe l'unité choisie (seule A est active par défaut).
- **Canaux** : par rangée (d'origine), canal forcé (1 à 16) ou décalage de canal (-15 à +15), par unité. Une collision de canal entre deux unités s'affiche en rouge à droite.
- **Chaînage** : aucun, série par rangée (A1 vers B1 vers C1...) ou série par unité (A, puis B...). « Retour à A » : la chaîne repart au début, sinon elle s'arrête après la dernière unité.
- **Liens...** : un lien de **transposition** (la note d'une rangée d'une unité règle les rangées relatives d'une autre) et un lien de **pilotage** (une rangée Prg d'une unité envoie ses Program Change à une autre). Un lien d'une unité vers elle-même est refusé.
- **Export MIDI** : la portée contient maintenant « Toutes les unités » ; les fichiers ont une piste par unité et par rangée (A1, B1...) avec les canaux de sortie, et la boucle exportée tient compte du chaînage et des liens.

Limites de cette version : un seul lien de chaque type (le moteur en gère huit), un seul canal de sortie par unité, les presets restent par unité (pas encore de preset de projet complet).

Seuls changent : `plugin/*` (Facade, PluginProcessor, CMakeLists), `src/engine/*` (Project, Remote, MidiExport, Unit, Types), `src/ui/Strings.*`, `tests/test_main.cpp`, `CMakeLists.txt` (racine), `README.md`. Tests du moteur : 4249 vérifications. La partie JUCE n'est compilée que par GitHub.

---

# Plugin MAQ 16/3 – étape 2b, lot 5a (moteur : 4 unités, chaînage, liens)

**Moteur seulement : rien de nouveau n'apparaît encore dans Bitwig.** Le branchement au plugin (paramètres des unités B à D, onglets A à D, réglages de chaînage et de sortie) viendra au lot 5b. Inutile de déposer ce lot sur GitHub avant.

Nouveaux fichiers du moteur : `src/engine/Project.h/.cpp` (4 unités, lecture en parallèle, chaînage en série, liens, canal de sortie par unité, collisions), `src/engine/Remote.h/.cpp` (table des Program Change de télécommande). `Unit` gagne le vrai mode Onr (chaînage entre rangées), la coupure d'une rangée entière et une note de référence par rangée relative.

Hypothèses à valider sur une vraie MAQ :
- Onr : au départ seule la première rangée Onr tourne ; à la fin de sa passe elle démarre la suivante, la dernière relance la première (séquence de 48 pas avec 3 rangées). Une rangée Onr seule se redéclenche elle-même.
- Chaînage en série par unité entière : les rangées Onr de l'unité s'enchaînent, puis l'unité suivante démarre ; la dernière ne relance pas la première si « retour à A » est désactivé.
- Pilotage entre unités : seuls les Program Change (table de l'Appendix A) sont appliqués ; les contrôleurs 0 à 30 attendent la validation de leurs échelles sur la vraie machine (lot 6). Cn1 et Ad1 restent à émuler.

Tests : 4211 vérifications.

---

# Plugin MAQ 16/3 pour Bitwig – étape 2b, lot 4b (compatibilité Ableton Live et Cubase)

## Lot 4b : déclaration « instrument »

Le plugin se déclare maintenant comme un **instrument** (entrée MIDI, sortie MIDI, sortie audio stéréo silencieuse) au lieu d'un « effet MIDI ». Raison : Ableton Live et Cubase n'ont pas d'emplacement « effet MIDI » pour les plugins tiers ; ils acceptent un instrument dont la sortie MIDI est branchée sur une autre piste. Pour revenir à l'ancienne déclaration : `cmake -DMAQ_AS_INSTRUMENT=OFF`. Seuls `plugin/CMakeLists.txt`, `plugin/PluginProcessor.h` et `plugin/PluginProcessor.cpp` changent.

---

# Lot 4 (export MIDI, capture, suivi de tonalité)

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
