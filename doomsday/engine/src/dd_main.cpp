/**
 * @file dd_main.cpp
 *
 * Engine core.
 *
 * @ingroup core
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#ifdef WIN32
#  define _WIN32_DCOM
#  include <objbase.h>
#endif

#include <QStringList>
#include <de/App>
#include <de/NativePath>

#include "de_platform.h"

#ifdef UNIX
#  include <ctype.h>
#endif

#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"
#include "de_network.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_audio.h"
#include "de_ui.h"
#include "de_bsp.h"
#include "de_edit.h"
#include "de_filesys.h"
#include "de_resource.h"

#include "gl/svg.h"
#include "ui/displaymode.h"
#include "updater.h"
#include "m_misc.h"

extern int renderTextures;
extern int monochrome;

using namespace de;

class ZipFileType : public de::NativeFileType
{
public:
    ZipFileType() : NativeFileType("FT_ZIP", RC_PACKAGE)
    {}

    de::File1* interpret(de::FileHandle& hndl, String path, FileInfo const& info) const
    {
        if(Zip::recognise(hndl))
        {
            LOG_AS("ZipFileType");
            LOG_VERBOSE("Interpreted \"" + NativePath(path).pretty() + "\".");
            return new Zip(hndl, path, info);
        }
        return 0;
    }
};

class WadFileType : public de::NativeFileType
{
public:
    WadFileType() : NativeFileType("FT_WAD", RC_PACKAGE)
    {}

    de::File1* interpret(de::FileHandle& hndl, String path, FileInfo const& info) const
    {
        if(Wad::recognise(hndl))
        {
            LOG_AS("WadFileType");
            LOG_VERBOSE("Interpreted \"" + NativePath(path).pretty() + "\".");
            return new Wad(hndl, path, info);
        }
        return 0;
    }
};

typedef struct ddvalue_s {
    int*            readPtr;
    int*            writePtr;
} ddvalue_t;

static int DD_StartupWorker(void* parameters);
static int DD_DummyWorker(void* parameters);
static void DD_AutoLoad();
static void initPathMappings();

/**
 * @param path          Path to the file to be loaded. Either a "real" file in
 *                      the local file system, or a "virtual" file.
 * @param baseOffset    Offset from the start of the file in bytes to begin.
 *
 * @return  @c true iff the referenced file was loaded.
 */
static de::File1* tryLoadFile(de::Uri const& path, size_t baseOffset = 0);

static bool tryUnloadFile(de::Uri const& path);

filename_t ddBasePath = ""; // Doomsday root directory is at...?
filename_t ddRuntimePath, ddBinPath;

int isDedicated;

int verbose; // For debug messages (-verbose).

// List of file names, whitespace seperating (written to .cfg).
char* startupFiles = (char*) ""; // ignore warning

// Id of the currently running title finale if playing, else zero.
finaleid_t titleFinale;

int gameDataFormat; // Use a game-specifc data format where applicable.

static NullFileType nullFileType;
static NullResourceClass nullResourceClass;
static ResourceClasses resourceClasses;

/// A symbolic name => file type map.
static FileTypes fileTypeMap;

// List of session data files (specified via the command line or in a cfg, or
// found using the default search algorithm (e.g., /auto and DOOMWADDIR)).
static ddstring_t** sessionResourceFileList;
static size_t numSessionResourceFileList;

#ifndef WIN32
extern GETGAMEAPI GetGameAPI;
#endif

// The Game collection.
static de::GameCollection* games;

D_CMD(CheckForUpdates)
{
    DENG_UNUSED(src); DENG_UNUSED(argc); DENG_UNUSED(argv);

    Con_Message("Checking for available updates...\n");
    Updater_CheckNow(false/* don't notify */);
    return true;
}

D_CMD(CheckForUpdatesAndNotify)
{
    DENG_UNUSED(src); DENG_UNUSED(argc); DENG_UNUSED(argv);

    /// @todo Combine into the same command with CheckForUpdates?
    Con_Message("Checking for available updates...\n");
    Updater_CheckNow(true/* do notify */);
    return true;
}

D_CMD(LastUpdated)
{
    DENG_UNUSED(src); DENG_UNUSED(argc); DENG_UNUSED(argv);

    Updater_PrintLastUpdated();
    return true;
}

D_CMD(ShowUpdateSettings)
{
    DENG_UNUSED(src); DENG_UNUSED(argc); DENG_UNUSED(argv);

    Updater_ShowSettings();
    return true;
}

void DD_CreateResourceClasses()
{
    resourceClasses.push_back(new ResourceClass("RC_PACKAGE",       "Packages"));
    resourceClasses.push_back(new ResourceClass("RC_DEFINITION",    "Defs"));
    resourceClasses.push_back(new ResourceClass("RC_GRAPHIC",       "Graphics"));
    resourceClasses.push_back(new ResourceClass("RC_MODEL",         "Models"));
    resourceClasses.push_back(new ResourceClass("RC_SOUND",         "Sfx"));
    resourceClasses.push_back(new ResourceClass("RC_MUSIC",         "Music"));
    resourceClasses.push_back(new ResourceClass("RC_FONT",          "Fonts"));
}

void DD_ClearResourceClasses()
{
    DENG2_FOR_EACH(ResourceClasses, i, resourceClasses)
    {
        delete *i;
    }
    resourceClasses.clear();
}

ResourceClass& DD_ResourceClassByName(String name)
{
    if(!name.isEmpty())
    {
        DENG2_FOR_EACH_CONST(ResourceClasses, i, resourceClasses)
        {
            ResourceClass& rclass = **i;
            if(!rclass.name().compareWithoutCase(name))
                return rclass;
        }
    }
    return nullResourceClass; // Not found.
}

ResourceClass& DD_ResourceClassById(resourceclassid_t id)
{
    if(id == RC_NULL) return nullResourceClass;
    if(!VALID_RESOURCECLASSID(id))
    {
        QByteArray msg = String("DD_ResourceClassById: Invalid id '%1'").arg(int(id)).toUtf8();
        LegacyCore_FatalError(msg.constData());
    }
    return *resourceClasses[uint(id)];
}

void DD_CreateFileTypes()
{
    FileType* ftype;

    /*
     * Packages types:
     */
    ResourceClass& packageClass = DD_ResourceClassByName("RC_PACKAGE");

    ftype = new ZipFileType();
    ftype->addKnownExtension(".pk3");
    ftype->addKnownExtension(".zip");
    packageClass.addFileType(*ftype);
    fileTypeMap.insert(ftype->name().toLower(), ftype);

    ftype = new WadFileType();
    ftype->addKnownExtension(".wad");
    packageClass.addFileType(*ftype);
    fileTypeMap.insert(ftype->name().toLower(), ftype);

    ftype = new FileType("FT_LMP", RC_PACKAGE); ///< Treat lumps as packages so they are mapped to $App.DataPath.
    ftype->addKnownExtension(".lmp");
    fileTypeMap.insert(ftype->name().toLower(), ftype);

    /*
     * Definition fileTypes:
     */
    ftype = new FileType("FT_DED", RC_DEFINITION);
    ftype->addKnownExtension(".ded");
    DD_ResourceClassByName("RC_DEFINITION").addFileType(*ftype);
    fileTypeMap.insert(ftype->name().toLower(), ftype);

    /*
     * Graphic fileTypes:
     */
    ResourceClass& graphicClass = DD_ResourceClassByName("RC_GRAPHIC");

    ftype = new FileType("FT_PNG", RC_GRAPHIC);
    ftype->addKnownExtension(".png");
    graphicClass.addFileType(*ftype);
    fileTypeMap.insert(ftype->name().toLower(), ftype);

    ftype = new FileType("FT_TGA", RC_GRAPHIC);
    ftype->addKnownExtension(".tga");
    graphicClass.addFileType(*ftype);
    fileTypeMap.insert(ftype->name().toLower(), ftype);

    ftype = new FileType("FT_JPG", RC_GRAPHIC);
    ftype->addKnownExtension(".jpg");
    graphicClass.addFileType(*ftype);
    fileTypeMap.insert(ftype->name().toLower(), ftype);

    ftype = new FileType("FT_PCX", RC_GRAPHIC);
    ftype->addKnownExtension(".pcx");
    graphicClass.addFileType(*ftype);
    fileTypeMap.insert(ftype->name().toLower(), ftype);

    /*
     * Model fileTypes:
     */
    ResourceClass& modelClass = DD_ResourceClassByName("RC_MODEL");

    ftype = new FileType("FT_DMD", RC_MODEL);
    ftype->addKnownExtension(".dmd");
    modelClass.addFileType(*ftype);
    fileTypeMap.insert(ftype->name().toLower(), ftype);

    ftype = new FileType("FT_MD2", RC_MODEL);
    ftype->addKnownExtension(".md2");
    modelClass.addFileType(*ftype);
    fileTypeMap.insert(ftype->name().toLower(), ftype);

    /*
     * Sound fileTypes:
     */
    ftype = new FileType("FT_WAV", RC_SOUND);
    ftype->addKnownExtension(".wav");
    DD_ResourceClassByName("RC_SOUND").addFileType(*ftype);
    fileTypeMap.insert(ftype->name().toLower(), ftype);

    /*
     * Music fileTypes:
     */
    ResourceClass& musicClass = DD_ResourceClassByName("RC_MUSIC");

    ftype = new FileType("FT_OGG", RC_MUSIC);
    ftype->addKnownExtension(".ogg");
    musicClass.addFileType(*ftype);
    fileTypeMap.insert(ftype->name().toLower(), ftype);

    ftype = new FileType("FT_MP3", RC_MUSIC);
    ftype->addKnownExtension(".mp3");
    musicClass.addFileType(*ftype);
    fileTypeMap.insert(ftype->name().toLower(), ftype);

    ftype = new FileType("FT_MOD", RC_MUSIC);
    ftype->addKnownExtension(".mod");
    musicClass.addFileType(*ftype);
    fileTypeMap.insert(ftype->name().toLower(), ftype);

    ftype = new FileType("FT_MID", RC_MUSIC);
    ftype->addKnownExtension(".mid");
    musicClass.addFileType(*ftype);
    fileTypeMap.insert(ftype->name().toLower(), ftype);

    /*
     * Font fileTypes:
     */
    ftype = new FileType("FT_DFN", RC_FONT);
    ftype->addKnownExtension(".dfn");
    DD_ResourceClassByName("RC_FONT").addFileType(*ftype);
    fileTypeMap.insert(ftype->name().toLower(), ftype);

    /*
     * Misc fileTypes:
     */
    ftype = new FileType("FT_DEH", RC_PACKAGE); ///< Treat DeHackEd patches as packages so they are mapped to $App.DataPath.
    ftype->addKnownExtension(".deh");
    fileTypeMap.insert(ftype->name().toLower(), ftype);
}

