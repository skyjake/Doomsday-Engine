/** @file texteditwidget.cpp  Widget for word-wrapped text editing.
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

#include <de/String>
#include <de/RectangleRule>
#include "de/shell/TextEditWidget"
#include "de/shell/TextRootWidget"
#include "de/shell/KeyEvent"

using namespace de;
using namespace de::shell;

struct TextEditWidget::Instance
{
    TextEditWidget &self;
    ConstantRule *height;

    String prompt;
    String text;
    int cursor; ///< Index in range [0...text.size()]

    // Word wrapping.
    struct Span
    {
        int start;
        int end;
        bool isFinal;
    };
    QList<int> wraps;

    Instance(TextEditWidget &cli) : self(cli), cursor(0)
    {
        // Initial height of the command line (1 row).
        height = new ConstantRule(1);

        wraps.append(0);
    }

    ~Instance()
    {
        releaseRef(height);
    }

    void doBackspace()
    {
        if(!text.isEmpty() && cursor > 0)
        {
            text.remove(--cursor, 1);
        }
    }

    void doDelete()
    {
        if(text.size() > cursor)
        {
            text.remove(cursor, 1);
        }
    }

    void doLeft()
    {
        if(cursor > 0) --cursor;
    }

    void doRight()
    {
        if(cursor < text.size()) ++cursor;
    }

    void insert(String const &str)
    {
        text.insert(cursor++, str);
    }

    /**
     * Determines where word wrapping needs to occur and updates the height of
     * the widget to accommodate all the needed lines.
     */
    void updateWrapsAndHeight()
    {
        wraps.clear();

        int const lineWidth = de::max(1, self.rule().recti().width() - prompt.size() - 1);

        int begin = 0;
        forever
        {
            int end = begin + lineWidth;
            if(end >= text.size())
            {
                // Time to stop.
                wraps.append(text.size());
                break;
            }
            // Find a good break point.
            while(!text.at(end).isSpace())
            {
                --end;
                if(end == begin)
                {
                    // Ran out of non-space chars, force a break.
                    end = begin + lineWidth;
                    break;
                }
            }
            if(text.at(end).isSpace()) ++end;
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
     * Calculates the visual position of the cursor (of the current command),
     * including the line that it is on.
     */
    de::Vector2i lineCursorPos() const
    {
        de::Vector2i pos(cursor);
        for(pos.y = 0; pos.y < wraps.size(); ++pos.y)
        {
            Span span = lineSpan(pos.y);
            if(!span.isFinal) span.end--;
            if(cursor >= span.start && cursor <= span.end)
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
        cursor = span.start + linePos.x;
        if(!span.isFinal) span.end--;
        if(cursor > span.end) cursor = span.end;
        return true;
    }
};

TextEditWidget::TextEditWidget(de::String const &name)
    : TextWidget(name), d(new Instance(*this))
{
    rule().setInput(RectangleRule::Height, d->height);
}

TextEditWidget::~TextEditWidget()
{
    delete d;
}

void TextEditWidget::setPrompt(const String &promptText)
{
    d->prompt = promptText;

    if(hasRoot())
    {
        d->updateWrapsAndHeight();
        root().requestDraw();
    }
}

Vector2i TextEditWidget::cursorPosition()
{
    de::Rectanglei pos = rule().recti();
    return pos.topLeft + Vector2i(d->prompt.size(), 0) + d->lineCursorPos();
}

void TextEditWidget::viewResized()
{
    d->updateWrapsAndHeight();
}

void TextEditWidget::draw()
{
    TextCanvas *cv = targetCanvas();
    if(!cv) return;

    Rectanglei pos = rule().recti();

    // Temporary buffer for drawing.
    TextCanvas buf(pos.size());

    TextCanvas::Char::Attribs attr = TextCanvas::Char::Reverse;
    buf.clear(TextCanvas::Char(' ', attr));

    buf.drawText(Vector2i(0, 0), d->prompt, attr | TextCanvas::Char::Bold);

    // Draw all the lines, wrapped as previously determined.
    for(int y = 0; y < d->wraps.size(); ++y)
    {
        Instance::Span span = d->lineSpan(y);
        String part = d->text.substr(span.start, span.end - span.start);
        buf.drawText(Vector2i(d->prompt.size(), y), part, attr);
    }

    buf.blit(*cv, pos.topLeft);
}

bool TextEditWidget::handleEvent(Event const *event)
{
    // There are only key press events.
    DENG2_ASSERT(event->type() == Event::KeyPress);
    KeyEvent const *ev = static_cast<KeyEvent const *>(event);

    bool eaten = true;

    // Insert text?
    if(!ev->text().isEmpty())
    {
        d->insert(ev->text());
    }
    else
    {
        // Control character.
        eaten = handleControlKey(ev->key());
    }

    if(eaten)
    {
        d->updateWrapsAndHeight();
        root().requestDraw();
    }
    return eaten;
}

bool TextEditWidget::handleControlKey(int key)
{
    switch(key)
    {
    case Qt::Key_Backspace:
        d->doBackspace();
        return true;

    case Qt::Key_Delete:
        d->doDelete();
        return true;

    case Qt::Key_Left:
        d->doLeft();
        return true;

    case Qt::Key_Right:
        d->doRight();
        return true;

    case Qt::Key_Home:
        d->cursor = d->lineSpan(d->lineCursorPos().y).start;
        return true;

    case Qt::Key_End: {
        Instance::Span const span = d->lineSpan(d->lineCursorPos().y);
        d->cursor = span.end - (span.isFinal? 0 : 1);
        return true; }

    case Qt::Key_K: // assuming Control mod
        d->text.remove(d->cursor, d->lineSpan(d->lineCursorPos().y).end - d->cursor);
        return true;

    case Qt::Key_Up:
        // First try moving within the current command.
        if(!d->moveCursorByLine(-1)) return false; // not eaten
        return true;

    case Qt::Key_Down:
        // First try moving within the current command.
        if(!d->moveCursorByLine(+1)) return false; // not eaten
        return true;

    case Qt::Key_Enter:
        emit enterPressed(d->text);
        return true;

    default:
        break;
    }

    return false;
}

void TextEditWidget::setText(String const &contents)
{
    d->text = contents;
    d->cursor = contents.size();
    d->updateWrapsAndHeight();
    root().requestDraw();
}

String TextEditWidget::text() const
{
    return d->text;
}

void TextEditWidget::setCursor(int index)
{
    d->cursor = index;
    root().requestDraw();
}

int TextEditWidget::cursor() const
{
    return d->cursor;
}
