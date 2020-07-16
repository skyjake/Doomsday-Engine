/** @file libshell/src/menuwidget.cpp  Menu with shortcuts.
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

#include "de/term/menuwidget.h"
#include "de/term/textrootwidget.h"
#include "de/constantrule.h"

namespace de { namespace term {

using AChar = TextCanvas::AttribChar;

DE_PIMPL(MenuWidget)
{
    ConstantRule * width;
    ConstantRule * height;
    AChar::Attribs borderAttr;
    AChar::Attribs backgroundAttr;
    AChar::Attribs selectionAttr;
    BorderStyle    borderStyle;
    Vec2i          cursorPos; ///< Visual position.
    bool           closable;
    bool           cycleCursor;

    struct Item
    {
        Action *action;
        String shortcutLabel;
        bool separatorAfter;

        Item() : action(nullptr), separatorAfter(false) {}

        Item(const Item &other)
            : action(holdRef(other.action))
            , shortcutLabel(other.shortcutLabel)
            , separatorAfter(other.separatorAfter) {}

        ~Item() {
            releaseRef(action);
        }

        Item &operator = (const Item &other) {
            changeRef(action, other.action);
            shortcutLabel = other.shortcutLabel;
            separatorAfter = other.separatorAfter;
            return *this;
        }
    };

    List<Item> items;
    int cursor;

    Impl(Public &i)
        : Base(i),
          borderAttr(AChar::Reverse),
          backgroundAttr(AChar::Reverse),
          borderStyle(LineBorder),
          closable(true),
          cycleCursor(true),
          cursor(0)
    {
        width  = new ConstantRule(1);
        height = new ConstantRule(1);
    }

    ~Impl()
    {
        clear();
        releaseRef(width);
        releaseRef(height);
    }

    void clear()
    {
        for (const Item &i : items)
        {
            self().removeAction(*i.action);
        }
        items.clear();
        updateSize();
    }

    void updateSize()
    {
        int cols = 0;
        int lines = (borderStyle == NoBorder? 0 : 2);
        for (const Item &item : items)
        {
            lines++;
            if (item.separatorAfter) lines++;

            int w = item.action->label().sizei();
            if (!item.shortcutLabel.isEmpty())
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
        self().removeAction(*items[pos].action);
        items.removeAt(pos);
        updateSize();
    }

    DE_PIMPL_AUDIENCE(Close)
};

DE_AUDIENCE_METHOD(MenuWidget, Close)

MenuWidget::MenuWidget(Preset preset, const String &name)
    : Widget(name), d(new Impl(*this))
{
    switch (preset)
    {
    case Popup:
        setBehavior(HandleEventsOnlyWhenFocused);
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

int MenuWidget::itemCount() const
{
    return d->items.sizei();
}

void MenuWidget::appendItem(RefArg<Action> action, const String &shortcutLabel)
{
    Impl::Item item;
    item.action = action.holdRef();
    item.shortcutLabel = shortcutLabel;
    d->items.append(item);
    d->updateSize();
    redraw();

    addAction(action);
}

void MenuWidget::appendSeparator()
{
    if (d->items.isEmpty()) return;

    d->items.last().separatorAfter = true;
    d->updateSize();
    redraw();
}

void MenuWidget::insertItem(int pos, RefArg<Action> action, const String &shortcutLabel)
{
    Impl::Item item;
    item.action = action.holdRef();
    item.shortcutLabel = shortcutLabel;
    d->items.insert(pos, item);
    d->updateSize();
    redraw();

    addAction(action);
}

void MenuWidget::insertSeparator(int pos)
{
    if (pos < 0 || pos >= d->items.sizei()) return;

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

int MenuWidget::findLabel(const String &label) const
{
    for (int i = 0; i < d->items.sizei(); ++i)
    {
        if (!d->items[i].action->label().compareWithoutCase(label))
            return i;
    }
    return -1;
}

bool MenuWidget::hasLabel(const String &label) const
{
    return findLabel(label) >= 0;
}

void MenuWidget::setCursor(int pos)
{
    d->cursor = de::min(pos, itemCount() - 1);
    redraw();
}

void MenuWidget::setCursorByLabel(const String &label)
{
    int idx = findLabel(label);
    if (idx >= 0)
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

void MenuWidget::setSelectionAttribs(const AChar::Attribs &attribs)
{
    d->selectionAttr = attribs;
    redraw();
}

void MenuWidget::setBackgroundAttribs(const AChar::Attribs &attribs)
{
    d->backgroundAttr = attribs;
    redraw();
}

void MenuWidget::setBorder(MenuWidget::BorderStyle style)
{
    d->borderStyle = style;
    redraw();
}

void MenuWidget::setBorderAttribs(const AChar::Attribs &attribs)
{
    d->borderAttr = attribs;
    redraw();
}

Vec2i MenuWidget::cursorPosition() const
{
    return d->cursorPos;
}

void MenuWidget::open()
{
    DE_ASSERT(hasRoot());

    root().setFocus(this);
    show();
    redraw();
}

void MenuWidget::close()
{
    if (d->closable)
    {
        DE_ASSERT(hasRoot());

        root().setFocus(nullptr);
        DE_NOTIFY(Close, i) i->menuClosed();
        hide();
        redraw();
    }
}

void MenuWidget::draw()
{
    Rectanglei pos = rule().recti();
    TextCanvas buf(pos.size());
    buf.clear(AChar(' ', d->backgroundAttr));

    const int border = (d->borderStyle == NoBorder? 0 : 1);
    int y = border;
    for (int i = 0; i < d->items.sizei(); ++i)
    {
        const Impl::Item &item = d->items[i];

        // Determine style.
        AChar::Attribs itemAttr =
            (d->cursor == i && hasFocus() ? d->selectionAttr : d->backgroundAttr);

        // Cursor.
        if (d->cursor == i)
        {
            buf.fill(Rectanglei(Vec2i(border, y), Vec2i(pos.width() - border, y + 1)),
                     AChar(' ', itemAttr));

            d->cursorPos = Vec2i(border + 1, y);
            buf.put(d->cursorPos, AChar('*', itemAttr));
            d->cursorPos += pos.topLeft;
        }

        buf.drawText(Vec2i(border + 3, y), item.action->label(),
                     itemAttr | (d->cursor == i? AChar::Bold : AChar::DefaultAttributes));

        if (item.shortcutLabel)
        {
            buf.drawText(Vec2i(buf.width() - 1 - border - item.shortcutLabel.sizei(), y),
                         item.shortcutLabel, itemAttr);
        }

        y++;

        // Draw a separator.
        if (item.separatorAfter)
        {
            buf.fill(Rectanglei(Vec2i(border, y), Vec2i(pos.width() - border, y + 1)),
                     AChar('-', d->borderAttr));
            y++;
        }
    }

    if (d->borderStyle == LineBorder)
    {
        // Draw a frame.
        buf.drawLineRect(buf.rect(), d->borderAttr);
    }

    targetCanvas().draw(buf, pos.topLeft);
}

bool MenuWidget::handleEvent(const Event &event)
{
    if (!itemCount() || event.type() != Event::KeyPress)
    {
        return false;
    }

    const KeyEvent &ev = event.as<KeyEvent>();

    // Check menu-related control keys.
    if (ev.text().isEmpty())
    {
        switch (ev.key())
        {
        case Key::Up:
            if (d->cursor == 0)
            {
                if (!d->cycleCursor) break;
                d->cursor = itemCount() - 1;
            }
            else
            {
                d->cursor--;
            }
            redraw();
            return true;

        case Key::Down:
            if (d->cursor == itemCount() - 1)
            {
                if (!d->cycleCursor) break;
                d->cursor = 0;
            }
            else
            {
                d->cursor++;
            }
            redraw();
            return true;

        case Key::Home:
        case Key::PageUp:
            d->cursor = 0;
            redraw();
            return true;

        case Key::End:
        case Key::PageDown:
            d->cursor = itemCount() - 1;
            redraw();
            return true;

        case Key::Enter:
            itemAction(d->cursor).trigger();
            close();
            return true;

        default:
            break;
        }
    }

    if (ev.text() == " ")
    {
        itemAction(d->cursor).trigger();
        close();
        return true;
    }

    // Check registered actions (shortcuts), focus navigation.
    if (Widget::handleEvent(event))
    {
        close();
        return true;
    }

    if (ev.text().isEmpty())
    {
        if (d->closable)
        {
            // Any other control key closes the menu.
            close();
            return true;
        }
    }
    else
    {
        // Look for an item that begins with the letter.
        for (int i = 0; i < d->items.sizei(); ++i)
        {
            int idx = (d->cursor + i + 1) % d->items.size();
            if (d->items[idx].action->label().beginsWith(ev.text(), CaseInsensitive))
            {
                setCursor(idx);
                return true;
            }
        }
    }

    return false;
}

}} // namespace de::term
