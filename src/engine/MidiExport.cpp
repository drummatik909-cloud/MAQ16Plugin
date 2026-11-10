#include "MidiExport.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <string>

namespace maq {

namespace {

constexpr int kTicksPerQuarter = 480;
constexpr int kTicksPerClock = kTicksPerQuarter / 24;  // 20

void putU16(std::vector<std::uint8_t>& v, unsigned x) {
    v.push_back(static_cast<std::uint8_t>((x >> 8) & 0xFF));
    v.push_back(static_cast<std::uint8_t>(x & 0xFF));
}

void putU32(std::vector<std::uint8_t>& v, std::uint32_t x) {
    v.push_back(static_cast<std::uint8_t>((x >> 24) & 0xFF));
    v.push_back(static_cast<std::uint8_t>((x >> 16) & 0xFF));
    v.push_back(static_cast<std::uint8_t>((x >> 8) & 0xFF));
    v.push_back(static_cast<std::uint8_t>(x & 0xFF));
}

void putVlq(std::vector<std::uint8_t>& v, std::uint32_t x) {
    std::uint8_t tmp[5];
    int n = 0;
    tmp[n++] = static_cast<std::uint8_t>(x & 0x7F);
    while ((x >>= 7) != 0) tmp[n++] = static_cast<std::uint8_t>((x & 0x7F) | 0x80);
    while (n > 0) v.push_back(tmp[--n]);
}

void appendChunk(std::vector<std::uint8_t>& file, const std::vector<std::uint8_t>& track) {
    file.push_back('M'); file.push_back('T'); file.push_back('r'); file.push_back('k');
    putU32(file, static_cast<std::uint32_t>(track.size()));
    file.insert(file.end(), track.begin(), track.end());
}

std::int64_t lcm64(std::int64_t a, std::int64_t b) {
    if (a == 0 || b == 0) return std::max(a, b);
    return a / std::gcd(a, b) * b;
}

int clampi(int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); }

struct Timed {
    std::int64_t tick;
    std::vector<std::uint8_t> bytes;  // message sans le temps delta
};

}  // namespace

std::int64_t rowPeriodClocks(const RowConfig& r) {
    if (r.event.kind == EventKind::Off) return 0;
    const int first = clampi(r.first, 1, kSteps);
    int last = clampi(r.last, 1, kSteps);
    if (last < first) last = first;
    const std::int64_t n = last - first + 1;
    const std::int64_t g = std::max(1, r.gate);
    switch (r.mode) {
        case PlayMode::Pendulum1: return 2 * n * g;
        case PlayMode::Pendulum2: return n == 1 ? g : (2 * n - 2) * g;
        case PlayMode::OneEvent: return g;
        default: return n * g;
    }
}

std::int64_t loopLengthClocks(const std::array<RowConfig, kRows>& rows) {
    std::int64_t best = 0;
    for (const auto& r : rows) best = std::max(best, rowPeriodClocks(r));
    return best > 0 ? best : kClocksPerBar;
}

std::int64_t cycleLengthClocks(const std::array<RowConfig, kRows>& rows, std::int64_t capClocks) {
    std::int64_t l = 0;
    for (const auto& r : rows) {
        const std::int64_t p = rowPeriodClocks(r);
        if (p == 0) continue;
        l = lcm64(l, p);
        if (l > capClocks) return capClocks;
    }
    return l > 0 ? l : kClocksPerBar;
}

std::vector<CapturedEvent> renderLoop(Unit unit, std::int64_t clocks) {
    std::vector<CapturedEvent> events;
    std::vector<MidiMsg> out;
    unit.start();
    for (std::int64_t c = 0; c < clocks; ++c) {
        out.clear();
        unit.tick(out);
        for (const auto& m : out) events.push_back({c, m});
    }
    out.clear();
    unit.stop(out);  // ferme les notes encore ouvertes
    for (const auto& m : out) events.push_back({clocks, m});
    return events;
}

