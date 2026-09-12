#include <cache/cache.h>

namespace cache
{
    Cache &Cache::instance()
    {
        static Cache instance;
        return instance;
    }

    void Cache::set_strategy(std::unique_ptr<ICacheStrategy> strategy)
    {
        if (!strategy)
        {
            throw std::invalid_argument("Cache strategy cannot be null");
        }
        strategy_ = std::move(strategy);
    }

    void Cache::enforce_strategy()
    {
        if (!strategy_)
        {
            throw std::invalid_argument("Cache strategy cannot be null");
        }
    }

    bool Cache::put(std::string_view key, cache::CacheEntry value)
    {
        enforce_strategy();
        bool result = strategy_->put(key, value);
        return result;
    }

    CacheEntry Cache::get(std::string_view key)
    {
        enforce_strategy();
        CacheEntry entry = strategy_->get(key);
        return entry;
    }

    bool Cache::remove(std::string_view key)
    {
        enforce_strategy();
        bool result = strategy_->remove(key);
        return result;
    }

    bool Cache::exists(std::string_view key)
    {
        enforce_strategy();
        bool result = strategy_->exists(key);
        return result;
    }

    void Cache::clear()
    {
        enforce_strategy();
        strategy_->clear();
    }

    size_t Cache::size()
    {
        enforce_strategy();
        size_t size = strategy_->size();
        return size;
    }
}