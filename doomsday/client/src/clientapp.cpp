/** @file clientapp.cpp  The client application.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de_platform.h"

#include <QMenuBar>
#include <QAction>
#include <QNetworkProxyFactory>
#include <QDesktopServices>
#include <QFontDatabase>
#include <QDebug>
#include <stdlib.h>

#include <de/Log>
#include <de/LogSink>
#include <de/DisplayMode>
#include <de/NativeFont>
#include <de/Error>
#include <de/ByteArrayFile>
#include <de/ArrayValue>
#include <de/DictionaryValue>
#include <de/VRConfig>
#include <de/c_wrapper.h>
#include <de/Garbage>

#include "clientapp.h"
#include "alertmask.h"
#include "dd_main.h"
#include "dd_def.h"
#include "dd_loop.h"
#include "de_audio.h"
#include "def_main.h"
#include "sys_system.h"
#include "audio/s_main.h"
#include "gl/gl_main.h"
#include "gl/gl_texmanager.h"
#include "ui/inputsystem.h"
#include "ui/b_main.h"
#include "ui/sys_input.h"
#include "ui/clientwindowsystem.h"
#include "ui/clientwindow.h"
#include "ui/widgets/taskbarwidget.h"
#include "ui/dialogs/alertdialog.h"
#include "ui/styledlogsinkformatter.h"
#include "updater.h"

#if WIN32
#  include "dd_winit.h"
#elif UNIX
#  include "dd_uinit.h"
#endif

#include <de/timer.h>

using namespace de;

static ClientApp *clientAppSingleton = 0;

static void handleLegacyCoreTerminate(char const *msg)
{
    App_Error("Application terminated due to exception:\n%s\n", msg);
}

static void continueInitWithEventLoopRunning()
{
    // Show the main window. This causes initialization to finish (in busy mode)
    // as the canvas is visible and ready for initialization.
    ClientWindowSystem::main().show();

    ClientApp::updater().setupUI();
}

static Value *Function_App_GamePlugin(Context &, Function::ArgumentValues const &)
{
    if(App_CurrentGame().isNull())
    {
        // The null game has no plugin.
        return 0;
    }
    String name = Plug_FileForPlugin(App_CurrentGame().pluginId()).name().fileNameWithoutExtension();
    if(name.startsWith("lib")) name.remove(0, 3);
    return new TextValue(name);
}

static Value *Function_App_Quit(Context &, Function::ArgumentValues const &)
{
    Sys_Quit();
    return 0;
}

DENG2_PIMPL(ClientApp)
{    
    Binder binder;
    QScopedPointer<Updater> updater;
    SettingsRegister audioSettings;
    SettingsRegister networkSettings;
    SettingsRegister logSettings;
    QMenuBar *menuBar;
    InputSystem *inputSys;
    RenderSystem *renderSys;
    ResourceSystem *resourceSys;
    ClientWindowSystem *winSys;
    InFineSystem infineSys; // instantiated at construction time
    ServerLink *svLink;
    Games games;
    WorldSystem *worldSys;

    /**
     * Log entry sink that passes warning messages to the main window's alert
     * notification dialog.
     */
    struct LogWarningAlarm : public LogSink
    {
        AlertMask alertMask;
        StyledLogSinkFormatter formatter;

        LogWarningAlarm()
            : LogSink(formatter)
            , formatter(LogEntry::Styled | LogEntry::OmitLevel | LogEntry::Simple)
        {
            //formatter.setOmitSectionIfNonDev(false); // always show section
            setMode(OnlyWarningEntries);
        }

        LogSink &operator << (LogEntry const &entry)
        {
            if(alertMask.shouldRaiseAlert(entry.metadata()))
            {
                // Don't raise alerts if the console history is open; the
                // warning/error will be shown there.
                if(ClientWindow::mainExists() &&
                   ClientWindow::main().taskBar().isOpen() &&
                   ClientWindow::main().taskBar().console().isLogOpen())
                {
                    return *this;
                }

                // We don't want to raise alerts about problems in id/Raven WADs,
                // since these just have to be accepted by the user.
                if((entry.metadata() & LogEntry::Map) &&
                   ClientApp::worldSystem().hasMap())
                {
                    if(MapDef *mapDef = ClientApp::worldSystem().map().def())
                    {
                        if(!mapDef->sourceFile()->hasCustom())
                        {
                            return *this;
                        }
                    }
                }

                foreach(String msg, formatter.logEntryToTextLines(entry))
                {
                    ClientApp::alert(msg, entry.level());
                }
            }
            return *this;
        }

        LogSink &operator << (String const &plainText)
        {
            ClientApp::alert(plainText);
            return *this;
        }

        void flush() {} // not buffered
    };

    LogWarningAlarm logAlarm;

    Instance(Public *i)
        : Base(i)
        , menuBar    (0)
        , inputSys   (0)
        , renderSys  (0)
        , resourceSys(0)
        , winSys     (0)
        //, infineSys  (0)
        , svLink     (0)
        , worldSys   (0)
    {
        clientAppSingleton = thisPublic;

        LogBuffer::get().addSink(logAlarm);
    }

    ~Instance()
    {
        LogBuffer::get().removeSink(logAlarm);

        self.vr().oculusRift().deinit();

        Sys_Shutdown();
        DD_Shutdown();

        updater.reset();
        delete worldSys;
        //delete infineSys;
        delete winSys;
        delete svLink;
        delete renderSys;
        delete resourceSys;
        delete inputSys;
        delete menuBar;
        clientAppSingleton = 0;
    }

    /**
     * Set up an application-wide menu.
     */
    void setupAppMenu()
    {
#ifdef MACOSX
        menuBar = new QMenuBar;
        QMenu *gameMenu = menuBar->addMenu("&Game");
        QAction *checkForUpdates = gameMenu->addAction("Check For &Updates...", updater.data(),
                                                       SLOT(checkNowShowingProgress()));
        checkForUpdates->setMenuRole(QAction::ApplicationSpecificRole);
#endif
    }

    void initSettings()
    {
        typedef SettingsRegister SReg; // convenience

        // Log filter and alert settings.
        for(int i = LogEntry::FirstDomainBit; i <= LogEntry::LastDomainBit; ++i)
        {
            String const name = LogFilter::domainRecordName(LogEntry::Context(1 << i));
            logSettings
                    .define(SReg::ConfigVariable, String("log.filter.%1.minLevel").arg(name))
                    .define(SReg::ConfigVariable, String("log.filter.%1.allowDev").arg(name))
                    .define(SReg::ConfigVariable, String("alert.%1").arg(name));
        }

        /// @todo These belong in their respective subsystems.

        networkSettings
                .define(SReg::StringCVar, "net-master-address", "www.dengine.net")
                .define(SReg::StringCVar, "net-master-path",    "/master.php")
                .define(SReg::IntCVar,    "net-master-port",    0)
                .define(SReg::IntCVar,    "net-dev",            0);

        audioSettings
                .define(SReg::IntCVar,   "sound-volume",        255 * 2/3)
                .define(SReg::IntCVar,   "music-volume",        255 * 2/3)
                .define(SReg::FloatCVar, "sound-reverb-volume", 0.5f)
                .define(SReg::IntCVar,   "sound-info",          0)
                .define(SReg::IntCVar,   "sound-rate",          11025)
                .define(SReg::IntCVar,   "sound-16bit",         0)
                .define(SReg::IntCVar,   "sound-3d",            0)
                .define(SReg::IntCVar,   "sound-overlap-stop",  0)
                .define(SReg::IntCVar,   "music-source",        MUSP_EXT);
    }

