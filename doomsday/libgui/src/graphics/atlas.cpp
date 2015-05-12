/** @file atlas.cpp  Image-based atlas.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
    int border;
    std::auto_ptr<IAllocator> allocator;
    Image backing;
    bool needCommit;
    bool needFullCommit;
    bool mayDefrag;
    Rectanglei changedArea;
    Time fullReportedAt;

    // Minimum backing size is 1x1 pixels.
    Instance(Public *i, Flags const &flg, Size const &size)
        : Base(i)
        , flags(flg)
        , totalSize(size.max(Size(1, 1)))
        , margin(1)
        , border(0)
        , needCommit(false)
        , needFullCommit(true)
        , mayDefrag(false)
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
         * Simple heuristic: if more than 70% of the pixels are included in the
         * changed area, simply copy the whole thing rather than doing a large
         * extra copy.
         */
        return (needFullCommit || changedPercentage() > .7f);
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

        DENG2_FOR_PUBLIC_AUDIENCE2(Reposition, i)
        {
            i->atlasContentRepositioned(self);
        }
    }

    Image::Size sizeWithBorders(Image::Size const &size)
    {
        return size + Image::Size(2 * border, 2 * border);
    }

    Rectanglei rectWithoutBorder(Id const &id) const
    {
        Rectanglei rect;
        allocator->rect(id, rect);
        return rect.shrunk(border);
    }

    DENG2_PIMPL_AUDIENCE(Reposition)
    DENG2_PIMPL_AUDIENCE(OutOfSpace)
};

DENG2_AUDIENCE_METHOD(Atlas, Reposition)
DENG2_AUDIENCE_METHOD(Atlas, OutOfSpace)

Atlas::Atlas(Flags const &flags, Size const &totalSize)
    : d(new Instance(this, flags, totalSize))
{}

void Atlas::setAllocator(IAllocator *allocator)
{
    DENG2_GUARD(this);

    clear();
    d->allocator.reset(allocator);
    if(d->allocator.get())
    {
        d->allocator->setMetrics(d->totalSize, d->margin);
        d->allocator->clear(); // using new metrics
    }
    d->markFullyChanged();
}

void Atlas::setMarginSize(dint marginPixels)
{
    d->margin = marginPixels;
    if(d->allocator.get())
    {
        d->allocator->setMetrics(d->totalSize, d->margin);
    }
}

void Atlas::setBorderSize(dint borderPixels)
{
    d->border = borderPixels;
}

void Atlas::clear()
{
    DENG2_GUARD(this);

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
    DENG2_GUARD(this);

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
    DENG2_GUARD(this);

    return d->totalSize;
}

Id Atlas::alloc(Image const &image)
{    
    if(image.isNull())
    {
        LOG_AS("Atlas");
        LOGDEV_GL_WARNING("Cannot allocate a zero-size image");
        return Id::None;
    }

    DENG2_GUARD(this);
    DENG2_ASSERT(d->allocator.get());

    Rectanglei rect;
    Id id = d->allocator->allocate(d->sizeWithBorders(image.size()), rect);

    if(id.isNone() && d->flags.testFlag(AllowDefragment) && d->mayDefrag)
    {
        // Allocation failed. Maybe we can defragment to get more space?
        d->defragment();

        // Try again...
        id = d->allocator->allocate(d->sizeWithBorders(image.size()), rect);
    }

    if(!id.isNone())
    {
        // Defragmenting may again be helpful.
        d->mayDefrag = true;

        Rectanglei const noBorders  = rect.shrunk(d->border);
        Rectanglei const withMargin = rect.expanded(d->margin);

        if(d->hasBacking())
        {
            // The margin is cleared to transparent black.
            d->backing.fill(withMargin, Image::Color(0, 0, 0, 0));

            if(d->border > 0)
            {
                if(d->flags.testFlag(WrapBordersInBackingStore))
                {
                    // Wrap using the source image (left, right, top, bottom edges).
                    d->backing.drawPartial(image, Rectanglei(0, 0, d->border, image.height()),
                                           rect.topRight() + Vector2i(-d->border, d->border));

                    d->backing.drawPartial(image, Rectanglei(image.width() - d->border, 0,
                                                             d->border, image.height()),
                                           rect.topLeft + Vector2i(0, d->border));

                    d->backing.drawPartial(image, Rectanglei(0, 0, image.width(), d->border),
                                           rect.bottomLeft() + Vector2i(d->border, -d->border));

                    d->backing.drawPartial(image, Rectanglei(0, image.height() - d->border,
                                                             image.width(), d->border),
                                           rect.topLeft + Vector2i(d->border, 0));
                }
            }
            d->backing.draw(image, noBorders.topLeft);

            //d->backing.toQImage().save(QString("backing-%1.png").arg(uint64_t(this)));

            d->markAsChanged(rect);
        }
        else
        {
            // No backing, must commit immediately.
            if(d->border > 0)
            {
                // Expand with borders (repeat edges).
                QImage const srcImg = image.toQImage();
                int const sw = srcImg.width();
                int const sh = srcImg.height();

                QImage bordered(QSize(rect.width(), rect.height()), srcImg.format());
                int const w = bordered.width();
                int const h = bordered.height();

                QPainter painter(&bordered);
                painter.setCompositionMode(QPainter::CompositionMode_Source);
                painter.fillRect(bordered.rect(), QColor(0, 0, 0, 0));

                /// @todo This really only works for a border of 1 pixels. Should
                /// repeat the same outmost edge pixels for every border. -jk

                painter.drawImage(d->border, d->border, srcImg);
                painter.drawImage(d->border, 0,     srcImg, 0, 0, sw, 1); // top
                painter.drawImage(d->border, h - 1, srcImg, 0, sh - 1, sw, 1); // bottom
                painter.drawImage(0, d->border,     srcImg, 0, 0, 1, sh); // left
                painter.drawImage(w - 1, d->border, srcImg, sw - 1, 0, 1, sh); // right

                // Corners.
                painter.drawImage(0, 0,         srcImg, 0, 0, 1, 1);
                painter.drawImage(w - 1, 0,     srcImg, sw - 1, 0, 1, 1);
                painter.drawImage(0, h - 1,     srcImg, 0, sh - 1, 1, 1);
                painter.drawImage(w - 1, h - 1, srcImg, sw - 1, sh - 1, 1, 1);

                commit(bordered, rect.topLeft);
            }
            else
            {
                commit(image, noBorders.topLeft);
            }
        }
    }
    else
    {
        LOG_AS("Atlas");
        if(!d->fullReportedAt.isValid() || d->fullReportedAt.since() > 1.0)
        {
            LOGDEV_GL_XVERBOSE("Full with %.1f%% usage") << d->usedPercentage() * 100;
            d->fullReportedAt = Time::currentHighPerformanceTime();
        }

        DENG2_FOR_AUDIENCE2(OutOfSpace, i)
        {
            i->atlasOutOfSpace(*this);
        }
    }
    return id;
}

