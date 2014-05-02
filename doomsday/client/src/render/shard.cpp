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
#include <QVector>
#include <QtAlgorithms>
#include "con_main.h"
#include "BiasIllum"
#include "BiasTracker"
#include "SectorCluster"

using namespace de;

static int devUpdateBiasContributors = true; //cvar

DENG2_PIMPL_NOREF(Shard)
{
    SectorCluster *owner;
    typedef QVector<BiasIllum *> BiasIllums;
    BiasIllums biasIllums;
    BiasTracker biasTracker;
    uint biasLastUpdateFrame;

    Instance() : owner(0), biasLastUpdateFrame(0) {}
    ~Instance() { qDeleteAll(biasIllums); }

    /**
     * Determines whether it is time to update bias lighting contributors.
     */
    bool needBiasContributorUpdate()
    {
        // Are updates disabled?
        if(!devUpdateBiasContributors) return false;

        // Unowned shards cannot be updated.
        if(!owner) return false;

        // If the data is already up to date, nothing needs to be done.
        uint lastChangeFrame = owner->biasLastChangeOnFrame();
        if(biasLastUpdateFrame == lastChangeFrame) return false;

        // Mark the data as having been updated (it will be soon).
        biasLastUpdateFrame = lastChangeFrame;
        return true;
    }
};

Shard::Shard(int numBiasIllums, SectorCluster *owner) : d(new Instance)
{
    setCluster(owner);
    if(numBiasIllums)
    {
        d->biasIllums.reserve(numBiasIllums);
        for(int i = 0; i < numBiasIllums; ++i)
        {
            d->biasIllums << new BiasIllum(&d->biasTracker);
        }
    }
}

void Shard::lightWithBiasSources(Vector3f const *posCoords, Vector4f *colorCoords,
    Matrix3f const &tangentMatrix, uint biasTime)
{
    DENG2_ASSERT(posCoords != 0 && colorCoords != 0);
    Vector3f const sufNormal = tangentMatrix.column(2);

    if(d->biasIllums.isEmpty()) return;

    // Is it time to update bias contributors?
    bool biasUpdated = false;
    if(d->needBiasContributorUpdate())
    {
        // Perhaps our owner has updated lighting contributions for us?
        biasUpdated = d->owner->updateBiasContributors(this);
    }

    // Light the given geometry.
    Vector3f const *posIt    = posCoords;
    Vector4f *colorIt        = colorCoords;
    for(int i = 0; i < d->biasIllums.count(); ++i, posIt++, colorIt++)
    {
        *colorIt += d->biasIllums[i]->evaluate(*posIt, sufNormal, biasTime);
    }

    if(biasUpdated)
    {
        // Any changes from contributors will have now been applied.
        d->biasTracker.markIllumUpdateCompleted();
    }
}

SectorCluster *Shard::cluster() const
{
    return d->owner;
}

void Shard::setCluster(SectorCluster *newOwner)
{
    d->owner = newOwner;
}

BiasTracker &Shard::biasTracker() const
{
    return d->biasTracker;
}

void Shard::updateBiasAfterMove()
{
    d->biasTracker.updateAllContributors();
}

void Shard::consoleRegister() // static
{
#ifdef __CLIENT__
    // Development variables.
    C_VAR_INT("rend-dev-bias-affected", &devUpdateBiasContributors, CVF_NO_ARCHIVE, 0, 1);
#endif
}
