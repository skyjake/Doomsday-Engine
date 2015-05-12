/** @file testapp.cpp  Test application.
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

#include "testapp.h"
#include "appwindowsystem.h"

#include <de/DisplayMode>

using namespace de;

DENG2_PIMPL(TestApp)
{
    QScopedPointer<AppWindowSystem> winSys;
    ImageBank images;

    Instance(Public *i) : Base(i) {}

    void loadAllShaders()
    {
        // Load all the shader program definitions.
        FS::FoundFiles found;
        self.findInPackages("shaders.dei", found);
        DENG2_FOR_EACH(FS::FoundFiles, i, found)
        {
            LOG_MSG("Loading shader definitions from %s") << (*i)->description();
            self.shaders().addFromInfo(**i);
        }
    }
};

TestApp::TestApp(int &argc, char **argv)
    : BaseGuiApp(argc, argv), d(new Instance(this))
{
    setMetadata("Deng Team", "dengine.net", "Application Framework Test", "1.0");
    setUnixHomeFolderName(".test_appfw");
}

void TestApp::initialize()
{
    DisplayMode_Init();
    addInitPackage("net.dengine.test.appfw");
    initSubsystems(App::DisablePlugins);

    // Create subsystems.
    d->winSys.reset(new AppWindowSystem);
    addSystem(*d->winSys);

    d->loadAllShaders();

    // Also load images.
    d->images.addFromInfo(rootFolder().locate<File>("/packs/net.dengine.test.appfw/images.dei"));

    // Create the window.
    MainWindow *win = d->winSys->newWindow<MainWindow>("main");

    scriptSystem().importModule("bootstrap");

    win->show();
}

TestApp &TestApp::app()
{
    return *static_cast<TestApp *>(DENG2_APP);
}

AppWindowSystem &TestApp::windowSystem()
{
    return *app().d->winSys;
}

MainWindow &TestApp::main()
{
    return windowSystem().main();
}

ImageBank &TestApp::images()
{
    return app().d->images;
}
