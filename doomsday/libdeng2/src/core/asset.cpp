/** @file asset.cpp  Information about the state of an asset (e.g., resource).
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Asset"

namespace de {

Asset::Asset(State initialState) : _state(initialState)
{}

Asset::~Asset()
{
    DENG2_FOR_AUDIENCE(Deletion, i) i->assetDeleted(*this);
}

void Asset::setState(State s)
{
    State old = _state;
    _state = s;
    if(old != _state)
    {
        DENG2_FOR_AUDIENCE(StateChange, i) i->assetStateChanged(*this);
    }
}

Asset::State Asset::state() const
{
    return _state;
}

bool Asset::isReady() const
{
    return _state == Ready;
}

//----------------------------------------------------------------------------

DENG2_PIMPL_NOREF(DependAssets)
{
    Dependencies deps;
};

DependAssets::DependAssets() : d(new Instance)
{}

DependAssets::~DependAssets()
{
    clear();
}

int DependAssets::size() const
{
    return d->deps.size();
}

void DependAssets::clear()
{
    DENG2_FOR_EACH(Dependencies, i, d->deps)
    {
        i->first->audienceForDeletion -= this;
    }

    d->deps.clear();
}

void DependAssets::insert(Asset const &asset, Policy policy)
{
    d->deps[&asset] = policy;
    asset.audienceForDeletion += this;
}

void DependAssets::remove(Asset const &asset)
{
    asset.audienceForDeletion -= this;
    d->deps.erase(&asset);
}

bool DependAssets::has(Asset const &asset) const
{
    return d->deps.find(&asset) != d->deps.end();
}

void DependAssets::setPolicy(Asset const &asset, Policy policy)
{
    DENG2_ASSERT(d->deps.find(&asset) != d->deps.end());

    d->deps[&asset] = policy;
}

bool DependAssets::mustSuspendTime() const
{
    DENG2_FOR_EACH_CONST(Dependencies, i, d->deps)
    {
        if(i->second == SuspendTime && !i->first->isReady())
        {
            return true;
        }
    }
    return false;
}

void DependAssets::assetDeleted(Asset &asset)
{
    remove(asset);
}

void DependAssets::assetStateChanged(Asset &asset)
{
    bool allReady = false;

    if(asset.state() == Ready)
    {
        // Perhaps everything is ready.
        DENG2_FOR_EACH_CONST(Dependencies, i, d->deps)
        {
            switch(i->second)
            {
            case Required:
            case SuspendTime:
                if(!i->first->isReady())
                {
                    goto breakLoop;
                }

            default:
                break;
            }
        }

        // Yay.
        allReady = true;
    }

breakLoop:

    // Update the state of the asset dependency group.
    setState(allReady? Ready : NotReady);
}

} // namespace de
