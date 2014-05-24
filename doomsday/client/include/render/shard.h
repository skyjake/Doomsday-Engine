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
#include <de/Vector>
#include "rendersystem.h"
#include "DrawList"

/**
 * 3D map geometry fragment.
 *
 * Shards are produced by SectorClusters when the map geometry is split into
 * drawable geometry fragments.
 */
class Shard
{
public:
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
        WorldVBuf const *vbuffer;
        WorldVBuf::Index vertCount;
        WorldVBuf::Index *indices;
        de::Vector2f texScale;
        de::Vector2f texOffset;
        de::Vector2f detailTexScale;
        de::Vector2f detailTexOffset;
    };
    typedef QList<Primitive> Primitives;
    Primitives primitives;

public:
    /**
     * Construct a new Shard of 3D map geometry.
     */
    Shard(DrawList::Spec const &listSpec,
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

#include <de/Matrix>

class BiasTracker;
class SectorCluster;

class BiasSurface
{
public:
    /**
     * Construct a new BiasSurface.
     *
     * @param numIllums  Number of illumination points for the geometry.
     * @param owner      SectorCluster which owns the surface (if any).
     */
    BiasSurface(int numIllums, SectorCluster *owner = 0);

    /**
     * Perform bias lighting for the vertex geometry supplied.
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
    void light(de::Vector3f const *posCoords, de::Vector4f *colorCoords,
               de::Matrix3f const &tangentMatrix, uint biasTime);
    void light(WorldVBuf::Index const *indices, WorldVBuf &vbuffer,
               de::Matrix3f const &tangentMatrix, uint biasTime);

    /**
     * Returns a pointer to the SectorCluster which owns the surface (if any).
     */
    SectorCluster *cluster() const;

    /**
     * Change SectorCluster which owns the surface to @a newOwner.
     */
    void setCluster(SectorCluster *newOwner);

    /**
     * Returns the BiasTracker for the surface.
     */
    BiasTracker &tracker() const;

    /**
     * Schedule a bias lighting update for the surface following a move/transform.
     */
    void updateAfterMove();

    /**
     * To be called to register the commands and variables of this module.
     */
    static void consoleRegister();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_RENDER_SHARD_H
