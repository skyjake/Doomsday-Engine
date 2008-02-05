/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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

#include "p_mapdata.h"

extern struct   polyobj_s **polyObjs; // list of all poly-objects on the level
extern uint     numPolyObjs;

// Polyobj system.
void            PO_InitForMap(void);
void            PO_SetCallback(void (*func) (mobj_t *, void *, void *));
polyobj_t      *PO_GetPolyobjForDegen(void *degenMobj);

// Polyobject interface.
boolean         P_PolyobjMove(uint num, float x, float y);
boolean         P_PolyobjRotate(uint num, angle_t angle);
void            P_PolyobjUnLink(polyobj_t *po);
void            P_PolyobjLink(polyobj_t *po);
void            P_PolyobjUpdateBBox(polyobj_t *po);

void            P_PolyobjLinkToRing(polyobj_t *po, linkpolyobj_t **link);
void            P_PolyobjUnlinkFromRing(polyobj_t *po, linkpolyobj_t **link);
boolean         P_PolyobjLinesIterator(polyobj_t *po, boolean (*func) (linedef_t *, void *),
                                       void *data);
#endif
