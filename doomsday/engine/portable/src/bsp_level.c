/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
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

static void buildSegsFromHEdges(gamemap_t *dest)
{
    uint        i;

    BSP_SortHEdgesByIndex();

    dest->numsegs = BSP_GetNumHEdges();

    dest->segs = Z_Calloc(dest->numsegs * sizeof(seg_t), PU_LEVELSTATIC, 0);
    for(i = 0; i < dest->numsegs; ++i)
    {
        seg_t      *seg = &dest->segs[i];
        hedge_t    *hEdge = LookupHEdge(i);

        seg->header.type = DMU_SEG;

        seg->SG_v1 = &dest->vertexes[hEdge->v[0]->buildData.index - 1];
        seg->SG_v2 = &dest->vertexes[hEdge->v[1]->buildData.index - 1];

        seg->side  = hEdge->side;
        if(hEdge->linedef)
            seg->linedef = &dest->lines[hEdge->linedef->buildData.index - 1];
        if(hEdge->twin)
            seg->backseg = &dest->segs[hEdge->twin->index];

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
            seg->offset = P_AccurateDistance(seg->SG_v1pos[VX] - vtx->V_pos[VX],
                                             seg->SG_v1pos[VY] - vtx->V_pos[VY]);
        }
        else
        {
            seg->linedef = NULL;
            seg->SG_frontsector = NULL;
            seg->SG_backsector = NULL;
        }

        if(seg->sidedef)
            hardenSideSegList(dest, seg->sidedef, seg, hEdge);

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
    }
}

static void hardenSSecSegList(gamemap_t *dest, subsector_t *ssec,
                              hedge_t *list, uint segCount)
{
    uint        i;
    hedge_t    *cur;
    seg_t    **segs;

    segs = Z_Malloc(sizeof(seg_t*) * (segCount + 1), PU_LEVELSTATIC, 0);

    for(cur = list, i = 0; cur; cur = cur->next, ++i)
        segs[i] = &dest->segs[cur->index];
    segs[segCount] = NULL; // Terminate.

    if(i != segCount)
        Con_Error("hardenSSecSegList: Miscounted?");

    ssec->segs = segs;
}

static void hardenSubSectors(gamemap_t *dest, editmap_t *src)
{
    uint        i;

    dest->numsubsectors = src->numsubsectors;
    dest->subsectors = Z_Malloc(dest->numsubsectors * sizeof(subsector_t),
                               PU_LEVELSTATIC, 0);
    for(i = 0; i < dest->numsubsectors; ++i)
    {
        subsector_t *destS = &dest->subsectors[i];
        subsector_t *srcS = src->subsectors[i];

        memcpy(destS, srcS, sizeof(*destS));
        destS->segcount = srcS->buildData.hEdgeCount;
        destS->sector = (srcS->sector? &dest->sectors[srcS->sector->buildData.index-1] : NULL);
        destS->shadows = NULL;
        destS->planes = NULL;
        destS->vertices = NULL;
        destS->group = 0;
        hardenSSecSegList(dest, destS, srcS->buildData.hEdges, srcS->buildData.hEdgeCount);
    }
}

static void hardenNode(gamemap_t *dest, node_t *mnode)
{
    node_t     *node;
    child_t    *right = &mnode->buildData.children[RIGHT];
    child_t    *left = &mnode->buildData.children[LEFT];

    if(right->node)
        hardenNode(dest, right->node);

    if(left->node)
        hardenNode(dest, left->node);

    node = &dest->nodes[mnode->buildData.index = nodeCurIndex++];
    node->header.type = DMU_NODE;

    node->x = mnode->x;
    node->y = mnode->y;
    node->dx = mnode->dx / (mnode->buildData.tooLong? 2 : 1);
    node->dy = mnode->dy / (mnode->buildData.tooLong? 2 : 1);

    node->bbox[RIGHT][BOXTOP]    = mnode->bbox[RIGHT][BOXTOP];
    node->bbox[RIGHT][BOXBOTTOM] = mnode->bbox[RIGHT][BOXBOTTOM];
    node->bbox[RIGHT][BOXLEFT]   = mnode->bbox[RIGHT][BOXLEFT];
    node->bbox[RIGHT][BOXRIGHT]  = mnode->bbox[RIGHT][BOXRIGHT];

    node->bbox[LEFT][BOXTOP]     = mnode->bbox[LEFT][BOXTOP];
    node->bbox[LEFT][BOXBOTTOM]  = mnode->bbox[LEFT][BOXBOTTOM];
    node->bbox[LEFT][BOXLEFT]    = mnode->bbox[LEFT][BOXLEFT];
    node->bbox[LEFT][BOXRIGHT]   = mnode->bbox[LEFT][BOXRIGHT];

    if(right->node)
        node->children[RIGHT] = right->node->buildData.index;
    else if(right->subSec)
        node->children[RIGHT] = (right->subSec->buildData.index - 1) | NF_SUBSECTOR;

    if(left->node)
        node->children[LEFT]  = left->node->buildData.index;
    else if(left->subSec)
        node->children[LEFT]  = (left->subSec->buildData.index - 1) | NF_SUBSECTOR;
}

