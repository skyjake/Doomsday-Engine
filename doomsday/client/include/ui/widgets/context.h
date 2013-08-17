/** @file context.h  UI data context.
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

#ifndef DENG_CLIENT_UI_CONTEXT_H
#define DENG_CLIENT_UI_CONTEXT_H

#include <de/Observers>
#include "ui/widgets/guiwidget.h"

namespace ui {

class Item;

/**
 * UI data context containing an enumerable collection of items. Context and
 * ui::Item are pure data -- they know nothing about how the data is presented.
 * There may be multiple simultaneous, alternative presentations of the same
 * context and items.
 *
 * Modifying a Context will automatically cause the changes to be reflected in
 * any widget currently presenting the context's items.
 *
 * Context has ownership of all the items in it.
 *
 * @see ContextWidgetOrganizer
 */
class Context
{
public:
    typedef de::dsize Pos;

    static de::dsize const InvalidPos;

    /**
     * Notified when a new item is added to the context.
     */
    DENG2_DEFINE_AUDIENCE(Addition, void contextItemAdded(Pos id, Item const &item))

    /**
     * Notified when an item is removed from the context.
     */
    DENG2_DEFINE_AUDIENCE(Removal, void contextItemBeingRemoved(Pos id, Item const &item))

    DENG2_DEFINE_AUDIENCE(OrderChange, void contextItemOrderChanged())

public:
    virtual ~Context() {}

    virtual void clear() = 0;

    inline bool isEmpty() const { return !size(); }

    inline Context &operator << (Item *item) { return append(item); }

    inline Context &append(Item *item) { return insert(size(), item); }

    /**
     * Insert an item into the context.
     *
     * @param pos   Position of the item.
     * @param item  Item to insert. Context gets ownership.
     */
    virtual Context &insert(Pos pos, Item *item) = 0;

    virtual void remove(Pos pos) = 0;

    virtual Item *take(Pos pos) = 0;

    virtual Item const &at(Pos pos) const = 0;

    /**
     * Finds the position of a specific item.
     *
     * @param item  Item to find.
     *
     * @return The items' position, or Context::InvalidPos if not found.
     */
    virtual Pos find(Item const &item) const = 0;

    enum SortMethod {
        Ascending,
        Descending
    };

    virtual void sort(SortMethod method = Ascending) = 0;

    /**
     * Returns the total number of items in the context.
     */
    virtual de::dsize size() const = 0;
};

} // namespace ui

#endif // DENG_CLIENT_UI_CONTEXT_H
