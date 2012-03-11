/**
 * @file bsp_edge.h
 * BSP Builder Half-edges. @ingroup map
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 *
 * @authors Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 * @authors Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 * @authors Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
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

#ifndef LIBDENG_MAP_BSP_HEDGE
#define LIBDENG_MAP_BSP_HEDGE

#define IFFY_LEN            4.0

// Smallest distance between two points before being considered equal.
#define DIST_EPSILON        (1.0 / 128.0)

// Smallest degrees between two angles before being considered equal.
#define ANG_EPSILON         (1.0 / 1024.0)

#define ET_prev             link[0]
#define ET_next             link[1]
#define ET_edge             hEdges

// An edge tip is where an edge meets a vertex.
typedef struct edgetip_s {
    // Link in list. List is kept in ANTI-clockwise order.
    struct edgetip_s* link[2]; // {prev, next};

    // Angle that line makes at vertex (degrees).
    angle_g angle;

    // Half-edge on each side of the edge. Left is the side of increasing
    // angles, right is the side of decreasing angles. Either can be NULL
    // for one sided edges.
    struct bsp_hedge_s* hEdges[2];
} edgetip_t;

typedef struct bsp_hedge_s {
    Vertex* v[2]; // [Start, End] of the half-edge..

    // Half-edge on the other side, or NULL if one-sided. This relationship
    // is always one-to-one -- if one of the half-edges is split, the twin
    // must also be split.
    struct bsp_hedge_s* twin;

    struct bsp_hedge_s* next;
    struct bsp_hedge_s* nextOnSide;
    struct bsp_hedge_s* prevOnSide;

    // Index of the half-edge. Only valid once the half-edge has been added
    // to a polygon. A negative value means it is invalid -- there
    // shouldn't be any of these once the BSP tree has been built.
    int index;

    // The superblock that contains this half-edge, or NULL if the half-edge
    // is no longer in any superblock (e.g. now in a leaf).
    struct superblock_s* block;

    // Precomputed data for faster calculations.
    double pSX, pSY;
    double pEX, pEY;
    double pDX, pDY;

    double pLength;
    double pAngle;
    double pPara;
    double pPerp;

    // Linedef that this half-edge goes along, or NULL if miniseg.
    LineDef* lineDef;

    // Linedef that this half-edge initially comes from.
    // For "real" half-edges, this is just the same as the 'linedef' field
    // above. For "miniedges", this is the linedef of the partition line.
    LineDef* sourceLineDef;

    Sector* sector; // Adjacent sector, or NULL if invalid sidedef or minihedge.
    byte side; // 0 for right, 1 for left.
} bsp_hedge_t;

/**
 * Init the half-edge block allocator.
 */
void BSP_InitHEdgeAllocator(void);

/**
 * Shutdown the half-edge block allocator. All elements will be free'd!
 */
void BSP_ShutdownHEdgeAllocator(void);

/**
 * Create a new half-edge.
 */
bsp_hedge_t* BSP_HEdge_Create(LineDef* line, LineDef* sourceLine, Vertex* start, Vertex* end,
    Sector* sec, boolean back);

/**
 * Destroys the given half-edge.
 *
 * @param hEdge  Ptr to the half-edge to be destroyed.
 */
void BSP_HEdge_Destroy(bsp_hedge_t* hEdge);

/**
 * Splits the given half-edge at the point (x,y). The new half-edge is returned.
 * The old half-edge is shortened (the original start vertex is unchanged), the
 * new half-edge becomes the cut-off tail (keeping the original end vertex).
 *
 * @note If the half-edge has a twin, it is also split and is inserted into the
 *       same list as the original (and after it), thus all half-edges (except
 *       the one we are currently splitting) must exist on a singly-linked list
 *       somewhere.
 *
 * @note We must update the count values of any superblock that contains the
 *       half-edge (and/or backseg), so that future processing is not messed up
 *       by incorrect counts.
 */
bsp_hedge_t* BSP_HEdge_Split(bsp_hedge_t* oldHEdge, double x, double y);

void BSP_CreateVertexEdgeTip(Vertex* vert, double dx, double dy, bsp_hedge_t* back, bsp_hedge_t* front);

void BSP_DestroyVertexEdgeTip(struct edgetip_s* tip);

/**
 * Check whether a line with the given delta coordinates and beginning at this
 * vertex is open. Returns a sector reference if it's open, or NULL if closed
 * (void space or directly along a linedef).
 */
Sector* BSP_VertexCheckOpen(Vertex* vert, double dx, double dy);

#endif /// LIBDENG_MAP_BSP_HEDGE
