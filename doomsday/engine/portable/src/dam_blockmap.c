/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
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
 * dam_blockmap.c: BlockMap generation.
 *
 * Generate valid blockmap data from the already loaded level data.
 * Adapted from algorithm used in prBoom 2.2.6 -DJS
 *
 * Algorithm is order of nlines*(ncols+nrows) not nlines*ncols*nrows
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_misc.h"

#include "p_mapdata.h"

#include <stdlib.h>
#include <ctype.h>
#include <math.h>

// MACROS ------------------------------------------------------------------

#define BLKSHIFT 7                // places to shift rel position for cell num
#define BLKMASK ((1<<BLKSHIFT)-1) // mask for rel position within cell
#define BLKMARGIN 0               // size guardband around map used

// TYPES -------------------------------------------------------------------

// Used to list lines in each block.
typedef struct linelist_s {
    long        num;
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
                         uint blockno, long lineno)
{
    linelist_t *l;

    if(done[blockno])
        return;

    l = M_Malloc(sizeof(linelist_t));
    l->num = lineno;
    l->next = lists[blockno];

    lists[blockno] = l;

    count[blockno]++;

    done[blockno] = 1;
}

/**
 * Construct a blockmap from the level data.
 *
 * This finds the intersection of each linedef with the column and
 * row lines at the left and bottom of each blockmap cell. It then
 * adds the line to all block lists touching the intersection.
 */
