/** @file bspleaf.h World map BSP leaf.
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

#include <QSet>

#include <de/Error>
#include <de/Vector>

#include "MapElement"
#include "Line"
#include "Sector"

#include "Mesh"

#ifdef __CLIENT__
#  include "BiasSurface"
#endif

struct polyobj_s;
#ifdef __CLIENT__
class BiasDigest;
#endif

/**
 * Represents a leaf in the map's binary space partition (BSP) tree. Each leaf
 * defines a two dimensioned convex subspace region (which, may be represented
 * by a face (polygon) in the map's half-edge @ref de::Mesh geometry).
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
#ifdef __CLIENT__
, public BiasSurface
#endif
{
    DENG2_NO_COPY  (BspLeaf)
    DENG2_NO_ASSIGN(BspLeaf)

public:
    /// Required sector cluster attribution is missing. @ingroup errors
    DENG2_ERROR(MissingClusterError);

    /// An invalid polygon was specified @ingroup errors
    DENG2_ERROR(InvalidPolyError);

    /// Required polygon geometry is missing. @ingroup errors
    DENG2_ERROR(MissingPolyError);

    /*
     * Linked-element lists/sets:
     */
    typedef QSet<de::Mesh *>   Meshes;
    typedef QSet<polyobj_s *>  Polyobjs;
#ifdef __CLIENT__
    typedef QSet<Line::Side *> ShadowLines;
#endif

#ifdef __CLIENT__
    // Final audio environment characteristics.
    typedef uint AudioEnvironmentFactors[NUM_REVERB_DATA];
#endif

public:
    /**
     * Construct a new BSP leaf and optionally attribute it to @a sector.
     * Ownership is unaffected.
     */
    explicit BspLeaf(Sector *sector = 0);

    /**
     * Returns @c true iff the BSP leaf is "degenerate", which is to say that
     * no convex face geometry is attributed.
     *
     * Equivalent to @code !hasPoly() @endcode
     */
    inline bool isDegenerate() const { return !hasPoly(); }

    /**
     * Determines whether a convex face geometry (a polygon) is attributed.
     *
     * @see poly(), setPoly()
     */
    bool hasPoly() const;

    /**
     * Provides access to the attributed convex face geometry (a polygon).
     *
     * @see hasPoly(), setPoly()
     */
    de::Face const &poly() const;

    /**
     * Change the convex face geometry attributed to the BSP leaf. Before the
     * geometry is accepted it is first conformance tested to ensure that it
     * represents a valid, simple convex polygon.
     *
     * @param polygon  New polygon to attribute to the BSP leaf. Ownership is
     *                 unaffected. Can be @c 0 (to clear the attribution).
     *
     * @see hasPoly(), poly()
     */
    void setPoly(de::Face *polygon);

    /**
     * Assign an additional mesh geometry to the BSP leaf. Such @em extra
     * meshes are used to represent geometry which would otherwise result in
     * a non-manifold mesh if incorporated in the primary mesh for the map.
     *
     * @param mesh  New mesh to be assigned to the BSP leaf. Ownership of the
     *              mesh is given to the BspLeaf.
     */
    void assignExtraMesh(de::Mesh &mesh);

    /**
     * Provides access to the set of 'extra' mesh geometries for the BSP leaf.
     *
     * @see assignExtraMesh()
     */
    Meshes const &extraMeshes() const;

    /**
     * Returns @c true iff a sector cluster is attributed to the BSP leaf. The
     * only time a leaf might not be attributed to a sector is if the map geometry
     * was @em orphaned by the partitioning algorithm (a bug).
     */
    bool hasCluster() const;

    /// @copydoc hasCluster()
    inline bool hasSector() const { return hasCluster(); }

    /**
     * Returns the sector cluster attributed to the BSP leaf.
     *
     * @see hasCluster()
     */
    Sector::Cluster &cluster() const;

    /**
     * Convenient method of returning the sector of the cluster attributed to
     * the BSP leaf.
     *
     * @see hasSector(), cluster()
     */
    inline Sector &sector() const { return cluster().sector(); }

    /**
     * Convenient method returning a pointer to the sector of the cluster
     * attributed to the BSP leaf. If not attributed then @c 0 is returned.
     *
     * @see hasSector(), sector()
     */
    inline Sector *sectorPtr() const { return hasSector()? &sector() : 0; }

    /**
     * Change the sector cluster attributed to the BSP leaf.
     *
     * @param newCluster New sector cluster to attribute to the BSP leaf.
     *                   Ownership is unaffected. Can be @c 0 (to clear the
     *                   attribution).
     *
     * @see hasCluster(), cluster()
     */
    void setCluster(Sector::Cluster *newCluster);

    /**
     * Returns the identified @em physical plane of the parent sector. Naturally
     * a sector must be attributed for this. Note that this is not the same as
     * the "visual" plane which may well be defined by another sector.
     *
     * @see hasSector()
     *
     * @param planeIndex  Index of the plane to return.
     */
    inline Plane &plane(int planeIndex) const { return cluster().plane(planeIndex); }

    /**
     * Returns the sector plane which defines the physical floor of the BSP leaf.
     * @see plane(), visFloor()
     */
    inline Plane &floor() const { return plane(Sector::Floor); }

    /**
     * Returns the sector plane which defines the physical ceiling of the BSP leaf.
     * @see plane(), visCeiling()
     */
    inline Plane &ceiling() const { return plane(Sector::Ceiling); }

    /**
     * Returns the identified @em visual sector plane for the BSP leaf (which may
     * or may not be the same as the physical plane). Note that a sector must be
     * attributed to "this" BSP leaf.
     *
     * @see hasSector()
     *
     * @param planeIndex  Index of the plane to return.
     */
    inline Plane &visPlane(int planeIndex) const { return cluster().visPlane(planeIndex); }

    /**
     * Returns the sector plane which defines the @em visual floor of the BSP leaf.
     * @see hasSector(), floor()
     */
    inline Plane &visFloor() const { return visPlane(Sector::Floor); }

    /**
     * Returns the sector plane which defines the @em visual ceiling of the BSP leaf.
     * @see hasSector(), ceiling()
     */
    inline Plane &visCeiling() const { return visPlane(Sector::Ceiling); }

