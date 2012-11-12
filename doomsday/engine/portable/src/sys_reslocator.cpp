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

#include <QDir>

#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"
#include "resourcerecord.h"
#include "resourcenamespace.h"

#include <de/NativePath>
#include <de/c_wrapper.h>
#include <de/memory.h>

using namespace de;

#define PATH_DELIMIT_CHAR       ';'
#define PATH_DELIMIT_STR        ";"

#define MAX_EXTENSIONS          (3)
typedef struct {
    /// Default class attributed to resources of this type.
    resourceclass_t defaultClass;
    char const* knownFileNameExtensions[MAX_EXTENSIONS];
} resourcetypeinfo_t;

/**
 * @defgroup resourceNamespaceFlags Resource Namespace Flags
 * @ingroup core flags
 * @{
 */
#define RNF_USE_VMAP            0x01 // Map resources in packages.
///@}

struct ResourceNamespaceInfo
{
    /// ResourceNamespace.
    ResourceNamespace* rnamespace;

    byte flags; // @see resourceNamespaceFlags
};

static bool inited = false;

static resourcetypeinfo_t const typeInfo[NUM_RESOURCE_TYPES] = {
    /* RT_ZIP */        { RC_PACKAGE,      {"pk3", "zip", 0} },
    /* RT_WAD */        { RC_PACKAGE,      {"wad", 0} },
    /* RT_DED */        { RC_DEFINITION,   {"ded", 0} },
    /* RT_PNG */        { RC_GRAPHIC,      {"png", 0} },
    /* RT_JPG */        { RC_GRAPHIC,      {"jpg", 0} },
    /* RT_TGA */        { RC_GRAPHIC,      {"tga", 0} },
    /* RT_PCX */        { RC_GRAPHIC,      {"pcx", 0} },
    /* RT_DMD */        { RC_MODEL,        {"dmd", 0} },
    /* RT_MD2 */        { RC_MODEL,        {"md2", 0} },
    /* RT_WAV */        { RC_SOUND,        {"wav", 0} },
    /* RT_OGG */        { RC_MUSIC,        {"ogg", 0} },
    /* RT_MP3 */        { RC_MUSIC,        {"mp3", 0} },
    /* RT_MOD */        { RC_MUSIC,        {"mod", 0} },
    /* RT_MID */        { RC_MUSIC,        {"mid", 0} },
    /* RT_DEH */        { RC_UNKNOWN,      {"deh", 0} },
    /* RT_DFN */        { RC_FONT,         {"dfn", 0} }
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

static de::Str const defaultNamespaceForClass[RESOURCECLASS_COUNT] = {
    /* RC_PACKAGE */    PACKAGES_RESOURCE_NAMESPACE_NAME,
    /* RC_DEFINITION */ DEFINITIONS_RESOURCE_NAMESPACE_NAME,
    /* RC_GRAPHIC */    GRAPHICS_RESOURCE_NAMESPACE_NAME,
    /* RC_MODEL */      MODELS_RESOURCE_NAMESPACE_NAME,
    /* RC_SOUND */      SOUNDS_RESOURCE_NAMESPACE_NAME,
    /* RC_MUSIC */      MUSIC_RESOURCE_NAMESPACE_NAME,
    /* RC_FONT */       FONTS_RESOURCE_NAMESPACE_NAME
};

static ResourceNamespaceInfo* namespaces = 0;
static uint numNamespaces = 0;

#define RESOURCENAMESPACE_MINNAMELENGTH URI_MINSCHEMELENGTH

/**
 * @param name      Unique symbolic name of this namespace. Must be at least
 *                  @c RESOURCENAMESPACE_MINNAMELENGTH characters long.
 * @param flags     @ref resourceNamespaceFlags
 */
ResourceNamespace* F_CreateResourceNamespace(char const* name, byte flags);

static void errorIfNotInited(const char* callerName)
{
    if(inited) return;
    Con_Error("%s: resource locator module is not presently initialized.", callerName);
    // Unreachable. Prevents static analysers from getting rather confused, poor things.
    exit(1);
}

static inline const resourcetypeinfo_t* getInfoForResourceType(resourcetype_t type)
{
    DENG_ASSERT(VALID_RESOURCE_TYPE(type));
    return &typeInfo[((uint)type)-1];
}

static inline ResourceNamespaceInfo* getNamespaceInfoForId(resourcenamespaceid_t rni)
{
    errorIfNotInited("getNamespaceForId");
    if(!F_IsValidResourceNamespaceId(rni))
        Con_Error("getNamespaceForId: Invalid namespace id %i.", (int)rni);
    return &namespaces[((uint)rni)-1];
}

static resourcenamespaceid_t findNamespaceForName(String name)
{
    if(!name.isEmpty())
    {
        for(uint i = 0; i < numNamespaces; ++i)
        {
            ResourceNamespaceInfo* info = &namespaces[i];
            if(!info->rnamespace->name().compareWithoutCase(name))
                return (resourcenamespaceid_t)(i+1);
        }
    }
    return 0;
}

static void destroyAllNamespaces(void)
{
    if(numNamespaces == 0) return;

    for(uint i = 0; i < numNamespaces; ++i)
    {
        ResourceNamespaceInfo* info = &namespaces[i];
        delete info->rnamespace;
    }
    M_Free(namespaces);
    namespaces = 0;
}

static void resetAllNamespaces(void)
{
    resourcenamespaceid_t rni;
    for(rni = 1; rni < numNamespaces+1; ++rni)
    {
        F_ResetResourceNamespace(rni);
    }
}

/**
 * Find a named resource in this namespace.
 *
 * @param rnamespace    ResourceNamespace to be searched.
 * @param searchPath    Relative or absolute path to the resource.
 * @param delimiter     Fragments of @a searchPath are delimited by this character.
 *
 * @return  The found PathTree node which represents the resource else @c NULL.
 */
static PathTree::Node* findResourceInNamespace(ResourceNamespace& rnamespace,
    String searchPath, QChar delimiter = '/')
{
    if(searchPath.isEmpty()) return 0;

    LOG_TRACE("Using namespace '%s'...") << rnamespace.name();

    // Ensure the namespace is up to date.
    rnamespace.rebuild();

    // Perform the search.
    ResourceNamespace::ResourceList foundResources;
    if(rnamespace.findAll(searchPath, foundResources))
    {
        // There is at least one name-matched (perhaps partially) resource.
        de::Uri searchPattern = de::Uri(searchPath, RC_NULL, delimiter.toLatin1());

        DENG2_FOR_EACH_CONST(ResourceNamespace::ResourceList, i, foundResources)
        {
            PathTree::Node* node = *i;
            if(node->comparePath(searchPattern, PCF_NO_BRANCH)) continue;

            // This is the resource we are looking for.
            return node;
        }
    }
    return 0; // Not found.
}

static bool tryFindResource2(resourceclass_t /*rclass*/, String searchPath,
    ddstring_t* foundPath, ResourceNamespace* rnamespace)
{
    if(searchPath.isEmpty()) return false;

    // Is there a namespace we should use?
    if(rnamespace)
    {
        if(PathTree::Node* found = findResourceInNamespace(*rnamespace, searchPath))
        {
            // Does the caller want to know the matched path?
            if(foundPath)
            {
                QByteArray path = found->composePath().toUtf8();
                Str_Set(foundPath, path.constData());
                F_PrependBasePath(foundPath, foundPath);
            }
            return true;
        }
    }

    if(App_FileSystem()->accessFile(searchPath))
    {
        if(foundPath)
        {
            QByteArray searchPathUtf8 = searchPath.toUtf8();
            Str_Set(foundPath, searchPathUtf8.constData());
            F_PrependBasePath(foundPath, foundPath);
        }
        return true;
    }
    return false;
}

static bool tryFindResource(int flags, resourceclass_t rclass, String searchPath,
    ddstring_t* foundPath, ResourceNamespace* rnamespace)
{
    if(searchPath.isEmpty()) return false;

    // If an extension was specified, first look for resources of the same type.
    String ext = searchPath.fileNameExtension();
    if(!ext.isEmpty() && ext.compare(".*"))
    {
        if(tryFindResource2(rclass, searchPath, foundPath, rnamespace)) return true;

        // If we are looking for a particular resource type, get out of here.
        if(flags & RLF_MATCH_EXTENSION) return false;
    }

    if(!(VALID_RESOURCE_CLASS(rclass) && searchTypeOrder[rclass][0] != RT_NONE)) return false;

    /*
     * Try some different name patterns (i.e., resource types) known to us.
     */

    // Create a copy of the searchPath minus file extension.
    String path2 = searchPath.fileNamePath() / searchPath.fileNameWithoutExtension() + ".";

    path2.reserve(path2.length() + 5 /*max expected extension length*/);

    bool found = false;
    resourcetype_t const* typeIter = searchTypeOrder[rclass];
    do
    {
        resourcetypeinfo_t const* typeInfo = getInfoForResourceType(*typeIter);
        if(typeInfo->knownFileNameExtensions[0])
        {
            char const* const* ext = typeInfo->knownFileNameExtensions;
            do
            {
                found = tryFindResource2(rclass, path2 + *ext, foundPath, rnamespace);
            } while(!found && *(++ext));
        }
    } while(!found && *(++typeIter) != RT_NONE);

    return found;
}

static bool findResource2(resourceclass_t rclass, String searchPath,
    ddstring_t* foundPath, int flags, ddstring_t const* optionalSuffix,
    ResourceNamespace* rnamespace)
{
    if(searchPath.isEmpty()) return false;

    // First try with the optional suffix.
    if(optionalSuffix && !Str_IsEmpty(optionalSuffix))
    {
        String searchPath2 = searchPath.fileNamePath()
                           / searchPath.fileNameWithoutExtension() + Str_Text(optionalSuffix) + searchPath.fileNameExtension();

        if(tryFindResource(flags, rclass, searchPath2, foundPath, rnamespace))
            return true;
    }

    // Try without a suffix.
    return tryFindResource(flags, rclass, searchPath, foundPath, rnamespace);
}

static int findResource(resourceclass_t rclass, uri_s const* const* list,
    ddstring_t* foundPath, int flags, ddstring_t const* optionalSuffix)
{
    DENG_ASSERT(list && (rclass == RC_UNKNOWN || VALID_RESOURCE_CLASS(rclass)));

    LOG_AS("findResource");

    uint result = 0, n = 1;
    for(uri_s const* const* ptr = list; *ptr; ptr++, n++)
    {
        de::Uri const& searchPath = reinterpret_cast<de::Uri const&>(**ptr);

        try
        {
            String const& resolvedPath = searchPath.resolved();

            // If this is an absolute path, locate using it.
            if(QDir::isAbsolutePath(resolvedPath))
            {
                if(findResource2(rclass, resolvedPath, foundPath, flags, optionalSuffix, NULL/*no namespace*/))
                    result = n;
            }
            // Probably a relative path. Has a namespace identifier been included?
            else if(!Str_IsEmpty(searchPath.scheme()))
            {
                resourcenamespaceid_t rni = F_ResourceNamespaceForName(Str_Text(searchPath.scheme()));
                ResourceNamespaceInfo* rnamespaceInfo = getNamespaceInfoForId(rni);
                if(findResource2(rclass, resolvedPath, foundPath, flags, optionalSuffix, rnamespaceInfo->rnamespace))
                    result = n;
            }
        }
        catch(de::Uri::ResolveError const& er)
        {
            LOG_DEBUG(er.asText());
            // Ignore incomplete paths.
        }

        if(result) break;
    }
    return result;
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
        if(verbose >= 1) \
            Con_Message(" %i: %s\n", n, F_PrettyPath(Str_Text(pathCopy))); \
        doomWadPaths = (ddstring_t**) M_Realloc(doomWadPaths, sizeof(*doomWadPaths) * ++doomWadPathsCount); \
        doomWadPaths[doomWadPathsCount-1] = pathCopy; \
    } \
}

        ddstring_t fullString; Str_Init(&fullString);

        Str_Set(&fullString, getenv("DOOMWADPATH"));
        Str_Strip(&fullString);
        if(!Str_IsEmpty(&fullString))
        {
            VERBOSE( Con_Message("Using DOOMWADPATH:\n") )

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
        doomWadDir = Str_New(); Str_Set(doomWadDir, getenv("DOOMWADDIR"));
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
            VERBOSE( Con_Message("Using DOOMWADDIR: %s\n", F_PrettyPath(Str_Text(doomWadDir))) )
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
    if(doomWadPaths != 0)
    {
        for(uint i = 0; i < doomWadPathsCount; ++i)
        {
            searchPaths[idx++] = new de::Uri(Str_Text(doomWadPaths[i]), RC_NULL);
            Str_Delete(doomWadPaths[i]);
        }
        M_Free(doomWadPaths);
    }

    // Add the path from the DOOMWADDIR environment variable.
    if(doomWadDir != 0)
    {
        searchPaths[idx++] = new de::Uri(Str_Text(doomWadDir), RC_NULL);
        Str_Delete(doomWadDir);
    }

    ResourceNamespace* rnamespace = F_CreateResourceNamespace(PACKAGES_RESOURCE_NAMESPACE_NAME, 0);
    if(searchPathsCount != 0)
    {
        for(uint i = 0; i < searchPathsCount; ++i)
        {
            rnamespace->addSearchPath(ResourceNamespace::DefaultPaths, *searchPaths[i], SPF_NO_DESCEND);
        }
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
        byte flags; /// @see resourceNamespaceFlags
        int searchPathFlags; /// @see searchPathFlags
        /// Priority is right to left.
        char const* searchPaths[NAMESPACEDEF_MAX_SEARCHPATHS];
    } defs[] = {
        { DEFINITIONS_RESOURCE_NAMESPACE_NAME,  NULL,           NULL,           0, 0,
            { "$(App.DefsPath)/", "$(App.DefsPath)/$(GamePlugin.Name)/", "$(App.DefsPath)/$(GamePlugin.Name)/$(Game.IdentityKey)/" }
        },
        { GRAPHICS_RESOURCE_NAMESPACE_NAME,     "-gfxdir2",     "-gfxdir",      0, 0,
            { "$(App.DataPath)/graphics/" }
        },
        { MODELS_RESOURCE_NAMESPACE_NAME,       "-modeldir2",   "-modeldir",    RNF_USE_VMAP, 0,
            { "$(App.DataPath)/$(GamePlugin.Name)/models/", "$(App.DataPath)/$(GamePlugin.Name)/models/$(Game.IdentityKey)/" }
        },
        { SOUNDS_RESOURCE_NAMESPACE_NAME,       "-sfxdir2",     "-sfxdir",      RNF_USE_VMAP, SPF_NO_DESCEND,
            { "$(App.DataPath)/$(GamePlugin.Name)/sfx/", "$(App.DataPath)/$(GamePlugin.Name)/sfx/$(Game.IdentityKey)/" }
        },
        { MUSIC_RESOURCE_NAMESPACE_NAME,        "-musdir2",     "-musdir",      RNF_USE_VMAP, SPF_NO_DESCEND,
            { "$(App.DataPath)/$(GamePlugin.Name)/music/", "$(App.DataPath)/$(GamePlugin.Name)/music/$(Game.IdentityKey)/" }
        },
        { TEXTURES_RESOURCE_NAMESPACE_NAME,     "-texdir2",     "-texdir",      RNF_USE_VMAP, SPF_NO_DESCEND,
            { "$(App.DataPath)/$(GamePlugin.Name)/textures/", "$(App.DataPath)/$(GamePlugin.Name)/textures/$(Game.IdentityKey)/" }
        },
        { FLATS_RESOURCE_NAMESPACE_NAME,        "-flatdir2",    "-flatdir",     RNF_USE_VMAP, SPF_NO_DESCEND,
            { "$(App.DataPath)/$(GamePlugin.Name)/flats/", "$(App.DataPath)/$(GamePlugin.Name)/flats/$(Game.IdentityKey)/" }
        },
        { PATCHES_RESOURCE_NAMESPACE_NAME,      "-patdir2",     "-patdir",      RNF_USE_VMAP, SPF_NO_DESCEND,
            { "$(App.DataPath)/$(GamePlugin.Name)/patches/", "$(App.DataPath)/$(GamePlugin.Name)/patches/$(Game.IdentityKey)/" }
        },
        { LIGHTMAPS_RESOURCE_NAMESPACE_NAME,    "-lmdir2",      "-lmdir",       RNF_USE_VMAP, 0,
            { "$(App.DataPath)/$(GamePlugin.Name)/lightmaps/" }
        },
        { FONTS_RESOURCE_NAMESPACE_NAME,        "-fontdir2",    "-fontdir",     RNF_USE_VMAP, SPF_NO_DESCEND,
            { "$(App.DataPath)/fonts/", "$(App.DataPath)/$(GamePlugin.Name)/fonts/", "$(App.DataPath)/$(GamePlugin.Name)/fonts/$(Game.IdentityKey)/" }
        },
    { 0, 0, 0, 0, 0, { 0 } }
    };

    // Setup of the Packages namespace is somewhat more involved...
    createPackagesResourceNamespace();

    // Setup the rest...
    for(size_t i = 0; defs[i].name; ++i)
    {
        uint j, searchPathCount;
        struct namespacedef_s* def = &defs[i];
        ResourceNamespace* rnamespace = F_CreateResourceNamespace(def->name, def->flags);

        searchPathCount = 0;
        while(def->searchPaths[searchPathCount] && ++searchPathCount < NAMESPACEDEF_MAX_SEARCHPATHS)
        {}

        for(j = 0; j < searchPathCount; ++j)
        {
            de::Uri uri = de::Uri(def->searchPaths[j], RC_NULL);
            rnamespace->addSearchPath(ResourceNamespace::DefaultPaths, uri, def->searchPathFlags);
        }

        if(def->optOverridePath && CommandLine_CheckWith(def->optOverridePath, 1))
        {
            String path = QDir::fromNativeSeparators(NativePath(CommandLine_NextAsPath()).expand());

            de::Uri uri2 = de::Uri(path / "$(Game.IdentityKey)/", RC_NULL);
            rnamespace->addSearchPath(ResourceNamespace::OverridePaths, uri2, def->searchPathFlags);

            de::Uri uri = de::Uri(path, RC_NULL);
            rnamespace->addSearchPath(ResourceNamespace::OverridePaths, uri, def->searchPathFlags);
        }

        if(def->optFallbackPath && CommandLine_CheckWith(def->optFallbackPath, 1))
        {
            String path = QDir::fromNativeSeparators(NativePath(CommandLine_NextAsPath()).expand());
            de::Uri uri = de::Uri(path, RC_NULL);
            rnamespace->addSearchPath(ResourceNamespace::FallbackPaths, uri, def->searchPathFlags);
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
    destroyAllNamespaces();
    inited = false;
}

void F_ResetAllResourceNamespaces(void)
{
    errorIfNotInited("F_ResetAllResourceNamespaces");
    resetAllNamespaces();
}

void F_ResetResourceNamespace(resourcenamespaceid_t rni)
{
    if(!F_IsValidResourceNamespaceId(rni)) return;

    ResourceNamespaceInfo* info = getNamespaceInfoForId(rni);
    ResourceNamespace& rnamespace = *info->rnamespace;
    rnamespace.clearSearchPaths(ResourceNamespace::ExtraPaths);
    rnamespace.clear();
}

ResourceNamespace* F_ToResourceNamespace(resourcenamespaceid_t rni)
{
    return getNamespaceInfoForId(rni)->rnamespace;
}

resourcenamespaceid_t F_SafeResourceNamespaceForName(char const* name)
{
    errorIfNotInited("F_SafeResourceNamespaceForName");
    return findNamespaceForName(name);
}

resourcenamespaceid_t F_ResourceNamespaceForName(char const* name)
{
    resourcenamespaceid_t result = F_SafeResourceNamespaceForName(name);
    if(result == 0)
        Con_Error("F_ResourceNamespaceForName: Failed to locate resource namespace \"%s\".", name);
    return result;
}

uint F_NumResourceNamespaces(void)
{
    errorIfNotInited("F_NumResourceNamespaces");
    return numNamespaces;
}

boolean F_IsValidResourceNamespaceId(int val)
{
    errorIfNotInited("F_IsValidResourceNamespaceId");
    return (boolean)(val > 0 && (unsigned)val < (F_NumResourceNamespaces() + 1)? 1 : 0);
}

ResourceNamespace* F_CreateResourceNamespace(char const* name,  byte flags)
{
    DENG_ASSERT(name);
    errorIfNotInited("F_CreateResourceNamespace");

    if(qstrlen(name) < RESOURCENAMESPACE_MINNAMELENGTH)
        Con_Error("F_CreateResourceNamespace: Invalid name '%s' (min length:%i)", name, (int)RESOURCENAMESPACE_MINNAMELENGTH);

    ResourceNamespace* rn = new ResourceNamespace(String(name));

    // Add this new namespace to the global list.
    namespaces = (ResourceNamespaceInfo*) M_Realloc(namespaces, sizeof *namespaces * ++numNamespaces);
    if(!namespaces) Con_Error("F_CreateResourceNamespace: Failed on (re)allocation of %lu bytes for new resource namespace\n", (unsigned long) sizeof *namespaces * numNamespaces);
    ResourceNamespaceInfo* info = &namespaces[numNamespaces-1];

    info->rnamespace = rn;
    info->flags = flags;

    return rn;
}

boolean F_AddExtraSearchPathToResourceNamespace(resourcenamespaceid_t rni, int flags,
    uri_s const* searchPath)
{
    if(!searchPath) return false;
    errorIfNotInited("F_AddSearchPathToResourceNamespace");

    ResourceNamespaceInfo* info = getNamespaceInfoForId(rni);
    ResourceNamespace& rnamespace = *info->rnamespace;
    return rnamespace.addSearchPath(ResourceNamespace::ExtraPaths, reinterpret_cast<de::Uri const&>(*searchPath), flags);
}

uri_s** F_CreateUriList2(resourceclass_t rclass, char const* nativeSearchPaths, int* count)
{
    if(!nativeSearchPaths || !nativeSearchPaths[0])
    {
        if(count) *count = 0;
        return 0;
    }

    int const FIXEDSIZE = 8;
    uri_s** list = 0, *localFixedList[FIXEDSIZE];

    ddstring_t buf; Str_Init(&buf);
    int numPaths = 0, n = 0;
    char const* p = nativeSearchPaths;
    do
    {
        if(numPaths)
        {
            // Prepare for another round.
            Str_Clear(&buf);
        }

        for(; *p && *p != PATH_DELIMIT_CHAR; ++p)
        {
            Str_PartAppend(&buf, p, 0, 1);
        }

        // Skip past the delimiter if present.
        if(*p) ++p;

        if(0 != Str_Length(&buf))
        {
            // A new path was parsed; add it to the list.
            if(n == FIXEDSIZE)
            {
                list = (uri_s**) M_Realloc(list, sizeof(*list) * (numPaths + 1));
                memcpy(list + (numPaths - FIXEDSIZE), localFixedList, sizeof(*list) * FIXEDSIZE);
                n = 0;
            }

            String path = QDir::fromNativeSeparators(String(Str_Text(&buf)));
            localFixedList[n++] = reinterpret_cast<uri_s*>(new de::Uri(path, rclass));
            ++numPaths;
        }
    } while(*p);

    if(numPaths <= FIXEDSIZE)
    {
        list = (uri_s**) M_Malloc(sizeof(*list) * (numPaths + 1));
        memcpy(list, localFixedList, sizeof(*list) * numPaths);
    }
    else if(n > 1)
    {
        list = (uri_s**) M_Realloc(list, sizeof(*list) * (numPaths + 1));
        memcpy(list + numPaths - n, localFixedList, sizeof(*list) * n);
    }
    else
    {
        list[numPaths-1] = localFixedList[0];
    }
    list[numPaths] = 0; // Terminate.

    Str_Free(&buf);

    if(count) *count = numPaths;
    return list;
}

uri_s** F_CreateUriList(resourceclass_t rclass, char const* nativeSearchPaths)
{
    return F_CreateUriList2(rclass, nativeSearchPaths, 0);
}

uri_s** F_CreateUriListStr2(resourceclass_t rclass, ddstring_t const* nativeSearchPaths, int* count)
{
    if(!nativeSearchPaths)
    {
        if(count) *count = 0;
        return 0;
    }
    return F_CreateUriList2(rclass, Str_Text(nativeSearchPaths), count);
}

uri_s** F_CreateUriListStr(resourceclass_t rclass, ddstring_t const* nativeSearchPaths)
{
    return F_CreateUriListStr2(rclass, nativeSearchPaths, 0);
}

void F_DestroyUriList(uri_s** list)
{
    if(!list) return;

    for(uri_s** ptr = list; *ptr; ptr++)
    {
        delete reinterpret_cast<de::Uri*>(*ptr);
    }
    M_Free(list);
}

uint F_FindResource5(resourceclass_t rclass, uri_s const** searchPaths,
    ddstring_t* foundPath, int flags, ddstring_t const* optionalSuffix)
{
    errorIfNotInited("F_FindResource4");
    if(rclass != RC_UNKNOWN && !VALID_RESOURCE_CLASS(rclass))
        Con_Error("F_FindResource: Invalid resource class %i.\n", rclass);
    if(!searchPaths) return 0;
    return findResource(rclass, searchPaths, foundPath, flags, optionalSuffix);
}

uint F_FindResourceStr4(resourceclass_t rclass, ddstring_t const* nativeSearchPaths,
    ddstring_t* foundPath, int flags, ddstring_t const* optionalSuffix)
{
    errorIfNotInited("F_FindResourceStr3");
    if(rclass != RC_UNKNOWN && !VALID_RESOURCE_CLASS(rclass))
        Con_Error("F_FindResource: Invalid resource class %i.\n", rclass);

    if(!nativeSearchPaths || Str_IsEmpty(nativeSearchPaths))
    {
#if _DEBUG
        Con_Message("F_FindResource: Invalid (NULL) search path, returning not-found.\n");
#endif
        return 0;
    }

    uri_s** list = F_CreateUriListStr(rclass, nativeSearchPaths);
    if(!list) return 0;

    int result = findResource(rclass, (uri_s const**)list, foundPath, flags, optionalSuffix);
    F_DestroyUriList(list);
    return result;
}

uint F_FindResourceStr3(resourceclass_t rclass, ddstring_t const* nativeSearchPaths,
    ddstring_t* foundPath, int flags)
{
    return F_FindResourceStr4(rclass, nativeSearchPaths, foundPath, flags, NULL/*no optional suffix*/);
}

uint F_FindResourceStr2(resourceclass_t rclass, ddstring_t const* nativeSearchPath, ddstring_t* foundPath)
{
    return F_FindResourceStr3(rclass, nativeSearchPath, foundPath, RLF_DEFAULT);
}

uint F_FindResourceStr(resourceclass_t rclass, ddstring_t const* nativeSearchPath)
{
    return F_FindResourceStr2(rclass, nativeSearchPath, NULL/*no found path*/);
}

uint F_FindResource4(resourceclass_t rclass, char const* _nativeSearchPaths,
    ddstring_t* foundPath, int flags, char const* _optionalSuffix)
{
    ddstring_t nativeSearchPaths, optionalSuffix;
    bool hasOptionalSuffix = false;
    uint result;
    Str_Init(&nativeSearchPaths); Str_Set(&nativeSearchPaths, _nativeSearchPaths);
    if(_optionalSuffix && _optionalSuffix[0])
    {
        Str_Init(&optionalSuffix); Str_Set(&optionalSuffix, _optionalSuffix);
        hasOptionalSuffix = true;
    }
    result = F_FindResourceStr4(rclass, &nativeSearchPaths, foundPath, flags, hasOptionalSuffix? &optionalSuffix : NULL);
    if(hasOptionalSuffix)
        Str_Free(&optionalSuffix);
    Str_Free(&nativeSearchPaths);
    return result;
}

uint F_FindResource3(resourceclass_t rclass, char const* nativeSearchPaths, ddstring_t* foundPath, int flags)
{
    return F_FindResource4(rclass, nativeSearchPaths, foundPath, flags, NULL/*no optional suffix*/);
}

uint F_FindResource2(resourceclass_t rclass, char const* nativeSearchPaths, ddstring_t* foundPath)
{
    return F_FindResource3(rclass, nativeSearchPaths, foundPath, RLF_DEFAULT);
}

uint F_FindResource(resourceclass_t rclass, char const* nativeSearchPaths)
{
    return F_FindResource2(rclass, nativeSearchPaths, NULL/*no found path*/);
}

resourceclass_t F_DefaultResourceClassForType(resourcetype_t type)
{
    errorIfNotInited("F_DefaultResourceClassForType");
    if(type == RT_NONE)
        return RC_UNKNOWN;
    return getInfoForResourceType(type)->defaultClass;
}

resourcenamespaceid_t F_DefaultResourceNamespaceForClass(resourceclass_t rclass)
{
    DENG_ASSERT(VALID_RESOURCE_CLASS(rclass));
    errorIfNotInited("F_DefaultResourceNamespaceForClass");
    return F_ResourceNamespaceForName(Str_Text(defaultNamespaceForClass[rclass]));
}

resourcetype_t F_GuessResourceTypeByName(char const* path)
{
    if(!path || !path[0])
        return RT_NONE; // Unrecognizable.

    // We require a file extension for this.
    char const* ext = strrchr(path, '.');
    if(ext)
    {
        ++ext;
        for(uint i = RT_FIRST; i < NUM_RESOURCE_TYPES; ++i)
        {
            resourcetypeinfo_t const* info = getInfoForResourceType((resourcetype_t)i);
            // Check the extension.
            if(info->knownFileNameExtensions[0])
            {
                char const* const* cand = info->knownFileNameExtensions;
                do
                {
                    if(!stricmp(*cand, ext))
                    {
                        return (resourcetype_t)i;
                    }
                } while(*(++cand));
            }
        }
    }
    return RT_NONE; // Unrecognizable.
}

boolean F_MapGameResourcePath(resourcenamespaceid_t rni, ddstring_t* path)
{
    if(path && !Str_IsEmpty(path))
    {
        ResourceNamespaceInfo* info = getNamespaceInfoForId(rni);
        ResourceNamespace& rnamespace = *info->rnamespace;
        if(info->flags & RNF_USE_VMAP)
        {
            QByteArray rnamespaceName = rnamespace.name().toUtf8();
            int const nameLen = rnamespaceName.length();
            int const pathLen = Str_Length(path);

            if(nameLen <= pathLen && Str_At(path, nameLen) == '/' &&
               !qstrnicmp(rnamespaceName.constData(), Str_Text(path), nameLen))
            {
                Str_Prepend(path, "$(App.DataPath)/$(GamePlugin.Name)/");
                return true;
            }
        }
    }
    return false;
}

boolean F_ApplyGamePathMapping(ddstring_t* path)
{
    DENG_ASSERT(path);
    errorIfNotInited("F_ApplyGamePathMapping");

    uint i = 1;
    boolean result = false;
    while(i < numNamespaces + 1 && !(result = F_MapGameResourcePath(i++, path)))
    {}
    return result;
}

char const* F_ResourceClassStr(resourceclass_t rclass)
{
    DENG_ASSERT(VALID_RESOURCE_CLASS(rclass));
    static de::Str const resourceClassNames[RESOURCECLASS_COUNT] = {
        "RC_PACKAGE",
        "RC_DEFINITION",
        "RC_GRAPHIC",
        "RC_MODEL",
        "RC_SOUND",
        "RC_MUSIC",
        "RC_FONT"
    };
    return Str_Text(resourceClassNames[(int)rclass]);
}
