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
 * bsp_level.c: GL-friendly BSP node builder.
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
#include <ctype.h>
#include <math.h>

// MACROS ------------------------------------------------------------------

#define ALLOC_BLKNUM  1024

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int numNormalVert = 0;
int numGLVert = 0;

#define LEVELARRAY(TYPE, BASEVAR, NUMVAR)  \
    TYPE ** BASEVAR = NULL;  \
    int NUMVAR = 0;

LEVELARRAY(mvertex_t,  levVertices,   numVertices)
LEVELARRAY(mlinedef_t, levLinedefs,   numLinedefs)
LEVELARRAY(msidedef_t, levSidedefs,   numSidedefs)
LEVELARRAY(msector_t,  levSectors,    numSectors)

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int nodeCurIndex;

static LEVELARRAY(msubsec_t,  levSubSecs,    numSubSecs)
static LEVELARRAY(mnode_t,    levNodes,      numNodes)

// CODE --------------------------------------------------------------------

#define ALLIGATOR(TYPE, BASEVAR, NUMVAR)  \
{  \
    if((NUMVAR % ALLOC_BLKNUM) == 0)  \
    {  \
        BASEVAR = M_Realloc(BASEVAR, (NUMVAR + ALLOC_BLKNUM) *  \
            sizeof(TYPE *));  \
    }  \
    BASEVAR[NUMVAR] = (TYPE *) M_Calloc(sizeof(TYPE));  \
    NUMVAR += 1;  \
    return BASEVAR[NUMVAR - 1];  \
}

mvertex_t *NewVertex(void)
    ALLIGATOR(mvertex_t, levVertices, numVertices)

mlinedef_t *NewLinedef(void)
    ALLIGATOR(mlinedef_t, levLinedefs, numLinedefs)

msidedef_t *NewSidedef(void)
    ALLIGATOR(msidedef_t, levSidedefs, numSidedefs)

msector_t *NewSector(void)
    ALLIGATOR(msector_t, levSectors, numSectors)

msubsec_t *NewSubsec(void)
    ALLIGATOR(msubsec_t, levSubSecs, numSubSecs)

mnode_t *NewNode(void)
    ALLIGATOR(mnode_t, levNodes, numNodes)

#define FREEMASON(TYPE, BASEVAR, NUMVAR)  \
{  \
    int         i;  \
    for(i = 0; i < NUMVAR; ++i)  \
        M_Free(BASEVAR[i]);  \
    if(BASEVAR)  \
        M_Free(BASEVAR);  \
    BASEVAR = NULL; \
    NUMVAR = 0;  \
}

void FreeVertices(void)
    FREEMASON(mvertex_t, levVertices, numVertices)

void FreeLinedefs(void)
    FREEMASON(mlinedef_t, levLinedefs, numLinedefs)

void FreeSidedefs(void)
    FREEMASON(msidedef_t, levSidedefs, numSidedefs)

void FreeSectors(void)
    FREEMASON(msector_t, levSectors, numSectors)

void FreeSubsecs(void)
    FREEMASON(msubsec_t, levSubSecs, numSubSecs)

void FreeNodes(void)
    FREEMASON(mnode_t, levNodes, numNodes)

#define LOOKERUPPER(BASEVAR, NUMVAR, NAMESTR)  \
{  \
    if(index >= NUMVAR)  \
        Con_Error("No such %s number #%d", NAMESTR, index);  \
    \
    return BASEVAR[index];  \
}

mvertex_t *LookupVertex(int index)
    LOOKERUPPER(levVertices, numVertices, "vertex")

mlinedef_t *LookupLinedef(int index)
    LOOKERUPPER(levLinedefs, numLinedefs, "linedef")

msidedef_t *LookupSidedef(int index)
    LOOKERUPPER(levSidedefs, numSidedefs, "sidedef")

msector_t *LookupSector(int index)
    LOOKERUPPER(levSectors, numSectors, "sector")

msubsec_t *LookupSubsec(int index)
    LOOKERUPPER(levSubSecs, numSubSecs, "subsector")

mnode_t *LookupNode(int index)
    LOOKERUPPER(levNodes, numNodes, "node")

