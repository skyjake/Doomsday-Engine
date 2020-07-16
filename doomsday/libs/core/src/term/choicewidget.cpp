/** @file libshell/src/choicewidget.cpp  Widget for selecting an item from multiple choices.
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

#include "de/term/choicewidget.h"
#include "de/term/menuwidget.h"
#include "de/term/textrootwidget.h"

namespace de { namespace term {

using AChar = TextCanvas::AttribChar;

DE_PIMPL(ChoiceWidget)
, DE_OBSERVES(MenuWidget, Close)
{
    Items           items;
    int             selection;
    MenuWidget *menu;
    String          prompt;

    Impl(Public &i) : Base(i), selection(0)
    {}

    void updateMenu()
    {
        menu->clear();
        for (const String &item : items)
        {
            menu->appendItem(new Action(item, [this]() { self().updateSelectionFromMenu(); }));
        }
        menu->setCursor(selection);
    }

    void updateLabel()
    {
        self().setLabel(prompt + items[selection], self().attribs());
    }

    void menuClosed() override
    {
        self().root().setFocus(thisPublic);
        self().root().remove(*menu);
        self().redraw();
        self().add(menu);
    }
};

ChoiceWidget::ChoiceWidget(const String &name)
    : LabelWidget(name)
    , d(new Impl(*this))
{
    setBehavior(HandleEventsOnlyWhenFocused);
    setAlignment(AlignLeft);

    d->menu = new MenuWidget(MenuWidget::Popup);
    add(d->menu);

    d->menu->rule()
            .setInput(Rule::Right, rule().right())
            .setInput(Rule::AnchorY, rule().top())
            .setAnchorPoint(Vec2f(0, .5f));

    d->menu->audienceForClose() += d;
}

void ChoiceWidget::setItems(const ChoiceWidget::Items &items)
{
    d->items = items;
    d->updateMenu();
    d->updateLabel();
}

void ChoiceWidget::setPrompt(const String &prompt)
{
    d->prompt = prompt;
    d->updateLabel();
    redraw();
}

ChoiceWidget::Items ChoiceWidget::items() const
{
    return d->items;
}

void ChoiceWidget::select(int pos)
{
    d->selection = pos;
    d->menu->setCursor(pos);
    d->updateLabel();
}

int ChoiceWidget::selection() const
{
    return d->selection;
}

List<int> ChoiceWidget::selections() const
{
    List<int> sels;
    sels.append(d->selection);
    return sels;
}

bool ChoiceWidget::isOpen() const
{
    return !d->menu->isHidden();
}

Vec2i ChoiceWidget::cursorPosition() const
{
    Rectanglei rect = rule().recti();
    return Vec2i(rect.left() + d->prompt.sizei(), rect.top());
}

void ChoiceWidget::focusLost()
{
    setAttribs(AChar::DefaultAttributes);
    setBackgroundAttribs(AChar::DefaultAttributes);
}

void ChoiceWidget::focusGained()
{
    setAttribs(AChar::Reverse);
    setBackgroundAttribs(AChar::Reverse);
}

void ChoiceWidget::draw()
{
    LabelWidget::draw();

    Rectanglei rect = rule().recti();
    targetCanvas().drawText(rect.topLeft, d->prompt, attribs() | AChar::Bold);
    targetCanvas().put(Vec2i(rect.right() - 1, rect.top()), AChar('>', attribs()));
}

bool ChoiceWidget::handleEvent(const Event &ev)
{
    if (ev.type() == Event::KeyPress)
    {
        const KeyEvent &event = ev.as<KeyEvent>();
        if (!event.text().isEmpty() || event.key() == Key::Enter)
        {
            DE_ASSERT(!isOpen());

            if (event.text().isEmpty() || event.text() == " ")
            {
                d->menu->setCursor(d->selection);
            }
            else
            {
                // Preselect the first item that begins with the given letter.
                int curs = d->selection;
                for (int i = 0; i < d->items.sizei(); ++i)
                {
                    if (d->items[i].beginsWith(event.text(), CaseInsensitive))
                    {
                        curs = i;
                        break;
                    }
                }
                d->menu->setCursor(curs);
            }
            remove(*d->menu);
            root().add(d->menu);
            d->menu->open();
            return true;
        }
    }

    return LabelWidget::handleEvent(ev);
}

void ChoiceWidget::updateSelectionFromMenu()
{
    DE_ASSERT(isOpen());
    d->selection = d->menu->cursor();
    d->updateLabel();
}

}} // namespace de::term
