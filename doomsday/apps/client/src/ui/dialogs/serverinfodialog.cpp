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
#include <de/CallbackAction>
#include <de/PackageLoader>
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
    PopupWidget *serverPopup;
    PackagesWidget *serverPackages;
    MapOutlineWidget *mapOutline;
    LabelWidget *gameState;
    ui::ListData serverPackageActions;

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
                << new DialogButtonItem(Default | Accept, tr("Close"))
                << new DialogButtonItem(ActionPopup | Id1, style().images().image("package.icon"))
                << new DialogButtonItem(Action | Id2, style().images().image("package.icon"),
                                        new CallbackAction([this] () { openLocalPackagesPopup(); }));

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
        //description->setFont("small");
        description->setSizePolicy(ui::Filled, ui::Expand);
        description->setTextColor("inverted.text");
        description->setTextLineAlignment(ui::AlignLeft);

        // Right column.

        LabelWidget *bg = new LabelWidget;
        bg->set(Background(Vector4f(style().colors().colorf("inverted.altaccent"), .1f),
                           Background::GradientFrameWithRoundedFill,
                           Vector4f(), 8));
        area.add(bg);

        mapOutline = new MapOutlineWidget;
        area.add(mapOutline);
        mapOutline->rule().setInput(Rule::Width, rule("dialog.serverinfo.mapoutline.width"));
        mapOutline->margins().set(rule("gap") * 2).setBottom("gap");

        gameState = LabelWidget::newWithText("", &area);
        //gameState->setFont("small");
        gameState->setSizePolicy(ui::Filled, ui::Expand);
        gameState->setTextColor("inverted.altaccent");
        gameState->margins().setBottom(mapOutline->margins().top());

        bg->rule().setRect(mapOutline->rule())
                  .setInput(Rule::Bottom, gameState->rule().bottom());

        serverPackageActions << new ui::SubwidgetItem(tr("..."), ui::Right, [this] () -> PopupWidget *
        {
             return new PackageInfoDialog(serverPackages->actionPackage());
        });

        // Popups.

        serverPopup = new PopupWidget;
        self().add(serverPopup);
        serverPackages = new PackagesWidget(PackagesWidget::PopulationDisabled);
        serverPackages->margins().set("gap");
        serverPackages->setHiddenTags(StringList()); // show everything
        serverPackages->setActionItems(serverPackageActions);
        serverPackages->setActionsAlwaysShown(true);
        serverPackages->setPackageStatus(*this);
        serverPackages->searchTermsEditor().setEmptyContentHint(tr("Filter Server Packages"));
        serverPackages->rule().setInput(Rule::Width, rule("dialog.serverinfo.popup.width"));
        serverPopup->setContent(serverPackages);

        auto *svBut = self().popupButtonWidget(Id1);
        svBut->setPopup(*serverPopup);
        svBut->setTextAlignment(ui::AlignLeft);

        updateLayout();
    }

    void updateLayout()
    {
        auto &area = self().area();
        SequentialLayout layout(area.contentRule().left(),
                                area.contentRule().top(),
                                ui::Down);
        layout.setOverrideWidth(rule("dialog.serverinfo.description.width"));
        layout << *title << *subtitle << *description;

        AutoRef<Rule> height(OperatorRule::maximum
                             (layout.height(), rule("dialog.serverinfo.content.minheight")));

        mapOutline->rule()
                .setInput(Rule::Height, height - gameState->rule().height())
                .setLeftTop(title->rule().right(), title->rule().top());

        gameState->rule()
                .setInput(Rule::Width,  mapOutline->rule().width())
                .setInput(Rule::Left,   mapOutline->rule().left())
                .setInput(Rule::Bottom, area.contentRule().bottom());

        area.setContentSize(layout.width() + mapOutline->rule().width(), height);
    }

    void updateContent()
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
                lines << "\n" _E(A) + serverInfo.description() + _E(.);
            }
            subtitle->setText(String::join(lines, "\n"));
        }

        // Additional information.
        {
            auto const players = serverInfo.players();
            String plrDesc;
            if (players.isEmpty())
            {
                plrDesc = DENG2_CHAR_MDASH;
            }
            else
            {
                plrDesc = String("%1 " DENG2_CHAR_MDASH " %2")
                        .arg(players.count())
                        .arg(String::join(players, ", "));
            }
            String msg = String(_E(Ta)_E(l) "%1:" _E(.)_E(Tb) " %2\n"
                                _E(Ta)_E(l) "%3:" _E(.)_E(Tb) " %4\n"
                                _E(Ta)_E(l) "%5:" _E(.)_E(Tb) " %6")
                    .arg(tr("Rules"))  .arg(serverInfo.gameConfig())
                    .arg(tr("Players")).arg(plrDesc)
                    .arg(tr("Version")).arg(serverInfo.version().asText());
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
            String msg = String(_E(b) "%1" _E(.)_E(s) "\n%2 " DENG2_CHAR_MDASH " %3")
                        .arg(serverInfo.map())
                        .arg(serverInfo.gameConfig().containsWord("coop")? tr("Co-op")
                                                                         : tr("Deathmatch"))
                        .arg(gameTitle);
            gameState->setText(msg);
        }

        if (!serverInfo.packages().isEmpty())
        {
            // Check which of the packages are locally available.
            StringList available;
            StringList missing;
            foreach (String pkgId, serverInfo.packages())
            {
                if (PackageLoader::get().select(pkgId))
                {
                    available << pkgId;
                }
                else
                {
                    auto const id_ver = Package::split(pkgId);
                    pkgId = String("%1 (%2)").arg(id_ver.first).arg(id_ver.second.asText());
                    Version localVersion;
                    if (id_ver.second.isValid())
                    {
                        // Perhaps we have another version of this package?
                        if (auto *pkgFile = PackageLoader::get().select(id_ver.first))
                        {
                            localVersion = Package::versionForFile(*pkgFile);
                            missing << String("%1 " _E(s) "(you have: %2)" _E(.))
                                       .arg(pkgId).arg(localVersion.asText());
                            continue;
                        }
                    }
                    missing << pkgId;
                }
            }

            if (missing.size() > 0)
            {
                description->setText(description->text() +
                                     _E(T`) "\n\n" _E(b) "Missing packages:" _E(.) "\n- " _E(>) +
                                     String::join(missing, _E(<) "\n- " _E(>)));
            }

            serverPackages->setPopulationEnabled(true);
            serverPackages->setManualPackageIds(available);

            self().buttonWidget(Id1)->setText(tr("Server: %1").arg(serverInfo.packages().size()));
        }
    }

    void openServerPackagesPopup()
    {
        serverPopup->openOrClose();
    }

    void openLocalPackagesPopup()
    {

    }

//- Queries to the server ---------------------------------------------------------------

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

                    //qDebug() << "[domain] reply from" << host.asText();
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
                    //qDebug() << "[ip] reply from" << host.asText();
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

    d->updateContent();

    d->startQuery(Impl::QueryStatus);
}
