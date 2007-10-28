/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006-2007 Jamie Jones <yagisan@dengine.net>
 *\author Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 *\author Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 *\author Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
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
 * GL-friendly BSP node builder.
 * bsp_analyze.c: Analyzing level structures.
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_bsp.h"
#include "de_misc.h"

#include <stdlib.h>
#include <math.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// Stuff needed from bsp_level.c (this file closely related).
extern mvertex_t  **levVertices;
extern mlinedef_t **levLinedefs;
extern msidedef_t **levSidedefs;
extern msector_t  **levSectors;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int mapBounds[4]; 
static int blockMapBounds[4];

// CODE --------------------------------------------------------------------

static int C_DECL vertexCompare(const void *p1, const void *p2)
{
    int         vert1 = ((const uint16_t *) p1)[0];
    int         vert2 = ((const uint16_t *) p2)[0];
    mvertex_t   *a, *b;

    if(vert1 == vert2)
        return 0;

    a = levVertices[vert1];
    b = levVertices[vert2];

    if((int) a->V_pos[VX] != (int) b->V_pos[VX])
        return (int) a->V_pos[VX] - (int) b->V_pos[VX];

    return (int) a->V_pos[VY] - (int) b->V_pos[VY];
}

#if 0 // Currently unused.
static int C_DECL sidedefCompare(const void *p1, const void *p2)
{
    int         comp;
    int         side1 = ((const uint16_t *) p1)[0];
    int         side2 = ((const uint16_t *) p2)[0];
    msidedef_t  *a, *b;

    if(side1 == side2)
        return 0;

    a = levSidedefs[side1];
    b = levSidedefs[side2];

    if(a->sector != b->sector)
    {
        if(a->sector == NULL)
            return -1;
        if(b->sector == NULL)
            return +1;

        return (a->sector->index - b->sector->index);
    }

    // Sidedefs must be the same.
    return 0;
}
#endif

#if 0 // Currently unused.
void BSP_DetectDuplicateSidedefs(void)
{
    int         i;
    uint16_t   *hits = M_Calloc(numSidedefs * sizeof(uint16_t));

    // Sort array of indices.
    for(i = 0; i < numSidedefs; ++i)
        hits[i] = i;

    qsort(hits, numSidedefs, sizeof(uint16_t), sidedefCompare);

    // Now mark them off.
    for(i = 0; i < numSidedefs - 1; ++i)
    {
        // A duplicate?
        if(sidedefCompare(hits + i, hits + i + 1) == 0)
        {   // Yes.
            msidedef_t *a = levSidedefs[hits[i]];
            msidedef_t *b = levSidedefs[hits[i + 1]];

            b->equiv = (a->equiv? a->equiv : a);
        }
    }

    M_Free(hits);
}
#endif

void BSP_GetBMapBounds(int *x, int *y, int *w, int *h)
{
    if(x)
        *x = blockMapBounds[BOXLEFT];
    if(y)
        *y = blockMapBounds[BOXBOTTOM];
    if(w)
        *w = blockMapBounds[BOXRIGHT];
    if(h)
        *h = blockMapBounds[BOXTOP];
}

static void findMapLimits(int *bbox)
{
    int         i;

    M_ClearBox(bbox);

    for(i = 0; i < numLinedefs; ++i)
    {
        mlinedef_t *L = LookupLinedef(i);

        if(!(L->mlFlags & MLF_ZEROLENGTH))
        {
            double      x1 = L->v[0]->V_pos[VX];
            double      y1 = L->v[0]->V_pos[VY];
            double      x2 = L->v[1]->V_pos[VX];
            double      y2 = L->v[1]->V_pos[VY];
            int         lX = (int) floor(MIN_OF(x1, x2));
            int         lY = (int) floor(MIN_OF(y1, y2));
            int         hX = (int) ceil(MAX_OF(x1, x2));
            int         hY = (int) ceil(MAX_OF(y1, y2));

            M_AddToBox(bbox, lX, lY);
            M_AddToBox(bbox, hX, hY);
        }
    }
}

