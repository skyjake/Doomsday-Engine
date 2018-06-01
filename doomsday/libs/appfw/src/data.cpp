/** @file data.cpp  UI data context.
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

#include "de/ui/Data"
#include "de/ui/Item"
#include "de/LabelWidget"

#include <QtAlgorithms>

namespace de {
namespace ui {

dsize const Data::InvalidPos = dsize(-1);

DE_PIMPL_NOREF(Data)
{
    DE_PIMPL_AUDIENCE(Addition)
    DE_PIMPL_AUDIENCE(Removal)
    DE_PIMPL_AUDIENCE(OrderChange)
};

DE_AUDIENCE_METHOD(Data, Addition)
DE_AUDIENCE_METHOD(Data, Removal)
DE_AUDIENCE_METHOD(Data, OrderChange)

Data::Data() : d(new Impl)
{}

void Data::sort(SortMethod method)
{
    switch (method)
    {
    case Ascending:
        sort([] (Item const &a, Item const &b) {
            return a.sortKey().compareWithoutCase(b.sortKey()) < 0;
        });
        break;

    case Descending:
        sort([] (Item const &a, Item const &b) {
            return a.sortKey().compareWithoutCase(b.sortKey()) > 0;
        });
        break;
    }
}

LoopResult Data::forAll(std::function<LoopResult (Item &)> func)
{
    for (DataPos pos = 0; pos < size(); ++pos)
    {
        if (auto result = func(at(pos)))
            return result;
    }
    return LoopContinue;
}

LoopResult Data::forAll(std::function<LoopResult (Item const &)> func) const
{
    for (DataPos pos = 0; pos < size(); ++pos)
    {
        if (auto result = func(at(pos)))
            return result;
    }
    return LoopContinue;
}

} // namespace ui
} // namespace de
