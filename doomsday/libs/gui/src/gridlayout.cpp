/** @file gridlayout.cpp Widget layout for a grid of widgets.
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

#include "de/GridLayout"
#include "de/SequentialLayout"

#include <de/Map>

namespace de {

DE_PIMPL(GridLayout)
{
    typedef Map<Vec2i, ui::Alignment> CellAlignments;

    GuiWidgetList widgets;
    Mode mode;
    int maxCols;
    int maxRows;
    Rule const *initialX;
    Rule const *initialY;
    Rule const *baseX;
    Rule const *baseY;
    Vec2i cell;
    Rule const *fixedCellWidth;
    Rule const *fixedCellHeight;
    Map<int, Rule const *> fixedColWidths;
    Map<GuiWidget const *, int> widgetMultiCellCount; ///< Only multicell widgets.
    CellAlignments cellAlignment;
    Rule const *colPad;
    Rule const *rowPad;

    struct Metric {
        Rule const *fixedLength; ///< May be @c NULL.
        Rule const *current;    ///< Current size of column/row (replaced many times).
        IndirectRule *final;    ///< Final size of column/row (for others to use).
        Rule const *accumulatedLengths; ///< Sum of sizes of previous columns/rows (for others to use).
        Rule const *minEdge;    ///< Rule for the left/top edge.
        Rule const *maxEdge;    ///< Rule for the right/bottom edge.
        ui::Alignment cellAlign;///< Cell alignment affecting the entire column/row.

        Metric()
            : fixedLength(nullptr)
            , current(nullptr)
            , final(new IndirectRule)
            , accumulatedLengths(nullptr)
            , minEdge(nullptr)
            , maxEdge(nullptr)
            , cellAlign(ui::AlignLeft)
        {}

        ~Metric()
        {
            releaseRef(fixedLength);
            releaseRef(current);
            releaseRef(final);
            releaseRef(accumulatedLengths);
            releaseRef(minEdge);
            releaseRef(maxEdge);
        }
    };
    typedef List<Metric *> Metrics;
    Metrics cols;
    Metrics rows;

    Rule const *totalWidth;
    Rule const *totalHeight;
    SequentialLayout *current;
    bool needTotalUpdate;

    IndirectRule *publicWidth;
    IndirectRule *publicHeight;

    Impl(Public *i, Rule const &x, Rule const &y, Mode layoutMode)
        : Base(i)
        , mode(layoutMode)
        , maxCols(1)
        , maxRows(1)
        , initialX(holdRef(x))
        , initialY(holdRef(y))
        , baseX(holdRef(x))
        , baseY(holdRef(y))
        , fixedCellWidth(0)
        , fixedCellHeight(0)
        , colPad(0)
        , rowPad(0)
        , totalWidth(new ConstantRule(0))
        , totalHeight(new ConstantRule(0))
        , current(0)
        , needTotalUpdate(false)
        , publicWidth(new IndirectRule)
        , publicHeight(new IndirectRule)
    {}

    ~Impl()
    {
        releaseRef(initialX);
        releaseRef(initialY);
        releaseRef(baseX);
        releaseRef(baseY);
        releaseRef(fixedCellWidth);
        releaseRef(fixedCellHeight);
        releaseRef(colPad);
        releaseRef(rowPad);
        releaseRef(totalWidth);
        releaseRef(totalHeight);
        releaseRef(publicWidth);
        releaseRef(publicHeight);
        for (auto &rule : fixedColWidths)
        {
            releaseRef(rule.second);
        }
        fixedColWidths.clear();

        clearMetrics();

        delete current;
    }

    void clear()
    {
        changeRef(baseX, *initialX);
        changeRef(baseY, *initialY);

        delete current;
        current = nullptr;

        publicWidth ->unsetSource();
        publicHeight->unsetSource();
        needTotalUpdate = true;

        widgets.clear();
        widgetMultiCellCount.clear();
        setup(maxCols, maxRows);
    }

    void clearMetrics()
    {
        deleteAll(cols); cols.clear();
        deleteAll(rows); rows.clear();

        cellAlignment.clear();
    }

    void setup(int numCols, int numRows)
    {
        clearMetrics();

        maxCols = numCols;
        maxRows = numRows;

        if (!maxCols)
        {
            mode = RowFirst;
        }
        else if (!maxRows)
        {
            mode = ColumnFirst;
        }

        // Allocate the right number of cols and rows.
        for (int i = 0; i < maxCols; ++i)
        {
            addMetric(cols);
        }
        for (int i = 0; i < maxRows; ++i)
        {
            addMetric(rows);
        }

        cell = Vec2i(0, 0);
    }

    Vec2i gridSize() const
    {
        return Vec2i(cols.size(), rows.size());
    }

    void addMetric(Metrics &list)
    {
        Metric *m = new Metric;
        int pos = list.sizei();

        // Check if there is a fixed width defined for this column.
        if (&list == &cols && fixedColWidths.contains(pos))
        {
            DE_ASSERT(fixedColWidths[pos] != 0);
            m->fixedLength = holdRef(*fixedColWidths[pos]);
        }

        for (int i = 0; i < list.sizei(); ++i)
        {
            sumInto(m->accumulatedLengths, list[i]->fixedLength? *list[i]->fixedLength :
                                                                 *list[i]->final);
        }
        list << m;
    }

    void updateMaximum(Metrics &list, int index, Rule const &rule)
    {
        if (index < 0) index = 0;
        if (index >= list.sizei())
        {
            // The list may expand.
            addMetric(list);
        }
        DE_ASSERT(index < list.sizei());

        DE_ASSERT(list[index] != nullptr);
        if (!list[index]) return;

        Metric &metric = *list[index];
        if (!metric.fixedLength)
        {
            changeRef(metric.current, OperatorRule::maximum(rule, metric.current));

            // Update the indirection.
            metric.final->setSource(*metric.current);
        }
        else
        {
            // Fixed lengths are never affected.
            metric.final->setSource(*metric.fixedLength);
        }
    }

    Rule const &columnLeftX(int col)
    {
        if (!cols.at(col)->minEdge)
        {
            Rule const *base = holdRef(initialX);
            if (col > 0)
            {
                if (colPad) changeRef(base, *base + *colPad * col);
                sumInto(base, *cols.at(col)->accumulatedLengths);
            }
            cols[col]->minEdge = base;
        }
        return *cols.at(col)->minEdge;
    }

    Rule const &columnRightX(int col)
    {
        if (col < cols.sizei() - 1)
        {
            return columnLeftX(col + 1);
        }

        if (!cols.at(col)->maxEdge)
        {
            cols[col]->maxEdge = holdRef(columnLeftX(col) + *cols.last()->final);
        }
        return *cols.at(col)->maxEdge;
    }

    Rule const &rowTopY(int row) const
    {
        if (!rows.at(row)->minEdge)
        {
            Rule const *base = holdRef(initialY);
            if (row > 0)
            {
                if (rowPad) changeRef(base, *base + *rowPad * row);
                sumInto(base, *rows.at(row)->accumulatedLengths);
            }
            rows[row]->minEdge = base;
        }
        return *rows.at(row)->minEdge;
    }

    ui::Alignment alignment(Vec2i pos) const
    {
        CellAlignments::const_iterator found = cellAlignment.find(pos);
        if (found != cellAlignment.end())
        {
            return found->second;
        }
        return cols.at(pos.x)->cellAlign;
    }

    /**
     * Begins the next column or row.
     */
    void begin()
    {
        if (current) return;

        current = new SequentialLayout(*baseX, *baseY, mode == ColumnFirst? ui::Right : ui::Down);

        if (fixedCellWidth)
        {
            current->setOverrideWidth(*fixedCellWidth);
        }
        if (fixedCellHeight)
        {
            current->setOverrideHeight(*fixedCellHeight);
        }
    }

    /**
     * Ends the current column or row, if it is full.
     */
    void end(int cellSpan)
    {
        DE_ASSERT(current != nullptr);

        // Advance to next cell.
        if (mode == ColumnFirst)
        {
            cell.x += cellSpan;

            if (maxCols > 0 && cell.x >= maxCols)
            {
                cell.x = 0;
                cell.y++;
                sumInto(baseY, current->height());
                if (rowPad) sumInto(baseY, *rowPad * cellSpan);

                // This one is finished.
                delete current; current = nullptr;
            }
        }
        else
        {
            cell.y += cellSpan;

            if (maxRows > 0 && cell.y >= maxRows)
            {
                cell.y = 0;
                cell.x++;
                sumInto(baseX, current->width());
                if (colPad) sumInto(baseX, *colPad * cellSpan);

                // This one is finished.
                delete current; current = nullptr;
            }
        }
    }

    /**
     * Appends a widget or empty cell into the grid.
     *
     * @param widget       Widget.
     * @param space        Empty cell.
     * @param cellSpan     Number of cells this widget/space will span.
     * @param layoutWidth  Optionally, a width to use for layout calculations instead
     *                     of the actual width of the widget/space.
     */
    void append(GuiWidget *widget, Rule const *space, int cellSpan = 1, Rule const *layoutWidth = 0)
    {
        DE_ASSERT(!(widget && space));
        DE_ASSERT(widget || space);

        begin();

        Rule const *pad = (mode == ColumnFirst? colPad : rowPad);

        widgets << widget; // NULLs included.

        if (cellSpan > 1)
        {
            widgetMultiCellCount.insert(widget, cellSpan);
        }

        if (widget)
        {
            if (pad && !current->isEmpty())
            {
                current->append(*widget, *pad);
            }
            else
            {
                current->append(*widget);
            }
        }
        else
        {
            if (pad && !current->isEmpty()) current->append(*pad);
            current->append(*space);
        }

        Rule const &cellWidth =
            (layoutWidth ? *layoutWidth : widget ? widget->rule().width() : *space);

        // Update the column and row maximum width/height.
        if (mode == ColumnFirst)
        {
            updateMaximum(cols, cell.x + cellSpan - 1, cellWidth);
            if (widget) updateMaximum(rows, cell.y, widget->rule().height());
        }
        else
        {
            updateMaximum(rows, cell.y + cellSpan - 1, widget ? widget->rule().height() : *space);
            if (widget) updateMaximum(cols, cell.x, cellWidth);
        }

        if (widget)
        {
            // Cells in variable-width columns/rows must be positioned according to
            // the final column/row base widths.
            if (mode == ColumnFirst && !fixedCellWidth)
            {
                if (alignment(cell) & ui::AlignRight)
                {
                    widget->rule()
                            .clearInput(Rule::Left)
                            .setInput(Rule::Right, columnRightX(cell.x + cellSpan - 1));
                }
                else
                {
                    widget->rule().setInput(Rule::Left, columnLeftX(cell.x));
                }
            }
            else if (mode == RowFirst && !fixedCellHeight)
            {
                widget->rule().setInput(Rule::Top, rowTopY(cell.y));
            }
        }

        end(cellSpan);

        needTotalUpdate = true;
    }

    void updateTotal()
    {
        if (!needTotalUpdate) return;

        Vec2i size = gridSize();

        // Paddings must be included in the total.
        if (colPad)
        {
            changeRef(totalWidth, *colPad * size.x);
        }
        else
        {
            releaseRef(totalWidth);
        }
        if (rowPad)
        {
            changeRef(totalHeight, *rowPad * size.y);
        }
        else
        {
            releaseRef(totalHeight);
        }

        // Sum up the column widths.
        for (int i = 0; i < size.x; ++i)
        {
            DE_ASSERT(cols.at(i));
            sumInto(totalWidth, *cols.at(i)->final);
        }

        // Sum up the row heights.
        for (int i = 0; i < size.y; ++i)
        {
            DE_ASSERT(rows.at(i));
            sumInto(totalHeight, *rows.at(i)->final);
        }

        if (!totalWidth)  totalWidth  = new ConstantRule(0);
        if (!totalHeight) totalHeight = new ConstantRule(0);

        publicWidth ->setSource(*totalWidth);
        publicHeight->setSource(*totalHeight);

        needTotalUpdate = false;
    }
};

