/** @file menuwidget.h
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

#ifndef DENG_CLIENT_MENUWIDGET_H
#define DENG_CLIENT_MENUWIDGET_H

#include "scrollareawidget.h"
#include "buttonwidget.h"

/**
 * Menu with an N-by-M grid of items (child widgets).
 *
 * One of the dimensions of the grid can be configured to use ui::Expand
 * policy, but then the child widgets must manage their size on that axis by
 * themselves.
 */
class MenuWidget : public ScrollAreaWidget
{
public:
    MenuWidget(de::String const &name = "");

    /**
     * Configures the layout grid.
     *
     * ui::Fixed policy means that the size of the menu rectangle is fixed, and
     * the size of the children is not modified.
     *
     * ui::Filled policy means that the size of the menu rectangle is fixed,
     * and the size of the children is adjusted to evenly fill the entire menu
     * rectangle.
     *
     * If one of the dimensions is set to ui::Expand policy, the menu's size in
     * that dimension is determined by the summed up size of the children, and
     * the specified number of columns/rows is ignored for that dimension. Only
     * one of the dimensions can be set to Expand.
     *
     * @param columns       Number of columns in the grid.
     * @param columnPolicy  Policy for sizing columns.
     * @param rows          Number of rows in the grid.
     * @param rowPolicy     Policy for sizing rows.
     */
    void setGridSize(int columns, ui::SizePolicy columnPolicy,
                     int rows, ui::SizePolicy rowPolicy);

    ButtonWidget *addItem(de::String const &styledText, de::Action *action = 0);

    ButtonWidget *addItem(de::Image const &image, de::String const &styledText, de::Action *action = 0);

    void removeItem(GuiWidget *child);

    /**
     * Returns the number of items in the menu.
     */
    int count() const;

    /**
     * Lays out children of the menu according to the grid setup. This should
     * be called if children are manually added or removed from the menu.
     */
    void updateLayout();

    /**
     * Constructs a rule for the width of a column, based on the widths of the
     * items in the column.
     *
     * @param column  Column index.
     *
     * @return Width rule with a reference count of one (given to the caller).
     */
    de::Rule const *newColumnWidthRule(int column) const;

    // Events.
    void update();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_MENUWIDGET_H
