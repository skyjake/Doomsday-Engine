/** @file data.cpp  UI data context.
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

#include "de/ui/Data"
#include "de/ui/Item"
#include "de/LabelWidget"

#include <QtAlgorithms>

namespace de {
namespace ui {

dsize const Data::InvalidPos = dsize(-1);

static bool itemLessThan(Item const &a, Item const &b)
{
    return a.sortKey().compareWithoutCase(b.sortKey()) < 0;
}

static bool itemGreaterThan(Item const &a, Item const &b)
{
    return a.sortKey().compareWithoutCase(b.sortKey()) > 0;
}

DENG2_PIMPL_NOREF(Data)
{
    DENG2_PIMPL_AUDIENCE(Addition)
    DENG2_PIMPL_AUDIENCE(Removal)
    DENG2_PIMPL_AUDIENCE(OrderChange)
};

DENG2_AUDIENCE_METHOD(Data, Addition)
DENG2_AUDIENCE_METHOD(Data, Removal)
DENG2_AUDIENCE_METHOD(Data, OrderChange)

Data::Data() : d(new Instance)
{}

void Data::sort(SortMethod method)
{
    switch(method)
    {
    case Ascending:
        sort(itemLessThan);
        break;

    case Descending:
        sort(itemGreaterThan);
        break;
    }
}

} // namespace ui
} // namespace de
