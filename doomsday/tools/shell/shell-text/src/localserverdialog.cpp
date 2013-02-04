/** @file localserverdialog.cpp  Dialog for starting a local server.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "persistentdata.h"
#include <de/shell/TextRootWidget>
#include <de/shell/LabelWidget>
#include <de/shell/MenuWidget>
#include <de/shell/ChoiceWidget>
#include <de/shell/LineEditWidget>

using namespace de;
using namespace de::shell;

struct LocalServerDialog::Instance
{
    ChoiceWidget *choice;
    LineEditWidget *port;
};

static struct
{
    char const *name;
    char const *mode;
}
gameModes[] =
{
    //{ "None",                                   "" },

    { "Shareware DOOM",                         "doom1-share" },
    { "DOOM",                                   "doom1" },
    { "Ultimate DOOM",                          "doom1-ultimate" },
    { "DOOM II",                                "doom2" },
    { "Final DOOM: Plutonia Experiment",        "doom2-plut" },
    { "Final DOOM: TNT Evilution",              "doom2-tnt" },
    { "Chex Quest",                             "chex" },
    { "HacX",                                   "hacx" },

    { "Shareware Heretic",                      "heretic-share" },
    { "Heretic",                                "heretic" },
    { "Heretic: Shadow of the Serpent Riders",  "heretic-ext" },

    { "Hexen v1.1",                             "hexen" },
    { "Hexen v1.0",                             "hexen-v10" },
    { "Hexen: Death Kings of Dark Citadel",     "hexen-dk" },
    { "Hexen Demo",                             "hexen-demo" },

    { 0, 0 }
};

LocalServerDialog::LocalServerDialog() : d(new Instance)
{
    d->choice = new ChoiceWidget("gameMode");
    d->port = new LineEditWidget("serverPort");
    add(d->choice);
    add(d->port);

    // Define the contents for the choice list.
    ChoiceWidget::Items modes;
    for(int i = 0; gameModes[i].name; ++i)
    {
        modes << gameModes[i].name;
    }
    d->choice->setItems(modes);
    d->choice->setPrompt(tr("Game mode: "));
    d->choice->setBackground(TextCanvas::Char(' ', TextCanvas::Char::Reverse));

    setFocusCycle(WidgetList() << d->choice << d->port << &lineEdit() << &menu());

    d->choice->rule()
            .setInput(Rule::Height, Const(1))
            .setInput(Rule::Width,  rule().width())
            .setInput(Rule::Left,   rule().left())
            .setInput(Rule::Top,    label().rule().bottom() + 1);

    d->port->setPrompt(tr("TCP port: "));

    d->port->rule()
            .setInput(Rule::Width,  Const(16))
            .setInput(Rule::Left,   rule().left())
            .setInput(Rule::Top,    d->choice->rule().bottom() + 1);

    lineEdit().rule()
            .setInput(Rule::Top,    d->port->rule().bottom() + 1);

    rule().setInput(Rule::Height,
                    label().rule()   .height() +
                    d->choice->rule().height() +
                    d->port->rule()  .height() +
                    lineEdit().rule().height() +
                    menu().rule()    .height() + 4);

    setDescription(tr("Specify the settings for starting a new local server."));

    setPrompt(tr("Options: "));

    setAcceptLabel(tr("Start local server"));

    // Values.
    d->choice->select (PersistentData::geti("LocalServer.gameMode"));
    d->port->setText  (PersistentData::get ("LocalServer.port", "13209"));
    lineEdit().setText(PersistentData::get ("LocalServer.options"));
}

LocalServerDialog::~LocalServerDialog()
{
    delete d;
}

duint16 LocalServerDialog::port() const
{
    return d->port->text().toInt();
}

String LocalServerDialog::gameMode() const
{
    return gameModes[d->choice->selection()].mode;
}

void LocalServerDialog::prepare()
{
    InputDialog::prepare();

    root().setFocus(d->choice);
}

void LocalServerDialog::finish(int result)
{
    InputDialog::finish(result);

    if(result)
    {
        PersistentData::set("LocalServer.gameMode", d->choice->selection());
        PersistentData::set("LocalServer.port", d->port->text());
        PersistentData::set("LocalServer.options", lineEdit().text());
    }
}
