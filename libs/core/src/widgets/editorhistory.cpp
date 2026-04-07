/** @file editorhistory.cpp  Text editor history buffer.
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

#include "de/editorhistory.h"
#include <de/math.h>

namespace de {

DE_PIMPL(EditorHistory)
{
    ITextEditor *editor;

    /**
     * Line of text with a cursor.
     */
    struct Command
    {
        String  text;
        String  original; ///< For undoing editing in history.
        BytePos cursor;   ///< Index in range [0...text.size()]

        Command() : cursor(0) {}
    };

    // Command history.
    List<Command> history;
    int historyPos;

    Impl(Public *i) : Base(i), editor(nullptr), historyPos(0)
    {
        history.append(Command());
    }

    ~Impl() {}

    void restore(const StringList& contents)
    {
        history.clear();
        if (contents.isEmpty())
        {
            history.append(Command());
            historyPos = 0;
            return;
        }
        for (const String &line : contents)
        {
            Command cmd;
            cmd.text = cmd.original = line;
            cmd.cursor = cmd.text.sizeb();
            history.append(cmd);
        }
        historyPos = history.sizei() - 1;
    }

    Command &command()
    {
        DE_ASSERT(historyPos >= 0 && historyPos < history.sizei());
        return history[historyPos];
    }

    const Command &command() const
    {
        DE_ASSERT(historyPos >= 0 && historyPos < history.sizei());
        return history[historyPos];
    }

    void updateCommandFromEditor()
    {
        DE_ASSERT(editor != nullptr);

        command().text = editor->text();
        command().cursor = editor->cursor();
    }

    void updateEditor()
    {
        DE_ASSERT(editor != 0);

        editor->setText(command().text);
        editor->setCursor(command().cursor);
    }

    bool navigateHistory(int offset)
    {
        if ((offset < 0 && historyPos >= -offset) ||
            (offset > 0 && historyPos < history.sizei() - offset))
        {
            // Save the current state.
            updateCommandFromEditor();

            historyPos += offset;

            // Update to the historical state.
            updateEditor();
            return true;
        }
        return false;
    }

    void restoreTextsToOriginal()
    {
        for (dsize i = 0; i < history.size(); ++i)
        {
            Command &cmd = history[i];
            cmd.text = cmd.original;
            cmd.cursor = de::min(cmd.cursor, cmd.text.sizeb());
        }
    }
};

EditorHistory::EditorHistory(ITextEditor *editor) : d(new Impl(this))
{
    d->editor = editor;
}

void EditorHistory::setEditor(ITextEditor &editor)
{
    d->editor = &editor;
}

ITextEditor &EditorHistory::editor()
{
    DE_ASSERT(d->editor != nullptr);
    return *d->editor;
}

bool EditorHistory::isAtLatest() const
{
    return d->historyPos == d->history.sizei() - 1;
}

void EditorHistory::goToLatest()
{
    d->updateCommandFromEditor();
    d->historyPos = d->history.sizei() - 1;
    d->updateEditor();
}

String EditorHistory::enter()
{
    d->updateCommandFromEditor();

    String entered = d->command().text;
    if (!entered.isEmpty())
    {
        // Update the history.
        if (d->historyPos < d->history.sizei() - 1)
        {
            if (d->history.last().text.isEmpty())
            {
                // Prune an empty entry in the end of history.
                d->history.removeLast();
            }
            // Currently back in the history; duplicate the edited entry.
            d->history.append(d->command());
        }
        d->history.last().original = entered;
        d->history.append(Impl::Command());
    }

    // Move on.
    d->historyPos = d->history.sizei() - 1;
    d->updateEditor();
    d->restoreTextsToOriginal();

    return entered;
}

bool EditorHistory::handleControlKey(term::Key key)
{
    switch (key)
    {
        case term::Key::Up:   d->navigateHistory(-1); return true;
        case term::Key::Down: d->navigateHistory(+1); return true;
        default: break;
    }
    return false;
}

StringList EditorHistory::fullHistory(int maxCount) const
{
    StringList lines;
    for (const auto &cmd : d->history)
    {
        lines << cmd.original;
        if (maxCount > 0 && lines.sizei() == maxCount)
        {
            break;
        }
    }
    return lines;
}

void EditorHistory::setFullHistory(const StringList& history)
{
    d->restore(history);
}

} // namespace de
