/** @file vissprite.cpp  Vissprite pool.
 *
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "render/vissprite.h"
#include <QVector>
#include <QtAlgorithms>

using namespace de;

static bool sortByDistanceDesc(IVissprite const *a, IVissprite const *b)
{
    return a->distance() > b->distance();
}

DENG2_PIMPL_NOREF(VisspritePool)
{
    template <typename Type>
    struct Pool : private QVector<Type>
    {
        typedef Type ObjectType;
        int _cursor;
        Pool() : _cursor(0) {
            QVector<ObjectType>::reserve(512);
        }

        int size() const { return _cursor; }
        inline bool isEmpty() const { return size() == 0; }

        void clear() {
            QVector<ObjectType>::clear();
            rewind();
        }

        void rewind() {
            _cursor = 0;
        }

        ObjectType *allocate() {
            if(_cursor >= QVector<ObjectType>::size())
                append(ObjectType());
            return &(*this)[_cursor++];
        }
    };

    Pool<visflare_t> flares;
    Pool<vismodel_t> models;
    Pool<vismaskedwall_t> maskedwalls;
    Pool<vissprite_t> sprites;

    SortedVissprites allUsed;
    bool needSortAllUsed;

    Instance() : needSortAllUsed(false)
    {}

    void sortAllUsedIfNeeded()
    {
        if(!needSortAllUsed) return;
        needSortAllUsed = false;
        qStableSort(allUsed.begin(), allUsed.end(), sortByDistanceDesc);
    }
};

VisspritePool::VisspritePool() : d(new Instance())
{}

void VisspritePool::reset()
{
    d->flares.rewind();
    d->models.rewind();
    d->maskedwalls.rewind();
    d->sprites.rewind();

    d->allUsed.clear();
}

visflare_t *VisspritePool::newFlare()
{
    visflare_t *vs = d->flares.allocate();
    d->allUsed.append(vs);
    d->needSortAllUsed = true;

    vs->init();
    return vs;
}

vismaskedwall_t *VisspritePool::newMaskedWall()
{
    vismaskedwall_t *vs = d->maskedwalls.allocate();
    d->allUsed.append(vs);
    d->needSortAllUsed = true;

    vs->init();
    return vs;
}

vismodel_t *VisspritePool::newModel()
{
    vismodel_t *vs = d->models.allocate();
    d->allUsed.append(vs);
    d->needSortAllUsed = true;

    vs->init();
    return vs;
}

vissprite_t *VisspritePool::newSprite()
{
    vissprite_t *vs = d->sprites.allocate();
    d->allUsed.append(vs);
    d->needSortAllUsed = true;

    vs->init();
    return vs;
}

VisspritePool::SortedVissprites const &VisspritePool::sorted() const
{
    d->sortAllUsedIfNeeded();
    return d->allUsed;
}

vispsprite_t visPSprites[DDMAXPSPRITES];
