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
#include "BiasTracker"

class BiasDigest;
class BiasIllum;

/**
 * 3D map geometry fragment.
 */
class Shard
{
public: /// @todo make private:
    struct BiasData {
        uint lastUpdateFrame;
        typedef QList<BiasIllum *> BiasIllums;
        BiasIllums illums;
        BiasTracker tracker;
    } bias;

public:
    /**
     * @param numBiasIllums  Number of bias illumination points for the geometry.
     */
    Shard(int numBiasIllums);
    ~Shard();

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

    /**
     * Apply bias lighting changes to the Shard.
     *
     * @param changes  Digest of lighting changes to be applied.
     */
    void applyBiasDigest(BiasDigest &changes);

    /**
     * Schedule a bias lighting update for the Shard following a move.
     */
    void updateBiasAfterMove();
};

#endif // DENG_CLIENT_RENDER_SHARD_H
