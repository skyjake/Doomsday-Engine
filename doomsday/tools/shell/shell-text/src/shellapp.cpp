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

using namespace de;

struct ShellApp::Instance
{
    ShellApp &self;
    LogWidget *log;
    CommandLineWidget *cli;
    ConstantRule *cliHeight;

    Instance(ShellApp &a) : self(a)
    {
        RootWidget &root = self.rootWidget();

        // Initial height of the command line (1 row).
        cliHeight = new ConstantRule(1);

        cli = new CommandLineWidget;
        cli->rule()
                .setInput(RectangleRule::Left,   root.viewLeft())
                .setInput(RectangleRule::Width,  root.viewWidth())
                .setInput(RectangleRule::Bottom, root.viewBottom())
                .setInput(RectangleRule::Height, cliHeight);

        log = new LogWidget;
        log->rule()
                .setInput(RectangleRule::Left,   root.viewLeft())
                .setInput(RectangleRule::Width,  root.viewWidth())
                .setInput(RectangleRule::Top,    root.viewTop())
                .setInput(RectangleRule::Bottom, cli->rule().top());

        root.add(cli); // takes ownership
        root.add(log); // takes ownership
    }

    ~Instance()
    {
        releaseRef(cliHeight);
    }
};

ShellApp::ShellApp(int &argc, char **argv)
    : CursesApp(argc, argv), d(new Instance(*this))
{}

ShellApp::~ShellApp()
{
    delete d;
}

