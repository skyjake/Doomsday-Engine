/** @file garbage.cpp  Garbage collector.
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/garbage.h"
#include "de/guard.h"
#include "de/lockable.h"
#include "de/log.h"
#include "de/thread.h"

#include <map>
#include <set>
#include <the_Foundation/thread.h>

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

    bool contains(const void *ptr) const
    {
        DE_GUARD(this);

        Allocs::const_iterator i = allocs.find(const_cast<void *>(ptr));
        return i != allocs.end();
    }

    void recycle(GarbageDestructor condition = nullptr)
    {
        DE_GUARD(this);

        if (allocs.empty()) return;

#ifdef DE_DEBUG
        //qDebug() << "[Garbage] Recycling" << allocs.size() << "allocs/objects";
#endif

        for (Allocs::iterator i = allocs.begin(); i != allocs.end(); )
        {
            Allocs::iterator next = i;
            ++next;

            DE_ASSERT(i->second);
            if (!condition || i->second == condition)
            {
                i->second(i->first);

                // Erase one by one if a condition has been given.
                if (condition) allocs.erase(i);
            }

            i = next;
        }

        if (!condition)
        {
            // All can be erased as we have no condition.
            allocs.clear();
        }
    }

    void forgetAndLeak()
    {
        allocs.clear(); // Oh well...
    }
};

struct Garbages : public std::map<iThread *, Garbage *>, public Lockable
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
        DE_GUARD(this);
        for (iterator i = begin(); i != end(); ++i)
        {
            delete i->second;
        }
        clear();
    }

    void recycleWithDestructor(GarbageDestructor func)
    {
        DE_GUARD(this);
        for (iterator i = begin(); i != end(); ++i)
        {
            i->second->recycle(func);
        }
    }

    void forgetAndLeak()
    {
        DE_GUARD(this);
        for (iterator i = begin(); i != end(); ++i)
        {
            i->second->forgetAndLeak();
        }
        clear();
    }
};

} // namespace internal
} // namespace de

using namespace de;
using namespace de::internal;

static Garbages garbages;

static Garbage *garbageForThread(iThread *thread)
{
    DE_GUARD(garbages);

    Garbage *result;
    Garbages::iterator i = garbages.find(thread);
    if (i != garbages.end())
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
    DE_GUARD(garbages);

    Garbages::iterator i = garbages.find(current_Thread());
    if (i != garbages.end())
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
    if (ptr)
    {
        Garbage *g = garbageForThread(current_Thread());
        DE_ASSERT(!Garbage_IsTrashed(ptr));
        g->allocs[ptr] = destructor;
    }
}

int Garbage_IsTrashed(const void *ptr)
{
    Garbage *g = garbageForThread(current_Thread());
    return g->contains(ptr);
}

void Garbage_Untrash(void *ptr)
{
    Garbage *g = garbageForThread(current_Thread());
    DE_ASSERT(g->contains(ptr));
    g->allocs.erase(ptr);
}

void Garbage_RemoveIfTrashed(void *ptr)
{
    Garbage *g = garbageForThread(current_Thread());
    Garbage::Allocs::iterator found = g->allocs.find(ptr);
    if (found != g->allocs.end())
    {
        g->allocs.erase(found);
    }
}

void Garbage_Recycle(void)
{
    Garbage *g = garbageForThread(current_Thread());
    g->recycle();
}

void Garbage_ForgetAndLeak(void)
{
    garbages.forgetAndLeak();
}

void Garbage_RecycleAllWithDestructor(GarbageDestructor destructor)
{
    garbages.recycleWithDestructor(destructor);
}
