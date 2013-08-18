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
 */
class GridLayout
{
public:
    enum Mode {
        ColumnFirst,    ///< Widgets added to a column until the column gets full.
        RowFirst        ///< Widgets added to a row until the row gets full.
    };

public:
    GridLayout(de::Rule const &startX, de::Rule const &startY,
               Mode mode = ColumnFirst);

    void setGridSize(int numCols, int numRows);

    void setOverrideWidth(de::Rule const &width);
    void setOverrideHeight(de::Rule const &height);
    void setColumnPadding(de::Rule const &gap);
    void setRowPadding(de::Rule const &gap);

    GridLayout &operator << (GuiWidget &widget) { return append(widget); }

    GridLayout &append(GuiWidget &widget);
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

    de::Rule const &width() const;
    de::Rule const &height() const;
    de::Rule const &columnWidth(int col) const;
    de::Rule const &rowHeight(int row) const;
    de::Rule const &overrideWidth() const;
    de::Rule const &overrideHeight() const;

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_GRIDLAYOUT_H
