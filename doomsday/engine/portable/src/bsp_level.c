/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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
 * bsp_level.c:
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_bsp.h"
#include "de_misc.h"
#include "de_play.h"
#include "de_edit.h"
#include "de_refresh.h"

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

static int nodeCurIndex;

// CODE --------------------------------------------------------------------

static void hardenSideSegList(gamemap_t *map, side_t *side, seg_t *seg,
                              hedge_t *hEdge)
{
    uint                count;
    hedge_t            *first, *other;

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
    side->segCount = count;
    side->segs =
        Z_Malloc(sizeof(seg_t*) * (side->segCount + 1), PU_LEVELSTATIC, 0);

    count = 0;
    other = first;
    while(other)
    {
        side->segs[count++] = &map->segs[other->index];
        other = other->nextOnSide;
    }
    side->segs[count] = NULL; // Terminate.
}

static void buildSegsFromHEdges(gamemap_t *dest)
{
    uint        i;

    BSP_SortHEdgesByIndex();

    dest->numSegs = BSP_GetNumHEdges();

    dest->segs = Z_Calloc(dest->numSegs * sizeof(seg_t), PU_LEVELSTATIC, 0);
    for(i = 0; i < dest->numSegs; ++i)
    {
        seg_t      *seg = &dest->segs[i];
        hedge_t    *hEdge = LookupHEdge(i);

        seg->header.type = DMU_SEG;

        seg->SG_v1 = &dest->vertexes[hEdge->v[0]->buildData.index - 1];
        seg->SG_v2 = &dest->vertexes[hEdge->v[1]->buildData.index - 1];

        seg->side  = hEdge->side;
        if(hEdge->lineDef)
            seg->lineDef = &dest->lines[hEdge->lineDef->buildData.index - 1];
        if(hEdge->twin)
            seg->backSeg = &dest->segs[hEdge->twin->index];

        seg->flags = 0;
        if(seg->lineDef)
        {
            line_t     *ldef = seg->lineDef;
            vertex_t   *vtx = seg->lineDef->L_v(seg->side);

            seg->SG_frontsector = ldef->L_side(seg->side)->sector;

            if((ldef->mapFlags & ML_TWOSIDED) && ldef->L_side(seg->side ^ 1))
            {
                seg->SG_backsector = ldef->L_side(seg->side ^ 1)->sector;
            }
            else
            {
                ldef->mapFlags &= ~ML_TWOSIDED;
                seg->SG_backsector = 0;
            }

            seg->sideDef = ldef->L_side(seg->side);
            seg->offset = P_AccurateDistance(seg->SG_v1pos[VX] - vtx->V_pos[VX],
                                             seg->SG_v1pos[VY] - vtx->V_pos[VY]);
        }
        else
        {
            seg->lineDef = NULL;
            seg->SG_frontsector = NULL;
            seg->SG_backsector = NULL;
        }

        if(seg->sideDef)
            hardenSideSegList(dest, seg->sideDef, seg, hEdge);

        seg->angle =
            bamsAtan2((int) (seg->SG_v2pos[VY] - seg->SG_v1pos[VY]),
                      (int) (seg->SG_v2pos[VX] - seg->SG_v1pos[VX])) << FRACBITS;

        // Calculate the length of the segment. We need this for
        // the texture coordinates. -jk
        seg->length = P_AccurateDistance(seg->SG_v2pos[VX] - seg->SG_v1pos[VX],
                                         seg->SG_v2pos[VY] - seg->SG_v1pos[VY]);

        if(seg->length == 0)
            seg->length = 0.01f; // Hmm...

        // Calculate the surface normals
        // Front first
        if(seg->sideDef)
        {
            surface_t *surface = &seg->sideDef->SW_topsurface;

            surface->normal[VY] = (seg->SG_v1pos[VX] - seg->SG_v2pos[VX]) / seg->length;
            surface->normal[VX] = (seg->SG_v2pos[VY] - seg->SG_v1pos[VY]) / seg->length;
            surface->normal[VZ] = 0;

            // All surfaces of a sidedef have the same normal.
            memcpy(seg->sideDef->SW_middlenormal, surface->normal, sizeof(surface->normal));
            memcpy(seg->sideDef->SW_bottomnormal, surface->normal, sizeof(surface->normal));
        }
    }
}

static void hardenSSecSegList(gamemap_t *dest, subsector_t *ssec,
                              hedge_t *list, size_t segCount)
{
    size_t              i;
    hedge_t            *cur;
    seg_t             **segs;

    segs = Z_Malloc(sizeof(seg_t*) * (segCount + 1), PU_LEVELSTATIC, 0);

    for(cur = list, i = 0; cur; cur = cur->next, ++i)
        segs[i] = &dest->segs[cur->index];
    segs[segCount] = NULL; // Terminate.

    if(i != segCount)
        Con_Error("hardenSSecSegList: Miscounted?");

    ssec->segs = segs;
}

