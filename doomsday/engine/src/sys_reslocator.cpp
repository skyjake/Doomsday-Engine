/**
 * @file sys_reslocator.cpp
 *
 * Resource location algorithms and bookeeping.
 *
 * @ingroup resources
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include <QList>

#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"
#include "resourcerecord.h"
#include "resourcenamespace.h"

#include <de/Error>
#include <de/NativePath>
#include <de/c_wrapper.h>
#include <de/memory.h>

using namespace de;

struct ResourceTypeInfo
{
    static int const max_extensions = 3;

    /// Default class attributed to resources of this type.
    resourceclass_t defaultClass;
    char const* knownFileNameExtensions[max_extensions];
};

static bool inited = false;

static ResourceTypeInfo const typeInfo[NUM_RESOURCE_TYPES] = {
    /* RT_ZIP */        { RC_PACKAGE,      { ".pk3", ".zip", 0 } },
    /* RT_WAD */        { RC_PACKAGE,      { ".wad", 0 } },
    /* RT_DED */        { RC_DEFINITION,   { ".ded", 0 } },
    /* RT_PNG */        { RC_GRAPHIC,      { ".png", 0 } },
    /* RT_JPG */        { RC_GRAPHIC,      { ".jpg", 0 } },
    /* RT_TGA */        { RC_GRAPHIC,      { ".tga", 0 } },
    /* RT_PCX */        { RC_GRAPHIC,      { ".pcx", 0 } },
    /* RT_DMD */        { RC_MODEL,        { ".dmd", 0 } },
    /* RT_MD2 */        { RC_MODEL,        { ".md2", 0 } },
    /* RT_WAV */        { RC_SOUND,        { ".wav", 0 } },
    /* RT_OGG */        { RC_MUSIC,        { ".ogg", 0 } },
    /* RT_MP3 */        { RC_MUSIC,        { ".mp3", 0 } },
    /* RT_MOD */        { RC_MUSIC,        { ".mod", 0 } },
    /* RT_MID */        { RC_MUSIC,        { ".mid", 0 } },
    /* RT_DEH */        { RC_UNKNOWN,      { ".deh", 0 } },
    /* RT_DFN */        { RC_FONT,         { ".dfn", 0 } }
};

// Recognized resource types (in order of importance, left to right).
#define MAX_TYPEORDER 6
static resourcetype_t const searchTypeOrder[RESOURCECLASS_COUNT][MAX_TYPEORDER] = {
    /* RC_PACKAGE */    { RT_ZIP, RT_WAD, RT_NONE }, // Favor ZIP over WAD.
    /* RC_DEFINITION */ { RT_DED, RT_NONE }, // Only DED files.
    /* RC_GRAPHIC */    { RT_PNG, RT_TGA, RT_JPG, RT_PCX, RT_NONE }, // Favour quality.
    /* RC_MODEL */      { RT_DMD, RT_MD2, RT_NONE }, // Favour DMD over MD2.
    /* RC_SOUND */      { RT_WAV, RT_NONE }, // Only WAV files.
    /* RC_MUSIC */      { RT_OGG, RT_MP3, RT_WAV, RT_MOD, RT_MID, RT_NONE },
    /* RC_FONT */       { RT_DFN, RT_NONE } // Only DFN fonts.
};

static ResourceNamespaces namespaces;

static inline ResourceTypeInfo const& resourceTypeInfo(resourcetype_t type)
{
    if(!VALID_RESOURCE_TYPE(type)) throw Error("resourceTypeInfo", String("Invalid type %1").arg(type));
    return typeInfo[uint(type) - 1];
}

static ResourceNamespace* namespaceByName(String name)
{
    if(!name.isEmpty())
    {
        for(int i = 0; i < namespaces.count(); ++i)
        {
            ResourceNamespace& rnamespace = *namespaces[i];
            if(!rnamespace.name().compareWithoutCase(name))
                return &rnamespace;
        }
    }
    return 0; // Not found.
}

static void resetAllNamespaces()
{
    DENG2_FOR_EACH(ResourceNamespaces, i, namespaces)
    {
        (*i)->reset();
    }
}

