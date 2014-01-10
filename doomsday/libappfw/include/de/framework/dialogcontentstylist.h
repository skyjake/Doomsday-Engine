/** @file dialogcontentstylist.h  Sets the style for widgets in a dialog.
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

#ifndef LIBAPPFW_DIALOGCONTENTSTYLIST_H
#define LIBAPPFW_DIALOGCONTENTSTYLIST_H

#include "libappfw.h"
#include "../ui/Stylist"

#include <de/Widget>

namespace de {

class DialogWidget;

/**
 * Sets the style for widgets in a dialog.
 */
class LIBAPPFW_PUBLIC DialogContentStylist
        : public ui::Stylist,
          DENG2_OBSERVES(Widget, ChildAddition)
{
public:
    DialogContentStylist();
    DialogContentStylist(DialogWidget &dialog);
    DialogContentStylist(GuiWidget &container);

    ~DialogContentStylist();

    void setContainer(GuiWidget &container);

    void applyStyle(GuiWidget &widget);

    // Observes when new children are added.
    void widgetChildAdded(Widget &child);

private:
    GuiWidget *_container;
};

} // namespace de

#endif // LIBAPPFW_DIALOGCONTENTSTYLIST_H
