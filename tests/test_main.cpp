// Tests du moteur (sans dépendance externe).
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>
#include "engine/Unit.h"
#include "engine/Capture.h"
#include "engine/Events.h"
#include "engine/MidiExport.h"
#include "engine/Project.h"
#include "engine/Remote.h"
#include "engine/Scale.h"
#include "presets/Bank.h"
#include "presets/FolderSettings.h"
#include "ui/Preferences.h"
#include "ui/Strings.h"

using namespace maq;
namespace fs = std::filesystem;

static int g_fail = 0, g_checks = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) { ++g_fail; std::printf("ECHEC %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
    } while (0)

static fs::path tempDir(const char* name);

struct Ev { int tick; MidiMsg m; };

static Unit makeUnit() {
    Unit u;
    u.rows[1].event.kind = EventKind::Off;
    u.rows[2].event.kind = EventKind::Off;
    u.rows[0].channel = 1;
    return u;
}

static std::vector<Ev> run(Unit& u, int ticks, bool doStart = true) {
    std::vector<Ev> all;
    if (doStart) u.start();
    for (int t = 0; t < ticks; ++t) {
        std::vector<MidiMsg> out;
        u.tick(out);
        for (auto& m : out) all.push_back({t, m});
    }
    return all;
}

// Avec g = 1 chaque horloge joue un pas : on relève le pas courant à chaque horloge.
static std::vector<int> steps(Unit& u, int ticks) {
    std::vector<int> s;
    u.start();
    for (int t = 0; t < ticks; ++t) {
        std::vector<MidiMsg> out;
        u.tick(out);
        s.push_back(u.currentStep(0));
    }
    return s;
}

static std::vector<int> noteOns(const std::vector<Ev>& evs) {
    std::vector<int> n;
    for (auto& e : evs) if (e.m.type == MidiMsg::Type::NoteOn) n.push_back(e.m.data1);
    return n;
}

static void testValueMapping() {
    CHECK(noteFromPot(1, 0) == 36);
    CHECK(noteFromPot(1, 127) == 48);
    CHECK(noteFromPot(2, 127) == 60);
    CHECK(noteFromPot(5, 127) == 96);
    CHECK(fixedCcNumber(0) == 0);
    CHECK(fixedCcNumber(19) == 19);
    CHECK(fixedCcNumber(20) == 64);
    CHECK(fixedCcNumber(30) == 74);
    CHECK(fixedCcNumber(31) == 84);
    CHECK(pitchBendFromPot(EventKind::PitchBend, 64) == 64);
    CHECK(pitchBendFromPot(EventKind::PitchBend, 66) == 64);  // plateau
    CHECK(pitchBendFromPot(EventKind::PitchBend, 0) == 0);
    CHECK(pitchBendFromPot(EventKind::PitchBend, 127) == 127);
    CHECK(pitchBendFromPot(EventKind::PitchBendPos, 0) == 64);
    CHECK(pitchBendFromPot(EventKind::PitchBendPos, 127) == 127);
    CHECK(pitchBendFromPot(EventKind::PitchBendNeg, 0) == 0);
    CHECK(pitchBendFromPot(EventKind::PitchBendNeg, 127) == 64);
}

static void testForwardNotes() {
    Unit u = makeUnit();
    for (int i = 0; i < 16; ++i) u.rows[0].pot[i] = i * 8;
    auto evs = run(u, 16 * 6);
    auto on = noteOns(evs);
    CHECK(on.size() == 16);
    for (int i = 0; i < 16 && i < static_cast<int>(on.size()); ++i) CHECK(on[i] == noteFromPot(1, i * 8));
    // un NoteOn toutes les 6 horloges
    int k = 0;
    for (auto& e : evs) if (e.m.type == MidiMsg::Type::NoteOn) { CHECK(e.tick == k * 6); ++k; }
}

static void testNoteLength() {
    {   // n = 3, g = 6
        Unit u = makeUnit();
        u.rows[0].noteLen = 3;
        auto evs = run(u, 7);
        int offTick = -1;
        for (auto& e : evs) if (e.m.type == MidiMsg::Type::NoteOff) { offTick = e.tick; break; }
        CHECK(offTick == 3);
    }
    {   // n = 1 : égal à g, le Note Off précède le Note On suivant au même instant
        Unit u = makeUnit();
        auto evs = run(u, 7);
        int offIdx = -1, secondOnIdx = -1, ons = 0;
        for (size_t i = 0; i < evs.size(); ++i) {
            if (evs[i].m.type == MidiMsg::Type::NoteOff && offIdx < 0) offIdx = static_cast<int>(i);
            if (evs[i].m.type == MidiMsg::Type::NoteOn && ++ons == 2) secondOnIdx = static_cast<int>(i);
        }
        CHECK(offIdx >= 0 && secondOnIdx >= 0 && offIdx < secondOnIdx);
        CHECK(evs[offIdx].tick == 6 && evs[secondOnIdx].tick == 6);
    }
}

static void testModes() {
    auto make = [](PlayMode m) {
        Unit u = makeUnit();
        u.rows[0].gate = 1;
        u.rows[0].first = 1; u.rows[0].last = 4;
        u.rows[0].mode = m;
        return u;
    };
    { Unit u = make(PlayMode::Forward);   CHECK((steps(u, 8)  == std::vector<int>{1,2,3,4,1,2,3,4})); }
    { Unit u = make(PlayMode::Backward);  CHECK((steps(u, 8)  == std::vector<int>{4,3,2,1,4,3,2,1})); }
    { Unit u = make(PlayMode::Pendulum1); CHECK((steps(u, 10) == std::vector<int>{1,2,3,4,4,3,2,1,1,2})); }
    { Unit u = make(PlayMode::Pendulum2); CHECK((steps(u, 10) == std::vector<int>{1,2,3,4,3,2,1,2,3,4})); }
    { Unit u = make(PlayMode::OneEvent);  CHECK((steps(u, 3)  == std::vector<int>{1,1,1})); }
    {   // One-Shot : s'arrête au dernier pas
        Unit u = make(PlayMode::OneShotSingle);
        auto evs = run(u, 12);
        CHECK(noteOns(evs).size() == 4);
        CHECK(u.currentStep(0) == 4);
    }
    {   // Random : déterministe avec la même graine, reste dans First/Last, utilise plusieurs pas
        Unit a = make(PlayMode::Random); a.seedRandom(42);
        Unit b = make(PlayMode::Random); b.seedRandom(42);
        auto sa = steps(a, 40), sb = steps(b, 40);
        CHECK(sa == sb);
        bool inRange = true; int distinct = 0; bool seen[5] = {false};
        for (int s : sa) { if (s < 1 || s > 4) inRange = false; else if (!seen[s]) { seen[s] = true; ++distinct; } }
        CHECK(inRange);
        CHECK(distinct >= 2);
    }
    {   // First/Last
        Unit u = make(PlayMode::Forward);
        u.rows[0].first = 3; u.rows[0].last = 5;
        CHECK((steps(u, 5) == std::vector<int>{3,4,5,3,4}));
    }
}

static void testPauseVelocityMute() {
    {   // PA : potard à fond à droite = pause
        Unit u = makeUnit();
        u.rows[0].event.kind = EventKind::NoteAbsPause;
        u.rows[0].gate = 1;
        u.rows[0].last = 3;
        u.rows[0].pot[0] = 10; u.rows[0].pot[1] = 127; u.rows[0].pot[2] = 20;
        auto on = noteOns(run(u, 3));
        CHECK(on.size() == 2);
    }
    {   // Vélocité par rangée de longueur différente (3 pas) sur notes de 4 pas
        Unit u = makeUnit();
        u.rows[0].gate = 1; u.rows[0].last = 4;
        u.rows[1].event.kind = EventKind::Velocity;
        u.rows[1].gate = 1; u.rows[1].last = 3;
        u.rows[1].velocityTargets = 1;  // rangée 1
        u.rows[1].pot[0] = 100; u.rows[1].pot[1] = 64; u.rows[1].pot[2] = 0;
        auto evs = run(u, 6);
        std::vector<int> vels;
        std::vector<int> ticks;
        for (auto& e : evs) if (e.m.type == MidiMsg::Type::NoteOn) { vels.push_back(e.m.data2); ticks.push_back(e.tick); }
        CHECK((vels == std::vector<int>{100, 64, 100, 64}));
        CHECK((ticks == std::vector<int>{0, 1, 3, 4}));  // vélocité 0 = pause
    }
    {   // Vélocité par défaut = 100
        Unit u = makeUnit();
        auto evs = run(u, 1);
        CHECK(!evs.empty() && evs[0].m.data2 == 100);
    }
    {   // Mute d'un pas
        Unit u = makeUnit();
        u.rows[0].gate = 1; u.rows[0].last = 3;
        u.rows[0].mute[1] = true;
        CHECK(noteOns(run(u, 3)).size() == 2);
    }
}

static void testOtherEvents() {
    Unit u = makeUnit();
    u.rows[0].event.kind = EventKind::FixedCC; u.rows[0].event.ccNumber = 20; u.rows[0].pot[0] = 77;
    u.rows[0].channel = 5;
    auto evs = run(u, 1);
    CHECK(evs.size() == 1 && evs[0].m.type == MidiMsg::Type::ControlChange);
    CHECK(evs[0].m.channel == 4 && evs[0].m.data1 == 64 && evs[0].m.data2 == 77);

    Unit f = makeUnit();
    f.rows[0].event.kind = EventKind::FreeCC; f.rows[0].event.ccNumber = 100; f.rows[0].pot[0] = 33;
    evs = run(f, 1);
    CHECK(evs.size() == 1 && evs[0].m.data1 == 100 && evs[0].m.data2 == 33);

    Unit p = makeUnit();
    p.rows[0].event.kind = EventKind::PitchBend; p.rows[0].pot[0] = 64;
    evs = run(p, 1);
    CHECK(evs.size() == 1 && evs[0].m.type == MidiMsg::Type::PitchBend && evs[0].m.data1 == 8192);
}

static void testPanicAndStop() {
    Unit u = makeUnit();
    run(u, 1);
    std::vector<MidiMsg> out;
    u.panic(out);
    bool off = false, allOff = false;
    for (auto& m : out) {
        if (m.type == MidiMsg::Type::NoteOff) off = true;
        if (m.type == MidiMsg::Type::ControlChange && m.data1 == 123) allOff = true;
    }
    CHECK(off && allOff);

    Unit v = makeUnit();
    run(v, 1);
    out.clear();
    v.stop(out);
    CHECK(out.size() == 1 && out[0].type == MidiMsg::Type::NoteOff);
    CHECK(!v.isPlaying());
}


static void testEventTable() {
    CHECK(eventForIndex(0).kind == EventKind::Off);
    CHECK(eventForIndex(1).kind == EventKind::FreeCC);
    CHECK(eventForIndex(2).kind == EventKind::NoteAbs && eventForIndex(2).octaves == 1);
    CHECK(eventForIndex(6).kind == EventKind::NoteAbs && eventForIndex(6).octaves == 5);
    CHECK(eventForIndex(7).kind == EventKind::NoteRel && eventForIndex(7).octaves == 1);
    CHECK(eventForIndex(11).kind == EventKind::NoteRel && eventForIndex(11).octaves == 5);
    CHECK(eventForIndex(12).kind == EventKind::Velocity);
    CHECK(eventForIndex(13).kind == EventKind::FixedCC && eventForIndex(13).ccNumber == 0);
    CHECK(eventForIndex(44).kind == EventKind::FixedCC && eventForIndex(44).ccNumber == 31);
    CHECK(eventForIndex(45).kind == EventKind::PitchBend);
    CHECK(eventForIndex(47).kind == EventKind::PitchBendNeg);
    CHECK(eventForIndex(48).kind == EventKind::AfterTouch);
    CHECK(eventForIndex(49).kind == EventKind::ProgramChange);
    CHECK(eventForIndex(50).kind == EventKind::NoteAbsPause && eventForIndex(50).octaves == 1);
    CHECK(eventForIndex(54).kind == EventKind::NoteAbsPause && eventForIndex(54).octaves == 5);
    CHECK(eventForIndex(55).kind == EventKind::NoteRelPause && eventForIndex(55).octaves == 1);
    CHECK(eventForIndex(59).kind == EventKind::NoteRelPause && eventForIndex(59).octaves == 5);
    CHECK(eventLabel(0) == "---");
    CHECK(eventLabel(2) == "nA1" && eventLabel(6) == "nA5");
    CHECK(eventLabel(7) == "nr1" && eventLabel(12) == "dYn");
    CHECK(eventLabel(13) == "C0" && eventLabel(32) == "C19" && eventLabel(33) == "C64" && eventLabel(43) == "C74" && eventLabel(44) == "C84");
    CHECK(eventLabel(45) == "PIt" && eventLabel(47) == "PI-" && eventLabel(49) == "Prg");
    CHECK(eventLabel(50) == "PA1" && eventLabel(59) == "Pr5");
    for (int i = 0; i < kEventCount; ++i) CHECK(eventLabel(i).size() <= 3);
    // la table des modes suit l'ordre de l'énumération du moteur
    CHECK(static_cast<int>(PlayMode::Forward) == 0 && static_cast<int>(PlayMode::OneEvent) == kModeCount - 1);
    CHECK(std::string(modeLabel(0)) == "FOr" && std::string(modeLabel(2)) == "PE1" && std::string(modeLabel(7)) == "OnE");
}


static void testNotesAndPresetHelpers() {
    // noms de notes, convention de Bitwig (60 = 3)
    CHECK(noteName(60, false) == "C3" && noteName(36, false) == "C1" && noteName(61, false) == "C#3");
    CHECK(noteName(69, false) == "A3" && noteName(0, false) == "C-2" && noteName(127, false) == "G8");
    CHECK(noteName(60, true) == "Do3" && noteName(62, true) == "R\xC3\xA9" "3" && noteName(68, true) == "Sol#3");

    EventConfig na1 = eventForIndex(2);    // nA1
    EventConfig pa2 = eventForIndex(51);   // PA2
    EventConfig nr1 = eventForIndex(7);    // nr1
    EventConfig prA = eventForIndex(55);   // Pr1
    CHECK(valueText(na1, 0, false, false) == "36" && valueText(na1, 127, false, false) == "48");
    CHECK(valueText(na1, 0, true, false) == "C1" && valueText(na1, 127, true, false) == "C2");
    CHECK(valueText(pa2, 127, true, false) == "---" && valueText(pa2, 126, true, false) != "---");
    CHECK(valueText(nr1, 127, true, false) == "+12" && valueText(nr1, 127, false, false) == "12");
    CHECK(valueText(prA, 127, true, true) == "---");
    CHECK(valueText(eventForIndex(0), 50, true, false) == "---");
    CHECK(valueText(eventForIndex(13), 77, true, false) == "77");  // contrôleur : toujours la valeur
    CHECK(isAbsoluteNoteEvent(na1) && !isAbsoluteNoteEvent(nr1));

    // la correspondance index <-> Event est exacte pour les 60 entrées
    for (int i = 0; i < kEventCount; ++i) CHECK(eventIndexFor(eventForIndex(i)) == i);
}

static void testFolderUtf8() {
    fs::path root = tempDir("maq_test_utf8");
    fs::path d = root / "dossier";
    fs::create_directories(d);
    FolderSettings s(root);
    s.setFolderUtf8(FolderPurpose::PresetExport, d.u8string());
    CHECK(s.folderForUtf8(FolderPurpose::PresetImport) == d.u8string());  // « garder le même dossier » par défaut
    CHECK(s.folderFor(FolderPurpose::PresetExport) == d);
}


static void testStringsAndHelp() {
    for (int i = 0; i < static_cast<int>(Str::Count); ++i) {
        const std::string f = tr(static_cast<Str>(i), Lang::French), e = tr(static_cast<Str>(i), Lang::English);
        CHECK(!f.empty() && !e.empty());
    }
    CHECK(tr(Str::Store, Lang::French) == "Enregistrer" && tr(Str::Store, Lang::English) == "Store");
    CHECK(fmt(Str::BankExported, Lang::English, "ma.maqbank", 7) == "Bank exported: ma.maqbank (7 presets)");
    CHECK(fmt(Str::PresetStored, Lang::French, "", 12) == "Preset 12 enregistr\xC3\xA9");
    CHECK(langFromCode("en") == Lang::English && langFromCode("fr") == Lang::French && langFromCode("xx") == Lang::French);
    CHECK(std::string(langCode(Lang::English)) == "en");

    // l'aide existe, diffère d'une langue à l'autre et ne contient aucun marqueur resté en place
    auto good = [](const HelpContext& c) {
        const std::string f = helpText(c, Lang::French), e = helpText(c, Lang::English);
        return !f.empty() && !e.empty() && f != e && f.find('{') == std::string::npos && e.find('{') == std::string::npos;
    };
    HelpContext c;
    for (HelpMenu m : {HelpMenu::None, HelpMenu::Channel, HelpMenu::SingleStep, HelpMenu::Preset}) {
        c.menu = m;
        for (bool flag : {false, true}) { c.singleRow = flag; c.store = flag; CHECK(good(c)); }
    }
    for (HelpMenu m : {HelpMenu::FirstLast, HelpMenu::Prescale}) {
        c.menu = m;
        for (bool flag : {false, true}) { c.second = flag; CHECK(good(c)); }
    }
    c.menu = HelpMenu::Event;
    for (int i = 0; i < kEventCount; ++i) { c.eventIndex = i; CHECK(good(c)); }
    c.ccEdit = true; CHECK(good(c)); c.ccEdit = false;
    c.menu = HelpMenu::Mode;
    for (int m = 0; m < kModeCount; ++m) { c.modeIndex = m; CHECK(good(c)); }
    c.menu = HelpMenu::Event; c.eventIndex = 2;
    CHECK(helpText(c, Lang::English).find("MIDI 36 to 48") != std::string::npos);
    CHECK(helpText(c, Lang::French).find("Do1") != std::string::npos);
}

static void testPreferences() {
    fs::path root = tempDir("maq_test_prefs");
    Preferences p;
    CHECK(p.language == Lang::French && p.manualPath.empty());   // défauts
    p.language = Lang::English;
    p.manualPath = "/chemin/vers/MAQ16-3 manuel.pdf";
    fs::path file = root / "sous" / "preferences.ini";
    CHECK(p.save(file));
    Preferences q;
    CHECK(q.load(file));
    CHECK(q.language == Lang::English && q.manualPath == p.manualPath);
    CHECK(!q.load(root / "absent.ini"));
    CHECK(!Preferences::defaultFile().empty());
}


static void testScales() {
    // appartenance et quantification (Do majeur)
    CHECK(inScale(60, 0, 1) && inScale(62, 0, 1) && !inScale(61, 0, 1) && !inScale(63, 0, 1) && inScale(71, 0, 1));
    CHECK(quantizeNote(61, 0, 1) == 60);   // à égalité : la plus grave
    CHECK(quantizeNote(63, 0, 1) == 62);
    CHECK(quantizeNote(66, 0, 1) == 65);
    CHECK(quantizeNote(60, 0, 1) == 60);
    // la fondamentale décale la gamme : Ré majeur (Fa# et Do# présents)
    CHECK(inScale(66, 2, 1) && inScale(61, 2, 1) && !inScale(60, 2, 1));
    // pentatonique mineure de La : La Do Ré Mi Sol
    CHECK(inScale(69, 9, 11) && inScale(72, 9, 11) && inScale(74, 9, 11) && !inScale(71, 9, 11));
    CHECK(quantizeNote(70, 9, 11) == 69);
    // chromatique : rien ne change
    for (int n = 0; n < 128; ++n) CHECK(quantizeNote(n, 5, 0) == n);
    // dans une plage
    CHECK(quantizeNoteInRange(49, 36, 48, 0, 1) == 48);   // 49 est hors plage : on reste dans 36..48
    CHECK(quantizeNoteInRange(37, 36, 48, 0, 1) == 36);
    // noms
    CHECK(scaleName(1, Lang::English) == "Major / Ionian" && scaleName(6, Lang::French) == "Phrygien");
    CHECK(pitchClassName(1, false) == "C#" && pitchClassName(3, true) == "R\xC3\xA9#" && pitchClassName(13, false) == "C#");
    for (int t = 0; t < kScaleCount; ++t) CHECK(!scaleName(t, Lang::French).empty() && scaleMask(t) != 0);
}

static void testPotConversions() {
    // note <-> potard : exact pour toutes les plages
    for (int oct = 1; oct <= 5; ++oct) {
        for (int n = 36; n <= 36 + 12 * oct; ++n) {
            const int p = potForNote(oct, n, false);
            CHECK(p >= 0 && noteFromPot(oct, p) == n);
            const int q = potForNote(oct, n, true);
            CHECK(q >= 0 && q <= 126 && noteFromPot(oct, q) == n);
        }
        CHECK(potForNote(oct, 35, false) == -1 && potForNote(oct, 37 + 12 * oct, false) == -1);
        for (int o = 0; o <= 12 * oct; ++o) {
            const int p = potForOffset(oct, o, true);
            CHECK(p >= 0 && p <= 126 && noteOffsetFromPot(oct, p) == o);
        }
    }
    // verrouillage : un potard de note tombe sur une note de la gamme, la pause reste une pause
    EventConfig na1 = eventForIndex(2);    // nA1
    EventConfig pa1 = eventForIndex(50);   // PA1
    for (int pot = 0; pot < 127; ++pot) {
        const int s = snapPot(na1, pot, 0, 1);
        CHECK(inScale(noteFromPot(1, s), 0, 1));
        CHECK(snapPot(na1, s, 0, 1) == s);   // idempotent
    }
    CHECK(snapPot(pa1, 127, 0, 1) == 127);
    EventConfig cc = eventForIndex(13);
    CHECK(snapPot(cc, 77, 0, 1) == 77);      // pas une rangée de notes
}

static void testLearn() {
    int ref = -1;
    EventConfig na2 = eventForIndex(3);    // nA2 : 36..60
    // une note de la plage
    int p = learnPot(na2, 48, ref, false, 0, 0);
    CHECK(p >= 0 && noteFromPot(2, p) == 48);
    // note trop grave ou trop aiguë : repliée dans la plage, même classe de hauteur
    p = learnPot(na2, 24, ref, false, 0, 0);
    CHECK(noteFromPot(2, p) == 36);
    p = learnPot(na2, 79, ref, false, 0, 0);   // Sol : repli vers 55
    CHECK(noteFromPot(2, p) == 55);
    // verrouillage : une note hors gamme est ramenée dans la gamme
    p = learnPot(na2, 49, ref, true, 0, 1);    // Do# en Do majeur -> Do
    CHECK(noteFromPot(2, p) == 48);
    // PA : la note la plus haute reste jouable (pas de pause)
    EventConfig pa1 = eventForIndex(50);
    p = learnPot(pa1, 48, ref, false, 0, 0);
    CHECK(p >= 0 && p <= 126 && noteFromPot(1, p) == 48);
    // notes relatives : la première note sert de référence
    int refR = -1;
    EventConfig nr1 = eventForIndex(7);
    p = learnPot(nr1, 60, refR, false, 0, 0);
    CHECK(refR == 60 && noteOffsetFromPot(1, p) == 0);
    p = learnPot(nr1, 67, refR, false, 0, 0);
    CHECK(noteOffsetFromPot(1, p) == 7);
    p = learnPot(nr1, 55, refR, false, 0, 0);  // plus grave que la référence : repli d'une octave vers le haut
    CHECK(noteOffsetFromPot(1, p) == 7);
    // les autres Events n'apprennent pas
    int ref2 = -1;
    CHECK(learnPot(eventForIndex(13), 60, ref2, false, 0, 0) == -1);
    CHECK(learnPot(eventForIndex(0), 60, ref2, false, 0, 0) == -1);
}

static void testOutputQuantize() {
    Unit u;
    u.rows[1].event.kind = EventKind::Off;
    u.rows[2].event.kind = EventKind::Off;
    u.rows[0].gate = 1;
    u.rows[0].last = 4;
    // Do#1 (37), Ré#1 (39), Fa#1 (42) : toutes hors Do majeur
    u.rows[0].pot[0] = potForNote(1, 37, false);
    u.rows[0].pot[1] = potForNote(1, 39, false);
    u.rows[0].pot[2] = potForNote(1, 42, false);
    u.rows[0].pot[3] = potForNote(1, 36, false);
    {   // sans gamme : les notes sont jouées telles quelles
        Unit a = u;
        auto on = noteOns(run(a, 4));
        CHECK((on == std::vector<int>{37, 39, 42, 36}));
    }
    {   // quantification en sortie (Do majeur) : Do#->Do, Ré#->Ré, Fa#->Fa
        Unit a = u;
        a.scaleMode = kScaleModeOutput; a.scaleRoot = 0; a.scaleType = 1;
        auto on = noteOns(run(a, 4));
        CHECK((on == std::vector<int>{36, 38, 41, 36}));
    }
    {   // verrouillage : le moteur ne quantifie pas, c'est l'interface qui ramène les potards
        Unit a = u;
        a.scaleMode = kScaleModeLock; a.scaleRoot = 0; a.scaleType = 1;
        auto on = noteOns(run(a, 4));
        CHECK((on == std::vector<int>{37, 39, 42, 36}));
    }
}


static void testScaleCatalogue() {
    CHECK(kScaleCount == 70 && kScaleCategoryCount == 8);
    // toutes les gammes sont valides, distinctes, contiennent la fondamentale ; les index 0..14 sont inchangés
    for (int t = 0; t < kScaleCount; ++t) {
        CHECK((scaleMask(t) & 1u) != 0);
        CHECK(!scaleName(t, Lang::French).empty() && !scaleName(t, Lang::English).empty());
        CHECK(scaleCategory(t) >= 0 && scaleCategory(t) < kScaleCategoryCount);
        for (int u = t + 1; u < kScaleCount; ++u) CHECK(scaleMask(t) != scaleMask(u));
    }
    for (int c = 0; c < kScaleCategoryCount; ++c) CHECK(!scaleCategoryName(c, Lang::French).empty());
    CHECK(scaleMask(0) == 0xFFFu);
    CHECK(inScale(62, 0, 4) && inScale(63, 0, 4) && inScale(71, 0, 4) && !inScale(61, 0, 4) && !inScale(70, 0, 4));  // mineure mélodique : Do Ré Mib Fa Sol La Si
    // quelques gammes du catalogue
    auto notes = [](int type) { std::vector<int> v; for (int i = 0; i < 12; ++i) if (scaleMask(type) & (1u << i)) v.push_back(i); return v; };
    auto find = [](const char* en) {
        for (int t = 0; t < kScaleCount; ++t)
            if (scaleName(t, Lang::English).find(en) == 0) return t;
        return -1;
    };
    CHECK((notes(find("Hirajoshi")) == std::vector<int>{0, 2, 3, 7, 8}));
    CHECK((notes(find("Hungarian Major")) == std::vector<int>{0, 3, 4, 6, 7, 9, 10}));
    CHECK((notes(find("Phrygian Dominant")) == std::vector<int>{0, 1, 4, 5, 7, 8, 10}));
    CHECK((notes(find("Double Harmonic Major")) == std::vector<int>{0, 1, 4, 5, 7, 8, 11}));
    CHECK((notes(find("Bebop Dominant")) == std::vector<int>{0, 2, 4, 5, 7, 9, 10, 11}));
    CHECK((notes(find("Six Tone")) == std::vector<int>{0, 1, 4, 5, 8, 9}));
    CHECK((notes(find("Persian")) == std::vector<int>{0, 1, 4, 5, 6, 8, 11}));
    CHECK((notes(find("Kumoi")) == std::vector<int>{0, 2, 3, 7, 9}));
    CHECK((notes(find("Pentatonic Dominant")) == std::vector<int>{0, 2, 4, 7, 10}));
    CHECK((notes(find("Altered")) == std::vector<int>{0, 1, 3, 4, 6, 8, 10}));
    CHECK(find("Todi") >= 60 && scaleName(find("Todi"), Lang::English).find("approx.") != std::string::npos);
    // la quantification fonctionne avec les nouvelles gammes (Hirajoshi en Do : Do Ré Mib Sol Lab)
    const int hira = find("Hirajoshi");
    CHECK(quantizeNote(65, 0, hira) == 63 || quantizeNote(65, 0, hira) == 67);   // Fa -> Mib ou Sol (à égalité : la plus grave)
    CHECK(quantizeNote(65, 0, hira) == 63);
    CHECK(quantizeNote(70, 0, hira) == 68);   // Sib -> Lab
}


// ---- Lecture minimale d'un fichier MIDI pour vérifier ce qu'on écrit
struct SmfEvent { long tick; int status; int d1; int d2; };
struct SmfTrack { std::vector<SmfEvent> events; long tempo = 0; long endTick = 0; std::string name; };
struct Smf { bool ok = false; int format = -1, ntrks = 0, division = 0; std::vector<SmfTrack> tracks; };

static Smf parseSmf(const std::vector<std::uint8_t>& d) {
    Smf f;
    size_t p = 0;
    auto u32 = [&]() { unsigned v = 0; for (int i = 0; i < 4; ++i) v = (v << 8) | d[p++]; return v; };
    auto u16 = [&]() { unsigned v = (d[p] << 8) | d[p + 1]; p += 2; return v; };
    if (d.size() < 14 || d[0] != 'M' || d[1] != 'T' || d[2] != 'h' || d[3] != 'd') return f;
    p = 4;
    if (u32() != 6) return f;
    f.format = static_cast<int>(u16());
    f.ntrks = static_cast<int>(u16());
    f.division = static_cast<int>(u16());
    for (int t = 0; t < f.ntrks; ++t) {
        if (p + 8 > d.size() || d[p] != 'M' || d[p + 1] != 'T' || d[p + 2] != 'r' || d[p + 3] != 'k') return f;
        p += 4;
        const size_t len = u32();
        const size_t end = p + len;
        if (end > d.size()) return f;
        SmfTrack tr;
        long tick = 0;
        int running = 0;
        while (p < end) {
            unsigned delta = 0; std::uint8_t b;
            do { b = d[p++]; delta = (delta << 7) | (b & 0x7F); } while (b & 0x80);
            tick += delta;
            int status = d[p];
            if (status == 0xFF) {
                ++p;
                const int type = d[p++];
                const size_t mlen = d[p++];
                if (type == 0x51 && mlen == 3) tr.tempo = (d[p] << 16) | (d[p + 1] << 8) | d[p + 2];
                if (type == 0x03) tr.name.assign(d.begin() + p, d.begin() + p + mlen);
                if (type == 0x2F) tr.endTick = tick;
                p += mlen;
            } else {
                if (status & 0x80) { running = status; ++p; } else status = running;
                const int hi = status & 0xF0;
                const int n = (hi == 0xC0 || hi == 0xD0) ? 1 : 2;
                SmfEvent e{tick, status, d[p], n == 2 ? d[p + 1] : 0};
                p += static_cast<size_t>(n);
                tr.events.push_back(e);
            }
        }
        f.tracks.push_back(tr);
    }
    f.ok = (p == d.size());
    return f;
}

static void testCapture() {
    CaptureBuffer cb;
    for (int c = 0; c < 100; ++c) {
        cb.push(c, MidiMsg{MidiMsg::Type::NoteOn, 0, 40 + (c % 10), 100, 0});
        cb.setLastClock(c);
    }
    auto w = cb.window(10);                       // horloges 89..99
    CHECK(!w.empty() && w.front().clock >= 0 && w.back().clock <= 10);
    CHECK(w.size() == 11);
    CHECK(cb.window(1000).size() == 100);
    // dépassement de capacité : on garde les plus récents
    CaptureBuffer big;
    for (int c = 0; c < static_cast<int>(CaptureBuffer::kCapacity) + 500; ++c) {
        big.push(c, MidiMsg{MidiMsg::Type::NoteOn, 0, 60, 100, 0});
        big.setLastClock(c);
    }
    CHECK(big.window(100).size() == 101);
    big.clear();
    CHECK(big.window(100).empty());
}

static void testRowIdsAndLengths() {
    Unit u;
    for (int r = 1; r < 3; ++r) u.rows[static_cast<size_t>(r)].event.kind = EventKind::Off;
    auto evs = run(u, 7);
    CHECK(!evs.empty());
    for (const auto& e : evs) CHECK(e.m.row == 0);

    std::array<RowConfig, kRows> rows{};
    rows[0].event.kind = EventKind::NoteAbs; rows[0].first = 1; rows[0].last = 16; rows[0].gate = 6;   // 96
    rows[1].event.kind = EventKind::NoteAbs; rows[1].first = 1; rows[1].last = 3;  rows[1].gate = 6;   // 18
    rows[2].event.kind = EventKind::Off;
    CHECK(rowPeriodClocks(rows[0]) == 96 && rowPeriodClocks(rows[1]) == 18 && rowPeriodClocks(rows[2]) == 0);
    CHECK(loopLengthClocks(rows) == 96);
    CHECK(cycleLengthClocks(rows, 100000) == 288);      // PPCM de 96 et 18
    CHECK(cycleLengthClocks(rows, 200) == 200);         // plafonné
    rows[0].mode = PlayMode::Pendulum1;  CHECK(rowPeriodClocks(rows[0]) == 192);
    rows[0].mode = PlayMode::Pendulum2;  CHECK(rowPeriodClocks(rows[0]) == 180);   // (2*16-2)*6
    rows[0].mode = PlayMode::OneEvent;   CHECK(rowPeriodClocks(rows[0]) == 6);
    std::array<RowConfig, kRows> none{};
    for (auto& r : none) r.event.kind = EventKind::Off;
    CHECK(loopLengthClocks(none) == kClocksPerBar && cycleLengthClocks(none, 1000) == kClocksPerBar);
}

static void testRenderAndMidiFile() {
    Unit u;
    u.rows[1].event.kind = EventKind::Off;
    u.rows[2].event.kind = EventKind::Off;
    for (int i = 0; i < 16; ++i) u.rows[0].pot[static_cast<size_t>(i)] = i * 8;
    auto a = renderLoop(u, 96), b = renderLoop(u, 96);
    CHECK(a.size() == b.size());
    int ons = 0, offs = 0;
    for (const auto& e : a) { if (e.msg.type == MidiMsg::Type::NoteOn) ++ons; if (e.msg.type == MidiMsg::Type::NoteOff) ++offs; }
    CHECK(ons == 16 && offs == 16);                    // toutes les notes sont fermées

    auto file = buildMidiFile(a, 120.0, 0x7, 96);
    Smf f = parseSmf(file);
    CHECK(f.ok && f.format == 1 && f.division == 480);
    CHECK(f.ntrks == 4);                               // tempo + 3 rangées
    CHECK(f.tracks[0].tempo == 500000);                // 120 BPM
    CHECK(f.tracks[1].name == "Row 1");
    int fileOns = 0; bool grid = true;
    for (const auto& e : f.tracks[1].events)
        if ((e.status & 0xF0) == 0x90) { ++fileOns; if (e.tick % 120 != 0) grid = false; }   // 6 horloges = 120 ticks
    CHECK(fileOns == 16 && grid);
    CHECK(f.tracks[1].endTick == 96 * 20);
    CHECK(f.tracks[2].events.empty() && f.tracks[3].events.empty());

    // masque de rangées : une seule piste en plus du tempo
    auto one = parseSmf(buildMidiFile(a, 90.0, 0x1, 96));
    CHECK(one.ok && one.ntrks == 2 && one.tracks[0].tempo == 666667);

    // Note Off orphelin écarté, note ouverte fermée à la fin, Pitch Bend codé sur 14 bits
    std::vector<CapturedEvent> ev;
    ev.push_back({2, MidiMsg{MidiMsg::Type::NoteOff, 0, 50, 0, 0}});           // orphelin
    ev.push_back({4, MidiMsg{MidiMsg::Type::NoteOn, 0, 52, 90, 0}});           // jamais fermée
    ev.push_back({6, MidiMsg{MidiMsg::Type::PitchBend, 1, 8192, 0, 0}});
    ev.push_back({8, MidiMsg{MidiMsg::Type::ControlChange, 1, 74, 99, 0}});
    ev.push_back({9, MidiMsg{MidiMsg::Type::ControlChange, 0, 123, 0, -1}});   // sans rangée : ignoré
    auto g = parseSmf(buildMidiFile(ev, 120.0, 0x1, 12));
    CHECK(g.ok && g.ntrks == 2);
    int offsSeen = 0, noteOnSeen = 0, bendOk = 0, ccOk = 0, ccCount = 0;
    for (const auto& e : g.tracks[1].events) {
        if ((e.status & 0xF0) == 0x80) { ++offsSeen; if (e.d1 == 52 && e.tick == 12 * 20) ++noteOnSeen; }
        if ((e.status & 0xF0) == 0xE0 && e.d1 == 0 && e.d2 == 64) ++bendOk;     // 8192 = 0x40 0x00
        if ((e.status & 0xF0) == 0xB0) { ++ccCount; if (e.d1 == 74 && e.d2 == 99) ++ccOk; }
    }
    CHECK(offsSeen == 1 && noteOnSeen == 1 && bendOk == 1 && ccOk == 1 && ccCount == 1);
}


// ============================================================ Lot 5 : unités, chaînage, liens

struct PEv { int tick; MidiMsg m; };

static std::vector<PEv> runProject(Project& p, int ticks, bool doStart = true) {
    std::vector<PEv> all;
    if (doStart) p.start();
    for (int t = 0; t < ticks; ++t) {
        std::vector<MidiMsg> out;
        p.tick(out);
        for (auto& m : out) all.push_back({t, m});
    }
    return all;
}

static std::vector<PEv> projNoteOns(const std::vector<PEv>& evs) {
    std::vector<PEv> n;
    for (auto& e : evs) if (e.m.type == MidiMsg::Type::NoteOn) n.push_back(e);
    return n;
}

static void onlyRow0(Unit& u, int channel, PlayMode mode, int first, int last) {
    u.rows[0].event.kind = EventKind::NoteAbs;
    u.rows[0].channel = channel;
    u.rows[0].mode = mode;
    u.rows[0].first = first;
    u.rows[0].last = last;
    u.rows[0].gate = 6;
    u.rows[1].event.kind = EventKind::Off;
    u.rows[2].event.kind = EventKind::Off;
}

static void testOnrChain() {
    // 3 rangées en Onr = une séquence de 12 pas : 4 pas de chaque rangée, puis retour à la première
    Unit u;
    for (int r = 0; r < 3; ++r) {
        u.rows[static_cast<size_t>(r)].event.kind = EventKind::NoteAbs;
        u.rows[static_cast<size_t>(r)].mode = PlayMode::OneShotRetrigger;
        u.rows[static_cast<size_t>(r)].first = 1;
        u.rows[static_cast<size_t>(r)].last = 4;
        u.rows[static_cast<size_t>(r)].gate = 6;
        u.rows[static_cast<size_t>(r)].channel = r + 1;
    }
    auto evs = run(u, 84);
    std::vector<int> rowsSeq, ticks;
    for (auto& e : evs) if (e.m.type == MidiMsg::Type::NoteOn) { rowsSeq.push_back(e.m.row); ticks.push_back(e.tick); }
    const std::vector<int> expected = {0,0,0,0, 1,1,1,1, 2,2,2,2, 0,0};
    CHECK(rowsSeq == expected);
    bool regular = ticks.size() == expected.size();
    for (size_t i = 0; regular && i < ticks.size(); ++i) regular = (ticks[i] == static_cast<int>(6 * i));
    CHECK(regular);  // aucun trou, aucune horloge de retard au passage d'une rangée à l'autre

    // la dernière rangée Onr ne relance pas la première si wrapOnr est faux
    Unit v = u;
    v.wrapOnr = false;
    int count = 0;
    for (auto& e : run(v, 200)) if (e.m.type == MidiMsg::Type::NoteOn) ++count;
    CHECK(count == 12);

    // une rangée Onr seule se redéclenche elle-même
    Unit w;
    onlyRow0(w, 1, PlayMode::OneShotRetrigger, 1, 4);
    const int pots[4] = {0, 32, 64, 127};
    for (int i = 0; i < 4; ++i) w.rows[0].pot[static_cast<size_t>(i)] = pots[i];
    auto notes = noteOns(run(w, 72));
    const std::vector<int> expectedNotes = {36,39,42,48, 36,39,42,48, 36,39,42,48};
    CHECK(notes == expectedNotes);

    // la rangée coupée (rowMute) ne joue plus
    Unit m;
    onlyRow0(m, 1, PlayMode::Forward, 1, 16);
    m.rows[0].rowMute = true;
    CHECK(noteOns(run(m, 60)).empty());
}

static void testRemoteProgramChange() {
    Unit u;
    int sel = 0;
    CHECK(applyProgramChange(u, sel, 1) && sel == 1);
    CHECK(applyProgramChange(u, sel, 63) && u.rows[1].first == 16);
    CHECK(applyProgramChange(u, sel, 64) && u.rows[1].last == 1);
    CHECK(applyProgramChange(u, sel, 10) && u.rows[1].mode == PlayMode::Pendulum1);
    CHECK(applyProgramChange(u, sel, 8) && u.rows[1].mode == PlayMode::Forward);
    CHECK(applyProgramChange(u, sel, 9) && u.rows[1].mode == PlayMode::Backward);
    CHECK(applyProgramChange(u, sel, 11) && u.rows[1].mode == PlayMode::Random);
    CHECK(applyProgramChange(u, sel, 4) && u.rows[1].rowMute);
    CHECK(applyProgramChange(u, sel, 3) && !u.rows[1].rowMute);
    CHECK(applyProgramChange(u, sel, 6) && u.rows[1].mute[0] && u.rows[1].mute[15]);
    CHECK(applyProgramChange(u, sel, 5) && !u.rows[1].mute[0] && !u.rows[1].mute[15]);
    CHECK(applyProgramChange(u, sel, 32 + 3) && u.rows[1].mute[3]);
    CHECK(applyProgramChange(u, sel, 16 + 3) && !u.rows[1].mute[3]);
    CHECK(applyProgramChange(u, sel, 85) && u.rows[1].event.kind == EventKind::FixedCC && u.rows[1].event.ccNumber == 5);
    CHECK(applyProgramChange(u, sel, 115) && u.rows[1].event.kind == EventKind::NoteAbs && u.rows[1].event.octaves == 4);
    CHECK(applyProgramChange(u, sel, 120) && u.rows[1].event.kind == EventKind::NoteRel && u.rows[1].event.octaves == 4);
    CHECK(applyProgramChange(u, sel, 122) && u.rows[1].event.kind == EventKind::PitchBend);
    CHECK(applyProgramChange(u, sel, 123) && u.rows[1].event.kind == EventKind::PitchBendPos);
    CHECK(applyProgramChange(u, sel, 124) && u.rows[1].event.kind == EventKind::PitchBendNeg);
    CHECK(applyProgramChange(u, sel, 125) && u.rows[1].event.kind == EventKind::ProgramChange);
    CHECK(!applyProgramChange(u, sel, 126) && !applyProgramChange(u, sel, 127));  // Cn1 et Ad1 : pas encore émulés
    CHECK(!applyProgramChange(u, sel, 7) && !applyProgramChange(u, sel, 200));
    CHECK(applyProgramChange(u, sel, 0) && sel == 0);
    CHECK(u.rows[0].first == 1 && u.rows[0].last == 16);  // la rangée 1 n'a pas été touchée
    CHECK(applyProgramChange(u, sel, 48) && u.rows[0].first == 1);
}

static void testProjectParallelAndOutput() {
    Project p;
    p.active = {true, true, false, false};
    onlyRow0(p.units[0], 1, PlayMode::Forward, 1, 16);
    onlyRow0(p.units[1], 3, PlayMode::Forward, 1, 16);
    onlyRow0(p.units[2], 5, PlayMode::Forward, 1, 16);  // inactive : ne doit rien jouer

    auto on = projNoteOns(runProject(p, 12));
    CHECK(on.size() == 4);
    CHECK(on[0].m.unit == 0 && on[1].m.unit == 1 && on[2].m.unit == 0 && on[3].m.unit == 1);
    CHECK(on[0].tick == 0 && on[1].tick == 0 && on[2].tick == 6 && on[3].tick == 6);
    CHECK(on[0].m.channel == 0 && on[1].m.channel == 2);

    CHECK(p.collisions().empty());                       // canaux 1 et 3
    p.units[1].rows[0].channel = 1;
    auto col = p.collisions();
    CHECK(col.size() == 1 && col[0].channel == 1 && col[0].unitMask == 3);
    p.units[2].rows[0].channel = 1;
    CHECK(p.collisions().size() == 1 && p.collisions()[0].unitMask == 3);   // une unité inactive ne compte pas
    p.units[0].rows[1].event.kind = EventKind::NoteAbs;
    p.units[0].rows[1].channel = 1;                      // deux rangées d'une même unité sur un canal : pas une collision
    p.units[1].rows[0].channel = 7;
    CHECK(p.collisions().empty());

    // modes de canal de sortie
    auto chOf = [&](int unit) {  // canal de sortie de la première note de l'unité
        for (auto& e : projNoteOns(runProject(p, 1))) if (e.m.unit == unit) return e.m.channel;
        return -1;
    };
    p.units[1].rows[0].channel = 3;
    p.output[1].mode = ChannelMode::Forced;  p.output[1].forced = 10;
    CHECK(chOf(1) == 9);
    p.output[1].mode = ChannelMode::Offset;  p.output[1].offset = 15;     // canal 3 + 15 = canal 2 (circulaire)
    CHECK(chOf(1) == 1);
    p.output[1].offset = -3;                                              // canal 3 - 3 = canal 16
    CHECK(chOf(1) == 15);
    CHECK(Project::outputChannel(UnitOutput{ChannelMode::PerRow, 1, 0}, 4) == 4);

    // arrêt : les Note Off sont étiquetés par unité et suivent le mode de canal
    p.output[1].mode = ChannelMode::Forced;  p.output[1].forced = 10;
    runProject(p, 2);
    std::vector<MidiMsg> out;
    p.stop(out);
    int offs = 0; bool ok = true;
    for (auto& m : out) if (m.type == MidiMsg::Type::NoteOff) {
        ++offs;
        if (m.unit == 1 && m.channel != 9) ok = false;
        if (m.unit == 0 && m.channel != 0) ok = false;
    }
    CHECK(offs >= 2 && ok);
    CHECK(!p.isPlaying());
}

static void testSeriesChain() {
    // --- par rangée : A1 -> B1 -> A1 ...
    Project p;
    p.active = {true, true, false, false};
    onlyRow0(p.units[0], 1, PlayMode::OneShotSingle, 1, 4);
    onlyRow0(p.units[1], 2, PlayMode::OneShotSingle, 1, 4);
    p.chain.kind = ChainKind::Series;
    p.chain.scope = ChainScope::PerRow;
    p.chain.loopToFirst = true;
    auto on = projNoteOns(runProject(p, 72));
    std::vector<int> units;
    bool regular = on.size() == 12;
    for (size_t i = 0; i < on.size(); ++i) {
        units.push_back(on[i].m.unit);
        if (on[i].tick != static_cast<int>(6 * i)) regular = false;
    }
    const std::vector<int> expectedUnits = {0,0,0,0, 1,1,1,1, 0,0,0,0};
    CHECK(units == expectedUnits && regular);

    // fin de chaîne : arrêt après la dernière unité
    p.chain.loopToFirst = false;
    CHECK(projNoteOns(runProject(p, 300)).size() == 8);

    // --- par unité entière : A (rangées 1 puis 2 en Onr) -> B (idem) -> A ...
    Project q;
    q.active = {true, true, false, false};
    for (int u = 0; u < 2; ++u) {
        for (int r = 0; r < 3; ++r) {
            RowConfig& row = q.units[static_cast<size_t>(u)].rows[static_cast<size_t>(r)];
            row.event.kind = r < 2 ? EventKind::NoteAbs : EventKind::Off;
            row.mode = PlayMode::OneShotRetrigger;
            row.first = 1; row.last = 4; row.gate = 6;
            row.channel = r + 1;
        }
    }
    q.chain.kind = ChainKind::Series;
    q.chain.scope = ChainScope::WholeUnit;
    q.chain.loopToFirst = true;
    on = projNoteOns(runProject(q, 120));
    // attendu : A1 x4, A2 x4, B1 x4, B2 x4, A1 x4
    std::vector<int> unitSeq, rowSeq;
    bool regular2 = on.size() == 20;
    for (size_t i = 0; i < on.size(); ++i) {
        unitSeq.push_back(on[i].m.unit);
        rowSeq.push_back(on[i].m.row);
        if (on[i].tick != static_cast<int>(6 * i)) regular2 = false;
    }
    const std::vector<int> expectedUnitSeq = {0,0,0,0, 0,0,0,0, 1,1,1,1, 1,1,1,1, 0,0,0,0};
    const std::vector<int> expectedRowSeq  = {0,0,0,0, 1,1,1,1, 0,0,0,0, 1,1,1,1, 0,0,0,0};
    CHECK(unitSeq == expectedUnitSeq && rowSeq == expectedRowSeq && regular2);

    q.chain.loopToFirst = false;
    CHECK(projNoteOns(runProject(q, 400)).size() == 16);   // 2 unités x 2 rangées x 4 pas, puis arrêt

    // sans chaînage, les deux unités jouent en parallèle
    q.chain.kind = ChainKind::None;
    for (int u = 0; u < 2; ++u)
        for (int r = 0; r < 2; ++r) q.units[static_cast<size_t>(u)].rows[static_cast<size_t>(r)].mode = PlayMode::Forward;
    CHECK(projNoteOns(runProject(q, 6)).size() == 4);
}

static void testLinks() {
    // --- transposition : la note de A (rangée 1) règle la référence de la rangée relative de B
    Project p;
    p.active = {true, true, false, false};
    onlyRow0(p.units[0], 1, PlayMode::Forward, 1, 16);
    onlyRow0(p.units[1], 2, PlayMode::Forward, 1, 16);
    p.units[0].rows[0].pot[0] = 64;   // note 42
    p.units[0].rows[0].pot[1] = 127;  // note 48
    p.units[1].rows[0].event.kind = EventKind::NoteRel;
    p.units[1].rows[0].event.octaves = 1;
    p.units[1].rows[0].pot[0] = 0;    // décalage 0
    p.units[1].rows[0].pot[1] = 0;
    auto on = projNoteOns(runProject(p, 7));
    CHECK(on.size() == 4 && on[1].m.unit == 1 && on[1].m.data1 == 36);   // sans lien : référence par défaut
    p.transposeLinks[0] = TransposeLink{true, 0, 0, 1, 0};
    on = projNoteOns(runProject(p, 7));
    CHECK(on.size() == 4);
    CHECK(on[0].m.unit == 0 && on[0].m.data1 == 42);
    CHECK(on[1].m.unit == 1 && on[1].m.data1 == 42 && on[1].tick == 0);  // même horloge que la note source
    CHECK(on[3].m.unit == 1 && on[3].m.data1 == 48 && on[3].tick == 6);

    // source dans l'unité B, cible dans l'unité A : l'ordre de lecture s'adapte
    Project q;
    q.active = {true, true, false, false};
    onlyRow0(q.units[0], 1, PlayMode::Forward, 1, 16);
    onlyRow0(q.units[1], 2, PlayMode::Forward, 1, 16);
    q.units[1].rows[0].pot[0] = 127;  // note 48
    q.units[0].rows[0].event.kind = EventKind::NoteRel;
    q.units[0].rows[0].pot[0] = 0;
    q.transposeLinks[0] = TransposeLink{true, 1, 0, 0, -1};
    on = projNoteOns(runProject(q, 1));
    bool found = false;
    for (auto& e : on) if (e.m.unit == 0 && e.m.data1 == 48 && e.tick == 0) found = true;
    CHECK(found);

    // --- pilotage : une rangée Prg de A envoie des commandes de télécommande à B
    Project c;
    c.active = {true, true, false, false};
    onlyRow0(c.units[0], 1, PlayMode::Forward, 1, 16);
    c.units[0].rows[0].event.kind = EventKind::ProgramChange;
    c.units[0].rows[0].pot[0] = 1;    // sélection de la rangée 2 de B
    c.units[0].rows[0].pot[1] = 63;   // premier pas = 16
    onlyRow0(c.units[1], 2, PlayMode::Forward, 1, 16);
    CHECK(c.setControlLink(0, ControlLink{true, 0, 0, 1}));
    auto evs = runProject(c, 7);
    CHECK(c.units[1].rows[1].first == 16 && c.units[1].rows[0].first == 1);
    CHECK(c.selectedRow(1) == 1);
    int pcs = 0;
    for (auto& e : evs) if (e.m.type == MidiMsg::Type::ProgramChange && e.m.unit == 0) ++pcs;
    CHECK(pcs == 2);   // les Program Change sortent aussi vers le synthé

    // protection contre les boucles de pilotage
    CHECK(!c.setControlLink(1, ControlLink{true, 1, 0, 0}));   // B -> A refermerait la boucle
    CHECK(!c.setControlLink(1, ControlLink{true, 2, 0, 2}));   // une unité ne se pilote pas elle-même
    CHECK(c.setControlLink(1, ControlLink{true, 1, 0, 2}));    // B -> C est permis
    CHECK(!c.setControlLink(2, ControlLink{true, 2, 0, 0}));   // C -> A : boucle A -> B -> C -> A
    CHECK(c.setControlLink(2, ControlLink{true, 2, 0, 3}));    // C -> D est permis
    CHECK(c.setControlLink(0, ControlLink{false, 0, 0, 1}));   // désactiver un lien est toujours permis
    CHECK(c.setControlLink(2, ControlLink{true, 2, 0, 0}));    // sans A -> B, C -> A ne boucle plus
}


static void testProjectExport() {
    // durées : sans chaînage, plus longue rangée des unités actives
    Project p;
    p.active = {true, true, false, false};
    onlyRow0(p.units[0], 1, PlayMode::Forward, 1, 16);   // 96
    onlyRow0(p.units[1], 2, PlayMode::Forward, 1, 3);    // 18
    onlyRow0(p.units[2], 3, PlayMode::Forward, 1, 5);    // inactive : ignorée
    CHECK(projectLoopClocks(p) == 96);
    CHECK(projectCycleClocks(p, 100000) == 288);          // PPCM de 96 et 18
    CHECK(projectCycleClocks(p, 200) == 200);
    // série par rangée : 96 + 18 sur la rangée 1
    p.chain.kind = ChainKind::Series; p.chain.scope = ChainScope::PerRow;
    CHECK(projectLoopClocks(p) == 96 + 18);
    // série par unité : somme des rangées One-Shot de chaque unité
    p.chain.scope = ChainScope::WholeUnit;
    p.units[0].rows[0].mode = PlayMode::OneShotRetrigger;
    p.units[1].rows[0].mode = PlayMode::OneShotRetrigger;
    CHECK(projectLoopClocks(p) == 96 + 18);
    CHECK(projectCycleClocks(p, 10) == 10);

    // rendu et fichier : une piste par unité et par rangée, nommées A1, B1...
    Project q;
    q.active = {true, true, false, false};
    onlyRow0(q.units[0], 1, PlayMode::Forward, 1, 4);
    onlyRow0(q.units[1], 5, PlayMode::Forward, 1, 4);
    q.output[1].mode = ChannelMode::Forced; q.output[1].forced = 9;     // les canaux de sortie sont exportés
    auto ev = renderProject(q, 24);
    int onA = 0, onB = 0, offs = 0;
    for (auto& e : ev) {
        if (e.msg.type == MidiMsg::Type::NoteOn) { if (e.msg.unit == 0) ++onA; else ++onB; }
        if (e.msg.type == MidiMsg::Type::NoteOff) ++offs;
    }
    CHECK(onA == 4 && onB == 4 && offs == 8);
    Smf f = parseSmf(buildMidiFile(ev, 120.0, 0x7, 0x3, 24));
    CHECK(f.ok && f.ntrks == 1 + 2 * 3);
    CHECK(f.tracks[1].name == "A1" && f.tracks[4].name == "B1");
    bool chOk = true; int bn = 0;
    for (auto& e : f.tracks[4].events) if ((e.status & 0xF0) == 0x90) { ++bn; if ((e.status & 0x0F) != 8) chOk = false; }
    CHECK(bn == 4 && chOk);                               // canal forcé 9 = valeur 8 dans le fichier
    // une seule unité exportée (B) : une piste par rangée du masque
    Smf g = parseSmf(buildMidiFile(ev, 120.0, 0x1, 0x2, 24));
    CHECK(g.ok && g.ntrks == 2 && g.tracks[1].name == "B1");
    // compatibilité : l'ancien appel garde les noms « Row N » pour l'unité A
    Smf h = parseSmf(buildMidiFile(ev, 120.0, 0x1, 24));
    CHECK(h.ok && h.ntrks == 2 && h.tracks[1].name == "Row 1");
}


static void testChainAdvice() {
    Project p;
    p.active = {true, true, false, false};
    onlyRow0(p.units[0], 1, PlayMode::Forward, 1, 16);
    onlyRow0(p.units[1], 1, PlayMode::Forward, 1, 16);
    int u = -1, r = -1;
    CHECK(!p.chainProblem(u, r));                         // pas de chaînage : rien à signaler
    p.chain.kind = ChainKind::Series;
    CHECK(p.activeMemberCount() == 2);
    CHECK(p.chainProblem(u, r) && u == 0 && r == 0);       // rangée en Forward : le relais ne viendra jamais
    p.units[0].rows[0].mode = PlayMode::OneShotSingle;
    CHECK(p.chainProblem(u, r) && u == 1 && r == 0);       // l'unité B pose le même problème
    p.units[1].rows[0].mode = PlayMode::OneShotRetrigger;
    CHECK(!p.chainProblem(u, r));
    p.active = {true, false, false, false};                // une seule unité active : pas de chaîne
    CHECK(p.activeMemberCount() == 1);
    p.units[0].rows[0].mode = PlayMode::Forward;
    CHECK(!p.chainProblem(u, r));

    // collisions : les unités enchaînées ne jouent pas ensemble
    Project q;
    q.active = {true, true, true, false};
    onlyRow0(q.units[0], 1, PlayMode::OneShotSingle, 1, 16);
    onlyRow0(q.units[1], 1, PlayMode::OneShotSingle, 1, 16);
    onlyRow0(q.units[2], 1, PlayMode::OneShotSingle, 1, 16);
    CHECK(q.collisions().size() == 1 && q.collisions()[0].unitMask == 7);   // sans chaînage : collision
    q.chain.kind = ChainKind::Series; q.chain.scope = ChainScope::WholeUnit;
    CHECK(q.collisions().empty());                                          // enchaînées par unité : aucune
    q.chain.scope = ChainScope::PerRow;
    CHECK(q.collisions().empty());                                          // même rangée : elles alternent
    q.units[1].rows[0].event.kind = EventKind::Off;
    q.units[1].rows[1].event.kind = EventKind::NoteAbs; q.units[1].rows[1].channel = 1;   // B2 sur le canal 1
    auto col = q.collisions();
    CHECK(col.size() == 1 && col[0].unitMask == 7);                         // B2 peut jouer avec A1 et C1 : vraie collision
    q.chain.member[1] = false;                                              // B hors de la chaîne : joue avec A
    col = q.collisions();
    CHECK(col.size() == 1 && (col[0].unitMask & 3) == 3);
}

static fs::path tempDir(const char* name) {
    fs::path d = fs::temp_directory_path() / name;
    std::error_code ec;
    fs::remove_all(d, ec);
    fs::create_directories(d, ec);
    return d;
}

static void testBank() {
    Bank b;
    Preset& p = b.slot(42);
    p.used = true;
    p.name = "Test preset  avec espaces";
    p.bpm = 133;
    p.rows[0].event.kind = EventKind::NoteAbsPause;
    p.rows[0].event.octaves = 3;
    p.rows[0].mode = PlayMode::Pendulum2;
    p.rows[0].first = 2; p.rows[0].last = 11; p.rows[0].gate = 12; p.rows[0].noteLen = 5;
    p.rows[0].pot[7] = 99; p.rows[0].mute[3] = true;
    p.rows[1].event.kind = EventKind::Velocity; p.rows[1].velocityTargets = 5;
    p.rows[2].event.kind = EventKind::FreeCC; p.rows[2].event.ccNumber = 111; p.rows[2].channel = 9;

    Bank c;
    CHECK(c.deserialize(b.serialize()));
    const Preset& q = c.slot(42);
    CHECK(q.used && q.name == p.name && q.bpm == 133);
    CHECK(!c.slot(41).used && !c.slot(43).used);
    CHECK(b.serialize().find("slot 42\n") != std::string::npos && b.serialize().find("slot 41\n") == std::string::npos);
    CHECK(q.rows[0].event.kind == EventKind::NoteAbsPause && q.rows[0].event.octaves == 3);
    CHECK(q.rows[0].mode == PlayMode::Pendulum2 && q.rows[0].first == 2 && q.rows[0].last == 11);
    CHECK(q.rows[0].gate == 12 && q.rows[0].noteLen == 5 && q.rows[0].pot[7] == 99 && q.rows[0].mute[3]);
    CHECK(q.rows[1].velocityTargets == 5);
    CHECK(q.rows[2].event.ccNumber == 111 && q.rows[2].channel == 9);
    CHECK(c.slot(99).rows[2].channel == 3);  // slot par défaut : canaux 1/2/3

    CHECK(!Bank().deserialize("pas une banque"));

    fs::path dir = tempDir("maq_test_bank");
    fs::path file = dir / "sous" / "dossier" / "ma_banque.maqbank";
    CHECK(b.saveToFile(file));
    Bank d;
    CHECK(d.loadFromFile(file));
    CHECK(d.slot(42).name == p.name);
    CHECK(!d.loadFromFile(dir / "inexistant.maqbank"));
}

static void testFolderSettings() {
    fs::path root = tempDir("maq_test_folders");
    fs::path def = root / "defaut";
    fs::create_directories(def);
    fs::path A = root / "A"; fs::create_directories(A);
    fs::path B = root / "B"; fs::create_directories(B);

    {   // Même dossier pour tout : choisir pour un usage le change pour tous
        FolderSettings s(def);
        CHECK(s.keepSameFolder());
        CHECK(s.folderFor(FolderPurpose::PresetExport) == def);  // rien de choisi : dossier par défaut
        s.setFolder(FolderPurpose::PresetExport, A);
        CHECK(s.folderFor(FolderPurpose::PresetImport) == A);
        CHECK(s.folderFor(FolderPurpose::BankSave) == A);
        CHECK(s.folderFor(FolderPurpose::SysexBackup) == A);
        CHECK(s.folderFor(FolderPurpose::MidiExport) == A);
    }
    {   // Dossiers séparés
        FolderSettings s(def);
        s.setKeepSameFolder(false);
        s.setFolder(FolderPurpose::PresetExport, A);
        s.setFolder(FolderPurpose::PresetImport, B);
        CHECK(s.folderFor(FolderPurpose::PresetExport) == A);
        CHECK(s.folderFor(FolderPurpose::PresetImport) == B);
        // passer en "même dossier" prend le dernier dossier choisi
        s.setKeepSameFolder(true);
        CHECK(s.folderFor(FolderPurpose::BankSave) == B);
        CHECK(s.folderFor(FolderPurpose::PresetExport) == B);
        // repasser en séparé conserve les dossiers par usage
        s.setKeepSameFolder(false);
        CHECK(s.folderFor(FolderPurpose::PresetExport) == A);
    }
    {   // Mémorisation entre sessions
        FolderSettings s(def);
        s.setKeepSameFolder(false);
        s.setFolder(FolderPurpose::PresetExport, A);
        s.setFolder(FolderPurpose::SysexBackup, B);
        fs::path file = root / "cfg" / "settings.ini";
        CHECK(s.save(file));
        FolderSettings t(def);
        CHECK(t.load(file));
        CHECK(!t.keepSameFolder());
        CHECK(t.folderFor(FolderPurpose::PresetExport) == A);
        CHECK(t.folderFor(FolderPurpose::SysexBackup) == B);
        CHECK(!t.load(root / "absent.ini"));
    }
    {   // Dossier mémorisé disparu : retombe sur le défaut et le signale
        FolderSettings s(def);
        fs::path gone = root / "disparu"; fs::create_directories(gone);
        s.setFolder(FolderPurpose::BankSave, gone);
        CHECK(!s.usingFallback(FolderPurpose::BankSave));
        fs::remove_all(gone);
        CHECK(s.usingFallback(FolderPurpose::BankSave));
        CHECK(s.folderFor(FolderPurpose::BankSave) == def);
    }
    CHECK(!FolderSettings::defaultSettingsFile().empty());
    CHECK(!FolderSettings::defaultPresetFolder().empty());
}

int main() {
    testValueMapping();
    testForwardNotes();
    testNoteLength();
    testModes();
    testPauseVelocityMute();
    testOtherEvents();
    testPanicAndStop();
    testBank();
    testFolderSettings();
    testEventTable();
    testNotesAndPresetHelpers();
    testFolderUtf8();
    testStringsAndHelp();
    testPreferences();
    testScales();
    testScaleCatalogue();
    testPotConversions();
    testLearn();
    testOutputQuantize();
    testCapture();
    testRowIdsAndLengths();
    testRenderAndMidiFile();
    testOnrChain();
    testRemoteProgramChange();
    testProjectParallelAndOutput();
    testSeriesChain();
    testLinks();
    testProjectExport();
    testChainAdvice();
    std::printf("%d verifications, %d echec(s)\n", g_checks, g_fail);
    return g_fail == 0 ? 0 : 1;
}
