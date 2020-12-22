/** @file lumpcache.cpp  Provides a data cache tailored to storing lumps (i.e., files).
 *
 * @author Copyright © 2013-2014 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/filesys/lumpcache.h"
#include <de/legacy/memory.h>
#include <de/legacy/memoryzone.h>
#include <de/error.h>
#include <de/log.h>

using namespace de;

LumpCache::Data::Data(uint8_t *data)
    : data_(data)
{}

LumpCache::Data::~Data()
{
    clearData();
}

uint8_t *LumpCache::Data::data() const
{
    if (data_ && Z_GetTag(data_) == PU_PURGELEVEL)
    {
        // Reaquire the data.
        Z_ChangeTag2(data_, PU_APPSTATIC);
        Z_ChangeUser(data_, (void*)&data_);
    }
    return data_;
}

const uint8_t *LumpCache::Data::replaceData(uint8_t *newData)
{
    clearData();
    data_ = newData;
    if (data_)
    {
        Z_ChangeUser(data_, &data_);
    }
    return newData;
}

LumpCache::Data &LumpCache::Data::clearData(bool *retCleared)
{
    bool hasData = !!data_;
    if (hasData)
    {
        /// @todo Implement a proper thread-safe locking mechanism.

        // Elevate the cached data to purge level so it will be explicitly
        // free'd by the Zone the next time the rover passes it.
        if (Z_GetTag(data_) != PU_PURGELEVEL)
        {
            Z_ChangeTag2(data_, PU_PURGELEVEL);
        }
        // Mark the data as unowned.
        Z_ChangeUser(data_, (void *) 0x2);
    }
    if (retCleared) *retCleared = hasData;
    return *this;
}

LumpCache::Data &LumpCache::Data::lock()
{
    /// @todo Implement a proper thread-safe locking mechanism.
    return *this;
}

LumpCache::Data &LumpCache::Data::unlock()
{
    /// @todo Implement a proper thread-safe locking mechanism.
    if (data_)
    {
        Z_ChangeTag2(data_, PU_PURGELEVEL);
    }
    return *this;
}

LumpCache::LumpCache(uint size) : _size(size), _dataCache(0)
{}

LumpCache::~LumpCache()
{
    if (_dataCache) delete _dataCache;
}

uint LumpCache::size() const
{
    return _size;
}

bool LumpCache::isValidIndex(uint idx) const
{
    return idx < _size;
}

const uint8_t *LumpCache::data(uint lumpIdx) const
{
    LOG_AS("LumpCache::data");
    Data const* record = cacheRecord(lumpIdx);
    return record? record->data() : 0;
}

LumpCache &LumpCache::insert(uint lumpIdx, uint8_t *data)
{
    LOG_AS("LumpCache::insert");
    if (!isValidIndex(lumpIdx)) throw Error("LumpCache::insert", stringf("Invalid index %u", lumpIdx));

    // Time to allocate the data cache?
    if (!_dataCache)
    {
        _dataCache = new DataCache(_size);
    }

    Data *record = cacheRecord(lumpIdx);
    record->replaceData(data);
    return *this;
}

LumpCache &LumpCache::insertAndLock(uint lumpIdx, uint8_t *data)
{
    return insert(lumpIdx, data).lock(lumpIdx);
}

LumpCache &LumpCache::lock(uint lumpIdx)
{
    LOG_AS("LumpCache::lock");
    if (!isValidIndex(lumpIdx)) throw Error("LumpCache::lock", stringf("Invalid index %u", lumpIdx));
    Data* record = cacheRecord(lumpIdx);
    record->lock();
    return *this;
}

LumpCache &LumpCache::unlock(uint lumpIdx)
{
    LOG_AS("LumpCache::unlock");
    if (!isValidIndex(lumpIdx)) throw Error("LumpCache::unlock", stringf("Invalid index %u", lumpIdx));
    Data* record = cacheRecord(lumpIdx);
    record->unlock();
    return *this;
}

LumpCache &LumpCache::remove(uint lumpIdx, bool *retRemoved)
{
    Data *record = cacheRecord(lumpIdx);
    if (record)
    {
        record->clearData(retRemoved);
    }
    else if (retRemoved)
    {
        *retRemoved = false;
    }
    return *this;
}

LumpCache &LumpCache::clear()
{
    if (_dataCache)
    {
        DE_FOR_EACH(DataCache, i, *_dataCache)
        {
            i->clearData();
        }
    }
    return *this;
}

LumpCache::Data *LumpCache::cacheRecord(uint lumpIdx)
{
    if (!isValidIndex(lumpIdx)) return 0;
    return _dataCache? &(*_dataCache)[lumpIdx] : 0;
}

const LumpCache::Data *LumpCache::cacheRecord(uint lumpIdx) const
{
    if (!isValidIndex(lumpIdx)) return 0;
    return _dataCache? &(*_dataCache)[lumpIdx] : 0;
}