GridLayout::GridLayout(Mode mode)
    : d(new Impl(this, Const(0), Const(0), mode))
{}

GridLayout::GridLayout(Rule const &startX, Rule const &startY, Mode mode)
    : d(new Impl(this, startX, startY, mode))
{}

void GridLayout::clear()
{
    d->clear();
}

void GridLayout::setMode(GridLayout::Mode mode)
{
    DE_ASSERT(isEmpty());

    d->mode = mode;
    d->setup(d->maxCols, d->maxRows);
}

void GridLayout::setLeftTop(Rule const &left, Rule const &top)
{
    DE_ASSERT(isEmpty());

    changeRef(d->initialX, left);
    changeRef(d->initialY, top);

    changeRef(d->baseX,    left);
    changeRef(d->baseY,    top);
}

void GridLayout::setGridSize(int numCols, int numRows)
{
    DE_ASSERT(numCols >= 0 && numRows >= 0);
    DE_ASSERT(numCols > 0 || numRows > 0);
    DE_ASSERT(isEmpty());

    d->setup(numCols, numRows);
}

void GridLayout::setModeAndGridSize(GridLayout::Mode mode, int numCols, int numRows)
{
    DE_ASSERT(isEmpty());

    d->mode = mode;
    setGridSize(numCols, numRows);
}

