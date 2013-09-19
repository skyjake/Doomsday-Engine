/** @file gridlayout.h Widget layout for a grid of widgets.
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

#ifndef DENG_CLIENT_GRIDLAYOUT_H
#define DENG_CLIENT_GRIDLAYOUT_H

#include "../uidefs.h"
#include "guiwidget.h"

/**
 * Widget layout for a grid of widgets.
 *
 * Layouts are utilities that modify the placement of widgets. The layout
 * instance itself does not need to remain in memory -- widget rules are
 * modified immediately as the widgets are added to the layout.
 */
class GridLayout
{
public:
    enum Mode {
        ColumnFirst,    ///< Widgets added to a column until the column gets full.
        RowFirst        ///< Widgets added to a row until the row gets full.
    };

public:
    GridLayout(Mode mode = ColumnFirst);

    GridLayout(de::Rule const &left, de::Rule const &top, Mode mode = ColumnFirst);

    void clear();

    void setMode(Mode mode);
    void setGridSize(int numCols, int numRows);
    void setModeAndGridSize(Mode mode, int numCols, int numRows);
    void setColumnAlignment(int column, ui::Alignment cellAlign);

    /**
     * Sets the alignment for an individual cell. Overrides column/row alignment.
     *
     * @param cell       Cell position.
     * @param cellAlign  Alignment for the cell.
     */
    void setCellAlignment(de::Vector2i const &cell, ui::Alignment cellAlign);

    void setColumnFixedWidth(int column, de::Rule const &fixedWidth);

    void setLeftTop(de::Rule const &left, de::Rule const &top);
    void setOverrideWidth(de::Rule const &width);
    void setOverrideHeight(de::Rule const &height);
    void setColumnPadding(de::Rule const &gap);
    void setRowPadding(de::Rule const &gap);

    GridLayout &operator << (GuiWidget &widget) { return append(widget); }
    GridLayout &operator << (de::Rule const &empty) { return append(empty); }

    GridLayout &append(GuiWidget &widget, int cellSpan = 1);

    /**
     * Appends a widget to the layout as the next cell. Instead of using the
     * widget's actual widget, @a layoutWidth is treated as the widget's width
     * in the layout. This allows specifying a custom width that may include
     * additional space needed by the widget.
     *
     * @param widget       Widget.
     * @param layoutWidth  Width of the widget, overriding the actual width.
     * @param cellSpan     How many cells does the widget span.
     *
     * @return Reference to this GridLayout.
     */
    GridLayout &append(GuiWidget &widget, de::Rule const &layoutWidth, int cellSpan = 1);

    GridLayout &append(de::Rule const &empty);

    /**
     * Appends an empty cell according to the override width/height,
     * which must be set previously.
     */
    GridLayout &appendEmpty();

    de::WidgetList widgets() const;
    int size() const;
    bool isEmpty() const;

    /**
     * Returns the actual grid size in columns and rows. This depends on how
     * many widgets have been added to the grid.
     */
    de::Vector2i gridSize() const;

    /**
     * Determines the cell coordinates of a particular widget. The widget
     * must be among the widgets previously added to the layout.
     *
     * @param widget  Widget to look up.
     *
     * @return Cell coordinates.
     */
    de::Vector2i widgetPos(GuiWidget &widget) const;

    GuiWidget *at(de::Vector2i const &cell) const;

    de::Rule const &width() const;
    de::Rule const &height() const;
    de::Rule const &columnLeft(int col) const;
    de::Rule const &columnRight(int col) const;
    de::Rule const &columnWidth(int col) const;
    de::Rule const &rowHeight(int row) const;
    de::Rule const &overrideWidth() const;
    de::Rule const &overrideHeight() const;

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_GRIDLAYOUT_H
