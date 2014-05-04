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
#include "dd_main.h"
#include "clientapp.h"

#include <doomsday/console/knownword.h>
#include <doomsday/console/exec.h>
#include <de/KeyEvent>
#include <de/App>

using namespace de;

DENG_GUI_PIMPL(ConsoleCommandWidget),
DENG2_OBSERVES(App, StartupComplete),
DENG2_OBSERVES(App, GameChange)
{
    Instance(Public *i) : Base(i)
    {
        App::app().audienceForStartupComplete() += this;
        App::app().audienceForGameChange() += this;
    }

    ~Instance()
    {
        App::app().audienceForStartupComplete() -= this;
        App::app().audienceForGameChange() -= this;
    }

    void appStartupCompleted()
    {
        updateLexicon();
    }

    void currentGameChanged(game::Game const &)
    {
        updateLexicon();
    }

    void updateLexicon()
    {
        self.setLexicon(Con_Lexicon());
    }
};

ConsoleCommandWidget::ConsoleCommandWidget(String const &name)
    : CommandWidget(name), d(new Instance(this))
{
    d->updateLexicon();
}

void ConsoleCommandWidget::focusGained()
{
    CommandWidget::focusGained();
    ClientApp::widgetActions().activateContext("console");
}

void ConsoleCommandWidget::focusLost()
{
    CommandWidget::focusLost();
    ClientApp::widgetActions().deactivateContext("console");
}

bool ConsoleCommandWidget::handleEvent(Event const &event)
{
    if(isDisabled()) return false;

    if(hasFocus())
    {
        // Console bindings override normal event handling.
        if(ClientApp::widgetActions().tryEvent(event, "console"))
        {
            // Eaten by bindings.
            return true;
        }
    }

    return CommandWidget::handleEvent(event);
}

bool ConsoleCommandWidget::isAcceptedAsCommand(String const &)
{
    // Everything is OK for a console command.
    return true;
}

void ConsoleCommandWidget::executeCommand(String const &text)
{
    LOG_SCR_NOTE(_E(1) "> ") << text;

    // Execute the command right away.
    Con_Execute(CMDS_CONSOLE, text.toUtf8(), false, false);
}

void ConsoleCommandWidget::autoCompletionBegan(String const &)
{
    // Prepare a list of annotated completions to show in the popup.
    QStringList const compls = suggestedCompletions();
    if(compls.size() > 1)
    {
        showAutocompletionPopup(Con_AnnotatedConsoleTerms(compls));
    }
}
