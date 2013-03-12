/** @file linedef.h Map LineDef.
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include <cmath>
#include <de/mathutil.h>

#include "de_base.h"
#include "de_render.h"
#include "m_misc.h"
#include "Materials"

#include "map/linedef.h"

using namespace de;

LineDef::LineDef() : MapElement(DMU_LINEDEF)
{
    std::memset(v, 0, sizeof(v));
    std::memset(vo, 0, sizeof(vo));
    std::memset(sides, 0, sizeof(sides));
    flags = 0;
    inFlags = 0;
    slopeType = (slopetype_t) 0;
    validCount = 0;
    angle = 0;
    std::memset(direction, 0, sizeof(direction));
    length = 0;
    std::memset(&aaBox, 0, sizeof(aaBox));
    std::memset(mapped, 0, sizeof(mapped));
    origIndex = 0;
}

LineDef::~LineDef()
{}

#ifdef __CLIENT__
static void calcNormal(LineDef const *l, byte side, pvec2f_t normal)
{
    V2f_Set(normal, (l->L_vorigin(side^1)[VY] - l->L_vorigin(side)  [VY]) / l->length,
                    (l->L_vorigin(side)  [VX] - l->L_vorigin(side^1)[VX]) / l->length);
}

static float calcLightLevelDelta(pvec2f_t const normal)
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
static bool backClosedForBlendNeighbor(LineDef const *lineDef, int side,
    bool ignoreOpacity)
{
    DENG_ASSERT(lineDef);

    if(!lineDef->L_frontsidedef)   return false;
    if(!lineDef->L_backsidedef) return true;

    Sector *frontSec = lineDef->L_sector(side);
    Sector *backSec  = lineDef->L_sector(side^1);
    if(frontSec == backSec) return false; // Never.

    if(frontSec && backSec)
    {
        if(backSec->SP_floorvisheight >= backSec->SP_ceilvisheight)   return true;
        if(backSec->SP_ceilvisheight  <= frontSec->SP_floorvisheight) return true;
        if(backSec->SP_floorvisheight >= frontSec->SP_ceilvisheight)  return true;
    }

    return R_MiddleMaterialCoversLineOpening(lineDef, side, ignoreOpacity);
}

static LineDef *findBlendNeighbor(LineDef const *l, byte side, byte right,
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

coord_t LineDef::pointDistance(coord_t const point[2], coord_t *offset) const
{
    return V2d_PointLineDistance(point, L_v1origin, direction, offset);
}

coord_t LineDef::pointOnSide(coord_t const point[2]) const
{
    DENG2_ASSERT(point);
    return V2d_PointOnLineSide(point, L_v1origin, direction);
}

int LineDef::boxOnSide(AABoxd const *box) const
{
    return M_BoxOnLineSide(box, L_v1origin, direction);
}

int LineDef::boxOnSide_FixedPrecision(AABoxd const *box) const
{
    /*
     * Apply an offset to both the box and the line to bring everything into
     * the 16.16 fixed-point range. We'll use the midpoint of the line as the
     * origin, as typically this test is called when a bounding box is
     * somewhere in the vicinity of the line. The offset is floored to integers
     * so we won't change the discretization of the fractional part into 16-bit
     * precision.
     */
    coord_t offset[2];
    offset[VX] = de::floor(L_v1origin[VX] + direction[VX]/2);
    offset[VY] = de::floor(L_v1origin[VY] + direction[VY]/2);

    fixed_t xbox[4];
    xbox[BOXLEFT]   = FLT2FIX(box->minX - offset[VX]);
    xbox[BOXRIGHT]  = FLT2FIX(box->maxX - offset[VX]);
    xbox[BOXBOTTOM] = FLT2FIX(box->minY - offset[VY]);
    xbox[BOXTOP]    = FLT2FIX(box->maxY - offset[VY]);

    fixed_t pos[2];
    pos[VX]         = FLT2FIX(L_v1origin[VX] - offset[VX]);
    pos[VY]         = FLT2FIX(L_v1origin[VY] - offset[VY]);

    fixed_t delta[2];
    delta[VX]       = FLT2FIX(direction[VX]);
    delta[VY]       = FLT2FIX(direction[VY]);

    return M_BoxOnLineSide_FixedPrecision(xbox, pos, delta);
}

void LineDef::setDivline(divline_t *dl) const
{
    if(!dl) return;

    dl->origin[VX]    = FLT2FIX(L_v1origin[VX]);
    dl->origin[VY]    = FLT2FIX(L_v1origin[VY]);
    dl->direction[VX] = FLT2FIX(direction[VX]);
    dl->direction[VY] = FLT2FIX(direction[VY]);
}