void Atlas::release(Id const &id)
{
    if(id.isNone()) return;

    DENG2_GUARD(this);
    DENG2_ASSERT(d->allocator.get());

    d->allocator->release(id);

    // Defragmenting may help us again.
    d->mayDefrag = true;
}

bool Atlas::contains(Id const &id) const
{
    DENG2_GUARD(this);

    if(d->allocator.get())
    {
        return d->allocator->ids().contains(id);
    }
    return false;
}

int Atlas::imageCount() const
{
    DENG2_GUARD(this);
    DENG2_ASSERT(d->allocator.get());

    return d->allocator->count();
}

Atlas::Ids Atlas::allImages() const
{
    DENG2_GUARD(this);
    DENG2_ASSERT(d->allocator.get());

    return d->allocator->ids();
}

Rectanglei Atlas::imageRect(Id const &id) const
{
    DENG2_GUARD(this);
    DENG2_ASSERT(d->allocator.get());
    return d->rectWithoutBorder(id);
}

Rectanglef Atlas::imageRectf(Id const &id) const
{
    DENG2_GUARD(this);
    DENG2_ASSERT(d->allocator.get());

    Rectanglei const rect = d->rectWithoutBorder(id);

    // Normalize within the atlas area.
    return Rectanglef(float(rect.topLeft.x) / float(d->totalSize.x),
                      float(rect.topLeft.y) / float(d->totalSize.y),
                      float(rect.width()  ) / float(d->totalSize.x),
                      float(rect.height() ) / float(d->totalSize.y));
}

Image Atlas::image(Id const &id) const
{
    DENG2_GUARD(this);
    if(d->hasBacking() && d->allocator.get() && contains(id))
    {
        return d->backing.subImage(imageRect(id));
    }
    return Image();
}

void Atlas::commit() const
{
    DENG2_GUARD(this);

    if(!d->needCommit || !d->hasBacking()) return;

    LOG_AS("Atlas");

    if(d->mustCommitFull())
    {
        DENG2_ASSERT(d->backing.size() == d->totalSize);
        if(d->flags.testFlag(LogCommitsAsXVerbose))
        {
            LOGDEV_GL_XVERBOSE("Full commit ") << d->backing.size().asText();
        }
        commitFull(d->backing);
    }
    else
    {
        if(d->flags.testFlag(LogCommitsAsXVerbose))
        {
            LOGDEV_GL_XVERBOSE("Partial commit ") << d->changedArea.asText();
        }

        // An extra copy is done to crop to the changed area.
        commit(d->backing.subImage(d->changedArea), d->changedArea.topLeft);
    }

    d->needCommit = false;
    d->needFullCommit = false;
}

} // namespace de
