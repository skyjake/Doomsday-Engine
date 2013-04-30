/** @file atlas.cpp  Image-based atlas.
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

#include "de/Atlas"

#include <QSet>
#include <QRect>
#include <QImage>
#include <QPainter>

namespace de {

DENG2_PIMPL(Atlas)
{
    Flags flags;
    Size totalSize;
    int margin;
    std::auto_ptr<IAllocator> allocator;
    Image backing;
    bool needCommit;
    bool needFullCommit;
    bool mayDefrag;
    Rectanglei changedArea;

    // Minimum backing size is 1x1 pixels.
    Instance(Public *i, Flags const &flg, Size const &size)
        : Base(i), flags(flg), totalSize(size.max(Size(1, 1))), margin(1),
          needCommit(false), needFullCommit(true), mayDefrag(false)
    {
        if(hasBacking())
        {
            backing = QImage(QSize(totalSize.x, totalSize.y), QImage::Format_ARGB32);
        }
    }

    bool hasBacking() const
    {
        return flags.testFlag(BackingStore);
    }

    void markAsChanged(Rectanglei const &rect)
    {
        if(needCommit)
        {
            // Merge to earlier changes.
            changedArea |= rect;
        }
        else
        {
            needCommit  = true;
            changedArea = rect;
        }
    }

    void markFullyChanged()
    {
        needCommit = true;
        needFullCommit = true;
        changedArea = backing.rect();
    }

    bool mustCommitFull() const
    {
        /*
         * Simple heuristic: if more than half the pixels are included in the
         * changed area, simply copy the whole thing rather than doing a
         * large extra copy.
         */
        return (needFullCommit || changedPercentage() > .5f);
    }

    float changedPercentage() const
    {
        if(!needCommit || totalSize == Size(0, 0)) return 0.0f;

        duint totalPx   = totalSize.x * totalSize.y;
        duint changedPx = changedArea.width() * changedArea.height();

        return float(changedPx) / float(totalPx);
    }

    float usedPercentage() const
    {
        if(!allocator.get()) return 0;

        duint totalPx = totalSize.x * totalSize.y;
        duint usedPx = 0;

        foreach(Rectanglei const &alloc, allocator->allocs().values())
        {
            usedPx += alloc.width() * alloc.height();
        }
        return float(usedPx) / float(totalPx);
    }

    /**
     * Compose a new backing store with an optimal layout.
     */
    void defragment()
    {
        DENG2_ASSERT(hasBacking());

        IAllocator::Allocations const oldLayout = allocator->allocs();
        if(!allocator->optimize())
        {
            // Optimization did not work out.
            mayDefrag = false;
            return;
        }

        Image defragged(QImage(QSize(backing.size().x, backing.size().y), backing.qtFormat()));
        defragged.fill(Image::Color(0, 0, 0, 0));

        // Copy all the images to their optimal places.
        IAllocator::Allocations optimal = allocator->allocs();
        DENG2_FOR_EACH(IAllocator::Allocations, i, optimal)
        {
            defragged.draw(backing.subImage(oldLayout[i.key()]),
                           i.value().topLeft);
        }

        // Defragmentation complete, use the revised backing store.
        backing = defragged;
        markFullyChanged();
        mayDefrag = false;

        DENG2_FOR_PUBLIC_AUDIENCE(Reposition, i)
        {
            i->atlasContentRepositioned(self);
        }
    }
};

Atlas::Atlas(Flags const &flags, Size const &totalSize)
    : d(new Instance(this, flags, totalSize))
{}

void Atlas::setAllocator(IAllocator *allocator)
{
    clear();
    d->allocator.reset(allocator);
    if(d->allocator.get())
    {
        d->allocator->setMetrics(d->totalSize, d->margin);
        d->allocator->clear(); // using new metrics
    }
    d->markFullyChanged();
}

