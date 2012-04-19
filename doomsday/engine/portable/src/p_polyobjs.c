/**\file p_polyobjs.c
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

#include "de_base.h"
#include "de_play.h"

// Called when the polyobj hits a mobj.
static void (*po_callback) (mobj_t* mobj, void* line, void* polyobj);

void P_PolyobjCallback(mobj_t* mobj, LineDef* lineDef, Polyobj* polyobj)
{
    if(!po_callback) return;
    po_callback(mobj, lineDef, polyobj);
}

void P_SetPolyobjCallback(void (*func) (struct mobj_s*, void*, void*))
{
    po_callback = func;
}

void P_PolyobjChanged(Polyobj* po)
{
    LineDef** lineIter;
    for(lineIter = po->lines; *lineIter; lineIter++)
    {
        LineDef* line = *lineIter;
        HEdge* hedge = line->L_frontside->hedgeLeft;
        int i;

        // Shadow bias must be told.
        for(i = 0; i < 3; ++i)
        {
            SB_SurfaceMoved(hedge->bsuf[i]);
        }
    }
}

void P_PolyobjUnlink(Polyobj* po)
{
    GameMap* map = theMap; /// @fixme Do not assume polyobj is from the CURRENT map.
    GameMap_UnlinkPolyobj(map, po);
}

void P_PolyobjLink(Polyobj* po)
{
    GameMap* map = theMap; /// @fixme Do not assume polyobj is from the CURRENT map.
    GameMap_LinkPolyobj(map, po);
}

/// @note Part of the Doomsday public API
Polyobj* P_PolyobjByID(uint id)
{
    if(!theMap) return NULL;
    return GameMap_PolyobjByID(theMap, id);
}

/// @note Part of the Doomsday public API
Polyobj* P_PolyobjByTag(int tag)
{
    if(!theMap) return NULL;
    return GameMap_PolyobjByTag(theMap, tag);
}

/// @note Part of the Doomsday public API
Polyobj* P_PolyobjByOrigin(void* ddMobjBase)
{
    if(!theMap) return NULL;
    return GameMap_PolyobjByBase(theMap, ddMobjBase);
}

/// @note Part of the Doomsday public API
boolean P_PolyobjMove(Polyobj* po, coord_t xy[2])
{
    if(!po) return false;
    return Polyobj_Move(po, xy);
}

/// @note Part of the Doomsday public API
boolean P_PolyobjMoveXY(Polyobj* po, coord_t x, coord_t y)
{
    if(!po) return false;
    return Polyobj_MoveXY(po, x, y);
}

/// @note Part of the Doomsday public API
boolean P_PolyobjRotate(Polyobj* po, angle_t angle)
{
    if(!po) return false;
    return Polyobj_Rotate(po, angle);
}
