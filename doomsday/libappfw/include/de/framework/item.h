/** @file item.h  Data context item.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_UI_DATAITEM_H
#define LIBAPPFW_UI_DATAITEM_H

#include "../libappfw.h"
#include <de/Observers>
#include <de/String>

#include <QVariant>

namespace de {
namespace ui {

class Data;

/**
 * Data item.
 *
 * Items are pure content -- the exact presentation parameters (widget type,
 * alignment, scaling, etc.) is determined by the container widget and/or
 * responsible organizer, not by the item. This allows one item to be presented
 * in different ways by different widgets/contexts.
 *
 * @see ui::Data
 */
class LIBAPPFW_PUBLIC Item
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
    };
    Q_DECLARE_FLAGS(Semantics, SemanticFlag)

    DENG2_DEFINE_AUDIENCE2(Change, void itemChanged(Item const &item))

public:
    Item(Semantics semantics = DefaultSemantics);

    Item(Semantics semantics, String const &label);

    virtual ~Item();

    Semantics semantics() const;

    void setLabel(String const &label);

    String label() const;

    void setDataContext(Data &context);

    bool hasDataContext() const;

    Data &dataContext() const;

    /**
     * Returns a text string that should be used for sorting the item inside a
     * context.
     */
    virtual String sortKey() const;

    /**
     * Sets the custom user data of the item.
     *
     * @param d  Variant data to be associated with the item.
     */
    void setData(QVariant const &d);

    QVariant const &data() const;

    DENG2_AS_IS_METHODS()

protected:
    /**
     * Notifies the Change audience of a changed property.
     */
    void notifyChange();

private:
    DENG2_PRIVATE(d)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Item::Semantics)

} // namespace ui
} // namespace de

#endif // LIBAPPFW_UI_DATAITEM_H
