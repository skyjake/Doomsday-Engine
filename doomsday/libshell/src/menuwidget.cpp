/** @file menuwidget.cpp  Menu with shortcuts.
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

#include "de/shell/MenuWidget"
#include "de/shell/TextRootWidget"
#include <de/ConstantRule>
#include <QList>

namespace de {
namespace shell {

struct MenuWidget::Instance
{
    MenuWidget &self;
    ConstantRule *width;
    ConstantRule *height;
    TextCanvas::Char::Attribs borderAttr;
    TextCanvas::Char::Attribs backgroundAttr;
    TextCanvas::Char::Attribs selectionAttr;
    BorderStyle borderStyle;
    Vector2i cursorPos; ///< Visual position.

    struct Item
    {
        Action *action;
        String shortcutLabel;
        bool separatorAfter;

        Item() : action(0), separatorAfter(false)
        {}
    };

    QList<Item> items;
    int cursor;

    Instance(MenuWidget &inst)
        : self(inst),
          borderAttr(TextCanvas::Char::Reverse),
          backgroundAttr(TextCanvas::Char::Reverse),
          borderStyle(LineBorder),
          cursor(0)
    {
        width  = new ConstantRule(1);
        height = new ConstantRule(1);
    }

    ~Instance()
    {
        clear();
        releaseRef(width);
        releaseRef(height);
    }

    void clear()
    {
        foreach(Item i, items)
        {
            self.removeAction(*i.action);
            delete i.action;
        }
        items.clear();
        updateSize();
    }

    void updateSize()
    {
        int cols = 0;
        int lines = 2; // borders
        foreach(Item const &item, items)
        {
            lines++;
            if(item.separatorAfter) lines++;

            int w = item.action->label().size();
            if(!item.shortcutLabel.isEmpty())
            {
                w += 1 + item.shortcutLabel.size();
            }
            cols = de::max(w, cols);
        }
        height->set(lines);
        width->set(6 + cols); // cursor and borders
    }

    void removeItem(int pos)
    {
        self.removeAction(*items[pos].action);
        delete items[pos].action;
        items.removeAt(pos);
        updateSize();
    }
};

MenuWidget::MenuWidget(const String &name)
    : TextWidget(name), d(new Instance(*this))
{
    rule().setInput(RuleRectangle::Width,  *d->width)
          .setInput(RuleRectangle::Height, *d->height);
}

MenuWidget::~MenuWidget()
{
    delete d;
}

int MenuWidget::itemCount() const
{
    return d->items.size();
}

void MenuWidget::appendItem(Action *action, String const &shortcutLabel)
{
    Instance::Item item;
    item.action = action;
    item.shortcutLabel = shortcutLabel;
    d->items.append(item);
    d->updateSize();
    redraw();

    addAction(action);
}

void MenuWidget::appendSeparator()
{
    if(d->items.isEmpty()) return;

    d->items.last().separatorAfter = true;
    d->updateSize();
    redraw();
}

void MenuWidget::insertItem(int pos, Action *action, String const &shortcutLabel)
{
    Instance::Item item;
    item.action = action;
    item.shortcutLabel = shortcutLabel;
    d->items.insert(pos, item);
    d->updateSize();
    redraw();

    addAction(action);
}

void MenuWidget::insertSeparator(int pos)
{
    if(pos < 0 || pos >= d->items.size()) return;

    d->items[pos].separatorAfter = true;
    d->updateSize();
    redraw();
}

void MenuWidget::clear()
{
    d->clear();
    redraw();
}

void MenuWidget::removeItem(int pos)
{
    d->removeItem(pos);
    redraw();
}

Action &MenuWidget::itemAction(int pos) const
{
    return *d->items[pos].action;
}

void MenuWidget::setCursor(int pos)
{
    d->cursor = pos;
    redraw();
}

int MenuWidget::cursor() const
{
    return d->cursor;
}

void MenuWidget::setSelectionAttribs(TextCanvas::Char::Attribs const &attribs)
{
    d->selectionAttr = attribs;
    redraw();
}

void MenuWidget::setBackgroundAttribs(TextCanvas::Char::Attribs const &attribs)
{
    d->backgroundAttr = attribs;
    redraw();
}

void MenuWidget::setBorder(MenuWidget::BorderStyle style)
{
    d->borderStyle = style;
    redraw();
}

void MenuWidget::setBorderAttribs(TextCanvas::Char::Attribs const &attribs)
{
    d->borderAttr = attribs;
    redraw();
}

Vector2i MenuWidget::cursorPosition() const
{
    return d->cursorPos;
}

void MenuWidget::open()
{
    show();
    redraw();
}

void MenuWidget::close()
{
    emit closed();
    hide();
    redraw();
}

void MenuWidget::draw()
{
    Rectanglei pos = rule().recti();
    TextCanvas buf(pos.size());
    buf.clear(TextCanvas::Char(' ', d->backgroundAttr));

    int y = 1;
    for(int i = 0; i < d->items.size(); ++i)
    {
        Instance::Item const &item = d->items[i];

        // Determine style.
        TextCanvas::Char::Attribs itemAttr = (d->cursor == i && hasFocus()?
                                              d->selectionAttr : d->backgroundAttr);

        // Cursor.
        if(d->cursor == i)
        {
            buf.fill(Rectanglei(Vector2i(1, y), Vector2i(pos.width() - 1, y + 1)),
                     TextCanvas::Char(' ', itemAttr));

            d->cursorPos = Vector2i(2, y);
            buf.put(d->cursorPos, TextCanvas::Char('*', itemAttr));
            d->cursorPos += pos.topLeft;
        }

        buf.drawText(Vector2i(4, y), item.action->label(),
                     itemAttr | (d->cursor == i? TextCanvas::Char::Bold : TextCanvas::Char::DefaultAttributes));

        if(item.shortcutLabel)
        {
            buf.drawText(Vector2i(buf.width() - 2 - item.shortcutLabel.size(), y),
                         item.shortcutLabel, itemAttr);
        }

        y++;

        // Draw a separator.
        if(item.separatorAfter)
        {
            buf.fill(Rectanglei(Vector2i(1, y), Vector2i(pos.width() - 1, y + 1)),
                     TextCanvas::Char('-', d->borderAttr));
            y++;
        }
    }

    if(d->borderStyle == LineBorder)
    {
        // Draw a frame.
        buf.drawLineRect(buf.rect(), d->borderAttr);
    }

    targetCanvas().draw(buf, pos.topLeft);
}

bool MenuWidget::handleEvent(Event const *event)
{
    if(!itemCount()) return false;

    if(event->type() != Event::KeyPress) return false;

    // Check registered actions.
    if(TextWidget::handleEvent(event))
        return true;

    KeyEvent const *ev = static_cast<KeyEvent const *>(event);

    if(ev->text() == " ")
    {
        itemAction(d->cursor).trigger();
        close();
        return true;
    }

    if(ev->text().isEmpty())
    {
        switch(ev->key())
        {
        case Qt::Key_Up:
            if(d->cursor == 0) d->cursor = itemCount() - 1;
            else d->cursor--;
            redraw();
            return true;

        case Qt::Key_Down:
            d->cursor = (d->cursor + 1) % itemCount();
            redraw();
            return true;

        case Qt::Key_Home:
        case Qt::Key_PageUp:
            d->cursor = 0;
            redraw();
            return true;

        case Qt::Key_End:
        case Qt::Key_PageDown:
            d->cursor = itemCount() - 1;
            redraw();
            return true;

        case Qt::Key_Enter:
            itemAction(d->cursor).trigger();
            close();
            return true;

        default:
            // Any other control key closes the menu.
            close();
            return true;
        }
    }

    // When open, a menu eats all key events.
    return true;
}

} // namespace shell
} // namespace de
