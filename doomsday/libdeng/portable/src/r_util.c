/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * r_util.c: Refresh Utility Routines
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_refresh.h"

#include "p_dmu.h"

// MACROS ------------------------------------------------------------------

#define SLOPERANGE      2048
#define SLOPEBITS       11
#define DBITS           (FRACBITS-SLOPEBITS)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int tantoangle[SLOPERANGE + 1];  // get from tables.c

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Which side of the partition does the point lie?
 *
 * @param x             X coordinate to test.
 * @param y             Y coordinate to test.
 * @return int          @c 0 = front, else @c 1 = back.
 */
int R_PointOnSide(const float x, const float y, const partition_t *par)
{
    float               dx, dy;
    float               left, right;

    if(!par->dX)
    {
        if(x <= par->x)
            return (par->dY > 0? 1:0);
        else
            return (par->dY < 0? 1:0);
    }
    if(!par->dY)
    {
        if(y <= par->y)
            return (par->dX < 0? 1:0);
        else
            return (par->dX > 0? 1:0);
    }

    dx = (x - par->x);
    dy = (y - par->y);

    // Try to quickly decide by looking at the signs.
    if(par->dX < 0)
    {
        if(par->dY < 0)
        {
            if(dx < 0)
            {
                if(dy >= 0)
                    return 0;
            }
            else if(dy < 0)
                return 1;
        }
        else
        {
            if(dx < 0)
            {
                if(dy < 0)
                    return 1;
            }
            else if(dy >= 0)
                return 0;
        }
    }
    else
    {
        if(par->dY < 0)
        {
            if(dx < 0)
            {
                if(dy < 0)
                    return 0;
            }
            else if(dy >= 0)
                return 1;
        }
        else
        {
            if(dx < 0)
            {
                if(dy >= 0)
                    return 1;
            }
            else if(dy < 0)
                return 0;
        }
    }

    left = par->dY * dx;
    right = dy * par->dX;

    if(right < left)
        return 0; // front side
    else
        return 1; // back side
}

int R_SlopeDiv(unsigned num, unsigned den)
{
    unsigned int        ans;

    if(den < 512)
        return SLOPERANGE;
    ans = (num << 3) / (den >> 8);
    return ans <= SLOPERANGE ? ans : SLOPERANGE;
}

/**
 * To get a global angle from cartesian coordinates, the coordinates are
 * flipped until they are in the first octant of the coordinate system, then
 * the y (<=x) is scaled and divided by x to get a tangent (slope) value
 * which is looked up in the tantoangle[] table.  The +1 size is to handle
 * the case when x==y without additional checking.
 *
 * @param   x           X coordinate to test.
 * @param   y           Y coordinate to test.
 *
 * @return  angle_t     Angle between the test point and viewX,y.
 */
angle_t R_PointToAngle(float x, float y)
{
    fixed_t             pos[2];

    x -= viewX;
    y -= viewY;

    if(x == 0 && y == 0)
        return 0;

    pos[VX] = FLT2FIX(x);
    pos[VY] = FLT2FIX(y);

    if(pos[VX] >= 0)
    {   // x >=0
        if(pos[VY] >= 0)
        {   // y>= 0
            if(pos[VX] > pos[VY])
                return tantoangle[R_SlopeDiv(pos[VY], pos[VX])];                // octant 0
            else
                return ANG90 - 1 - tantoangle[R_SlopeDiv(pos[VX], pos[VY])];    // octant 1
        }
        else
        {   // y<0
            pos[VY] = -pos[VY];
            if(pos[VX] > pos[VY])
                return -tantoangle[R_SlopeDiv(pos[VY], pos[VX])];               // octant 8
            else
                return ANG270 + tantoangle[R_SlopeDiv(pos[VX], pos[VY])];       // octant 7
        }
    }
    else
    {   // x<0
        pos[VX] = -pos[VX];
        if(pos[VY] >= 0)
        {   // y>= 0
            if(pos[VX] > pos[VY])
                return ANG180 - 1 - tantoangle[R_SlopeDiv(pos[VY], pos[VX])];   // octant 3
            else
                return ANG90 + tantoangle[R_SlopeDiv(pos[VX], pos[VY])];        // octant 2
        }
        else
        {   // y<0
            pos[VY] = -pos[VY];
            if(pos[VX] > pos[VY])
                return ANG180 + tantoangle[R_SlopeDiv(pos[VY], pos[VX])];       // octant 4
            else
                return ANG270 - 1 - tantoangle[R_SlopeDiv(pos[VX], pos[VY])];   // octant 5
        }
    }
}

