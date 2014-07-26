/** @file thinkers.cpp World map thinker management.
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

#define DENG_NO_API_MACROS_THINKER

#include "de_base.h"
#include "world/thinkers.h"

#ifdef __CLIENT__
#  include "client/cl_mobj.h"
#  include "world/clientmobjthinkerdata.h"
#endif

#ifdef __SERVER__
#  include "def_main.h"
#  include "server/sv_pool.h"
#  include <doomsday/world/mobjthinkerdata.h>
#endif

#include "world/map.h"
#include "world/p_object.h"

#include <de/memoryzone.h>
#include <QList>
#include <QtAlgorithms>

dd_bool Thinker_IsMobjFunc(thinkfunc_t func)
{
    return (func && func == reinterpret_cast<thinkfunc_t>(gx.MobjThinker));
}

de::Map &Thinker_Map(thinker_t const & /*th*/)
{
    /// @todo Do not assume the current map.
    return App_WorldSystem().map();
}

namespace de {

struct ThinkerList
{
    bool isPublic; ///< All thinkers in this list are visible publically.

    Thinker sentinel;

    ThinkerList(thinkfunc_t func, bool isPublic) : isPublic(isPublic)
    {
        sentinel.function = func;
        sentinel.disable(); // Safety measure.

        sentinel.prev = sentinel.next = sentinel;
    }

    void reinit()
    {
        sentinel.prev = sentinel.next = sentinel;
    }

    thinkfunc_t function() const
    {
        return sentinel.function;
    }

    // Link the thinker to the list.
    void link(thinker_t &th)
    {
        sentinel.prev->next = &th;
        th.next = sentinel;
        th.prev = sentinel.prev;
        sentinel.prev = &th;
    }

    int count(int *numInStasis) const
    {
        int num = 0;
        thinker_t *th = sentinel.next;
        while(th != &sentinel.base() && th)
        {
#ifdef LIBDENG_FAKE_MEMORY_ZONE
            DENG_ASSERT(th->next != 0);
            DENG_ASSERT(th->prev != 0);
#endif
            num += 1;
            if(numInStasis && Thinker_InStasis(th))
            {
                (*numInStasis) += 1;
            }
            th = th->next;
        }
        return num;
    }

    int iterate(int (*callback) (thinker_t *, void *), void *parameters = 0)
    {
        int result = false;

        thinker_t *th = sentinel.next;
        while(th != &sentinel.base() && th)
        {
#ifdef LIBDENG_FAKE_MEMORY_ZONE
            DENG_ASSERT(th->next != 0);
            DENG_ASSERT(th->prev != 0);
#endif
            thinker_t *next = th->next;

            result = callback(th, parameters);
            if(result) break;

            th = next;
        }

        return result;
    }

