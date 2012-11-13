/**
 * @file lumpcache.h
 * Provides a thread-safe data cache tailored to storing lumps (i.e., files).
 *
 * @ingroup fs
 *
 * @author Copyright &copy; 2012 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDENG_FILESYS_LUMPCACHE_H
#define LIBDENG_FILESYS_LUMPCACHE_H

#include "de_base.h"

#include <vector>

#include <de/Error>
#include <de/Log>
#include <de/memory.h>
#include <de/memoryzone.h>

class LumpCache
{
private:
    class CacheRecord
    {
    public:
        explicit CacheRecord(uint8_t* data = 0) : data_(data) {}
        ~CacheRecord()
        {
            clearData();
        }

        uint8_t* data() const
        {
            if(data_ && Z_GetTag(data_) == PU_PURGELEVEL)
            {
                // Reaquire the data.
                Z_ChangeTag2(data_, PU_APPSTATIC);
                Z_ChangeUser(data_, (void*)&data_);
            }
            return data_;
        }

        uint8_t const* replaceData(uint8_t* newData)
        {
            clearData();
            data_ = newData;
            if(data_)
            {
                Z_ChangeUser(data_, &data_);
            }
            return newData;
        }

        CacheRecord& clearData(bool* retCleared = 0)
        {
            bool hasData = !!data_;
            if(hasData)
            {
                /// @todo Implement a proper thread-safe locking mechanism.

                // Elevate the cached data to purge level so it will be explicitly
                // free'd by the Zone the next time the rover passes it.
                if(Z_GetTag(data_) != PU_PURGELEVEL)
                {
                    Z_ChangeTag2(data_, PU_PURGELEVEL);
                }
                // Mark the data as unowned.
                Z_ChangeUser(data_, (void*) 0x2);
            }
            if(retCleared) *retCleared = hasData;
            return *this;
        }

        CacheRecord& lock()
        {
            /// @todo Implement a proper thread-safe locking mechanism.
            return *this;
        }

        CacheRecord& unlock()
        {
            /// @todo Implement a proper thread-safe locking mechanism.
            if(data_)
            {
                Z_ChangeTag2(data_, PU_PURGELEVEL);
            }
            return *this;
        }

    private:
        uint8_t* data_;
    };
    typedef std::vector<CacheRecord> DataCache;

public:
    explicit LumpCache(uint size) : size_(size), dataCache(0)
    {}

    ~LumpCache()
    {
        if(dataCache) delete dataCache;
    }

    uint size() const { return size_; }

    bool isValidIndex(uint idx) const { return idx < size_; }

    uint8_t const* data(uint lumpIdx) const
    {
        LOG_AS("LumpCache::data");
        CacheRecord const* record = cacheRecord(lumpIdx);
        return record? record->data() : 0;
    }

    LumpCache& insert(uint lumpIdx, uint8_t* data)
    {
        LOG_AS("LumpCache::insert");
        if(!isValidIndex(lumpIdx)) throw de::Error("LumpCache::insert", QString("Invalid index %1").arg(lumpIdx));

        // Time to allocate the data cache?
        if(!dataCache)
        {
            dataCache = new DataCache(size_);
        }

        CacheRecord* record = cacheRecord(lumpIdx);
        record->replaceData(data);
        return *this;
    }

    LumpCache& insertAndLock(uint lumpIdx, uint8_t* data)
    {
        return insert(lumpIdx, data).lock(lumpIdx);
    }

    LumpCache& lock(uint lumpIdx)
    {
        LOG_AS("LumpCache::lock");
        if(!isValidIndex(lumpIdx)) throw de::Error("LumpCache::lock", QString("Invalid index %1").arg(lumpIdx));
        CacheRecord* record = cacheRecord(lumpIdx);
        record->lock();
        return *this;
    }

    LumpCache& unlock(uint lumpIdx)
    {
        LOG_AS("LumpCache::unlock");
        if(!isValidIndex(lumpIdx)) throw de::Error("LumpCache::unlock", QString("Invalid index %1").arg(lumpIdx));
        CacheRecord* record = cacheRecord(lumpIdx);
        record->unlock();
        return *this;
    }

    LumpCache& remove(uint lumpIdx, bool* retRemoved = 0)
    {
        CacheRecord* record = cacheRecord(lumpIdx);
        if(record)
        {
            record->clearData(retRemoved);
        }
        else if(retRemoved)
        {
            *retRemoved = false;
        }
        return *this;
    }

    LumpCache& clear()
    {
        if(dataCache)
        {
            DENG2_FOR_EACH(DataCache, i, *dataCache)
            {
                i->clearData();
            }
        }
        return *this;
    }

private:
    CacheRecord* cacheRecord(uint lumpIdx)
    {
        if(!isValidIndex(lumpIdx)) return 0;
        return dataCache? &(*dataCache)[lumpIdx] : 0;
    }

    CacheRecord const* cacheRecord(uint lumpIdx) const
    {
        if(!isValidIndex(lumpIdx)) return 0;
        return dataCache? &(*dataCache)[lumpIdx] : 0;
    }

private:
    /// Number of data lumps which can be stored in the cache.
    uint size_;

    /// The cached data.
    DataCache* dataCache;
};

#endif /* LIBDENG_FILESYS_LUMPCACHE_H */
