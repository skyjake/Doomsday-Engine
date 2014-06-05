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
#include "render/rendersystem.h" // WorldVBuf
#include "BiasDigest"
#include "BiasIllum"
#include "BiasTracker"
#include "Line"
#include "MapElement"
#include <QSet>
#include <QMap>
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

    /// Final audio environment characteristics.
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

        void markBiasContributorUpdateNeeded();
        void markBiasIllumUpdateCompleted();
    };

    /// @todo Avoid two-stage lookup.
    typedef QMap<int, GeometryData *> GeometryGroup;
    typedef QMap<de::MapElement *, GeometryGroup> GeometryGroups;

public:
    Subsector(ConvexSubspace &convexSubspace);

    /**
     * Returns the number of vertices needed for a triangle fan GL primitive
     */
    int numFanVertices() const;

    /**
     * Write world position coords for the subsector to the given vertex buffer.
     *
     * @param vbuf       Vertex buffer being written to.
     * @param indices    Vertex buffer indices of the elements being written.
     * @param direction  Vertex winding order.
     * @param height     Z coordinate in map space.
     *
     * @see numFanVertices()
     */
    void writePosCoords(WorldVBuf &vbuf, WorldVBuf::Indices const &indices,
                        de::ClockDirection direction, coord_t height);

    /**
     * Determines whether GeometryData exists for the referenced @a mapElement.
     */
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

    /**
     * Provides access to all the GeometryData for the subsector, for efficient traversal.
     */
    GeometryGroups const &geomGroups() const;

    /**
     * Convenient method returning the bias illumination points for the given
     * geometry data (allocating them if necessary).
     *
     * @param gdata  Geometry data to retrieve illumination points for.
     */
    GeometryData::BiasIllums &biasIllums(GeometryData &gdata);

    /**
     * Apply bias lighting changes to @em all geometry illumination points for
     * the subsector.
     *
     * @param changes  Digest of lighting changes to be applied.
     */
    void applyBiasDigest(BiasDigest &allChanges);

    /**
     * To be called (periodically) to update the bias light source contributors
     * for the given geometry data.
     */
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

    /**
     * Provides access to the list of geometry Shards linked to the subsector.
     * Anyone may add or remove shards from this list as its sole purpose is to
     * provide a convenient place for them, for drawing (during BSP traversal).
     */
    Shards &shards();

    /**
     * Returns the ConvexSubspace associated with the subsector (FYI).
     */
    ConvexSubspace &convexSubspace() const;

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_WORLD_CLIENT_SUBSECTOR_H
