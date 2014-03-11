/** @file garbage.cpp  Garbage collector.
 *
 * @authors Copyright © 2012-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#include "de/Garbage"
#include "de/Lockable"
#include "de/Guard"
#include "de/Log"

#include <QThread>
#include <map>
#include <set>

namespace de {
namespace internal {

struct Garbage : public Lockable
{
    typedef std::map<void *, GarbageDestructor> Allocs; // O(log n) search
    Allocs allocs;

    ~Garbage()
    {
        recycle();
    }

    bool contains(void const *ptr) const
    {
        DENG2_GUARD(this);

        Allocs::const_iterator i = allocs.find(const_cast<void *>(ptr));
        return i != allocs.end();
    }

    void recycle(GarbageDestructor condition = 0)
    {
        DENG2_GUARD(this);

        if(allocs.empty()) return;

        LOG_TRACE("Recycling %i allocations/instances") << allocs.size();

        for(Allocs::iterator i = allocs.begin(); i != allocs.end(); )
        {
            Allocs::iterator next = i;
            ++next;

            DENG2_ASSERT(i->second);
            if(!condition || i->second == condition)
            {
                i->second(i->first);

                // Erase one by one if a condition has been given.
                if(condition) allocs.erase(i);
            }

            i = next;
        }

        if(!condition)
        {
            // All can be erased as we have no condition.
            allocs.clear();
        }
    }
};

struct Garbages : public std::map<QThread *, Garbage *>, public Lockable
{
    /**
     * Recycles all collected garbage and deletes the collectors.
     */
    ~Garbages()
    {
        clearAll();
    }

    void clearAll()
    {
        DENG2_GUARD(this);
        for(iterator i = begin(); i != end(); ++i)
        {
            delete i->second;
        }
        clear();
    }

    void recycleWithDestructor(GarbageDestructor func)
    {
        DENG2_GUARD(this);
        for(iterator i = begin(); i != end(); ++i)
        {
            i->second->recycle(func);
        }
    }
};

} // namespace internal
} // namespace de

using namespace de;
using namespace de::internal;

static Garbages garbages;

static Garbage *garbageForThread(QThread *thread)
{
    DENG2_GUARD(garbages);

    Garbage *result;
    Garbages::iterator i = garbages.find(thread);
    if(i != garbages.end())
    {
        result = i->second;
    }
    else
    {
        // Allocate a new one.
        garbages[thread] = result = new Garbage;
    }
    return result;
}

void Garbage_ClearForThread(void)
{
    DENG2_GUARD(garbages);

    Garbages::iterator i = garbages.find(QThread::currentThread());
    if(i != garbages.end())
    {
        Garbage *g = i->second;
        delete g;
        garbages.erase(i);
    }
}

void Garbage_TrashMalloc(void *ptr)
{
    Garbage_TrashInstance(ptr, free);
}

void Garbage_TrashInstance(void *ptr, GarbageDestructor destructor)
{
    Garbage *g = garbageForThread(QThread::currentThread());
    g->allocs[ptr] = destructor;
}

int Garbage_IsTrashed(void const *ptr)
{
    Garbage *g = garbageForThread(QThread::currentThread());
    return g->contains(ptr);
}

void Garbage_Untrash(void *ptr)
{
    Garbage *g = garbageForThread(QThread::currentThread());
    DENG2_ASSERT(g->contains(ptr));
    g->allocs.erase(ptr);
}

void Garbage_RemoveIfTrashed(void *ptr)
{
    Garbage *g = garbageForThread(QThread::currentThread());
    Garbage::Allocs::iterator found = g->allocs.find(ptr);
    if(found != g->allocs.end())
    {
        g->allocs.erase(found);
    }
}

void Garbage_Recycle(void)
{
    Garbage *g = garbageForThread(QThread::currentThread());
    g->recycle();
}

void Garbage_RecycleAllWithDestructor(GarbageDestructor destructor)
{
    garbages.recycleWithDestructor(destructor);
}
