/** @file doomsdayapp.cpp  Common application-level state and components.
 *
 * @authors Copyright (c) 2015-2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "doomsday/gameprofiles.h"
#include "doomsday/console/exec.h"
#include "doomsday/filesys/fs_util.h"
#include "doomsday/res/resources.h"
#include "doomsday/res/bundles.h"
#include "doomsday/res/bundlelinkfeed.h"
#include "doomsday/filesys/fs_main.h"
#include "doomsday/filesys/datafile.h"
#include "doomsday/filesys/datafolder.h"
#include "doomsday/filesys/virtualmappings.h"
#include "doomsday/filesys/idgameslink.h"
#include "doomsday/net.h"
#include "doomsday/busymode.h"
#include "doomsday/world/world.h"
#include "doomsday/world/entitydef.h"
#include "doomsday/world/materials.h"
#include "doomsday/savegames.h"
#include "doomsday/abstractsession.h"
#include "doomsday/gamestatefolder.h"

#include "api_def.h"
#include "api_filesys.h"
#include "api_mapedit.h"
#include "api_material.h"
#include "api_uri.h"

#include <de/app.h>
#include <de/archivefolder.h>
#include <de/archivefeed.h>
#include <de/commandline.h>
#include <de/config.h>
#include <de/directoryfeed.h>
#include <de/extension.h>
#include <de/folder.h>
#include <de/garbage.h>
#include <de/loop.h>
#include <de/metadatabank.h>
#include <de/nativefile.h>
#include <de/nativepath.h>
#include <de/packageloader.h>
#include <de/filesys/remotefeedrelay.h>
#include <de/dscript.h>
#include <de/timer.h>
#include <de/c_wrapper.h>
#include <de/legacy/strutil.h>
#include <de/legacy/memoryzone.h>
#include <de/legacy/memory.h>

#ifdef DE_WINDOWS
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <de/registry.h>
#  define ENV_PATH_SEP_CHAR ';'
#else
#  define ENV_PATH_SEP_CHAR ':'
#endif

using namespace de;
using World = world::World;

DE_EXTENSION(importdeh);
DE_EXTENSION(importidtech1);
DE_EXTENSION(importudmf);

// Public API used in extensions:
DE_USING_API(Base);
DE_USING_API(Def);
DE_USING_API(F);
DE_USING_API(Material);
DE_USING_API(MPE);
DE_USING_API(Uri);

DE_DECLARE_API(Base);
DE_DECLARE_API(Def);
DE_DECLARE_API(F);
DE_DECLARE_API(Material);
DE_DECLARE_API(MPE);
DE_DECLARE_API(Uri);

DE_API_EXCHANGE(
    DE_GET_API(DE_API_BASE, Base);
    DE_GET_API(DE_API_DEFINITIONS, Def);
    DE_GET_API(DE_API_FILE_SYSTEM, F);
    DE_GET_API(DE_API_MATERIALS, Material);
    DE_GET_API(DE_API_MAP_EDIT, MPE);
    DE_GET_API(DE_API_URI, Uri);
)

static const char *deng_LibraryType(void)
{
    return "library/generic";
}

DE_EXTERN_C void *extension_doomsday_symbol(const char *name)
{
    DE_SYMBOL_PTR(name, deng_LibraryType);
    DE_SYMBOL_PTR(name, deng_API);
    return nullptr;
}

DE_EXTENSION(doomsday); // only for acquiring the APIs

DE_STATIC_STRING(PATH_LOCAL_WADS,  "/local/wads");
DE_STATIC_STRING(PATH_LOCAL_PACKS, "/local/packs");

static DoomsdayApp *theDoomsdayApp = nullptr;

int DoomsdayApp::verbose;

DE_PIMPL(DoomsdayApp)
, public IFolderPopulationObserver
{
    std::string ddBasePath; // Doomsday root directory is at...?

    Flags              appFlags;
    Binder             binder;
    bool               initialized      = false;
    bool               gameBeingChanged = false;
    bool               shuttingDown     = false;
    Plugins            plugins;
    Games              games;
    Game *             currentGame = nullptr;
    GameProfile        adhocProfile;
    const GameProfile *currentProfile = nullptr;
    StringList         preGamePackages;
    GameProfiles       gameProfiles;
    BusyMode           busyMode;
    Players            players;
    res::Bundles       dataBundles;
    PackageDownloader  packageDownloader;
    SaveGames          saveGames;
    Net                net;
    Dispatch           mainCall;
    Timer              configSaveTimer;

    /**
     * Delegates game change notifications to scripts.
     */
    class GameChangeScriptAudience : DE_OBSERVES(DoomsdayApp, GameChange)
    {
    public:
        void currentGameChanged(const Game &newGame)
        {
            ArrayValue args;
            args << DictionaryValue() << TextValue(newGame.id());
            App::scriptSystem()["App"]["audienceForGameChange"]
                    .array().callElements(args);
        }
    };

    GameChangeScriptAudience scriptAudienceForGameChange;

    Impl(Public *i, const Players::Constructor &playerConstructor, Flags flags)
        : Base(i)
        , appFlags(flags)
        , players(playerConstructor)
    {
        // Script bindings.
        Record &appModule = App::scriptSystem()["App"];
        appModule.addArray("audienceForGameChange"); // game change observers
        audienceForGameChange += scriptAudienceForGameChange;

        initBindings(binder);
        players.initBindings();

        gameProfiles.setGames(games);
        saveGames   .setGames(games);

        audienceForFolderPopulation += this;

        if (~appFlags & DisablePersistentConfig)
        {
            // Periodically save the configuration files (after they've been changed).
            configSaveTimer.setInterval(1.0);
            configSaveTimer.setSingleShot(false);
            configSaveTimer += [this]()
            {
                DE_NOTIFY_PUBLIC(PeriodicAutosave, i)
                {
                    if (!this->busyMode.isActive())
                    {
                        i->periodicAutosave();
                    }
                }
            };
            configSaveTimer.start();
        }

        // File system extensions.
        filesys::RemoteFeedRelay::get().defineLink(IdgamesLink::construct);
    }

    ~Impl() override
    {
        if (initialized && (~appFlags & DisableGameProfiles))
        {
            // Save any changes to the game profiles.
            gameProfiles.serialize();
        }
        // Delete the temporary folder from the system disk.
        if (Folder *tmp = FS::tryLocate<Folder>("/tmp"))
        {
            tmp->destroyAllFilesRecursively();
            tmp->correspondingNativePath().destroy();
        }
        theDoomsdayApp = nullptr;
        Garbage_Recycle();
    }

    Flags directoryPopulationMode(const NativePath &path) const
    {
        const TextValue dir{path.toString()};
        if (Config::get().has("resource.recursedFolders"))
        {
            const auto &elems = Config::get().getdt("resource.recursedFolders").elements();
            auto        i     = elems.find(&dir);
            if (i != elems.end())
            {
                return i->second->isTrue() ? DirectoryFeed::PopulateNativeSubfolders
                                           : DirectoryFeed::OnlyThisFolder;
            }
        }
        return DirectoryFeed::PopulateNativeSubfolders;
    }

    /**
     * Composes a path that currently does not exist. The returned path is a subfolder of
     * @a base and uses segments from @a path.
     */
    Path composeUniqueFolderPath(Path base, const NativePath &path) const
    {
        if (path.segmentCount() >= 2)
        {
            base = base / path.lastSegment();
        }
        // Choose a unique folder name.
        // First try adding another segment.
        Path folderPath = base;
        if (FS::tryLocate<Folder>(folderPath) && path.segmentCount() >= 3)
        {
            folderPath = folderPath.subPath(Rangei(0, folderPath.segmentCount() - 1)) / 
                         path.reverseSegment(1) / folderPath.lastSegment();
        }
        // Add a number.
        int counter = 0;
        while (FS::tryLocate<Folder>(folderPath))
        {
            folderPath = Path(base.toString() + String::format("%03d", ++counter));
        }
        return folderPath;
    }

    bool isValidDataPath(const NativePath &path) const
    {
        // Don't allow the App's built-in /data and /home directories to be re-added.
        for (const char *builtinDir : {"/data", "/home"})
        {
            const auto &folder = FS::locate<Folder>(builtinDir);
            for (const auto *feed : folder.feeds())
            {
                if (const auto *dirFeed = maybeAs<DirectoryFeed>(feed))
                {
                    if (dirFeed->nativePath() == path)
                    {
                        return false;
                    }
                }
            }
        }
        return true;
    }

    void attachWadFeed(const String &    description,
                       const NativePath &path,
                       Flags             populationMode = DirectoryFeed::OnlyThisFolder)
    {
        if (!path.isEmpty())
        {
            if (!isValidDataPath(path))
        {
                LOG_RES_WARNING("Redundant %s WAD folder: %s") << description << path.pretty();
                return;
            }

            if (path.exists())
            {
                LOG_RES_NOTE("Using %s WAD folder%s: %s")
                    << description
                    << (populationMode == DirectoryFeed::OnlyThisFolder ? ""
                                                                        : " (including subfolders)")
                    << path.pretty();

                const Path folderPath = composeUniqueFolderPath(PATH_LOCAL_WADS(), path);
                FS::get().makeFolder(folderPath)
                        .attach(new DirectoryFeed(path, populationMode));
            }
            else
            {
                LOG_RES_NOTE("Ignoring non-existent %s WAD folder: %s")
                        << description << path.pretty();
            }
        }
    }

    void attachPacksFeed(const String &description, const NativePath &path,
                         Flags populationMode)
    {
        if (!path.isEmpty())
        {
            if (!isValidDataPath(path))
            {
                LOG_RES_WARNING("Redundant %s package folder: %s") << description << path.pretty();
                return;
            }

            if (path.exists())
            {
                LOG_RES_NOTE("Using %s package folder%s: %s")
                    << description
                    << (populationMode == DirectoryFeed::OnlyThisFolder ? ""
                                                                        : " (including subfolders)")
                    << path.pretty();
                const Path folderPath = composeUniqueFolderPath(PATH_LOCAL_PACKS(), path);
                FS::get().makeFolder(folderPath)
                    .attach(new DirectoryFeed(path, populationMode));
            }
            else
            {
                LOG_RES_NOTE("Ignoring non-existent %s package folder: %s")
                        << description << path.pretty();
            }
        }
    }

    void initCommandLineFiles(const String &option)
    {
        FileSystem::get().makeFolder("/sys/cmdline", FS::DontInheritFeeds);

        CommandLine::get().forAllParameters(option, [] (duint pos, const String &)
        {
            try
            {
                auto &cmdLine = CommandLine::get();
                cmdLine.makeAbsolutePath(pos);
                Folder &argFolder = FS::get().makeFolder(Stringf("/sys/cmdline/arg%03i", pos));
                const File &argFile = DirectoryFeed::manuallyPopulateSingleFile
                        (cmdLine.at(pos), argFolder);
                // For future reference, store the name of the actual intended file as
                // metadata in the "arg00N" folder. This way we don't need to go looking
                // for it again later.
                argFolder.objectNamespace().set("argPath", argFile.path());
            }
            catch (const Error &er)
            {
                throw Error("DoomsdayApp::initCommandLineFiles",
                            stringf("Problem with file path in command line argument %u: %s",
                                    pos,
                                    er.asText().c_str()));
            }
        });
    }

    /**
     * Doomsday can locate WAD files from various places. This method attaches
     * a set of feeds to /local/wads/ so that all the native folders where the
     * user keeps WAD files are available in the tree.
     */
    void initWadFolders()
    {
        // "/local" is for various files on the local computer.
        Folder &wads = FileSystem::get().makeFolder(PATH_LOCAL_WADS(), FS::DontInheritFeeds);
        wads.clear();
        wads.clearFeeds();

        CommandLine &cmdLine = App::commandLine();
        const NativePath startupPath = cmdLine.startupPath();

        // Feeds are added in ascending priority.

        // Check for games installed using Steam.
        const NativePath steamBase = steamBasePath();
        if (steamBase.exists() && !cmdLine.has("-nosteam"))
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

            for (const auto &appDir : appDirs)
            {
                const NativePath p = steamPath / appDir;
                if (p.exists())
                {
                    attachWadFeed("Steam", p);
                }
            }
        }

        // Check for games installed from GOG.com.
        if (!cmdLine.has("-nogog"))
        {
            for (const NativePath &gogPath : gogComPaths())
            {
                attachWadFeed("GOG.com", gogPath);
            }
        }

