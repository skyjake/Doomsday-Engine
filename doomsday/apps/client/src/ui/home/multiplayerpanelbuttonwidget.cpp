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
#include "ui/dialogs/serverinfodialog.h"
#include "ui/clientwindow.h"
#include "network/net_main.h"
#include "network/serverlink.h"
#include "resource/idtech1image.h"
#include "clientapp.h"
#include "dd_main.h"

#include <doomsday/doomsdayapp.h>
#include <doomsday/games.h>
#include <doomsday/console/exec.h>
#include <doomsday/LumpCatalog>
#include <doomsday/Games>
#include <de/charsymbols.h>
#include <de/CallbackAction>
#include <de/MessageDialog>
#include <de/PopupButtonWidget>
#include <de/PopupMenuWidget>

#include <QRegExp>

using namespace de;

DENG_GUI_PIMPL(MultiplayerPanelButtonWidget)
, DENG2_OBSERVES(Games, Readiness)
{
    shell::ServerInfo serverInfo;
    ButtonWidget *joinButton;
    String gameConfig;
    LabelWidget *info;
    PopupButtonWidget *extra;
    PopupMenuWidget *extraMenu;
    res::LumpCatalog catalog;

    Impl(Public *i) : Base(i)
    {
        DoomsdayApp::games().audienceForReadiness() += this;

        joinButton = new ButtonWidget;
        joinButton->setText(tr("Join"));
        joinButton->useInfoStyle();
        joinButton->setSizePolicy(ui::Expand, ui::Expand);
        joinButton->setActionFn([this] () { joinButtonPressed(); });
        self().addButton(joinButton);

        info = new LabelWidget;
        info->setSizePolicy(ui::Fixed, ui::Expand);
        info->setAlignment(ui::AlignLeft);
        info->setTextLineAlignment(ui::AlignLeft);
        info->rule().setInput(Rule::Width, self().rule().width());
        info->margins().setLeft(self().icon().rule().width());

        // Menu for additional functions.
        extra = new PopupButtonWidget;
        extra->hide();
        extra->setSizePolicy(ui::Expand, ui::Expand);
        extra->setText("...");
        extra->setFont("small");
        extra->margins().setTopBottom(RuleBank::UNIT);
        extra->rule()
                .setInput(Rule::Bottom, info->rule().bottom() - info->margins().bottom())
                .setMidAnchorX(info->rule().left() + self().icon().rule().width()/2);
        info->add(extra);

        self().panel().setContent(info);
        self().panel().open();

        extra->setPopup([this] (PopupButtonWidget const &) -> PopupWidget * {
            auto *dlg = new ServerInfoDialog(serverInfo);
            QObject::connect(dlg, SIGNAL(joinGame()), thisPublic, SLOT(joinGame()));
            return dlg;
        }, ui::Right);
    }

    void joinButtonPressed() const
    {
        self().root().setFocus(nullptr);
        DENG2_FOR_PUBLIC_AUDIENCE2(AboutToJoin, i)
        {
            i->aboutToJoinMultiplayerGame(serverInfo);
        }
        ClientApp::serverLink().connectToServerAndChangeGame(serverInfo);
    }

    bool hasConfig(String const &token) const
    {
        return gameConfig.containsWord(token);
    }

    void gameReadinessUpdated()
    {
        // Let's refresh the icons.
        catalog.clear();
        self().updateContent(serverInfo);
    }

    DENG2_PIMPL_AUDIENCE(AboutToJoin)
};

DENG2_AUDIENCE_METHOD(MultiplayerPanelButtonWidget, AboutToJoin)

MultiplayerPanelButtonWidget::MultiplayerPanelButtonWidget()
    : d(new Impl(this))
{}

ButtonWidget &MultiplayerPanelButtonWidget::joinButton()
{
    return *d->joinButton;
}

void MultiplayerPanelButtonWidget::setSelected(bool selected)
{
    PanelButtonWidget::setSelected(selected);
    d->extra->show(selected);
}

void MultiplayerPanelButtonWidget::itemRightClicked()
{
    PanelButtonWidget::itemRightClicked();
    d->extra->trigger();
}

void MultiplayerPanelButtonWidget::updateContent(shell::ServerInfo const &info)
{
    d->serverInfo = info;
    d->gameConfig = info.gameConfig();

#define DIM_MDASH   _E(C) DENG2_CHAR_MDASH _E(.)

    //label().setText(info.name);
    String meta;
    int const playerCount = info.playerCount();
    if (playerCount > 0)
    {
        meta = String("%1 player%2 " DIM_MDASH " ")
                .arg(playerCount)
                .arg(DENG2_PLURAL_S(playerCount));
    }

    meta += String("%1").arg(tr(d->hasConfig("coop")? "Co-op" :
                                d->hasConfig("dm2")?  "Deathmatch II" :
                                                      "Deathmatch"));

    if (ClientApp::serverLink().isServerOnLocalNetwork(info.address()))
    {
        meta = "LAN " DIM_MDASH " " + meta;
    }

    label().setText(String(_E(b) "%1\n" _E(l) "%2")
                    .arg(info.name())
                    .arg(meta));

    // Additional information.
    String infoText = String(info.map()) + " " DIM_MDASH " ";
    if (DoomsdayApp::games().contains(info.gameId()))
    {
        auto const &game = DoomsdayApp::games()[info.gameId()];
        infoText += game.title();
        d->joinButton->enable();

        /// @todo The server info should include the list of packages.
        if (d->catalog.setPackages(game.requiredPackages()))
        {
            icon().setImage(IdTech1Image::makeGameLogo(game, d->catalog));
        }
    }
    else
    {
        infoText += tr("Unknown game");
        d->joinButton->disable();

        icon().setImage(nullptr);
    }
    infoText += "\n" _E(C) + String(info.description());

    d->info->setFont("small");
    d->info->setText(infoText);
}

void MultiplayerPanelButtonWidget::joinGame()
{
    d->joinButtonPressed();
}
