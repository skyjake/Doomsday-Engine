/** @file testapp.cpp  Test application.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "mainwindow.h"

#include <de/dscript.h>
#include <de/filesystem.h>
#include <de/packageloader.h>
#include <de/windowsystem.h>

using namespace de;

DE_PIMPL(TestApp)
{
    ImageBank images;

    Impl(Public *i) : Base(i) {}

    ~Impl()
    {
        // Windows will be closed; OpenGL context will be gone.
        self().glDeinit();
    }

    void loadAllShaders()
    {
        // Load all the shader program definitions.
        FS::FoundFiles found;
        self().findInPackages("shaders.dei", found);
        DE_FOR_EACH(FS::FoundFiles, i, found)
        {
            LOG_MSG("Loading shader definitions from %s") << (*i)->description();
            self().shaders().addFromInfo(**i);
        }
    }
};

TestApp::TestApp(const StringList &args)
    : BaseGuiApp(args)
    , d(new Impl(this))
{
    setMetadata("Deng Team", "dengine.net", "Application Framework Test", "1.0");
    setUnixHomeFolderName(".test_appfw");
}

void TestApp::initialize()
{
    addInitPackage("net.dengine.test.appfw");
    initSubsystems();

    // Create subsystems.
    windowSystem().style().load(packageLoader().package("net.dengine.stdlib.gui"));

    d->loadAllShaders();

    // Also load images.
    d->images.addFromInfo(rootFolder().locate<File>("/packs/net.dengine.test.appfw/images.dei"));

    // Create the window.
    auto *win = windowSystem().newWindow<MainWindow>("main");

    scriptSystem().importModule("bootstrap");

    win->show();
}

void TestApp::createAnotherWindow()
{
    if (!windowSystem().findWindow("extra"))
    {
        windowSystem().newWindow<MainWindow>("extra")->show();
    }
}

TestApp &TestApp::app()
{
    return *static_cast<TestApp *>(DE_APP);
}

MainWindow &TestApp::mainWindow()
{
    return windowSystem().getMain().as<MainWindow>();
}

ImageBank &TestApp::images()
{
    return app().d->images;
}
