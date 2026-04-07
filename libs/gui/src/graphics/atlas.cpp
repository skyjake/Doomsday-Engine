/** @file atlas.cpp  Image-based atlas.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/atlas.h"

#include <de/logbuffer.h>

namespace de {

DE_PIMPL(Atlas)
{
    Flags flags;
    Size totalSize;
    int margin;
    int border;
    std::unique_ptr<IAllocator> allocator;
    Hash<Id::Type, Image *> deferred; ///< Only used with DeferredAllocations.
    Image backing;
    bool needCommit;
    bool needFullCommit;
    bool mayDefrag;
    List<Rectanglei> changedAreas;
    Time fullReportedAt;

    // Minimum backing size is 1x1 pixels.
    Impl(Public *i, const Flags &flg, const Size &size)
        : Base(i)
        , flags(flg)
        , totalSize(size.max(Size(1, 1)))
        , margin(1)
        , border(0)
        , needCommit(false)
        , needFullCommit(true)
        , mayDefrag(false)
    {
        if (hasBacking())
        {
            backing = Image(Image::Size(totalSize.x, totalSize.y), Image::RGBA_8888);
        }
    }

    ~Impl()
    {
        allocator.reset();
        self().cancelDeferred();
    }

    inline bool usingDeferredMode() const
    {
        return flags.testFlag(DeferredAllocations);
    }

    inline bool hasBacking() const
    {
        return flags.testFlag(BackingStore);
    }

    void markAsChanged(const Rectanglei &changedRect)
    {
        //if (needCommit)
        //{
            //changedArea |= rect;
//        }
        //else
        //{
            needCommit = true;
            //changedArea = rect;
        //}
        // Merge to earlier changes.
        changedAreas.append(changedRect);
    }

    void markFullyChanged()
    {
        needCommit = true;
        needFullCommit = true;
        changedAreas.clear();
        changedAreas.append(backing.rect());
    }

    bool mustCommitFull() const
    {
        /*
         * Simple heuristic: if more than 95% of the pixels are included in the
         * changed area, simply copy the whole thing.
         */
        return (needFullCommit || changedPercentage() > .95f);
    }

    float changedPercentage() const
    {
        if (!needCommit || totalSize == Size(0, 0)) return 0.0f;

        const duint totalPx = totalSize.x * totalSize.y;
        duint changedPx = 0;
        for (const Rectanglei &rect : changedAreas)
        {
            changedPx += rect.width() * rect.height();
        }

        return float(changedPx) / float(totalPx);
    }

    float usedPercentage() const
    {
        if (!allocator) return 0;

        duint totalPx = totalSize.x * totalSize.y;
        duint usedPx = 0;

        for (const auto &alloc : allocator->allocs())
        {
            usedPx += alloc.second.width() * alloc.second.height();
        }
        return float(usedPx) / float(totalPx);
    }

    /**
     * Submits the image to the backing store, or commits it if no backing
     * store is available.
     *
     * @param image  Image.
     * @param rect   Rectangle for the image determined by an IAllocator.
     */
    void submitImage(const Image &submittedImage, const Rectanglei &rect)
    {
        Image image = submittedImage.convertToFormat(hasBacking() ? backing.format()
                                                                  : submittedImage.format());
        /*{
            static int count = 0;
            NativePath p = NativePath::workPath() / Stringf("submitted-%04i.png", count++);
            debug("writing %s", p.c_str());
            image.save(p);
        }*/

        const Rectanglei noBorders  = rect.shrunk(border);
        const Rectanglei withMargin = rect.expanded(margin);

        if (hasBacking())
        {
            // The margin is cleared to transparent black.
            backing.fill(withMargin, Image::Color(0, 0, 0, 0));

            if (border > 0)
            {
                if (flags.testFlag(WrapBordersInBackingStore))
                {
                    // Wrap using the source image (left, right, top, bottom edges).
                    backing.drawPartial(image, Rectanglei(0, 0, border, image.height()),
                                           rect.topRight() + Vec2i(-border, border));

                    backing.drawPartial(image, Rectanglei(image.width() - border, 0,
                                                             border, image.height()),
                                           rect.topLeft + Vec2i(0, border));

                    backing.drawPartial(image, Rectanglei(0, 0, image.width(), border),
                                           rect.bottomLeft() + Vec2i(border, -border));

                    backing.drawPartial(image, Rectanglei(0, image.height() - border,
                                                             image.width(), border),
                                           rect.topLeft + Vec2i(border, 0));
                }
            }
            backing.draw(image, noBorders.topLeft);

            //backing.save(NativePath::homePath() / Stringf("backing-%p.png", this));

            markAsChanged(rect);
        }
        else
        {
            // No backing, must commit immediately.
            if (border > 0)
            {
                // Expand with borders (repeat edges).
                const Image &srcImg = image;
                const int    sw     = srcImg.width();
                const int    sh     = srcImg.height();

                Image bordered(rect.size(), srcImg.format());
                const int w = bordered.width();
                const int h = bordered.height();

                //QPainter painter(&bordered);
                //painter.setCompositionMode(QPainter::CompositionMode_Source);
                //painter.fillRect(bordered.rect(), QColor(0, 0, 0, 0));
                bordered.fill(Image::Color(0, 0, 0, 0));

                /// @todo This really only works for a border of 1 pixels. Should
                /// repeat the same outmost edge pixels for every border. -jk

                bordered.draw(border, border, srcImg);
                bordered.draw(border, 0,     srcImg, 0, 0, sw, 1); // top
                bordered.draw(border, h - 1, srcImg, 0, sh - 1, sw, 1); // bottom
                bordered.draw(0, border,     srcImg, 0, 0, 1, sh); // left
                bordered.draw(w - 1, border, srcImg, sw - 1, 0, 1, sh); // right

                // Corners.
                bordered.draw(0, 0,         srcImg, 0, 0, 1, 1);
                bordered.draw(w - 1, 0,     srcImg, sw - 1, 0, 1, 1);
                bordered.draw(0, h - 1,     srcImg, 0, sh - 1, 1, 1);
                bordered.draw(w - 1, h - 1, srcImg, sw - 1, sh - 1, 1, 1);

                self().commit(bordered, rect.topLeft);
            }
            else
            {
                self().commit(image, noBorders.topLeft);
            }
        }
    }

    void submitDeferred()
    {
        for (auto i = deferred.begin(); i != deferred.end(); ++i)
        {
            try
            {
                Rectanglei rect;
                allocator->rect(i->first, rect);
                submitImage(*i->second, rect);
            }
            catch (const Error &er)
            {
                LOG_GL_ERROR("Allocation %s could not be submitted: %s")
                        << i->first << er.asText();
            }
            delete i->second; // we own the Image
        }
        deferred.clear();
    }

    /**
     * Compose a new backing store with an optimal layout.
     */
    void defragment()
    {
        DE_ASSERT(hasBacking());

        const IAllocator::Allocations oldLayout = allocator->allocs();
        if (!allocator->optimize())
        {
            // Optimization did not work out.
            mayDefrag = false;
            return;
        }

        Image defragged(backing.size(), backing.format());
        defragged.fill(Image::Color(0, 0, 0, 0));

        // Copy all the images to their optimal places.
        IAllocator::Allocations optimal = allocator->allocs();
        for (const auto &i : optimal) //IAllocator::Allocations, i, optimal)
        {
            defragged.draw(backing.subImage(oldLayout[i.first]),
                           i.second.topLeft);
        }

        // Defragmentation complete, use the revised backing store.
        backing = defragged;
        markFullyChanged();
        mayDefrag = false;

        DE_NOTIFY_PUBLIC(Reposition, i)
        {
            i->atlasContentRepositioned(self());
        }
    }

    Image::Size sizeWithBorders(const Image::Size &size)
    {
        return size + Image::Size(2 * border, 2 * border);
    }

    Rectanglei rectWithoutBorder(const Id &id) const
    {
        Rectanglei rect;
        allocator->rect(id, rect);
        return rect.shrunk(border);
    }

    DE_PIMPL_AUDIENCE(Reposition)
    DE_PIMPL_AUDIENCE(OutOfSpace)
};

