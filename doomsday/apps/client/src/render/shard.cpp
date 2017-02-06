#if 0

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

#include "BiasIllum"
#include "BiasTracker"
#include "client/clientsubsector.h"

#include <doomsday/console/var.h>
#include <QVector>
#include <QtAlgorithms>

using namespace de;

DENG2_PIMPL_NOREF(Shard)
{
    world::Subsector *owner = nullptr;
#if 0
    typedef QVector<BiasIllum *> BiasIllums;
    BiasIllums biasIllums;
    BiasTracker biasTracker;
    duint biasLastUpdateFrame = 0;
#endif

    ~Impl() {} //{ qDeleteAll(biasIllums); }

#if 0
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
        duint lastChangeFrame = owner->as<world::ClientSubsector>().biasLastChangeOnFrame();
        if(biasLastUpdateFrame == lastChangeFrame) return false;

        // Mark the data as having been updated (it will be soon).
        biasLastUpdateFrame = lastChangeFrame;
        return true;
    }
#endif
};

Shard::Shard(/*dint numBiasIllums, */world::Subsector *owner) : d(new Impl)
{
    setSubsector(owner);
#if 0
    if(numBiasIllums)
    {
        d->biasIllums.reserve(numBiasIllums);
        for(dint i = 0; i < numBiasIllums; ++i)
        {
            d->biasIllums << new BiasIllum(&d->biasTracker);
        }
    }
#endif
}

#if 0
void Shard::lightWithBiasSources(Vector3f const *posCoords, Vector4f *colorCoords,
    Matrix3f const &tangentMatrix, duint biasTime)
{
    DENG2_ASSERT(posCoords && colorCoords);
    Vector3f const sufNormal = tangentMatrix.column(2);

    if(d->biasIllums.isEmpty()) return;

    // Is it time to update bias contributors?
    bool biasUpdated = false;
    if(d->needBiasContributorUpdate())
    {
        // Perhaps our owner has updated lighting contributions for us?
        biasUpdated = d->owner->as<world::ClientSubsector>().updateBiasContributors(this);
    }

    // Light the given geometry.
    Vector3f const *posIt    = posCoords;
    Vector4f *colorIt        = colorCoords;
    for(dint i = 0; i < d->biasIllums.count(); ++i, posIt++, colorIt++)
    {
        *colorIt += d->biasIllums[i]->evaluate(*posIt, sufNormal, biasTime);
    }

    if(biasUpdated)
    {
        // Any changes from contributors will have now been applied.
        d->biasTracker.markIllumUpdateCompleted();
    }
}
#endif

world::Subsector *Shard::subsector() const
{
    return d->owner;
}

void Shard::setSubsector(world::Subsector *newOwner)
{
    d->owner = newOwner;
}

#if 0
BiasTracker &Shard::biasTracker() const
{
    return d->biasTracker;
}

void Shard::updateBiasAfterMove()
{
    d->biasTracker.updateAllContributors();
}
#endif

void Shard::consoleRegister() // static
{
#if 0
    static dint devUpdateBiasContributors = true;  //cvar

    // Development variables.
    C_VAR_INT("rend-dev-bias-affected", &devUpdateBiasContributors, CVF_NO_ARCHIVE, 0, 1);
#endif
}

#endif
