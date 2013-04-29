/** @file rowatlasallocator.cpp  Row-based atlas allocator.
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

#include "de/RowAtlasAllocator"

#include <QList>

namespace de {

DENG2_PIMPL(RowAtlasAllocator)
{
    typedef QList<Rectanglei> RectList;

    Atlas::Size size;
    int margin;
    Allocations allocs;
    RectList unused;
    Vector2i rover;
    int rowHeight;

    Instance(Public *i) : Base(i), margin(0), rowHeight(0)
    {}

    bool allocUsingRover(Atlas::Size const &allocSize, Rectanglei &rect)
    {
        int const w = allocSize.x;
        int const h = allocSize.y;
        int availHoriz = size.x - rover.x - margin;
        int availVert  = size.y - rover.y - margin;

        // The margin will be left as a gap between regions.

        if(availHoriz >= w && availVert >= h)
        {
            // Fits on the current row.
            rect = Rectanglei::fromSize(rover, allocSize);
            rowHeight = de::max(rowHeight, h + margin);
        }
        else if(availVert - rowHeight >= h)
        {
            // There is room below.
            if(availHoriz >= 8 && rowHeight > margin)
            {
                // This row is full; mark the unused space at the end of the row.
                unused.append(Rectanglei::fromSize(rover, Atlas::Size(availHoriz, rowHeight - margin)));
            }

            // Move to the next row.
            rover.x = margin;
            rover.y += rowHeight;

            rect = Rectanglei::fromSize(rover, allocSize);
            rowHeight = h + margin;
        }
        else
        {
            // Rover appears to be near the bottom right corner.
            return false;
        }

        // Advance the rover.
        rover.x += w + margin;

        return true;
    }

    bool reuseUnusedSpace(Atlas::Size const &allocSize, Rectanglei &rect)
    {
        DENG2_FOR_EACH(RectList, i, unused)
        {
            if(i->width() >= allocSize.x && i->height() >= allocSize.y)
            {
                // This region of available space is big enough for us.
                rect = Rectanglei::fromSize(i->topLeft, allocSize);
                unused.erase(i);
                return true;
            }
        }
        return false;
    }

    struct ContentSize {
        Id::Type id;
        int width;
        int height;

        ContentSize(Id const &allocId, Vector2ui const &size)
            : id(allocId), width(size.x), height(size.y) {}

        // Sort descending.
        bool operator < (ContentSize const &other) const {
            if(height == other.height) {
                // Secondary sorting by descending width.
                return width > other.width;
            }
            return height > other.height;
        }
    };

    bool optimize()
    {
        // Set up a LUT based on descending allocation width.
        QList<ContentSize> descending;
        DENG2_FOR_EACH(Allocations, i, allocs)
        {
            descending.append(ContentSize(i.key(), i.value().size()));
        }
        qSort(descending);

        Allocations optimal;
        rover = Vector2i(margin, margin);
        rowHeight = 0;
        unused.clear();

        /*
         * Attempt to optimize space usage by placing the longest allocations
         * possible on each row. This will never produce a worse layout than
         * the previous one -- we will always succeed in placing all the
         * allocations (more) optimally.
         */
        while(!descending.isEmpty())
        {
            int availHoriz = size.x - rover.x - margin;
            bool moveOn = false;

            // If the narrowest allocation doesn't fit on this row, we have to
            // move on to the next row.
            if(descending.back().width > availHoriz)
            {
                moveOn = true;
            }

            // Find the longest alloc that fits.
            DENG2_FOR_EACH(QList<ContentSize>, i, descending)
            {
                if(!moveOn && i->width > availHoriz)
                {
                    // We'll try the next longest instead.
                    continue;
                }

                // We've found the longest suitable one.
                Rectanglei optRect;
                if(!allocUsingRover(allocs[i->id].size(), optRect))
                {
                    // Failed to optimize: maybe the new total size is smaller
                    // than what we had before.
                    return false;
                }
                optimal[i->id] = optRect;

                // This rectangle has been defragmented.
                descending.erase(i);
                break;
            }
        }

        // Use the new layout.
        allocs = optimal;
        return true;
    }
};

RowAtlasAllocator::RowAtlasAllocator() : d(new Instance(this))
{}

void RowAtlasAllocator::setMetrics(Atlas::Size const &totalSize, int margin)
{
    d->size   = totalSize;
    d->margin = margin;
}

void RowAtlasAllocator::clear()
{
    d->allocs.clear();
    d->unused.clear();

    d->rover     = Vector2i(d->margin, d->margin);
    d->rowHeight = 0;
}

Id RowAtlasAllocator::allocate(Atlas::Size const &size, Rectanglei &rect)
{
    // The rover proceeds along rows.
    if(!d->allocUsingRover(size, rect))
    {
        // Rover ran out of space.
        if(!d->reuseUnusedSpace(size, rect))
        {
            // We're completely tapped out.
            return 0;
        }
    }

    Id newId;
    d->allocs[newId] = rect;
    return newId;
}

void RowAtlasAllocator::release(Id const &id)
{
    DENG2_ASSERT(d->allocs.contains(id));

    d->unused.append(d->allocs[id]);
    d->allocs.remove(id);
}

int RowAtlasAllocator::count() const
{
    return d->allocs.size();
}

Atlas::Ids RowAtlasAllocator::ids() const
{
    Atlas::Ids ids;
    foreach(Id const &id, d->allocs.keys())
    {
        ids.insert(id);
    }
    return ids;
}

void RowAtlasAllocator::rect(Id const &id, Rectanglei &rect) const
{
    DENG2_ASSERT(d->allocs.contains(id));
    rect = d->allocs[id];
}

RowAtlasAllocator::Allocations RowAtlasAllocator::allocs() const
{
    return d->allocs;
}

bool RowAtlasAllocator::optimize()
{
    return d->optimize();
}

} // namespace de
