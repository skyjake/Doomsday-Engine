/**
 * @file p_polyob.h
 * Moveable Polygonal Map Objects (Polyobj). @ingroup map
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_MAP_POLYOB_H
#define LIBDENG_MAP_POLYOB_H

// We'll use the base polyobj template directly as our mobj.
typedef struct polyobj_s {
DD_BASE_POLYOBJ_ELEMENTS()
} polyobj_t;

#define POLYOBJ_SIZE        gx.polyobjSize

/**
 * Initialize polyobjs on the current map. To be called after map load.
 */
void P_MapInitPolyobjs(void);

/**
 * The po_callback is called when a polyobj hits a mobj.
 */
void P_SetPolyobjCallback(void (*func) (struct mobj_s*, void*, void*));

/**
 * Lookup a Polyobj on the current map by unique ID.
 *
 * @param id  Unique identifier of the Polyobj to be found.
 * @return  Found Polyobj instance else @c NULL.
 */
polyobj_t* P_PolyobjByID(uint id);

/**
 * Lookup a Polyobj on the current map by tag.
 *
 * @param tag  Tag associated with the Polyobj to be found.
 * @return  Found Polyobj instance else @c NULL.
 */
polyobj_t* P_PolyobjByTag(int tag);

/**
 * Lookup a Polyobj on the current map by origin.
 *
 * @param tag  Tag associated with the Polyobj to be found.
 * @return  Found Polyobj instance else @c NULL.
 */
polyobj_t* P_PolyobjByOrigin(void* ddMobjBase);

/**
 * Translate the origin of @a polyobj in the map coordinate space.
 */
boolean P_PolyobjMove(polyobj_t* polyobj, float xy[2]);
boolean P_PolyobjMoveXY(polyobj_t* polyobj, float x, float y);

/**
 * Rotate @a polyobj in the map coordinate space.
 */
boolean P_PolyobjRotate(polyobj_t* polyobj, angle_t angle);

/**
 * Link @a polyobj to the current map. To be called after moving, rotating
 * or any other translation of the Polyobj within the map.
 */
void P_PolyobjLink(polyobj_t* polyobj);

/**
 * Unlink @a polyobj from the current map. To be called prior to moving,
 * rotating or any other translation of the Polyobj within the map.
 */
void P_PolyobjUnlink(polyobj_t* polyobj);

/**
 * Update the Polyobj's map space axis-aligned bounding box to encompass
 * the points defined by it's vertices.
 *
 * @param polyobj  Polyobj instance.
 */
void Polyobj_UpdateAABox(polyobj_t* polyobj);

/**
 * Update the Polyobj's map space surface tangents according to the points
 * defined by the associated LineDef's vertices.
 *
 * @param polyobj  Polyobj instance.
 */
void Polyobj_UpdateSurfaceTangents(polyobj_t* polyobj);

/**
 * Iterate over the lines of the Polyobj making a callback for each.
 * Iteration ends when all lines have been visited or @a callback
 * returns non-zero.
 *
 * Caller should increment validCount if necessary before calling this
 * function as it is used to prevent repeated processing of lines.
 *
 * @param po          Polyobj instance.
 * @param callback    Callback function ptr.
 * @param paramaters  Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int Polyobj_LineIterator(polyobj_t* polyobj,
    int (*callback) (struct linedef_s*, void* paramaters), void* paramaters);

#endif /// LIBDENG_MAP_POLYOB_H
