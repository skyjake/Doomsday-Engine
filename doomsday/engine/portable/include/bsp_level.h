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
 * GL-friendly BSP node builder.
 * bsp_level.h: Level structure read/write functions.
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 */

#ifndef __BSP_LEVEL_H__
#define __BSP_LEVEL_H__

struct node_s;
struct sector_s;
struct superblock_s;

#define V_pos					v.pos

typedef struct dvertex_s {
    double      pos[2];
} dvertex_t;

typedef struct mvertex_s {
    // Vertex index. Always valid after loading and pruning of unused
    // vertices has occurred.
    int         index;

    // Reference count. When building normal node info, unused vertices
    // will be pruned.
    int         refCount;

    // Usually NULL, unless this vertex occupies the same location as a
    // previous vertex. Only used during the pruning phase.
    struct mvertex_s *equiv;

    struct edgetip_s *tipSet; // Set of wall_tips.

// Final data.
    dvertex_t   v;
} mvertex_t;

typedef struct msector_s {
    // Sector index. Always valid after loading & pruning.
    int         index;

    // Suppress superfluous mini warnings.
    int         warnedFacing;
    boolean     warnedUnclosed;
} msector_t;

#define FRONT                   0
#define BACK                    1

typedef struct msidedef_s {
    msector_t  *sector; // Adjacent sector. Can be NULL (invalid sidedef).

    // Sidedef index. Always valid after loading & pruning.
    int         index;
} msidedef_t;

#define MLF_TWOSIDED            0x1 // Line is marked two-sided.
#define MLF_ZEROLENGTH          0x2 // Zero length (line should be totally ignored).
#define MLF_SELFREF             0x4 // Sector is the same on both sides.

typedef struct mlinedef_s {
    mvertex_t  *v[2]; // [Start, End] vertexes.
    msidedef_t *sides[2]; // [FRONT, BACK] sidedefs.

    int         mlFlags; // MLF_* flags.

    // One-sided linedef used for a special effect (windows).
    // The value refers to the opposite sector on the back side.
    msector_t  *windowEffect;

    // Normally NULL, except when this linedef directly overlaps an earlier
    // one (a rarely-used trick to create higher mid-masked textures).
    // No segs should be created for these overlapping linedefs.
    struct mlinedef_s *overlap;

    // Linedef index. Always valid after loading & pruning of zero
    // length lines has occurred.
    int         index;
} mlinedef_t;

typedef struct msubsec_s {
    // Approximate middle point.
    double      midPoint[2];

    // Subsector index. Always valid, set when the subsector is
    // initially created.
    int         index;

    int         hEdgeCount;
    struct hedge_s *hEdges; // Head ptr to a list of half-edges in this subsector.
} msubsec_t;

// Level data arrays:
extern int numVertices;
extern int numLinedefs;
extern int numSidedefs;
extern int numSectors;
extern int numSubSecs;

extern int numNormalVert;
extern int numGLVert;

// Allocation routines:
mvertex_t  *NewVertex(void);
mlinedef_t *NewLinedef(void);
msidedef_t *NewSidedef(void);
msector_t  *NewSector(void);
msubsec_t  *NewSubsec(void);

// Lookup routines:
mvertex_t  *LookupVertex(int index);
mlinedef_t *LookupLinedef(int index);
msidedef_t *LookupSidedef(int index);
msector_t  *LookupSector(int index);
msubsec_t  *LookupSubsec(int index);

// Load all level data for the current level.
void        LoadMap(struct gamemap_s *map);
void        CleanMap(struct gamemap_s *map);

// Free all level data.
void        FreeMap(void);
#endif
