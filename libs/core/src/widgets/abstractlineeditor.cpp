/** @file abstractlineeditor.cpp  Abstract line editor.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/abstractlineeditor.h"
#include "de/lexicon.h"
#include "de/constantrule.h"

namespace de {

DE_PIMPL(AbstractLineEditor)
{
    String   prompt;
    String   text;
    BytePos  cursor; ///< Index in range [0...text.size()]
    Lexicon  lexicon;
    EchoMode echoMode;

    std::unique_ptr<ILineWrapping> wraps;

    struct Completion {
        BytePos pos;
        dsize size;
        int ordinal; ///< Ordinal within list of possible completions.

        void reset()
        {
            pos     = BytePos(0);
            size    = 0;
            ordinal = 0;
        }
        String::ByteRange range() const { return {pos, pos + size}; }
    };
    Completion completion;
    StringList suggestions;
    bool       suggesting;
    bool       completionNotified;

    Impl(Public *i, ILineWrapping *lineWraps)
        : Base(i)
        , cursor(0)
        , echoMode(NormalEchoMode)
        , wraps(lineWraps)
        , suggesting(false)
        , completionNotified(false)
    {
        // Initialize line wrapping.
        completion.reset();
    }

    mb_iterator iterator(BytePos pos) const { return {text.data() + pos, text.data()}; }

    WrappedLine lineSpan(int line) const
    {
        DE_ASSERT(line < wraps->height());
        return wraps->line(line);
    }

    void rewrapLater()
    {
        wraps->clear();
        self().contentChanged();
    }

    void rewrapNow()
    {
        updateWraps();
        self().contentChanged();
    }

    /**
     * Determines where word wrapping needs to occur and updates the height of
     * the widget to accommodate all the needed lines.
     */
    void updateWraps()
    {
        wraps->wrapTextToWidth(text, max(1, self().maximumWidth()));

        if (wraps->height() > 0)
        {
            self().numberOfLinesChanged(wraps->height());
        }
        else
        {
            self().numberOfLinesChanged(1);
        }
    }

    LineBytePos lineCursorPos() const
    {
        return linePos(cursor);
    }

    LineBytePos linePos(BytePos mark) const
    {
        LineBytePos pos{mark, 0};
        for (pos.line = 0; pos.line < wraps->height(); ++pos.line)
        {
            WrappedLine span = lineSpan(pos.line);
            if (!span.isFinal)
            {
                span.range.setEnd(span.range.end() - 1);
            }
            if (mark >= span.range.begin().pos(text) && mark <= span.range.end().pos(text))
            {
                // Stop here. Mark is on this line.
                break;
            }
            pos.x -= span.range.size();
            pos.x = (iterator(pos.x) - 1).pos(text);
        }
        return pos;
    }

    const char *cursorPtr() const
    {
        return text.data() + cursor;
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

        DE_ASSERT(lineOff == 1 || lineOff == -1);

        const LineBytePos linePos = lineCursorPos();

        // Check for no room.
        if (text.empty()) return false;
        if (linePos.line <= 0 && lineOff < 0) return false;
        if (linePos.line >= wraps->height() - 1 && lineOff > 0) return false;

        const auto destWidth = wraps->rangeWidth({lineSpan(linePos.line).range.begin(), cursorPtr()});

        // Move cursor onto the adjacent line.
        WrappedLine span = lineSpan(linePos.line + lineOff);
        cursor = wraps->indexAtWidth(span.range, destWidth);
        if (!span.isFinal)
        {
            // Step back by one character.
            span.range.setEnd(span.range.end() - 1);
        }
        if (cursorPtr() > span.range.end())
        {
            cursor = span.range.end().pos(text);
        }

        self().cursorMoved();
        return true;
    }

    void insert(const String &str)
    {
        acceptCompletion();
        text.insert(cursor, str);
        cursor += str.sizeb();
        rewrapNow();
    }

    void doBackspace()
    {
        if (rejectCompletion())
            return;

        if (text && cursor > 0)
        {
            const BytePos oldPos = cursor;
            cursor = (iterator(cursor) - 1).pos();
            text.remove(cursor, oldPos - cursor);
            rewrapNow();
        }
    }

    void doWordBackspace()
    {
        rejectCompletion();

        if (text && cursor > 0)
        {
            auto to = wordJumpLeft(cursor);
            text.remove(to, cursor - to);
            cursor = to;
            rewrapNow();
        }
    }

    void doDelete()
    {
        if (text.sizeb() > cursor)
        {
            text.remove(cursor, CharPos(1));
            rewrapNow();
        }
    }

    bool doLeft()
    {
        acceptCompletion();

        if (cursor > 0)
        {
            cursor = (iterator(cursor) - 1).pos();
            self().cursorMoved();
            return true;
        }
        return false;
    }

    bool doRight()
    {
        acceptCompletion();

        if (cursor < text.size())
        {
            cursor = (iterator(cursor) + 1).pos();
            self().cursorMoved();
            return true;
        }
        return false;
    }

    BytePos wordJumpLeft(BytePos pos) const
    {
        auto iter = iterator(de::min(pos, text.sizeb() - 1));

        // First jump over any non-word chars.
        //while (pos > 0 && !text[pos].isLetterOrNumber()) pos--;
        while (iter.pos() > 0 && !(*iter).isAlphaNumeric()) { --iter; }

        // At least move one character.
        if (iter.pos() > 0) --iter;

        // We're inside a word, jump to its beginning.
        while (iter.pos() > 0 && (*(iter - 1)).isAlphaNumeric()) { --iter; }

        return iter.pos();
    }

    void doWordLeft()
    {
        acceptCompletion();
        cursor = wordJumpLeft(cursor);
        self().cursorMoved();
    }

    void doWordRight()
    {
        acceptCompletion();

        const auto last = text.end();
        mb_iterator iter = iterator(cursor);

        // If inside a word, jump to its end.
        while (iter != last && (*iter).isAlphaNumeric())
        {
            iter++;
        }

        // Jump over any non-word chars.
        while (iter != last && !(*iter).isAlphaNumeric())
        {
            iter++;
        }

        cursor = iter.pos();

        self().cursorMoved();
    }

    void doHome()
    {
        acceptCompletion();

        cursor = lineSpan(lineCursorPos().line).range.begin().pos(text);
        self().cursorMoved();
    }

    void doEnd()
    {
        acceptCompletion();

        const WrappedLine span = lineSpan(lineCursorPos().line);
        mb_iterator p = span.range.end();
        if (!span.isFinal)
        {
            --p;
        }
        cursor = p.pos(text);
        self().cursorMoved();
    }

    void killEndOfLine()
    {
        text.remove(cursor,
                    CString(cursorPtr(), lineSpan(lineCursorPos().line).range.end()).size());
        rewrapNow();
    }

    bool suggestingCompletion() const
    {
        return suggesting;
    }

    String wordBehindPos(BytePos pos) const
    {
        String word;
        /// @todo Could alternatively find a range and do a single insertion...
        mb_iterator iter = iterator(pos);
        --iter;
        while (iter.pos() >= 0 && lexicon.isWordChar(*iter))
        {
            word.prepend(*iter--);
        }
        return word;
    }

    String wordBehindCursor() const
    {
        return wordBehindPos(cursor);
    }

    StringList completionsForBase(const String& base, String &commonPrefix) const
    {
        const CaseSensitivity sensitivity =
                lexicon.isCaseSensitive()? CaseSensitive : CaseInsensitive;

        bool first = true;
        StringList sugs;

        for (const String &term : lexicon.terms())
        {
            if (term.beginsWith(base, sensitivity) && term.size() > base.size())
            {
                sugs << term;

                // Determine if all the suggestions have a common prefix.
                if (first)
                {
                    commonPrefix = term;
                    first = false;
                }
                else if (!commonPrefix.isEmpty())
                {
                    auto len = commonPrefix.commonPrefixLength(term, sensitivity);
                    commonPrefix = commonPrefix.left(len);
                }
            }
        }
        sugs.sort();
        return sugs;
    }

    bool doCompletion(bool forwardCycle)
    {
        if (!suggestingCompletion())
        {
            completionNotified = false;
            const String base = wordBehindCursor();
            if (!base.isEmpty())
            {
                // Find all the possible completions and apply the first one.
                String commonPrefix;
                suggestions = completionsForBase(base, commonPrefix);
                if (!commonPrefix.isEmpty() && commonPrefix != base)
                {
                    // Insert the common prefix.
                    completion.ordinal = -1;
                    commonPrefix.remove(BytePos(0), base.sizeb());
                    completion.pos = cursor;
                    completion.size = commonPrefix.size();
                    text.insert(cursor, commonPrefix);
                    cursor += completion.size;
                    rewrapNow();
                    suggesting = true;
                    return true;
                }
                if (!suggestions.isEmpty())
                {
                    completion.ordinal = -1;
                    completion.pos = cursor;
                    completion.size = 0;
                    suggesting = true;
                    // Notify immediately.
                    self().autoCompletionBegan(base);
                    completionNotified = true;
                    return true;
                }
            }
        }
        else
        {
            if (!completionNotified)
            {
                // Time to notify now.
                self().autoCompletionBegan(wordBehindPos(completion.pos));
                completionNotified = true;
                return true;
            }

            // Replace the current completion with another suggestion.
            cursor = completion.pos;
            const String base = wordBehindCursor();

            if (completion.ordinal < 0)
            {
                // This occurs after a common prefix is inserted rather than
                // a full suggestion.
                completion.ordinal = (forwardCycle? 0 : suggestions.sizei() - 1);

                if (base + text.substr(completion.pos, completion.size) == suggestions[completion.ordinal])
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
            comp.remove(BytePos(0), base.sizeb());

            text.remove(completion.pos, completion.size);
            text.insert(completion.pos, comp);
            completion.size = comp.sizei();
            cursor = completion.pos + completion.size;
            rewrapNow();

            return true;
        }
        return false;
    }

    void cycleCompletion(bool forwardCycle)
    {
        completion.ordinal = de::wrap(completion.ordinal + (forwardCycle? 1 : -1),
                                      0, suggestions.sizei());
    }

    void resetCompletion()
    {
        completion.reset();
        suggestions.clear();
        suggesting = false;
        completionNotified = false;
    }

    void acceptCompletion()
    {
        if (!suggestingCompletion()) return;

        resetCompletion();

        self().autoCompletionEnded(true);
    }

    bool rejectCompletion()
    {
        if (!suggestingCompletion()) return false;

        auto oldCursor = cursor;

        text.remove(completion.pos, completion.size);
        cursor = completion.pos;
        resetCompletion();
        rewrapNow();

        self().autoCompletionEnded(false);

        return cursor != oldCursor; // cursor was moved as part of the rejection
    }
};

