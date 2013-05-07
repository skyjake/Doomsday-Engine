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

#include <de/memoryzone.h>
#include <de/vector1.h> /// @todo remove me
#include <de/Vector>

#include <de/Error>
#include <de/Log>

#include "de_base.h"
#include "de_render.h"

#include "map/lineowner.h"
#include "map/gamemap.h"
#include "Vertex"

#include "render/rend_fakeradio.h"

using namespace de;

static LineSideRadioData *lineSideRadioData;
static zblockset_t *shadowLinksBlockSet;

bool Rend_RadioLineCastsShadow(Line const &line)
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

LineSideRadioData &Rend_RadioDataForLineSide(Line::Side &side)
{
    return lineSideRadioData[side.line().indexInMap() * 2 + (side.isBack()? 1 : 0)];
}

/**
 * Given two lines "connected" by shared origin coordinates (0, 0) at a "corner"
 * vertex, calculate the point which lies @a distA away from @a lineA and also
 * @a distB from @a lineB. The point should also be the nearest point to the
 * origin (in case of parallel lines).
 *
 * @param lineA  Direction vector for the "left" line.
 * @param lineB  Direction vector for the "right" line.
 * @param distA  Distance from @a lineA to offset the corner point.
 * @param distB  Distance from @a lineB to offset the corner point.
 *
 * Return values:
 * @param point  Coordinates for the corner point are written here. Can be @c 0.
 * @param lp     Coordinates for the "extended" point are written here. Can be @c 0.
 */
static void cornerNormalPoint(Vector2d const &lineADirection, double dist1,
                              Vector2d const &lineBDirection, double dist2,
                              Vector2d *point, Vector2d *lp)
{
    // Any work to be done?
    if(!point && !lp) return;

    // Length of both lines.
    double len1 = lineADirection.length();
    double len2 = lineBDirection.length();

    // Calculate normals for both lines.
    Vector2d norm1(-lineADirection.y / len1 * dist1,  lineADirection.x / len1 * dist1);
    Vector2d norm2( lineBDirection.y / len2 * dist2, -lineBDirection.x / len2 * dist2);

    // Do we need to calculate the extended points, too?  Check that
    // the extension does not bleed too badly outside the legal shadow
    // area.
    if(lp)
    {
        *lp = lineBDirection / len2 * dist2;
    }

    // Do we need to determine the intercept point?
    if(!point) return;

    // Normal shift to produce the lines we need to find the intersection.
    Partition lineA(norm1, lineADirection);
    Partition lineB(norm2, lineBDirection);

    if(!lineA.isParallelTo(lineB))
    {
        *point = lineA.intercept(lineB);
        return;
    }

    // Special case: parallel
    // There will be no intersection at any point therefore it will not be
    // possible to determine our corner point (so just use a normal as the
    // point instead).
    *point = norm1;
}

/**
 * @return  The width (world units) of the shadow edge. It is scaled depending on
 *          the length of @a edge.
 */
static double shadowEdgeWidth(Vector2d const &edge)
{
    double const normalWidth = 20; //16;
    double const maxWidth    = 60;

    // A long edge?
    double length = edge.length();
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

    Vector2d leftDir, rightDir;

    LineOwner *base = vtx.firstLineOwner();
    LineOwner *own = base;
    do
    {
        Line const &lineB = own->line();
        Line const &lineA = own->next().line();

        if(&lineB.from() == &vtx)
        {
            rightDir = lineB.direction();
        }
        else
        {
            rightDir = -lineB.direction();
        }

        if(&lineA.from() == &vtx)
        {
            leftDir = -lineA.direction();
        }
        else
        {
            leftDir = lineA.direction();
        }

        // The left side is always flipped.
        leftDir *= -1;

        cornerNormalPoint(leftDir,  shadowEdgeWidth(leftDir),
                          rightDir, shadowEdgeWidth(rightDir),
                          &own->_shadowOffsets.inner,
                          &own->_shadowOffsets.extended);

        own = &own->next();
    } while(own != base);
}

/**
 * Link @a line to @a bspLeaf for the purposes of shadowing.
 */
static void linkShadowLineToSSec(Line::Side &side, BspLeaf &bspLeaf)
{
#ifdef DENG_DEBUG
    // Check the links for dupes!
    for(ShadowLink *i = bspLeaf.firstShadowLink(); i; i = i->next)
    {
        if(i->side == &side)
            throw Error("R_LinkShadow", "Already here!!");
    }
#endif

    // We'll need to allocate a new link.
    ShadowLink *link = (ShadowLink *) ZBlockSet_Allocate(shadowLinksBlockSet);

    link->side = &side;
    // The links are stored into a linked list.
    link->next = bspLeaf._shadows;
    bspLeaf._shadows = link;
}

static int RIT_ShadowBspLeafLinker(BspLeaf *bspLeaf, void *context)
{
    linkShadowLineToSSec(*static_cast<Line::Side *>(context), *bspLeaf);
    return false; // Continue iteration.
}

void Rend_RadioInitForMap()
{
    DENG_ASSERT(theMap != 0);

    Time begunAt;

    lineSideRadioData = reinterpret_cast<LineSideRadioData *>(Z_Calloc(sizeof(*lineSideRadioData) * theMap->sideCount(), PU_MAP, 0));

    foreach(Vertex *vertex, theMap->vertexes())
    {
        Rend_RadioUpdateVertexShadowOffsets(*vertex);
    }

    /**
     * The algorithm:
     *
     * 1. Use the BSP leaf blockmap to look for all the blocks that are
     *    within the line's shadow bounding box.
     *
     * 2. Check the BspLeafs whose sector is the same as the line.
     *
     * 3. If any of the shadow points are in the BSP leaf, or any of the
     *    shadow edges cross one of the BSP leaf's edges (not parallel),
     *    link the line to the BspLeaf.
     */
    shadowLinksBlockSet = ZBlockSet_New(sizeof(ShadowLink), 1024, PU_MAP);

    AABoxd bounds;

    foreach(Line *line, theMap->lines())
    {
        if(!Rend_RadioLineCastsShadow(*line)) continue;

        // For each side of the line.
        for(uint i = 0; i < 2; ++i)
        {
            Line::Side &side = line->side(i);

            if(!side.hasSector() || !side.hasSections()) continue;

            Vertex &vtx0 = line->vertex(i);
            Vertex &vtx1 = line->vertex(i^1);
            LineOwner &vo0 = line->vertexOwner(i)->next();
            LineOwner &vo1 = line->vertexOwner(i^1)->prev();

            V2d_CopyBox(bounds.arvec2, line->aaBox().arvec2);

            // Use the extended points, they are wider than inoffsets.
            Vector2d point = vtx0.origin() + vo0.extendedShadowOffset();
            V2d_AddToBoxXY(bounds.arvec2, point.x, point.y);

            point = vtx1.origin() + vo1.extendedShadowOffset();
            V2d_AddToBoxXY(bounds.arvec2, point.x, point.y);

            // Link the shadowing line to all the BspLeafs whose axis-aligned
            // bounding box intersects 'bounds'.
            theMap->bspLeafsBoxIterator(bounds, side.sectorPtr(),
                                        RIT_ShadowBspLeafLinker, &side);
        }
    }

    LOG_INFO(String("Rend_RadioInitForMap: Done in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));
}
