/** @file data.h  UI data context.
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

#ifndef LIBAPPFW_UI_DATA_H
#define LIBAPPFW_UI_DATA_H

#include <de/Observers>
#include "../GuiWidget"

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
 */
class LIBAPPFW_PUBLIC Data
{
public:
    typedef dsize Pos;

    static dsize const InvalidPos;

    /**
     * Notified when a new item is added to the data context.
     */
    DENG2_DEFINE_AUDIENCE(Addition, void contextItemAdded(Pos id, Item const &item))

    /**
     * Notified when an item has been removed from the data context. When this
     * is called @a item is no longer in the context and can be modified at
     * will.
     */
    DENG2_DEFINE_AUDIENCE(Removal, void contextItemRemoved(Pos oldId, Item &item))

    DENG2_DEFINE_AUDIENCE(OrderChange, void contextItemOrderChanged())

public:
    virtual ~Data() {}

    virtual Data &clear() = 0;

    inline bool isEmpty() const { return !size(); }

    inline Data &operator << (Item *item) { return append(item); }

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

    virtual Item const &at(Pos pos) const = 0;

    /**
     * Finds the position of a specific item.
     *
     * @param item  Item to find.
     *
     * @return The items' position, or Data::InvalidPos if not found.
     */
    virtual Pos find(Item const &item) const = 0;

    /**
     * Finds the position of an item with a specific data.
     *
     * @param data  Data to find.
     *
     * @return The items' position, or Data::InvalidPos if not found.
     */
    virtual Pos findData(QVariant const &data) const = 0;

    enum SortMethod {
        Ascending,
        Descending
    };

    virtual void sort(SortMethod method = Ascending);

    typedef bool (*LessThanFunc)(Item const &, Item const &);

    virtual void sort(LessThanFunc func) = 0;

    virtual void stableSort(LessThanFunc func) = 0;

    /**
     * Returns the total number of items in the data context.
     */
    virtual dsize size() const = 0;
};

} // namespace ui
} // namespace de

#endif // LIBAPPFW_UI_DATA_H