std::vector<std::uint8_t> buildMidiFile(const std::vector<CapturedEvent>& events, double bpm, int rowMask,
                                        std::int64_t lengthClocks) {
    const std::int64_t endTick = std::max<std::int64_t>(0, lengthClocks) * kTicksPerClock;

    std::vector<std::uint8_t> file;
    int rowsUsed = 0;
    for (int r = 0; r < kRows; ++r)
        if (rowMask & (1 << r)) ++rowsUsed;

    // En-tête
    file.push_back('M'); file.push_back('T'); file.push_back('h'); file.push_back('d');
    putU32(file, 6);
    putU16(file, 1);                                   // format 1
    putU16(file, static_cast<unsigned>(1 + rowsUsed));  // nombre de pistes
    putU16(file, kTicksPerQuarter);

    // Piste 0 : tempo et mesure à 4/4
    {
        std::vector<std::uint8_t> t;
        const double safeBpm = bpm > 1.0 ? bpm : 120.0;
        const std::uint32_t usPerQuarter = static_cast<std::uint32_t>(std::lround(60000000.0 / safeBpm));
        putVlq(t, 0);
        t.push_back(0xFF); t.push_back(0x51); t.push_back(0x03);
        t.push_back(static_cast<std::uint8_t>((usPerQuarter >> 16) & 0xFF));
        t.push_back(static_cast<std::uint8_t>((usPerQuarter >> 8) & 0xFF));
        t.push_back(static_cast<std::uint8_t>(usPerQuarter & 0xFF));
        putVlq(t, 0);
        t.push_back(0xFF); t.push_back(0x58); t.push_back(0x04);
        t.push_back(4); t.push_back(2); t.push_back(24); t.push_back(8);
        putVlq(t, static_cast<std::uint32_t>(endTick));
        t.push_back(0xFF); t.push_back(0x2F); t.push_back(0x00);
        appendChunk(file, t);
    }

    for (int r = 0; r < kRows; ++r) {
        if (!(rowMask & (1 << r))) continue;

        std::vector<Timed> msgs;
        int active[16][128] = {};  // notes ouvertes par canal
        for (const auto& e : events) {
            if (e.msg.row != r) continue;
            const std::int64_t tick = std::min(endTick, std::max<std::int64_t>(0, e.clock) * kTicksPerClock);
            const int ch = clampi(e.msg.channel, 0, 15);
            Timed t;
            t.tick = tick;
            switch (e.msg.type) {
                case MidiMsg::Type::NoteOn: {
                    const int n = clampi(e.msg.data1, 0, 127);
                    ++active[ch][n];
                    t.bytes = {static_cast<std::uint8_t>(0x90 | ch), static_cast<std::uint8_t>(n),
                               static_cast<std::uint8_t>(clampi(e.msg.data2, 1, 127))};
                    break;
                }
                case MidiMsg::Type::NoteOff: {
                    const int n = clampi(e.msg.data1, 0, 127);
                    if (active[ch][n] == 0) continue;  // Note Off orphelin (note commencée avant la fenêtre)
                    --active[ch][n];
                    t.bytes = {static_cast<std::uint8_t>(0x80 | ch), static_cast<std::uint8_t>(n), 0};
                    break;
                }
                case MidiMsg::Type::ControlChange:
                    t.bytes = {static_cast<std::uint8_t>(0xB0 | ch), static_cast<std::uint8_t>(clampi(e.msg.data1, 0, 127)),
                               static_cast<std::uint8_t>(clampi(e.msg.data2, 0, 127))};
                    break;
                case MidiMsg::Type::PitchBend: {
                    const int v = clampi(e.msg.data1, 0, 16383);
                    t.bytes = {static_cast<std::uint8_t>(0xE0 | ch), static_cast<std::uint8_t>(v & 0x7F),
                               static_cast<std::uint8_t>((v >> 7) & 0x7F)};
                    break;
                }
                case MidiMsg::Type::ChannelPressure:
                    t.bytes = {static_cast<std::uint8_t>(0xD0 | ch), static_cast<std::uint8_t>(clampi(e.msg.data1, 0, 127))};
                    break;
                case MidiMsg::Type::ProgramChange:
                    t.bytes = {static_cast<std::uint8_t>(0xC0 | ch), static_cast<std::uint8_t>(clampi(e.msg.data1, 0, 127))};
                    break;
            }
            msgs.push_back(std::move(t));
        }
        // Notes encore ouvertes : fermées à la fin
        for (int ch = 0; ch < 16; ++ch)
            for (int n = 0; n < 128; ++n)
                for (int k = 0; k < active[ch][n]; ++k)
                    msgs.push_back({endTick, {static_cast<std::uint8_t>(0x80 | ch), static_cast<std::uint8_t>(n), 0}});

        std::stable_sort(msgs.begin(), msgs.end(), [](const Timed& a, const Timed& b) { return a.tick < b.tick; });

        std::vector<std::uint8_t> t;
        const std::string name = "Row " + std::to_string(r + 1);
        putVlq(t, 0);
        t.push_back(0xFF); t.push_back(0x03); t.push_back(static_cast<std::uint8_t>(name.size()));
        t.insert(t.end(), name.begin(), name.end());
        std::int64_t last = 0;
        for (const auto& m : msgs) {
            putVlq(t, static_cast<std::uint32_t>(m.tick - last));
            last = m.tick;
            t.insert(t.end(), m.bytes.begin(), m.bytes.end());
        }
        putVlq(t, static_cast<std::uint32_t>(std::max<std::int64_t>(0, endTick - last)));
        t.push_back(0xFF); t.push_back(0x2F); t.push_back(0x00);
        appendChunk(file, t);
    }
    return file;
}

}  // namespace maq
