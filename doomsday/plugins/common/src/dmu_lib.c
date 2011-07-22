/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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
 * dmu_lib.c: Helper routines for accessing the DMU API
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <string.h>

#include "common.h"
#include "dmu_lib.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

linedef_t* P_AllocDummyLine(void)
{
    xline_t* extra = Z_Calloc(sizeof(xline_t), PU_STATIC, 0);
    return P_AllocDummy(DMU_LINEDEF, extra);
}

void P_FreeDummyLine(linedef_t* line)
{
    Z_Free(P_DummyExtraData(line));
    P_FreeDummy(line);
}

/**
 * Copies all (changeable) properties from one line to another including the
 * extended properties.
 */
void P_CopyLine(linedef_t* dest, linedef_t* src)
{
    int                 i, sidx;
    sidedef_t*          sidefrom, *sideto;
    xline_t*            xsrc = P_ToXLine(src);
    xline_t*            xdest = P_ToXLine(dest);

    if(src == dest)
        return; // no point copying self

    // Copy the built-in properties
    for(i = 0; i < 2; ++i) // For each side
    {
        sidx = (i==0? DMU_SIDEDEF0 : DMU_SIDEDEF1);

        sidefrom = P_GetPtrp(src, sidx);
        sideto = P_GetPtrp(dest, sidx);

        if(!sidefrom || !sideto)
            continue;

#if 0
        // P_Copyp is not implemented in Doomsday yet.
        P_Copyp(DMU_TOP_MATERIAL_OFFSET_XY, sidefrom, sideto);
        P_Copyp(DMU_TOP_MATERIAL, sidefrom, sideto);
        P_Copyp(DMU_TOP_COLOR, sidefrom, sideto);

        P_Copyp(DMU_MIDDLE_MATERIAL, sidefrom, sideto);
        P_Copyp(DMU_MIDDLE_COLOR, sidefrom, sideto);
        P_Copyp(DMU_MIDDLE_BLENDMODE, sidefrom, sideto);

        P_Copyp(DMU_BOTTOM_MATERIAL, sidefrom, sideto);
        P_Copyp(DMU_BOTTOM_COLOR, sidefrom, sideto);
#else
        {
        float temp[4];
        float itemp[2];

        P_SetPtrp(sideto, DMU_TOP_MATERIAL, P_GetPtrp(sidefrom, DMU_TOP_MATERIAL));
        P_GetFloatpv(sidefrom, DMU_TOP_MATERIAL_OFFSET_XY, itemp);
        P_SetFloatpv(sideto, DMU_TOP_MATERIAL_OFFSET_XY, itemp);
        P_GetFloatpv(sidefrom, DMU_TOP_COLOR, temp);
        P_SetFloatpv(sideto, DMU_TOP_COLOR, temp);

        P_SetPtrp(sideto, DMU_MIDDLE_MATERIAL, P_GetPtrp(sidefrom, DMU_MIDDLE_MATERIAL));
        P_GetFloatpv(sidefrom, DMU_MIDDLE_COLOR, temp);
        P_GetFloatpv(sidefrom, DMU_MIDDLE_MATERIAL_OFFSET_XY, itemp);
        P_SetFloatpv(sideto, DMU_MIDDLE_MATERIAL_OFFSET_XY, itemp);
        P_SetFloatpv(sideto, DMU_MIDDLE_COLOR, temp);
        P_SetIntp(sideto, DMU_MIDDLE_BLENDMODE, P_GetIntp(sidefrom, DMU_MIDDLE_BLENDMODE));

        P_SetPtrp(sideto, DMU_BOTTOM_MATERIAL, P_GetPtrp(sidefrom, DMU_BOTTOM_MATERIAL));
        P_GetFloatpv(sidefrom, DMU_BOTTOM_MATERIAL_OFFSET_XY, itemp);
        P_SetFloatpv(sideto, DMU_BOTTOM_MATERIAL_OFFSET_XY, itemp);
        P_GetFloatpv(sidefrom, DMU_BOTTOM_COLOR, temp);
        P_SetFloatpv(sideto, DMU_BOTTOM_COLOR, temp);
        }
#endif
    }

    // Copy the extended properties too
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    xdest->special = xsrc->special;
    if(xsrc->xg && xdest->xg)
        memcpy(xdest->xg, xsrc->xg, sizeof(*xdest->xg));
    else
        xdest->xg = NULL;
#else
    xdest->special = xsrc->special;
    xdest->arg1 = xsrc->arg1;
    xdest->arg2 = xsrc->arg2;
    xdest->arg3 = xsrc->arg3;
    xdest->arg4 = xsrc->arg4;
    xdest->arg5 = xsrc->arg5;
#endif
}

/**
 * Copies all (changeable) properties from one sector to another including
 * the extended properties.
 */
