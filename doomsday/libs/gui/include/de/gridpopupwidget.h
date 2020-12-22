/** @file
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

#ifndef LIBAPPFW_GRIDPOPUPWIDGET_H
#define LIBAPPFW_GRIDPOPUPWIDGET_H

#include "de/popupwidget.h"
#include "de/labelwidget.h"

namespace de {

class GridLayout;

/**
 * Popup with a grid layout for children.
 *
 * The default layout is 2 columns with unlimited rows, with the leftmost
 * column aligned to the right.
 *
 * Used for instance in the settings dialogs.
 *
 * @ingroup guiWidgets
 */
class LIBGUI_PUBLIC GridPopupWidget : public PopupWidget
{
public:
    GridPopupWidget(const String &name = String());

    /**
     * Returns the layout used by the popup's contents.
     */
    GridLayout &layout();

    LabelWidget &addSeparatorLabel(const String &labelText);

    /**
     * Adds a widget to the popup grid. The widget becomes a child of the
     * popup's container and is added to the grid layout as the next item.
     *
     * @param widget  Widget to add.
     *
     * @return Reference to this widget (fluent interface).
     */
    GridPopupWidget &operator << (GuiWidget *widget);

    /**
     * Adds an empty cell to the popup grid.
     *
     * @param rule  Amount of space to add in the cell.
     *
     * @return Reference to this widget (fluent interface).
     */
    GridPopupWidget &operator << (const Rule &rule);

    /**
     * Adds a widget to the popup grid, spanning more than one columns.
     * The widget becomes a child of the popup's container and is added
     * to the grid layout as the next item.
     *
     * @param widget    Widget to add
     * @param cellSpan  Number of columns to span.
     */
    GridPopupWidget &addSpanning(GuiWidget *widget, int cellSpan = 2);

    /**
     * Finalizes the layout of the popup. Call this after all the layout items
     * have been added to the widget.
     */
    void commit();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_GRIDPOPUPWIDGET_H