FileType& DD_FileTypeByName(String name)
{
    if(!name.isEmpty())
    {
        FileTypes::iterator found = fileTypeMap.find(name.toLower());
        if(found != fileTypeMap.end()) return **found;
    }
    return nullFileType; // Not found.
}

FileType& DD_GuessFileTypeFromFileName(String path)
{
    if(!path.isEmpty())
    {
        DENG2_FOR_EACH_CONST(FileTypes, i, fileTypeMap)
        {
            FileType& ftype = **i;
            if(ftype.fileNameIsKnown(path))
                return ftype;
        }
    }
    return nullFileType;
}

FileTypes const& DD_FileTypes()
{
    return fileTypeMap;
}

static void createPackagesScheme()
{
    FS1::Scheme& scheme = App_FileSystem()->createScheme("Packages");

    /*
     * Add default search paths.
     *
     * Note that the order here defines the order in which these paths are searched
     * thus paths must be added in priority order (newer paths have priority).
     */

#ifdef UNIX
    // There may be an iwaddir specified in a system-level config file.
    filename_t fn;
    if(UnixInfo_GetConfigValue("paths", "iwaddir", fn, FILENAME_T_MAXLEN))
    {
        NativePath path = de::App::app().commandLine().startupPath() / fn;
        scheme.addSearchPath(SearchPath(de::Uri::fromNativeDirPath(path), SearchPath::NoDescend));
        LOG_INFO("Using paths.iwaddir: %s") << path.pretty();
    }
#endif

    // Add the path from the DOOMWADDIR environment variable.
    if(!CommandLine_Check("-nodoomwaddir") && getenv("DOOMWADDIR"))
    {
        NativePath path = App::app().commandLine().startupPath() / getenv("DOOMWADDIR");
        scheme.addSearchPath(SearchPath(de::Uri::fromNativeDirPath(path), SearchPath::NoDescend));
        LOG_INFO("Using DOOMWADDIR: %s") << path.pretty();
    }

    // Add any paths from the DOOMWADPATH environment variable.
    if(!CommandLine_Check("-nodoomwadpath") && getenv("DOOMWADPATH"))
    {
#if WIN32
#  define SEP_CHAR      ';'
#else
#  define SEP_CHAR      ':'
#endif

        QStringList allPaths = QString(getenv("DOOMWADPATH")).split(SEP_CHAR, QString::SkipEmptyParts);
        for(int i = allPaths.count(); i--> 0; )
        {
            NativePath path = App::app().commandLine().startupPath() / allPaths[i];
            scheme.addSearchPath(SearchPath(de::Uri::fromNativeDirPath(path), SearchPath::NoDescend));
            LOG_INFO("Using DOOMWADPATH: %s") << path.pretty();
        }

#undef SEP_CHAR
    }

    scheme.addSearchPath(SearchPath(de::Uri("$(App.DataPath)/", RC_NULL), SearchPath::NoDescend));
    scheme.addSearchPath(SearchPath(de::Uri("$(App.DataPath)/$(GamePlugin.Name)/", RC_NULL), SearchPath::NoDescend));
}

void DD_CreateFileSystemSchemes()
{
    int const schemedef_max_searchpaths = 5;
    struct schemedef_s {
        char const* name;
        char const* optOverridePath;
        char const* optFallbackPath;
        FS1::Scheme::Flags flags;
        SearchPath::Flags searchPathFlags;
        /// Priority is right to left.
        char const* searchPaths[schemedef_max_searchpaths];
    } defs[] = {
        { "Defs",         NULL,           NULL,           FS1::Scheme::Flag(0), 0,
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
        },
        { 0, 0, 0, FS1::Scheme::Flag(0), 0, { 0 } }
    };

    createPackagesScheme();

    // Setup the rest...
    struct schemedef_s const* def = defs;
    for(int i = 0; defs[i].name; ++i, ++def)
    {
        FS1::Scheme& scheme = App_FileSystem()->createScheme(def->name, def->flags);

        int searchPathCount = 0;
        while(def->searchPaths[searchPathCount] && ++searchPathCount < schemedef_max_searchpaths)
        {}

        for(int j = 0; j < searchPathCount; ++j)
        {
            scheme.addSearchPath(SearchPath(de::Uri(def->searchPaths[j], RC_NULL), def->searchPathFlags));
        }

        if(def->optOverridePath && CommandLine_CheckWith(def->optOverridePath, 1))
        {
            NativePath path = NativePath(CommandLine_NextAsPath());
            scheme.addSearchPath(SearchPath(de::Uri::fromNativeDirPath(path), def->searchPathFlags), FS1::OverridePaths);
            path = path / "$(Game.IdentityKey)";
            scheme.addSearchPath(SearchPath(de::Uri::fromNativeDirPath(path), def->searchPathFlags), FS1::OverridePaths);
        }

        if(def->optFallbackPath && CommandLine_CheckWith(def->optFallbackPath, 1))
        {
            NativePath path = NativePath(CommandLine_NextAsPath());
            scheme.addSearchPath(SearchPath(de::Uri::fromNativeDirPath(path), def->searchPathFlags), FS1::FallbackPaths);
        }
    }
}

void DD_CreateTextureSchemes()
{
    LOG_VERBOSE("Initializing Textures subsystem...");

    Textures &textures = *App_Textures();
    textures.createScheme("System");
    textures.createScheme("Flats");
    textures.createScheme("Textures");
    textures.createScheme("Sprites");
    textures.createScheme("Patches");
    textures.createScheme("Details");
    textures.createScheme("Reflections");
    textures.createScheme("Masks");
    textures.createScheme("ModelSkins");
    textures.createScheme("ModelReflectionSkins");
    textures.createScheme("Lightmaps");
    textures.createScheme("Flaremaps");
}

void DD_ClearRuntimeTextureSchemes()
{
    Textures &textures = *App_Textures();
    if(!textures.count()) return;

    textures.scheme("Flats").clear();
    textures.scheme("Textures").clear();
    textures.scheme("Patches").clear();
    textures.scheme("Sprites").clear();
    textures.scheme("Details").clear();
    textures.scheme("Reflections").clear();
    textures.scheme("Masks").clear();
    textures.scheme("ModelSkins").clear();
    textures.scheme("ModelReflectionSkins").clear();
    textures.scheme("Lightmaps").clear();
    textures.scheme("Flaremaps").clear();

    GL_PruneTextureVariantSpecifications();
}

void DD_ClearSystemTextureSchemes()
{
    Textures &textures = *App_Textures();
    if(!textures.count()) return;

    textures.scheme("System").clear();
    GL_PruneTextureVariantSpecifications();
}

/**
 * Register the engine commands and variables.
 */
void DD_Register(void)
{
    C_CMD("update",          "", CheckForUpdates);
    C_CMD("updateandnotify", "", CheckForUpdatesAndNotify);
    C_CMD("updatesettings",  "", ShowUpdateSettings);
    C_CMD("lastupdated",     "", LastUpdated);

    DD_RegisterLoop();
    DD_RegisterInput();
    F_Register();
    B_Register(); // for control bindings
    Con_Register();
    DH_Register();
    R_Register();
    S_Register();
    SBE_Register(); // for bias editor
    Rend_Register();
    GL_Register();
    Net_Register();
    I_Register();
    H_Register();
    DAM_Register();
    BspBuilder_Register();
    UI_Register();
    Demo_Register();
    P_ControlRegister();
    FI_Register();
}

static void addToPathList(ddstring_t*** list, size_t* listSize, const char* rawPath)
{
    ddstring_t* newPath = Str_New();
    DENG_ASSERT(list && listSize && rawPath && rawPath[0]);

    Str_Set(newPath, rawPath);
    F_FixSlashes(newPath, newPath);
    F_ExpandBasePath(newPath, newPath);

    *list = (ddstring_t**) M_Realloc(*list, sizeof(**list) * ++(*listSize));
    (*list)[(*listSize)-1] = newPath;
}

static void parseStartupFilePathsAndAddFiles(const char* pathString)
{
#define ATWSEPS                 ",; \t"

    char* token, *buffer;
    size_t len;

    if(!pathString || !pathString[0]) return;

    len = strlen(pathString);
    buffer = (char*)malloc(len + 1);
    if(!buffer) Con_Error("parseStartupFilePathsAndAddFiles: Failed on allocation of %lu bytes for parse buffer.", (unsigned long) (len+1));

    strcpy(buffer, pathString);
    token = strtok(buffer, ATWSEPS);
    while(token)
    {
        tryLoadFile(de::Uri(token, RC_NULL));
        token = strtok(NULL, ATWSEPS);
    }
    free(buffer);

#undef ATWSEPS
}

static void destroyPathList(ddstring_t*** list, size_t* listSize)
{
    DENG_ASSERT(list && listSize);
    if(*list)
    {
        size_t i;
        for(i = 0; i < *listSize; ++i)
            Str_Delete((*list)[i]);
        M_Free(*list); *list = 0;
    }
    *listSize = 0;
}

boolean DD_GameLoaded(void)
{
    DENG_ASSERT(games);
    return !isNullGame(games->currentGame());
}

void DD_DestroyGames(void)
{
    destroyPathList(&sessionResourceFileList, &numSessionResourceFileList);

    delete games;
}

/**
 * Begin the Doomsday title animation sequence.
 */
void DD_StartTitle(void)
{
    ddstring_t setupCmds;
    const char* fontName;
    ddfinale_t fin;
    int i;

    if(isDedicated || !Def_Get(DD_DEF_FINALE, "background", &fin)) return;

    Str_Init(&setupCmds);

    // Configure the predefined fonts (all normal, variable width).
    fontName = R_ChooseVariableFont(FS_NORMAL, Window_Width(theWindow),
                                               Window_Height(theWindow));
    for(i = 1; i <= FIPAGE_NUM_PREDEFINED_FONTS; ++i)
    {
        Str_Appendf(&setupCmds, "prefont %i System:%s\n", i, fontName);
    }

    // Configure the predefined colors.
    for(i = 1; i <= MIN_OF(NUM_UI_COLORS, FIPAGE_NUM_PREDEFINED_FONTS); ++i)
    {
        ui_color_t* color = UI_Color(i-1);
        Str_Appendf(&setupCmds, "precolor %i %f %f %f\n", i, color->red, color->green, color->blue);
    }

    titleFinale = FI_Execute2(fin.script, FF_LOCAL, Str_Text(&setupCmds));
    Str_Free(&setupCmds);
}

/**
 * Find all game data file paths in the auto directory with the extensions
 * wad, lmp, pk3, zip and deh.
 *
 * @param found         List of paths to be populated.
 *
 * @return  Number of paths added to @a found.
 */
static int findAllGameDataPaths(FS1::PathList& found)
{
    static char const* extensions[] = {
        "wad", "lmp", "pk3", "zip", "deh",
#ifdef UNIX
        "WAD", "LMP", "PK3", "ZIP", "DEH", // upper case alternatives
#endif
        0
    };
    int const numFoundSoFar = found.count();
    for(uint extIdx = 0; extensions[extIdx]; ++extIdx)
    {
        String pattern = String("$(App.DataPath)/$(GamePlugin.Name)/auto/*.") + extensions[extIdx];
        App_FileSystem()->findAllPaths(de::Uri(pattern, RC_NULL).resolved(), 0, found);
    }
    return found.count() - numFoundSoFar;
}

