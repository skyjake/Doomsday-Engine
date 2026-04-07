/** @file dialogcontentstylist.h  Sets the style for widgets in a dialog.
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

#ifndef LIBAPPFW_DIALOGCONTENTSTYLIST_H
#define LIBAPPFW_DIALOGCONTENTSTYLIST_H

#include "libgui.h"
#include "de/ui/stylist.h"

#include <de/guiwidget.h>

namespace de {

class DialogWidget;

/**
 * Sets the style for widgets in a dialog.
 *
 * @ingroup appfw
 */
class LIBGUI_PUBLIC DialogContentStylist
    : public ui::Stylist
    , DE_OBSERVES(Widget, ChildAddition)
{
public:
    DialogContentStylist();
    DialogContentStylist(DialogWidget &dialog);
    DialogContentStylist(GuiWidget &container);

    ~DialogContentStylist() override;

    void clear();

    void setContainer(GuiWidget &container);

    /**
     * Adds a new container without detaching from the existing one(s).
     *
     * @param container  New container to style.
     */
    void addContainer(GuiWidget &container);

    void setInfoStyle(bool useInfoStyle);

    void setAdjustMargins(bool yes);

    void applyStyle(GuiWidget &widget) const override;

    // Observes when new children are added.
    void widgetChildAdded(Widget &child) override;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_DIALOGCONTENTSTYLIST_H
