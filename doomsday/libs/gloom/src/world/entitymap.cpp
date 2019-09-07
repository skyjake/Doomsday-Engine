/** @file entitymap.cpp
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "gloom/world/entitymap.h"

using namespace de;

namespace gloom {

DE_PIMPL(EntityMap)
{
    struct Block {
        de::List<const Entity *> entities; // owned
    };
    typedef de::List<Block *> Blocks;

    Rectangled mapBounds;
    float      blockSize;
    Blocks     blocks;
    Vec2i   size;

    Impl(Public *i) : Base(i), blockSize(32)
    {}

    void clear()
    {
        blocks.clear();
    }

    void initForSize(const Rectangled &boundsInMeters)
    {
        clear();

        mapBounds = boundsInMeters;
        size.x = int(std::ceil(mapBounds.width() / blockSize));
        size.y = int(std::ceil(mapBounds.height() / blockSize));

        debug("Total blocks: %i", size.area());
        for (int i = 0; i < size.area(); ++i)
        {
            blocks << 0;
        }
    }

    Vec2i blockCoord(const Vec2f &pos) const
    {
        return Vec2i(int(clamp(0.0, (pos.x + mapBounds.width()/2)  / blockSize, size.x - 1.0)),
                     int(clamp(0.0, (pos.y + mapBounds.height()/2) / blockSize, size.y - 1.0)));
    }

    int blockIndex(const Vec2f &pos) const
    {
        Vec2i coord = blockCoord(pos);
        return coord.x + coord.y * size.x;
    }

    Block &block(const Vec2f &pos)
    {
        int idx = blockIndex(pos);
        if (!blocks[idx])
        {
            blocks[idx] = new Block;
        }
        return *blocks[idx];
    }

    const Block *block(const Vec2f &pos) const
    {
        return blocks.at(blockIndex(pos));
    }

    const Block *blockAtCoord(const Vec2i &blockPos) const
    {
        const int idx = blockPos.x + blockPos.y * size.x;
        if (idx < 0 || idx >= blocks.sizei()) return 0;
        return blocks.at(idx);
    }

    void insert(const Entity &entity)
    {
        block(entity.position().xz()).entities.append(&entity);
    }

    /*
    void iterateBlockAtCoord(const Vec2i &coord, const Vec3f &origin, float radius,
                             const Callback &callback) const
    {
        const Instance::Block *bk = blockAtCoord(coord);
        if(bk)
        {
            QList<const Entity *> found;
            foreach(const Entity *e, bk->entities)
            {
                if((e->position() - origin).length() < radius)
                {
                    found << e;
                }
            }

            // Sort by distance to the center.
            qSort(found.begin(), found.end(), [&] (const Entity *a, const Entity *b) {
                return (a->position() - origin).lengthSquared() >
                       (b->position() - origin).lengthSquared();
            });

            // Do the callback for each.
            foreach(const Entity *e, found)
            {
                callback(*e);
            }
        }
    }*/
};

EntityMap::EntityMap() : d(new Impl(this))
{}

void EntityMap::clear()
{
    d->clear();
}

void EntityMap::setBounds(const Rectangled &bounds)
{
    d->initForSize(bounds);
}

void EntityMap::insert(const Entity &entity)
{
    d->insert(entity);
}

EntityMap::EntityList EntityMap::listRegionBackToFront(const Vec3f &pos, float radius) const
{
    Vec2i min = d->blockCoord(pos.xz() - Vec2f(radius, radius));
    Vec2i max = d->blockCoord(pos.xz() + Vec2f(radius, radius));

    EntityList found;
    for (int y = min.y; y <= max.y; ++y)
    {
        for (int x = min.x; x <= max.x; ++x)
        {
            if (const auto *bk = d->blockAtCoord(Vec2i(x, y)))
            {
                for (const Entity *e : bk->entities)
                {
                    if ((e->position() - pos).length() < radius)
                    {
                        found << e;
                    }
                }
            }
        }
    }

    // Sort by distance to the center.
    std::sort(found.begin(), found.end(), [&pos] (const Entity *a, const Entity *b) {
        return (a->position() - pos).lengthSquared() >
               (b->position() - pos).lengthSquared();
    });

    return found;
}

void EntityMap::iterateRegion(const Vec3f &pos,
                              float        radius,
                              const std::function<void(const Entity &)> &callback) const
{
    // Do the callback for each.
    for (const Entity *e : listRegionBackToFront(pos, radius))
    {
        callback(*e);
    }
}

} // namespace gloom
