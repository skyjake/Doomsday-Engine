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
 * bsp_main.c: GL-friendly BSP node builder.
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_bsp.h"
#include "de_misc.h"

#include "p_mapdata.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int bspFactor = 7;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Register the ccmds and cvars of the BSP builder. Called during engine
 * startup
 */
void BSP_Register(void)
{
    C_VAR_INT("bsp-factor", &bspFactor, CVF_NO_MAX, 0, 0);
}


/**
 * Initially create all half-edges, one for each side of a linedef.
 *
 * \pre Blockmap must be initialized before this is called!
 *
 * @return              The list of created half-edges.
 */
static superblock_t *createInitialHEdges(void)
{
    int         i;
    int         bw, bh;
    hedge_t    *back, *front;
    superblock_t *block;

    block = BSP_SuperBlockCreate();

    BSP_GetBMapBounds(&block->x1, &block->y1, &bw, &bh);

    block->x2 = block->x1 + 128 * M_CeilPow2(bw);
    block->y2 = block->y1 + 128 * M_CeilPow2(bh);

    // Step through linedefs and get side numbers.
    for(i = 0; i < numLinedefs; ++i)
    {
        mlinedef_t *line = LookupLinedef(i);

        front = back = NULL;

        // Ignore zero-length and overlapping lines.
        if(!(line->mlFlags & MLF_ZEROLENGTH) && !line->overlap)
        {
            // Check for Humungously long lines.
            if(ABS(line->v[0]->V_pos[VX] - line->v[1]->V_pos[VX]) >= 10000 ||
               ABS(line->v[0]->V_pos[VY] - line->v[1]->V_pos[VY]) >= 10000)
            {
                if(3000 >=
                   M_Length(line->v[0]->V_pos[VX] - line->v[1]->V_pos[VX],
                            line->v[0]->V_pos[VY] - line->v[1]->V_pos[VY]))
                {
                    Con_Message("Linedef #%d is VERY long, it may cause problems\n",
                                line->index);
                }
            }

            if(line->sides[FRONT])
            {
                msidedef_t     *side = line->sides[FRONT];

                // Check for a bad sidedef.
                if(!side->sector)
                    Con_Message("Bad sidedef on linedef #%d (Z_CheckHeap error)\n",
                                line->index);

                front = BSP_CreateHEdge(line, line, line->v[0], line->v[1],
                                        side->sector, false);
                BSP_AddHEdgeToSuperBlock(block, front);
            }
            else
                Con_Message("Linedef #%d has no front sidedef!\n",
                            line->index);

            if(line->sides[BACK])
            {
                msidedef_t     *side = line->sides[BACK];

                // Check for a bad sidedef.
                if(!side->sector)
                    Con_Message("Bad sidedef on linedef #%d (Z_CheckHeap error)\n",
                                line->index);

                back = BSP_CreateHEdge(line, line, line->v[1], line->v[0],
                                       side->sector, true);
                BSP_AddHEdgeToSuperBlock(block, back);

                if(front)
                {
                    // Half-edges always maintain a one-to-one relationship
                    // with their twins, so if one gets split, the other
                    // must be split also.
                    back->twin = front;
                    front->twin = back;
                }
            }
            else
            {
                if(line->mlFlags & MLF_TWOSIDED)
                {
                    Con_Message("Linedef #%d is 2s but has no back sidedef\n",
                                line->index);
                    line->mlFlags &= ~MLF_TWOSIDED;
                }

                // Handle the 'One-Sided Window' trick.
                if(line->windowEffect)
                {
                    hedge_t    *other;
                    
                    other = BSP_CreateHEdge(front->linedef, line,
                                            line->v[1], line->v[0],
                                            line->windowEffect, true);

                    BSP_AddHEdgeToSuperBlock(block, other);

                    // Setup the twin-ing (it's very strange to have a mini
                    // and a normal partnered together).
                    other->twin = front;
                    front->twin = other;
                }
            }
        }

        // \todo edge tips should be created when half-edges are created.
        {
        double x1 = line->v[0]->V_pos[VX];
        double y1 = line->v[0]->V_pos[VY];
        double x2 = line->v[1]->V_pos[VX];
        double y2 = line->v[1]->V_pos[VY];

        BSP_CreateVertexEdgeTip(line->v[0], x2 - x1, y2 - y1, back, front);
        BSP_CreateVertexEdgeTip(line->v[1], x1 - x2, y1 - y2, front, back);
        }
    }

/*
#if _DEBUG
for(i = 0; i < numVertices; ++i)
{
    mvertex_t *vert = LookupVertex(i);
    edgetip_t *tip;

    Con_Message("EdgeTips for vertex %d:\n", i);

    for(tip = vert->tipSet; tip; tip = tip->next)
    {
        Con_Message("  Angle=%1.1f left=%d right=%d\n", tip->angle,
                    (tip->left? tip->left->index : -1),
                    (tip->right? tip->right->index : -1));
    }
}
#endif
*/
    return block;
}

/**
 * Build the BSP for the given map.
 *
 * @param map           The map to build the BSP for.
 * @return              @c true, if completed successfully.
 */
boolean BSP_Build(gamemap_t *map)
{
    mnode_t    *rootNode;
    msubsec_t  *rootSub;
    superblock_t *hEdgeList;
    boolean     builtOK;
    uint        startTime;

    if(verbose >= 1)
    {
        Con_Message("BSP_Build: Processing map \"%s\" using tunable "
                    "factor of %d...\n", map->levelid, bspFactor);
    }

    // It begins...
    startTime = Sys_GetRealTime();

    BSP_InitSuperBlockAllocator();
    BSP_InitIntersectionAllocator();

    LoadMap(map);
    CleanMap(map);
    BSP_InitAnalyzer();

    BSP_DetectOverlappingLines();
    BSP_DetectWindowEffects();

    // Create initial half-edges.
    hEdgeList = createInitialHEdges();

    // Recursively create nodes.
    builtOK = BuildNodes(hEdgeList, &rootNode, &rootSub, 0);

    // The superblock data is no longer needed.
    BSP_SuperBlockDestroy(hEdgeList);

    if(builtOK)
    {   // Success!
        // Wind the BSP tree and link to the map.
        ClockwiseBspTree(rootNode);
        SaveMap(map, rootNode);

        Con_Message("BSP_Build: Built %d Nodes, %d Subsectors, %d Segs, %d Vertexes\n",
                    map->numnodes, map->numsubsectors, map->numsegs,
                    numNormalVert + numGLVert);

        if(rootNode)
            Con_Message("  Heights of left and right subtrees (%d, %d).\n",
                        ComputeBspHeight(rootNode->children[RIGHT].node),
                        ComputeBspHeight(rootNode->children[LEFT].node));
    }
    else
    {
        Con_Message("BSP_Build: Failed building nodes for map \"s\"!\n",
                    map->levelid);
    }

    // Free temporary storage.
    FreeMap();
    BSP_ShutdownSuperBlockAllocator();
    BSP_ShutdownIntersectionAllocator();

    // How much time did we spend?
    VERBOSE(Con_Message("  Done in %.2f seconds.\n",
                        (Sys_GetRealTime() - startTime) / 1000.0f));

    return builtOK;
}