/**
 * Find and try to load all game data file paths in auto directory.
 *
 * @return Number of new files that were loaded.
 */
static int loadFilesFromDataGameAuto()
{
    FS1::PathList found;
    findAllGameDataPaths(found);

    int numLoaded = 0;
    DENG2_FOR_EACH_CONST(FS1::PathList, i, found)
    {
        // Ignore directories.
        if(i->attrib & A_SUBDIR) continue;

        if(tryLoadFile(de::Uri(i->path, RC_NULL)))
        {
            numLoaded += 1;
        }
    }
    return numLoaded;
}

/**
 * Find and list all game data file paths in the auto directory.
 *
 * @return Number of new files that were added to the list.
 */
static int listFilesFromDataGameAuto(ddstring_t*** list, size_t* listSize)
{
    if(!list || !listSize) return 0;

    FS1::PathList found;
    findAllGameDataPaths(found);

    int numFilesAdded = 0;
    DENG2_FOR_EACH_CONST(FS1::PathList, i, found)
    {
        // Ignore directories.
        if(i->attrib & A_SUBDIR) continue;

        QByteArray foundPath = i->path.toUtf8();
        addToPathList(list, listSize, foundPath.constData());

        numFilesAdded += 1;
    }
    return numFilesAdded;
}

boolean DD_ExchangeGamePluginEntryPoints(pluginid_t pluginId)
{
    if(pluginId != 0)
    {
        // Do the API transfer.
        GETGAMEAPI fptAdr;
        if(!(fptAdr = (GETGAMEAPI) DD_FindEntryPoint(pluginId, "GetGameAPI")))
            return false;
        app.GetGameAPI = fptAdr;
        DD_InitAPI();
        Def_GetGameClasses();
    }
    else
    {
        app.GetGameAPI = 0;
        DD_InitAPI();
        Def_GetGameClasses();
    }
    return true;
}

static void loadResource(Manifest& record)
{
    DENG_ASSERT(record.resourceClass() == RC_PACKAGE);

    de::Uri path = de::Uri(record.resolvedPath(false/*do not locate resource*/), RC_NULL);
    if(path.isEmpty()) return;

    if(de::File1* file = tryLoadFile(path, 0/*base offset*/))
    {
        // Mark this as an original game resource.
        file->setCustom(false);

        // Print the 'CRC' number of IWADs, so they can be identified.
        if(Wad* wad = dynamic_cast<Wad*>(file))
        {
            Con_Message("  IWAD identification: %08x\n", wad->calculateCRC());
        }
    }
}

typedef struct {
    /// @c true iff caller (i.e., DD_ChangeGame) initiated busy mode.
    boolean initiatedBusyMode;
} ddgamechange_paramaters_t;

static int DD_BeginGameChangeWorker(void* parameters)
{
    ddgamechange_paramaters_t* p = (ddgamechange_paramaters_t*)parameters;
    DENG_ASSERT(p);

    P_InitMapUpdate();
    P_InitMapEntityDefs();

    if(p->initiatedBusyMode)
        Con_SetProgress(100);

    DAM_Init();

    if(p->initiatedBusyMode)
    {
        Con_SetProgress(200);
        BusyMode_WorkerEnd();
    }
    return 0;
}

static int DD_LoadGameStartupResourcesWorker(void* parameters)
{
    ddgamechange_paramaters_t* p = (ddgamechange_paramaters_t*)parameters;
    DENG_ASSERT(p);

    // Reset file Ids so previously seen files can be processed again.
    App_FileSystem()->resetFileIds();
    initPathMappings();
    App_FileSystem()->resetAllSchemes();

    if(p->initiatedBusyMode)
        Con_SetProgress(50);

    if(DD_GameLoaded())
    {
        // Create default Auto mappings in the runtime directory.

        // Data class resources.
        App_FileSystem()->addPathMapping("auto/", de::Uri("$(App.DataPath)/$(GamePlugin.Name)/auto/", RC_NULL).resolved());

        // Definition class resources.
        App_FileSystem()->addPathMapping("auto/", de::Uri("$(App.DefsPath)/$(GamePlugin.Name)/auto/", RC_NULL).resolved());
    }

    /**
     * Open all the files, load headers, count lumps, etc, etc...
     * @note  Duplicate processing of the same file is automatically guarded
     *        against by the virtual file system layer.
     */
    Con_Message("Loading game resources%s\n", verbose >= 1? ":" : "...");

    de::Game::Manifests const& gameManifests = games->currentGame().manifests();
    int const numPackages = gameManifests.count(RC_PACKAGE);
    int packageIdx = 0;
    for(de::Game::Manifests::const_iterator i = gameManifests.find(RC_PACKAGE);
        i != gameManifests.end() && i.key() == RC_PACKAGE; ++i, ++packageIdx)
    {
        Manifest& record = **i;
        loadResource(record);

        // Update our progress.
        if(p->initiatedBusyMode)
        {
            Con_SetProgress((packageIdx + 1) * (200 - 50) / numPackages - 1);
        }
    }

    if(p->initiatedBusyMode)
    {
        Con_SetProgress(200);
        BusyMode_WorkerEnd();
    }
    return 0;
}

static int addListFiles(ddstring_t*** list, size_t* listSize, FileType const& ftype)
{
    size_t i;
    int count = 0;
    if(!list || !listSize) return 0;
    for(i = 0; i < *listSize; ++i)
    {
        if(&ftype != &DD_GuessFileTypeFromFileName(Str_Text((*list)[i]))) continue;
        if(tryLoadFile(de::Uri(Str_Text((*list)[i]), RC_NULL)))
        {
            count += 1;
        }
    }
    return count;
}

/**
 * (Re-)Initialize the VFS path mappings.
 */
static void initPathMappings()
{
    App_FileSystem()->clearPathMappings();

    if(DD_IsShuttingDown()) return;

    // Create virtual directory mappings by processing all -vdmap options.
    int argC = CommandLine_Count();
    for(int i = 0; i < argC; ++i)
    {
        if(strnicmp("-vdmap", CommandLine_At(i), 6)) continue;

        if(i < argC - 1 && !CommandLine_IsOption(i + 1) && !CommandLine_IsOption(i + 2))
        {
            String source      = NativePath(CommandLine_PathAt(i + 1)).expand().withSeparators('/');
            String destination = NativePath(CommandLine_PathAt(i + 2)).expand().withSeparators('/');
            App_FileSystem()->addPathMapping(source, destination);
            i += 2;
        }
    }
}

/// Skip all whitespace except newlines.
static inline char const* skipSpace(char const* ptr)
{
    DENG_ASSERT(ptr);
    while(*ptr && *ptr != '\n' && isspace(*ptr))
    { ptr++; }
    return ptr;
}

static bool parsePathLumpMapping(lumpname_t lumpName, ddstring_t* path, char const* buffer)
{
    DENG_ASSERT(lumpName && path);

    // Find the start of the lump name.
    char const* ptr = skipSpace(buffer);

    // Just whitespace?
    if(!*ptr || *ptr == '\n') return false;

    // Find the end of the lump name.
    char const* end = (char const*)M_FindWhite((char*)ptr);
    if(!*end || *end == '\n') return false;

    size_t len = end - ptr;
    // Invalid lump name?
    if(len > 8) return false;

    memset(lumpName, 0, LUMPNAME_T_MAXLEN);
    strncpy(lumpName, ptr, len);
    strupr(lumpName);

    // Find the start of the file path.
    ptr = skipSpace(end);
    if(!*ptr || *ptr == '\n') return false; // Missing file path.

    // We're at the file path.
    Str_Set(path, ptr);
    // Get rid of any extra whitespace on the end.
    Str_StripRight(path);
    F_FixSlashes(path, path);
    return true;
}

/**
 * <pre> LUMPNAM0 \\Path\\In\\The\\Base.ext
 * LUMPNAM1 Path\\In\\The\\RuntimeDir.ext
 *  :</pre>
 */
static bool parsePathLumpMappings(char const* buffer)
{
    DENG_ASSERT(buffer);

    bool successful = false;
    ddstring_t path; Str_Init(&path);
    ddstring_t line; Str_Init(&line);

    char const* ch = buffer;
    lumpname_t lumpName;
    do
    {
        ch = Str_GetLine(&line, ch);
        if(!parsePathLumpMapping(lumpName, &path, Str_Text(&line)))
        {
            // Failure parsing the mapping.
            // Ignore errors in individual mappings and continue parsing.
            //goto parseEnded;
        }
        else
        {
            String destination = NativePath(Str_Text(&path)).expand().withSeparators('/');
            App_FileSystem()->addPathLumpMapping(lumpName, destination);
        }
    } while(*ch);

    // Success.
    successful = true;

//parseEnded:
    Str_Free(&line);
    Str_Free(&path);
    return successful;
}

/**
 * (Re-)Initialize the path => lump mappings.
 * @note Should be called after WADs have been processed.
 */
static void initPathLumpMappings()
{
    // Free old paths, if any.
    App_FileSystem()->clearPathLumpMappings();

    if(DD_IsShuttingDown()) return;

    size_t bufSize = 0;
    uint8_t* buf = NULL;

    // Add the contents of all DD_DIREC lumps.
    DENG2_FOR_EACH_CONST(LumpIndex::Lumps, i, App_FileSystem()->nameIndex().lumps())
    {
        de::File1& lump = **i;
        FileInfo const& lumpInfo = lump.info();

        if(!lump.name().beginsWith("DD_DIREC", Qt::CaseInsensitive)) continue;

        // Make a copy of it so we can ensure it ends in a null.
        if(bufSize < lumpInfo.size + 1)
        {
            bufSize = lumpInfo.size + 1;
            buf = (uint8_t*) M_Realloc(buf, bufSize);
            if(!buf) Con_Error("initPathLumpMappings: Failed on (re)allocation of %lu bytes for temporary read buffer.", (unsigned long) bufSize);
        }

        lump.read(buf, 0, lumpInfo.size);
        buf[lumpInfo.size] = 0;
        parsePathLumpMappings(reinterpret_cast<char const*>(buf));
    }

    if(buf) M_Free(buf);
}

