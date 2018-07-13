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
#include <de/App>
#include <de/Beacon>
#include <de/CommandLine>
#include <de/EventLoop>
#include <de/Loop>
#include <de/Info>
#include <QApplication>
#include <QMessageBox>
#include <QTimer>

using namespace de;

static const duint16 COMMAND_PORT = 14666;

struct GloomCommander : DE_OBSERVES(Beacon, Discovery)
{
    cplus::ref<iProcess> proc;
    Beacon               beacon{{COMMAND_PORT, COMMAND_PORT + 4}};
    Address              address;

    GloomCommander()
    {
        beacon.audienceForDiscovery() += this;
    }

    void beaconFoundHost(const Address &host, const Block &message) override
    {
        qDebug("GloomEd beacon found:%s [%s]", host.asText().c_str(), message.c_str());
        if (message.beginsWith(DE_STR("GloomApp:")))
        {
            const Info msg(message.mid(9));
            const duint16 commandPort = duint16(msg.root().keyValue("port").text.toInt());
            qDebug("Viewer command port: %u", commandPort);

            beacon.stop();
            address = host;
        }
    }
};
static GloomCommander *gloomCommander;

static bool launchGloom()
{
    if (gloomCommander)
    {
        if (gloomCommander->proc && isRunning_Process(gloomCommander->proc))
        {
            return true;
        }
    }
    else
    {
        gloomCommander = new GloomCommander;
    }

    gloomCommander->beacon.discover(0.0, 0.5);

    CommandLine cmd;
#if defined (MACOSX)
    cmd << convert(qApp->applicationDirPath() + "/../../../Gloom.app/Contents/MacOS/Gloom");
#endif
    gloomCommander->proc.reset(cmd.executeProcess());
    return bool(gloomCommander->proc);
}

struct EmbeddedApp : public App
{
    EventLoop deEventLoop{EventLoop::Manual};
    Loop      deLoop;

    EmbeddedApp(const StringList &args) : App(args)
    {}

    NativePath appDataPath() const
    {
        return NativePath::homePath() / unixHomeFolderName();
    }

    void processEvents()
    {
        // Manually handle events and loop iteration callbacks.
        deLoop.iterate();
        deEventLoop.processQueuedEvents();
        fflush(stdout);
        fflush(stderr);
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
            // Export/update the map package.

            // Launch the Gloom app.
            if (!launchGloom())
            {
                QMessageBox::critical(&win, app.applicationName(), "Failed to launch Gloom.");
                return;
            }

            // Wait for the process to start listening and tell it to load the map.

        }
        catch (const Error &er)
        {
            warning("Map build error: %s", er.asPlainText().c_str());
        }
    });

    win.showNormal();

    /* We are not running a de::App, but some classes assume that the Loop/EventLoop are
     * available and active. Use a QTimer to continually check for events and perform
     * loop iteration.
     */
    EmbeddedApp deApp(makeList(argc, argv));
    deApp.initSubsystems(App::DisablePersistentData | App::DisablePlugins);
    QTimer deTimer;
    QObject::connect(&deTimer, &QTimer::timeout, [&deApp]() { deApp.processEvents(); });
    deTimer.start(100);

    int rc = app.exec();
    delete gloomCommander;
    return rc;
}
