/** @file doomsdayapp.cpp  Common application-level state and components.
 *
 * @authors Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/doomsdayapp.h"
#include "doomsday/games.h"
#include "doomsday/console/exec.h"
#include "doomsday/filesys/sys_direc.h"
#include "doomsday/filesys/fs_util.h"
#include "doomsday/resource/resources.h"
#include "doomsday/resource/bundles.h"
#include "doomsday/filesys/datafile.h"
#include "doomsday/filesys/datafolder.h"
#include "doomsday/paths.h"
#include "doomsday/world/world.h"
#include "doomsday/world/entitydef.h"
#include "doomsday/Session"
#include "doomsday/SavedSession"

#include <de/App>
#include <de/Loop>
#include <de/Folder>
#include <de/DirectoryFeed>
#include <de/DictionaryValue>
#include <de/c_wrapper.h>
#include <de/strutil.h>
#include <de/memoryzone.h>

#include <QSettings>
#include <QDir>

#ifdef WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  define ENV_PATH_SEP_CHAR ';'
#else
#  define ENV_PATH_SEP_CHAR ':'
#endif

using namespace de;

static String const PATH_LOCAL_WADS("/local/wads");

static DoomsdayApp *theDoomsdayApp = nullptr;

DENG2_PIMPL_NOREF(DoomsdayApp)
{
    std::string ddBasePath; // Doomsday root directory is at...?
    std::string ddRuntimePath;

    bool initialized = false;
    Plugins plugins;
    Games games;
    Game *currentGame = nullptr;
    BusyMode busyMode;
    Players players;
    res::Bundles dataBundles;

    /// @c true = We are using a custom user dir specified on the command line.
    bool usingUserDir = false;

#ifdef UNIX
# ifndef MACOSX
    /// @c true = We are using the user dir defined in the HOME environment.
    bool usingHomeDir = false;
# endif
#endif

#ifdef WIN32
    HINSTANCE hInstance = NULL;
#endif

    /**
     * Delegates game change notifications to scripts.
     */
    class GameChangeScriptAudience : DENG2_OBSERVES(DoomsdayApp, GameChange)
    {
    public:
        void currentGameChanged(Game const &newGame)
        {
            ArrayValue args;
            args << DictionaryValue() << TextValue(newGame.id());
            App::scriptSystem().nativeModule("App")["audienceForGameChange"]
                    .array().callElements(args);
        }
    };

    GameChangeScriptAudience scriptAudienceForGameChange;

    Instance(Players::Constructor playerConstructor)
        : players(playerConstructor)
    {
        Record &appModule = App::scriptSystem().nativeModule("App");
        appModule.addArray("audienceForGameChange"); // game change observers
        audienceForGameChange += scriptAudienceForGameChange;

#ifdef WIN32
        hInstance = GetModuleHandle(NULL);
#endif
    }

    ~Instance()
    {
        theDoomsdayApp = nullptr;
    }

    void attachWadFeed(String const &description, NativePath const &path)
    {
        if(!path.isEmpty())
        {
            if(path.exists())
            {
                LOG_RES_NOTE("Using %s WAD folder: %s") << description << path.pretty();
                App::rootFolder().locate<Folder>(PATH_LOCAL_WADS).attach(new DirectoryFeed(path));
            }
            else
            {
                LOG_RES_NOTE("Ignoring non-existent %s WAD folder: %s")
                        << description << path.pretty();
            }
        }
    }

    /**
     * Doomsday can locate WAD files from various places. This method attaches
     * a set of feeds to /local/wads/ so that all the native folders where the
     * user keeps WAD files are available in the tree.
     */
    void initWadFolders()
    {
        // "/local" is for various files on the local computer.
        Folder &wads = App::fileSystem().makeFolder(PATH_LOCAL_WADS, FS::DontInheritFeeds);
        wads.clear();
        wads.clearFeeds();

        CommandLine &cmdLine = App::commandLine();
        NativePath const startupPath = cmdLine.startupPath();

        // Feeds are added in ascending priority.

        // Check for games installed using Steam.
        NativePath const steamBase = steamBasePath();
        if(steamBase.exists())
        {
            NativePath steamPath = steamBase / "SteamApps/common/";
            LOG_RES_NOTE("Detected SteamApps path: %s") << steamPath.pretty();

            static String const appDirs[] = {
                "DOOM 2/base",
                "Final DOOM/base",
                "Heretic Shadow of the Serpent Riders/base",
                "Hexen/base",
                "Hexen Deathkings of the Dark Citadel/base",
                "Ultimate Doom/base",
                "DOOM 3 BFG Edition/base/wads"
            };

            for(auto const &appDir : appDirs)
            {
                NativePath const p = steamPath / appDir;
                if(p.exists())
                {
                    attachWadFeed("Steam", p);
                }
            }
        }

#ifdef UNIX
        NativePath const systemWads("/usr/share/games/doom");
        if(systemWads.exists())
        {
            attachWadFeed("system", systemWads);
        }
#endif

        // Add all paths from the DOOMWADPATH environment variable.
        if(getenv("DOOMWADPATH"))
        {
            // Interpreted similarly to the PATH variable.
            QStringList paths = String(getenv("DOOMWADPATH"))
                    .split(ENV_PATH_SEP_CHAR, String::SkipEmptyParts);
            while(!paths.isEmpty())
            {
                attachWadFeed(_E(m) "DOOMWADPATH" _E(.), startupPath / paths.takeLast());
            }
        }

        // Add the path from the DOOMWADDIR environment variable.
        if(getenv("DOOMWADDIR"))
        {
            attachWadFeed(_E(m) "DOOMWADDIR" _E(.), startupPath / getenv("DOOMWADDIR"));
        }

#ifdef UNIX
        // There may be an iwaddir specified in a system-level config file.
        filename_t fn;
        if(UnixInfo_GetConfigValue("paths", "iwaddir", fn, FILENAME_T_MAXLEN))
        {
            attachWadFeed("UnixInfo " _E(i) "paths.iwaddir" _E(.), startupPath / fn);
        }
#endif

        // Command line paths.
        if(auto arg = cmdLine.check("-iwad", 1)) // has at least one parameter
        {
            for(dint p = arg.pos + 1; p < cmdLine.count(); ++p)
            {
                if(cmdLine.isOption(p)) break;

                cmdLine.makeAbsolutePath(p);
                attachWadFeed("command-line", cmdLine.at(p));
            }
        }

        // Configured via GUI.
        attachWadFeed("user-selected", App::config().gets("resource.iwadFolder", ""));

        wads.populate();
    }

