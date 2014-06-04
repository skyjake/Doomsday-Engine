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

class Subsector
{
public:
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

public:
    Subsector(ConvexSubspace &subspace);

    ConvexSubspace &convexSubspace() const;

    void addBiasSurfaceIfMissing(GeometryData &gdata);

    GeometryData::BiasIllums &biasIllums(GeometryData &gdata);

    BiasTracker &biasTracker(GeometryData &gdata);

    uint lastBiasUpdateFrame(GeometryData &gdata);

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

    GeometryGroups const &geomGroups() const;

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

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_WORLD_CLIENT_SUBSECTOR_H
