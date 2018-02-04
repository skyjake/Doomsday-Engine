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

#include "de/ui/FilteredData"
#include "de/ui/Item"

#include <QHash>
#include <QList>

namespace de {
namespace ui {

DENG2_PIMPL(FilteredData)
, DENG2_OBSERVES(Data, Addition)
, DENG2_OBSERVES(Data, Removal)
, DENG2_OBSERVES(Data, OrderChange)
{
    typedef QHash<Item const *, Pos> PosMapping;

    Data const &source;
    QList<Item const *> items; ///< Maps filtered items to source items.
    PosMapping reverseMapping;
    FilterFunc isItemAccepted;

    Impl(Public *i, Data const &source)
        : Base(i)
        , source(source)
    {
        source.audienceForAddition()    += this;
        source.audienceForRemoval()     += this;
        source.audienceForOrderChange() += this;
    }

    void dataItemAdded(Pos, Item const &item)
    {
        if (isItemAccepted && isItemAccepted(item))
        {
            // New items always go in the end so we don't need to redo the reverse
            // item mapping.
            Pos pos = items.size();
            items << &item;
            reverseMapping.insert(&item, pos);

            DENG2_FOR_PUBLIC_AUDIENCE2(Addition, i) i->dataItemAdded(pos, item);
        }
    }

    void dataItemRemoved(Pos, Item &item)
    {
        auto mapped = reverseMapping.find(&item);
        if (mapped != reverseMapping.end())
        {
            Pos oldPos = mapped.value();
            items.removeAt(oldPos);
            reverseMapping.erase(mapped);

            // Removing the last item is cheap because the existing reverse mappings
            // will not be affected.
            if (oldPos != Pos(items.size()))
            {
                // Update reverse mapping to account for the removed item.
                QMutableHashIterator<Item const *, Pos> iter(reverseMapping);
                while (iter.hasNext())
                {
                    if (iter.next().value() > oldPos)
                    {
                        iter.setValue(iter.value() - 1);
                    }
                }
            }

            DENG2_FOR_PUBLIC_AUDIENCE2(Removal, i) i->dataItemRemoved(oldPos, item);
        }
    }

    void dataItemOrderChanged()
    {
        remap();
        DENG2_FOR_PUBLIC_AUDIENCE2(OrderChange, i) i->dataItemOrderChanged();
    }

    void applyFilter(FilterFunc filterFunc)
    {
        items.clear();
        reverseMapping.clear();

        // A filter must be defined.
        if (!filterFunc) return;

        for (Pos i = 0; i < source.size(); ++i)
        {
            Item const *item = &source.at(i);
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
        PosMapping const oldMapping = reverseMapping;
        applyFilter([&oldMapping] (Item const &item)
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

FilteredData::FilteredData(Data const &source)
    : d(new Impl(this, source))
{}

Data &FilteredData::clear()
{
    throw ImmutableError("FilteredData::clear", "Cannot clear an immutable data model");
}

void FilteredData::setFilter(std::function<bool (Item const &item)> isItemAccepted)
{
    d->isItemAccepted = isItemAccepted;
    refilter();
}

void FilteredData::refilter()
{
    auto const oldMapping = d->reverseMapping;

    d->applyFilter(d->isItemAccepted);

    // Notify about items that were removed and added in the filtering.
    for (auto iter = oldMapping.constBegin(); iter != oldMapping.constEnd(); ++iter)
    {
        if (!d->reverseMapping.contains(iter.key()))
        {
            DENG2_FOR_AUDIENCE2(Removal, i) i->dataItemRemoved(iter.value(), *const_cast<Item *>(iter.key()));
        }
    }
    for (auto iter = d->reverseMapping.constBegin(); iter != d->reverseMapping.constEnd(); ++iter)
    {
        if (!oldMapping.contains(iter.key()))
        {
            DENG2_FOR_AUDIENCE2(Addition, i) i->dataItemAdded(iter.value(), *iter.key());
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
    DENG2_ASSERT(pos < size());
    return *const_cast<Item *>(d->items.at(pos));
}

Item const &FilteredData::at(Pos pos) const
{
    DENG2_ASSERT(pos < size());
    return *d->items.at(pos);
}

Data::Pos FilteredData::find(Item const &item) const
{
    auto found = d->reverseMapping.constFind(&item);
    if (found != d->reverseMapping.constEnd())
    {
        return found.value();
    }
    return InvalidPos;
}

Data::Pos FilteredData::findLabel(String const &label) const
{
    for (Pos i = 0; i < Pos(d->items.size()); ++i)
    {
        if (d->items.at(i)->label() == label) return i;
    }
    return InvalidPos;
}

Data::Pos FilteredData::findData(QVariant const &data) const
{
    for (Pos i = 0; i < Pos(d->items.size()); ++i)
    {
        if (d->items.at(i)->data() == data) return i;
    }
    return InvalidPos;
}

void FilteredData::sort(LessThanFunc lessThan)
{
    qSort(d->items.begin(), d->items.end(), [this, &lessThan] (Item const *a, Item const *b) {
        return lessThan(*a, *b);
    });
    d->updateReverseMapping();

    DENG2_FOR_AUDIENCE2(OrderChange, i) i->dataItemOrderChanged();
}

void FilteredData::stableSort(LessThanFunc lessThan)
{
    qStableSort(d->items.begin(), d->items.end(), [this, &lessThan] (Item const *a, Item const *b) {
        return lessThan(*a, *b);
    });
    d->updateReverseMapping();

    DENG2_FOR_AUDIENCE2(OrderChange, i) i->dataItemOrderChanged();
}

dsize FilteredData::size() const
{
    return d->items.size();
}

} // namespace ui
} // namespace de
