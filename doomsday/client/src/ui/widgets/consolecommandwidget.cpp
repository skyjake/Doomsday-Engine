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
#include "ui/widgets/documentwidget.h"
#include "ui/widgets/popupwidget.h"
#include "ui/style.h"
#include "con_main.h"
#include "dd_main.h"
#include "clientapp.h"

#include <de/shell/EditorHistory>
#include <de/KeyEvent>
#include <de/App>

using namespace de;

DENG_GUI_PIMPL(ConsoleCommandWidget),
DENG2_OBSERVES(App, StartupComplete),
DENG2_OBSERVES(App, GameChange)
{
    shell::EditorHistory history;
    DocumentWidget *completions;
    PopupWidget *popup; ///< Popup for autocompletions.

    Instance(Public *i) : Base(i), history(i)
    {
        App::app().audienceForStartupComplete += this;
        App::app().audienceForGameChange += this;

        // Popup for autocompletions.
        completions = new DocumentWidget;
        completions->setMaximumLineWidth(640);

        popup = new PopupWidget;
        popup->useInfoStyle();
        /*
        popup->set(Background(st.colors().colorf("editor.completion.background"),
                              Background::BorderGlow,
                              st.colors().colorf("editor.completion.glow"),
                              st.rules().rule("glow").valuei()));
                              */
        popup->setContent(completions);

        // Height for the content: depends on the document height (plus margins), but at
        // most 400; never extend outside the view, though.
        completions->rule().setInput(Rule::Height,
                OperatorRule::minimum(
                    OperatorRule::minimum(style().rules().rule("editor.completion.height"),
                                          completions->contentRule().height() +
                                          completions->margins().height()),
                    self.rule().top() - style().rules().rule("gap")));

        self.add(popup);
    }

    ~Instance()
    {
        App::app().audienceForStartupComplete -= this;
        App::app().audienceForGameChange -= this;
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

    // Get rid of the autocompletion popup.
    d->popup->close();

    emit lostFocus();
}

bool ConsoleCommandWidget::handleEvent(Event const &event)
{
    if(hasFocus())
    {
        // Console bindings override normal event handling.
        if(ClientApp::widgetActions().tryEvent(event, "console"))
        {
            // Eaten by bindings.
            return true;
        }
    }

    if(hasFocus() && event.isKeyDown())
    {
        KeyEvent const &key = event.as<KeyEvent>();

        // Override the handling of the Enter key.
        if(key.qtKey() == Qt::Key_Return || key.qtKey() == Qt::Key_Enter)
        {
            // We must make sure that the ongoing autocompletion ends.
            acceptCompletion();

            String const entered = d->history.enter();

            LOG_INFO(_E(1) "> ") << entered;

            // Execute the command right away.
            Con_Execute(CMDS_CONSOLE, entered.toUtf8(), false, false);

            emit commandEntered(entered);

            return true;
        }
    }

    if(LineEditWidget::handleEvent(event))
    {
        // Editor handled the event normally.
        return true;
    }

    if(hasFocus())
    {
        // All Tab keys are eaten by a focused console command widget.
        if(event.isKey() && event.as<KeyEvent>().ddKey() == DDKEY_TAB)
        {
            return true;
        }

        if(event.isKeyDown())
        {
            // Fall back to history navigation.
            return d->history.handleControlKey(event.as<KeyEvent>().qtKey());
        }
    }
    return false;
}

void ConsoleCommandWidget::dismissContentToHistory()
{
    d->history.goToLatest();

    if(!text().isEmpty())
    {
        d->history.enter();
    }
}

void ConsoleCommandWidget::autoCompletionBegan(String const &)
{
    // Prepare a list of annotated completions to show in the popup.
    QStringList const compls = suggestedCompletions();
    if(compls.size() > 1)
    {
        d->completions->setText(Con_AnnotatedConsoleTerms(compls));
        d->completions->scrollToTop(0);

        // Note: this is a fixed position, so it will not be updated if the view is resized.
        d->popup->setAnchorX(cursorRect().middle().x);
        d->popup->setAnchorY(rule().top());
        d->popup->open();
    }
}

void ConsoleCommandWidget::autoCompletionEnded(bool accepted)
{
    LineEditWidget::autoCompletionEnded(accepted);

    d->popup->close();
}
