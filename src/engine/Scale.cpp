#include "Scale.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include "Events.h"

namespace maq {

namespace {

struct ScaleInfo {
    const char* fr;
    const char* en;
    unsigned mask;
};

constexpr unsigned bit(int n) { return 1u << n; }

const ScaleInfo kScales[kScaleCount] = {
    {"Chromatique", "Chromatic", 0xFFFu},
    {"Majeure", "Major", bit(0) | bit(2) | bit(4) | bit(5) | bit(7) | bit(9) | bit(11)},
    {"Mineure naturelle", "Natural minor", bit(0) | bit(2) | bit(3) | bit(5) | bit(7) | bit(8) | bit(10)},
    {"Mineure harmonique", "Harmonic minor", bit(0) | bit(2) | bit(3) | bit(5) | bit(7) | bit(8) | bit(11)},
    {"Mineure mélodique", "Melodic minor", bit(0) | bit(2) | bit(3) | bit(5) | bit(7) | bit(9) | bit(11)},
    {"Dorien", "Dorian", bit(0) | bit(2) | bit(3) | bit(5) | bit(7) | bit(9) | bit(10)},
    {"Phrygien", "Phrygian", bit(0) | bit(1) | bit(3) | bit(5) | bit(7) | bit(8) | bit(10)},
    {"Lydien", "Lydian", bit(0) | bit(2) | bit(4) | bit(6) | bit(7) | bit(9) | bit(11)},
    {"Mixolydien", "Mixolydian", bit(0) | bit(2) | bit(4) | bit(5) | bit(7) | bit(9) | bit(10)},
    {"Locrien", "Locrian", bit(0) | bit(1) | bit(3) | bit(5) | bit(6) | bit(8) | bit(10)},
    {"Pentatonique majeure", "Major pentatonic", bit(0) | bit(2) | bit(4) | bit(7) | bit(9)},
    {"Pentatonique mineure", "Minor pentatonic", bit(0) | bit(3) | bit(5) | bit(7) | bit(10)},
    {"Blues", "Blues", bit(0) | bit(3) | bit(5) | bit(6) | bit(7) | bit(10)},
    {"Ton entier", "Whole tone", bit(0) | bit(2) | bit(4) | bit(6) | bit(8) | bit(10)},
    {"Diminuée", "Diminished", bit(0) | bit(1) | bit(3) | bit(4) | bit(6) | bit(7) | bit(9) | bit(10)},
};

int clampType(int type) { return std::max(0, std::min(kScaleCount - 1, type)); }
int mod12(int v) { return ((v % 12) + 12) % 12; }

bool isPauseKind(EventKind k) { return k == EventKind::NoteAbsPause || k == EventKind::NoteRelPause; }

// Ramène une note dans [lo, hi] en conservant sa classe de hauteur si possible
int foldNote(int note, int lo, int hi) {
    if (hi - lo >= 11) {
        while (note < lo) note += 12;
        while (note > hi) note -= 12;
    }
    return std::max(lo, std::min(hi, note));
}

}  // namespace

unsigned scaleMask(int type) { return kScales[clampType(type)].mask; }

bool inScale(int midiNote, int root, int type) {
    return (scaleMask(type) & bit(mod12(midiNote - root))) != 0;
}

int quantizeNote(int midiNote, int root, int type) {
    const int n = std::max(0, std::min(127, midiNote));
    for (int d = 0; d < 12; ++d) {
        if (n - d >= 0 && inScale(n - d, root, type)) return n - d;
        if (n + d <= 127 && inScale(n + d, root, type)) return n + d;
    }
    return n;
}

int quantizeNoteInRange(int midiNote, int lo, int hi, int root, int type) {
    const int n = std::max(lo, std::min(hi, midiNote));
    for (int d = 0; d < 12; ++d) {
        if (n - d >= lo && inScale(n - d, root, type)) return n - d;
        if (n + d <= hi && inScale(n + d, root, type)) return n + d;
    }
    return n;
}

std::string scaleName(int type, Lang lang) {
    const ScaleInfo& s = kScales[clampType(type)];
    return lang == Lang::French ? s.fr : s.en;
}

std::string pitchClassName(int pitchClass, bool french) {
    static const char* const en[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    static const char* const fr[12] = {"Do", "Do#", "Ré", "Ré#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si"};
    const int pc = mod12(pitchClass);
    return french ? fr[pc] : en[pc];
}

int potForNote(int octaves, int note, bool avoidPause) {
    octaves = std::max(1, std::min(5, octaves));
    if (note < 36 || note > 36 + 12 * octaves) return -1;
    const int maxPot = avoidPause ? 126 : 127;
    const int centre = static_cast<int>(std::lround((note - 36) * 127.0 / (12.0 * octaves)));
    for (int d = 0; d <= 3; ++d) {
        for (int sign : {-1, +1}) {
            const int p = centre + sign * d;
            if (p >= 0 && p <= maxPot && noteFromPot(octaves, p) == note) return p;
        }
    }
    for (int p = 0; p <= maxPot; ++p)
        if (noteFromPot(octaves, p) == note) return p;
    return -1;
}

int potForOffset(int octaves, int offset, bool avoidPause) {
    octaves = std::max(1, std::min(5, octaves));
    if (offset < 0 || offset > 12 * octaves) return -1;
    const int maxPot = avoidPause ? 126 : 127;
    const int centre = static_cast<int>(std::lround(offset * 127.0 / (12.0 * octaves)));
    for (int d = 0; d <= 3; ++d) {
        for (int sign : {-1, +1}) {
            const int p = centre + sign * d;
            if (p >= 0 && p <= maxPot && noteOffsetFromPot(octaves, p) == offset) return p;
        }
    }
    for (int p = 0; p <= maxPot; ++p)
        if (noteOffsetFromPot(octaves, p) == offset) return p;
    return -1;
}

int snapPot(const EventConfig& e, int pot, int root, int type) {
    if (!isAbsoluteNoteEvent(e)) return pot;
    const int lo = 36, hi = 36 + 12 * e.octaves;
    const int note = noteFromPot(e.octaves, pot);
    if (isPauseValue(e, pot)) return pot;  // la pause reste une pause
    const int q = quantizeNoteInRange(note, lo, hi, root, type);
    if (q == note) return pot;
    const int p = potForNote(e.octaves, q, isPauseKind(e.kind));
    return p >= 0 ? p : pot;
}

int learnPot(const EventConfig& e, int note, int& reference, bool lockToScale, int root, int type) {
    const bool pause = isPauseKind(e.kind);
    if (isAbsoluteNoteEvent(e)) {
        const int lo = 36, hi = 36 + 12 * e.octaves;
        int n = foldNote(note, lo, hi);
        if (lockToScale) n = quantizeNoteInRange(n, lo, hi, root, type);
        return potForNote(e.octaves, n, pause);
    }
    if (e.kind == EventKind::NoteRel || e.kind == EventKind::NoteRelPause) {
        if (reference < 0) reference = note;  // la première note jouée sert de référence
        const int hi = 12 * e.octaves;
        const int offset = foldNote(note - reference, 0, hi);
        return potForOffset(e.octaves, offset, pause);
    }
    return -1;
}

}  // namespace maq
