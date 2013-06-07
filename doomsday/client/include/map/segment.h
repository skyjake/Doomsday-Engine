/** @file map/segment.h World Map Line Segment.
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

#ifndef DENG_WORLD_MAP_LINESEGMENT
#define DENG_WORLD_MAP_LINESEGMENT

#include <de/Error>

#include "MapElement"
#include "BspLeaf"
#include "Face"
#include "HEdge"
#include "Line"
#include "Vertex"

#ifdef __CLIENT__
struct BiasSurface;
#endif

class Sector;

/**
 * @todo Consolidate/merge with bsp::LineSegment
 */
class Segment : public de::MapElement
{
    DENG2_NO_COPY(Segment)
    DENG2_NO_ASSIGN(Segment)

public:
    /// Required half-edge attribution is missing. @ingroup errors
    DENG2_ERROR(MissingHEdgeError);

    /// Required line attribution is missing. @ingroup errors
    DENG2_ERROR(MissingLineSideError);

#ifdef __CLIENT__

    /// The referenced geometry group does not exist. @ingroup errors
    DENG2_ERROR(UnknownGeometryGroupError);

#endif

    enum Flag
    {
        FacingFront = 0x1
    };
    Q_DECLARE_FLAGS(Flags, Flag)

public:
    Segment(Line::Side *lineSide = 0, de::HEdge *hedge = 0);

    de::HEdge &hedge() const;

    bool hasBack() const;

    /**
     * Returns the segment on the back side of "this".
     */
    Segment &back() const;

    void setBack(Segment *newBack);

    inline Vertex &from() const { return hedge().vertex(); }

    inline Vertex &to() const { return hedge().twin().vertex(); }

    /**
     * Returns @c true iff a polygon attributed to a BSP leaf is associated
     * with the line segment.
     *
     * @see HEdge::hasFace(), Face::mapElement()
     */
    inline bool hasBspLeaf() const {
        return hedge().hasFace() && hedge().face().mapElement() != 0;
    }

    /**
     * Convenience accessor which returns the BspLeaf attributed to the polygon
     * of which the line segment is a part.
     *
     * @see hasBspLeaf(), Face::mapElement()
     */
    inline BspLeaf &bspLeaf() const {
        return *hedge().face().mapElement()->castTo<BspLeaf>();
    }

    /**
     * Convenience accessor which returns the Sector attributed to the BspLeaf
     * attributed to the polygon of which the line segment is a part.
     *
     * @see BspLeaf::sector()
     */
    Sector &sector() const;

    /**
     * Convenience accessor which returns a pointer to the Sector attributed to
     * the BspLeaf attributed to the polygon of which the line segment is a part.
     *
     * @see hasBspLeaf(), BspLeaf::sectorPtr()
     */
    Sector *sectorPtr() const;

    /**
     * Returns @c true iff a Line::Side is attributed to the line segment.
     */
    bool hasLineSide() const;

    /**
     * Returns the Line::Side attributed to the line segment.
     *
     * @see hasLineSide()
     */
    Line::Side &lineSide() const;

    /**
     * Convenient accessor method for returning the line of the Line::Side
     * attributed to the line segment.
     *
     * @see hasLineSide(), lineSide()
     */
    inline Line &line() const { return lineSide().line(); }

    /**
     * Returns the distance along the attributed map line at which the from vertex
     * vertex occurs. If the segment is not attributed to a map line then @c 0 is
     * returned.
     *
     * @see hasLineSide(), lineSide()
     */
    coord_t lineSideOffset() const;

    /// @todo Refactor away.
    void setLineSideOffset(coord_t newOffset);

    /**
     * Returns the world angle of the line segment.
     */
    angle_t angle() const;

    /// @todo Refactor away.
    void setAngle(angle_t newAngle);

    /**
     * Returns the accurate length of the line segment, from the 'from' vertex to
     * the 'to' vertex in map coordinate space units.
     */
    coord_t length() const;

    /// @todo Refactor away.
    void setLength(coord_t newLength);

    /**
     * Returns the distance from @a point to the nearest point along the Segment
     * (in the inclussive range 0..1).
     *
     * @param point  Point to measure the distance to in the map coordinate space.
     */
    coord_t pointDistance(const_pvec2d_t point, coord_t *offset) const;

    /**
     * Returns the distance from @a point to the nearest point along the Segment
     * (in the inclussive range 0..1).
     *
     * @param x  X axis point to measure the distance to in the map coordinate space.
     * @param y  Y axis point to measure the distance to in the map coordinate space.
     */
    inline coord_t pointDistance(coord_t x, coord_t y, coord_t *offset) const
    {
        coord_t point[2] = { x, y };
        return pointDistance(point, offset);
    }

    /**
     * On which side of the Segment does the specified @a point lie?
     *
     * @param point  Point to test in the map coordinate space.
     *
     * @return @c <0 Point is to the left/back of the segment.
     *         @c =0 Point lies directly on the segment.
     *         @c >0 Point is to the right/front of the segment.
     */
    coord_t pointOnSide(const_pvec2d_t point) const;

    /**
     * On which side of the Segment does the specified @a point lie?
     *
     * @param x  X axis point to test in the map coordinate space.
     * @param y  Y axis point to test in the map coordinate space.
     *
     * @return @c <0 Point is to the left/back of the segment.
     *         @c =0 Point lies directly on the segment.
     *         @c >0 Point is to the right/front of the segment.
     */
    inline coord_t pointOnSide(coord_t x, coord_t y) const
    {
        coord_t point[2] = { x, y };
        return pointOnSide(point);
    }

    /**
     * Returns the current value of the flags of the line segment.
     */
    Flags flags() const;

    /**
     * Returns @c true iff the line segment is flagged @a flagsToTest.
     */
    inline bool isFlagged(int flagsToTest) const { return (flags() & flagsToTest) != 0; }

    /**
     * Change the line segment's flags.
     *
     * @param flagsToChange  Flags to change the value of.
     * @param operation      Logical operation to perform on the flags.
     */
    void setFlags(Flags flagsToChange, de::FlagOp operation = de::SetFlags);

#ifdef __CLIENT__

    /**
     * Retrieve the bias surface for specified geometry @a group.
     *
     * @param group  Geometry group identifier for the bias surface.
     */
    BiasSurface &biasSurface(int group);

    /**
     * Assign a new bias surface to the specified geometry @a group.
     *
     * @param group        Geometry group identifier for the surface.
     * @param biasSurface  New BiasSurface for the identified @a group. Any
     *                     existing bias surface will be replaced (destroyed).
     *                     Ownership is given to the segment.
     */
    void setBiasSurface(int group, BiasSurface *biasSurface);

#endif // __CLIENT__

protected:
    int property(setargs_t &args) const;

private:
    DENG2_PRIVATE(d)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Segment::Flags)

#endif // DENG_WORLD_MAP_LINESEGMENT
