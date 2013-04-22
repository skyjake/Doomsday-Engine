/** @file map/hedge.h World Map Geometry Half-Edge.
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

#ifndef DENG_WORLD_MAP_HEDGE
#define DENG_WORLD_MAP_HEDGE

#include <de/Error>

#include "p_dmu.h"

#include "MapElement"
#include "BspLeaf"
#include "Line"
#include "Sector"
#include "Vertex"

#include "resource/r_data.h"
#include "render/walldiv.h"
#include "render/rend_bias.h"

// Helper macros for accessing hedge data elements.
#define FRONT 0
#define BACK  1

/**
 * @defgroup hedgeFrameFlags  Half-edge Frame Flags
 * @ingroup map
 */
///@{
#define HEDGEINF_FACINGFRONT      0x0001
///@}

// Logical edge identifiers:
/// @addtogroup map
///@{
#define FROM                    0
#define TO                      1

/// Aliases:
#define START                   FROM
#define END                     TO
///@}

/**
 * Map geometry half-edge.
 *
 * @em The twin of a half-edge is another HEdge, that which is on the other
 * side. If a twin half-edge is linked, they must always be updated as one.
 * (i.e., if one of the half-edges is split, the twin must be split also).
 *
 * @todo The design of this component presently means it cannot yet be rightly
 * considered a half-edge (there are two vertex links and two adjacent edge
 * links (among other discrepancies)). This component should be revised,
 * simplifying it accordingly, as this will in turn lead to dependent logic
 * improvements elsewhere. -ds
 *
 * @ingroup map
 */
class HEdge : public de::MapElement
{
public:
    /// Required BSP leaf is missing. @ingroup errors
    DENG2_ERROR(MissingBspLeafError);

    /// Required twin half-edge is missing. @ingroup errors
    DENG2_ERROR(MissingTwinError);

    /// Required line attribution is missing. @ingroup errors
    DENG2_ERROR(MissingLineSideError);

    /// The referenced geometry group does not exist. @ingroup errors
    DENG2_ERROR(UnknownGeometryGroupError);

public: /// @todo Make private:
    /// Start and End vertexes of the segment.
    Vertex *_from, *_to;

    /// Next half-edge (clockwise) around the @em face.
    HEdge *_next;

    /// Previous half-edge (anticlockwise) around the @em face.
    HEdge *_prev;

    /// Linked @em twin half-edge (that on the other side of "this" half-edge).
    HEdge *_twin;

    /// The @em face of the half-edge (a BSP leaf).
    BspLeaf *_bspLeaf;

    /// Point along the attributed line at which v1 occurs; otherwise @c 0.
    coord_t _lineOffset;

    /// World angle.
    angle_t _angle;

    /// Accurate length of the segment (v1 -> v2).
    coord_t _length;

    /// For each section of a Line::Side.
    biassurface_t *_bsuf[3];

    /// @ref hedgeFrameFlags
    short _frameFlags;

public:
    HEdge(Vertex &from, Line::Side *lineSide = 0);
    HEdge(HEdge const &other);
    ~HEdge();

    /**
     * Returns the specified edge vertex for the half-edge.
     *
     * @param to  If not @c 0 return the To vertex; otherwise the From vertex.
     */
    Vertex &vertex(int to);

    /// @copydoc vertex()
    Vertex const &vertex(int to) const;

    /**
     * Convenient accessor method for returning the origin of the specified
     * edge vertex for the half-edge.
     *
     * @see vertex()
     */
    inline de::Vector2d const &vertexOrigin(int to) const
    {
        return vertex(to).origin();
    }

    /**
     * Returns the From/Start vertex for the half-edge.
     */
    inline Vertex &v1() { return vertex(FROM); }

    /// @copydoc v1()
    inline Vertex const &v1() const { return vertex(FROM); }

    /// @copydoc v1()
    /// An alias of v1().
    inline Vertex &from() { return v1(); }

    /// @copydoc from()
    /// An alias of v1().
    inline Vertex const &from() const { return v1(); }

    /**
     * Convenient accessor method for returning the origin of the From/Start
     * vertex for the half-edge.
     *
     * @see v1()
     */
    inline de::Vector2d const &v1Origin() const { return v1().origin(); }

    /// @copydoc v1Origin()
    /// An alias of v1Origin()
    inline de::Vector2d const &fromOrigin() const { return v1Origin(); }

    /**
     * Returns the To/End vertex for the half-edge.
     */
    inline Vertex &v2() { return vertex(TO); }

    /// @copydoc v2()
    inline Vertex const &v2() const { return vertex(TO); }

    /// @copydoc v2()
    /// An alias of v2().
    inline Vertex &to() { return v2(); }

    /// @copydoc to()
    /// An alias of v2().
    inline Vertex const &to() const { return v2(); }

    /**
     * Convenient accessor method for returning the origin of the To/End
     * vertex for the half-edge.
     *
     * @see v2()
     */
    inline de::Vector2d const &v2Origin() const { return v2().origin(); }

    /// @copydoc v2Origin()
    /// An alias of v2Origin()
    inline de::Vector2d const &toOrigin() const { return v2Origin(); }

