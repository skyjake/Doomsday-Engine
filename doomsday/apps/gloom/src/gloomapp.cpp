
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
#include "mainwindow.h"
#include "gloomwidget.h"

#include <gloom/world.h>
#include <gloom/world/user.h>
#include <gloom/world/map.h>

#include <doomsday/res/databundle.h>

#include <de/beacon.h>
#include <de/directoryfeed.h>
#include <de/dscript.h>
#include <de/filesystem.h>
#include <de/info.h>
#include <de/packageloader.h>
#include <de/windowsystem.h>

#include <the_Foundation/datagram.h>

using namespace de;
using namespace gloom;

static const duint16 COMMAND_PORT = 14666;

DE_PIMPL(GloomApp)
{
    ImageBank                    images;
    tF::ref<iDatagram>           commandSocket;
    Beacon                       beacon{{COMMAND_PORT, COMMAND_PORT + 4}};
    std::unique_ptr<AudioSystem> audioSys;
    std::unique_ptr<World>       world;
    String                       currentMap;

    Impl(Public *i) : Base(i)
    {
        // We will be accessing data bundles (WADs).
        static DataBundle::Interpreter intrpDataBundle;
        FileSystem::get().addInterpreter(intrpDataBundle);

        // GloomEd will tell us what to do via the command socket.
        {
            commandSocket = tF::make_ref(new_Datagram());
            setUserData_Object(commandSocket, this);
            iConnect(Datagram, commandSocket, message, commandSocket, receivedRemoteCommand);
            for (int attempt = 0; attempt < 12; ++attempt)
            {
                if (open_Datagram(commandSocket, duint16(COMMAND_PORT + 4 + attempt)))
                {
                    LOG_NET_NOTE("Listening to commands on port %u") << port_Datagram(commandSocket);
                    break;
                }
            }
            if (!isOpen_Datagram(commandSocket))
            {
                throw Error("GloomApp::GloomApp",
                            "Failed to open socket for listening to commands from GloomEd");
            }
            beacon.setMessage(Stringf("GloomApp: port=%u", port_Datagram(commandSocket)));
            beacon.start();
        }
    }

    ~Impl()
    {
        // Windows will be closed; OpenGL context will be gone. Deinitialize everything.
        if (auto *win = self().windowSystem().mainPtr())
        {
            win->glActivate();
            world->glDeinit();
        }
        world.reset();

        self().glDeinit();
    }

    static void receivedRemoteCommand(iAny *, iDatagram *socket)
    {
        auto *d = reinterpret_cast<Impl *>(userData_Object(socket));
        while (auto msgData = Block::take(receive_Datagram(socket, nullptr)))
        {
            Loop::mainCall([d, msgData]() {
                const Info msg(msgData);
                for (const auto *elem : msg.root().contentsInOrder())
                {
                    if (elem->isBlock())
                    {
                        const auto &block = elem->as<Info::BlockElement>();
                        if (block.blockType() == "command")
                        {
                            if (block.name() == "loadmap")
                            {
                                d->loadMapPackage(block["map"], block["package"], block["nativePath"]);
                            }
                        }
                    }
                }
            });
        }
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

    void loadMapPackage(const String &mapId, const String &packageId, const NativePath &location)
    {
        debug("load map '%s' from package '%s' in '%s'",
              mapId.c_str(),
              packageId.c_str(),
              location.c_str());

        if (!mapId || !packageId || location.isEmpty() || !location.exists()) return;

        auto &pld = PackageLoader::get();

        if (currentMap)
        {
            pld.unload(currentMap);
            pld.refresh();
            currentMap.clear();
        }

        FS::get().makeFolderWithFeed("/remote/gloom", new DirectoryFeed(location));

        pld.load(packageId);
        pld.refresh();
        currentMap = packageId;

        world->loadMap(mapId);
    }
};

GloomApp::GloomApp(const StringList &args)
    : BaseGuiApp(args)
    , d(new Impl(this))
{
    setMetadata("Deng Team", "dengine.net", "Gloom Test", "1.0");
    setUnixHomeFolderName(".gloom");
}

void GloomApp::initialize()
{
    using gloom::Map;

    d->world.reset(new World(shaders(), images()));

#if 0
    // Set up the editor.
    {
        d->editWin.reset(new EditorWindow);
        d->editWin->show();
        d->editWin->raise();

        QObject::connect(&d->editWin->editor(), &Editor::buildMapRequested, [this]() {
            try
            {
                AppWindowSystem::main().glActivate();

                auto &pld = PackageLoader::get();

                // Unload the previous map pacakge. This will make the assets in the package
                // unavailable.
                pld.unload("user.editorproject");
                pld.refresh();

                // TODO: (Re)export the map package. Requires that the user has chosen
                // the project package, which may contain multiple maps.

                // Load the exported package and the contained assets.
                pld.load("user.editorproject");
                pld.refresh();

                // Load the map.
                Map loadedMap;
                {
                    const auto &asset = App::asset(d->editWin->editor().mapId());
                    loadedMap.deserialize(FS::locate<const File>(asset.absolutePath("path")));
                }
                d->world->setMap(loadedMap);
            }
            catch (const Error &er)
            {
                warning("Map build error: %s", er.asPlainText().c_str());
            }
        });
    }
#endif

    addInitPackage("net.dengine.gloom");
    addInitPackage("net.dengine.gloom.test");

    initSubsystems();

    // Create subsystems.
    {
        windowSystem().style().load(packageLoader().load("net.dengine.gloom.test.defaultstyle"));

        d->audioSys.reset(new AudioSystem);
        addSystem(*d->audioSys);
    }

    d->loadAllShaders();

    // Load resource banks.
    {
        const Package &base = packageLoader().package("net.dengine.gloom.test");
        d->images  .addFromInfo(base.root().locate<File>("images.dei"));
        waveforms().addFromInfo(base.root().locate<File>("audio.dei"));
    }

    // Create the window.
    MainWindow *win = windowSystem().newWindow<MainWindow>("main");

    win->root().find("gloomwidget")->as<GloomWidget>().setWorld(d->world.get());

    scriptSystem().importModule("bootstrap");
    win->show();
}

NativePath GloomApp::userDir() const
{
    const auto dir = NativePath::homePath() / unixHomeFolderName();
    if (!dir.exists())
    {
        dir.create();
    }
    return dir;
}

GloomApp &GloomApp::app()
{
    return *static_cast<GloomApp *>(DE_APP);
}

AudioSystem &GloomApp::audioSystem()
{
    return *app().d->audioSys;
}

MainWindow &GloomApp::mainWindow()
{
    return windowSystem().getMain().as<MainWindow>();
}

ImageBank &GloomApp::images()
{
    return app().d->images;
}