#ifdef UNIX
        NativePath const systemWads("/usr/share/games/doom");
        if (systemWads.exists())
        {
            attachWadFeed("system", systemWads);
        }
#endif

        // Add all paths from the DOOMWADPATH environment variable.
        if (getenv("DOOMWADPATH"))
        {
            // Interpreted similarly to the PATH variable.
            StringList paths = String(getenv("DOOMWADPATH")).split(ENV_PATH_SEP_CHAR);
            while (!paths.isEmpty())
            {
                attachWadFeed(_E(m) "DOOMWADPATH" _E(.), startupPath / paths.takeLast());
            }
        }

        // Add the path from the DOOMWADDIR environment variable.
        if (getenv("DOOMWADDIR"))
        {
            attachWadFeed(_E(m) "DOOMWADDIR" _E(.), startupPath / getenv("DOOMWADDIR"));
        }

#ifdef UNIX
        // There may be an iwaddir specified in a system-level config file.
        if (char *fn = UnixInfo_GetConfigValue("paths", "iwaddir"))
        {
            attachWadFeed("UnixInfo " _E(i) "paths.iwaddir" _E(.), startupPath / fn);
            free(fn);
        }
#endif

        // Command line paths.
        for (const char *iwadArg : {"-iwad", "-iwadr"})
        {
            if (auto arg = cmdLine.check(iwadArg, 1)) // has at least one parameter
            {
                for (auto p = dsize(arg.pos + 1); p < cmdLine.size(); ++p)
                {
                    if (cmdLine.isOption(p)) break;

                    cmdLine.makeAbsolutePath(p);
                    attachWadFeed("command-line",
                                  cmdLine.at(p),
                                  iCmpStr(iwadArg, "-iwadr") == 0
                                      ? DirectoryFeed::PopulateNativeSubfolders
                                      : DirectoryFeed::OnlyThisFolder);
                }
            }
        }

        // Configured via GUI.
        for (const String &path : App::config().getStringList("resource.iwadFolder"))
        {
            attachWadFeed("user-selected", path, directoryPopulationMode(path));
        }

        wads.populate(Folder::PopulateAsyncFullTree);
    }

    void initPackageFolders()
    {
        Folder &packs = FS::get().makeFolder(PATH_LOCAL_PACKS(), FS::DontInheritFeeds);
        packs.clear();
        packs.clearFeeds();

        auto &cmdLine = App::commandLine();

#ifdef UNIX
        // There may be an iwaddir specified in a system-level config file.
        if (char *fn = UnixInfo_GetConfigValue("paths", "packsdir"))
        {
            attachPacksFeed("UnixInfo " _E(i) "paths.packsdir" _E(.),
                            cmdLine.startupPath() / fn,
                            DirectoryFeed::DefaultFlags);
            free(fn);
        }
#endif

        // Command line paths.
        if (auto arg = cmdLine.check("-packs", 1))
        {
            for (auto p = dsize(arg.pos + 1); p < cmdLine.size(); ++p)
            {
                if (cmdLine.isOption(p)) break;

                cmdLine.makeAbsolutePath(p);
                attachPacksFeed("command-line", cmdLine.at(p), DirectoryFeed::DefaultFlags);
            }
        }

        // Configured via GUI.
        for (const String &path : App::config().getStringList("resource.packageFolder"))
        {
            attachPacksFeed("user-selected", path, directoryPopulationMode(path));
        }

        packs.populate(Folder::PopulateAsyncFullTree);
    }

    void initRemoteRepositories()
    {
        /// @todo Initialize repositories based on Config.
#if 0
        filesys::RemoteFeedRelay::get().addRepository("https://www.quaddicted.com/files/idgames/",
                                                      "/remote/www.quaddicted.com");
#endif
    }

