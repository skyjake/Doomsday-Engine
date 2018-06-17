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

#include "de/shell/ChoiceTextWidget"
#include "de/shell/MenuTextWidget"
#include "de/shell/TextRootWidget"

namespace de { namespace shell {

using AChar = TextCanvas::AttribChar;

DE_PIMPL(ChoiceTextWidget)
, DE_OBSERVES(MenuTextWidget, Close)
{
    Items           items;
    int             selection;
    MenuTextWidget *menu;
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

ChoiceTextWidget::ChoiceTextWidget(String const &name)
    : LabelTextWidget(name)
    , d(new Impl(*this))
{
    setBehavior(HandleEventsOnlyWhenFocused);
    setAlignment(AlignLeft);

    d->menu = new MenuTextWidget(MenuTextWidget::Popup);
    add(d->menu);

    d->menu->rule()
            .setInput(Rule::Right, rule().right())
            .setInput(Rule::AnchorY, rule().top())
            .setAnchorPoint(Vec2f(0, .5f));

    d->menu->audienceForClose() += d;
}

void ChoiceTextWidget::setItems(ChoiceTextWidget::Items const &items)
{
    d->items = items;
    d->updateMenu();
    d->updateLabel();
}

void ChoiceTextWidget::setPrompt(String const &prompt)
{
    d->prompt = prompt;
    d->updateLabel();
    redraw();
}

ChoiceTextWidget::Items ChoiceTextWidget::items() const
{
    return d->items;
}

void ChoiceTextWidget::select(int pos)
{
    d->selection = pos;
    d->menu->setCursor(pos);
    d->updateLabel();
}

int ChoiceTextWidget::selection() const
{
    return d->selection;
}

List<int> ChoiceTextWidget::selections() const
{
    List<int> sels;
    sels.append(d->selection);
    return sels;
}

bool ChoiceTextWidget::isOpen() const
{
    return !d->menu->isHidden();
}

Vec2i ChoiceTextWidget::cursorPosition() const
{
    Rectanglei rect = rule().recti();
    return Vec2i(rect.left() + d->prompt.sizei(), rect.top());
}

void ChoiceTextWidget::focusLost()
{
    setAttribs(AChar::DefaultAttributes);
    setBackgroundAttribs(AChar::DefaultAttributes);
}

void ChoiceTextWidget::focusGained()
{
    setAttribs(AChar::Reverse);
    setBackgroundAttribs(AChar::Reverse);
}

void ChoiceTextWidget::draw()
{
    LabelTextWidget::draw();

    Rectanglei rect = rule().recti();
    targetCanvas().drawText(rect.topLeft, d->prompt, attribs() | AChar::Bold);
    targetCanvas().put(Vec2i(rect.right() - 1, rect.top()), AChar('>', attribs()));
}

bool ChoiceTextWidget::handleEvent(Event const &ev)
{
    if (ev.type() == Event::KeyPress)
    {
        KeyEvent const &event = ev.as<KeyEvent>();
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

    return LabelTextWidget::handleEvent(ev);
}

void ChoiceTextWidget::updateSelectionFromMenu()
{
    DE_ASSERT(isOpen());
    d->selection = d->menu->cursor();
    d->updateLabel();
}

}} // namespace de::shell