void BSP_InitAnalyzer(void)
{
    // Find maximal vertexes, and store as map limits.
    findMapLimits(mapBounds);

    VERBOSE(Con_Message("Map goes from (%d,%d) to (%d,%d)\n",
                        mapBounds[BOXLEFT], mapBounds[BOXBOTTOM],
                        mapBounds[BOXRIGHT], mapBounds[BOXTOP]));

    blockMapBounds[BOXLEFT] =
        mapBounds[BOXLEFT] - (mapBounds[BOXLEFT] & 0x7);
    blockMapBounds[BOXBOTTOM] =
        mapBounds[BOXBOTTOM] - (mapBounds[BOXBOTTOM] & 0x7);

    blockMapBounds[BOXRIGHT] =
        ((mapBounds[BOXRIGHT] - blockMapBounds[BOXLEFT]) / 128) + 1;
    blockMapBounds[BOXTOP] =
        ((mapBounds[BOXTOP] - blockMapBounds[BOXBOTTOM]) / 128) + 1;
}

void BSP_DetectDuplicateVertices(void)
{
    int         i;
    uint16_t   *hits = M_Calloc(numVertices * sizeof(uint16_t));

    // Sort array of indices.
    for(i = 0; i < numVertices; ++i)
        hits[i] = i;

    qsort(hits, numVertices, sizeof(uint16_t), vertexCompare);

    // Now mark them off.
    for(i = 0; i < numVertices - 1; ++i)
    {
        // A duplicate?
        if(vertexCompare(hits + i, hits + i + 1) == 0)
        {   // Yes.
            mvertex_t *a = levVertices[hits[i]];
            mvertex_t *b = levVertices[hits[i + 1]];

            b->equiv = (a->equiv ? a->equiv : a);
        }
    }

    M_Free(hits);
}

/**
 * Checks if the index is in the bitfield.
 */
static __inline boolean hasIndexBit(uint index, uint *bitfield)
{
    // Assume 32-bit uint.
    return (bitfield[index >> 5] & (1 << (index & 0x1f))) != 0;
}

/**
 * Sets the index in the bitfield.
 * Count is incremented when a zero bit is changed to one.
 */
static __inline void addIndexBit(uint index, uint *bitfield)
{
    // Assume 32-bit uint.
    bitfield[index >> 5] |= (1 << (index & 0x1f));
}

static pruneLinedefs(void)
{
    int         i, newNum;

    // Scan all linedefs.
    for(i = 0, newNum = 0; i < numLinedefs; ++i)
    {
        mlinedef_t *l = levLinedefs[i];

        // Handle duplicated vertices.
        while(l->v[0]->equiv)
        {
            l->v[0]->refCount--;
            l->v[0] = l->v[0]->equiv;
            l->v[0]->refCount++;
        }

        while(l->v[1]->equiv)
        {
            l->v[1]->refCount--;
            l->v[1] = l->v[1]->equiv;
            l->v[1]->refCount++;
        }

        // Remove zero length lines.
        if(l->mlFlags & MLF_ZEROLENGTH)
        {
            l->v[0]->refCount--;
            l->v[1]->refCount--;

            M_Free(l);
            continue;
        }

        l->index = newNum;
        levLinedefs[newNum++] = l;
    }

    if(newNum < numLinedefs)
    {
        VERBOSE(Con_Message("  Pruned %d zero-length linedefs\n",
                            numLinedefs - newNum));
        numLinedefs = newNum;
    }
}

static void pruneVertices(void)
{
    int         i, newNum, unused = 0;

    // Scan all vertices.
    for(i = 0, newNum = 0; i < numVertices; ++i)
    {
        mvertex_t *v = levVertices[i];

        if(v->refCount < 0)
            Con_Error("Vertex %d ref_count is %d", i, v->refCount);

        if(v->refCount == 0)
        {
            if(v->equiv == NULL)
                unused++;

            M_Free(v);
            continue;
        }

        v->index = newNum;
        levVertices[newNum++] = v;
    }

    if(newNum < numVertices)
    {
        int         dupNum = numVertices - newNum - unused;

        if(verbose >= 1)
        {
            if(unused > 0)
                Con_Message("  Pruned %d unused vertices.\n", unused);

            if(dupNum > 0)
                Con_Message("  Pruned %d duplicate vertices\n", dupNum);
        }

        numVertices = newNum;
    }

    numNormalVert = numVertices;
}

