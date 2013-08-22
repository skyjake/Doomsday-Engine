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

#include <QVariant>

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
     * Determines the item's behavior and look'n'feel. This acts as a hint for
     * the containing widget (and the responsible organizer) so it can adjust
     * its behavior accordingly.
     */
    enum SemanticFlag {
        ShownAsLabel          = 0x1,
        ShownAsButton         = 0x2,
        ShownAsToggle         = 0x4,

        ActivationClosesPopup = 0x100,
        Separator             = 0x200,

        DefaultSemantics      = ShownAsLabel

        //Action, ///< Closes popup menu.
        //Submenu,
        //Separator,
        //Toggle
    };
    Q_DECLARE_FLAGS(Semantics, SemanticFlag)

    DENG2_DEFINE_AUDIENCE(Change, void itemChanged(Item const &item))

public:
    Item(Semantics semantics = DefaultSemantics);

    Item(Semantics semantics, de::String const &label);

    virtual ~Item();

    Semantics semantics() const;

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

    void setData(QVariant const &d) { _data = d; }

    QVariant const &data() const { return _data; }

    DENG2_IS_AS_METHODS()

protected:
    /**
     * Notifies the Change audience of a changed property.
     */
    void notifyChange();

private:
    Semantics _semantics;
    Context *_context;
    de::String _label;
    QVariant _data;

    friend class Context;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Item::Semantics)

} // namespace ui

#endif // DENG_CLIENT_UI_CONTEXTITEM_H