//    void remoteRepositoryStatusChanged(const String &address, filesys::RemoteFeedRelay::Status status) override
//    {
//        foreach (auto p, filesys::RemoteFeedRelay::get().locatePackages(
//                     StringList({"idgames.levels.doom2.deadmen"})))
//        {
//            qDebug() << p.link->address() << p.localPath << p.remotePath;
//        }
//    }

    void folderPopulationFinished() override
    {
        if (initialized)
        {
            dataBundles.identify();
        }
    }

    void determineGlobalPaths()
    {
#if !defined(DE_WINDOWS)
        // By default, make sure the working path is the home folder.
        App::setCurrentWorkPath(App::app().nativeHomePath());

        // libcore has determined the native base path, so let FS1 know about it.
        self().setDoomsdayBasePath(DE_APP->nativeBasePath());
#else
        // Use a custom base directory?
        if (CommandLine_CheckWith("-basedir", 1))
        {
            self().setDoomsdayBasePath(CommandLine_Next());
        }
        else
        {
            // The default base directory is one level up from the bin dir.
            self().setDoomsdayBasePath(App::executableDir().fileNamePath());
        }
#endif // DE_WINDOWS
    }

    DE_PIMPL_AUDIENCES(
        GameLoad, GameUnload, GameChange, ConsoleRegistration, FileRefresh, PeriodicAutosave)
};

