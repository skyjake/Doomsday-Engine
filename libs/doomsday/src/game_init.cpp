/** @file game_init.cpp  Routines for initializing a game.
 *
 * @authors Copyright (c) 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright (c) 2005-2015 Daniel Swanson <danij@dengine.net>
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
#include "doomsday/busymode.h"
#include "doomsday/abstractsession.h"
#include "doomsday/console/var.h"
#include "doomsday/filesys/fs_main.h"
#include "doomsday/filesys/virtualmappings.h"
#include "doomsday/filesys/wad.h"
#include "doomsday/res/bundles.h"
#include "doomsday/manifest.h"
#include "doomsday/world/entitydef.h"

#include <de/app.h>
#include <de/archivefeed.h>
#include <de/archiveentryfile.h>
#include <de/logbuffer.h>
#include <de/nativefile.h>
#include <de/packageloader.h>
#include <de/legacy/findfile.h>
#include <de/legacy/memory.h>

using namespace de;
using namespace res;

static void updateProgress(int progress)
{
    DE_FOR_OBSERVERS(i, DoomsdayApp::games().audienceForProgress())
    {
        i->gameWorkerProgress(progress);
    }
}

int beginGameChangeBusyWorker(void *context)
{
    const DoomsdayApp::GameChangeParameters *parms = reinterpret_cast<
            DoomsdayApp::GameChangeParameters *>(context);

    P_InitMapEntityDefs();
    if (parms->initiatedBusyMode)
    {
        updateProgress(200);
    }
    return 0;
}

static File1 *tryLoadFile(const res::Uri &search, size_t baseOffset = 0)
{
    auto &fs1 = App_FileSystem();
    try
    {
        FileHandle &hndl = fs1.openFile(search.path(), "rb", baseOffset, false /* no duplicates */);

        res::Uri foundFileUri = hndl.file().composeUri();
        LOG_VERBOSE("Loading \"%s\"...") << NativePath(foundFileUri.asText()).pretty();

        fs1.index(hndl.file());

        return &hndl.file();
    }
    catch (FS1::NotFoundError const&)
    {
        if (fs1.accessFile(search))
        {
            // Must already be loaded.
            LOG_RES_XVERBOSE("\"%s\" already loaded", NativePath(search.asText()).pretty());
        }
    }
    return nullptr;
}

namespace res {

// Helper function for accessing files via the legacy FS1.
static void forNativeDataFiles(const DataBundle &bundle, std::function<void (const String &)> func)
{
    DE_ASSERT(bundle.isLinkedAsPackage()); // couldn't be accessed otherwise

    switch (bundle.format())
    {
    case DataBundle::Iwad:
    case DataBundle::Pwad:
    case DataBundle::Lump:
    case DataBundle::Pk3:
    {
        const Record &meta = bundle.packageMetadata();
        for (const auto *v : meta.geta("dataFiles").elements())
        {
//            LOG_RES_MSG("bundle root: %s -> trying to load %s") << bundle.rootPath()
//                     << v->asText();
            const String dataFilePath = bundle.rootPath() / v->asText();
            if (const File *dataFile = FS::tryLocate<File const>(dataFilePath))
            {
                if (is<NativeFile>(dataFile->source()))
                {
                    func(dataFilePath);
                }
                else
                {
                    LOG_RES_WARNING("%s: cannot access data file within another file")
                            << dataFile->description();
                }
            }
        }
        break;
    }

    default:
        break;
    }
}

File1 *File1::tryLoad(LoadFileMode loadMode, const Uri &search, size_t baseOffset) // static
{
    File1 *file = tryLoadFile(search, baseOffset);
    if (file)
    {
        file->setCustom(loadMode == LoadAsCustomFile);
    }
    return file;
}

bool File1::tryUnload(const Uri &search) // static
{
    try
    {
        File1 &file = App_FileSystem().find(search);
        res::Uri foundFileUri = file.composeUri();
        NativePath nativePath(foundFileUri.asText());

        // Do not attempt to unload a resource required by the current game.
        if (DoomsdayApp::game().isRequiredFile(file))
        {
            LOG_RES_NOTE("\"%s\" is required by the current game."
                         " Required game files cannot be unloaded in isolation.")
                    << nativePath.pretty();
            return false;
        }

        LOG_RES_VERBOSE("Unloading \"%s\"...") << nativePath.pretty();

        App_FileSystem().deindex(file);
        delete &file;

        return true;
    }
    catch (const FS1::NotFoundError &er)
    {
        LOG_RES_MSG("Cannot unload file: %s") << er.asText();
        return false;
    }
}

File1 *File1::tryLoad(const DataBundle &bundle)
{
    // If the bundle has been identified based on the known criteria, treat it as
    // one of the vanilla files.
    LoadFileMode loadMode = bundle.packageMetadata().geti("bundleScore", 0) > 0? File1::LoadAsVanillaFile
                                                                               : File1::LoadAsCustomFile;
    LOG_RES_NOTE("Loading %s (as %s)")
            << bundle.description()
            << (loadMode == LoadAsVanillaFile? "vanilla" : "custom");

    File1 *result = nullptr;
    forNativeDataFiles(bundle, [&result, loadMode] (const String &path)
    {
        const NativeFile &dataFile = App::rootFolder().locate<File const>(path).source()->as<NativeFile>();
        if (File1 *file = tryLoad(loadMode, res::Uri::fromNativePath(dataFile.nativePath())))
        {
            result = file; // note: multiple files may actually be loaded
            LOG_RES_VERBOSE("%s: ok") << dataFile.nativePath();
        }
        else
        {
            LOG_RES_WARNING("%s: could not load file") << dataFile.nativePath();
        }
    });
    return result;
}

bool File1::tryUnload(const DataBundle &bundle)
{
    LOG_RES_NOTE("Unloading %s") << bundle.description();

    bool unloaded = false;
    forNativeDataFiles(bundle, [&unloaded] (const String &path)
    {
        const NativeFile &dataFile = App::rootFolder().locate<File const>(path).source()->as<NativeFile>();
        unloaded = tryUnload(res::Uri::fromNativePath(dataFile.nativePath()));
    });
    return unloaded;
}

} // namespace res

