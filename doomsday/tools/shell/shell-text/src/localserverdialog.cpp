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
#include <de/shell/DoomsdayInfo>

using namespace de;
using namespace de::shell;

struct LocalServerDialog::Instance
{
    ChoiceWidget *choice;
    LineEditWidget *port;
};

LocalServerDialog::LocalServerDialog() : d(new Instance)
{
    d->choice = new ChoiceWidget("gameMode");
    d->port = new LineEditWidget("serverPort");
    add(d->choice);
    add(d->port);

    // Define the contents for the choice list.
    ChoiceWidget::Items modes;
    foreach(DoomsdayInfo::GameMode const &mode, DoomsdayInfo::allGameModes())
    {
        modes << mode.title;
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
            .setInput(Rule::Top,    d->port->rule().bottom());

    rule().setInput(Rule::Height,
                    label().rule()   .height() +
                    d->choice->rule().height() +
                    d->port->rule()  .height() +
                    lineEdit().rule().height() +
                    menu().rule()    .height() + 3);

    setDescription(tr("Specify the settings for starting a new local server."));

    setPrompt(tr("Options: "));

    setAcceptLabel(tr("Start local server"));

    // Values.
    d->choice->select (PersistentData::geti("LocalServer/gameMode"));
    d->port->setText  (PersistentData::get ("LocalServer/port", "13209"));
    lineEdit().setText(PersistentData::get ("LocalServer/options"));
}

duint16 LocalServerDialog::port() const
{
    return d->port->text().toInt();
}

String LocalServerDialog::gameMode() const
{
    return DoomsdayInfo::allGameModes()[d->choice->selection()].option;
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
        PersistentData::set("LocalServer/gameMode", d->choice->selection());
        PersistentData::set("LocalServer/port",     d->port->text());
        PersistentData::set("LocalServer/options",  lineEdit().text());
    }
}
