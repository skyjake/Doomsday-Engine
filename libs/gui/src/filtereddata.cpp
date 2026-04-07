#include <utility>

/** @file filtereddata.cpp  Data model that filters another model.
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

#include "de/ui/filtereddata.h"
#include "de/ui/item.h"

#include <de/hash.h>
#include <de/list.h>

namespace de {
namespace ui {

DE_PIMPL(FilteredData)
, DE_OBSERVES(Data, Addition)
, DE_OBSERVES(Data, Removal)
, DE_OBSERVES(Data, OrderChange)
{
    typedef Hash<const Item *, Pos> PosMapping;

    const Data &source;
    List<const Item *> items; ///< Maps filtered items to source items.
    PosMapping reverseMapping;
    FilterFunc isItemAccepted;

    Impl(Public *i, const Data &source)
        : Base(i)
        , source(source)
    {
        source.audienceForAddition()    += this;
        source.audienceForRemoval()     += this;
        source.audienceForOrderChange() += this;
    }

    void dataItemAdded(Pos, const Item &item)
    {
        if (isItemAccepted && isItemAccepted(item))
        {
            // New items always go in the end so we don't need to redo the reverse
            // item mapping.
            Pos pos = items.size();
            items << &item;
            reverseMapping.insert(&item, pos);

            DE_NOTIFY_PUBLIC(Addition, i) i->dataItemAdded(pos, item);
        }
    }

    void dataItemRemoved(Pos, Item &item)
    {
        auto mapped = reverseMapping.find(&item);
        if (mapped != reverseMapping.end())
        {
            Pos oldPos = mapped->second;
            items.removeAt(oldPos);
            reverseMapping.erase(mapped);

            // Removing the last item is cheap because the existing reverse mappings
            // will not be affected.
            if (oldPos != Pos(items.size()))
            {
                // Update reverse mapping to account for the removed item.
                for (auto iter = reverseMapping.begin(); iter != reverseMapping.end(); ++iter)
                {
                    if (iter->second > oldPos)
                    {
                        --iter->second; /// @todo Does this actually decrement the stored value?
                    }
                }
            }

            DE_NOTIFY_PUBLIC(Removal, i) i->dataItemRemoved(oldPos, item);
        }
    }

    void dataItemOrderChanged()
    {
        remap();
        DE_NOTIFY_PUBLIC(OrderChange, i) i->dataItemOrderChanged();
    }

    void applyFilter(const FilterFunc& filterFunc)
    {
        items.clear();
        reverseMapping.clear();

        // A filter must be defined.
        if (!filterFunc) return;

        for (Pos i = 0; i < source.size(); ++i)
        {
            const Item *item = &source.at(i);
            if (filterFunc(*item))
            {
                reverseMapping.insert(item, items.size());
                items << item;
            }
        }
    }

    /**
     * Reorder the filtered items in the same order as the source items without checking
     * the filter function.
     */
    void remap()
    {
        const PosMapping oldMapping = reverseMapping;
        applyFilter([&oldMapping] (const Item &item)
        {
            // All items already mapped will be included in the filtered items.
            return oldMapping.contains(&item);
        });
    }

    /**
     * Update all reverse-mapped positions to match the filtered items' current
     * positions.
     */
    void updateReverseMapping()
    {
        reverseMapping.clear();
        for (Pos i = 0; i < Pos(items.size()); ++i)
        {
            reverseMapping.insert(items.at(i), i);
        }
    }
};

FilteredData::FilteredData(const Data &source)
    : d(new Impl(this, source))
{}

Data &FilteredData::clear()
{
    throw ImmutableError("FilteredData::clear", "Cannot clear an immutable data model");
}

void FilteredData::setFilter(std::function<bool (const Item &item)> isItemAccepted)
{
    d->isItemAccepted = std::move(isItemAccepted);
    refilter();
}

void FilteredData::refilter()
{
    const auto oldMapping = d->reverseMapping;

    d->applyFilter(d->isItemAccepted);

    // Notify about items that were removed and added in the filtering.
    for (auto iter = oldMapping.begin(); iter != oldMapping.end(); ++iter)
    {
        if (!d->reverseMapping.contains(iter->first))
        {
            DE_NOTIFY(Removal, i)
            {
                i->dataItemRemoved(iter->second, *const_cast<Item *>(iter->first));
            }
        }
    }
    for (auto iter = d->reverseMapping.begin(); iter != d->reverseMapping.end(); ++iter)
    {
        if (!oldMapping.contains(iter->first))
        {
            DE_NOTIFY(Addition, i)
            {
                i->dataItemAdded(iter->second, *iter->first);
            }
        }
    }
}

Data &FilteredData::insert(Pos, Item *)
{
    throw ImmutableError("FilteredData::insert", "Data model is immutable");
}

void FilteredData::remove(Pos)
{
    throw ImmutableError("FilteredData::remove", "Data model is immutable");
}

Item *FilteredData::take(Pos)
{
    throw ImmutableError("FilteredData::take", "Data model is immutable");
}

Item &FilteredData::at(Pos pos)
{
    DE_ASSERT(pos < size());
    return *const_cast<Item *>(d->items.at(pos));
}

const Item &FilteredData::at(Pos pos) const
{
    DE_ASSERT(pos < size());
    return *d->items.at(pos);
}

Data::Pos FilteredData::find(const Item &item) const
{
    auto found = d->reverseMapping.find(&item);
    if (found != d->reverseMapping.end())
    {
        return found->second;
    }
    return InvalidPos;
}

Data::Pos FilteredData::findLabel(const String &label) const
{
    for (Pos i = 0; i < Pos(d->items.size()); ++i)
    {
        if (d->items.at(i)->label() == label) return i;
    }
    return InvalidPos;
}

Data::Pos FilteredData::findData(const Value &data) const
{
    for (Pos i = 0; i < Pos(d->items.size()); ++i)
    {
        if (d->items.at(i)->data().compare(data) == 0) return i;
    }
    return InvalidPos;
}

void FilteredData::sort(LessThanFunc lessThan)
{
    std::sort(d->items.begin(), d->items.end(), [&lessThan] (const Item *a, const Item *b) {
        return lessThan(*a, *b);
    });
    d->updateReverseMapping();
    DE_NOTIFY(OrderChange, i) i->dataItemOrderChanged();
}

void FilteredData::stableSort(LessThanFunc lessThan)
{
    std::stable_sort(d->items.begin(), d->items.end(), [&lessThan] (const Item *a, const Item *b) {
        return lessThan(*a, *b);
    });
    d->updateReverseMapping();
    DE_NOTIFY(OrderChange, i) i->dataItemOrderChanged();
}

dsize FilteredData::size() const
{
    return d->items.size();
}

} // namespace ui
} // namespace de