DE_AUDIENCE_METHODS(DoomsdayApp,
                    GameLoad,
                    GameUnload,
                    GameChange,
                    ConsoleRegistration,
                    PeriodicAutosave)

DoomsdayApp::DoomsdayApp(const Players::Constructor &playerConstructor, Flags flags)
    : d(new Impl(this, playerConstructor, flags))
{
    DE_ASSERT(!theDoomsdayApp);
    theDoomsdayApp = this;

    App::app().addInitPackage("net.dengine.base");

    static GameStateFolder::Interpreter intrpGameStateFolder;
    static DataBundle::Interpreter      intrpDataBundle;

    FileSystem::get().addInterpreter(intrpGameStateFolder);
    FileSystem::get().addInterpreter(intrpDataBundle);
}

DoomsdayApp::~DoomsdayApp()
{}

void DoomsdayApp::initialize()
{
    auto &fs = FileSystem::get();

    // Folder for temporary native files.
    Folder &tmpFolder = fs.makeFolder("/tmp");
    tmpFolder.attach(new DirectoryFeed(App::tempPath(),
                                       DirectoryFeed::AllowWrite |
                                       DirectoryFeed::CreateIfMissing |
                                       DirectoryFeed::OnlyThisFolder));
    tmpFolder.populate(Folder::PopulateOnlyThisFolder);

    if (~d->appFlags & DisableSaveGames)
    {
    d->saveGames.initialize();
    }

    // "/sys/bundles" has package-like symlinks to files that are not in
    // Doomsday 2 format but can be loaded as packages.
    fs.makeFolder("/sys/bundles", FS::DontInheritFeeds)
            .attach(new res::BundleLinkFeed); // prunes expired symlinks

    d->initCommandLineFiles("-file");
    d->initWadFolders();
    d->initPackageFolders();

    // We need to access the local file system to complete initialization.
    Folder::waitForPopulation(Folder::BlockingMainThread);

    d->dataBundles.identify();

    if (~d->appFlags & DisableGameProfiles)
    {
    d->gameProfiles.deserialize();
    }

    // Register some remote repositories.
    d->initRemoteRepositories();

    d->initialized = true;
}

