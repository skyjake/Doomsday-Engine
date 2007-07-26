/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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

/*
 * p_polyob.h: Polygon Objects
 */

#ifndef __DOOMSDAY_POLYOB_H__
#define __DOOMSDAY_POLYOB_H__

#include "p_mapdata.h"

extern struct   polyobj_s *polyobjs; // list of all poly-objects on the level
extern uint     po_NumPolyobjs;

void            PO_SetCallback(void (*func) (mobj_t *, void *, void *));
boolean         PO_MovePolyobj(uint num, int x, int y);
boolean         PO_RotatePolyobj(uint num, angle_t angle);
void            PO_UnLinkPolyobj(polyobj_t *po);
void            PO_LinkPolyobj(polyobj_t *po);
polyobj_t      *PO_GetForDegen(void *degenMobj);
void            PO_SetupPolyobjs(void);

#endif
