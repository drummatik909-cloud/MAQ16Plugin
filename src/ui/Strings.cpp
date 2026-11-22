#include "Strings.h"
#include <cstddef>
#include "engine/Events.h"

namespace maq {

namespace {

struct Pair {
    const char* fr;
    const char* en;
};

// Dans l'ordre de l'énumération Str
const Pair kStrings[] = {
    {"Notes", "Notes"},                                                  // Notes
    {"Enregistrer", "Store"},                                            // Store
    {"Rappeler", "Recall"},                                              // Recall
    {"Importer...", "Import..."},                                        // Import
    {"Exporter...", "Export..."},                                        // Export
    {"Dossier import", "Import folder"},                                 // ImportDir
    {"Dossier export", "Export folder"},                                 // ExportDir
    {"Garder le même dossier", "Keep the same folder"},                  // KeepSameFolder
    {"Manuel", "Manual"},                                                // Manual
    {"Panic", "Panic"},                                                  // Panic
    {"nom du preset", "preset name"},                                    // PresetName
    {"(slot vide)", "(empty slot)"},                                     // SlotEmpty
    {"Choisis un slot de 1 à 99", "Choose a slot from 1 to 99"},         // ChooseSlot
    {"Preset {n} enregistré", "Preset {n} stored"},                      // PresetStored
    {"Preset {n} rappelé", "Preset {n} recalled"},                       // PresetRecalled
    {"Preset {n} : slot vide", "Preset {n}: empty slot"},                // PresetEmpty
    {"Banque exportée : {file} ({n} presets)", "Bank exported: {file} ({n} presets)"},   // BankExported
    {"Banque importée : {file} ({n} presets)", "Bank imported: {file} ({n} presets)"},   // BankImported
    {"Échec de l'export", "Export failed"},                              // ExportFailed
    {"Échec de l'import : fichier non valide", "Import failed: invalid file"},           // ImportFailed
    {"Dossier : {file}", "Folder: {file}"},                              // FolderSet
    {"Un seul dossier pour tous les usages", "One folder for every use"},                // OneFolder
    {"Un dossier par usage", "One folder per use"},                      // PerUseFolder
    {"Exporter la banque de presets", "Export the preset bank"},         // ExportTitle
    {"Importer une banque de presets", "Import a preset bank"},          // ImportTitle
    {"Dossier d'import des presets", "Preset import folder"},            // ImportDirTitle
    {"Dossier d'export des presets", "Preset export folder"},            // ExportDirTitle
    {"Ouvrir le manuel en ligne (Doepfer)", "Open the manual online (Doepfer)"},         // ManualOnline
    {"Ouvrir ma copie locale (PDF)", "Open my local copy (PDF)"},        // ManualLocal
    {"Choisir ma copie locale...", "Choose my local copy..."},           // ManualChoose
    {"Choisir le manuel du MAQ16/3 (PDF)", "Choose the MAQ16/3 manual (PDF)"},          // ManualChooseTitle
    {"Manuel local enregistré", "Local manual saved"},                   // ManualSaved
    {"Fichier introuvable", "File not found"},                           // ManualMissing
    {"Panic : toutes les notes sont coupées", "Panic: all notes are off"},               // PanicDone
    {"Apprentissage", "Learn"},                                          // Learn
    {"Pause", "Rest"},                                                   // Pause
    {"Saut", "Skip"},                                                    // Skip
    {"Gamme", "Scale"},                                                  // ScaleLabel
    {"Aucune", "Off"},                                                   // ScaleOff
    {"Verrouillage", "Lock"},                                            // ScaleLock
    {"Quantif. en sortie", "Output quantize"},                           // ScaleOutput
    {"Apprentissage : clique un potard, puis joue des notes", "Learn: click a knob, then play notes"},  // LearnOn
    {"Apprentissage terminé", "Learning finished"},                      // LearnOff
    {"MIDI", "MIDI"},                                                    // MidiLabel
    {"Boucle", "Loop"},                                                  // SrcLoop
    {"Capture", "Capture"},                                              // SrcCapture
    {"1 boucle", "1 loop"},                                              // Len1Loop
    {"{n} boucles", "{n} loops"},                                        // LenNLoops
    {"Cycle complet", "Full cycle"},                                     // LenCycle
    {"1 mesure", "1 bar"},                                               // LenBar
    {"{n} mesures", "{n} bars"},                                         // LenBars
    {"3 rangées", "3 rows"},                                             // ScopeAll
    {"Rangée {n}", "Row {n}"},                                           // ScopeRow
    {"Exporter .mid", "Export .mid"},                                    // ExportMidi
    {"Glisser le MIDI", "Drag the MIDI"},                                // DragMidi
    {"MIDI exporté : {file}", "MIDI exported: {file}"},                  // MidiExported
    {"Échec de l'export MIDI", "MIDI export failed"},                    // MidiFailed
    {"Rien à exporter (aucune note : capture vide ou rangées désactivées)", "Nothing to export (no notes: empty capture or rows switched off)"},  // MidiEmpty
    {"Exporter le MIDI", "Export the MIDI"},                             // ExportMidiTitle
    {"Suivre Bitwig", "Follow Bitwig"},                                  // FollowKey
    {"Suivi de la tonalité : le plugin écoute les notes MIDI du canal 16 (la note jouée devient la fondamentale)", "Key following: the plugin listens to MIDI notes on channel 16 (the note played becomes the root)"},  // FollowKeyOn
    {"Suivi de la tonalité désactivé", "Key following off"},             // FollowKeyOff
    {"Unités", "Units"},                                                 // UnitsLabel
    {"Actif", "On"},                                                     // UnitActive
    {"Canaux : par rangée", "Channels: per row"},                        // OutPerRow
    {"Canal forcé", "Forced channel"},                                   // OutForced
    {"Décalage de canal", "Channel offset"},                             // OutOffset
    {"Chaînage : aucun", "Chain: off"},                                  // ChainNone
    {"Série par rangée", "Series by row"},                               // ChainByRow
    {"Série par unité", "Series by unit"},                               // ChainByUnit
    {"Retour à A", "Loop to A"},                                         // ChainLoop
    {"Liens...", "Links..."},                                            // LinksButton
    {"Liens entre unités", "Links between units"},                       // LinksTitle
    {"Transposition : la note d'une rangée devient la référence des rangées relatives (nr, Pr) d'une autre unité. Pilotage : une rangée Prg envoie ses Program Change à une autre unité, comme la télécommande.", "Transposition: a row's note becomes the reference of the relative rows (nr, Pr) of another unit. Control: a Prg row sends its Program Changes to another unit, like the remote control."},  // LinksNote
    {"Désactivé", "Off"},                                                // LinkOff
    {"Activé", "On"},                                                    // LinkOn
    {"Transposition", "Transposition"},                                  // LinkTranspose
    {"Pilotage", "Control"},                                             // LinkControl
    {"source : unité", "source: unit"},                                  // LinkSrcUnit
    {"source : rangée", "source: row"},                                  // LinkSrcRow
    {"cible : unité", "target: unit"},                                   // LinkDstUnit
    {"cible : rangée", "target: row"},                                   // LinkDstRow
    {"toutes les rangées", "all rows"},                                  // LinkAllRows
    {"OK", "OK"},                                                        // LinkOk
    {"Annuler", "Cancel"},                                               // LinkCancel
    {"Lien de pilotage refusé : il refermerait une boucle", "Control link refused: it would close a loop"},  // LinkRefused
    {"Collision de canal {n} : unités {file}", "Channel clash {n}: units {file}"},  // CollisionText
    {"Toutes les unités", "All units"},                                  // ScopeAllUnits
    {"Unités A à D : quatre séquenceurs MAQ complets (3 rangées de 16 pas). Clique sur un onglet pour régler cette unité avec la façade. Seules les unités « Actif » jouent.", "Units A to D: four complete MAQ sequencers (3 rows of 16 steps). Click a tab to edit that unit with the faceplate. Only units marked \"On\" play."},  // HelpUnitTab
    {"Actif : l'unité joue dès que le séquenceur démarre, en même temps que les autres unités actives (jusqu'à 12 rangées). Seule A est active au départ.", "On: the unit plays as soon as the sequencer starts, together with the other active units (up to 12 rows). Only A is on at first."},  // HelpUnitActive
    {"Canaux : « par rangée » garde les canaux réglés dans l'unité ; « forcé » envoie toute l'unité sur un seul canal ; « décalage » ajoute N aux canaux de l'unité (circulaire).", "Channels: \"per row\" keeps the channels set in the unit; \"forced\" sends the whole unit on one channel; \"offset\" adds N to the unit's channels (wrapping around)."},  // HelpUnitChannels
    {"Chaînage : les unités actives jouent l'une après l'autre (A, B, C, D) au lieu de jouer ensemble. Leurs rangées doivent être en OnS ou Onr (un passage), sinon le relais n'est jamais donné. Détails : bouton « Aide unités ».", "Chaining: the active units play one after the other (A, B, C, D) instead of together. Their rows must be in OnS or Onr (one pass), otherwise the handover never happens. Details: \"Units help\" button."},  // HelpChain
    {"Retour à A : à la fin de la dernière unité de la chaîne, la première repart. Sinon la séquence s'arrête.", "Loop to A: at the end of the last unit of the chain, the first one starts again. Otherwise the sequence stops."},  // HelpChainLoop
    {"Liens : transposition (la note d'une rangée règle les rangées relatives nr, Pr d'une autre unité) et pilotage (une rangée Prg commande une autre unité comme la télécommande).", "Links: transposition (a row's note sets the relative rows nr, Pr of another unit) and control (a Prg row drives another unit like the remote control)."},  // HelpLinks
    {"Ouvre le guide complet des unités, du chaînage, des canaux et des liens, avec des exemples.", "Opens the full guide to units, chaining, channels and links, with examples."},  // HelpGuideBtn
    {"Aide unités", "Units help"},  // GuideButton
    {"Unités A à D et chaînage", "Units A to D and chaining"},  // GuideTitle
    {"UNITÉS A À D ET CHAÎNAGE\n\n1. LES QUATRE UNITÉS\nLe plugin contient quatre séquenceurs MAQ complets : A, B, C et D (3 rangées de 16 pas chacun). Les onglets A à D choisissent l'unité que tu règles avec la façade : potards, menus, presets et apprentissage s'appliquent à l'unité affichée (sa lettre rouge est en haut à droite de la façade).\n\n2. QUAND JOUENT-ELLES ?\nSeules les unités « Actif » jouent. Au départ, seule A est active. Quand le transport de Bitwig démarre, toutes les unités actives démarrent en même temps et jouent en parallèle (jusqu'à 12 rangées), chacune sur ses propres canaux MIDI. Sans chaînage, c'est tout.\n\n3. LE CHAÎNAGE EN SÉRIE\nAvec le chaînage, les unités actives ne jouent plus ensemble mais l'une après l'autre, dans l'ordre A, B, C, D. Seule la première démarre ; chacune passe le relais à la suivante quand elle a fini.\n\nATTENTION : une rangée ne « finit » que si elle est en mode OnS (un passage) ou Onr. En Forward, Backward, Pendulum ou Random, elle tourne sans fin et le relais n'est jamais donné. Règle le mode de chaque rangée avec le bouton MODE de la façade. Un message orange te prévient quand c'est le cas.\n\n- Série par rangée : le relais se fait rangée par rangée. La rangée 1 de A joue un passage, puis la rangée 1 de B, etc. Les rangées 2 et 3 font de même, chacune de son côté. Une rangée dont l'événement est coupé (---) ne participe pas.\n- Série par unité : le relais se fait par unité entière. Dans une unité, les rangées en mode Onr s'enchaînent (rangée 1, puis 2, puis 3 : 48 pas) et les rangées en OnS jouent ensemble ; puis l'unité suivante démarre.\n- Retour à A : à la fin de la dernière unité de la chaîne, la première repart (boucle). Sans « Retour à A », la séquence s'arrête.\n\n4. CHOISIR LES UNITÉS ENCHAÎNÉES\nLe chaînage suit l'ordre A, B, C, D et ne prend que les unités « Actif ». Pour enchaîner A avec C seulement, active A et C et laisse B et D coupées. L'ordre ne peut pas être changé (A puis C, jamais C puis A). Il faut au moins deux unités actives.\n\n5. CANAUX MIDI\nLe chaînage ne dépend pas des canaux : chaque unité envoie sur ses propres canaux. Même canal pour A et B : un seul synthé qui continue la phrase. Canaux différents : un autre synthé prend le relais. Le réglage « Canaux » de chaque unité permet de garder les canaux réglés dans l'unité (par rangée), de tout envoyer sur un seul canal (forcé) ou de décaler tous les canaux de N (décalage). Le message rouge « Collision de canal » ne signale que les unités qui peuvent jouer en même temps sur le même canal : des unités enchaînées ne comptent pas.\n\n6. LIENS\n- Transposition : la note jouée par une rangée d'une unité devient la note de référence des rangées relatives (nr, Pr) d'une autre unité.\n- Pilotage : une rangée en mode Prg (Program Change) d'une unité commande une autre unité comme la télécommande de la vraie MAQ (choix de rangée, mode, premier ou dernier pas...).\n\nEXEMPLE 1 – Une phrase de 32 pas : active A et B, mets la rangée 1 de A et de B en OnS (16 pas chacune), chaînage « Série par rangée », « Retour à A » : A1, puis B1, en boucle.\nEXEMPLE 2 – Deux synthés qui se répondent : A sur le canal 1, B sur le canal 2, rangées en OnS, chaînage en série : le premier synthé joue sa phrase, le second lui répond.\nEXEMPLE 3 – Deux séquences en même temps : active A et B, chaînage « aucun » : les deux jouent ensemble.\n", "UNITS A TO D AND CHAINING\n\n1. THE FOUR UNITS\nThe plugin holds four complete MAQ sequencers: A, B, C and D (3 rows of 16 steps each). The A to D tabs choose the unit you edit with the faceplate: knobs, menus, presets and learn mode apply to the displayed unit (its red letter is at the top right of the faceplate).\n\n2. WHEN DO THEY PLAY?\nOnly the units marked \"On\" play. At first only A is on. When Bitwig's transport starts, all active units start together and play in parallel (up to 12 rows), each on its own MIDI channels. Without chaining, that is all.\n\n3. SERIES CHAINING\nWith chaining, the active units no longer play together but one after the other, in the order A, B, C, D. Only the first one starts; each hands over to the next when it has finished.\n\nWARNING: a row only \"finishes\" in OnS (one pass) or Onr mode. In Forward, Backward, Pendulum or Random it runs forever and the handover never happens. Set each row's mode with the MODE button of the faceplate. An orange message warns you when this is the case.\n\n- Series by row: the handover happens row by row. Row 1 of A plays one pass, then row 1 of B, and so on. Rows 2 and 3 do the same, each on its own. A row whose event is off (---) does not take part.\n- Series by unit: the handover happens by whole unit. Inside a unit, rows in Onr mode follow one another (row 1, then 2, then 3: 48 steps) and rows in OnS play together; then the next unit starts.\n- Loop to A: at the end of the last unit of the chain, the first one starts again (loop). Without \"Loop to A\" the sequence stops.\n\n4. CHOOSING THE CHAINED UNITS\nChaining follows the order A, B, C, D and only takes the units marked \"On\". To chain A with C only, switch A and C on and leave B and D off. The order cannot be changed (A then C, never C then A). At least two units must be on.\n\n5. MIDI CHANNELS\nChaining does not depend on channels: each unit sends on its own channels. Same channel for A and B: one synth that carries on the phrase. Different channels: another synth takes over. Each unit's \"Channels\" setting keeps the channels set in the unit (per row), sends everything on one channel (forced) or shifts all channels by N (offset). The red \"Channel clash\" message only reports units that can play at the same time on the same channel: chained units do not count.\n\n6. LINKS\n- Transposition: the note played by a row of one unit becomes the reference note of the relative rows (nr, Pr) of another unit.\n- Control: a row in Prg (Program Change) mode of one unit drives another unit like the real MAQ's remote control (row choice, mode, first or last step...).\n\nEXAMPLE 1 - A 32-step phrase: switch A and B on, set row 1 of A and B to OnS (16 steps each), chaining \"Series by row\", \"Loop to A\": A1, then B1, in a loop.\nEXAMPLE 2 - Two synths answering each other: A on channel 1, B on channel 2, rows in OnS, series chaining: the first synth plays its phrase, the second answers.\nEXAMPLE 3 - Two sequences at once: switch A and B on, chaining \"off\": both play together.\n"},  // UnitsGuide
    {"Chaînage : la rangée {n} de l'unité {file} n'est pas en OnS ou Onr, le relais ne sera jamais donné", "Chaining: row {n} of unit {file} is not in OnS or Onr, so the handover will never happen"},  // ChainHintMode
    {"Chaînage : active au moins deux unités pour qu'elles s'enchaînent", "Chaining: switch on at least two units for them to follow one another"},  // ChainHintUnits
};
static_assert(sizeof(kStrings) / sizeof(kStrings[0]) == static_cast<std::size_t>(Str::Count),
              "la table de textes doit suivre l'énumération Str");

std::string replaceAll(std::string s, const std::string& from, const std::string& to) {
    std::size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

std::string pick(Lang l, const std::string& fr, const std::string& en) { return l == Lang::French ? fr : en; }

std::string eventHelp(int index, Lang l) {
    const EventConfig e = eventForIndex(index);
    const bool f = (l == Lang::French);
    const std::string label = eventLabel(index);
    const std::string k = std::to_string(e.octaves);
    const std::string top = std::to_string(36 + 12 * e.octaves);
    const std::string hi = std::to_string(12 * e.octaves);

    switch (e.kind) {
        case EventKind::Off:
            return pick(l, label + " : rangée désactivée, elle n'envoie rien.",
                           label + ": row switched off, it sends nothing.");
        case EventKind::FreeCC:
            return pick(l, "Contrôleur libre : le potard envoie un Control Change (0 à 127). Appuie sur PRESET pour régler le numéro du contrôleur avec DATA.",
                           "Free controller: the knob sends a Control Change (0 to 127). Press PRESET to set the controller number with DATA.");
        case EventKind::NoteAbs:
            return pick(l, label + " – Note absolue, " + k + " octave(s) : le potard parcourt les notes de " + noteName(36, f) + " à " +
                           noteName(36 + 12 * e.octaves, f) + " (MIDI 36 à " + top + "). Elles ne dépendent pas des notes reçues.",
                           label + " – Absolute note, " + k + " octave(s): the knob covers " + noteName(36, f) + " to " +
                           noteName(36 + 12 * e.octaves, f) + " (MIDI 36 to " + top + "). They do not depend on incoming notes.");
        case EventKind::NoteRel:
            return pick(l, label + " – Note relative, " + k + " octave(s) : le potard donne un décalage de 0 à " + hi +
                           " demi-tons, ajouté aux notes reçues sur le canal de la rangée (transposition).",
                           label + " – Relative note, " + k + " octave(s): the knob gives an offset of 0 to " + hi +
                           " semitones, added to the notes received on the row's channel (transposition).");
        case EventKind::NoteAbsPause:
            return pick(l, label + " – Note absolue avec pause, " + k + " octave(s) : comme nA, mais le potard à fond à droite coupe le pas (pause).",
                           label + " – Absolute note with pause, " + k + " octave(s): like nA, but the knob fully right mutes the step (pause).");
        case EventKind::NoteRelPause:
            return pick(l, label + " – Note relative avec pause, " + k + " octave(s) : comme nr, mais le potard à fond à droite coupe le pas (pause).",
                           label + " – Relative note with pause, " + k + " octave(s): like nr, but the knob fully right mutes the step (pause).");
        case EventKind::Velocity:
            return pick(l, "dYn – Vélocité : cette rangée fixe la force des notes des autres rangées. Potard à fond à gauche = pas muet (pause).",
                           "dYn – Velocity: this row sets the strength of the notes of the other rows. Knob fully left = muted step (pause).");
        case EventKind::FixedCC: {
            const std::string n = std::to_string(fixedCcNumber(e.ccNumber));
            return pick(l, label + " – Contrôleur fixe n°" + n + " : le potard envoie la valeur 0 à 127 du Control Change " + n + ".",
                           label + " – Fixed controller no. " + n + ": the knob sends the value 0 to 127 of Control Change " + n + ".");
        }
        case EventKind::PitchBend:
            return pick(l, "PIt – Pitch Bend complet (0 à 127) : un plateau au centre aide à retrouver la hauteur neutre.",
                           "PIt – Full Pitch Bend (0 to 127): a plateau around the centre makes the neutral pitch easy to find.");
        case EventKind::PitchBendPos:
            return pick(l, "PIP – Pitch Bend positif seulement (64 à 127).", "PIP – Positive Pitch Bend only (64 to 127).");
        case EventKind::PitchBendNeg:
            return pick(l, "PI- – Pitch Bend négatif seulement (0 à 64).", "PI- – Negative Pitch Bend only (0 to 64).");
        case EventKind::AfterTouch:
            return pick(l, "At – Aftertouch monophonique : pression de 0 à 127 sur le canal de la rangée.",
                           "At – Monophonic aftertouch: pressure from 0 to 127 on the row's channel.");
        case EventKind::ProgramChange:
            return pick(l, "Prg – Program Change : change de son à chaque pas (le synthétiseur doit le permettre sans retard).",
                           "Prg – Program Change: switches sound at every step (the synthesizer must allow it without delay).");
    }
    return std::string();
}

std::string modeHelp(int mode, Lang l) {
    switch (mode) {
        case 0: return pick(l, "FOr – Avant : de First à Last, puis la rangée recommence.",
                               "FOr – Forward: from First to Last, then the row starts again.");
        case 1: return pick(l, "bAC – Arrière : de Last à First, puis la rangée recommence.",
                               "bAC – Backward: from Last to First, then the row starts again.");
        case 2: return pick(l, "PE1 – Pendule 1 : va-et-vient ; le premier et le dernier pas sont joués deux fois.",
                               "PE1 – Pendulum 1: back and forth; the first and the last step are played twice.");
        case 3: return pick(l, "rnd – Aléatoire : un pas au hasard entre First et Last à chaque pas.",
                               "rnd – Random: a random step between First and Last at every step.");
        case 4: return pick(l, "PE2 – Pendule 2 : va-et-vient ; le premier et le dernier pas sont joués une seule fois (longueur effective 15 pour 16 pas).",
                               "PE2 – Pendulum 2: back and forth; the first and the last step are played only once (effective length 15 for 16 steps).");
        case 5: return pick(l, "OnS – Un passage : la rangée s'arrête au dernier pas.",
                               "OnS – One shot: the row stops at the last step.");
        case 6: return pick(l, "Onr – Un passage avec relance : à la fin, elle doit lancer la rangée suivante (ce chaînage n'est pas encore disponible ici : elle s'arrête comme OnS).",
                               "Onr – One shot with retrigger: at the end it should start the next row (this chaining is not available here yet: it stops like OnS).");
        default: return pick(l, "OnE – Un seul pas : seul le premier pas est joué.",
                                "OnE – One event: only the first step is played.");
    }
}

}  // namespace

std::string tr(Str s, Lang lang) {
    const auto i = static_cast<std::size_t>(s);
    if (i >= static_cast<std::size_t>(Str::Count)) return std::string();
    return lang == Lang::French ? kStrings[i].fr : kStrings[i].en;
}

std::string fmt(Str s, Lang lang, const std::string& file, int n) {
    return replaceAll(replaceAll(tr(s, lang), "{file}", file), "{n}", std::to_string(n));
}

Lang langFromCode(const std::string& code) { return code == "en" ? Lang::English : Lang::French; }
const char* langCode(Lang lang) { return lang == Lang::English ? "en" : "fr"; }

std::string helpText(const HelpContext& c, Lang l) {
    if (c.learn && c.menu == HelpMenu::None)
        return pick(l, "Apprentissage : clique sur un potard pour placer le curseur (LED vive), puis joue des notes au clavier. Chaque note règle le pas et le curseur avance. Pause et Saut passent un pas.",
                       "Learn: click a knob to place the cursor (bright LED), then play notes on your keyboard. Each note sets the step and the cursor moves on. Rest and Skip pass over a step.");
    switch (c.menu) {
        case HelpMenu::None:
            return pick(l, "Tempo : le séquenceur suit l'horloge de Bitwig (24 impulsions par noire). Choisis un menu avec les 8 boutons ; l'encodeur DATA règle la valeur.",
                           "Tempo: the sequencer follows Bitwig's clock (24 pulses per quarter note). Pick a menu with the 8 buttons; the DATA encoder sets the value.");
        case HelpMenu::Event:
            return c.ccEdit ? eventHelp(maq::kFreeCcEventIndex, l) : eventHelp(c.eventIndex, l);
        case HelpMenu::Channel:
            return pick(l, "Canal MIDI de la rangée (1 à 16). Rappuie sur CHN pour passer à la rangée suivante. Sans effet sur une rangée de vélocité.",
                           "MIDI channel of the row (1 to 16). Press CHN again to move to the next row. No effect on a velocity row.");
        case HelpMenu::FirstLast:
            return c.second
                ? pick(l, "Dernier pas (L) de la rangée : après lui, la lecture revient au premier pas. Des longueurs différentes d'une rangée à l'autre créent des motifs qui se décalent.",
                          "Last step (L) of the row: after it, playback goes back to the first step. Different lengths from row to row create patterns that shift against each other.")
                : pick(l, "Premier pas (F) de la rangée : la lecture commence ici. Rappuie sur FIRST/LAST pour passer aux autres rangées, puis aux derniers pas (L).",
                          "First step (F) of the row: playback starts here. Press FIRST/LAST again to go through the other rows, then the last steps (L).");
        case HelpMenu::Prescale:
            return c.second
                ? pick(l, "Durée de note n : temps entre Note On et Note Off, toujours inférieur ou égal à g. 1 = égal à g.",
                          "Note length n: time between Note On and Note Off, always less than or equal to g. 1 = equal to g.")
                : pick(l, "Gate g : nombre d'impulsions d'horloge entre deux pas (24 = noire, 12 = croche, 6 = double croche). Rappuie sur PRESCALE pour changer de rangée, puis régler n.",
                          "Gate g: number of clock pulses between two steps (24 = quarter note, 12 = eighth, 6 = sixteenth). Press PRESCALE again to change row, then set n.");
        case HelpMenu::Mode:
            return modeHelp(c.modeIndex, l);
        case HelpMenu::SingleStep:
            return c.singleRow
                ? pick(l, "Single Step, une rangée : DATA choisit le pas et l'afficheur montre sa valeur. START/STOP coupe ou réactive le pas (LED éteinte = muet). Rappuie pour la rangée suivante.",
                          "Single Step, one row: DATA picks the step and the display shows its value. START/STOP mutes or unmutes the step (LED off = muted). Press again for the next row.")
                : pick(l, "Single Step : DATA choisit le pas (1 à 16), sélectionné dans les 3 rangées. Rappuie sur SINGLE STEP pour agir sur une seule rangée.",
                          "Single Step: DATA picks the step (1 to 16), selected in all 3 rows. Press SINGLE STEP again to work on a single row.");
        case HelpMenu::Preset:
            return c.store
                ? pick(l, "Enregistrement : DATA choisit le slot (1 à 99), START/STOP (Enter) écrase ce slot avec les réglages actuels. Un autre bouton de menu annule.",
                          "Store: DATA picks the slot (1 to 99), START/STOP (Enter) overwrites that slot with the current settings. Another menu button cancels.")
                : pick(l, "Presets : DATA choisit le slot (1 à 99, un point = déjà enregistré), START/STOP (Enter) le rappelle. Rappuie sur PRESET pour passer en enregistrement.",
                          "Presets: DATA picks the slot (1 to 99, a dot = already stored), START/STOP (Enter) recalls it. Press PRESET again to switch to storing.");
    }
    return std::string();
}

}  // namespace maq
