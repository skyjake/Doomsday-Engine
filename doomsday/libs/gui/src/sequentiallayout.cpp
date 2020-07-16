/** @file sequentiallayout.cpp  Widget layout for a sequence of widgets.
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

#include "de/sequentiallayout.h"

namespace de {

DE_PIMPL(SequentialLayout)
{
    GuiWidgetList widgets;
    ui::Direction dir;
    const Rule *initialX;
    const Rule *initialY;
    const Rule *posX;
    const Rule *posY;
    const Rule *fixedWidth;
    const Rule *fixedHeight;
    const Rule *totalWidth;
    const Rule *totalHeight;

    Impl(Public *i, const Rule &x, const Rule &y, ui::Direction direc)
        : Base(i),
          dir(direc),
          initialX(holdRef(x)),
          initialY(holdRef(y)),
          posX(holdRef(x)),
          posY(holdRef(y)),
          fixedWidth(nullptr),
          fixedHeight(nullptr),
          totalWidth(new ConstantRule(0)),
          totalHeight(new ConstantRule(0))
    {}

    ~Impl()
    {
        releaseRef(initialX);
        releaseRef(initialY);
        releaseRef(posX);
        releaseRef(posY);
        releaseRef(fixedWidth);
        releaseRef(fixedHeight);
        releaseRef(totalWidth);
        releaseRef(totalHeight);
    }

    void clear()
    {
        widgets.clear();
        changeRef(posX, *initialX);
        changeRef(posY, *initialY);
        changeRef(totalWidth,  *refless(new ConstantRule(0)));
        changeRef(totalHeight, *refless(new ConstantRule(0)));
    }

    void advancePos(const Rule &amount)
    {
        switch (dir)
        {
        case ui::Right:
            changeRef(posX, *posX + amount);
            changeRef(totalWidth, *totalWidth + amount);
            break;

        case ui::Left:
            changeRef(posX, *posX - amount);
            changeRef(totalWidth, *totalWidth + amount);
            break;

        case ui::Down:
            changeRef(posY, *posY + amount);
            changeRef(totalHeight, *totalHeight + amount);
            break;

        case ui::Up:
            changeRef(posY, *posY - amount);
            changeRef(totalHeight, *totalHeight + amount);
            break;

        default:
            break;
        }
    }

    void append(GuiWidget *widget, const Rule *spaceBefore, AppendMode mode)
    {
        if (spaceBefore)
        {
            advancePos(*spaceBefore);
        }

        if (!widget) return;

        widgets << widget;

        // Override the widget's size as requested.
        if (fixedWidth)  widget->rule().setInput(Rule::Width,  *fixedWidth);
        if (fixedHeight) widget->rule().setInput(Rule::Height, *fixedHeight);

        RuleRectangle &rule = widget->rule();

        // Set position on the minor axis.
        if (isVertical(dir) || dir == ui::NoDirection)
        {
            rule.setInput(Rule::Left, *posX);
        }
        if (isHorizontal(dir) || dir == ui::NoDirection)
        {
            rule.setInput(Rule::Top, *posY);
        }

        const Rule &w = (fixedWidth?  *fixedWidth  : rule.width());
        const Rule &h = (fixedHeight? *fixedHeight : rule.height());

        // Update the minor axis maximum size.
        if (mode == UpdateMinorAxis)
        {
            if (isHorizontal(dir) && !fixedHeight)
            {
                changeRef(totalHeight, OperatorRule::maximum(*totalHeight, h));
            }
            else if (isVertical(dir) && !fixedWidth)
            {
                changeRef(totalWidth, OperatorRule::maximum(*totalWidth, w));
            }
        }

        // Move along the movement direction for the major axis.
        switch (dir)
        {
        case ui::Right:
            rule.setInput(Rule::Left, *posX);
            advancePos(w);
            break;

        case ui::Left:
            rule.setInput(Rule::Right, *posX);
            advancePos(w);
            break;

        case ui::Down:
            rule.setInput(Rule::Top, *posY);
            advancePos(h);
            break;

        case ui::Up:
            rule.setInput(Rule::Bottom, *posY);
            advancePos(h);
            break;

        default:
            break;
        }
    }
};

SequentialLayout::SequentialLayout(const Rule &startX, const Rule &startY, ui::Direction direction)
    : d(new Impl(this, startX, startY, direction))
{}

void SequentialLayout::clear()
{
    d->clear();
}

void SequentialLayout::setStartX(const Rule &startX)
{
    changeRef(d->initialX, startX);
}

void SequentialLayout::setStartY(const Rule &startY)
{
    changeRef(d->initialY, startY);
}

void SequentialLayout::setDirection(ui::Direction direction)
{
    DE_ASSERT(isEmpty());

    d->dir = direction;
}

ui::Direction SequentialLayout::direction() const
{
    return d->dir;
}

void SequentialLayout::setOverrideWidth(const Rule &width)
{
    DE_ASSERT(isEmpty());

    changeRef(d->fixedWidth, width);
    changeRef(d->totalWidth, width);
}

void SequentialLayout::setOverrideHeight(const Rule &height)
{
    DE_ASSERT(isEmpty());

    changeRef(d->fixedHeight, height);
    changeRef(d->totalHeight, height);
}

SequentialLayout &SequentialLayout::append(GuiWidget &widget, AppendMode mode)
{
    d->append(&widget, nullptr, mode);
    return *this;
}

SequentialLayout &SequentialLayout::append(GuiWidget &widget, const Rule &spaceBefore, AppendMode mode)
{
    d->append(&widget, &spaceBefore, mode);
    return *this;
}

SequentialLayout &SequentialLayout::append(const Rule &emptySpace)
{
    d->append(nullptr, &emptySpace, IgnoreMinorAxis);
    return *this;
}

GuiWidgetList SequentialLayout::widgets() const
{
    return d->widgets;
}

int SequentialLayout::size() const
{
    return d->widgets.sizei();
}

bool SequentialLayout::isEmpty() const
{
    return d->widgets.isEmpty();
}

const Rule &SequentialLayout::width() const
{
    return *d->totalWidth;
}

const Rule &SequentialLayout::height() const
{
    return *d->totalHeight;
}

} // namespace de
