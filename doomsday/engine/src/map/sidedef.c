/**
 * @file sidedef.c
 * SideDef implementation. @ingroup map
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
#include "de_play.h"

void SideDef_UpdateBaseOrigins(SideDef* side)
{
    assert(side);
    if(!side->line) return;
    Surface_UpdateBaseOrigin(&side->SW_middlesurface);
    Surface_UpdateBaseOrigin(&side->SW_bottomsurface);
    Surface_UpdateBaseOrigin(&side->SW_topsurface);
}

void SideDef_UpdateSurfaceTangents(SideDef* side)
{
    Surface* surface = &side->SW_topsurface;
    LineDef* line = side->line;
    byte sid;
    assert(side);

    if(!line) return;

    sid = line->L_frontsidedef == side? FRONT : BACK;
    surface->normal[VY] = (line->L_vorigin(sid  )[VX] - line->L_vorigin(sid^1)[VX]) / line->length;
    surface->normal[VX] = (line->L_vorigin(sid^1)[VY] - line->L_vorigin(sid  )[VY]) / line->length;
    surface->normal[VZ] = 0;
    V3f_BuildTangents(surface->tangent, surface->bitangent, surface->normal);

    // All surfaces of a sidedef have the same vectors.
    memcpy(side->SW_middletangent, surface->tangent, sizeof(surface->tangent));
    memcpy(side->SW_middlebitangent, surface->bitangent, sizeof(surface->bitangent));
    memcpy(side->SW_middlenormal, surface->normal, sizeof(surface->normal));

    memcpy(side->SW_bottomtangent, surface->tangent, sizeof(surface->tangent));
    memcpy(side->SW_bottombitangent, surface->bitangent, sizeof(surface->bitangent));
    memcpy(side->SW_bottomnormal, surface->normal, sizeof(surface->normal));
}

int SideDef_SetProperty(SideDef* side, const setargs_t* args)
{
    assert(side);
    switch(args->prop)
    {
    case DMU_FLAGS:
        DMU_SetValue(DMT_SIDEDEF_FLAGS, &side->flags, args, 0);
        break;

    case DMU_LINEDEF:
        DMU_SetValue(DMT_SIDEDEF_LINE, &side->line, args, 0);
        break;

    default:
        Con_Error("SideDef_SetProperty: Property %s is not writable.\n", DMU_Str(args->prop));
    }
    return false; // Continue iteration.
}

int SideDef_GetProperty(const SideDef* side, setargs_t* args)
{
    assert(side);
    switch(args->prop)
    {
    case DMU_SECTOR: {
        Sector* sector = side->line->L_sector(side == side->line->L_frontsidedef? FRONT : BACK);
        DMU_GetValue(DMT_SIDEDEF_SECTOR, &sector, args, 0);
        break; }
    case DMU_LINEDEF:
        DMU_GetValue(DMT_SIDEDEF_LINE, &side->line, args, 0);
        break;
    case DMU_FLAGS:
        DMU_GetValue(DMT_SIDEDEF_FLAGS, &side->flags, args, 0);
        break;
    default:
        Con_Error("SideDef_GetProperty: Has no property %s.\n", DMU_Str(args->prop));
    }
    return false; // Continue iteration.
}
