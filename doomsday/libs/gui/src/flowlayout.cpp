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

#include "de/FlowLayout"

namespace de {

DE_PIMPL(FlowLayout)
{
    GuiWidgetList widgets;
    const Rule *  maxLength;
    const Rule *  rightEdge;
    const Rule *  rowWidth;
    const Rule *  rowHeight;
    const Rule *  initialX;
    const Rule *  initialY;
    const Rule *  posX;
    const Rule *  posY;
//    const Rule *  totalWidth;
    const Rule *  totalHeight;

    Impl(Public *i, const Rule &x, const Rule &y, const Rule &maxLength)
        : Base(i)
        , maxLength(holdRef(maxLength))
        , rightEdge(new OperatorRule(OperatorRule::Sum, x, maxLength))
        , rowWidth(nullptr)
        , rowHeight(nullptr)
        , initialX(holdRef(x))
        , initialY(holdRef(y))
        , posX(holdRef(x))
        , posY(holdRef(y))
//        , totalWidth(new ConstantRule(0))
        , totalHeight(new ConstantRule(0))
    {}

    ~Impl()
    {
        releaseRef(maxLength);
        releaseRef(rightEdge);
        releaseRef(rowWidth);
        releaseRef(rowHeight);
        releaseRef(initialX);
        releaseRef(initialY);
        releaseRef(posX);
        releaseRef(posY);
//        releaseRef(totalWidth);
        releaseRef(totalHeight);
    }

    void clear()
    {
        widgets.clear();
        changeRef(posX, *initialX);
        changeRef(posY, *initialY);
        releaseRef(rowWidth);
        releaseRef(rowHeight);
//        changeRef(totalWidth,  *refless(new ConstantRule(0)));
        changeRef(totalHeight, *refless(new ConstantRule(0)));
    }

    void advancePos(const Rule &amount)
    {
        changeRef(posX, *posX + amount);
        changeRef(rowWidth, *rowWidth + amount);
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
            changeRef(posX, *initialX + w);
            changeRef(posY, *initialY);
//            changeRef(rowWidth, w);
            changeRef(rowHeight, h);
            changeRef(totalHeight, h);
        }
        else
        {
            AutoRef<Rule> rightX     = *posX + w;
            AutoRef<Rule> isPastEdge = *rightX - *rightEdge;
            AutoRef<Rule> nextRow    = *posY + *rowHeight;

            rule.setInput(Rule::Left, OperatorRule::select(*posX,
                                                           *initialX,
                                                           isPastEdge));
            rule.setInput(Rule::Top, OperatorRule::select(*posY,
                                                          nextRow,
                                                          isPastEdge));

            changeRef(posX, OperatorRule::select(*rightX,
                                                 *initialX + w,
                                                 isPastEdge));
            changeRef(posY, OperatorRule::select(*posY,
                                                 nextRow,
                                                 isPastEdge));
            changeRef(rowHeight,
                      OperatorRule::select(OperatorRule::maximum(*rowHeight, h),
                                           h,
                                           isPastEdge));

            changeRef(totalHeight, *posY + *rowHeight - *initialY);
        }

        widgets << widget;

//            }
//            else if (isVertical(dir) && !fixedWidth)
//            {
//                changeRef(totalWidth, OperatorRule::maximum(*totalWidth, w));
//            }
//        }

        // Move along the movement direction for the major axis.
//        switch (dir)
//        {
//            case ui::Right:

//        rule.setInput(Rule::Left, *posX);
//        advancePos(w);

//                break;

//            case ui::Left:
//                rule.setInput(Rule::Right, *posX);
//                advancePos(w);
//                break;

//            case ui::Down:
//                rule.setInput(Rule::Top, *posY);
//                advancePos(h);
//                break;

//            case ui::Up:
//                rule.setInput(Rule::Bottom, *posY);
//                advancePos(h);
//                break;

//            default:
//                break;
//        }
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
    return *d->totalHeight;
}

} // namespace de
