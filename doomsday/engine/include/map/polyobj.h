/**
 * @file polyobj.h
 * Moveable Polygonal Map Objects (Polyobj). @ingroup map
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

#ifndef LIBDENG_MAP_POLYOBJ_H
#define LIBDENG_MAP_POLYOBJ_H

#include "dd_share.h"

// We'll use the base polyobj template directly as our polyobj.
typedef struct polyobj_s {
DD_BASE_POLYOBJ_ELEMENTS()
} Polyobj;

#define POLYOBJ_SIZE        gx.polyobjSize

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Translate the origin in the map coordinate space.
 *
 * @param po     Polyobj instance.
 * @param delta  Movement delta on the X|Y plane of the map coordinate space.
 */
boolean Polyobj_Move(Polyobj* po, coord_t delta[2]);
boolean Polyobj_MoveXY(Polyobj* po, coord_t x, coord_t y);

/**
 * Rotate in the map coordinate space.
 *
 * @param po     Polyobj instance.
 * @param angle  World angle delta.
 */
boolean Polyobj_Rotate(Polyobj* po, angle_t angle);

/**
 * Update the Polyobj's map space axis-aligned bounding box to encompass
 * the points defined by it's vertices.
 *
 * @param polyobj  Polyobj instance.
 */
void Polyobj_UpdateAABox(Polyobj* polyobj);

/**
 * Update the Polyobj's map space surface tangents according to the points
 * defined by the associated LineDef's vertices.
 *
 * @param polyobj  Polyobj instance.
 */
void Polyobj_UpdateSurfaceTangents(Polyobj* polyobj);

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
int Polyobj_LineIterator(Polyobj* polyobj,
    int (*callback) (struct linedef_s*, void* paramaters), void* paramaters);

#if 0
boolean Polyobj_GetProperty(const Polyobj* po, setargs_t* args);
boolean Polyobj_SetProperty(Polyobj* po, const setargs_t* args);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_MAP_POLYOB_H
