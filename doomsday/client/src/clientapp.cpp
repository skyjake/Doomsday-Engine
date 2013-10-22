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
#include <de/DisplayMode>
#include <de/Error>
#include <de/ByteArrayFile>
#include <de/c_wrapper.h>
#include <de/garbage.h>

#include "clientapp.h"
#include "dd_main.h"
#include "dd_def.h"
#include "dd_loop.h"
#include "de_audio.h"
#include "con_main.h"
#include "sys_system.h"
#include "audio/s_main.h"
#include "gl/gl_main.h"
#include "gl/gl_texmanager.h"
#include "ui/inputsystem.h"
#include "ui/windowsystem.h"
#include "ui/clientwindow.h"
#include "updater.h"

#if WIN32
#  include "dd_winit.h"
#elif UNIX
#  include "dd_uinit.h"
#endif

using namespace de;

static ClientApp *clientAppSingleton = 0;

static void handleLegacyCoreTerminate(char const *msg)
{
    Con_Error("Application terminated due to exception:\n%s\n", msg);
}

static void continueInitWithEventLoopRunning()
{
    // Show the main window. This causes initialization to finish (in busy mode)
    // as the canvas is visible and ready for initialization.
    WindowSystem::main().show();

    ClientApp::updater().setupUI();
}

Value *Binding_App_GamePlugin(Context &, Function::ArgumentValues const &)
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

Value *Binding_App_LoadFont(Context &, Function::ArgumentValues const &args)
{
    LOG_AS("ClientApp");

    // We must have one argument.
    if(args.size() != 1)
    {
        throw Function::WrongArgumentsError("Binding_App_LoadFont",
                                            "Expected one argument");
    }

    try
    {
        // Try to load the specific font.
        Block data(App::fileSystem().root().locate<File const>(args.at(0)->asText()));
        int id;
        id = QFontDatabase::addApplicationFontFromData(data);
        if(id < 0)
        {
            LOG_WARNING("Failed to load font:");
        }
        else
        {
            LOG_VERBOSE("Loaded font: %s") << args.at(0)->asText();
            //qDebug() << args.at(0)->asText();
            //qDebug() << "Families:" << QFontDatabase::applicationFontFamilies(id);
        }
    }
    catch(Error const &er)
    {
        LOG_WARNING("Failed to load font:\n") << er.asText();
    }
    return 0;
}

