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

using namespace de;

struct ShellApp::Instance
{
    ShellApp &self;
    LogWidget *logWidget;

    Instance(ShellApp &a) : self(a)
    {
        RootWidget &root = self.rootWidget();

        logWidget = new LogWidget;

        ScalarRule *anim = refless(new ScalarRule(0));
        anim->set(refless(new OperatorRule(
                OperatorRule::Divide, root.viewWidth(), refless(new ConstantRule(2)))));
        /*anim->set(refless(new ConstantRule(10)), 2);*/

        logWidget->rule()
                .setInput(RectangleRule::Left,   anim)
                .setInput(RectangleRule::Top,    refless(new ConstantRule(0)))
                .setInput(RectangleRule::Width,  root.viewWidth())
                .setInput(RectangleRule::Height, root.viewHeight());

        root.add(logWidget); // takes ownership
    }
};

ShellApp::ShellApp(int &argc, char **argv) : CursesApp(argc, argv), d(new Instance(*this))
{}

ShellApp::~ShellApp()
{
    delete d;
}

