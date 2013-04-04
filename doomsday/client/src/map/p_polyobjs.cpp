/** @file p_polyobjs.cpp
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

#define DENG_NO_API_MACROS_MAP

#include "de_base.h"
#include "de_play.h"

#include "map/gamemap.h"

// Called when the polyobj hits a mobj.
static void (*po_callback) (mobj_t *mobj, void *line, void *polyobj);

void P_PolyobjCallback(mobj_t *mobj, LineDef *line, Polyobj *polyobj)
{
    if(!po_callback) return;
    po_callback(mobj, line, polyobj);
}

#undef P_SetPolyobjCallback
DENG_EXTERN_C void P_SetPolyobjCallback(void (*func) (struct mobj_s *, void *, void *))
{
    po_callback = func;
}

#undef P_PolyobjUnlink
DENG_EXTERN_C void P_PolyobjUnlink(Polyobj *po)
{
    if(!po) return;
    /// @todo Do not assume polyobj is from the CURRENT map.
    theMap->unlinkPolyobj(*po);
}

#undef P_PolyobjLink
DENG_EXTERN_C void P_PolyobjLink(Polyobj *po)
{
    if(!po) return;
    /// @todo Do not assume polyobj is from the CURRENT map.
    theMap->linkPolyobj(*po);
}

#undef P_PolyobjByID
DENG_EXTERN_C Polyobj *P_PolyobjByID(uint index)
{
    if(!theMap) return NULL;
    return theMap->polyobjs().at(index);
}

#undef P_PolyobjByTag
DENG_EXTERN_C Polyobj *P_PolyobjByTag(int tag)
{
    if(!theMap) return NULL;
    return theMap->polyobjByTag(tag);
}

#undef P_PolyobjByBase
DENG_EXTERN_C Polyobj *P_PolyobjByBase(void *ddMobjBase)
{
    if(!theMap || !ddMobjBase) return NULL;
    return theMap->polyobjByBase(*reinterpret_cast<ddmobj_base_t *>(ddMobjBase));
}

#undef P_PolyobjMove
DENG_EXTERN_C boolean P_PolyobjMove(Polyobj *po, const_pvec3d_t xy)
{
    if(!po) return false;
    return po->move(xy);
}

#undef P_PolyobjMoveXY
DENG_EXTERN_C boolean P_PolyobjMoveXY(Polyobj *po, coord_t x, coord_t y)
{
    if(!po) return false;
    return po->move(x, y);
}

#undef P_PolyobjRotate
DENG_EXTERN_C boolean P_PolyobjRotate(Polyobj *po, angle_t angle)
{
    if(!po) return false;
    return po->rotate(angle);
}

#undef P_PolyobjFirstLine
DENG_EXTERN_C LineDef *P_PolyobjFirstLine(Polyobj *po)
{
    if(!po) return 0;
    /// @todo Do not assume polyobj is from the CURRENT map.
    return po->lines()[0];
}