void GridLayout::setColumnAlignment(int column, ui::Alignment cellAlign)
{
    DE_ASSERT(column >= 0 && column < d->cols.sizei());
    d->cols[column]->cellAlign = cellAlign;
}

void GridLayout::setColumnFixedWidth(int column, Rule const &fixedWidth)
{
    DE_ASSERT(isEmpty());

    if (d->fixedColWidths.contains(column))
    {
        releaseRef(d->fixedColWidths[column]);
    }

    d->fixedColWidths[column] = holdRef(fixedWidth);

    // Set up the rules again.
    d->setup(d->maxCols, d->maxRows);
}

void GridLayout::setOverrideWidth(Rule const &width)
{
    changeRef(d->fixedCellWidth, width);
}

void GridLayout::setOverrideHeight(Rule const &height)
{
    changeRef(d->fixedCellHeight, height);
}

void GridLayout::setColumnPadding(Rule const &gap)
{
    DE_ASSERT(isEmpty());
    changeRef(d->colPad, gap);
}

void GridLayout::setRowPadding(Rule const &gap)
{
    DE_ASSERT(isEmpty());
    changeRef(d->rowPad, gap);
}

GridLayout &GridLayout::append(GuiWidget &widget, int cellSpan)
{
    d->append(&widget, 0, cellSpan);
    return *this;
}

