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
#define L_v(n)                  v[(n)? 1:0]
#define L_vorigin(n)            v[(n)? 1:0]->origin()

#define L_v1                    L_v(0)
#define L_v1origin              L_v(0)->origin()

#define L_v2                    L_v(1)
#define L_v2origin              L_v(1)->origin()

#define L_vo(n)                 vo[(n)? 1:0]
#define L_vo1                   L_vo(0)
#define L_vo2                   L_vo(1)

#define L_frontside             sides[0]
#define L_backside              sides[1]
#define L_side(n)               sides[(n)? 1:0]

#define L_sidedef(n)            L_side(n).sideDef
#define L_frontsidedef          L_sidedef(FRONT)
#define L_backsidedef           L_sidedef(BACK)

#define L_sector(n)             L_side(n).sector
#define L_frontsector           L_sector(FRONT)
#define L_backsector            L_sector(BACK)
///@}

// Is this line self-referencing (front sec == back sec)?
#define LINE_SELFREF(l)         ((l)->L_frontsidedef && (l)->L_backsidedef && \
                                 (l)->L_frontsector == (l)->L_backsector)

// Internal flags:
#define LF_POLYOBJ              0x1 ///< Line is part of a polyobject.
#define LF_BSPWINDOW            0x2 ///< Line produced a BSP window. @todo Refactor away.

/**
 * @defgroup sideSectionFlags  Side Section Flags
 * @ingroup map
 */
///@{
#define SSF_MIDDLE          0x1
#define SSF_BOTTOM          0x2
#define SSF_TOP             0x4
///@}

typedef struct lineside_s {
    /// Sector on this side.
    Sector *sector;

    /// SideDef on this side.
    SideDef *sideDef;

    /// Left-most HEdge on this side.
    HEdge *hedgeLeft;

    /// Right-most HEdge on this side.
    HEdge *hedgeRight;

    /// Framecount of last time shadows were drawn on this side.
    ushort shadowVisFrame;
} lineside_t;

/**
 * Map line.
 *
 * @ingroup map
 */
class LineDef : public de::MapElement
{
public:
    /// The referenced property does not exist. @ingroup errors
    DENG2_ERROR(UnknownPropertyError);

    /// The referenced property is not writeable. @ingroup errors
    DENG2_ERROR(WritePropertyError);

public: /// @todo make private:
    Vertex *v[2];

    /// Links to vertex line owner nodes [left, right].
    LineOwner *vo[2];

    lineside_t sides[2];

    /// Public DDLF_* flags.
    int flags;

    /// Internal LF_* flags.
    byte inFlags;

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
     * On which side of the line does the specified box lie?
     *
     * @param box   Bounding box.
     *
     * @return  @c <0= bbox is wholly on the left side.
     *          @c  0= line intersects bbox.
     *          @c >0= bbox wholly on the right side.
     */
    int boxOnSide(AABoxd const *box) const;

    /**
     * On which side of the line does the specified box lie? The test is
     * carried out using fixed-point math for behavior compatible with
     * vanilla DOOM. Note that this means there is a maximum size for both
     * the bounding box and the line: neither can exceed the fixed-point
     * 16.16 range (about 65k units).
     *
     * @param box  Bounding box.
     *
     * @return One of the following:
     * - Negative: bbox is entirely on the left side.
     * - Zero: line intersects bbox.
     * - Positive: bbox isentirely on the right side.
     */
    int boxOnSide_FixedPrecision(AABoxd const *box) const;

    /**
     * @param offset  Returns the position of the nearest point along the line [0..1].
     */
    coord_t pointDistance(coord_t const point[2], coord_t *offset) const;

    inline coord_t LineDef::pointDistance(coord_t x, coord_t y, coord_t *offset) const
    {
        coord_t point[2] = { x, y };
        return pointDistance(point, offset);
    }

    /**
     * On which side of the line does the specified point lie?
     *
     * @param xy  Map space point to test.
     *
     * @return @c <0 Point is to the left/back of the line.
     *         @c =0 Point lies directly on the line.
     *         @c >0 Point is to the right/front of the line.
     */
    coord_t pointOnSide(coord_t const point[2]) const;

    inline coord_t LineDef::pointOnSide(coord_t x, coord_t y) const
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
    void setDivline(divline_t *divline) const;

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
    void setTraceOpening(TraceOpening *opening) const;

    /**
     * Calculate a unit vector parallel to the line.
     *
     * @todo No longer needed (SideDef stores tangent space normals).
     *
     * @param unitVector  Unit vector is written here.
     */
    void unitVector(float *unitVec) const;

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
     * @param deltaL  Light delta for the left edge written here.
     * @param deltaR  Light delta for the right edge written here.
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
