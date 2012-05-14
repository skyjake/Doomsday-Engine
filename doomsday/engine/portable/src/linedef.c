/**
 * @file linedef.h
 * Map LineDef implementation. @ingroup map
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

#include "m_bams.h"
#include "m_misc.h"
#include "materialvariant.h"
#include "materials.h"

#include "linedef.h"

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
static boolean backClosedForBlendNeighbor(const LineDef* lineDef, int side, boolean ignoreOpacity)
{
    Sector* frontSec;
    Sector* backSec;
    assert(lineDef);

    if(!lineDef->L_side(side))   return false;
    if(!lineDef->L_side(side^1)) return true;

    frontSec = lineDef->L_sector(side);
    backSec  = lineDef->L_sector(side^1);
    if(frontSec == backSec) return false; // Never.

    if(frontSec && backSec)
    {
        if(backSec->SP_floorvisheight >= backSec->SP_ceilvisheight)   return true;
        if(backSec->SP_ceilvisheight  <= frontSec->SP_floorvisheight) return true;
        if(backSec->SP_floorvisheight >= frontSec->SP_ceilvisheight)  return true;
    }

    return LineDef_MiddleMaterialCoversOpening(lineDef, side, ignoreOpacity);
}

static LineDef* findBlendNeighbor(const LineDef* l, byte side, byte right,
    binangle_t* diff)
{
    const lineowner_t* farVertOwner = l->L_vo(right^side);
    if(backClosedForBlendNeighbor(l, side, true/*ignore opacity*/))
    {
        return R_FindSolidLineNeighbor(l->L_sector(side), l, farVertOwner, right, diff);
    }
    return R_FindLineNeighbor(l->L_sector(side), l, farVertOwner, right, diff);
}

coord_t LineDef_PointDistance(LineDef* line, coord_t const point[2], coord_t* offset)
{
    assert(line);
    return V2d_PointLineDistance(point, line->L_v1origin, line->direction, offset);
}

coord_t LineDef_PointXYDistance(LineDef* line, coord_t x, coord_t y, coord_t* offset)
{
    coord_t point[2] = { x, y };
    return LineDef_PointDistance(line, point, offset);
}

coord_t LineDef_PointOnSide(const LineDef* line, coord_t const point[2])
{
    assert(line);
    if(!point)
    {
        DEBUG_Message(("LineDef_PointOnSide: Invalid arguments, returning >0.\n"));
        return 1;
    }
    return V2d_PointOnLineSide(point, line->L_v1origin, line->direction);
}

coord_t LineDef_PointXYOnSide(const LineDef* line, coord_t x, coord_t y)
{
    coord_t point[2] = { x, y };
    return LineDef_PointOnSide(line, point);
}

int LineDef_BoxOnSide(LineDef* line, const AABoxd* box)
{
    assert(line);
    return M_BoxOnLineSide(box, line->L_v1origin, line->direction);
}

void LineDef_SetDivline(const LineDef* line, divline_t* dl)
{
    assert(line);

    if(!dl) return;

    dl->origin[VX] = FLT2FIX(line->L_v1origin[VX]);
    dl->origin[VY] = FLT2FIX(line->L_v1origin[VY]);
    dl->direction[VX] = FLT2FIX(line->direction[VX]);
    dl->direction[VY] = FLT2FIX(line->direction[VY]);
}

void LineDef_SetTraceOpening(const LineDef* line, TraceOpening* opening)
{
    Sector* front, *back;
    assert(line);

    if(!opening) return;

    if(!line->L_backside)
    {
        opening->range = 0;
        return;
    }

    front = line->L_frontsector;
    back  = line->L_backsector;

    if(front->SP_ceilheight < back->SP_ceilheight)
        opening->top = front->SP_ceilheight;
    else
        opening->top = back->SP_ceilheight;

    if(front->SP_floorheight > back->SP_floorheight)
    {
        opening->bottom   = front->SP_floorheight;
        opening->lowFloor = back->SP_floorheight;
    }
    else
    {
        opening->bottom   = back->SP_floorheight;
        opening->lowFloor = front->SP_floorheight;
    }

    opening->range = opening->top - opening->bottom;
}

void LineDef_UpdateSlope(LineDef* line)
{
    assert(line);
    V2d_Subtract(line->direction, line->L_v2origin, line->L_v1origin);
    line->slopeType = M_SlopeType(line->direction);
}

/**
 * Returns a two-component float unit vector parallel to the line.
 */