DENG2_PIMPL(ClientApp)
{    
    QScopedPointer<Updater> updater;
    SettingsRegister rendererSettings;
    SettingsRegister rendererAppearanceSettings;
    SettingsRegister audioSettings;
    QMenuBar *menuBar;
    InputSystem *inputSys;
    QScopedPointer<WidgetActions> widgetActions;
    WindowSystem *winSys;
    ServerLink *svLink;
    GLShaderBank shaderBank;
    Games games;
    World world;

    Instance(Public *i)
        : Base(i),
          menuBar(0),
          inputSys(0),
          winSys(0),
          svLink(0)
    {
        clientAppSingleton = thisPublic;
    }

    ~Instance()
    {
        Sys_Shutdown();
        DD_Shutdown();

        delete svLink;
        delete winSys;
        delete inputSys;
        delete menuBar;
        clientAppSingleton = 0;

        deinitScriptBindings();
    }

    void initScriptBindings()
    {
        Function::registerNativeEntryPoint("App_GamePlugin", Binding_App_GamePlugin);
        Function::registerNativeEntryPoint("App_LoadFont", Binding_App_LoadFont);

        Record &appModule = self.scriptSystem().nativeModule("App");
        appModule.addFunction("gamePlugin", refless(new Function("App_GamePlugin"))).setReadOnly();
        appModule.addFunction("loadFont",   refless(new Function("App_LoadFont", Function::Arguments() << "fileName"))).setReadOnly();
    }

    void deinitScriptBindings()
    {
        Function::unregisterNativeEntryPoint("App_GamePlugin");
        Function::unregisterNativeEntryPoint("App_LoadFont");
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
        typedef SettingsRegister SReg;

        /// @todo These belong in their respective subsystems.

        rendererSettings
                .define(SReg::FloatCVar, "rend-camera-fov", 95.f)
                .define(SReg::IntCVar,   "rend-model-mirror-hud", 0)
                .define(SReg::IntCVar,   "rend-model-precache", 1)
                .define(SReg::IntCVar,   "rend-sprite-precache", 1)
                .define(SReg::IntCVar,   "rend-light-multitex", 1)
                .define(SReg::IntCVar,   "rend-model-shiny-multitex", 1)
                .define(SReg::IntCVar,   "rend-tex-detail-multitex", 1)
                .define(SReg::IntCVar,   "rend-tex", 1)
                .define(SReg::IntCVar,   "rend-dev-wireframe", 0)
                .define(SReg::IntCVar,   "rend-dev-thinker-ids", 0)
                .define(SReg::IntCVar,   "rend-dev-mobj-bbox", 0)
                .define(SReg::IntCVar,   "rend-dev-polyobj-bbox", 0)
                .define(SReg::IntCVar,   "rend-dev-sector-show-indices", 0)
                .define(SReg::IntCVar,   "rend-dev-vertex-show-indices", 0)
                .define(SReg::IntCVar,   "rend-dev-generator-show-indices", 0);

        rendererAppearanceSettings.setPersistentName("renderer");
        rendererAppearanceSettings
                .define(SReg::IntCVar,   "rend-light", 1)
                .define(SReg::IntCVar,   "rend-light-decor", 1)
                .define(SReg::IntCVar,   "rend-light-blend", 0)
                .define(SReg::IntCVar,   "rend-light-num", 0)
                .define(SReg::FloatCVar, "rend-light-bright", .5f)
                .define(SReg::FloatCVar, "rend-light-fog-bright", .15f)
                .define(SReg::FloatCVar, "rend-light-radius-scale", 4.24f)
                .define(SReg::IntCVar,   "rend-light-radius-max", 256)
                .define(SReg::IntCVar,   "rend-light-ambient", 0)
                .define(SReg::FloatCVar, "rend-light-compression", 0)
                .define(SReg::IntCVar,   "rend-light-attenuation", 924)
                .define(SReg::IntCVar,   "rend-light-sky-auto", 1)
                .define(SReg::FloatCVar, "rend-light-sky", .273f)
                .define(SReg::IntCVar,   "rend-light-wall-angle-smooth", 1)
                .define(SReg::FloatCVar, "rend-light-wall-angle", 1.2f)

                .define(SReg::IntCVar,   "rend-vignette", 1)
                .define(SReg::FloatCVar, "rend-vignette-darkness", 1)
                .define(SReg::FloatCVar, "rend-vignette-width", 1)

                .define(SReg::IntCVar,   "rend-halo-realistic", 1)
                .define(SReg::IntCVar,   "rend-halo", 5)
                .define(SReg::IntCVar,   "rend-halo-bright", 45)
                .define(SReg::IntCVar,   "rend-halo-size", 80)
                .define(SReg::IntCVar,   "rend-halo-occlusion", 48)
                .define(SReg::FloatCVar, "rend-halo-radius-min", 20)
                .define(SReg::FloatCVar, "rend-halo-secondary-limit", 1)
                .define(SReg::FloatCVar, "rend-halo-dim-near", 10)
                .define(SReg::FloatCVar, "rend-halo-dim-far", 100)
                .define(SReg::FloatCVar, "rend-halo-zmag-div", 62)

                .define(SReg::FloatCVar, "rend-glow", .8f)
                .define(SReg::IntCVar,   "rend-glow-height", 100)
                .define(SReg::FloatCVar, "rend-glow-scale", 3)
                .define(SReg::IntCVar,   "rend-glow-wall", 1)

                .define(SReg::IntCVar,   "rend-fakeradio", 1)
                .define(SReg::FloatCVar, "rend-fakeradio-darkness", 1.2f)
                .define(SReg::IntCVar,   "rend-shadow", 1)
                .define(SReg::FloatCVar, "rend-shadow-darkness", 1.2f)
                .define(SReg::IntCVar,   "rend-shadow-far", 1000)
                .define(SReg::IntCVar,   "rend-shadow-radius-max", 80)

                .define(SReg::IntCVar,   "rend-tex-shiny", 1)
                .define(SReg::IntCVar,   "rend-tex-mipmap", 5)
                .define(SReg::IntCVar,   "rend-tex-quality", TEXQ_BEST)
                .define(SReg::IntCVar,   "rend-tex-anim-smooth", 1)
                .define(SReg::IntCVar,   "rend-tex-filter-smart", 0)
                .define(SReg::IntCVar,   "rend-tex-filter-sprite", 1)
                .define(SReg::IntCVar,   "rend-tex-filter-mag", 1)
                .define(SReg::IntCVar,   "rend-tex-filter-ui", 1)
                .define(SReg::IntCVar,   "rend-tex-filter-anisotropic", -1)
                .define(SReg::IntCVar,   "rend-tex-detail", 1)
                .define(SReg::FloatCVar, "rend-tex-detail-scale", 4)
                .define(SReg::FloatCVar, "rend-tex-detail-strength", .5f)

                .define(SReg::IntCVar,   "rend-mobj-smooth-move", 2)
                .define(SReg::IntCVar,   "rend-mobj-smooth-turn", 1)

                .define(SReg::IntCVar,   "rend-model", 1)
                .define(SReg::IntCVar,   "rend-model-inter", 1)
                .define(SReg::IntCVar,   "rend-model-distance", 1500)
                .define(SReg::FloatCVar, "rend-model-lod", 256)
                .define(SReg::FloatCVar, "rend-model-lights", 4)

                .define(SReg::IntCVar,   "rend-sprite-mode", 0)
                .define(SReg::IntCVar,   "rend-sprite-blend", 1)
                .define(SReg::IntCVar,   "rend-sprite-lights", 4)
                .define(SReg::IntCVar,   "rend-sprite-align", 0)
                .define(SReg::IntCVar,   "rend-sprite-noz", 0)

                .define(SReg::IntCVar,   "rend-particle", 1)
                .define(SReg::IntCVar,   "rend-particle-max", 0)
                .define(SReg::FloatCVar, "rend-particle-rate", 1)
                .define(SReg::FloatCVar, "rend-particle-diffuse", 4)
                .define(SReg::IntCVar,   "rend-particle-visible-near", 0)

                .define(SReg::FloatCVar, "rend-sky-distance", 1600);

        audioSettings
                .define(SReg::IntCVar,   "sound-volume",        255)
                .define(SReg::IntCVar,   "music-volume",        255)
                .define(SReg::FloatCVar, "sound-reverb-volume", 0.5f)
                .define(SReg::IntCVar,   "sound-info",          0)
                .define(SReg::IntCVar,   "sound-rate",          11025)
                .define(SReg::IntCVar,   "sound-16bit",         0)
                .define(SReg::IntCVar,   "sound-3d",            0)
                .define(SReg::IntCVar,   "sound-overlap-stop",  0)
                .define(SReg::IntCVar,   "music-source",        MUSP_EXT);
    }
};

