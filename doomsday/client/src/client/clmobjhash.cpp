/** @file clmobjhash.cpp  Clientside mobj hash.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "client/clmobjhash.h"

using namespace de;

ClMobjHash::ClMobjHash()
{
    zap(_buckets);
}

void ClMobjHash::clear()
{
    for(int i = 0; i < SIZE; ++i)
    for(ClMobjInfo *info = _buckets[i].first; info; info = info->next)
    {
        mobj_t *mo = ClMobj_MobjForInfo(info);
        // Players' clmobjs are not linked anywhere.
        if(!mo->dPlayer)
            ClMobj_Unlink(mo);
    }
}

void ClMobjHash::insert(mobj_t *mo, thid_t id)
{
    if(!mo) return;

    Bucket &bucket = bucketFor(id);
    ClMobjInfo *info = ClMobj_GetInfo(mo);

    CL_ASSERT_CLMOBJ(mo);

#ifdef DENG_DEBUG
    assertValid();
#endif

    // Set the ID.
    mo->thinker.id = id;
    info->next = 0;

    if(bucket.last)
    {
        bucket.last->next = info;
        info->prev = bucket.last;
    }
    bucket.last = info;

    if(!bucket.first)
    {
        bucket.first = info;
    }

#ifdef DENG_DEBUG
    assertValid();
#endif
}

void ClMobjHash::remove(mobj_t *mo)
{
    Bucket &bucket = bucketFor(mo->thinker.id);
    ClMobjInfo *info = ClMobj_GetInfo(mo);

    CL_ASSERT_CLMOBJ(mo);

#ifdef DENG_DEBUG
    assertValid();
#endif

    if(bucket.first == info)
        bucket.first = info->next;
    if(bucket.last == info)
        bucket.last = info->prev;
    if(info->next)
        info->next->prev = info->prev;
    if(info->prev)
        info->prev->next = info->next;

#ifdef DENG_DEBUG
    assertValid();
#endif
}

mobj_t *ClMobjHash::find(thid_t id) const
{
    if(!id) return 0;

    // Scan the existing client mobjs.
    Bucket const &bucket = bucketFor(id);
    for(ClMobjInfo *info = bucket.first; info; info = info->next)
    {
        mobj_t *mo = ClMobj_MobjForInfo(info);
        if(mo->thinker.id == id)
            return mo;
    }

    // Not found!
    return 0;
}

int ClMobjHash::iterate(int (*callback) (mobj_t *, void *), void *context)
{
    for(int i = 0; i < SIZE; ++i)
    {
        ClMobjInfo *info = _buckets[i].first;
        while(info)
        {
            ClMobjInfo *next = info->next;
            if(int result = callback(ClMobj_MobjForInfo(info), context))
                return result;
            info = next;
        }
    }
    return true;
}

#ifdef DENG_DEBUG
void ClMobjHash::assertValid() const
{
    for(int i = 0; i < SIZE; ++i)
    {
        int count1 = 0, count2 = 0;
        for(ClMobjInfo *info = _buckets[i].first; info; info = info->next, count1++)
        {
            DENG2_ASSERT(ClMobj_MobjForInfo(info) != 0);
        }
        for(ClMobjInfo *info = _buckets[i].last; info; info = info->prev, count2++)
        {
            DENG2_ASSERT(ClMobj_MobjForInfo(info) != 0);
        }
        DENG2_ASSERT(count1 == count2);
    }
}
#endif
