/** @file clientapp.cpp  The client application.
 *
 * @authors Copyright © 2013-2015 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013-2015 Daniel Swanson <danij@dengine.net>
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
#include "clientapp.h"

#include <cstdlib>
#include <QAction>
#include <QDebug>
#include <QDesktopServices>
#include <QFontDatabase>
#include <QMenuBar>
#include <QNetworkProxyFactory>
#include <QSplashScreen>

#include <de/c_wrapper.h>
#include <de/ArrayValue>
#include <de/ByteArrayFile>
#include <de/CallbackAction>
#include <de/CommandLine>
#include <de/DictionaryValue>
#include <de/DisplayMode>
#include <de/Error>
#include <de/FileSystem>
#include <de/Garbage>
#include <de/Log>
#include <de/LogSink>
#include <de/NativeFont>
#include <de/ScriptSystem>
#include <de/VRConfig>

#include <doomsday/console/exec.h>

#include "clientplayer.h"
#include "dd_main.h"
#include "dd_def.h"
#include "dd_loop.h"
#include "def_main.h"
#include "sys_system.h"
#include "con_config.h"

#include "audio/system.h"

#include "gl/gl_main.h"
#include "gl/gl_texmanager.h"
#include "gl/gl_defer.h"
#include "gl/svg.h"

#include "world/map.h"
#include "world/contact.h"
#include "world/p_players.h"

#include "ui/alertmask.h"
#include "ui/inputsystem.h"
#include "ui/b_main.h"
#include "ui/sys_input.h"
#include "ui/clientwindowsystem.h"
#include "ui/clientwindow.h"
#include "ui/progress.h"
#include "ui/widgets/taskbarwidget.h"
#include "ui/dialogs/alertdialog.h"
#include "ui/dialogs/packagecompatibilitydialog.h"
#include "ui/styledlogsinkformatter.h"
#include "ui/viewcompositor.h"
#include "render/rend_particle.h"
#include "render/r_draw.h"
#include "network/net_demo.h"
#include "updater.h"
#include "updater/downloaddialog.h"

#if WIN32
#  include "dd_winit.h"
#elif UNIX
#  include "dd_uinit.h"
#endif

#include <de/timer.h>

#include "ui/splash.xpm"

using namespace de;

static ClientApp *clientAppSingleton = 0;

static void handleLegacyCoreTerminate(char const *msg)
{
    App_Error("Application terminated due to exception:\n%s\n", msg);
}

static void continueInitWithEventLoopRunning()
{
    if (!ClientWindowSystem::mainExists()) return;

    // Show the main window. This causes initialization to finish (in busy mode)
    // as the canvas is visible and ready for initialization.
    ClientWindowSystem::main().show();

    ClientApp::updater().setupUI();
}

static Value *Function_App_GamePlugin(Context &, Function::ArgumentValues const &)
{
    if (App_CurrentGame().isNull())
    {
        // The null game has no plugin.
        return 0;
    }
    String name = DoomsdayApp::plugins().fileForPlugin(App_CurrentGame().pluginId())
            .name().fileNameWithoutExtension();
    if (name.startsWith("lib")) name.remove(0, 3);
    return new TextValue(name);
}

static Value *Function_App_Quit(Context &, Function::ArgumentValues const &)
{
    Sys_Quit();
    return 0;
}

DENG2_PIMPL(ClientApp)
, DENG2_OBSERVES(Plugins, PublishAPI)
, DENG2_OBSERVES(Plugins, Notification)
, DENG2_OBSERVES(Games, Progress)
, DENG2_OBSERVES(DoomsdayApp, GameChange)
, DENG2_OBSERVES(DoomsdayApp, GameUnload)
, DENG2_OBSERVES(DoomsdayApp, ConsoleRegistration)
{
    Binder binder;
    QScopedPointer<Updater> updater;
    BusyRunner busyRunner;
    ConfigProfiles audioSettings;
    ConfigProfiles networkSettings;
    ConfigProfiles logSettings;
    ConfigProfiles uiSettings;
    QMenuBar *menuBar;
    InputSystem *inputSys;
    ::audio::System *audioSys;
    RenderSystem *rendSys;
    ClientResources *resources;
    ClientWindowSystem *winSys;
    InFineSystem infineSys; // instantiated at construction time
    ServerLink *svLink;
    ClientServerWorld *world;

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
            if (alertMask.shouldRaiseAlert(entry.metadata()))
            {
                // Don't raise alerts if the console history is open; the
                // warning/error will be shown there.
                if (ClientWindow::mainExists() &&
                    ClientWindow::main().taskBar().isOpen() &&
                    ClientWindow::main().taskBar().console().isLogOpen())
                {
                    return *this;
                }

                // We don't want to raise alerts about problems in id/Raven WADs,
                // since these just have to be accepted by the user.
                if ((entry.metadata() & LogEntry::Map) &&
                   ClientApp::world().hasMap())
                {
                    world::Map const &map = ClientApp::world().map();
                    if (map.hasManifest() && !map.manifest().sourceFile()->hasCustom())
                    {
                        return *this;
                    }
                }

                foreach (String msg, formatter.logEntryToTextLines(entry))
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

    Impl(Public *i)
        : Base(i)
        , menuBar  (0)
        , inputSys (0)
        , audioSys (0)
        , rendSys  (0)
        , resources(0)
        , winSys   (0)
        , svLink   (0)
        , world    (0)
    {
        clientAppSingleton = thisPublic;

        LogBuffer::get().addSink(logAlarm);
        DoomsdayApp::plugins().audienceForPublishAPI() += this;
        DoomsdayApp::plugins().audienceForNotification() += this;
        self.audienceForGameChange() += this;
        self.audienceForGameUnload() += this;
        self.audienceForConsoleRegistration() += this;
        self.games().audienceForProgress() += this;
    }

    ~Impl()
    {
        try
        {
            ClientWindow::main().glActivate(); // for GL deinit

            LogBuffer::get().removeSink(logAlarm);

            self.players().forAll([] (Player &p)
            {
                p.as<ClientPlayer>().viewCompositor().glDeinit();
                return LoopContinue;
            });
            self.glDeinit();

            Sys_Shutdown();
            DD_Shutdown();
        }
        catch (Error const &er)
        {
            qWarning() << "Exception during ~ClientApp:" << er.asText();
            DENG2_ASSERT("Unclean shutdown: exception in ~ClientApp"!=0);
        }

        updater.reset();
        delete inputSys;
        delete resources;
        delete winSys;
        delete audioSys;
        delete rendSys;
        delete world;
        delete svLink;
        delete menuBar;
        clientAppSingleton = 0;
    }

    void publishAPIToPlugin(::Library *plugin)
    {
        DD_PublishAPIs(plugin);
    }

    void pluginSentNotification(int notification, void *data)
    {
        LOG_AS("ClientApp::pluginSentNotification");

        switch (notification)
        {
        case DD_NOTIFY_GAME_SAVED:
            // If an update has been downloaded and is ready to go, we should
            // re-show the dialog now that the user has saved the game as prompted.
            LOG_DEBUG("Game saved");
            DownloadDialog::showCompletedDownload();
            break;

        case DD_NOTIFY_PSPRITE_STATE_CHANGED:
            if (data)
            {
                auto const *args = (ddnotify_psprite_state_changed_t *) data;
                self.players().at(args->player)
                        .as<ClientPlayer>()
                        .weaponStateChanged(args->state);
            }
            break;

        case DD_NOTIFY_PLAYER_WEAPON_CHANGED:
            if (data)
            {
                auto const *args = (ddnotify_player_weapon_changed_t *) data;
                self.players().at(args->player)
                        .as<ClientPlayer>()
                        .setWeaponAssetId(args->weaponId);
            }
            break;

        default:
            break;
        }
    }

    void gameWorkerProgress(int progress)
    {
        Con_SetProgress(progress);
    }

    void consoleRegistration()
    {
        DD_ConsoleRegister();
    }

    void aboutToUnloadGame(Game const &/*gameBeingUnloaded*/)
    {
        DENG_ASSERT(ClientWindow::mainExists());

        // Quit netGame if one is in progress.
        if (netGame)
        {
            Con_Execute(CMDS_DDAY, "net disconnect", true, false);
        }

        Demo_StopPlayback();
        GL_PurgeDeferredTasks();

        App_Resources().releaseAllGLTextures();
        App_Resources().pruneUnusedTextureSpecs();
        GL_LoadLightingSystemTextures();
        GL_LoadFlareTextures();
        Rend_ParticleLoadSystemTextures();
        GL_ResetViewEffects();

        infineSystem().reset();

        if (App_GameLoaded())
        {
            // Write cvars and bindings to .cfg files.
            Con_SaveDefaults();

            R_ClearViewData();
            world::R_DestroyContactLists();
            P_ClearPlayerImpulses();

            Con_Execute(CMDS_DDAY, "clearbindings", true, false);
            inputSys->bindDefaults();
            inputSys->initialContextActivations();
        }

        infineSys.deinitBindingContext();
    }

    void currentGameChanged(Game const &newGame)
    {
        if (Sys_IsShuttingDown()) return;

        infineSys.initBindingContext();

        // Process any GL-related tasks we couldn't while Busy.
        Rend_ParticleLoadExtraTextures();

        /**
         * Clear any input events we may have accumulated during this process.
         * @note Only necessary here because we might not have been able to use
         *       busy mode (which would normally do this for us on end).
         */
        inputSys->clearEvents();

        if (newGame.isNull())
        {
            // The mouse is free while in the Home.
            ClientWindow::main().eventHandler().trapMouse(false);
        }

        ClientWindow::main().console().zeroLogHeight();
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
        using Prof = ConfigProfiles; // convenience

        // Log filter and alert settings.
        for (int i = LogEntry::FirstDomainBit; i <= LogEntry::LastDomainBit; ++i)
        {
            String const name = LogFilter::domainRecordName(LogEntry::Context(1 << i));
            logSettings
                    .define(Prof::ConfigVariable, String("log.filter.%1.minLevel").arg(name))
                    .define(Prof::ConfigVariable, String("log.filter.%1.allowDev").arg(name))
                    .define(Prof::ConfigVariable, String("alert.%1").arg(name));
        }

        uiSettings
                .define(Prof::ConfigVariable, "ui.scaleFactor")
                .define(Prof::ConfigVariable, "ui.showAnnotations")
                .define(Prof::ConfigVariable, "home.showColumnDescription")
                .define(Prof::ConfigVariable, "home.showUnplayableGames")
                .define(Prof::ConfigVariable, "home.columns.doom")
                .define(Prof::ConfigVariable, "home.columns.heretic")
                .define(Prof::ConfigVariable, "home.columns.hexen")
                .define(Prof::ConfigVariable, "home.columns.otherGames")
                .define(Prof::ConfigVariable, "home.columns.multiplayer");

        /// @todo These belong in their respective subsystems.

        networkSettings
                .define(Prof::ConfigVariable, "masterServer.apiUrl", "www.dengine.net/master.php")
                .define(Prof::IntCVar,        "net-dev",             0);

        audioSettings
                .define(Prof::IntCVar,    "sound-volume",        255 * 2/3)
                .define(Prof::IntCVar,    "music-volume",        255 * 2/3)
                .define(Prof::FloatCVar,  "sound-reverb-volume", 0.5f)
                .define(Prof::IntCVar,    "sound-info",          0)
                .define(Prof::IntCVar,    "sound-rate",          11025)
                .define(Prof::IntCVar,    "sound-16bit",         0)
                .define(Prof::IntCVar,    "sound-3d",            0)
                .define(Prof::IntCVar,    "sound-overlap-stop",  0)
                .define(Prof::IntCVar,    "music-source",        ::audio::System::MUSP_EXT)
                .define(Prof::StringCVar, "music-soundfont",     "");
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
    : BaseGuiApp(argc, argv)
    , DoomsdayApp([] () -> Player * { return new ClientPlayer; })
    , d(new Impl(this))
{
    novideo = false;

    // Use the host system's proxy configuration.
    QNetworkProxyFactory::setUseSystemConfiguration(true);

    // Metadata.
    setMetadata("Deng Team", "dengine.net", "Doomsday Engine", DOOMSDAY_VERSION_BASE);
    setUnixHomeFolderName(".doomsday");

    // Callbacks.
    setTerminateFunc(handleLegacyCoreTerminate);

    // We must presently set the current game manually (the collection is global).
    setGame(games().nullGame());

    d->binder.init(scriptSystem().nativeModule("App"))
            << DENG2_FUNC_NOARG (App_GamePlugin, "gamePlugin")
            << DENG2_FUNC_NOARG (App_Quit,       "quit");

    /// @todo Remove the splash screen when file system indexing can be done as
    /// a background task and the main window can be opened instantly. -jk
    QPixmap const pixmap(doomsdaySplashXpm);
    QSplashScreen *splash = new QSplashScreen(pixmap);
    splash->show();
    splash->showMessage(Version::currentBuild().asText(),
                        Qt::AlignHCenter | Qt::AlignBottom,
                        QColor(90, 110, 95));
    processEvents();
    splash->deleteLater();
}

void ClientApp::initialize()
{
    Libdeng_Init();

#ifdef UNIX
    // Some common Unix command line options.
    if (commandLine().has("--version") || commandLine().has("-version"))
    {
        d->printVersionToStdOut();
        ::exit(0);
    }
    if (commandLine().has("--help") || commandLine().has("-h") || commandLine().has("-?"))
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

    addInitPackage("net.dengine.client");
    initSubsystems(); // loads Config
    DoomsdayApp::initialize();

    // Initialize players.
    for (int i = 0; i < players().count(); ++i)
    {
        players().at(i).as<ClientPlayer>().viewCompositor().setPlayerNumber(i);
    }

    // Set up the log alerts (observes Config variables).
    d->logAlarm.alertMask.init();

    // Create the user's configurations and settings folder, if it doesn't exist.
    fileSystem().makeFolder("/home/configs");

    d->initSettings();

    // Initialize.
#if WIN32
    if (!DD_Win32_Init())
    {
        throw Error("ClientApp::initialize", "DD_Win32_Init failed");
    }
#elif UNIX
    if (!DD_Unix_Init())
    {
        throw Error("ClientApp::initialize", "DD_Unix_Init failed");
    }
#endif

    // Create the world system.
    d->world = new ClientServerWorld;
    addSystem(*d->world);

    // Create the render system.
    d->rendSys = new RenderSystem;
    addSystem(*d->rendSys);

    // Create the audio system.
    d->audioSys = new ::audio::System;
    addSystem(*d->audioSys);

    // Create the window system.
    d->winSys = new ClientWindowSystem;
    WindowSystem::setAppWindowSystem(*d->winSys);
    addSystem(*d->winSys);

    // Check for updates automatically.
    d->updater.reset(new Updater);
    d->setupAppMenu();

    // Create the resource system.
    d->resources = new ClientResources;
    addSystem(*d->resources);

    plugins().loadAll();

    // Create the main window.
    d->winSys->createWindow()->setTitle(DD_ComposeMainWindowTitle());

    // Create the input system.
    d->inputSys = new InputSystem;
    addSystem(*d->inputSys);
    B_Init();

    // Finally, run the bootstrap script.
    scriptSystem().importModule("bootstrap");

    App_Timer(1, continueInitWithEventLoopRunning);
}

void ClientApp::preFrame()
{
    // Frame synchronous I/O operations.
    App_AudioSystem().startFrame();

    if (gx.BeginFrame) /// @todo Move to GameSystem::timeChanged().
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

    if (gx.EndFrame)
    {
        gx.EndFrame();
    }

    App_AudioSystem().endFrame();

    // This is a good time to recycle unneeded memory allocations, as we're just
    // finished and shown a frame and there might be free time before we have to
    // begin drawing the next frame.
    Garbage_Recycle();
}

void ClientApp::checkPackageCompatibility(StringList const &packageIds,
                                          String const &userMessageIfIncompatible,
                                          std::function<void ()> finalizeFunc)
{
    if (packageIds.isEmpty() || // Metadata did not specify packages.
        GameProfiles::arePackageListsCompatible(packageIds, loadedPackagesIncludedInSavegames()))
    {
        finalizeFunc();
    }
    else
    {
        auto *dlg = new PackageCompatibilityDialog;
        dlg->setMessage(userMessageIfIncompatible);
        dlg->setWantedPackages(packageIds);
        dlg->setAcceptanceAction(new CallbackAction(finalizeFunc));

        if (!dlg->isCompatible())
        {
            // Run the dialog's event loop in a separate timer callback so it doesn't
            // interfere with the app's event loop.
            Loop::timer(.01, [this, dlg] ()
            {
                dlg->setDeleteAfterDismissed(true);
                dlg->exec(ClientWindow::main().root());
            });
        }
        else
        {
            delete dlg;
        }
    }
}

void ClientApp::alert(String const &msg, LogEntry::Level level)
{
    if (ClientWindow::mainExists())
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

BusyRunner &ClientApp::busyRunner()
{
    return app().d->busyRunner;
}

Updater &ClientApp::updater()
{
    DENG2_ASSERT(!app().d->updater.isNull());
    return *app().d->updater;
}

ConfigProfiles &ClientApp::logSettings()
{
    return app().d->logSettings;
}

ConfigProfiles &ClientApp::networkSettings()
{
    return app().d->networkSettings;
}

ConfigProfiles &ClientApp::audioSettings()
{
    return app().d->audioSettings;
}

ConfigProfiles &ClientApp::uiSettings()
{
    return app().d->uiSettings;
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

bool ClientApp::hasInputSystem()
{
    return ClientApp::app().d->inputSys != nullptr;
}

RenderSystem &ClientApp::renderSystem()
{
    ClientApp &a = ClientApp::app();
    DENG2_ASSERT(hasRenderSystem());
    return *a.d->rendSys;
}

bool ClientApp::hasRenderSystem()
{
    return ClientApp::app().d->rendSys != nullptr;
}

::audio::System &ClientApp::audioSystem()
{
    ClientApp &a = ClientApp::app();
    DENG2_ASSERT(hasAudioSystem());
    return *a.d->audioSys;
}

bool ClientApp::hasAudioSystem()
{
    return ClientApp::app().d->audioSys != nullptr;
}

ClientResources &ClientApp::resources()
{
    ClientApp &a = ClientApp::app();
    DENG2_ASSERT(a.d->resources != 0);
    return *a.d->resources;
}

ClientWindowSystem &ClientApp::windowSystem()
{
    ClientApp &a = ClientApp::app();
    DENG2_ASSERT(a.d->winSys != 0);
    return *a.d->winSys;
}

ClientServerWorld &ClientApp::world()
{
    ClientApp &a = ClientApp::app();
    DENG2_ASSERT(a.d->world != 0);
    return *a.d->world;
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

void ClientApp::unloadGame(GameProfile const &upcomingGame)
{
    DoomsdayApp::unloadGame(upcomingGame);

    // Game has been set to null, update window.
    ClientWindow::main().setTitle(DD_ComposeMainWindowTitle());

    if (!upcomingGame.game().isEmpty())
    {
        ClientWindow &mainWin = ClientWindow::main();
        mainWin.taskBar().close();

        // Trap the mouse automatically when loading a game in fullscreen.
        if (mainWin.isFullScreen())
        {
            mainWin.eventHandler().trapMouse();
        }
    }

    R_InitViewWindow();
    R_InitSvgs();

    world::Map::initDummies();
}

void ClientApp::makeGameCurrent(GameProfile const &newGame)
{
    DoomsdayApp::makeGameCurrent(newGame);

    // Game has been changed, update window.
    ClientWindow::main().setTitle(DD_ComposeMainWindowTitle());
}

void ClientApp::reset()
{
    DoomsdayApp::reset();

    if (App_GameLoaded())
    {
        d->inputSys->initAllDevices();
    }
}