static void hardenNodes(gamemap_t *dest, editmap_t *src)
{
    nodeCurIndex = 0;

    dest->numnodes = src->numnodes;
    dest->nodes =
        Z_Calloc(dest->numnodes * sizeof(node_t), PU_LEVELSTATIC, 0);

    if(src->rootNode)
        hardenNode(dest, src->rootNode);
}

void BSP_InitForNodeBuild(editmap_t *map)
{
    uint        i;

    for(i = 0; i < map->numlines; ++i)
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

    dest->numlines = src->numlines;
    dest->lines = Z_Calloc(dest->numlines * sizeof(line_t), PU_LEVELSTATIC, 0);

    for(i = 0; i < dest->numlines; ++i)
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

    dest->numvertexes = src->numvertexes;
    dest->vertexes =
        Z_Calloc(dest->numvertexes * sizeof(vertex_t), PU_LEVELSTATIC, 0);

    for(i = 0; i < dest->numvertexes; ++i)
    {
        vertex_t   *destV = &dest->vertexes[i];
        vertex_t   *srcV = src->vertexes[i];

        destV->header.type = DMU_VERTEX;
        destV->numlineowners = 0;
        destV->lineowners = NULL;
        destV->anchored = false;

        //// \fixme Add some rounding.
        destV->V_pos[VX] = (float) srcV->buildData.pos[VX];
        destV->V_pos[VY] = (float) srcV->buildData.pos[VY];
    }
}

static void hardenSidedefs(gamemap_t *dest, editmap_t *src)
{
    uint            i;

    dest->numsides = src->numsides;
    dest->sides = Z_Malloc(dest->numsides * sizeof(side_t), PU_LEVELSTATIC, 0);

    for(i = 0; i < dest->numsides; ++i)
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

    dest->numsectors = src->numsectors;
    dest->sectors = Z_Malloc(dest->numsectors * sizeof(sector_t), PU_LEVELSTATIC, 0);

    for(i = 0; i < dest->numsectors; ++i)
    {
        sector_t           *destS = &dest->sectors[i];
        sector_t           *srcS = src->sectors[i];
        plane_t            *pln;

        memcpy(destS, srcS, sizeof(*destS));
        destS->planecount = 0;
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

    if(src->numpolyobjs == 0)
    {
        dest->numpolyobjs = 0;
        dest->polyobjs = NULL;
        return;
    }

    dest->numpolyobjs = src->numpolyobjs;
    dest->polyobjs = Z_Malloc((dest->numpolyobjs+1) * sizeof(polyobj_t*),
                              PU_LEVEL, 0);

    for(i = 0; i < dest->numpolyobjs; ++i)
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
            seg->linedef = line;
            seg->SG_v1 = line->L_v1;
            seg->SG_v2 = line->L_v2;
            dx = line->L_v2pos[VX] - line->L_v1pos[VX];
            dy = line->L_v2pos[VY] - line->L_v1pos[VY];
            seg->length = P_AccurateDistance(dx, dy);
            seg->backseg = NULL;
            seg->sidedef = line->L_frontside;
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

            destP->segs[j] = seg;
        }
        destP->segs[j] = NULL; // Terminate.
        destP->numsegs = srcP->buildData.lineCount;

        dest->polyobjs[i] = destP;
    }
    dest->polyobjs[i] = NULL; // Terminate.
}

void SaveMap(gamemap_t *dest, editmap_t *src)
{
    hardenVertexes(dest, src);
    hardenSectors(dest, src);
    hardenSidedefs(dest, src);
    hardenLinedefs(dest, src);
    buildSegsFromHEdges(dest);
    hardenSubSectors(dest, src);
    hardenNodes(dest, src);
    hardenPolyobjs(dest, src);
}
