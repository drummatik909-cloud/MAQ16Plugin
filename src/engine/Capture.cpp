#include "Capture.h"

namespace maq {

CaptureBuffer::CaptureBuffer() : buf_(kCapacity) {}

void CaptureBuffer::push(std::int64_t clock, const MidiMsg& m) {
    const std::uint64_t n = count_.load(std::memory_order_relaxed);
    CapturedEvent& slot = buf_[static_cast<std::size_t>(n % kCapacity)];
    slot.clock = clock;
    slot.msg = m;
    count_.store(n + 1, std::memory_order_release);
}

std::vector<CapturedEvent> CaptureBuffer::window(std::int64_t lengthClocks) const {
    std::vector<CapturedEvent> out;
    const std::uint64_t n = count_.load(std::memory_order_acquire);
    const std::int64_t end = lastClock();
    const std::int64_t start = end - lengthClocks;
    const std::uint64_t first = n > kCapacity ? n - kCapacity : 0;
    for (std::uint64_t i = first; i < n; ++i) {
        CapturedEvent e = buf_[static_cast<std::size_t>(i % kCapacity)];
        if (e.clock < start || e.clock > end) continue;
        e.clock -= start;
        out.push_back(e);
    }
    return out;
}

void CaptureBuffer::clear() {
    count_.store(0, std::memory_order_release);
    lastClock_.store(0, std::memory_order_relaxed);
}

}  // namespace maq
