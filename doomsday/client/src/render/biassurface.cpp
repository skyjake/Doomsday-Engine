/** @file biassurface.cpp Shadow Bias surface.
 *
 * @authors Copyright © 2005-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include <de/Observers>

#include <QtAlgorithms>
#include <QVector>

#include "de_base.h"
#include "de_console.h"

#include "BiasTracker"
#include "BiasIllum"

#include "render/biassurface.h"

using namespace de;

static int devUpdateAffected = true; //cvar

struct Contributor
{
    BiasSource *source;
    float influence;
};

#define MAX_CONTRIBUTORS BiasIllum::MAX_CONTRIBUTORS

/**
 * @todo Defer allocation of most data -- adopt a 'fly-weight' approach.
 *
 * @todo Do not observe source deletion. A better solution would represent any
 * source deletions within the change tracker.
 */
DENG2_PIMPL_NOREF(BiasSurface),
DENG2_OBSERVES(BiasSource, Deletion)
{
    QVector<BiasIllum *> illums; /// @todo use an external allocator.

    Contributor contributors[MAX_CONTRIBUTORS];
    byte activeContributors;
    byte changedContributions;

    uint lastUpdateOnFrame;
    uint lastSourceDeletion; // Milliseconds.

    Instance(int size)
        : illums(size),
          activeContributors(0),
          changedContributions(0),
          lastUpdateOnFrame(0),
          lastSourceDeletion(0)
    {
        zap(contributors);
    }

    ~Instance()
    {
        qDeleteAll(illums);
    }

    /// Observes BiasSource Deletion
    void biasSourceBeingDeleted(BiasSource const &source)
    {
        Contributor *ctbr = contributors;
        for(int i = 0; i < MAX_CONTRIBUTORS; ++i, ctbr++)
        {
            if(ctbr->source == &source)
            {
                ctbr->source = 0;
                activeContributors &= ~(1 << i);
                changedContributions |= 1 << i;

                // Remember the current time (used for interpolation).
                /// @todo Do not assume the 'current' map.
                lastSourceDeletion = App_World().map().biasCurrentTime();
                break;
            }
        }
    }
};

BiasSurface::BiasSurface(int size) : d(new Instance(size))
{}

void BiasSurface::consoleRegister() // static
{
    // Development variables.
    C_VAR_INT("rend-dev-bias-affected", &devUpdateAffected, CVF_NO_ARCHIVE, 0, 1);
}

uint BiasSurface::lastUpdateOnFrame() const
{
    return d->lastUpdateOnFrame;
}

void BiasSurface::setLastUpdateOnFrame(uint newLastUpdateFrameNumber)
{
    d->lastUpdateOnFrame = newLastUpdateFrameNumber;
}

void BiasSurface::clearContributors()
{
    d->activeContributors = 0;
}

void BiasSurface::addContributor(BiasSource *source, float intensity)
{
    if(!source) return;

    // If its too weak we will ignore it entirely.
    if(intensity < BiasIllum::MIN_INTENSITY)
        return;

    int firstUnusedSlot = -1;
    int slot = -1;

    // Do we have a latent contribution or an unused slot?
    Contributor *ctbr = d->contributors;
    for(int i = 0; i < MAX_CONTRIBUTORS; ++i, ctbr++)
    {
        if(!ctbr->source)
        {
            // Remember the first unused slot.
            if(firstUnusedSlot == -1)
                firstUnusedSlot = i;
        }
        // A latent contribution?
        else if(ctbr->source == source)
        {
            slot = i;
            break;
        }
    }

    if(slot == -1)
    {
        if(firstUnusedSlot != -1)
        {
            slot = firstUnusedSlot;
        }
        else
        {
            // Dang, we'll need to drop the weakest.
            int weakest = -1;
            Contributor *ctbr = d->contributors;
            for(int i = 0; i < MAX_CONTRIBUTORS; ++i, ctbr++)
            {
                DENG_ASSERT(ctbr->source != 0);
                if(i == 0 || ctbr->influence < d->contributors[weakest].influence)
                {
                    weakest = i;
                }
            }

            if(intensity <= d->contributors[weakest].influence)
                return;

            slot = weakest;
            ctbr->source->audienceForDeletion -= d;
            ctbr->source = 0;
        }
    }

    DENG_ASSERT(slot >= 0 && slot < MAX_CONTRIBUTORS);
    ctbr = &d->contributors[slot];

    // When reactivating a latent contribution if the intensity has not
    // changed we don't need to force an update.
    if(!(ctbr->source == source && de::fequal(ctbr->influence, intensity)))
        d->changedContributions |= (1 << slot);

    if(!ctbr->source)
        source->audienceForDeletion += d;

    ctbr->source = source;
    ctbr->influence = intensity;

    // (Re)activate this contributor.
    d->activeContributors |= 1 << slot;
}

