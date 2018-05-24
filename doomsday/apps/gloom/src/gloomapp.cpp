/** @file gloomapp.cpp  Test application.
 *
 * @authors Copyright (c) 2014-2018 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "gloomapp.h"
#include "editorwindow.h"
#include "appwindowsystem.h"
#include "../gloom/gloomworld.h"
#include "../gloom/gloomwidget.h"
#include "../gloom/world/user.h"

#include <de/DisplayMode>
#include <de/FileSystem>
#include <de/PackageLoader>
#include <de/ScriptSystem>
#include <doomsday/DataBundle>

using namespace de;
using namespace gloom;

DENG2_PIMPL(GloomApp)
{
    ImageBank                        images;
    std::unique_ptr<EditorWindow>    editWin;
    std::unique_ptr<AppWindowSystem> winSys;
    std::unique_ptr<AudioSystem>     audioSys;
    std::unique_ptr<GloomWorld>      world;

    Impl(Public *i) : Base(i)
    {
        // We will be accessing data bundles (WADs).
        static DataBundle::Interpreter intrpDataBundle;
        FileSystem::get().addInterpreter(intrpDataBundle);
    }

    ~Impl()
    {
        // Windows will be closed; OpenGL context will be gone.
        // Deinitalize everything.
        winSys->main().glActivate();
        world->glDeinit();
        world.reset();

        self().glDeinit();
    }

    void loadAllShaders()
    {
        // Load all the shader program definitions.
        FS::FoundFiles found;
        self().findInPackages("shaders.dei", found);
        DENG2_FOR_EACH(FS::FoundFiles, i, found)
        {
            LOG_MSG("Loading shader definitions from %s") << (*i)->description();
            self().shaders().addFromInfo(**i);
        }
    }
};

GloomApp::GloomApp(int &argc, char **argv)
    : BaseGuiApp(argc, argv), d(new Impl(this))
{
    setMetadata("Deng Team", "dengine.net", "Gloom Test", "1.0");
    setUnixHomeFolderName(".gloom");
}

void GloomApp::initialize()
{
    d->world.reset(new GloomWorld);

    // Set up the editor.
    {
        d->editWin.reset(new EditorWindow);
        d->editWin->show();
        d->editWin->raise();

        connect(&d->editWin->editor(), &Editor::buildMapRequested, [this]() {
            try
            {
                auto &fs  = FS::get();
                auto &pld = PackageLoader::get();

                AppWindowSystem::main().glActivate();

                // Unload the previous map pacakge. This will make the assets in the package
                // unavailable.
                pld.unload("user.editorproject");

                // TODO: (Re)export the map package. Requires that the user has chosen
                // the project package, which may contain multiple maps.

                // Load the exported package and the contained assets.
                pld.load("user.editorproject");

                // Load the map.
                Map loadedMap;
                {
                    const auto &asset = App::asset(d->editWin->editor().mapId());
                    loadedMap.deserialize(FS::locate<const File>(asset.absolutePath("path")));
                    //loadedMap.setMetersPerUnit(vectorFromValue<Vec3d>(asset.get("metersPerUnit"));
                }

                d->world->setMap(loadedMap);
            }
            catch (const Error &er)
            {
                qWarning() << "Map build error:" << er.asText();
            }
        });
    }

    DisplayMode_Init();
    addInitPackage("net.dengine.gloom");
    initSubsystems(App::DisablePlugins);

    // Create subsystems.
    {
        d->winSys.reset(new AppWindowSystem);
        addSystem(*d->winSys);

        d->audioSys.reset(new AudioSystem);
        addSystem(*d->audioSys);
    }

    d->loadAllShaders();

    // Load resource banks.
    {
        const Package &base = App::packageLoader().package("net.dengine.gloom");
        d->images  .addFromInfo(base.root().locate<File>("images.dei"));
        waveforms().addFromInfo(base.root().locate<File>("audio.dei"));
    }

    // Create the window.
    MainWindow *win = d->winSys->newWindow<MainWindow>("main");

    win->root().find("gloomwidget")->as<GloomWidget>().setWorld(d->world.get());

    scriptSystem().importModule("bootstrap");
    win->show();
}

QDir GloomApp::userDir() const
{
    const QDir home = QDir::home();
    const QDir dir = home.filePath(unixHomeFolderName());
    if (!dir.exists())
    {
        home.mkdir(unixHomeFolderName());
    }
    return dir;
}

GloomApp &GloomApp::app()
{
    return *static_cast<GloomApp *>(DENG2_APP);
}

AppWindowSystem &GloomApp::windowSystem()
{
    return *app().d->winSys;
}

AudioSystem &GloomApp::audioSystem()
{
    return *app().d->audioSys;
}

MainWindow &GloomApp::main()
{
    return windowSystem().main();
}

ImageBank &GloomApp::images()
{
    return app().d->images;
}
