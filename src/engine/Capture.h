#pragma once
// Mémoire des derniers messages MIDI joués (bouton « Capture »).
// Un seul écrivain (le fil audio) ; la lecture se fait depuis l'interface.
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>
#include "Types.h"

namespace maq {

struct CapturedEvent {
    std::int64_t clock = 0;  // horloge MIDI (24 par noire)
    MidiMsg msg{};
};

class CaptureBuffer {
public:
    static constexpr std::size_t kCapacity = 16384;

    CaptureBuffer();  // la mémoire (environ 0,5 Mo) est réservée dans le tas : l'objet reste petit

    void push(std::int64_t clock, const MidiMsg& m);
    void setLastClock(std::int64_t clock) { lastClock_.store(clock, std::memory_order_relaxed); }
    std::int64_t lastClock() const { return lastClock_.load(std::memory_order_relaxed); }

    // Messages des « lengthClocks » dernières horloges, dates ramenées à 0..lengthClocks
    std::vector<CapturedEvent> window(std::int64_t lengthClocks) const;
    void clear();

private:
    std::vector<CapturedEvent> buf_;  // taille fixe, jamais réallouée : sûr pour une lecture pendant l'écriture
    std::atomic<std::uint64_t> count_{0};
    std::atomic<std::int64_t> lastClock_{0};
};

}  // namespace maq
