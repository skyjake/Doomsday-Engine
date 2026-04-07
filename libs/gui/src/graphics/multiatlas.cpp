/** @file multiatlas.cpp
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/multiatlas.h"
#include <de/deletable.h>

#include <de/hash.h>
#include <de/list.h>
#include <de/set.h>

namespace de {

typedef Hash<Id::Type, Image *> PendingImages;

DE_PIMPL(MultiAtlas)
, public Deletable
{
    IAtlasFactory &factory;
    List<Atlas *> atlases;

    Impl(Public *i, IAtlasFactory &factory)
        : Base(i)
        , factory(factory)
    {}

    ~Impl()
    {
        // Delete all the atlases.
        release();

        // AllocGroups will get notified by Deletable and make themselves invalid.
    }

    void release()
    {
        deleteAll(atlases);
        atlases.clear();
    }

    Atlas *getEmptyAtlas()
    {
        // Reuse an empty atlas.
        for (Atlas *atlas : atlases)
        {
            if (atlas->isEmpty()) return atlas;
        }
        // Make a new atlas.
        Atlas *blank = factory.makeAtlas(self());
        DE_ASSERT(blank->flags().testFlag(Atlas::DeferredAllocations));
        atlases.prepend(blank);
        return blank;
    }

    bool tryAllocatePending(Atlas &atlas, const PendingImages &pending)
    {
        DE_ASSERT(atlas.flags().testFlag(Atlas::DeferredAllocations));

        for (auto i = pending.begin(); i != pending.end(); ++i)
        {
            if (!atlas.alloc(*i->second, i->first))
            {
                // Cannot fit on this atlas!
                atlas.cancelDeferred();
                return false;
            }
        }
        // If all allocations succeeded, we're good to go.
        atlas.commit();
        return true;
    }

    /**
     * Selects a suitable atlas for the provided set of images. All the images
     * must fit on a single atlas. A new atlas is created if necessary.
     *
     * @param pendingAllocs  Set of images to be allocated.
     *
     * @return Atlas object. Caller must commit the deferred allocations.
     */
    Atlas &allocatePending(const PendingImages &pending)
    {
        // Let's see if the images fit on one of our existing atlases.
        for (Atlas *atlas : atlases)
        {
            if (tryAllocatePending(*atlas, pending))
            {
                return *atlas;
            }
        }
        // None of the existing atlases were suitable. Get a new one.
        Atlas *blank = getEmptyAtlas();
        if (tryAllocatePending(*blank, pending))
        {
            return *blank;
        }
        throw IAtlas::OutOfSpaceError("MultiAtlas::allocatePending",
                                      "Even an empty atlas cannot fit the pending allocations");
    }
};

MultiAtlas::MultiAtlas(IAtlasFactory &factory)
    : d(new Impl(this, factory))
{}

void MultiAtlas::clear()
{
    d->release();
}

} // namespace de

//-----------------------------------------------------------------------------

namespace de {

DE_PIMPL_NOREF(MultiAtlas::AllocGroup)
, DE_OBSERVES(Deletable, Deletion)
{
    AllocGroup *self;
    MultiAtlas *owner;
    PendingImages pending; // owned
    Atlas *atlas = nullptr; ///< Committed atlas.
    Set<Id::Type> allocated; // committed to the atlas

    Impl(AllocGroup *i, MultiAtlas &owner)
        : self(i)
        , owner(&owner)
    {
        this->owner->d->audienceForDeletion += this;
    }

    ~Impl()
    {
        if (owner)
        {
            release();
        }
    }

    void objectWasDeleted(Deletable *deleted)
    {
        // This group is no longer valid for use.
        if (deleted == owner->d)
        {
            owner = nullptr;
            if (atlas) atlas->audienceForDeletion -= this;
            atlas = nullptr;
            cancelPending();
        }
        else
        {
            // Just the atlas.
            DE_ASSERT(deleted == atlas);
            atlas = nullptr;
        }
        allocated.clear();
        self->setState(Asset::NotReady);
    }

    void cancelPending()
    {
        pending.deleteAll();
        pending.clear();
    }

    /// Release all allocations and cancel pending ones.
    void release()
    {
        cancelPending();
        if (atlas)
        {
            for (auto i : allocated)
            {
                atlas->release(i);
            }
        }
        allocated.clear();
    }
};

MultiAtlas::AllocGroup::AllocGroup(MultiAtlas &multiAtlas)
    : d(new Impl(this, multiAtlas))
{}

Id MultiAtlas::AllocGroup::alloc(const Image &image, const Id &knownId)
{
    if (!d->atlas)
    {
        // This will be a pending allocation until the group is committed.
        // This Id will be used in the atlas when committing.
        Id allocId { knownId.isNone()? Id() : knownId };
        d->pending.insert(allocId, new Image(image));
        return allocId;
    }
    else
    {
        // After committing, allocations are always done in the chosen atlas.
        Id allocId { d->atlas->alloc(image, knownId) };
        d->allocated.insert(allocId);
        return allocId;
    }
}

void MultiAtlas::AllocGroup::release(const Id &id)
{
    auto foundPending = d->pending.find(id);
    if (foundPending != d->pending.end())
    {
        delete foundPending->second;
        d->pending.erase(foundPending);
        return;
    }

    if (d->atlas && d->allocated.contains(id))
    {
        d->allocated.remove(id);
        d->atlas->release(id);
    }
}

bool MultiAtlas::AllocGroup::contains(const Id &id) const
{
    return d->pending.contains(id) || d->allocated.contains(id);
}

void MultiAtlas::AllocGroup::commit() const
{
    if (!d->owner)
    {
        throw InvalidError("MultiAtlas::AllocGroup::commit",
                           "Allocation group has been invalidated");
    }
    if (!d->atlas)
    {
        // Time to decide which atlas to use.
        d->atlas = &d->owner->d->allocatePending(d->pending);
        d->atlas->audienceForDeletion += d;
    }
    for (auto i = d->pending.begin(); i != d->pending.end(); ++i)
    {
        d->allocated.insert(i->first);
        delete i->second; // free the Image
    }
    d->pending.clear();

    const_cast<MultiAtlas::AllocGroup *>(this)->setState(Ready);
}

Rectanglef MultiAtlas::AllocGroup::imageRectf(const Id &id) const
{
    if (d->atlas)
    {
        return d->atlas->imageRectf(id);
    }
    throw InvalidError("MultiAtlas::AllocGroup::imageRectf",
                       "Allocation group has not yet been committed to an atlas");
}

const Atlas *MultiAtlas::AllocGroup::atlas() const
{
    return d->atlas;
}

MultiAtlas &MultiAtlas::AllocGroup::multiAtlas()
{
    DE_ASSERT(d->owner != nullptr);
    return *d->owner;
}

} // namespace de
