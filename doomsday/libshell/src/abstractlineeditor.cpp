/** @file abstractlineeditor.cpp  Abstract line editor.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/shell/AbstractLineEditor"
#include "de/shell/Lexicon"
#include <de/ConstantRule>

#include <QList>
#include <QScopedPointer>

namespace de {
namespace shell {

DENG2_PIMPL(AbstractLineEditor)
{
    String prompt;
    String text;
    int cursor; ///< Index in range [0...text.size()]
    Lexicon lexicon;
    EchoMode echoMode;
    QScopedPointer<ILineWrapping> wraps;

    struct Completion {
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

    Instance(Public *i, ILineWrapping *lineWraps)
        : Base(i),
          cursor(0),
          echoMode(NormalEchoMode),
          wraps(lineWraps)
    {
        // Initialize line wrapping.
        completion.reset();
    }

    WrappedLine lineSpan(int line) const
    {
        DENG2_ASSERT(line < wraps->height());
        return wraps->line(line);
    }

    void rewrapLater()
    {
        wraps->clear();
        self.contentChanged();
    }

    void rewrapNow()
    {
        updateWraps();
        self.contentChanged();
    }

    /**
     * Determines where word wrapping needs to occur and updates the height of
     * the widget to accommodate all the needed lines.
     */
    void updateWraps()
    {
        wraps->wrapTextToWidth(text, de::max(1, self.maximumWidth()));

        if(wraps->height() > 0)
        {
            self.numberOfLinesChanged(wraps->height());
        }
        else
        {
            self.numberOfLinesChanged(1);
        }
    }

    de::Vector2i lineCursorPos() const
    {
        de::Vector2i pos(cursor);
        for(pos.y = 0; pos.y < wraps->height(); ++pos.y)
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
        int const destWidth = wraps->rangeWidth(Range(lineSpan(linePos.y).range.start, cursor));

        // Check for no room.
        if(!linePos.y && lineOff < 0) return false;
        if(linePos.y == wraps->height() - 1 && lineOff > 0) return false;

        // Move cursor onto the adjacent line.
        WrappedLine span = lineSpan(linePos.y + lineOff);
        cursor = wraps->indexAtWidth(span.range, destWidth);
        if(!span.isFinal) span.range.end--;
        if(cursor > span.range.end) cursor = span.range.end;

        self.cursorMoved();
        return true;
    }

    void insert(String const &str)
    {
        acceptCompletion();
        text.insert(cursor++, str);
        rewrapNow();
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
            rewrapNow();
        }
    }

    void doDelete()
    {
        if(text.size() > cursor)
        {
            text.remove(cursor, 1);
            rewrapNow();
        }
    }

    void doLeft()
    {
        acceptCompletion();

        if(cursor > 0)
        {
            --cursor;
            self.cursorMoved();
        }
    }

    void doRight()
    {
        acceptCompletion();

        if(cursor < text.size())
        {
            ++cursor;
            self.cursorMoved();
        }
    }

    void doHome()
    {
        acceptCompletion();

        cursor = lineSpan(lineCursorPos().y).range.start;
        self.cursorMoved();
    }

    void doEnd()
    {
        acceptCompletion();

        WrappedLine const span = lineSpan(lineCursorPos().y);
        cursor = span.range.end - (span.isFinal? 0 : 1);
        self.cursorMoved();
    }

    void killEndOfLine()
    {
        text.remove(cursor, lineSpan(lineCursorPos().y).range.end - cursor);
        rewrapNow();
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

    QList<String> completionsForBase(String base, String &commonPrefix) const
    {
        bool first = true;
        QList<String> suggestions;
        foreach(String term, lexicon.terms())
        {
            if(term.startsWith(base, Qt::CaseInsensitive) && term.size() > base.size())
            {
                suggestions.append(term);

                // Determine if all the suggestions have a common prefix.
                if(first)
                {
                    commonPrefix = term;
                    first = false;
                }
                else if(!commonPrefix.isEmpty())
                {
                    int len = commonPrefix.commonPrefixLength(term, Qt::CaseInsensitive);
                    commonPrefix = commonPrefix.left(len);
                }
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
                String commonPrefix;
                suggestions = completionsForBase(base, commonPrefix);
                if(!commonPrefix.isEmpty() && commonPrefix != base)
                {
                    completion.ordinal = -1;
                    commonPrefix.remove(0, base.size());
                    completion.pos = cursor;
                    completion.size = commonPrefix.size();
                    text.insert(cursor, commonPrefix);
                    cursor += completion.size;
                    rewrapNow();
                    return true;
                }
                if(!suggestions.isEmpty())
                {
                    completion.ordinal = (forwardCycle? 0 : suggestions.size() - 1);
                    String comp = suggestions[completion.ordinal];
                    comp.remove(0, base.size());
                    completion.pos = cursor;
                    completion.size = comp.size();
                    text.insert(cursor, comp);
                    cursor += completion.size;
                    rewrapNow();
                    return true;
                }
            }
        }
        else
        {
            // Replace the current completion with another suggestion.
            cursor = completion.pos;
            String const base = wordBehindCursor();

            if(completion.ordinal < 0)
            {
                // This occurs after a common prefix is inserted rather than
                // a full suggestion.
                completion.ordinal = (forwardCycle? 0 : suggestions.size() - 1);

                if(base + text.mid(completion.pos, completion.size) == suggestions[completion.ordinal])
                {
                    // We already had this one, skip it.
                    cycleCompletion(forwardCycle);
                }
            }
            else
            {
                cycleCompletion(forwardCycle);
            }

            String comp = suggestions[completion.ordinal];
            comp.remove(0, base.size());

            text.remove(completion.pos, completion.size);
            text.insert(completion.pos, comp);
            completion.size = comp.size();
            cursor = completion.pos + completion.size;
            rewrapNow();
            return true;
        }
        return false;
    }

    void cycleCompletion(bool forwardCycle)
    {
        completion.ordinal = de::wrap(completion.ordinal + (forwardCycle? 1 : -1),
                                      0, suggestions.size());
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
        rewrapNow();
    }
};

