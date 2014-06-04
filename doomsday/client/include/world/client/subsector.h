/** @file subsector.h  Client-side world sector subspace.
 *
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DENG_WORLD_CLIENT_SUBSECTOR_H
#define DENG_WORLD_CLIENT_SUBSECTOR_H

#include "BiasIllum"
#include "BiasTracker"
#include "ConvexSubspace"
#include "MapElement"
#include <QVector>

struct Subsector
{
    ConvexSubspace *subspace;

    struct GeometryData
    {
        typedef QVector<BiasIllum *> BiasIllums;

        de::MapElement *mapElement;
        int geomId;
        struct BiasSurface
        {
            BiasTracker tracker;
            uint lastUpdateFrame;
            BiasIllums illums;

            BiasSurface() : lastUpdateFrame(0) {}
            ~BiasSurface() { qDeleteAll(illums); }
        };
        QScopedPointer<BiasSurface> biasSurface;

    public:
        GeometryData(de::MapElement *mapElement, int geomId);

        void applyBiasDigest(BiasDigest &allChanges);
        void markBiasContributorUpdateNeeded();
        void markBiasIllumUpdateCompleted();
        void setBiasLastUpdateFrame(uint updateFrame);
    };

    /// @todo Avoid two-stage lookup.
    typedef QMap<int, GeometryData *> GeometryGroup;
    typedef QMap<de::MapElement *, GeometryGroup> GeometryGroups;
    GeometryGroups geomGroups;

    de::HEdge *_fanBase;      ///< Trifan base Half-edge (otherwise the center point is used).
    bool _needUpdateFanBase;  ///< @c true= need to rechoose a fan base half-edge.

    Subsector();
    ~Subsector();

    ConvexSubspace &convexSubspace() const;
    void setConvexSubspace(ConvexSubspace &newSubspace);

    void addBiasSurfaceIfMissing(GeometryData &gdata);

    GeometryData::BiasIllums &biasIllums(GeometryData &gdata);

    BiasTracker &biasTracker(GeometryData &gdata);

    uint lastBiasUpdateFrame(GeometryData &gdata);

    /**
     * Determine the half-edge whose vertex is suitable for use as the center point
     * of a trifan primitive.
     *
     * Note that we do not want any overlapping or zero-area (degenerate) triangles.
     *
     * @par Algorithm
     * <pre>For each vertex
     *    For each triangle
     *        if area is not greater than minimum bound, move to next vertex
     *    Vertex is suitable
     * </pre>
     *
     * If a vertex exists which results in no zero-area triangles it is suitable for
     * use as the center of our trifan. If a suitable vertex is not found then the
     * center of BSP leaf should be selected instead (it will always be valid as
     * BSP leafs are convex).
     */
    void chooseFanBase();

    /**
     * Returns a pointer to the face geometry half-edge which has been chosen
     * for use as the base for a triangle fan GL primitive. May return @c 0 if
     * no suitable base was determined.
     */
    de::HEdge *fanBase() const;

    /**
     * Returns the number of vertices needed for a triangle fan GL primitive.
     * @see fanBase()
     */
    int numFanVertices() const;

    void clearAllSubspaceShards() const;

    /**
     * Find the GeometryData for a MapElement by the element-unique @a group
     * identifier.
     *
     * @param geomId    Geometry identifier.
     * @param canAlloc  @c true= to allocate if no data exists. Note that the
     *                  number of vertices in the fan geometry must be known
     *                  at this time.
     */
    GeometryData *geomData(de::MapElement &mapElement, int geomId, bool canAlloc = false);

    bool updateBiasContributorsIfNeeded(GeometryData &gdata);
};

#endif // DENG_WORLD_CLIENT_SUBSECTOR_H
