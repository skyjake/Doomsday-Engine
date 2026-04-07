/** @file thinkers.cpp  World map thinker management.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#define DE_NO_API_MACROS_THINKER

#include "doomsday/world/thinkers.h"
#include "doomsday/world/map.h"
#include "doomsday/world/mobj.h"
#include "doomsday/world/world.h"
#include "doomsday/world/thinkerdata.h"
#include "doomsday/doomsdayapp.h"

#include <de/legacy/memoryzone.h>
#include <de/list.h>

using namespace de;

bool Thinker_IsMobjFunc(thinkfunc_t func)
{
    const auto &gx = DoomsdayApp::app().plugins().gameExports();
    return (func && func == reinterpret_cast<thinkfunc_t>(gx.MobjThinker));
}

bool Thinker_IsMobj(const thinker_t *th)
{
    return (th && Thinker_IsMobjFunc(th->function));
}

world::Map &Thinker_Map(const thinker_t & /*th*/)
{
    /// @todo Do not assume the current map.
    return world::World::get().map();
}

namespace world {

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

    dint count(dint *numInStasis) const
    {
        dint num = 0;
        thinker_t *th = sentinel.next;
        while (th != &sentinel.base() && th)
        {
#ifdef DE_FAKE_MEMORY_ZONE
            DE_ASSERT(th->next);
            DE_ASSERT(th->prev);
#endif
            num += 1;
            if (numInStasis && Thinker_InStasis(th))
            {
                (*numInStasis) += 1;
            }
            th = th->next;
        }
        return num;
    }

    void releaseAll()
    {
        for (thinker_t *th = sentinel.next; th != &sentinel.base() && th; th = th->next)
        {
            Thinker::release(*th);
        }
    }
};

DE_PIMPL(Thinkers)
{
    dint idtable[2048];     ///< 65536 bits telling which IDs are in use.
    dushort iddealer = 0;

    std::function<void (thinker_t &)> idAssignor;
    List<ThinkerList *>       lists;
    Hash<thid_t, mobj_t *>    mobjIdLookup;    ///< public only
    Hash<thid_t, thinker_t *> thinkerIdLookup; ///< all thinkers with ID

    bool inited = false;

    Impl(Public *i) : Base(i)
    {
        clearMobjIds();
    }

    ~Impl()
    {
        // Make sure the private instances of thinkers are released.
        releaseAllThinkers();

        // Note that most thinkers are allocated from the memory zone
        // so there is no memory leak here as this memory will be purged
        // automatically when the map is "unloaded".
        deleteAll(lists);
    }

    void releaseAllThinkers()
    {
        thinkerIdLookup.clear();
        for (ThinkerList *list : lists)
        {
            list->releaseAll();
        }
    }

    void clearMobjIds()
    {
        de::zap(idtable);
        idtable[0] |= 1;  // ID zero is always "used" (it's not a valid ID).

        mobjIdLookup.clear();
        thinkerIdLookup.clear();
    }

    thid_t newMobjId()
    {
        // Increment the ID dealer until a free ID is found.
        /// @todo fixme: What if all IDs are in use? 65535 thinkers!?
        while (self().isUsedMobjId(++iddealer)) {}

        // Mark this ID as used.
        self().setMobjId(iddealer);

        return iddealer;
    }

    ThinkerList *listForThinkFunc(thinkfunc_t func, bool makePublic = true,
                                  bool canCreate = false)
    {
        for (dint i = 0; i < lists.count(); ++i)
        {
            ThinkerList *list = lists[i];
            if (list->function() == func && list->isPublic == makePublic)
                return list;
        }

        if (!canCreate) return nullptr;

        // A new thinker type.
        lists.append(new ThinkerList(func, makePublic));
        return lists.last();
    }

    DE_PIMPL_AUDIENCE(Removal)
};

DE_AUDIENCE_METHOD(Thinkers, Removal)

Thinkers::Thinkers() : d(new Impl(this))
{}

void Thinkers::setIdAssignmentFunc(const std::function<void (thinker_t &)> &func)
{
    d->idAssignor = func;
}

bool Thinkers::isUsedMobjId(thid_t id)
{
    return d->idtable[id >> 5] & (1 << (id & 31) /*(id % 32) */ );
}

void Thinkers::setMobjId(thid_t id, bool inUse)
{
    dint c = id >> 5, bit = 1 << (id & 31); //(id % 32);

    if (inUse) d->idtable[c] |= bit;
    else       d->idtable[c] &= ~bit;
}

struct mobj_s *Thinkers::mobjById(dint id)
{
    auto found = d->mobjIdLookup.find(thid_t(id));
    if (found != d->mobjIdLookup.end())
    {
        return found->second;
    }
    return nullptr;
}

thinker_t *Thinkers::find(thid_t id)
{
    auto found = d->thinkerIdLookup.find(id);
    if (found != d->thinkerIdLookup.end())
    {
        return found->second;
    }
    return nullptr;
}