/**
 * @param name      Unique symbolic name of this namespace. Must be at least
 *                  @c RESOURCENAMESPACE_MINNAMELENGTH characters long.
 * @param flags     @ref ResourceNamespace::Flag
 */
static ResourceNamespace& createResourceNamespace(String name, ResourceNamespace::Flags flags = 0)
{
    DENG_ASSERT(name.length() >= RESOURCENAMESPACE_MINNAMELENGTH);
    namespaces.push_back(new ResourceNamespace(name, flags));
    return *namespaces.back();
}

static bool findResourceInNamespace(ResourceNamespace& rnamespace, de::Uri const& searchPath,
    ddstring_t* foundPath)
{
    if(searchPath.isEmpty()) return 0;

    LOG_TRACE("Using namespace '%s'...") << rnamespace.name();

    // Ensure the namespace is up to date.
    rnamespace.rebuild();

    // A resource name is the file name sans extension.
    String name = searchPath.firstPathNode().toString().fileNameWithoutExtension();

    // Perform the search.
    ResourceNamespace::ResourceList foundResources;
    if(rnamespace.findAll(name, foundResources))
    {
        // There is at least one name-matched (perhaps partially) resource.
        DENG2_FOR_EACH_CONST(ResourceNamespace::ResourceList, i, foundResources)
        {
            PathTree::Node& node = **i;
            if(!node.comparePath(searchPath, PCF_NO_BRANCH))
            {
                // This is the resource we are looking for.
                // Does the caller want to know the matched path?
                if(foundPath)
                {
                    QByteArray path = node.composePath().toUtf8();
                    Str_Set(foundPath, path.constData());
                    F_PrependBasePath(foundPath, foundPath);
                }
                return true;
            }
        }
    }
    return false; // Not found.
}

static bool findResourceFile(de::Uri const& searchPath, ddstring_t* foundPath)
{
    try
    {
        de::File1& file = App_FileSystem()->find(searchPath.compose());
        // Does the caller want to know the matched path?
        if(foundPath)
        {
            QByteArray searchPathUtf8 = file.composePath().toUtf8();
            Str_Set(foundPath, searchPathUtf8.constData());
            F_PrependBasePath(foundPath, foundPath);
        }
        return true;
    }
    catch(FS1::NotFoundError const&)
    {} // Ignore this error.
    return false;
}

static bool findResource3(ResourceNamespace* rnamespace, de::Uri const& searchPath,
    ddstring_t* foundPath)
{
    // Is there a namespace we should use?
    if(rnamespace)
    {
        return findResourceInNamespace(*rnamespace, searchPath, foundPath);
    }
    return findResourceFile(searchPath, foundPath);
}

static bool findResource2(int flags, resourceclass_t rclass, String searchPath,
    ddstring_t* foundPath, ResourceNamespace* rnamespace)
{
    if(searchPath.isEmpty()) return false;

    // If an extension was specified, first look for resources of the same type.
    String ext = searchPath.fileNameExtension();
    if(!ext.isEmpty() && ext.compare(".*"))
    {
        if(findResource3(rnamespace, de::Uri(searchPath, RC_NULL), foundPath)) return true;

        // If we are looking for a particular resource type, get out of here.
        if(flags & RLF_MATCH_EXTENSION) return false;
    }

    if(!(VALID_RESOURCE_CLASS(rclass) && searchTypeOrder[rclass][0] != RT_NONE)) return false;

    /*
     * Try some different name patterns (i.e., resource types) known to us.
     */

    // Create a copy of the searchPath minus file extension.
    String path2 = searchPath.fileNamePath() / searchPath.fileNameWithoutExtension();

    path2.reserve(path2.length() + 5 /*max expected extension length*/);

    bool found = false;
    resourcetype_t const* typeIter = searchTypeOrder[rclass];
    do
    {
        ResourceTypeInfo const& typeInfo = resourceTypeInfo(*typeIter);
        if(!typeInfo.knownFileNameExtensions[0]) continue;

        char const* const* ext = typeInfo.knownFileNameExtensions;
        do
        {
            found = findResource3(rnamespace, de::Uri(path2 + *ext, RC_NULL), foundPath);

        } while(!found && *(++ext));

    } while(!found && *(++typeIter) != RT_NONE);

    return found;
}

