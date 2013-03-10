/** @file linedef.h Map LineDef implementation. 
 * @ingroup map
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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
#include "de_render.h"

#include "m_misc.h"
#include "Materials"
#include "map/linedef.h"

#include <math.h>
#include <de/mathutil.h>
#include <de/binangle.h>

#ifdef __CLIENT__
static void calcNormal(const LineDef* l, byte side, pvec2f_t normal)
{
    V2f_Set(normal, (l->L_vorigin(side^1)[VY] - l->L_vorigin(side)  [VY]) / l->length,
                    (l->L_vorigin(side)  [VX] - l->L_vorigin(side^1)[VX]) / l->length);
}

static float lightLevelDelta(const pvec2f_t normal)
{
    return (1.0f / 255) * (normal[VX] * 18) * rendLightWallAngle;
}

/**
 * @param lineDef  LineDef instance.
 * @param ignoreOpacity  @c true= do not consider Material opacity.
 * @return  @c true if this LineDef's side is considered "closed" (i.e.,
 *     there is no opening through which the back Sector can be seen).
 *     Tests consider all Planes which interface with this and the "middle"
 *     Material used on the relative front side (if any).
 */
static boolean backClosedForBlendNeighbor(LineDef* lineDef, int side, boolean ignoreOpacity)
{
    Sector* frontSec;
    Sector* backSec;
    DENG_ASSERT(lineDef);

    if(!lineDef->L_frontsidedef)   return false;
    if(!lineDef->L_backsidedef) return true;

    frontSec = lineDef->L_sector(side);
    backSec  = lineDef->L_sector(side^1);
    if(frontSec == backSec) return false; // Never.

    if(frontSec && backSec)
    {
        if(backSec->SP_floorvisheight >= backSec->SP_ceilvisheight)   return true;
        if(backSec->SP_ceilvisheight  <= frontSec->SP_floorvisheight) return true;
        if(backSec->SP_floorvisheight >= frontSec->SP_ceilvisheight)  return true;
    }

    return R_MiddleMaterialCoversLineOpening(lineDef, side, ignoreOpacity);
}

static LineDef *findBlendNeighbor(LineDef *l, byte side, byte right,
    binangle_t *diff)
{
    LineOwner const *farVertOwner = l->L_vo(right^side);
    if(backClosedForBlendNeighbor(l, side, true/*ignore opacity*/))
    {
        return R_FindSolidLineNeighbor(l->L_sector(side), l, farVertOwner, right, diff);
    }
    return R_FindLineNeighbor(l->L_sector(side), l, farVertOwner, right, diff);
}
#endif // __CLIENT__

#undef LineDef_PointDistance
DENG_EXTERN_C coord_t LineDef_PointDistance(LineDef* line, coord_t const point[2], coord_t* offset)
{
    DENG_ASSERT(line);
    return V2d_PointLineDistance(point, line->L_v1origin, line->direction, offset);
}

#undef LineDef_PointXYDistance
DENG_EXTERN_C coord_t LineDef_PointXYDistance(LineDef* line, coord_t x, coord_t y, coord_t* offset)
{
    coord_t point[2] = { x, y };
    return LineDef_PointDistance(line, point, offset);
}

#undef LineDef_PointOnSide
DENG_EXTERN_C coord_t LineDef_PointOnSide(const LineDef* line, coord_t const point[2])
{
    DENG_ASSERT(line);
    if(!point)
    {
        DEBUG_Message(("LineDef_PointOnSide: Invalid arguments, returning >0.\n"));
        return 1;
    }
    return V2d_PointOnLineSide(point, line->L_v1origin, line->direction);
}

#undef LineDef_PointXYOnSide
DENG_EXTERN_C coord_t LineDef_PointXYOnSide(const LineDef* line, coord_t x, coord_t y)
{
    coord_t point[2] = { x, y };
    return LineDef_PointOnSide(line, point);
}

#undef LineDef_BoxOnSide
DENG_EXTERN_C int LineDef_BoxOnSide(LineDef* line, const AABoxd* box)
{
    DENG_ASSERT(line);
    return M_BoxOnLineSide(box, line->L_v1origin, line->direction);
}

#undef LineDef_BoxOnSide_FixedPrecision
DENG_EXTERN_C int LineDef_BoxOnSide_FixedPrecision(LineDef* line, const AABoxd* box)
{
    fixed_t xbox[4];
    fixed_t pos[2];
    fixed_t delta[2];
    double offset[2];

    DENG_ASSERT(line);

    /*
     * Apply an offset to both the box and the line to bring everything into
     * the 16.16 fixed-point range. We'll use the midpoint of the line as the
     * origin, as typically this test is called when a bounding box is
     * somewhere in the vicinity of the line. The offset is floored to integers
     * so we won't change the discretization of the fractional part into 16-bit
     * precision.
     */
    offset[VX] = floor(line->L_v1origin[VX] + line->direction[VX]/2);
    offset[VY] = floor(line->L_v1origin[VY] + line->direction[VY]/2);

    xbox[BOXLEFT]   = FLT2FIX(box->minX - offset[VX]);
    xbox[BOXRIGHT]  = FLT2FIX(box->maxX - offset[VX]);
    xbox[BOXBOTTOM] = FLT2FIX(box->minY - offset[VY]);
    xbox[BOXTOP]    = FLT2FIX(box->maxY - offset[VY]);

    pos[VX]         = FLT2FIX(line->L_v1origin[VX] - offset[VX]);
    pos[VY]         = FLT2FIX(line->L_v1origin[VY] - offset[VY]);

    delta[VX]       = FLT2FIX(line->direction[VX]);
    delta[VY]       = FLT2FIX(line->direction[VY]);

    return M_BoxOnLineSide_FixedPrecision(xbox, pos, delta);
}