    /**
     * Returns the linked @em next half-edge (clockwise) around the face of
     * the half-edge (which might be *this* half-edge).
     *
     * @see nextIsThis()
     */
    HEdge &next() const;

    /**
     * Returns @c true iff the @em next half-edge (clockwise) around the face
     * of the half-edge is *this*.
     *
     * @see next()
     */
    inline bool nextIsThis() const { return &next() == this; }

    /**
     * Returns the linked @em previous half-edge (anticlockwise) around the
     * face of the half-edge (which might be *this* half-edge).
     *
     * @see prevIsThis()
     */
    HEdge &prev() const;

    /**
     * Returns @c true iff the @em previous half-edge (anticlockwise) around
     * the face of the half-edge is *this*.
     *
     * @see prev()
     */
    inline bool prevIsThis() const { return &prev() == this; }

    /**
     * Returns @c true iff a @em twin is linked to the half-edge.
     */
    bool hasTwin() const;

    /**
     * Returns the linked @em twin of the half-edge.
     */
    HEdge &twin() const;

    /**
     * Returns a pointer to the linked @em twin of the half-edge; otherwise @c 0.
     *
     * @see hasTwin()
     */
    inline HEdge *twinPtr() const { return hasTwin()? &twin() : 0; }

    /**
     * Returns @c true iff a BspLeaf is linked to the half-edge.
     */
    bool hasBspLeaf() const;

    /**
     * Returns the BSP leaf for the half-edge.
     */
    BspLeaf &bspLeaf() const;

    /**
     * Convenient accessor method for returning the sector attributed to the BSP
     * leaf for the half-edge. One should first determine whether a sector is indeed
     * attributed to the BSP leaf (e.g., by calling @ref BspLeaf::hasSector()).
     *
     * @see bspLeaf(), BspLeaf::sector()
     */
    inline Sector &bspLeafSector() const { return bspLeaf().sector(); }

    /// Variant of @ref bspLeafSector() which returns a pointer.
    inline Sector *bspLeafSectorPtr() const { return bspLeaf().sectorPtr(); }

    /**
     * Returns @c true iff a Line::Side is attributed to the half-edge.
     */
    bool hasLineSide() const;

    /**
     * Returns the Line::Side attributed to the half-edge.
     *
     * @see hasLineSide()
     */
    Line::Side &lineSide() const;

    /**
     * Convenient accessor method for returning the line of the Line::Side which
     * is attributed to the half-edge.
     *
     * @see hasLineSide(), lineSide()
     */
    inline Line &line() const { return lineSide().line(); }

    /**
     * Convenient accessor method for returning the line side identifier of the
     * Line::Side attributed to the half-edge.
     *
     * @see hasLineSide(), lineSide()
     */
    inline int lineSideId() const { return lineSide().lineSideId(); }

    /**
     * Returns the offset point at which v1 occurs from the attributed line for
     * the half-edge.
     *
     * @see hasLineSide()
     */
    coord_t lineOffset() const;

    /**
     * Returns the world angle of the half-edge.
     */
    angle_t angle() const;

    /**
     * Returns the accurate length of the half-edge from v1 to v2.
     */
    coord_t length() const;

    /**
     * Returns the distance from @a point to the nearest point along the HEdge [0..1].
     *
     * @param point  Point to measure the distance to in the map coordinate space.
     */
    coord_t pointDistance(const_pvec2d_t point, coord_t *offset) const;

    /**
     * Returns the distance from @a point to the nearest point along the HEdge [0..1].
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
     * On which side of the HEdge does the specified @a point lie?
     *
     * @param point  Point to test in the map coordinate space.
     *
     * @return @c <0 Point is to the left/back of the hedge.
     *         @c =0 Point lies directly on the hedge.
     *         @c >0 Point is to the right/front of the hedge.
     */
    coord_t pointOnSide(const_pvec2d_t point) const;

    /**
     * On which side of the HEdge does the specified @a point lie?
     *
     * @param x  X axis point to test in the map coordinate space.
     * @param y  Y axis point to test in the map coordinate space.
     *
     * @return @c <0 Point is to the left/back of the hedge.
     *         @c =0 Point lies directly on the hedge.
     *         @c >0 Point is to the right/front of the hedge.
     */
    inline coord_t pointOnSide(coord_t x, coord_t y) const
    {
        coord_t point[2] = { x, y };
        return pointOnSide(point);
    }

    /**
     * Prepare wall division data for a section of the HEdge.
     *
     * @param section        Line::Side section to prepare divisions for.
     * @param leftWallDivs   Division data for the left edge is written here.
     * @param rightWallDivs  Division data for the right edge is written here.
     * @param materialOrigin Material origin offset data is written here. Can be @c 0.
     *
     * @return  @c true if divisions were prepared (the specified @a section has a
     *          non-zero Z axis height).
     */
    bool prepareWallDivs(int section, walldivs_t *leftWallDivs, walldivs_t *rightWallDivs,
        de::Vector2f *materialOrigin) const;

    /**
     * Retrieve the bias surface for specified geometry @a groupId
     *
     * @param groupId  Geometry group identifier for the bias surface.
     */
    biassurface_t &biasSurfaceForGeometryGroup(uint groupId);

protected:
    int property(setargs_t &args) const;

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_WORLD_MAP_HEDGE
