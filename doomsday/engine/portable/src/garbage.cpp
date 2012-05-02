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

#include "garbage.h"
#include "concurrency.h"
#include "dd_zone.h"

#include <assert.h>
#include <map>
#include <set>

struct Garbage
{
    typedef std::map<void*, GarbageDestructor> Allocs; // O(log n) search
    Allocs allocs;

    ~Garbage()
    {
        recycle();
    }

    bool contains(const void* ptr) const
    {
        Allocs::const_iterator i = allocs.find(const_cast<void*>(ptr));
        return i != allocs.end();
    }

    void recycle()
    {
        for(Allocs::iterator i = allocs.begin(); i != allocs.end(); ++i)
        {
            assert(i->second);
            i->second(i->first);
        }
        allocs.clear();
    }
};

typedef std::map<uint, Garbage*> Garbages; // threadId => Garbage
static Garbages garbages;

static Garbage* garbageForThread(uint thread)
{
    Garbages::iterator i = garbages.find(thread);
    if(i != garbages.end()) return i->second;

    Garbage* g = new Garbage;
    garbages[thread] = g;
    return g;
}

void Garbage_Shutdown(void)
{
    for(Garbages::iterator i = garbages.begin(); i != garbages.end(); ++i)
    {
        delete i->second;
    }
    garbages.clear();
}

void Garbage_ClearForThread(void)
{
    Garbages::iterator i = garbages.find(Sys_CurrentThreadId());
    if(i == garbages.end()) return;

    Garbage* g = i->second;
    delete g;
    garbages.erase(i);
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
    Garbage* g = garbageForThread(Sys_CurrentThreadId());
    return g->contains(ptr);
}

void Garbage_Untrash(void* ptr)
{
    Garbage* g = garbageForThread(Sys_CurrentThreadId());
    assert(g->contains(ptr));
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
