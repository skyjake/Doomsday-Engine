/** @file dd_main.cpp  Engine core.
 *
 * @todo Much of this should be refactored and merged into the App classes.
 * @todo The rest should be split into smaller, perhaps domain-specific files.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#define DE_NO_API_MACROS_BASE // functions defined here

#include "de_platform.h"
#include "dd_main.h"

#ifdef WIN32
#  define _WIN32_DCOM
#  include <objbase.h>
#endif
#include <cstring>
#ifdef UNIX
#  include <ctype.h>
#endif

#include <de/charsymbols.h>
#include <de/legacy/concurrency.h>
#include <de/legacy/findfile.h>
#include <de/legacy/memoryzone.h>
#include <de/legacy/memory.h>
#include <de/legacy/timer.h>
#include <de/arrayvalue.h>
#include <de/commandline.h>
#include <de/garbage.h>
#include <de/nativefile.h>
#include <de/packageloader.h>
#include <de/linkfile.h>
#include <de/logbuffer.h>
#include <de/dictionaryvalue.h>
#include <de/log.h>
#include <de/escapeparser.h>
#include <de/nativepath.h>

#include <doomsday/abstractsession.h>
#include <doomsday/console/alias.h>
#include <doomsday/console/cmd.h>
#include <doomsday/console/exec.h>
#include <doomsday/console/var.h>
#include <doomsday/filesys/fs_main.h>
#include <doomsday/filesys/fs_util.h>
#include <doomsday/filesys/virtualmappings.h>
#include <doomsday/filesys/wad.h>
#include <doomsday/filesys/zip.h>
#include <doomsday/res/databundle.h>
#include <doomsday/manifest.h>
#include <doomsday/res/resources.h>
#include <doomsday/res/bundles.h>
#include <doomsday/res/doomsdaypackage.h>
#include <doomsday/res/mapmanifests.h>
#include <doomsday/res/sprites.h>
#include <doomsday/res/textures.h>
#include <doomsday/world/materials.h>
#include <doomsday/world/entitydef.h>
#include <doomsday/world/sector.h>
#include <doomsday/help.h>

#include "dd_loop.h"
#include "def_main.h"
#include "busyrunner.h"
#include "con_config.h"
#include "sys_system.h"

#include "world/p_players.h"

#include "ui/infine/infinesystem.h"
#include "ui/nativeui.h"
#include "ui/progress.h"

#ifdef __CLIENT__
#  include "clientapp.h"
#  include "world/clientworld.h"

#  include "client/cl_def.h"
#  include "client/cl_infine.h"
#  include "client/cledgeloop.h"
#  include "world/subsector.h"
#  include "world/map.h"

#  include "gl/gl_main.h"
#  include "gl/gl_defer.h"
#  include "gl/gl_texmanager.h"
#  include "gl/svg.h"

#  include "network/net_main.h"
#  include "network/net_demo.h"

#  include "render/rendersystem.h"
#  include "render/cameralensfx.h"
#  include "render/r_draw.h" // R_InitViewWindow
#  include "render/r_main.h" // pspOffset
#  include "render/rend_main.h"
#  include "render/rend_font.h"
#  include "render/rend_particle.h" // Rend_ParticleLoadSystemTextures
#  include "render/vr.h"

#  include "world/contact.h"
#  include "resource/materialanimator.h"

#  include "ui/ui_main.h"
#  include "ui/inputsystem.h"
#  include "ui/busyvisual.h"
#  include "ui/sys_input.h"
#  include "ui/widgets/sidebarwidget.h"
#  include "ui/widgets/taskbarwidget.h"
#  include "ui/home/homewidget.h"

#  include "updater.h"
#  include "updater/updatedownloaddialog.h"

#  include <de/legacy/texgamma.h>
#  include <de/glwindow.h>
#  include <de/windowsystem.h>
#endif

#ifdef __SERVER__
#  include "serverapp.h"
#  include "serverworld.h"
#  include "network/net_main.h"
#  include "server/sv_def.h"
#  include <doomsday/world/map.h>
#endif

using namespace de;
using namespace res;
using World = world::World;

class ZipFileType : public NativeFileType
{
public:
    ZipFileType() : NativeFileType("FT_ZIP", RC_PACKAGE)
    {
        addKnownExtension(".pk3");
        addKnownExtension(".zip");
    }

    File1 *interpret(FileHandle &hndl, String path, const FileInfo &info) const
    {
        if (Zip::recognise(hndl))
        {
            LOG_AS("ZipFileType");
            LOG_RES_VERBOSE("Interpreted \"" + NativePath(path).pretty() + "\"");
            return new Zip(hndl, path, info);
        }
        return nullptr;
    }
};

class WadFileType : public NativeFileType
{
public:
    WadFileType() : NativeFileType("FT_WAD", RC_PACKAGE)
    {
        addKnownExtension(".wad");
    }

    File1 *interpret(FileHandle &hndl, String path, const FileInfo &info) const
    {
        if (Wad::recognise(hndl))
        {
            LOG_AS("WadFileType");
            LOG_RES_VERBOSE("Interpreted \"" + NativePath(path).pretty() + "\"");
            return new Wad(hndl, path, info);
        }
        return nullptr;
    }
};

static dint DD_StartupWorker(void *context);
static dint DD_DummyWorker(void *context);

dint isDedicated;
#ifdef __CLIENT__
dint symbolicEchoMode = false;     ///< @note Mutable via public API.
#endif

static char *startupFiles = const_cast<char *>("");  ///< List of file names, whitespace seperating (written to .cfg).

static void registerResourceFileTypes()
{
    FileType *ftype;

    //
    // Packages types:
    //
    ResourceClass &packageClass = App_ResourceClass("RC_PACKAGE");

    ftype = new ZipFileType();
    packageClass.addFileType(ftype);
    DD_AddFileType(*ftype);

    ftype = new WadFileType();
    packageClass.addFileType(ftype);
    DD_AddFileType(*ftype);

    ftype = new FileType("FT_LMP", RC_PACKAGE);  ///< Treat lumps as packages so they are mapped to $App.DataPath.
    ftype->addKnownExtension(".lmp");
    DD_AddFileType(*ftype);
    /// @todo ftype leaks. -jk

    //
    // Definition fileTypes:
    //
    ftype = new FileType("FT_DED", RC_DEFINITION);
    ftype->addKnownExtension(".ded");
    App_ResourceClass("RC_DEFINITION").addFileType(ftype);
    DD_AddFileType(*ftype);

    //
    // Graphic fileTypes:
    //
    ResourceClass &graphicClass = App_ResourceClass("RC_GRAPHIC");

    ftype = new FileType("FT_PNG", RC_GRAPHIC);
    ftype->addKnownExtension(".png");
    graphicClass.addFileType(ftype);
    DD_AddFileType(*ftype);

    ftype = new FileType("FT_TGA", RC_GRAPHIC);
    ftype->addKnownExtension(".tga");
    graphicClass.addFileType(ftype);
    DD_AddFileType(*ftype);

    ftype = new FileType("FT_JPG", RC_GRAPHIC);
    ftype->addKnownExtension(".jpg");
    graphicClass.addFileType(ftype);
    DD_AddFileType(*ftype);

    ftype = new FileType("FT_PCX", RC_GRAPHIC);
    ftype->addKnownExtension(".pcx");
    graphicClass.addFileType(ftype);
    DD_AddFileType(*ftype);

    //
    // Model fileTypes:
    //
    ResourceClass &modelClass = App_ResourceClass("RC_MODEL");

    ftype = new FileType("FT_DMD", RC_MODEL);
    ftype->addKnownExtension(".dmd");
    modelClass.addFileType(ftype);
    DD_AddFileType(*ftype);

    ftype = new FileType("FT_MD2", RC_MODEL);
    ftype->addKnownExtension(".md2");
    modelClass.addFileType(ftype);
    DD_AddFileType(*ftype);

    //
    // Sound fileTypes:
    //
    ftype = new FileType("FT_WAV", RC_SOUND);
    ftype->addKnownExtension(".wav");
    App_ResourceClass("RC_SOUND").addFileType(ftype);
    DD_AddFileType(*ftype);

    //
    // Music fileTypes:
    //
    ResourceClass &musicClass = App_ResourceClass("RC_MUSIC");

    ftype = new FileType("FT_OGG", RC_MUSIC);
    ftype->addKnownExtension(".ogg");
    musicClass.addFileType(ftype);
    DD_AddFileType(*ftype);

    ftype = new FileType("FT_MP3", RC_MUSIC);
    ftype->addKnownExtension(".mp3");
    musicClass.addFileType(ftype);
    DD_AddFileType(*ftype);

    ftype = new FileType("FT_MOD", RC_MUSIC);
    ftype->addKnownExtension(".mod");
    musicClass.addFileType(ftype);
    DD_AddFileType(*ftype);

    ftype = new FileType("FT_MID", RC_MUSIC);
    ftype->addKnownExtension(".mid");
    musicClass.addFileType(ftype);
    DD_AddFileType(*ftype);

    //
    // Font fileTypes:
    //
    ftype = new FileType("FT_DFN", RC_FONT);
    ftype->addKnownExtension(".dfn");
    App_ResourceClass("RC_FONT").addFileType(ftype);
    DD_AddFileType(*ftype);

    //
    // Misc fileTypes:
    //
    ftype = new FileType("FT_DEH", RC_PACKAGE);  ///< Treat DeHackEd patches as packages so they are mapped to $App.DataPath.
    ftype->addKnownExtension(".deh");
    DD_AddFileType(*ftype);
    /// @todo ftype leaks. -jk
}

#if 0
static void createPackagesScheme()
{
    FS1::Scheme &scheme = App_FileSystem().createScheme("Packages");

    //
    // Add default search paths.
    //
    // Note that the order here defines the order in which these paths are searched
    // thus paths must be added in priority order (newer paths have priority).
    //

#ifdef UNIX
    // There may be an iwaddir specified in a system-level config file.
    if (char *fn = UnixInfo_GetConfigValue("paths", "iwaddir"))
    {
        NativePath path = de::App::commandLine().startupPath() / fn;
        scheme.addSearchPath(SearchPath(res::Uri::fromNativeDirPath(path), SearchPath::NoDescend));
        LOG_RES_NOTE("Using paths.iwaddir: %s") << path.pretty();
        free(fn);
    }
#endif

    // Add paths to games bought with/using Steam.
    if (!CommandLine_Check("-nosteamapps"))
    {
        NativePath steamBase = DoomsdayApp::steamBasePath();
        if (!steamBase.isEmpty())
        {
            NativePath steamPath = steamBase / "SteamApps/common/";
            LOG_RES_NOTE("Using SteamApps path: %s") << steamPath.pretty();

            static String const appDirs[] =
            {
                "doom 2/base",
                "final doom/base",
                "heretic shadow of the serpent riders/base",
                "hexen/base",
                "hexen deathkings of the dark citadel/base",
                "ultimate doom/base",
                "DOOM 3 BFG Edition/base/wads",
                ""
            };
            for (dint i = 0; !appDirs[i].isEmpty(); ++i)
            {
                scheme.addSearchPath(SearchPath(res::Uri::fromNativeDirPath(steamPath / appDirs[i]),
                                                SearchPath::NoDescend));
            }
        }
    }

#ifdef UNIX
    NativePath systemWads("/usr/share/games/doom");
    if (systemWads.exists())
    {
        scheme.addSearchPath(SearchPath(res::Uri::fromNativeDirPath(systemWads),
                                        SearchPath::NoDescend));
    }
#endif

    // Add the path from the DOOMWADDIR environment variable.
    if (!CommandLine_Check("-nodoomwaddir") && getenv("DOOMWADDIR"))
    {
        NativePath path = App::commandLine().startupPath() / getenv("DOOMWADDIR");
        scheme.addSearchPath(SearchPath(res::Uri::fromNativeDirPath(path), SearchPath::NoDescend));
        LOG_RES_NOTE("Using DOOMWADDIR: %s") << path.pretty();
    }

    // Add any paths from the DOOMWADPATH environment variable.
    if (!CommandLine_Check("-nodoomwadpath") && getenv("DOOMWADPATH"))
    {
#if WIN32
#  define SEP_CHAR      ';'
#else
#  define SEP_CHAR      ':'
#endif

        QStringList allPaths = String(getenv("DOOMWADPATH")).split(SEP_CHAR, String::SkipEmptyParts);
        for (dint i = allPaths.count(); i--> 0; )
        {
            NativePath path = App::commandLine().startupPath() / allPaths[i];
            scheme.addSearchPath(SearchPath(res::Uri::fromNativeDirPath(path), SearchPath::NoDescend));
            LOG_RES_NOTE("Using DOOMWADPATH: %s") << path.pretty();
        }

#undef SEP_CHAR
    }

    scheme.addSearchPath(SearchPath(res::makeUri("$(App.DataPath)/"), SearchPath::NoDescend));
    scheme.addSearchPath(SearchPath(res::makeUri("$(App.DataPath)/$(GamePlugin.Name)/"), SearchPath::NoDescend));
}
#endif

void DD_CreateFileSystemSchemes()
{
    const dint schemedef_max_searchpaths = 5;
    struct schemedef_s {
        const char *name;
        const char *optOverridePath;
        const char *optFallbackPath;
        Flags flags;
        Flags searchPathFlags;
        /// Priority is right to left.
        const char *searchPaths[schemedef_max_searchpaths];
    } defs[] = {
        { "Defs",         nullptr,           nullptr,     FS1::Scheme::Flag(0), 0,
            { "$(App.DefsPath)/", "$(App.DefsPath)/$(GamePlugin.Name)/", "$(App.DefsPath)/$(GamePlugin.Name)/$(Game.IdentityKey)/" }
        },
        { "Graphics",     "-gfxdir2",     "-gfxdir",      FS1::Scheme::Flag(0), 0,
            { "$(App.DataPath)/graphics/" }
        },
        { "Models",       "-modeldir2",   "-modeldir",    FS1::Scheme::MappedInPackages, 0,
            { "$(App.DataPath)/$(GamePlugin.Name)/models/", "$(App.DataPath)/$(GamePlugin.Name)/models/$(Game.IdentityKey)/" }
        },
        { "Sfx",          "-sfxdir2",     "-sfxdir",      FS1::Scheme::MappedInPackages, SearchPath::NoDescend,
            { "$(App.DataPath)/$(GamePlugin.Name)/sfx/", "$(App.DataPath)/$(GamePlugin.Name)/sfx/$(Game.IdentityKey)/" }
        },
        { "Music",        "-musdir2",     "-musdir",      FS1::Scheme::MappedInPackages, SearchPath::NoDescend,
            { "$(App.DataPath)/$(GamePlugin.Name)/music/", "$(App.DataPath)/$(GamePlugin.Name)/music/$(Game.IdentityKey)/" }
        },
        { "Textures",     "-texdir2",     "-texdir",      FS1::Scheme::MappedInPackages, SearchPath::NoDescend,
            { "$(App.DataPath)/$(GamePlugin.Name)/textures/", "$(App.DataPath)/$(GamePlugin.Name)/textures/$(Game.IdentityKey)/" }
        },
        { "Flats",        "-flatdir2",    "-flatdir",     FS1::Scheme::MappedInPackages, SearchPath::NoDescend,
            { "$(App.DataPath)/$(GamePlugin.Name)/flats/", "$(App.DataPath)/$(GamePlugin.Name)/flats/$(Game.IdentityKey)/" }
        },
        { "Patches",      "-patdir2",     "-patdir",      FS1::Scheme::MappedInPackages, SearchPath::NoDescend,
            { "$(App.DataPath)/$(GamePlugin.Name)/patches/", "$(App.DataPath)/$(GamePlugin.Name)/patches/$(Game.IdentityKey)/" }
        },
        { "LightMaps",    "-lmdir2",      "-lmdir",       FS1::Scheme::MappedInPackages, 0,
            { "$(App.DataPath)/$(GamePlugin.Name)/lightmaps/" }
        },
        { "Fonts",        "-fontdir2",    "-fontdir",     FS1::Scheme::MappedInPackages, SearchPath::NoDescend,
            { "$(App.DataPath)/fonts/", "$(App.DataPath)/$(GamePlugin.Name)/fonts/", "$(App.DataPath)/$(GamePlugin.Name)/fonts/$(Game.IdentityKey)/" }
        }
    };

    //createPackagesScheme();

    // Setup the rest...
    for (const schemedef_s &def : defs)
    {
        FS1::Scheme &scheme = App_FileSystem().createScheme(def.name, def.flags);

        dint searchPathCount = 0;
        while (def.searchPaths[searchPathCount] && ++searchPathCount < schemedef_max_searchpaths)
        {}

        for (dint i = 0; i < searchPathCount; ++i)
        {
            scheme.addSearchPath(SearchPath(res::makeUri(def.searchPaths[i]), def.searchPathFlags));
        }

        if (def.optOverridePath && CommandLine_CheckWith(def.optOverridePath, 1))
        {
            NativePath path = NativePath(CommandLine_NextAsPath());
            scheme.addSearchPath(SearchPath(res::Uri::fromNativeDirPath(path), def.searchPathFlags), FS1::OverridePaths);
            path = path / "$(Game.IdentityKey)";
            scheme.addSearchPath(SearchPath(res::Uri::fromNativeDirPath(path), def.searchPathFlags), FS1::OverridePaths);
        }

        if (def.optFallbackPath && CommandLine_CheckWith(def.optFallbackPath, 1))
        {
            NativePath path = NativePath(CommandLine_NextAsPath());
            scheme.addSearchPath(SearchPath(res::Uri::fromNativeDirPath(path), def.searchPathFlags), FS1::FallbackPaths);
        }
    }
}

void App_Error(const char *error, ...)
{
    static bool errorInProgress = false;

    LogBuffer_Flush();

    char buff[2048], err[256];
    va_list argptr;

#ifdef __CLIENT__
    ClientWindow::main().eventHandler().trapMouse(false);
#endif

    // Already in an error?
    if (errorInProgress)
    {
        va_start(argptr, error);
        dd_vsnprintf(buff, sizeof(buff), error, argptr);
        va_end(argptr);

#if defined (__CLIENT__) && defined (DE_HAVE_BUSYRUNNER)
        if (!ClientApp::busyRunner().inWorkerThread())
        {
            Sys_MessageBox(MBT_ERROR, DOOMSDAY_NICENAME, buff, 0);
        }
#endif

        // Exit immediately, lest we go into an infinite loop.
        exit(1);
    }

    // We've experienced a fatal error; program will be shut down.
    errorInProgress = true;

    // Get back to the directory we started from.
    //Dir_SetCurrent(DD_RuntimePath());

    va_start(argptr, error);
    dd_vsnprintf(err, sizeof(err), error, argptr);
    va_end(argptr);

    LOG_CRITICAL("") << err;
    LogBuffer_Flush();

    strcpy(buff, "");
    strcat(buff, "\n");
    strcat(buff, err);

    if (BusyMode_Active())
    {
        DoomsdayApp::app().busyMode().abort(buff);

#if defined (__CLIENT__) && defined (DE_HAVE_BUSYRUNNER)
        if (ClientApp::busyRunner().inWorkerThread())
        {
            // We should not continue to execute the worker any more.
            // The thread will be terminated imminently.
            for (;;) Thread_Sleep(10000);
        }
#endif
    }
    else
    {
        App_AbnormalShutdown(buff);
    }
    exit(-1);
}

void App_AbnormalShutdown(const char *message)
{
    DE_ASSERT_IN_MAIN_THREAD();

#ifdef __CLIENT__
    // This is a crash landing, better be safe than sorry.
    DoomsdayApp::app().busyMode().setTaskRunner(nullptr);
#endif

    Sys_Shutdown();

#ifdef __CLIENT__
    //DisplayMode_Shutdown();
    DE_GUI_APP->loop().pause();

    // This is an abnormal shutdown, we cannot continue drawing any of the
    // windows. (Alternatively could hide/disable drawing of the windows.) Note
    // that the app's event loop is running normally while we show the native
    // message box below -- if the app windows are not hidden/closed, they might
    // receive draw events.
    ClientApp::windowSystem().forAll([] (GLWindow &win) {
        win.hide();
        return LoopContinue;
    });
#endif

    if (message) // Only show if a message given.
    {
        // Make sure all the buffered stuff goes into the file.
        LogBuffer_Flush();

        /// @todo Get the actual output filename (might be a custom one).
        Sys_MessageBoxWithDetailsFromFile(MBT_ERROR,
                                          DOOMSDAY_NICENAME,
                                          message,
                                          "See the doomsday.out log file for more details.",
                                          LogBuffer::get().outputFile());
    }

    //Sys_Shutdown();
    DD_Shutdown();

    Garbage_ForgetAndLeak(); // At this point, it's too late.

    // Get outta here.
    exit(1);
}

AudioSystem &App_AudioSystem()
{
    if (App::appExists())
    {
#ifdef __CLIENT__
        if (ClientApp::hasAudio())
        {
            return ClientApp::audio();
        }
#endif
#ifdef __SERVER__
        return ServerApp::audio();
#endif
    }
    throw Error("App_AudioSystem", "App not yet initialized");
}

#ifdef __CLIENT__
ClientResources &App_Resources()
{
    return ClientResources::get();
}

ClientWorld &App_World()
{
    return ClientApp::classicWorld();
}
#else
Resources &App_Resources()
{
    return Resources::get();
}

ServerWorld &App_World()
{
    return static_cast<ServerWorld &>(world::World::get());
}
#endif

InFineSystem &App_InFineSystem()
{
    if (App::appExists())
    {
#ifdef __CLIENT__
        return ClientApp::infine();
#endif
#ifdef __SERVER__
        return ServerApp::infine();
#endif
    }
    throw Error("App_InFineSystem", "App not yet initialized");
}

#undef Con_Open
void Con_Open(dint yes)
{
#ifdef __CLIENT__
    if (yes)
    {
        ClientWindow &win = ClientWindow::main();
        win.taskBar().open();
        win.root().setFocus(&win.console().commandLine());
    }
    else
    {
        ClientWindow::main().console().closeLog();
    }
#endif

#ifdef __SERVER__
    DE_UNUSED(yes);
#endif
}

#ifdef __CLIENT__

/**
 * Console command to open/close the console prompt.
 */
