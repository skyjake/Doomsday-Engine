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

#include "ui/Data"
#include "ContextWidgetOrganizer"
#include "ui/ActionItem"
#include "ui/SubmenuItem"
#include "ui/VariableToggleItem"
#include "GridLayout"
#include "scrollareawidget.h"
#include "buttonwidget.h"

/**
 * Menu with an N-by-M grid of items (child widgets).
 *
 * One of the dimensions of the grid can be configured to use ui::Expand
 * policy, but then the child widgets must manage their size on that axis by
 * themselves.
 *
 * A sort order for the items can be optionally defined using
 * MenuWidget::ISortOrder. Sorting affects layout only, not the actual order of
 * the children.
 *
 * MenuWidget uses a ContextWidgetOrganizer to create widgets based on the
 * provided menu items. The organizer can be queried to find widgets matching
 * specific items.
 */
class MenuWidget : public ScrollAreaWidget
{
    Q_OBJECT

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
     * the specified number of columns/rows is ignored for that dimension. Both
     * dimensions cannot be set to zero columns/rows.
     *
     * @param columns       Number of columns in the grid.
     * @param columnPolicy  Policy for sizing columns.
     * @param rows          Number of rows in the grid.
     * @param rowPolicy     Policy for sizing rows.
     * @param layoutMode    Layout mode (column or row first).
     */
    void setGridSize(int columns, ui::SizePolicy columnPolicy,
                     int rows, ui::SizePolicy rowPolicy,
                     GridLayout::Mode layoutMode = GridLayout::ColumnFirst);

    ui::Data &items();

    ui::Data const &items() const;

    /**
     * Sets the data context of the menu to some existing context. The context
     * must remain in existence until the MenuWidget is deleted.
     *
     * @param items  Ownership not taken.
     */
    void setItems(ui::Data const &items);

    ContextWidgetOrganizer const &organizer() const;

    /**
     * Returns the number of visible items in the menu. Hidden items are not
     * included in this count.
     */
    int count() const;

    /**
     * Determines if a widget is included in the menu. Hidden widgets are not
     * part of the menu.
     *
     * @param widget  Widget.
     *
     * @return @c true, if the widget is laid out as part of the menu.
     */
    bool isWidgetPartOfMenu(de::Widget const &widget) const;

    /**
     * Lays out children of the menu according to the grid setup. This should
     * be called if children are manually added or removed from the menu.
     */
    void updateLayout();

    /**
     * Provides read-only access to the layout metrics.
     */
    GridLayout const &layout() const;

    // Events.
    void update();
    bool handleEvent(de::Event const &event);

public slots:
    void dismissPopups();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_MENUWIDGET_H
