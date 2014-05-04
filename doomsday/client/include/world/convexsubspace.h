/** @file convexsubspace.h  World map convex subspace.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_WORLD_CONVEXSUBSPACE_H
#define DENG_WORLD_CONVEXSUBSPACE_H

#include <QSet>
#include <de/Error>
#include <de/Vector>

#include "Mesh"

#include "MapElement"
#include "Line"
#include "Sector"

class SectorCluster;
struct polyobj_s;
#ifdef __CLIENT__
class Lumobj;
#endif

/**
 * @ingroup world
 */
class ConvexSubspace
{
public:
    /// An invalid polygon was specified. @ingroup errors
    DENG2_ERROR(InvalidPolyError);

    /// Required sector cluster attribution is missing. @ingroup errors
    DENG2_ERROR(MissingClusterError);

    /// Linked-element lists/sets:
    typedef QSet<de::Mesh *>  Meshes;
    typedef QSet<polyobj_s *> Polyobjs;

#ifdef __CLIENT__
    typedef QSet<Lumobj *>    Lumobjs;
    typedef QSet<LineSide *>  ShadowLines;

    // Final audio environment characteristics.
    typedef uint AudioEnvironmentFactors[NUM_REVERB_DATA];
#endif

public:
    /**
     * Attempt to construct a ConvexSubspace from the Face geometry provided.
     * Before the geometry is accepted it is first conformance tested to ensure
     * that it is a simple convex polygon.
     *
     * @param poly  Polygon to construct from. Ownership is unaffected.
     */
    static ConvexSubspace *newFromConvexPoly(de::Face &poly);

    /**
     * Provides access to the attributed convex face geometry (a polygon).
     */
    de::Face &poly() const;

    /**
     * Determines whether the specified @a point in the map coordinate space
     * lies inside the polygon geometry of the BSP leaf on the XY plane. If no
     * face is attributed with @ref setPoly() @c false is returned.
     *
     * @param point  Map space coordinate to test.
     *
     * @return  @c true iff the point lies inside the BSP leaf's geometry.
     *
     * @see http://www.alienryderflex.com/polygon/
     */
    bool contains(de::Vector2d const &point) const;

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
     * Remove the given @a polyobj from the set of those linked to the BSP leaf.
     *
     * @return  @c true= @a polyobj was linked and subsequently removed.
     */
    bool unlink(polyobj_s const &polyobj);

    /**
     * Add the given @a polyobj to the set of those linked to the BSP leaf.
     * Ownership is unaffected. If the polyobj is already linked in this set
     * then nothing will happen.
     */
    void link(struct polyobj_s const &polyobj);

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
     * Returns @c true iff a sector cluster is attributed to the BSP leaf. The
     * only time a leaf might not be attributed to a sector is if the geometry
     * was @em orphaned by the partitioning algorithm (a bug).
     */
    bool hasCluster() const;

    /**
     * Change the sector cluster attributed to the BSP leaf.
     *
     * @param newCluster New sector cluster to attribute to the BSP leaf.
     *                   Ownership is unaffected. Can be @c 0 (to clear the
     *                   attribution).
     *
     * @see hasCluster(), cluster()
     */
    void setCluster(SectorCluster *newCluster);

    /**
     * Returns the sector cluster attributed to the BSP leaf.
     *
     * @see hasCluster()
     */
    SectorCluster &cluster() const;

    /**
     * Convenient method returning a pointer to the sector cluster attributed to
     * the BSP leaf. If not attributed then @c 0 is returned.
     *
     * @see hasCluster(), cluster()
     */
    inline SectorCluster *clusterPtr() const {
        return hasCluster()? &cluster() : 0;
    }

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
    void addShadowLine(LineSide &side);

    /**
     * Provides access to the set of fake radio shadow lines for the BSP leaf.
     */
    ShadowLines const &shadowLines() const;

    /**
     * Clear all lumobj links for the BSP leaf.
     */
    void unlinkAllLumobjs();

    /**
     * Unlink the specified @a lumobj in the BSP leaf. If the lumobj is not
     * linked then nothing will happen.
     *
     * @param lumobj  Lumobj to unlink.
     *
     * @see link()
     */
    void unlink(Lumobj &lumobj);

    /**
     * Link the specified @a lumobj in the BSP leaf. If the lumobj is already
     * linked then nothing will happen.
     *
     * @param lumobj  Lumobj to link.
     *
     * @see lumobjs(), unlink()
     */
    void link(Lumobj &lumobj);

    /**
     * Provides access to the set of lumobjs linked to the BSP leaf.
     *
     * @see linkLumobj(), clearLumobjs()
     */
    Lumobjs const &lumobjs() const;

    /**
     * Returns the frame number of the last time mobj sprite projection was
     * performed for the BSP leaf.
     */
    int lastSpriteProjectFrame() const;

    /**
     * Change the frame number of the last time mobj sprite projection was
     * performed for the BSP leaf.
     *
     * @param newFrame  New frame number.
     */
    void setLastSpriteProjectFrame(int newFrame);

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
     * Recalculate the environmental audio characteristics (reverb) of the BSP leaf.
     */
    bool updateReverb();

    /**
     * Provides access to the final environmental audio characteristics (reverb)
     * of the BSP leaf, for efficient accumulation.
     */
    AudioEnvironmentFactors const &reverb() const;

#endif // __CLIENT__

private:
    ConvexSubspace(de::Face &convexPolygon);

    DENG2_PRIVATE(d)
};

#endif // DENG_WORLD_CONVEXSUBSPACE_H
