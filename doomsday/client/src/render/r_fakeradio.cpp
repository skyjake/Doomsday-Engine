/** @file r_fakeradio.cpp Faked Radiosity Lighting
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "de_console.h"
#include "de_misc.h"
#include "de_play.h"
#include "de_render.h"

static zblockset_t *shadowLinksBlockSet;

/**
 * Line1 and line2 are the (dx,dy)s for two lines, connected at the
 * origin (0,0).  Dist1 and dist2 are the distances from these lines.
 * The returned point (in 'point') is dist1 away from line1 and dist2
 * from line2, while also being the nearest point to the origin (in
 * case the lines are parallel).
 */
void R_CornerNormalPoint(const_pvec2d_t line1, double dist1,
                         const_pvec2d_t line2, double dist2, pvec2d_t point,
                         pvec2d_t lp)
{
    double len1, len2;
    vec2d_t norm1, norm2;

    // Length of both lines.
    len1 = V2d_Length(line1);
    len2 = V2d_Length(line2);

    // Calculate normals for both lines.
    V2d_Set(norm1, -line1[VY] / len1 * dist1, line1[VX] / len1 * dist1);
    V2d_Set(norm2, line2[VY] / len2 * dist2, -line2[VX] / len2 * dist2);

    // Do we need to calculate the extended points, too?  Check that
    // the extension does not bleed too badly outside the legal shadow
    // area.
    if(lp)
    {
        V2d_Set(lp, line2[VX] / len2 * dist2, line2[VY] / len2 * dist2);
    }

    // Are the lines parallel?  If so, they won't connect at any
    // point, and it will be impossible to determine a corner point.
    if(V2d_IsParallel(line1, line2))
    {
        // Just use a normal as the point.
        if(point)
            V2d_Copy(point, norm1);
        return;
    }

    // Find the intersection of normal-shifted lines.  That'll be our
    // corner point.
    if(point)
        V2d_Intersection(norm1, line1, norm2, line2, point);
}

/**
 * @return          The width (world units) of the shadow edge.
 *                  It is scaled depending on the length of the edge.
 */
double R_ShadowEdgeWidth(const pvec2d_t edge)
{
    double length = V2d_Length(edge);
    double normalWidth = 20;   //16;
    double maxWidth = 60;
    double w;

    // A long edge?
    if(length > 600)
    {
        w = length - 600;
        if(w > 1000)
            w = 1000;
        return normalWidth + w / 1000 * maxWidth;
    }

    return normalWidth;
}

void Rend_RadioUpdateVertexShadowOffsets(Vertex *vtx)
{
    if(!vtx) return;
    if(!vtx->lineOwnerCount()) return;

    vec2d_t left, right;

    LineOwner *base = vtx->firstLineOwner();
    LineOwner *own = base;
    do
    {
        LineDef *lineB = &own->line();
        LineDef *lineA = &own->next().line();

        if(&lineB->v1() == vtx)
        {
            right[VX] = lineB->direction[VX];
            right[VY] = lineB->direction[VY];
        }
        else
        {
            right[VX] = -lineB->direction[VX];
            right[VY] = -lineB->direction[VY];
        }

        if(&lineA->v1() == vtx)
        {
            left[VX] = -lineA->direction[VX];
            left[VY] = -lineA->direction[VY];
        }
        else
        {
            left[VX] = lineA->direction[VX];
            left[VY] = lineA->direction[VY];
        }

        // The left side is always flipped.
        V2d_Scale(left, -1);

        R_CornerNormalPoint(left, R_ShadowEdgeWidth(left), right,
                            R_ShadowEdgeWidth(right),
                            own->_shadowOffsets.inner,
                            own->_shadowOffsets.extended);

        own = &own->next();
    } while(own != base);
}

/**
 * Link a half-edge to an arbitary BSP leaf for the purposes of shadowing.
 */
