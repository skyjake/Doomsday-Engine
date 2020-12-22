/** @file localserverdialog.cpp  Dialog for starting a local server.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "localserverdialog.h"
#include <de/term/textrootwidget.h>
#include <de/term/labelwidget.h>
#include <de/term/menuwidget.h>
#include <de/term/choicewidget.h>
#include <de/term/lineeditwidget.h>
#include <doomsday/doomsdayinfo.h>
#include <de/config.h>
#include <de/serverinfo.h>

using namespace de;
using namespace de::term;

DE_PIMPL_NOREF(LocalServerDialog)
{
    ChoiceWidget *  choice;
    LineEditWidget *port;
};

LocalServerDialog::LocalServerDialog() : d(new Impl)
{
    add(d->choice = new ChoiceWidget  ("gameMode"));
    add(d->port   = new LineEditWidget("serverPort"));

    // Define the contents for the choice list.
    ChoiceWidget::Items modes;
    for (const DoomsdayInfo::Game &game : DoomsdayInfo::allGames())
    {
        modes << game.title;
    }
    d->choice->setItems(modes);
    d->choice->setPrompt("Game mode: ");
    d->choice->setBackground(TextCanvas::AttribChar(' ', TextCanvas::AttribChar::Reverse));

    setFocusCycle({d->choice, d->port, &lineEdit(), &menu()});

    d->choice->rule()
            .setInput(Rule::Height, Const(1))
            .setInput(Rule::Width,  rule().width())
            .setInput(Rule::Left,   rule().left())
            .setInput(Rule::Top,    label().rule().bottom() + 1);

    d->port->setPrompt("TCP port: ");

    d->port->rule()
            .setInput(Rule::Width,  Const(16))
            .setInput(Rule::Left,   rule().left())
            .setInput(Rule::Top,    d->choice->rule().bottom() + 1);

    lineEdit().rule()
            .setInput(Rule::Top,    d->port->rule().bottom());

    rule().setInput(Rule::Height,
                    label().rule()   .height() +
                    d->choice->rule().height() +
                    d->port->rule()  .height() +
                    lineEdit().rule().height() +
                    menu().rule()    .height() + 3);

    setDescription("Specify the settings for starting a new local server.");

    setPrompt("Options: ");

    setAcceptLabel("Start local server");

    // Values.
    auto &cfg = Config::get();
    d->choice->select (cfg.geti("LocalServer.gameMode", 0));
    d->port->setText  (cfg.gets("LocalServer.port",     String::asText(DEFAULT_PORT)));
    lineEdit().setText(cfg.gets("LocalServer.options",  ""));
}

duint16 LocalServerDialog::port() const
{
    return duint16(d->port->text().toInt());
}

String LocalServerDialog::gameMode() const
{
    return DoomsdayInfo::allGames()[d->choice->selection()].option;
}

void LocalServerDialog::prepare()
{
    InputDialogWidget::prepare();
    root().setFocus(d->choice);
}

void LocalServerDialog::finish(int result)
{
    InputDialogWidget::finish(result);

    if (result)
    {
        auto &cfg = Config::get();
        cfg.set("LocalServer.gameMode", d->choice->selection());
        cfg.set("LocalServer.port",     d->port->text());
        cfg.set("LocalServer.options",  lineEdit().text());
    }
}
