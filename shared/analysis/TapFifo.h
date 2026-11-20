#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <vector>

namespace na::analysis
{

/** Lock-free single-producer / single-consumer FIFO of aligned sample frames.

    Each frame holds `NumStreams` floats (e.g. in, out, gain reduction), so the
    streams always stay sample-aligned. The audio thread pushes, the analysis
    side pops. Memory is allocated once in the constructor; push() and pop()
    never allocate, lock or block. If the reader falls behind, push() drops the
    block and counts it rather than waiting.
*/
template <size_t NumStreams>
class TapFifo
{
public:
    /** capacity is rounded up to a power of two. */
    explicit TapFifo (size_t capacityFrames)
    {
        size_t cap = 1;
        while (cap < capacityFrames) cap <<= 1;
        mask = cap - 1;
        for (auto& s : streams) s.assign (cap, 0.0f);
    }

    size_t capacity() const noexcept { return mask + 1; }

    /** Audio thread. streamData[k] points at n samples of stream k.
        Returns false (and drops the block) if there isn't room. */
    bool push (const float* const* streamData, size_t n) noexcept
    {
        const auto w = writePos.load (std::memory_order_relaxed);
        const auto r = readPos.load (std::memory_order_acquire);
        if (capacity() - (w - r) < n)
        {
            dropped.fetch_add (1, std::memory_order_relaxed);
            return false;
        }
        for (size_t k = 0; k < NumStreams; ++k)
            for (size_t i = 0; i < n; ++i)
                streams[k][(w + i) & mask] = streamData[k][i];
        writePos.store (w + n, std::memory_order_release);
        return true;
    }

    /** Analysis side. Copies up to maxFrames into dest[k] and returns how many. */
    size_t pop (float* const* dest, size_t maxFrames) noexcept
    {
        const auto r = readPos.load (std::memory_order_relaxed);
        const auto w = writePos.load (std::memory_order_acquire);
        const auto n = std::min (maxFrames, (size_t) (w - r));
        for (size_t k = 0; k < NumStreams; ++k)
            for (size_t i = 0; i < n; ++i)
                dest[k][i] = streams[k][(r + i) & mask];
        readPos.store (r + n, std::memory_order_release);
        return n;
    }

    /** Analysis side: throw away everything waiting. */
    void discardAll() noexcept { readPos.store (writePos.load (std::memory_order_acquire), std::memory_order_release); }

    size_t numReady() const noexcept { return (size_t) (writePos.load (std::memory_order_acquire) - readPos.load (std::memory_order_acquire)); }

    /** Blocks dropped because the reader fell behind (debug info). */
    unsigned getDroppedBlocks() const noexcept { return dropped.load (std::memory_order_relaxed); }

private:
    std::vector<float> streams[NumStreams];
    size_t mask = 0;
    std::atomic<size_t> writePos { 0 }, readPos { 0 };
    std::atomic<unsigned> dropped { 0 };
};

} // namespace na::analysis
