/** @file multiplayerpanelbuttonwidget.cpp
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "ui/clientstyle.h"
#include "ui/clientwindow.h"
#include "network/net_main.h"
#include "network/serverlink.h"
#include "clientapp.h"
#include "dd_main.h"

#include <doomsday/doomsdayapp.h>
#include <doomsday/games.h>
#include <doomsday/console/exec.h>
#include <doomsday/res/lumpcatalog.h>
#include <doomsday/games.h>

#include <de/charsymbols.h>
#include <de/callbackaction.h>
#include <de/messagedialog.h>
#include <de/popupbuttonwidget.h>
#include <de/popupmenuwidget.h>
#include <de/regexp.h>
#include <de/taskpool.h>

using namespace de;

DE_GUI_PIMPL(MultiplayerPanelButtonWidget)
, DE_OBSERVES(Games, Readiness)
{
    ServerInfo         serverInfo;
    ButtonWidget *     joinButton;
    String             gameConfig;
    LabelWidget *      info;
    PopupButtonWidget *extra;
    PopupMenuWidget *  extraMenu;
    res::LumpCatalog   catalog;
    TaskPool           tasks;

    Impl(Public *i) : Base(i)
    {
        DoomsdayApp::games().audienceForReadiness() += this;

        joinButton = new ButtonWidget;
        joinButton->setAttribute(AutomaticOpacity);
        joinButton->disable();
        joinButton->setText("Join");
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

        extra->setPopup([this] (const PopupButtonWidget &) -> PopupWidget * {
            auto *dlg = new ServerInfoDialog(serverInfo);
            dlg->audienceForJoinGame() += [this](){ self().joinGame(); };
            return dlg;
        }, ui::Right);
    }

    void joinButtonPressed() const
    {
        self().root().setFocus(nullptr);
        DE_NOTIFY_PUBLIC(AboutToJoin, i)
        {
            i->aboutToJoinMultiplayerGame(serverInfo);
        }
        ClientApp::serverLink().connectToServerAndChangeGameAsync(serverInfo);
    }

    bool hasConfig(const String &token) const
    {
        return gameConfig.containsWord(token);
    }

    void gameReadinessUpdated()
    {
        // Let's refresh the icons.
        catalog.clear();
        self().updateContent(serverInfo);
    }

    DE_PIMPL_AUDIENCE(AboutToJoin)
};

DE_AUDIENCE_METHOD(MultiplayerPanelButtonWidget, AboutToJoin)

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

void MultiplayerPanelButtonWidget::updateContent(const ServerInfo &info)
{
    d->serverInfo = info;
    d->gameConfig = info.gameConfig();

#define DIM_MDASH   _E(C) DE_CHAR_MDASH _E(.)

    //label().setText(info.name);
    String meta;
    const int playerCount = info.playerCount();
    if (playerCount > 0)
    {
        meta = Stringf("%i player%s " DIM_MDASH " ", playerCount, DE_PLURAL_S(playerCount));
    }

    meta += d->hasConfig("coop")? "Co-op" :
            d->hasConfig("dm2")?  "Deathmatch II" :
                                  "Deathmatch";

    if (ClientApp::serverLink().isServerOnLocalNetwork(info.address()))
    {
        meta = "LAN " DIM_MDASH " " + meta;
    }

    label().setText(Stringf(_E(b) "%s\n" _E(l) "%s", info.name().c_str(), meta.c_str()));

    // Additional information.
    String infoText = String(info.map()) + " " DIM_MDASH " ";
    if (DoomsdayApp::games().contains(info.gameId()))
    {
        const auto &game = DoomsdayApp::games()[info.gameId()];
        infoText += game.title();
        d->joinButton->enable();

        if (d->catalog.setPackages(game.requiredPackages()))
        {
            res::LumpCatalog catalog{d->catalog};
            d->tasks.async([&game, catalog]() { return Variant(ClientStyle::makeGameLogo(game, catalog)); },
                           [this](const Variant &logo) { icon().setImage(logo.value<Image>()); });
        }
    }
    else
    {
        infoText += "Unknown game";
        d->joinButton->disable();

        icon().setImage(nullptr);
    }
    if (!info.flags().testFlag(ServerInfo::AllowJoin))
    {
        d->joinButton->disable();
    }
    infoText += "\n" _E(C) + info.description() + _E(.);

    const int localCount = Game::localMultiplayerPackages(info.gameId()).sizei();
    if (localCount)
    {
        infoText += "\n" _E(D) _E(b) +
                    Stringf("%i local package%s", localCount, DE_PLURAL_S(localCount));
    }

    d->info->setFont("small");
    d->info->setText(infoText);
}

void MultiplayerPanelButtonWidget::joinGame()
{
    d->joinButtonPressed();
}