static bool findResource(resourceclass_t rclass, de::Uri const& searchPath,
    ddstring_t* foundPath, int flags, String optionalSuffix = "")
{
    DENG_ASSERT(rclass == RC_UNKNOWN || VALID_RESOURCE_CLASS(rclass));

    LOG_AS("findResource");

    if(searchPath.isEmpty()) return false;

    try
    {
        String const& resolvedPath = searchPath.resolved();

        // Is a namespace specified?
        ResourceNamespace* rnamespace = namespaceByName(Str_Text(searchPath.scheme()));

        // First try with the optional suffix.
        if(!optionalSuffix.isEmpty())
        {
            String resolvedPath2 = resolvedPath.fileNamePath()
                                 / resolvedPath.fileNameWithoutExtension() + optionalSuffix + resolvedPath.fileNameExtension();

            if(findResource2(flags, rclass, resolvedPath2, foundPath, rnamespace))
                return true;
        }

        // Try without a suffix.
        return findResource2(flags, rclass, resolvedPath, foundPath, rnamespace);
    }
    catch(de::Uri::ResolveError const& er)
    {
        // Log but otherwise ignore incomplete paths.
        LOG_DEBUG(er.asText());
    }
    return false;
}

static void createPackagesResourceNamespace(void)
{
    ddstring_t** doomWadPaths = 0, *doomWadDir = 0;
    uint doomWadPathsCount = 0, idx;

#ifdef UNIX
    {
        // Check the system-level config files.
        filename_t fn;
        if(UnixInfo_GetConfigValue("paths", "iwaddir", fn, FILENAME_T_MAXLEN))
        {
            doomWadDir = Str_Set(Str_New(), fn);
        }
    }
#endif

    // Is the DOOMWADPATH environment variable in use?
    if(!CommandLine_Check("-nodoomwadpath") && getenv("DOOMWADPATH"))
    {
#if WIN32
# define PATH_DELIMITER_CHAR    ';'
#else
# define PATH_DELIMITER_CHAR    ':'
#endif

#define ADDDOOMWADPATH(path) \
{ \
    Str_Strip(path); \
    if(!Str_IsEmpty(path) && F_IsAbsolute(path)) \
    { \
        ddstring_t* pathCopy = Str_New(); \
        F_FixSlashes(pathCopy, path); \
        F_AppendMissingSlash(pathCopy); \
        LOG_INFO(" %i: %s") << n << NativePath(Str_Text(pathCopy)).pretty(); \
        doomWadPaths = (ddstring_t**) M_Realloc(doomWadPaths, sizeof(*doomWadPaths) * ++doomWadPathsCount); \
        doomWadPaths[doomWadPathsCount-1] = pathCopy; \
    } \
}

        ddstring_t fullString; Str_Init(&fullString);

        Str_Set(&fullString, getenv("DOOMWADPATH"));
        Str_Strip(&fullString);
        if(!Str_IsEmpty(&fullString))
        {
            LOG_INFO("Using DOOMWADPATH:");

            // Split into paths.
            char const* c = Str_Text(&fullString);
            ddstring_t path; Str_Init(&path);
            int n = 0;
            while((c = Str_CopyDelim2(&path, c, PATH_DELIMITER_CHAR, CDF_OMIT_DELIMITER))) // Get the next path.
            {
                ADDDOOMWADPATH(&path)
                n++;
            }
            // Add the last path.
            if(!Str_IsEmpty(&path))
            {
                ADDDOOMWADPATH(&path)
            }
            Str_Free(&path);
        }

        Str_Free(&fullString);

#undef ADDDOOMWADPATH
#undef PATH_DELIMITER_CHAR
    }

    // Is the DOOMWADDIR environment variable in use?
    if(!doomWadDir && !CommandLine_Check("-nodoomwaddir") && getenv("DOOMWADDIR"))
    {
        doomWadDir = Str_Set(Str_New(), getenv("DOOMWADDIR"));
        Str_Strip(doomWadDir);
        F_FixSlashes(doomWadDir, doomWadDir);
        if(Str_IsEmpty(doomWadDir) || !F_IsAbsolute(doomWadDir))
        {
            Str_Delete(doomWadDir);
            doomWadDir = 0;
        }
        else
        {
            F_AppendMissingSlash(doomWadDir);
            LOG_INFO("Using DOOMWADDIR: %s") << NativePath(Str_Text(doomWadDir)).pretty();
        }
    }

    // Construct the search path list.
    uint searchPathsCount = 2 + doomWadPathsCount + (doomWadDir != 0? 1 : 0);
    de::Uri** searchPaths = (de::Uri**) M_Malloc(sizeof(*searchPaths) * searchPathsCount);
    if(!searchPaths) Con_Error("createPackagesResourceNamespace: Failed on allocation of %lu bytes.", (unsigned long) (sizeof(*searchPaths) * searchPathsCount));

    idx = 0;
    // Add the default paths.
    searchPaths[idx++] = new de::Uri("$(App.DataPath)/", RC_NULL);
    searchPaths[idx++] = new de::Uri("$(App.DataPath)/$(GamePlugin.Name)/", RC_NULL);

    // Add any paths from the DOOMWADPATH environment variable.
    if(doomWadPaths)
    {
        for(uint i = 0; i < doomWadPathsCount; ++i)
        {
            searchPaths[idx++] = new de::Uri(Str_Text(doomWadPaths[i]), RC_NULL);
            Str_Delete(doomWadPaths[i]);
        }
        M_Free(doomWadPaths);
    }

    // Add the path from the DOOMWADDIR environment variable.
    if(doomWadDir)
    {
        searchPaths[idx++] = new de::Uri(Str_Text(doomWadDir), RC_NULL);
        Str_Delete(doomWadDir);
    }

    ResourceNamespace& rnamespace = createResourceNamespace("Packages");
    for(uint i = 0; i < searchPathsCount; ++i)
    {
        rnamespace.addSearchPath(ResourceNamespace::DefaultPaths, *searchPaths[i], SPF_NO_DESCEND);
    }

    for(uint i = 0; i < searchPathsCount; ++i)
    {
        delete searchPaths[i];
    }
    M_Free(searchPaths);
}

