/** @file world/generators.h Generator collection.
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef DENG_WORLD_GENERATORS_H
#define DENG_WORLD_GENERATORS_H

#include <de/libdeng2.h>

#include "p_particle.h"

namespace de {

/**
 * A collection of ptcgen_t instances and implements all bookkeeping logic
 * pertinent to the management of said instances.
 *
 * @ingroup world
 */
class Generators
{
public:
    /// Unique identifier associated with each generator in the collection.
    typedef short ptcgenid_t;

    /// Maximum number of ptcgen_ts supported by a Generators instance.
    static int const GENERATORS_MAX = 512;

public:
    /**
     * Construct a new generators collection.
     *
     * @param listCount  Number of lists the collection must support.
     */
    Generators(uint listCount);

    /**
     * Clear all ptcgen_t references in this collection.
     *
     * @warning Does nothing about any memory allocated for said instances.
     */
    void clear();

    /**
     * Retrieve the generator associated with the unique @a generatorId
     *
     * @param id  Unique id of the generator to lookup.
     * @return  Pointer to ptcgen iff found, else @c NULL.
     */
    ptcgen_t *generator(ptcgenid_t id) const;

    /**
     * Lookup the unique id of @a generator in this collection.
     *
     * @param generator   Generator to lookup an id for.
     * @return  The unique id if found else @c -1 iff if @a generator is not linked.
     */
    ptcgenid_t generatorId(ptcgen_t const *generator) const;

    /**
     * Retrieve the next available generator id.
     *
     * @return  The next available id else @c -1 iff there are no unused ids.
     */
    ptcgenid_t nextAvailableId() const;

    /**
     * Link a generator into this collection. Ownership does NOT transfer to
     * the collection.
     *
     * @param generator  Generator to link.
     * @param slot       Logical slot into which the generator will be linked.
     *
     * @return  Same as @a generator for caller convenience.
     */
    ptcgen_t *link(ptcgen_t *generator, ptcgenid_t slot);

    /**
     * Unlink a generator from this collection. Ownership is unaffected.
     *
     * @param generator   Generator to be unlinked.
     * @return  Same as @a generator for caller convenience.
     */
    ptcgen_t *unlink(ptcgen_t *generator);

    /**
     * Empty all generator link lists.
     */
    void emptyLists();

    /**
     * Link the a sector with a generator.
     *
     * @param generator   Generator to link with the identified list.
     * @param listIndex  Index of the list to link the generator on.
     *
     * @return  Same as @a generator for caller convenience.
     */
    ptcgen_t *linkToList(ptcgen_t *generator, uint listIndex);

    /**
     * Iterate over all generators in the collection making a callback for each.
     * Iteration ends when all generators have been processed or a callback returns
     * non-zero.
     *
     * @param callback  Callback to make for each iteration.
     * @param parameters  User data to be passed to the callback.
     *
     * @return  @c 0 iff iteration completed wholly.
     */
    int iterate(int (*callback) (ptcgen_t *, void *), void *parameters = 0);

    /**
     * Iterate over all generators in the collection which are present on the identified
     * list making a callback for each. Iteration ends when all targeted generators have
     * been processed or a callback returns non-zero.
     *
     * @param listIndex  Index of the list to traverse.
     * @param callback  Callback to make for each iteration.
     * @param parameters  User data to be passed to the callback.
     *
     * @return  @c 0 iff iteration completed wholly.
     */
    int iterateList(uint listIndex, int (*callback) (ptcgen_t *, void *), void *parameters = 0);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // DENG_WORLD_GENERATORS_H
