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

#ifdef WIN32
#  include <direct.h>
#endif

#ifdef UNIX
#  include <pwd.h>
#  include <limits.h>
#  include <unistd.h>
#  include <sys/stat.h>
#endif

#include <cctype>

#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"
#include "abstractresource.h"
#include "resourcenamespace.h"

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
 * @defgroup ResourceNamespaceFlags Resource Namespace Flags
 * @ingroup core
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

static resourcenamespaceid_t findNamespaceForName(char const* name)
{
    if(name && name[0])
    {
        for(uint i = 0; i < numNamespaces; ++i)
        {
            ResourceNamespaceInfo* info = &namespaces[i];
            if(!stricmp(Str_Text(info->rnamespace->name()), name))
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
 * @param searchPath        Relative or absolute path to the resource.
 * @param searchDelimiter   Fragments of @a searchPath are delimited by this character.
 *
 * @return  The found PathTree node which represents the resource else @c NULL.
 */
static de::PathTree::Node* findResourceInNamespace(ResourceNamespace& rnamespace,
    ddstring_t const* searchPath, char delimiter)
{
    if(!searchPath || Str_IsEmpty(searchPath)) return 0;

    // Ensure the namespace is up to date.
    rnamespace.rebuild();

    // Perform the search.
    de::PathTree::Node* found = 0;
    ResourceNamespace::ResourceList foundResources;
    if(rnamespace.findAll(searchPath, foundResources))
    {
        // There is at least one name-matched (perhaps partially) resource.
        PathMap searchPattern;
        PathMap_Initialize2(&searchPattern, de::PathTree::hashPathFragment, Str_Text(searchPath), delimiter);

        DENG2_FOR_EACH_CONST(ResourceNamespace::ResourceList, i, foundResources)
        {
            de::PathTree::Node* node = *i;
            if(!node->comparePath(&searchPattern, PCF_NO_BRANCH)) continue;

            // This is the resource we are looking for.
            found = node;
            break;
        }

        // Cleanup.
        PathMap_Destroy(&searchPattern);
    }
    return found;
}

static bool tryFindResource2(resourceclass_t rclass, ddstring_t const* rawSearchPath,
    ddstring_t* foundPath, ResourceNamespaceInfo* rnInfo)
{
    DENG_ASSERT(inited && rawSearchPath && !Str_IsEmpty(rawSearchPath));
    DENG_UNUSED(rclass);

    AutoStr* searchPath = AutoStr_NewStd();
    F_FixSlashes(searchPath, rawSearchPath);

    // Is there a namespace we should use?
    if(rnInfo)
    {
        ResourceNamespace& rnamespace = *rnInfo->rnamespace;
        if(de::PathTree::Node* found = findResourceInNamespace(rnamespace, searchPath, '/'))
        {
            // Does the caller want to know the matched path?
            if(foundPath)
            {
                found->composePath(foundPath, NULL, '/');
                F_PrependBasePath(foundPath, foundPath);
            }
            return true;
        }
    }

    if(App_FileSystem()->accessFile(Str_Text(searchPath)))
    {
        if(foundPath) F_PrependBasePath(foundPath, searchPath);
        return true;
    }
    return false;
}

/**
 * @param flags  @see resourceLocationFlags
 */
static bool tryFindResource(int flags, resourceclass_t rclass, ddstring_t const* searchPath,
    ddstring_t* foundPath, ResourceNamespaceInfo* rnamespaceInfo)
{
    DENG_ASSERT(inited && searchPath && !Str_IsEmpty(searchPath));

    // If an extension was specified, first look for resources of the same type.
    char const* ptr = F_FindFileExtension(Str_Text(searchPath));
    if(ptr && *ptr != '*')
    {
        if(tryFindResource2(rclass, searchPath, foundPath, rnamespaceInfo)) return true;

        // If we are looking for a particular resource type, get out of here.
        if(flags & RLF_MATCH_EXTENSION) return false;
    }

    if(!(VALID_RESOURCE_CLASS(rclass) && searchTypeOrder[rclass][0] != RT_NONE)) return false;

    /**
     * Lets try some different name patterns (i.e., resource types) known to us.
     */

    // Create a copy of the searchPath minus file extension.
    ddstring_t path2; Str_Init(&path2);
    Str_Reserve(&path2, Str_Length(searchPath)+1/*period*/);
    if(ptr)
    {
        Str_PartAppend(&path2, Str_Text(searchPath), 0, ptr - Str_Text(searchPath));
    }
    else
    {
        Str_Copy(&path2, searchPath);
        Str_AppendChar(&path2, '.');
    }

    ddstring_t tmp; Str_Init(&tmp);
    Str_Reserve(&tmp, Str_Length(&path2) +5/*max expected extension length*/);
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
                Str_Clear(&tmp);
                Str_Appendf(&tmp, "%s%s", Str_Text(&path2), *ext);
                found = tryFindResource2(rclass, &tmp, foundPath, rnamespaceInfo);
            } while(!found && *(++ext));
        }
    } while(!found && *(++typeIter) != RT_NONE);

    Str_Free(&tmp);
    Str_Free(&path2);
    return found;
}