void F_CreateNamespacesForFileResourcePaths(void)
{
#define NAMESPACEDEF_MAX_SEARCHPATHS        5

    struct namespacedef_s {
        char const* name;
        char const* optOverridePath;
        char const* optFallbackPath;
        ResourceNamespace::Flags flags;
        int searchPathFlags; /// @see searchPathFlags
        /// Priority is right to left.
        char const* searchPaths[NAMESPACEDEF_MAX_SEARCHPATHS];
    } defs[] = {
        { "Defs",         NULL,           NULL,           ResourceNamespace::Flag(0), 0,
            { "$(App.DefsPath)/", "$(App.DefsPath)/$(GamePlugin.Name)/", "$(App.DefsPath)/$(GamePlugin.Name)/$(Game.IdentityKey)/" }
        },
        { "Graphics",     "-gfxdir2",     "-gfxdir",      ResourceNamespace::Flag(0), 0,
            { "$(App.DataPath)/graphics/" }
        },
        { "Models",       "-modeldir2",   "-modeldir",    ResourceNamespace::MappedInPackages, 0,
            { "$(App.DataPath)/$(GamePlugin.Name)/models/", "$(App.DataPath)/$(GamePlugin.Name)/models/$(Game.IdentityKey)/" }
        },
        { "Sfx",          "-sfxdir2",     "-sfxdir",      ResourceNamespace::MappedInPackages, SPF_NO_DESCEND,
            { "$(App.DataPath)/$(GamePlugin.Name)/sfx/", "$(App.DataPath)/$(GamePlugin.Name)/sfx/$(Game.IdentityKey)/" }
        },
        { "Music",        "-musdir2",     "-musdir",      ResourceNamespace::MappedInPackages, SPF_NO_DESCEND,
            { "$(App.DataPath)/$(GamePlugin.Name)/music/", "$(App.DataPath)/$(GamePlugin.Name)/music/$(Game.IdentityKey)/" }
        },
        { "Textures",     "-texdir2",     "-texdir",      ResourceNamespace::MappedInPackages, SPF_NO_DESCEND,
            { "$(App.DataPath)/$(GamePlugin.Name)/textures/", "$(App.DataPath)/$(GamePlugin.Name)/textures/$(Game.IdentityKey)/" }
        },
        { "Flats",        "-flatdir2",    "-flatdir",     ResourceNamespace::MappedInPackages, SPF_NO_DESCEND,
            { "$(App.DataPath)/$(GamePlugin.Name)/flats/", "$(App.DataPath)/$(GamePlugin.Name)/flats/$(Game.IdentityKey)/" }
        },
        { "Patches",      "-patdir2",     "-patdir",      ResourceNamespace::MappedInPackages, SPF_NO_DESCEND,
            { "$(App.DataPath)/$(GamePlugin.Name)/patches/", "$(App.DataPath)/$(GamePlugin.Name)/patches/$(Game.IdentityKey)/" }
        },
        { "LightMaps",    "-lmdir2",      "-lmdir",       ResourceNamespace::MappedInPackages, 0,
            { "$(App.DataPath)/$(GamePlugin.Name)/lightmaps/" }
        },
        { "Fonts",        "-fontdir2",    "-fontdir",     ResourceNamespace::MappedInPackages, SPF_NO_DESCEND,
            { "$(App.DataPath)/fonts/", "$(App.DataPath)/$(GamePlugin.Name)/fonts/", "$(App.DataPath)/$(GamePlugin.Name)/fonts/$(Game.IdentityKey)/" }
        },
        { 0, 0, 0, ResourceNamespace::Flag(0), 0, { 0 } }
    };

    // Setup of the Packages namespace is somewhat more involved...
    createPackagesResourceNamespace();

    // Setup the rest...
    for(size_t i = 0; defs[i].name; ++i)
    {
        uint j, searchPathCount;
        struct namespacedef_s* def = &defs[i];
        ResourceNamespace& rnamespace = createResourceNamespace(def->name, def->flags);

        searchPathCount = 0;
        while(def->searchPaths[searchPathCount] && ++searchPathCount < NAMESPACEDEF_MAX_SEARCHPATHS)
        {}

        for(j = 0; j < searchPathCount; ++j)
        {
            de::Uri uri = de::Uri(def->searchPaths[j], RC_NULL);
            rnamespace.addSearchPath(ResourceNamespace::DefaultPaths, uri, def->searchPathFlags);
        }

        if(def->optOverridePath && CommandLine_CheckWith(def->optOverridePath, 1))
        {
            String path = NativePath(CommandLine_NextAsPath()).expand().withSeparators('/');

            de::Uri uri2 = de::Uri(path / "$(Game.IdentityKey)/", RC_NULL);
            rnamespace.addSearchPath(ResourceNamespace::OverridePaths, uri2, def->searchPathFlags);

            de::Uri uri = de::Uri(path, RC_NULL);
            rnamespace.addSearchPath(ResourceNamespace::OverridePaths, uri, def->searchPathFlags);
        }

        if(def->optFallbackPath && CommandLine_CheckWith(def->optFallbackPath, 1))
        {
            String path = NativePath(CommandLine_NextAsPath()).expand().withSeparators('/');
            de::Uri uri = de::Uri(path, RC_NULL);
            rnamespace.addSearchPath(ResourceNamespace::FallbackPaths, uri, def->searchPathFlags);
        }
    }

#undef NAMESPACEDEF_MAX_SEARCHPATHS
}

