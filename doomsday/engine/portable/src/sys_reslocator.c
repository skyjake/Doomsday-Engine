/**\file sys_reslocator.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
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

#include <ctype.h>

#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"
#include "m_misc.h"

#include "filedirectory.h"
#include "abstractresource.h"
#include "resourcenamespace.h"

#define PATH_DELIMIT_CHAR       ';'
#define PATH_DELIMIT_STR        ";"

#define MAX_EXTENSIONS          (3)
typedef struct {
    /// Default class attributed to resources of this type.
    resourceclass_t defaultClass;
    char* knownFileNameExtensions[MAX_EXTENSIONS];
} resourcetypeinfo_t;

/**
 * @defgroup ResourceNamespaceFlags Resource Namespace Flags
 * @ingroup core.
 */
/*@{*/
#define RNF_USE_VMAP            0x01 // Map resources in packages.
#define RNF_IS_DIRTY            0x80 // Filehash needs to be (re)built (avoid allocating an empty name hash).
/*@}*/

#define RESOURCENAMESPACE_MINNAMELENGTH    URI_MINSCHEMELENGTH

typedef struct {
    /// Unique symbolic name of this namespace (e.g., "Models").
    /// Must be at least @c RESOURCENAMESPACE_MINNAMELENGTH characters long.
    ddstring_t name;

    /// ResourceNamespace.
    resourcenamespace_t* rnamespace;

    /// Associated path directory for this namespace.
    FileDirectory* directory;

    /// Algorithm used to compose the name of a resource in this namespace.
    ddstring_t* (*composeName) (const ddstring_t* path);

    byte flags; // @see resourceNamespaceFlags
} resourcenamespaceinfo_t;

static boolean inited = false;