#ifdef UNIX
    void determineGlobalPaths()
    {
        // By default, make sure the working path is the home folder.
        App::setCurrentWorkPath(App::app().nativeHomePath());

# ifndef MACOSX
        if(getenv("HOME"))
        {
            filename_t homePath;
            directory_t* temp;
            dd_snprintf(homePath, FILENAME_T_MAXLEN, "%s/%s/runtime/", getenv("HOME"),
                        DENG2_APP->unixHomeFolderName().toLatin1().constData());
            temp = Dir_New(homePath);
            Dir_mkpath(Dir_Path(temp));
            usingHomeDir = Dir_SetCurrent(Dir_Path(temp));
            if(usingHomeDir)
            {
                DD_SetRuntimePath(Dir_Path(temp));
            }
            Dir_Delete(temp);
        }
# endif

        // The -userdir option sets the working directory.
        if(CommandLine_CheckWith("-userdir", 1))
        {
            filename_t runtimePath;
            directory_t* temp;

            strncpy(runtimePath, CommandLine_NextAsPath(), FILENAME_T_MAXLEN);
            Dir_CleanPath(runtimePath, FILENAME_T_MAXLEN);
            // Ensure the path is closed with a directory separator.
            F_AppendMissingSlashCString(runtimePath, FILENAME_T_MAXLEN);

            temp = Dir_New(runtimePath);
            usingUserDir = Dir_SetCurrent(Dir_Path(temp));
            if(usingUserDir)
            {
                DD_SetRuntimePath(Dir_Path(temp));
# ifndef MACOSX
                usingHomeDir = false;
# endif
            }
            Dir_Delete(temp);
        }

# ifndef MACOSX
        if(!usingHomeDir && !usingUserDir)
# else
        if(!usingUserDir)
# endif
        {
            // The current working directory is the runtime dir.
            directory_t* temp = Dir_NewFromCWD();
            DD_SetRuntimePath(Dir_Path(temp));
            Dir_Delete(temp);
        }

        // libcore has determined the native base path, so let FS1 know about it.
        DD_SetBasePath(DENG2_APP->nativeBasePath().toUtf8());
    }
