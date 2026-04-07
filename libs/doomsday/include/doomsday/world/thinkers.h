/** @file thinkers.h  map thinkers.
 * @ingroup world
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2016 Daniel Swanson <danij@dengine.net>
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

#pragma once

#include <functional>
#include <de/error.h>
#include <de/id.h>
#include <de/observers.h>
#include "api_thinker.h"

namespace world {

class Map;

/**
 * World map thinker lists / collection.
 *
 * @ingroup world
 *
 * @todo Replace with new mechanism from the old 'new-order' branch(es).
 */
class LIBDOOMSDAY_PUBLIC Thinkers
{
public:
    DE_AUDIENCE(Removal, void thinkerRemoved(thinker_t &))

public:
    Thinkers();

    /**
     * Sets a function that gets called on each newly added thinker.
     * It should assign a new ID to the thinker if appropriate.
     */
    void setIdAssignmentFunc(const std::function<void (thinker_t &)> &);

    /**
     * Returns @c true iff the thinker lists been initialized.
     */
    bool isInited() const;

    /**
     * Init the thinker lists.
     *
     * @param flags  @c 0x1 = Init public thinkers.
     *               @c 0x2 = Init private (engine-internal) thinkers.
     */
    void initLists(de::dbyte flags);

    /**
     * @param thinker     Thinker to be added.
     * @param makePublic  @c true = @a thinker will be visible publically
     *                    via the Doomsday public API thinker interface(s).
     */
    void add(thinker_t &thinker, bool makePublic = true);

    /**
     * Deallocation is lazy -- it will not actually be freed until its
     * thinking turn comes up.
     */
    void remove(thinker_t &thinker);

    /**
     * Iterate the list of thinkers making a callback for each.
     *
     * @param flags     Thinker filter flags.
     * @param func      Callback to make for each thinker_t.
     */
    de::LoopResult forAll(de::dbyte flags, const std::function<de::LoopResult (thinker_t *th)>& func) const;

    /**
     * Iterate the list of thinkers making a callback for each.
     *
     * @param thinkFunc  Only make a callback for thinkers whose function matches this.
     * @param flags      Thinker filter flags.
     * @param func       Callback to make for each thinker_t.
     *
     * @overload
     */
    de::LoopResult forAll(thinkfunc_t thinkFunc,
                          de::dbyte   flags,
                          const std::function<de::LoopResult(thinker_t *th)> &func) const;

    /**
     * Locates a mobj by its unique identifier in the map.
     *
     * @param id  Unique id of the mobj to lookup.
     */
    struct mobj_s *mobjById(int id);

    /**
     * Finds a thinker by its identifier.
     * @param id  Thinker ID.
     * @return Thinker object or @c nullptr.
     */
    thinker_t *find(thid_t id);

    /**
     * @param id  Thinker id to test.
     */
    bool isUsedMobjId(thid_t id);

    /**
     * @param id     New thinker id.
     * @param inUse  In-use state of @a id. @c true = the id is in use.
     */
    void setMobjId(thid_t id, bool inUse = true);

    /**
     * Returns the total number of thinkers (of any type) in the collection.
     *
     * @param numInStasis  If not @c nullptr, the number of thinkers in stasis will
     *                     be added to the current value (caller must ensure to
     *                     initialize this).
     */
    int count(int *numInStasis = nullptr) const;
    
    thid_t newMobjId();

private:
    DE_PRIVATE(d)
};

}  // namespace world

LIBDOOMSDAY_PUBLIC bool        Thinker_IsMobj      (const thinker_t *th);
LIBDOOMSDAY_PUBLIC bool        Thinker_IsMobjFunc  (thinkfunc_t func);
LIBDOOMSDAY_PUBLIC world::Map &Thinker_Map         (const thinker_t &th);

/**
 * Initializes the private data object of a thinker. The type of private data is chosen
 * based on whether the thinker is on the client or server, and possibly based on other
 * factors.
 *
 * Only call this when the thinker does not have a private data object.
 *
 * @param th       Thinker.
 * @param knownId  Known private ID to use for the thinker. If zero, a new ID is generated.
 */
LIBDOOMSDAY_PUBLIC void Thinker_InitPrivateData(thinker_t *th, de::Id::Type knownId = 0);
