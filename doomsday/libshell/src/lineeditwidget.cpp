/** @file lineeditwidget.cpp  Widget for word-wrapped text editing.
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

#include "de/shell/LineEditWidget"
#include "de/shell/TextRootWidget"
#include "de/shell/KeyEvent"
#include "de/shell/Lexicon"
#include <de/RuleRectangle>
#include <de/String>
#include <de/Log>
#include <QSet>

namespace de {
namespace shell {

DENG2_PIMPL(LineEditWidget)
{
    ConstantRule *height;
    bool signalOnEnter;
    String prompt;
    String text;
    int cursor; ///< Index in range [0...text.size()]
    Lexicon lexicon;
    struct Completion
    {
        int pos;
        int size;
        int ordinal; ///< Ordinal within list of possible completions.

        void reset() {
            pos = size = ordinal = 0;
        }
        Range range() const {
            return Range(pos, pos + size);
        }
    };
    Completion completion;
    QList<String> suggestions;

    // Word wrapping.
    LineWrapping wraps;

    Instance(Public &i)
        : Private(i),
          signalOnEnter(true),
          cursor(0)
    {
        // Initial height of the command line (1 row).
        height = new ConstantRule(1);

        wraps.append(WrappedLine(Range(), true));

        completion.reset();
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
        wraps.wrapTextToWidth(text, de::max(1, self.rule().recti().width() - prompt.size() - 1));
        height->set(wraps.height());
    }

    WrappedLine lineSpan(int line) const
    {
        DENG2_ASSERT(line < wraps.size());
        return wraps[line];
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
            WrappedLine span = lineSpan(pos.y);
            if(!span.isFinal) span.range.end--;
            if(cursor >= span.range.start && cursor <= span.range.end)
            {
                // Stop here. Cursor is on this line.
                break;
            }
            pos.x -= span.range.end - span.range.start + 1;
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
        acceptCompletion();

        DENG2_ASSERT(lineOff == 1 || lineOff == -1);

        de::Vector2i const linePos = lineCursorPos();

        // Check for no room.
        if(!linePos.y && lineOff < 0) return false;
        if(linePos.y == wraps.size() - 1 && lineOff > 0) return false;

        // Move cursor onto the adjacent line.
        WrappedLine span = lineSpan(linePos.y + lineOff);
        cursor = span.range.start + linePos.x;
        if(!span.isFinal) span.range.end--;
        if(cursor > span.range.end) cursor = span.range.end;
        return true;
    }

    void insert(String const &str)
    {
        acceptCompletion();

        text.insert(cursor++, str);
    }

    void doBackspace()
    {
        if(suggestingCompletion())
        {
            rejectCompletion();
            return;
        }

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
        acceptCompletion();

        if(cursor > 0) --cursor;
    }

    void doRight()
    {
        acceptCompletion();

        if(cursor < text.size()) ++cursor;
    }

    void doHome()
    {
        acceptCompletion();

        cursor = lineSpan(lineCursorPos().y).range.start;
    }

    void doEnd()
    {
        acceptCompletion();

        WrappedLine const span = lineSpan(lineCursorPos().y);
        cursor = span.range.end - (span.isFinal? 0 : 1);
    }

    void killEndOfLine()
    {
        text.remove(cursor, lineSpan(lineCursorPos().y).range.end - cursor);
    }

    bool suggestingCompletion() const
    {
        return completion.size > 0;
    }

    String wordBehindCursor() const
    {
        String word;
        int i = cursor - 1;
        while(i >= 0 && lexicon.isWordChar(text[i])) word.prepend(text[i--]);
        return word;
    }

    QList<String> completionsForBase(String base) const
    {
        QList<String> suggestions;
        foreach(String term, lexicon.terms())
        {
            if(term.startsWith(base, Qt::CaseInsensitive) && term.size() > base.size())
            {
                suggestions.append(term);
            }
        }
        qSort(suggestions);
        return suggestions;
    }

    bool doCompletion(bool forwardCycle)
    {        
        if(!suggestingCompletion())
        {
            String const base = wordBehindCursor();
            if(!base.isEmpty())
            {
                // Find all the possible completions and apply the first one.
                suggestions = completionsForBase(base);
                if(!suggestions.isEmpty())
                {
                    completion.ordinal = (forwardCycle? 0 : suggestions.size() - 1);
                    String comp = suggestions[completion.ordinal];
                    comp.remove(0, base.size());
                    completion.pos = cursor;
                    completion.size = comp.size();
                    text.insert(cursor, comp);
                    cursor += completion.size;
                    return true;
                }
            }
        }
        else
        {
            // Replace the current completion with another suggestion.
            cursor = completion.pos;
            String const base = wordBehindCursor();

            // Go to next suggestion.
            completion.ordinal = de::wrap(completion.ordinal + (forwardCycle? 1 : -1),
                                          0, suggestions.size());
            String comp = suggestions[completion.ordinal];
            comp.remove(0, base.size());

            text.remove(completion.pos, completion.size);
            text.insert(completion.pos, comp);
            completion.size = comp.size();
            cursor = completion.pos + completion.size;
            return true;
        }
        return false;
    }

    void acceptCompletion()
    {
        completion.reset();
    }

    void rejectCompletion()
    {
        text.remove(completion.pos, completion.size);
        cursor = completion.pos;
        completion.reset();
    }
};

LineEditWidget::LineEditWidget(de::String const &name)
    : TextWidget(name), d(new Instance(*this))
{
    setBehavior(HandleEventsOnlyWhenFocused);

    rule().setInput(Rule::Height, *d->height);
}

LineEditWidget::~LineEditWidget()
{
    delete d;
}

void LineEditWidget::setPrompt(String const &promptText)
{
    d->prompt = promptText;
    d->wraps.clear();

    if(hasRoot())
    {
        d->updateWrapsAndHeight();
        redraw();
    }
}

Vector2i LineEditWidget::cursorPosition() const
{
    de::Rectanglei pos = rule().recti();
    return pos.topLeft + Vector2i(d->prompt.size(), 0) + d->lineCursorPos();
}

void LineEditWidget::viewResized()
{
    d->updateWrapsAndHeight();
}

void LineEditWidget::update()
{
    if(d->wraps.isEmpty())
    {
        d->updateWrapsAndHeight();
    }
}

void LineEditWidget::draw()
{
    Rectanglei pos = rule().recti();

    // Temporary buffer for drawing.
    TextCanvas buf(pos.size());

    TextCanvas::Char::Attribs attr =
            (hasFocus()? TextCanvas::Char::Reverse : TextCanvas::Char::DefaultAttributes);
    buf.clear(TextCanvas::Char(' ', attr));

    buf.drawText(Vector2i(0, 0), d->prompt, attr | TextCanvas::Char::Bold);

    // Underline the suggestion for completion.
    if(d->suggestingCompletion())
    {
        buf.setRichFormatRange(TextCanvas::Char::Underline, d->completion.range());
    }
    buf.drawWrappedText(Vector2i(d->prompt.size(), 0), d->text, d->wraps, attr);

    targetCanvas().draw(buf, pos.topLeft);
}

bool LineEditWidget::handleEvent(Event const *event)
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
        redraw();
        return true;
    }

    return TextWidget::handleEvent(event);
}

bool LineEditWidget::handleControlKey(int key)
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
        d->doHome();
        return true;

    case Qt::Key_End:
        d->doEnd();
        return true;

    case Qt::Key_Tab:
    case Qt::Key_Backtab:
        if(d->doCompletion(key == Qt::Key_Tab))
        {
            return true;
        }
        break;

    case Qt::Key_K: // assuming Control mod
        d->killEndOfLine();
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
        d->acceptCompletion();
        if(d->signalOnEnter)
        {
            emit enterPressed(d->text);
            return true;
        }
        break;

    default:
        break;
    }

    return false;
}

void LineEditWidget::setText(String const &contents)
{
    d->completion.reset();
    d->text = contents;
    d->cursor = contents.size();
    d->wraps.clear();

    if(hasRoot())
    {
        d->updateWrapsAndHeight();
        redraw();
    }
}

String LineEditWidget::text() const
{
    return d->text;
}

void LineEditWidget::setCursor(int index)
{
    d->completion.reset();
    d->cursor = index;
    redraw();
}

int LineEditWidget::cursor() const
{
    return d->cursor;
}

void LineEditWidget::setLexicon(Lexicon const &lexicon)
{
    d->lexicon = lexicon;
}

void LineEditWidget::setSignalOnEnter(int enterSignal)
{
    d->signalOnEnter = enterSignal;
}

} // namespace shell
} // namespace de