static int DD_LoadAddonResourcesWorker(void* parameters)
{
    ddgamechange_paramaters_t* p = (ddgamechange_paramaters_t*)parameters;
    DENG_ASSERT(p);

    /**
     * Add additional game-startup files.
     * @note These must take precedence over Auto but not game-resource files.
     */
    if(startupFiles && startupFiles[0])
        parseStartupFilePathsAndAddFiles(startupFiles);

    if(p->initiatedBusyMode)
        Con_SetProgress(50);

    if(DD_GameLoaded())
    {
        /**
         * Phase 3: Add real files from the Auto directory.
         * First ZIPs then WADs (they may contain WAD files).
         */
        listFilesFromDataGameAuto(&sessionResourceFileList, &numSessionResourceFileList);
        if(numSessionResourceFileList > 0)
        {
            addListFiles(&sessionResourceFileList, &numSessionResourceFileList, DD_FileTypeByName("FT_ZIP"));

            addListFiles(&sessionResourceFileList, &numSessionResourceFileList, DD_FileTypeByName("FT_WAD"));
        }

        // Final autoload round.
        DD_AutoLoad();
    }

    if(p->initiatedBusyMode)
        Con_SetProgress(180);

    initPathLumpMappings();

    // Re-initialize the resource locator as there are now new resources to be found
    // on existing search paths (probably that is).
    App_FileSystem()->resetAllSchemes();

    if(p->initiatedBusyMode)
    {
        Con_SetProgress(200);
        BusyMode_WorkerEnd();
    }
    return 0;
}

static int DD_ActivateGameWorker(void* parameters)
{
    ddgamechange_paramaters_t* p = (ddgamechange_paramaters_t*)parameters;
    uint i;
    DENG_ASSERT(p);

    // Texture resources are located now, prior to initializing the game.
    R_InitCompositeTextures();
    R_InitFlatTextures();
    R_InitSpriteTextures();

    if(p->initiatedBusyMode)
        Con_SetProgress(50);

    // Now that resources have been located we can begin to initialize the game.
    if(DD_GameLoaded() && gx.PreInit)
    {
        DENG_ASSERT(games->currentGame().pluginId() != 0);

        DD_SetActivePluginId(games->currentGame().pluginId());
        gx.PreInit(games->id(games->currentGame()));
        DD_SetActivePluginId(0);
    }

    if(p->initiatedBusyMode)
        Con_SetProgress(100);

    /**
     * Parse the game's main config file.
     * If a custom top-level config is specified; let it override.
     */
    { ddstring_t const* configFileName = 0;
    ddstring_t tmp;
    if(CommandLine_CheckWith("-config", 1))
    {
        Str_Init(&tmp); Str_Set(&tmp, CommandLine_Next());
        F_FixSlashes(&tmp, &tmp);
        configFileName = &tmp;
    }
    else
    {
        configFileName = &games->currentGame().mainConfig();
    }

    Con_Message("Parsing primary config \"%s\"...\n", F_PrettyPath(Str_Text(configFileName)));
    Con_ParseCommands2(Str_Text(configFileName), CPCF_SET_DEFAULT | CPCF_ALLOW_SAVE_STATE);
    if(configFileName == &tmp)
        Str_Free(&tmp);
    }

    if(!isDedicated && DD_GameLoaded())
    {
        // Apply default control bindings for this game.
        B_BindGameDefaults();

        // Read bindings for this game and merge with the working set.
        Con_ParseCommands2(Str_Text(&games->currentGame().bindingConfig()), CPCF_ALLOW_SAVE_BINDINGS);
    }

    if(p->initiatedBusyMode)
        Con_SetProgress(120);

    Def_Read();

    if(p->initiatedBusyMode)
        Con_SetProgress(130);

    R_InitSprites(); // Fully initialize sprites.
    Models_Init();

    Def_PostInit();

    DD_ReadGameHelp();

    // Re-init to update the title, background etc.
    Rend_ConsoleInit();

    // Reset the tictimer so than any fractional accumulation is not added to
    // the tic/game timer of the newly-loaded game.
    gameTime = 0;
    DD_ResetTimer();

    // Make sure that the next frame does not use a filtered viewer.
    R_ResetViewer();

    // Invalidate old cmds and init player values.
    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        player_t* plr = &ddPlayers[i];

        plr->extraLight = plr->targetExtraLight = 0;
        plr->extraLightCounter = 0;
    }

    if(gx.PostInit)
    {
        DD_SetActivePluginId(games->currentGame().pluginId());
        gx.PostInit();
        DD_SetActivePluginId(0);
    }

    if(p->initiatedBusyMode)
    {
        Con_SetProgress(200);
        BusyMode_WorkerEnd();
    }

    return 0;
}

struct gamecollection_s* App_GameCollection()
{
    return reinterpret_cast<struct gamecollection_s*>(games);
}

struct game_s* App_CurrentGame()
{
    if(!games) return 0;
    return reinterpret_cast<struct game_s*>(&games->currentGame());
}

static void populateGameInfo(GameInfo& info, de::Game& game)
{
    info.identityKey = Str_Text(&game.identityKey());
    info.title       = Str_Text(&game.title());
    info.author      = Str_Text(&game.author());
}

/// @note Part of the Doomsday public API.
boolean DD_GameInfo(GameInfo* info)
{
    if(!info)
    {
#if _DEBUG
        Con_Message("Warning: DD_GameInfo: Received invalid info (=NULL), ignoring.");
#endif
        return false;
    }

    memset(info, 0, sizeof(*info));

    if(DD_GameLoaded())
    {
        populateGameInfo(*info, games->currentGame());
        return true;
    }

#if _DEBUG
    Con_Message("DD_GameInfo: Warning, no game currently loaded - returning false.\n");
#endif
    return false;
}

/// @note Part of the Doomsday public API.
void DD_AddGameResource(gameid_t gameId, resourceclassid_t classId, int rflags,
    char const* names, void* params)
{
    DENG_ASSERT(games);

    if(!VALID_RESOURCECLASSID(classId)) Con_Error("DD_AddGameResource: Unknown resource class %i.", (int)classId);
    if(!names || !names[0]) Con_Error("DD_AddGameResource: Invalid name argument.");

    // Construct and attach the new resource record.
    de::Game& game = games->byId(gameId);
    Manifest* record = new Manifest(classId, rflags);
    game.addManifest(*record);

    // Add the name list to the resource record.
    QStringList nameList = String(names).split(";", QString::SkipEmptyParts);
    DENG2_FOR_EACH_CONST(QStringList, i, nameList)
    {
        record->addName(*i);
    }

    if(params && classId == RC_PACKAGE)
    {
        // Add the identityKey list to the resource record.
        QStringList idKeys = String((char const*) params).split(";", QString::SkipEmptyParts);
        DENG2_FOR_EACH_CONST(QStringList, i, idKeys)
        {
            record->addIdentityKey(*i);
        }
    }
}

/// @note Part of the Doomsday public API.
gameid_t DD_DefineGame(GameDef const* def)
{
    if(!def)
    {
#if _DEBUG
        Con_Message("Warning: DD_DefineGame: Received invalid GameDef (=NULL), ignoring.");
#endif
        return 0; // Invalid id.
    }

    // Game mode identity keys must be unique. Ensure that is the case.
    DENG_ASSERT(games);
    try
    {
        /*de::Game& game =*/ games->byIdentityKey(def->identityKey);
#if _DEBUG
        Con_Message("Warning: DD_DefineGame: Failed adding game \"%s\", identity key '%s' already in use, ignoring.\n", def->defaultTitle, def->identityKey);
#endif
        return 0; // Invalid id.
    }
    catch(const de::GameCollection::NotFoundError&)
    {} // Ignore the error.

    de::Game* game = de::Game::fromDef(*def);
    if(!game) return 0; // Invalid def.

    // Add this game to our records.
    game->setPluginId(DD_ActivePluginId());
    games->add(*game);
    return games->id(*game);
}

/// @note Part of the Doomsday public API.
gameid_t DD_GameIdForKey(const char* identityKey)
{
    DENG_ASSERT(games);
    try
    {
        return games->id(games->byIdentityKey(identityKey));
    }
    catch(const de::GameCollection::NotFoundError&)
    {
        DEBUG_Message(("Warning: DD_GameIdForKey: Game \"%s\" not defined.\n", identityKey));
    }
    return 0; // Invalid id.
}

/**
 * Switch to/activate the specified game.
 */
