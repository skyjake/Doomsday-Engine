/** @file editorapp.cpp  GloomEd application.
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "editorapp.h"
#include "editor.h"
#include "commander.h"
#include "utils.h"

#include <de/Async>
#include <de/FileSystem>
#include <QMessageBox>
#include <QTimer>

using namespace de;

DE_PIMPL(EditorApp)
{
    QTimer    deTimer;
    Commander commander;

    Impl(Public *i) : Base(i)
    {}
};

EditorApp::EditorApp(int &argc, char **argv)
    : QApplication(argc, argv)
    , EmbeddedApp(makeList(argc, argv))
    , DoomsdayApp(nullptr,
                  DoomsdayApp::DisableGameProfiles |
                  DoomsdayApp::DisablePersistentConfig |
                  DoomsdayApp::DisableSaveGames)
    , d(new Impl(this))
{
    setApplicationName("GloomEd");
    setApplicationVersion("1.0");
    setOrganizationName("Deng Team");
    setOrganizationDomain("dengine.net");

    // Same metdata for Doomsday.
    {
        auto &amd = metadata();
        amd.set(App::APP_NAME,    convert(applicationName()));
        amd.set(App::APP_VERSION, convert(applicationVersion()));
        amd.set(App::ORG_NAME,    convert(organizationName()));
        amd.set(App::ORG_DOMAIN,  convert(organizationDomain()));
        amd.set(App::UNIX_HOME,   ".gloomed");
    }
}

void EditorApp::initialize()
{
    initSubsystems(DisablePersistentData | DisablePlugins);
    DoomsdayApp::initialize();

    /* We are not running a de::App, but some classes assume that the Loop/EventLoop are
     * available and active. Use a QTimer to continually check for events and perform
     * loop iteration.
     */
    QObject::connect(&d->deTimer, &QTimer::timeout, [this]() { EmbeddedApp::processEvents(); });
    d->deTimer.start(100);
}

void EditorApp::checkPackageCompatibility(const StringList &,
                                          const String &,
                                          const std::function<void()> &finalizeFunc)
{
    finalizeFunc();
}

bool EditorApp::launchViewer()
{
    if (d->commander.isConnected())
    {
        return true; // Already have a viewer.
    }
    if (d->commander.isLaunched())
    {
        return true; // Should be connected soon.
    }
    // Get a new viewer.
    return d->commander.launch();
}

void EditorApp::loadEditorMapInViewer(Editor &editor)
{
    try
    {
        // Export/update the map package.
        editor.exportPackage();

        // Launch the Gloom app.
        if (!launchViewer())
        {
            QMessageBox::critical(nullptr, applicationName(), "Failed to launch Gloom.");
            return;
        }

        // Wait for the process to start listening and tell it to load the map.
        async(
            [this, &editor]() {
                d->commander.sendCommand(
                    Stringf("command loadmap{map:%s\npackage:%s\nnativePath:%s\n}",
                            editor.mapId().c_str(),
                            editor.packageName().c_str(),
                            FS::locate<const Folder>("/home").correspondingNativePath().c_str()));
                return 0;
            },
            [](int) {
                qDebug("Viewer has been requested to load the map");
            });
    }
    catch (const Error &er)
    {
        qWarning("Map build error: %s", er.asPlainText().c_str());
    }
}
