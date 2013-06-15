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

#ifndef DENG_WORLD_P_OBJLINK_H
#define DENG_WORLD_P_OBJLINK_H

class BspLeaf;

enum objtype_t
{
    OT_MOBJ       = 0,
    OT_LUMOBJ     = 1,
    NUM_OBJ_TYPES
};

#define VALID_OBJTYPE(val) ((val) >= OT_MOBJ && (val) < NUM_OBJ_TYPES)

/**
 * To be called during a game change/on shutdown to destroy the objlink
 * blockmap. This is necessary because the blockmaps are allocated from
 * the Zone with a >= PU_MAP purge level and access to them is handled
 * with global pointers.
 *
 * @todo Encapsulate allocation of and access to the objlink blockmaps
 *       within de::Map
 */
void R_DestroyObjlinkBlockmap();

/**
 * Construct the objlink blockmap for the current map.
 */
void R_InitObjlinkBlockmapForMap();

/**
 * Initialize the object => BspLeaf contact lists, ready for linking to
 * objects. To be called at the beginning of a new world frame.
 */
void R_InitForNewFrame();

/**
 * To be called at the begining of a render frame to clear the objlink
 * blockmap prior to linking objects for the new viewer.
 */
void R_ClearObjlinksForFrame();

/**
 * Create a new object link of the specified @a type in the objlink blockmap.
 */
void R_ObjlinkCreate(struct mobj_s &mobj);

#ifdef __CLIENT__
/// @copydoc R_ObjlinkCreate()
void R_ObjlinkCreate(struct lumobj_s &lumobj);
#endif

/**
 * To be called at the beginning of a render frame to link all objects
 * into the objlink blockmap.
 */
void R_LinkObjs();

/**
 * Spread object => BspLeaf links for the given @a BspLeaf. Note that
 * all object types will be spread at this time.
 */
void R_InitForBspLeaf(BspLeaf &bspLeaf);

/**
 * Create a new object => BspLeaf contact in the objlink blockmap.
 */
void R_LinkObjToBspLeaf(BspLeaf &bspLeaf, struct mobj_s &mobj);
#ifdef __CLIENT__
/// @copydoc R_LinkObjToBspLeaf()
void R_LinkObjToBspLeaf(BspLeaf &bspLeaf, struct lumobj_s &lumobj);
#endif

/**
 * Traverse the list of objects of the specified @a type which have been linked
 * with @a bspLeaf for the current render frame.
 */
int R_IterateBspLeafContacts(BspLeaf &bspLeaf, objtype_t type,
    int (*func) (void *object, void *parameters), void *parameters = 0);

#endif // DENG_WORLD_P_OBJLINK_H
