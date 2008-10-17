/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
#include "de_play.h"

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

void BSP_GetBMapBounds(int* x, int* y, int* w, int* h)
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

static void findMapLimits(gamemap_t* src, int* bbox)
{
    uint                i;

    M_ClearBox(bbox);

    for(i = 0; i < src->numLineDefs; ++i)
    {
        linedef_t*          l = &src->lineDefs[i];

        if(!(l->buildData.mlFlags & MLF_ZEROLENGTH))
        {
            double              x1 = l->v[0]->buildData.pos[VX];
            double              y1 = l->v[0]->buildData.pos[VY];
            double              x2 = l->v[1]->buildData.pos[VX];
            double              y2 = l->v[1]->buildData.pos[VY];
            int                 lX = (int) floor(MIN_OF(x1, x2));
            int                 lY = (int) floor(MIN_OF(y1, y2));
            int                 hX = (int) ceil(MAX_OF(x1, x2));
            int                 hY = (int) ceil(MAX_OF(y1, y2));

            M_AddToBox(bbox, lX, lY);
            M_AddToBox(bbox, hX, hY);
        }
    }
}

void BSP_InitAnalyzer(gamemap_t* map)
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

#if 0 /* Currently unused. */
/**
 * @return              The "lowest" vertex (normally the left-most, but if
 *                      the line is vertical, then the bottom-most).
 *                      @c => 0 for start, 1 for end.
 */
static __inline int lineVertexLowest(const linedef_t* l)
{
    return (((int) l->v[0]->buildData.pos[VX] < (int) l->v[1]->buildData.pos[VX] ||
             ((int) l->v[0]->buildData.pos[VX] == (int) l->v[1]->buildData.pos[VX] &&
              (int) l->v[0]->buildData.pos[VY] < (int) l->v[1]->buildData.pos[VY]))? 0 : 1);
}

static int C_DECL lineStartCompare(const void* p1, const void* p2)
{
    const linedef_t*    a = (const linedef_t*) p1;
    const linedef_t*    b = (const linedef_t*) p2;
    vertex_t*           c, *d;

    // Determine left-most vertex of each line.
    c = (lineVertexLowest(a)? a->v[1] : a->v[0]);
    d = (lineVertexLowest(b)? b->v[1] : b->v[0]);

    if((int) c->buildData.pos[VX] != (int) d->buildData.pos[VX])
        return (int) c->buildData.pos[VX] - (int) d->buildData.pos[VX];

    return (int) c->buildData.pos[VY] - (int) d->buildData.pos[VY];
}

static int C_DECL lineEndCompare(const void* p1, const void* p2)
{
    const linedef_t*    a = (const linedef_t*) p1;
    const linedef_t*    b = (const linedef_t*) p2;
    vertex_t*           c, *d;

    // Determine right-most vertex of each line.
    c = (lineVertexLowest(a)? a->v[0] : a->v[1]);
    d = (lineVertexLowest(b)? b->v[0] : b->v[1]);

    if((int) c->buildData.pos[VX] != (int) d->buildData.pos[VX])
        return (int) c->buildData.pos[VX] - (int) d->buildData.pos[VX];

    return (int) c->buildData.pos[VY] - (int) d->buildData.pos[VY];
}

size_t numOverlaps;

boolean testOverlaps(linedef_t* b, void* data)
{
    linedef_t*          a = (linedef_t*) data;

    if(a != b)
    {
        if(lineStartCompare(a, b) == 0)
            if(lineEndCompare(a, b) == 0)
            {   // Found an overlap!
                b->buildData.overlap =
                    (a->buildData.overlap ? a->buildData.overlap : a);
                numOverlaps++;
            }
    }

    return true; // Continue iteration.
}

typedef struct {
    blockmap_t*         blockMap;
    uint                block[2];
} findoverlaps_params_t;

boolean findOverlapsForLinedef(linedef_t* l, void* data)
{
    findoverlaps_params_t* params = (findoverlaps_params_t*) data;

    P_BlockmapLinesIterator(params->blockMap, params->block, testOverlaps, l);
    return true; // Continue iteration.
}

/**
 * \note Does not detect partially overlapping lines!
 */
void BSP_DetectOverlappingLines(gamemap_t* map)
{
    uint                x, y, bmapDimensions[2];
    findoverlaps_params_t params;

    params.blockMap = map->blockMap;
    numOverlaps = 0;

    P_GetBlockmapDimensions(map->blockMap, bmapDimensions);

    for(y = 0; y < bmapDimensions[VY]; ++y)
        for(x = 0; x < bmapDimensions[VX]; ++x)
        {
            params.block[VX] = x;
            params.block[VY] = y;

            P_BlockmapLinesIterator(map->blockMap, params.block,
                                    findOverlapsForLinedef, &params);
        }

    if(numOverlaps > 0)
        VERBOSE(Con_Message("Detected %lu overlapped linedefs\n",
                            (unsigned long) numOverlaps));
}
#endif