void LineDef_SetDivline(const LineDef* line, divline_t* dl)
{
    DENG_ASSERT(line);

    if(!dl) return;

    dl->origin[VX] = FLT2FIX(line->L_v1origin[VX]);
    dl->origin[VY] = FLT2FIX(line->L_v1origin[VY]);
    dl->direction[VX] = FLT2FIX(line->direction[VX]);
    dl->direction[VY] = FLT2FIX(line->direction[VY]);
}

coord_t LineDef_OpenRange(const LineDef* line, int side, coord_t* retBottom, coord_t* retTop)
{
    DENG_ASSERT(line);
    return R_OpenRange(line->L_sector(side), line->L_sector(side^1), retBottom, retTop);
}

coord_t LineDef_VisOpenRange(const LineDef* line, int side, coord_t* retBottom, coord_t* retTop)
{
    DENG_ASSERT(line);
    return R_VisOpenRange(line->L_sector(side), line->L_sector(side^1), retBottom, retTop);
}

void LineDef_SetTraceOpening(const LineDef* line, TraceOpening* opening)
{
    DENG_ASSERT(line);
{
    Sector* front, *back;
    coord_t bottom, top;

    if(!opening) return;

    if(!line->L_backsidedef)
    {
        opening->range = 0;
        return;
    }

    opening->range  = (float)LineDef_OpenRange(line, FRONT, &bottom, &top);
    opening->bottom = (float)bottom;
    opening->top    = (float)top;

    // Determine the "low floor".
    front = line->L_frontsector;
    back  = line->L_backsector;

    if(front->SP_floorheight > back->SP_floorheight)
    {
        opening->lowFloor = (float)back->SP_floorheight;
    }
    else
    {
        opening->lowFloor = (float)front->SP_floorheight;
    }
}}

void LineDef_UpdateSlope(LineDef* line)
{
    DENG_ASSERT(line);
    V2d_Subtract(line->direction, line->L_v2origin, line->L_v1origin);
    line->slopeType = M_SlopeType(line->direction);
}

/**
 * Returns a two-component float unit vector parallel to the line.
 */
void LineDef_UnitVector(LineDef* line, float* unitvec)
{
    coord_t len;
    DENG_ASSERT(line);

    len = M_ApproxDistance(line->direction[VX], line->direction[VY]);
    if(len)
    {
        unitvec[VX] = line->direction[VX] / len;
        unitvec[VY] = line->direction[VY] / len;
    }
    else
    {
        unitvec[VX] = unitvec[VY] = 0;
    }
}

void LineDef_UpdateAABox(LineDef* line)
{
    DENG_ASSERT(line);

    line->aaBox.minX = MIN_OF(line->L_v2origin[VX], line->L_v1origin[VX]);
    line->aaBox.minY = MIN_OF(line->L_v2origin[VY], line->L_v1origin[VY]);

    line->aaBox.maxX = MAX_OF(line->L_v2origin[VX], line->L_v1origin[VX]);
    line->aaBox.maxY = MAX_OF(line->L_v2origin[VY], line->L_v1origin[VY]);
}

void LineDef_LightLevelDelta(LineDef *l, int side, float* deltaL, float* deltaR)
{
#ifdef __CLIENT__
    // Disabled?
    if(!(rendLightWallAngle > 0))
    {
        *deltaL = *deltaR = 0;
        return;
    }

    vec2f_t normal;
    calcNormal(l, side, normal);
    float delta = lightLevelDelta(normal);

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
    binangle_t diff = 0;
    LineDef *other = findBlendNeighbor(l, side, 0, &diff);
    if(other && INRANGE_OF(diff, BANG_180, BANG_45))
    {
        vec2f_t otherNormal;
        calcNormal(other, other->L_v2 != l->L_v(side), otherNormal);

        // Average normals.
        V2f_Sum(otherNormal, otherNormal, normal);
        otherNormal[VX] /= 2; otherNormal[VY] /= 2;

        *deltaL = lightLevelDelta(otherNormal);
    }
    else
    {
        *deltaL = delta;
    }

    // Do the same for the right edge but with the right neighbour linedef.
    diff = 0;
    other = findBlendNeighbor(l, side, 1, &diff);
    if(other && INRANGE_OF(diff, BANG_180, BANG_45))
    {
        vec2f_t otherNormal;
        calcNormal(other, other->L_v1 != l->L_v(side^1), otherNormal);

        // Average normals.
        V2f_Sum(otherNormal, otherNormal, normal);
        otherNormal[VX] /= 2; otherNormal[VY] /= 2;

        *deltaR = lightLevelDelta(otherNormal);
    }
    else
    {
        *deltaR = delta;
    }
#else // !__CLIENT__
    DENG2_UNUSED2(l, side);

    if(deltaL) *deltaL = 0;
    if(deltaR) *deltaR = 0;
#endif
}

