/** @file item.h Context item.
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

#ifndef DENG_CLIENT_UI_CONTEXTITEM_H
#define DENG_CLIENT_UI_CONTEXTITEM_H

#include <de/Observers>
#include <de/String>

namespace ui {

class Context;

/**
 * Data item.
 *
 * Context items are pure content -- the exact presentation parameters (widget
 * type, alignment, scaling, etc.) is determined by the container widget and/or
 * responsible organizer, not by the item. This allows one item to be presented
 * in different ways by different widgets/contexts.
 *
 * @see ui::Context
 */
class Item
{
public:
    /**
     * Informs what kind of data the item is representing. This acts as a hint
     * for the containing widget so it can adjust its behavior accordingly.
     */
    enum Semantic {
        Label,
        Action,
        Submenu,
        Separator,
        Toggle
    };

    DENG2_DEFINE_AUDIENCE(Change, void itemChanged(Item const &item))

public:
    Item(Semantic semantic = Label);

    Item(Semantic semantic, de::String const &label);

    virtual ~Item();

    Semantic semantic() const;

    void setLabel(de::String const &label);

    de::String label() const;

    void setContext(Context &context) { _context = &context; }

    bool hasContext() const { return _context != 0; }

    Context &context() const {
        DENG2_ASSERT(_context != 0);
        return *_context;
    }

    /**
     * Returns a text string that should be used for sorting the item inside a
     * context.
     */
    virtual de::String sortKey() const;

    template <typename Type>
    Type &as() {
        Type *p = dynamic_cast<Type *>(this);
        DENG2_ASSERT(p != 0);
        return *p;
    }

    template <typename Type>
    Type const &as() const {
        Type const *p = dynamic_cast<Type const *>(this);
        DENG2_ASSERT(p != 0);
        return *p;
    }

protected:
    /**
     * Notifies the Change audience of a changed property.
     */
    void notifyChange();

private:
    Semantic _semantic;
    Context *_context;
    de::String _label;

    friend class Context;
};

} // namespace ui

#endif // DENG_CLIENT_UI_CONTEXTITEM_H