void GetVertices(gamemap_t *map)
{
    uint        i;

    for(i = 0; i < map->numvertexes; ++i)
    {
        vertex_t   *v = &map->vertexes[i];
        mvertex_t *vert = NewVertex();

        vert->index = i;
        vert->V_pos[VX] = (double) v->V_pos[VX];
        vert->V_pos[VY] = (double) v->V_pos[VY];
    }

    numNormalVert = numVertices;
    numGLVert = 0;
}

void GetSectors(gamemap_t *map)
{
    uint        i, count = -1;

    for(i = 0; i < map->numsectors; ++i)
    {
        msector_t *sector = NewSector();

        // Sector indices never change.
        sector->index = i;
        sector->warnedFacing = -1;
    }
}

void GetSidedefs(gamemap_t *map)
{
    uint        i;

    for(i = 0; i < map->numsides; ++i)
    {
        side_t *sid = &map->sides[i];
        msidedef_t *side = NewSidedef();

        // Sidedef indices never change.
        side->index = i;
        side->sector =
            (sid->sector? LookupSector(sid->sector - map->sectors) : NULL);
    }
}

void GetLinedefs(gamemap_t *map)
{
    uint        i;

    for(i = 0; i < map->numlines; ++i)
    {
        line_t     *l = &map->lines[i];
        mlinedef_t *line = NewLinedef();
        mvertex_t *start = LookupVertex(l->v[0] - map->vertexes);
        mvertex_t *end   = LookupVertex(l->v[1] - map->vertexes);

        start->refCount++;
        end->refCount++;

        line->index = i;
        line->v[0] = start;
        line->v[1] = end;
        line->mlFlags = 0;

        // Check for zero-length line.
        if((fabs(start->V_pos[VX] - end->V_pos[VX]) < DIST_EPSILON) &&
           (fabs(start->V_pos[VY] - end->V_pos[VY]) < DIST_EPSILON))
            line->mlFlags |= MLF_ZEROLENGTH;

        if(l->L_frontside)
            line->sides[FRONT] = LookupSidedef(l->L_frontside - map->sides);
        else
            line->sides[FRONT] = NULL;

        if(l->L_backside)
            line->sides[BACK] = LookupSidedef(l->L_backside - map->sides);
        else
            line->sides[BACK] = NULL;

        if(line->sides[BACK] && line->sides[FRONT])
        {
            line->mlFlags |= MLF_TWOSIDED;

            if(line->sides[BACK]->sector == line->sides[FRONT]->sector)
                line->mlFlags |= MLF_SELFREF;
        }
    }
}

static void hardenVertexes(gamemap_t *map)
{
    uint        i;

    Z_Free(map->vertexes);

    map->numvertexes = numVertices;
    map->vertexes = Z_Calloc(map->numvertexes * sizeof(vertex_t),
                             PU_LEVELSTATIC, 0);
    for(i = 0; i < map->numvertexes; ++i)
    {
        vertex_t   *vtx = &map->vertexes[i];
        mvertex_t   *vert = levVertices[i];

        vtx->header.type = DMU_VERTEX;
        vtx->numlineowners = 0;
        vtx->lineowners = NULL;
        vtx->anchored = false;

        //// \fixme Add some rounding.
        vtx->V_pos[VX] = (float) vert->V_pos[VX];
        vtx->V_pos[VY] = (float) vert->V_pos[VY];
    }
}

static void hardenSideSegList(gamemap_t *map, side_t *side, seg_t *seg,
                              hedge_t *hEdge)
{
    uint        count;
    hedge_t     *first, *other;

    // Have we already processed this side?
    if(side->segs)
        return;

    // Find the first seg.
    first = hEdge;
    while(first->prevOnSide)
        first = first->prevOnSide;

    // Count the segs for this side.
    count = 0;
    other = first;
    while(other)
    {
        other = other->nextOnSide;
        count++;
    }

    // Allocate the final side seg table.
    side->segcount = count;
    side->segs =
        Z_Malloc(sizeof(seg_t*) * (side->segcount + 1), PU_LEVELSTATIC, 0);

    count = 0;
    other = first;
    while(other)
    {
        side->segs[count++] = &map->segs[other->index];
        other = other->nextOnSide;
    }
    side->segs[count] = NULL; // Terminate.
}