int LineDef_SetProperty(LineDef* lin, const setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_FRONT_SECTOR:
        DMU_SetValue(DMT_LINEDEF_SECTOR, &lin->L_frontsector, args, 0);
        break;
    case DMU_BACK_SECTOR:
        DMU_SetValue(DMT_LINEDEF_SECTOR, &lin->L_backsector, args, 0);
        break;
    case DMU_SIDEDEF0:
        DMU_SetValue(DMT_LINEDEF_SIDEDEF, &lin->L_frontsidedef, args, 0);
        break;
    case DMU_SIDEDEF1:
        DMU_SetValue(DMT_LINEDEF_SIDEDEF, &lin->L_backsidedef, args, 0);
        break;
    case DMU_VALID_COUNT:
        DMU_SetValue(DMT_LINEDEF_VALIDCOUNT, &lin->validCount, args, 0);
        break;
    case DMU_FLAGS: {
        DMU_SetValue(DMT_LINEDEF_FLAGS, &lin->flags, args, 0);

        SideDef *s = lin->L_frontsidedef;
        s->SW_topsurface.update();
        s->SW_bottomsurface.update();
        s->SW_middlesurface.update();
        if(lin->L_backsidedef)
        {
            s = lin->L_backsidedef;
            s->SW_topsurface.update();
            s->SW_bottomsurface.update();
            s->SW_middlesurface.update();
        }
        break; }
    default:
        Con_Error("LineDef_SetProperty: Property %s is not writable.\n", DMU_Str(args->prop));
    }

    return false; // Continue iteration.
}

int LineDef_GetProperty(const LineDef* lin, setargs_t* args)
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
        DMU_GetValue(DMT_LINEDEF_DX, &lin->direction[VX], args, 0);
        break;
    case DMU_DY:
        DMU_GetValue(DMT_LINEDEF_DY, &lin->direction[VY], args, 0);
        break;
    case DMU_DXY:
        DMU_GetValue(DMT_LINEDEF_DX, &lin->direction[VX], args, 0);
        DMU_GetValue(DMT_LINEDEF_DY, &lin->direction[VY], args, 1);
        break;
    case DMU_LENGTH:
        DMU_GetValue(DMT_LINEDEF_LENGTH, &lin->length, args, 0);
        break;
    case DMU_ANGLE: {
        angle_t lineAngle = BANG_TO_ANGLE(lin->angle);
        DMU_GetValue(DDVT_ANGLE, &lineAngle, args, 0);
        break;
      }
    case DMU_SLOPETYPE:
        DMU_GetValue(DMT_LINEDEF_SLOPETYPE, &lin->slopeType, args, 0);
        break;
    case DMU_FRONT_SECTOR: {
        Sector* sec = (lin->L_frontsidedef? lin->L_frontsector : NULL);
        DMU_GetValue(DMT_LINEDEF_SECTOR, &sec, args, 0);
        break;
      }
    case DMU_BACK_SECTOR: {
        Sector* sec = (lin->L_backsidedef? lin->L_backsector : NULL);
        DMU_GetValue(DMT_LINEDEF_SECTOR, &sec, args, 0);
        break;
      }
    case DMU_FLAGS:
        DMU_GetValue(DMT_LINEDEF_FLAGS, &lin->flags, args, 0);
        break;
    case DMU_SIDEDEF0:
        DMU_GetValue(DDVT_PTR, &lin->L_frontsidedef, args, 0);
        break;
    case DMU_SIDEDEF1:
        DMU_GetValue(DDVT_PTR, &lin->L_backsidedef, args, 0);
        break;
    case DMU_BOUNDING_BOX:
        if(args->valueType == DDVT_PTR)
        {
            const AABoxd* aaBox = &lin->aaBox;
            DMU_GetValue(DDVT_PTR, &aaBox, args, 0);
        }
        else
        {
            DMU_GetValue(DMT_LINEDEF_AABOX, &lin->aaBox.minX, args, 0);
            DMU_GetValue(DMT_LINEDEF_AABOX, &lin->aaBox.maxX, args, 1);
            DMU_GetValue(DMT_LINEDEF_AABOX, &lin->aaBox.minY, args, 2);
            DMU_GetValue(DMT_LINEDEF_AABOX, &lin->aaBox.maxY, args, 3);
        }
        break;
    case DMU_VALID_COUNT:
        DMU_GetValue(DMT_LINEDEF_VALIDCOUNT, &lin->validCount, args, 0);
        break;
    default:
        Con_Error("LineDef_GetProperty: No property %s.\n", DMU_Str(args->prop));
    }

    return false; // Continue iteration.
}
