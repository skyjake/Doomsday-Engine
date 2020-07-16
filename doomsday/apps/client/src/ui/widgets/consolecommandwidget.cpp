/** @file consolecommandwidget.h  Text editor with a history buffer.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/app.h>
#include <de/keyevent.h>
#include <doomsday/console/knownword.h>
#include <doomsday/console/exec.h>
#include "ui/inputsystem.h"
#include "clientapp.h"
#include "dd_main.h"
#include "ui/bindcontext.h"

using namespace de;

DE_GUI_PIMPL(ConsoleCommandWidget),
DE_OBSERVES(App, StartupComplete),
DE_OBSERVES(DoomsdayApp, GameChange)
{
    Impl(Public *i) : Base(i)
    {
        App::app().audienceForStartupComplete() += this;
        DoomsdayApp::app().audienceForGameChange() += this;
    }

//    ~Impl()
//    {
//        App::app().audienceForStartupComplete() -= this;
//        DoomsdayApp::app().audienceForGameChange() -= this;
//    }

    void appStartupCompleted()
    {
        updateLexicon();
    }

    void currentGameChanged(const Game &)
    {
        updateLexicon();
    }

    void updateLexicon()
    {
        self().setLexicon(Con_Lexicon());
    }
};

ConsoleCommandWidget::ConsoleCommandWidget(const String &name)
    : CommandWidget(name), d(new Impl(this))
{
    d->updateLexicon();
}

void ConsoleCommandWidget::focusGained()
{
    CommandWidget::focusGained();
    ClientApp::input().context("console").activate();
}

void ConsoleCommandWidget::focusLost()
{
    CommandWidget::focusLost();
    ClientApp::input().context("console").deactivate();
}

bool ConsoleCommandWidget::handleEvent(const Event &event)
{
    if (isDisabled()) return false;

    if (hasFocus())
    {
        // Console bindings override normal event handling.
        if (ClientApp::input().tryEvent(event, "console"))
        {
            // Eaten by bindings.
            return true;
        }
    }

    return CommandWidget::handleEvent(event);
}

bool ConsoleCommandWidget::isAcceptedAsCommand(const String &)
{
    // Everything is OK for a console command.
    return true;
}

void ConsoleCommandWidget::executeCommand(const String &text)
{
    LOG_SCR_NOTE(_E(1) "> ") << text;

    // Execute the command right away.
    Con_Execute(CMDS_CONSOLE, text, false, false);
}

void ConsoleCommandWidget::autoCompletionBegan(const String &)
{
    // Prepare a list of annotated completions to show in the popup.
    const auto compls = suggestedCompletions();
    if (compls.size() > 1)
    {
        showAutocompletionPopup(Con_AnnotatedConsoleTerms(compls));
    }
}
