/** @file multiplayerpanelbuttonwidget.cpp
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/home/multiplayerpanelbuttonwidget.h"
#include "ui/widgets/taskbarwidget.h"
#include "ui/clientwindow.h"
#include "clientapp.h"
#include "dd_share.h" // serverinfo_s
#include "dd_main.h"

#include <doomsday/doomsdayapp.h>
#include <doomsday/games.h>
#include <doomsday/console/exec.h>
#include <doomsday/LumpCatalog>
#include <doomsday/Games>
#include <de/charsymbols.h>
#include <de/CallbackAction>
#include <de/PopupButtonWidget>
#include <de/PopupMenuWidget>

#include <QRegExp>

using namespace de;

DENG_GUI_PIMPL(MultiplayerPanelButtonWidget)
, DENG2_OBSERVES(Games, Readiness)
{
    serverinfo_t serverInfo;
    ButtonWidget *joinButton;
    String gameConfig;
    LabelWidget *info;
    PopupButtonWidget *extra;
    PopupMenuWidget *extraMenu;
    res::LumpCatalog catalog;

    Instance(Public *i) : Base(i)
    {
        DoomsdayApp::games().audienceForReadiness() += this;

        joinButton = new ButtonWidget;
        joinButton->setText(tr("Join"));
        joinButton->useInfoStyle();
        joinButton->setSizePolicy(ui::Expand, ui::Expand);
        joinButton->setActionFn([this] () { joinButtonPressed(); });
        self.addButton(joinButton);

        info = new LabelWidget;
        info->setSizePolicy(ui::Fixed, ui::Expand);
        info->setAlignment(ui::AlignLeft);
        info->setTextLineAlignment(ui::AlignLeft);
        info->rule().setInput(Rule::Width, self.rule().width());
        info->margins().setLeft(self.icon().rule().width());

        // Menu for additional functions.
        extra = new PopupButtonWidget;
        extra->hide();
        extra->setSizePolicy(ui::Expand, ui::Expand);
        extra->setText("...");
        extra->setFont("small");
        extra->margins().setTopBottom("unit");
        extra->rule()
                .setInput(Rule::Bottom, info->rule().bottom() - info->margins().bottom())
                .setMidAnchorX(info->rule().left() + self.icon().rule().width()/2);
        info->add(extra);

        self.panel().setContent(info);
        self.panel().open();
    }

//    ~Instance()
//    {
//        DoomsdayApp::games().audienceForReadiness() -= this;
//    }

    void joinButtonPressed() const
    {
        // Switch locally to the game running on the server.
        BusyMode_FreezeGameForBusyMode();
        ClientWindow::main().taskBar().close();

        // Automatically leave the current MP game.
        if (netGame && isClient)
        {
            ClientApp::serverLink().disconnect();
        }

        /// @todo Set up a temporary profile using packages from the server.

        DoomsdayApp::app().changeGame(
                    DoomsdayApp::games()[serverInfo.gameIdentityKey].profile(),
                    DD_ActivateGameWorker);
        Con_Execute(CMDS_DDAY, String("connect %1:%2")
                    .arg(serverInfo.address).arg(serverInfo.port).toLatin1(),
                    false, false);
    }

    bool hasConfig(String const &token) const
    {
        return QRegExp("\\b" + token + "\\b").indexIn(gameConfig) >= 0;
    }

    void gameReadinessUpdated()
    {
        // Let's refresh the icons.
        catalog.clear();
        self.updateContent(serverInfo);
    }
};

MultiplayerPanelButtonWidget::MultiplayerPanelButtonWidget()
    : d(new Instance(this))
{}

void MultiplayerPanelButtonWidget::setSelected(bool selected)
{
    PanelButtonWidget::setSelected(selected);
    d->extra->show(selected);
}

void MultiplayerPanelButtonWidget::updateContent(serverinfo_s const &info)
{
    d->serverInfo = info;
    d->gameConfig = info.gameConfig;

    //label().setText(info.name);
    String meta;
    if (info.numPlayers > 0)
    {
        meta = String("%1 player%2 " DENG2_CHAR_MDASH " ")
                .arg(info.numPlayers)
                .arg(info.numPlayers != 1? "s" : "");
    }

    meta += String("%1").arg(tr(d->hasConfig("coop")? "Co-op" :
                                d->hasConfig("dm2")?  "Deathmatch v2" :
                                                      "Deathmatch"));

    label().setText(String(_E(b) "%1\n" _E(l) "%2")
                    .arg(info.name)
                    .arg(meta));

    // Additional information.
    String infoText = String(info.map) + " " DENG2_CHAR_MDASH " ";
    if (DoomsdayApp::games().contains(info.gameIdentityKey))
    {
        auto const &game = DoomsdayApp::games()[info.gameIdentityKey];
        infoText += game.title();
        d->joinButton->enable();

        /// @todo The server info should include the list of packages.
        if (d->catalog.setPackages(game.requiredPackages()))
        {
            icon().setImage(makeGameLogo(game, d->catalog));
        }
    }
    else
    {
        infoText += tr("Unknown game");
        d->joinButton->disable();

        icon().setImage(nullptr);
    }
    infoText += "\n" _E(C) + String(info.description);

    d->info->setFont("small");
    d->info->setText(infoText);
}
