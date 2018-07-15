/** @file main.cpp
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

#include "editorwindow.h"
#include "utils.h"

#include <doomsday/DoomsdayApp>

#include <de/App>
#include <de/Async>
#include <de/Beacon>
#include <de/CommandLine>
#include <de/EmbeddedApp>
#include <de/EventLoop>
#include <de/Loop>
#include <de/Info>

#include <QApplication>
#include <QMessageBox>
#include <QTimer>

#include <c_plus/datagram.h>

using namespace de;

struct GloomCommander;

static GloomCommander *gloomCommander;
static const duint16   COMMAND_PORT = 14666;

/**
 * Sends commands to the Gloom viewer app and listens to beacon messages.
 */
struct GloomCommander : DE_OBSERVES(Beacon, Discovery), public Waitable
{
    cplus::ref<iProcess>  proc;
    Beacon                beacon{{COMMAND_PORT, COMMAND_PORT + 4}};
    Address               address;
    cplus::ref<iDatagram> commandSocket;

    GloomCommander()
    {
        beacon.audienceForDiscovery() += this;
    }

    void beaconFoundHost(const Address &host, const Block &message) override
    {
        if (isConnected()) return; // Ignore additional replies.

        qDebug("GloomEd beacon found:%s [%s]", host.asText().c_str(), message.c_str());
        if (message.beginsWith(DE_STR("GloomApp:")))
        {
            const Info    msg(message.mid(9));
            const duint16 commandPort = duint16(msg["port"].toUInt32());

            beacon.stop();
            address = host;

            commandSocket.reset(new_Datagram());
            for (int i = 0; i < 10; ++i)
            {
                // Open a socket in the private port range.
                if (open_Datagram(commandSocket, duint16(Rangeui(0xc000, 0x10000).random())))
                {
                    break;
                }
            }
            if (!isOpen_Datagram(commandSocket))
            {
                qWarning("Failed to open UDP port for sending commands");
            }
            connect_Datagram(commandSocket, Address(host.hostName(), commandPort));

            post();
        }
    }

    bool isConnected() const
    {
        return !address.isNull();
    }

    void sendCommand(const String &command)
    {
        if (!isConnected())
        {
            wait(15.0);
        }
        write_Datagram(commandSocket, command.toUtf8());
    }
};

/**
 * Launch the Gloom viewer, if one is not already running.
 */
static bool launchGloom()
{
    if (gloomCommander)
    {
#if 0
        if (gloomCommander->proc && isRunning_Process(gloomCommander->proc))
#endif
        {
            return true;
        }
    }
    else
    {
        gloomCommander = new GloomCommander;
    }

    gloomCommander->beacon.discover(0.0, 0.5);
    return true;

#if 0
    CommandLine cmd;
#if defined (MACOSX)
    cmd << convert(qApp->applicationDirPath() + "/../../../Gloom.app/Contents/MacOS/Gloom");
#endif
    gloomCommander->proc.reset(cmd.executeProcess());
    return bool(gloomCommander->proc);
#endif
}

struct EditorApp : public EmbeddedApp, public DoomsdayApp
{
    EditorApp(const StringList &args)
        : EmbeddedApp(args)
        , DoomsdayApp(nullptr,
                      DoomsdayApp::DisableGameProfiles |
                      DoomsdayApp::DisablePersistentConfig |
                      DoomsdayApp::DisableSaveGames)
    {}

    void initialize()
    {
        initSubsystems(DisablePersistentData | DisablePlugins);
        DoomsdayApp::initialize();
    }

    void checkPackageCompatibility(const de::StringList &,
                                   const de::String &,
                                   const std::function<void()> &finalizeFunc)
    {
        finalizeFunc();
    }
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    app.setApplicationName   ("GloomEd");
    app.setApplicationVersion("1.0");
    app.setOrganizationName  ("Deng Team");
    app.setOrganizationDomain("dengine.net");

    EditorWindow win;

    QObject::connect(&win.editor(), &Editor::buildMapRequested, &win, [&win, &app]() {
        try
        {
            auto &editor = win.editor();

            // Export/update the map package.
            editor.exportPackage();

            // Launch the Gloom app.
            if (!launchGloom())
            {
                QMessageBox::critical(&win, app.applicationName(), "Failed to launch Gloom.");
                return;
            }

            // Wait for the process to start listening and tell it to load the map.
            async(
                [&editor]() {
                    gloomCommander->sendCommand(
                        Stringf("command loadmap{package=\"%s\"}", editor.packageName().c_str()));
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
    });

    win.showNormal();

    /* We are not running a de::App, but some classes assume that the Loop/EventLoop are
     * available and active. Use a QTimer to continually check for events and perform
     * loop iteration.
     */
    EditorApp edApp(makeList(argc, argv));
    {
        auto &amd = edApp.metadata();
        amd.set(App::APP_NAME,    convert(app.applicationName()));
        amd.set(App::APP_VERSION, convert(app.applicationVersion()));
        amd.set(App::ORG_NAME,    convert(app.organizationName()));
        amd.set(App::ORG_DOMAIN,  convert(app.organizationDomain()));
        amd.set(App::UNIX_HOME,   ".gloomed");
    }
    edApp.initialize();
    QTimer deTimer;
    QObject::connect(&deTimer, &QTimer::timeout, [&edApp]() { edApp.processEvents(); });
    deTimer.start(100);

    int rc = app.exec();
    delete gloomCommander;
    return rc;
}
