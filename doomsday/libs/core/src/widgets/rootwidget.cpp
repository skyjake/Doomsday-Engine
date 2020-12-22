/** @file rootwidget.cpp Widget for managing the root of the UI.
 * @ingroup widget
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/rootwidget.h"
#include "de/constantrule.h"
#include "de/rulerectangle.h"
#include "de/math.h"

namespace de {

DE_PIMPL_NOREF(RootWidget)
{
    RuleRectangle *viewRect;
    SafeWidgetPtr<Widget> focus;

    Impl() : focus(0)
    {
        viewRect = new RuleRectangle;
        viewRect->setLeftTop    (Const(0), Const(0))
                 .setRightBottom(Const(0), Const(0));
    }

    ~Impl()
    {
        delete viewRect;
    }

    Size viewSize() const
    {
        return Size(de::max(0, viewRect->width().valuei()),
                    de::max(0, viewRect->height().valuei()));
    }

    DE_PIMPL_AUDIENCE(FocusChange)
};

DE_AUDIENCE_METHOD(RootWidget, FocusChange)

RootWidget::RootWidget() : Widget(), d(new Impl)
{}

RootWidget::Size RootWidget::viewSize() const
{
    return d->viewSize();
}

const RuleRectangle &RootWidget::viewRule() const
{
    return *d->viewRect;
}

const Rule &RootWidget::viewLeft() const
{
    return d->viewRect->left();
}

const Rule &RootWidget::viewRight() const
{
    return d->viewRect->right();
}

const Rule &RootWidget::viewTop() const
{
    return d->viewRect->top();
}

const Rule &RootWidget::viewBottom() const
{
    return d->viewRect->bottom();
}

const Rule &RootWidget::viewWidth() const
{
    return d->viewRect->width();
}

const Rule &RootWidget::viewHeight() const
{
    return d->viewRect->height();
}

void RootWidget::setViewSize(const Size &size)
{
#if defined (DE_MOBILE)
    DE_GUARD(this);
#endif

    d->viewRect->setInput(Rule::Right,  Constu(size.x));
    d->viewRect->setInput(Rule::Bottom, Constu(size.y));

    notifyTree(&Widget::viewResized);
}

void RootWidget::setFocus(const Widget *widget)
{
    if (widget == d->focus) return; // No change.

    Widget *oldFocus = d->focus;

    d->focus.reset();
    if (oldFocus) oldFocus->focusLost();

    if (widget && widget->behavior().testFlag(Focusable))
    {
        d->focus.reset(const_cast<Widget *>(widget));
        if (d->focus)
        {
            //qDebug() << "focus gained by" << d->focus;
            d->focus->focusGained();
        }
    }

    if (d->focus != oldFocus)
    {
        //qDebug() << "focus changed to" << d->focus;
        DE_NOTIFY(FocusChange, i)
        {
            i->focusedWidgetChanged(const_cast<Widget *>(widget));
        }
    }
}

Widget *RootWidget::focus() const
{
    return d->focus;
}

void RootWidget::initialize()
{
#if defined (DE_MOBILE)
    DE_GUARD(this);
#endif
    notifyTree(&Widget::initialize);
}

void RootWidget::update()
{
#if defined (DE_MOBILE)
    DE_GUARD(this);
#endif
    notifyTree(&Widget::update);
}

void RootWidget::draw()
{
#if defined (DE_MOBILE)
    DE_GUARD(this);
#endif
    notifyTree(notifyArgsForDraw());
    Rule::markRulesValid(); // All done for this frame.
}

bool RootWidget::processEvent(const Event &event)
{
#if defined (DE_MOBILE)
    DE_GUARD(this);
#endif

    // Focus is only for the keyboard.
    if (event.isKey() && focus())
    {
        if (focus()->isDisabled())
        {
            // Disabled widgets shouldn't hold the focus.
            setFocus(nullptr);
        }
        else if (focus()->handleEvent(event))
        {
            //qDebug() << "focused widget" << focus() << "ate the event";
            // The focused widget ate the event.
            return true;
        }
    }
    return dispatchEvent(event, &Widget::handleEvent);
}

} // namespace de
