#pragma once
#include "DnaProfile.h"
#include <array>
#include <atomic>

// Writers are serialised off the audio thread. Readers pin one immutable slot per block.
class DnaExchange
{
public:
    struct Read
    {
        DnaExchange& owner; int slot;
        Read(DnaExchange& o, int s) : owner(o), slot(s) {}
        Read(const Read&) = delete;
        ~Read() { if (slot >= 0) owner.readers[static_cast<std::size_t>(slot)].fetch_sub(1); }
        const DnaProfile* get() const noexcept { return slot >= 0 ? &owner.profiles[static_cast<std::size_t>(slot)] : nullptr; }
    };
    Read read() noexcept
    {
        for (int attempt = 0; attempt < 3; ++attempt)
        {
            const int slot = active.load();
            readers[static_cast<std::size_t>(slot)].fetch_add(1);
            if (slot == active.load()) return Read(*this, slot);
            readers[static_cast<std::size_t>(slot)].fetch_sub(1);
        }
        return Read(*this, -1);
    }
    bool publish(const DnaProfile& profile) noexcept
    {
        for (int slot = 0; slot < 4; ++slot)
            if (slot != active.load() && readers[static_cast<std::size_t>(slot)].load() == 0)
            {
                profiles[static_cast<std::size_t>(slot)] = profile;
                active.store(slot); return true;
            }
        return false;
    }
private:
    static_assert(std::atomic<int>::is_always_lock_free, "Audio profile pinning must be lock-free");
    std::array<DnaProfile, 4> profiles {};
    std::array<std::atomic<int>, 4> readers {};
    std::atomic<int> active { 0 };
};
