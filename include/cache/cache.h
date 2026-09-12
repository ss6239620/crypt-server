#pragma once

#include "cache/cache_strategy.h"
#include <memory>
#include <cstddef>
#include <string_view>
#include <stdexcept>

namespace cache
{
    class Cache
    {
    public:
        static Cache &instance();

        // Prevent copying and assignment
        Cache(const Cache &) = delete;
        Cache &operator=(const Cache &) = delete;

        /**
         * @brief Swap or configure the active strategy at runtime or startup.
         */
        void set_strategy(std::unique_ptr<ICacheStrategy> strategy);

        bool put(std::string_view key, cache::CacheEntry value);
        CacheEntry get(std::string_view key);
        bool remove(std::string_view key);
        bool exists(std::string_view key);
        void clear();
        size_t size();

        void enforce_strategy();

    private:
        Cache() = default;
        std::unique_ptr<ICacheStrategy> strategy_; // Pointer to the active caching strategy
    };
}