D_CMD(OpenClose)
{
    DE_UNUSED(src, argc);

    if (!stricmp(argv[0], "conopen"))
    {
        Con_Open(true);
    }
    else if (!stricmp(argv[0], "conclose"))
    {
        Con_Open(false);
    }
    else
    {
        Con_Open(!ClientWindow::main().console().isLogOpen());
    }
    return true;
}

D_CMD(TaskBar)
{
    DE_UNUSED(src, argc, argv);

    ClientWindow &win = ClientWindow::main();
    if (!win.taskBar().isOpen() || !win.console().commandLine().hasFocus())
    {
        win.taskBar().open();
        win.console().focusOnCommandLine();
    }
    else
    {
        win.taskBar().close();
    }
    return true;
}

D_CMD(PackagesSidebar)
{
    DE_UNUSED(src, argc, argv);

    if (!DoomsdayApp::isGameLoaded()) return false;

    ClientWindow &win = ClientWindow::main();
    if (!win.hasSidebar())
    {
        win.taskBar().openPackagesSidebar();
    }
    else
    {
        win.sidebar().as<SidebarWidget>().close();
    }
    return true;
}

D_CMD(Tutorial)
{
    DE_UNUSED(src, argc, argv);
    ClientWindow::main().taskBar().showTutorial();
    return true;
}