coord_t LineDef::openRange(int side, coord_t *retBottom, coord_t *retTop) const
{
    return R_OpenRange(L_sector(side), L_sector(side^1), retBottom, retTop);
}

coord_t LineDef::visOpenRange(int side, coord_t *retBottom, coord_t *retTop) const
{
    return R_VisOpenRange(L_sector(side), L_sector(side^1), retBottom, retTop);
}

void LineDef::setTraceOpening(TraceOpening *opening) const
{
    if(!opening) return;

    if(!L_backsidedef)
    {
        opening->range = 0;
        return;
    }

    coord_t bottom, top;
    opening->range  = float( openRange(FRONT, &bottom, &top) );
    opening->bottom = float( bottom );
    opening->top    = float( top );

    // Determine the "low floor".
    Sector *front = L_frontsector;
    Sector *back  = L_backsector;

    if(front->SP_floorheight > back->SP_floorheight)
    {
        opening->lowFloor = float( back->SP_floorheight );
    }
    else
    {
        opening->lowFloor = float( front->SP_floorheight );
    }
}

void LineDef::updateSlope()
{
    V2d_Subtract(direction, L_v2origin, L_v1origin);
    slopeType = M_SlopeType(direction);
}

void LineDef::unitVector(float *unitvec) const
{
    coord_t len = M_ApproxDistance(direction[VX], direction[VY]);
    if(len)
    {
        unitvec[VX] = direction[VX] / len;
        unitvec[VY] = direction[VY] / len;
    }
    else
    {
        unitvec[VX] = unitvec[VY] = 0;
    }
}

void LineDef::updateAABox()
{
    aaBox.minX = de::min(L_v2origin[VX], L_v1origin[VX]);
    aaBox.minY = de::min(L_v2origin[VY], L_v1origin[VY]);

    aaBox.maxX = de::max(L_v2origin[VX], L_v1origin[VX]);
    aaBox.maxY = de::max(L_v2origin[VY], L_v1origin[VY]);
}

void LineDef::lightLevelDelta(int side, float *deltaL, float *deltaR) const
{
#ifdef __CLIENT__
    // Disabled?
    if(!(rendLightWallAngle > 0))
    {
        *deltaL = *deltaR = 0;
        return;
    }

    vec2f_t normal;
    calcNormal(this, side, normal);
    float delta = calcLightLevelDelta(normal);

    // If smoothing is disabled use this delta for left and right edges.
    // Must forcibly disable smoothing for polyobj linedefs as they have
    // no owner rings.
    if(!rendLightWallAngleSmooth || (inFlags & LF_POLYOBJ))
    {
        *deltaL = *deltaR = delta;
        return;
    }

    // Find the left neighbour linedef for which we will calculate the
    // lightlevel delta and then blend with this to produce the value for
    // the left edge. Blend iff the angle between the two linedefs is less
    // than 45 degrees.
    binangle_t diff = 0;
    LineDef *other = findBlendNeighbor(this, side, 0, &diff);
    if(other && INRANGE_OF(diff, BANG_180, BANG_45))
    {
        vec2f_t otherNormal;
        calcNormal(other, other->L_v2 != L_v(side), otherNormal);

        // Average normals.
        V2f_Sum(otherNormal, otherNormal, normal);
        otherNormal[VX] /= 2; otherNormal[VY] /= 2;

        *deltaL = calcLightLevelDelta(otherNormal);
    }
    else
    {
        *deltaL = delta;
    }

    // Do the same for the right edge but with the right neighbour linedef.
    diff = 0;
    other = findBlendNeighbor(this, side, 1, &diff);
    if(other && INRANGE_OF(diff, BANG_180, BANG_45))
    {
        vec2f_t otherNormal;
        calcNormal(other, other->L_v1 != L_v(side^1), otherNormal);

        // Average normals.
        V2f_Sum(otherNormal, otherNormal, normal);
        otherNormal[VX] /= 2; otherNormal[VY] /= 2;

        *deltaR = calcLightLevelDelta(otherNormal);
    }
    else
    {
        *deltaR = delta;
    }
#else // !__CLIENT__
    DENG2_UNUSED(side);

    if(deltaL) *deltaL = 0;
    if(deltaR) *deltaR = 0;
#endif
}

