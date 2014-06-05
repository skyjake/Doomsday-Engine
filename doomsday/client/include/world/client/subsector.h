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

#include "dd_share.h" // NUM_REVERB_DATA
#include "BiasIllum"
#include "BiasTracker"
#include "Line"
#include "MapElement"
#include <QSet>
#include <QList>
#include <QVector>

class ConvexSubspace;
class Lumobj;
class Shard;

/**
 * Client side world sector subspace.
 *
 * Provides and/or links to various geometry data assets and properties used to
 * visualize the subspace.
 *
 * @ingroup world
 *
 * @todo ConvexSubspace/Subsector are 1:1 (Subsector should extend ConvexSubspace).
 */
class Subsector
{
public:
    /// Linked-element lists/sets:
    typedef QSet<Lumobj *>    Lumobjs;
    typedef QSet<LineSide *>  ShadowLines;
    typedef QSet<Shard *>     Shards;

    // Final audio environment characteristics.
    typedef uint AudioEnvironmentFactors[NUM_REVERB_DATA];

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

    GeometryData::BiasIllums &biasIllums(GeometryData &gdata);

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

    bool hasGeomData(de::MapElement &mapElement, int geomId) const;

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

    GeometryGroups const &geomGroups() const;

    bool updateBiasContributorsIfNeeded(GeometryData &gdata);

    /**
     * Clear the list of fake radio shadow line sides for the subspace.
     */
    void clearShadowLines();

    /**
     * Add the specified line @a side to the set of fake radio shadow lines for
     * the subspace. If the line is already present in this set then nothing
     * will happen.
     *
     * @param side  Map line side to add to the set.
     */
    void addShadowLine(LineSide &side);

    /**
     * Provides access to the set of fake radio shadow lines for the subspace.
     */
    ShadowLines const &shadowLines() const;

    /**
     * Clear all lumobj links for the subspace.
     */
    void unlinkAllLumobjs();

    /**
     * Unlink the specified @a lumobj in the subspace. If the lumobj is not
     * linked then nothing will happen.
     *
     * @param lumobj  Lumobj to unlink.
     *
     * @see link()
     */
    void unlink(Lumobj &lumobj);

    /**
     * Link the specified @a lumobj in the subspace. If the lumobj is already
     * linked then nothing will happen.
     *
     * @param lumobj  Lumobj to link.
     *
     * @see lumobjs(), unlink()
     */
    void link(Lumobj &lumobj);

    /**
     * Provides access to the set of lumobjs linked to the subspace.
     *
     * @see linkLumobj(), clearLumobjs()
     */
    Lumobjs const &lumobjs() const;

    /**
     * Recalculate the environmental audio characteristics (reverb) of the subspace.
     */
    bool updateReverb();

    /**
     * Provides access to the final environmental audio characteristics (reverb)
     * of the subspace, for efficient accumulation.
     */
    AudioEnvironmentFactors const &reverb() const;

    Shards &shards();

    void clearShards() const;

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_WORLD_CLIENT_SUBSECTOR_H