#ifdef __CLIENT__

    /**
     * Convenient method of accessing the visual (i.e., smoothed) height of the
     * @em visual floor plane.
     * @see visFloor()
     */
    inline coord_t visFloorHeight() const   { return visFloor().visHeight(); }

    /**
     * Convenient method of accessing the visual (i.e., smoothed) height of the
     * @em visual ceiling plane.
     * @see visCeiling()
     */
    inline coord_t visCeilingHeight() const { return visCeiling().visHeight(); }

#endif // __CLIENT__

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
     * Convenient method of returning the total number of polyobjs linked to the
     * BSP leaf.
     */
    inline int polyobjCount() { return polyobjs().count(); }

    /**
     * Returns the vector described by the offset from the map coordinate space
     * origin to the top most, left most point of the geometry of the BSP leaf.
     *
     * @see aaBox()
     */
    de::Vector2d const &worldGridOffset() const;

    /**
     * Determines whether the specified @a point in the map coordinate space
     * lies within the BSP leaf (according to the edges).
     *
     * @param point  Map space coordinate to test.
     *
     * @return  @c true iff the point lies inside the BSP leaf.
     *
     * @see http://www.alienryderflex.com/polygon/
     */
    bool pointInside(de::Vector2d const &point) const;

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
     * Perform bias lighting for the supplied geometry.
     *
     * @important It is assumed there are least @ref numFanVertices() elements!
     *
     * @param group        Geometry group identifier.
     * @param posCoords    World coordinates for each vertex.
     * @param colorCoords  Final lighting values will be written here.
     */
    void lightBiasPoly(int group, de::Vector3f const *posCoords,
                       de::Vector4f *colorCoords);

    void updateBiasAfterGeometryMove(int group);

    /**
     * Apply bias lighting changes to @em all map element geometries at this
     * leaf of the BSP.
     *
     * @param changes  Digest of lighting changes to be applied.
     */
    void applyBiasDigest(BiasDigest &changes);

    /**
     * Recalculate the environmental audio characteristics (reverb) of the BSP leaf.
     */
    bool updateReverb();

    /**
     * Provides access to the final environmental audio characteristics (reverb)
     * of the BSP leaf, for efficient accumulation.
     */
    AudioEnvironmentFactors const &reverb() const;

    /**
     * Clear the list of fake radio shadow line sides for the BSP leaf.
     */
    void clearShadowLines();

    /**
     * Add the specified line @a side to the set of fake radio shadow lines for
     * the BSP leaf. If the line is already present in this set then nothing
     * will happen.
     *
     * @param side  Map line side to add to the set.
     */
    void addShadowLine(Line::Side &side);

    /**
     * Provides access to the set of fake radio shadow lines for the BSP leaf.
     */
    ShadowLines const &shadowLines() const;

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
