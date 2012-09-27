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

#include <de/Error>
#include <de/Log>
#include <de/memory.h>
#include <de/memoryzone.h>

class LumpCache
{
public:
    LumpCache(uint size) : size_(size)
    {
        lumps = (void**) M_Calloc(size_ * sizeof(*lumps));
    }

    ~LumpCache()
    {
        clear();
        if(lumps) M_Free(lumps);
    }

    uint size() const { return size_; }

    bool isValidIndex(uint idx) const { return idx < size_; }

    uint8_t const* data(uint lumpIdx) const
    {
        void** cacheAdr = address(lumpIdx);
        return reinterpret_cast<uint8_t*>(cacheAdr? *cacheAdr : 0);
    }

    LumpCache& insert(uint lumpIdx, uint8_t* data)
    {
        LOG_AS("LumpCache::insert");
        if(!isValidIndex(lumpIdx)) throw de::Error("LumpCache::insert", QString("Invalid index %1").arg(lumpIdx));

        remove(lumpIdx);

        void** cacheAdr = address(lumpIdx);
        *cacheAdr = data;
        Z_ChangeUser(*cacheAdr, cacheAdr);
        return *this;
    }

    LumpCache& insertAndLock(uint lumpIdx, uint8_t* data)
    {
        return insert(lumpIdx, data).lock(lumpIdx);
    }

    LumpCache& lock(uint /*lumpIdx*/)
    {
        /// @todo Implement a proper thread-safe locking mechanism.
        return *this;
    }

    LumpCache& unlock(uint lumpIdx)
    {
        /// @todo Implement a proper thread-safe locking mechanism.
        void** cacheAdr = address(lumpIdx);
        bool isCached = cacheAdr && *cacheAdr;
        if(isCached)
        {
            Z_ChangeTag2(*cacheAdr, PU_PURGELEVEL);
        }
        return *this;
    }

    LumpCache& remove(uint lumpIdx, bool* retRemoved = 0)
    {
        void** cacheAdr = address(lumpIdx);
        bool isCached = (cacheAdr && *cacheAdr);
        if(isCached)
        {
            /// @todo Implement a proper thread-safe locking mechanism.

            // Elevate the cached data to purge level so it will be explicitly
            // free'd by the Zone the next time the rover passes it.
            if(Z_GetTag(*cacheAdr) != PU_PURGELEVEL)
            {
                Z_ChangeTag2(*cacheAdr, PU_PURGELEVEL);
            }
            // Mark the data as unowned.
            Z_ChangeUser(*cacheAdr, (void*) 0x2);
        }

        if(retRemoved) *retRemoved = isCached;
        return *this;
    }

    LumpCache& clear()
    {
        for(uint i = 0; i < size_; ++i)
        {
            remove(i);
        }
        return *this;
    }

private:
    void** address(uint lumpIdx) const
    {
        if(!lumps) return 0;
        if(!isValidIndex(lumpIdx)) return 0;
        return &lumps[lumpIdx];
    }

private:
    /// Number of data lumps which can be stored in the cache.
    uint size_;

    /// Lump data pointers.
    void** lumps;
};

#endif /* LIBDENG_FILESYS_LUMPCACHE_H */
