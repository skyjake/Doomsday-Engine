/** @file linedef.h Map LineDef.
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

#ifndef LIBDENG_MAP_LINEDEF
#define LIBDENG_MAP_LINEDEF

#include <de/binangle.h>
#include <de/mathutil.h> // Divline
#include <de/Error>
#include "resource/r_data.h"
#include "map/vertex.h"
#include "p_mapdata.h"
#include "p_dmu.h"
#include "MapElement"

class Sector;
class SideDef;
class HEdge;

/*
 * Helper macros for accessing linedef data elements:
 */
/// @addtogroup map
///@{
#define L_vo(n)                 vo[(n)? 1:0]
#define L_vo1                   L_vo(0)
#define L_vo2                   L_vo(1)

#define L_sidedef(n)            side(n).sideDef
#define L_frontsidedef          L_sidedef(FRONT)
#define L_backsidedef           L_sidedef(BACK)
///@}

// Internal flags:
#define LF_POLYOBJ              0x1 ///< Line is part of a polyobject.
#define LF_BSPWINDOW            0x2 ///< Line produced a BSP window. @todo Refactor away.

// Logical face identifiers:
/// @addtogroup map
///@{
#define FRONT                   0
#define BACK                    1
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
 * @defgroup sideSectionFlags  Side Section Flags
 * @ingroup map
 */
///@{
#define SSF_MIDDLE              0x1
#define SSF_BOTTOM              0x2
#define SSF_TOP                 0x4
///@}

/**
 * Map line.
 *
 * Despite sharing it's name with a map element present in the id Tech 1 map
 * format, this component has a notably different design and slightly different
 * purpose in the Doomsday Engine.
 *
 * Lines always have two logical sides, however they may not have a sector
 * attributed to either or both sides.
 *
 * @note Lines are @em not considered to define the geometry of a map. Instead
 * a line should be thought of as a finite line segment in the plane, according
 * to the standard definition of a line as used with an arrangement of lines in
 * computational geometry.
 *
 * @see http://en.wikipedia.org/wiki/Arrangement_of_lines
 *
 * @ingroup map
 *
 * @todo Should be renamed "Line" (this is not a definition, it @em is a line).
 */
class LineDef : public de::MapElement
{
public:
    /// Required sector attribution is missing. @ingroup errors
    DENG2_ERROR(MissingSectorError);

    /// The referenced property does not exist. @ingroup errors
    DENG2_ERROR(UnknownPropertyError);

    /// The referenced property is not writeable. @ingroup errors
    DENG2_ERROR(WritePropertyError);

    /**
     * Logical side of which there are always two (a front and a back).
     */
    struct Side
    {
    public: /// @todo make private:
        /// Sector on this side.
        Sector *_sector;

        /// SideDef on this side.
        SideDef *sideDef;

        /// Left-most HEdge on this side.
        HEdge *hedgeLeft;

        /// Right-most HEdge on this side.
        HEdge *hedgeRight;

        /// Framecount of last time shadows were drawn on this side.
        ushort shadowVisFrame;

    public:
        /**
         * Returns @c true iff a Sector is attributed to the side.
         */
        bool hasSector() const;

        /**
         * Returns the Sector attributed to the side.
         *
         * @see hasSector()
         */
        Sector &sector() const;

        /**
         * Returns a pointer to the sector attributed to the side; otherwise @c 0.
         *
         * @see hasSector()
         */
        inline Sector *sectorPtr() const { return hasSector()? &sector() : 0; }
    };

public: /// @todo make private:
    /// Vertexes [From, To]
    Vertex *_v[2];

    /// Links to vertex line owner nodes [From, To].
    LineOwner *vo[2];

    /// Logical sides [Front, Back]
    Side _sides[2];

    /// Public DDLF_* flags.
    int flags;

    /// Internal LF_* flags.
    byte inFlags;

    /// Logical slope type.
    slopetype_t slopeType;

    int validCount;

    /// Calculated from front side's normal.
    binangle_t angle;

    coord_t direction[2];

    /// Accurate length.
    coord_t length;

    AABoxd aaBox;

    /// Whether the line has been mapped by each player yet.
    boolean mapped[DDMAXPLAYERS];

    /// Original index in the archived map.
    int origIndex;

public:
    LineDef();
    ~LineDef();

    /**
     * Returns the specified logical side of the line.
     *
     * @param back  If not @c 0 return the Back side; otherwise the Front side.
     */
    Side &side(int back);

