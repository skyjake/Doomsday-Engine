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
#include <doomsday/console/var.h>
#include "clientapp.h"
#include "BiasIllum"
#include "BiasTracker"
#include "SectorCluster"

using namespace de;

static int devUpdateContributors = true; //cvar

DENG2_PIMPL_NOREF(BiasSurface)
{
    SectorCluster *owner;
    typedef QVector<BiasIllum *> Illums;
    Illums illums;
    BiasTracker tracker;
    uint lastUpdateFrame;

    Instance() : owner(0), lastUpdateFrame(0) {}
    ~Instance() { qDeleteAll(illums); }

    /**
     * Determines whether it is time to update lighting contributors.
     */
    bool needContributorUpdate()
    {
        // Are updates disabled?
        if(!devUpdateContributors) return false;

        // Unowned surfaces cannot be updated.
        if(!owner) return false;

        // If the data is already up to date, nothing needs to be done.
        uint lastChangeFrame = owner->biasLastChangeOnFrame();
        if(lastUpdateFrame == lastChangeFrame) return false;

        // Mark the data as having been updated (it will be soon).
        lastUpdateFrame = lastChangeFrame;
        return true;
    }
};

BiasSurface::BiasSurface(int numIllums, SectorCluster *owner) : d(new Instance)
{
    setCluster(owner);
    if(numIllums)
    {
        d->illums.reserve(numIllums);
        for(int i = 0; i < numIllums; ++i)
        {
            d->illums << new BiasIllum(&d->tracker);
        }
    }
}

void BiasSurface::light(Vector3f const *posCoords, Vector4f *colorCoords,
    Matrix3f const &tangentMatrix, uint biasTime)
{
    DENG2_ASSERT(posCoords != 0 && colorCoords != 0);
    Vector3f const sufNormal = tangentMatrix.column(2);

    if(d->illums.isEmpty()) return;

    // Is it time to update contributors?
    bool updated = false;
    if(d->needContributorUpdate())
    {
        // Perhaps our owner has updated lighting contributions for us?
        updated = d->owner->updateBiasContributors(this);
    }

    // Light the given geometry.
    Vector3f const *posIt    = posCoords;
    Vector4f *colorIt        = colorCoords;
    for(int i = 0; i < d->illums.count(); ++i, posIt++, colorIt++)
    {
        *colorIt += d->illums[i]->evaluate(*posIt, sufNormal, biasTime);
    }

    if(updated)
    {
        // Any changes from contributors will have now been applied.
        d->tracker.markIllumUpdateCompleted();
    }
}

void BiasSurface::light(WorldVBuf::Index const *indices, WorldVBuf &vbuffer,
    Matrix3f const &tangentMatrix, uint biasTime)
{
    DENG2_ASSERT(indices != 0);

    if(d->illums.isEmpty()) return;

    // Is it time to update contributors?
    bool updated = false;
    if(d->needContributorUpdate())
    {
        // Perhaps our owner has updated lighting contributions for us?
        updated = d->owner->updateBiasContributors(this);
    }

    // Light the given geometry.
    Vector3f const sufNormal = tangentMatrix.column(2);
    for(WorldVBuf::Index i = 0; i < d->illums.count(); ++i)
    {
        WorldVBuf::Type &vertex = vbuffer[indices[i]];
        vertex.rgba += d->illums[i]->evaluate(vertex.pos, sufNormal, biasTime);
    }

    if(updated)
    {
        // Any changes from contributors will have now been applied.
        d->tracker.markIllumUpdateCompleted();
    }
}

SectorCluster *BiasSurface::cluster() const
{
    return d->owner;
}

void BiasSurface::setCluster(SectorCluster *newOwner)
{
    d->owner = newOwner;
}

BiasTracker &BiasSurface::tracker() const
{
    return d->tracker;
}

void BiasSurface::updateAfterMove()
{
    d->tracker.updateAllContributors();
}

void BiasSurface::consoleRegister() // static
{
#ifdef __CLIENT__
    // Development variables.
    C_VAR_INT("rend-dev-bias-affected", &devUpdateContributors, CVF_NO_ARCHIVE, 0, 1);
#endif
}
