/** @file world/bspleaf.h World BSP Leaf.
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

#ifndef DENG_WORLD_BSPLEAF_H
#define DENG_WORLD_BSPLEAF_H

#include <QList>
#include <QSet>

#include <de/Error>
#include <de/Vector>

#include "MapElement"
#include "Line"
#include "Mesh"

class Sector;
class Segment;
struct polyobj_s;

#ifdef __CLIENT__
struct BiasSurface;
struct ShadowLink;
#endif

/**
 * Represents a leaf in the map's binary space partition (BSP) tree. Each leaf
 * defines a two dimensioned convex subspace region (which, may be represented
 * by a face (polygon) in the map's half-edge @ref Mesh geometry).
 *
 * On client side a leaf also provides / links to various geometry data assets
 * and properties used to visualize the subspace.
 *
 * Each leaf is attributed to a @ref Sector in the map (with the exception of
 * wholly degenerate subspaces which may occur during the partitioning process).
 *
 * @see http://en.wikipedia.org/wiki/Binary_space_partitioning
 *
 * @ingroup world
 */
class BspLeaf : public de::MapElement
{
public:
    /// An invalid polygon was specified @ingroup errors
    DENG2_ERROR(InvalidPolygonError);

    /// Required face geometry is missing. @ingroup errors
    DENG2_ERROR(MissingFaceError);

    /// Required sector attribution is missing. @ingroup errors
    DENG2_ERROR(MissingSectorError);

#ifdef __CLIENT__
    /// The referenced geometry group does not exist. @ingroup errors
    DENG2_ERROR(UnknownGeometryGroupError);
#endif

    typedef QSet<polyobj_s *> Polyobjs;
    typedef QList<Segment *>  Segments;

public: /// @todo Make private:
#ifdef __CLIENT__

    ShadowLink *_shadows;

    uint _reverb[NUM_REVERB_DATA];

#endif // __CLIENT__

public:
    /**
     * Construct a new BSP leaf and optionally attribute it to @a sector.
     * Ownership is unaffected.
     */
    explicit BspLeaf(Sector *sector = 0);

    /**
     * Returns @c true iff the BSP leaf is "degenerate", which is to say there
     * is no convex Polygon assigned to it.
     *
     * Equivalent to @code !hasFace() @endcode
     */
    inline bool isDegenerate() const { return !hasFace(); }

    /**
     * Determines whether a convex face geometry (a polygon) is assigned.
     *
     * @see face(), setFace()
     */
    bool hasFace() const;

    /**
     * Provides access to the assigned convex face geometry (a polygon).
     *
     * @see hasFace(), assignPoly()
     */
    de::Face const &face() const;

    /**
     * Change the convex face geometry assigned to the BSP leaf. Before the
     * geometry is accepted it is first conformance tested to ensure that it
     * represents a valid, simple convex polygon.
     *
     * @param polygon  New polygon to be assigned to the BSP leaf. Ownership
     *                 of the polygon is given to the BspLeaf if it passes all
     *                 conformance checks.
     *
     * @see hasFace(), face()
     */
    void assignPoly(de::Mesh *polygon);

    /**
     * Assign an additional mesh geometry to the BSP leaf. Such @em extra
     * meshes are used to represent geometry which would otherwise result in a
     * non-manifold mesh if incorporated in the primary mesh for the map.
     *
     * @param mesh  New mesh to be assigned to the BSP leaf. Ownership of the
     *              mesh is given to the BspLeaf.
     */
    void assignExtraMesh(de::Mesh &mesh);

    /**
     * Provides a clockwise ordered list of all the line segments which comprise
     * the convex face geometry (a polygon) assigned to the BSP leaf.
     *
     * @see allSegments(), assignPoly()
     */
    Segments const &clockwiseSegments() const;

    /**
     * Provides a list of all the line segments from the convex face geometry
     * and any @em extra meshes assigned to the BSP leaf.
     *
     * @see clockwiseSegments(), assignExtraMesh()
     */
    Segments const &allSegments() const;

    /**
     * Returns @c true iff a sector is attributed to the BSP leaf. The only
     * time a leaf might not be attributed to a sector is if the map geometry
     * was @em orphaned by the partitioning algorithm (a bug).
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
     * @param newSector  New sector to be attributed. Ownership is unaffected.
     *                   Can be @c 0 (to clear the attribution).
     *
     * @todo Refactor away.
     */
    void setSector(Sector *newSector);

#ifdef __CLIENT__

    /**
     * Determines whether the BSP leaf has a positive world volume. For this
     * to be true the following criteria must be met:
     *
     * - The polygon geometry is @em not degenerate (see @ref isDegenerate()).
     * - A sector is attributed (see @ref hasSector())
     * - The height of floor is lower than that of the ceiling plane for the
     *   attributed sector.
     *
     * @param useVisualHeights  @c true= use the visual (i.e., smoothed) plane
     *                          heights instead of the @em sharp heights.
     */
    bool hasWorldVolume(bool useVisualHeights = true) const;

#endif // __CLIENT__

    /**
     * Returns @c true iff at least one polyobj is linked to the BSP leaf.
     */
    inline bool hasPolyobj() { return !polyobjs().isEmpty(); }

    /**
     * Add the given @a polyobj to the set of those linked to the BSP leaf.
     * Ownership is unaffected. If the polyobj is already linked in this set
     * then nothing will happen.
     */
    void addOnePolyobj(struct polyobj_s const &polyobj);

    /**
     * Remove the given @a polyobj from the set of those linked to the BSP leaf.
     *
     * @return  @c true= @a polyobj was linked and subsequently removed.
     */
    bool removeOnePolyobj(polyobj_s const &polyobj);

    /**
     * Provides access to the set of polyobjs linked to the BSP leaf.
     */
    Polyobjs const &polyobjs() const;

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
     * Returns a pointer to the face geometry half-edge which has been chosen
     * for use as the base for a triangle fan GL primitive. May return @c 0 if
     * no suitable base was determined.
     */
    de::HEdge *fanBase() const;

    /**
     * Returns the number of vertices needed for a triangle fan GL primitive.
     *
     * @note When first called after a face geometry is assigned a new 'base'
     * half-edge for the triangle fan primitive will be determined.
     *
     * @see fanBase()
     */
    int numFanVertices() const;

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
     *                     Ownership is given to the BSP leaf.
     */
    void setBiasSurface(int group, BiasSurface *biasSurface);

    /**
     * Returns a pointer to the first ShadowLink; otherwise @c 0.
     */
    ShadowLink *firstShadowLink() const;

    /**
     * Returns the frame number of the last time mobj sprite projection was
     * performed for the BSP leaf.
     */
    int addSpriteCount() const;

    /**
     * Change the frame number of the last time mobj sprite projection was
     * performed for the BSP leaf.
     *
     * @param newFrameCount  New frame number.
     */
    void setAddSpriteCount(int newFrameCount);

#endif // __CLIENT__

protected:
    int property(DmuArgs &args) const;

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_WORLD_BSPLEAF_H