/**
 * @param flags  @see resourceLocationFlags
 */
static bool findResource2(resourceclass_t rclass, ddstring_t const* searchPath,
    ddstring_t* foundPath, int flags, ddstring_t const* optionalSuffix,
    ResourceNamespaceInfo* rnamespaceInfo)
{
    DENG_ASSERT(inited && searchPath && !Str_IsEmpty(searchPath));

#if _DEBUG
    VERBOSE2( Con_Message("Using rnamespace '%s'...\n", rnamespaceInfo? Str_Text(rnamespaceInfo->rnamespace->name()) : "None") )
#endif

    bool found = false;

    // First try with the optional suffix.
    if(optionalSuffix)
    {
        ddstring_t fn; Str_Init(&fn);

        // Has an extension been specified?
        char const* ptr = F_FindFileExtension(Str_Text(searchPath));
        if(ptr && *ptr != '*')
        {
            char ext[10];
            strncpy(ext, ptr, 10);

            Str_PartAppend(&fn, Str_Text(searchPath), 0, ptr - 1 - Str_Text(searchPath));
            Str_Appendf(&fn, "%s%s", Str_Text(optionalSuffix), ext);
        }
        else
        {
            Str_Appendf(&fn, "%s%s", Str_Text(searchPath), Str_Text(optionalSuffix));
        }

        found = tryFindResource(flags, rclass, &fn, foundPath, rnamespaceInfo);
        Str_Free(&fn);
    }

    // Try without a suffix.
    if(!found)
    {
        found = tryFindResource(flags, rclass, searchPath, foundPath, rnamespaceInfo);
    }

    return found;
}

/**
 * @param flags  @see resourceLocationFlags
 */
static int findResource(resourceclass_t rclass, uri_s const* const* list,
    ddstring_t* foundPath, int flags, ddstring_t const* optionalSuffix)
{
    DENG_ASSERT(inited && list && (rclass == RC_UNKNOWN || VALID_RESOURCE_CLASS(rclass)));

    uint result = 0, n = 1;

    for(uri_s const* const* ptr = list; *ptr; ptr++, n++)
    {
        uri_s const* searchPath = *ptr;

        // Ignore incomplete paths.
        ddstring_t const* resolvedPath = Uri_ResolvedConst(searchPath);
        if(!resolvedPath) continue;

        // If this is an absolute path, locate using it.
        if(F_IsAbsolute(resolvedPath))
        {
            if(findResource2(rclass, resolvedPath, foundPath, flags, optionalSuffix, NULL/*no namespace*/))
                result = n;
        }
        else
        {
            // Probably a relative path. Has a namespace identifier been included?
            if(!Str_IsEmpty(Uri_Scheme(searchPath)))
            {
                resourcenamespaceid_t rni = F_SafeResourceNamespaceForName(Str_Text(Uri_Scheme(searchPath)));
                if(rni)
                {
                    ResourceNamespaceInfo* rnamespaceInfo = getNamespaceInfoForId(rni);
                    if(findResource2(rclass, resolvedPath, foundPath, flags, optionalSuffix, rnamespaceInfo))
                        result = n;
                }
#if _DEBUG
                else
                {
                    AutoStr* rawPath = Uri_ToString(searchPath);
                    Con_Message("Warning: findResource: Unknown namespace in searchPath \"%s\", using default for %s.\n",
                                Str_Text(rawPath), F_ResourceClassStr(rclass));
                }
#endif
            }
        }

        if(result) break;
    }
    return result;
}

