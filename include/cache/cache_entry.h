#pragma once

#include <string>
#include <cstddef>
#include <string_view>
#include <memory>

namespace cache
{
    /**
     * @brief Generic container for cached payloads.
     *
     * Owns the underlying string buffer or shared buffer to prevent dangling
     * pointers when entries are modified or evicted under concurrent operations.
     */
    struct CacheEntry
    {
        std::shared_ptr<std::string> payload;

        // Direct accessors matching raw buffer usage
        [[nodiscard]] const char *data() const noexcept
        {
            return payload ? payload->data() : nullptr;
        }

        [[nodiscard]] size_t size() const noexcept
        {
            return payload ? payload->size() : 0;
        }

        [[nodiscard]] std::string_view view() const noexcept
        {
            return payload ? std::string_view(*payload) : std::string_view();
        }

        [[nodiscard]] bool empty() const noexcept
        {
            return !payload || payload->empty();
        }

        [[nodiscard]] bool found() const noexcept
        {
            return payload != nullptr;
        }
    };
}