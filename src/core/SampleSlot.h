#pragma once
#include "SampleData.h"
#include <array>
#include <atomic>

namespace mf
{
/** Lock-free hand-off of immutable sample buffers from the message thread to the audio thread.

    - publish()        : message thread. Wraps the shared_ptr in a heap "holder" and publishes it.
    - acquire()        : audio thread, once per block. Swaps in a pending holder (if any) and
                         pushes the previous one to a lock-free retire queue. No allocation,
                         no deallocation, no refcount changes on the audio thread.
    - collectGarbage() : message thread (timer). Deletes retired holders.
*/
class SampleSlot
{
public:
    using Holder = SamplePtr;

    SampleSlot() = default;
    ~SampleSlot()
    {
        delete pending.exchange (nullptr);
        delete current;
        collectGarbage();
    }

    void publish (SamplePtr s)
    {
        auto* h = new Holder (std::move (s));
        if (auto* old = pending.exchange (h, std::memory_order_acq_rel))
            delete old; // never seen by the audio thread
    }

    void collectGarbage()
    {
        int start1, size1, start2, size2;
        retireFifo.prepareToRead (retireFifo.getNumReady(), start1, size1, start2, size2);
        for (int i = 0; i < size1; ++i) { delete retired[(size_t) (start1 + i)]; retired[(size_t) (start1 + i)] = nullptr; }
        for (int i = 0; i < size2; ++i) { delete retired[(size_t) (start2 + i)]; retired[(size_t) (start2 + i)] = nullptr; }
        retireFifo.finishedRead (size1 + size2);
    }

    /** Audio thread. Returns the sample to play (may be nullptr). */
    const SampleData* acquire() noexcept
    {
        if (auto* h = pending.exchange (nullptr, std::memory_order_acq_rel))
        {
            if (current != nullptr)
            {
                int s1, n1, s2, n2;
                retireFifo.prepareToWrite (1, s1, n1, s2, n2);
                if (n1 > 0)
                    retired[(size_t) s1] = current;
                // else: queue full (should never happen) -> leak instead of freeing here
                retireFifo.finishedWrite (n1);
            }
            current = h;
            generation.fetch_add (1, std::memory_order_relaxed);
        }
        return current != nullptr ? current->get() : nullptr;
    }

    /** Audio thread only. */
    const SampleData* get() const noexcept { return current != nullptr ? current->get() : nullptr; }
    uint32_t getGeneration() const noexcept { return generation.load (std::memory_order_relaxed); }

private:
    static constexpr int kRetireCapacity = 128;
    std::atomic<Holder*> pending { nullptr };
    Holder* current = nullptr; // audio thread
    std::atomic<uint32_t> generation { 0 };
    juce::AbstractFifo retireFifo { kRetireCapacity };
    std::array<Holder*, kRetireCapacity> retired {};
};
} // namespace mf