    void releaseAll()
    {
        for(thinker_t *th = sentinel.next; th != &sentinel.base() && th; th = th->next)
        {
            Thinker::release(*th);
        }
    }
};

typedef QHash<thid_t, mobj_t *> MobjHash;

DENG2_PIMPL(Thinkers)
{
    typedef QList<ThinkerList *> Lists;

    int idtable[2048]; // 65536 bits telling which IDs are in use.
    ushort iddealer;

    Lists lists;
    bool inited;

    MobjHash mobjIdLookup; // public only

    Instance(Public *i)
        : Base(i),
          iddealer(0),
          inited(false)
    {
        clearMobjIds();
    }

    ~Instance()
    {
        // Make sure the private instances of thinkers are released.
        releaseAllThinkers();

        // Note that most thinkers are allocated from the memory zone
        // so there is no memory leak here as this memory will be purged
        // automatically when the map is "unloaded".
        qDeleteAll(lists);
    }

    void releaseAllThinkers()
    {
        foreach(ThinkerList *list, lists)
        {
            list->releaseAll();
        }
    }

    void clearMobjIds()
    {
        zap(idtable);
        idtable[0] |= 1; // ID zero is always "used" (it's not a valid ID).

        mobjIdLookup.clear();
    }

    thid_t newMobjId()
    {
        // Increment the ID dealer until a free ID is found.
        /// @todo fixme: What if all IDs are in use? 65535 thinkers!?
        while(self.isUsedMobjId(++iddealer)) {}

        // Mark this ID as used.
        self.setMobjId(iddealer);

        return iddealer;
    }

    ThinkerList *listForThinkFunc(thinkfunc_t func, bool makePublic = true,
                                  bool canCreate = false)
    {
        for(int i = 0; i < lists.count(); ++i)
        {
            ThinkerList *list = lists[i];
            if(list->function() == func && list->isPublic == makePublic)
                return list;
        }

        if(!canCreate) return 0;

        // A new thinker type.
        lists.append(new ThinkerList(func, makePublic));
        return lists.last();
    }
};

Thinkers::Thinkers() : d(new Instance(this))
{}

bool Thinkers::isUsedMobjId(thid_t id)
{
    return d->idtable[id >> 5] & (1 << (id & 31) /*(id % 32) */ );
}

void Thinkers::setMobjId(thid_t id, bool inUse)
{
    int c = id >> 5, bit = 1 << (id & 31); //(id % 32);

    if(inUse) d->idtable[c] |= bit;
    else      d->idtable[c] &= ~bit;
}

struct mobj_s *Thinkers::mobjById(int id)
{
    MobjHash::const_iterator found = d->mobjIdLookup.constFind(id);
    if(found != d->mobjIdLookup.constEnd())
    {
        return found.value();
    }
    return 0;
}

void Thinkers::add(thinker_t &th, bool makePublic)
{
    if(!th.function)
        throw Error("Thinkers::add", "Invalid thinker function");

    // Will it need an ID?
    if(Thinker_IsMobjFunc(th.function))
    {
        // It is a mobj, give it an ID (not for client mobjs, though, they
        // already have an id).
#ifdef __CLIENT__
        if(!Cl_IsClientMobj(reinterpret_cast<mobj_t *>(&th)))
#endif
        {
            th.id = d->newMobjId();
        }

        if(makePublic && th.id)
        {
            d->mobjIdLookup.insert(th.id, reinterpret_cast<mobj_t *>(&th));
        }
    }
    else
    {
        th.id = 0; // Zero is not a valid ID.
    }

    // Link the thinker to the thinker list.
    ThinkerList *list = d->listForThinkFunc(th.function, makePublic, true /*can create*/);
    list->link(th);
}

void Thinkers::remove(thinker_t &th)
{
    // Has got an ID?
    if(th.id)
    {
        // Flag the identifier as free.
        setMobjId(th.id, false);

        d->mobjIdLookup.remove(th.id);

#ifdef __SERVER__
        // Then it must be a mobj.
        mobj_t *mo = reinterpret_cast<mobj_t *>(&th);

        // If the state of the mobj is the NULL state, this is a
        // predictable mobj removal (result of animation reaching its
        // end) and shouldn't be included in netGame deltas.
        if(!mo->state || !runtimeDefs.states.indexOf(mo->state))
        {
            Sv_MobjRemoved(th.id);
        }
#endif
    }

    th.function = (thinkfunc_t) -1;

    Thinker::release(th);
}

void Thinkers::initLists(byte flags)
{
    if(!d->inited)
    {
        d->lists.clear();
    }
    else
    {
        for(int i = 0; i < d->lists.count(); ++i)
        {
            ThinkerList *list = d->lists[i];

            if(list->isPublic && !(flags & 0x1)) continue;
            if(!list->isPublic && !(flags & 0x2)) continue;

            list->reinit();
        }
    }

    d->clearMobjIds();
    d->inited = true;
}

bool Thinkers::isInited() const
{
    return d->inited;
}

int Thinkers::iterate(thinkfunc_t func, byte flags,
    int (*callback) (thinker_t *, void *), void *context)
{
    if(!d->inited) return false;

    int result = false;
    if(func)
    {
        // We might have both public and shared lists for this func.
        if(flags & 0x1)
        {
            if(ThinkerList *list = d->listForThinkFunc(func))
            {
                result = list->iterate(callback, context);
            }
        }

        if(!result && (flags & 0x2))
        {
            if(ThinkerList *list = d->listForThinkFunc(func, false /*not public*/))
            {
                result = list->iterate(callback, context);
            }
        }

        return result;
    }

    for(int i = 0; i < d->lists.count(); ++i)
    {
        ThinkerList *list = d->lists[i];

        if(list->isPublic && !(flags & 0x1)) continue;
        if(!list->isPublic && !(flags & 0x2)) continue;

        result = list->iterate(callback, context);
        if(result) break;
    }
    return result;
}

int Thinkers::count(int *numInStasis) const
{
    int total = 0;
    if(isInited())
    {
        for(int i = 0; i < d->lists.count(); ++i)
        {
            ThinkerList *list = d->lists[i];
            total += list->count(numInStasis);
        }
    }
    return total;
}

static void unlinkThinkerFromList(thinker_t *th)
{
    th->next->prev = th->prev;
    th->prev->next = th->next;
}

static void initPrivateData(thinker_t *th)
{
    DENG2_ASSERT(th->d == 0);

    /// @todo The game should be asked to create its own private data. -jk

    if(Thinker_IsMobjFunc(th->function))
    {
#ifdef __CLIENT__
        th->d = new ClientMobjThinkerData;
#else
        th->d = new MobjThinkerData;
#endif
    }
    else
    {
        // Generic thinker data (Doomsday Script namespace, etc.).
        th->d = new ThinkerData;
    }

    if(th->d) THINKER_DATA(*th, ThinkerData).setThinker(th);
}

static int runThinker(thinker_t *th, void * /*context*/)
{
    if(Thinker_InStasis(th)) return false; // Skip and continue.

    // Time to remove it?
    if(th->function == (thinkfunc_t) -1)
    {
        unlinkThinkerFromList(th);

        if(th->id)
        {
            // Recycle for reduced allocation overhead.
            P_MobjRecycle((mobj_t *) th);
        }
        else
        {
            // Non-mobjs are just deleted right away.
            Thinker::destroy(th);
        }
    }
    else if(th->function)
    {
        // Create a private data instance of appropriate type.
        if(!th->d)
        {
            initPrivateData(th);
        }

        th->function(th);
    }

    return false; // Continue iteration.
}

} // namespace de

