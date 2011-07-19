/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * World linedefs.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_refresh.h"
#include "de_play.h"

#include "m_bams.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static void calcNormal(const linedef_t* l, byte side, pvec2_t normal)
{
    V2_Set(normal, (l->L_vpos(side^1)[VY] - l->L_vpos(side)[VY])   / l->length,
                   (l->L_vpos(side)[VX]   - l->L_vpos(side^1)[VX]) / l->length);
}

static float lightLevelDelta(const pvec2_t normal)
{
    return (1.0f / 255) * (normal[VX] * 18) * rendLightWallAngle;
}

static linedef_t* findBlendNeighbor(const linedef_t* l, byte side, byte right,
    binangle_t* diff)
{
    if(!l->L_backside ||
       l->L_backsector->SP_ceilvisheight <= l->L_frontsector->SP_floorvisheight ||
       l->L_backsector->SP_floorvisheight >= l->L_frontsector->SP_ceilvisheight)
    {
        return R_FindSolidLineNeighbor(l->L_sector(side), l, l->L_vo(right^side), right, diff);
    }

    return R_FindLineNeighbor(l->L_sector(side), l, l->L_vo(right^side), right, diff);
}

/**
 * The DOOM lighting model applies a sector light level delta when drawing
 * segs based on their 2D world angle.
 *
 * @param l             Linedef to calculate the delta for.
 * @param side          Side of the linedef we are interested in.
 * @param deltaL        Light delta for the left edge is written back here.
 * @param deltaR        Light delta for the right edge is written back here.
 */
void Linedef_LightLevelDelta(const linedef_t* l, byte side, float* deltaL, float* deltaR)
{
    vec2_t normal;
    float delta;

    // Disabled?
    if(!(rendLightWallAngle > 0))
    {
        *deltaL = *deltaR = 0;
        return;
    }

    calcNormal(l, side, normal);
    delta = lightLevelDelta(normal);

    // If smoothing is disabled use this delta for left and right edges.
    // Must forcibly disable smoothing for polyobj linedefs as they have
    // no owner rings.
    if(!rendLightWallAngleSmooth || (l->inFlags & LF_POLYOBJ))
    {
        *deltaL = *deltaR = delta;
        return;
    }

    // Find the left neighbour linedef for which we will calculate the
    // lightlevel delta and then blend with this to produce the value for
    // the left edge. Blend iff the angle between the two linedefs is less
    // than 45 degrees.
    {
    binangle_t diff = 0;
    linedef_t* other = findBlendNeighbor(l, side, 0, &diff);
    if(other && INRANGE_OF(diff, BANG_180, BANG_45))
    {
        vec2_t otherNormal;

        calcNormal(other, other->L_v2 != l->L_v(side), otherNormal);

        // Average normals.
        V2_Sum(otherNormal, otherNormal, normal);
        otherNormal[VX] /= 2; otherNormal[VY] /= 2;

        *deltaL = lightLevelDelta(otherNormal);
    }
    else
        *deltaL = delta;
    }

    // Do the same for the right edge but with the right neighbour linedef.
    {
    binangle_t diff = 0;
    linedef_t* other = findBlendNeighbor(l, side, 1, &diff);
    if(other && INRANGE_OF(diff, BANG_180, BANG_45))
    {
        vec2_t otherNormal;

        calcNormal(other, other->L_v1 != l->L_v(side^1), otherNormal);

        // Average normals.
        V2_Sum(otherNormal, otherNormal, normal);
        otherNormal[VX] /= 2; otherNormal[VY] /= 2;

        *deltaR = lightLevelDelta(otherNormal);
    }
    else
        *deltaR = delta;
    }
}

/**
 * Update the linedef, property is selected by DMU_* name.
 */