BiasSource &BiasSurface::contributor(int index) const
{
    if(index >= 0 && index < MAX_CONTRIBUTORS &&
       (d->activeContributors & (1 << index)))
    {
        DENG_ASSERT(d->contributors[index].source != 0);
        return *d->contributors[index].source;
    }
    /// @throw UnknownContributorError An invalid contributor index was specified.
    throw UnknownContributorError("BiasSurface::lightContributor", QString("Index %1 invalid/out of range").arg(index));
}

uint BiasSurface::timeOfLatestContributorUpdate() const
{
    uint latest = 0;

    if(d->changedContributions)
    {
        Contributor const *ctbr = d->contributors;
        for(int i = 0; i < MAX_CONTRIBUTORS; ++i, ctbr++)
        {
            if(!(d->changedContributions & (1 << i)))
                continue;

            if(!ctbr->source && !(d->activeContributors & (1 << i)))
            {
                // The source of the contribution was deleted.
                if(latest < d->lastSourceDeletion)
                    latest = d->lastSourceDeletion;
            }
            else if(latest < ctbr->source->lastUpdateTime())
            {
                latest = ctbr->source->lastUpdateTime();
            }
        }
    }

    return latest;
}

void BiasSurface::updateAffection(BiasTracker &changes)
{
    // All contributions from changed sources will need to be updated.

    Contributor *ctbr = d->contributors;
    for(int i = 0; i < MAX_CONTRIBUTORS; ++i, ctbr++)
    {
        if(!ctbr->source)
            continue;

        /// @todo optimize: This O(n) lookup can be avoided if we 1) reference
        /// sources by unique in-map index, and 2) re-index source references
        /// here upon deletion. The assumption being that affection changes
        /// occur far more frequently.
        if(changes.check(App_World().map().toIndex(*ctbr->source)))
        {
            d->changedContributions |= 1 << i;
            break;
        }
    }
}

void BiasSurface::updateAfterMove()
{
    Contributor *ctbr = d->contributors;
    for(int i = 0; i < MAX_CONTRIBUTORS; ++i, ctbr++)
    {
        if(ctbr->source)
        {
            ctbr->source->forceUpdate();
        }
    }
}

void BiasSurface::lightPoly(Vector3f const &surfaceNormal, uint biasTime,
    int vertCount, rvertex_t const *positions, ColorRawf *colors)
{
    DENG_ASSERT(vertCount == d->illums.count()); // sanity check
    DENG_ASSERT(positions != 0 && colors != 0);

    // Time to allocate the illumination data?
    if(!d->illums[0])
    {
        for(int i = 0; i < d->illums.count(); ++i)
        {
            d->illums[i] = new BiasIllum(this);
        }
    }

    rvertex_t const *vtx = positions;
    ColorRawf *color     = colors;
    for(int i = 0; i < vertCount; ++i, vtx++, color++)
    {
        Vector3d surfacePoint(vtx->pos[VX], vtx->pos[VY], vtx->pos[VZ]);
        d->illums[i]->evaluate(*color, surfacePoint, surfaceNormal, biasTime,
                               d->activeContributors, d->changedContributions);
    }

    // Any changes from contributors will have now been applied.
    d->changedContributions = 0;
}