static void hardenSubsector(gamemap_t *map, subsector_t *dest, const subsector_t *src)
{
    memcpy(dest, src, sizeof(subsector_t));
    dest->segCount = (uint) src->buildData.hEdgeCount;
    dest->sector = (src->sector? &map->sectors[src->sector->buildData.index-1] : NULL);
    dest->shadows = NULL;
    dest->planes = NULL;
    dest->vertices = NULL;
    dest->group = 0;
    hardenSSecSegList(map, dest, src->buildData.hEdges, src->buildData.hEdgeCount);
}

typedef struct {
    gamemap_t      *dest;
    uint            ssecCurIndex;
    uint            nodeCurIndex;
} hardenbspparams_t;

static boolean C_DECL hardenNode(binarytree_t *tree, void *data)
{
    binarytree_t       *right, *left;
    bspnodedata_t      *nodeData;
    hardenbspparams_t  *params;
    node_t             *node;

    if(BinaryTree_IsLeaf(tree))
        return true; // Continue iteration.

    nodeData = BinaryTree_GetData(tree);
    params = (hardenbspparams_t*) data;

    node = &params->dest->nodes[nodeData->index = params->nodeCurIndex++];
    node->header.type = DMU_NODE;

    node->x = nodeData->x;
    node->y = nodeData->y;
    node->dX = nodeData->dX / (nodeData->tooLong? 2 : 1);
    node->dY = nodeData->dY / (nodeData->tooLong? 2 : 1);

    node->bBox[RIGHT][BOXTOP]    = nodeData->bBox[RIGHT][BOXTOP];
    node->bBox[RIGHT][BOXBOTTOM] = nodeData->bBox[RIGHT][BOXBOTTOM];
    node->bBox[RIGHT][BOXLEFT]   = nodeData->bBox[RIGHT][BOXLEFT];
    node->bBox[RIGHT][BOXRIGHT]  = nodeData->bBox[RIGHT][BOXRIGHT];

    node->bBox[LEFT][BOXTOP]     = nodeData->bBox[LEFT][BOXTOP];
    node->bBox[LEFT][BOXBOTTOM]  = nodeData->bBox[LEFT][BOXBOTTOM];
    node->bBox[LEFT][BOXLEFT]    = nodeData->bBox[LEFT][BOXLEFT];
    node->bBox[LEFT][BOXRIGHT]   = nodeData->bBox[LEFT][BOXRIGHT];

    right = BinaryTree_GetChild(tree, RIGHT);
    if(right)
    {
        if(BinaryTree_IsLeaf(right))
        {
            subsector_t    *ssec = (subsector_t*) BinaryTree_GetData(right);
            uint            idx = params->ssecCurIndex++;

            node->children[RIGHT] = idx | NF_SUBSECTOR;
            hardenSubsector(params->dest, &params->dest->subsectors[idx], ssec);
        }
        else
        {
            bspnodedata_t *data = (bspnodedata_t*) BinaryTree_GetData(right);
            node->children[RIGHT] = data->index;
        }
    }

    left = BinaryTree_GetChild(tree, LEFT);
    if(left)
    {
        if(BinaryTree_IsLeaf(left))
        {
            subsector_t    *ssec = (subsector_t*) BinaryTree_GetData(left);
            uint            idx = params->ssecCurIndex++;

            node->children[LEFT] = idx | NF_SUBSECTOR;
            hardenSubsector(params->dest, &params->dest->subsectors[idx], ssec);
        }
        else
        {
            bspnodedata_t *data = (bspnodedata_t*) BinaryTree_GetData(left);
            node->children[LEFT]  = data->index;
        }
    }

    return true; // Continue iteration.
}

static boolean C_DECL countNode(binarytree_t *tree, void *data)
{
    if(!BinaryTree_IsLeaf(tree))
        (*((uint*) data))++;

    return true; // Continue iteration.
}

static boolean C_DECL countSSec(binarytree_t *tree, void *data)
{
    if(BinaryTree_IsLeaf(tree))
        (*((uint*) data))++;

    return true; // Continue iteration.
}