static void createPackagesResourceNamespace(void)
{
    ddstring_t** doomWadPaths = 0, *doomWadDir = 0;
    uint doomWadPathsCount = 0, searchPathsCount, idx;
    uri_s** searchPaths;

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
    searchPathsCount = 2 + doomWadPathsCount + (doomWadDir != 0? 1 : 0);
    searchPaths = (uri_s**) M_Malloc(sizeof(*searchPaths) * searchPathsCount);
    if(!searchPaths) Con_Error("createPackagesResourceNamespace: Failed on allocation of %lu bytes.", (unsigned long) (sizeof(*searchPaths) * searchPathsCount));

    idx = 0;
    // Add the default paths.
    searchPaths[idx++] = Uri_NewWithPath2("$(App.DataPath)/", RC_NULL);
    searchPaths[idx++] = Uri_NewWithPath2("$(App.DataPath)/$(GamePlugin.Name)/", RC_NULL);

    // Add any paths from the DOOMWADPATH environment variable.
    if(doomWadPaths != 0)
    {
        for(uint i = 0; i < doomWadPathsCount; ++i)
        {
            searchPaths[idx++] = Uri_NewWithPath2(Str_Text(doomWadPaths[i]), RC_NULL);
            Str_Delete(doomWadPaths[i]);
        }
        M_Free(doomWadPaths);
    }

    // Add the path from the DOOMWADDIR environment variable.
    if(doomWadDir != 0)
    {
        searchPaths[idx++] = Uri_NewWithPath2(Str_Text(doomWadDir), RC_NULL);
        Str_Delete(doomWadDir);
    }

    ResourceNamespace* rnamespace = F_CreateResourceNamespace(PACKAGES_RESOURCE_NAMESPACE_NAME, 0);
    if(searchPathsCount != 0)
    {
        for(uint i = 0; i < searchPathsCount; ++i)
        {
            rnamespace->addSearchPath(ResourceNamespace::DefaultPaths, searchPaths[i], SPF_NO_DESCEND);
        }
    }

    for(uint i = 0; i < searchPathsCount; ++i)
    {
        Uri_Delete(searchPaths[i]);
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
    uri_s* uri = Uri_New();

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
            Uri_SetUri2(uri, def->searchPaths[j], RC_NULL);
            rnamespace->addSearchPath(ResourceNamespace::DefaultPaths, uri, def->searchPathFlags);
        }

        if(def->optOverridePath && CommandLine_CheckWith(def->optOverridePath, 1))
        {
            char const* path = CommandLine_NextAsPath();

            QByteArray path2 = String("%1$(Game.IdentityKey)/").arg(path).toUtf8();
            Uri_SetUri2(uri, path2.constData(), RC_NULL);
            rnamespace->addSearchPath(ResourceNamespace::OverridePaths, uri, def->searchPathFlags);

            Uri_SetUri2(uri, path, RC_NULL);
            rnamespace->addSearchPath(ResourceNamespace::OverridePaths, uri, def->searchPathFlags);
        }

        if(def->optFallbackPath && CommandLine_CheckWith(def->optFallbackPath, 1))
        {
            Uri_SetUri2(uri, CommandLine_NextAsPath(), RC_NULL);
            rnamespace->addSearchPath(ResourceNamespace::FallbackPaths, uri, def->searchPathFlags);
        }
    }

    Uri_Delete(uri);

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

    if(strlen(name) < RESOURCENAMESPACE_MINNAMELENGTH)
        Con_Error("F_CreateResourceNamespace: Invalid name '%s' (min length:%i)", name, (int)RESOURCENAMESPACE_MINNAMELENGTH);

    ResourceNamespace* rn = new ResourceNamespace(name);

    // Add this new namespace to the global list.
    namespaces = (ResourceNamespaceInfo*) M_Realloc(namespaces, sizeof *namespaces * ++numNamespaces);
    if(!namespaces)
        Con_Error("F_CreateResourceNamespace: Failed on (re)allocation of %lu bytes for new resource namespace\n",
            (unsigned long) sizeof *namespaces * numNamespaces);
    ResourceNamespaceInfo* info = &namespaces[numNamespaces-1];

    info->rnamespace = rn;
    info->flags = flags;

    return rn;
}

boolean F_AddExtraSearchPathToResourceNamespace(resourcenamespaceid_t rni, int flags,
    uri_s const* searchPath)
{
    errorIfNotInited("F_AddSearchPathToResourceNamespace");

    ResourceNamespaceInfo* info = getNamespaceInfoForId(rni);
    ResourceNamespace& rnamespace = *info->rnamespace;
    return rnamespace.addSearchPath(ResourceNamespace::ExtraPaths, searchPath, flags);
}

ddstring_t const* F_ResourceNamespaceName(resourcenamespaceid_t rni)
{
    return (getNamespaceInfoForId(rni))->rnamespace->name();
}

