// Tests du moteur (sans dépendance externe).
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>
#include "engine/Unit.h"
#include "engine/Events.h"
#include "presets/Bank.h"
#include "presets/FolderSettings.h"

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
    std::printf("%d verifications, %d echec(s)\n", g_checks, g_fail);
    return g_fail == 0 ? 0 : 1;
}
