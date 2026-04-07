/** @file asset.cpp  Information about the state of an asset (e.g., resource).
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/asset.h"
#include "de/waitable.h"
#include "de/widget.h"

namespace de {

DE_PIMPL_NOREF(Asset)
{
    State state;

    Impl(State s) : state(s) {}
    Impl(const Impl &other) : de::IPrivate(), state(other.state) {}

    DE_PIMPL_AUDIENCE(StateChange)
    DE_PIMPL_AUDIENCE(Deletion)
};

DE_AUDIENCE_METHOD(Asset, StateChange)
DE_AUDIENCE_METHOD(Asset, Deletion)

Asset::Asset(State initialState) : d(new Impl(initialState))
{}

Asset::Asset(const Asset &other) : d(new Impl(*other.d))
{}

Asset::~Asset()
{
    DE_NOTIFY(Deletion, i) i->assetBeingDeleted(*this);
}

void Asset::setState(State s)
{
    const State old = d->state;
    d->state = s;
    if (old != d->state)
    {
        DE_NOTIFY(StateChange, i) i->assetStateChanged(*this);
    }
}

void Asset::setState(bool assetReady)
{
    setState(assetReady ? Ready : NotReady);
}

Asset::State Asset::state() const
{
    return d->state;
}

bool Asset::isReady() const
{
    return d->state == Ready;
}

void Asset::waitForState(State s) const
{
    struct Waiter
        : public Waitable
        , DE_OBSERVES(Asset, StateChange) {
        State waitingFor;
        Waiter(State wf)
            : waitingFor(wf)
        {}
        void assetStateChanged(Asset &asset) override
        {
            if (asset.state() == waitingFor)
            {
                post();
            }
        }
    };

    Waiter w(s);
    audienceForStateChange() += w;
    if (d->state != s)
    {
        w.wait();
    }
}
    
String Asset::asText() const
{
    if (const auto *w = dynamic_cast<const Widget *>(this))
    {
        return w->name();
    }
    return Stringf("Asset %p", this);
}

//----------------------------------------------------------------------------

DE_PIMPL_NOREF(AssetGroup)
{
    Members deps;

    /**
     * Determines if all the assets in the group are ready.
     */
    bool allReady() const
    {
        DE_FOR_EACH_CONST(Members, i, deps)
        {
            switch (i->second)
            {
            case Required:
                if (!i->first->isReady()) return false;
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

AssetGroup::AssetGroup() : d(new Impl)
{
    // An empty set of members means the group is Ready.
    setState(Ready);
}

AssetGroup::~AssetGroup()
{
    // We are about to be deleted; no need to notify everyone that the AssetGroup
    // becomes ready after all its members are removed. The deletion will be
    // notified anyway immediately afterwards.
    audienceForStateChange().clear();

    clear();
}

dint AssetGroup::size() const
{
    return dint(d->deps.size());
}

void AssetGroup::clear()
{
    DE_FOR_EACH(Members, i, d->deps)
    {
        i->first->audienceForDeletion() -= this;
        i->first->audienceForStateChange() -= this;
    }

    d->deps.clear();
    d->update(*this);
}

void AssetGroup::insert(const Asset &asset, Policy policy)
{
    d->deps[&asset] = policy;
    asset.audienceForDeletion() += this;
    asset.audienceForStateChange() += this;
    d->update(*this);
}

void AssetGroup::remove(const Asset &asset)
{
    asset.audienceForDeletion() -= this;
    asset.audienceForStateChange() -= this;
    d->deps.erase(&asset);
    d->update(*this);
}

bool AssetGroup::has(const Asset &asset) const
{
    return d->deps.find(&asset) != d->deps.end();
}

void AssetGroup::setPolicy(const Asset &asset, Policy policy)
{
    auto found = d->deps.find(&asset);
    DE_ASSERT(found != d->deps.end());
    if (found->second != policy)
    {
        found->second = policy;
        d->update(*this);
    }
}

const AssetGroup::Members &AssetGroup::all() const
{
    return d->deps;
}

void AssetGroup::assetBeingDeleted(Asset &asset)
{
    if (has(asset))
    {
        remove(asset);
    }
}

void AssetGroup::assetStateChanged(Asset &)
{
    DE_ASSERT(!d.isNull());
    d->update(*this);
}

String AssetGroup::asText() const
{
    StringList str;
    for (auto i = d->deps.begin(); i != d->deps.end(); ++i)
    {
        str.append(i->first->asText());
    }
    return Stringf("AssetGroup with %zu members:\n%s", str.size(), String::join(str, "\n").c_str());
}
    
IAssetGroup::~IAssetGroup()
{}

IAssetGroup::operator Asset &()
{
    return assets();
}

} // namespace de