static const resourcetypeinfo_t typeInfo[NUM_RESOURCE_TYPES] = {
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
static const resourcetype_t searchTypeOrder[RESOURCECLASS_COUNT][MAX_TYPEORDER] = {
    /* RC_PACKAGE */    { RT_ZIP, RT_WAD, 0 }, // Favor ZIP over WAD.
    /* RC_DEFINITION */ { RT_DED, 0 }, // Only DED files.
    /* RC_GRAPHIC */    { RT_PNG, RT_TGA, RT_JPG, RT_PCX, 0 }, // Favour quality.
    /* RC_MODEL */      { RT_DMD, RT_MD2, 0 }, // Favour DMD over MD2.
    /* RC_SOUND */      { RT_WAV, 0 }, // Only WAV files.
    /* RC_MUSIC */      { RT_OGG, RT_MP3, RT_WAV, RT_MOD, RT_MID, 0 },
    /* RC_FONT */       { RT_DFN, 0 } // Only DFN fonts.
};

static const ddstring_t defaultNamespaceForClass[RESOURCECLASS_COUNT] = {
    /* RC_PACKAGE */    { PACKAGES_RESOURCE_NAMESPACE_NAME },
    /* RC_DEFINITION */ { DEFINITIONS_RESOURCE_NAMESPACE_NAME },
    /* RC_GRAPHIC */    { GRAPHICS_RESOURCE_NAMESPACE_NAME },
    /* RC_MODEL */      { MODELS_RESOURCE_NAMESPACE_NAME },
    /* RC_SOUND */      { SOUNDS_RESOURCE_NAMESPACE_NAME },
    /* RC_MUSIC */      { MUSIC_RESOURCE_NAMESPACE_NAME },
    /* RC_FONT */       { FONTS_RESOURCE_NAMESPACE_NAME }
};

static resourcenamespaceinfo_t* namespaces = 0;
static uint numNamespaces = 0;

static void errorIfNotInited(const char* callerName)
{
    if(inited) return;
    Con_Error("%s: resource locator module is not presently initialized.", callerName);
    // Unreachable. Prevents static analysers from getting rather confused, poor things.
    exit(1);
}

static __inline const resourcetypeinfo_t* getInfoForResourceType(resourcetype_t type)
{
    assert(VALID_RESOURCE_TYPE(type));
    return &typeInfo[((uint)type)-1];
}

static __inline resourcenamespaceinfo_t* getNamespaceInfoForId(resourcenamespaceid_t rni)
{
    errorIfNotInited("getNamespaceForId");
    if(!F_IsValidResourceNamespaceId(rni))
        Con_Error("getNamespaceForId: Invalid namespace id %i.", (int)rni);
    return &namespaces[((uint)rni)-1];
}

static resourcenamespaceid_t findNamespaceId(resourcenamespace_t* rnamespace)
{
    if(rnamespace)
    {
        uint i;
        for(i = 0; i < numNamespaces; ++i)
        {
            resourcenamespaceinfo_t* info = &namespaces[i];
            if(info->rnamespace == rnamespace)
                return (resourcenamespaceid_t)(i+1);
        }
    }
    return 0;
}

static resourcenamespaceid_t findNamespaceForName(const char* name)
{
    if(name && name[0])
    {
        uint i;
        for(i = 0; i < numNamespaces; ++i)
        {
            resourcenamespaceinfo_t* info = &namespaces[i];
            if(!stricmp(Str_Text(&info->name), name))
                return (resourcenamespaceid_t)(i+1);
        }
    }
    return 0;
}

static void destroyAllNamespaces(void)
{
    uint i;
    if(numNamespaces == 0) return;

    for(i = 0; i < numNamespaces; ++i)
    {
        resourcenamespaceinfo_t* info = &namespaces[i];
        if(info->directory) FileDirectory_Delete(info->directory);
        ResourceNamespace_Delete(info->rnamespace);
        Str_Free(&info->name);
    }
    free(namespaces);
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

static void addResourceToNamespace(resourcenamespaceinfo_t* rnInfo,
    PathDirectoryNode* node)
{
    ddstring_t* name;
    assert(rnInfo && node);

    name = rnInfo->composeName(PathDirectory_GetFragment(PathDirectoryNode_Directory(node), node));
    if(ResourceNamespace_Add(rnInfo->rnamespace, name, node, NULL))
    {
        // We will need to rebuild this namespace (if we aren't already doing so,
        // in the case of auto-populated namespaces built from FileDirectorys).
        rnInfo->flags |= RNF_IS_DIRTY;
    }
    Str_Delete(name);
}

static int addFileResourceWorker(PathDirectoryNode* node, void* paramaters)
{
    resourcenamespaceinfo_t* rnInfo = (resourcenamespaceinfo_t*) paramaters;
    // We are only interested in leafs (i.e., files and not directories).
    if(PathDirectoryNode_Type(node) == PT_LEAF)
    {
        addResourceToNamespace(rnInfo, node);
    }
    return 0; // Continue adding.
}

static int rebuildResourceNamespaceWorker(const Uri* searchPath, int flags,
    void* paramaters)
{
    resourcenamespaceinfo_t* rnInfo = (resourcenamespaceinfo_t*)paramaters;
    FileDirectory_AddPaths3(rnInfo->directory, flags, &searchPath, 1,
                            addFileResourceWorker, (void*)rnInfo);
    return 0; // Continue iteration.
}

static void rebuildResourceNamespace(resourcenamespaceinfo_t* rnInfo)
{
/*#if _DEBUG
    uint startTime;
#endif*/

    assert(rnInfo);
    if(!(rnInfo->flags & RNF_IS_DIRTY)) return;

/*#if _DEBUG
    VERBOSE( Con_Message("Rebuilding rnamespace '%s'...\n", Str_Text(&rnInfo->name)) )
    VERBOSE2( startTime = Sys_GetRealTime() )
#endif*/

    // (Re)populate the directory and insert found paths into the resource namespace.
    /// @todo It should not be necessary for a unique directory per namespace.

    ResourceNamespace_Clear(rnInfo->rnamespace);
    FileDirectory_Clear(rnInfo->directory);

/*#if _DEBUG
    VERBOSE2(
        ddstring_t* searchPathList = ResourceNamespace_ComposeSearchPathList(rnInfo->rnamespace);
        Con_PrintPathList(Str_Text(searchPathList));
        Str_Delete(searchPathList)
        )
#endif*/

    ResourceNamespace_IterateSearchPaths2(rnInfo->rnamespace, rebuildResourceNamespaceWorker, (void*)rnInfo);
    rnInfo->flags &= ~RNF_IS_DIRTY;

/*#if _DEBUG
    VERBOSE2( FileDirectory_Print(rnInfo->directory) )
    VERBOSE2( ResourceNamespace_Print(rnInfo->rnamespace) )
    VERBOSE2( Con_Message("  Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) )
#endif*/
}

typedef struct {
    const char* path;
    char delimiter;
    PathMap searchPattern;
    boolean searchInited;
    PathDirectoryNode* foundNode;
} findresourceinnamespaceworker_params_t;

static int findResourceInNamespaceWorker(PathDirectoryNode* node, void* paramaters)
{
    findresourceinnamespaceworker_params_t* p = (findresourceinnamespaceworker_params_t*)paramaters;
    assert(node && p);
    // Are we yet to initialize the search?
    if(!p->searchInited)
    {
        PathMap_Initialize2(&p->searchPattern, p->path, p->delimiter);
        p->searchInited = true;
    }
    // Stop iteration of resources as soon as a match is found.
    if(PathDirectoryNode_MatchDirectory(node, PCF_NO_BRANCH, &p->searchPattern, NULL))
    {
        p->foundNode = node;
        return 1;
    }
    return 0; // Continue iteration.
}

/**
 * Find a named resource in this namespace.
 *
 * @param name  Name of the resource being searched for.
 * @param searchPath  Relative or absolute path to the resource.
 * @param searchDelimiter  Fragments of @a searchPath are delimited by this character.
 * @param foundPath  If not @c NULL and a path is found, it is written back here.
 * @param foundDelimiter  Delimiter to be used when composing @a foundPath.
 * @return  @c true= A resource was found.
 */
static boolean findResourceInNamespace(resourcenamespaceinfo_t* rnInfo, const ddstring_t* name,
    const ddstring_t* searchPath, char delimiter, ddstring_t* foundPath, char foundDelimiter)
{
    boolean found = false;
    assert(rnInfo && name && searchPath);

    if(!Str_IsEmpty(searchPath))
    {
        findresourceinnamespaceworker_params_t p;

        // Ensure the namespace is up to date.
        rebuildResourceNamespace(rnInfo);

        // There may not be any matching named resources, so we defer initialization
        // of the PathMap until the first name-match is found.
        p.path = Str_Text(searchPath);
        p.delimiter = delimiter;
        p.searchInited = false;
        p.foundNode = NULL;

        // Perform the search.
        if(found = ResourceNamespace_Iterate2(rnInfo->rnamespace, name, findResourceInNamespaceWorker, (void*)&p))
        {
            // Does the caller want to know the matched path?
            if(foundPath)
            {
                PathDirectoryNode* node = p.foundNode;
                PathDirectory_ComposePath(PathDirectoryNode_Directory(node), node, foundPath, NULL, foundDelimiter);
            }
        }

        // Cleanup.
        if(p.searchInited) PathMap_Destroy(&p.searchPattern);
    }
    return found;
}

static boolean tryFindResource2(resourceclass_t rclass, const ddstring_t* rawSearchPath,
    ddstring_t* foundPath, resourcenamespaceinfo_t* rnamespaceInfo)
{
    ddstring_t searchPath;
    assert(inited && rawSearchPath && !Str_IsEmpty(rawSearchPath));

    Str_Init(&searchPath);
    F_FixSlashes(&searchPath, rawSearchPath);

    // Is there a namespace we should use?
    if(rnamespaceInfo)
    {
        ddstring_t* name = rnamespaceInfo->composeName(&searchPath);
        if(findResourceInNamespace(rnamespaceInfo, name, &searchPath, '/', foundPath, '/'))
        {
            Str_Free(&searchPath);
            Str_Delete(name);
            if(foundPath) F_PrependBasePath(foundPath, foundPath);
            return true;
        }
        Str_Delete(name);
    }

    if(0 != F_Access(Str_Text(&searchPath)))
    {
        if(foundPath) F_PrependBasePath(foundPath, &searchPath);
        Str_Free(&searchPath);
        return true;
    }
    Str_Free(&searchPath);
    return false;
}

/**
 * @param flags  @see resourceLocationFlags
 */
static boolean tryFindResource(int flags, resourceclass_t rclass, const ddstring_t* searchPath,
    ddstring_t* foundPath, resourcenamespaceinfo_t* rnamespaceInfo)
{
    const resourcetype_t* typeIter;
    ddstring_t path2, tmp;
    boolean found;
    const char* ptr;
    assert(inited && searchPath && !Str_IsEmpty(searchPath));

    // If an extension was specified, first look for resources of the same type.
    ptr = F_FindFileExtension(Str_Text(searchPath));
    if(ptr && *ptr != '*')
    {
        if(tryFindResource2(rclass, searchPath, foundPath, rnamespaceInfo))
            return true;
        // If we are looking for a particular resource type, get out of here.
        if(flags & RLF_MATCH_EXTENSION) return false;
    }

    if(!(VALID_RESOURCE_CLASS(rclass) && searchTypeOrder[rclass][0] != RT_NONE)) return false;

    /**
     * Lets try some different name patterns (i.e., resource types) known to us.
     */

    // Create a copy of the searchPath minus file extension.
    Str_Init(&path2);
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

    Str_Init(&tmp);
    Str_Reserve(&tmp, Str_Length(&path2) +5/*max expected extension length*/);
    found = false;
    typeIter = searchTypeOrder[rclass];
    do
    {
        const resourcetypeinfo_t* typeInfo = getInfoForResourceType(*typeIter);
        if(typeInfo->knownFileNameExtensions[0])
        {
            char* const* ext = typeInfo->knownFileNameExtensions;
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
static boolean findResource2(resourceclass_t rclass, const ddstring_t* searchPath,
    ddstring_t* foundPath, int flags, const ddstring_t* optionalSuffix, resourcenamespaceinfo_t* rnamespaceInfo)
{
    boolean found = false;
    assert(inited && searchPath && !Str_IsEmpty(searchPath));

#if _DEBUG
    VERBOSE2( Con_Message("Using rnamespace '%s'...\n", rnamespaceInfo? Str_Text(&rnamespaceInfo->name) : "None") )
#endif

    // First try with the optional suffix.
    if(optionalSuffix)
    {
        const char* ptr;
        ddstring_t fn;
        Str_Init(&fn);

        // Has an extension been specified?
        ptr = F_FindFileExtension(Str_Text(searchPath));
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
        found = tryFindResource(flags, rclass, searchPath, foundPath, rnamespaceInfo);

    return found;
}

/**
 * @param flags  @see resourceLocationFlags
 */
static int findResource(resourceclass_t rclass, const Uri* const* list,
    ddstring_t* foundPath, int flags, const ddstring_t* optionalSuffix)
{
    uint result = 0, n = 1;
    const Uri* const* ptr;
    assert(inited && list && (rclass == RC_UNKNOWN || VALID_RESOURCE_CLASS(rclass)));

    for(ptr = list; *ptr; ptr++, n++)
    {
        const Uri* searchPath = *ptr;
        ddstring_t* resolvedPath;

        // Ignore incomplete paths.
        resolvedPath = Uri_Resolved(searchPath);
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
                    resourcenamespaceinfo_t* rnamespaceInfo = getNamespaceInfoForId(rni);
                    if(findResource2(rclass, resolvedPath, foundPath, flags, optionalSuffix, rnamespaceInfo))
                        result = n;
                }
#if _DEBUG
                else
                {
                    ddstring_t* rawPath = Uri_ToString(searchPath);
                    Con_Message("Warning:findResource: Unknown namespace in searchPath \"%s\", using default for %s.\n", Str_Text(rawPath), F_ResourceClassStr(rclass));
                    Str_Delete(rawPath);
                }
#endif
            }
        }
        Str_Delete(resolvedPath);

        if(result) break;
    }
    return result;
}

ddstring_t* F_ComposeHashNameForFilePath(const ddstring_t* filePath)
{
    ddstring_t* hashName = Str_New();
    F_FileName(hashName, Str_Text(filePath));
    return hashName;
}

resourcenamespace_namehash_key_t F_HashKeyForAlphaNumericNameIgnoreCase(const ddstring_t* name)
{
    resourcenamespace_namehash_key_t key = 0;
    byte op = 0;
    const char* c;
    assert(name);

    for(c = Str_Text(name); *c; c++)
    {
        switch(op)
        {
        case 0: key ^= tolower(*c); ++op;   break;
        case 1: key *= tolower(*c); ++op;   break;
        case 2: key -= tolower(*c);   op=0; break;
        }
    }
    return key % RESOURCENAMESPACE_HASHSIZE;
}

static void createPackagesResourceNamespace(void)
{
    ddstring_t** doomWadPaths = 0, *doomWadDir = 0;
    uint doomWadPathsCount = 0, searchPathsCount, idx;
    resourcenamespace_t* rnamespace;
    FileDirectory* directory;
    Uri** searchPaths;

#ifdef UNIX
    {
        // Check the system-level config files.
        filename_t fn;
        if(DD_Unix_GetConfigValue("paths", "iwaddir", fn, FILENAME_T_MAXLEN))
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
        char last = Str_RAt(path, 0); \
        ddstring_t* pathCopy = Str_New(); \
        F_FixSlashes(pathCopy, path); \
        F_AppendMissingSlash(pathCopy); \
        if(verbose >= 1) \
            Con_Message(" %i: %s\n", n, F_PrettyPath(Str_Text(pathCopy))); \
        doomWadPaths = realloc(doomWadPaths, sizeof(*doomWadPaths) * ++doomWadPathsCount); \
        doomWadPaths[doomWadPathsCount-1] = pathCopy; \
    } \
}

        ddstring_t fullString; Str_Init(&fullString);

        Str_Set(&fullString, getenv("DOOMWADPATH"));
        Str_Strip(&fullString);
        if(!Str_IsEmpty(&fullString))
        {
            ddstring_t path;
            // Split into paths.
            const char* c = Str_Text(&fullString);
            int n;

            VERBOSE( Con_Message("Using DOOMWADPATH:\n") )

            Str_Init(&path);
            n = 0;
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
    if((searchPaths = malloc(sizeof(*searchPaths) * searchPathsCount)) == 0)
        Con_Error("createPackagesResourceNamespace: Failed on allocation of %lu bytes.",
                  (unsigned long) (sizeof(*searchPaths) * searchPathsCount));

    idx = 0;
    // Add the default paths.
    searchPaths[idx++] = Uri_NewWithPath2("$(App.DataPath)/", RC_NULL);
    searchPaths[idx++] = Uri_NewWithPath2("$(Game.DataPath)/", RC_NULL);

    // Add any paths from the DOOMWADPATH environment variable.
    if(doomWadPaths != 0)
    {
        uint i;
        for(i = 0; i < doomWadPathsCount; ++i)
        {
            searchPaths[idx++] = Uri_NewWithPath2(Str_Text(doomWadPaths[i]), RC_NULL);
            Str_Delete(doomWadPaths[i]);
        }
        free(doomWadPaths);
    }

    // Add the path from the DOOMWADDIR environment variable.
    if(doomWadDir != 0)
    {
        searchPaths[idx++] = Uri_NewWithPath2(Str_Text(doomWadDir), RC_NULL);
        Str_Delete(doomWadDir);
    }

    directory = FileDirectory_New(ddBasePath);
    rnamespace = F_CreateResourceNamespace(PACKAGES_RESOURCE_NAMESPACE_NAME, directory,
        F_ComposeHashNameForFilePath, F_HashKeyForFilePathHashName, 0);

    if(searchPathsCount != 0)
    {
        uint i;
        for(i = 0; i < searchPathsCount; ++i)
        {
            ResourceNamespace_AddSearchPath(rnamespace, SPF_NO_DESCEND, searchPaths[i], SPG_DEFAULT);
        }
    }

    for(idx = 0; idx < searchPathsCount; ++idx)
        Uri_Delete(searchPaths[idx]);
    free(searchPaths);
}

void F_CreateNamespacesForFileResourcePaths(void)
{
#define NAMESPACEDEF_MAX_SEARCHPATHS        5

    struct namespacedef_s {
        const char* name;
        const char* optOverridePath;
        const char* optFallbackPath;
        byte flags; /// @see resourceNamespaceFlags
        int searchPathFlags; /// @see searchPathFlags
        /// Priority is right to left.
        const char* searchPaths[NAMESPACEDEF_MAX_SEARCHPATHS];
    } defs[] = {
        { DEFINITIONS_RESOURCE_NAMESPACE_NAME,  NULL,           NULL,           0, 0,
            { "$(App.DefsPath)/", "$(Game.DefsPath)/", "$(Game.DefsPath)/$(Game.IdentityKey)/" }
        },
        { GRAPHICS_RESOURCE_NAMESPACE_NAME,     "-gfxdir2",     "-gfxdir",      0, 0,
            { "$(App.DataPath)/graphics/" }
        },
        { MODELS_RESOURCE_NAMESPACE_NAME,       "-modeldir2",   "-modeldir",    RNF_USE_VMAP, 0,
            { "$(Game.DataPath)/models/", "$(Game.DataPath)/models/$(Game.IdentityKey)/" }
        },
        { SOUNDS_RESOURCE_NAMESPACE_NAME,       "-sfxdir2",     "-sfxdir",      RNF_USE_VMAP, SPF_NO_DESCEND,
            { "$(Game.DataPath)/sfx/", "$(Game.DataPath)/sfx/$(Game.IdentityKey)/" }
        },
        { MUSIC_RESOURCE_NAMESPACE_NAME,        "-musdir2",     "-musdir",      RNF_USE_VMAP, SPF_NO_DESCEND,
            { "$(Game.DataPath)/music/", "$(Game.DataPath)/music/$(Game.IdentityKey)/" }
        },
        { TEXTURES_RESOURCE_NAMESPACE_NAME,     "-texdir2",     "-texdir",      RNF_USE_VMAP, SPF_NO_DESCEND,
            { "$(Game.DataPath)/textures/", "$(Game.DataPath)/textures/$(Game.IdentityKey)/" }
        },
        { FLATS_RESOURCE_NAMESPACE_NAME,        "-flatdir2",    "-flatdir",     RNF_USE_VMAP, SPF_NO_DESCEND,
            { "$(Game.DataPath)/flats/", "$(Game.DataPath)/flats/$(Game.IdentityKey)/" }
        },
        { PATCHES_RESOURCE_NAMESPACE_NAME,      "-patdir2",     "-patdir",      RNF_USE_VMAP, SPF_NO_DESCEND,
            { "$(Game.DataPath)/patches/", "$(Game.DataPath)/patches/$(Game.IdentityKey)/" }
        },
        { LIGHTMAPS_RESOURCE_NAMESPACE_NAME,    "-lmdir2",      "-lmdir",       RNF_USE_VMAP, 0,
            { "$(Game.DataPath)/lightmaps/" }
        },
        { FONTS_RESOURCE_NAMESPACE_NAME,        "-fontdir2",    "-fontdir",     RNF_USE_VMAP, SPF_NO_DESCEND,
            { "$(App.DataPath)/fonts/", "$(Game.DataPath)/fonts/", "$(Game.DataPath)/fonts/$(Game.IdentityKey)/" }
        },
        { NULL }
    };
    Uri* uri = Uri_New();

    // Setup of the Packages namespace is somewhat more involved...
    createPackagesResourceNamespace();

    // Setup the rest...
    { size_t i;
    for(i = 0; defs[i].name; ++i)
    {
        uint j, searchPathCount;
        struct namespacedef_s* def = &defs[i];
        FileDirectory* directory = FileDirectory_New(ddBasePath);
        resourcenamespace_t* rnamespace = F_CreateResourceNamespace(def->name, directory,
            F_ComposeHashNameForFilePath, F_HashKeyForFilePathHashName, def->flags);

        searchPathCount = 0;
        while(def->searchPaths[searchPathCount] && ++searchPathCount < NAMESPACEDEF_MAX_SEARCHPATHS)
        {}

        for(j = 0; j < searchPathCount; ++j)
        {
            Uri_SetUri3(uri, def->searchPaths[j], RC_NULL);
            ResourceNamespace_AddSearchPath(rnamespace, def->searchPathFlags, uri, SPG_DEFAULT);
        }

        if(def->optOverridePath && CommandLine_CheckWith(def->optOverridePath, 1))
        {
            const char* path = CommandLine_NextAsPath();
            ddstring_t path2;

            // Override paths are added in reverse order.
            Str_Init(&path2);
            Str_Appendf(&path2, "%s/$(Game.IdentityKey)", path);
            Uri_SetUri3(uri, Str_Text(&path2), RC_NULL);
            ResourceNamespace_AddSearchPath(rnamespace, def->searchPathFlags, uri, SPG_OVERRIDE);

            Uri_SetUri3(uri, path, RC_NULL);
            ResourceNamespace_AddSearchPath(rnamespace, def->searchPathFlags, uri, SPG_OVERRIDE);

            Str_Free(&path2);
        }

        if(def->optFallbackPath && CommandLine_CheckWith(def->optFallbackPath, 1))
        {
            Uri_SetUri3(uri, CommandLine_NextAsPath(), RC_NULL);
            ResourceNamespace_AddSearchPath(rnamespace, def->searchPathFlags, uri, SPG_FALLBACK);
        }
    }}

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
    if(!inited)
        return;
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
    resourcenamespaceinfo_t* info;
    if(!F_IsValidResourceNamespaceId(rni)) return;

    info = getNamespaceInfoForId(rni);
    ResourceNamespace_ClearSearchPaths(info->rnamespace, SPG_EXTRA);
    ResourceNamespace_Clear(info->rnamespace);
    if(info->directory)
    {
        FileDirectory_Clear(info->directory);
    }
    info->flags |= RNF_IS_DIRTY;
}

resourcenamespace_t* F_ToResourceNamespace(resourcenamespaceid_t rni)
{
    return getNamespaceInfoForId(rni)->rnamespace;
}

resourcenamespaceid_t F_SafeResourceNamespaceForName(const char* name)
{
    errorIfNotInited("F_SafeResourceNamespaceForName");
    return findNamespaceForName(name);
}

resourcenamespaceid_t F_ResourceNamespaceForName(const char* name)
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
    return (boolean)(val>0 && (unsigned)val < (F_NumResourceNamespaces()+1)? 1 : 0);
}

resourcenamespace_t* F_CreateResourceNamespace(const char* name, FileDirectory* directory,
    ddstring_t* (*composeNameFunc) (const ddstring_t* path),
    resourcenamespace_namehash_key_t (*hashNameFunc) (const ddstring_t* name), byte flags)
{
    resourcenamespaceinfo_t* info;
    resourcenamespace_t* rn;

    assert(name && directory && composeNameFunc);
    errorIfNotInited("F_CreateResourceNamespace");

    if(strlen(name) < RESOURCENAMESPACE_MINNAMELENGTH)
        Con_Error("F_CreateResourceNamespace: Invalid name '%s' (min length:%i)",
            name, (int)RESOURCENAMESPACE_MINNAMELENGTH);

    rn = ResourceNamespace_New(hashNameFunc);

    // Add this new namespace to the global list.
    namespaces = (resourcenamespaceinfo_t*) realloc(namespaces, sizeof *namespaces * ++numNamespaces);
    if(!namespaces)
        Con_Error("F_CreateResourceNamespace: Failed on (re)allocation of %lu bytes for new resource namespace\n",
            (unsigned long) sizeof *namespaces * numNamespaces);
    info = &namespaces[numNamespaces-1];

    Str_Init(&info->name);
    Str_Set(&info->name, name);
    info->rnamespace = rn;
    info->directory = directory;
    info->composeName = composeNameFunc;
    info->flags = flags | RNF_IS_DIRTY;

    return rn;
}

boolean F_AddSearchPathToResourceNamespace(resourcenamespaceid_t rni, int flags,
    const Uri* searchPath, resourcenamespace_searchpathgroup_t group)
{
    resourcenamespaceinfo_t* info;
    errorIfNotInited("F_AddSearchPathToResourceNamespace");
    info = getNamespaceInfoForId(rni);
    if(ResourceNamespace_AddSearchPath(info->rnamespace, flags, searchPath, group))
    {
        info->flags |= RNF_IS_DIRTY;
        return true;
    }
    return false;
}

const ddstring_t* F_ResourceNamespaceName(resourcenamespaceid_t rni)
{
    return &(getNamespaceInfoForId(rni))->name;
}

Uri** F_CreateUriList2(resourceclass_t rclass, const char* searchPaths,
    size_t* count)
{
#define FIXEDSIZE           (8)

    Uri** list = 0, *localFixedList[FIXEDSIZE];
    size_t numPaths = 0, n = 0;
    ddstring_t buf;
    const char* p;

    if(!searchPaths || !searchPaths[0])
    {
        if(count)
            *count = 0;
        return 0;
    }

    Str_Init(&buf);
    numPaths = n = 0;
    p = searchPaths;
    do
    {
        if(0 != numPaths)
        {   // Prepare for another round.
            Str_Clear(&buf);
        }

        for(; *p && *p != PATH_DELIMIT_CHAR; ++p)
        {
            Str_PartAppend(&buf, p, 0, 1);
        }

        // Skip past the delimiter if present.
        if(*p) ++p;

        if(0 != Str_Length(&buf))
        {   // A new path was parsed; add it to the list.
            if(n == FIXEDSIZE)
            {
                list = realloc(list, sizeof(*list) * (numPaths + 1));
                memcpy(list + (numPaths - FIXEDSIZE), localFixedList, sizeof(*list) * FIXEDSIZE);
                n = 0;
            }
            localFixedList[n++] = Uri_NewWithPath2(Str_Text(&buf), rclass);
            ++numPaths;
        }
    } while(*p);

    if(numPaths <= FIXEDSIZE)
    {
        list = malloc(sizeof(*list) * (numPaths + 1));
        memcpy(list, localFixedList, sizeof(*list) * numPaths);
    }
    else if(n > 1)
    {
        list = realloc(list, sizeof(*list) * (numPaths + 1));
        memcpy(list + numPaths - n, localFixedList, sizeof(*list) * n);
    }
    else
    {
        list[numPaths-1] = localFixedList[0];
    }
    list[numPaths] = 0; // Terminate.

    Str_Free(&buf);

    if(count)
        *count = numPaths;
    return list;

#undef FIXEDSIZE
}

Uri** F_CreateUriList(resourceclass_t rclass, const char* searchPaths)
{
    return F_CreateUriList2(rclass, searchPaths, 0);
}

Uri** F_CreateUriListStr2(resourceclass_t rclass, const ddstring_t* searchPaths,
    size_t* count)
{
    if(!searchPaths)
    {
        if(count)
            *count = 0;
        return 0;
    }
    return F_CreateUriList2(rclass, Str_Text(searchPaths), count);
}

Uri** F_CreateUriListStr(resourceclass_t rclass, const ddstring_t* searchPaths)
{
    return F_CreateUriListStr2(rclass, searchPaths, 0);
}

void F_DestroyUriList(Uri** list)
{
    if(list)
    {
        Uri** ptr;
        for(ptr = list; *ptr; ptr++)
            Uri_Delete(*ptr);
        free(list);
    }
}

ddstring_t** F_ResolvePathList2(resourceclass_t defaultResourceClass,
    const ddstring_t* pathList, size_t* count, char delimiter)
{
    Uri** uriList = F_CreateUriListStr(RC_NULL, pathList);
    size_t resolvedPathCount = 0;
    ddstring_t** paths = NULL;

    if(uriList)
    {
        Uri** uriIt;
        for(uriIt = uriList; *uriIt; ++uriIt)
        {
            // Ignore incomplete paths.
            ddstring_t* resolvedPath = Uri_Resolved(*uriIt);
            if(!resolvedPath) continue;

            ++resolvedPathCount;
            Str_Delete(resolvedPath);
        }

        if(resolvedPathCount)
        {
            uint n = 0;

            paths = (ddstring_t**)malloc(sizeof(*paths) * (resolvedPathCount+1));
            if(!paths) Con_Error("F_ResolvePathList: Failed on allocation of %lu bytes for new path list.", (unsigned long) sizeof(*paths) * (resolvedPathCount+1));

            for(uriIt = uriList; *uriIt; ++uriIt)
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
    const ddstring_t* pathList, size_t* count)
{
    return F_ResolvePathList2(defaultResourceClass, pathList, count, PATH_DELIMIT_CHAR);
}

void F_DestroyStringList(ddstring_t** list)
{
    if(list)
    {
        ddstring_t** ptr;
        for(ptr = list; *ptr; ptr++)
            Str_Delete(*ptr);
        free(list);
    }
}

#if _DEBUG
void F_PrintStringList(const ddstring_t** strings, size_t stringsCount)
{
    if(!strings || stringsCount == 0)
        return;

    { const ddstring_t** ptr = strings;
    size_t i;
    for(i = 0; i < stringsCount && *ptr; ++i, ptr++)
        Con_Printf("  \"%s\"\n", Str_Text(*ptr));
    }
}
#endif

uint F_FindResource5(resourceclass_t rclass, const Uri** searchPaths,
    ddstring_t* foundPath, int flags, const ddstring_t* optionalSuffix)
{
    errorIfNotInited("F_FindResource4");
    if(rclass != RC_UNKNOWN && !VALID_RESOURCE_CLASS(rclass))
        Con_Error("F_FindResource: Invalid resource class %i.\n", rclass);
    if(!searchPaths) return 0;
    return findResource(rclass, searchPaths, foundPath, flags, optionalSuffix);
}

uint F_FindResourceStr4(resourceclass_t rclass, const ddstring_t* searchPaths,
    ddstring_t* foundPath, int flags, const ddstring_t* optionalSuffix)
{
    Uri** list;
    int result = 0;

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

    list = F_CreateUriListStr(rclass, searchPaths);
    if(!list) return 0;

    result = findResource(rclass, (const Uri**)list, foundPath, flags, optionalSuffix);
    F_DestroyUriList(list);
    return result;
}

uint F_FindResourceForRecord2(AbstractResource* rec, ddstring_t* foundPath, const Uri* const* searchPaths)
{
    return findResource(AbstractResource_ResourceClass(rec),
                        searchPaths, foundPath, RLF_DEFAULT, NULL/*no optional suffix*/);
}

uint F_FindResourceForRecord(AbstractResource* rec, ddstring_t* foundPath)
{
    return F_FindResourceForRecord2(rec, foundPath,
                                    (const Uri* const*) AbstractResource_SearchPaths(rec));
}

uint F_FindResourceStr3(resourceclass_t rclass, const ddstring_t* searchPaths,
    ddstring_t* foundPath, int flags)
{
    return F_FindResourceStr4(rclass, searchPaths, foundPath, flags, NULL/*no optional suffix*/);
}

uint F_FindResourceStr2(resourceclass_t rclass, const ddstring_t* searchPath, ddstring_t* foundPath)
{
    return F_FindResourceStr3(rclass, searchPath, foundPath, RLF_DEFAULT);
}

uint F_FindResourceStr(resourceclass_t rclass, const ddstring_t* searchPath)
{
    return F_FindResourceStr2(rclass, searchPath, NULL/*no found path*/);
}

uint F_FindResource4(resourceclass_t rclass, const char* _searchPaths,
    ddstring_t* foundPath, int flags, const char* _optionalSuffix)
{
    ddstring_t searchPaths, optionalSuffix;
    boolean hasOptionalSuffix = false;
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

uint F_FindResource3(resourceclass_t rclass, const char* searchPaths, ddstring_t* foundPath, int flags)
{
    return F_FindResource4(rclass, searchPaths, foundPath, flags, NULL/*no optional suffix*/);
}

uint F_FindResource2(resourceclass_t rclass, const char* searchPaths, ddstring_t* foundPath)
{
    return F_FindResource3(rclass, searchPaths, foundPath, RLF_DEFAULT);
}

uint F_FindResource(resourceclass_t rclass, const char* searchPaths)
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
    assert(VALID_RESOURCE_CLASS(rclass));
    errorIfNotInited("F_DefaultResourceNamespaceForClass");
    return F_ResourceNamespaceForName(Str_Text(&defaultNamespaceForClass[rclass]));
}

resourcetype_t F_GuessResourceTypeByName(const char* path)
{
    if(!path || !path[0])
        return RT_NONE; // Unrecognizable.

    // We require a file extension for this.
    { char* ext = strrchr(path, '.');
    if(NULL != ext)
    {
        uint i;
        ++ext;
        for(i = RT_FIRST; i < NUM_RESOURCE_TYPES; ++i)
        {
            const resourcetypeinfo_t* info = getInfoForResourceType((resourcetype_t)i);
            // Check the extension.
            if(info->knownFileNameExtensions[0])
            {
                char* const* cand = info->knownFileNameExtensions;
                do
                {
                    if(!stricmp(*cand, ext))
                        return (resourcetype_t)i;
                } while(*(++cand));
            }
        }
    }}
    return RT_NONE; // Unrecognizable.
}

boolean F_MapResourcePath(resourcenamespaceid_t rni, ddstring_t* path)
{
    if(path && !Str_IsEmpty(path))
    {
        resourcenamespaceinfo_t* info = getNamespaceInfoForId(rni);
        if(info->flags & RNF_USE_VMAP)
        {
            int nameLen = Str_Length(&info->name), pathLen = Str_Length(path);
            if(nameLen <= pathLen && Str_At(path, nameLen) == '/' &&
               !strnicmp(Str_Text(&info->name), Str_Text(path), nameLen))
            {
                Str_Prepend(path, Str_Text(Game_DataPath(theGame)));
                return true;
            }
        }
    }
    return false;
}

boolean F_ApplyPathMapping(ddstring_t* path)
{
    assert(path);
    errorIfNotInited("F_ApplyPathMapping");
    { uint i = 1;
    boolean result = false;
    while(i < numNamespaces+1 && !(result = F_MapResourcePath(i++, path)));
    return result;
    }
}

const char* F_ResourceClassStr(resourceclass_t rclass)
{
    assert(VALID_RESOURCE_CLASS(rclass));
    {
    static const char* resourceClassNames[RESOURCECLASS_COUNT] = {
        "RC_PACKAGE",
        "RC_DEFINITION",
        "RC_GRAPHIC",
        "RC_MODEL",
        "RC_SOUND",
        "RC_MUSIC",
        "RC_FONT"
    };
    return resourceClassNames[(int)rclass];
    }
}

const char* F_ParseSearchPath2(Uri* dst, const char* src, char delim,
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
        Uri_SetUri3(dst, Str_Text(&buf), defaultResourceClass);
        Str_Free(&buf);
    }
    if(!*src)
        return 0; // It ended.
    // Skip past the delimiter.
    return src + 1;
}

const char* F_ParseSearchPath(Uri* dst, const char* src, char delim)
{
    return F_ParseSearchPath2(dst, src, delim, RC_UNKNOWN);
}
