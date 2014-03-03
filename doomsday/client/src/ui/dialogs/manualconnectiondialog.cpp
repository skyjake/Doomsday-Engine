/** @file manualconnectiondialog.cpp  Dialog for connecting to a server.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/dialogs/manualconnectiondialog.h"
#include "ui/widgets/mpselectionwidget.h"
#include "clientapp.h"

#include <de/SignalAction>
#include <de/FoldPanelWidget>
#include <de/PersistentState>

using namespace de;

DENG2_PIMPL(ManualConnectionDialog)
, DENG2_OBSERVES(ServerLink, DiscoveryUpdate)
, DENG2_OBSERVES(MPSelectionWidget, Selection)
{
    String usedAddress;
    FoldPanelWidget *fold;
    MPSelectionWidget *games;
    ProgressWidget *progress;
    bool querying;
    bool joinWhenEnterPressed;

    Instance(Public *i)
        : Base(i)
        , querying(false)
        , joinWhenEnterPressed(false)
    {
        ClientApp::serverLink().audienceForDiscoveryUpdate += this;
    }

    ~Instance()
    {
        ClientApp::serverLink().audienceForDiscoveryUpdate -= this;
    }

    void linkDiscoveryUpdate(ServerLink const &link)
    {
        if(querying)
        {
            // Time to show what we found.
            querying = false;
            progress->setRotationSpeed(0);
            self.editor().enable();
            self.validate();

            if(link.foundServerCount(ServerLink::Direct) > 0)
            {
                progress->hide();
                fold->open();

                if(link.foundServerCount(ServerLink::Direct) == 1)
                {
                    joinWhenEnterPressed = true;
                }
            }
            else
            {
                fold->close(0);
                progress->setRotationSpeed(0);
                progress->setText(_E(l) + tr("No response"));
                progress->setOpacity(0, 4, 2);
            }
        }
    }

    void gameSelected(serverinfo_t const &info)
    {
        self.setAcceptanceAction(new MPSelectionWidget::JoinAction(info));
        self.accept();
    }

    ButtonWidget &connectButton()
    {
        return self.buttonWidget(tr("Connect"));
    }
};

ManualConnectionDialog::ManualConnectionDialog(String const &name)
    : InputDialog(name), d(new Instance(this))
{
    add(d->progress = new ProgressWidget);
    d->progress->useMiniStyle("altaccent");
    d->progress->setWidthPolicy(ui::Expand);
    d->progress->setTextAlignment(ui::AlignLeft);
    d->progress->hide();

    // The found games are shown inside a fold panel.
    d->fold  = new FoldPanelWidget;
    d->games = new MPSelectionWidget(MPSelectionWidget::DirectDiscoveryOnly);
    d->games->setJoinGameWhenSelected(false);
    d->games->audienceForSelection += d;
    d->games->setColumns(1);
    d->games->rule().setInput(Rule::Width, rule().width() - margins().width());
    d->fold->setContent(d->games);
    area().add(d->fold);

    title().setText(tr("Connect to Server"));
    message().setText(tr("Enter the IP address or domain name of the multiplayer server you want to connect to. "
                         "Optionally, you may include a TCP port number, for example "
                         _E(b) "10.0.1.1:13209" _E(.) "."));

    buttons().clear()
            << new DialogButtonItem(Default, tr("Connect"),
                                    new SignalAction(this, SLOT(queryOrConnect())))
            << new DialogButtonItem(Reject);

    d->connectButton().disable();

    d->progress->rule()
            .setInput(Rule::Top,    buttonsMenu().rule().top())
            .setInput(Rule::Right,  buttonsMenu().rule().left())
            .setInput(Rule::Height, buttonsMenu().rule().height() - margins().bottom());

    disconnect(&editor(), SIGNAL(enterPressed(QString)), this, SLOT(accept()));
    connect(&editor(), SIGNAL(enterPressed(QString)), this, SLOT(queryOrConnect()));
    connect(&editor(), SIGNAL(editorContentChanged()), this, SLOT(validate()));
    connect(&editor(), SIGNAL(editorContentChanged()), this, SLOT(contentChanged()));
    connect(&ClientApp::serverLink(), SIGNAL(disconnected()), this, SLOT(disconnected()));

    updateLayout();
}

void ManualConnectionDialog::operator >> (PersistentState &toState) const
{
    toState.names().set(name() + ".address", d->usedAddress);
}

void ManualConnectionDialog::operator << (PersistentState const &fromState)
{
    d->usedAddress = fromState.names()[name() + ".address"];
    editor().setText(d->usedAddress);
    validate();
}

void ManualConnectionDialog::queryOrConnect()
{
    if(d->connectButton().isDisabled())
    {
        // Should not try now.
        return;
    }

    if(!d->querying)
    {
        // Automatically connect if there is a single choice.
        if(d->joinWhenEnterPressed)
        {  
            d->gameSelected(d->games->serverInfo(0));
            return;
        }

        d->joinWhenEnterPressed = false;
        d->querying = true;
        d->progress->setText(_E(l) + tr("Looking for host..."));
        d->progress->setRotationSpeed(40);
        d->progress->show();
        d->progress->setOpacity(1);
        editor().disable();
        validate();

        ClientApp::serverLink().discover(editor().text());
    }
}

void ManualConnectionDialog::contentChanged()
{
    d->joinWhenEnterPressed = false;
}

void ManualConnectionDialog::validate()
{
    bool valid = true;

    if(d->querying)
    {
        valid = false;
    }

    if(editor().text().isEmpty() || editor().text().contains(';') ||
       editor().text().endsWith(":") || editor().text().startsWith(":"))
    {
        valid = false;
    }

    d->connectButton().enable(valid);
}

void ManualConnectionDialog::disconnected()
{
    d->linkDiscoveryUpdate(ClientApp::serverLink());
}

void ManualConnectionDialog::finish(int result)
{
    if(result)
    {
        // The dialog was accepted.
        d->usedAddress = editor().text();
    }

    InputDialog::finish(result);
}
