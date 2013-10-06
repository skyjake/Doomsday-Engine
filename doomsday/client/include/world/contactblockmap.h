/** @file contactblockmap.h World object => BSP leaf "contact" blockmap.
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

#ifdef __CLIENT__
#ifndef DENG_CLIENT_WORLD_CONTACTBLOCKMAP_H
#define DENG_CLIENT_WORLD_CONTACTBLOCKMAP_H

#include <de/aabox.h>
#include "world/map.h"

class BspLeaf;
struct Contact;
class Lumobj;

class ContactBlockmap
{
public:
    /**
     * Construct a new contact blockmap.
     *
     * @param bounds     Bounding box of the blockmap.
     * @param blockSize  Size of each block.
     */
    ContactBlockmap(AABoxd const &bounds, uint blockSize = 128);

    /**
     * Returns the origin of the blockmap in the map coordinate space.
     */
    de::Vector2d origin() const;

    /**
     * Returns the bounds of the blockmap in the map coordinate space.
     */
    AABoxd const &bounds() const;

    /**
     * @param contact  Contact to be linked. Note that if the object's origin
     *                 lies outside the blockmap it will not be linked!
     */
    void link(Contact *contact);

    /**
     * Clear all linked contacts.
     */
    void unlinkAll();

    /**
     * Spread contacts in the blockmap to any touched neighbors.
     *
     * @param box   Map space region in which to perform spreading.
     */
    void spreadAllContacts(AABoxd const &box);

private:
    DENG2_PRIVATE(d)
};

/**
 * To be called during game change/on shutdown to destroy all contact blockmaps.
 * This is necessary because the blockmaps are allocated from the Zone using a
 * >= PU_MAP purge level and access to them is handled with global pointers.
 *
 * @todo Encapsulate allocation of and access to the blockmaps in de::Map
 */
void R_DestroyContactBlockmaps();

/**
 * Initialize contact blockmaps for the current map.
 */
void R_InitContactBlockmaps(de::Map &map);

/**
 * To be called at the beginning of a render frame to clear all contacts ready
 * for the new frame.
 */
void R_ClearContacts(de::Map &map);

/**
 * Add a new contact for the specified mobj, for spreading purposes.
 */
void R_AddContact(struct mobj_s &mobj);

/**
 * Add a new contact for the specified lumobj, for spreading purposes.
 */
void R_AddContact(Lumobj &lumobj);

/**
 * To be called to link all contacts into the contact blockmaps.
 *
 * @todo Why don't we link contacts immediately? -ds
 */
void R_LinkContacts();

/**
 * Perform all contact spreading for the specified @a BspLeaf. Note this should
 * only be called once per BSP leaf per render frame!
 */
void R_SpreadContacts(BspLeaf &bspLeaf);

/**
 * Traverse the list of mobj contacts linked directly to @a bspLeaf, for the
 * current render frame.
 */
int R_IterateBspLeafMobjContacts(BspLeaf &bspLeaf,
    int (*callback) (struct mobj_s &, void *), void *context = 0);

/**
 * Traverse the list of lumobj contacts linked directly to @a bspLeaf, for the
 * current render frame.
 */
int R_IterateBspLeafLumobjContacts(BspLeaf &bspLeaf,
    int (*callback) (Lumobj &, void *), void *context = 0);

#endif // DENG_CLIENT_WORLD_CONTACTBLOCKMAP_H
#endif // __CLIENT__
