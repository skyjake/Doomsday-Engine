/** @file textwidget.cpp  Generic widget with a text-based visual.
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

#include "de/term/widget.h"
#include "de/term/textrootwidget.h"
#include "de/term/action.h"

namespace de { namespace term {

DE_PIMPL_NOREF(Widget)
{
    TextCanvas *    canvas;
    RuleRectangle * rule;
    List<Action *>  actions;

    Impl() : canvas(nullptr), rule(new RuleRectangle)
    {}

    ~Impl()
    {
        delete rule;
        for (Action *act : actions) releaseRef(act);
    }

    void removeAction(Action &action)
    {
        for (int i = actions.sizei() - 1; i >= 0; --i)
        {
            if (actions.at(i) == &action)
            {
                releaseRef(actions[i]);
                actions.removeAt(i);
            }
        }
    }

    /**
     * Navigates focus to another widget, assuming this widget currently has
     * focus. Used in focus cycle navigation.
     *
     * @param root  Root widget.
     * @param name  Name of the widget to change focus to.
     *
     * @return Focus changed successfully.
     */
    bool navigateFocus(TextRootWidget &root, const String &name)
    {
        if (auto *w = root.find(name))
        {
            root.setFocus(w);
            root.requestDraw();
            return true;
        }
        return false;
    }
};

Widget::Widget(const String &name)
    : de::Widget(name)
    , d(new Impl)
{
    setBehavior(Focusable, SetFlags);
}

TextRootWidget &Widget::root() const
{
    TextRootWidget *r = dynamic_cast<TextRootWidget *>(&de::Widget::root());
    DE_ASSERT(r != nullptr);
    return *r;
}

void Widget::setTargetCanvas(TextCanvas *canvas)
{
    d->canvas = canvas;
}

TextCanvas &Widget::targetCanvas() const
{
    if (!d->canvas)
    {
        // A specific target not defined, use the root canvas.
        return root().rootCanvas();
    }
    return *d->canvas;
}

void Widget::redraw()
{
    if (hasRoot() && !isHidden()) root().requestDraw();
}

void Widget::drawAndShow()
{
    if (!isHidden())
    {
        draw();

        NotifyArgs args(&Widget::draw);
        args.conditionFunc = &Widget::isVisible;
        notifyTree(args);

        targetCanvas().show();
    }
}

RuleRectangle &Widget::rule()
{
    DE_ASSERT(d->rule != nullptr);
    return *d->rule;
}

const RuleRectangle &Widget::rule() const
{
    DE_ASSERT(d->rule != nullptr);
    return *d->rule;
}

Vec2i Widget::cursorPosition() const
{
    return Vec2i(rule().left().valuei(), rule().top().valuei());
}

void Widget::addAction(RefArg<Action> action)
{
    d->actions.append(action.holdRef());
}

void Widget::removeAction(Action &action)
{
    d->removeAction(action);
}

bool Widget::handleEvent(const Event &event)
{
    // We only support KeyEvents.
    if (event.type() == Event::KeyPress)
    {
        const KeyEvent &keyEvent = event.as<KeyEvent>();

        for (Action *act : d->actions)
        {
            // Event will be used by actions.
            if (act->tryTrigger(keyEvent)) return true;
        }

        // Focus navigation.
        if ((keyEvent.key() == Key::Tab || keyEvent.key() == Key::Down) &&
                hasFocus() && !focusNext().isEmpty())
        {
            if (d->navigateFocus(root(), focusNext()))
                return true;
        }
        if ((keyEvent.key() == Key::Backtab || keyEvent.key() == Key::Up) &&
                hasFocus() && !focusPrev().isEmpty())
        {
            if (d->navigateFocus(root(), focusPrev()))
                return true;
        }
    }

    return de::Widget::handleEvent(event);
}

}} // namespace de::term
