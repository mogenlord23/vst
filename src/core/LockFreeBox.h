#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <atomic>

namespace mf
{
/** Generic version of SampleSlot for plain objects: the message thread publishes heap
    objects, the audio thread swaps them in (no allocation, no free, no locks) and retires the
    previous one through a lock-free FIFO that the message thread drains. */
template <typename T>
class LockFreeBox
{
public:
    ~LockFreeBox()
    {
        delete pending.exchange (nullptr);
        delete current;
        collectGarbage();
    }

    /** Message thread. Takes ownership. */
    void publish (T* obj)
    {
        if (auto* old = pending.exchange (obj, std::memory_order_acq_rel))
            delete old;
    }

    /** Message thread. */
    void collectGarbage()
    {
        const auto scope = fifo.read (fifo.getNumReady());
        for (int i = 0; i < scope.blockSize1; ++i) { delete retired[(size_t) (scope.startIndex1 + i)]; }
        for (int i = 0; i < scope.blockSize2; ++i) { delete retired[(size_t) (scope.startIndex2 + i)]; }
    }

    /** Audio thread, once per block. */
    const T* acquire() noexcept
    {
        if (auto* p = pending.exchange (nullptr, std::memory_order_acq_rel))
        {
            if (current != nullptr)
            {
                const auto scope = fifo.write (1);
                if (scope.blockSize1 > 0)
                    retired[(size_t) scope.startIndex1] = current; // else: leak rather than free here
            }
            current = p;
        }
        return current;
    }

private:
    static constexpr int kCapacity = 64;
    std::atomic<T*> pending { nullptr };
    T* current = nullptr;
    juce::AbstractFifo fifo { kCapacity };
    std::array<T*, kCapacity> retired {};
};
} // namespace mf