    /// @copydoc side()
    Side const &side(int back) const;

    /**
     * Returns the logical Front side of the line.
     */
    inline Side &front() { return side(FRONT); }

    /// @copydoc front()
    inline Side const &front() const { return side(FRONT); }

    /**
     * Returns the logical Back side of the line.
     */
    inline Side &back() { return side(BACK); }

    /// @copydoc back()
    inline Side const &back() const { return side(BACK); }

    /**
     * Returns @c true iff a sector is attributed to the specified side of the line.
     *
     * @param back  If not @c 0 test the Back side; otherwise the Front side.
     */
    inline bool hasSector(int back) const { return side(back).hasSector(); }

    /**
     * Returns @c true iff a sector is attributed to the Front side of the line.
     */
    inline bool hasFrontSector() const { return hasSector(FRONT); }

    /**
     * Returns @c true iff a sector is attributed to the Back side of the line.
     */
    inline bool hasBackSector() const { return hasSector(BACK); }

    /**
     * Convenient accessor method for returning the sector attributed to the
     * specified side of the line.
     *
     * @param back  If not @c 0 return the sector for the Back side; otherwise
     *              the sector of the Front side.
     */
    inline Sector &sector(int back) { return side(back).sector(); }

    /// @copydoc sector()
    inline Sector const &sector(int back) const { return side(back).sector(); }

    /**
     * Convenient accessor method for returning a pointer to the sector attributed
     * to the specified side of the line.
     *
     * @param back  If not @c 0 return the sector for the Back side; otherwise
     *              the sector of the Front side.
     */
    inline Sector *sectorPtr(int back) { return side(back).sectorPtr(); }

    /// @copydoc sector()
    inline Sector const *sectorPtr(int back) const { return side(back).sectorPtr(); }

    /**
     * Returns the sector attributed to the Front side of the line.
     */
    inline Sector &frontSector() { return sector(FRONT); }

    /// @copydoc backSector()
    inline Sector const &frontSector() const { return sector(FRONT); }

    /**
     * Returns the sector attributed to the Back side of the line.
     */
    inline Sector &backSector() { return sector(BACK); }

    /// @copydoc backSector()
    inline Sector const &backSector() const { return sector(BACK); }

    /**
     * Convenient accessor method for returning a pointer to the sector attributed
     * to the front side of the line.
     */
    inline Sector *frontSectorPtr() { return sectorPtr(FRONT); }

    /// @copydoc frontSectorPtr()
    inline Sector const *frontSectorPtr() const { return sectorPtr(FRONT); }

    /**
     * Convenient accessor method for returning a pointer to the sector attributed
     * to the back side of the line.
     */
    inline Sector *backSectorPtr() { return sectorPtr(BACK); }

    /// @copydoc frontSectorPtr()
    inline Sector const *backSectorPtr() const { return sectorPtr(BACK); }

    /**
     * Returns @c true iff the line is considered @em self-referencing.
     * In this context, self-referencing (a term whose origins stem from the
     * DOOM modding community) means a two-sided line (which is to say that
     * a Sector is attributed to both logical sides of the line) where the
     * attributed sectors for each logical side are the same.
     */
    inline bool isSelfReferencing() const
    {
        return L_frontsidedef && L_backsidedef && frontSectorPtr() == backSectorPtr();
    }

    /**
     * Returns the specified edge vertex for the line.
     *
     * @param to  If not @c 0 return the To vertex; otherwise the From vertex.
     */
    Vertex &vertex(int to);

    /// @copydoc vertex()
    Vertex const &vertex(int to) const;

    /**
     * Convenient accessor method for returning the origin of the specified
     * edge vertex for the line.
     *
     * @see vertex()
     */
    inline const_pvec2d_t &vertexOrigin(int to) const
    {
        return vertex(to).origin();
    }

    /**
     * Returns the From/Start vertex for the line.
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
     * vertex for the line.
     *
     * @see v1()
     */
    inline const_pvec2d_t &v1Origin() const { return v1().origin(); }

    /// @copydoc v1Origin()
    /// An alias of v1Origin()
    inline const_pvec2d_t &fromOrigin() const { return v1Origin(); }

    /**
     * Returns the To/End vertex for the line.
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
     * vertex for the line.
     *
     * @see v2()
     */
    inline const_pvec2d_t &v2Origin() const { return v2().origin(); }

    /// @copydoc v2Origin()
    /// An alias of v2Origin()
    inline const_pvec2d_t &toOrigin() const { return v2Origin(); }

