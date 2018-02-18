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

#include "entitymap.h"

using namespace de;

namespace gloom {

DENG2_PIMPL(EntityMap)
{
    struct Block {
        QList<const Entity *> entities; // owned
    };
    typedef QList<Block *> Blocks;

    Rectangled mapBounds;
    float      blockSize;
    Blocks     blocks;
    Vector2i   size;

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

        qDebug() << "Total blocks:" << size.area();
        for (int i = 0; i < size.area(); ++i)
        {
            blocks << 0;
        }
    }

    Vector2i blockCoord(const Vector2f &pos) const
    {
        return Vector2i(int(clamp(0.0, (pos.x + mapBounds.width()/2)  / blockSize, size.x - 1.0)),
                        int(clamp(0.0, (pos.y + mapBounds.height()/2) / blockSize, size.y - 1.0)));
    }

    int blockIndex(const Vector2f &pos) const
    {
        Vector2i coord = blockCoord(pos);
        return coord.x + coord.y * size.x;
    }

    Block &block(const Vector2f &pos)
    {
        int idx = blockIndex(pos);
        if (!blocks[idx])
        {
            blocks[idx] = new Block;
        }
        return *blocks[idx];
    }

    const Block *block(const Vector2f &pos) const
    {
        return blocks.at(blockIndex(pos));
    }

    const Block *blockAtCoord(const Vector2i &blockPos) const
    {
        const int idx = blockPos.x + blockPos.y * size.x;
        if (idx < 0 || idx >= blocks.size()) return 0;
        return blocks.at(idx);
    }

    void insert(const Entity &entity)
    {
        block(entity.position().xz()).entities.append(&entity);
    }

    /*
    void iterateBlockAtCoord(Vector2i const &coord, Vector3f const &origin, float radius,
                             Callback const &callback) const
    {
        Instance::Block const *bk = blockAtCoord(coord);
        if(bk)
        {
            QList<Entity const *> found;
            foreach(Entity const *e, bk->entities)
            {
                if((e->position() - origin).length() < radius)
                {
                    found << e;
                }
            }

            // Sort by distance to the center.
            qSort(found.begin(), found.end(), [&] (Entity const *a, Entity const *b) {
                return (a->position() - origin).lengthSquared() >
                       (b->position() - origin).lengthSquared();
            });

            // Do the callback for each.
            foreach(Entity const *e, found)
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

EntityMap::EntityList EntityMap::listRegionBackToFront(const Vector3f &pos, float radius) const
{
    Vector2i min = d->blockCoord(pos.xz() - Vector2f(radius, radius));
    Vector2i max = d->blockCoord(pos.xz() + Vector2f(radius, radius));

    EntityList found;
    for (int y = min.y; y <= max.y; ++y)
    {
        for (int x = min.x; x <= max.x; ++x)
        {
            if (const auto *bk = d->blockAtCoord(Vector2i(x, y)))
            {
                foreach (Entity const *e, bk->entities)
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
    qSort(found.begin(), found.end(), [&pos] (const Entity *a, const Entity *b) {
        return (a->position() - pos).lengthSquared() >
               (b->position() - pos).lengthSquared();
    });

    return found;
}

void EntityMap::iterateRegion(const Vector3f &                    pos,
                              float                               radius,
                              std::function<void(const Entity &)> callback) const
{
    // Do the callback for each.
    foreach (Entity const *e, listRegionBackToFront(pos, radius))
    {
        callback(*e);
    }
}

} // namespace gloom
