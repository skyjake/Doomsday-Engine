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
#include "network/serverlink.h"

#include <de/ButtonWidget>
#include <de/SequentialLayout>

using namespace de;

DENG_GUI_PIMPL(ServerInfoDialog)
{
    Address host;
    String domainName;
    ServerLink link; // querying details from the server
    GameProfile profile;
    shell::ServerInfo serverInfo;
    LabelWidget *title;
    LabelWidget *subtitle;
    LabelWidget *description;

    Impl(Public *i, shell::ServerInfo const &sv)
        : Base(i)
        , link(ServerLink::ManualConnectionOnly)
        , serverInfo(sv)
    {
        self().useInfoStyle();

        // The Close button is always available. Other actions are shown depending
        // on what kind of package is being displayed.
        self().buttons()
                << new DialogButtonItem(Default | Accept, tr("Close"));

        createWidgets();
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

        SequentialLayout layout(area.contentRule().left(),
                                area.contentRule().top(),
                                ui::Down);
        layout.setOverrideWidth(Const(2*4*90));
        layout << *title << *subtitle << *description;

        area.setContentSize(layout.width(), layout.height());
    }

    void updateContent()
    {
        qDebug() << "\n\nupdating with:\n" << serverInfo.asText().toLatin1().constData();

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
    }
};

ServerInfoDialog::ServerInfoDialog(shell::ServerInfo const &serverInfo)
    : d(new Impl(this, serverInfo))
{
    d->domainName = serverInfo.domainName();
    d->host       = serverInfo.address();

    d->updateContent();

    if (!d->domainName.isEmpty())
    {
        // Begin a query for the latest details.
        d->link.acquireServerProfile(d->domainName, [this] (Address resolvedAddress,
                                                            GameProfile const *profile)
        {
            d->host = resolvedAddress;

            qDebug() << "[domain] reply from" << d->host.asText();
            d->link.foundServerInfo(0, d->serverInfo);
            d->profile = *profile;
            d->updateContent();
        });
    }
    else
    {
        d->link.acquireServerProfile(d->host, [this] (GameProfile const *profile)
        {
            qDebug() << "[ip] reply from" << d->host.asText();
            d->link.foundServerInfo(0, d->serverInfo);
            d->profile = *profile;
            d->updateContent();
        });
    }
}