bool DD_ChangeGame(de::Game& game, bool allowReload = false)
{
    bool isReload = false;

    // Ignore attempts to re-load the current game?
    if(games->isCurrentGame(game))
    {
        if(!allowReload)
        {
            if(DD_GameLoaded())
                Con_Message("%s (%s) - already loaded.\n", Str_Text(&game.title()), Str_Text(&game.identityKey()));
            return true;
        }
        // We are re-loading.
        isReload = true;
    }

    // Quit netGame if one is in progress.
    if(netGame)
        Con_Execute(CMDS_DDAY, isServer ? "net server close" : "net disconnect", true, false);

    S_Reset();
    Demo_StopPlayback();

    GL_PurgeDeferredTasks();
    GL_ResetTextureManager();
    GL_SetFilter(false);

    // If a game is presently loaded; unload it.
    if(DD_GameLoaded())
    {
        if(gx.Shutdown)
            gx.Shutdown();
        Con_SaveDefaults();

        LO_Clear();
        R_DestroyObjlinkBlockmap();
        R_ClearAnimGroups();

        P_ControlShutdown();
        Con_Execute(CMDS_DDAY, "clearbindings", true, false);
        B_BindDefaults();
        B_InitialContextActivations();

        for(uint i = 0; i < DDMAXPLAYERS; ++i)
        {
            player_t* plr = &ddPlayers[i];
            ddplayer_t* ddpl = &plr->shared;

            // Mobjs go down with the map.
            ddpl->mo = 0;
            // States have changed, the states are unknown.
            ddpl->pSprites[0].statePtr = ddpl->pSprites[1].statePtr = 0;

            //ddpl->inGame = false;
            ddpl->flags &= ~DDPF_CAMERA;

            ddpl->fixedColorMap = 0;
            ddpl->extraLight = 0;
        }

        // If a map was loaded; unload it.
        if(theMap)
        {
            GameMap_ClMobjReset(theMap);
        }
        // Clear player data, too, since we just lost all clmobjs.
        Cl_InitPlayers();

        P_SetCurrentMap(0);
        Z_FreeTags(PU_GAMESTATIC, PU_PURGELEVEL - 1);

        P_ShutdownMapEntityDefs();

        R_ShutdownSvgs();
        R_DestroyColorPalettes();

        Fonts_ClearRuntime();
        DD_ClearRuntimeTextureSchemes();

        Sfx_InitLogical();

        /// @todo Why is this being done here?
        if(theMap)
        {
            GameMap_InitThinkerLists(theMap, 0x1|0x2);
        }

        Con_ClearDatabases();

        { // Tell the plugin it is being unloaded.
            void* unloader = DD_FindEntryPoint(games->currentGame().pluginId(), "DP_Unload");
            DEBUG_Message(("DD_ChangeGame: Calling DP_Unload (%p)\n", unloader));
            DD_SetActivePluginId(games->currentGame().pluginId());
            if(unloader) ((pluginfunc_t)unloader)();
            DD_SetActivePluginId(0);
        }

        // The current game is now the special "null-game".
        games->setCurrentGame(games->nullGame());

        Con_InitDatabases();
        DD_Register();
        I_InitVirtualInputDevices();

        R_InitSvgs();
        R_InitViewWindow();

        App_FileSystem()->unloadAllNonStartupFiles();

        // Reset file IDs so previously seen files can be processed again.
        /// @todo this releases the IDs of startup files too but given the
        /// only startup file is doomsday.pk3 which we never attempt to load
        /// again post engine startup, this isn't an immediate problem.
        App_FileSystem()->resetFileIds();

        // Update the dir/WAD translations.
        initPathLumpMappings();
        initPathMappings();

        App_FileSystem()->resetAllSchemes();
    }

    FI_Shutdown();
    titleFinale = 0; // If the title finale was in progress it isn't now.

    /// @todo Materials database should not be shutdown during a reload.
    Materials_Shutdown();

    VERBOSE(
        if(!isNullGame(game))
        {
            Con_Message("Selecting game '%s'...\n", Str_Text(&game.identityKey()));
        }
        else if(!isReload)
        {
            Con_Message("Unloaded game.\n");
        }
    )

    // Remove all entries; some may have been created by the game plugin (if it used libdeng2 C++ API).
    LogBuffer_Clear();

    Library_ReleaseGames();

    char buf[256];
    DD_ComposeMainWindowTitle(buf);
    Window_SetTitle(theWindow, buf);

    if(!DD_IsShuttingDown())
    {
        // Re-initialize subsystems needed even when in ringzero.
        if(!DD_ExchangeGamePluginEntryPoints(game.pluginId()))
        {
            Con_Message("Warning: DD_ChangeGame: Failed exchanging entrypoints with plugin %i, aborting.\n", (int)game.pluginId());
            return false;
        }

        Materials_Init();
        FI_Init();
    }

    // This is now the current game.
    games->setCurrentGame(game);

    DD_ComposeMainWindowTitle(buf);
    Window_SetTitle(theWindow, buf);

    /**
     * If we aren't shutting down then we are either loading a game or switching
     * to ringzero (the current game will have already been unloaded).
     */
    if(!DD_IsShuttingDown())
    {
        /**
         * The bulk of this we can do in busy mode unless we are already busy
         * (which can happen if a fatal error occurs during game load and we must
         * shutdown immediately; Sys_Shutdown will call back to load the special
         * "null-game" game).
         */
        const int busyMode = BUSYF_PROGRESS_BAR | (verbose? BUSYF_CONSOLE_OUTPUT : 0);
        ddgamechange_paramaters_t p;
        BusyTask gameChangeTasks[] = {
            // Phase 1: Initialization.
            { DD_BeginGameChangeWorker,          &p, busyMode, "Loading game...",   200, 0.0f, 0.1f, 0 },

            // Phase 2: Loading "startup" resources.
            { DD_LoadGameStartupResourcesWorker, &p, busyMode, NULL,                200, 0.1f, 0.3f, 0 },

            // Phase 3: Loading "addon" resources.
            { DD_LoadAddonResourcesWorker,       &p, busyMode, "Loading addons...", 200, 0.3f, 0.7f, 0 },

            // Phase 4: Game activation.
            { DD_ActivateGameWorker,             &p, busyMode, "Starting game...",  200, 0.7f, 1.0f, 0 }
        };

        p.initiatedBusyMode = !BusyMode_Active();

        if(DD_GameLoaded())
        {
            // Tell the plugin it is being loaded.
            /// @todo Must this be done in the main thread?
            void* loader = DD_FindEntryPoint(games->currentGame().pluginId(), "DP_Load");
            DEBUG_Message(("DD_ChangeGame: Calling DP_Load (%p)\n", loader));
            DD_SetActivePluginId(games->currentGame().pluginId());
            if(loader) ((pluginfunc_t)loader)();
            DD_SetActivePluginId(0);
        }

        /// @todo Kludge: Use more appropriate task names when unloading a game.
        if(isNullGame(game))
        {
            gameChangeTasks[0].name = "Unloading game...";
            gameChangeTasks[3].name = "Switching to ringzero...";
        }
        // kludge end

        BusyMode_RunTasks(gameChangeTasks, sizeof(gameChangeTasks)/sizeof(gameChangeTasks[0]));

        // Process any GL-related tasks we couldn't while Busy.
        Rend_ParticleLoadExtraTextures();

        if(DD_GameLoaded())
        {
            de::Game::printBanner(games->currentGame());
        }
        else
        {
            // Lets play a nice title animation.
            DD_StartTitle();
        }
    }

    DENG_ASSERT(DD_ActivePluginId() == 0);

    /**
     * Clear any input events we may have accumulated during this process.
     * @note Only necessary here because we might not have been able to use
     *       busy mode (which would normally do this for us on end).
     */
    DD_ClearEvents();
    return true;
}

boolean DD_IsShuttingDown(void)
{
    return Sys_IsShuttingDown();
}

/**
 * Looks for new files to autoload from the auto-load data directory.
 */
static void DD_AutoLoad(void)
{
    /**
     * Keep loading files if any are found because virtual files may now
     * exist in the auto-load directory.
     */
    int numNewFiles;
    while((numNewFiles = loadFilesFromDataGameAuto()) > 0)
    {
        VERBOSE( Con_Message("Autoload round completed with %i new files.\n", numNewFiles) );
    }
}

/**
 * Attempt to determine which game is to be played.
 *
 * @todo Logic here could be much more elaborate but is it necessary?
 */
de::Game* DD_AutoselectGame(void)
{
    if(CommandLine_CheckWith("-game", 1))
    {
        char const* identityKey = CommandLine_Next();
        try
        {
            de::Game& game = games->byIdentityKey(identityKey);
            if(game.allStartupFilesFound())
            {
                return &game;
            }
        }
        catch(const de::GameCollection::NotFoundError&)
        {} // Ignore the error.
    }

    // If but one lonely game; select it.
    if(games->numPlayable() == 1)
    {
        return games->firstPlayable();
    }

    // We don't know what to do.
    return NULL;
}

int DD_EarlyInit(void)
{
    // Determine the requested degree of verbosity.
    verbose = CommandLine_Exists("-verbose");

    // Bring the console online as soon as we can.
    DD_ConsoleInit();

    Con_InitDatabases();

    // Register the engine's console commands and variables.
    DD_Register();

    // Bring the window manager online.
    Sys_InitWindowManager();

    // Instantiate the Games collection.
    games = new de::GameCollection();

    return true;
}

/**
 * This gets called when the main window is ready for GL init. The application
 * event loop is already running.
 */
void DD_FinishInitializationAfterWindowReady(void)
{    
#ifdef WIN32
    // Now we can get the color transfer table as the window is available.
    DisplayMode_SaveOriginalColorTransfer();
#endif

    if(!Sys_GLInitialize())
    {
        Con_Error("Error initializing OpenGL.\n");
    }
    else
    {
        char buf[256];
        DD_ComposeMainWindowTitle(buf);
        Window_SetTitle(theWindow, buf);
    }

    // Now we can start executing the engine's main loop.
    LegacyCore_SetLoopFunc(DD_GameLoopCallback);

    // Initialize engine subsystems and initial state.
    if(!DD_Init())
    {
        exit(2); // Cannot continue...
        return;
    }

    // Start drawing with the game loop drawer.
    Window_SetDrawFunc(Window_Main(), DD_GameLoopDrawer);
}

/**
 * Engine initialization. After completed, the game loop is ready to be started.
 * Called from the app entrypoint function.
 *
 * @return  @c true on success, @c false if an error occurred.
 */