ClientApp::ClientApp(int &argc, char **argv)
    : GuiApp(argc, argv), d(new Instance(this))
{
    novideo = false;

    // Override the system locale (affects number/time formatting).
    QLocale::setDefault(QLocale("en_US.UTF-8"));

    // Use the host system's proxy configuration.
    QNetworkProxyFactory::setUseSystemConfiguration(true);

    // Metadata.
    QCoreApplication::setOrganizationDomain ("dengine.net");
    QCoreApplication::setOrganizationName   ("Deng Team");
    QCoreApplication::setApplicationName    ("Doomsday Engine");
    QCoreApplication::setApplicationVersion (DOOMSDAY_VERSION_BASE);

    setTerminateFunc(handleLegacyCoreTerminate);

    // We must presently set the current game manually (the collection is global).
    setGame(d->games.nullGame());

    d->initScriptBindings();
}

void ClientApp::initialize()
{
    Libdeng_Init();

    d->svLink = new ServerLink;

    // Config needs DisplayMode, so let's initialize it before the libdeng2
    // subsystems and Config.
    DisplayMode_Init();

    initSubsystems(); // loads Config

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

    // Load all the shader program definitions.
    FS::FoundFiles found;
    fileSystem().findAll("shaders.dei", found);
    DENG2_FOR_EACH(FS::FoundFiles, i, found)
    {
        LOG_MSG("Loading shader definitions from %s") << (*i)->description();
        d->shaderBank.addFromInfo(**i);
    }

    // Create the window system.
    d->winSys = new WindowSystem;
    addSystem(*d->winSys);

    // Check for updates automatically.
    d->updater.reset(new Updater);
    d->setupAppMenu();

    Plug_LoadAll();

    // Create the main window.
    char title[256];
    DD_ComposeMainWindowTitle(title);
    d->winSys->createWindow()->setWindowTitle(title);

    // Create the input system.
    d->inputSys = new InputSystem;
    addSystem(*d->inputSys);
    d->widgetActions.reset(new WidgetActions);

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

    if(gx.EndFrame)
    {
        gx.EndFrame();
    }

    S_EndFrame();

    Garbage_Recycle();
    loop().resume();
}

bool ClientApp::haveApp()
{
    return clientAppSingleton != 0;
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

SettingsRegister &ClientApp::rendererSettings()
{
    return app().d->rendererSettings;
}

SettingsRegister &ClientApp::rendererAppearanceSettings()
{
    return app().d->rendererAppearanceSettings;
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

InputSystem &ClientApp::inputSystem()
{
    ClientApp &a = ClientApp::app();
    DENG2_ASSERT(a.d->inputSys != 0);
    return *a.d->inputSys;
}

WindowSystem &ClientApp::windowSystem()
{
    ClientApp &a = ClientApp::app();
    DENG2_ASSERT(a.d->winSys != 0);
    return *a.d->winSys;
}

WidgetActions &ClientApp::widgetActions()
{
    return *app().d->widgetActions;
}

GLShaderBank &ClientApp::glShaderBank()
{
    return app().d->shaderBank;
}

Games &ClientApp::games()
{
    return app().d->games;
}

World &ClientApp::world()
{
    return app().d->world;
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