#ifdef UNIX
    void printVersionToStdOut()
    {
        printf("%s\n", String("%1 %2")
               .arg(DOOMSDAY_NICENAME)
               .arg(DOOMSDAY_VERSION_FULLTEXT)
               .toLatin1().constData());
    }

    void printHelpToStdOut()
    {
        printVersionToStdOut();
        printf("Usage: %s [options]\n", self.commandLine().at(0).toLatin1().constData());
        printf(" -iwad (dir)  Set directory containing IWAD files.\n");
        printf(" -file (f)    Load one or more PWAD files at startup.\n");
        printf(" -game (id)   Set game to load at startup.\n");
        printf(" -nomaximize  Do not maximize window at startup.\n");
        printf(" -wnd         Start in windowed mode.\n");
        printf(" -wh (w) (h)  Set window width and height.\n");
        printf(" --version    Print current version.\n");
        printf("For more options and information, see \"man doomsday\".\n");
    }
#endif
};

ClientApp::ClientApp(int &argc, char **argv)
    : BaseGuiApp(argc, argv), d(new Instance(this))
{
    novideo = false;

    // Use the host system's proxy configuration.
    QNetworkProxyFactory::setUseSystemConfiguration(true);

    // Metadata.
    setMetadata("Deng Team", "dengine.net", "Doomsday Engine", DOOMSDAY_VERSION_BASE);
    setUnixHomeFolderName(".doomsday");

    setTerminateFunc(handleLegacyCoreTerminate);

    // We must presently set the current game manually (the collection is global).
    setGame(d->games.nullGame());

    d->binder.init(scriptSystem().nativeModule("App"))
            << DENG2_FUNC_NOARG (App_GamePlugin, "gamePlugin")
            << DENG2_FUNC_NOARG (App_Quit,       "quit");
}

