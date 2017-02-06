#if 0

/** @file biastracker.cpp  Shadow Bias illumination tracker.
 *
 * @authors Copyright © 2005-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#include "de_platform.h"
#include "render/biastracker.h"

#include "dd_main.h"
#include "world/map.h"
#include "BiasSource"
#include <de/Observers>
#include <array>

using namespace de;
using namespace world;

namespace render {

struct Contributor
{
    BiasSource *source = nullptr;
    dfloat influence   = 0;
};

typedef std::array<Contributor, BiasTracker::MAX_CONTRIBUTORS> Contributors;

} // namespace render

/**
 * @todo Do not observe source deletion. A better solution would represent any source
 * deletions in the change digest itself.
 */
DENG2_PIMPL_NOREF(BiasTracker)
, DENG2_OBSERVES(BiasSource, Deletion)
{
    render::Contributors contributors;

    QBitArray activeContributors   = QBitArray(MAX_CONTRIBUTORS);
    QBitArray changedContributions = QBitArray(MAX_CONTRIBUTORS);

    duint lastSourceDeletion = 0;  // Milliseconds.

    /// Observes BiasSource Deletion
    void biasSourceBeingDeleted(BiasSource const &source)
    {
        for(render::Contributors::size_type i = 0; i < contributors.size(); ++i)
        {
            auto &ctbr = contributors[i];

            if(ctbr.source == &source)
            {
                ctbr.source = nullptr;
                activeContributors.clearBit(i);
                changedContributions.setBit(i);

                // Remember the current time (used for interpolation).
                /// @todo Do not assume the 'current' map.
                lastSourceDeletion = App_World().map().biasCurrentTime();
                break;
            }
        }
    }
};

BiasTracker::BiasTracker() : d(new Impl())
{}

void BiasTracker::clearContributors()
{
    d->activeContributors.fill(0);
}

dint BiasTracker::addContributor(BiasSource *source, dfloat intensity)
{
    if(!source) return -1;

    // If its too weak we will ignore it entirely.
    if(intensity < BiasIllum::MIN_INTENSITY)
        return -1;

    dint firstUnusedSlot = -1;
    dint slot = -1;

    // Do we have a latent contribution or an unused slot?
    for(render::Contributors::size_type i = 0; i < d->contributors.size(); ++i)
    {
        auto const &ctbr = d->contributors[i];
        if(!ctbr.source)
        {
            // Remember the first unused slot.
            if(firstUnusedSlot == -1)
                firstUnusedSlot = i;
        }
        // A latent contribution?
        else if(ctbr.source == source)
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
            dint weakest = -1;
            for(render::Contributors::size_type i = 0; i < d->contributors.size(); ++i)
            {
                auto *ctbr = &d->contributors[i];
                DENG2_ASSERT(ctbr->source);
                if(i == 0 || ctbr->influence < d->contributors[weakest].influence)
                {
                    weakest = i;
                }
            }
            auto *ctbr = &d->contributors[weakest];

            if(intensity <= ctbr->influence)
                return - 1;

            slot = weakest;
            ctbr->source->audienceForDeletion -= d;
            ctbr->source = nullptr;
        }
    }

    auto *ctbr = &d->contributors[slot];

    // When reactivating a latent contribution if the intensity has not
    // changed we don't need to force an update.
    if(!(ctbr->source == source && de::fequal(ctbr->influence, intensity)))
        d->changedContributions.setBit(slot);

    if(!ctbr->source)
        source->audienceForDeletion += d;

    ctbr->source    = source;
    ctbr->influence = intensity;

    // (Re)activate this contributor.
    d->activeContributors.setBit(slot);

    return slot;
}

BiasSource &BiasTracker::contributor(dint index) const
{
    if(index >= 0 && index < d->activeContributors.size() &&
       d->activeContributors.testBit(index))
    {
        DENG2_ASSERT(d->contributors[index].source);
        return *d->contributors[index].source;
    }
    /// @throw UnknownContributorError An invalid contributor index was specified.
    throw UnknownContributorError("BiasTracker::lightContributor", QString("Index %1 invalid/out of range").arg(index));
}

duint BiasTracker::timeOfLatestContributorUpdate() const
{
    duint latest = 0;

    for(render::Contributors::size_type i = 0; i < d->contributors.size(); ++i)
    {
        auto const &ctbr = d->contributors[i];

        if(!d->changedContributions.testBit(i))
            continue;

        if(!ctbr.source && !d->activeContributors.testBit(i))
        {
            // The source of the contribution was deleted.
            if(latest < d->lastSourceDeletion)
                latest = d->lastSourceDeletion;
        }
        else if(latest < ctbr.source->lastUpdateTime())
        {
            latest = ctbr.source->lastUpdateTime();
        }
    }

    return latest;
}

QBitArray const &BiasTracker::activeContributors() const
{
    return d->activeContributors;
}

QBitArray const &BiasTracker::changedContributions() const
{
    return d->changedContributions;
}

void BiasTracker::updateAllContributors()
{
    for(render::Contributor &ctbr : d->contributors)
    {
        if(ctbr.source)
        {
            ctbr.source->forceUpdate();
        }
    }
}

void BiasTracker::applyChanges(QBitArray &changes)
{
    // All contributions from changed sources will need to be updated.

    for(render::Contributors::size_type i = 0; i < MAX_CONTRIBUTORS; ++i)
    {
        auto &ctbr = d->contributors[i];

        if(!ctbr.source) continue;

        /// @todo optimize: This O(n) lookup can be avoided if we 1) reference
        /// sources by unique in-map index, and 2) re-index source references
        /// here upon deletion. The assumption being that affection changes
        /// occur far more frequently.
        if(changes.testBit(App_World().map().indexOf(*ctbr.source)))
        {
            d->changedContributions.setBit(i);
        }
    }
}

void BiasTracker::markIllumUpdateCompleted()
{
    d->changedContributions.fill(0);
}

#endif