static void linkShadowLineDefToSSec(LineDef *line, byte side, BspLeaf* bspLeaf)
{
#ifdef DENG_DEBUG
    // Check the links for dupes!
    for(shadowlink_t *i = bspLeaf->firstShadowLink(); i; i = i->next)
    {
        if(i->lineDef == line && i->side == side)
            Con_Error("R_LinkShadow: Already here!!\n");
    }
#endif

    // We'll need to allocate a new link.
    shadowlink_t *link = (shadowlink_t *) ZBlockSet_Allocate(shadowLinksBlockSet);

    // The links are stored into a linked list.
    link->next = bspLeaf->_shadows;
    bspLeaf->_shadows = link;
    link->lineDef = line;
    link->side = side;
}

typedef struct shadowlinkerparms_s {
    LineDef *lineDef;
    byte side;
} shadowlinkerparms_t;

/**
 * If the shadow polygon (parm) contacts the BspLeaf, link the poly
 * to the BspLeaf's shadow list.
 */
int RIT_ShadowBspLeafLinker(BspLeaf *bspLeaf, void *parm)
{
    shadowlinkerparms_t *data = (shadowlinkerparms_t *) parm;
    linkShadowLineDefToSSec(data->lineDef, data->side, bspLeaf);
    return false; // Continue iteration.
}

boolean Rend_RadioIsShadowingLineDef(LineDef *line)
{
    if(line)
    {
        if(!line->isSelfReferencing() && !(line->inFlags & LF_POLYOBJ) &&
           !(&line->v1Owner()->next().line() == line ||
             &line->v2Owner()->next().line() == line))
        {
            return true;
        }
    }

    return false;
}

void Rend_RadioInitForMap()
{
    uint startTime = Timer_RealMilliseconds();

    for(uint i = 0; i < NUM_VERTEXES; ++i)
    {
        Rend_RadioUpdateVertexShadowOffsets(VERTEX_PTR(i));
    }

    /**
     * The algorithm:
     *
     * 1. Use the BSP leaf blockmap to look for all the blocks that are
     *    within the linedef's shadow bounding box.
     *
     * 2. Check the BspLeafs whose sector is the same as the linedef.
     *
     * 3. If any of the shadow points are in the BSP leaf, or any of the
     *    shadow edges cross one of the BSP leaf's edges (not parallel),
     *    link the linedef to the BspLeaf.
     */
    shadowLinksBlockSet = ZBlockSet_New(sizeof(shadowlink_t), 1024, PU_MAP);

    shadowlinkerparms_t data;
    AABoxd bounds;
    vec2d_t point;

    for(uint i = 0; i < NUM_LINEDEFS; ++i)
    {
        LineDef *line = LINE_PTR(i);
        if(!Rend_RadioIsShadowingLineDef(line)) continue;

        for(uint j = 0; j < 2; ++j)
        {
            if(!line->hasSector(j) || !line->hasSideDef(j)) continue;

            Vertex &vtx0 = line->vertex(j);
            Vertex &vtx1 = line->vertex(j^1);
            LineOwner *vo0 = &line->vertexOwner(j)->next();
            LineOwner *vo1 = &line->vertexOwner(j^1)->prev();

            // Use the extended points, they are wider than inoffsets.
            V2d_Copy(point, vtx0.origin());
            V2d_InitBox(bounds.arvec2, point);

            V2d_Sum(point, point, vo0->extendedShadowOffset());
            V2d_AddToBox(bounds.arvec2, point);

            V2d_Copy(point, vtx1.origin());
            V2d_AddToBox(bounds.arvec2, point);

            V2d_Sum(point, point, vo1->extendedShadowOffset());
            V2d_AddToBox(bounds.arvec2, point);

            data.lineDef = line;
            data.side = j;

            P_BspLeafsBoxIterator(&bounds, line->sectorPtr(j),
                                  RIT_ShadowBspLeafLinker, &data);
        }
    }

    VERBOSE2( Con_Message("R_InitFakeRadioForMap: Done in %.2f seconds.", (Timer_RealMilliseconds() - startTime) / 1000.0f) )
}
