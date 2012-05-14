/**\file p_polyobjs.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_P_POLYOBJS_H
#define LIBDENG_P_POLYOBJS_H

#include "r_data.h"

/**
 * Action the callback if set, otherwise this is no-op.
 */
void P_PolyobjCallback(struct mobj_s* mobj, LineDef* lineDef, Polyobj* polyobj);

/**
 * The po_callback is called when a polyobj hits a mobj.
 */
void P_SetPolyobjCallback(void (*func) (struct mobj_s*, void*, void*));

void P_PolyobjChanged(Polyobj* po);

/**
 * Lookup a Polyobj on the current map by unique ID.
 *
 * @param id  Unique identifier of the Polyobj to be found.
 * @return  Found Polyobj instance else @c NULL.
 */
Polyobj* P_PolyobjByID(uint id);

/**
 * Lookup a Polyobj on the current map by tag.
 *
 * @param tag  Tag associated with the Polyobj to be found.
 * @return  Found Polyobj instance else @c NULL.
 */
Polyobj* P_PolyobjByTag(int tag);

/**
 * Lookup a Polyobj on the current map by origin.
 *
 * @param tag  Tag associated with the Polyobj to be found.
 * @return  Found Polyobj instance else @c NULL.
 */
Polyobj* P_PolyobjByBase(void* ddMobjBase);

/**
 * Translate the origin of @a polyobj in the map coordinate space.
 */
boolean P_PolyobjMove(Polyobj* polyobj, coord_t xy[]);
boolean P_PolyobjMoveXY(Polyobj* polyobj, coord_t x, coord_t y);

/**
 * Rotate @a polyobj in the map coordinate space.
 */
boolean P_PolyobjRotate(Polyobj* polyobj, angle_t angle);

/**
 * Link @a polyobj to the current map. To be called after moving, rotating
 * or any other translation of the Polyobj within the map.
 */
void P_PolyobjLink(Polyobj* polyobj);

/**
 * Unlink @a polyobj from the current map. To be called prior to moving,
 * rotating or any other translation of the Polyobj within the map.
 */
void P_PolyobjUnlink(Polyobj* polyobj);

#endif /// LIBDENG_P_POLYOBJS_H
