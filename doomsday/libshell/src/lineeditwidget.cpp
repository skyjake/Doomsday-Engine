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
#include <de/RuleRectangle>
#include <de/String>
#include <de/Log>
#include <QSet>

namespace de {
namespace shell {

struct LineEditWidget::Instance
{
    LineEditWidget &self;
    ConstantRule *height;
    bool signalOnEnter;
    String prompt;
    String text;
    int cursor; ///< Index in range [0...text.size()]
    QSet<String> lexicon;
    struct Suggestion
    {
        int pos;
        int size;
        int lexiconIndex;

        void reset() {
            pos = size = lexiconIndex = 0;
        }
        Range range() const {
            return Range(pos, pos + size);
        }
    };
    Suggestion suggestion;

    // Word wrapping.
    LineWrapping wraps;

    Instance(LineEditWidget &cli)
        : self(cli),
          signalOnEnter(true),
          cursor(0)
    {
        // Initial height of the command line (1 row).
        height = new ConstantRule(1);

        wraps.append(WrappedLine(Range(), true));

        suggestion.reset();

        // testing
        lexicon.insert("verbs");
        lexicon.insert("verbose");
        lexicon.insert("version");
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
        acceptSuggestion();

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
        acceptSuggestion();

        text.insert(cursor++, str);
    }

    void doBackspace()
    {
        if(suggestingCompletion())
        {
            rejectSuggestion();
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
        acceptSuggestion();

        if(cursor > 0) --cursor;
    }

    void doRight()
    {
        acceptSuggestion();

        if(cursor < text.size()) ++cursor;
    }

    void doHome()
    {
        acceptSuggestion();

        cursor = lineSpan(lineCursorPos().y).range.start;
    }

    void doEnd()
    {
        acceptSuggestion();

        WrappedLine const span = lineSpan(lineCursorPos().y);
        cursor = span.range.end - (span.isFinal? 0 : 1);
    }

    void killEndOfLine()
    {
        text.remove(cursor, lineSpan(lineCursorPos().y).range.end - cursor);
    }

    bool suggestingCompletion() const
    {
        return suggestion.size > 0;
    }

    String wordBehindCursor() const
    {
        String word;
        int i = cursor - 1;
        while(i >= 0 && !text[i].isSpace()) word.prepend(text[i--]);
        return word;
    }

    QList<String> suggestionsForBase(String base) const
    {
        QList<String> suggestions;
        foreach(String term, lexicon)
        {
            if(term.startsWith(base, Qt::CaseInsensitive))
            {
                suggestions.append(term);
            }
        }
        return suggestions;
    }

    bool doCompletion()
    {        
        if(!suggestingCompletion())
        {
            String const base = wordBehindCursor();
            if(!base.isEmpty())
            {
                QList<String> suggestions = suggestionsForBase(base);
                if(!suggestions.isEmpty())
                {
                    String sug = suggestions.first();
                    sug.remove(0, base.size());
                    suggestion.lexiconIndex = 0;
                    suggestion.pos = cursor;
                    suggestion.size = sug.size();
                    text.insert(cursor, sug);
                    cursor += suggestion.size;
                    return true;
                }
            }
        }
        else
        {
            cursor = suggestion.pos;
            String const base = wordBehindCursor();

            // Go to next suggestion.
            QList<String> suggestions = suggestionsForBase(base);
            suggestion.lexiconIndex = (suggestion.lexiconIndex + 1) % suggestions.size();
            String newSug = suggestions[suggestion.lexiconIndex];
            newSug.remove(0, base.size());

            text.remove(suggestion.pos, suggestion.size);
            text.insert(suggestion.pos, newSug);
            suggestion.size = newSug.size();
            cursor = suggestion.pos + suggestion.size;

            return true;
        }
        return false;
    }

    void acceptSuggestion()
    {
        suggestion.reset();
    }

    void rejectSuggestion()
    {
        text.remove(suggestion.pos, suggestion.size);
        cursor = suggestion.pos;
        suggestion.reset();
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
        buf.setRichFormatRange(TextCanvas::Char::Underline, d->suggestion.range());
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
        if(d->doCompletion())
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
        d->acceptSuggestion();
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
    d->suggestion.reset();
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
    d->suggestion.reset();
    d->cursor = index;
    redraw();
}

int LineEditWidget::cursor() const
{
    return d->cursor;
}

void LineEditWidget::setSignalOnEnter(int enterSignal)
{
    d->signalOnEnter = enterSignal;
}

} // namespace shell
} // namespace de
