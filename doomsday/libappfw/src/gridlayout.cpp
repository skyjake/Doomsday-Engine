/** @file gridlayout.cpp Widget layout for a grid of widgets.
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

#include "de/GridLayout"
#include "de/SequentialLayout"

#include <QMap>

namespace de {

DENG2_PIMPL(GridLayout)
{
    typedef QMap<Vector2i, ui::Alignment> CellAlignments;

    WidgetList widgets;
    Mode mode;
    int maxCols;
    int maxRows;
    Rule const *initialX;
    Rule const *initialY;
    Rule const *baseX;
    Rule const *baseY;
    Vector2i cell;
    Rule const *fixedCellWidth;
    Rule const *fixedCellHeight;
    QMap<int, Rule const *> fixedColWidths;
    CellAlignments cellAlignment;
    Rule const *colPad;
    Rule const *rowPad;
    Rule const *zeroRule;

    struct Metric {
        Rule const *fixedLength; ///< May be @c NULL.
        Rule const *current;    ///< Current size of column/row (replaced many times).
        IndirectRule *final;    ///< Final size of column/row (for others to use).
        Rule const *accumulatedLengths; ///< Sum of sizes of previous columns/rows (for others to use).
        Rule const *minEdge;    ///< Rule for the left/top edge.
        Rule const *maxEdge;    ///< Rule for the right/bottom edge.
        ui::Alignment cellAlign;///< Cell alignment affecting the entire column/row.

        Metric()
            : fixedLength(0),
              current(0),
              final(new IndirectRule),
              accumulatedLengths(0),
              minEdge(0),
              maxEdge(0),
              cellAlign(ui::AlignLeft) {}

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
    typedef QList<Metric *> Metrics;
    Metrics cols;
    Metrics rows;

    Rule const *totalWidth;
    Rule const *totalHeight;
    SequentialLayout *current;
    bool needTotalUpdate;

    Instance(Public *i, Rule const &x, Rule const &y, Mode layoutMode)
        : Base(i),
          mode(layoutMode),
          maxCols(1),
          maxRows(1),
          initialX(holdRef(x)),
          initialY(holdRef(y)),
          baseX(holdRef(x)),
          baseY(holdRef(y)),
          fixedCellWidth(0),
          fixedCellHeight(0),
          colPad(0),
          rowPad(0),
          zeroRule(new ConstantRule(0)),
          totalWidth(new ConstantRule(0)),
          totalHeight(new ConstantRule(0)),
          current(0),
          needTotalUpdate(false)
    {}

    ~Instance()
    {
        releaseRef(initialX);
        releaseRef(initialY);
        releaseRef(baseX);
        releaseRef(baseY);
        releaseRef(fixedCellWidth);
        releaseRef(fixedCellHeight);
        releaseRef(colPad);
        releaseRef(rowPad);
        releaseRef(zeroRule);
        releaseRef(totalWidth);
        releaseRef(totalHeight);

        foreach(Rule const *rule, fixedColWidths.values())
        {
            releaseRef(rule);
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
        current = 0;

        needTotalUpdate = true;

        widgets.clear();
        setup(maxCols, maxRows);
    }

    void clearMetrics()
    {
        qDeleteAll(cols); cols.clear();
        qDeleteAll(rows); rows.clear();

        cellAlignment.clear();
    }

    void setup(int numCols, int numRows)
    {
        clearMetrics();

        maxCols = numCols;
        maxRows = numRows;

        if(!maxCols)
        {
            mode = RowFirst;
        }
        else if(!maxRows)
        {
            mode = ColumnFirst;
        }

        // Allocate the right number of cols and rows.
        for(int i = 0; i < maxCols; ++i)
        {
            addMetric(cols);
        }
        for(int i = 0; i < maxRows; ++i)
        {
            addMetric(rows);
        }

        cell = Vector2i(0, 0);
    }

    Vector2i gridSize() const
    {
        return Vector2i(cols.size(), rows.size());
    }

    void addMetric(Metrics &list)
    {
        Metric *m = new Metric;
        int pos = list.size();

        // Check if there is a fixed width defined for this column.
        if(&list == &cols && fixedColWidths.contains(pos))
        {
            DENG2_ASSERT(fixedColWidths[pos] != 0);
            m->fixedLength = holdRef(*fixedColWidths[pos]);
        }

        for(int i = 0; i < list.size(); ++i)
        {
            sumInto(m->accumulatedLengths, list[i]->fixedLength? *list[i]->fixedLength :
                                                                 *list[i]->final);
        }
        list << m;
    }

    void updateMaximum(Metrics &list, int index, Rule const &rule)
    {
        if(index < 0) index = 0;
        if(index >= list.size())
        {
            // The list may expand.
            addMetric(list);
        }
        DENG2_ASSERT(index < list.size());

        Metric &metric = *list[index];
        if(!metric.fixedLength)
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
        if(!cols.at(col)->minEdge)
        {
            Rule const *base = holdRef(initialX);
            if(col > 0)
            {
                if(colPad) changeRef(base, *base + *colPad * col);
                sumInto(base, *cols.at(col)->accumulatedLengths);
            }
            cols[col]->minEdge = base;
        }
        return *cols.at(col)->minEdge;
    }

    Rule const &columnRightX(int col)
    {
        if(col < cols.size() - 1)
        {
            return columnLeftX(col + 1);
        }

        if(!cols.at(col)->maxEdge)
        {
            cols[col]->maxEdge = holdRef(columnLeftX(col) + *cols.last()->final);
        }
        return *cols.at(col)->maxEdge;
    }

    Rule const &rowTopY(int row) const
    {
        if(!rows.at(row)->minEdge)
        {
            Rule const *base = holdRef(initialY);
            if(row > 0)
            {
                if(rowPad) changeRef(base, *base + *rowPad * row);
                sumInto(base, *rows.at(row)->accumulatedLengths);
            }
            rows[row]->minEdge = base;
        }
        return *rows.at(row)->minEdge;
    }

    ui::Alignment alignment(Vector2i pos) const
    {
        CellAlignments::const_iterator found = cellAlignment.find(pos);
        if(found != cellAlignment.end())
        {
            return found.value();
        }
        return cols.at(pos.x)->cellAlign;
    }

    /**
     * Begins the next column or row.
     */
    void begin()
    {
        if(current) return;

        current = new SequentialLayout(*baseX, *baseY, mode == ColumnFirst? ui::Right : ui::Down);

        if(fixedCellWidth)
        {
            current->setOverrideWidth(*fixedCellWidth);
        }
        if(fixedCellHeight)
        {
            current->setOverrideHeight(*fixedCellHeight);
        }
    }

    /**
     * Ends the current column or row, if it is full.
     */
    void end(int cellSpan)
    {
        DENG2_ASSERT(current != 0);

        // Advance to next cell.
        if(mode == ColumnFirst)
        {
            cell.x += cellSpan;

            if(maxCols > 0 && cell.x >= maxCols)
            {
                cell.x = 0;
                cell.y++;
                sumInto(baseY, current->height());
                if(rowPad) sumInto(baseY, *rowPad * cellSpan);

                // This one is finished.
                delete current; current = 0;
            }
        }
        else
        {
            cell.y += cellSpan;

            if(maxRows > 0 && cell.y >= maxRows)
            {
                cell.y = 0;
                cell.x++;
                sumInto(baseX, current->width());
                if(colPad) sumInto(baseX, *colPad * cellSpan);

                // This one is finished.
                delete current; current = 0;
            }
        }
    }

    /**
     * Appends a widget or empty cell into the grid.
     *
     * @param widget  Widget.
     * @param space   Empty cell.
     */
    void append(GuiWidget *widget, Rule const *space, int cellSpan = 1, Rule const *layoutWidth = 0)
    {
        DENG2_ASSERT(!(widget && space));
        DENG2_ASSERT(widget || space);

        begin();

        Rule const *pad = (mode == ColumnFirst? colPad : rowPad);

        widgets << widget; // NULLs included.

        if(widget)
        {
            if(pad && !current->isEmpty())
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
            if(pad && !current->isEmpty()) current->append(*pad);
            current->append(*space);
        }

        Rule const &cellWidth = (layoutWidth? *layoutWidth : widget? widget->rule().width() : *space);

        // Update the column and row maximum width/height.
        if(mode == ColumnFirst)
        {
            if(cellSpan == 1) updateMaximum(cols, cell.x, cellWidth);
            if(widget) updateMaximum(rows, cell.y, widget->rule().height());
        }
        else
        {
            if(cellSpan == 1) updateMaximum(rows, cell.y, widget? widget->rule().height() : *space);
            if(widget) updateMaximum(cols, cell.x, cellWidth);
        }

        if(widget)
        {
            // Cells in variable-width columns/rows must be positioned according to
            // the final column/row base widths.
            if(mode == ColumnFirst && !fixedCellWidth)
            {
                if(alignment(cell) & ui::AlignRight)
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
            else if(mode == RowFirst && !fixedCellHeight)
            {
                widget->rule().setInput(Rule::Top, rowTopY(cell.y));
            }
        }

        end(cellSpan);

        needTotalUpdate = true;
    }

    void updateTotal()
    {
        if(!needTotalUpdate) return;

        Vector2i size = gridSize();

        // Paddings must be included in the total.
        if(colPad)
        {
            changeRef(totalWidth, *colPad * size.x);
        }
        else
        {
            releaseRef(totalWidth);
        }
        if(rowPad)
        {
            changeRef(totalHeight, *rowPad * size.y);
        }
        else
        {
            releaseRef(totalHeight);
        }

        // Sum up the column widths.
        for(int i = 0; i < size.x; ++i)
        {
            DENG2_ASSERT(cols.at(i));
            sumInto(totalWidth, *cols.at(i)->final);
        }

        // Sum up the row heights.
        for(int i = 0; i < size.y; ++i)
        {
            DENG2_ASSERT(rows.at(i));
            sumInto(totalHeight, *rows.at(i)->final);
        }

        if(!totalWidth)  totalWidth  = new ConstantRule(0);
        if(!totalHeight) totalHeight = new ConstantRule(0);

        needTotalUpdate = false;
    }
};

