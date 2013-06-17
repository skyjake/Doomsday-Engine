/** @file consolecommandwidget.h  Text editor with a history buffer.
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

#include "ui/widgets/consolecommandwidget.h"
#include "con_main.h"
#include "dd_main.h"

#include <de/shell/EditorHistory>
#include <de/KeyEvent>
#include <de/App>

using namespace de;

DENG2_PIMPL(ConsoleCommandWidget),
DENG2_OBSERVES(App, StartupComplete),
public IGameChangeObserver
{
    shell::EditorHistory history;

    Instance(Public *i) : Base(i), history(i)
    {
        App::app().audienceForStartupComplete += this;
        audienceForGameChange += this;
    }

    ~Instance()
    {
        App::app().audienceForStartupComplete -= this;
        audienceForGameChange -= this;
    }

    void appStartupCompleted()
    {
        updateLexicon();
    }

    void currentGameChanged(Game &)
    {
        updateLexicon();
    }

    void updateLexicon()
    {
        self.setLexicon(Con_Lexicon());
    }
};

ConsoleCommandWidget::ConsoleCommandWidget(String const &name)
    : LineEditWidget(name), d(new Instance(this))
{
    d->updateLexicon();
}

void ConsoleCommandWidget::focusGained()
{
    LineEditWidget::focusGained();
    emit gotFocus();
}

void ConsoleCommandWidget::focusLost()
{
    LineEditWidget::focusLost();
    emit lostFocus();
}

bool ConsoleCommandWidget::handleEvent(Event const &event)
{
    if(hasFocus() && event.isKeyDown())
    {
        KeyEvent const &key = event.as<KeyEvent>();

        // Override the handling of the Enter key.
        if(key.qtKey() == Qt::Key_Return || key.qtKey() == Qt::Key_Enter)
        {
            String const entered = d->history.enter();

            LOG_INFO(_E(1) "> ") << entered;

            // Execute the command right away.
            Con_Execute(CMDS_CONSOLE, entered.toUtf8(), false, false);

            return true;
        }
    }

    if(LineEditWidget::handleEvent(event))
    {
        // Editor handled the event normally.
        return true;
    }

    if(hasFocus() && event.isKeyDown())
    {
        // Fall back to history navigation.
        return d->history.handleControlKey(event.as<KeyEvent>().qtKey());
    }
    return false;
}
