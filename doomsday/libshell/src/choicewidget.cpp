/** @file choicewidget.cpp  Widget for selecting an item from multiple choices.
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

#include "de/shell/ChoiceWidget"
#include "de/shell/MenuWidget"
#include <de/shell/TextRootWidget>

namespace de {
namespace shell {

struct ChoiceWidget::Instance
{
    ChoiceWidget &self;
    Items items;
    int selection;
    MenuWidget *menu;
    String prompt;

    Instance(ChoiceWidget &inst) : self(inst), selection(0)
    {}

    void updateMenu()
    {
        menu->clear();
        foreach(String item, items)
        {
            menu->appendItem(new Action(item, &self, SLOT(updateSelectionFromMenu())));
        }
        menu->setCursor(selection);
    }

    void updateLabel()
    {
        self.setLabel(prompt + items[selection], self.attribs());
    }
};

ChoiceWidget::ChoiceWidget(const String &name)
    : LabelWidget(name), d(new Instance(*this))
{
    setBehavior(HandleEventsOnlyWhenFocused);
    setAlignment(AlignLeft);

    d->menu = new MenuWidget(MenuWidget::Popup);
    add(d->menu);

    d->menu->rule()
            .setInput(Rule::Right, rule().right())
            .setInput(Rule::AnchorY, rule().top())
            .setAnchorPoint(Vector2f(0, .5f));

    connect(d->menu, SIGNAL(closed()), this, SLOT(menuClosed()));
}

ChoiceWidget::~ChoiceWidget()
{
    delete d;
}

void ChoiceWidget::setItems(ChoiceWidget::Items const &items)
{
    d->items = items;
    d->updateMenu();
    d->updateLabel();
}

void ChoiceWidget::setPrompt(String const &prompt)
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

QList<int> ChoiceWidget::selections() const
{
    QList<int> sels;
    sels.append(d->selection);
    return sels;
}

bool ChoiceWidget::isOpen() const
{
    return !d->menu->isHidden();
}

Vector2i ChoiceWidget::cursorPosition() const
{
    Rectanglei rect = rule().recti();
    return Vector2i(rect.left() + d->prompt.size(), rect.top());
}

void ChoiceWidget::focusLost()
{
    setAttribs(TextCanvas::Char::DefaultAttributes);
    setBackgroundAttribs(TextCanvas::Char::DefaultAttributes);
}

void ChoiceWidget::focusGained()
{
    setAttribs(TextCanvas::Char::Reverse);
    setBackgroundAttribs(TextCanvas::Char::Reverse);
}

void ChoiceWidget::draw()
{
    LabelWidget::draw();

    Rectanglei rect = rule().recti();
    targetCanvas().drawText(rect.topLeft, d->prompt, attribs() | TextCanvas::Char::Bold);
    targetCanvas().put(Vector2i(rect.right() - 1, rect.top()),
                       TextCanvas::Char('>', attribs()));
}

bool ChoiceWidget::handleEvent(Event const *ev)
{
    if(ev->type() == Event::KeyPress)
    {
        KeyEvent const *event = static_cast<KeyEvent const *>(ev);
        if(!event->text().isEmpty() ||
                event->key() == Qt::Key_Enter ||
                event->key() == Qt::Key_Up ||
                event->key() == Qt::Key_Down)
        {
            DENG2_ASSERT(!isOpen());
            if(event->text().isEmpty())
            {
                d->menu->setCursor(d->selection);
            }
            else
            {
                // Preselect the first item that begins with the given letter.
                int curs = d->selection;
                for(int i = 0; i < d->items.size(); ++i)
                {
                    if(d->items[i].toLower().startsWith(event->text().toLower()))
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
    DENG2_ASSERT(isOpen());
    d->selection = d->menu->cursor();
    d->updateLabel();
}

void ChoiceWidget::menuClosed()
{
    root().setFocus(this);
    root().remove(*d->menu);
    redraw();

    add(d->menu);
}

} // namespace shell
} // namespace de
