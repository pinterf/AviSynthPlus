#ifndef AVS_LRUCACHE_H
#define AVS_LRUCACHE_H

#include <mutex>
#include <condition_variable>
#include <memory>
#include <cassert>
#include "ObjectPool.h"
#include "SimpleLruCache.h"
#include "InternalEnvironment.h"

enum LruLookupResult
{
  LRU_LOOKUP_NOT_FOUND,          // Item has not been found, storage is reserved to be used by caller
  LRU_LOOKUP_FOUND_AND_READY,    // Item has been found and returned
  LRU_LOOKUP_FOUND_BUT_NOTAVAIL, // Item has been found, but is waiting for completion and is not yet ready
  LRU_LOOKUP_NO_CACHE            // Item will not be cached, no storage is returned
};

template<typename K, typename V>
class LruCache : public std::enable_shared_from_this<LruCache<K, V> >
{
private:
  enum LruEntryState
  {
    LRU_ENTRY_EMPTY,
    LRU_ENTRY_AVAILABLE,
    LRU_ENTRY_ROLLED_BACK
  };

  struct LruGhostEntry
  {
    K key;
    size_t ghosted;

    LruGhostEntry() :
      key(0), ghosted(0)
    {
    }

    LruGhostEntry(K key, size_t ghosted) :
      key(key), ghosted(ghosted)
    {
    }
  };

  struct LruEntry
  {
    K key;
    V value;
    size_t locks;      // the number of threads waiting on this entry. used to prevent eviction when readers are waiting on it
    size_t ghosted;    // the number of times this entry has entered the ghost list
    std::condition_variable ready_cond;
    enum LruEntryState state;

    LruEntry(const K& key)
    {
      reset(key, V());
    }

    void reset(const K& k, const V& v)
    {
      key = k;
      value = v;
      locks = 0;
      ghosted = 0;
      state = LRU_ENTRY_EMPTY;
    }

  private:
    LruEntry(const LruEntry&);
    LruEntry& operator=(const LruEntry&);
  };

  const int GHOSTS_MIN_CAPACITY;
  typedef LruEntry entry_type;
  typedef entry_type* entry_ptr;
  typedef SimpleLruCache<K, entry_ptr> CacheType;
  typedef SimpleLruCache<K, LruGhostEntry> GhostCacheType;

  typedef size_t  size_type;

  CacheMode mode;
  CacheType MainCache;
  GhostCacheType Ghosts;
  ObjectPool<entry_type> EntryPool;
  mutable std::mutex mutex;

  static bool MainEvictEvent(CacheType* cache, const typename CacheType::Entry& entry, void* userData)
  {
    if (entry.value->locks > 0)
      return false;

    LruCache* me = reinterpret_cast<LruCache*>(userData);

    bool ghost_found;
    auto *g = me->Ghosts.lookup(entry.key, &ghost_found);
    if (!ghost_found)
    {
      *g = LruGhostEntry(entry.key, entry.value->ghosted + 1);
    }
    else
    {
      g->ghosted++;
    }

    entry.value->reset(0, NULL);
    me->EntryPool.Destruct(entry.value);
    return true;
  }

public:

  typedef std::pair<entry_ptr, std::shared_ptr<LruCache> > handle;

  LruCache(size_type capacity, CacheMode mode) :
    GHOSTS_MIN_CAPACITY(50),
    mode(mode),
    MainCache(capacity, &MainEvictEvent, reinterpret_cast<void*>(this)),
    Ghosts(GHOSTS_MIN_CAPACITY, typename GhostCacheType::EvictEventType(), reinterpret_cast<void*>(this))
  {
  }

  size_type size() const
  {
    return MainCache.size();
  }

  size_t requested_capacity() const
  {
    return MainCache.requested_capacity();
  }

  size_t capacity() const
  {
    return MainCache.capacity();
  }

  void limits(size_t* min, size_t* max) const
  {
    std::unique_lock<std::mutex> global_lock(mutex);

    MainCache.limits(min, max);
  }

  void set_limits(size_t min, size_t max)
  {
    std::unique_lock<std::mutex> global_lock(mutex);

    MainCache.set_limits(min, max);
  }

