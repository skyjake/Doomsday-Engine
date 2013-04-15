/** @file bspleaf.h World Map BSP Leaf.
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

#ifndef DENG_WORLD_MAP_BSPLEAF
#define DENG_WORLD_MAP_BSPLEAF

#include <de/aabox.h>
#include <de/vector1.h>

#include <de/Error>

#include "MapElement"
#include "p_dmu.h"
#ifdef __CLIENT__
#  include "render/rend_bias.h"
#endif

class HEdge;
class Sector;
struct polyobj_s;

#ifdef __CLIENT__
struct ShadowLink;
#endif // __CLIENT__

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

#ifdef __CLIENT__
    /// The referenced geometry group does not exist. @ingroup errors
    DENG2_ERROR(UnknownGeometryGroupError);
#endif

    /// The referenced property does not exist. @ingroup errors
    DENG2_ERROR(UnknownPropertyError);

    /// The referenced property is not writeable. @ingroup errors
    DENG2_ERROR(WritePropertyError);

public: /// @todo Make private:
    /// First half-edge in the leaf. Ordered by angle, clockwise starting from
    /// the smallest angle.
    HEdge *_hedge;

    /// Number of HEdge's in the leaf.
    uint _hedgeCount;

#ifdef __CLIENT__

    ShadowLink *_shadows;

    /// [sector->planeCount] size.
    struct biassurface_s **_bsuf;

    uint _reverb[NUM_REVERB_DATA];

#endif // __CLIENT__

public:
    BspLeaf();
    ~BspLeaf();

    /**
     * Returns the axis-aligned bounding box which encompases all the vertexes
     * which define the geometry of the BSP leaf in map coordinate space units.
     */
    AABoxd const &aaBox() const;

    /**
     * Update the BSP leaf's map space axis-aligned bounding box to encompass
     * the points defined by it's vertices.
     */
    void updateAABox();

    /**
     * Returns the point described by the average origin coordinates of all the
     * vertexes which define the geometry of the BSP leaf in map coordinate space
     * units.
     */
    vec2d_t const &center() const;

    /**
     * Update the center point in the map coordinate space.
     *
     * @pre Axis-aligned bounding box must have been initialized.
     */
    void updateCenter();

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
     * Change the sector attributed to the BSP leaf.
     *
     * @param newSector  New sector to be attributed. Can be @c 0.
     *
     * @todo Refactor away.
     */
    void setSector(Sector *newSector);

    /**
     * Returns @c true iff there is at least one polyobj linked with the BSP leaf.
     */
    inline bool hasPolyobj() { return firstPolyobj() != 0; }

    /**
     * Returns a pointer to the first polyobj linked to the BSP leaf; otherwise @c 0.
     */
    struct polyobj_s *firstPolyobj() const;

    /**
     * Change the first polyobj linked to the BSP leaf.
     *
     * @param newPolyobj  New polyobj. Can be @c 0.
     */
    void setFirstPolyobj(struct polyobj_s *newPolyobj);

    /**
     * Returns the original index of the BSP leaf.
     */
    uint origIndex() const;

    void setOrigIndex(uint newOrigIndex);

    /**
     * Returns the @em validCount of the BSP leaf. Used by some legacy iteration
     * algorithms for marking leafs as processed/visited.
     *
     * @todo Refactor away.
     */
    int validCount() const;

    /// @todo Refactor away.
    void setValidCount(int newValidCount);

    /**
     * Update the world grid offset.
     *
     * @pre Axis-aligned bounding box must have been initialized.
     */
    void updateWorldGridOffset();

    /**
     * Returns the vector described by the offset from the map coordinate space
     * origin to the top most, left most point of the geometry of the BSP leaf.
     *
     * @see aaBox()
     */
    vec2d_t const &worldGridOffset() const;

#ifdef __CLIENT__

    /**
     * Returns a pointer to the HEdge of the BSP leaf which has been chosen for
     * use as the base for a triangle fan geometry. May return @c 0 if no suitable
     * base was determined.
     */
    HEdge *fanBase() const;

    /**
     * Retrieve the bias surface for specified geometry @a groupId
     *
     * @param groupId  Geometry group identifier for the bias surface.
     */
    biassurface_t &biasSurfaceForGeometryGroup(uint groupId);

    /**
     * Returns the first ShadowLink associated with the BSP leaf; otherwise @c 0.
     */
    ShadowLink *firstShadowLink() const;

    /**
     * Returns the frame number of the last time sprites were projected for the
     * BSP leaf.
     */
    int addSpriteCount() const;

    /**
     * Change the frame number of the last time sprites were projected for the
     * BSP leaf.
     *
     * @param newFrameCount  New frame number.
     */
    void setAddSpriteCount(int newFrameCount);

#endif // __CLIENT__

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

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_WORLD_MAP_BSPLEAF
