#include "Unit.h"
#include "Scale.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace maq {

namespace {
constexpr int kPitchPlateau = 3;  // demi-largeur du plateau central (hypothèse, à valider sur la machine)
bool isNoteKind(EventKind k) {
    return k == EventKind::NoteAbs || k == EventKind::NoteRel ||
           k == EventKind::NoteAbsPause || k == EventKind::NoteRelPause;
}
bool isPauseKind(EventKind k) { return k == EventKind::NoteAbsPause || k == EventKind::NoteRelPause; }
bool isRelativeKind(EventKind k) { return k == EventKind::NoteRel || k == EventKind::NoteRelPause; }
int clampi(int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); }
}  // namespace

int noteOffsetFromPot(int octaves, int pot) {
    octaves = clampi(octaves, 1, 5);
    pot = clampi(pot, 0, 127);
    return static_cast<int>(std::lround(pot * 12.0 * octaves / 127.0));
}

int noteFromPot(int octaves, int pot) { return 36 + noteOffsetFromPot(octaves, pot); }

int fixedCcNumber(int index) {
    index = clampi(index, 0, 31);
    if (index < 20) return index;
    if (index < 31) return 64 + (index - 20);
    return 84;
}

int pitchBendFromPot(EventKind kind, int pot) {
    pot = clampi(pot, 0, 127);
    switch (kind) {
        case EventKind::PitchBendPos: return 64 + static_cast<int>(std::lround(pot * 63.0 / 127.0));
        case EventKind::PitchBendNeg: return static_cast<int>(std::lround(pot * 64.0 / 127.0));
        default:
            if (std::abs(pot - 64) <= kPitchPlateau) return 64;
            return pot;
    }
}

Unit::Unit() {
    relativeBaseRow.fill(-1);
    finished_.reserve(8);
    resetStates();
}

int Unit::baseFor(int r) const { return relativeBaseRow[static_cast<size_t>(r)] >= 0 ? relativeBaseRow[static_cast<size_t>(r)] : relativeBase; }

void Unit::setRelativeBase(int row, int note) {
    note = clampi(note, 0, 127);
    if (row < 0) relativeBaseRow.fill(note);
    else if (row < kRows) relativeBaseRow[static_cast<size_t>(row)] = note;
}

bool Unit::isOnr(int r) const {
    return rows[static_cast<size_t>(r)].mode == PlayMode::OneShotRetrigger && rows[static_cast<size_t>(r)].event.kind != EventKind::Off;
}

// Rangée Onr suivante (circulaire) ; la rangée elle-même s'il n'y en a pas d'autre ; -1 si aucune
int Unit::nextOnrRow(int r) const {
    for (int k = 1; k <= kRows; ++k) {
        const int c = (r + k) % kRows;
        if (isOnr(c)) return c;
    }
    return -1;
}

// Au départ, seule la première rangée Onr tourne : les autres attendent que la précédente les déclenche
void Unit::applyOnrHead() {
    if (!chainInsideUnit) return;
    int head = -1;
    for (int r = 0; r < kRows; ++r) {
        if (!isOnr(r)) continue;
        if (head < 0) head = r;
        else st_[static_cast<size_t>(r)].running = false;
    }
}

bool Unit::anyRowRunning() const {
    for (int r = 0; r < kRows; ++r)
        if (rows[static_cast<size_t>(r)].event.kind != EventKind::Off && st_[static_cast<size_t>(r)].running) return true;
    return false;
}

void Unit::holdRow(int r) {
    if (r >= 0 && r < kRows) st_[static_cast<size_t>(r)].running = false;
}

void Unit::triggerRow(int r, std::vector<MidiMsg>& out) {
    if (!playing_ || r < 0 || r >= kRows || rows[static_cast<size_t>(r)].event.kind == EventKind::Off) return;
    RowState& s = st_[static_cast<size_t>(r)];
    s.t = 0; s.phase = 0; s.running = true;
    fireRow(r, out);
    s.phase = 1 % std::max(1, rows[static_cast<size_t>(r)].gate);
}

