/** @file thinkers.h  World map thinkers.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_WORLD_THINKERS_H
#define DENG_WORLD_THINKERS_H

#include <functional>
#include <de/Error>
#include "api_thinker.h"

namespace de {

/**
 * World map thinker lists / collection.
 *
 * @ingroup world
 *
 * @todo Replace with new mechanism from the old 'new-order' branch(es).
 */
class Thinkers
{
public:
    Thinkers();

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
    void initLists(dbyte flags);

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
     * @param callback  Callback to make for each thinker_t.
     */
    de::LoopResult forAll(dbyte flags, std::function<de::LoopResult (thinker_t *th)> func) const;

    /**
     * Iterate the list of thinkers making a callback for each.
     *
     * @param thinkFunc  Only make a callback for thinkers whose function matches this.
     * @param flags      Thinker filter flags.
     * @param callback   Callback to make for each thinker_t.
     *
     * @overload
     */
    de::LoopResult forAll(thinkfunc_t thinkFunc, dbyte flags, std::function<de::LoopResult (thinker_t *th)> func) const;

    /**
     * Locates a mobj by it's unique identifier in the map.
     *
     * @param id  Unique id of the mobj to lookup.
     */
    struct mobj_s *mobjById(dint id);

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
    dint count(dint *numInStasis = nullptr) const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

dd_bool Thinker_IsMobjFunc(thinkfunc_t func);
de::Map &Thinker_Map(thinker_t const &th);

/**
 * Initializes the private data object of a thinker. The type of private data is chosen
 * based on whether the thinker is on the client or server, and possibly based on other
 * factors.
 *
 * Only call this when the thinker does not have a private data object.
 *
 * @param th  Thinker.
 */
void Thinker_InitPrivateData(thinker_t *th);

#endif  // DENG_WORLD_THINKERS_H