using namespace de;

/**
 * Locates a mobj by it's unique identifier in the CURRENT map.
 */
#undef Mobj_ById
DENG_EXTERN_C struct mobj_s *Mobj_ById(int id)
{
    /// @todo fixme: Do not assume the current map.
    if(!App_WorldSystem().hasMap()) return 0;
    return App_WorldSystem().map().thinkers().mobjById(id);
}

#undef Thinker_Init
void Thinker_Init()
{
    /// @todo fixme: Do not assume the current map.
    if(!App_WorldSystem().hasMap()) return;
    App_WorldSystem().map().thinkers().initLists(0x1); // Init the public thinker lists.
}

#undef Thinker_Run
void Thinker_Run()
{
    /// @todo fixme: Do not assume the current map.
    if(!App_WorldSystem().hasMap()) return;
    App_WorldSystem().map().thinkers().iterate(NULL, 0x1 | 0x2, runThinker);
}

#undef Thinker_Add
void Thinker_Add(thinker_t *th)
{
    if(!th) return;
    Thinker_Map(*th).thinkers().add(*th);
}

#undef Thinker_Remove
void Thinker_Remove(thinker_t *th)
{
    if(!th) return;
    Thinker_Map(*th).thinkers().remove(*th);
}

#undef Thinker_Iterate
int Thinker_Iterate(thinkfunc_t func, int (*callback) (thinker_t *, void *), void *context)
{
    if(!App_WorldSystem().hasMap()) return false; // Continue iteration.
    return App_WorldSystem().map().thinkers().iterate(func, 0x1, callback, context);
}

DENG_DECLARE_API(Thinker) =
{
    { DE_API_THINKER },
    Thinker_Init,
    Thinker_Run,
    Thinker_Add,
    Thinker_Remove,
    Thinker_Iterate
};
