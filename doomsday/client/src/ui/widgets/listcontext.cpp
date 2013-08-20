/** @file listcontext.cpp  List-based UI data context.
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

#include "ui/widgets/listcontext.h"

#include <QtAlgorithms>
#include <algorithm>

using namespace de;
using namespace ui;

ListContext::~ListContext()
{
    // Delete all items.
    qDeleteAll(_items);
}

dsize ListContext::size() const
{
    return _items.size();
}

Item const &ListContext::at(Pos pos) const
{
    DENG2_ASSERT(pos < size());
    return *_items.at(pos);
}

Context::Pos ListContext::find(Item const &item) const
{
    for(Pos i = 0; i < size(); ++i)
    {
        if(&at(i) == &item) return i;
    }
    return InvalidPos;
}

Context::Pos ListContext::findData(QVariant const &data) const
{
    for(Pos i = 0; i < size(); ++i)
    {
        if(at(i).data() == data) return i;
    }
    return InvalidPos;
}

void ListContext::clear()
{
    while(!isEmpty())
    {
        remove(size() - 1);
    }
}

Context &ListContext::insert(Pos pos, Item *item)
{    
    _items.insert(pos, item);
    item->setContext(*this);

    // Notify.
    DENG2_FOR_AUDIENCE(Addition, i)
    {
        i->contextItemAdded(pos, *item);
    }

    return *this;
}

void ListContext::remove(Pos pos)
{
    delete take(pos);
}

Item *ListContext::take(Context::Pos pos)
{
    DENG2_ASSERT(pos < size());

    // Notify.
    DENG2_FOR_AUDIENCE(Removal, i)
    {
        i->contextItemBeingRemoved(pos, at(pos));
    }

    return _items.takeAt(pos);
}

struct ListItemSorter {
    Context::LessThanFunc lessThan;

    ListItemSorter(Context::LessThanFunc func) : lessThan(func) {}
    bool operator () (Item const *a, Item const *b) const {
        return lessThan(*a, *b);
    }
};

void ListContext::sort(LessThanFunc lessThan)
{
    qSort(_items.begin(), _items.end(), ListItemSorter(lessThan));

    // Notify.
    DENG2_FOR_AUDIENCE(OrderChange, i)
    {
        i->contextItemOrderChanged();
    }
}

void ListContext::stableSort(LessThanFunc lessThan)
{
    qStableSort(_items.begin(), _items.end(), ListItemSorter(lessThan));

    // Notify.
    DENG2_FOR_AUDIENCE(OrderChange, i)
    {
        i->contextItemOrderChanged();
    }
}