void Unit::triggerUnit(std::vector<MidiMsg>& out) {
    if (!playing_) return;
    for (int r = 0; r < kRows; ++r) {  // on garde la note en cours : son Note Off doit encore sortir
        RowState& s = st_[static_cast<size_t>(r)];
        s.t = 0; s.phase = 0; s.running = true; s.hasCurrent = false;
    }
    applyOnrHead();
    for (int r = 0; r < kRows; ++r)
        if (st_[static_cast<size_t>(r)].running) triggerRow(r, out);
}

uint32_t Unit::nextRandom(uint32_t& s) {
    if (s == 0) s = 1;
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    return s;
}

void Unit::seedRandom(uint32_t seed) {
    seed_ = seed ? seed : 1;
    for (int r = 0; r < kRows; ++r) st_[r].rng = seed_ + 7919u * (r + 1);
}

void Unit::resetStates() {
    for (int r = 0; r < kRows; ++r) {
        uint32_t keep = st_[r].rng;
        st_[r] = RowState{};
        st_[r].rng = (keep > 1) ? keep : seed_ + 7919u * (r + 1);
    }
}

void Unit::start() { resetStates(); applyOnrHead(); playing_ = true; }
void Unit::continuePlaying() { playing_ = true; }

void Unit::noteOffIfActive(int r, std::vector<MidiMsg>& out) {
    RowState& s = st_[r];
    if (s.noteActive) {
        out.push_back({MidiMsg::Type::NoteOff, s.noteChannel, s.note, 0, r});
        s.noteActive = false;
    }
}

void Unit::stop(std::vector<MidiMsg>& out) {
    for (int r = 0; r < kRows; ++r) noteOffIfActive(r, out);
    playing_ = false;
}

void Unit::panic(std::vector<MidiMsg>& out) {
    for (int r = 0; r < kRows; ++r) noteOffIfActive(r, out);
    bool used[16] = {false};
    for (const auto& row : rows)
        if (row.event.kind != EventKind::Off) used[clampi(row.channel, 1, 16) - 1] = true;
    for (int c = 0; c < 16; ++c)
        if (used[c]) out.push_back({MidiMsg::Type::ControlChange, c, 123, 0, -1});
}

int Unit::currentStep(int row) const { return st_[clampi(row, 0, kRows - 1)].lastStep; }

int Unit::position(int r) {
    const RowConfig& cfg = rows[r];
    RowState& s = st_[r];
    int first = clampi(cfg.first, 1, kSteps);
    int last = clampi(cfg.last, 1, kSteps);
    if (last < first) last = first;
    const int n = last - first + 1;
    const int t = s.t;
    switch (cfg.mode) {
        case PlayMode::Forward: return first + (t % n);
        case PlayMode::Backward: return last - (t % n);
        case PlayMode::Pendulum1: {
            int k = t % (2 * n);
            return k < n ? first + k : last - (k - n);
        }
        case PlayMode::Pendulum2: {
            if (n == 1) return first;
            int k = t % (2 * n - 2);
            return k < n ? first + k : last - (k - (n - 1));
        }
        case PlayMode::Random: return first + static_cast<int>(nextRandom(s.rng) % static_cast<uint32_t>(n));
        case PlayMode::OneShotSingle:
        case PlayMode::OneShotRetrigger:
            return t < n ? first + t : -1;
        case PlayMode::OneEvent: return first;
    }
    return first;
}

