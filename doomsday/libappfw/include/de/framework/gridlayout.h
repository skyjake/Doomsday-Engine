/** @file gridlayout.h  Widget layout for a grid of widgets.
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

#ifndef LIBAPPFW_GRIDLAYOUT_H
#define LIBAPPFW_GRIDLAYOUT_H

#include "../ui/defs.h"
#include "../GuiWidget"

namespace de {

/**
 * Widget layout for a grid of widgets.
 *
 * Layouts are utilities that modify the placement of widgets. The layout
 * instance itself does not need to remain in memory -- widget rules are
 * modified immediately as the widgets are added to the layout.
 */
class LIBAPPFW_PUBLIC GridLayout
{
public:
    enum Mode {
        ColumnFirst,    ///< Widgets added to a column until the column gets full.
        RowFirst        ///< Widgets added to a row until the row gets full.
    };

public:
    GridLayout(Mode mode = ColumnFirst);

    GridLayout(Rule const &left, Rule const &top, Mode mode = ColumnFirst);

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
    void setCellAlignment(Vector2i const &cell, ui::Alignment cellAlign);

    void setColumnFixedWidth(int column, Rule const &fixedWidth);

    void setLeftTop(Rule const &left, Rule const &top);
    void setOverrideWidth(Rule const &width);
    void setOverrideHeight(Rule const &height);
    void setColumnPadding(Rule const &gap);
    void setRowPadding(Rule const &gap);

    GridLayout &operator << (GuiWidget &widget) { return append(widget); }
    GridLayout &operator << (Rule const &empty) { return append(empty); }

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
    GridLayout &append(GuiWidget &widget, Rule const &layoutWidth, int cellSpan = 1);

    GridLayout &append(Rule const &empty);

    /**
     * Appends an empty cell according to the override width/height,
     * which must be set previously.
     */
    GridLayout &appendEmpty();

    WidgetList widgets() const;
    int size() const;
    bool isEmpty() const;

    /**
     * Returns the maximum grid size. These values were given to setGridSize().
     */
    Vector2i maxGridSize() const;

    /**
     * Returns the actual grid size in columns and rows. This depends on how
     * many widgets have been added to the grid.
     */
    Vector2i gridSize() const;

    /**
     * Determines the cell coordinates of a particular widget. The widget
     * must be among the widgets previously added to the layout.
     *
     * @param widget  Widget to look up.
     *
     * @return Cell coordinates.
     */
    Vector2i widgetPos(GuiWidget &widget) const;

    GuiWidget *at(Vector2i const &cell) const;

    Rule const &width() const;
    Rule const &height() const;
    Rule const &columnLeft(int col) const;
    Rule const &columnRight(int col) const;
    Rule const &columnWidth(int col) const;
    Rule const &rowHeight(int row) const;
    Rule const &overrideWidth() const;
    Rule const &overrideHeight() const;
    Rule const &columnPadding() const;
    Rule const &rowPadding() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_GRIDLAYOUT_H