static void hardenBSP(gamemap_t *dest, editmap_t *src)
{
    dest->numNodes = 0;
    BinaryTree_PostOrder(src->rootNode, countNode, &dest->numNodes);
    dest->nodes =
        Z_Calloc(dest->numNodes * sizeof(node_t), PU_LEVELSTATIC, 0);

    dest->numSubsectors = 0;
    BinaryTree_PostOrder(src->rootNode, countSSec, &dest->numSubsectors);
    dest->subsectors =
        Z_Calloc(dest->numSubsectors * sizeof(subsector_t), PU_LEVELSTATIC, 0);

    if(src->rootNode)
    {
        hardenbspparams_t params;

        params.dest = dest;
        params.ssecCurIndex = 0;
        params.nodeCurIndex = 0;

        BinaryTree_PostOrder(src->rootNode, hardenNode, &params);
    }
}

void BSP_InitForNodeBuild(editmap_t *map)
{
    uint            i;

    for(i = 0; i < map->numLines; ++i)
    {
        line_t     *l = map->lines[i];
        vertex_t   *start = l->v[0];
        vertex_t   *end   = l->v[1];

        start->buildData.refCount++;
        end->buildData.refCount++;

        l->buildData.mlFlags = 0;

        // Check for zero-length line.
        if((fabs(start->buildData.pos[VX] - end->buildData.pos[VX]) < DIST_EPSILON) &&
           (fabs(start->buildData.pos[VY] - end->buildData.pos[VY]) < DIST_EPSILON))
            l->buildData.mlFlags |= MLF_ZEROLENGTH;

        if(l->flags & LINEF_POLYOBJ)
            l->buildData.mlFlags |= MLF_POLYOBJ;

        if(l->sides[BACK] && l->sides[FRONT])
        {
            l->buildData.mlFlags |= MLF_TWOSIDED;

            if(l->sides[BACK]->sector == l->sides[FRONT]->sector)
                l->buildData.mlFlags |= MLF_SELFREF;
        }
    }
}

void FreeMap(void)
{
    BSP_FreeHEdges();
    BSP_FreeEdgeTips();
}

static void hardenLinedefs(gamemap_t *dest, editmap_t *src)
{
    uint        i;

    dest->numLines = src->numLines;
    dest->lines = Z_Calloc(dest->numLines * sizeof(line_t), PU_LEVELSTATIC, 0);

    for(i = 0; i < dest->numLines; ++i)
    {
        line_t     *destL = &dest->lines[i];
        line_t     *srcL = src->lines[i];

        memcpy(destL, srcL, sizeof(*destL));
        destL->v[0] = &dest->vertexes[srcL->v[0]->buildData.index - 1];
        destL->v[1] = &dest->vertexes[srcL->v[1]->buildData.index - 1];
        //// \todo We shouldn't still have lines with missing fronts but...
        destL->L_frontside = (srcL->L_frontside?
            &dest->sides[srcL->L_frontside->buildData.index - 1] : NULL);
        destL->L_backside = (srcL->L_backside?
            &dest->sides[srcL->L_backside->buildData.index - 1] : NULL);
    }
}

static void hardenVertexes(gamemap_t *dest, editmap_t *src)
{
    uint            i;

    dest->numVertexes = src->numVertexes;
    dest->vertexes =
        Z_Calloc(dest->numVertexes * sizeof(vertex_t), PU_LEVELSTATIC, 0);

    for(i = 0; i < dest->numVertexes; ++i)
    {
        vertex_t   *destV = &dest->vertexes[i];
        vertex_t   *srcV = src->vertexes[i];

        destV->header.type = DMU_VERTEX;
        destV->numLineOwners = 0;
        destV->lineOwners = NULL;
        destV->anchored = false;

        //// \fixme Add some rounding.
        destV->V_pos[VX] = (float) srcV->buildData.pos[VX];
        destV->V_pos[VY] = (float) srcV->buildData.pos[VY];
    }
}

static void hardenSidedefs(gamemap_t *dest, editmap_t *src)
{
    uint            i;

    dest->numSides = src->numSides;
    dest->sides = Z_Malloc(dest->numSides * sizeof(side_t), PU_LEVELSTATIC, 0);

    for(i = 0; i < dest->numSides; ++i)
    {
        side_t         *destS = &dest->sides[i];
        side_t         *srcS = src->sides[i];

        memcpy(destS, srcS, sizeof(*destS));
        destS->sector = &dest->sectors[srcS->sector->buildData.index - 1];
    }
}

static void hardenSectors(gamemap_t *dest, editmap_t *src)
{
    uint            i;

    dest->numSectors = src->numSectors;
    dest->sectors = Z_Malloc(dest->numSectors * sizeof(sector_t), PU_LEVELSTATIC, 0);

    for(i = 0; i < dest->numSectors; ++i)
    {
        sector_t           *destS = &dest->sectors[i];
        sector_t           *srcS = src->sectors[i];
        plane_t            *pln;

        memcpy(destS, srcS, sizeof(*destS));
        destS->planeCount = 0;
        destS->planes = NULL;

        pln = R_NewPlaneForSector(destS);
        memcpy(pln, srcS->planes[PLN_FLOOR], sizeof(*pln));
        pln->sector = destS;

        pln = R_NewPlaneForSector(destS);
        memcpy(pln, srcS->planes[PLN_CEILING], sizeof(*pln));
        pln->sector = destS;
    }
}

