/** @file multiplayercolumnwidget.cpp
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

#include "ui/home/multiplayercolumnwidget.h"
#include "ui/widgets/multiplayerservermenuwidget.h"
#include "ui/widgets/taskbarwidget.h"
#include "ui/home/headerwidget.h"
#include "ui/clientwindow.h"
#include "clientapp.h"

#include <doomsday/Games>
#include <de/PopupButtonWidget>
#include <de/PopupMenuWidget>
#include <de/SignalAction>
#include <de/ui/SubwidgetItem>

using namespace de;

DENG_GUI_PIMPL(MultiplayerColumnWidget)
{
    MultiplayerServerMenuWidget *menu;

    Impl(Public *i) : Base(i)
    {
        // Set up the widgets.
        ScrollAreaWidget &area = self.scrollArea();
        area.add(menu = new MultiplayerServerMenuWidget);

        self.header().menuButton().setPopup([] (PopupButtonWidget const &) -> PopupWidget * {
            auto *menu = new PopupMenuWidget;
            menu->items() << new ui::ActionItem(tr("Connect to Server..."),
                                                new SignalAction(&ClientWindow::main().taskBar(),
                                                                 SLOT(connectToServerManually())));
            return menu;
        }, ui::Down);

        menu->rule()
                .setInput(Rule::Width, area.contentRule().width())
                .setInput(Rule::Left,  area.contentRule().left())
                .setInput(Rule::Top,   self.header().rule().bottom());
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

    header().title().setText(_E(s)_E(C) "dengine.net\n" _E(.)_E(.)_E(w) + tr("Multiplayer Games"));
    header().info().setText(tr("Multiplayer servers are discovered via the dengine.net "
                               "master server and broadcasting on the local network."));
}

String MultiplayerColumnWidget::tabHeading() const
{
    return tr("Multiplayer");
}

String MultiplayerColumnWidget::configVariableName() const
{
    return "home.columns.multiplayer";
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
