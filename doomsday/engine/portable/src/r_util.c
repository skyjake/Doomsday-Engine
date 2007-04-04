/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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

/*
 * r_util.c: Refresh Utility Routines
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_refresh.h"

#include "p_dmu.h"

// MACROS ------------------------------------------------------------------

#define SLOPERANGE  2048
#define SLOPEBITS   11
#define DBITS       (FRACBITS-SLOPEBITS)

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
 * Which side of the node does the point lie?
 *
 * @param x         X coordinate to test.
 * @param y         Y coordinate to test.
 * @return int      <code>0</code> = front, else <code>1</code> = back.
 */
int R_PointOnSide(fixed_t x, fixed_t y, node_t *node)
{
    float       fx = FIX2FLT(x), fy = FIX2FLT(y);
    float       dx, dy;
    float       left, right;

    if(!node->dx)
    {
        if(fx <= node->x)
            return (node->dy > 0? 1:0);
        else
            return (node->dy < 0? 1:0);
    }
    if(!node->dy)
    {
        if(fy <= node->y)
            return (node->dx < 0? 1:0);
        else
            return (node->dx > 0? 1:0);
    }

    dx = (fx - node->x);
    dy = (fy - node->y);

    // Try to quickly decide by looking at the signs.
    if(node->dx < 0)
    {
        if(node->dy < 0)
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
        if(node->dy < 0)
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

    left = node->dy * dx;
    right = dy * node->dx;

    if(right < left)
        return 0;               // front side
    else
        return 1;               // back side
}

/*
   int  R_PointOnSegSide (fixed_t x, fixed_t y, seg_t *line)
   {
   fixed_t  lx, ly;
   fixed_t  ldx, ldy;
   fixed_t  dx,dy;
   fixed_t  left, right;

   lx = line->v1->x;
   ly = line->v1->y;

   ldx = line->v2->x - lx;
   ldy = line->v2->y - ly;

   if (!ldx)
   {
   if (x <= lx)
   return ldy > 0;
   return ldy < 0;
   }
   if (!ldy)
   {
   if (y <= ly)
   return ldx < 0;
   return ldx > 0;
   }

   dx = (x - lx);
   dy = (y - ly);

   // try to quickly decide by looking at sign bits
   if ( (ldy ^ ldx ^ dx ^ dy)&0x80000000 )
   {
   if  ( (ldy ^ dx) & 0x80000000 )
   return 1;    // (left is negative)
   return 0;
   }

   left = FixedMul ( ldy>>FRACBITS , dx );
   right = FixedMul ( dy , ldx>>FRACBITS );

   if (right < left)
   return 0;        // front side
   return 1;            // back side
   }
 */

int R_SlopeDiv(unsigned num, unsigned den)
{
    unsigned    ans;

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
 * @return  angle_t     Angle between the test point and viewx,y.
 */
angle_t R_PointToAngle(fixed_t x, fixed_t y)
{
    x -= viewx;
    y -= viewy;
    if((!x) && (!y))
        return 0;

    if(x >= 0)
    {                           // x >=0
        if(y >= 0)
        {                       // y>= 0
            if(x > y)
                return tantoangle[R_SlopeDiv(y, x)];                // octant 0
            else
                return ANG90 - 1 - tantoangle[R_SlopeDiv(x, y)];    // octant 1
        }
        else
        {                       // y<0
            y = -y;
            if(x > y)
                return -tantoangle[R_SlopeDiv(y, x)];               // octant 8
            else
                return ANG270 + tantoangle[R_SlopeDiv(x, y)];       // octant 7
        }
    }
    else
    {                           // x<0
        x = -x;
        if(y >= 0)
        {                       // y>= 0
            if(x > y)
                return ANG180 - 1 - tantoangle[R_SlopeDiv(y, x)];   // octant 3
            else
                return ANG90 + tantoangle[R_SlopeDiv(x, y)];        // octant 2
        }
        else
        {                       // y<0
            y = -y;
            if(x > y)
                return ANG180 + tantoangle[R_SlopeDiv(y, x)];       // octant 4
            else
                return ANG270 - 1 - tantoangle[R_SlopeDiv(x, y)];   // octant 5
        }
    }
}

angle_t R_PointToAngle2(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2)
{
    viewx = x1;
    viewy = y1;
    return R_PointToAngle(x2, y2);
}

fixed_t R_PointToDist(fixed_t x, fixed_t y)
{
    int         angle;
    fixed_t     dx, dy, temp;
    fixed_t     dist;

    dx = abs(x - viewx);
    dy = abs(y - viewy);

    if(dy > dx)
    {
        temp = dx;
        dx = dy;
        dy = temp;
    }

    angle =
        (tantoangle[FixedDiv(dy, dx) >> DBITS] + ANG90) >> ANGLETOFINESHIFT;

    dist = FixedDiv(dx, finesine[angle]);   // use as cosine

    return dist;
}

subsector_t *R_PointInSubsector(fixed_t x, fixed_t y)
{
    node_t     *node = 0;
    uint        nodenum = 0;

    if(!numnodes)               // single subsector is a special case
        return (subsector_t *) subsectors;

    nodenum = numnodes - 1;

    while(!(nodenum & NF_SUBSECTOR))
    {
        node = NODE_PTR(nodenum);
        ASSERT_DMU_TYPE(node, DMU_NODE);
        nodenum = node->children[ R_PointOnSide(x, y, node) ];
    }
    return SUBSECTOR_PTR(nodenum & ~NF_SUBSECTOR);
}

line_t *R_GetLineForSide(uint sideNumber)
{
    uint        i;
    side_t     *side = SIDE_PTR(sideNumber);
    sector_t   *sector = side->sector;

    // All sides may not have a sector.
    if(!sector)
        return NULL;

    for(i = 0; i < sector->linecount; ++i)
        if(sector->Lines[i]->sides[0] == side ||
           sector->Lines[i]->sides[1] == side)
        {
            return sector->Lines[i];
        }

    return NULL;
}

/**
 * Is the point inside the sector, according to the edge lines of the
 * subsector. Uses the well-known algorithm described here:
 * http://www.alienryderflex.com/polygon/
 *
 * @param   x           X coordinate to test.
 * @param   y           Y coordinate to test.
 * @param   sector      Sector to test.
 *
 * @return  boolean     (TRUE) If the point is inside the sector.
 */
boolean R_IsPointInSector(fixed_t x, fixed_t y, sector_t *sector)
{
#define VI      (sector->Lines[i]->v[0])
#define VJ      (sector->Lines[i]->v[1])

    uint        i;
    boolean     isOdd = false;
    float       fx = FIX2FLT(x), fy = FIX2FLT(y);

    for(i = 0; i < sector->linecount; ++i)
    {
        // Skip lines that aren't sector boundaries.
        if(sector->Lines[i]->L_frontsector == sector &&
           sector->Lines[i]->L_backsector == sector)
            continue;

        // It shouldn't matter whether the line faces inward or outward.
        if((VI->pos[VY] < fy && VJ->pos[VY] >= fy) ||
           (VJ->pos[VY] < fy && VI->pos[VY] >= fy))
        {
            if(VI->pos[VX] +
               (((fy - VI->pos[VY]) / (VJ->pos[VY] - VI->pos[VY])) *
                (VJ->pos[VX] - VI->pos[VX])) < fx)
            {
                // Toggle oddness.
                isOdd = !isOdd;
            }
        }
    }

    // The point is inside if the number of crossed nodes is odd.
    return isOdd;

#undef VI
#undef VJ
}

/**
 * Is the point inside the sector, according to the edge lines of the
 * subsector. Uses the well-known algorithm described here:
 * http://www.alienryderflex.com/polygon/
 *
 * More accurate than R_IsPointInSector.
 *
 * @param   x           X coordinate to test.
 * @param   y           Y coordinate to test.
 * @param   sector      Sector to test.
 *
 * @return  boolean     (TRUE) If the point is inside the sector.
 */
boolean R_IsPointInSector2(fixed_t x, fixed_t y, sector_t *sector)
{
    uint        i;
    subsector_t *subsector;
    fvertex_t  *vi, *vj;
    float       fx, fy;

    subsector = R_PointInSubsector(x, y);
    if(subsector->sector != sector)
    {
        // Wrong sector.
        return false;
    }

    fx = FIX2FLT(x);
    fy = FIX2FLT(y);

    for(i = 0; i < subsector->numverts; ++i)
    {
        vi = &subsector->verts[i];
        vj = &subsector->verts[(i + 1) % subsector->numverts];

        if(((vi->pos[VY] - fy) * (vj->pos[VX] - vi->pos[VX]) -
            (vi->pos[VX] - fx) * (vj->pos[VY] - vi->pos[VY])) < 0)
        {
            // Outside the subsector's edges.
            return false;
        }

/*      if((vi->pos[VY] < fy && vj->pos[VY] >= fy) ||
           (vj->pos[VY] < fy && vi->pos[VY] >= fy))
        {
            if(vi->pos[VX] + (((fy - vi->pos[VY])/(vj->pos[VY] - vi->pos[VY])) *
                              (vj->pos[VX] - vi->pos[VX])) < fx)
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

/**
 * Returns a ptr to the sector which owns the given degenmobj.
 *
 * @param degenmobj     degenmobj to search for.
 *
 * @return              Ptr to the Sector where the degenmobj resides,
 *                      else <code>NULL</code>.
 */
sector_t *R_GetSectorForDegen(void *degenmobj)
{
    uint        i, k;
    sector_t   *sec;

    // Check all sectors; find where the sound is coming from.
    for(i = 0; i < numsectors; ++i)
    {
        sec = SECTOR_PTR(i);

        if(degenmobj == &sec->soundorg)
            return sec;
        else
        {   // Check the planes of this sector
            for(k = 0; k < sec->planecount; ++k)
                if(degenmobj == &sec->planes[k]->soundorg)
                {
                    return sec;
                }
        }
    }
    return NULL;
}