boolean DAM_BuildBlockMap(gamemap_t* map)
{
    uint        i;
    int         j;
    int         bMapWidth, bMapHeight;  // blockmap dimensions
    static vec2_t bMapOrigin;           // blockmap origin (lower left)
    static vec2_t blockSize;            // size of the blocks
    uint       *blockcount = NULL;      // array of counters of line lists
    uint       *blockdone = NULL;       // array keeping track of blocks/line
    uint        numBlocks;              // number of cells = nrows*ncols

    linelist_t **blocklists = NULL;     // array of pointers to lists of lines
    long linetotal = 0;                 // total length of all blocklists

    vec2_t  bounds[2], point, dims;
    vertex_t *vtx;

    // scan for map limits, which the blockmap must enclose
    for(i = 0; i < map->numvertexes; ++i)
    {
        vtx = &map->vertexes[i];
        V2_Set(point, vtx->V_pos[VX], vtx->V_pos[VY]);
        if(!i)
            V2_InitBox(bounds, point);
        else
            V2_AddToBox(bounds, point);
    }

    // Setup the blockmap area to enclose the whole map,
    // plus a margin (margin is needed for a map that fits
    // entirely inside one blockmap cell).
    V2_Set(bounds[0], bounds[0][VX] - BLKMARGIN, bounds[0][VY] - BLKMARGIN);
    V2_Set(bounds[1], bounds[1][VX] + BLKMARGIN + 1, bounds[1][VY] + BLKMARGIN + 1);

    // Select a good size for the blocks.
    V2_Set(blockSize, 128, 128);
    V2_Copy(bMapOrigin, bounds[0]);   // min point
    V2_Subtract(dims, bounds[1], bounds[0]);

    // Calculate the dimensions of the blockmap.
    bMapWidth = ceil(dims[VX] / blockSize[VX]) + 1;
    bMapHeight = ceil(dims[VY] / blockSize[VY]) + 1;
    numBlocks = bMapWidth * bMapHeight;

    // Create the array of pointers on NBlocks to blocklists,
    // create an array of linelist counts on NBlocks, then finally,
    // make an array in which we can mark blocks done per line
    blocklists = M_Calloc(numBlocks * sizeof(linelist_t *));
    blockcount = M_Calloc(numBlocks * sizeof(uint));
    blockdone = M_Malloc(numBlocks * sizeof(uint));

    // Initialize each blocklist, and enter the trailing -1 in all blocklists.
    // NOTE: the linked list of lines grows backwards.
    for(i = 0; i < numBlocks; ++i)
    {
        blocklists[i] = M_Malloc(sizeof(linelist_t));
        blocklists[i]->num = -1;
        blocklists[i]->next = NULL;
        blockcount[i]++;
    }

    // For each linedef in the wad, determine all blockmap blocks it touches
    // and add the linedef number to the blocklists for those blocks.
    {
    int xorg = (int) bMapOrigin[VX];
    int yorg = (int) bMapOrigin[VY];
    int     v1[2], v2[2];
    int     dx, dy;
    int     vert, horiz;
    boolean slopePos, slopeNeg;
    int     bx, by;
    int     minx, maxx, miny, maxy;

    for(i = 0; i < map->numlines; ++i)
    {
        line_t *line = &map->lines[i];

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

        // extremal lines[i] coords
        minx = (v1[VX] > v2[VX]? v2[VX] : v1[VX]);
        maxx = (v1[VX] > v2[VX]? v1[VX] : v2[VX]);
        miny = (v1[VY] > v2[VY]? v2[VY] : v1[VY]);
        maxy = (v1[VY] > v2[VY]? v1[VY] : v2[VY]);

        // no blocks done for this linedef yet
        memset(blockdone, 0, numBlocks * sizeof(uint));

        // The line always belongs to the blocks containing its endpoints
        bx = (v1[VX] - xorg) >> BLKSHIFT;
        by = (v1[VY] - yorg) >> BLKSHIFT;
        addBlockLine(blocklists, blockcount, blockdone, by * bMapWidth + bx, i);

        bx = (v2[VX] - xorg) >> BLKSHIFT;
        by = (v2[VY] - yorg) >> BLKSHIFT;
        addBlockLine(blocklists, blockcount, blockdone, by * bMapWidth + bx, i);

        // For each column, see where the line along its left edge, which
        // it contains, intersects the Linedef i. Add i to each corresponding
        // blocklist.
        // We don't want to interesect vertical lines with columns.
        if(!vert)
        {
            for(j = 0; j < bMapWidth; ++j)
            {
                // intersection of Linedef with x=xorg+(j<<BLKSHIFT)
                // (y-v1[VY])*dx = dy*(x-v1[VX])
                // y = dy*(x-v1[VX])+v1[VY]*dx;
                int     x = xorg + (j << BLKSHIFT);       // (x,y) is intersection
                int     y = (dy * (x - v1[VX])) / dx + v1[VY];
                int     yb = (y - yorg) >> BLKSHIFT;      // block row number
                int     yp = (y - yorg) & BLKMASK;        // y position within block

                // Already outside the blockmap?
                if(yb < 0 || yb > (bMapHeight - 1))
                    continue;

                // Does the line touch this column at all?
                if(x < minx || x > maxx)
                    continue;

                // The cell that contains the intersection point is always added
                addBlockLine(blocklists, blockcount, blockdone, bMapWidth * yb + j, i);

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
                                         blockdone, bMapWidth * (yb - 1) + j, i);

                        if(j > 0 && minx < x)
                            addBlockLine(blocklists, blockcount,
                                         blockdone, bMapWidth * yb + j - 1, i);
                    }
                    else if(slopePos) //   / - block x-,y-
                    {
                        if(yb > 0 && j > 0 && minx < x)
                            addBlockLine(blocklists, blockcount,
                                         blockdone, bMapWidth * (yb - 1) + j - 1, i);
                    }
                    else if(horiz) //   - - block x-,y
                    {
                        if(j > 0 && minx < x)
                            addBlockLine(blocklists, blockcount,
                                         blockdone, bMapWidth * yb + j - 1, i);
                    }
                }
                else if(j > 0 && minx < x)
                {
                    // Else not at corner: x-,y
                    addBlockLine(blocklists, blockcount,
                                 blockdone, bMapWidth * yb + j - 1, i);
                }
            }
        }

        // For each row, see where the line along its bottom edge, which
        // it contains, intersects the Linedef i. Add i to all the corresponding
        // blocklists.
        if(!horiz)
        {
            for(j = 0; j < bMapHeight; ++j)
            {
                // intersection of Linedef with y=yorg+(j<<BLKSHIFT)
                // (x,y) on Linedef i satisfies: (y-v1[VY])*dx = dy*(x-v1[VX])
                // x = dx*(y-v1[VY])/dy+v1[VX];
                int     y = yorg + (j << BLKSHIFT);       // (x,y) is intersection
                int     x = (dx * (y - v1[VY])) / dy + v1[VX];
                int     xb = (x - xorg) >> BLKSHIFT;      // block column number
                int     xp = (x - xorg) & BLKMASK;        // x position within block

                // Outside the blockmap?
                if(xb < 0 || xb > bMapWidth - 1)
                    continue;

                // Touches this row?
                if(y < miny || y > maxy)
                    continue;

                // The cell that contains the intersection point is always added
                addBlockLine(blocklists, blockcount, blockdone, bMapWidth * j + xb, i);

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
                                         bMapWidth * (j - 1) + xb, i);
                        if(xb > 0 && minx < x)
                            addBlockLine(blocklists, blockcount, blockdone,
                                         bMapWidth * j + xb - 1, i);
                    }
                    else if(vert) //   | - block x,y-
                    {
                        if(j > 0 && miny < y)
                            addBlockLine(blocklists, blockcount, blockdone,
                                         bMapWidth * (j - 1) + xb, i);
                    }
                    else if(slopePos) //   / - block x-,y-
                    {
                        if(xb > 0 && j > 0 && miny < y)
                            addBlockLine(blocklists, blockcount, blockdone,
                                         bMapWidth * (j - 1) + xb -1, i);
                    }
                }
                else if(j > 0 && miny < y)
                {
                    // Else not on a corner: x, y-
                    addBlockLine(blocklists, blockcount, blockdone,
                                 bMapWidth * (j - 1) + xb, i);
                }
            }
        }
    }
    }

    // Add initial 0 to all blocklists
    // count the total number of lines (and 0's and -1's)
    memset(blockdone, 0, numBlocks * sizeof(uint));
    for(i = 0, linetotal = 0; i < numBlocks; ++i)
    {
        addBlockLine(blocklists, blockcount, blockdone, i, 0);
        linetotal += blockcount[i];
    }

    // Create the blockmap lump
    map->blockmaplump =
        Z_Malloc(sizeof(*map->blockmaplump) * (4 + numBlocks + linetotal),
                 PU_LEVELSTATIC, 0);
    // blockmap header
    map->blockmaplump[0] = map->bmaporgx = FLT2FIX(bMapOrigin[VX]);
    map->blockmaplump[1] = map->bmaporgy = FLT2FIX(bMapOrigin[VY]);
    map->blockmaplump[2] = map->bmapwidth  = bMapWidth;
    map->blockmaplump[3] = map->bmapheight = bMapHeight;

    // offsets to lists and block lists
    for(i = 0; i < numBlocks; ++i)
    {
        linelist_t *bl = blocklists[i];
        long    offs = map->blockmaplump[4 + i] =   // set offset to block's list
        (i? map->blockmaplump[4 + i - 1] : 4 + numBlocks) + (i? blockcount[i - 1] : 0);

        // add the lines in each block's list to the blockmaplump
        // delete each list node as we go
        while(bl)
        {
            linelist_t *tmp = bl->next;

            map->blockmaplump[offs++] = bl->num;
            M_Free(bl);
            bl = tmp;
        }
    }

    map->blockmap = map->blockmaplump + 4;

    // free all temporary storage
    M_Free(blocklists);
    M_Free(blockcount);
    M_Free(blockdone);

    return true;
}