angle_t R_PointToAngle2(float x1, float y1, float x2, float y2)
{
    viewX = x1;
    viewY = y1;
    return R_PointToAngle(x2, y2);
}

float R_PointToDist(const float x, const float y)
{
    uint                angle;
    float               dx, dy, temp;
    float               dist;

    dx = fabs(x - viewX);
    dy = fabs(y - viewY);

    if(dy > dx)
    {
        temp = dx;
        dx = dy;
        dy = temp;
    }

    angle =
        (tantoangle[FLT2FIX(dy / dx) >> DBITS] + ANG90) >> ANGLETOFINESHIFT;

    dist = dx / FIX2FLT(finesine[angle]); // Use as cosine

    return dist;
}

subsector_t *R_PointInSubsector(float x, float y)
{
    node_t             *node = 0;
    uint                nodenum = 0;

    if(!numNodes)               // single subsector is a special case
        return (subsector_t *) ssectors;

    nodenum = numNodes - 1;

    while(!(nodenum & NF_SUBSECTOR))
    {
        node = NODE_PTR(nodenum);
        ASSERT_DMU_TYPE(node, DMU_NODE);
        nodenum = node->children[R_PointOnSide(x, y, &node->partition)];
    }

    return SUBSECTOR_PTR(nodenum & ~NF_SUBSECTOR);
}

linedef_t *R_GetLineForSide(const uint sideNumber)
{
    uint                i;
    sidedef_t          *side = SIDE_PTR(sideNumber);
    sector_t           *sector = side->sector;

    // All sides may not have a sector.
    if(!sector)
        return NULL;

    for(i = 0; i < sector->lineDefCount; ++i)
        if(sector->lineDefs[i]->L_frontside == side ||
           sector->lineDefs[i]->L_backside == side)
        {
            return sector->lineDefs[i];
        }

    return NULL;
}

/**
 * Is the point inside the sector, according to the edge lines of the
 * subsector. Uses the well-known algorithm described here:
 * http://www.alienryderflex.com/polygon/
 *
 * @param               X coordinate to test.
 * @param               Y coordinate to test.
 * @param               Sector to test.
 *
 * @return              @c true, if the point is inside the sector.
 */
boolean R_IsPointInSector(const float x, const float y,
                          const sector_t *sector)
{
    uint                i;
    boolean             isOdd = false;

    for(i = 0; i < sector->lineDefCount; ++i)
    {
        linedef_t          *line = sector->lineDefs[i];
        vertex_t           *vtx[2];

        // Skip lines that aren't sector boundaries.
        if(line->L_frontside && line->L_backside &&
           line->L_frontsector == sector &&
           line->L_backsector == sector)
            continue;

        vtx[0] = line->L_v1;
        vtx[1] = line->L_v2;
        // It shouldn't matter whether the line faces inward or outward.
        if((vtx[0]->V_pos[VY] < y && vtx[1]->V_pos[VY] >= y) ||
           (vtx[1]->V_pos[VY] < y && vtx[0]->V_pos[VY] >= y))
        {
            if(vtx[0]->V_pos[VX] +
               (((y - vtx[0]->V_pos[VY]) / (vtx[1]->V_pos[VY] - vtx[0]->V_pos[VY])) *
                (vtx[1]->V_pos[VX] - vtx[0]->V_pos[VX])) < x)
            {
                // Toggle oddness.
                isOdd = !isOdd;
            }
        }
    }

    // The point is inside if the number of crossed nodes is odd.
    return isOdd;
}

