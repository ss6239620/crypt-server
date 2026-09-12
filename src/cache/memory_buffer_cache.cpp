#include "cache/memory_buffer_cache.h"

namespace cache
{

    bool MemoryBufferCache::put(std::string_view key, cache::CacheEntry value)
    {
        mem_buf_lock_.lock();
        cache_[std::string(key)] = std::move(value);
        mem_buf_lock_.unlock();
        return true;
    }

    CacheEntry MemoryBufferCache::get(std::string_view key)
    {
        mem_buf_lock_.lock();
        auto it = cache_.find(std::string(key));
        if (it == cache_.end())
        {
            mem_buf_lock_.unlock();
            return {};
        }
        CacheEntry entry = it->second;
        mem_buf_lock_.unlock();
        return entry;
    }

    bool MemoryBufferCache::remove(std::string_view key){
        mem_buf_lock_.lock();
        bool result = cache_.erase(std::string(key))>0;
        mem_buf_lock_.unlock();
        return result;
    }

    bool MemoryBufferCache::exists(std::string_view key){
        mem_buf_lock_.lock();
        bool result = cache_.find(std::string(key)) != cache_.end();
        mem_buf_lock_.unlock();
        return result;
    }

    void MemoryBufferCache::clear(){
         mem_buf_lock_.lock();
        cache_.clear();
        mem_buf_lock_.unlock();
    }

    size_t MemoryBufferCache::size(){
        mem_buf_lock_.lock();
        size_t result = cache_.size();
        mem_buf_lock_.unlock();
        return result;
    }

}