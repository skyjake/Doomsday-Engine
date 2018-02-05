#include "entitymap.h"

using namespace de;

DENG2_PIMPL(EntityMap)
{
    Vector2f mapSize;
    float blockSize;

    struct Block
    {
        QList<Entity *> entities; // owned

        Block()
        {}

        ~Block()
        {
            qDeleteAll(entities);
        }
    };
    typedef QList<Block *> Blocks;
    Blocks blocks;
    Vector2i size;

    Instance(Public *i) : Base(i), blockSize(64)
    {}

    void clear()
    {
        qDeleteAll(blocks);
        blocks.clear();
    }

    void initForSize(Vector2f const &sizeInMeters)
    {
        clear();

        mapSize = sizeInMeters;
        size.x = ceil(mapSize.x / blockSize);
        size.y = ceil(mapSize.y / blockSize);

        qDebug() << "Total blocks:" << size.x * size.y;
        for(int i = 0; i < size.x * size.y; ++i)
            blocks << 0;
    }

    Vector2i blockCoord(Vector2f const &pos) const
    {
        return Vector2i(clamp(0.f, (pos.x + mapSize.x/2) / blockSize, size.x - 1.f),
                        clamp(0.f, (pos.y + mapSize.y/2) / blockSize, size.y - 1.f));
    }

    int blockIndex(Vector2f const &pos) const
    {
        Vector2i coord = blockCoord(pos);
        return coord.x + coord.y * size.x;
    }

    Block &block(Vector2f const &pos)
    {
        int idx = blockIndex(pos);
        if(!blocks[idx])
        {
            blocks[idx] = new Block;
        }
        return *blocks[idx];
    }

    Block const *block(Vector2f const &pos) const
    {
        return blocks.at(blockIndex(pos));
    }

    Block const *blockAtCoord(Vector2i const &blockPos) const
    {
        int idx = blockPos.x + blockPos.y * size.x;
        if(idx < 0 || idx >= blocks.size()) return 0;
        return blocks.at(idx);
    }

    void insert(Entity *entity)
    {
        Block &bk = block(entity->position().xz());
        bk.entities.append(entity);
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

EntityMap::EntityMap() : d(new Instance(this))
{}

void EntityMap::setSize(Vector2f const &size)
{
    d->initForSize(size);
}

void EntityMap::insert(Entity *entity)
{
    d->insert(entity);
}

QList<Entity const *> EntityMap::listRegionBackToFront(const Vector3f &pos, float radius) const
{
    Vector2i min = d->blockCoord(pos.xz() - Vector2f(radius, radius));
    Vector2i max = d->blockCoord(pos.xz() + Vector2f(radius, radius));

    QList<Entity const *> found;

    for(int y = min.y; y <= max.y; ++y)
    {
        for(int x = min.x; x <= max.x; ++x)
        {
            Instance::Block const *bk = d->blockAtCoord(Vector2i(x, y));
            if(bk)
            {
                foreach(Entity const *e, bk->entities)
                {
                    if((e->position() - pos).length() < radius)
                    {
                        found << e;
                    }
                }
            }
        }
    }

    // Sort by distance to the center.
    qSort(found.begin(), found.end(), [&] (Entity const *a, Entity const *b) {
        return (a->position() - pos).lengthSquared() >
               (b->position() - pos).lengthSquared();
    });

    return found;
}

void EntityMap::iterateRegion(Vector3f const &pos, float radius, Callback const &callback) const
{
    // Do the callback for each.
    foreach(Entity const *e, listRegionBackToFront(pos, radius))
    {
        callback(*e);
    }
}
