#pragma once

#include "cache/cache_entry.h"
#include <string>
#include <string_view>

namespace cache {
class ICacheStrategy {
public:
    virtual ~ICacheStrategy() = default;

    /**
     * @brief Store an item in the cache.
     * @param key Unique identifier (e.g. "response:/about")
     * @param value Raw byte/string content to store
     */
    virtual bool put(std::string_view key, cache::CacheEntry value) = 0;

    /**
     * @brief Retrieve an item from the cache.
     * @param key Unique identifier (e.g. "response:/about")
     * @return CacheEntry containing the cached value and metadata
     */
    virtual cache::CacheEntry get(std::string_view key) = 0;

    /**
     * @brief Remove an item from the cache.
     * @param key Unique identifier (e.g. "response:/about")
     */
    virtual bool remove(std::string_view key) = 0;

    /**
     * @brief Check if an item exists in the cache.
     * @param key Unique identifier (e.g. "response:/about")
     * @return true if the item exists, false otherwise
     */
    virtual bool exists(std::string_view key) = 0;

    /**
     * @brief Clear all items from the cache.
     */
    virtual void clear() = 0;

    /**
     * @brief Get the number of items currently stored in the cache.
     * @return Number of items in the cache
     */
    virtual size_t size() = 0;
};
}