static void hardenPolyobjs(gamemap_t *dest, editmap_t *src)
{
    uint            i;

    if(src->numPolyobjs == 0)
    {
        dest->numPolyobjs = 0;
        dest->polyobjs = NULL;
        return;
    }

    dest->numPolyobjs = src->numPolyobjs;
    dest->polyobjs = Z_Malloc((dest->numPolyobjs+1) * sizeof(polyobj_t*),
                              PU_LEVEL, 0);

    for(i = 0; i < dest->numPolyobjs; ++i)
    {
        uint            j;
        polyobj_t      *destP, *srcP = src->polyobjs[i];
        seg_t          *segs;

        destP = Z_Calloc(sizeof(*destP), PU_LEVEL, 0);
        destP->header.type = DMU_POLYOBJ;
        destP->idx = i;
        destP->crush = srcP->crush;
        destP->tag = srcP->tag;
        destP->seqType = srcP->seqType;
        destP->startSpot.pos[VX] = srcP->startSpot.pos[VX];
        destP->startSpot.pos[VY] = srcP->startSpot.pos[VY];

        destP->numSegs = srcP->buildData.lineCount;

        destP->originalPts =
            Z_Malloc(destP->numSegs * sizeof(fvertex_t), PU_LEVEL, 0);
        destP->prevPts =
            Z_Malloc(destP->numSegs * sizeof(fvertex_t), PU_LEVEL, 0);

        // Create a seg for each line of this polyobj.
        segs = Z_Calloc(sizeof(seg_t) * srcP->buildData.lineCount, PU_LEVEL, 0);
        destP->segs = Z_Malloc(sizeof(seg_t*) * (srcP->buildData.lineCount+1), PU_LEVEL, 0);
        for(j = 0; j < srcP->buildData.lineCount; ++j)
        {
            line_t         *line = &dest->lines[srcP->buildData.lines[j]->buildData.index - 1];
            seg_t          *seg = &segs[j];
            float           dx, dy;
            uint            k;

            // This line is part of a polyobj.
            line->flags |= LINEF_POLYOBJ;

            seg->header.type = DMU_SEG;
            seg->lineDef = line;
            seg->SG_v1 = line->L_v1;
            seg->SG_v2 = line->L_v2;
            dx = line->L_v2pos[VX] - line->L_v1pos[VX];
            dy = line->L_v2pos[VY] - line->L_v1pos[VY];
            seg->length = P_AccurateDistance(dx, dy);
            seg->backSeg = NULL;
            seg->sideDef = line->L_frontside;
            seg->subsector = NULL;
            seg->SG_frontsector = line->L_frontsector;
            seg->SG_backsector = NULL;
            seg->flags |= SEGF_POLYOBJ;

            // Initialize the bias illumination data.
            for(k = 0; k < 4; ++k)
            {
                uint        l;

                for(l = 0; l < 3; ++l)
                {
                    uint        m;
                    seg->illum[l][k].flags = VIF_STILL_UNSEEN;

                    for(m = 0; m < MAX_BIAS_AFFECTED; ++m)
                    {
                        seg->illum[l][k].casted[m].source = -1;
                    }
                }
            }

            // The original Pts are based off the anchor Pt, and are unique
            // to each seg, not each linedef.
            destP->originalPts[j].pos[VX] =
                seg->SG_v1pos[VX] - destP->startSpot.pos[VX];
            destP->originalPts[j].pos[VY] =
                seg->SG_v1pos[VY] - destP->startSpot.pos[VY];

            destP->segs[j] = seg;
        }
        destP->segs[j] = NULL; // Terminate.

        // Add this polyobj to the global list.
        dest->polyobjs[i] = destP;
    }
    dest->polyobjs[i] = NULL; // Terminate.
}

void SaveMap(gamemap_t *dest, editmap_t *src)
{
    uint            startTime = Sys_GetRealTime();

    hardenVertexes(dest, src);
    hardenSectors(dest, src);
    hardenSidedefs(dest, src);
    hardenLinedefs(dest, src);
    buildSegsFromHEdges(dest);
    hardenBSP(dest, src);
    hardenPolyobjs(dest, src);

    // How much time did we spend?
    VERBOSE(Con_Message
            ("SaveMap: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));
}
