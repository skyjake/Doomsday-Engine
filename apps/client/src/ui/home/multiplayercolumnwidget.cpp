/** @file multiplayercolumnwidget.cpp
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

#include "ui/home/multiplayercolumnwidget.h"
#include "ui/widgets/multiplayerservermenuwidget.h"
#include "ui/widgets/taskbarwidget.h"
#include "ui/home/headerwidget.h"
#include "ui/clientwindow.h"
#include "ui/clientstyle.h"
#include "network/serverlink.h"

#include <doomsday/games.h>
#include <de/callbackaction.h>
#include <de/popupbuttonwidget.h>
#include <de/popupmenuwidget.h>
#include <de/ui/subwidgetitem.h>

using namespace de;

DE_GUI_PIMPL(MultiplayerColumnWidget)
, DE_OBSERVES(ui::Data, Addition)
, DE_OBSERVES(ui::Data, Removal)
{
    MultiplayerServerMenuWidget *menu;
    LabelWidget *noServers;

    Impl(Public *i) : Base(i)
    {
        // Set up the widgets.
        ScrollAreaWidget &area = self().scrollArea();
        area.add(menu = new MultiplayerServerMenuWidget);

        self().header().menuButton().setPopup([] (const PopupButtonWidget &) -> PopupWidget * {
            auto *menu = new PopupMenuWidget;
            menu->items()
                    << new ui::ActionItem("Connect to Server...",
                                          []() { ClientWindow::main().taskBar().connectToServerManually(); })
                    << new ui::ActionItem("Refresh List", []() {
                            ServerLink::get().discoverUsingMaster(); });
            return menu;
        }, ui::Down);

        menu->rule()
                .setInput(Rule::Width, area.contentRule().width())
                .setInput(Rule::Left,  area.contentRule().left())
                .setInput(Rule::Top,   self().header().rule().bottom());

        // Empty content label.
        noServers = &self().addNew<LabelWidget>();
        style().emptyContentLabelStylist().applyStyle(*noServers);
        noServers->setText("No Servers Found");
        noServers->rule().setRect(self().rule());

        menu->items().audienceForAddition() += this;
        menu->items().audienceForRemoval() += this;
    }

    void dataItemAdded(ui::DataPos, const ui::Item &) override
    {
        noServers->hide();
    }

    void dataItemRemoved(ui::DataPos, ui::Item &) override
    {
        if (menu->items().isEmpty())
        {
            noServers->show();
        }
    }
};

MultiplayerColumnWidget::MultiplayerColumnWidget()
    : ColumnWidget("multiplayer-column")
    , d(new Impl(this))
{
    scrollArea().setContentSize(maximumContentWidth(),
                                header().rule().height() +
                                rule("gap") +
                                d->menu->rule().height());

    header().title().setText(_E(s)_E(C) "dengine.net\n" _E(.)_E(.)_E(w) "Multiplayer Games");
    header().info().setText("Multiplayer servers are discovered via the dengine.net "
                            "master server and by broadcasting on the local network.");
}

String MultiplayerColumnWidget::tabHeading() const
{
    return DE_STR("Multiplayer");
}

int MultiplayerColumnWidget::tabShortcut() const
{
    return 'm';
}

String MultiplayerColumnWidget::configVariableName() const
{
    return DE_STR("home.columns.multiplayer");
}

void MultiplayerColumnWidget::setHighlighted(bool highlighted)
{
    ColumnWidget::setHighlighted(highlighted);

    if (highlighted)
    {
        d->menu->restorePreviousSelection();
    }
    else
    {
        root().setFocus(nullptr);
        d->menu->unselectAll();
    }
}