AbstractLineEditor::AbstractLineEditor(ILineWrapping *lineWraps) : d(new Impl(this, lineWraps))
{}

const ILineWrapping &AbstractLineEditor::lineWraps() const
{
    return *d->wraps;
}

void AbstractLineEditor::setPrompt(const String &promptText)
{
    d->prompt = promptText;
    d->rewrapLater();
}

String AbstractLineEditor::prompt() const
{
    return d->prompt;
}

void AbstractLineEditor::setText(const String &contents)
{
    d->completion.reset();
    d->text = contents;
    d->cursor = contents.sizeb();
    d->rewrapLater();
}

String AbstractLineEditor::text() const
{
    return d->text;
}

void AbstractLineEditor::setCursor(BytePos index)
{
    d->completion.reset();
    d->cursor = index;
    cursorMoved();
}

BytePos AbstractLineEditor::cursor() const
{
    return d->cursor;
}

AbstractLineEditor::LineBytePos AbstractLineEditor::linePos(BytePos index) const
{
    return d->linePos(index);
}

bool AbstractLineEditor::isSuggestingCompletion() const
{
    return d->suggestingCompletion();
}

String::ByteRange AbstractLineEditor::completionRange() const
{
    return d->completion.range();
}

StringList AbstractLineEditor::suggestedCompletions() const
{
    if (!isSuggestingCompletion()) return StringList();

    return d->suggestions;
}

