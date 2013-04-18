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

DENG2_PIMPL_NOREF(AssetGroup)
{
    Members deps;

    /**
     * Determines if all the assets in the group are ready.
     */
    bool allReady() const
    {
        DENG2_FOR_EACH_CONST(Members, i, deps)
        {
            switch(i->second)
            {
            case Required:
                if(!i->first->isReady()) return false;
                break;

            default:
                break;
            }
        }
        // Yay.
        return true;
    }

    void update(AssetGroup &self)
    {
        self.setState(allReady()? Ready : NotReady);
    }
};

AssetGroup::AssetGroup() : d(new Instance)
{
    // An empty set of members means the group is Ready.
    setState(Ready);
}

AssetGroup::~AssetGroup()
{
    // We are about to be deleted.
    audienceForStateChange.clear();

    clear();
}

int AssetGroup::size() const
{
    return d->deps.size();
}

void AssetGroup::clear()
{
    DENG2_FOR_EACH(Members, i, d->deps)
    {
        i->first->audienceForDeletion -= this;
    }

    d->deps.clear();
    d->update(*this);
}

void AssetGroup::insert(Asset const &asset, Policy policy)
{
    d->deps[&asset] = policy;
    asset.audienceForDeletion += this;
    d->update(*this);
}

void AssetGroup::remove(Asset const &asset)
{
    asset.audienceForDeletion -= this;
    d->deps.erase(&asset);
    d->update(*this);
}

bool AssetGroup::has(Asset const &asset) const
{
    return d->deps.find(&asset) != d->deps.end();
}

void AssetGroup::setPolicy(Asset const &asset, Policy policy)
{
    DENG2_ASSERT(d->deps.find(&asset) != d->deps.end());

    d->deps[&asset] = policy;
    d->update(*this);
}

void AssetGroup::assetDeleted(Asset &asset)
{
    remove(asset);
}

void AssetGroup::assetStateChanged(Asset &)
{
    d->update(*this);
}

} // namespace de