#endif // __CLIENT__

int DD_ActivateGameWorker(void *context)
{
    DoomsdayApp::GameChangeParameters &parms = *(DoomsdayApp::GameChangeParameters *) context;

    auto &plugins = DoomsdayApp::plugins();
    auto &resSys = App_Resources();

    // Some resources types are located prior to initializing the game.
    auto &textures = res::Textures::get();
    textures.initTextures();
    textures.textureScheme("Lightmaps").clear();
    textures.textureScheme("Flaremaps").clear();
    resSys.mapManifests().initMapManifests();

    if (parms.initiatedBusyMode)
    {
        Con_SetProgress(50);
    }

    // Now that resources have been located we can begin to initialize the game.
    if (App_GameLoaded())
    {
        // Any game initialization hooks?
        plugins.callAllHooks(HOOK_GAME_INIT);

        if (gx.PreInit)
        {
            DE_ASSERT(App_CurrentGame().pluginId() != 0);

            plugins.setActivePluginId(App_CurrentGame().pluginId());
            gx.PreInit(App_CurrentGame().id());
            plugins.setActivePluginId(0);
        }
    }

    if (parms.initiatedBusyMode)
    {
        Con_SetProgress(100);
    }

    if (App_GameLoaded())
    {
        const File *configFile;

        // Parse the game's main config file.
        // If a custom top-level config is specified; let it override.
        if (CommandLine_CheckWith("-config", 1))
        {
            Con_ParseCommands(NativePath(CommandLine_NextAsPath()));
        }
        else
        {
            configFile = FS::tryLocate<const File>(App_CurrentGame().mainConfig());
            Con_SetDefaultPath(App_CurrentGame().mainConfig());

            // This will be missing on the first launch.
            if (configFile)
            {
                LOG_SCR_NOTE("Parsing primary config %s...") << configFile->description();
                Con_ParseCommands(*configFile);
            }
        }
        Con_SetAllowed(CPCF_ALLOW_SAVE_STATE);

#ifdef __CLIENT__
        // Apply default control bindings for this game.
        ClientApp::input().bindGameDefaults();

        // Read bindings for this game and merge with the working set.
        if ((configFile = FS::tryLocate<const File>(App_CurrentGame().bindingConfig()))
                != nullptr)
        {
            Con_ParseCommands(*configFile);
        }
        Con_SetAllowed(CPCF_ALLOW_SAVE_BINDINGS);
#endif
    }

    if (parms.initiatedBusyMode)
    {
        Con_SetProgress(120);
    }

    Def_Read();

    if (parms.initiatedBusyMode)
    {
        Con_SetProgress(130);
    }

    resSys.sprites().initSprites(); // Fully initialize sprites.
#ifdef __CLIENT__
    resSys.initModels();
#endif

    Def_PostInit();

    DD_ReadGameHelp();

    // Reset the tictimer so than any fractional accumulation is not added to
    // the tic/game timer of the newly-loaded game.
    gameTime = 0;
    DD_ResetTimer();

#ifdef __CLIENT__
    // Make sure that the next frame does not use a filtered viewer.
    R_ResetViewer();
#endif

    // Init player values.
    DoomsdayApp::players().forAll([] (Player &plr)
    {
        plr.extraLight        = 0;
        plr.targetExtraLight  = 0;
        plr.extraLightCounter = 0;
        return LoopContinue;
    });

    if (gx.PostInit)
    {
        plugins.setActivePluginId(App_CurrentGame().pluginId());
        gx.PostInit();
        plugins.setActivePluginId(0);
    }

    if (parms.initiatedBusyMode)
    {
        Con_SetProgress(200);
    }

    return 0;
}

Games &App_Games()
{
    if (App::appExists())
    {
#ifdef __CLIENT__
        return ClientApp::games();
#endif
#ifdef __SERVER__
        return ServerApp::games();
#endif
    }
    throw Error("App_Games", "App not yet initialized");
}

void App_ClearGames()
{
    App_Games().clear();
    DoomsdayApp::setGame(App_Games().nullGame());
}

static void populateGameInfo(GameInfo &info, const Game &game)
{
    info.identityKey = AutoStr_FromTextStd(game.id());
    info.title       = AutoStr_FromTextStd(game.title());
    info.author      = AutoStr_FromTextStd(game.author());
}

/// @note Part of the Doomsday public API.
#undef DD_GameInfo
dd_bool DD_GameInfo(GameInfo *info)
{
    LOG_AS("DD_GameInfo");
    if (!info) return false;

    zapPtr(info);

    if (App_GameLoaded())
    {
        populateGameInfo(*info, App_CurrentGame());
        return true;
    }

    LOGDEV_WARNING("No game currently loaded");
    return false;
}

const Game &App_CurrentGame()
{
    return DoomsdayApp::game();
}

static GameProfile automaticProfile;