void LineDef_UnitVector(LineDef* line, float* unitvec)
{
    coord_t len;
    assert(line);

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
    assert(line);

    line->aaBox.minX = MIN_OF(line->L_v2origin[VX], line->L_v1origin[VX]);
    line->aaBox.minY = MIN_OF(line->L_v2origin[VY], line->L_v1origin[VY]);

    line->aaBox.maxX = MAX_OF(line->L_v2origin[VX], line->L_v1origin[VX]);
    line->aaBox.maxY = MAX_OF(line->L_v2origin[VY], line->L_v1origin[VY]);
}

/**
 * @todo Now that we store surface tangent space normals use those rather than angles.
 */
void LineDef_LightLevelDelta(const LineDef* l, int side, float* deltaL, float* deltaR)
{
    binangle_t diff;
    LineDef* other;
    vec2f_t normal;
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
    diff = 0;
    other = findBlendNeighbor(l, side, 0, &diff);
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
}

int LineDef_MiddleMaterialCoords(const LineDef* lineDef, int side,
    coord_t* bottomLeft, coord_t* bottomRight, coord_t* topLeft, coord_t* topRight,
    float* texOffY, boolean lowerUnpeg, boolean clipTop, boolean clipBottom)
{
    coord_t* top[2], *bottom[2], openingTop[2], openingBottom[2]; // {left, right}
    coord_t tcYOff;
    SideDef* sideDef;
    int i, texHeight;
    assert(lineDef && bottomLeft && bottomRight && topLeft && topRight);

    if(texOffY)  *texOffY  = 0;

    sideDef = lineDef->L_side(side);
    if(!sideDef || !sideDef->SW_middlematerial) return false;

    texHeight = Material_Height(sideDef->SW_middlematerial);
    tcYOff = sideDef->SW_middlevisoffset[VY];

    top[0] = topLeft;
    top[1] = topRight;
    bottom[0] = bottomLeft;
    bottom[1] = bottomRight;

    openingTop[0] = *top[0];
    openingTop[1] = *top[1];
    openingBottom[0] = *bottom[0];
    openingBottom[1] = *bottom[1];

    if(openingTop[0] <= openingBottom[0] &&
       openingTop[1] <= openingBottom[1]) return false;

    // For each edge (left then right).
    for(i = 0; i < 2; ++i)
    {
        if(lowerUnpeg)
        {
            *bottom[i] += tcYOff;
            *top[i] = *bottom[i] + texHeight;
        }
        else
        {
            *top[i] += tcYOff;
            *bottom[i] = *top[i] - texHeight;
        }
    }

    if(texOffY && (*top[0] > openingTop[0] || *top[1] > openingTop[1]))
    {
        if(*top[1] > *top[0])
            *texOffY += *top[1] - openingTop[1];
        else
            *texOffY += *top[0] - openingTop[0];
    }

    // Clip it.
    if(clipTop || clipBottom)
    {
        // For each edge (left then right).
        for(i = 0; i < 2; ++i)
        {
            if(clipBottom && *bottom[i] < openingBottom[i])
                *bottom[i] = openingBottom[i];

            if(clipTop && *top[i] > openingTop[i])
                *top[i] = openingTop[i];
        }
    }

    return true;
}

/**
 * @fixme No need to do this each frame. Set a flag in SideDef->flags to
 * denote this. Is sensitive to plane heights, surface properties
 * (e.g. alpha) and surface texture properties.
 */
