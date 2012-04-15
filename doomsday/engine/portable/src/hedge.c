/**
 * @file hedge.c
 * Map Half-edge implementation. @ingroup map
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "de_console.h"
#include "de_refresh.h"
#include "de_play.h"

HEdge* HEdge_New(void)
{
    HEdge* hedge = Z_Calloc(sizeof *hedge, PU_MAPSTATIC, 0);
    hedge->header.type = DMU_HEDGE;
    return hedge;
}

HEdge* HEdge_NewCopy(const HEdge* other)
{
    HEdge* hedge;
    assert(other);
    hedge = Z_Malloc(sizeof *hedge, PU_MAPSTATIC, 0);
    memcpy(hedge, other, sizeof *hedge);
    return hedge;
}

void HEdge_Delete(HEdge* hedge)
{
    uint i;
    assert(hedge);
    for(i = 0; i < 3; ++i)
    {
        if(hedge->bsuf[i])
        {
            SB_DestroySurface(hedge->bsuf[i]);
        }
    }
    Z_Free(hedge);
}

int HEdge_SetProperty(HEdge* hedge, const setargs_t* args)
{
    assert(hedge);
    Con_Error("HEdge_SetProperty: Property %s is not writable.\n", DMU_Str(args->prop));
    return false; // Continue iteration.
}

int HEdge_GetProperty(const HEdge* hedge, setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_VERTEX0:
        DMU_GetValue(DMT_HEDGE_V, &hedge->HE_v1, args, 0);
        break;
    case DMU_VERTEX1:
        DMU_GetValue(DMT_HEDGE_V, &hedge->HE_v2, args, 0);
        break;
    case DMU_LENGTH:
        DMU_GetValue(DMT_HEDGE_LENGTH, &hedge->length, args, 0);
        break;
    case DMU_OFFSET:
        DMU_GetValue(DMT_HEDGE_OFFSET, &hedge->offset, args, 0);
        break;
    case DMU_SIDEDEF:
        DMU_GetValue(DMT_HEDGE_SIDEDEF, &HEDGE_SIDEDEF(hedge), args, 0);
        break;
    case DMU_LINEDEF:
        DMU_GetValue(DMT_HEDGE_LINEDEF, &hedge->lineDef, args, 0);
        break;
    case DMU_FRONT_SECTOR: {
        Sector* sec = hedge->sector? hedge->sector : NULL;
        DMU_GetValue(DMT_HEDGE_SECTOR, &sec, args, 0);
        break;
      }
    case DMU_BACK_SECTOR: {
        Sector* sec = HEDGE_BACK_SECTOR(hedge);
        DMU_GetValue(DMT_HEDGE_SECTOR, &sec, args, 0);
        break;
      }
    case DMU_ANGLE:
        DMU_GetValue(DMT_HEDGE_ANGLE, &hedge->angle, args, 0);
        break;
    default:
        Con_Error("HEdge_GetProperty: No property %s.\n", DMU_Str(args->prop));
    }

    return false; // Continue iteration.
}