static const GameProfile *autoselectGameProfile()
{
    if (auto arg = CommandLine::get().check("-game", 1))
    {
        const String param = arg.params.first();
        Games &games = DoomsdayApp::games();

        // The argument can be a game ID or a profile name.
        if (games.contains(param))
        {
            auto &prof = DoomsdayApp::gameProfiles().find(games[param].title()).as<GameProfile>();
            prof.setLastPlayedAt();
            automaticProfile = prof;
        }
        else if (auto *prof = maybeAs<GameProfile>(DoomsdayApp::gameProfiles().tryFind(param)))
        {
            prof->setLastPlayedAt();
            automaticProfile = *prof;
        }

        // Packages from the command line.
        for (String packageId : PackageLoader::get().loadedFromCommandLine())
        {
            StringList pkgs = automaticProfile.packages();
            pkgs << packageId;
            automaticProfile.setPackages(pkgs);
        }

        // Also append the packages specified as files on the command line.
        for (File *f : DoomsdayApp::app().filesFromCommandLine())
        {
            String packageId;
            if (const auto *bundle = maybeAs<DataBundle>(f))
            {
                packageId = bundle->packageId();
            }
            else if (f->extension() == ".pack")
            {
                packageId = Package::identifierForFile(*f);
            }
            else
            {
                LOG_RES_WARNING("Unknown file %s will not be loaded")
                        << f->description();
            }

            if (!packageId.isEmpty())
            {
                StringList pkgs = automaticProfile.packages();
                pkgs << packageId;
                automaticProfile.setPackages(pkgs);
            }
        }

        if (automaticProfile.isPlayable())
        {
            return &automaticProfile;
        }
    }

    // We don't know what to do.
    return nullptr;
}

dint DD_EarlyInit()
{
    // Determine the requested degree of verbosity.
    DoomsdayApp::verbose = CommandLine_Exists("-verbose");

#ifdef __SERVER__
    ::isDedicated = true;
#else
    ::isDedicated = false;
#endif

    // Bring the console online as soon as we can.
    DD_ConsoleInit();
    Con_InitDatabases();

    // Register the engine's console commands and variables.
    DD_ConsoleRegister();

    return true;
}

// Perform basic runtime type size checks.
#ifdef DE_DEBUG
static void assertTypeSizes()
{
    void *ptr = 0;
    int32_t int32 = 0;
    int16_t int16 = 0;
    dfloat float32 = 0;

    DE_UNUSED(ptr);
    DE_UNUSED(int32);
    DE_UNUSED(int16);
    DE_UNUSED(float32);

    ASSERT_32BIT(int32);
    ASSERT_16BIT(int16);
    ASSERT_32BIT(float32);
#ifdef __64BIT__
    ASSERT_64BIT(ptr);
    ASSERT_64BIT(int64_t);
#else
    ASSERT_NOT_64BIT(ptr);
#endif
}
#endif

/**
 * Engine initialization. Once completed the game loop is ready to be started.
 * Called from the app entrypoint function.
 */
static void initializeWithWindowReady()
{
    DE_DEBUG_ONLY( assertTypeSizes(); )

    static const char *AUTOEXEC_NAME = "autoexec.cfg";

#ifdef __CLIENT__
    GLWindow::glActivateMain();
    GL_EarlyInit();
#endif

    // Initialize the subsystems needed prior to entering busy mode for the first time.
    Sys_Init();
    ResourceClass::setResourceClassCallback(App_ResourceClass);
    registerResourceFileTypes();
    F_Init();
    DD_CreateFileSystemSchemes();

#ifdef __CLIENT__
    FR_Init();

    // Enter busy mode until startup complete.
    //Con_InitProgress(200);
#endif

    BusyMode_RunNewTaskWithName(BUSYF_NO_UPLOADS | BUSYF_STARTUP | (DoomsdayApp::verbose? BUSYF_CONSOLE_OUTPUT : 0),
                                DD_StartupWorker, 0, "Starting up...");

    // Engine initialization is complete. Now finish up with the GL.
#ifdef __CLIENT__
    GL_Init();
    GL_InitRefresh();
    App_Resources().clearAllTextureSpecs();
    LensFx_Init();
    R_InitViewWindow();
    UI_LoadFonts();
#endif
    App_Resources().initSystemTextures();

//#ifdef __CLIENT__
//    // Do deferred uploads.
//    Con_SetProgress(100);
//#endif
    BusyMode_RunNewTaskWithName(BUSYF_STARTUP | (DoomsdayApp::verbose? BUSYF_CONSOLE_OUTPUT : 0),
                                DD_DummyWorker, 0, "Buffering...");

    //
    // Try to locate all required data files for all registered games.
    //
//#ifdef __CLIENT__
//    Con_SetProgress(200);
//#endif

    App_Games().checkReadiness();

    // Attempt automatic game selection.
    if (!CommandLine_Exists("-noautoselect") || isDedicated)
    {
        if (const GameProfile *game = autoselectGameProfile())
        {
#ifdef __CLIENT__
            ClientWindow::main().home().moveOffscreen(0.0);
#endif
            // Begin the game session.
            DoomsdayApp::app().changeGame(*game, DD_ActivateGameWorker);
        }
#ifdef __SERVER__
        else
        {
            // A server is presently useless without a game, as shell
            // connections can only be made after a game is loaded and the
            // server mode started.
            /// @todo Allow shell connections in Home, too.
            String msg = "Could not determine which game to start. "
                         "Please specify one with the " _E(b) "-game" _E(.) " option. ";
            const auto playable = DoomsdayApp::gameProfiles().allPlayableProfiles();
            if (playable.isEmpty())
            {
                msg += "However, it seems all games are missing required data files. "
                       "Check that the " _E(b) "-iwad" _E(.) " option specifies a "
                       "folder with game WAD files.";
            }
            else
            {
                StringList ids;
                for (const GameProfile *prof : playable) ids << prof->gameId();
                msg += "The following games are playable: " + String::join(ids, ", ");
            }
            EscapeParser esc;
            esc.parse(msg);
            App_Error("%s", esc.plainText().c_str());
        }
#endif
    }

    FS_InitPathLumpMappings();

    // Re-initialize the filesystem subspace schemes as there are now new
    // resources to be found on existing search paths (probably that is).
    App_FileSystem().resetAllSchemes();

    //
    // One-time execution of various command line features available during startup.
    //
    if (CommandLine_CheckWith("-dumplump", 1))
    {
        String name = CommandLine_Next();
        lumpnum_t lumpNum = App_FileSystem().lumpNumForName(name);
        if (lumpNum >= 0)
        {
            F_DumpFile(App_FileSystem().lump(lumpNum), 0);
        }
        else
        {
            LOG_RES_WARNING("Cannot dump unknown lump \"%s\"") << name;
        }
    }

    if (CommandLine_Check("-dumpwaddir"))
    {
        Con_Executef(CMDS_CMDLINE, false, "listlumps");
    }

    // Try to load the autoexec file. This is done here to make sure everything is
    // initialized: the user can do here anything that s/he'd be able to do in-game
    // provided a game was loaded during startup.
    Con_ParseCommands(App::app().nativeHomePath() / AUTOEXEC_NAME);

    // Read additional config files that should be processed post engine init.
    if (CommandLine_CheckWith("-parse", 1))
    {
        LOG_AS("-parse");
        Time begunAt;
        for (;;)
        {
            const char *arg = CommandLine_NextAsPath();
            if (!arg || arg[0] == '-') break;

            LOG_NOTE("Additional pre-init config file \"%s\"") << NativePath(arg).pretty();
            Con_ParseCommands(NativePath(arg));
        }
        LOGDEV_SCR_VERBOSE("Completed in %.2f seconds") << begunAt.since();
    }

    // A console command on the command line?
    for (dint p = 1; p < CommandLine_Count() - 1; p++)
    {
        if (stricmp(CommandLine_At(p), "-command") &&
            stricmp(CommandLine_At(p), "-cmd"))
        {
            continue;
        }

        for (++p; p < CommandLine_Count(); p++)
        {
            const char *arg = CommandLine_At(p);

            if (arg[0] == '-')
            {
                p--;
                break;
            }
            Con_Execute(CMDS_CMDLINE, arg, false, false);
        }
    }

    //
    // One-time execution of network commands on the command line.
    // Commands are only executed if we have loaded a game during startup.
    //
    if (App_GameLoaded())
    {
        // Client connection command.
        if (CommandLine_CheckWith("-connect", 1))
        {
            Con_Executef(CMDS_CMDLINE, false, "connect %s", CommandLine_Next());
        }

        // Incoming TCP port.
        if (CommandLine_CheckWith("-port", 1))
        {
            Con_Executef(CMDS_CMDLINE, false, "net-ip-port %s", CommandLine_Next());
        }

#ifdef __SERVER__
        // Automatically start the server.
        N_ServerOpen();
#endif
    }
    else
    {
        // No game loaded.
        // Lets get most of everything else initialized.
        // Reset file IDs so previously seen files can be processed again.
        App_FileSystem().resetFileIds();
        FS_InitPathLumpMappings();
        FS_InitVirtualPathMappings();
        App_FileSystem().resetAllSchemes();

        auto &textures = res::Textures::get();
        textures.initTextures();
        textures.textureScheme("Lightmaps").clear();
        textures.textureScheme("Flaremaps").clear();
        App_Resources().mapManifests().initMapManifests();

        Def_Read();

        App_Resources().sprites().initSprites();
#ifdef __CLIENT__
        App_Resources().initModels();
#endif

        Def_PostInit();

        if (!CommandLine_Exists("-noautoselect"))
        {
            LOG_NOTE("Game could not be selected automatically");
        }
    }
}

/**
 * This gets called when the main window is ready for GL init. The application
 * event loop is already running.
 */
