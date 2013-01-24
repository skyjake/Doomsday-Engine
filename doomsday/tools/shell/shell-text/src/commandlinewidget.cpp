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
#include <de/RectangleRule>
#include <de/String>

using namespace de;

struct CommandLineWidget::Instance
{
    CommandLineWidget &self;
    ConstantRule *height;
    String command;
    int cursorPos;
    QList<int> wraps;

    struct Span
    {
        int start;
        int end;
        bool isFinal;
    };

    Instance(CommandLineWidget &cli) : self(cli), cursorPos(0)
    {
        // Initial height of the command line (1 row).
        height = new ConstantRule(1);

        wraps.append(0);
    }

    ~Instance()
    {
        releaseRef(height);
    }

    /**
     * Determines where word wrapping needs to occur and updates the height of
     * the widget to accommodate all the needed lines.
     */
    void updateWrapsAndHeight()
    {
        wraps.clear();

        int const lineWidth = self.rule().recti().width() - 3;

        int begin = 0;
        forever
        {
            int end = begin + lineWidth;
            if(end >= command.size())
            {
                // Time to stop.
                wraps.append(command.size());
                break;
            }
            // Find a good break point.
            while(!command.at(end).isSpace())
            {
                --end;
                if(end == begin)
                {
                    // Ran out of non-space chars, force a break.
                    end = begin + lineWidth;
                    break;
                }
            }
            if(command.at(end).isSpace()) ++end;
            wraps.append(end);
            begin = end;
        }

        height->set(wraps.size());
    }

    Span lineSpan(int line) const
    {
        DENG2_ASSERT(line < wraps.size());

        Span s;
        s.isFinal = (line == wraps.size() - 1);
        s.end = wraps[line];
        if(!line)
        {
            s.start = 0;
        }
        else
        {
            s.start = wraps[line - 1];
        }
        return s;
    }

    /**
     * Calculates the visual position of the cursor, including the line that it
     * is on.
     */
    de::Vector2i lineCursorPos() const
    {
        de::Vector2i pos(cursorPos);
        for(pos.y = 0; pos.y < wraps.size(); ++pos.y)
        {
            Span span = lineSpan(pos.y);
            if(!span.isFinal) span.end--;
            if(cursorPos >= span.start && cursorPos <= span.end)
            {
                // Stop here. Cursor is on this line.
                break;
            }
            pos.x -= span.end - span.start + 1;
        }
        return pos;
    }

    /**
     * Attemps to move the cursor up or down by a line.
     *
     * @return @c true, if cursor was moved. @c false, if there were no more
     * lines available in that direction.
     */
    bool moveCursorByLine(int lineOff)
    {
        DENG2_ASSERT(lineOff == 1 || lineOff == -1);

        de::Vector2i const linePos = lineCursorPos();

        // Check for no room.
        if(!linePos.y && lineOff < 0) return false;
        if(linePos.y == wraps.size() - 1 && lineOff > 0) return false;

        // Move cursor onto the adjacent line.
        Span span = lineSpan(linePos.y + lineOff);
        cursorPos = span.start + linePos.x;
        if(!span.isFinal) span.end--;
        if(cursorPos > span.end) cursorPos = span.end;
        return true;
    }
};

CommandLineWidget::CommandLineWidget(de::String const &name)
    : TextWidget(name), d(new Instance(*this))
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
    return pos.topLeft + Vector2i(2, 0) + d->lineCursorPos();
}

void CommandLineWidget::viewResized()
{
    d->updateWrapsAndHeight();
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

    // Draw all the lines, wrapped as previously determined.
    for(int y = 0; y < d->wraps.size(); ++y)
    {
        Instance::Span span = d->lineSpan(y);
        String part = d->command.substr(span.start, span.end - span.start);
        cv->drawText(pos.topLeft + Vector2i(2, y), part, attr);
    }
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

    d->updateWrapsAndHeight();

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
        d->cursorPos = d->lineSpan(d->lineCursorPos().y).start;
        return true;

    case Qt::Key_End:
    {
        Instance::Span span = d->lineSpan(d->lineCursorPos().y);
        d->cursorPos = span.end - (span.isFinal? 0 : 1);
        return true;
    }

    case Qt::Key_K: // assuming Control mod
        d->command = d->command.remove(d->cursorPos,
                d->lineSpan(d->lineCursorPos().y).end - d->cursorPos);
        return true;

    case Qt::Key_Up:
        // First try moving within the current command.
        d->moveCursorByLine(-1);
        return true;

    case Qt::Key_Down:
        // First try moving within the current command.
        d->moveCursorByLine(+1);
        return true;

    default:
        break;
    }

    return false;
}
