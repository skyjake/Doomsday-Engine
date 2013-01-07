/**
 * @file generators.h
 * Generator collection. @ingroup map
 *
 * A collection of ptcgen_t instances and implements all bookkeeping logic
 * pertinent to the management of said instances.
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_MAP_GENERATORS
#define LIBDENG_MAP_GENERATORS

#include "p_particle.h"

/// Unique identifier associated with each generator in the collection.
typedef short ptcgenid_t;

/// Maximum number of ptcgen_ts supported by a Generators instance.
#define GENERATORS_MAX                  512

/**
 * Generators instance. Created with Generators_New().
 */
typedef struct generators_s Generators;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Constructs a new generators collection. Must be deleted with Generators_Delete().
 *
 * @param listCount  Number of lists the collection must support.
 */
Generators* Generators_New(uint listCount);

/**
 * Destructs the generators collection @a generators.
 * @param generators  Generators instance.
 */
void Generators_Delete(Generators* generators);

/**
 * Clear all ptcgen_t references in this collection.
 *
 * @warning Does nothing about any memory allocated for said instances.
 *
 * @param generators  Generators instance.
 */
void Generators_Clear(Generators* generators);

/**
 * Retrieve the generator associated with the unique @a generatorId
 *
 * @param generators   Generators instance.
 * @param generatorId  Unique id of the generator to lookup.
 * @return  Pointer to ptcgen iff found, else @c NULL.
 */
ptcgen_t* Generators_Generator(Generators* generators, ptcgenid_t generatorId);

/**
 * Lookup the unique id of @a generator in this collection.
 *
 * @param generators  Generators instance.
 * @param generator   Generator to lookup an id for.
 * @return  The unique id if found else @c -1 iff if @a generator is not linked.
 */
ptcgenid_t Generators_GeneratorId(Generators* generators, const ptcgen_t* generator);

/**
 * Retrieve the next available generator id.
 *
 * @param generators  Generators instance.
 * @return  The next available id else @c -1 iff there are no unused ids.
 */
ptcgenid_t Generators_NextAvailableId(Generators* generators);

/**
 * Unlink a generator from this collection. Ownership is unaffected.
 *
 * @param generators  Generators instance.
 * @param generator   Generator to be unlinked.
 * @return  Same as @a generator for caller convenience.
 */
ptcgen_t* Generators_Unlink(Generators* generators, ptcgen_t* generator);

/**
 * Link a generator into this collection. Ownership does NOT transfer to
 * the collection.
 *
 * @param generators  Generators instance.
 * @param slot        Logical slot into which the generator will be linked.
 * @param generator   Generator to link.
 *
 * @return  Same as @a generator for caller convenience.
 */
ptcgen_t* Generators_Link(Generators* generators, ptcgenid_t slot, ptcgen_t* generator);

/**
 * Empty all generator link lists.
 *
 * @param generators  Generators instance.
 */
void Generators_EmptyLists(Generators* generators);

/**
 * Link the a sector with a generator.
 *
 * @param generators  Generators instance.
 * @param generator   Generator to link with the identified list.
 * @param listIndex  Index of the list to link the generator on.
 *
 * @return  Same as @a generator for caller convenience.
 */
ptcgen_t* Generators_LinkToList(Generators* generators, ptcgen_t* generator, uint listIndex);

/**
 * Iterate over all generators in the collection making a callback for each.
 * Iteration ends when all generators have been processed or a callback returns
 * non-zero.
 *
 * @param generators  Generators instance.
 * @param callback  Callback to make for each iteration.
 * @param parameters  User data to be passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int Generators_Iterate(Generators* generators, int (*callback) (ptcgen_t*, void*), void* parameters);

/**
 * Iterate over all generators in the collection which are present on the identified
 * list making a callback for each. Iteration ends when all targeted generators have
 * been processed or a callback returns non-zero.
 *
 * @param generators  Generators instance.
 * @param listIndex  Index of the list to traverse.
 * @param callback  Callback to make for each iteration.
 * @param parameters  User data to be passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int Generators_IterateList(Generators* generators, uint listIndex,
    int (*callback) (ptcgen_t*, void*), void* parameters);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_MAP_GENERATORS
