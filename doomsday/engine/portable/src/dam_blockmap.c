/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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
 * BlockMap generation.
 *
 * Generate valid blockmap data from the already loaded map data.
 * Adapted from algorithm used in prBoom 2.2.6 -DJS
 *
 * Algorithm is order of nlines*(ncols+nrows) not nlines*ncols*nrows
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <ctype.h>
#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_misc.h"
#include "de_play.h"

#include "p_mapdata.h"

// MACROS ------------------------------------------------------------------

#define BLKSHIFT                7 // places to shift rel position for cell num
#define BLKMASK                 ((1<<BLKSHIFT)-1) // mask for rel position within cell
#define BLKMARGIN               (8) // size guardband around map
#define MAPBLOCKUNITS           128

// TYPES -------------------------------------------------------------------

// Used to list lines in each block.
typedef struct linelist_s {
    linedef_t         *line;
    struct linelist_s *next;
} linelist_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Subroutine to add a line number to a block list
 * It simply returns if the line is already in the block
 */
static void addBlockLine(linelist_t **lists, uint *count, uint *done,
                         uint blockno, linedef_t *line)
{
    linelist_t*         l;

    if(done[blockno])
        return;

    l = M_Malloc(sizeof(linelist_t));

    l->line = line;

    l->next = lists[blockno];
    lists[blockno] = l;

    count[blockno]++;

    done[blockno] = 1;
}

/**
 * Construct a blockmap from the map data.
 *
 * This finds the intersection of each linedef with the column and row
 * lines at the left and bottom of each blockmap cell. It then adds the
 * line to all block lists touching the intersection.
 */