boolean DD_Init(void)
{
    // By default, use the resolution defined in (default).cfg.
    //int winWidth = defResX, winHeight = defResY, winBPP = defBPP, winX = 0, winY = 0;
    //uint winFlags = DDWF_VISIBLE | DDWF_CENTER | (defFullscreen? DDWF_FULLSCREEN : 0);
    //boolean noCenter = false;
    //int i; //, exitCode = 0;

#ifdef _DEBUG
    // Type size check.
    {
        void* ptr = 0;
        int32_t int32 = 0;
        int16_t int16 = 0;
        float float32 = 0;

        DENG_UNUSED(ptr);
        DENG_UNUSED(int32);
        DENG_UNUSED(int16);
        DENG_UNUSED(float32);

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

    if(!GL_EarlyInit())
    {
        Sys_CriticalMessage("GL_EarlyInit() failed.");
        return false;
    }

    /*
    DENG_ASSERT(!Sys_GLCheckError());
    if(!novideo)
    {
        // Render a few black frames before we continue. This will help to
        // stabilize things before we begin drawing for real and to avoid any
        // unwanted video artefacts.
        i = 0;
        while(i++ < 3)
        {
            glClear(GL_COLOR_BUFFER_BIT);
            GL_DoUpdate();
        }
    }
    DENG_ASSERT(!Sys_GLCheckError());
*/

    // Initialize the subsystems needed prior to entering busy mode for the first time.
    Sys_Init();
    DD_CreateResourceClasses();
    DD_CreateFileTypes();
    F_Init();
    DD_CreateFileSystemSchemes();

    Fonts_Init();
    FR_Init();
    DD_InitInput();

    // Enter busy mode until startup complete.
    Con_InitProgress2(200, 0, .25f); // First half.
    BusyMode_RunNewTaskWithName(BUSYF_NO_UPLOADS | BUSYF_STARTUP | BUSYF_PROGRESS_BAR | (verbose? BUSYF_CONSOLE_OUTPUT : 0),
                                DD_StartupWorker, 0, "Starting up...");

    // Engine initialization is complete. Now finish up with the GL.
    GL_Init();
    GL_InitRefresh();

    // Do deferred uploads.
    Con_InitProgress2(200, .25f, .25f); // Stop here for a while.
    BusyMode_RunNewTaskWithName(BUSYF_STARTUP | BUSYF_PROGRESS_BAR | (verbose? BUSYF_CONSOLE_OUTPUT : 0),
                                DD_DummyWorker, 0, "Buffering...");

    // Add resource paths specified using -iwad on the command line.
    FS1::Scheme& scheme = App_FileSystem()->scheme(DD_ResourceClassByName("RC_PACKAGE").defaultScheme());
    for(int p = 0; p < CommandLine_Count(); ++p)
    {
        if(!CommandLine_IsMatchingAlias("-iwad", CommandLine_At(p)))
            continue;

        while(++p != CommandLine_Count() && !CommandLine_IsOption(p))
        {
            /// @todo Do not add these as search paths, publish them directly to
            ///       the "Packages" scheme.

            // CommandLine_PathAt() always returns an absolute path.
            directory_t* dir = Dir_FromText(CommandLine_PathAt(p));
            de::Uri uri = de::Uri::fromNativeDirPath(Dir_Path(dir), RC_PACKAGE);

            scheme.addSearchPath(SearchPath(uri, SearchPath::NoDescend));

            Dir_Delete(dir);
        }

        p--;/* For ArgIsOption(p) necessary, for p==Argc() harmless */
    }

    // Try to locate all required data files for all registered games.
    Con_InitProgress2(200, .25f, 1); // Second half.
    games->locateAllResources();

    /*
    // Unless we reenter busy-mode due to automatic game selection, we won't be
    // drawing anything further until DD_GameLoop; so lets clean up.
    if(!novideo)
    {
        glClear(GL_COLOR_BUFFER_BIT);
        GL_DoUpdate();
    }
    */

    // Attempt automatic game selection.
    if(!CommandLine_Exists("-noautoselect"))
    {
        de::Game* game = DD_AutoselectGame();

        if(game)
        {
            // An implicit game session has been defined.
            int p;

            // Add all resources specified using -file options on the command line
            // to the list for this session.
            for(p = 0; p < CommandLine_Count(); ++p)
            {
                if(!CommandLine_IsMatchingAlias("-file", CommandLine_At(p))) continue;

                while(++p != CommandLine_Count() && !CommandLine_IsOption(p))
                {
                    addToPathList(&sessionResourceFileList, &numSessionResourceFileList, CommandLine_PathAt(p));
                }

                p--;/* For ArgIsOption(p) necessary, for p==Argc() harmless */
            }

            // Begin the game session.
            DD_ChangeGame(*game);

            // We do not want to load these resources again on next game change.
            destroyPathList(&sessionResourceFileList, &numSessionResourceFileList);
        }
    }

    initPathLumpMappings();

    // Re-initialize the filesystem subspace schemess as there are now new
    // resources to be found on existing search paths (probably that is).
    App_FileSystem()->resetAllSchemes();

    // One-time execution of various command line features available during startup.
    if(CommandLine_CheckWith("-dumplump", 1))
    {
        String name = CommandLine_Next();
        lumpnum_t lumpNum = App_FileSystem()->lumpNumForName(name);
        if(lumpNum >= 0)
        {
            F_DumpLump(lumpNum);
        }
        else
        {
            Con_Message("Warning: Cannot dump unknown lump \"%s\", ignoring.\n", name.toAscii().constData());
        }
    }

    if(CommandLine_Check("-dumpwaddir"))
    {
        Con_Executef(CMDS_CMDLINE, false, "listlumps");
    }

    // Try to load the autoexec file. This is done here to make sure everything is
    // initialized: the user can do here anything that s/he'd be able to do in-game
    // provided a game was loaded during startup.
    Con_ParseCommands("autoexec.cfg");

    // Read additional config files that should be processed post engine init.
    if(CommandLine_CheckWith("-parse", 1))
    {
        uint startTime;
        Con_Message("Parsing additional (pre-init) config files:\n");
        startTime = Timer_RealMilliseconds();
        for(;;)
        {
            const char* arg = CommandLine_Next();
            if(!arg || arg[0] == '-') break;

            Con_Message("  Processing \"%s\"...\n", F_PrettyPath(arg));
            Con_ParseCommands(arg);
        }
        VERBOSE( Con_Message("  Done in %.2f seconds.\n", (Timer_RealMilliseconds() - startTime) / 1000.0f) );
    }

    // A console command on the command line?
    { int p;
    for(p = 1; p < CommandLine_Count() - 1; p++)
    {
        if(stricmp(CommandLine_At(p), "-command") && stricmp(CommandLine_At(p), "-cmd"))
            continue;

        for(++p; p < CommandLine_Count(); p++)
        {
            const char* arg = CommandLine_At(p);

            if(arg[0] == '-')
            {
                p--;
                break;
            }
            Con_Execute(CMDS_CMDLINE, arg, false, false);
        }
    }}

    /**
     * One-time execution of network commands on the command line.
     * Commands are only executed if we have loaded a game during startup.
     */
    if(DD_GameLoaded())
    {
        // Client connection command.
        if(CommandLine_CheckWith("-connect", 1))
            Con_Executef(CMDS_CMDLINE, false, "connect %s", CommandLine_Next());

        // Incoming TCP port.
        if(CommandLine_CheckWith("-port", 1))
            Con_Executef(CMDS_CMDLINE, false, "net-ip-port %s", CommandLine_Next());

        // Server start command.
        // (shortcut for -command "net init tcpip; net server start").
        if(CommandLine_Exists("-server"))
        {
            if(!N_InitService(true))
                Con_Message("Can't start server: network init failed.\n");
            else
                Con_Executef(CMDS_CMDLINE, false, "net server start");
        }
    }
    else
    {
        // No game loaded.
        // Lets get most of everything else initialized.
        // Reset file IDs so previously seen files can be processed again.
        App_FileSystem()->resetFileIds();
        initPathLumpMappings();
        initPathMappings();
        App_FileSystem()->resetAllSchemes();

        R_InitCompositeTextures();
        R_InitFlatTextures();
        R_InitSpriteTextures();

        Def_Read();

        R_InitSprites();
        Models_Init();

        Def_PostInit();

        // Lets play a nice title animation.
        DD_StartTitle();

        // We'll open the console and print a list of the known games too.
        Con_Execute(CMDS_DDAY, "conopen", true, false);
        if(!CommandLine_Exists("-noautoselect"))
            Con_Message("Automatic game selection failed.\n");
        Con_Execute(CMDS_DDAY, "listgames", false, false);
    }

    return true;
}

static void DD_InitResourceSystem(void)
{
    Con_Message("Initializing Resource subsystem...\n");

    initPathMappings();

    App_FileSystem()->resetAllSchemes();

    // Initialize the definition databases.
    Def_Init();
}

static int DD_StartupWorker(void* parm)
{
    DENG_UNUSED(parm);

#ifdef WIN32
    // Initialize COM for this thread (needed for DirectInput).
    CoInitialize(NULL);
#endif

    Con_SetProgress(10);

    // Any startup hooks?
    DD_CallHooks(HOOK_STARTUP, 0, 0);

    Con_SetProgress(20);

    // Was the change to userdir OK?
    if(CommandLine_CheckWith("-userdir", 1) && !app.usingUserDir)
        Con_Message("--(!)-- User directory not found (check -userdir).\n");

    bamsInit(); // Binary angle calculations.

    DD_InitResourceSystem();

    Con_SetProgress(40);

    Net_Init();
    // Now we can hide the mouse cursor for good.
    Sys_HideMouse();

    // Read config files that should be read BEFORE engine init.
    if(CommandLine_CheckWith("-cparse", 1))
    {
        uint startTime;
        Con_Message("Parsing additional (pre-init) config files:\n");
        startTime = Timer_RealMilliseconds();
        for(;;)
        {
            const char* arg = CommandLine_NextAsPath();
            if(!arg || arg[0] == '-')
                break;
            Con_Message("  Processing \"%s\"...\n", F_PrettyPath(arg));
            Con_ParseCommands(arg);
        }
        VERBOSE( Con_Message("  Done in %.2f seconds.\n", (Timer_RealMilliseconds() - startTime) / 1000.0f) );
    }

    // Add required engine resource files.
    de::Uri searchPath("Packages:doomsday.pk3");
    AutoStr* foundPath = AutoStr_NewStd();
    if(!F_FindPath(RC_PACKAGE, reinterpret_cast<uri_s*>(&searchPath), foundPath) ||
       !tryLoadFile(de::Uri(Str_Text(foundPath), RC_NULL)))
    {
        Con_Error("DD_StartupWorker: Failed to locate required resource \"doomsday.pk3\".");
    }

    // No more lumps/packages will be loaded in startup mode after this point.
    F_EndStartup();

    // Load engine help resources.
    DD_InitHelp();

    Con_SetProgress(60);

    // Execute the startup script (Startup.cfg).
    Con_ParseCommands("startup.cfg");

    Con_SetProgress(90);
    GL_EarlyInitTextureManager();

    Textures_Init();
    DD_CreateTextureSchemes();

    // Get the material manager up and running.
    Materials_Init();

    Con_SetProgress(140);
    Con_Message("Initializing Binding subsystem...\n");
    B_Init();

    Con_SetProgress(150);
    R_Init();

    Con_SetProgress(165);
    Net_InitGame();
    Demo_Init();

    Con_Message("Initializing InFine subsystem...\n");
    FI_Init();

    Con_Message("Initializing UI subsystem...\n");
    UI_Init();

    Con_SetProgress(190);

    // In dedicated mode the console must be opened, so all input events
    // will be handled by it.
    if(isDedicated)
    {
        Con_Open(true);

        // Also make sure the game loop isn't running needlessly often.
        LegacyCore_SetLoopRate(35);
    }

    Con_SetProgress(199);

    DD_CallHooks(HOOK_INIT, 0, 0); // Any initialization hooks?

    Con_SetProgress(200);

#ifdef WIN32
    // This thread has finished using COM.
    CoUninitialize();
#endif

    BusyMode_WorkerEnd();
    return 0;
}

/**
 * This only exists so we have something to call while the deferred uploads of the
 * startup are processed.
 */
static int DD_DummyWorker(void *parm)
{
    DENG_UNUSED(parm);
    Con_SetProgress(200);
    BusyMode_WorkerEnd();
    return 0;
}

void DD_CheckTimeDemo(void)
{
    static boolean checked = false;

    if(!checked)
    {
        checked = true;
        if(CommandLine_CheckWith("-timedemo", 1) || // Timedemo mode.
           CommandLine_CheckWith("-playdemo", 1)) // Play-once mode.
        {
            char            buf[200];

            sprintf(buf, "playdemo %s", CommandLine_Next());
            Con_Execute(CMDS_CMDLINE, buf, false, false);
        }
    }
}

typedef struct {
    /// @c true iff caller (i.e., DD_UpdateEngineState) initiated busy mode.
    boolean initiatedBusyMode;
} ddupdateenginestateworker_paramaters_t;

static int DD_UpdateEngineStateWorker(void* parameters)
{
    DENG_ASSERT(parameters);
    ddupdateenginestateworker_paramaters_t* p = (ddupdateenginestateworker_paramaters_t*) parameters;

    if(!novideo)
    {
        GL_InitRefresh();
    }

    if(p->initiatedBusyMode)
        Con_SetProgress(50);

    R_Update();

    if(p->initiatedBusyMode)
    {
        Con_SetProgress(200);
        BusyMode_WorkerEnd();
    }
    return 0;
}

void DD_UpdateEngineState(void)
{
    boolean hadFog;

    Con_Message("Updating engine state...\n");

    // Stop playing sounds and music.
    GL_SetFilter(false);
    Demo_StopPlayback();
    S_Reset();

    //App_FileSystem()->resetFileIds();

    // Update the dir/WAD translations.
    initPathLumpMappings();
    initPathMappings();

    // Re-build the filesystem subspace schemes as there may be new resources to be found.
    App_FileSystem()->resetAllSchemes();

    R_InitCompositeTextures();
    R_InitFlatTextures();
    R_InitSpriteTextures();

    if(DD_GameLoaded() && gx.UpdateState)
        gx.UpdateState(DD_PRE);

    hadFog = usingFog;
    GL_TotalReset();
    GL_TotalRestore(); // Bring GL back online.

    // Make sure the fog is enabled, if necessary.
    if(hadFog)
        GL_UseFog(true);

    /**
     * The bulk of this we can do in busy mode unless we are already busy
     * (which can happen during a runtime game change).
     */
    { ddupdateenginestateworker_paramaters_t p;
    p.initiatedBusyMode = !BusyMode_Active();
    if(p.initiatedBusyMode)
    {
        Con_InitProgress(200);
        BusyMode_RunNewTaskWithName(BUSYF_ACTIVITY | BUSYF_PROGRESS_BAR | (verbose? BUSYF_CONSOLE_OUTPUT : 0),
                                    DD_UpdateEngineStateWorker, &p, "Updating engine state...");
    }
    else
    {
        /// @todo Update the current task name and push progress.
        DD_UpdateEngineStateWorker(&p);
    }
    }

    if(DD_GameLoaded() && gx.UpdateState)
        gx.UpdateState(DD_POST);

    // Reset the anim groups (if in-game)
    Materials_ResetAnimGroups();
}

/* *INDENT-OFF* */
ddvalue_t ddValues[DD_LAST_VALUE - DD_FIRST_VALUE - 1] = {
    {&netGame, 0},
    {&isServer, 0},                         // An *open* server?
    {&isClient, 0},
    {&allowFrames, &allowFrames},
    {&consolePlayer, &consolePlayer},
    {&displayPlayer, 0 /*&displayPlayer*/}, // use R_SetViewPortPlayer() instead
    {&mipmapping, 0},
    {&filterUI, 0},
    {&defResX, &defResX},
    {&defResY, &defResY},
    {0, 0},
    {0, 0}, //{&mouseInverseY, &mouseInverseY},
    {&levelFullBright, &levelFullBright},
    {&CmdReturnValue, 0},
    {&gameReady, &gameReady},
    {&isDedicated, 0},
    {&novideo, 0},
    {&defs.count.mobjs.num, 0},
    {&gotFrame, 0},
    {&playback, 0},
    {&defs.count.sounds.num, 0},
    {&defs.count.music.num, 0},
    {0, 0},
    {&clientPaused, &clientPaused},
    {&weaponOffsetScaleY, &weaponOffsetScaleY},
    {&monochrome, &monochrome},
    {&gameDataFormat, &gameDataFormat},
    {&gameDrawHUD, 0},
    {&upscaleAndSharpenPatches, &upscaleAndSharpenPatches},
    {&symbolicEchoMode, &symbolicEchoMode},
    {&numTexUnits, 0}
};
/* *INDENT-ON* */

/**
 * Get a 32-bit signed integer value.
 */
int DD_GetInteger(int ddvalue)
{
    switch(ddvalue)
    {
    case DD_SHIFT_DOWN:
        return I_ShiftDown();

    case DD_WINDOW_WIDTH:
        return Window_Width(theWindow);

    case DD_WINDOW_HEIGHT:
        return Window_Height(theWindow);

    case DD_DYNLIGHT_TEXTURE:
        return (int) GL_PrepareLSTexture(LST_DYNAMIC);

    case DD_NUMLUMPS:
        return F_LumpCount();

    case DD_CURRENT_CLIENT_FINALE_ID:
        return Cl_CurrentFinale();

    case DD_MAP_MUSIC: {
        GameMap* map = theMap;
        if(map)
        {
            ded_mapinfo_t* mapInfo = Def_GetMapInfo(GameMap_Uri(map));
            if(mapInfo)
            {
                return Def_GetMusicNum(mapInfo->music);
            }
        }
        return -1;
      }
    default: break;
    }

    if(ddvalue >= DD_LAST_VALUE || ddvalue <= DD_FIRST_VALUE)
        return 0;
    if(ddValues[ddvalue].readPtr == 0)
        return 0;
    return *ddValues[ddvalue].readPtr;
}

/**
 * Set a 32-bit signed integer value.
 */
void DD_SetInteger(int ddvalue, int parm)
{
    if(ddvalue <= DD_FIRST_VALUE || ddvalue >= DD_LAST_VALUE)
        return;
    if(ddValues[ddvalue].writePtr)
        *ddValues[ddvalue].writePtr = parm;
}

/**
 * Get a pointer to the value of a variable. Not all variables support
 * this. Added for 64-bit support.
 */
void* DD_GetVariable(int ddvalue)
{
    static uint valueU;
    static float valueF;
    static double valueD;

    switch(ddvalue)
    {
    case DD_GAME_EXPORTS:
        return &gx;

    case DD_SECTOR_COUNT:
        valueU = theMap? GameMap_SectorCount(theMap) : 0;
        return &valueU;

    case DD_LINE_COUNT:
        valueU = theMap? GameMap_LineDefCount(theMap) : 0;
        return &valueU;

    case DD_SIDE_COUNT:
        valueU = theMap? GameMap_SideDefCount(theMap) : 0;
        return &valueU;

    case DD_VERTEX_COUNT:
        valueU = theMap? GameMap_VertexCount(theMap) : 0;
        return &valueU;

    case DD_POLYOBJ_COUNT:
        valueU = theMap? GameMap_PolyobjCount(theMap) : 0;
        return &valueU;

    case DD_HEDGE_COUNT:
        valueU = theMap? GameMap_HEdgeCount(theMap) : 0;
        return &valueU;

    case DD_BSPLEAF_COUNT:
        valueU = theMap? GameMap_BspLeafCount(theMap) : 0;
        return &valueU;

    case DD_BSPNODE_COUNT:
        valueU = theMap? GameMap_BspNodeCount(theMap) : 0;
        return &valueU;

    case DD_TRACE_ADDRESS:
        /// @todo Do not cast away const.
        return (void*)P_TraceLOS();

    case DD_TRANSLATIONTABLES_ADDRESS:
        return translationTables;

    case DD_MAP_NAME:
        if(theMap)
        {
            ded_mapinfo_t* mapInfo = Def_GetMapInfo(GameMap_Uri(theMap));
            if(mapInfo && mapInfo->name[0])
            {
                int id = Def_Get(DD_DEF_TEXT, mapInfo->name, NULL);
                if(id != -1)
                {
                    return defs.text[id].text;
                }
                return mapInfo->name;
            }
        }
        return NULL;

    case DD_MAP_AUTHOR:
        if(theMap)
        {
            ded_mapinfo_t* mapInfo = Def_GetMapInfo(GameMap_Uri(theMap));
            if(mapInfo && mapInfo->author[0])
            {
                return mapInfo->author;
            }
        }
        return NULL;

    case DD_MAP_MIN_X:
        if(theMap)
        {
            return &theMap->aaBox.minX;
        }
        return NULL;

    case DD_MAP_MIN_Y:
        if(theMap)
        {
            return &theMap->aaBox.minY;
        }
        return NULL;

    case DD_MAP_MAX_X:
        if(theMap)
        {
            return &theMap->aaBox.maxX;
        }
        return NULL;

    case DD_MAP_MAX_Y:
        if(theMap)
        {
            return &theMap->aaBox.maxY;
        }
        return NULL;

    case DD_PSPRITE_OFFSET_X:
        return &pspOffset[VX];

    case DD_PSPRITE_OFFSET_Y:
        return &pspOffset[VY];

    case DD_PSPRITE_LIGHTLEVEL_MULTIPLIER:
        return &pspLightLevelMultiplier;

    /*case DD_CPLAYER_THRUST_MUL:
        return &cplrThrustMul;*/

    case DD_GRAVITY:
        valueD = theMap? GameMap_Gravity(theMap) : 0;
        return &valueD;

    case DD_TORCH_RED:
        return &torchColor[CR];

    case DD_TORCH_GREEN:
        return &torchColor[CG];

    case DD_TORCH_BLUE:
        return &torchColor[CB];

    case DD_TORCH_ADDITIVE:
        return &torchAdditive;

#ifdef WIN32
    case DD_WINDOW_HANDLE:
        return Window_NativeHandle(Window_Main());
#endif

    // We have to separately calculate the 35 Hz ticks.
    case DD_GAMETIC: {
        static timespan_t       fracTic;
        fracTic = gameTime * TICSPERSEC;
        return &fracTic; }

    case DD_OPENRANGE: {
        const TraceOpening* open = P_TraceOpening();
        valueF = open->range;
        return &valueF; }

    case DD_OPENTOP: {
        const TraceOpening* open = P_TraceOpening();
        valueF = open->top;
        return &valueF; }

    case DD_OPENBOTTOM: {
        const TraceOpening* open = P_TraceOpening();
        valueF = open->bottom;
        return &valueF; }

    case DD_LOWFLOOR: {
        const TraceOpening* open = P_TraceOpening();
        valueF = open->lowFloor;
        return &valueF; }

    case DD_NUMLUMPS: {
        static int count;
        count = F_LumpCount();
        return &count; }

    default: break;
    }

    if(ddvalue >= DD_LAST_VALUE || ddvalue <= DD_FIRST_VALUE)
        return 0;

    // Other values not supported.
    return ddValues[ddvalue].writePtr;
}

/**
 * Set the value of a variable. The pointer can point to any data, its
 * interpretation depends on the variable. Added for 64-bit support.
 */
void DD_SetVariable(int ddvalue, void *parm)
{
    if(ddvalue <= DD_FIRST_VALUE || ddvalue >= DD_LAST_VALUE)
    {
        switch(ddvalue)
        {
        /*case DD_CPLAYER_THRUST_MUL:
            cplrThrustMul = *(float*) parm;
            return;*/

        case DD_GRAVITY:
            if(theMap) GameMap_SetGravity(theMap, *(coord_t*) parm);
            return;

        case DD_PSPRITE_OFFSET_X:
            pspOffset[VX] = *(float*) parm;
            return;

        case DD_PSPRITE_OFFSET_Y:
            pspOffset[VY] = *(float*) parm;
            return;

        case DD_PSPRITE_LIGHTLEVEL_MULTIPLIER:
            pspLightLevelMultiplier = *(float*) parm;
            return;

        case DD_TORCH_RED:
            torchColor[CR] = MINMAX_OF(0, *((float*) parm), 1);
            return;

        case DD_TORCH_GREEN:
            torchColor[CG] = MINMAX_OF(0, *((float*) parm), 1);
            return;

        case DD_TORCH_BLUE:
            torchColor[CB] = MINMAX_OF(0, *((float*) parm), 1);
            return;

        case DD_TORCH_ADDITIVE:
            torchAdditive = (*(int*) parm)? true : false;
            break;

        default:
            break;
        }
    }
}

/// @note Part of the Doomsday public API.
materialschemeid_t DD_ParseMaterialSchemeName(char const *str)
{
    return Materials_ParseSchemeName(str);
}

/// @note Part of the Doomsday public API.
fontschemeid_t DD_ParseFontSchemeName(const char* str)
{
    return Fonts_ParseScheme(str);
}

ddstring_t const *DD_MaterialSchemeNameForTextureScheme(String textureSchemeName)
{
    materialschemeid_t schemeId = MS_INVALID; // Unknown.

    if(!textureSchemeName.compareWithoutCase("Textures"))
    {
        schemeId = MS_TEXTURES;
    }
    else if(!textureSchemeName.compareWithoutCase("Flats"))
    {
        schemeId = MS_FLATS;
    }
    else if(!textureSchemeName.compareWithoutCase("Sprites"))
    {
        schemeId = MS_SPRITES;
    }
    else if(!textureSchemeName.compareWithoutCase("Patches"))
    {
        schemeId = MS_ANY; // No materials for these yet.
    }
    else if(!textureSchemeName.compareWithoutCase("System"))
    {
        schemeId = MS_SYSTEM;
    }

    return Materials_SchemeName(schemeId);
}

ddstring_t const *DD_MaterialSchemeNameForTextureScheme(ddstring_t const *textureSchemeName)
{
    if(!textureSchemeName) return Materials_SchemeName(MS_INVALID);
    return DD_MaterialSchemeNameForTextureScheme(Str_Text(textureSchemeName));
}

/// @todo Replace with a version accepting a URN -ds
materialid_t DD_MaterialForTextureUniqueId(char const *schemeName, int uniqueId)
{
    try
    {
        de::Uri uri = App_Textures()->scheme(schemeName).findByUniqueId(uniqueId).composeUri();
        uri.setScheme(Str_Text(DD_MaterialSchemeNameForTextureScheme(uri.scheme())));
        return Materials_ResolveUri2(reinterpret_cast<uri_s*>(&uri), true/*quiet please*/);
    }
    catch(Textures::UnknownSchemeError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_WARNING(er.asText() + ", ignoring.");
    }
    catch(Textures::Scheme::NotFoundError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_WARNING(er.asText() + ", ignoring.");
    }
    return NOMATERIALID;
}

/**
 * Gets the data of a player.
 */
ddplayer_t* DD_GetPlayer(int number)
{
    return (ddplayer_t *) &ddPlayers[number].shared;
}

/**
 * Convert propertyType enum constant into a string for error/debug messages.
 */
const char* value_Str(int val)
{
    static char valStr[40];
    struct val_s {
        int                 val;
        const char*         str;
    } valuetypes[] = {
        { DDVT_BOOL, "DDVT_BOOL" },
        { DDVT_BYTE, "DDVT_BYTE" },
        { DDVT_SHORT, "DDVT_SHORT" },
        { DDVT_INT, "DDVT_INT" },
        { DDVT_UINT, "DDVT_UINT" },
        { DDVT_FIXED, "DDVT_FIXED" },
        { DDVT_ANGLE, "DDVT_ANGLE" },
        { DDVT_FLOAT, "DDVT_FLOAT" },
        { DDVT_DOUBLE, "DDVT_DOUBLE" },
        { DDVT_LONG, "DDVT_LONG" },
        { DDVT_ULONG, "DDVT_ULONG" },
        { DDVT_PTR, "DDVT_PTR" },
        { DDVT_BLENDMODE, "DDVT_BLENDMODE" },
        { 0, NULL }
    };
    uint i;

    for(i = 0; valuetypes[i].str; ++i)
        if(valuetypes[i].val == val)
            return valuetypes[i].str;

    sprintf(valStr, "(unnamed %i)", val);
    return valStr;
}

D_CMD(Load)
{
    DENG_UNUSED(src);

    boolean didLoadGame = false, didLoadResource = false;
    ddstring_t searchPath;
    int arg = 1;

    Str_Init(&searchPath);
    Str_Set(&searchPath, argv[arg]);
    Str_Strip(&searchPath);
    if(Str_IsEmpty(&searchPath))
    {
        Str_Free(&searchPath);
        return false;
    }
    F_FixSlashes(&searchPath, &searchPath);

    // Ignore attempts to load directories.
    if(Str_RAt(&searchPath, 0) == '/')
    {
        Con_Message("Directories cannot be \"loaded\" (only files and/or known games).\n");
        Str_Free(&searchPath);
        return true;
    }

    // Are we loading a game?
    try
    {
        de::Game& game = games->byIdentityKey(Str_Text(&searchPath));
        if(!game.allStartupFilesFound())
        {
            Con_Message("Failed to locate all required startup resources:\n");
            de::Game::printFiles(game, FF_STARTUP);
            Con_Message("%s (%s) cannot be loaded.\n", Str_Text(&game.title()), Str_Text(&game.identityKey()));
            Str_Free(&searchPath);
            return true;
        }
        if(!DD_ChangeGame(game))
        {
            Str_Free(&searchPath);
            return false;
        }
        didLoadGame = true;
        ++arg;
    }
    catch(const de::GameCollection::NotFoundError&)
    {} // Ignore the error.

    // Try the resource locator.
    AutoStr* foundPath = AutoStr_NewStd();
    for(; arg < argc; ++arg)
    {
        de::Uri searchPath = de::Uri::fromNativePath(argv[arg], RC_PACKAGE);
        if(!F_FindPath2(RC_PACKAGE, reinterpret_cast<uri_s*>(&searchPath), foundPath, RLF_MATCH_EXTENSION)) continue;

        if(tryLoadFile(de::Uri(Str_Text(foundPath), RC_NULL)))
        {
            didLoadResource = true;
        }
    }

    if(didLoadResource)
        DD_UpdateEngineState();

    Str_Free(&searchPath);
    return (didLoadGame || didLoadResource);
}

static de::File1* tryLoadFile(de::Uri const& search, size_t baseOffset)
{
    try
    {
        de::FileHandle& hndl = App_FileSystem()->openFile(search.path(), "rb", baseOffset, false /* no duplicates */);

        de::Uri foundFileUri = hndl.file().composeUri();
        VERBOSE( Con_Message("Loading \"%s\"...\n", NativePath(foundFileUri.asText()).pretty().toUtf8().constData()) )

        App_FileSystem()->index(hndl.file());

        return &hndl.file();
    }
    catch(FS1::NotFoundError const&)
    {
        if(App_FileSystem()->accessFile(search))
        {
            // Must already be loaded.
            Con_Message("\"%s\" already loaded.\n", NativePath(search.asText()).pretty().toUtf8().constData());
        }
    }
    return 0;
}

static bool tryUnloadFile(de::Uri const& search)
{
    try
    {
        de::File1& file = App_FileSystem()->find(search);
        de::Uri foundFileUri = file.composeUri();
        QByteArray pathUtf8 = NativePath(foundFileUri.asText()).pretty().toUtf8();

        // Do not attempt to unload a resource required by the current game.
        if(games->currentGame().isRequiredFile(file))
        {
            Con_Message("\"%s\" is required by the current game.\n"
                        "Required game files cannot be unloaded in isolation.\n",
                        pathUtf8.constData());
            return false;
        }

        VERBOSE2( Con_Message("Unloading \"%s\"...\n", pathUtf8.constData()) )

        App_FileSystem()->deindex(file);
        delete &file;

        VERBOSE2( Con_Message("Done unloading \"%s\".\n", pathUtf8.constData()) )
        return true;
    }
    catch(FS1::NotFoundError const&)
    {} // Ignore.
    return false;
}

D_CMD(Unload)
{
    DENG_UNUSED(src);

    ddstring_t searchPath;
    int i;

    // No arguments; unload the current game if loaded.
    if(argc == 1)
    {
        if(!DD_GameLoaded())
        {
            Con_Message("There is no game currently loaded.\n");
            return true;
        }
        return DD_ChangeGame(games->nullGame());
    }

    Str_Init(&searchPath);
    Str_Set(&searchPath, argv[1]);
    Str_Strip(&searchPath);
    if(Str_IsEmpty(&searchPath))
    {
        Str_Free(&searchPath);
        return false;
    }
    F_FixSlashes(&searchPath, &searchPath);

    // Ignore attempts to unload directories.
    if(Str_RAt(&searchPath, 0) == '/')
    {
        Con_Message("Directories cannot be \"unloaded\" (only files and/or known games).\n");
        Str_Free(&searchPath);
        return true;
    }

    // Unload the current game if specified.
    if(argc == 2)
    {
        try
        {
            de::Game& game = games->byIdentityKey(Str_Text(&searchPath));
            Str_Free(&searchPath);
            if(DD_GameLoaded())
            {
                return DD_ChangeGame(games->nullGame());
            }

            Con_Message("%s is not currently loaded.\n", Str_Text(&game.identityKey()));
            return true;
        }
        catch(de::GameCollection::NotFoundError const&)
        {} // Ignore the error.
    }

    // Try the resource locator.
    bool didUnloadFiles = false;
    AutoStr* foundPath = AutoStr_NewStd();
    for(i = 1; i < argc; ++i)
    {
        de::Uri searchPath = de::Uri(NativePath(argv[1]).expand().withSeparators('/'), RC_PACKAGE);
        if(!F_FindPath(RC_PACKAGE, reinterpret_cast<uri_s*>(&searchPath), foundPath)) continue;

        if(tryUnloadFile(de::Uri(Str_Text(foundPath), RC_NULL)))
        {
            didUnloadFiles = true;
        }
    }

    if(didUnloadFiles)
    {
        // A changed file list may alter the main lump directory.
        DD_UpdateEngineState();
    }

    Str_Free(&searchPath);
    return didUnloadFiles;
}

D_CMD(Reset)
{
    DENG_UNUSED(src); DENG_UNUSED(argc); DENG_UNUSED(argv);

    DD_UpdateEngineState();
    return true;
}

D_CMD(ReloadGame)
{
    DENG_UNUSED(src); DENG_UNUSED(argc); DENG_UNUSED(argv);

    if(!DD_GameLoaded())
    {
        Con_Message("No game is presently loaded.\n");
        return true;
    }
    DD_ChangeGame(games->currentGame(), true/* allow reload */);
    return true;
}

#ifdef UNIX
/**
 * Some routines not available on the *nix platform.
 */
char* strupr(char* string)
{
    char* ch = string;
    for(; *ch; ch++)
        *ch = toupper(*ch);
    return string;
}

char* strlwr(char* string)
{
    char* ch = string;
    for(; *ch; ch++)
        *ch = tolower(*ch);
    return string;
}
#endif

/**
 * Prints a formatted string into a fixed-size buffer. At most @c size
 * characters will be written to the output buffer @c str. The output will
 * always contain a terminating null character.
 *
 * @param str           Output buffer.
 * @param size          Size of the output buffer.
 * @param format        Format of the output.
 *
 * @return              Number of characters written to the output buffer
 *                      if lower than or equal to @c size, else @c -1.
 */
int dd_snprintf(char* str, size_t size, const char* format, ...)
{
    int result = 0;

    va_list args;
    va_start(args, format);
    result = dd_vsnprintf(str, size, format, args);
    va_end(args);

    return result;
}
