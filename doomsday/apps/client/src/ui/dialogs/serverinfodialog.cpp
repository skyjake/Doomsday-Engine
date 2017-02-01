/** @file serverinfodialog.cpp  Information about a multiplayer server.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/dialogs/serverinfodialog.h"
#include "ui/dialogs/packageinfodialog.h"
#include "ui/widgets/packageswidget.h"
#include "ui/widgets/homemenuwidget.h"
#include "ui/widgets/mapoutlinewidget.h"
#include "network/serverlink.h"

#include <doomsday/Games>

#include <de/charsymbols.h>
#include <de/ButtonWidget>
#include <de/SequentialLayout>
#include <de/ui/SubwidgetItem>
#include <de/ProgressWidget>

#include <QTimer>

using namespace de;

DENG_GUI_PIMPL(ServerInfoDialog)
, DENG2_OBSERVES(ServerLink, MapOutline)
, public PackagesWidget::IPackageStatus
{
    // Server info & status.
    Address host;
    String domainName;
    GameProfile profile;
    shell::ServerInfo serverInfo;

    // Network queries.
    ServerLink link; // querying details from the server
    QTimer queryTimer; // allow the dialog to open nicely before starting network queries
    enum Query
    {
        QueryNone,
        QueryStatus,
        QueryMapOutline,
    };
    Query pendingQuery = QueryNone;

    // Widgets.
    LabelWidget *title;
    LabelWidget *subtitle;
    LabelWidget *description;
    PackagesWidget *packages = nullptr;
    MapOutlineWidget *mapOutline;
    LabelWidget *gameState;
    ui::ListData packageActions;

    Impl(Public *i, shell::ServerInfo const &sv)
        : Base(i)
        , serverInfo(sv)
        , link(ServerLink::ManualConnectionOnly)
    {
        link.audienceForMapOutline() += this;
        connect(&queryTimer, &QTimer::timeout, [this] () { beginPendingQuery(); });

        self().useInfoStyle();

        // The Close button is always available. Other actions are shown depending
        // on what kind of package is being displayed.
        self().buttons()
                << new DialogButtonItem(Default | Accept, tr("Close"));

        packageActions << new ui::SubwidgetItem(tr("..."), ui::Right, [this] () -> PopupWidget *
        {
             return new PackageInfoDialog(packages->actionPackage());
        });

        createWidgets();
    }

    bool isPackageHighlighted(String const &) const
    {
        // No highlights.
        return false;
    }

    void createWidgets()
    {
        auto &area = self().area();

        // Left column.
        title = LabelWidget::newWithText("", &area);
        title->setFont("title");
        title->setSizePolicy(ui::Filled, ui::Expand);
        title->setTextColor("inverted.accent");
        title->setTextLineAlignment(ui::AlignLeft);
        title->margins().setBottom("");

        subtitle = LabelWidget::newWithText("", &area);
        subtitle->setSizePolicy(ui::Filled, ui::Expand);
        subtitle->setTextColor("inverted.altaccent");
        subtitle->setTextLineAlignment(ui::AlignLeft);
        subtitle->margins().setTop("unit");

        description = LabelWidget::newWithText("", &area);
        description->setFont("small");
        description->setSizePolicy(ui::Filled, ui::Expand);
        description->setTextColor("inverted.text");
        description->setTextLineAlignment(ui::AlignLeft);

        mapOutline = new MapOutlineWidget;
        area.add(mapOutline);
        mapOutline->rule().setInput(Rule::Width, Const(2*4*90));

        gameState = LabelWidget::newWithText("", &area);
        gameState->setFont("small");
        gameState->setSizePolicy(ui::Filled, ui::Expand);
        gameState->setTextColor("inverted.altaccent");

        updateLayout();
    }

    void updateLayout()
    {
        auto &area = self().area();
        SequentialLayout layout(area.contentRule().left(),
                                area.contentRule().top(),
                                ui::Down);
        layout.setOverrideWidth(Const(2*4*90));
        layout << *title << *subtitle << *description;
        if (packages)
        {
            layout << *packages;
        }

        /*SequentialLayout rightLayout(title->rule().right(),
                                     title->rule().top(),
                                     ui::Down);
        rightLayout << *mapOutline;*/

        mapOutline->rule()
                .setInput(Rule::Height, layout.height() - gameState->rule().height())
                .setLeftTop(title->rule().right(), title->rule().top());

        gameState->rule()
                .setInput(Rule::Width,  mapOutline->rule().width())
                .setInput(Rule::Left,   mapOutline->rule().left())
                .setInput(Rule::Bottom, area.contentRule().bottom());

        area.setContentSize(layout.width() + mapOutline->rule().width(),
                            layout.height());
    }

    void updateContent(bool updatePackages = true)
    {
        title->setText(serverInfo.name());

        qDebug() << serverInfo.asText();

        // Title and description.
        {
            StringList lines;
            if (!domainName.isEmpty())
            {
                lines << _E(b) + domainName + _E(.) + String(" (%1)").arg(host.asText());
            }
            else
            {
                lines << _E(b) + host.asText() + _E(.);
            }
            if (!serverInfo.description().isEmpty())
            {
                lines << _E(A) + serverInfo.description() + _E(.);
            }
            subtitle->setText(String::join(lines, "\n"));
        }

        // Additional information.
        {
            String msg;
            msg = tr("Version: ") + serverInfo.version().asText() + "\n" +
                  tr("Rules: ")   + serverInfo.gameConfig() + "\n";
            auto const players = serverInfo.players();
            if (players.isEmpty())
            {
                msg += tr("No players.");
            }
            else
            {
                msg += tr("%1 connected player%2: ")
                        .arg(players.count())
                        .arg(DENG2_PLURAL_S(players.count()))
                        + String::join(players, ", ");
            }
            description->setText(msg);
        }

        // Game state.
        {
            String const gameId = serverInfo.gameId();
            String gameTitle = gameId;
            if (Games::get().contains(gameId))
            {
                gameTitle = Games::get()[gameId].title();
            }
            String msg = String(_E(b) "%1" _E(.) "\n%2 " DENG2_CHAR_MDASH " %3")
                        .arg(serverInfo.map())
                        .arg(serverInfo.gameConfig().containsWord("coop")? tr("Co-op")
                                                                         : tr("Deathmatch"))
                        .arg(gameTitle);
            gameState->setText(msg);
        }

        if (updatePackages && !serverInfo.packages().isEmpty())
        {
            //qDebug() << "updating with packages:" << serverInfo.packages();
            if (!packages)
            {
                packages = new PackagesWidget(serverInfo.packages());
                packages->setHiddenTags(StringList()); // show everything
                packages->setActionItems(packageActions);
                packages->setActionsAlwaysShown(true);
                packages->setPackageStatus(*this);
                packages->searchTermsEditor().setColorTheme(Inverted);
                packages->searchTermsEditor().setFont("small");
                packages->searchTermsEditor().setEmptyContentHint(tr("Filter Server Packages"), "editor.hint.small");
                packages->searchTermsEditor().margins().setTopBottom("dialog.gap");
                packages->setColorTheme(Inverted, Inverted, Inverted, Inverted);
                self().area().add(packages);

                updateLayout();
            }
            else
            {
                packages->setManualPackageIds(serverInfo.packages());
            }
        }
    }

    void startQuery(Query query)
    {
        pendingQuery = query;

        queryTimer.stop();
        queryTimer.setInterval(500);
        queryTimer.setSingleShot(true);
        queryTimer.start();
    }

    void beginPendingQuery()
    {
        Query const handling = pendingQuery;
        pendingQuery = QueryNone;

        switch (handling)
        {
        case QueryStatus:
            if (!domainName.isEmpty())
            {
                // Begin a query for the latest details.
                link.acquireServerProfile(domainName, [this] (Address resolvedAddress,
                                                              GameProfile const *svProfile)
                {
                    host = resolvedAddress;

                    qDebug() << "[domain] reply from" << host.asText();
                    link.foundServerInfo(0, serverInfo);
                    profile = *svProfile;
                    updateContent();

                    startQuery(QueryMapOutline);
                });
            }
            else
            {
                link.acquireServerProfile(host, [this] (GameProfile const *svProfile)
                {
                    qDebug() << "[ip] reply from" << host.asText();
                    link.foundServerInfo(0, serverInfo);
                    profile = *svProfile;
                    updateContent();

                    startQuery(QueryMapOutline);
                });
            }
            break;

        case QueryMapOutline:
            link.requestMapOutline(host);
            break;

        case QueryNone:
            break;
        }
    }

    void mapOutlineReceived(Address const &, shell::MapOutlinePacket const &packet)
    {
        mapOutline->setOutline(packet);
    }
};

ServerInfoDialog::ServerInfoDialog(shell::ServerInfo const &serverInfo)
    : d(new Impl(this, serverInfo))
{
    d->domainName = serverInfo.domainName();
    d->host       = serverInfo.address();

    d->updateContent(false /* don't update packages yet */);

    d->startQuery(Impl::QueryStatus);
}
