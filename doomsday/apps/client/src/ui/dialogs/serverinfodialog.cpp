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

#include <de/ButtonWidget>
#include <de/SequentialLayout>
#include <de/ui/SubwidgetItem>
#include <de/ProgressWidget>

#include <QTimer>

using namespace de;

DENG_GUI_PIMPL(ServerInfoDialog)
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
    ui::ListData packageActions;

    Impl(Public *i, shell::ServerInfo const &sv)
        : Base(i)
        , serverInfo(sv)
        , link(ServerLink::ManualConnectionOnly)
    {
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
        mapOutline->rule().setInput(Rule::Width, Const(2*4*90));
        area.add(mapOutline);

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
                .setInput(Rule::Height, layout.height())
                .setLeftTop(title->rule().right(), title->rule().top());

        area.setContentSize(layout.width() + mapOutline->rule().width(),
                            layout.height());
                            //OperatorRule::maximum(layout.height());
                                                  //rightLayout.height()));
    }

    void updateContent(bool updatePackages = true)
    {
        title->setText(serverInfo.name());

        // Title and description.
        {
            StringList lines;
            if (!domainName.isEmpty())
            {
                lines << _E(b) + domainName + _E(.) +
                         String(" (%1)").arg(host.asText());
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

        if (updatePackages && !serverInfo.packages().isEmpty())
        {
            qDebug() << "updating with packages:" << serverInfo.packages();
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
        switch (pendingQuery)
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
                });
            }
            break;

        case QueryMapOutline:
            break;

        case QueryNone:
            break;
        }

        pendingQuery = QueryNone;
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
