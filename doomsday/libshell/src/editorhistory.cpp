/** @file editorhistory.cpp  Text editor history buffer.
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

#include "de/shell/EditorHistory"
#include <de/math.h>

namespace de {
namespace shell {

DENG2_PIMPL(EditorHistory)
{
    ITextEditor *editor;

    /**
     * Line of text with a cursor.
     */
    struct Command
    {
        String text;
        String original; ///< For undoing editing in history.
        int cursor; ///< Index in range [0...text.size()]

        Command() : cursor(0) {}
    };

    // Command history.
    QList<Command> history;
    int historyPos;

    Instance(Public *i) : Base(i), editor(0), historyPos(0)
    {
        history.append(Command());
    }

    ~Instance() {}

    Command &command()
    {
        DENG2_ASSERT(historyPos >= 0 && historyPos < history.size());
        return history[historyPos];
    }

    Command const &command() const
    {
        DENG2_ASSERT(historyPos >= 0 && historyPos < history.size());
        return history[historyPos];
    }

    void updateCommandFromEditor()
    {
        DENG2_ASSERT(editor != 0);

        command().text = editor->text();
        command().cursor = editor->cursor();
    }

    void updateEditor()
    {
        DENG2_ASSERT(editor != 0);

        editor->setText(command().text);
        editor->setCursor(command().cursor);
    }

    bool navigateHistory(int offset)
    {
        if((offset < 0 && historyPos >= -offset) ||
           (offset > 0 && historyPos < history.size() - offset))
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
        for(int i = 0; i < history.size(); ++i)
        {
            Command &cmd = history[i];
            cmd.text = cmd.original;
            cmd.cursor = de::min(cmd.cursor, cmd.text.size());
        }
    }
};

EditorHistory::EditorHistory(ITextEditor *editor) : d(new Instance(this))
{
    d->editor = editor;
}

void EditorHistory::setEditor(ITextEditor &editor)
{
    d->editor = &editor;
}

ITextEditor &EditorHistory::editor()
{
    DENG2_ASSERT(d->editor != 0);
    return *d->editor;
}

String EditorHistory::enter()
{
    d->updateCommandFromEditor();

    String entered = d->command().text;

    // Update the history.
    if(d->historyPos < d->history.size() - 1)
    {
        if(d->history.last().text.isEmpty())
        {
            // Prune an empty entry in the end of history.
            d->history.removeLast();
        }
        // Currently back in the history; duplicate the edited entry.
        d->history.append(d->command());
    }

    d->history.last().original = entered;

    // Move on.
    d->history.append(Instance::Command());
    d->historyPos = d->history.size() - 1;
    d->updateEditor();
    d->restoreTextsToOriginal();

    return entered;
}

bool EditorHistory::handleControlKey(int qtKey)
{
    switch(qtKey)
    {
    case Qt::Key_Up:
        d->navigateHistory(-1);
        return true;

    case Qt::Key_Down:
        d->navigateHistory(+1);
        return true;

    default:
        break;
    }
    return false;
}

} // namespace shell
} // namespace de
