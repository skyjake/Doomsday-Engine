/** @file vertex.cpp  World map vertex.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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
#include "world/vertex.h"

#include <de/Vector>
#include "Line"
#include "world/lineowner.h"  /// @todo remove me
#include "Sector"
#ifdef __CLIENT__
#  include "partition.h"
#endif

using namespace de;

Vertex::Vertex(Mesh &mesh, Vector2d const &origin)
    : MapElement(DMU_VERTEX)
    , MeshElement(mesh)
{
    _origin = origin;
}

Vector2d const &Vertex::origin() const
{
    return _origin;
}

void Vertex::setOrigin(Vector2d const &newOrigin)
{
    if(_origin != newOrigin)
    {
        _origin = newOrigin;
        DENG2_FOR_AUDIENCE(OriginChange, i)
        {
            i->vertexOriginChanged(*this);
        }
    }
}

int Vertex::property(DmuArgs &args) const
{
    switch(args.prop)
    {
    case DMU_X:
        args.setValue(DMT_VERTEX_ORIGIN, &_origin.x, 0);
        break;
    case DMU_Y:
        args.setValue(DMT_VERTEX_ORIGIN, &_origin.y, 0);
        break;
    case DMU_XY:
        args.setValue(DMT_VERTEX_ORIGIN, &_origin.x, 0);
        args.setValue(DMT_VERTEX_ORIGIN, &_origin.y, 1);
        break;
    default:
        return MapElement::property(args);
    }
    return false;  // Continue iteration.
}

// ---------------------------------------------------------------------------

duint Vertex::lineOwnerCount() const
{
    return _numLineOwners;
}

void Vertex::countLineOwners()
{
    _onesOwnerCount = _twosOwnerCount = 0;

    if(LineOwner const *firstOwn = firstLineOwner())
    {
        LineOwner const *own = firstOwn;
        do
        {
            if(!own->line().hasFrontSector() || !own->line().hasBackSector())
            {
                _onesOwnerCount += 1;
            }
            else
            {
                _twosOwnerCount += 1;
            }
        } while((own = &own->next()) != firstOwn);
    }
}

LineOwner *Vertex::firstLineOwner() const
{
    return _lineOwners;
}

#ifdef __CLIENT__

/**
 * Given two lines "connected" by shared origin coordinates (0, 0) at a "corner"
 * vertex, calculate the point which lies @a distA away from @a lineA and also
 * @a distB from @a lineB. The point should also be the nearest point to the
 * origin (in case of parallel lines).
 *
 * @param lineADirection  Direction vector for the "left" line.
 * @param dist1           Distance from @a lineA to offset the corner point.
 * @param lineBDirection  Direction vector for the "right" line.
 * @param dist2           Distance from @a lineB to offset the corner point.
 *
 * Return values:
 * @param point  Coordinates for the corner point are written here. Can be @c nullptr.
 * @param lp     Coordinates for the "extended" point are written here. Can be @c nullptr.
 */
static void cornerNormalPoint(Vector2d const &lineADirection, ddouble dist1,
                              Vector2d const &lineBDirection, ddouble dist2,
                              Vector2d *point, Vector2d *lp)
{
    // Any work to be done?
    if(!point && !lp) return;

    // Length of both lines.
    ddouble len1 = lineADirection.length();
    ddouble len2 = lineBDirection.length();

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
    if(point)
    {
        // Normal shift to produce the lines we need to find the intersection.
        Partition lineA(lineADirection, norm1);
        Partition lineB(lineBDirection, norm2);

        if(lineA.isParallelTo(lineB))
        {
            // There will be no intersection at any point therefore it will not be
            // possible to determine our corner point (so just use a normal as the
            // point instead).
            *point = norm1;
        }
        else
        {
            *point = lineA.intercept(lineB);
        }
    }
}

/**
 * Returns the width (world units) of a shadow edge (scaled depending on the length of @a edge).
 */
static ddouble shadowEdgeWidth(Vector2d const &edge)
{
    ddouble const normalWidth = 20; //16;
    ddouble const maxWidth    = 60;

    // A long edge?
    ddouble length = edge.length();
    if(length > 600)
    {
        ddouble w = length - 600;
        if(w > 1000) w = 1000;
        return normalWidth + w / 1000 * maxWidth;
    }

    return normalWidth;
}

void Vertex::updateShadowOffsets()
{
    LineOwner *base = firstLineOwner();
    if(!base) return;

    LineOwner *own = base;
    do
    {
        Line const &lineB = own->line();
        Line const &lineA = own->next().line();

        Vector2d const rightDir = (&lineB.from() == this?  lineB.direction() : -lineB.direction());
        Vector2d const leftDir  = (&lineA.from() == this? -lineA.direction() :  lineA.direction()) * -1;  // Always flipped.

        cornerNormalPoint(leftDir,  shadowEdgeWidth(leftDir),
                          rightDir, shadowEdgeWidth(rightDir),
                          &own->_shadowOffsets.inner,
                          &own->_shadowOffsets.extended);

        own = &own->next();
    } while(own != base);
}

#endif  // __CLIENT__