void F_InitResourceLocator(void)
{
    // Allow re-init.
    inited = true;
}

void F_ShutdownResourceLocator(void)
{
    if(!inited) return;
    DENG2_FOR_EACH(ResourceNamespaces, i, namespaces)
    {
        delete *i;
    }
    namespaces.clear();
    inited = false;
}

void F_ResetAllResourceNamespaces(void)
{
    resetAllNamespaces();
}

ResourceNamespace* F_ResourceNamespaceByName(String name)
{
    return namespaceByName(name);
}

ResourceNamespaces const& F_ResourceNamespaces()
{
    DENG_ASSERT(inited);
    return namespaces;
}

boolean F_FindResource4(resourceclass_t rclass, uri_s const* searchPath,
    ddstring_t* foundPath, int flags, char const* optionalSuffix)
{
    DENG_ASSERT(searchPath);
    return findResource(rclass, reinterpret_cast<de::Uri const&>(*searchPath), foundPath, flags, optionalSuffix);
}

boolean F_FindResource3(resourceclass_t rclass, uri_s const* searchPath,
    ddstring_t* foundPath, int flags)
{
    return F_FindResource4(rclass, searchPath, foundPath, flags, NULL);
}

boolean F_FindResource2(resourceclass_t rclass, uri_s const* searchPath,
    ddstring_t* foundPath)
{
    return F_FindResource3(rclass, searchPath, foundPath, RLF_DEFAULT);
}

