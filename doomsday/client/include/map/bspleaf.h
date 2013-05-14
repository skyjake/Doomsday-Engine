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

#include <de/Error>
#include <de/Vector>

#include "MapElement"
#include "Face"

class HEdge;
class Sector;
struct polyobj_s;

#ifdef __CLIENT__
struct BiasSurface;
struct ShadowLink;
#endif // __CLIENT__

/**
 * Map data element describing a @em leaf in a binary space partition tree (BSP)
 * (a two dimensional convex polygon).
 *
 * @see http://en.wikipedia.org/wiki/Binary_space_partitioning
 *
 * @ingroup map
 */
class BspLeaf : public de::MapElement, public de::Face
{
public:
    /// Required sector attribution is missing. @ingroup errors
    DENG2_ERROR(MissingSectorError);

#ifdef __CLIENT__
    /// The referenced geometry group does not exist. @ingroup errors
    DENG2_ERROR(UnknownGeometryGroupError);
#endif

public: /// @todo Make private:
#ifdef __CLIENT__

    ShadowLink *_shadows;

    /// Sector::planeCount() size.
    BiasSurface **_bsuf;

    uint _reverb[NUM_REVERB_DATA];

#endif // __CLIENT__

public:
    BspLeaf();
    ~BspLeaf();

    /**
     * Returns @c true iff the map geometry Face for the BSP leaf is "degenerate",
     * which is to say it is composed of fewer than @em three half-edges.
     */
    bool hasDegenerateFace() const;

    /**
     * Returns the axis-aligned bounding box which encompases all the vertexes
     * which define the geometry of the Face for for BSP leaf in map coordinate
     * space units.
     */
    AABoxd const &aaBox() const;

    /**
     * Update the BSP leaf's map space axis-aligned bounding box to encompass
     * the points defined by the vertexes of the map geometry Face.
     */
    void updateAABox();

    /**
     * Returns the point described by the average origin coordinates of all the
     * vertexes which define the Face of the BSP leaf in map coordinate space
     * units.
     */
    de::Vector2d const &center() const;

    /**
     * Update the center point in the map coordinate space.
     *
     * @pre Axis-aligned bounding box must have been initialized.
     */
    void updateCenter();

    /**
     * Returns @c true iff a sector is attributed to the BSP leaf. The only time
     * a leaf might not be attributed to a sector is if the map geometry was
     * @em orphaned by the partitioning algorithm (a bug).
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
     * Determines whether the BSP leaf has a positive world volume. For this to
     * be true the following criteria must be met:
     *
     * - The Face geometry is @em not degenerate (see @ref hasDegenerateFace()).
     * - A sector is attributed (see @ref hasSector())
     * - The height of floor is lower than that of the ceiling plane for the
     *   attributed sector.
     *
     * @param useVisualHeights  @c true= use the visual (i.e., smoothed) plane
     *                          heights instead of the @em sharp heights.
     */
    bool hasWorldVolume(bool useVisualHeights = true) const;

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
    de::Vector2d const &worldGridOffset() const;

    /**
     * Returns the @em validCount of the BSP leaf. Used by some legacy iteration
     * algorithms for marking leafs as processed/visited.
     *
     * @todo Refactor away.
     */
    int validCount() const;

    /// @todo Refactor away.
    void setValidCount(int newValidCount);

#ifdef __CLIENT__

    /**
     * Returns a pointer to the HEdge of the BSP leaf which has been chosen for
     * use as the base for a triangle fan geometry. May return @c 0 if no suitable
     * base was determined.
     */
    HEdge *fanBase() const;

    /**
     * Returns the number of vertices needed for the BSP leaf's triangle fan.
     * @note May incurr updating the fan base HEdge if not already determined.
     *
     * @see fanBase()
     */
    int numFanVertices() const;

    /**
     * Retrieve the bias surface for specified geometry @a groupId
     *
     * @param groupId  Geometry group identifier for the bias surface.
     */
    BiasSurface &biasSurfaceForGeometryGroup(int groupId);

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

#ifdef DENG_DEBUG
    void printFaceGeometry() const;
#endif

protected:
    int property(setargs_t &args) const;

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_WORLD_MAP_BSPLEAF