void Atlas::clear()
{
    if(d->allocator.get())
    {
        d->allocator->clear();
    }
    if(d->hasBacking())
    {
        d->backing.fill(Image::Color(0, 0, 0, 0));
        d->markFullyChanged();
    }
    d->mayDefrag = false;
}

void Atlas::setTotalSize(Size const &totalSize)
{
    d->totalSize = totalSize;

    if(d->allocator.get())
    {
        d->allocator->setMetrics(totalSize, d->margin);
    }

    if(d->hasBacking())
    {
        d->backing.resize(totalSize);
        d->markFullyChanged();
        d->defragment();
    }
}

Atlas::Size Atlas::totalSize() const
{
    return d->totalSize;
}

Id Atlas::alloc(Image const &image)
{
    DENG2_ASSERT(d->allocator.get());

    Rectanglei rect;
    Id id = d->allocator->allocate(image.size(), rect);

    if(id.isNone() && d->flags.testFlag(AllowDefragment) && d->mayDefrag)
    {
        // Allocation failed. Maybe we can defragment to get more space?
        d->defragment();

        // Try again...
        id = d->allocator->allocate(image.size(), rect);
    }

    if(!id.isNone())
    {
        if(d->hasBacking())
        {
            // Make sure the margin is cleared to black.
            d->backing.fill(rect.expanded(d->margin), Image::Color(0, 0, 0, 0));
            d->backing.draw(image, rect.topLeft);

            d->markAsChanged(rect);
        }
        else
        {
            // No backing, must commit immediately.
            commit(image, rect.topLeft);
        }

        // After a successful alloc we can attempt to defragment
        // later.
        d->mayDefrag = true;
    }
    else
    {
        LOG_DEBUG("Atlas is full with %.1f%% usage") << d->usedPercentage()*100;
    }
    return id;
}

void Atlas::release(Id const &id)
{
    DENG2_ASSERT(d->allocator.get());

    d->allocator->release(id);
    d->mayDefrag = true;
}

bool Atlas::contains(Id const &id) const
{
    if(d->allocator.get())
    {
        return d->allocator->ids().contains(id);
    }
    return false;
}

int Atlas::imageCount() const
{
    DENG2_ASSERT(d->allocator.get());

    return d->allocator->count();
}

Atlas::Ids Atlas::allImages() const
{
    DENG2_ASSERT(d->allocator.get());

    return d->allocator->ids();
}

Rectanglei Atlas::imageRect(Id const &id) const
{
    DENG2_ASSERT(d->allocator.get());

    Rectanglei rect;
    d->allocator->rect(id, rect);
    return rect;
}

Rectanglef Atlas::imageRectf(Id const &id) const
{
    DENG2_ASSERT(d->allocator.get());

    Rectanglei rect;
    d->allocator->rect(id, rect);

    // Normalize within the atlas area.
    return Rectanglef(float(rect.topLeft.x) / float(d->totalSize.x),
                      float(rect.topLeft.y) / float(d->totalSize.y),
                      float(rect.width())   / float(d->totalSize.x),
                      float(rect.height())  / float(d->totalSize.y));
}

Image Atlas::image(Id const &id) const
{
    if(d->hasBacking() && d->allocator.get() && contains(id))
    {
        return d->backing.subImage(imageRect(id));
    }
    return Image();
}

void Atlas::commit() const
{
    if(!d->needCommit) return;

    LOG_AS("Atlas");

    if(d->mustCommitFull())
    {
        DENG2_ASSERT(d->backing.size() == d->totalSize);
        LOG_DEBUG("Full commit ") << d->backing.size().asText();

        commitFull(d->backing);
    }
    else
    {
        LOG_DEBUG("Partial commit ") << d->changedArea.asText();

        // An extra copy is done to crop to the changed area.
        commit(d->backing.subImage(d->changedArea), d->changedArea.topLeft);
    }

    d->needCommit = false;
    d->needFullCommit = false;
}

} // namespace de
