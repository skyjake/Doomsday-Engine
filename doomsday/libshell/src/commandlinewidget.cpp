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

#include "de/shell/CommandLineWidget"
#include "de/shell/TextRootWidget"
#include "de/shell/KeyEvent"
#include <de/String>

namespace de {
namespace shell {

DENG2_PIMPL(CommandLineWidget)
{
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

    Instance(Public &i) : Base(i), historyPos(0)
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
        command().text = self.text();
        command().cursor = self.cursor();
    }

    void updateEditor()
    {
        self.setText(command().text);
        self.setCursor(command().cursor);
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

CommandLineWidget::CommandLineWidget(de::String const &name)
    : LineEditWidget(name), d(new Instance(*this))
{
    setPrompt("> ");
}

CommandLineWidget::~CommandLineWidget()
{
    delete d;
}

bool CommandLineWidget::handleEvent(Event const &event)
{
    // There are only key press events.
    DENG2_ASSERT(event.type() == Event::KeyPress);
    KeyEvent const &ev = static_cast<KeyEvent const &>(event);

    bool eaten = true;

    // Control char?
    if(ev.text().isEmpty())
    {
        // Override the editor's normal Enter handling.
        if(ev.key() == Qt::Key_Enter)
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

            emit commandEntered(entered);

            return true;
        }

        eaten = LineEditWidget::handleEvent(event);

        if(!eaten)
        {
            // History navigation.
            switch(ev.key())
            {
            case Qt::Key_Up:
                d->navigateHistory(-1);
                eaten = true;
                break;

            case Qt::Key_Down:
                d->navigateHistory(+1);
                eaten = true;
                break;

            default:
                break;
            }
        }
        return eaten;
    }
    else
    {
        return LineEditWidget::handleEvent(event);
    }
}

} // namespace shell
} // namespace de