void ClientApp::initialize()
{
    Libdeng_Init();

#ifdef UNIX
    // Some common Unix command line options.
    if(commandLine().has("--version") || commandLine().has("-version"))
    {
        d->printVersionToStdOut();
        ::exit(0);
    }
    if(commandLine().has("--help") || commandLine().has("-h") || commandLine().has("-?"))
    {
        d->printHelpToStdOut();
        ::exit(0);
    }
#endif

    d->svLink = new ServerLink;

    // Config needs DisplayMode, so let's initialize it before the libcore
    // subsystems and Config.
    DisplayMode_Init();

    // Initialize definitions before the files are indexed.
    Def_Init();

    addInitPackage("net.dengine.base");
    addInitPackage("net.dengine.client");
    initSubsystems(); // loads Config

    // Set up the log alerts (observes Config variables).
    d->logAlarm.alertMask.init();

    // Create the user's configurations and settings folder, if it doesn't exist.
    fileSystem().makeFolder("/home/configs");

    d->initSettings();

    // Initialize.
#if WIN32
    if(!DD_Win32_Init())
    {
        throw Error("ClientApp::initialize", "DD_Win32_Init failed");
    }
#elif UNIX
    if(!DD_Unix_Init())
    {
        throw Error("ClientApp::initialize", "DD_Unix_Init failed");
    }
#endif

    // Create the render system.
    d->renderSys = new RenderSystem;
    addSystem(*d->renderSys);

    // Create the window system.
    d->winSys = new ClientWindowSystem;
    WindowSystem::setAppWindowSystem(*d->winSys);
    addSystem(*d->winSys);

    // Check for updates automatically.
    d->updater.reset(new Updater);
    d->setupAppMenu();

    // Create the resource system.
    d->resourceSys = new ResourceSystem;
    addSystem(*d->resourceSys);

    Plug_LoadAll();

    // Create the main window.
    d->winSys->createWindow()->setWindowTitle(DD_ComposeMainWindowTitle());

    // Create the input system.
    d->inputSys = new InputSystem;
    addSystem(*d->inputSys);
    B_Init();

    //d->infineSys = new InFineSystem;
    //addSystem(*d->infineSys);

    // Create the world system.
    d->worldSys = new WorldSystem;
    addSystem(*d->worldSys);

    // Finally, run the bootstrap script.
    scriptSystem().importModule("bootstrap");

    App_Timer(1, continueInitWithEventLoopRunning);
}