DE_AUDIENCE_METHOD(Atlas, Reposition)
DE_AUDIENCE_METHOD(Atlas, OutOfSpace)

Atlas::Atlas(const Flags &flags, const Size &totalSize)
    : d(new Impl(this, flags, totalSize))
{}

Flags Atlas::flags() const
{
    return d->flags;
}

void Atlas::setAllocator(IAllocator *allocator)
{
    DE_GUARD(this);

    clear();
    d->allocator.reset(allocator);
    if (d->allocator.get())
    {
        d->allocator->setMetrics(d->totalSize, d->margin);
        d->allocator->clear(); // using new metrics
    }
    d->markFullyChanged();
}

Atlas::IAllocator *Atlas::takeAllocator()
{
    return d->allocator.release();
}

void Atlas::setMarginSize(dint marginPixels)
{
    d->margin = marginPixels;
    if (d->allocator.get())
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
    DE_GUARD(this);

    if (d->allocator)
    {
        d->allocator->clear();
    }
    if (d->hasBacking())
    {
        d->backing.fill(Image::Color(0, 0, 0, 0));
        d->markFullyChanged();
    }
    d->mayDefrag = false;
}

void Atlas::setTotalSize(const Size &totalSize)
{
    DE_GUARD(this);

    d->totalSize = totalSize;

    if (d->allocator)
    {
        d->allocator->setMetrics(totalSize, d->margin);
    }

    if (d->hasBacking())
    {
        d->backing.resize(totalSize);
        d->markFullyChanged();
        d->defragment();
    }
}