void DD_FinishInitializationAfterWindowReady()
{
    LOGDEV_MSG("Window is ready, finishing initialization");

#ifdef __CLIENT__
//# ifdef WIN32
    // Now we can get the color transfer table as the window is available.
    //DisplayMode_SaveOriginalColorTransfer();
//# endif
    if (!Sys_GLInitialize())
    {
        App_Error("Error initializing OpenGL.\n");
    }
    else
    {
        ClientWindow::main().setTitle(DD_ComposeMainWindowTitle());
    }
#endif // __CLIENT__

    // Initialize engine subsystems and initial state.
    Loop::timer(0.01, []() {
        try
        {
            // The rest of the initialization assumes that the main window exists.
            initializeWithWindowReady();
            // Let everyone know we're up and running.
            App::app().notifyStartupComplete();
            return;
        }
        catch (const Error &er)
        {
            EscapeParser esc;
            esc.parse(er.asText());
            Sys_CriticalMessage(esc.plainText());
        }
        catch (...)
        {}
        // Shut down the application.
#if defined (__CLIENT__)
        DE_GUI_APP->quit(2);
#else
        DE_TEXT_APP->quit(2);
#endif
    });
}

static dint DD_StartupWorker(void * /*context*/)
{
#ifdef WIN32
    // Initialize COM for this thread (needed for DirectInput).
    CoInitialize(nullptr);
#endif
    //Con_SetProgress(10);

    // Any startup hooks?
    DoomsdayApp::plugins().callAllHooks(HOOK_STARTUP);
    //Con_SetProgress(20);

    // Was the change to userdir OK?
    /*if (CommandLine_CheckWith("-userdir", 1) && !DoomsdayApp::app().isUsingUserDir())
    {
        LOG_WARNING("User directory not found (check -userdir)");
    }*/

    FS_InitVirtualPathMappings();
    App_FileSystem().resetAllSchemes();

    //Con_SetProgress(40);

    Net_Init();
    Sys_HideMouseCursor();

    // Read config files that should be read BEFORE engine init.
    if (CommandLine_CheckWith("-cparse", 1))
    {
        Time begunAt;
        LOG_AS("-cparse")

        for (;;)
        {
            const char *arg = CommandLine_NextAsPath();
            if (!arg || arg[0] == '-') break;

            LOG_MSG("Additional (pre-init) config file \"%s\"") << NativePath(arg).pretty();
            Con_ParseCommands(arg);
        }
        LOGDEV_SCR_VERBOSE("Completed in %.2f seconds") << begunAt.since();
    }

    //
    // Add required engine resource files.
    //

    // Make sure all files have been found so we can determine which games are playable.
    /*Folder::waitForPopulation(
            #if defined (__SERVER__)
                Folder::BlockingMainThread
            #endif
                );
    DoomsdayApp::bundles().waitForEverythingIdentified();*/
    FS::waitForIdle();

    /*String foundPath = App_FileSystem().findPath(res::Uri("doomsday.pk3", RC_PACKAGE),
                                                 RLF_DEFAULT, App_ResourceClass(RC_PACKAGE));
    foundPath = App_BasePath() / foundPath;  // Ensure the path is absolute.
    File1 *loadedFile = File1::tryLoad(res::makeUri(foundPath));
    DE_ASSERT(loadedFile);
    DE_UNUSED(loadedFile);*/

    // It is assumed that doomsday.pk3 is currently stored in a native file.
    if (const File *basePack = App::packageLoader().select("net.dengine.legacy.base"))
    {
        // The returned file is a symlink to the actual data file.
        // Since we're loading with FS1, we need to look up the native path.
        // The data file is an interpreter in /local/wads, whose source is the native file.
        File1::tryLoad(File1::LoadAsVanillaFile,
                       res::DoomsdayPackage::loadableUri(*basePack));
    }
    else
    {
        throw Error("DD_StartupWorker", "Failed to find \"net.dengine.legacy.base\" package");
    }

    // No more files or packages will be loaded in "startup mode" after this point.
    App_FileSystem().endStartup();

    // Load engine help resources.
    DD_InitHelp();
    //Con_SetProgress(60);

    // Execute the startup script (Startup.cfg).
    const char *startupConfig = "startup.cfg";
    if (F_FileExists(startupConfig))
    {
        Con_ParseCommands(startupConfig);
    }
    //Con_SetProgress(90);

#ifdef __CLIENT__
    R_BuildTexGammaLut(texGamma);
    R_InitSvgs();
    R_ResetFrameCount();
#endif
    //Con_SetProgress(165);

    Net_InitGame();
#ifdef __CLIENT__
    Demo_Init();
#endif
    //Con_SetProgress(190);

    // In dedicated mode the console must be opened, so all input events
    // will be handled by it.
    if (isDedicated)
    {
        Con_Open(true);
    }
    //Con_SetProgress(199);

    // Any initialization hooks?
    DoomsdayApp::plugins().callAllHooks(HOOK_INIT);
    //Con_SetProgress(200);

    // Release all cached uncompressed entries. If the contents of the compressed
    // files are needed, they will be decompressed and cached again.
    DoomsdayApp::app().uncacheFilesFromMemory();

#ifdef WIN32
    // This thread has finished using COM.
    CoUninitialize();
#endif

    return 0;
}

#if 1
/**
 * This only exists so we have something to call while the deferred uploads of the
 * startup are processed.
 */
static dint DD_DummyWorker(void * /*context*/)
{
    Con_SetProgress(200);
    return 0;
}
#endif

void DD_CheckTimeDemo()
{
    static bool checked = false;

    if (!checked)
    {
        checked = true;
        if (CommandLine_CheckWith("-timedemo", 1) || // Timedemo mode.
            CommandLine_CheckWith("-playdemo", 1))   // Play-once mode.
        {
            Con_Execute(
                CMDS_CMDLINE, Stringf("playdemo %s", CommandLine_Next()), false, false);
        }
    }
}

static dint DD_UpdateEngineStateWorker(void *context)
{
    DE_ASSERT(context);
    const auto initiatedBusyMode = *static_cast<bool *>(context);

#ifdef __CLIENT__
    if (!novideo)
    {
        GL_InitRefresh();
        App_Resources().clearAllTextureSpecs();
    }
#endif
    App_Resources().initSystemTextures();

    if (initiatedBusyMode)
    {
        Con_SetProgress(50);
    }

    // Allow previously seen files to be processed again.
    App_FileSystem().resetFileIds();

    // Re-read definitions.
    Def_Read();

    //
    // Rebuild resource data models (defs might've changed).
    //
    App_Resources().sprites().initSprites();
#ifdef __CLIENT__
    App_Resources().clearAllRawTextures();
    App_Resources().initModels();
#endif
    Def_PostInit();

    //
    // Update misc subsystems.
    //
    App_World().update();

#ifdef __CLIENT__
    // Recalculate the light range mod matrix.
    Rend_UpdateLightModMatrix();
    // The rendering lists have persistent data that has changed during the
    // re-initialization.
    ClientApp::render().clearDrawLists();
#endif

    /// @todo fixme: Update the game title and the status.

#ifdef DE_DEBUG
    Z_CheckHeap();
#endif

    if (initiatedBusyMode)
    {
        Con_SetProgress(200);
    }
    return 0;
}

void DD_UpdateEngineState()
{
    LOG_MSG("Updating engine state...");

#ifdef __CLIENT__
    // Stop playing sounds and music.
    App_AudioSystem().reset();

    BusyMode_FreezeGameForBusyMode();
    GL_SetFilter(false);
    Demo_StopPlayback();
    Rend_ResetLookups();
#endif

    //App_FileSystem().resetFileIds();

    // Update the dir/WAD translations.
    FS_InitPathLumpMappings();
    FS_InitVirtualPathMappings();
    // Re-build the filesystem subspace schemes as there may be new resources to be found.
    App_FileSystem().resetAllSchemes();

    res::Textures::get().initTextures();
    Resources::get().mapManifests().initMapManifests();

    if (App_GameLoaded() && gx.UpdateState)
    {
        gx.UpdateState(DD_PRE);
    }

#ifdef __CLIENT__
    dd_bool hadFog = fogParams.usingFog;

    GL_TotalReset();
    GL_TotalRestore(); // Bring GL back online.

    // Make sure the fog is enabled, if necessary.
    if (hadFog)
    {
        GL_UseFog(true);
    }
#endif

    // The bulk of this we can do in busy mode unless we are already busy
    // (which can happen during a runtime game change).
    bool initiatedBusyMode = !BusyMode_Active();
    if (initiatedBusyMode)
    {
#ifdef __CLIENT__
        Con_InitProgress(200);
#endif
        BusyMode_RunNewTaskWithName(BUSYF_ACTIVITY | BUSYF_PROGRESS_BAR | (DoomsdayApp::verbose? BUSYF_CONSOLE_OUTPUT : 0),
                                    DD_UpdateEngineStateWorker, &initiatedBusyMode,
                                    "Updating engine state...");
    }
    else
    {
        /// @todo Update the current task name and push progress.
        DD_UpdateEngineStateWorker(&initiatedBusyMode);
    }

    if (App_GameLoaded() && gx.UpdateState)
    {
        gx.UpdateState(DD_POST);
    }

#ifdef __CLIENT__
    world::Materials::get().forAllMaterials([] (world::Material &material)
    {
        return static_cast<ClientMaterial &>(material).forAllAnimators([] (MaterialAnimator &animator)
        {
            animator.rewind();
            return LoopContinue;
        });
    });
#endif
}

struct ddvalue_t
{
    dint *readPtr;
    dint *writePtr;
};