static void pruneUnusedSidedefs(void)
{
    int         i, newNum, unused = 0;
    size_t      bitfieldSize;
    uint       *indexBitfield = 0;

    bitfieldSize = 4 * (numSidedefs + 7) / 8;
    indexBitfield = M_Calloc(bitfieldSize);

    for(i = 0; i < numLinedefs; ++i)
    {
        mlinedef_t *l = levLinedefs[i];

        if(l->sides[FRONT])
            addIndexBit(l->sides[FRONT]->index, indexBitfield);

        if(l->sides[BACK])
            addIndexBit(l->sides[BACK]->index, indexBitfield);
    }

    // Scan all sidedefs.
    for(i = 0, newNum = 0; i < numSidedefs; ++i)
    {
        msidedef_t *s = levSidedefs[i];

        if(!hasIndexBit(s->index, indexBitfield))
        {
            unused++;

            M_Free(s);
            continue;
        }

        s->index = newNum;
        levSidedefs[newNum++] = s;
    }

    M_Free(indexBitfield);

    if(newNum < numSidedefs)
    {
        int         dupNum = numSidedefs - newNum - unused;

        if(verbose >= 1)
        {
            if(unused > 0)
                Con_Message("  Pruned %d unused sidedefs\n", unused);

            if(dupNum > 0)
                Con_Message("  Pruned %d duplicate sidedefs\n", dupNum);
        }

        numSidedefs = newNum;
    }
}

static void pruneUnusedSectors(void)
{
    int         i, newNum;
    size_t      bitfieldSize;
    uint       *indexBitfield = 0;

    bitfieldSize = 4 * (numSectors + 7) / 8;
    indexBitfield = M_Calloc(bitfieldSize);

    for(i = 0; i < numSidedefs; ++i)
    {
        msidedef_t *s = levSidedefs[i];

        if(s->sector)
            addIndexBit(s->sector->index, indexBitfield);
    }

    // Scan all sectors.
    for(i = 0, newNum = 0; i < numSectors; ++i)
    {
        msector_t *s = levSectors[i];

        if(!hasIndexBit(s->index, indexBitfield))
        {
            M_Free(s);
            continue;
        }

        s->index = newNum;
        levSectors[newNum++] = s;
    }

    M_Free(indexBitfield);

    if(newNum < numSectors)
    {
        VERBOSE(Con_Message("  Pruned %d unused sectors\n",
                            numSectors - newNum));
        numSectors = newNum;
    }
}

/**
 * \note Order here is critical!
 */
void BSP_PruneRedundantMapData(int flags)
{
    if(flags & PRUNE_LINEDEFS)
        pruneLinedefs();

    if(flags & PRUNE_VERTEXES)
        pruneVertices();

    if(flags & PRUNE_SIDEDEFS)
        pruneUnusedSidedefs();

    if(flags & PRUNE_SECTORS)
        pruneUnusedSectors();
}

/**
 * @return          The "lowest" vertex (normally the left-most, but if the
 *                  line is vertical, then the bottom-most).
 *                  @c => 0 for start, 1 for end.
 */
static __inline int lineVertexLowest(const mlinedef_t *l)
{
    return (((int) l->v[0]->V_pos[VX] < (int) l->v[1]->V_pos[VX] ||
             ((int) l->v[0]->V_pos[VX] == (int) l->v[1]->V_pos[VX] &&
              (int) l->v[0]->V_pos[VY] <  (int) l->v[1]->V_pos[VY]))? 0 : 1);
}

static int
#ifdef WIN32
__cdecl
#endif
lineStartCompare(const void *p1, const void *p2)
{
    int         line1 = ((const int *) p1)[0];
    int         line2 = ((const int *) p2)[0];
    mlinedef_t  *a = levLinedefs[line1];
    mlinedef_t  *b = levLinedefs[line2];
    mvertex_t   *c, *d;

    if(line1 == line2)
        return 0;

    // Determine left-most vertex of each line.
    c = (lineVertexLowest(a)? a->v[1] : a->v[0]);
    d = (lineVertexLowest(b)? b->v[1] : b->v[0]);

    if((int) c->V_pos[VX] != (int) d->V_pos[VX])
        return (int) c->V_pos[VX] - (int) d->V_pos[VX];

    return (int) c->V_pos[VY] - (int) d->V_pos[VY];
}

