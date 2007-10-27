/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007 Daniel Swanson <danij@dengine.net>
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
 * bsp_node.h: BSP node builder. Recursive node creation.
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 */

#ifndef __BSP_NODE_H__
#define __BSP_NODE_H__

#include "p_mapdata.h"

typedef struct child_s {
    // Child node or subsector (one must be NULL).
    struct mnode_s *node;
    msubsec_t  *subSec;
} child_t;

#define RIGHT                   0
#define LEFT                    1

typedef struct mnode_s {
    // Node index. Only valid once the NODES or GL_NODES lump has been
    // created.
    int         index;

    // The node is too long, and the (dx,dy) values should be halved
    // when writing into the NODES lump.
    boolean     tooLong;

// Final data.
    int         x, y; // Starting point.
    int         dX, dY; // Offset to ending point.

    int         bBox[2][4]; // Bounding box for each child.
    child_t     children[2]; // Children {RIGHT, LEFT}
} mnode_t;

extern int numNodes;

mnode_t    *NewNode(void);
mnode_t    *LookupNode(int index);

int         BoxOnLineSide(struct superblock_s *box, struct hedge_s *part);

boolean     BuildNodes(struct superblock_s *hEdgeList, mnode_t **n, msubsec_t **s,
                       int depth);
void        BSP_AddHEdgeToSuperBlock(struct superblock_s *block, struct hedge_s *hEdge);

int         ComputeBspHeight(mnode_t *node);
void        ClockwiseBspTree(mnode_t *root);

void        SaveMap(gamemap_t *map, mnode_t *rootNode);
#endif