/**
 * Is the point inside the sector, according to the edge lines of the
 * subsector. Uses the well-known algorithm described here:
 * http://www.alienryderflex.com/polygon/
 *
 * More accurate than R_IsPointInSector.
 *
 * @param               X coordinate to test.
 * @param               Y coordinate to test.
 * @param               Sector to test.
 *
 * @return              @c true, if the point is inside the sector.
 */
boolean R_IsPointInSector2(const float x, const float y,
                           const sector_t *sector)
{
    uint                i;
    subsector_t        *subsector;
    fvertex_t          *vi, *vj;

    subsector = R_PointInSubsector(x, y);
    if(subsector->sector != sector)
    {
        // Wrong sector.
        return false;
    }

    for(i = 0; i < subsector->segCount; ++i)
    {
        vi = &subsector->segs[i]->SG_v1->v;
        vj = &subsector->segs[(i + 1) % subsector->segCount]->SG_v1->v;

        if(((vi->pos[VY] - y) * (vj->pos[VX] - vi->pos[VX]) -
            (vi->pos[VX] - x) * (vj->pos[VY] - vi->pos[VY])) < 0)
        {
            // Outside the subsector's edges.
            return false;
        }

/*      if((vi->pos[VY] < y && vj->pos[VY] >= y) ||
           (vj->pos[VY] < y && vi->pos[VY] >= y))
        {
            if(vi->pos[VX] + (((y - vi->pos[VY])/(vj->pos[VY] - vi->pos[VY])) *
                              (vj->pos[VX] - vi->pos[VX])) < x)
            {
                // Toggle oddness.
                isOdd = !isOdd;
            }
        }
*/
    }

    // All tests passed.
    return true;
}

void R_ScaleAmbientRGB(float *out, const float *in, float mul)
{
    int                 i;
    float               val;

    if(mul < 0)
        mul = 0;
    else if(mul > 1)
        mul = 1;

    for(i = 0; i < 3; ++i)
    {
        val = in[i] * mul;

        if(out[i] < val)
            out[i] = val;
    }
}

/**
 * Conversion from HSV to RGB.  Everything is [0,1].
 */
void R_HSVToRGB(float* rgb, float h, float s, float v)
{
    int                 i;
    float               f, p, q, t;

    if(!rgb)
        return;

    if(s == 0)
    {
        // achromatic (grey)
        rgb[0] = rgb[1] = rgb[2] = v;
        return;
    }

    if(h >= 1)
        h -= 1;

    h *= 6; // sector 0 to 5
    i = floor(h);
    f = h - i; // factorial part of h
    p = v * (1 - s);
    q = v * (1 - s * f);
    t = v * (1 - s * (1 - f));

    switch(i)
    {
    case 0:
        rgb[0] = v;
        rgb[1] = t;
        rgb[2] = p;
        break;

    case 1:
        rgb[0] = q;
        rgb[1] = v;
        rgb[2] = p;
        break;

    case 2:
        rgb[0] = p;
        rgb[1] = v;
        rgb[2] = t;
        break;

    case 3:
        rgb[0] = p;
        rgb[1] = q;
        rgb[2] = v;
        break;

    case 4:
        rgb[0] = t;
        rgb[1] = p;
        rgb[2] = v;
        break;

    default:
        rgb[0] = v;
        rgb[1] = p;
        rgb[2] = q;
        break;
    }
}

/**
 * Returns a ptr to the sector which owns the given ddmobj_base_t.
 *
 * @param ddMobjBase    ddmobj_base_t to search for.
 *
 * @return              Ptr to the Sector where the ddmobj_base_t resides,
 *                      else @c NULL.
 */
sector_t* R_GetSectorForOrigin(const void* ddMobjBase)
{
    uint                i, k;
    sector_t*           sec;

    // Check all sectors; find where the sound is coming from.
    for(i = 0; i < numSectors; ++i)
    {
        sec = SECTOR_PTR(i);

        if(ddMobjBase == &sec->soundOrg)
            return sec;
        else
        {   // Check the planes of this sector
            for(k = 0; k < sec->planeCount; ++k)
                if(ddMobjBase == &sec->planes[k]->soundOrg)
                {
                    return sec;
                }
        }
    }
    return NULL;
}
