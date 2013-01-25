/** @file shellapp.cpp Doomsday shell connection app.
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

#include "shellapp.h"
#include "logwidget.h"
#include "commandlinewidget.h"
#include "statuswidget.h"
#include <de/shell/Link>
#include <QStringList>

using namespace de;

struct ShellApp::Instance
{
    ShellApp &self;
    LogWidget *log;
    CommandLineWidget *cli;
    StatusWidget *status;
    shell::Link *link;

    Instance(ShellApp &a) : self(a), link(0)
    {
        RootWidget &root = self.rootWidget();

        status = new StatusWidget;
        status->rule()
                .setInput(RectangleRule::Height, refless(new ConstantRule(1)))
                .setInput(RectangleRule::Bottom, root.viewBottom())
                .setInput(RectangleRule::Width,  root.viewWidth())
                .setInput(RectangleRule::Left,   root.viewLeft());

        cli = new CommandLineWidget;
        cli->rule()
                .setInput(RectangleRule::Left,   root.viewLeft())
                .setInput(RectangleRule::Width,  root.viewWidth())
                .setInput(RectangleRule::Bottom, status->rule().top());

        log = new LogWidget;
        log->rule()
                .setInput(RectangleRule::Left,   root.viewLeft())
                .setInput(RectangleRule::Width,  root.viewWidth())
                .setInput(RectangleRule::Top,    root.viewTop())
                .setInput(RectangleRule::Bottom, cli->rule().top());

        // Ownership also given to the root widget:
        root.add(status);
        root.add(cli);
        root.add(log);

        root.setFocus(cli);

        QObject::connect(cli, SIGNAL(commandEntered(de::String)), &self, SLOT(sendCommandToServer(de::String)));
    }

    ~Instance()
    {
        delete link;
    }
};

ShellApp::ShellApp(int &argc, char **argv)
    : CursesApp(argc, argv), d(new Instance(*this))
{
    QStringList args = arguments();

    if(args.size() > 1)
    {
        // Open a connection.
        d->link = new shell::Link(Address(args[1]));
        d->status->setShellLink(d->link);
    }
}

ShellApp::~ShellApp()
{
    delete d;
}

void ShellApp::sendCommandToServer(String command)
{
    DENG2_UNUSED(command);

    // todo
}