GridLayout::GridLayout(Mode mode)
    : d(new Instance(this, Const(0), Const(0), mode))
{}

GridLayout::GridLayout(Rule const &startX, Rule const &startY, Mode mode)
    : d(new Instance(this, startX, startY, mode))
{}

void GridLayout::clear()
{
    d->clear();
}

void GridLayout::setMode(GridLayout::Mode mode)
{
    DENG2_ASSERT(isEmpty());

    d->mode = mode;
    d->setup(d->maxCols, d->maxRows);
}

void GridLayout::setLeftTop(Rule const &left, Rule const &top)
{
    DENG2_ASSERT(isEmpty());

    changeRef(d->initialX, left);
    changeRef(d->initialY, top);

    changeRef(d->baseX,    left);
    changeRef(d->baseY,    top);
}

void GridLayout::setGridSize(int numCols, int numRows)
{
    DENG2_ASSERT(numCols >= 0 && numRows >= 0);
    DENG2_ASSERT(numCols > 0 || numRows > 0);
    DENG2_ASSERT(isEmpty());

    d->setup(numCols, numRows);
}

void GridLayout::setModeAndGridSize(GridLayout::Mode mode, int numCols, int numRows)
{
    DENG2_ASSERT(isEmpty());

    d->mode = mode;
    setGridSize(numCols, numRows);
}

