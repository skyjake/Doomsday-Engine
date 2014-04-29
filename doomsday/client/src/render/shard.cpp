/** @file shard.cpp  3D map geometry shard.
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

#include "render/shard.h"
#include "BiasDigest"

using namespace de;

Shard::Shard(int numBiasIllums)
{
    if(numBiasIllums)
    {
        bias.illums.reserve(numBiasIllums);
        for(int i = 0; i < numBiasIllums; ++i)
        {
            bias.illums.append(BiasIllum(&bias.tracker));
        }
    }
}

void Shard::lightWithBiasSources(Vector3f const *posCoords, Vector4f *colorCoords,
    Matrix3f const &tangentMatrix, uint biasTime)
{
    Vector3f const sufNormal = tangentMatrix.column(2);

    Vector3f const *posIt = posCoords;
    Vector4f *colorIt     = colorCoords;
    for(int i = 0; i < bias.illums.count(); ++i, posIt++, colorIt++)
    {
        *colorIt += bias.illums[i].evaluate(*posIt, sufNormal, biasTime);
    }
}

void Shard::applyBiasDigest(BiasDigest &changes)
{
    bias.tracker.applyChanges(changes);
}

void Shard::updateBiasAfterMove()
{
    bias.tracker.updateAllContributors();
}