void AbstractLineEditor::acceptCompletion()
{
    d->acceptCompletion();
}

void AbstractLineEditor::setLexicon(const Lexicon &lexicon)
{
    d->lexicon = lexicon;
}

const Lexicon &AbstractLineEditor::lexicon() const
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

bool AbstractLineEditor::handleControlKey(term::Key key, const KeyModifiers &mods)
{
#ifdef MACOSX
#  define WORD_JUMP_MODIFIER    Alt
#else
#  define WORD_JUMP_MODIFIER    Control
#endif

    switch (key)
    {
    case term::Key::Backspace:
        if (mods.testFlag(WORD_JUMP_MODIFIER))
        {
            d->doWordBackspace();
        }
        else
        {
            d->doBackspace();
        }
        return true;

    case term::Key::Delete:
        d->doDelete();
        return true;

    case term::Key::Left:
#ifdef MACOSX
        if (mods.testFlag(Control))
        {
            d->doHome();
            return true;
        }
#endif
        if (mods.testFlag(WORD_JUMP_MODIFIER))
        {
            d->doWordLeft();
        }
        else
        {
            return d->doLeft();
        }
        return true;

    case term::Key::Right:
#ifdef MACOSX
        if (mods.testFlag(Control))
        {
            d->doEnd();
            return true;
        }
#endif
        if (mods.testFlag(WORD_JUMP_MODIFIER))
        {
            d->doWordRight();
        }
        else
        {
            return d->doRight();
        }
        return true;

    case term::Key::Home:
        d->doHome();
        return true;

    case term::Key::End:
        d->doEnd();
        return true;

    case term::Key::Tab:
    case term::Key::Backtab:
        if (d->doCompletion(key == term::Key::Tab))
        {
            return true;
        }
        break;

    case term::Key::Kill:
//        if (mods.testFlag(Control))
//        {
        d->killEndOfLine();
        return true;
//        }
//        break;

    case term::Key::Up:
        // First try moving within the current command.
        if (!d->moveCursorByLine(-1)) return false; // not eaten
        return true;

    case term::Key::Down:
        // First try moving within the current command.
        if (!d->moveCursorByLine(+1)) return false; // not eaten
        return true;

    case term::Key::Enter:
//    case Key::Return:
        d->acceptCompletion();
        return true;

    default:
        break;
    }

    return false;
}

void AbstractLineEditor::insert(const String &text)
{
    return d->insert(text);
}

ILineWrapping &AbstractLineEditor::lineWraps()
{
    return *d->wraps;
}

void AbstractLineEditor::autoCompletionBegan(const String &)
{}

void AbstractLineEditor::autoCompletionEnded(bool /*accepted*/)
{}

void AbstractLineEditor::updateLineWraps(LineWrapUpdateBehavior behavior)
{
    if (behavior == WrapUnlessWrappedAlready && !d->wraps->isEmpty())
        return; // Already wrapped.

    d->updateWraps();
}

} // namespace de