  LruLookupResult lookup(const K& key, handle *hndl, bool block_for_completion, V& foundItem, bool* suppressCaching = nullptr)
  {
    bool suppress = (suppressCaching != nullptr) && *suppressCaching;
    hndl->first = nullptr; // clear handle

    std::unique_lock<std::mutex> global_lock(mutex);

    bool found;
    entry_ptr* entryp = MainCache.lookup(key, &found, suppress);

    if (found)
    {
      entry_ptr entry = *entryp;

      if (!block_for_completion && (entry->state != LRU_ENTRY_AVAILABLE))
      {
        return LRU_LOOKUP_FOUND_BUT_NOTAVAIL;
      }

      // wait until data becomes available
      ++(entry->locks);
      while (entry->state == LRU_ENTRY_EMPTY)
      {
        entry->ready_cond.wait(global_lock);

        switch (entry->state)
        {
        case LRU_ENTRY_EMPTY:           // do nothing, spurious wakeup
          break;
        case LRU_ENTRY_AVAILABLE:       // finally, data available
          break;
        case LRU_ENTRY_ROLLED_BACK:     // whoever we were waiting for decided to step back. we take over his place.
          entry->state = LRU_ENTRY_EMPTY;
          *hndl = handle(entry, this->shared_from_this());
          return LRU_LOOKUP_NOT_FOUND;
        default:
          assert(0);
        }
      }
      // copy and return entry->value before releasing lock
      foundItem = entry->value;
      --(entry->locks);
      return LRU_LOOKUP_FOUND_AND_READY;
    }
    else if (suppress == false)
    {
      // ghost: self-tuning caching algorithm
      bool ghost_found;
      auto *g = Ghosts.lookup(key, &ghost_found);
      assert(g != NULL);
      if (!ghost_found)
      {
        *g = LruGhostEntry(key, 0);
      }
      else if (g->ghosted > 1)
      {
        // Fix for Issues #270 and #379: require a frame to have been evicted
        // at least twice (ghosted > 1) before triggering a cache resize.
        //
        // With the old condition (ghosted > 0), any single eviction + re-request
        // caused a +1 resize.  This led to unbounded cache growth in two cases:
        //
        // 1. Backward seeking (Issue #379): frames played forward get evicted and
        //    ghosted=1.  Every backstep hits a ghost (1 > 0 was true) → +1 on each
        //    step.
        //
        // 2. Bob()/SeparateFields access pattern (Issue #270): the n, n/2 pattern
        //    keeps evicting the same frames with ghosted=1, so every GetFrame call
        //    triggered a resize.
        //    Infinite cache growth occured when the requested frame number pattern from 
        //    source filter is something like that:
        //    0,0, 0,1, 1,2, 1,3, 2,4, 2,5, 3,6, 3,7, 4,8,...
        // Sample script:
        //   ConvertToY8()
        //   org = last
        //   Bob()
        //   Merge(last, org)
        // With ghosted > 1: both cases are suppressed because frames evicted only
        // once stay at ghosted=1 (1 > 1 is false).  An undersized cache
        // still grows: frames that keep cycling will be evicted a second time,
        // pushing ghosted to 2, and growth resumes from that point forward.
        if (mode != CACHE_NO_RESIZE) {
#ifdef CACHE_GROWTH_INFINITELY_TEST
          // When the above (g->ghosted > 1) was (g->ghosted > 0)
          _RPT1(0, "Not in cache but in ghost! g->ghosted > 0 => resize! MainCache.capacity()=%d -> += 1", MainCache.capacity() + 1);
#endif
          MainCache.resize(MainCache.capacity() + 1);
          Ghosts.resize(GHOSTS_MIN_CAPACITY + MainCache.capacity() * 2);
        }

        // Nekopanda: reduce amount of cache.
        // when this filter increased the cache, we prevent lower filters increase their cache
        // because the requests to the lower filters were not needed if this filter cached the frame.
        if (mode == CACHE_OPTIMAL_SIZE && suppressCaching != nullptr) {
          *suppressCaching = true;
        }
      }
      else
      {
        // This cannot happen
        //assert(0); LOL maybe it can...
      }

      if (entryp != NULL)
      {
        *entryp = EntryPool.Construct(key);
        entry_ptr entry = *entryp;
        *hndl = handle(entry, this->shared_from_this());
        entry->locks = 1;
        entry->ghosted = g->ghosted;
        entry->value = NULL;
        return LRU_LOOKUP_NOT_FOUND;
      }
      else
      {
        g->ghosted++;
        return LRU_LOOKUP_NO_CACHE;
      }
    } // if
    else {
      assert(entryp == nullptr);
      return LRU_LOOKUP_NO_CACHE;
    }
  }

  void commit_value(handle *hndl)
  {
    std::unique_lock<std::mutex> global_lock(mutex);

    // mark data as ready
    entry_ptr e = hndl->first;
    e->state = LRU_ENTRY_AVAILABLE;
    --(e->locks);

    // notify waiters
    global_lock.unlock();
    e->ready_cond.notify_all();

    hndl->second.reset();
  }

  void rollback(handle *hndl)
  {
    std::unique_lock<std::mutex> global_lock(mutex);

    entry_ptr e = hndl->first;
    assert(e->locks > 0);

    if (e->locks == 1)
    {
      MainCache.remove(e->key);
    }
    else
    {
      // others have started waiting for this data, so another one will have to take over
      --(e->locks);
      e->state = LRU_ENTRY_ROLLED_BACK;

      // notify one waiter
      global_lock.unlock();
      e->ready_cond.notify_one();
    }

    hndl->second.reset();
  }
};

#endif  // AVS_LRUCACHE_H
