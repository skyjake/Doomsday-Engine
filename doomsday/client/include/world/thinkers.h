/** @file thinkers.h World map thinkers.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include "api_thinker.h"

#include <de/Error>

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
    void initLists(byte flags);

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
     * @param thinkFunc  If not @c NULL, only make a callback for thinkers
     *                   whose function matches this.
     * @param flags      Thinker filter flags.
     * @param callback   The callback to make. Iteration will continue
     *                   until a callback returns a non-zero value.
     * @param context  Passed to the callback function.
     */
    int iterate(thinkfunc_t thinkFunc, byte flags,
                int (*callback) (thinker_t *th, void *), void *context);

    /**
     * Locates a mobj by it's unique identifier in the map.
     *
     * @param id  Unique id of the mobj to lookup.
     */
    struct mobj_s *mobjById(int id);

    /**
     * @param id  Thinker id to test.
     */
    bool isUsedMobjId(thid_t id);

    /**
     * @param id     New thinker id.
     * @param inUse  In-use state of @a id. @c true = the id is in use.
     */
    void setMobjId(thid_t id, bool inUse = true);

    void clearMobjIds();

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // DENG_WORLD_THINKERS_H