void DoomsdayApp::initWadFolders()
{
    FS::waitForIdle();
    d->initWadFolders();
}

void DoomsdayApp::initPackageFolders()
{
    FS::waitForIdle();
    d->initPackageFolders();
}

List<File *> DoomsdayApp::filesFromCommandLine() const
{
    List<File *> files;
    FS::locate<Folder const>("/sys/cmdline").forContents([&files] (String name, File &file)
    {
        try
        {
            if (name.beginsWith("arg"))
            {
                files << &FS::locate<File>(file.as<Folder>().objectNamespace().gets("argPath"));
            }
        }
        catch (const Error &er)
        {
            LOG_RES_ERROR("Problem with a file specified on the command line: %s")
                    << er.asText();
        }
        return LoopContinue;
    });
    return files;
}

void DoomsdayApp::determineGlobalPaths()
{
    d->determineGlobalPaths();
}

DoomsdayApp &DoomsdayApp::app()
{
    DE_ASSERT(theDoomsdayApp);
    return *theDoomsdayApp;
}

PackageDownloader &DoomsdayApp::packageDownloader()
{
    return DoomsdayApp::app().d->packageDownloader;
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

GameProfiles &DoomsdayApp::gameProfiles()
{
    return DoomsdayApp::app().d->gameProfiles;
}

Players &DoomsdayApp::players()
{
    return DoomsdayApp::app().d->players;
}

BusyMode &DoomsdayApp::busyMode()
{
    return DoomsdayApp::app().d->busyMode;
}

SaveGames &DoomsdayApp::saveGames()
{
    return DoomsdayApp::app().d->saveGames;
}

NativePath DoomsdayApp::steamBasePath()
{
#if defined (DE_WINDOWS)
    // The path to Steam can be queried from the registry.
    {
        const String path = WindowsRegistry::textValue(
            "HKEY_CURRENT_USER\\Software\\Valve\\Steam",
            "SteamPath");
        if (path) return path;
    }
    {
        const String path = WindowsRegistry::textValue(
            "HKEY_LOCAL_MACHINE\\Software\\Valve\\Steam",
            "InstallPath");
        if (path) return path;
    }
    return "";
#elif defined (MACOSX)
    return NativePath::homePath() / "Library/Application Support/Steam/";
#else
    /// @todo Where are Steam apps located on Linux?
    return "";
#endif
}

List<NativePath> DoomsdayApp::gogComPaths()
{
    List<NativePath> paths;

#ifdef DE_WINDOWS
    // Look up all the Doom GOG.com paths.
    StringList const subfolders({ "", "doom2", "master\\wads", "Plutonia", "TNT" });
    StringList const gogIds    ({ "1435827232", "1435848814", "1435848742" });
    for (const auto &gogId : gogIds)
    {
        NativePath basePath = WindowsRegistry::textValue(
            "HKEY_LOCAL_MACHINE\\Software\\GOG.com\\Games\\" + gogId, "PATH");
        if (basePath.isEmpty())
        {
            basePath = WindowsRegistry::textValue(
                "HKEY_LOCAL_MACHINE\\Software\\WOW6432Node\\GOG.com\\Games\\" + gogId, "PATH");
        }
        if (!basePath.isEmpty())
        {
            for (const auto &sub : subfolders)
            {
                NativePath path(basePath / sub);
                if (path.exists())
                {
                    paths << path;
                }
            }
        }
    }
#endif

    return paths;
}

bool DoomsdayApp::isShuttingDown() const
{
    return d->shuttingDown;
}

void DoomsdayApp::setShuttingDown(bool shuttingDown)
{
    d->shuttingDown = shuttingDown;
}

const std::string &DoomsdayApp::doomsdayBasePath() const
{
    return d->ddBasePath;
}

GameProfile &DoomsdayApp::adhocProfile()
{
    return d->adhocProfile;
}

void DoomsdayApp::setDoomsdayBasePath(const NativePath &path)
{
    NativePath cleaned = App::commandLine().startupPath() / path; // In case it's relative.
    cleaned.addTerminatingSeparator();

    d->ddBasePath = cleaned.toString().toStdString();
}

// #ifdef WIN32
// void *DoomsdayApp::moduleHandle() const
// {
//     return d->hInstance;
// }
// #endif

const Game &DoomsdayApp::game()
{
    DE_ASSERT(app().d->currentGame != 0);
    return *app().d->currentGame;
}

const GameProfile *DoomsdayApp::currentGameProfile()
{
    return app().d->currentProfile;
}

bool DoomsdayApp::isGameLoaded()
{
    return App::appExists() && !DoomsdayApp::game().isNull();
}

StringList DoomsdayApp::loadedPackagesAffectingGameplay() // static
{
    StringList ids = PackageLoader::get().loadedPackageIdsInOrder();
    for (auto iter = ids.begin(); iter != ids.end(); )
    {
        if (!GameStateFolder::isPackageAffectingGameplay(*iter))
        {
            iter = ids.erase(iter);
        }
        else
        {
            ++iter;
        }
    }
    return ids;
}

void DoomsdayApp::unloadGame(const GameProfile &/*upcomingGame*/)
{
    auto &gx = plugins().gameExports();

    if (App_GameLoaded())
    {
        LOG_MSG("Unloading game...");

        if (gx.Shutdown)
        {
            gx.Shutdown();
        }

        // Tell the plugin it is being unloaded.
        {
            void *unloader = plugins().findEntryPoint(game().pluginId(), "DP_Unload");
            LOGDEV_MSG("Calling DP_Unload %p") << unloader;
            plugins().setActivePluginId(game().pluginId());
            if (unloader) reinterpret_cast<pluginfunc_t>(unloader)();
            plugins().setActivePluginId(0);
        }

        // Unload all packages that weren't loaded before the game was loaded.
        auto packagesToUnload = PackageLoader::get().loadedPackages();
        for (const auto &i : packagesToUnload)
        {
            if (!d->preGamePackages.contains(i.first))
            {
                PackageLoader::get().unload(i.first);
            }
        }

        // Clear application and subsystem state.
        reset();
        Resources::get().clear();

        // We do not want to load session resources specified on the command line again.
//        AbstractSession::profile().resourceFiles.clear();

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
    world::Materials::get().clearAllMaterialSchemes();
}

void DoomsdayApp::uncacheFilesFromMemory()
{
    ArchiveFeed::uncacheAllEntries(StringList({ DE_TYPE_NAME(Folder),
                                                DE_TYPE_NAME(ArchiveFolder),
                                                DE_TYPE_NAME(DataFolder),
                                                DE_TYPE_NAME(GameStateFolder) }));
}

void DoomsdayApp::clearCache()
{
    LOG_RES_NOTE("Clearing metadata cache contents");
    MetadataBank::get().clear();
}

void DoomsdayApp::reset()
{
    // Reset the world back to it's initial state (unload the map, reset players, etc...).
    World::get().reset();
    uncacheFilesFromMemory();

    Z_FreeTags(PU_GAMESTATIC, PU_PURGELEVEL - 1);

    P_ShutdownMapEntityDefs();

    // Reinitialize the console.
    Con_ClearDatabases();
    Con_InitDatabases();
    DE_NOTIFY(ConsoleRegistration, i)
    {
        i->consoleRegistration();
    }

    d->currentProfile = nullptr;
}

void DoomsdayApp::gameSessionWasSaved(const AbstractSession &, GameStateFolder &)
{
    //qDebug() << "App saving to" << toFolder.description();
}

void DoomsdayApp::gameSessionWasLoaded(const AbstractSession &, const GameStateFolder &)
{
    //qDebug() << "App loading from" << fromFolder.description();
}

Net &DoomsdayApp::net()
{
    return app().d->net;
}

void DoomsdayApp::setGame(const Game &game)
{
    app().d->currentGame = const_cast<Game *>(&game);
}

void DoomsdayApp::makeGameCurrent(const GameProfile &profile)
{
    const auto &newGame = profile.game();

    if (!newGame.isNull())
    {
        LOG_MSG("Loading game \"%s\"...") << profile.name();
    }

    //Library_ReleaseGames();

    if (!isShuttingDown())
    {
        // Re-initialize subsystems needed even when in Home.
        if (!plugins().exchangeGameEntryPoints(newGame.pluginId()))
        {
            throw Plugins::EntryPointError("DoomsdayApp::makeGameCurrent",
                                           stringf("Failed to exchange entrypoints with plugin %i",
                                                   newGame.pluginId()));
        }
    }

    // This is now the current game.
    setGame(newGame);
    d->currentProfile = &profile;

    profile.checkSaveLocation(); // in case it's gone missing

    if (!newGame.isNull())
    {
        // Remember what was loaded beforehand.
        d->preGamePackages = PackageLoader::get().loadedPackageIdsInOrder(PackageLoader::NonVersioned);

        // Ensure game profiles have been saved.
        d->gameProfiles.serialize();
    }

    try
    {
        profile.loadPackages();
    }
    catch (const Error &er)
    {
        LOG_RES_ERROR("Failed to load the packages of profile \"%s\": %s")
                << profile.name()
                << er.asText();
    }
}

// from game_init.cpp
extern int beginGameChangeBusyWorker(void *context);
extern int loadGameStartupResourcesBusyWorker(void *context);
extern int loadAddonResourcesBusyWorker(void *context);

bool DoomsdayApp::changeGame(const GameProfile &profile,
                             const std::function<int (void *)> &gameActivationFunc,
                             Behaviors behaviors)
{
    const auto &newGame = profile.game();

    const bool arePackagesDifferent =
            !GameProfiles::arePackageListsCompatible(DoomsdayApp::app().loadedPackagesAffectingGameplay(),
                                                     profile.packagesAffectingGameplay());

    // Ignore attempts to reload the current game?
    if (game().id() == newGame.id() && !arePackagesDifferent)
    {
        // We are reloading.
        if (!behaviors.testFlag(AllowReload))
        {
            if (isGameLoaded())
            {
                LOG_NOTE("%s (%s) is already loaded") << newGame.title() << newGame.id();
            }
            return true;
        }
    }

    d->gameBeingChanged = true;

    // The current game will now be unloaded.
    DE_NOTIFY(GameUnload, i) i->aboutToUnloadGame(game());
    unloadGame(profile);

    // Do the switch.
    DE_NOTIFY(GameLoad, i) i->aboutToLoadGame(newGame);
    makeGameCurrent(profile);

    /*
     * If we aren't shutting down then we are either loading a game or switching
     * to Home (the current game will have already been unloaded).
     */
    if (!isShuttingDown())
    {
        /*
         * The bulk of this we can do in busy mode unless we are already busy
         * (which can happen if a fatal error occurs during game load and we must
         * shutdown immediately; Sys_Shutdown will call back to load the special
         * "null-game" game).
         */
        const dint busyMode = BUSYF_PROGRESS_BAR; //  | (verbose? BUSYF_CONSOLE_OUTPUT : 0);
        GameChangeParameters p;
        BusyTask gameChangeTasks[] =
        {
            // Phase 1: Initialization.
            { beginGameChangeBusyWorker,          &p, busyMode, "Loading game...",   200, 0.0f, 0.1f },

            // Phase 2: Loading "startup" resources.
            { loadGameStartupResourcesBusyWorker, &p, busyMode, nullptr,             200, 0.1f, 0.3f },

            // Phase 3: Loading "add-on" resources.
            { loadAddonResourcesBusyWorker,       &p, busyMode, "Loading add-ons...", 200, 0.3f, 0.7f },

            // Phase 4: Game activation.
            { gameActivationFunc,                 &p, busyMode, "Starting game...",  200, 0.7f, 1.0f }
        };

        p.initiatedBusyMode = !BusyMode_Active();

        if (isGameLoaded())
        {
            // Tell the plugin it is being loaded.
            /// @todo Must this be done in the main thread?
            void *loader = plugins().findEntryPoint(game().pluginId(), "DP_Load");
            LOGDEV_MSG("Calling DP_Load %p") << loader;
            plugins().setActivePluginId(game().pluginId());
            if (loader) reinterpret_cast<pluginfunc_t>(loader)();
            plugins().setActivePluginId(0);
        }

        /// @todo Kludge: Use more appropriate task names when unloading a game.
        if (newGame.isNull())
        {
            gameChangeTasks[0].name = "Unloading game...";
            gameChangeTasks[3].name = "Switching to Home...";
        }
        // kludge end

        BusyMode_RunTasks(gameChangeTasks, sizeof(gameChangeTasks)/sizeof(gameChangeTasks[0]));

        if (isGameLoaded())
        {
            Game::printBanner(game());
        }
    }

    DE_ASSERT(plugins().activePluginId() == 0);

    d->gameBeingChanged = false;

    // Game change is complete.
    DE_NOTIFY(GameChange, i)
    {
        i->currentGameChanged(game());
    }
    return true;
}

bool DoomsdayApp::isGameBeingChanged() // static
{
    return app().d->gameBeingChanged;
}

bool App_GameLoaded()
{
    return DoomsdayApp::isGameLoaded();
}