#endif // UNIX

#ifdef WIN32
    void determineGlobalPaths()
    {
        // Change to a custom working directory?
        if(CommandLine_CheckWith("-userdir", 1))
        {
            if(NativePath::setWorkPath(CommandLine_NextAsPath()))
            {
                LOG_VERBOSE("Changed current directory to \"%s\"") << NativePath::workPath();
                usingUserDir = true;
            }
        }

        // The runtime directory is the current working directory.
        DD_SetRuntimePath((NativePath::workPath().withSeparators('/') + '/').toUtf8().constData());

        // Use a custom base directory?
        if(CommandLine_CheckWith("-basedir", 1))
        {
            DD_SetBasePath(CommandLine_Next());
        }
        else
        {
            // The default base directory is one level up from the bin dir.
            String binDir = App::executablePath().fileNamePath().withSeparators('/');
            String baseDir = String(QDir::cleanPath(binDir / String(".."))) + '/';
            DD_SetBasePath(baseDir.toUtf8().constData());
        }
    }
#endif // WIN32

    DENG2_PIMPL_AUDIENCE(GameUnload)
    DENG2_PIMPL_AUDIENCE(GameChange)
};

DENG2_AUDIENCE_METHOD(DoomsdayApp, GameUnload)
DENG2_AUDIENCE_METHOD(DoomsdayApp, GameChange)

DoomsdayApp::DoomsdayApp(Players::Constructor playerConstructor)
    : d(new Instance(playerConstructor))
{
    DENG2_ASSERT(!theDoomsdayApp);
    theDoomsdayApp = this;

    App::app().addInitPackage("net.dengine.base");

    static SavedSession::Interpreter intrpSavedSession;
    static DataBundle::Interpreter   intrpDataBundle;

    App::fileSystem().addInterpreter(intrpSavedSession);
    App::fileSystem().addInterpreter(intrpDataBundle);
}

void DoomsdayApp::initialize()
{
    d->initWadFolders();

    // "/sys/bundles" has package-like symlinks to files that are not in
    // Doomsday 2 format but can be loaded as packages.
    App::fileSystem().makeFolder("/sys/bundles", FS::DontInheritFeeds);

    d->initialized = true;

    d->dataBundles.identify();
}

void DoomsdayApp::determineGlobalPaths()
{
    d->determineGlobalPaths();
}

DoomsdayApp &DoomsdayApp::app()
{
    DENG2_ASSERT(theDoomsdayApp);
    return *theDoomsdayApp;
}

res::Bundles &DoomsdayApp::bundles()
{
    return DoomsdayApp::app().d->dataBundles;
}

Plugins &DoomsdayApp::plugins()
{
    return DoomsdayApp::app().d->plugins;
}

Games &DoomsdayApp::games()
{
    return DoomsdayApp::app().d->games;
}

Players &DoomsdayApp::players()
{
    return DoomsdayApp::app().d->players;
}

Game &DoomsdayApp::currentGame()
{
    return game();
}

BusyMode &DoomsdayApp::busyMode()
{
    return DoomsdayApp::app().d->busyMode;
}