GridLayout &GridLayout::append(GuiWidget &widget, de::Rule const &layoutWidth, int cellSpan)
{
    d->append(&widget, 0, cellSpan, &layoutWidth);
    return *this;
}

GridLayout &GridLayout::append(Rule const &empty)
{
    d->append(0, &empty);
    return *this;
}

GridLayout &GridLayout::appendEmpty()
{
    if (d->mode == ColumnFirst)
    {
        append(overrideWidth());
    }
    else
    {
        append(overrideHeight());
    }
    return *this;
}

GuiWidgetList GridLayout::widgets() const
{
    return d->widgets;
}

int GridLayout::size() const
{
    return d->widgets.sizei();
}

bool GridLayout::isEmpty() const
{
    return !size();
}

Vec2i GridLayout::maxGridSize() const
{
    return Vec2i(d->maxCols, d->maxRows);
}

Vec2i GridLayout::gridSize() const
{
    return d->gridSize();
}

Vec2i GridLayout::widgetPos(GuiWidget &widget) const
{
    Vec2i pos;
    for (Widget *w : d->widgets)
    {
        if (w == &widget) return pos;

        switch (d->mode)
        {
        case ColumnFirst:
            if (++pos.x >= d->maxCols)
            {
                pos.x = 0;
                ++pos.y;
            }
            break;

        case RowFirst:
            if (++pos.y >= d->maxRows)
            {
                pos.y = 0;
                ++pos.x;
            }
            break;
        }
    }
    return Vec2i(-1, -1);
}

GuiWidget *GridLayout::at(Vec2i const &cell) const
{
    Vec2i pos;
    for (GuiWidget *w : d->widgets)
    {
        if (pos == cell)
        {
            if (w) return w;
            return 0;
        }

        switch (d->mode)
        {
        case ColumnFirst:
            if (++pos.x >= d->maxCols)
            {
                pos.x = 0;
                ++pos.y;
            }
            break;

        case RowFirst:
            if (++pos.y >= d->maxRows)
            {
                pos.y = 0;
                ++pos.x;
            }
            break;
        }
    }
    return nullptr;
}

int GridLayout::widgetCellSpan(GuiWidget const &widget) const
{
    auto found = d->widgetMultiCellCount.find(&widget);
    if (found != d->widgetMultiCellCount.end())
    {
        return found->second;
    }
    return 1;
}

Rule const &GridLayout::width() const
{
    d->updateTotal();
    return *d->publicWidth;
}

Rule const &GridLayout::height() const
{
    d->updateTotal();
    return *d->publicHeight;
}

Rule const &GridLayout::columnLeft(int col) const
{
    DE_ASSERT(col >= 0 && col < d->cols.sizei());
    return d->columnLeftX(col);
}

Rule const &GridLayout::columnRight(int col) const
{
    DE_ASSERT(col >= 0 && col < d->cols.sizei());
    return d->columnRightX(col);
}

Rule const &GridLayout::columnWidth(int col) const
{
    DE_ASSERT(col >= 0 && col < d->cols.sizei());
    return *d->cols.at(col)->final;
}

Rule const &GridLayout::rowHeight(int row) const
{
    DE_ASSERT(row >= 0 && row < d->rows.sizei());
    return *d->rows.at(row)->final;
}

Rule const &GridLayout::overrideWidth() const
{
    DE_ASSERT(d->fixedCellWidth != nullptr);
    return *d->fixedCellWidth;
}

Rule const &GridLayout::overrideHeight() const
{
    DE_ASSERT(d->fixedCellHeight != nullptr);
    return *d->fixedCellHeight;
}

Rule const &GridLayout::columnPadding() const
{
    if (d->colPad) return *d->colPad;
    return ConstantRule::zero();
}

Rule const &GridLayout::rowPadding() const
{
    if (d->rowPad) return *d->rowPad;
    return ConstantRule::zero();
}

void GridLayout::setCellAlignment(Vec2i const &cell, ui::Alignment cellAlign)
{
    d->cellAlignment[cell] = cellAlign;
}

} // namespace de
