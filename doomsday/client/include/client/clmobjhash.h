/** @file clmobjhash.h  Clientside mobj hash.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DENG_CLIENT_MOBJHASH_H
#define DENG_CLIENT_MOBJHASH_H

#include "client/cl_mobj.h"

/**
 * Client mobjs are stored into a hash for fast lookup by it's identifier.
 *
 * @ingroup world
 */
class ClMobjHash
{
public:
    static int const SIZE = 256;

public:
    ClMobjHash();

    void clear();

    /**
     * Links the clmobj into the client mobj hash table.
     */
    void insert(mobj_t *mo, thid_t id);

    /**
     * Unlinks the clmobj from the client mobj hash table.
     */
    void remove(mobj_t *mo);

    /**
     * Searches through the client mobj hash table for the CURRENT map and
     * returns the clmobj with the specified ID, if that exists. Note that
     * client mobjs are also linked to the thinkers list.
     *
     * @param id  Mobj identifier.
     *
     * @return  Pointer to the mobj.
     */
    mobj_t *find(thid_t id) const;

    /**
     * Iterate the client mobj hash, exec the callback on each. Abort if callback
     * returns non-zero.
     *
     * @param callback  Function to callback for each client mobj.
     * @param context   Data pointer passed to the callback.
     *
     * @return  @c 0 if all callbacks return @c 0; otherwise the result of the last.
     */
    int iterate(int (*callback) (mobj_t *, void *), void *context = 0);

#ifdef DENG_DEBUG
    void assertValid() const;
#endif

private:
    struct Bucket {
        ClMobjInfo *first, *last;
    };
    Bucket _buckets[SIZE];

    inline Bucket &bucketFor(thid_t id) {
        return _buckets[(uint) id % SIZE];
    }
    inline Bucket const &bucketFor(thid_t id) const {
        return _buckets[(uint) id % SIZE];
    }
};


#endif // DENG_CLIENT_MOBJHASH_H