void Thinkers::add(thinker_t &th, bool makePublic)
{
    if (!th.function)
        throw Error("Thinkers::add", "Invalid thinker function");

    // Will it need an ID?
    if (Thinker_IsMobj(&th))
    {
        // It is a mobj, give it an ID (not for client mobjs, though, they
        // already have an id).
        if (d->idAssignor)
        {
            d->idAssignor(th);
        }
        else
        {
            th.id = d->newMobjId();
        }

        if (makePublic && th.id)
        {
            d->mobjIdLookup.insert(th.id, reinterpret_cast<mobj_t *>(&th));
        }
    }
    else
    {
        th.id = 0;  // Zero is not a valid ID.
    }

    if (th.id)
    {
        d->thinkerIdLookup.insert(th.id, &th);
    }

    // Link the thinker to the thinker list.
    ThinkerList *list = d->listForThinkFunc(th.function, makePublic, true /*can create*/);
    list->link(th);
}

void Thinkers::remove(thinker_t &th)
{
    // Has got an ID?
    if (th.id)
    {
        // Flag the identifier as free.
        setMobjId(th.id, false);

        d->mobjIdLookup.remove(th.id);
        d->thinkerIdLookup.remove(th.id);

        DE_NOTIFY(Removal, i) i->thinkerRemoved(th);
    }

    th.function = thinkfunc_t(-1);

    Thinker::release(th);
}

void Thinkers::initLists(dbyte flags)
{
    if (!d->inited)
    {
        d->lists.clear();
    }
    else
    {
        for (dint i = 0; i < d->lists.count(); ++i)
        {
            ThinkerList *list = d->lists[i];

            if (list->isPublic && !(flags & 0x1)) continue;
            if (!list->isPublic && !(flags & 0x2)) continue;

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

LoopResult Thinkers::forAll(dbyte flags, const std::function<LoopResult (thinker_t *)>& func) const
{
    if (!d->inited) return LoopContinue;

    for (dint i = 0; i < d->lists.count(); ++i)
    {
        ThinkerList *list = d->lists[i];

        if ( list->isPublic && !(flags & 0x1)) continue;
        if (!list->isPublic && !(flags & 0x2)) continue;

        thinker_t *th = list->sentinel.next;
        while (th != &list->sentinel.base() && th)
        {
#ifdef DE_FAKE_MEMORY_ZONE
            DE_ASSERT(th->next);
            DE_ASSERT(th->prev);
#endif
            thinker_t *next = th->next;

            if (auto result = func(th))
                return result;

            th = next;
        }
    }

    return LoopContinue;
}

LoopResult Thinkers::forAll(thinkfunc_t thinkFunc,
                            dbyte       flags,
                            const std::function<LoopResult(thinker_t *)> &func) const
{
    if (!d->inited) return LoopContinue;

    if (!thinkFunc)
    {
        return forAll(flags, func);
    }

    if (flags & 0x1 /*public*/)
    {
        if (ThinkerList *list = d->listForThinkFunc(thinkFunc))
        {
            thinker_t *th = list->sentinel.next;
            while (th != &list->sentinel.base() && th)
            {
#ifdef DE_FAKE_MEMORY_ZONE
                DE_ASSERT(th->next);
                DE_ASSERT(th->prev);
#endif
                thinker_t *next = th->next;

                if (auto result = func(th))
                    return result;

                th = next;
            }
        }
    }
    if (flags & 0x2 /*private*/)
    {
        if (ThinkerList *list = d->listForThinkFunc(thinkFunc, false /*private*/))
        {
            thinker_t *th = list->sentinel.next;
            while (th != &list->sentinel.base() && th)
            {
#ifdef DE_FAKE_MEMORY_ZONE
                DE_ASSERT(th->next);
                DE_ASSERT(th->prev);
#endif
                thinker_t *next = th->next;

                if (auto result = func(th))
                    return result;

                th = next;
            }
        }
    }

    return LoopContinue;
}

dint Thinkers::count(dint *numInStasis) const
{
    dint total = 0;
    if (isInited())
    {
        for (dint i = 0; i < d->lists.count(); ++i)
        {
            ThinkerList *list = d->lists[i];
            total += list->count(numInStasis);
        }
    }
    return total;
}

thid_t Thinkers::newMobjId()
{
    return d->newMobjId();
}

}  // namespace world

void Thinker_InitPrivateData(thinker_t *th, Id::Type knownId)
{
    using namespace world;

    /// @todo The game should be asked to create its own private data. -jk

    if (th->d == nullptr)
    {
        const Id privateId = knownId? Id(knownId) : Id(/* get a new ID */);

        if (Thinker_IsMobj(th))
        {
            th->d = Factory::newMobjThinkerData(privateId);
        }
        else
        {
            // Generic thinker data (Doomsday Script namespace, etc.).
            th->d = new ThinkerData(privateId);
        }

        auto &thinkerData = THINKER_DATA(*th, ThinkerData);
        thinkerData.setThinker(th);
        thinkerData.initBindings();
    }
    else
    {
        DE_ASSERT(knownId != 0);

        // Change the private identifier of the existing thinker data.
        THINKER_DATA(*th, ThinkerData).setId(knownId);
    }
}
