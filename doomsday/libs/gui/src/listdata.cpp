/** @file listdata.cpp  List-based UI data context.
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

#include "de/ui/listdata.h"
#include <algorithm>

namespace de {

using namespace ui;

ListData::~ListData()
{
    // Delete all items.
    deleteAll(_items);
}

dsize ListData::size() const
{
    return _items.size();
}

Item &ListData::at(Data::Pos pos)
{
    DE_ASSERT(pos < size());
    return *_items[pos];
}

const Item &ListData::at(Pos pos) const
{
    DE_ASSERT(pos < size());
    return *_items.at(pos);
}

Data::Pos ListData::find(const Item &item) const
{
    for (Pos i = 0; i < size(); ++i)
    {
        if (&at(i) == &item) return i;
    }
    return InvalidPos;
}

Data::Pos ListData::findLabel(const String &label) const
{
    for (Pos i = 0; i < size(); ++i)
    {
        if (at(i).label() == label) return i;
    }
    return InvalidPos;
}

Data::Pos ListData::findData(const Value &data) const
{
    for (Pos i = 0; i < size(); ++i)
    {
        if (at(i).data().compare(data) == 0) return i;
    }
    return InvalidPos;
}

Data &ListData::clear()
{
    while (!isEmpty())
    {
        remove(size() - 1);
    }
    return *this;
}

Data &ListData::insert(Pos pos, Item *item)
{
    _items.insert(pos, item);
    item->setDataContext(*this);
    DE_NOTIFY(Addition, i) { i->dataItemAdded(pos, *item); }
    return *this;
}

void ListData::remove(Pos pos)
{
    delete take(pos);
}

Item *ListData::take(Data::Pos pos)
{
    DE_ASSERT(pos < size());
    Item *taken = _items.takeAt(pos);
    DE_NOTIFY(Removal, i) { i->dataItemRemoved(pos, *taken); }
    return taken;
}

void ListData::sort(LessThanFunc lessThan)
{
    std::sort(_items.begin(), _items.end(), [&lessThan] (const Item *a, const Item *b) {
        return lessThan(*a, *b);
    });
    DE_NOTIFY(OrderChange, i) i->dataItemOrderChanged();
}

void ListData::stableSort(LessThanFunc lessThan)
{
    std::stable_sort(_items.begin(), _items.end(), [&lessThan] (const Item *a, const Item *b) {
        return lessThan(*a, *b);
    });
    DE_NOTIFY(OrderChange, i) i->dataItemOrderChanged();
}

} // namespace de
