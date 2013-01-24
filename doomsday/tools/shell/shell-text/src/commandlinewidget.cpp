/** @file commandlinewidget.cpp  Widget for command line input.
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

#include "commandlinewidget.h"
#include "textrootwidget.h"
#include "keyevent.h"
#include <de/String>

using namespace de;

struct CommandLineWidget::Instance
{
    ConstantRule *height;
    String command;
    int cursorPos;

    Instance() : cursorPos(0)
    {
        // Initial height of the command line (1 row).
        height = new ConstantRule(1);
    }

    ~Instance()
    {
        releaseRef(height);
    }
};

CommandLineWidget::CommandLineWidget(de::String const &name) : TextWidget(name), d(new Instance)
{
    rule().setInput(RectangleRule::Height, d->height);
}

CommandLineWidget::~CommandLineWidget()
{
    delete d;
}

Vector2i CommandLineWidget::cursorPosition()
{
    de::Rectanglei pos = rule().recti();
    return pos.topLeft + Vector2i(2 + d->cursorPos);
}

void CommandLineWidget::draw()
{
    TextCanvas *cv = targetCanvas();
    if(!cv) return;

    de::Rectanglei pos = rule().recti();

    TextCanvas::Char::Attribs attr = TextCanvas::Char::Reverse;
    TextCanvas::Char bg(' ', attr);

    cv->fill(pos, bg);
    cv->put(pos.topLeft, TextCanvas::Char('>', attr | TextCanvas::Char::Bold));
    cv->drawText(pos.topLeft + Vector2i(2, 0), d->command, attr);
}

bool CommandLineWidget::handleEvent(Event const *event)
{
    // There are only key press events.
    DENG2_ASSERT(event->type() == Event::KeyPress);
    KeyEvent const *ev = static_cast<KeyEvent const *>(event);

    bool eaten = true;

    // Insert text?
    if(!ev->text().isEmpty())
    {
        d->command.insert(d->cursorPos++, ev->text());
    }
    else
    {
        // Control character.
        eaten = handleControlKey(ev->key());
    }

    root().requestDraw();
    return eaten;
}

bool CommandLineWidget::handleControlKey(int key)
{
    switch(key)
    {
    case Qt::Key_Backspace:
        if(d->command.size() > 0 && d->cursorPos > 0)
        {
            d->command.remove(--d->cursorPos, 1);
        }
        return true;

    case Qt::Key_Delete:
        if(d->command.size() > d->cursorPos)
        {
            d->command.remove(d->cursorPos, 1);
        }
        return true;

    case Qt::Key_Left:
        if(d->cursorPos > 0) --d->cursorPos;
        return true;

    case Qt::Key_Right:
        if(d->cursorPos < d->command.size()) ++d->cursorPos;
        return true;

    case Qt::Key_Home:
        d->cursorPos = 0;
        return true;

    case Qt::Key_End:
        d->cursorPos = d->command.size();
        return true;

    case Qt::Key_K: // assuming Control mod
        d->command = d->command.left(d->cursorPos);
        return true;

    default:
        break;
    }

    return false;
}