    /**
     * On which side of the line does the specified box lie?
     *
     * @param box  Bounding box to test.
     *
     * @return One of the following:
     * - Negative: @a box is entirely on the left side.
     * - Zero: @a box intersects the line.
     * - Positive: @a box is entirely on the right side.
     */
    int boxOnSide(AABoxd const &box) const;

    /**
     * On which side of the line does the specified box lie? The test is
     * carried out using fixed-point math for behavior compatible with
     * vanilla DOOM. Note that this means there is a maximum size for both
     * the bounding box and the line: neither can exceed the fixed-point
     * 16.16 range (about 65k units).
     *
     * @param box  Bounding box to test.
     *
     * @return One of the following:
     * - Negative: @a box is entirely on the left side.
     * - Zero: @a box intersects the line.
     * - Positive: @a box is entirely on the right side.
     */
    int boxOnSide_FixedPrecision(AABoxd const &box) const;

    /**
     * @param offset  Returns the position of the nearest point along the line [0..1].
     */
    coord_t pointDistance(const_pvec2d_t point, coord_t *offset) const;

    inline coord_t pointDistance(coord_t x, coord_t y, coord_t *offset) const
    {
        coord_t point[2] = { x, y };
        return pointDistance(point, offset);
    }

    /**
     * On which side of the line does the specified point lie?
     *
     * @param point  Point in the map coordinate space to test.
     *
     * @return One of the following:
     * - Negative: @a point is to the left/back side.
     * - Zero: @a point lies directly on the line.
     * - Positive: @a point is to the right/front side.
     */
    coord_t pointOnSide(const_pvec2d_t point) const;

    inline coord_t pointOnSide(coord_t x, coord_t y) const
    {
        coord_t point[2] = { x, y };
        return pointOnSide(point);
    }

    /**
     * Configure the specified divline_t by setting the origin point to the
     * line's left (i.e., first) vertex and the direction vector parallel to
     * the line's direction vector.
     *
     * @param divline  divline_t instance to be configured.
     */
    void configureDivline(divline_t &divline) const;

    /**
     * Find the "sharp" Z coordinate range of the opening on @a side. The
     * open range is defined as the gap between foor and ceiling on @a side
     * clipped by the floor and ceiling planes on the back side (if present).
     *
     * @param bottom    Bottom Z height is written here. Can be @c NULL.
     * @param top       Top Z height is written here. Can be @c NULL.
     *
     * @return Height of the open range.
     */
    coord_t openRange(int side, coord_t *bottom, coord_t *top) const;

    /// Same as openRange() but works with the "visual" (i.e., smoothed)
    /// plane height coordinates rather than the "sharp" coordinates.
    coord_t visOpenRange(int side, coord_t *bottom, coord_t *top) const;

    /**
     * Configure the specified TraceOpening according to the opening defined
     * by the inner-minimal plane heights which intercept the line.
     *
     * @param opening  TraceOpening instance to be configured.
     */
    void configureTraceOpening(TraceOpening &opening) const;

    /**
     * Calculate a unit vector parallel to the line.
     *
     * @todo No longer needed (SideDef stores tangent space normals).
     *
     * @param unitVector  Unit vector is written here.
     */
    void unitVector(pvec2f_t unitVec) const;

    /**
     * Update the line's slopetype and map space angle delta according to
     * the points defined by it's vertices.
     */
    void updateSlope();

    /**
     * Update the line's map space axis-aligned bounding box to encompass
     * the points defined by it's vertices.
     */
    void updateAABox();

    /**
     * The DOOM lighting model applies a sector light level delta when drawing
     * line segments based on their 2D world angle.
     *
     * @param side  Side of the line we are interested in.
     * @param deltaL  Light delta for the left edge written here. Can be @c NULL.
     * @param deltaR  Light delta for the right edge written here. Can be @c NULL.
     *
     * @deprecated Now that we store surface tangent space normals use those
     *             rather than angles. @todo Remove me.
     */
    void lightLevelDelta(int side, float *deltaL, float *deltaR) const;

    /**
     * Get a property value, selected by DMU_* name.
     *
     * @param args  Property arguments.
     * @return  Always @c 0 (can be used as an iterator).
     */
    int property(setargs_t &args) const;

    /**
     * Update a property value, selected by DMU_* name.
     *
     * @param args  Property arguments.
     * @return  Always @c 0 (can be used as an iterator).
     */
    int setProperty(setargs_t const &args);
};

#endif // LIBDENG_MAP_LINEDEF
