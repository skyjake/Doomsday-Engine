/** @file lumpcache.h  Provides a data cache tailored to storing lumps (i.e., files).
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

#ifndef DE_FILESYS_LUMPCACHE_H
#define DE_FILESYS_LUMPCACHE_H

#include "../libdoomsday.h"
#include "dd_types.h"
#include <vector>

/**
 * @ingroup fs
 */
class LIBDOOMSDAY_PUBLIC LumpCache
{
private:
    /**
     * Data item. Represents a lump of data in the cache.
     */
    class Data
    {
    public:
        explicit Data(uint8_t *data = 0);

        ~Data();

        uint8_t *data() const;

        const uint8_t *replaceData(uint8_t *newData);

        Data &clearData(bool *retCleared = 0);

        Data &lock();

        Data &unlock();

    private:
        uint8_t *data_;
    };
    typedef std::vector<Data> DataCache;

public:
    explicit LumpCache(uint size);
    ~LumpCache();

    uint size() const;

    bool isValidIndex(uint idx) const;

    const uint8_t *data(uint lumpIdx) const;

    LumpCache &insert(uint lumpIdx, uint8_t *data);

    LumpCache &insertAndLock(uint lumpIdx, uint8_t *data);

    LumpCache &lock(uint lumpIdx);

    LumpCache &unlock(uint lumpIdx);

    LumpCache &remove(uint lumpIdx, bool *retRemoved = 0);

    LumpCache &clear();

protected:
    Data *cacheRecord(uint lumpIdx);

    const Data *cacheRecord(uint lumpIdx) const;

private:
    uint _size;             ///< Number of data lumps which can be stored in the cache.
    DataCache *_dataCache;  ///< The cached data.
};

#endif /* DE_FILESYS_LUMPCACHE_H */