static void buildSegsFromHEdges(gamemap_t *map)
{
    uint        i;

    BSP_SortHEdgesByIndex();

    map->numsegs = BSP_GetNumHEdges();

    map->segs = Z_Calloc(map->numsegs * sizeof(seg_t), PU_LEVELSTATIC, 0);
    for(i = 0; i < map->numsegs; ++i)
    {
        uint        k;
        seg_t      *seg = &map->segs[i];
        hedge_t    *hEdge = LookupHEdge(i);

        seg->header.type = DMU_SEG;

        seg->SG_v1 = &map->vertexes[hEdge->v[0]->index];
        seg->SG_v2 = &map->vertexes[hEdge->v[1]->index];

        seg->side  = hEdge->side;
        if(hEdge->linedef)
            seg->linedef = &map->lines[hEdge->linedef->index];
        if(hEdge->twin)
            seg->backseg = &map->segs[hEdge->twin->index];

        seg->flags = 0;
        if(seg->linedef)
        {
            line_t     *ldef = seg->linedef;
            vertex_t   *vtx = seg->linedef->L_v(seg->side);

            seg->SG_frontsector = ldef->L_side(seg->side)->sector;

            if((ldef->mapflags & ML_TWOSIDED) && ldef->L_side(seg->side ^ 1))
            {
                seg->SG_backsector = ldef->L_side(seg->side ^ 1)->sector;
            }
            else
            {
                ldef->mapflags &= ~ML_TWOSIDED;
                seg->SG_backsector = 0;
            }

            seg->sidedef = ldef->L_side(seg->side);
            seg->offset = P_AccurateDistancef(seg->SG_v1pos[VX] - vtx->V_pos[VX],
                                              seg->SG_v1pos[VY] - vtx->V_pos[VY]);
        }
        else
        {
            seg->linedef = NULL;
            seg->SG_frontsector = NULL;
            seg->SG_backsector = NULL;
        }

        if(seg->sidedef)
            hardenSideSegList(map, seg->sidedef, seg, hEdge);

        seg->angle =
            bamsAtan2((int) (seg->SG_v2pos[VY] - seg->SG_v1pos[VY]),
                      (int) (seg->SG_v2pos[VX] - seg->SG_v1pos[VX])) << FRACBITS;

        // Calculate the length of the segment. We need this for
        // the texture coordinates. -jk
        seg->length = P_AccurateDistancef(seg->SG_v2pos[VX] - seg->SG_v1pos[VX],
                                          seg->SG_v2pos[VY] - seg->SG_v1pos[VY]);

        if(seg->length == 0)
            seg->length = 0.01f; // Hmm...

        // Calculate the surface normals
        // Front first
        if(seg->sidedef)
        {
            surface_t *surface = &seg->sidedef->SW_topsurface;

            surface->normal[VY] = (seg->SG_v1pos[VX] - seg->SG_v2pos[VX]) / seg->length;
            surface->normal[VX] = (seg->SG_v2pos[VY] - seg->SG_v1pos[VY]) / seg->length;
            surface->normal[VZ] = 0;

            // All surfaces of a sidedef have the same normal.
            memcpy(seg->sidedef->SW_middlenormal, surface->normal, sizeof(surface->normal));
            memcpy(seg->sidedef->SW_bottomnormal, surface->normal, sizeof(surface->normal));
        }

        // Initialize the bias illumination data.
        for(k = 0; k < 4; ++k)
        {
            uint        j;

            for(j = 0; j < 3; ++j)
            {
                uint        n;
                seg->illum[j][k].flags = VIF_STILL_UNSEEN;

                for(n = 0; n < MAX_BIAS_AFFECTED; ++n)
                {
                    seg->illum[j][k].casted[n].source = -1;
                }
            }
        }
    }
}

static void hardenSSecSegList(gamemap_t *map, subsector_t *ssec,
                              hedge_t *list, uint segCount)
{
    uint        i;
    hedge_t     *cur;
    seg_t    **segs;

    segs = Z_Malloc(sizeof(seg_t*) * (segCount + 1), PU_LEVELSTATIC, 0);

    for(cur = list, i = 0; cur; cur = cur->next, ++i)
        segs[i] = &map->segs[cur->index];
    segs[segCount] = NULL; // Terminate.

    if(i != segCount)
        Con_Error("hardenSSecSegList: Miscounted?");

    ssec->segs = segs;
}

