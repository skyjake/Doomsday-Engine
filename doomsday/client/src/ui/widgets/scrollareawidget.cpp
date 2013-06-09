/** @file scrollareawidget.cpp
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

#include "ui/widgets/scrollareawidget.h"

using namespace de;

DENG2_PIMPL(ScrollAreaWidget)
{
    RuleRectangle contentRule;
    ScalarRule *x;
    ScalarRule *y;
    Rule *maxX;
    Rule *maxY;

    Instance(Public *i) : Base(i)
    {
        x = new ScalarRule(0);
        y = new ScalarRule(0);
        maxX = new OperatorRule(OperatorRule::Maximum, Const(0), contentRule.width() - self.rule().width());
        maxY = new OperatorRule(OperatorRule::Maximum, Const(0), contentRule.height() - self.rule().height());
    }

    ~Instance()
    {
        releaseRef(x);
        releaseRef(y);
        releaseRef(maxX);
        releaseRef(maxY);
    }
};

ScrollAreaWidget::ScrollAreaWidget(String const &name)
    : GuiWidget(name), d(new Instance(this))
{
    // Link the content rule into the widget's rectangle.
    d->contentRule.setInput(Rule::Left, rule().left() - OperatorRule::minimum(*d->x, *d->maxX))
                  .setInput(Rule::Top,  rule().top()  - OperatorRule::minimum(*d->y, *d->maxY));
}

void ScrollAreaWidget::setContentWidth(int width)
{
    d->contentRule.setInput(Rule::Width, Const(width));
}

void ScrollAreaWidget::setContentHeight(int height)
{
    d->contentRule.setInput(Rule::Height, Const(height));
}

void ScrollAreaWidget::setContentSize(Vector2i const &size)
{
    setContentWidth(size.x);
    setContentHeight(size.y);
}

RuleRectangle const &ScrollAreaWidget::contentRule() const
{
    return d->contentRule;
}

Rule const &ScrollAreaWidget::scrollPositionX() const
{
    return *d->x;
}

Rule const &ScrollAreaWidget::scrollPositionY() const
{
    return *d->y;
}

Rule const &ScrollAreaWidget::maximumScrollX() const
{
    return *d->maxX;
}

Rule const &ScrollAreaWidget::maximumScrollY() const
{
    return *d->maxY;
}

Vector2i ScrollAreaWidget::scrollPosition() const
{
    return Vector2i(scrollPositionX().valuei(), scrollPositionY().valuei());
}

Vector2i ScrollAreaWidget::scrollPageSize() const
{
    return Vector2i(rule().width().valuei(), rule().height().valuei());
}

Vector2i ScrollAreaWidget::maximumScroll() const
{
    return Vector2i(maximumScrollX().valuei(), maximumScrollY().valuei());
}

void ScrollAreaWidget::scroll(Vector2i const &to, TimeDelta span)
{
    scrollX(to.x, span);
    scrollY(to.y, span);
}

void ScrollAreaWidget::scrollX(int to, TimeDelta span)
{
    d->x->set(de::clamp(0, to, maximumScrollX().valuei()), span);
}

void ScrollAreaWidget::scrollY(int to, TimeDelta span)
{
    d->y->set(de::clamp(0, to, maximumScrollY().valuei()), span);
}

bool ScrollAreaWidget::handleEvent(const Event &event)
{
    return false;
}

void ScrollAreaWidget::scrollToTop(TimeDelta span)
{
    scrollY(0, span);
}

void ScrollAreaWidget::scrollToBottom(TimeDelta span)
{
    scrollY(maximumScrollY().valuei(), span);
}

void ScrollAreaWidget::scrollToLeft(TimeDelta span)
{
    scrollX(0, span);
}

void ScrollAreaWidget::scrollToRight(TimeDelta span)
{
    scrollX(maximumScrollX().valuei(), span);
}

