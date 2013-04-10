/** @file r_fakeradio.cpp Faked Radiosity Lighting.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
#include "de_render.h"
#include <de/Error>
#include <de/Log>
#include <de/memoryzone.h>
#include <de/vector1.h>
#include "map/gamemap.h"
#include "map/vertex.h"

using namespace de;

static zblockset_t *shadowLinksBlockSet;

bool Rend_RadioLineCastsShadow(LineDef const &line)
{
    if(line.isFromPolyobj()) return false;
    if(line.isSelfReferencing()) return false;

    // Lines with no other neighbor do not qualify for shadowing.
    if(&line.v1Owner()->next().line() == &line ||
       &line.v2Owner()->next().line() == &line) return false;

    return true;
}

bool Rend_RadioPlaneCastsShadow(Plane const &plane)
{
    if(plane.surface().hasMaterial())
    {
        Material const &surfaceMaterial = plane.surface().material();
        if(!surfaceMaterial.isDrawable()) return false;
        if(surfaceMaterial.hasGlow())     return false;
        if(surfaceMaterial.isSkyMasked()) return false;
    }
    return true;
}

/**
 * Given two lines "connected" by shared origin coordinates (0, 0) at a "corner"
 * vertex, calculate the point which lies @a dist1 away from @a line1 and also
 * @a dist2 from @a line2. The point should also be the nearest point to the
 * origin (in case of parallel lines).
 *
 * @param line1  Direction vector for the "left" line.
 * @param line2  Direction vector for the "right" line.
 * @param dist1  Distance from @a line1 to offset the corner point.
 * @param dist2  Distance from @a line2 to offset the corner point.
 *
 * Return values:
 * @param point  Coordinates for the corner point are written here. Can be @c 0.
 * @param lp     Coordinates for the "extended" point are written here. Can be @c 0.
 */
static void cornerNormalPoint(const_pvec2d_t line1, double dist1, const_pvec2d_t line2,
    double dist2, pvec2d_t point, pvec2d_t lp)
{
    if(!point && !lp) return;

    // Length of both lines.
    double len1 = V2d_Length(line1);
    double len2 = V2d_Length(line2);

    // Calculate normals for both lines.
    vec2d_t norm1; V2d_Set(norm1, -line1[VY] / len1 * dist1, line1[VX] / len1 * dist1);
    vec2d_t norm2; V2d_Set(norm2, line2[VY] / len2 * dist2, -line2[VX] / len2 * dist2);

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
 * @return  The width (world units) of the shadow edge. It is scaled depending on
 *          the length of @a edge.
 */
static double shadowEdgeWidth(pvec2d_t const edge)
{
    double const normalWidth = 20; //16;
    double const maxWidth    = 60;

    // A long edge?
    double length = V2d_Length(edge);
    if(length > 600)
    {
        double w = length - 600;
        if(w > 1000)
            w = 1000;
        return normalWidth + w / 1000 * maxWidth;
    }

    return normalWidth;
}

void Rend_RadioUpdateVertexShadowOffsets(Vertex &vtx)
{
    if(!vtx.lineOwnerCount()) return;

    vec2d_t leftDir, rightDir;

    LineOwner *base = vtx.firstLineOwner();
    LineOwner *own = base;
    do
    {
        LineDef const &lineB = own->line();
        LineDef const &lineA = own->next().line();

        if(&lineB.v1() == &vtx)
        {
            rightDir[VX] = lineB.direction().x;
            rightDir[VY] = lineB.direction().y;
        }
        else
        {
            rightDir[VX] = -lineB.direction().x;
            rightDir[VY] = -lineB.direction().y;
        }

        if(&lineA.v1() == &vtx)
        {
            leftDir[VX] = -lineA.direction().x;
            leftDir[VY] = -lineA.direction().y;
        }
        else
        {
            leftDir[VX] = lineA.direction().x;
            leftDir[VY] = lineA.direction().y;
        }

        // The left side is always flipped.
        V2d_Scale(leftDir, -1);

        cornerNormalPoint(leftDir,  shadowEdgeWidth(leftDir),
                          rightDir, shadowEdgeWidth(rightDir),
                          own->_shadowOffsets.inner,
                          own->_shadowOffsets.extended);

        own = &own->next();
    } while(own != base);
}

/**
 * Link @a line to @a bspLeaf for the purposes of shadowing.
 */
static void linkShadowLineDefToSSec(LineDef *line, byte side, BspLeaf *bspLeaf)
{
    DENG_ASSERT(line && bspLeaf);

#ifdef DENG_DEBUG
    // Check the links for dupes!
    for(ShadowLink *i = bspLeaf->firstShadowLink(); i; i = i->next)
    {
        if(i->line == line && i->side == side)
            throw Error("R_LinkShadow", "Already here!!");
    }
#endif

    // We'll need to allocate a new link.
    ShadowLink *link = (ShadowLink *) ZBlockSet_Allocate(shadowLinksBlockSet);

    link->line = line;
    link->side = side;
    // The links are stored into a linked list.
    link->next = bspLeaf->_shadows;
    bspLeaf->_shadows = link;
}

struct ShadowLinkerParms
{
    LineDef *line;
    byte side;
};

static int RIT_ShadowBspLeafLinker(BspLeaf *bspLeaf, void *context)
{
    ShadowLinkerParms *parms = static_cast<ShadowLinkerParms *>(context);
    linkShadowLineDefToSSec(parms->line, parms->side, bspLeaf);
    return false; // Continue iteration.
}

void Rend_RadioInitForMap()
{
    DENG_ASSERT(theMap != 0);

    Time begunAt;

    foreach(Vertex *vertex, theMap->vertexes())
    {
        Rend_RadioUpdateVertexShadowOffsets(*vertex);
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
    shadowLinksBlockSet = ZBlockSet_New(sizeof(ShadowLink), 1024, PU_MAP);

    ShadowLinkerParms parms;
    AABoxd bounds;
    vec2d_t point;

    foreach(LineDef *line, theMap->lines())
    {
        if(!Rend_RadioLineCastsShadow(*line)) continue;

        // For each side of the line.
        for(uint i = 0; i < 2; ++i)
        {
            LineDef::Side &side = line->side(i);

            if(!side.hasSector() || !side.hasSideDef()) continue;

            Vertex &vtx0 = line->vertex(i);
            Vertex &vtx1 = line->vertex(i^1);
            LineOwner &vo0 = line->vertexOwner(i)->next();
            LineOwner &vo1 = line->vertexOwner(i^1)->prev();

            V2d_CopyBox(bounds.arvec2, line->aaBox().arvec2);

            // Use the extended points, they are wider than inoffsets.
            V2d_Sum(point, vtx0.origin(), vo0.extendedShadowOffset());
            V2d_AddToBox(bounds.arvec2, point);

            V2d_Sum(point, vtx1.origin(), vo1.extendedShadowOffset());
            V2d_AddToBox(bounds.arvec2, point);

            parms.line = line;
            parms.side = i;

            // Link the shadowing line to all the BspLeafs whose axis-aligned
            // bounding box intersects 'bounds'.
            theMap->bspLeafsBoxIterator(bounds, side.sectorPtr(),
                                        RIT_ShadowBspLeafLinker, &parms);
        }
    }

    LOG_INFO(String("Rend_RadioInitForMap: Done in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));
}
