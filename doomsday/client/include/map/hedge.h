/** @file hedge.h Map Geometry Half-Edge.
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

#ifndef LIBDENG_MAP_HEDGE
#define LIBDENG_MAP_HEDGE

#include "MapElement"
#include "resource/r_data.h"
#include "p_dmu.h"
#include "sector.h"
#include "render/walldiv.h"
#include "render/rend_bias.h"
#include <de/Error>

// Helper macros for accessing hedge data elements.
#define FRONT 0
#define BACK  1

#define HE_v(n)                   v[(n)? 1:0]
#define HE_vorigin(n)             HE_v(n)->origin()

#define HE_v1                     HE_v(0)
#define HE_v1origin               HE_v(0)->origin()

#define HE_v2                     HE_v(1)
#define HE_v2origin               HE_v(1)->origin()

#define HEDGE_BACK_SECTOR(h)      ((h)->twin ? (h)->twin->sector : NULL)

#define HEDGE_SIDE(h)             ((h)->line ? &(h)->line->side((h)->side) : NULL)
#define HEDGE_SIDEDEF(h)          ((h)->line ? (h)->line->sideDefPtr((h)->side) : NULL)

// HEdge frame flags
#define HEDGEINF_FACINGFRONT      0x0001

class Vertex;

/**
 * Half-Edge.
 *
 * @ingroup map
 */
class HEdge : public de::MapElement
{
public:
    /// The referenced property does not exist. @ingroup errors
    DENG2_ERROR(UnknownPropertyError);

    /// The referenced property is not writeable. @ingroup errors
    DENG2_ERROR(WritePropertyError);

public: /// @todo Make private:
    /// [Start, End] of the segment.
    Vertex *v[2];

    HEdge *next;

    HEdge *prev;

    /// Half-edge on the other side, or NULL if one-sided. This relationship
    /// is always one-to-one -- if one of the half-edges is split, the twin
    /// must also be split.
    HEdge *twin;

    BspLeaf *bspLeaf;

    LineDef *line;

    Sector *sector;

    angle_t angle;

    /// On which side of the LineDef (0=front, 1=back)?
    int side;

    /// Accurate length of the segment (v1 -> v2).
    coord_t length;

    coord_t offset;

    /// For each @ref SideDefSection.
    biassurface_t *bsuf[3];

    short frameFlags;

    /// Unique. Set when saving the BSP.
    uint index;

public:
    HEdge();
    HEdge(HEdge const &other);
    ~HEdge();

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
     * @param section        Section to prepare divisions for.
     * @param frontSector    Sector to use for the front side.
     * @param backSector     Sector to use for the back side.
     * @param leftWallDivs   Division data for the left edge is written here.
     * @param rightWallDivs  Division data for the right edge is written here.
     * @param matOffset      Material offset data is written here.
     *
     * @return  @c true if divisions were prepared (the specified @a section has a
     *          non-zero Z axis height).
     */
    bool prepareWallDivs(SideDefSection section, Sector *frontSector, Sector *backSector,
        walldivs_t *leftWallDivs, walldivs_t *rightWallDivs, pvec2f_t matOffset) const;

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

#endif // LIBDENG_MAP_HEDGE