boolean F_FindResource(resourceclass_t rclass, uri_s const* searchPath)
{
    return F_FindResource2(rclass, searchPath, NULL);
}

uint F_FindResourceFromList(resourceclass_t rclass, char const* searchPaths,
    ddstring_t* foundPath, int flags, char const* optionalSuffix)
{
    if(!searchPaths || !searchPaths[0]) return 0;

    QStringList paths = String(searchPaths).split(';', QString::SkipEmptyParts);
    int pathIndex = 0;
    for(QStringList::const_iterator i = paths.constBegin(); i != paths.constEnd(); ++i, ++pathIndex)
    {
        de::Uri searchPath = de::Uri(*i, rclass);
        if(findResource(rclass, searchPath, foundPath, flags, optionalSuffix))
        {
            return pathIndex + 1; // 1-based index.
        }
    }

    return 0; // Not found.
}

resourceclass_t F_DefaultResourceClassForType(resourcetype_t type)
{
    if(type == RT_NONE) return RC_UNKNOWN;
    return resourceTypeInfo(type).defaultClass;
}

ResourceNamespace* F_DefaultResourceNamespaceForClass(resourceclass_t rclass)
{
    DENG_ASSERT(VALID_RESOURCE_CLASS(rclass));
    static String const defaultNamespaces[RESOURCECLASS_COUNT] = {
        /* RC_PACKAGE */    "Packages",
        /* RC_DEFINITION */ "Defs",
        /* RC_GRAPHIC */    "Graphics",
        /* RC_MODEL */      "Models",
        /* RC_SOUND */      "Sfx",
        /* RC_MUSIC */      "Music",
        /* RC_FONT */       "Fonts"
    };
    return namespaceByName(defaultNamespaces[rclass]);
}

resourcetype_t F_GuessResourceTypeByName(String path)
{
    if(path.isEmpty()) return RT_NONE;

    // We require a file extension for this.
    String ext = path.fileNameExtension();
    if(ext.isEmpty()) return RT_NONE;

    for(uint i = RT_FIRST; i < NUM_RESOURCE_TYPES; ++i)
    {
        ResourceTypeInfo const& info = resourceTypeInfo(resourcetype_t(i));
        if(!info.knownFileNameExtensions) continue;

        for(char const* const* cand = info.knownFileNameExtensions; *cand; ++cand)
        {
            if(!ext.compareWithoutCase(ext))
            {
                return resourcetype_t(i);
            }
        }
    }

    return RT_NONE;
}

String const& F_ResourceClassStr(resourceclass_t rclass)
{
    DENG_ASSERT(VALID_RESOURCE_CLASS(rclass));
    static String const resourceClassNames[RESOURCECLASS_COUNT] = {
        "RC_PACKAGE",
        "RC_DEFINITION",
        "RC_GRAPHIC",
        "RC_MODEL",
        "RC_SOUND",
        "RC_MUSIC",
        "RC_FONT"
    };
    return resourceClassNames[uint(rclass)];
}