blockmap_t* DAM_BuildBlockMap(vertex_t*** vertexes, uint* numVertexes,
                              linedef_t** lineDefs, uint* numLineDefs)
{
    uint                startTime = Sys_GetRealTime();

    uint                i;
    int                 j;
    uint                bMapWidth, bMapHeight; // Blockmap dimensions.
    vec2_t              blockSize; // Size of the blocks.
    uint*               blockcount = NULL; // Array of counters of line lists.
    uint*               blockdone = NULL; // Array keeping track of blocks/line.
    uint                numBlocks; // Number of cells = nrows*ncols.

    linelist_t**        blocklists = NULL; // Array of pointers to lists of lines.
    vec2_t              bounds[2], point, dims;
    vertex_t*           vtx;
    blockmap_t*         blockmap;

    // Scan for map limits, which the blockmap must enclose.
    for(i = 0; i < *numVertexes; ++i)
    {
        vtx = (*vertexes)[i];
        V2_Set(point, vtx->V_pos[VX], vtx->V_pos[VY]);
        if(!i)
            V2_InitBox(bounds, point);
        else
            V2_AddToBox(bounds, point);
    }

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap
    // cell).
    V2_Set(bounds[0], bounds[0][VX] - BLKMARGIN, bounds[0][VY] - BLKMARGIN);
    V2_Set(bounds[1], bounds[1][VX] + BLKMARGIN, bounds[1][VY] + BLKMARGIN);

    // Select a good size for the blocks.
    V2_Set(blockSize, MAPBLOCKUNITS, MAPBLOCKUNITS);
    V2_Subtract(dims, bounds[1], bounds[0]);

    // Calculate the dimensions of the blockmap.
    if(dims[VX] <= blockSize[VX])
        bMapWidth = 1;
    else
        bMapWidth = ceil(dims[VX] / blockSize[VX]);

    if(dims[VY] <= blockSize[VY])
        bMapHeight = 1;
    else
        bMapHeight = ceil(dims[VY] / blockSize[VY]);
    numBlocks = bMapWidth * bMapHeight;

    // Adjust the max bound so we have whole blocks.
    V2_Set(bounds[1], bounds[0][VX] + bMapWidth  * blockSize[VX],
                      bounds[0][VY] + bMapHeight * blockSize[VY]);

    // Create the array of pointers on NBlocks to blocklists, create an array
    // of linelist counts on NBlocks, then finally, make an array in which we
    // can mark blocks done per line.
    blocklists = M_Calloc(numBlocks * sizeof(linelist_t*));
    blockcount = M_Calloc(numBlocks * sizeof(uint));
    blockdone  = M_Malloc(numBlocks * sizeof(uint));

    // For each linedef in the wad, determine all blockmap blocks it touches
    // and add the linedef number to the blocklists for those blocks.
    {
    int                 xorg = (int) bounds[0][VX];
    int                 yorg = (int) bounds[0][VY];
    int                 v1[2], v2[2];
    int                 dx, dy;
    int                 vert, horiz;
    boolean             slopePos, slopeNeg;
    int                 bx, by;
    int                 minx, maxx, miny, maxy;

    for(i = 0; i < *numLineDefs; ++i)
    {
        linedef_t*      line = &(*lineDefs)[i];

        if(line->inFlags & LF_POLYOBJ)
            continue; // Polyobj lines don't get into the blockmap.

        v1[VX] = (int) line->L_v1pos[VX];
        v1[VY] = (int) line->L_v1pos[VY];
        v2[VX] = (int) line->L_v2pos[VX];
        v2[VY] = (int) line->L_v2pos[VY];
        dx = v2[VX] - v1[VX];
        dy = v2[VY] - v1[VY];
        vert = !dx;
        horiz = !dy;
        slopePos = (dx ^ dy) > 0;
        slopeNeg = (dx ^ dy) < 0;

        // Extremal lines[i] coords.
        minx = (v1[VX] > v2[VX]? v2[VX] : v1[VX]);
        maxx = (v1[VX] > v2[VX]? v1[VX] : v2[VX]);
        miny = (v1[VY] > v2[VY]? v2[VY] : v1[VY]);
        maxy = (v1[VY] > v2[VY]? v1[VY] : v2[VY]);

        // No blocks done for this linedef yet.
        memset(blockdone, 0, numBlocks * sizeof(uint));

        // The line always belongs to the blocks containing its endpoints
        bx = (v1[VX] - xorg) >> BLKSHIFT;
        by = (v1[VY] - yorg) >> BLKSHIFT;
        addBlockLine(blocklists, blockcount, blockdone, by * bMapWidth + bx,
                     line);

        bx = (v2[VX] - xorg) >> BLKSHIFT;
        by = (v2[VY] - yorg) >> BLKSHIFT;
        addBlockLine(blocklists, blockcount, blockdone, by * bMapWidth + bx,
                     line);

        // For each column, see where the line along its left edge, which
        // it contains, intersects the Linedef i. Add i to each corresponding
        // blocklist.
        // We don't want to interesect vertical lines with columns.
        if(!vert)
        {
            for(j = 0; j < (signed) bMapWidth; ++j)
            {
                // intersection of Linedef with x=xorg+(j<<BLKSHIFT)
                // (y-v1[VY])*dx = dy*(x-v1[VX])
                // y = dy*(x-v1[VX])+v1[VY]*dx;
                int             x = xorg + (j << BLKSHIFT); // (x,y) is intersection
                int             y = (dy * (x - v1[VX])) / dx + v1[VY];
                int             yb = (y - yorg) >> BLKSHIFT; // block row number
                int             yp = (y - yorg) & BLKMASK; // y position within block

                // Already outside the blockmap?
                if(yb < 0 || yb > (signed) (bMapHeight) + 1)
                    continue;

                // Does the line touch this column at all?
                if(x < minx || x > maxx)
                    continue;

                // The cell that contains the intersection point is always added
                addBlockLine(blocklists, blockcount, blockdone, bMapWidth * yb + j,
                             line);

                // If the intersection is at a corner it depends on the slope
                // (and whether the line extends past the intersection) which
                // blocks are hit.

                // Where does the intersection occur?
                if(yp == 0)
                {
                    // Intersection occured at a corner
                    if(slopeNeg) //   \ - blocks x,y-, x-,y
                    {
                        if(yb > 0 && miny < y)
                            addBlockLine(blocklists, blockcount,
                                         blockdone, bMapWidth * (yb - 1) + j,
                                         line);

                        if(j > 0 && minx < x)
                            addBlockLine(blocklists, blockcount,
                                         blockdone, bMapWidth * yb + j - 1,
                                         line);
                    }
                    else if(slopePos) //   / - block x-,y-
                    {
                        if(yb > 0 && j > 0 && minx < x)
                            addBlockLine(blocklists, blockcount,
                                         blockdone, bMapWidth * (yb - 1) + j - 1,
                                         line);
                    }
                    else if(horiz) //   - - block x-,y
                    {
                        if(j > 0 && minx < x)
                            addBlockLine(blocklists, blockcount,
                                         blockdone, bMapWidth * yb + j - 1,
                                         line);
                    }
                }
                else if(j > 0 && minx < x)
                {
                    // Else not at corner: x-,y
                    addBlockLine(blocklists, blockcount,
                                 blockdone, bMapWidth * yb + j - 1,
                                 line);
                }
            }
        }

        // For each row, see where the line along its bottom edge, which
        // it contains, intersects the Linedef i. Add i to all the corresponding
        // blocklists.
        if(!horiz)
        {
            for(j = 0; j < (signed) bMapHeight; ++j)
            {
                // intersection of Linedef with y=yorg+(j<<BLKSHIFT)
                // (x,y) on Linedef i satisfies: (y-v1[VY])*dx = dy*(x-v1[VX])
                // x = dx*(y-v1[VY])/dy+v1[VX];
                int             y = yorg + (j << BLKSHIFT); // (x,y) is intersection
                int             x = (dx * (y - v1[VY])) / dy + v1[VX];
                int             xb = (x - xorg) >> BLKSHIFT; // block column number
                int             xp = (x - xorg) & BLKMASK; // x position within block

                // Outside the blockmap?
                if(xb < 0 || xb > (signed) (bMapWidth) + 1)
                    continue;

                // Touches this row?
                if(y < miny || y > maxy)
                    continue;

                // The cell that contains the intersection point is always added
                addBlockLine(blocklists, blockcount, blockdone, bMapWidth * j + xb,
                             line);

                // If the intersection is at a corner it depends on the slope
                // (and whether the line extends past the intersection) which
                // blocks are hit

                // Where does the intersection occur?
                if(xp == 0)
                {
                    // Intersection occured at a corner
                    if(slopeNeg) //   \ - blocks x,y-, x-,y
                    {
                        if(j > 0 && miny < y)
                            addBlockLine(blocklists, blockcount, blockdone,
                                         bMapWidth * (j - 1) + xb, line);
                        if(xb > 0 && minx < x)
                            addBlockLine(blocklists, blockcount, blockdone,
                                         bMapWidth * j + xb - 1, line);
                    }
                    else if(vert) //   | - block x,y-
                    {
                        if(j > 0 && miny < y)
                            addBlockLine(blocklists, blockcount, blockdone,
                                         bMapWidth * (j - 1) + xb, line);
                    }
                    else if(slopePos) //   / - block x-,y-
                    {
                        if(xb > 0 && j > 0 && miny < y)
                            addBlockLine(blocklists, blockcount, blockdone,
                                         bMapWidth * (j - 1) + xb -1, line);
                    }
                }
                else if(j > 0 && miny < y)
                {
                    // Else not on a corner: x, y-
                    addBlockLine(blocklists, blockcount, blockdone,
                                 bMapWidth * (j - 1) + xb, line);
                }
            }
        }
    }
    }

    // Create the blockmap.
    blockmap = P_BlockmapCreate(bounds[0], bounds[1],
                                bMapWidth, bMapHeight);

    // Create the actual links by 'hardening' the lists into arrays.
    {
    uint            x, y;
    for(y = 0; y < bMapHeight; ++y)
        for(x = 0; x < bMapWidth; ++x)
        {
            uint            count = blockcount[y * bMapWidth + x];
            linelist_t*     bl = blocklists[y * bMapWidth + x];

            if(count > 0)
            {
                linedef_t**     lines, **ptr;

                // A NULL-terminated array of pointers to lines.
                lines = Z_Malloc((count + 1) * sizeof(linedef_t *),
                                 PU_MAPSTATIC, NULL);

                // Copy pointers to the array, delete the nodes.
                ptr = lines;
                while(bl)
                {
                    linelist_t *tmp = bl->next;

                    *ptr++ = (linedef_t *) bl->line;

                    M_Free(bl);
                    bl = tmp;
                }
                // Terminate.
                *ptr = NULL;

                // Link it into the BlockMap.
                P_BlockmapSetBlock(blockmap, x, y, lines, NULL, NULL);
            }
        }
    }

    // free all temporary storage
    M_Free(blocklists);
    M_Free(blockcount);
    M_Free(blockdone);

    VERBOSE2( Con_Message("DAM_BuildBlockMap: Done in %.2f seconds.\n",
        (Sys_GetRealTime() - startTime) / 1000.0f) )

    return blockmap;
}
