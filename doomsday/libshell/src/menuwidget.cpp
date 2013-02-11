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

DENG2_PIMPL(MenuWidget)
{
    ConstantRule *width;
    ConstantRule *height;
    TextCanvas::Char::Attribs borderAttr;
    TextCanvas::Char::Attribs backgroundAttr;
    TextCanvas::Char::Attribs selectionAttr;
    BorderStyle borderStyle;
    Vector2i cursorPos; ///< Visual position.
    bool closable;
    bool cycleCursor;

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

    Instance(Public &i)
        : Private(i),
          borderAttr(TextCanvas::Char::Reverse),
          backgroundAttr(TextCanvas::Char::Reverse),
          borderStyle(LineBorder),
          closable(true),
          cycleCursor(true),
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
        int lines = (borderStyle == NoBorder? 0 : 2);
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
        width->set(4 + cols + (borderStyle == NoBorder? 0 : 2)); // cursor
    }

    void removeItem(int pos)
    {
        self.removeAction(*items[pos].action);
        delete items[pos].action;
        items.removeAt(pos);
        updateSize();
    }
};

MenuWidget::MenuWidget(Preset preset, String const &name)
    : TextWidget(name), d(new Instance(*this))
{
    switch(preset)
    {
    case Popup:
        setBehavior(HandleEventsOnlyWhenFocused, true);
        setClosable(true);
        d->cycleCursor = true;
        hide();
        break;

    case AlwaysOpen:
        setClosable(false);
        d->cycleCursor = false;
        break;
    }

    rule().setSize(*d->width, *d->height);
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

int MenuWidget::findLabel(String const &label) const
{
    for(int i = 0; i < d->items.size(); ++i)
    {
        if(!d->items[i].action->label().compareWithoutCase(label))
            return i;
    }
    return -1;
}

bool MenuWidget::hasLabel(String const &label) const
{
    return findLabel(label) >= 0;
}

void MenuWidget::setCursor(int pos)
{
    d->cursor = de::min(pos, itemCount() - 1);
    redraw();
}

void MenuWidget::setCursorByLabel(String const &label)
{
    int idx = findLabel(label);
    if(idx >= 0)
    {
        setCursor(idx);
    }
    else
    {
        // Try to reselect the previous item by index.
        setCursor(d->cursor);
    }
}

int MenuWidget::cursor() const
{
    return d->cursor;
}

void MenuWidget::setClosable(bool canBeClosed)
{
    d->closable = canBeClosed;
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
    DENG2_ASSERT(hasRoot());

    root().setFocus(this);
    show();
    redraw();
}

void MenuWidget::close()
{
    if(d->closable)
    {
        DENG2_ASSERT(hasRoot());

        root().setFocus(0);
        emit closed();
        hide();
        redraw();
    }
}

void MenuWidget::draw()
{
    Rectanglei pos = rule().recti();
    TextCanvas buf(pos.size());
    buf.clear(TextCanvas::Char(' ', d->backgroundAttr));

    int const border = (d->borderStyle == NoBorder? 0 : 1);
    int y = border;
    for(int i = 0; i < d->items.size(); ++i)
    {
        Instance::Item const &item = d->items[i];

        // Determine style.
        TextCanvas::Char::Attribs itemAttr = (d->cursor == i && hasFocus()?
                                              d->selectionAttr : d->backgroundAttr);

        // Cursor.
        if(d->cursor == i)
        {
            buf.fill(Rectanglei(Vector2i(border, y), Vector2i(pos.width() - border, y + 1)),
                     TextCanvas::Char(' ', itemAttr));

            d->cursorPos = Vector2i(border + 1, y);
            buf.put(d->cursorPos, TextCanvas::Char('*', itemAttr));
            d->cursorPos += pos.topLeft;
        }

        buf.drawText(Vector2i(border + 3, y), item.action->label(),
                     itemAttr | (d->cursor == i? TextCanvas::Char::Bold : TextCanvas::Char::DefaultAttributes));

        if(item.shortcutLabel)
        {
            buf.drawText(Vector2i(buf.width() - 1 - border - item.shortcutLabel.size(), y),
                         item.shortcutLabel, itemAttr);
        }

        y++;

        // Draw a separator.
        if(item.separatorAfter)
        {
            buf.fill(Rectanglei(Vector2i(border, y), Vector2i(pos.width() - border, y + 1)),
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

    KeyEvent const *ev = static_cast<KeyEvent const *>(event);

    // Check menu-related control keys.
    if(ev->text().isEmpty())
    {
        switch(ev->key())
        {
        case Qt::Key_Up:
            if(d->cursor == 0)
            {
                if(!d->cycleCursor) break;
                d->cursor = itemCount() - 1;
            }
            else
            {
                d->cursor--;
            }
            redraw();
            return true;

        case Qt::Key_Down:
            if(d->cursor == itemCount() - 1)
            {
                if(!d->cycleCursor) break;
                d->cursor = 0;
            }
            else
            {
                d->cursor++;
            }
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
            break;
        }
    }

    if(ev->text() == " ")
    {
        itemAction(d->cursor).trigger();
        close();
        return true;
    }

    // Check registered actions (shortcuts), focus navigation.
    if(TextWidget::handleEvent(event))
    {
        close();
        return true;
    }

    if(ev->text().isEmpty())
    {
        if(d->closable)
        {
            // Any other control key closes the menu.
            close();
            return true;
        }
    }
    else
    {
        // Look for an item that begins with the letter.
        for(int i = 0; i < d->items.size(); ++i)
        {
            int idx = (d->cursor + i + 1) % d->items.size();
            if(d->items[idx].action->label().startsWith(ev->text(), Qt::CaseInsensitive))
            {
                setCursor(idx);
                return true;
            }
        }
    }

    return false;
}

} // namespace shell
} // namespace de
