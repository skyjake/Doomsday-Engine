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
 * Two dimensional convex polygon describing a @em leaf in a binary space
 * partition tree (BSP).
 *
 * @see http://en.wikipedia.org/wiki/Binary_space_partitioning
 *
 * @ingroup map
 */
class BspLeaf : public de::MapElement
{
public:
    /// Required sector attribution is missing. @ingroup errors
    DENG2_ERROR(MissingSectorError);

    /// The referenced geometry group does not exist. @ingroup errors
    DENG2_ERROR(UnknownGeometryGroupError);

    /// The referenced property does not exist. @ingroup errors
    DENG2_ERROR(UnknownPropertyError);

    /// The referenced property is not writeable. @ingroup errors
    DENG2_ERROR(WritePropertyError);

public: /// @todo Make private:
    /// First HEdge in this leaf.
    HEdge *_hedge;

    /// @ref bspLeafFlags.
    int _flags;

    /// Unique. Set when saving the BSP.
    uint _index;

    /// Frame number of last R_AddSprites.
    int _addSpriteCount;

    int _validCount;

    /// Number of HEdge's in this leaf.
    uint _hedgeCount;

    Sector *_sector;

    /// First Polyobj in this leaf. Can be @c NULL.
    struct polyobj_s *_polyObj;

    /// HEdge whose vertex to use as the base for a trifan.
    /// If @c NULL then midPoint will be used instead.
    HEdge *_fanBase;

    struct shadowlink_s *_shadows;

    /// Vertex bounding box in the map coordinate space.
    AABoxd _aaBox;

    /// Center of vertices.
    coord_t _center[2];

    /// Offset to align the top left of materials in the built geometry to the
    /// map coordinate space grid.
    coord_t _worldGridOffset[2];

    /// [sector->planeCount] size.
    struct biassurface_s **_bsuf;

    uint _reverb[NUM_REVERB_DATA];

public:
    BspLeaf();
    ~BspLeaf();

    /**
     * Returns the axis-aligned bounding box which encompases all the vertexes
     * which define the geometry of the BSP leaf in map coordinate space units.
     */
    AABoxd const &aaBox() const;

    /**
     * Returns the point described by the average origin coordinates of all the
     * vertexes which define the geometry of the BSP leaf in map coordinate space
     * units.
     */
    vec2d_t const &center() const;

    /**
     * Returns a pointer to the first HEdge of the BSP leaf; otherwise @c 0.
     */
    HEdge *firstHEdge() const;

    /**
     * Returns the total number of half-edges in the BSP leaf.
     */
    uint hedgeCount() const;

    /**
     * Returns @c true iff a sector is attributed to the BSP leaf. The only time
     * a leaf might not be attributed to a sector is if the leaf was @em orphaned
     * by the partitioning algorithm (a bug).
     */
    bool hasSector() const;

    /**
     * Returns the sector attributed to the BSP leaf.
     *
     * @see hasSector()
     */
    Sector &sector() const;

    /**
     * Returns a pointer to the sector attributed to the BSP leaf; otherwise @c 0.
     *
     * @see hasSector()
     */
    inline Sector *sectorPtr() const { return hasSector()? &sector() : 0; }

    /**
     * Returns @c true iff there is at least one polyobj linked with the BSP leaf.
     */
    inline bool hasPolyobj() { return !!firstPolyobj(); }

    /**
     * Returns a pointer to the first polyobj linked to the BSP leaf; otherwise @c 0.
     */
    struct polyobj_s *firstPolyobj() const;

    /**
     * Returns the @ref bspLeafFlags of the BSP leaf.
     */
    int flags() const;

    /**
     * Returns the original index of the BSP leaf.
     */
    uint origIndex() const;

    /**
     * Returns the frame number of the last time sprites were projected for the
     * BSP leaf.
     */
    int addSpriteCount() const;

    /**
     * Returns the @em validCount of the BSP leaf. Used by some legacy iteration
     * algorithms for marking leafs as processed/visited.
     *
     * @todo Refactor me away.
     */
    int validCount() const;

    /**
     * Retrieve the bias surface for specified geometry @a groupId
     *
     * @param groupId  Geometry group identifier for the bias surface.
     */
    biassurface_t &biasSurfaceForGeometryGroup(uint groupId);

    /**
     * Update the BSP leaf's map space axis-aligned bounding box to encompass
     * the points defined by it's vertices.
     */
    void updateAABox();

    /**
     * Update the center point in the map coordinate space.
     *
     * @pre Axis-aligned bounding box must have been initialized.
     */
    void updateCenter();

    /**
     * Update the world grid offset.
     *
     * @pre Axis-aligned bounding box must have been initialized.
     */
    void updateWorldGridOffset();

    /**
     * Returns a pointer to the HEdge of the BSP lead which has been chosen for
     * use as the base for a triangle fan geometry. May return @c 0 if no suitable
     * base is configured.
     */
    HEdge *fanBase() const;

    /**
     * Returns the first shadowlink_t associated with the BSP leaf; otherwise @c 0.
     */
    struct shadowlink_s *firstShadowLink() const;

    /**
     * Returns the vector described by the offset from the map coordinate space
     * origin to the top most, left most point of the geometry of the BSP leaf.
     *
     * @see aaBox()
     */
    vec2d_t const &worldGridOffset() const;

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