static void loadResource(ResourceManifest &manifest)
{
    DE_ASSERT(manifest.resourceClass() == RC_PACKAGE);

    res::Uri path(manifest.resolvedPath(false/*do not locate resource*/), RC_NULL);
    if (path.isEmpty()) return;

    if (File1 *file = tryLoadFile(path))
    {
        // Mark this as an original game resource.
        file->setCustom(false);

        // Print the 'CRC' number of IWADs, so they can be identified.
        if (Wad *wad = maybeAs<Wad>(file))
        {
            LOG_RES_MSG("IWAD identification: %08x") << wad->calculateCRC();
        }
    }
}

static void parseStartupFilePathsAndAddFiles(const char *pathString)
{
    static const char *ATWSEPS = ",; \t";

    if (!pathString || !pathString[0]) return;

    size_t len = strlen(pathString);
    char *buffer = (char *) M_Malloc(len + 1);

    strcpy(buffer, pathString);
    char *token = strtok(buffer, ATWSEPS);
    while (token)
    {
        tryLoadFile(res::makeUri(token));
        token = strtok(nullptr, ATWSEPS);
    }
    M_Free(buffer);
}

static dint addListFiles(const StringList &list, const FileType &ftype)
{
    dint numAdded = 0;
    for (const auto &path : list)
    {
        if (&ftype != &DD_GuessFileTypeFromFileName(path))
        {
            continue;
        }
        if (tryLoadFile(res::makeUri(path)))
        {
            numAdded += 1;
        }
    }
    return numAdded;
}

int loadGameStartupResourcesBusyWorker(void *context)
{
    DoomsdayApp::GameChangeParameters &parms = *(DoomsdayApp::GameChangeParameters *) context;

    // Reset file Ids so previously seen files can be processed again.
    App_FileSystem().resetFileIds();
    FS_InitVirtualPathMappings();
    App_FileSystem().resetAllSchemes();

    if (parms.initiatedBusyMode)
    {
        updateProgress(50);
    }

    if (App_GameLoaded())
    {
        // Create default Auto mappings in the runtime directory.

        // Data class resources.
        App_FileSystem().addPathMapping("auto/", res::makeUri("$(App.DataPath)/$(GamePlugin.Name)/auto/").resolved());

        // Definition class resources.
        App_FileSystem().addPathMapping("auto/", res::makeUri("$(App.DefsPath)/$(GamePlugin.Name)/auto/").resolved());
    }

    // Load data files.
    for (const DataBundle *bundle : DataBundle::loadedBundles())
    {
        File1::tryLoad(*bundle);
    }

    /**
     * Open all the files, load headers, count lumps, etc, etc...
     * @note  Duplicate processing of the same file is automatically guarded
     *        against by the virtual file system layer.
     */
    const GameManifests &gameManifests = DoomsdayApp::game().manifests();
    const dsize numPackages = gameManifests.count(RC_PACKAGE);
    if (numPackages)
    {
        LOG_RES_MSG("Loading game resources...");

        int packageIdx = 0;
        const auto packages = gameManifests.equal_range(RC_PACKAGE);
        for (auto i = packages.first; i != packages.second; ++i, ++packageIdx)
        {
            loadResource(*i->second);

            // Update our progress.
            if (parms.initiatedBusyMode)
            {
                updateProgress((packageIdx + 1) * (200 - 50) / numPackages - 1);
            }
        }
    }

    if (parms.initiatedBusyMode)
    {
        updateProgress(200);
    }

    return 0;
}

