/** @file multiplayerdialog.cpp  Dialog for listing found servers and joining games.
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

#include "ui/dialogs/multiplayerdialog.h"
#include "network/serverlink.h"
#include "clientapp.h"
#include "dd_main.h"
#include "con_main.h"
#include "CommandAction"
#include "ui/widgets/taskbarwidget.h"

#include <de/charsymbols.h>
#include <de/SignalAction>
#include <de/SequentialLayout>
#include <de/ChildWidgetOrganizer>
#include <de/DocumentWidget>
#include <de/ui/Item>

using namespace de;

DENG_GUI_PIMPL(MultiplayerDialog)
, DENG2_OBSERVES(ServerLink, DiscoveryUpdate)
, public ChildWidgetOrganizer::IWidgetFactory
{
    static String hostId(serverinfo_t const &sv)
    {
        return String("%1:%2").arg(sv.address).arg(sv.port);
    }

    class ServerListItem : public ui::Item
    {
    public:
        ServerListItem(serverinfo_t const &serverInfo)
        {
            setData(hostId(serverInfo));
            std::memcpy(&_info, &serverInfo, sizeof(serverInfo));
        }

        serverinfo_t const &info() const
        {
            return _info;
        }

        void setInfo(serverinfo_t const &serverInfo)
        {
            std::memcpy(&_info, &serverInfo, sizeof(serverInfo));
            notifyChange();
        }

    private:
        serverinfo_t _info;
    };

    /**
     * Widget representing a ServerListItem in the dialog's menu.
     */
    struct ServerWidget : public GuiWidget
    {
        LabelWidget *title;
        ButtonWidget *extra;
        ButtonWidget *join;
        QScopedPointer<SequentialLayout> layout;
        PopupWidget *popup;
        DocumentWidget *info;

        struct JoinAction : public Action
        {
        public:
            JoinAction(serverinfo_t const &sv, ButtonWidget *owner)
                : _owner(owner)
            {
                _gameId = sv.gameIdentityKey;
                _cmd = String("connect %1 %2").arg(sv.address).arg(sv.port);
            }

            void trigger()
            {
                Action::trigger();

                BusyMode_FreezeGameForBusyMode();

                // Closing the taskbar will cause this action to be deleted. Let's take
                // ownership of the action so we can delete after we're done.
                _owner->takeAction();

                ClientWindow::main().taskBar().close();

                App_ChangeGame(App_Games().byIdentityKey(_gameId), false /*no reload*/);
                Con_Execute(CMDS_DDAY, _cmd.toLatin1(), false, false);

                delete this;
            }

            Action *duplicate() const
            {
                DENG2_ASSERT(!"JoinAction: cannot duplicate");
                return 0;
            }

        private:
            ButtonWidget *_owner;
            String _gameId;
            String _cmd;
        };

        ServerWidget()
        {
            setBehavior(ContentClipping);

            add(title = new LabelWidget);
            add(extra = new ButtonWidget);
            add(join  = new ButtonWidget);

            extra->setText(tr("..."));
            join->setText(tr("Join"));

            title->setSizePolicy(ui::Expand, ui::Expand);
            title->setAppearanceAnimation(LabelWidget::AppearGrowVertically, 0.5);
            title->setAlignment(ui::AlignTop);
            title->setTextAlignment(ui::AlignRight);
            title->setTextLineAlignment(ui::AlignLeft);
            title->setImageAlignment(ui::AlignCenter);
            title->setMaximumTextWidth(style().rules().rule("dialog.multiplayer.width").valuei());

            extra->setSizePolicy(ui::Expand, ui::Expand);
            join->setSizePolicy(ui::Expand, ui::Expand);

            join->disable();

            layout.reset(new SequentialLayout(rule().left(), rule().top(), ui::Right));
            *layout << *title << *extra << *join;
            rule().setSize(layout->width(), title->rule().height());

            // Extra info popup.
            popup = new PopupWidget;
            popup->useInfoStyle();
            popup->setContent(info = new DocumentWidget);
            info->setMaximumLineWidth(style().rules().rule("dialog.multiplayer.width").valuei());
            popup->setAnchorAndOpeningDirection(extra->rule(), ui::Up);
            add(popup);

            extra->setAction(new SignalAction(popup, SLOT(open())));
        }

        void updateFromItem(ServerListItem const &item)
        {
            try
            {
                Game const &svGame = App_Games().byIdentityKey(item.info().gameIdentityKey);

                if(style().images().has(svGame.logoImageId()))
                {
                    title->setImage(style().images().image(svGame.logoImageId()));
                }

                serverinfo_t const &sv = item.info();

                join->enable(sv.canJoin);
                if(sv.canJoin)
                {
                    join->setAction(new JoinAction(sv, join));
                }

                title->setText(String(_E(1) "%1 " _E(.)_E(2) "(%5/%6)" _E(.) "\n%2"
                                      _E(D)_E(l) "\n%7 %4")
                               .arg(sv.name)
                               .arg(svGame.title())
                               .arg(sv.gameConfig)
                               .arg(sv.numPlayers)
                               .arg(sv.maxPlayers)
                               .arg(sv.map));

                // Extra information.
#define TABBED(A, B) _E(Ta)_E(l) "  " A _E(.) " " _E(\t) B "\n"
                info->setText(String(_E(b) "%1" _E(.) "\n%2\n" _E(T`)
                                     TABBED("Joinable:", "%5")
                                     TABBED("Players:", "%3 / %4%13")
                                     TABBED("Game:", "%9\n%10\n%12 %11")
                                     TABBED("PWADs:", "%14")
                                     TABBED("Address:", "%6:%7")
                                     TABBED("Ping:", "%8 ms (approx)"))
                              .arg(sv.name)
                              .arg(sv.description)
                              .arg(sv.numPlayers)
                              .arg(sv.maxPlayers)
                              .arg(sv.canJoin? "Yes" : "No") // 5
                              .arg(sv.address)
                              .arg(sv.port)
                              .arg(sv.ping)
                              .arg(sv.plugin)
                              .arg(sv.gameIdentityKey) // 10
                              .arg(sv.gameConfig)
                              .arg(sv.map)
                              .arg(!String(sv.clientNames).isEmpty()? String(_E(2) " (%1)" _E(.)).arg(sv.clientNames) : "")
                              .arg(String(sv.pwads).isEmpty()? String(DENG2_CHAR_MDASH) : String(sv.pwads))); // 14
#undef TABBED
            }
            catch(Error const &)
            {
                /// @todo
            }
        }
    };

    MenuWidget *list;

    Instance(Public *i) : Base(i)
    {
        self.area().add(list = new MenuWidget);

        // Configure the server list widet.
        list->setGridSize(1, ui::Expand, 0, ui::Expand);
        list->organizer().setWidgetFactory(*this);

        link().audienceForDiscoveryUpdate += this;
    }

    ~Instance()
    {
        link().audienceForDiscoveryUpdate -= this;
    }

    GuiWidget *makeItemWidget(ui::Item const &item, GuiWidget const *)
    {
        return new ServerWidget;
    }

    void updateItemWidget(GuiWidget &widget, ui::Item const &item)
    {
        widget.as<ServerWidget>().updateFromItem(item.as<ServerListItem>());
    }

    void linkDiscoveryUpdate(ServerLink const &link)
    {
        // Remove obsolete entries.
        for(ui::Data::Pos idx = 0; idx < list->items().size(); ++idx)
        {
            String const id = list->items().at(idx).data().toString();
            if(!link.isFound(Address::parse(id)))
            {
                list->items().remove(idx--);
            }
        }

        // Add new entries and update existing ones.
        foreach(de::Address const &host, link.foundServers())
        {
            serverinfo_t info;
            if(!link.foundServerInfo(host, &info)) continue;

            ui::Data::Pos found = list->items().findData(hostId(info));
            if(found == ui::Data::InvalidPos)
            {
                // Needs to be added.
                list->items().append(new ServerListItem(info));
            }
            else
            {
                // Update the info.
                list->items().at(found).as<ServerListItem>().setInfo(info);
            }
        }
    }

    static ServerLink &link()
    {
        return ClientApp::serverLink();
    }
};

MultiplayerDialog::MultiplayerDialog(String const &name)
    : DialogWidget(name, WithHeading), d(new Instance(this))
{
    heading().setText(tr("Multiplayer"));

    LabelWidget *lab = LabelWidget::newWithText(tr("Games from Master Server and local network:"), &area());

    GridLayout layout(area().contentRule().left(), area().contentRule().top());
    layout.setGridSize(1, 0);
    //layout.setColumnAlignment(0, ui::AlignRight);

    layout << *lab << *d->list;

    area().setContentSize(layout.width(), layout.height());

    buttons()
            << new DialogButtonItem(DialogWidget::Default | DialogWidget::Accept, tr("Close"))
            << new DialogButtonItem(DialogWidget::Action, style().images().image("gear"),
                                    new SignalAction(this, SLOT(showSettings())));
}

void MultiplayerDialog::showSettings()
{

}
