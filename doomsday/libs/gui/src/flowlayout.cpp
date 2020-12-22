/** @file flowlayout.cpp  Widget layout for a row-based flow of widgets.
 *
 * @authors Copyright (c) 2019 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/flowlayout.h"

namespace de {

DE_PIMPL(FlowLayout)
{
    GuiWidgetList widgets;
    const Rule *  maxLength;
    const Rule *  rightEdge;
    const Rule *  rowHeight;
    const Rule *  initialX;
    const Rule *  initialY;
    const Rule *  posX;
    const Rule *  posY;
    const Rule *  totalHeight;
    IndirectRule *outHeight;

    Impl(Public *i, const Rule &x, const Rule &y, const Rule &maxLength)
        : Base(i)
        , maxLength(holdRef(maxLength))
        , rightEdge(new OperatorRule(OperatorRule::Sum, x, maxLength))
        , rowHeight(nullptr)
        , initialX(holdRef(x))
        , initialY(holdRef(y))
        , posX(holdRef(x))
        , posY(holdRef(y))
        , totalHeight(new ConstantRule(0))
        , outHeight(new IndirectRule)
    {}

    ~Impl()
    {
        releaseRef(outHeight);
        releaseRef(maxLength);
        releaseRef(rightEdge);
        releaseRef(rowHeight);
        releaseRef(initialX);
        releaseRef(initialY);
        releaseRef(posX);
        releaseRef(posY);
        releaseRef(totalHeight);
    }

    void clear()
    {
        widgets.clear();
        changeRef(posX, *initialX);
        changeRef(posY, *initialY);
        releaseRef(rowHeight);
        changeRef(totalHeight, *refless(new ConstantRule(0)));
        outHeight->setSource(*totalHeight);
    }

    void append(GuiWidget *widget, const Rule *spaceBefore)
    {
        if (spaceBefore)
        {
            changeRef(posX, *posX + *spaceBefore);
        }
        if (!widget) return;

        RuleRectangle &rule = widget->rule();
        const Rule &w = rule.width();
        const Rule &h = rule.height();

        if (widgets.isEmpty())
        {
            // The first one is trivial.
            rule.setLeftTop(*initialX, *initialY);
            changeRef(posX, widget->rule().right());
            changeRef(posY, widget->rule().top());
            changeRef(rowHeight, h);
            changeRef(totalHeight, h);
        }
        else
        {
            AutoRef<Rule> isPastEdge = *posX + w - *rightEdge;
            AutoRef<Rule> nextRow    = *posY + *rowHeight;

            rule.setInput(Rule::Left, OperatorRule::select(*posX,
                                                           *initialX,
                                                           isPastEdge));
            rule.setInput(Rule::Top, OperatorRule::select(*posY,
                                                          nextRow,
                                                          isPastEdge));

            changeRef(posX, widget->rule().right());
            changeRef(posY, widget->rule().top());
            changeRef(rowHeight,
                      OperatorRule::select(OperatorRule::maximum(*rowHeight, h),
                                           h,
                                           isPastEdge));

            changeRef(totalHeight, *posY + *rowHeight - *initialY);
        }
        outHeight->setSource(*totalHeight);

        widgets << widget;
    }
};

FlowLayout::FlowLayout(const Rule &startX, const Rule &startY, const Rule &rowLength)
    : d(new Impl(this, startX, startY, rowLength))
{}

void FlowLayout::clear()
{
    d->clear();
}

void FlowLayout::setStartX(const Rule &startX)
{
    changeRef(d->initialX, startX);
}

void FlowLayout::setStartY(const Rule &startY)
{
    changeRef(d->initialY, startY);
}

FlowLayout &FlowLayout::append(GuiWidget &widget)
{
    d->append(&widget, nullptr);
    return *this;
}

FlowLayout &FlowLayout::append(const Rule &emptySpace)
{
    d->append(nullptr, &emptySpace);
    return *this;
}

GuiWidgetList FlowLayout::widgets() const
{
    return d->widgets;
}

int FlowLayout::size() const
{
    return d->widgets.sizei();
}

bool FlowLayout::isEmpty() const
{
    return d->widgets.isEmpty();
}

const Rule &FlowLayout::width() const
{
    return *d->maxLength;
}

const Rule &FlowLayout::height() const
{
    return *d->outHeight;
}

} // namespace de
