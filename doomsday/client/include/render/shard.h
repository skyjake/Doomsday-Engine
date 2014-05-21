/** @file shard.h  3D map geometry fragment.
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

#ifndef DENG_CLIENT_RENDER_SHARD_H
#define DENG_CLIENT_RENDER_SHARD_H

#include <QList>
#include <de/Matrix>
#include <de/Vector>
#include "rendersystem.h"
#include "DrawList"

class BiasTracker;
class SectorCluster;

/**
 * 3D map geometry fragment.
 *
 * Shards are produced (and perhaps owned) by SectorClusters when the map geometry
 * is split into drawable geometry fragments. Shard ownership may be transferred
 * however a shard should never outlive the MapElement for which it was produced.
 */
class Shard
{
public:
    struct Geom
    {
        DrawList::Spec listSpec;
        blendmode_t blendmode;
        GLuint modTex;
        de::Vector3f modColor;
        bool hasDynlights;
        typedef WorldVBuf::Indices Indices;
        Indices indices;
        struct Primitive
        {
            de::gl::Primitive type;
            WorldVBuf::Index vertCount;
            WorldVBuf::Index const *indices;
            de::Vector2f texScale;
            de::Vector2f texOffset;
            de::Vector2f detailTexScale;
            de::Vector2f detailTexOffset;
        };
        typedef QList<Primitive> Primitives;
        Primitives primitives;

        Geom(DrawList::Spec const &listSpec,
             blendmode_t blendmode        = BM_NORMAL,
             GLuint modTex                = 0,
             de::Vector3f const &modColor = de::Vector3f(),
             bool hasDynlights            = false)
            : listSpec    (listSpec)
            , blendmode   (blendmode)
            , modTex      (modTex)
            , modColor    (modColor)
            , hasDynlights(hasDynlights)
        {}
    };

public:
    /**
     * Construct a new Shard of 3D map geometry.
     *
     * @param numBiasIllums  Number of bias illumination points for the geometry.
     * @param owner          SectorCluster which owns the shard (if any).
     */
    Shard(int numBiasIllums, SectorCluster *owner = 0);

    /**
     * Perform bias lighting for the supplied vertex geometry.
     *
     * @note Important: It assumed that the given geometry buffers have at least
     * the same number of elements as there are bias illumination points.
     *
     * @param posCoords      Position coords (in map space) for each vertex.
     * @param colorCoords    Color coords for each vertex. Light contributions are
     *                       applied additively to the @em current values. Therefore
     *                       they should be suitably initialized with a base value
     *                       before calling (perhaps zeroed).
     *
     * @param tangentMatrix  Tangent space matrix for the surface geometry.
     * @param biasTime       Time in milliseconds of the last bias frame update,
     *                       used for interpolation.
     */
    void lightWithBiasSources(de::Vector3f const *posCoords, de::Vector4f *colorCoords,
                              de::Matrix3f const &tangentMatrix, uint biasTime);
    void lightWithBiasSources(WorldVBuf::Index const *indices,
                              de::Matrix3f const &tangentMatrix, uint biasTime);

    /**
     * Returns a pointer to the SectorCluster which owns the shard (if any).
     */
    SectorCluster *cluster() const;

    /**
     * Change SectorCluster which owns the shard to @a newOwner.
     */
    void setCluster(SectorCluster *newOwner);

    /**
     * Returns the BiasTracker for the shard.
     */
    BiasTracker &biasTracker() const;

    /**
     * Schedule a bias lighting update for the Shard following a move/transform.
     */
    void updateBiasAfterMove();

    /**
     * To be called to register the commands and variables of this module.
     */
    static void consoleRegister();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_RENDER_SHARD_H
