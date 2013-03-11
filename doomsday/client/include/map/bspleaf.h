/** @file bspleaf.h Map BSP Leaf.
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

#ifndef LIBDENG_MAP_BSPLEAF
#define LIBDENG_MAP_BSPLEAF

#include "MapElement"
#include "resource/r_data.h"
#include "render/rend_bias.h"
#include "p_mapdata.h"
#include "p_dmu.h"
#include <de/Error>

/**
 * @defgroup bspLeafFlags  Bsp Leaf Flags
 * @ingroup flags
 */
///@{
#define BLF_UPDATE_FANBASE      0x1 ///< The tri-fan base requires an update.
///@}

class Sector;
class HEdge;

/**
 * Map BSP leaf.
 *
 * @ingroup map
 */
class BspLeaf : public de::MapElement
{
public:
    /// The referenced geometry group does not exist. @ingroup errors
    DENG2_ERROR(UnknownGeometryGroupError);

    /// The referenced property does not exist. @ingroup errors
    DENG2_ERROR(UnknownPropertyError);

    /// The referenced property is not writeable. @ingroup errors
    DENG2_ERROR(WritePropertyError);

public: /// @todo Make private:
    /// First HEdge in this leaf.
    HEdge *hedge;

    /// @ref bspLeafFlags.
    int flags;

    /// Unique. Set when saving the BSP.
    uint index;

    /// Frame number of last R_AddSprites.
    int addSpriteCount;

    int validCount;

    /// Number of HEdge's in this leaf.
    uint hedgeCount;

    Sector *sector;

    /// First polyobj in this leaf. Can be @c NULL.
    struct polyobj_s *polyObj;

    /// HEdge whose vertex to use as the base for a trifan. If @c NULL then midPoint is used instead.
    HEdge *fanBase;

    struct shadowlink_s *shadows;

    /// Vertex bounding box in the map coordinate space.
    AABoxd aaBox;

    /// Center of vertices.
    coord_t midPoint[2];

    /// Offset to align the top left of materials in the built geometry to the map coordinate space grid.
    coord_t worldGridOffset[2];

    /// [sector->planeCount] size.
    struct biassurface_s **bsuf;

    uint reverb[NUM_REVERB_DATA];

public:
    BspLeaf();
    ~BspLeaf();

    /**
     * Retrieve the bias surface for specified geometry @a groupId
     *
     * @param groupId  Geometry group identifier for the bias surface.
     */
    biassurface_t &biasSurfaceForGeometryGroup(uint groupId);

    /**
     * Update the BspLeaf's map space axis-aligned bounding box to encompass
     * the points defined by it's vertices.
     */
    void updateAABox();

    /**
     * Update the mid point in the map coordinate space.
     *
     * @pre Axis-aligned bounding box must have been initialized.
     */
    void updateMidPoint();

    /**
     * Update the world grid offset.
     *
     * @pre Axis-aligned bounding box must have been initialized.
     */
    void updateWorldGridOffset();

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

#endif // LIBDENG_MAP_BSPLEAF