int LineDef::property(setargs_t &args) const
{
    switch(args.prop)
    {
    case DMU_VERTEX0:
        DMU_GetValue(DMT_LINEDEF_V, &L_v1, &args, 0);
        break;
    case DMU_VERTEX1:
        DMU_GetValue(DMT_LINEDEF_V, &L_v2, &args, 0);
        break;
    case DMU_DX:
        DMU_GetValue(DMT_LINEDEF_DX, &direction[VX], &args, 0);
        break;
    case DMU_DY:
        DMU_GetValue(DMT_LINEDEF_DY, &direction[VY], &args, 0);
        break;
    case DMU_DXY:
        DMU_GetValue(DMT_LINEDEF_DX, &direction[VX], &args, 0);
        DMU_GetValue(DMT_LINEDEF_DY, &direction[VY], &args, 1);
        break;
    case DMU_LENGTH:
        DMU_GetValue(DMT_LINEDEF_LENGTH, &length, &args, 0);
        break;
    case DMU_ANGLE: {
        angle_t lineAngle = BANG_TO_ANGLE(angle);
        DMU_GetValue(DDVT_ANGLE, &lineAngle, &args, 0);
        break; }
    case DMU_SLOPETYPE:
        DMU_GetValue(DMT_LINEDEF_SLOPETYPE, &slopeType, &args, 0);
        break;
    case DMU_FRONT_SECTOR: {
        Sector *sec = (L_frontsidedef? L_frontsector : NULL);
        DMU_GetValue(DMT_LINEDEF_SECTOR, &sec, &args, 0);
        break; }
    case DMU_BACK_SECTOR: {
        Sector *sec = (L_backsidedef? L_backsector : NULL);
        DMU_GetValue(DMT_LINEDEF_SECTOR, &sec, &args, 0);
        break; }
    case DMU_FLAGS:
        DMU_GetValue(DMT_LINEDEF_FLAGS, &flags, &args, 0);
        break;
    case DMU_SIDEDEF0:
        DMU_GetValue(DDVT_PTR, &L_frontsidedef, &args, 0);
        break;
    case DMU_SIDEDEF1:
        DMU_GetValue(DDVT_PTR, &L_backsidedef, &args, 0);
        break;
    case DMU_BOUNDING_BOX:
        if(args.valueType == DDVT_PTR)
        {
            AABoxd const *aaBoxAdr = &aaBox;
            DMU_GetValue(DDVT_PTR, &aaBoxAdr, &args, 0);
        }
        else
        {
            DMU_GetValue(DMT_LINEDEF_AABOX, &aaBox.minX, &args, 0);
            DMU_GetValue(DMT_LINEDEF_AABOX, &aaBox.maxX, &args, 1);
            DMU_GetValue(DMT_LINEDEF_AABOX, &aaBox.minY, &args, 2);
            DMU_GetValue(DMT_LINEDEF_AABOX, &aaBox.maxY, &args, 3);
        }
        break;
    case DMU_VALID_COUNT:
        DMU_GetValue(DMT_LINEDEF_VALIDCOUNT, &validCount, &args, 0);
        break;
    default:
        /// @throw UnknownPropertyError  The requested property does not exist.
        throw UnknownPropertyError("LineDef::property", QString("Property '%1' is unknown").arg(DMU_Str(args.prop)));
    }

    return false; // Continue iteration.
}

int LineDef::setProperty(setargs_t const &args)
{
    switch(args.prop)
    {
    case DMU_FRONT_SECTOR:
        DMU_SetValue(DMT_LINEDEF_SECTOR, &L_frontsector, &args, 0);
        break;
    case DMU_BACK_SECTOR:
        DMU_SetValue(DMT_LINEDEF_SECTOR, &L_backsector, &args, 0);
        break;
    case DMU_SIDEDEF0:
        DMU_SetValue(DMT_LINEDEF_SIDEDEF, &L_frontsidedef, &args, 0);
        break;
    case DMU_SIDEDEF1:
        DMU_SetValue(DMT_LINEDEF_SIDEDEF, &L_backsidedef, &args, 0);
        break;
    case DMU_VALID_COUNT:
        DMU_SetValue(DMT_LINEDEF_VALIDCOUNT, &validCount, &args, 0);
        break;
    case DMU_FLAGS: {
        DMU_SetValue(DMT_LINEDEF_FLAGS, &flags, &args, 0);

        SideDef *s = L_frontsidedef;
        s->SW_topsurface.update();
        s->SW_bottomsurface.update();
        s->SW_middlesurface.update();
        if(L_backsidedef)
        {
            s = L_backsidedef;
            s->SW_topsurface.update();
            s->SW_bottomsurface.update();
            s->SW_middlesurface.update();
        }
        break; }
    default:
        /// @throw WritePropertyError  The requested property is not writable.
        throw WritePropertyError("LineDef::setProperty", QString("Property '%1' is not writable").arg(DMU_Str(args.prop)));
    }

    return false; // Continue iteration.
}
