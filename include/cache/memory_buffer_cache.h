#pragma once

#include "cache/cache_strategy.h"
#include <unordered_map>
#include "lock/locker.h"
#include <string>

namespace cache
{
    class MemoryBufferCache : public ICacheStrategy
    {
    public:
        MemoryBufferCache() = default;
        ~MemoryBufferCache() override = default;

        bool put(std::string_view key, cache::CacheEntry value) override;
        cache::CacheEntry get(std::string_view key) override;
        bool remove(std::string_view key) override;
        bool exists(std::string_view key) override;
        void clear() override;
        size_t size() override;

    private:
        // Use an unordered_map to store cache entries in memory
        std::unordered_map<std::string, cache::CacheEntry> cache_;

        // Locker to ensure thread-safe access to the cache
        LOCKER mem_buf_lock_;
    };
}