NativePath DoomsdayApp::steamBasePath()
{
#ifdef WIN32
    // The path to Steam can be queried from the registry.
    {
        QSettings st("HKEY_CURRENT_USER\\Software\\Valve\\Steam\\", QSettings::NativeFormat);
        String path = st.value("SteamPath").toString();
        if(!path.isEmpty()) return path;
    }
    {
        QSettings st("HKEY_LOCAL_MACHINE\\Software\\Valve\\Steam\\", QSettings::NativeFormat);
        String path = st.value("InstallPath").toString();
        if(!path.isEmpty()) return path;
    }
#elif MACOSX
    return NativePath(QDir::homePath()) / "Library/Application Support/Steam/";
#endif
    /// @todo Where are Steam apps located on Linux?
    return "";
}

bool DoomsdayApp::isUsingUserDir() const
{
    return d->usingUserDir;
}

std::string const &DoomsdayApp::doomsdayBasePath() const
{
    return d->ddBasePath;
}

void DoomsdayApp::setDoomsdayBasePath(NativePath const &path)
{
    /// @todo Unfortunately Dir/fs_util assumes fixed-size strings, so we
    /// can't take advantage of std::string. -jk
    filename_t temp;
    strncpy(temp, path.toUtf8(), FILENAME_T_MAXLEN);

    Dir_CleanPath(temp, FILENAME_T_MAXLEN);
    Dir_MakeAbsolutePath(temp, FILENAME_T_MAXLEN);

    // Ensure it ends with a directory separator.
    F_AppendMissingSlashCString(temp, FILENAME_T_MAXLEN);

    d->ddBasePath = temp;
}

std::string const &DoomsdayApp::doomsdayRuntimePath() const
{
    return d->ddRuntimePath;
}

void DoomsdayApp::setDoomsdayRuntimePath(NativePath const &path)
{
    d->ddRuntimePath = path.toUtf8().constData();
}

#ifdef WIN32
void *DoomsdayApp::moduleHandle() const
{
    return d->hInstance;
}
#endif

Game &DoomsdayApp::game()
{
    DENG2_ASSERT(app().d->currentGame != 0);
    return *app().d->currentGame;
}

void DoomsdayApp::aboutToChangeGame(Game const &)
{
    auto &gx = plugins().gameExports();

    if(App_GameLoaded())
    {
        if(gx.Shutdown)
        {
            gx.Shutdown();
        }

        // Tell the plugin it is being unloaded.
        {
            void *unloader = plugins().findEntryPoint(game().pluginId(), "DP_Unload");
            LOGDEV_MSG("Calling DP_Unload %p") << unloader;
            plugins().setActivePluginId(game().pluginId());
            if(unloader) ((pluginfunc_t)unloader)();
            plugins().setActivePluginId(0);
        }

        // Clear application and subsystem state.
        reset();
        Resources::get().clear();

        // We do not want to load session resources specified on the command line again.
        Session::profile().resourceFiles.clear();

        // The current game is now the special "null-game".
        setGame(games().nullGame());

        App_FileSystem().unloadAllNonStartupFiles();

        // Reset file IDs so previously seen files can be processed again.
        /// @todo this releases the IDs of startup files too but given the
        /// only startup file is doomsday.pk3 which we never attempt to load
        /// again post engine startup, this isn't an immediate problem.
        App_FileSystem().resetFileIds();

        // Update the dir/WAD translations.
        FS_InitPathLumpMappings();
        FS_InitVirtualPathMappings();

        App_FileSystem().resetAllSchemes();
    }

    /// @todo The entire material collection should not be destroyed during a reload.
    Resources::get().clearAllMaterialSchemes();
}

void DoomsdayApp::reset()
{
    // Reset the world back to it's initial state (unload the map, reset players, etc...).
    World::get().reset();

    Z_FreeTags(PU_GAMESTATIC, PU_PURGELEVEL - 1);

    P_ShutdownMapEntityDefs();

    Con_ClearDatabases();
    Con_InitDatabases();
}

void DoomsdayApp::setGame(Game &game)
{
    app().d->currentGame = &game;
}

bool App_GameLoaded()
{
    return App::appExists() && !DoomsdayApp::currentGame().isNull();
}
