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

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int mapBounds[4];
static int blockMapBounds[4];

// CODE --------------------------------------------------------------------

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

static void findMapLimits(editmap_t *src, int *bbox)
{
    uint        i;

    M_ClearBox(bbox);

    for(i = 0; i < src->numlines; ++i)
    {
        line_t     *L = src->lines[i];

        if(!(L->buildData.mlFlags & MLF_ZEROLENGTH))
        {
            double      x1 = L->v[0]->buildData.pos[VX];
            double      y1 = L->v[0]->buildData.pos[VY];
            double      x2 = L->v[1]->buildData.pos[VX];
            double      y2 = L->v[1]->buildData.pos[VY];
            int         lX = (int) floor(MIN_OF(x1, x2));
            int         lY = (int) floor(MIN_OF(y1, y2));
            int         hX = (int) ceil(MAX_OF(x1, x2));
            int         hY = (int) ceil(MAX_OF(y1, y2));

            M_AddToBox(bbox, lX, lY);
            M_AddToBox(bbox, hX, hY);
        }
    }
}

void BSP_InitAnalyzer(editmap_t *map)
{
    // Find maximal vertexes, and store as map limits.
    findMapLimits(map, mapBounds);

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

static void pruneLinedefs(editmap_t *src)
{
    uint            i, newNum;

    // Scan all linedefs.
    for(i = 0, newNum = 0; i < src->numlines; ++i)
    {
        line_t         *l = src->lines[i];

        // Handle duplicated vertices.
        while(l->v[0]->buildData.equiv)
        {
            l->v[0]->buildData.refCount--;
            l->v[0] = l->v[0]->buildData.equiv;
            l->v[0]->buildData.refCount++;
        }

        while(l->v[1]->buildData.equiv)
        {
            l->v[1]->buildData.refCount--;
            l->v[1] = l->v[1]->buildData.equiv;
            l->v[1]->buildData.refCount++;
        }

        // Remove zero length lines.
        if(l->buildData.mlFlags & MLF_ZEROLENGTH)
        {
            l->v[0]->buildData.refCount--;
            l->v[1]->buildData.refCount--;

            M_Free(src->lines[i]);
            src->lines[i] = NULL;
            continue;
        }

        l->buildData.index = newNum;
        src->lines[newNum++] = src->lines[i];
    }

    if(newNum < src->numlines)
    {
        VERBOSE(Con_Message("  Pruned %d zero-length linedefs\n",
                            src->numlines - newNum));
        src->numlines = newNum;
    }
}

static void pruneVertices(editmap_t *map)
{
    uint            i, newNum, unused = 0;

    // Scan all vertices.
    for(i = 0, newNum = 0; i < map->numvertexes; ++i)
    {
        vertex_t           *v = map->vertexes[i];

        if(v->buildData.refCount < 0)
            Con_Error("Vertex %d ref_count is %d", i, v->buildData.refCount);

        if(v->buildData.refCount == 0)
        {
            if(v->buildData.equiv == NULL)
                unused++;

            M_Free(v);
            continue;
        }

        v->buildData.index = newNum;
        map->vertexes[newNum++] = v;
    }

    if(newNum < map->numvertexes)
    {
        int         dupNum = map->numvertexes - newNum - unused;

        if(verbose >= 1)
        {
            if(unused > 0)
                Con_Message("  Pruned %d unused vertices.\n", unused);

            if(dupNum > 0)
                Con_Message("  Pruned %d duplicate vertices\n", dupNum);
        }

        map->numvertexes = newNum;
    }
}

#if 0 // Currently unused.
static void pruneUnusedSidedefs(void)
{
    int         i, newNum, unused = 0;
    size_t      bitfieldSize;
    uint       *indexBitfield = 0;

    bitfieldSize = 4 * (numSidedefs + 7) / 8;
    indexBitfield = M_Calloc(bitfieldSize);

    for(i = 0; i < numLinedefs; ++i)
    {
        line_t         *l = levLinedefs[i];

        if(l->sides[FRONT])
            addIndexBit(l->sides[FRONT]->buildData.index, indexBitfield);

        if(l->sides[BACK])
            addIndexBit(l->sides[BACK]->buildData.index, indexBitfield);
    }

    // Scan all sidedefs.
    for(i = 0, newNum = 0; i < numSidedefs; ++i)
    {
        side_t *s = levSidedefs[i];

        if(!hasIndexBit(s->buildData.index, indexBitfield))
        {
            unused++;

            M_Free(s);
            continue;
        }

        s->buildData.index = newNum;
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
#endif

#if 0 // Currently unused.
static void pruneUnusedSectors(void)
{
    int         i, newNum;
    size_t      bitfieldSize;
    uint       *indexBitfield = 0;

    bitfieldSize = 4 * (numSectors + 7) / 8;
    indexBitfield = M_Calloc(bitfieldSize);

    for(i = 0; i < numSidedefs; ++i)
    {
        side_t *s = levSidedefs[i];

        if(s->sector)
            addIndexBit(s->sector->buildData.index, indexBitfield);
    }

    // Scan all sectors.
    for(i = 0, newNum = 0; i < numSectors; ++i)
    {
        sector_t *s = levSectors[i];

        if(!hasIndexBit(s->buildData.index, indexBitfield))
        {
            M_Free(s);
            continue;
        }

        s->buildData.index = newNum;
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
#endif

/**
 * \note Order here is critical!
 */
void BSP_PruneRedundantMapData(editmap_t *map, int flags)
{
    if(flags & PRUNE_LINEDEFS)
        pruneLinedefs(map);

    if(flags & PRUNE_VERTEXES)
        pruneVertices(map);

    //if(flags & PRUNE_SIDEDEFS)
    //    pruneUnusedSidedefs();

    //if(flags & PRUNE_SECTORS)
    //    pruneUnusedSectors();
}

/**
 * @return          The "lowest" vertex (normally the left-most, but if the
 *                  line is vertical, then the bottom-most).
 *                  @c => 0 for start, 1 for end.
 */
static __inline int lineVertexLowest(const line_t *l)
{
    return (((int) l->v[0]->buildData.pos[VX] < (int) l->v[1]->buildData.pos[VX] ||
             ((int) l->v[0]->buildData.pos[VX] == (int) l->v[1]->buildData.pos[VX] &&
              (int) l->v[0]->buildData.pos[VY] <  (int) l->v[1]->buildData.pos[VY]))? 0 : 1);
}

static editmap_t *globalMap;

static int
#ifdef WIN32
__cdecl
#endif
lineStartCompare(const void *p1, const void *p2)
{
    uint            line1 = ((const uint *) p1)[0];
    uint            line2 = ((const uint *) p2)[0];
    line_t         *a = globalMap->lines[line1];
    line_t         *b = globalMap->lines[line2];
    vertex_t       *c, *d;

    if(line1 == line2)
        return 0;

    // Determine left-most vertex of each line.
    c = (lineVertexLowest(a)? a->v[1] : a->v[0]);
    d = (lineVertexLowest(b)? b->v[1] : b->v[0]);

    if((int) c->buildData.pos[VX] != (int) d->buildData.pos[VX])
        return (int) c->buildData.pos[VX] - (int) d->buildData.pos[VX];

    return (int) c->buildData.pos[VY] - (int) d->buildData.pos[VY];
}

static int
#ifdef WIN32
__cdecl
#endif
lineEndCompare(const void *p1, const void *p2)
{
    uint            line1 = ((const uint *) p1)[0];
    uint            line2 = ((const uint *) p2)[0];
    line_t         *a = globalMap->lines[line1];
    line_t         *b = globalMap->lines[line2];
    vertex_t       *c, *d;

    if(line1 == line2)
        return 0;

    // Determine right-most vertex of each line.
    c = (lineVertexLowest(a)? a->v[0] : a->v[1]);
    d = (lineVertexLowest(b)? b->v[0] : b->v[1]);

    if((int) c->buildData.pos[VX] != (int) d->buildData.pos[VX])
        return (int) c->buildData.pos[VX] - (int) d->buildData.pos[VX];

    return (int) c->buildData.pos[VY] - (int) d->buildData.pos[VY];
}

/**
 * \note Algorithm:
 * Sort all lines by left-most vertex.
 * Overlapping lines will then be near each other in this set but note this
 * does not detect partially overlapping lines!
 */
void BSP_DetectOverlappingLines(editmap_t *map)
{
    uint        i;
    uint       *hits = M_Malloc(map->numlines * sizeof(uint));
    uint        count = 0;

    globalMap = map;

    // Sort array of indices.
    for(i = 0; i < map->numlines; ++i)
        hits[i] = i;
    qsort(hits, map->numlines, sizeof(uint), lineStartCompare);

    for(i = 0; i < map->numlines - 1; ++i)
    {
        uint        j;

        for(j = i + 1; j < map->numlines; ++j)
        {
            if(lineStartCompare(hits + i, hits + j) != 0)
                break;

            if(lineEndCompare(hits + i, hits + j) == 0)
            {   // Found an overlap!
                line_t *a = map->lines[hits[i]];
                line_t *b = map->lines[hits[j]];

                b->buildData.overlap = (a->buildData.overlap ? a->buildData.overlap : a);
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
static void testForWindowEffect(editmap_t *map, line_t *l)
{
    uint        i;
    double      mX = (l->v[0]->buildData.pos[VX] + l->v[1]->buildData.pos[VX]) / 2.0;
    double      mY = (l->v[0]->buildData.pos[VY] + l->v[1]->buildData.pos[VY]) / 2.0;
    double      dX =  l->v[1]->buildData.pos[VX] - l->v[0]->buildData.pos[VX];
    double      dY =  l->v[1]->buildData.pos[VY] - l->v[0]->buildData.pos[VY];
    int         castHoriz = fabs(dX) < fabs(dY) ? 1 : 0;

    double      backDist = 999999.0;
    sector_t  *backOpen = NULL;
    int         backLine = -1;

    double      frontDist = 999999.0;
    sector_t  *frontOpen = NULL;
    int         frontLine = -1;

    for(i = 0; i < map->numlines; ++i)
    {
        line_t         *n = map->lines[i];
        double          dist;
        boolean         isFront;
        side_t         *hitSide;
        double          dX2, dY2;

        if(n == l || (n->buildData.mlFlags & MLF_ZEROLENGTH) || n->buildData.overlap)
            continue;

        dX2 = n->v[1]->buildData.pos[VX] - n->v[0]->buildData.pos[VX];
        dY2 = n->v[1]->buildData.pos[VY] - n->v[0]->buildData.pos[VY];

        if(castHoriz)
        {   // Horizontal.
            if(fabs(dY2) < DIST_EPSILON)
                continue;

            if((MAX_OF(n->v[0]->buildData.pos[VY], n->v[1]->buildData.pos[VY]) < mY - DIST_EPSILON) ||
               (MIN_OF(n->v[0]->buildData.pos[VY], n->v[1]->buildData.pos[VY]) > mY + DIST_EPSILON))
                continue;

            dist =
                (n->v[0]->buildData.pos[VX] + (mY - n->v[0]->buildData.pos[VY]) * dX2 / dY2) - mX;

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

            if((MAX_OF(n->v[0]->buildData.pos[VX], n->v[1]->buildData.pos[VX]) < mX - DIST_EPSILON) ||
               (MIN_OF(n->v[0]->buildData.pos[VX], n->v[1]->buildData.pos[VX]) > mX + DIST_EPSILON))
                continue;

            dist =
                (n->v[0]->buildData.pos[VY] + (mX - n->v[0]->buildData.pos[VX]) * dY2 / dX2) - mY;

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
        l->buildData.windowEffect = backOpen;
        Con_Message("Linedef #%d seems to be a One-Sided Window "
                    "(back faces sector #%d).\n", l->buildData.index,
                    backOpen->buildData.index);
    }
}

/**
 * \note Algorithm:
 * Scan the linedef list looking for possible candidates, checking for an
 * odd number of one-sided linedefs connected to a single vertex.
 * This idea courtesy of Graham Jackson.
 */
void BSP_DetectWindowEffects(editmap_t *map)
{
    uint            i, oneSiders, twoSiders;

    for(i = 0; i < map->numlines; ++i)
    {
        line_t         *l = map->lines[i];

        if((l->buildData.mlFlags & MLF_TWOSIDED) || (l->buildData.mlFlags & MLF_ZEROLENGTH) ||
            l->buildData.overlap || !l->sides[FRONT])
            continue;

        BSP_CountEdgeTips(l->v[0], &oneSiders, &twoSiders);

        if((oneSiders % 2) == 1 && (oneSiders + twoSiders) > 1)
        {
/*
#if _DEBUG
Con_Message("FUNNY LINE %d : start vertex %d has odd number of one-siders\n",
            i, l->buildData.v[0]->index);
#endif
*/
            testForWindowEffect(map, l);
            continue;
        }

        BSP_CountEdgeTips(l->v[1], &oneSiders, &twoSiders);

        if((oneSiders % 2) == 1 && (oneSiders + twoSiders) > 1)
        {
/*
#if _DEBUG
Con_Message("FUNNY LINE %d : end vertex %d has odd number of one-siders\n",
            i, l->buildData.v[1]->index));
#endif
*/
            testForWindowEffect(map, l);
        }
    }
}
