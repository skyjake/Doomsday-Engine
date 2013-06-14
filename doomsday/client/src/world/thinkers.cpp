/** @file p_think.cpp World map thinker management.
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

#include <QList>
#include <QtAlgorithms>

#include <de/memoryzone.h>

#include "de_base.h"
#include "de_defs.h"
#include "de_console.h"
#include "de_network.h"

#include "world/map.h"

#include "world/thinkers.h"

boolean Thinker_IsMobjFunc(thinkfunc_t func)
{
    return (func && func == reinterpret_cast<thinkfunc_t>(gx.MobjThinker));
}

namespace de {

struct ThinkerList
{
    bool isPublic; ///< All thinkers in this list are visible publically.

    thinker_t sentinel;

    ThinkerList(thinkfunc_t func, bool isPublic) : isPublic(isPublic)
    {
        zap(sentinel);

        sentinel.function = func;
        sentinel.inStasis = true; // Safety measure.

        sentinel.prev = sentinel.next = &sentinel;
    }

    void reInit()
    {
        sentinel.prev = sentinel.next = &sentinel;
    }

    thinkfunc_t function() const
    {
        return sentinel.function;
    }

    // Link the thinker to the list.
    void link(thinker_t &th)
    {
        sentinel.prev->next = &th;
        th.next = &sentinel;
        th.prev = sentinel.prev;
        sentinel.prev = &th;
    }

    int iterate(int (*callback) (thinker_t *, void *), void *parameters = 0)
    {
        int result = false;

        thinker_t *th = sentinel.next;
        while(th != &sentinel && th)
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
};

DENG2_PIMPL(Thinkers)
{
    typedef QList<ThinkerList *> Lists;

    int idtable[2048]; // 65536 bits telling which IDs are in use.
    ushort iddealer;

    Lists lists;
    bool inited;

    Instance(Public *i)
        : Base(i),
          iddealer(0),
          inited(false)
    {
        self.clearMobjIds();
    }

    ~Instance()
    {
        qDeleteAll(lists);
    }

    thid_t newMobjID()
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

void Thinkers::clearMobjIds()
{
    zap(d->idtable);
    d->idtable[0] |= 1; // ID zero is always "used" (it's not a valid ID).
}

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

struct mobjidlookup_t
{
    thid_t id;
    mobj_t *result;
};

static int mobjIdLookup(thinker_t *thinker, void *context)
{
    mobjidlookup_t *lookup = (mobjidlookup_t *) context;
    if(thinker->id == lookup->id)
    {
        lookup->result = (mobj_t *) thinker;
        return true; // Stop iteration.
    }
    return false; // Continue iteration.
}

struct mobj_s *Thinkers::mobjById(int id)
{
    /// @todo  A hash table wouldn't hurt (see client's mobj id table).
    mobjidlookup_t lookup;
    lookup.id = id;
    lookup.result = 0;
    iterate(reinterpret_cast<thinkfunc_t>(gx.MobjThinker),
            0x1/*mobjs are public*/, mobjIdLookup, &lookup);
    return lookup.result;
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
            th.id = d->newMobjID();
        }
    }
    else
    {
        // Zero is not a valid ID.
        th.id = 0;
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

#ifdef __SERVER__
        // Then it must be a mobj.
        mobj_t *mo = reinterpret_cast<mobj_t *>(&th);

        // If the state of the mobj is the NULL state, this is a
        // predictable mobj removal (result of animation reaching its
        // end) and shouldn't be included in netGame deltas.
        if(!mo->state || mo->state == states)
        {
            Sv_MobjRemoved(th.id);
        }
#endif
    }

    th.function = (thinkfunc_t) -1;
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

            list->reInit();
        }
    }

    clearMobjIds();
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

void unlinkThinkerFromList(thinker_t *th)
{
    th->next->prev = th->prev;
    th->prev->next = th->next;
}

static int runThinker(thinker_t *th, void *context)
{
    DENG_UNUSED(context);

    // Thinker cannot think when in stasis.
    if(!th->inStasis)
    {
        // Time to remove it?
        if(th->function == (thinkfunc_t) -1)
        {
            unlinkThinkerFromList(th);

            if(th->id)
            {
                mobj_t *mo = (mobj_t *) th;
#ifdef __CLIENT__
                if(!Cl_IsClientMobj(mo))
                {
                    // It's a regular mobj: recycle for reduced allocation overhead.
                    P_MobjRecycle(mo);
                }
                else
                {
                    // Delete the client mobj.
                    ClMobj_Destroy(mo);
                }
#else
                P_MobjRecycle(mo);
#endif
            }
            else
            {
                // Non-mobjs are just deleted right away.
                Z_Free(th);
            }
        }
        else if(th->function)
        {
            th->function(th);
        }
    }

    return false; // Continue iteration.
}

} // namespace de

using namespace de;

/**
 * Locates a mobj by it's unique identifier in the CURRENT map.
 */
#undef P_MobjForID
DENG_EXTERN_C struct mobj_s *P_MobjForID(int id)
{
    if(!App_World().hasMap()) return 0;
    return App_World().map().thinkers().mobjById(id);
}

#undef Thinker_Init
void Thinker_Init()
{
    if(!App_World().hasMap()) return;
    App_World().map().thinkers().initLists(0x1); // Init the public thinker lists.
}

#undef Thinker_Run
void Thinker_Run()
{
    if(!App_World().hasMap()) return;
    App_World().map().thinkers().iterate(NULL, 0x1 | 0x2, runThinker, NULL);
}

#undef Thinker_Add
void Thinker_Add(thinker_t *th)
{
    if(!th || !App_World().hasMap()) return;
    App_World().map().thinkers().add(*th);
}

#undef Thinker_Remove
void Thinker_Remove(thinker_t *th)
{
    if(!th || !App_World().hasMap()) return;
    App_World().map().thinkers().remove(*th);
}

#undef Thinker_SetStasis
void Thinker_SetStasis(thinker_t *th, boolean on)
{
    if(th)
    {
        th->inStasis = on;
    }
}

#undef Thinker_Iterate
int Thinker_Iterate(thinkfunc_t func, int (*callback) (thinker_t *, void *), void *context)
{
    if(!App_World().hasMap()) return false; // Continue iteration.
    return App_World().map().thinkers().iterate(func, 0x1, callback, context);
}

DENG_DECLARE_API(Thinker) =
{
    { DE_API_THINKER },
    Thinker_Init,
    Thinker_Run,
    Thinker_Add,
    Thinker_Remove,
    Thinker_SetStasis,
    Thinker_Iterate
};