static ddvalue_t ddValues[DD_LAST_VALUE - DD_FIRST_VALUE] = {
    {&novideo, 0},
    {&netGame, 0},
    {&isServer, 0}, // An *open* server?
    {&isClient, 0},
    {&consolePlayer, &consolePlayer},
    {&displayPlayer, 0}, // use R_SetViewPortPlayer() to change
    {&gotFrame, 0},
    {0, 0}, // pointer updated when queried (DED sound def count)

#ifdef __SERVER__
    {&allowFrames, &allowFrames},
#else
    {0, 0},
#endif

#ifdef __CLIENT__
    {&levelFullBright, &levelFullBright},
    {&gameReady, &gameReady},
    {&playback, 0},
    {&clientPaused, &clientPaused},
    {&weaponOffsetScaleY, &weaponOffsetScaleY},
    {&gameDrawHUD, 0},
    {&symbolicEchoMode, &symbolicEchoMode},
    {&rendLightAttenuateFixedColormap, &rendLightAttenuateFixedColormap},
#else
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
#endif
};

#undef DD_GetInteger
/**
 * Get a 32-bit signed integer value.
 */
dint DD_GetInteger(dint ddvalue)
{
    switch (ddvalue)
    {
#ifdef __CLIENT__
    case DD_SHIFT_DOWN:
        return dint( ClientApp::input().shiftDown() );

    case DD_WINDOW_WIDTH:
        return DE_GAMEVIEW_WIDTH;

    case DD_WINDOW_HEIGHT:
        return DE_GAMEVIEW_HEIGHT;

    case DD_CURRENT_CLIENT_FINALE_ID:
        return Cl_CurrentFinale();

    case DD_DYNLIGHT_TEXTURE:
        return dint( GL_PrepareLSTexture(LST_DYNAMIC) );

    case DD_USING_HEAD_TRACKING:
        return vrCfg().mode() == VRConfig::OculusRift && vrCfg().oculusRift().isReady();
#endif

    case DD_NUMMOBJTYPES:
        return DED_Definitions()->things.size();

    case DD_MAP_MUSIC:
        if (World::get().hasMap())
        {
            const Record &mapInfo = World::get().map().mapInfo();
            return DED_Definitions()->getMusicNum(mapInfo.gets("music"));
        }
        return -1;

    default: break;
    }

    if (ddvalue >= DD_LAST_VALUE || ddvalue < DD_FIRST_VALUE)
    {
        return 0;
    }

    // Update pointers.
    if (ddvalue == DD_NUMSOUNDS)
    {
        ddValues[ddvalue].readPtr = &DED_Definitions()->sounds.count.num;
    }

    if (ddValues[ddvalue].readPtr == 0)
    {
        return 0;
    }
    return *ddValues[ddvalue].readPtr;
}

#undef DD_SetInteger
/**
 * Set a 32-bit signed integer value.
 */
void DD_SetInteger(dint ddvalue, dint parm)
{
    if (ddvalue < DD_FIRST_VALUE || ddvalue >= DD_LAST_VALUE)
    {
        return;
    }

    if (ddValues[ddvalue].writePtr)
    {
        *ddValues[ddvalue].writePtr = parm;
    }
}

#undef DD_GetVariable
/**
 * Get a pointer to the value of a variable. Not all variables support
 * this. Added for 64-bit support.
 */
void *DD_GetVariable(dint ddvalue)
{
    static dint value;
    static ddouble valueD;
    static timespan_t valueT;
    static AABoxd valueBox;

    switch (ddvalue)
    {
    case DD_GAME_EXPORTS:
        return &gx;

    case DD_MAP_POLYOBJ_COUNT:
        value = World::get().hasMap()? World::get().map().polyobjCount() : 0;
        return &value;

    case DD_MAP_BOUNDING_BOX:
        if (World::get().hasMap())
        {
            valueBox = World::get().map().bounds();
        }
        else
        {
            valueBox = AABoxd(0.0, 0.0, 0.0, 0.0);
        }
        return &valueBox;

    case DD_MAP_MIN_X:
        valueD = World::get().hasMap()? World::get().map().bounds().minX : 0;
        return &valueD;

    case DD_MAP_MIN_Y:
        valueD = World::get().hasMap()? World::get().map().bounds().minY : 0;
        return &valueD;

    case DD_MAP_MAX_X:
        valueD = World::get().hasMap()? World::get().map().bounds().maxX : 0;
        return &valueD;

    case DD_MAP_MAX_Y:
        valueD = World::get().hasMap()? World::get().map().bounds().maxY : 0;
        return &valueD;

    /*case DD_CPLAYER_THRUST_MUL:
        return &cplrThrustMul;*/

    case DD_MAP_GRAVITY:
        valueD = World::get().hasMap()? World::get().map().gravity() : 0;
        return &valueD;

#ifdef __CLIENT__
    case DD_PSPRITE_OFFSET_X:
        return &pspOffset[0];

    case DD_PSPRITE_OFFSET_Y:
        return &pspOffset[1];

    case DD_PSPRITE_LIGHTLEVEL_MULTIPLIER:
        return &pspLightLevelMultiplier;

    case DD_TORCH_RED:
        return &torchColor.x;

    case DD_TORCH_GREEN:
        return &torchColor.y;

    case DD_TORCH_BLUE:
        return &torchColor.z;

    //case DD_TORCH_ADDITIVE:
    //    return &torchAdditive;

//# ifdef WIN32
//    case DD_WINDOW_HANDLE:
//        return ClientWindow::main().nativeHandle();
//# endif
#endif

    // We have to separately calculate the 35 Hz ticks.
    case DD_GAMETIC:
        valueT = gameTime * TICSPERSEC;
        return &valueT;

    case DD_DEFS:
        return DED_Definitions();

    default: break;
    }

    if (ddvalue >= DD_LAST_VALUE || ddvalue < DD_FIRST_VALUE)
    {
        return 0;
    }

    // Other values not supported.
    return ddValues[ddvalue].writePtr;
}

/**
 * Set the value of a variable. The pointer can point to any data, its
 * interpretation depends on the variable. Added for 64-bit support.
 */
#undef DD_SetVariable
void DD_SetVariable(dint ddvalue, void *parm)
{
    if (ddvalue < DD_FIRST_VALUE || ddvalue >= DD_LAST_VALUE)
    {
        switch (ddvalue)
        {
        /*case DD_CPLAYER_THRUST_MUL:
            cplrThrustMul = *(dfloat*) parm;
            return;*/

        case DD_MAP_GRAVITY:
            if (World::get().hasMap())
                World::get().map().setGravity(*(coord_t*) parm);
            return;

#ifdef __CLIENT__
        case DD_PSPRITE_OFFSET_X:
            pspOffset[0] = *(dfloat *) parm;
            return;

        case DD_PSPRITE_OFFSET_Y:
            pspOffset[1] = *(dfloat *) parm;
            return;

        case DD_PSPRITE_LIGHTLEVEL_MULTIPLIER:
            pspLightLevelMultiplier = *(dfloat *) parm;
            return;

        case DD_TORCH_RED:
            torchColor.x = de::clamp(0.f, *((dfloat*) parm), 1.f);
            return;

        case DD_TORCH_GREEN:
            torchColor.y = de::clamp(0.f, *((dfloat*) parm), 1.f);
            return;

        case DD_TORCH_BLUE:
            torchColor.z = de::clamp(0.f, *((dfloat*) parm), 1.f);
            return;

        //case DD_TORCH_ADDITIVE:
        //    torchAdditive = (*(dint*) parm)? true : false;
        //    break;
#endif

        default:
            break;
        }
    }
}

void DD_ReadGameHelp()
{
    using namespace de;
    LOG_AS("DD_ReadGameHelp");
    try
    {
        if (App_GameLoaded())
        {
            const res::Uri uri(Path("$(App.DataPath)/$(GamePlugin.Name)/conhelp.txt"));
            FS::FoundFiles found;
            FS::get().findAll(uri.resolved(), found);
            if (found.empty())
            {
                throw Error("DD_ReadGameHelp", "conhelp.txt not found");
            }
            Help_ReadStrings(*found.front());
        }
    }
    catch (const Error &er)
    {
        LOG_RES_WARNING("") << er.asText();
    }
}

/// @note Part of the Doomsday public API.
fontschemeid_t DD_ParseFontSchemeName(const char *str)
{
#ifdef __CLIENT__
    try
    {
        FontScheme &scheme = App_Resources().fontScheme(str);
        if (!scheme.name().compareWithoutCase("System"))
        {
            return FS_SYSTEM;
        }
        if (!scheme.name().compareWithoutCase("Game"))
        {
            return FS_GAME;
        }
    }
    catch (const Resources::UnknownSchemeError &)
    {}
#endif
    debug("Unknown font scheme: \"%s\", returning 'FS_INVALID'", str);
    return FS_INVALID;
}

String DD_MaterialSchemeNameForTextureScheme(const String &textureSchemeName)
{
    if (!textureSchemeName.compareWithoutCase("Textures"))
    {
        return "Textures";
    }
    if (!textureSchemeName.compareWithoutCase("Flats"))
    {
        return "Flats";
    }
    if (!textureSchemeName.compareWithoutCase("Sprites"))
    {
        return "Sprites";
    }
    if (!textureSchemeName.compareWithoutCase("System"))
    {
        return "System";
    }
    return "";
}

AutoStr *DD_MaterialSchemeNameForTextureScheme(const ddstring_t *textureSchemeName)
{
    if (!textureSchemeName)
    {
        return AutoStr_FromTextStd("");
    }
    else
    {
        return AutoStr_FromTextStd(
            DD_MaterialSchemeNameForTextureScheme(Str_Text(textureSchemeName)));
    }
}

