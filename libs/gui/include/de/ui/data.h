/** @file data.h  UI data context.
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

#ifndef LIBAPPFW_UI_DATA_H
#define LIBAPPFW_UI_DATA_H

#include <de/observers.h>
#include <functional>
#include "de/guiwidget.h"

namespace de {
namespace ui {

class Item;

/**
 * UI data context containing an enumerable collection of items. ui::Data and
 * ui::Item are pure content -- they know nothing about how the data is
 * presented. There may be multiple simultaneous, alternative presentations of
 * the same context and items.
 *
 * Modifying Data will automatically cause the changes to be reflected in
 * any widget currently presenting the data context's items.
 *
 * Data has ownership of all the items in it.
 *
 * @see ChildWidgetOrganizer
 *
 * @ingroup uidata
 */
class LIBGUI_PUBLIC Data
{
public:
    typedef dsize Pos;

    static dsize const InvalidPos;

    /**
     * Notified when a new item is added to the data context.
     */
    DE_AUDIENCE(Addition, void dataItemAdded(Pos id, const Item &item))

    /**
     * Notified when an item has been removed from the data context. When this
     * is called @a item is no longer in the context and can be modified at
     * will.
     */
    DE_AUDIENCE(Removal, void dataItemRemoved(Pos oldId, Item &item))

    DE_AUDIENCE(OrderChange, void dataItemOrderChanged())

public:
    Data();

    virtual ~Data() = default;

    virtual Data &clear() = 0;

    inline bool isEmpty() const { return !size(); }

    inline Data &operator<<(Item *item) { return append(item); }

    inline Data &append(Item *item) { return insert(size(), item); }

    /**
     * Insert an item into the data context.
     *
     * @param pos   Position of the item.
     * @param item  Item to insert. Context gets ownership.
     *
     * @return Reference to this Data.
     */
    virtual Data &insert(Pos pos, Item *item) = 0;

    virtual void remove(Pos pos) = 0;

    virtual Item *take(Pos pos) = 0;

    virtual Item &at(Pos pos) = 0;

    virtual const Item &at(Pos pos) const = 0;

    /**
     * Finds the position of a specific item.
     *
     * @param item  Item to find.
     *
     * @return The items' position, or Data::InvalidPos if not found.
     */
    virtual Pos find(const Item &item) const = 0;

    virtual Pos findLabel(const String &label) const = 0;

    /**
     * Finds the position of an item with a specific data.
     *
     * @param data  Data to find.
     *
     * @return The items' position, or Data::InvalidPos if not found.
     */
    virtual Pos findData(const Value &data) const = 0;

    enum SortMethod { Ascending, Descending };

    virtual void sort(SortMethod method = Ascending);

    typedef std::function<bool (const Item &, const Item &)> LessThanFunc;

    virtual void sort(LessThanFunc func) = 0;

    virtual void stableSort(LessThanFunc func) = 0;

    /**
     * Returns the total number of items in the data context.
     */
    virtual dsize size() const = 0;

    LoopResult forAll(const std::function<LoopResult (Item &)>& func);

    LoopResult forAll(const std::function<LoopResult (const Item &)> &func) const;

private:
    DE_PRIVATE(d)
};

typedef Data::Pos DataPos;

} // namespace ui
} // namespace de

#endif // LIBAPPFW_UI_DATA_H
