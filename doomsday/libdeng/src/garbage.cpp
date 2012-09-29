/**
 * @file garbage.cpp
 * Garbage collector. @ingroup base
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/garbage.h"
#include "de/concurrency.h"
#include "de/memoryzone.h"

#include <map>
#include <set>
#include <QMutex>
#include <de/Log>

struct Garbage
{
    typedef std::map<void*, GarbageDestructor> Allocs; // O(log n) search
    Allocs allocs;
    bool beingRecycled;

    Garbage() : beingRecycled(false)
    {}

    ~Garbage()
    {
        recycle();
    }

    bool contains(const void* ptr) const
    {
        if(beingRecycled) return false;

        Allocs::const_iterator i = allocs.find(const_cast<void*>(ptr));
        return i != allocs.end();
    }

    void recycle()
    {
        if(allocs.empty()) return;

        beingRecycled = true;

        LOG_DEBUG("Recycling %i allocations/instances.") << allocs.size();

        for(Allocs::iterator i = allocs.begin(); i != allocs.end(); ++i)
        {
            DENG_ASSERT(i->second);
            i->second(i->first);
        }
        allocs.clear();

        beingRecycled = false;
    }
};

typedef std::map<uint, Garbage*> Garbages; // threadId => Garbage
static QMutex garbageMutex; // for accessing Garbages
static Garbages* garbages;

static Garbage* garbageForThread(uint thread)
{
    DENG_ASSERT(garbages != 0);

    Garbage* result;
    garbageMutex.lock();
    Garbages::iterator i = garbages->find(thread);
    if(i != garbages->end())
    {
        result = i->second;
    }
    else
    {
        // Allocate a new one.
        (*garbages)[thread] = result = new Garbage;
    }
    garbageMutex.unlock();
    return result;
}

void Garbage_Init(void)
{
    DENG_ASSERT(garbages == 0);
    garbages = new Garbages;
}

void Garbage_Shutdown(void)
{
    DENG_ASSERT(garbages != 0);
    garbageMutex.lock();
    for(Garbages::iterator i = garbages->begin(); i != garbages->end(); ++i)
    {
        delete i->second;
    }
    delete garbages;
    garbages = 0;
    garbageMutex.unlock();
}

void Garbage_ClearForThread(void)
{
    garbageMutex.lock();
    Garbages::iterator i = garbages->find(Sys_CurrentThreadId());
    if(i != garbages->end())
    {
        Garbage* g = i->second;
        delete g;
        garbages->erase(i);
    }
    garbageMutex.unlock();
}

void Garbage_Trash(void* ptr)
{
    Garbage* g = garbageForThread(Sys_CurrentThreadId());
    g->allocs[ptr] = Z_Contains(ptr)? Z_Free : free;
}

void Garbage_TrashInstance(void* ptr, GarbageDestructor destructor)
{
    Garbage* g = garbageForThread(Sys_CurrentThreadId());
    g->allocs[ptr] = destructor;
}

boolean Garbage_IsTrashed(const void* ptr)
{
    if(!garbages) return false;
    Garbage* g = garbageForThread(Sys_CurrentThreadId());
    return g->contains(ptr);
}

void Garbage_Untrash(void* ptr)
{
    Garbage* g = garbageForThread(Sys_CurrentThreadId());
    DENG_ASSERT(g->contains(ptr));
    g->allocs.erase(ptr);
}

void Garbage_RemoveIfTrashed(void* ptr)
{
    Garbage* g = garbageForThread(Sys_CurrentThreadId());
    Garbage::Allocs::iterator found = g->allocs.find(ptr);
    if(found != g->allocs.end())
    {
        g->allocs.erase(found);
    }
}

void Garbage_Recycle(void)
{
    Garbage* g = garbageForThread(Sys_CurrentThreadId());
    g->recycle();
}
