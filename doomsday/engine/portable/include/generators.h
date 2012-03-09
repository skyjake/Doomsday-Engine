/**
 * @file generators.h
 * Generator collection. @ingroup map
 *
 * A collection of ptcgen_t instances and implements all bookkeeping logic
 * pertinent to the management of said instances.
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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
#define GENERATORS_MAX                  (256)

/**
 * Generators instance. Created with Generators_New().
 */
typedef struct generators_s Generators;

/**
 * Constructs a new generators collection. Must be deleted with Generators_Delete().
 *
 * @param sectorCount  Number of sectors the collection must support.
 */
Generators* Generators_New(uint sectorCount);

/**
 * Destructs the generators collection @a generators.
 * @param generators  Generators instance.
 */
void Generators_Delete(Generators* generators);

/**
 * Clear all references to any ptcgen_t instances currently owned by this
 * collection.
 *
 * @warning Does nothing about any memory allocated for said instances.
 *          It is therefore the caller's responsibilty to
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
 * @return  Same as @a generator for caller convenience.
 */
ptcgen_t* Generators_Link(Generators* generators, ptcgenid_t slot, ptcgen_t* generator);

/**
 * Clear all sector --> generator links.
 *
 * @param generators  Generators instance.
 */
void Generators_ClearSectorLinks(Generators* generators);

/**
 * Link the a sector with a generator.
 *
 * @param generators  Generators instance.
 * @param generator   Generator to link with the identified sector.
 * @param sectorIndex  Index of the sector to link the generator with.
 *
 * @return  Same as @a generator for caller convenience.
 */
ptcgen_t* Generators_LinkToSector(Generators* generators, ptcgen_t* generator, uint sectorIndex);

/**
 * Walk the entire list of generators.
 *
 * @param generators  Generators instance.
 */
int Generators_Iterate(Generators* generators, int (*callback) (ptcgen_t*, void*), void* parameters);

/**
 * Walk the list of sector-linked generators.
 *
 * @param generators  Generators instance.
 */
int Generators_IterateSectorLinked(Generators* generators, uint sectorIndex,
    int (*callback) (ptcgen_t*, void*), void* parameters);

#endif /// LIBDENG_MAP_GENERATORS