static int
#ifdef WIN32
__cdecl
#endif
lineEndCompare(const void *p1, const void *p2)
{
    int         line1 = ((const int *) p1)[0];
    int         line2 = ((const int *) p2)[0];
    mlinedef_t  *a = levLinedefs[line1];
    mlinedef_t  *b = levLinedefs[line2];
    mvertex_t   *c, *d;

    if(line1 == line2)
        return 0;

    // Determine right-most vertex of each line.
    c = (lineVertexLowest(a)? a->v[0] : a->v[1]);
    d = (lineVertexLowest(b)? b->v[0] : b->v[1]);

    if((int) c->V_pos[VX] != (int) d->V_pos[VX])
        return (int) c->V_pos[VX] - (int) d->V_pos[VX];

    return (int) c->V_pos[VY] - (int) d->V_pos[VY];
}

/**
 * \note Algorithm:
 * Sort all lines by left-most vertex.
 * Overlapping lines will then be near each other in this set but note this
 * does not detect partially overlapping lines!
 */
void BSP_DetectOverlappingLines(void)
{
    int         i;
    int        *hits = M_Calloc(numLinedefs * sizeof(int));
    int         count = 0;

    // Sort array of indices.
    for(i = 0; i < numLinedefs; ++i)
        hits[i] = i;
    qsort(hits, numLinedefs, sizeof(int), lineStartCompare);

    for(i = 0; i < numLinedefs - 1; ++i)
    {
        int         j;

        for(j = i + 1; j < numLinedefs; ++j)
        {
            if(lineStartCompare(hits + i, hits + j) != 0)
                break;

            if(lineEndCompare(hits + i, hits + j) == 0)
            {   // Found an overlap!
                mlinedef_t *a = levLinedefs[hits[i]];
                mlinedef_t *b = levLinedefs[hits[j]];

                b->overlap = (a->overlap ? a->overlap : a);
                count++;
            }
        }
    }

    M_Free(hits);

    if(count > 0)
        VERBOSE(Con_Message("Detected %d overlapped linedefs\n", count));
}

/**
 * \note Algorithm:
 * Cast a line horizontally or vertically and see what we hit (OUCH, we
 * have to iterate over all linedefs!).
 */