Atlas::Size Atlas::totalSize() const
{
    DE_GUARD(this);

    return d->totalSize;
}

Id Atlas::alloc(const Image &image, const Id &chosenId)
{
    if (image.isNull())
    {
        LOG_AS("Atlas");
        LOGDEV_GL_VERBOSE("Attempted to allocate a null image");
        return Id::None;
    }

    DE_GUARD(this);
    DE_ASSERT(d->allocator.get());

    Rectanglei rect;
    Id id = d->allocator->allocate(d->sizeWithBorders(image.size()), rect, chosenId);

    if (id.isNone() && d->flags.testFlag(AllowDefragment) && d->mayDefrag)
    {
        // Allocation failed. Maybe we can defragment to get more space?
        d->defragment();

        // Try again...
        id = d->allocator->allocate(d->sizeWithBorders(image.size()), rect, chosenId);
    }

    if (!id.isNone())
    {
        // Defragmenting may again be helpful.
        d->mayDefrag = true;

        if (!d->usingDeferredMode())
        {
            // Submit the image to the backing store (or commit).
            d->submitImage(image, rect);
        }
        else
        {
            // Keep the image in a buffer. This will be submitted later
            // if the user chooses to do so.
            d->deferred.insert(id, new Image(image));
        }
    }
    else if (!d->usingDeferredMode())
    {
        LOG_AS("Atlas");
        if (!d->fullReportedAt.isValid() || d->fullReportedAt.since() > 1.0)
        {
            LOGDEV_GL_XVERBOSE("Full with %.1f%% usage", d->usedPercentage() * 100);
            d->fullReportedAt = Time::currentHighPerformanceTime();
        }

        DE_NOTIFY(OutOfSpace, i)
        {
            i->atlasOutOfSpace(*this);
        }
    }
    return id;
}

void Atlas::release(const Id &id)
{
    if (id.isNone()) return;

    DE_GUARD(this);
    DE_ASSERT(d->allocator.get());

    d->allocator->release(id);

    // Defragmenting may help us again.
    d->mayDefrag = true;
}

bool Atlas::contains(const Id &id) const
{
    DE_GUARD(this);

    if (d->allocator.get())
    {
        return d->allocator->ids().contains(id);
    }
    return false;
}

int Atlas::imageCount() const
{
    DE_GUARD(this);
    DE_ASSERT(d->allocator.get());

    return d->allocator->count();
}

Atlas::Ids Atlas::allImages() const
{
    DE_GUARD(this);
    DE_ASSERT(d->allocator.get());

    return d->allocator->ids();
}

Rectanglei Atlas::imageRect(const Id &id) const
{
    DE_GUARD(this);
    DE_ASSERT(d->allocator.get());
    return d->rectWithoutBorder(id);
}

Rectanglef Atlas::imageRectf(const Id &id) const
{
    DE_GUARD(this);
    DE_ASSERT(d->allocator.get());

    const Rectanglei rect = d->rectWithoutBorder(id);

    // Normalize within the atlas area.
    return Rectanglef(float(rect.topLeft.x) / float(d->totalSize.x),
                      float(rect.topLeft.y) / float(d->totalSize.y),
                      float(rect.width()  ) / float(d->totalSize.x),
                      float(rect.height() ) / float(d->totalSize.y));
}

Image Atlas::image(const Id &id) const
{
    DE_GUARD(this);
    if (d->deferred.contains(id))
    {
        return *d->deferred[id];
    }
    if (d->hasBacking() && d->allocator.get() && contains(id))
    {
        return d->backing.subImage(imageRect(id));
    }
    return Image();
}

void Atlas::commit() const
{
    DE_GUARD(this);

    LOG_AS("Atlas");
    d->submitDeferred();

    if (!d->needCommit || !d->hasBacking()) return;

    if (d->mustCommitFull())
    {
        DE_ASSERT(d->backing.size() == d->totalSize);
        //qDebug() << "Full commit:" << d->backing.size().asText();
        commitFull(d->backing);
    }
    else
    {
        for (const Rectanglei &rect : d->changedAreas)
        {
            commit(d->backing, rect);
        }
    }

    d->changedAreas.clear();
    d->needCommit = false;
    d->needFullCommit = false;
}

void Atlas::cancelDeferred()
{
    for (auto i = d->deferred.begin(); i != d->deferred.end(); ++i)
    {
        delete i->second; // we own a copy of the Image
        if (d->allocator)
        {
            release(i->first);
        }
    }
    d->deferred.clear();
}

} // namespace de
