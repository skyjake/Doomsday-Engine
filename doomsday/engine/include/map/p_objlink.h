/**\file p_objlink.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * p_objlink.c: Objlink management.
 *
 * Object => BspLeaf contacts and object => BspLeaf spreading.
 */

#ifndef LIBDENG_OBJLINK_BLOCKMAP_H
#define LIBDENG_OBJLINK_BLOCKMAP_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OT_MOBJ,
    OT_LUMOBJ,
    NUM_OBJ_TYPES
} objtype_t;

#define VALID_OBJTYPE(val) ((val) >= OT_MOBJ && (val) < NUM_OBJ_TYPES)

/**
 * To be called during a game change/on shutdown to destroy the objlink
 * blockmap. This is necessary because the blockmaps are allocated from
 * the Zone with a >= PU_MAP purge level and access to them is handled
 * with global pointers.
 *
 * \todo Encapsulate allocation of and access to the objlink blockmaps
 *       within de::Map
 */
void R_DestroyObjlinkBlockmap(void);

/**
 * Construct the objlink blockmap for the current map.
 */
void R_InitObjlinkBlockmapForMap(void);

/**
 * Initialize the object => BspLeaf contact lists, ready for linking to
 * objects. To be called at the beginning of a new world frame.
 */
void R_InitForNewFrame(void);

/**
 * To be called at the begining of a render frame to clear the objlink
 * blockmap prior to linking objects for the new viewer.
 */
void R_ClearObjlinksForFrame(void);

/**
 * Create a new object link of the specified @a type in the objlink blockmap.
 */
void R_ObjlinkCreate(void* object, objtype_t type);

/**
 * To be called at the beginning of a render frame to link all objects
 * into the objlink blockmap.
 */
void R_LinkObjs(void);

/**
 * Spread object => BspLeaf links for the given @a BspLeaf. Note that
 * all object types will be spread at this time.
 */
void R_InitForBspLeaf(BspLeaf* bspLeaf);

typedef struct {
    void* obj;
    objtype_t type;
} linkobjtobspleafparams_t;

/**
 * Create a new object => BspLeaf contact in the objlink blockmap.
 * Can be used as an iterator.
 *
 * @param paramaters  See: linkobjtobspleafparams_t
 *
 * @return  @c false (always).
 */
int RIT_LinkObjToBspLeaf(BspLeaf* bspLeaf, void* paramaters);

/**
 * Traverse the list of objects of the specified @a type which have been linked
 * with @a bspLeaf for the current render frame.
 */
int R_IterateBspLeafContacts2(BspLeaf* bspLeaf, objtype_t type,
    int (*func) (void* object, void* paramaters), void* paramaters);
int R_IterateBspLeafContacts(BspLeaf* bspLeaf, objtype_t type,
    int (*func) (void* object, void* paramaters)); /*paramaters=NULL*/

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_OBJLINK_BLOCKMAP_H */
