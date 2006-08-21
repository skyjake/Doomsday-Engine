/* DE1: $Id$
 * Copyright (C) 2005 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * dmu_lib.c: Helper routines for accessing the DMU API
 */

// HEADER FILES ------------------------------------------------------------

#if  __DOOM64TC__
#  include "doom64tc.h"
#elif __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

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

line_t *P_AllocDummyLine(void)
{
    xline_t* extra = Z_Calloc(sizeof(xline_t), PU_STATIC, 0);
    return P_AllocDummy(DMU_LINE, extra);
}

void P_FreeDummyLine(line_t* line)
{
    Z_Free(P_DummyExtraData(line));
    P_FreeDummy(line);
}

/*
 * Copies all (changeable) properties from one line
 * to another including the extended properties.
 */
void P_CopyLine(line_t* from, line_t* to)
{
    int i, sidx;
    side_t *sidefrom, *sideto;
    xline_t* xfrom = P_XLine(from);
    xline_t* xto = P_XLine(to);

    if(from == to)
        return; // no point copying self

    // Copy the built-in properties
#if 0
    P_Copyp(DMU_FLAGS, from, to);
#else
    P_SetIntp(to, DMU_FLAGS, P_GetIntp(from, DMU_FLAGS));
#endif

    for(i = 0; i < 2; ++i) // For each side
    {
        sidx = (i==0? DMU_SIDE0 : DMU_SIDE1);

        sidefrom = P_GetPtrp(from, sidx);
        sideto = P_GetPtrp(to, sidx);

        if(!sidefrom || !sideto)
            continue;

#if 0
        // P_Copyp is not implemented in Doomsday yet.
        P_Copyp(DMU_TEXTURE_OFFSET_XY, sidefrom, sideto);

        P_Copyp(DMU_TOP_TEXTURE, sidefrom, sideto);
        P_Copyp(DMU_TOP_COLOR, sidefrom, sideto);

        P_Copyp(DMU_MIDDLE_TEXTURE, sidefrom, sideto);
        P_Copyp(DMU_MIDDLE_COLOR, sidefrom, sideto);
        P_Copyp(DMU_MIDDLE_BLENDMODE, sidefrom, sideto);

        P_Copyp(DMU_BOTTOM_TEXTURE, sidefrom, sideto);
        P_Copyp(DMU_BOTTOM_COLOR, sidefrom, sideto);
#else
        {
        byte temp[4];
        int  itemp[2];

        P_GetIntpv(sidefrom, DMU_TEXTURE_OFFSET_XY, itemp);
        P_SetIntpv(sideto, DMU_TEXTURE_OFFSET_XY, itemp);

        P_SetIntp(sideto, DMU_TOP_TEXTURE, P_GetIntp(sidefrom, DMU_TOP_TEXTURE));

        P_GetBytepv(sidefrom, DMU_TOP_COLOR, temp);
        P_SetBytepv(sideto, DMU_TOP_COLOR, temp);

        P_SetIntp(sideto, DMU_MIDDLE_TEXTURE, P_GetIntp(sidefrom, DMU_MIDDLE_TEXTURE));
        P_GetBytepv(sidefrom, DMU_MIDDLE_COLOR, temp);
        P_SetBytepv(sideto, DMU_MIDDLE_COLOR, temp);
        P_SetIntp(sideto, DMU_MIDDLE_BLENDMODE, P_GetIntp(sidefrom, DMU_MIDDLE_BLENDMODE));

        P_SetIntp(sideto, DMU_BOTTOM_TEXTURE, P_GetIntp(sidefrom, DMU_BOTTOM_TEXTURE));
        P_GetBytepv(sidefrom, DMU_BOTTOM_COLOR, temp);
        P_SetBytepv(sideto, DMU_BOTTOM_COLOR, temp);
        }
#endif
    }

    // Copy the extended properties too
#if __JDOOM__ || __JHERETIC__
    xto->special = xfrom->special;
    xto->tag = xfrom->tag;
    xto->specialdata = xfrom->specialdata;
    if(xfrom->xg && xto->xg)
        memcpy(xto->xg, xfrom->xg, sizeof(*xto->xg));
    else
        xto->xg = NULL;
#else
    xto->special = xfrom->special;
    xto->arg1 = xfrom->arg1;
    xto->arg2 = xfrom->arg2;
    xto->arg3 = xfrom->arg3;
    xto->arg4 = xfrom->arg4;
    xto->arg5 = xfrom->arg5;
    xto->specialdata = xfrom->specialdata;
#endif
}

/*
 * Copies all (changeable) properties from one sector
 * to another including the extended properties.
 */