static void testForWindowEffect(mlinedef_t *l)
{
    int         i;
    double      mX = (l->v[0]->V_pos[VX] + l->v[1]->V_pos[VX]) / 2.0;
    double      mY = (l->v[0]->V_pos[VY] + l->v[1]->V_pos[VY]) / 2.0;
    double      dX =  l->v[1]->V_pos[VX] - l->v[0]->V_pos[VX];
    double      dY =  l->v[1]->V_pos[VY] - l->v[0]->V_pos[VY];
    int         castHoriz = fabs(dX) < fabs(dY) ? 1 : 0;

    double      backDist = 999999.0;
    msector_t  *backOpen = NULL;
    int         backLine = -1;

    double      frontDist = 999999.0;
    msector_t  *frontOpen = NULL;
    int         frontLine = -1;

    for(i = 0; i < numLinedefs; ++i)
    {
        mlinedef_t     *n = levLinedefs[i];

        double          dist;
        boolean         isFront;
        msidedef_t     *hitSide;
        double          dX2, dY2;

        if(n == l || (n->mlFlags & MLF_ZEROLENGTH) || n->overlap)
            continue;

        dX2 = n->v[1]->V_pos[VX] - n->v[0]->V_pos[VX];
        dY2 = n->v[1]->V_pos[VY] - n->v[0]->V_pos[VY];

        if(castHoriz)
        {   // Horizontal.
            if(fabs(dY2) < DIST_EPSILON)
                continue;

            if((MAX_OF(n->v[0]->V_pos[VY], n->v[1]->V_pos[VY]) < mY - DIST_EPSILON) ||
               (MIN_OF(n->v[0]->V_pos[VY], n->v[1]->V_pos[VY]) > mY + DIST_EPSILON))
                continue;

            dist =
                (n->v[0]->V_pos[VX] + (mY - n->v[0]->V_pos[VY]) * dX2 / dY2) - mX;

            isFront = (((dY > 0) == (dist > 0)) ? true : false);

            dist = fabs(dist);
            if(dist < DIST_EPSILON)
                continue; // Too close (overlapping lines ?)

            hitSide = n->sides[(dY > 0) ^ (dY2 > 0) ^ !isFront];
        }
        else
        {   // Vertical.
            if(fabs(dX2) < DIST_EPSILON)
                continue;

            if((MAX_OF(n->v[0]->V_pos[VX], n->v[1]->V_pos[VX]) < mX - DIST_EPSILON) ||
               (MIN_OF(n->v[0]->V_pos[VX], n->v[1]->V_pos[VX]) > mX + DIST_EPSILON))
                continue;

            dist =
                (n->v[0]->V_pos[VY] + (mX - n->v[0]->V_pos[VX]) * dY2 / dX2) - mY;

            isFront = (((dX > 0) != (dist > 0)) ? true : false);

            dist = fabs(dist);

            hitSide = n->sides[(dX > 0) ^ (dX2 > 0) ^ !isFront];
        }

        if(dist < DIST_EPSILON)  // too close (overlapping lines ?)
            continue;

        if(isFront)
        {
            if(dist < frontDist)
            {
                frontDist = dist;
                if(hitSide)
                    frontOpen = hitSide->sector;
                else
                    frontOpen = NULL;
                frontLine = i;
            }
        }
        else
        {
            if(dist < backDist)
            {
                backDist = dist;
                if(hitSide)
                    backOpen = hitSide->sector;
                else
                    backOpen = NULL;
                backLine = i;
            }
        }
    }
/*
#if _DEBUG
Con_Message("back line: %d  back dist: %1.1f  back_open: %s\n",
            backLine, backDist, (backOpen ? "OPEN" : "CLOSED"));
Con_Message("front line: %d  front dist: %1.1f  front_open: %s\n",
            frontLine, frontDist, (frontOpen ? "OPEN" : "CLOSED"));
#endif
*/
    if(backOpen && frontOpen && l->sides[FRONT]->sector == frontOpen)
    {
        l->windowEffect = backOpen;
        Con_Message("Linedef #%d seems to be a One-Sided Window "
                    "(back faces sector #%d).\n", l->index,
                    backOpen->index);
    }
}

/**
 * \note Algorithm:
 * Scan the linedef list looking for possible candidates, checking for an
 * odd number of one-sided linedefs connected to a single vertex.
 * This idea courtesy of Graham Jackson.
 */
void BSP_DetectWindowEffects(void)
{
    int         i, oneSiders, twoSiders;

    for(i = 0; i < numLinedefs; ++i)
    {
        mlinedef_t *l = levLinedefs[i];

        if((l->mlFlags & MLF_TWOSIDED) || (l->mlFlags & MLF_ZEROLENGTH) ||
            l->overlap || !l->sides[FRONT])
            continue;

        BSP_CountEdgeTips(l->v[0], &oneSiders, &twoSiders);

        if((oneSiders % 2) == 1 && (oneSiders + twoSiders) > 1)
        {
/*
#if _DEBUG
Con_Message("FUNNY LINE %d : start vertex %d has odd number of one-siders\n",
            i, l->v[0]->index);
#endif
*/
            testForWindowEffect(l);
            continue;
        }

        BSP_CountEdgeTips(l->v[1], &oneSiders, &twoSiders);

        if((oneSiders % 2) == 1 && (oneSiders + twoSiders) > 1)
        {
/*
#if _DEBUG
Con_Message("FUNNY LINE %d : end vertex %d has odd number of one-siders\n",
            i, l->v[1]->index));
#endif
*/
            testForWindowEffect(l);
        }
    }
}