void ClientApp::preFrame()
{
    // Frame syncronous I/O operations.
    S_StartFrame(); /// @todo Move to AudioSystem::timeChanged().

    if(gx.BeginFrame) /// @todo Move to GameSystem::timeChanged().
    {
        gx.BeginFrame();
    }
}

void ClientApp::postFrame()
{
    /// @todo Should these be here? Consider multiple windows, each having a postFrame?
    /// Or maybe the frames need to be synced? Or only one of them has a postFrame?

    // We will arrive here always at the same time in relation to the displayed
    // frame: it is a good time to update the mouse state.
    Mouse_Poll();

    if(gx.EndFrame)
    {
        gx.EndFrame();
    }

    S_EndFrame();

    // This is a good time to recycle unneeded memory allocations, as we're just
    // finished and shown a frame and there might be free time before we have to
    // begin drawing the next frame.
    Garbage_Recycle();
}

void ClientApp::alert(String const &msg, LogEntry::Level level)
{
    if(ClientWindow::mainExists())
    {
        ClientWindow::main().alerts()
                .newAlert(msg, level >= LogEntry::Error?   AlertDialog::Major  :
                               level == LogEntry::Warning? AlertDialog::Normal :
                                                           AlertDialog::Minor);
    }
    /**
     * @todo If there is no window, the alert could be stored until the window becomes
     * available. -jk
     */
}

ClientApp &ClientApp::app()
{
    DENG2_ASSERT(clientAppSingleton != 0);
    return *clientAppSingleton;
}

Updater &ClientApp::updater()
{
    DENG2_ASSERT(!app().d->updater.isNull());
    return *app().d->updater;
}

SettingsRegister &ClientApp::logSettings()
{
    return app().d->logSettings;
}

SettingsRegister &ClientApp::networkSettings()
{
    return app().d->networkSettings;
}

SettingsRegister &ClientApp::audioSettings()
{
    return app().d->audioSettings;
}

ServerLink &ClientApp::serverLink()
{
    ClientApp &a = ClientApp::app();
    DENG2_ASSERT(a.d->svLink != 0);
    return *a.d->svLink;
}

InFineSystem &ClientApp::infineSystem()
{
    ClientApp &a = ClientApp::app();
    //DENG2_ASSERT(a.d->infineSys != 0);
    return a.d->infineSys;
}

InputSystem &ClientApp::inputSystem()
{
    ClientApp &a = ClientApp::app();
    DENG2_ASSERT(a.d->inputSys != 0);
    return *a.d->inputSys;
}

RenderSystem &ClientApp::renderSystem()
{
    ClientApp &a = ClientApp::app();
    DENG2_ASSERT(hasRenderSystem());
    return *a.d->renderSys;
}

bool ClientApp::hasRenderSystem()
{
    return ClientApp::app().d->renderSys != 0;
}

ResourceSystem &ClientApp::resourceSystem()
{
    ClientApp &a = ClientApp::app();
    DENG2_ASSERT(a.d->resourceSys != 0);
    return *a.d->resourceSys;
}

ClientWindowSystem &ClientApp::windowSystem()
{
    ClientApp &a = ClientApp::app();
    DENG2_ASSERT(a.d->winSys != 0);
    return *a.d->winSys;
}

Games &ClientApp::games()
{
    return app().d->games;
}

WorldSystem &ClientApp::worldSystem()
{
    ClientApp &a = ClientApp::app();
    DENG2_ASSERT(a.d->worldSys != 0);
    return *a.d->worldSys;
}

void ClientApp::openHomepageInBrowser()
{
    openInBrowser(QUrl(DOOMSDAY_HOMEURL));
}

void ClientApp::openInBrowser(QUrl url)
{
    // Get out of fullscreen mode.
    int windowed[] = {
        ClientWindow::Fullscreen, false,
        ClientWindow::End
    };
    ClientWindow::main().changeAttributes(windowed);

    QDesktopServices::openUrl(url);
}
