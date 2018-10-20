/** @file listdata.h  List-based UI data context.
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

#ifndef LIBAPPFW_UI_LISTDATA_H
#define LIBAPPFW_UI_LISTDATA_H

#include "../ui/Data"
#include "../ui/Item"

#include <de/List>

namespace de {
namespace ui {

/**
 * List-based UI data context.
 *
 * @ingroup uidata
 */
class LIBGUI_PUBLIC ListData : public Data
{
public:
    ListData() = default;
    ~ListData() override;

    dsize       size() const override;
    Item &      at(Pos pos) override;
    Item const &at(Pos pos) const override;
    Pos         find(Item const &item) const override;
    Pos         findLabel(String const &label) const override;
    Pos         findData(const Value &data) const override;

    Data &clear() override;
    Data &insert(Pos pos, Item *item) override;
    void  remove(Pos pos) override;
    Item *take(Pos pos) override;
    void  sort(SortMethod method = Ascending) override { Data::sort(method); }
    void  sort(LessThanFunc lessThan) override;
    void  stableSort(LessThanFunc lessThan) override;

private:
    typedef List<Item *> Items;
    Items _items;
};

/**
 * Utility template for data that uses a specific type of item.
 */
template <typename ItemType>
class ListDataT : public ListData
{
public:
    ItemType &at(Pos pos) override {
        return static_cast<ItemType &>(ListData::at(pos));
    }
    ItemType const &at(Pos pos) const override {
        return static_cast<ItemType const &>(ListData::at(pos));
    }
    ItemType *take(Pos pos) override {
        return static_cast<ItemType *>(ListData::take(pos));
    }
};

} // namespace ui
} // namespace de

#endif // LIBAPPFW_UI_LISTDATA_H
