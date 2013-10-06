/** @file p_objlink.h World map object => BSP leaf contact blockmap.
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

#ifndef DENG_CLIENT_WORLD_OBJLINK_H
#define DENG_CLIENT_WORLD_OBJLINK_H

#ifdef __CLIENT__

#include "world/map.h"

class BspLeaf;
class Lumobj;

/**
 * To be called during a game change/on shutdown to destroy the objlink
 * blockmap. This is necessary because the blockmaps are allocated from
 * the Zone with a >= PU_MAP purge level and access to them is handled
 * with global pointers.
 *
 * @todo Encapsulate allocation of and access to the blockmaps in de::Map
 */
void R_DestroyContactBlockmaps();

/**
 * Construct the objlink blockmap for the current map.
 */
void R_InitContactBlockmaps(de::Map &map);

/**
 * Initialize the object => BspLeaf contact lists, ready for linking to
 * objects. To be called at the beginning of a new world frame.
 */
void R_ClearContacts(de::Map &map);

/**
 * Create a new object link of the specified @a type in the objlink blockmap.
 */
void R_AddContact(struct mobj_s &mobj);

/// @copydoc R_ObjlinkCreate()
void R_AddContact(Lumobj &lumobj);

/**
 * To be called at the beginning of a render frame to link all objects into the
 * objlink blockmap.
 *
 * @todo Why don't we link contacts immediately? -ds
 */
void R_LinkContacts();

/**
 * Spread object => BspLeaf links for the given @a BspLeaf. Note that all object
 * types will be spread at this time. It is assumed that the BSP leaf is @em not
 * degenerate.
 */
void R_SpreadContacts(BspLeaf &bspLeaf);

/**
 * Traverse the list of mobj contacts which have been linked with @a bspLeaf for
 * the current render frame.
 */
int R_IterateBspLeafMobjContacts(BspLeaf &bspLeaf,
    int (*callback) (struct mobj_s &, void *), void *context = 0);

/**
 * Traverse the list of lumobj contacts which have been linked with @a bspLeaf for
 * the current render frame.
 */
int R_IterateBspLeafLumobjContacts(BspLeaf &bspLeaf,
    int (*callback) (Lumobj &, void *), void *context = 0);

#endif // __CLIENT__

#endif // DENG_CLIENT_WORLD_OBJLINK_H
