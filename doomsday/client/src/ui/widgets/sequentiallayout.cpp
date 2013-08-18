/** @file sequentiallayout.cpp  Widget layout for a sequence of widgets.
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

#include "ui/widgets/sequentiallayout.h"

using namespace de;

DENG2_PIMPL(SequentialLayout)
{
    WidgetList widgets;
    ui::Direction dir;
    Rule const *posX;
    Rule const *posY;
    Rule const *fixedWidth;
    Rule const *fixedHeight;
    Rule const *totalWidth;
    Rule const *totalHeight;

    Instance(Public *i, Rule const &x, Rule const &y, ui::Direction direc)
        : Base(i),
          dir(direc),
          posX(holdRef(x)),
          posY(holdRef(y)),
          fixedWidth(0),
          fixedHeight(0),
          totalWidth(new ConstantRule(0)),
          totalHeight(new ConstantRule(0))
    {}

    ~Instance()
    {
        releaseRef(posX);
        releaseRef(posY);
        releaseRef(fixedWidth);
        releaseRef(fixedHeight);
        releaseRef(totalWidth);
        releaseRef(totalHeight);
    }

    void advancePos(Rule const &amount)
    {
        switch(dir)
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

    void append(GuiWidget *widget, Rule const *spaceBefore)
    {
        if(spaceBefore)
        {
            advancePos(*spaceBefore);
        }

        if(!widget) return;

        widgets << widget;

        // Override the widget's size as requested.
        if(fixedWidth)  widget->rule().setInput(Rule::Width,  *fixedWidth);
        if(fixedHeight) widget->rule().setInput(Rule::Height, *fixedHeight);

        RuleRectangle &rule = widget->rule();

        // Update the minor axis.
        if(isVertical(dir) || dir == ui::NoDirection)
        {
            rule.setInput(Rule::Left, *posX);
        }
        if(isHorizontal(dir) || dir == ui::NoDirection)
        {
            rule.setInput(Rule::Top, *posY);
        }

        Rule const &w = (fixedWidth?  *fixedWidth  : rule.width());
        Rule const &h = (fixedHeight? *fixedHeight : rule.height());

        // Update the minor axis maximum size.
        if(isHorizontal(dir) && !fixedHeight)
        {
            changeRef(totalHeight, OperatorRule::maximum(*totalHeight, h));
        }
        else if(isVertical(dir) && !fixedWidth)
        {
            changeRef(totalWidth, OperatorRule::maximum(*totalWidth, w));
        }

        // Move along the movement direction for the major axis.
        switch(dir)
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

SequentialLayout::SequentialLayout(Rule const &startX, Rule const &startY, ui::Direction direction)
    : d(new Instance(this, startX, startY, direction))
{}

void SequentialLayout::setDirection(ui::Direction direction)
{
    DENG2_ASSERT(isEmpty());

    d->dir = direction;
}

ui::Direction SequentialLayout::direction() const
{
    return d->dir;
}

void SequentialLayout::setOverrideWidth(Rule const &width)
{
    DENG2_ASSERT(isEmpty());

    changeRef(d->fixedWidth, width);
    changeRef(d->totalWidth, width);
}

void SequentialLayout::setOverrideHeight(Rule const &height)
{
    DENG2_ASSERT(isEmpty());

    changeRef(d->fixedHeight, height);
    changeRef(d->totalHeight, height);
}

SequentialLayout &SequentialLayout::append(GuiWidget &widget)
{
    d->append(&widget, 0);
    return *this;
}

SequentialLayout &SequentialLayout::append(GuiWidget &widget, Rule const &spaceBefore)
{
    d->append(&widget, &spaceBefore);
    return *this;
}

SequentialLayout &SequentialLayout::append(Rule const &emptySpace)
{
    d->append(0, &emptySpace);
    return *this;
}

WidgetList SequentialLayout::widgets() const
{
    return d->widgets;
}

int SequentialLayout::size() const
{
    return d->widgets.size();
}

bool SequentialLayout::isEmpty() const
{
    return d->widgets.isEmpty();
}

Rule const &SequentialLayout::width() const
{
    return *d->totalWidth;
}

Rule const &SequentialLayout::height() const
{
    return *d->totalHeight;
}