static void hardenSubSectors(gamemap_t *map)
{
    uint        i;

    map->numsubsectors = numSubSecs;
    map->subsectors = Z_Calloc(map->numsubsectors * sizeof(subsector_t),
                               PU_LEVELSTATIC, 0);
    for(i = 0; i < map->numsubsectors; ++i)
    {
        subsector_t *ssec = &map->subsectors[i];
        msubsec_t  *mssec = levSubSecs[i];

        ssec->header.type = DMU_SUBSECTOR;
        ssec->group = 0;

        ssec->segcount = mssec->hEdgeCount;
        hardenSSecSegList(map, ssec, mssec->hEdges, mssec->hEdgeCount);
    }
}

static void hardenNode(gamemap_t *map, mnode_t *mnode)
{
    node_t     *node;
    child_t    *right = &mnode->children[RIGHT];
    child_t    *left = &mnode->children[LEFT];

    if(right->node)
        hardenNode(map, right->node);

    if(left->node)
        hardenNode(map, left->node);

    node = &map->nodes[mnode->index = nodeCurIndex++];
    node->header.type = DMU_NODE;

    node->x = mnode->x;
    node->y = mnode->y;
    node->dx = mnode->dX / (mnode->tooLong? 2 : 1);
    node->dy = mnode->dY / (mnode->tooLong? 2 : 1);

    node->bbox[RIGHT][BOXTOP]    = mnode->bBox[RIGHT][BOXTOP];
    node->bbox[RIGHT][BOXBOTTOM] = mnode->bBox[RIGHT][BOXBOTTOM];
    node->bbox[RIGHT][BOXLEFT]   = mnode->bBox[RIGHT][BOXLEFT];
    node->bbox[RIGHT][BOXRIGHT]  = mnode->bBox[RIGHT][BOXRIGHT];

    node->bbox[LEFT][BOXTOP]     = mnode->bBox[LEFT][BOXTOP];
    node->bbox[LEFT][BOXBOTTOM]  = mnode->bBox[LEFT][BOXBOTTOM];
    node->bbox[LEFT][BOXLEFT]    = mnode->bBox[LEFT][BOXLEFT];
    node->bbox[LEFT][BOXRIGHT]   = mnode->bBox[LEFT][BOXRIGHT];

    if(right->node)
        node->children[RIGHT] = right->node->index;
    else if(right->subSec)
        node->children[RIGHT] = right->subSec->index | NF_SUBSECTOR;

    if(left->node)
        node->children[LEFT]  = left->node->index;
    else if(left->subSec)
        node->children[LEFT]  = left->subSec->index | NF_SUBSECTOR;
}

static void hardenNodes(gamemap_t *map, mnode_t *root)
{
    nodeCurIndex = 0;

    map->numnodes = numNodes;
    map->nodes = Z_Calloc(map->numnodes * sizeof(node_t),
                          PU_LEVELSTATIC, 0);
    if(root)
    {
        hardenNode(map, root);
    }
}

/**
 * Perform cleanup on the loaded map data, removing duplicate vertexes,
 * pruning unused sectors etc, etc... 
 */
void CleanMap(gamemap_t *map)
{
    BSP_DetectDuplicateVertices();
    //BSP_PruneRedundantMapData(PRUNE_ALL);
}

/**
 * \note Order here is critical!
 */
void LoadMap(gamemap_t *map)
{
    GetVertices(map);
    GetSectors(map);
    GetSidedefs(map);
    GetLinedefs(map);
}

void FreeMap(void)
{
    FreeVertices();
    FreeSidedefs();
    FreeLinedefs();
    FreeSectors();
    BSP_FreeHEdges();
    FreeSubsecs();
    FreeNodes();
    BSP_FreeEdgeTips();
}

static void updateLinedefs(gamemap_t *map)
{
    uint        i;

    for(i = 0; i < map->numlines; ++i)
    {
        line_t     *l = &map->lines[i];
        mlinedef_t *ml = levLinedefs[i];

        l->v[0] = &map->vertexes[ml->v[0]->index];
        l->v[1] = &map->vertexes[ml->v[1]->index];
    }
}

void SaveMap(gamemap_t *map, mnode_t *rootNode)
{
    hardenVertexes(map);
    updateLinedefs(map);
    buildSegsFromHEdges(map);
    hardenSubSectors(map);
    hardenNodes(map, rootNode);
}