uri_s** F_CreateUriList2(resourceclass_t rclass, char const* searchPaths, int* count)
{
    if(!searchPaths || !searchPaths[0])
    {
        if(count) *count = 0;
        return 0;
    }

    int const FIXEDSIZE = 8;
    uri_s** list = 0, *localFixedList[FIXEDSIZE];

    ddstring_t buf; Str_Init(&buf);
    int numPaths = 0, n = 0;
    char const* p = searchPaths;
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
            localFixedList[n++] = Uri_NewWithPath2(Str_Text(&buf), rclass);
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

uri_s** F_CreateUriList(resourceclass_t rclass, char const* searchPaths)
{
    return F_CreateUriList2(rclass, searchPaths, 0);
}

uri_s** F_CreateUriListStr2(resourceclass_t rclass, ddstring_t const* searchPaths, int* count)
{
    if(!searchPaths)
    {
        if(count) *count = 0;
        return 0;
    }
    return F_CreateUriList2(rclass, Str_Text(searchPaths), count);
}

uri_s** F_CreateUriListStr(resourceclass_t rclass, ddstring_t const* searchPaths)
{
    return F_CreateUriListStr2(rclass, searchPaths, 0);
}

void F_DestroyUriList(uri_s** list)
{
    if(!list) return;

    for(uri_s** ptr = list; *ptr; ptr++)
    {
        Uri_Delete(*ptr);
    }
    M_Free(list);
}

ddstring_t** F_ResolvePathList2(resourceclass_t /*defaultResourceClass*/,
    ddstring_t const* pathList, size_t* count, char /*delimiter*/)
{
    uri_s** uriList = F_CreateUriListStr(RC_NULL, pathList);
    size_t resolvedPathCount = 0;
    ddstring_t** paths = NULL;

    if(uriList)
    {
        for(uri_s** uriIt = uriList; *uriIt; ++uriIt)
        {
            // Ignore incomplete paths.
            ddstring_t const* resolvedPath = Uri_ResolvedConst(*uriIt);
            if(!resolvedPath) continue;

            ++resolvedPathCount;
        }

        if(resolvedPathCount)
        {
            paths = (ddstring_t**) M_Malloc(sizeof(*paths) * (resolvedPathCount+1));
            if(!paths) Con_Error("F_ResolvePathList: Failed on allocation of %lu bytes for new path list.", (unsigned long) sizeof(*paths) * (resolvedPathCount+1));

            uint n = 0;
            for(uri_s** uriIt = uriList; *uriIt; ++uriIt)
            {
                // Ignore incomplete paths.
                ddstring_t* resolvedPath = Uri_Resolved(*uriIt);
                if(!resolvedPath) continue;

                paths[n++] = resolvedPath;
            }
            paths[n] = NULL; // Terminate.
        }

        F_DestroyUriList(uriList);
    }

    if(count) *count = resolvedPathCount;
    return paths;
}

ddstring_t** F_ResolvePathList(resourceclass_t defaultResourceClass,
    ddstring_t const* pathList, size_t* count)
{
    return F_ResolvePathList2(defaultResourceClass, pathList, count, PATH_DELIMIT_CHAR);
}

void F_DestroyStringList(ddstring_t** list)
{
    if(!list) return;

    for(ddstring_t** ptr = list; *ptr; ptr++)
    {
        Str_Delete(*ptr);
    }
    M_Free(list);
}

#if _DEBUG
void F_PrintStringList(ddstring_t const** strings, size_t stringsCount)
{
    if(!strings || stringsCount == 0) return;

    ddstring_t const** ptr = strings;
    for(size_t i = 0; i < stringsCount && *ptr; ++i, ptr++)
    {
        Con_Printf("  \"%s\"\n", Str_Text(*ptr));
    }
}
#endif

uint F_FindResource5(resourceclass_t rclass, uri_s const** searchPaths,
    ddstring_t* foundPath, int flags, ddstring_t const* optionalSuffix)
{
    errorIfNotInited("F_FindResource4");
    if(rclass != RC_UNKNOWN && !VALID_RESOURCE_CLASS(rclass))
        Con_Error("F_FindResource: Invalid resource class %i.\n", rclass);
    if(!searchPaths) return 0;
    return findResource(rclass, searchPaths, foundPath, flags, optionalSuffix);
}

uint F_FindResourceStr4(resourceclass_t rclass, ddstring_t const* searchPaths,
    ddstring_t* foundPath, int flags, ddstring_t const* optionalSuffix)
{
    errorIfNotInited("F_FindResourceStr3");
    if(rclass != RC_UNKNOWN && !VALID_RESOURCE_CLASS(rclass))
        Con_Error("F_FindResource: Invalid resource class %i.\n", rclass);

    if(!searchPaths || Str_IsEmpty(searchPaths))
    {
#if _DEBUG
        Con_Message("F_FindResource: Invalid (NULL) search path, returning not-found.\n");
#endif
        return 0;
    }

    uri_s** list = F_CreateUriListStr(rclass, searchPaths);
    if(!list) return 0;

    int result = findResource(rclass, (uri_s const**)list, foundPath, flags, optionalSuffix);
    F_DestroyUriList(list);
    return result;
}

uint F_FindResourceForRecord2(AbstractResource* rec, ddstring_t* foundPath, uri_s const* const* searchPaths)
{
    return findResource(AbstractResource_ResourceClass(rec),
                        searchPaths, foundPath, RLF_DEFAULT, NULL/*no optional suffix*/);
}

uint F_FindResourceForRecord(AbstractResource* rec, ddstring_t* foundPath)
{
    return F_FindResourceForRecord2(rec, foundPath,
                                    (uri_s const* const*) AbstractResource_SearchPaths(rec));
}

uint F_FindResourceStr3(resourceclass_t rclass, ddstring_t const* searchPaths,
    ddstring_t* foundPath, int flags)
{
    return F_FindResourceStr4(rclass, searchPaths, foundPath, flags, NULL/*no optional suffix*/);
}

uint F_FindResourceStr2(resourceclass_t rclass, ddstring_t const* searchPath, ddstring_t* foundPath)
{
    return F_FindResourceStr3(rclass, searchPath, foundPath, RLF_DEFAULT);
}

uint F_FindResourceStr(resourceclass_t rclass, ddstring_t const* searchPath)
{
    return F_FindResourceStr2(rclass, searchPath, NULL/*no found path*/);
}

uint F_FindResource4(resourceclass_t rclass, char const* _searchPaths,
    ddstring_t* foundPath, int flags, char const* _optionalSuffix)
{
    ddstring_t searchPaths, optionalSuffix;
    bool hasOptionalSuffix = false;
    uint result;
    Str_Init(&searchPaths); Str_Set(&searchPaths, _searchPaths);
    if(_optionalSuffix && _optionalSuffix[0])
    {
        Str_Init(&optionalSuffix); Str_Set(&optionalSuffix, _optionalSuffix);
        hasOptionalSuffix = true;
    }
    result = F_FindResourceStr4(rclass, &searchPaths, foundPath, flags, hasOptionalSuffix? &optionalSuffix : NULL);
    if(hasOptionalSuffix)
        Str_Free(&optionalSuffix);
    Str_Free(&searchPaths);
    return result;
}

uint F_FindResource3(resourceclass_t rclass, char const* searchPaths, ddstring_t* foundPath, int flags)
{
    return F_FindResource4(rclass, searchPaths, foundPath, flags, NULL/*no optional suffix*/);
}

uint F_FindResource2(resourceclass_t rclass, char const* searchPaths, ddstring_t* foundPath)
{
    return F_FindResource3(rclass, searchPaths, foundPath, RLF_DEFAULT);
}

uint F_FindResource(resourceclass_t rclass, char const* searchPaths)
{
    return F_FindResource2(rclass, searchPaths, NULL/*no found path*/);
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
            int const nameLen = Str_Length(rnamespace.name());
            int const pathLen = Str_Length(path);

            if(nameLen <= pathLen && Str_At(path, nameLen) == '/' &&
               !strnicmp(Str_Text(rnamespace.name()), Str_Text(path), nameLen))
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

char const* F_ParseSearchPath2(uri_s* dst, char const* src, char delim,
    resourceclass_t defaultResourceClass)
{
    Uri_Clear(dst);
    if(src)
    {
        ddstring_t buf; Str_Init(&buf);
        for(; *src && *src != delim; ++src)
        {
            Str_PartAppend(&buf, src, 0, 1);
        }
        Uri_SetUri2(dst, Str_Text(&buf), defaultResourceClass);
        Str_Free(&buf);
    }
    if(!*src)
        return 0; // It ended.
    // Skip past the delimiter.
    return src + 1;
}

char const* F_ParseSearchPath(uri_s* dst, char const* src, char delim)
{
    return F_ParseSearchPath2(dst, src, delim, RC_UNKNOWN);
}
