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

#include "ui/widgets/gridlayout.h"
#include "ui/widgets/sequentiallayout.h"

using namespace de;

DENG2_PIMPL(GridLayout)
{
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
    Rule const *colPad;
    Rule const *rowPad;

    struct Metric {
        Rule const *current;
        IndirectRule *final;
        Rule const *accum;

        Metric() : current(0), final(new IndirectRule), accum(0) {}

        ~Metric()
        {
            releaseRef(current);
            releaseRef(final);
            releaseRef(accum);
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
        releaseRef(totalWidth);
        releaseRef(totalHeight);

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
        for(int i = 0; i < list.size(); ++i)
        {
            sumInto(m->accum, *list[i]->final);
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
        changeRef(metric.current, OperatorRule::maximum(rule, metric.current));

        // Update the indirection.
        metric.final->setSource(*metric.current);
    }

    Rule const &columnBaseX(int col) const // refless
    {
        Rule const *base = holdRef(initialX);
        if(col > 0 && colPad) changeRef(base, *base + *colPad * col);
        sumInto(base, *cols.at(col)->accum);
        return *refless(base);
    }

    Rule const &rowBaseY(int row) const // refless
    {
        Rule const *base = holdRef(initialY);
        if(row > 0 && rowPad) changeRef(base, *base + *rowPad * row);
        sumInto(base, *rows.at(row)->accum);
        return *refless(base);
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
    void end()
    {
        DENG2_ASSERT(current != 0);

        // Advance to next cell.
        if(mode == ColumnFirst)
        {
            ++cell.x;

            if(maxCols > 0 && current->size() == maxCols)
            {
                cell.x = 0;
                cell.y++;
                sumInto(baseY, current->height());
                if(rowPad) sumInto(baseY, *rowPad);

                // This one is finished.
                delete current; current = 0;
            }
        }
        else
        {
            ++cell.y;

            if(maxRows > 0 && current->size() == maxRows)
            {
                cell.y = 0;
                cell.x++;
                sumInto(baseX, current->width());
                if(colPad) sumInto(baseX, *colPad);

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
    void append(GuiWidget *widget, Rule const *space)
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

        // Update the column and row maximum width/height.
        if(mode == ColumnFirst)
        {
            updateMaximum(cols, cell.x, widget? widget->rule().width() : *space);
            if(widget) updateMaximum(rows, cell.y, widget->rule().height());
        }
        else
        {
            updateMaximum(rows, cell.y, widget? widget->rule().height() : *space);
            if(widget) updateMaximum(cols, cell.x, widget->rule().width());
        }

        // Cells in variable-width columns/rows must be positioned according to
        // the final column/row base widths.
        if(mode == ColumnFirst && !fixedCellWidth)
        {
            widget->rule().setInput(Rule::Left, columnBaseX(cell.x));
        }
        else if(mode == RowFirst && !fixedCellHeight)
        {
            widget->rule().setInput(Rule::Top, rowBaseY(cell.y));
        }

        end();

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
    changeRef(d->colPad, gap);
}

void GridLayout::setRowPadding(Rule const &gap)
{
    changeRef(d->rowPad, gap);
}

GridLayout &GridLayout::append(GuiWidget &widget)
{
    d->append(&widget, 0);
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

Vector2i GridLayout::gridSize() const
{
    return d->gridSize();
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