boolean LineDef_MiddleMaterialCoversOpening(const LineDef *line, int side,
    boolean ignoreOpacity)
{
    assert(line);
    if(line->L_backside)
    {
        SideDef* sideDef = line->L_side(side);
        Sector* frontSec = line->L_sector(side);
        Sector*  backSec = line->L_sector(side^1);

        if(sideDef->SW_middlematerial)
        {
            // Ensure we have up to date info.
            const materialvariantspecification_t* spec = Materials_VariantSpecificationForContext(
                MC_MAPSURFACE, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT, -1, -1, -1, true, true, false, false);
            material_t* mat = sideDef->SW_middlematerial;
            const materialsnapshot_t* ms = Materials_Prepare(mat, spec, true);

            if(ignoreOpacity || (ms->isOpaque && !sideDef->SW_middleblendmode && sideDef->SW_middlergba[3] >= 1))
            {
                coord_t openTop[2], matTop[2];
                coord_t openBottom[2], matBottom[2];

                if(sideDef->flags & SDF_MIDDLE_STRETCH)
                    return true;

                openTop[0] = openTop[1] =
                    matTop[0] = matTop[1] = LineDef_CeilingMin(line)->visHeight;
                openBottom[0] = openBottom[1] =
                    matBottom[0] = matBottom[1] = LineDef_FloorMax(line)->visHeight;

                // Could the mid texture fill enough of this gap for us
                // to consider it completely closed?
                if(ms->size.height >= (openTop[0] - openBottom[0]) &&
                   ms->size.height >= (openTop[1] - openBottom[1]))
                {
                    // Possibly. Check the placement of the mid texture.
                    if(LineDef_MiddleMaterialCoords(line, side, &matBottom[0], &matBottom[1],
                        &matTop[0], &matTop[1], NULL, 0 != (line->flags & DDLF_DONTPEGBOTTOM),
                        !(Surface_IsSkyMasked(&frontSec->SP_ceilsurface) &&
                          Surface_IsSkyMasked(&backSec->SP_ceilsurface)),
                        !(Surface_IsSkyMasked(&frontSec->SP_floorsurface) &&
                          Surface_IsSkyMasked(&backSec->SP_floorsurface))))
                    {
                        if(matTop[0] >= openTop[0] &&
                           matTop[1] >= openTop[1] &&
                           matBottom[0] <= openBottom[0] &&
                           matBottom[1] <= openBottom[1])
                            return true;
                    }
                }
            }
        }
    }

    return false;
}

Plane* LineDef_FloorMin(const LineDef* lineDef)
{
    assert(lineDef);
    if(!lineDef->L_frontsector) return NULL; // No interfaces.
    if(!lineDef->L_backside || lineDef->L_backsector == lineDef->L_frontsector)
        return lineDef->L_frontsector->SP_plane(PLN_FLOOR);
    return lineDef->L_backsector->SP_floorvisheight < lineDef->L_frontsector->SP_floorvisheight?
               lineDef->L_backsector->SP_plane(PLN_FLOOR) : lineDef->L_frontsector->SP_plane(PLN_FLOOR);
}

Plane* LineDef_FloorMax(const LineDef* lineDef)
{
    assert(lineDef);
    if(!lineDef->L_frontsector) return NULL; // No interfaces.
    if(!lineDef->L_backside || lineDef->L_backsector == lineDef->L_frontsector)
        return lineDef->L_frontsector->SP_plane(PLN_FLOOR);
    return lineDef->L_backsector->SP_floorvisheight > lineDef->L_frontsector->SP_floorvisheight?
               lineDef->L_backsector->SP_plane(PLN_FLOOR) : lineDef->L_frontsector->SP_plane(PLN_FLOOR);
}

Plane* LineDef_CeilingMin(const LineDef* lineDef)
{
    assert(lineDef);
    if(!lineDef->L_frontsector) return NULL; // No interfaces.
    if(!lineDef->L_backside || lineDef->L_backsector == lineDef->L_frontsector)
        return lineDef->L_frontsector->SP_plane(PLN_CEILING);
    return lineDef->L_backsector->SP_ceilvisheight < lineDef->L_frontsector->SP_ceilvisheight?
               lineDef->L_backsector->SP_plane(PLN_CEILING) : lineDef->L_frontsector->SP_plane(PLN_CEILING);
}

Plane* LineDef_CeilingMax(const LineDef* lineDef)
{
    assert(lineDef);
    if(!lineDef->L_frontsector) return NULL; // No interfaces.
    if(!lineDef->L_backside || lineDef->L_backsector == lineDef->L_frontsector)
        return lineDef->L_frontsector->SP_plane(PLN_CEILING);
    return lineDef->L_backsector->SP_ceilvisheight > lineDef->L_frontsector->SP_ceilvisheight?
               lineDef->L_backsector->SP_plane(PLN_CEILING) : lineDef->L_frontsector->SP_plane(PLN_CEILING);
}

int LineDef_SetProperty(LineDef* lin, const setargs_t* args)
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
    case DMU_FLAGS: {
        SideDef* s;

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
        DMU_GetValue(DDVT_FLOAT, &lin->length, args, 0);
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
        Sector* sec = (lin->L_frontside? lin->L_frontsector : NULL);
        DMU_GetValue(DMT_LINEDEF_SEC, &sec, args, 0);
        break;
      }
    case DMU_BACK_SECTOR: {
        Sector* sec = (lin->L_backside? lin->L_backsector : NULL);
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