void P_CopySector(sector_t* from, sector_t* to)
{
    xsector_t* xfrom = P_XSector(from);
    xsector_t* xto = P_XSector(to);

    if(from == to)
        return; // no point copying self

    // Copy the built-in properties
#if 0
    // P_Copyp is not implemented in Doomsday yet.
    P_Copyp(DMU_LIGHT_LEVEL, from, to);
    P_Copyp(DMU_COLOR, from, to);
    P_Copyp(DMU_SOUND_REVERB, from, to);

    P_Copyp(DMU_FLOOR_HEIGHT, from, to);
    P_Copyp(DMU_FLOOR_TEXTURE, from, to);
    P_Copyp(DMU_FLOOR_COLOR, from, to);
    P_Copyp(DMU_FLOOR_OFFSET_XY, from, to);
    P_Copyp(DMU_FLOOR_TEXTURE_MOVE_XY, from, to);
    P_Copyp(DMU_FLOOR_SPEED, from, to);
    P_Copyp(DMU_FLOOR_TARGET, from, to);

    P_Copyp(DMU_CEILING_HEIGHT, from, to);
    P_Copyp(DMU_CEILING_TEXTURE, from, to);
    P_Copyp(DMU_CEILING_COLOR, from, to);
    P_Copyp(DMU_CEILING_OFFSET_XY, from, to);
    P_Copyp(DMU_CEILING_TEXTURE_MOVE_XY, from, to);
    P_Copyp(DMU_CEILING_SPEED, from, to);
    P_Copyp(DMU_CEILING_TARGET, from, to);
#else
    {
    byte temp[4];
    int  itemp[2];
    float ftemp[NUM_REVERB_DATA];

    P_SetIntp(to, DMU_LIGHT_LEVEL, P_GetIntp(from, DMU_LIGHT_LEVEL));
    P_GetBytepv(from, DMU_COLOR, temp);
    P_SetBytepv(to, DMU_COLOR, temp);
    P_GetFloatpv(from, DMU_SOUND_REVERB, ftemp);
    P_SetFloatpv(to, DMU_SOUND_REVERB, ftemp);

    P_SetFixedp(to, DMU_FLOOR_HEIGHT, P_GetFixedp(from, DMU_FLOOR_HEIGHT));
    P_SetIntp(to, DMU_FLOOR_TEXTURE, P_GetIntp(from, DMU_FLOOR_TEXTURE));
    P_GetBytepv(from, DMU_FLOOR_COLOR, temp);
    P_SetBytepv(to, DMU_FLOOR_COLOR, temp);
    P_GetFloatpv(from, DMU_FLOOR_OFFSET_XY, ftemp);
    P_SetFloatpv(to, DMU_FLOOR_OFFSET_XY, ftemp);
    P_GetIntpv(from, DMU_FLOOR_TEXTURE_MOVE_XY, itemp);
    P_SetIntpv(to, DMU_FLOOR_TEXTURE_MOVE_XY, itemp);
    P_SetIntp(to, DMU_FLOOR_SPEED, P_GetIntp(from, DMU_FLOOR_SPEED));
    P_SetIntp(to, DMU_FLOOR_TARGET, P_GetIntp(from, DMU_FLOOR_TARGET));

    P_SetFixedp(to, DMU_CEILING_HEIGHT, P_GetFixedp(from, DMU_CEILING_HEIGHT));
    P_SetIntp(to, DMU_CEILING_TEXTURE, P_GetIntp(from, DMU_CEILING_TEXTURE));
    P_GetBytepv(from, DMU_CEILING_COLOR, temp);
    P_SetBytepv(to, DMU_CEILING_COLOR, temp);
    P_GetFloatpv(from, DMU_CEILING_OFFSET_XY, ftemp);
    P_SetFloatpv(to, DMU_CEILING_OFFSET_XY, ftemp);
    P_GetIntpv(from, DMU_CEILING_TEXTURE_MOVE_XY, itemp);
    P_SetIntpv(to, DMU_CEILING_TEXTURE_MOVE_XY, itemp);
    P_SetIntp(to, DMU_CEILING_SPEED, P_GetIntp(from, DMU_CEILING_SPEED));
    P_SetIntp(to, DMU_CEILING_TARGET, P_GetIntp(from, DMU_CEILING_TARGET));
    }
#endif

    // Copy the extended properties too
#if __JDOOM__ || __JHERETIC__
    xto->special = xfrom->special;
    xto->tag = xfrom->tag;
    xto->soundtraversed = xfrom->soundtraversed;
    xto->soundtarget = xfrom->soundtarget;
    xto->specialdata = xfrom->specialdata;
#if __JHERETIC__
    xto->seqType = xfrom->seqType;
#endif
    xto->origfloor = xfrom->origfloor;
    xto->origceiling = xfrom->origceiling;
    xto->origlight = xfrom->origlight;
    memcpy(xto->origrgb, xfrom->origrgb, 3);
    xto->xg = xfrom->xg;
#else
    xto->special = xfrom->special;
    xto->tag = xfrom->tag;
    xto->soundtraversed = xfrom->soundtraversed;
    xto->soundtarget = xfrom->soundtarget;
    xto->seqType = xfrom->seqType;
    xto->specialdata = xfrom->specialdata;
#endif
}

int P_SectorLight(sector_t* sector)
{
    return P_GetIntp(sector, DMU_LIGHT_LEVEL);
}

void P_SectorSetLight(sector_t* sector, int level)
{
    P_SetIntp(sector, DMU_LIGHT_LEVEL, level);
}

void P_SectorModifyLight(sector_t* sector, int value)
{
    int level = P_SectorLight(sector);

    level += value;

    if(level < 0) level = 0;
    if(level > 255) level = 255;

    P_SectorSetLight(sector, level);
}

fixed_t P_SectorLightx(sector_t* sector)
{
    return P_GetFixedp(sector, DMU_LIGHT_LEVEL);
}

void P_SectorModifyLightx(sector_t* sector, fixed_t value)
{
    P_SetFixedp(sector, DMU_LIGHT_LEVEL,
                P_SectorLightx(sector) + value);
}

void *P_SectorSoundOrigin(sector_t *sec)
{
    return P_GetPtrp(sec, DMU_SOUND_ORIGIN);
}