void Unit::fireRow(int r, std::vector<MidiMsg>& out) {
    const RowConfig& cfg = rows[r];
    RowState& s = st_[r];
    const int idx = position(r);
    if (idx < 0) { s.running = false; finished_.push_back(r); return; }
    s.lastStep = idx;
    s.t++;
    const int pot = clampi(cfg.pot[idx - 1], 0, 127);
    const bool muted = cfg.mute[idx - 1];
    const int ch = clampi(cfg.channel, 1, 16) - 1;
    const EventKind k = cfg.event.kind;

    if (k == EventKind::Velocity) { s.currentPot = pot; s.hasCurrent = true; return; }
    if (muted || cfg.rowMute) return;

    if (isNoteKind(k)) {
        if (isPauseKind(k) && pot == 127) return;
        int note;
        if (isRelativeKind(k)) note = clampi(baseFor(r) + noteOffsetFromPot(cfg.event.octaves, pot), 0, 127);
        else note = clampi(noteFromPot(cfg.event.octaves, pot), 0, 127);
        if (scaleMode == kScaleModeOutput) note = quantizeNote(note, scaleRoot, scaleType);
        int vel = 100;
        for (int v = 0; v < kRows; ++v) {
            if (rows[v].event.kind == EventKind::Velocity && (rows[v].velocityTargets & (1 << r)) && st_[v].hasCurrent) {
                vel = st_[v].currentPot;
                break;
            }
        }
        if (vel == 0) return;  // vélocité 0 = pause
        noteOffIfActive(r, out);
        out.push_back({MidiMsg::Type::NoteOn, ch, note, vel, r});
        s.noteActive = true; s.note = note; s.noteChannel = ch;
        const int g = std::max(1, cfg.gate);
        s.noteLeft = (cfg.noteLen <= 1) ? g : std::min(cfg.noteLen, g);
        return;
    }
    switch (k) {
        case EventKind::FreeCC:
            out.push_back({MidiMsg::Type::ControlChange, ch, clampi(cfg.event.ccNumber, 0, 127), pot, r}); break;
        case EventKind::FixedCC:
            out.push_back({MidiMsg::Type::ControlChange, ch, fixedCcNumber(cfg.event.ccNumber), pot, r}); break;
        case EventKind::PitchBend:
        case EventKind::PitchBendPos:
        case EventKind::PitchBendNeg:
            out.push_back({MidiMsg::Type::PitchBend, ch, pitchBendFromPot(k, pot) << 7, 0, r}); break;
        case EventKind::AfterTouch:
            out.push_back({MidiMsg::Type::ChannelPressure, ch, pot, 0, r}); break;
        case EventKind::ProgramChange:
            out.push_back({MidiMsg::Type::ProgramChange, ch, pot, 0, r}); break;
        default: break;
    }
}

void Unit::tick(std::vector<MidiMsg>& out) {
    if (!playing_) return;
    finished_.clear();
    // 1. fin des notes en cours
    for (int r = 0; r < kRows; ++r) {
        RowState& s = st_[r];
        if (s.noteActive && --s.noteLeft <= 0) noteOffIfActive(r, out);
    }
    // 2. pas : d'abord les rangées de vélocité (modificateurs), puis les autres
    for (int pass = 0; pass < 2; ++pass) {
        for (int r = 0; r < kRows; ++r) {
            const bool isMod = rows[r].event.kind == EventKind::Velocity;
            if ((pass == 0) != isMod) continue;
            if (rows[r].event.kind == EventKind::Off || !st_[r].running) continue;
            RowState& s = st_[r];
            if (s.phase == 0) fireRow(r, out);
            s.phase = (s.phase + 1) % std::max(1, rows[r].gate);
        }
    }
    // 3. chaînage Onr entre les rangées de l'unité (sans allocation : au plus kRows rangées terminées)
    if (chainInsideUnit) {
        int done[kRows];
        int nd = 0;
        for (int f : finished_)
            if (nd < kRows) done[nd++] = f;
        for (int i = 0; i < nd; ++i) {
            const int f = done[i];
            if (!isOnr(f)) continue;
            const int next = nextOnrRow(f);
            if (next < 0) continue;
            if (!wrapOnr && next <= f) continue;  // fin de la chaîne
            triggerRow(next, out);
        }
    }
}

}  // namespace maq