/**
 * Find all game data file paths in the auto directory with the extensions
 * wad, lmp, pk3, zip and deh.
 *
 * @param found  List of paths to be populated.
 *
 * @return  Number of paths added to @a found.
 */
static dint findAllGameDataPaths(FS1::PathList &found)
{
    static String const extensions[] = {
        "wad", "lmp", "pk3", "zip", "deh"
#ifdef UNIX
        "WAD", "LMP", "PK3", "ZIP", "DEH" // upper case alternatives
#endif
    };
    const dint numFoundSoFar = found.count();
    for (const String &ext : extensions)
    {
        DE_ASSERT(!ext.isEmpty());
        const String searchPath = res::Uri(Path("$(App.DataPath)/$(GamePlugin.Name)/auto/*." + ext)).resolved();
        App_FileSystem().findAllPaths(searchPath, 0, found);
    }
    return found.count() - numFoundSoFar;
}

/**
 * Find and try to load all game data file paths in auto directory.
 *
 * @return Number of new files that were loaded.
 */
static dint loadFilesFromDataGameAuto()
{
    FS1::PathList found;
    findAllGameDataPaths(found);

    dint numLoaded = 0;
    DE_FOR_EACH_CONST(FS1::PathList, i, found)
    {
        // Ignore directories.
        if (i->attrib & A_SUBDIR) continue;

        if (tryLoadFile(res::makeUri(i->path)))
        {
            numLoaded += 1;
        }
    }
    return numLoaded;
}

/**
 * Looks for new files to autoload from the auto-load data directory.
 */
static void autoLoadFiles()
{
    /**
     * Keep loading files if any are found because virtual files may now
     * exist in the auto-load directory.
     */
    dint numNewFiles;
    while ((numNewFiles = loadFilesFromDataGameAuto()) > 0)
    {
        LOG_RES_VERBOSE("Autoload round completed with %i new files") << numNewFiles;
    }
}

int loadAddonResourcesBusyWorker(void *context)
{
    DoomsdayApp::GameChangeParameters &parms = *(DoomsdayApp::GameChangeParameters *) context;

    const char *startupFiles = CVar_String(Con_FindVariable("file-startup"));

    /**
     * Add additional game-startup files.
     * @note These must take precedence over Auto but not game-resource files.
     */
    if (startupFiles && startupFiles[0])
    {
        parseStartupFilePathsAndAddFiles(startupFiles);
    }

    if (parms.initiatedBusyMode)
    {
        updateProgress(50);
    }

    if (App_GameLoaded())
    {
        /**
         * Phase 3: Add real files from the Auto directory.
         */
        //auto &prof = AbstractSession::profile();
        StringList resourceFiles;

        FS1::PathList found;
        findAllGameDataPaths(found);
        DE_FOR_EACH_CONST(FS1::PathList, i, found)
        {
            // Ignore directories.
            if (i->attrib & A_SUBDIR) continue;

            /// @todo Is expansion of symbolics still necessary here?
            resourceFiles << NativePath(i->path).expand().withSeparators('/');
        }

        if (!resourceFiles.isEmpty())
        {
            // First ZIPs then WADs (they may contain WAD files).
            addListFiles(resourceFiles, DD_FileTypeByName("FT_ZIP"));
            addListFiles(resourceFiles, DD_FileTypeByName("FT_WAD"));
        }

        // Final autoload round.
        autoLoadFiles();
    }

    if (parms.initiatedBusyMode)
    {
        updateProgress(180);
    }

    FS_InitPathLumpMappings();

    // Re-initialize the resource locator as there are now new resources to be found
    // on existing search paths (probably that is).
    App_FileSystem().resetAllSchemes();

    if (parms.initiatedBusyMode)
    {
        updateProgress(200);
    }

    return 0;
}
