/**\file p_seg.c
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
#include "de_console.h"
#include "de_refresh.h"
#include "de_play.h"

int Seg_SetProperty(seg_t* seg, const setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_FLAGS:
        DMU_SetValue(DMT_SEG_FLAGS, &seg->flags, args, 0);
        break;
    default:
        Con_Error("Seg_SetProperty: Property %s is not writable.\n", DMU_Str(args->prop));
    }

    return false; // Continue iteration.
}

int Seg_GetProperty(const seg_t* seg, setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_VERTEX0:
        DMU_GetValue(DMT_SEG_V, &seg->SG_v1, args, 0);
        break;
    case DMU_VERTEX1:
        DMU_GetValue(DMT_SEG_V, &seg->SG_v2, args, 0);
        break;
    case DMU_LENGTH:
        DMU_GetValue(DMT_SEG_LENGTH, &seg->length, args, 0);
        break;
    case DMU_OFFSET:
        DMU_GetValue(DMT_SEG_OFFSET, &seg->offset, args, 0);
        break;
    case DMU_SIDEDEF:
        DMU_GetValue(DMT_SEG_SIDEDEF, &SEG_SIDEDEF(seg), args, 0);
        break;
    case DMU_LINEDEF:
        DMU_GetValue(DMT_SEG_LINEDEF, &seg->lineDef, args, 0);
        break;
    case DMU_FRONT_SECTOR: {
        sector_t* sec = NULL;
        if(seg->SG_frontsector && seg->lineDef)
            sec = seg->SG_frontsector;
        DMU_GetValue(DMT_SEG_SEC, &sec, args, 0);
        break;
      }
    case DMU_BACK_SECTOR: {
        sector_t* sec = NULL;
        if(seg->SG_backsector && seg->lineDef)
            sec = seg->SG_backsector;
        DMU_GetValue(DMT_SEG_SEC, &seg->SG_backsector, args, 0);
        break;
      }
    case DMU_FLAGS:
        DMU_GetValue(DMT_SEG_FLAGS, &seg->flags, args, 0);
        break;
    case DMU_ANGLE:
        DMU_GetValue(DMT_SEG_ANGLE, &seg->angle, args, 0);
        break;
    default:
        Con_Error("Seg_GetProperty: No property %s.\n", DMU_Str(args->prop));
    }

    return false; // Continue iteration.
}