void GridLayout::setColumnAlignment(int column, ui::Alignment cellAlign)
{
    DENG2_ASSERT(column >= 0 && column < d->cols.size());
    d->cols[column]->cellAlign = cellAlign;
}

void GridLayout::setColumnFixedWidth(int column, Rule const &fixedWidth)
{
    DENG2_ASSERT(isEmpty());

    if(d->fixedColWidths.contains(column))
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
    DENG2_ASSERT(isEmpty());
    changeRef(d->colPad, gap);
}

void GridLayout::setRowPadding(Rule const &gap)
{
    DENG2_ASSERT(isEmpty());
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
    if(d->mode == ColumnFirst)
    {
        append(overrideWidth());
    }
    else
    {
        append(overrideHeight());
    }
    return *this;
}

WidgetList GridLayout::widgets() const
{
    return d->widgets;
}

int GridLayout::size() const
{
    return d->widgets.size();
}

bool GridLayout::isEmpty() const
{
    return !size();
}

Vector2i GridLayout::maxGridSize() const
{
    return Vector2i(d->maxCols, d->maxRows);
}

Vector2i GridLayout::gridSize() const
{
    return d->gridSize();
}

Vector2i GridLayout::widgetPos(GuiWidget &widget) const
{
    Vector2i pos;
    foreach(Widget *w, d->widgets)
    {
        if(w == &widget) return pos;

        switch(d->mode)
        {
        case ColumnFirst:
            if(++pos.x >= d->maxCols)
            {
                pos.x = 0;
                ++pos.y;
            }
            break;

        case RowFirst:
            if(++pos.y >= d->maxRows)
            {
                pos.y = 0;
                ++pos.x;
            }
            break;
        }
    }
    return Vector2i(-1, -1);
}