boolean Linedef_SetProperty(linedef_t *lin, const setargs_t *args)
{
    switch(args->prop)
    {
    case DMU_FRONT_SECTOR:
        DMU_SetValue(DMT_LINEDEF_SEC, &lin->L_frontsector, args, 0);
        break;
    case DMU_BACK_SECTOR:
        DMU_SetValue(DMT_LINEDEF_SEC, &lin->L_backsector, args, 0);
        break;
    case DMU_SIDEDEF0:
        DMU_SetValue(DMT_LINEDEF_SIDEDEFS, &lin->L_frontside, args, 0);
        break;
    case DMU_SIDEDEF1:
        DMU_SetValue(DMT_LINEDEF_SIDEDEFS, &lin->L_backside, args, 0);
        break;
    case DMU_VALID_COUNT:
        DMU_SetValue(DMT_LINEDEF_VALIDCOUNT, &lin->validCount, args, 0);
        break;
    case DMU_FLAGS:
        {
        sidedef_t          *s;

        DMU_SetValue(DMT_LINEDEF_FLAGS, &lin->flags, args, 0);

        s = lin->L_frontside;
        Surface_Update(&s->SW_topsurface);
        Surface_Update(&s->SW_bottomsurface);
        Surface_Update(&s->SW_middlesurface);
        if(lin->L_backside)
        {
            s = lin->L_backside;
            Surface_Update(&s->SW_topsurface);
            Surface_Update(&s->SW_bottomsurface);
            Surface_Update(&s->SW_middlesurface);
        }
        break;
        }
    default:
        Con_Error("Linedef_SetProperty: Property %s is not writable.\n",
                  DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}

/**
 * Get the value of a linedef property, selected by DMU_* name.
 */
boolean Linedef_GetProperty(const linedef_t *lin, setargs_t *args)
{
   switch(args->prop)
    {
    case DMU_VERTEX0:
        DMU_GetValue(DMT_LINEDEF_V, &lin->L_v1, args, 0);
        break;
    case DMU_VERTEX1:
        DMU_GetValue(DMT_LINEDEF_V, &lin->L_v2, args, 0);
        break;
    case DMU_DX:
        DMU_GetValue(DMT_LINEDEF_DX, &lin->dX, args, 0);
        break;
    case DMU_DY:
        DMU_GetValue(DMT_LINEDEF_DY, &lin->dY, args, 0);
        break;
    case DMU_DXY:
        DMU_GetValue(DMT_LINEDEF_DX, &lin->dX, args, 0);
        DMU_GetValue(DMT_LINEDEF_DY, &lin->dY, args, 1);
        break;
    case DMU_LENGTH:
        DMU_GetValue(DDVT_FLOAT, &lin->length, args, 0);
        break;
    case DMU_ANGLE:
        DMU_GetValue(DDVT_ANGLE, &lin->angle, args, 0);
        break;
    case DMU_SLOPE_TYPE:
        DMU_GetValue(DMT_LINEDEF_SLOPETYPE, &lin->slopeType, args, 0);
        break;
    case DMU_FRONT_SECTOR:
    {
        sector_t *sec = (lin->L_frontside? lin->L_frontsector : NULL);
        DMU_GetValue(DMT_LINEDEF_SEC, &sec, args, 0);
        break;
    }
    case DMU_BACK_SECTOR:
    {
        sector_t *sec = (lin->L_backside? lin->L_backsector : NULL);
        DMU_GetValue(DMT_LINEDEF_SEC, &sec, args, 0);
        break;
    }
    case DMU_FLAGS:
        DMU_GetValue(DMT_LINEDEF_FLAGS, &lin->flags, args, 0);
        break;
    case DMU_SIDEDEF0:
        DMU_GetValue(DDVT_PTR, &lin->L_frontside, args, 0);
        break;
    case DMU_SIDEDEF1:
        DMU_GetValue(DDVT_PTR, &lin->L_backside, args, 0);
        break;

    case DMU_BOUNDING_BOX:
        if(args->valueType == DDVT_PTR)
        {
            const float* bbox = lin->bBox;
            DMU_GetValue(DDVT_PTR, &bbox, args, 0);
        }
        else
        {
            DMU_GetValue(DMT_LINEDEF_BBOX, &lin->bBox[0], args, 0);
            DMU_GetValue(DMT_LINEDEF_BBOX, &lin->bBox[1], args, 1);
            DMU_GetValue(DMT_LINEDEF_BBOX, &lin->bBox[2], args, 2);
            DMU_GetValue(DMT_LINEDEF_BBOX, &lin->bBox[3], args, 3);
        }
        break;
    case DMU_VALID_COUNT:
        DMU_GetValue(DMT_LINEDEF_VALIDCOUNT, &lin->validCount, args, 0);
        break;
    default:
        Con_Error("Linedef_GetProperty: No property %s.\n",
                  DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}