void P_CopySector(sector_t* dest, sector_t* src)
{
    xsector_t*          xsrc = P_ToXSector(src);
    xsector_t*          xdest = P_ToXSector(dest);

    if(src == dest)
        return; // no point copying self.

    // Copy the built-in properties.
#if 0
    // P_Copyp is not implemented in Doomsday yet.
    P_Copyp(DMU_LIGHT_LEVEL, src, dest);
    P_Copyp(DMU_COLOR, src, dest);

    P_Copyp(DMU_FLOOR_HEIGHT, src, dest);
    P_Copyp(DMU_FLOOR_MATERIAL, src, dest);
    P_Copyp(DMU_FLOOR_COLOR, src, dest);
    P_Copyp(DMU_FLOOR_MATERIAL_OFFSET_XY, src, dest);
    P_Copyp(DMU_FLOOR_SPEED, src, dest);
    P_Copyp(DMU_FLOOR_TARGET_HEIGHT, src, dest);

    P_Copyp(DMU_CEILING_HEIGHT, src, dest);
    P_Copyp(DMU_CEILING_MATERIAL, src, dest);
    P_Copyp(DMU_CEILING_COLOR, src, dest);
    P_Copyp(DMU_CEILING_MATERIAL_OFFSET_XY, src, dest);
    P_Copyp(DMU_CEILING_SPEED, src, dest);
    P_Copyp(DMU_CEILING_TARGET_HEIGHT, src, dest);
#else
    {
    float ftemp[4];

    P_SetFloatp(dest, DMU_LIGHT_LEVEL, P_GetFloatp(src, DMU_LIGHT_LEVEL));
    P_GetFloatpv(src, DMU_COLOR, ftemp);
    P_SetFloatpv(dest, DMU_COLOR, ftemp);

    P_SetFloatp(dest, DMU_FLOOR_HEIGHT, P_GetFloatp(src, DMU_FLOOR_HEIGHT));
    P_SetPtrp(dest, DMU_FLOOR_MATERIAL, P_GetPtrp(src, DMU_FLOOR_MATERIAL));
    P_GetFloatpv(src, DMU_FLOOR_COLOR, ftemp);
    P_SetFloatpv(dest, DMU_FLOOR_COLOR, ftemp);
    P_GetFloatpv(src, DMU_FLOOR_MATERIAL_OFFSET_XY, ftemp);
    P_SetFloatpv(dest, DMU_FLOOR_MATERIAL_OFFSET_XY, ftemp);
    P_SetIntp(dest, DMU_FLOOR_SPEED, P_GetIntp(src, DMU_FLOOR_SPEED));
    P_SetFloatp(dest, DMU_FLOOR_TARGET_HEIGHT, P_GetFloatp(src, DMU_FLOOR_TARGET_HEIGHT));

    P_SetFloatp(dest, DMU_CEILING_HEIGHT, P_GetFloatp(src, DMU_CEILING_HEIGHT));
    P_SetPtrp(dest, DMU_CEILING_MATERIAL, P_GetPtrp(src, DMU_CEILING_MATERIAL));
    P_GetFloatpv(src, DMU_CEILING_COLOR, ftemp);
    P_SetFloatpv(dest, DMU_CEILING_COLOR, ftemp);
    P_GetFloatpv(src, DMU_CEILING_MATERIAL_OFFSET_XY, ftemp);
    P_SetFloatpv(dest, DMU_CEILING_MATERIAL_OFFSET_XY, ftemp);
    P_SetIntp(dest, DMU_CEILING_SPEED, P_GetIntp(src, DMU_CEILING_SPEED));
    P_SetFloatp(dest, DMU_CEILING_TARGET_HEIGHT, P_GetFloatp(src, DMU_CEILING_TARGET_HEIGHT));
    }
#endif

    // Copy the extended properties too
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    xdest->special = xsrc->special;
    xdest->soundTraversed = xsrc->soundTraversed;
    xdest->soundTarget = xsrc->soundTarget;
#if __JHERETIC__
    xdest->seqType = xsrc->seqType;
#endif
    xdest->SP_floororigheight = xsrc->SP_floororigheight;
    xdest->SP_ceilorigheight = xsrc->SP_ceilorigheight;
    xdest->origLight = xsrc->origLight;
    memcpy(xdest->origRGB, xsrc->origRGB, sizeof(float) * 3);
    if(xsrc->xg && xdest->xg)
        memcpy(xdest->xg, xsrc->xg, sizeof(*xdest->xg));
    else
        xdest->xg = NULL;
#else
    xdest->special = xsrc->special;
    xdest->soundTraversed = xsrc->soundTraversed;
    xdest->soundTarget = xsrc->soundTarget;
    xdest->seqType = xsrc->seqType;
#endif
}

float P_SectorLight(sector_t* sector)
{
    return P_GetFloatp(sector, DMU_LIGHT_LEVEL);
}

void P_SectorSetLight(sector_t* sector, float level)
{
    P_SetFloatp(sector, DMU_LIGHT_LEVEL, level);
}

void P_SectorModifyLight(sector_t* sector, float value)
{
    float               level =
        MINMAX_OF(0.f, P_SectorLight(sector) + value, 1.f);

    P_SectorSetLight(sector, level);
}

void P_SectorModifyLightx(sector_t* sector, fixed_t value)
{
    P_SetFloatp(sector, DMU_LIGHT_LEVEL,
                P_SectorLight(sector) + FIX2FLT(value) / 255.0f);
}

void* P_SectorSoundOrigin(sector_t* sec)
{
    return P_GetPtrp(sec, DMU_SOUND_ORIGIN);
}
