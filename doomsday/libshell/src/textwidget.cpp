/** @file textwidget.cpp  Generic widget with a text-based visual.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/shell/TextWidget"
#include "de/shell/TextRootWidget"
#include "de/shell/Action"
#include <QList>

namespace de {
namespace shell {

struct TextWidget::Instance
{
    TextCanvas *canvas;
    RuleRectangle *rule;
    QList<Action *> actions;

    Instance() : canvas(0), rule(new RuleRectangle)
    {}

    ~Instance()
    {
        delete rule;
        foreach(Action *act, actions) delete act;
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
    bool navigateFocus(TextRootWidget &root, String const &name)
    {
        Widget *w = root.find(name);
        if(w)
        {
            root.setFocus(w);
            root.requestDraw();
            return true;
        }
        return false;
    }
};

TextWidget::TextWidget(String const &name) : Widget(name), d(new Instance)
{}

TextWidget::~TextWidget()
{
    delete d;
}

TextRootWidget &TextWidget::root() const
{
    TextRootWidget *r = dynamic_cast<TextRootWidget *>(&Widget::root());
    DENG2_ASSERT(r != 0);
    return *r;
}

void TextWidget::setTargetCanvas(TextCanvas *canvas)
{
    d->canvas = canvas;
}

TextCanvas &TextWidget::targetCanvas() const
{
    if(!d->canvas)
    {
        // A specific target not defined, use the root canvas.
        return root().rootCanvas();
    }
    return *d->canvas;
}

void TextWidget::redraw()
{
    if(hasRoot() && !isHidden()) root().requestDraw();
}

void TextWidget::drawAndShow()
{
    if(!isHidden())
    {
        draw();
        notifyTree(&Widget::drawIfVisible);
        targetCanvas().show();
    }
}

RuleRectangle &TextWidget::rule()
{
    DENG2_ASSERT(d->rule != 0);
    return *d->rule;
}

RuleRectangle const &TextWidget::rule() const
{
    DENG2_ASSERT(d->rule != 0);
    return *d->rule;
}

Vector2i TextWidget::cursorPosition() const
{
    return Vector2i(floor(rule().left().value()),
                    floor(rule().top().value()));
}

void TextWidget::addAction(Action *action)
{
    d->actions.append(action);
}

void TextWidget::removeAction(Action &action)
{
    d->actions.removeAll(&action);
}

bool TextWidget::handleEvent(Event const *event)
{
    // We only support KeyEvents.
    if(event->type() == Event::KeyPress)
    {
        DENG2_ASSERT(dynamic_cast<KeyEvent const *>(event) != 0);

        KeyEvent const *keyEvent = static_cast<KeyEvent const *>(event);

        foreach(Action *act, d->actions)
        {
            // Event will be used by actions.
            if(act->tryTrigger(*keyEvent)) return true;
        }

        // Focus navigation.
        if((keyEvent->key() == Qt::Key_Tab || keyEvent->key() == Qt::Key_Down) &&
                hasFocus() && !focusNext().isEmpty())
        {
            if(d->navigateFocus(root(), focusNext()))
                return true;
        }
        if((keyEvent->key() == Qt::Key_Backtab || keyEvent->key() == Qt::Key_Up) &&
                hasFocus() && !focusPrev().isEmpty())
        {
            if(d->navigateFocus(root(), focusPrev()))
                return true;
        }
    }

    return Widget::handleEvent(event);
}

} // namespace shell
} // namespace de