GuiWidget *GridLayout::at(Vector2i const &cell) const
{
    Vector2i pos;
    foreach(Widget *w, d->widgets)
    {
        if(pos == cell)
        {
            if(w) return &w->as<GuiWidget>();
            return 0;
        }

        switch(d->mode)
        {
        case ColumnFirst:
            if(++pos.x >= d->maxCols)
            {
                pos.x = 0;
                ++pos.y;
            }
            break;

        case RowFirst:
            if(++pos.y >= d->maxRows)
            {
                pos.y = 0;
                ++pos.x;
            }
            break;
        }
    }
    return 0;
}

Rule const &GridLayout::width() const
{
    d->updateTotal();
    return *d->totalWidth;
}

Rule const &GridLayout::height() const
{
    d->updateTotal();
    return *d->totalHeight;
}

Rule const &GridLayout::columnLeft(int col) const
{
    DENG2_ASSERT(col >= 0 && col < d->cols.size());
    return d->columnLeftX(col);
}

Rule const &GridLayout::columnRight(int col) const
{
    DENG2_ASSERT(col >= 0 && col < d->cols.size());
    return d->columnRightX(col);
}

Rule const &GridLayout::columnWidth(int col) const
{
    DENG2_ASSERT(col >= 0 && col < d->cols.size());
    return *d->cols.at(col)->final;
}

Rule const &GridLayout::rowHeight(int row) const
{
    DENG2_ASSERT(row >= 0 && row < d->rows.size());
    return *d->rows.at(row)->final;
}

Rule const &GridLayout::overrideWidth() const
{
    DENG2_ASSERT(d->fixedCellWidth != 0);
    return *d->fixedCellWidth;
}

Rule const &GridLayout::overrideHeight() const
{
    DENG2_ASSERT(d->fixedCellHeight != 0);
    return *d->fixedCellHeight;
}

Rule const &GridLayout::columnPadding() const
{
    if(d->colPad) return *d->colPad;
    return *d->zeroRule;
}

Rule const &GridLayout::rowPadding() const
{
    if(d->rowPad) return *d->rowPad;
    return *d->zeroRule;
}

void GridLayout::setCellAlignment(Vector2i const &cell, ui::Alignment cellAlign)
{
    d->cellAlignment[cell] = cellAlign;
}

} // namespace de
