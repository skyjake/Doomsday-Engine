/** @file listdata.cpp  List-based UI data context.
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

#include "de/ui/ListData"

#include <QtAlgorithms>
#include <algorithm>

namespace de {

using namespace ui;

ListData::~ListData()
{
    // Delete all items.
    qDeleteAll(_items);
}

dsize ListData::size() const
{
    return _items.size();
}

Item &ListData::at(Data::Pos pos)
{
    DENG2_ASSERT(pos < size());
    return *_items[pos];
}

Item const &ListData::at(Pos pos) const
{
    DENG2_ASSERT(pos < size());
    return *_items.at(pos);
}

Data::Pos ListData::find(Item const &item) const
{
    for(Pos i = 0; i < size(); ++i)
    {
        if(&at(i) == &item) return i;
    }
    return InvalidPos;
}

Data::Pos ListData::findData(QVariant const &data) const
{
    for(Pos i = 0; i < size(); ++i)
    {
        if(at(i).data() == data) return i;
    }
    return InvalidPos;
}

Data &ListData::clear()
{
    while(!isEmpty())
    {
        remove(size() - 1);
    }
    return *this;
}

Data &ListData::insert(Pos pos, Item *item)
{    
    _items.insert(pos, item);
    item->setDataContext(*this);

    // Notify.
    DENG2_FOR_AUDIENCE2(Addition, i)
    {
        i->dataItemAdded(pos, *item);
    }

    return *this;
}

void ListData::remove(Pos pos)
{
    delete take(pos);
}

Item *ListData::take(Data::Pos pos)
{
    DENG2_ASSERT(pos < size());

    Item *taken = _items.takeAt(pos);

    // Notify.
    DENG2_FOR_AUDIENCE2(Removal, i)
    {
        i->dataItemRemoved(pos, *taken);
    }

    return taken;
}

struct ListItemSorter {
    Data::LessThanFunc lessThan;

    ListItemSorter(Data::LessThanFunc func) : lessThan(func) {}
    bool operator () (Item const *a, Item const *b) const {
        return lessThan(*a, *b);
    }
};

void ListData::sort(LessThanFunc lessThan)
{
    qSort(_items.begin(), _items.end(), ListItemSorter(lessThan));

    // Notify.
    DENG2_FOR_AUDIENCE2(OrderChange, i)
    {
        i->dataItemOrderChanged();
    }
}

void ListData::stableSort(LessThanFunc lessThan)
{
    qStableSort(_items.begin(), _items.end(), ListItemSorter(lessThan));

    // Notify.
    DENG2_FOR_AUDIENCE2(OrderChange, i)
    {
        i->dataItemOrderChanged();
    }
}

} // namespace de