D_CMD(Load)
{
    DE_UNUSED(src);
    BusyMode_FreezeGameForBusyMode();
    auto &loader = PackageLoader::get();

    for (int arg = 1; arg < argc; ++arg)
    {
        String searchTerm = String(argv[arg]).strip();
        if (!searchTerm) continue;

        // Are we loading a game?
        if (DoomsdayApp::games().contains(searchTerm))
        {
            const Game &game = DoomsdayApp::games()[searchTerm];
            if (!game.isPlayable())
            {
                LOG_SCR_ERROR("Game \"%s\" is missing one or more required packages: %s")
                        << game.id()
                        << String::join(game.profile().unavailablePackages(), ", ");
                return true;
            }
            if (DoomsdayApp::app().changeGame(game.profile(), DD_ActivateGameWorker))
            {
                game.profile().setLastPlayedAt();
                continue;
            }
            return false;
        }

        // It could also be a game profile.
        if (auto *profile = DoomsdayApp::gameProfiles().tryFind(searchTerm))
        {
            auto &gameProf = profile->as<GameProfile>();
            if (!gameProf.isPlayable())
            {
                LOG_SCR_ERROR("Profile \"%s\" is missing one or more required packages: ")
                        << String::join(gameProf.unavailablePackages(), ", ");
                return true;
            }
            if (DoomsdayApp::app().changeGame(gameProf, DD_ActivateGameWorker))
            {
                gameProf.setLastPlayedAt();
                continue;
            }
            return false;
        }

        try
        {
            // Check packages with a matching name.
            if (loader.isAvailable(searchTerm))
            {
                if (loader.isLoaded(searchTerm))
                {
                    LOG_SCR_MSG("Package \"%s\" is already loaded") << searchTerm;
                    continue;
                }
                loader.load(searchTerm);
                continue;
            }

            // Check data bundles with a matching name. We assume the search term
            // is a native path.
            if (!DoomsdayApp::isGameLoaded())
            {
                LOG_SCR_ERROR("Cannot load data files when a game isn't loaded");
                return false;
            }
            auto files = DataBundle::findAllNative(searchTerm);
            if (files.size() == 1)
            {
                if (!files.first()->isLinkedAsPackage())
                {
                    LOG_SCR_ERROR("%s cannot be loaded (could be ignored due to "
                                  "being unsupported or invalid") << files.first()->description();
                    return false;
                }
                loader.load(files.first()->packageId());
                continue;
            }
            else if (files.size() > 1)
            {
                LOG_SCR_MSG("There are %i possible matches for the name \"%s\"")
                        << files.size()
                        << searchTerm;
                if (files.size() <= 10)
                {
                    LOG_SCR_MSG("Maybe you meant:");
                    for (const auto *f : files)
                    {
                        LOG_SCR_MSG("- " _E(>) "%s") << f->description();
                    }
                }
                return false;
            }
            else
            {
                LOG_SCR_ERROR("No files found matching the name \"%s\"") << searchTerm;
                return false;
            }
        }
        catch (const Error &er)
        {
            LOG_SCR_ERROR("Failed to load package \"%s\": %s") << searchTerm << er.asText();
            return false;
        }
    }
    return true;
}

D_CMD(Unload)
{
    DE_UNUSED(src);
    BusyMode_FreezeGameForBusyMode();

    DoomsdayApp &app = DoomsdayApp::app();

    try
    {
        // No arguments; unload the current game if loaded.
        if (argc == 1)
        {
            if (!app.isGameLoaded())
            {
                LOG_SCR_MSG("No game is currently loaded");
                return true;
            }
            return app.changeGame(GameProfiles::null(), DD_ActivateGameWorker);
        }

        auto &     loader         = PackageLoader::get();
        const auto loadedPackages = loader.loadedPackages();
        auto       loadedBundles  = DataBundle::loadedBundles();

        for (int arg = 1; arg < argc; ++arg)
        {
            String searchTerm = String(argv[arg]).strip();
            if (!searchTerm) continue;

            if (app.isGameLoaded() && searchTerm == DoomsdayApp::game().id())
            {
                if (!app.changeGame(GameProfiles::null(), DD_ActivateGameWorker))
                {
                    return false;
                }
                continue;
            }

            // Is this one of the loaded packages?
            if (loadedPackages.contains(searchTerm) && loader.isAvailable(searchTerm))
            {
                loader.unload(searchTerm);
                continue;
            }
            for (const DataBundle *bundle : loadedBundles)
            {
                if (!bundle->sourceFile().name().compareWithoutCase(searchTerm))
                {
                    loadedBundles.removeOne(bundle);
                    loader.unload(bundle->packageId());
                    break;
                }
            }
        }
    }
    catch (const Error &er)
    {
        LOG_SCR_ERROR("Problem while unloading: %s") << er.asText();
        return false;
    }

    /*AutoStr *searchPath = AutoStr_NewStd();
    Str_Set(searchPath, argv[1]);
    Str_Strip(searchPath);
    if (Str_IsEmpty(searchPath)) return false;

    F_FixSlashes(searchPath, searchPath);

    // Ignore attempts to unload directories.
    if (Str_RAt(searchPath, 0) == '/')
    {
        LOG_MSG("Directories cannot be \"unloaded\" (only files and/or known games).");
        return true;
    }

    // Unload the current game if specified.
    if (argc == 2)
    {
        try
        {
            Game &game = App_Games()[Str_Text(searchPath)];
            if (App_GameLoaded())
            {
                return DoomsdayApp::app().changeGame(GameProfiles::null(),
                                                     DD_ActivateGameWorker);
            }

            LOG_MSG("%s is not currently loaded.") << game.id();
            return true;
        }
        catch (const Games::NotFoundError &)
        {} // Ignore the error.
    }

    // Try the resource locator.
    bool didUnloadFiles = false;
    for (dint i = 1; i < argc; ++i)
    {
        try
        {
            String foundPath = App_FileSystem().findPath(res::Uri::fromNativePath(argv[1], RC_PACKAGE),
                                                         RLF_MATCH_EXTENSION, App_ResourceClass(RC_PACKAGE));
            foundPath = App_BasePath() / foundPath; // Ensure the path is absolute.

            if (File1::tryUnload(res::makeUri(foundPath)))
            {
                didUnloadFiles = true;
            }
        }
        catch (FS1::NotFoundError const&)
        {} // Ignore this error.
    }

    if (didUnloadFiles)
    {
        // A changed file list may alter the main lump directory.
        DD_UpdateEngineState();
    }

    return didUnloadFiles;*/

    return true;
}

D_CMD(Reset)
{
    DE_UNUSED(src, argc, argv);

    DD_UpdateEngineState();
    return true;
}

D_CMD(ReloadGame)
{
    DE_UNUSED(src, argc, argv);

    if (!DoomsdayApp::currentGameProfile())
    {
        LOG_MSG("No game is presently loaded.");
        return true;
    }
    DoomsdayApp::app().changeGame(*DoomsdayApp::currentGameProfile(),
                                  DD_ActivateGameWorker,
                                  DoomsdayApp::AllowReload);
    return true;
}

#if defined (DE_HAVE_UPDATER)

D_CMD(CheckForUpdates)
{
    DE_UNUSED(src, argc, argv);

    LOG_MSG("Checking for available updates...");
    ClientApp::updater().checkNow(Updater::OnlyShowResultIfUpdateAvailable);
    return true;
}

D_CMD(CheckForUpdatesAndNotify)
{
    DE_UNUSED(src, argc, argv);

    LOG_MSG("Checking for available updates...");
    ClientApp::updater().checkNow(Updater::AlwaysShowResult);
    return true;
}

D_CMD(LastUpdated)
{
    DE_UNUSED(src, argc, argv);

    ClientApp::updater().printLastUpdated();
    return true;
}

D_CMD(ShowUpdateSettings)
{
    DE_UNUSED(src, argc, argv);

    ClientApp::updater().showSettings();
    return true;
}

#endif // DE_HAVE_UPDATER

D_CMD(Version)
{
    DE_UNUSED(src, argc, argv);

    LOG_SCR_NOTE(_E(D) DOOMSDAY_NICENAME " %s") << Version::currentBuild().asHumanReadableText();
    LOG_SCR_MSG(_E(l) "Homepage: " _E(.) _E(i) DOOMSDAY_HOMEURL _E(.)
            "\n" _E(l) "Project: " _E(.) _E(i) DENGPROJECT_HOMEURL);

    // Print the version info of the current game if loaded.
    if (App_GameLoaded())
    {
        LOG_SCR_MSG(_E(l) "Game: " _E(.) "%s") << (const char *) gx.GetPointer(DD_PLUGIN_VERSION_LONG);
    }

    // Additional information for developers.
    Version const ver;
    if (!ver.gitDescription.isEmpty())
    {
        LOGDEV_SCR_MSG(_E(l) "Git revision: " _E(.) "%s") << ver.gitDescription;
    }
    return true;
}

D_CMD(Quit)
{
    DE_UNUSED(src, argc);

#if defined (DE_HAVE_UPDATER)
    if (UpdateDownloadDialog::isDownloadInProgress())
    {
        LOG_WARNING("Cannot quit while downloading an update");
        ClientWindow::main().taskBar().openAndPauseGame();
        UpdateDownloadDialog::currentDownload().open();
        return false;
    }
#endif

    if (argv[0][4] == '!' || isDedicated || !App_GameLoaded() ||
       gx.TryShutdown == 0)
    {
        // No questions asked.
        Sys_Quit();
        return true; // Never reached.
    }

#ifdef __CLIENT__
    // Dismiss the taskbar if it happens to be open, we are expecting
    // the game to handle this from now on.
    ClientWindow::main().taskBar().close();
#endif

    // Defer this decision to the loaded game.
    return gx.TryShutdown();
}

