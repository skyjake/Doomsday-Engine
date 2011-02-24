/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * p_polyob.h: Polygon Objects
 */

#ifndef __DOOMSDAY_POLYOB_H__
#define __DOOMSDAY_POLYOB_H__

// We'll use the base polyobj template directly as our mobj.
typedef struct polyobj_s {
DD_BASE_POLYOBJ_ELEMENTS()} polyobj_t;

#define POLYOBJ_SIZE        gx.polyobjSize

extern polyobj_t** polyObjs; // List of all poly-objects on the map.
extern uint numPolyObjs;

// Polyobj system.
void            P_MapInitPolyobjs(void);
void            P_SetPolyobjCallback(void (*func) (struct mobj_s*, void*, void*));

polyobj_t*      P_GetPolyobj(uint num);
boolean         P_IsPolyobjOrigin(void* ddMobjBase);

// Polyobject interface.
boolean         P_PolyobjMove(struct polyobj_s* po, float x, float y);
boolean         P_PolyobjRotate(struct polyobj_s* po, angle_t angle);
void            P_PolyobjLink(struct polyobj_s* po);
void            P_PolyobjUnLink(struct polyobj_s* po);

void            P_PolyobjUpdateBBox(polyobj_t* po);

void            P_PolyobjLinkToRing(polyobj_t* po, linkpolyobj_t** link);
void            P_PolyobjUnlinkFromRing(polyobj_t* po, linkpolyobj_t** link);
boolean         P_PolyobjLinesIterator(polyobj_t* po, boolean (*func) (struct linedef_s*, void*),
                                       void* data);
#endif
