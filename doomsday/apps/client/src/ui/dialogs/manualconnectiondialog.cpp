/** @file manualconnectiondialog.cpp  Dialog for connecting to a server.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "ui/widgets/multiplayerservermenuwidget.h"
#include "ui/home/multiplayerpanelbuttonwidget.h"
#include "network/serverlink.h"
#include "clientapp.h"

#include <de/foldpanelwidget.h>
#include <de/persistentstate.h>
#include <de/progresswidget.h>

using namespace de;

DE_PIMPL(ManualConnectionDialog)
, DE_OBSERVES(ServerLink, Discovery)
, DE_OBSERVES(MultiplayerServerMenuWidget, AboutToJoin)
{
    String usedAddress;
    FoldPanelWidget *fold;
    MultiplayerServerMenuWidget *servers;
    ProgressWidget *progress;
    bool querying;
    bool joinWhenEnterPressed;
    bool autoJoin;

    Impl(Public *i)
        : Base(i)
        , querying(false)
        , joinWhenEnterPressed(false)
        , autoJoin(true)
    {
        ClientApp::serverLink().audienceForDiscovery() += this;
    }

    void serversDiscovered(const ServerLink &link) override
    {
        if (querying)
        {
            // Time to show what we found.
            querying = false;
            progress->setRotationSpeed(0);
            self().editor().enable();
            self().validate();

            if (link.foundServerCount(ServerLink::Direct) > 0)
            {
                progress->hide();
                fold->open();

                if (link.foundServerCount(ServerLink::Direct) == 1)
                {
                    joinWhenEnterPressed = true;
                }
            }
            else
            {
                fold->close(0.0);
                progress->setRotationSpeed(0);
                progress->setText(_E(l) "No response");
                progress->setOpacity(0, 4.0, 2.0);
            }
        }
    }

    ButtonWidget &connectButton()
    {
        return self().buttonWidget("Connect");
    }

    void aboutToJoinMultiplayerGame(const ServerInfo &) override
    {
        self().accept();
    }
};

ManualConnectionDialog::ManualConnectionDialog(const String &name)
    : InputDialog(name), d(new Impl(this))
{
    area().enableIndicatorDraw(true);

    add(d->progress = new ProgressWidget);
    d->progress->useMiniStyle("altaccent");
    d->progress->setWidthPolicy(ui::Expand);
    d->progress->setTextAlignment(ui::AlignLeft);
    d->progress->hide();

    // The found servers are shown inside a fold panel.
    d->fold = new FoldPanelWidget;
    d->servers = new MultiplayerServerMenuWidget(MultiplayerServerMenuWidget::DirectDiscoveryOnly);
    d->servers->audienceForAboutToJoin() += d;
    d->servers->margins().setLeft("dialog.gap");
    /*connect(d->servers, SIGNAL(sessionSelected(const de::ui::Item *)),
            this,     SIGNAL(selected(const de::ui::Item *)));
    connect(d->servers, SIGNAL(sessionSelected(const de::ui::Item *)),
            this,     SLOT  (serverSelected(const de::ui::Item *)));*/
    d->servers->rule().setInput(Rule::Width, rule().width() - margins().width());
    d->fold->setContent(d->servers);
    area().add(d->fold);

    title().setText("Connect to Server");
    message().setText(
        "Enter the IP address or domain name of the multiplayer server you want to connect to. "
        "Optionally, you may include a TCP port number, for example " _E(
            b) "10.0.1.1:13209" _E(.) ".");

    buttons().clear() << new DialogButtonItem(Default, "Connect", [this]() { queryOrConnect(); })
                      << new DialogButtonItem(Reject);

    d->connectButton().disable();

    d->progress->rule()
            .setInput(Rule::Top,    buttonsMenu().rule().top())
            .setInput(Rule::Right,  buttonsMenu().rule().left())
            .setInput(Rule::Height, buttonsMenu().rule().height() - margins().bottom());

    editor().audienceForEnter().clear();
    editor().audienceForEnter() += [this](){ queryOrConnect(); };
    editor().audienceForContentChange() += [this](){ validate(); contentChanged(); };

    updateLayout(IncludeHidden); // fold widgets are hidden while closed
}

void ManualConnectionDialog::enableJoinWhenSelected(bool joinWhenSelected)
{
    d->autoJoin = joinWhenSelected;
}

void ManualConnectionDialog::operator>>(PersistentState &toState) const
{
    toState.objectNamespace().set(name() + ".address", d->usedAddress);
}

void ManualConnectionDialog::operator<<(const PersistentState &fromState)
{
    d->usedAddress = fromState[name() + ".address"];
    editor().setText(d->usedAddress);
    validate();
}

void ManualConnectionDialog::queryOrConnect()
{
    if (d->connectButton().isDisabled())
    {
        // Should not try now.
        return;
    }

    if (!d->querying)
    {
        // Automatically connect if there is a single choice.
        if (d->joinWhenEnterPressed)
        {
            d->servers->childWidgets().first()->as<MultiplayerPanelButtonWidget>()
                    .joinButton().trigger();
            //emit selected(&d->servers->items().at(0));
            //serverSelected(&d->servers->items().at(0));
            return;
        }

        d->joinWhenEnterPressed = false;
        d->querying = true;
        d->progress->setText(_E(l) "Looking for host...");
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

    if (d->querying)
    {
        valid = false;
    }

    if (editor().text().isEmpty()     ||
        editor().text().contains(';') ||
        editor().text().endsWith(":") ||
        (editor().text().beginsWith(":") && !editor().text().beginsWith("::")))
    {
        valid = false;
    }

    d->connectButton().enable(valid);
}

/*
void ManualConnectionDialog::serverSelected(const ui::Item *item)
{
    if (d->autoJoin)
    {
        setAcceptanceAction(makeAction(*item));
    }
    accept();
}*/

void ManualConnectionDialog::finish(int result)
{
    if (result)
    {
        // The dialog was accepted.
        d->usedAddress = editor().text();
    }
    InputDialog::finish(result);
}