#ifdef _DEBUG
D_CMD(DebugError)
{
    DE_UNUSED(src, argv, argc);
    App_Error("Fatal error!\n");
}
#endif

D_CMD(Help)
{
    DE_UNUSED(src, argc, argv);

    /*
#ifdef __CLIENT__
    char actKeyName[40];
    strcpy(actKeyName, B_ShortNameForKey(consoleActiveKey));
    actKeyName[0] = toupper(actKeyName[0]);
#endif
*/

    LOG_SCR_NOTE(_E(b) DOOMSDAY_NICENAME " %s Console") << Version::currentBuild().compactNumber();

#define TABBED(A, B) "\n" _E(Ta) _E(b) "  " << A << " " _E(.) _E(Tb) << B

#ifdef __CLIENT__
    LOG_SCR_MSG(_E(D) "Keys:" _E(.))
            << TABBED(DE_CHAR_SHIFT_KEY "Esc", "Open the taskbar and console")
            << TABBED("Tab", "Autocomplete the word at the cursor")
            << TABBED(DE_CHAR_UP_DOWN_ARROW, "Move backwards/forwards through the input command history, or up/down one line inside a multi-line command")
            << TABBED("PgUp/Dn", "Scroll up/down in the history, or expand the history to full height")
            << TABBED(DE_CHAR_SHIFT_KEY "PgUp/Dn", "Jump to the top/bottom of the history")
            << TABBED("Home", "Move the cursor to the start of the command line")
            << TABBED("End", "Move the cursor to the end of the command line")
            << TABBED(DE_CHAR_CONTROL_KEY "K", "Clear everything on the line right of the cursor position")
            << TABBED("F5", "Clear the console message history");
#endif
    LOG_SCR_MSG(_E(D) "Getting started:");
    LOG_SCR_MSG("  " _E(>) "Enter " _E(b) "help (what)" _E(.) " for information about " _E(l) "(what)");
    LOG_SCR_MSG("  " _E(>) "Enter " _E(b) "listcmds" _E(.) " to list available commands");
    LOG_SCR_MSG("  " _E(>) "Enter " _E(b) "listgames" _E(.) " to list installed games and their status");
    LOG_SCR_MSG("  " _E(>) "Enter " _E(b) "listvars" _E(.) " to list available variables");

#undef TABBED

    return true;
}

static void printHelpAbout(const char *query)
{
    // Try the console commands first.
    if (ccmd_t *ccmd = Con_FindCommand(query))
    {
        LOG_SCR_MSG(_E(b) "%s" _E(.) " (Command)") << ccmd->name;

        HelpId help = DH_Find(ccmd->name);
        if (const char *description = DH_GetString(help, HST_DESCRIPTION))
        {
            LOG_SCR_MSG("") << description;
        }

        Con_PrintCommandUsage(ccmd);  // For all overloaded variants.

        // Any extra info?
        if (const char *info = DH_GetString(help, HST_INFO))
        {
            LOG_SCR_MSG("  " _E(>) _E(l)) << info;
        }
        return;
    }

    if (cvar_t *var = Con_FindVariable(query))
    {
        AutoStr *path = CVar_ComposePath(var);
        LOG_SCR_MSG(_E(b) "%s" _E(.) " (Variable)") << Str_Text(path);

        HelpId help = DH_Find(Str_Text(path));
        if (const char *description = DH_GetString(help, HST_DESCRIPTION))
        {
            LOG_SCR_MSG("") << description;
        }
        return;
    }

    if (calias_t *calias = Con_FindAlias(query))
    {
        LOG_SCR_MSG(_E(b) "%s" _E(.) " alias of:\n")
                << calias->name << calias->command;
        return;
    }

    // Perhaps a game?
    try
    {
        Game &game = App_Games()[query];
        LOG_SCR_MSG(_E(b) "%s" _E(.) " (IdentityKey)") << game.id();

        LOG_SCR_MSG("Unique identifier of the " _E(b) "%s" _E(.) " game mode.") << game.title();
        LOG_SCR_MSG("An 'IdentityKey' is used when referencing a game unambiguously from the console and on the command line.");
        LOG_SCR_MSG(_E(D) "Related commands:");
        LOG_SCR_MSG("  " _E(>) "Enter " _E(b) "inspectgame %s" _E(.) " for information and status of this game") << game.id();
        LOG_SCR_MSG("  " _E(>) "Enter " _E(b) "listgames" _E(.) " to list all installed games and their status");
        LOG_SCR_MSG("  " _E(>) "Enter " _E(b) "load %s" _E(.) " to load the " _E(l) "%s" _E(.) " game mode") << game.id() << game.title();
        return;
    }
    catch (const Games::NotFoundError &)
    {}  // Ignore this error.

    LOG_SCR_NOTE("There is no help about '%s'") << query;
}

D_CMD(HelpWhat)
{
    DE_UNUSED(argc, src);

    if (!String(argv[1]).compareWithoutCase("(what)"))
    {
        LOG_SCR_MSG("You've got to be kidding!");
        return true;
    }

    printHelpAbout(argv[1]);
    return true;
}

#ifdef __CLIENT__
D_CMD(Clear)
{
    DE_UNUSED(src, argc, argv);

    ClientWindow::main().console().clearLog();
    return true;
}
#endif

void DD_ConsoleRegister()
{
    C_VAR_CHARPTR("file-startup", &startupFiles, 0, 0, 0);

    C_CMD("help",           "",     Help);
    C_CMD("help",           "s",    HelpWhat);
    C_CMD("version",        "",     Version);
    C_CMD("quit",           "",     Quit);
    C_CMD("quit!",          "",     Quit);
    C_CMD("load",           "s*",   Load);
    C_CMD("reset",          "",     Reset);
    C_CMD("reload",         "",     ReloadGame);
    C_CMD("unload",         "*",    Unload);
    C_CMD("write",          "s",    WriteConsole);

#ifdef DE_DEBUG
    C_CMD("fatalerror",     nullptr,   DebugError);
#endif

    DD_RegisterLoop();
    Def_ConsoleRegister();
    FS1::consoleRegister();
    Con_Register();
    Games::consoleRegister();
    DH_Register();
    AudioSystem::consoleRegister();

#ifdef __CLIENT__
    C_CMD("clear",           "", Clear);

#if defined (DE_HAVE_UPDATER)
    C_CMD("update",          "", CheckForUpdates);
    C_CMD("updateandnotify", "", CheckForUpdatesAndNotify);
    C_CMD("updatesettings",  "", ShowUpdateSettings);
    C_CMD("lastupdated",     "", LastUpdated);
#endif

    C_CMD_FLAGS("conclose",       "",     OpenClose,    CMDF_NO_DEDICATED);
    C_CMD_FLAGS("conopen",        "",     OpenClose,    CMDF_NO_DEDICATED);
    C_CMD_FLAGS("contoggle",      "",     OpenClose,    CMDF_NO_DEDICATED);
    C_CMD      ("taskbar",        "",     TaskBar);
    C_CMD      ("tutorial",       "",     Tutorial);
    C_CMD      ("packages",       "",     PackagesSidebar);

    /// @todo Move to UI module.
    Con_TransitionRegister();

    InputSystem::consoleRegister();
#if 0
    SBE_Register();  // for bias editor
#endif
    RenderSystem::consoleRegister();
    GL_Register();
    //UI_Register();
    Demo_Register();
    P_ConsoleRegister();
    I_Register();
    ClientResources::consoleRegister();
#endif

#ifdef __SERVER__
    Resources::consoleRegister();
#endif

    Net_Register();
    world::Map::consoleRegister();
    InFineSystem::consoleRegister();
}

// dd_loop.c
DE_EXTERN_C dd_bool DD_IsSharpTick(void);

// net_main.c
DE_EXTERN_C void Net_SendPacket(dint to_player, dint type, const void* data, size_t length);

#undef R_SetupMap
DE_EXTERN_C void R_SetupMap(dint mode, dint flags)
{
    DE_UNUSED(mode, flags);

    if (!World::get().hasMap()) return; // Huh?

    // Perform map setup again. Its possible that after loading we now
    // have more HOMs to fix, etc..
    world::Map &map = World::get().map();

#ifdef __CLIENT__
    map.as<Map>().initSkyFix();
#endif

#ifdef __CLIENT__
    /*
    // Update all sectors.
    /// @todo Refactor away.
    map.forAllSectors([] (Sector &sector)
    {
        return sector.forAllSubsectors([] (world::Subsector &subsec)
        {
            return subsec.as<world::ClientSubsector>().forAllEdgeLoops([] (world::ClEdgeLoop &loop)
            {
                loop.fixSurfacesMissingMaterials();
                return LoopContinue;
            });
        });
    });
    */
#endif

    // Re-initialize polyobjs.
    /// @todo Still necessary?
    map.initPolyobjs();

    // Reset the timer so that it will appear that no time has passed.
    DD_ResetTimer();
}

// sys_system.c
DE_EXTERN_C void Sys_Quit();

DE_DECLARE_API(Base) =
{
    { DE_API_BASE },

    Sys_Quit,
    DD_GetInteger,
    DD_SetInteger,
    DD_GetVariable,
    DD_SetVariable,
    DD_GameInfo,
    DD_IsSharpTick,
    Net_SendPacket,
    R_SetupMap
};
