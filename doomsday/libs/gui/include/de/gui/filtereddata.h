/** @file filtereddata.h  Data model that filters another model.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_UI_FILTEREDDATA_H
#define LIBAPPFW_UI_FILTEREDDATA_H

#include "data.h"
#include <functional>

namespace de {
namespace ui {

/**
 * Item collection that is a filtered subset of another ui::Data.
 *
 * FilteredData is for immutable access only: inserting and removing items is not
 * allowed, but sorting the filtered items is permitted because it doesn't affect the
 * source items. However, the source data can be freely modified and FilteredData will
 * update itself accordingly.
 *
 * The filter function is checked whenever items are added to the source data, or when
 * filtering is manually requested.
 *
 * @ingroup uidata
 */
class LIBGUI_PUBLIC FilteredData : public Data
{
public:
    /// FilteredData is meant for immutable access only. @ingroup errors
    DE_ERROR(ImmutableError);

    typedef std::function<bool (const Item &item)> FilterFunc;

public:
    /**
     * Constructs a new FilteredData.
     *
     * @param source  Source items. This must not be deleted while FilteredData is
     *                being used.
     */
    FilteredData(const Data &source);

    Data &clear() override;

    /**
     * Sets the filter function that decides which source items are included in the
     * filtered set. Any existing source items get filtered before the method returns.
     *
     * @param isItemAccepted  Callback function for determining whether an item is
     *                        filtered.
     */
    void setFilter(FilterFunc isItemAccepted);

    /**
     * Reassess all items whether they are filtered or not. This is necessary if the
     * filtering criteria change but setFilter() is not called.
     */
    void refilter();

    Data &insert(Pos pos, Item *item) override;
    void        remove(Pos pos) override;
    Item *      take(Pos pos) override;
    Item &      at(Pos pos) override;
    const Item &at(Pos pos) const override;
    Pos         find(const Item &item) const override;
    Pos         findLabel(const String &label) const override;
    Pos         findData(const Value &data) const override;
    void        sort(SortMethod method = Ascending) override { Data::sort(method); }
    void        sort(LessThanFunc lessThan) override;
    void        stableSort(LessThanFunc lessThan) override;
    dsize       size() const override;

private:
    DE_PRIVATE(d)
};

/**
 * Utility template for data that uses a specific type of item.
 */
template <typename ItemType>
class FilteredDataT : public FilteredData
{
public:
    FilteredDataT(const Data &source) : FilteredData(source) {}
    ItemType &at(Pos pos) override {
        return static_cast<ItemType &>(FilteredData::at(pos));
    }
    const ItemType &at(Pos pos) const override {
        return static_cast<const ItemType &>(FilteredData::at(pos));
    }
};

} // namespace ui
} // namespace de

#endif // LIBAPPFW_UI_FILTEREDDATA_H