AbstractLineEditor::AbstractLineEditor(ILineWrapping *lineWraps) : d(new Instance(this, lineWraps))
{}

ILineWrapping const &AbstractLineEditor::lineWraps() const
{
    return *d->wraps;
}

void AbstractLineEditor::setPrompt(String const &promptText)
{
    d->prompt = promptText;
    d->rewrapLater();
}

String AbstractLineEditor::prompt() const
{
    return d->prompt;
}

void AbstractLineEditor::setText(String const &contents)
{
    d->completion.reset();
    d->text = contents;
    d->cursor = contents.size();
    d->rewrapLater();
}

String AbstractLineEditor::text() const
{
    return d->text;
}

void AbstractLineEditor::setCursor(int index)
{
    d->completion.reset();
    d->cursor = index;
    cursorMoved();
}

int AbstractLineEditor::cursor() const
{
    return d->cursor;
}

Vector2i AbstractLineEditor::lineCursorPos() const
{
    return d->lineCursorPos();
}

bool AbstractLineEditor::isSuggestingCompletion() const
{
    return d->suggestingCompletion();
}

Range AbstractLineEditor::completionRange() const
{
    return d->completion.range();
}

void AbstractLineEditor::setLexicon(Lexicon const &lexicon)
{
    d->lexicon = lexicon;
}

Lexicon const &AbstractLineEditor::lexicon() const
{
    return d->lexicon;
}

void AbstractLineEditor::setEchoMode(EchoMode mode)
{
    d->echoMode = mode;
}

AbstractLineEditor::EchoMode AbstractLineEditor::echoMode() const
{
    return d->echoMode;
}

bool AbstractLineEditor::handleControlKey(int qtKey)
{
    switch(qtKey)
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
        if(d->doCompletion(qtKey == Qt::Key_Tab))
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
        return true;

    default:
        break;
    }

    return false;
}

void AbstractLineEditor::insert(String const &text)
{
    return d->insert(text);
}

ILineWrapping &AbstractLineEditor::lineWraps()
{
    return *d->wraps;
}

void AbstractLineEditor::updateLineWraps(LineWrapUpdateBehavior behavior)
{
    if(behavior == WrapUnlessWrappedAlready && !d->wraps->isEmpty())
        return; // Already wrapped.

    d->updateWraps();
}

} // namespace shell
} // namespace de
