/**\file sys_reslocator.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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

/**
 * Routines for locating resources.
 */

// HEADER FILES ------------------------------------------------------------

#ifdef WIN32
#  include <direct.h>
#endif

#ifdef UNIX
#  include <pwd.h>
#  include <limits.h>
#  include <unistd.h>
#endif

#include "de_base.h"
#include "de_console.h"
#include "m_args.h"
#include "m_misc.h"

#include "filedirectory.h"
#include "resourcerecord.h"
#include "resourcenamespace.h"

// MACROS ------------------------------------------------------------------

#define PATH_DELIMIT_CHAR       ';'
#define PATH_DELIMIT_STR        ";"

// TYPES -------------------------------------------------------------------

#define MAX_EXTENSIONS          (3)
typedef struct {
    /// Default class attributed to resources of this type.
    resourceclass_t defaultClass;
    char* knownFileNameExtensions[MAX_EXTENSIONS];
} resourcetypeinfo_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean inited = false;

static const resourcetypeinfo_t typeInfo[NUM_RESOURCE_TYPES] = {
    /* RT_ZIP */        { RC_PACKAGE,      {"pk3", "zip", 0} },
    /* RT_WAD */        { RC_PACKAGE,      {"wad", 0} },
    /* RT_DED */        { RC_DEFINITION,   {"ded", 0} },
    /* RT_PNG */        { RC_GRAPHIC,      {"png", 0} },
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
    /* RC_GRAPHIC */    { RT_PNG, RT_TGA, RT_PCX, 0 }, // Favour quality.
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

static resourcenamespace_t** namespaces = 0;
static uint numNamespaces = 0;

// CODE --------------------------------------------------------------------

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

static __inline resourcenamespace_t* getNamespaceForId(resourcenamespaceid_t rni)
{
    errorIfNotInited("getNamespaceForId");
    if(!F_IsValidResourceNamespaceId(rni))
        Con_Error("getNamespaceForId: Invalid namespace id %i.", (int)rni);
    return namespaces[((uint)rni)-1];
}

static resourcenamespaceid_t findNamespaceId(resourcenamespace_t* rnamespace)
{
    if(NULL != rnamespace)
    {
        uint i;
        for(i = 0; i < numNamespaces; ++i)
        {
            resourcenamespace_t* other = namespaces[i];
            if(other == rnamespace)
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
            resourcenamespace_t* rnamespace = namespaces[i];
            if(!stricmp(Str_Text(ResourceNamespace_Name(rnamespace)), name))
                return (resourcenamespaceid_t)(i+1);
        }
    }
    return 0;
}

static void destroyAllNamespaces(void)
{
    if(numNamespaces == 0)
        return;
    { uint i;
    for(i = 0; i < numNamespaces; ++i)
    {
        resourcenamespace_t* rnamespace = namespaces[i];
        if(NULL != ResourceNamespace_Directory(rnamespace))
        {
            FileDirectory_Delete(ResourceNamespace_Directory(rnamespace));
        }
        ResourceNamespace_Delete(rnamespace);
    }}
    free(namespaces);
    namespaces = 0;
}

static void resetAllNamespaces(void)
{
    uint i;
    for(i = 0; i < numNamespaces; ++i)
        ResourceNamespace_Reset(namespaces[i]);
}

static boolean tryFindResource2(resourceclass_t rclass, const ddstring_t* searchPath,
    ddstring_t* foundPath, resourcenamespace_t* rnamespace)
{
    assert(inited && searchPath && !Str_IsEmpty(searchPath));
    // Is there a namespace we should use?
    if(rnamespace && ResourceNamespace_Find2(rnamespace, searchPath, foundPath))
    {
        if(foundPath)
            F_PrependBasePath(foundPath, foundPath);
        return true;
    }
    if(0 != F_Access(Str_Text(searchPath)))
    {
        if(foundPath)
            F_PrependBasePath(foundPath, searchPath);
        return true;
    }
    return false;
}

static boolean tryFindResource(resourceclass_t rclass, const ddstring_t* searchPath,
    ddstring_t* foundPath, resourcenamespace_t* rnamespace)
{
    assert(inited && searchPath && !Str_IsEmpty(searchPath));
    {
    boolean found = false;
    const char* ptr;

    // Has an extension been specified?
    ptr = F_FindFileExtension(Str_Text(searchPath));
    if(ptr && *ptr != '*') // Try this first.
        found = tryFindResource2(rclass, searchPath, foundPath, rnamespace);

    if(!found)
    {
        ddstring_t path2;
        Str_Init(&path2);

        // Create a copy of the searchPath minus file extension.
        if(ptr)
        {
            Str_PartAppend(&path2, Str_Text(searchPath), 0, ptr - Str_Text(searchPath));
        }
        else
        {
            Str_Copy(&path2, searchPath);
            Str_AppendChar(&path2, '.');
        }

        if(VALID_RESOURCE_CLASS(rclass) && searchTypeOrder[rclass][0] != RT_NONE)
        {
            const resourcetype_t* type = searchTypeOrder[rclass];
            ddstring_t tmp;
            Str_Init(&tmp);
            do
            {
                const resourcetypeinfo_t* info = getInfoForResourceType(*type);
                if(info->knownFileNameExtensions[0])
                {
                    char* const* ext = info->knownFileNameExtensions;
                    do
                    {
                        Str_Clear(&tmp);
                        Str_Appendf(&tmp, "%s%s", Str_Text(&path2), *ext);
                        found = tryFindResource2(rclass, &tmp, foundPath, rnamespace);
                    } while(!found && *(++ext));
                }
            } while(!found && *(++type) != RT_NONE);
            Str_Free(&tmp);
        }
        Str_Free(&path2);
    }

    return found;
    }
}

static boolean findResource2(resourceclass_t rclass, const ddstring_t* searchPath,
    const ddstring_t* optionalSuffix, ddstring_t* foundPath, resourcenamespace_t* rnamespace)
{
    assert(inited && searchPath && !Str_IsEmpty(searchPath));
    {
    boolean found = false;

#if _DEBUG
    VERBOSE2( Con_Message("Using rnamespace '%s'...\n", rnamespace? Str_Text(ResourceNamespace_Name(rnamespace)) : "None") )
#endif

    // First try with the optional suffix.
    if(optionalSuffix)
    {
        ddstring_t fn;

        Str_Init(&fn);

        // Has an extension been specified?
        { const char* ptr = F_FindFileExtension(Str_Text(searchPath));
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
        }}

        found = tryFindResource(rclass, &fn, foundPath, rnamespace);
        Str_Free(&fn);
    }

    // Try without a suffix.
    if(!found)
        found = tryFindResource(rclass, searchPath, foundPath, rnamespace);

    return found;
    }
}

static int findResource(resourceclass_t rclass, const Uri* const* list,
    const ddstring_t* optionalSuffix, ddstring_t* foundPath)
{
    assert(inited && list && (rclass == RC_UNKNOWN || VALID_RESOURCE_CLASS(rclass)));
    {
    uint result = 0, n = 1;
    const Uri* const* ptr;

    for(ptr = list; *ptr; ptr++, n++)
    {
        const Uri* searchPath = *ptr;
        ddstring_t* resolvedPath;

        if((resolvedPath = Uri_Resolved(searchPath)) == 0)
            continue; // Incomplete path; ignore it.

        // If this is an absolute path, locate using it.
        if(F_IsAbsolute(resolvedPath))
        {
            if(findResource2(rclass, resolvedPath, optionalSuffix, foundPath, 0))
                result = n;
        }
        else
        {   // Probably a relative path.
            // Has a namespace identifier been included?
            if(!Str_IsEmpty(Uri_Scheme(searchPath)))
            {
                resourcenamespaceid_t rni;
                if((rni = F_SafeResourceNamespaceForName(Str_Text(Uri_Scheme(searchPath)))) != 0)
                {
                    if(findResource2(rclass, resolvedPath, optionalSuffix, foundPath, getNamespaceForId(rni)))
                        result = n;
                }
                else
                {
                    ddstring_t* rawPath = Uri_ToString(searchPath);
                    Con_Message("tryLocateResource: Unknown rnamespace in searchPath \"%s\", "
                                "using default for %s.\n", Str_Text(rawPath), F_ResourceClassStr(rclass));
                    Str_Delete(rawPath);
                }
            }
        }
        Str_Delete(resolvedPath);

        if(result != 0)
            break;
    }
    return result;
    }
}

ddstring_t* F_ComposeHashNameForFilePath(const ddstring_t* filePath)
{
    ddstring_t* hashName = Str_New();
    F_FileName(hashName, Str_Text(filePath));
    return hashName;
}

resourcenamespace_namehash_key_t F_HashKeyForAlphaNumericNameIgnoreCase(const ddstring_t* name)
{
    assert(name);
    {
    resourcenamespace_namehash_key_t key = 0;
    byte op = 0;
    const char* c;
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
}


static void createPackagesResourceNamespace(void)
{
    ddstring_t** doomWadPaths = 0, *doomWadDir = 0;
    uint doomWadPathsCount = 0, searchPathsCount, idx;
    resourcenamespace_t* rnamespace;
    filedirectory_t* directory;
    Uri** searchPaths;

    // Is the DOOMWADPATH environment variable in use?
    if(!ArgCheck("-nodoomwadpath") && getenv("DOOMWADPATH"))
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
        if(last != DIR_SEP_CHAR && last != DIR_WRONG_SEP_CHAR) \
            Str_Appendf(doomWadDir, "%s"DIR_SEP_STR, Str_Text(path)); \
        else \
            Str_Copy(pathCopy, (path)); \
        doomWadPaths = realloc(doomWadPaths, sizeof(*doomWadPaths) * ++doomWadPathsCount); \
        doomWadPaths[doomWadPathsCount-1] = pathCopy; \
    } \
}

        ddstring_t fullString; Str_Init(&fullString);
        
        Str_Set(&fullString, getenv("DOOMWADPATH"));
        Str_Strip(&fullString);
        if(!Str_IsEmpty(&fullString))
        {
            ddstring_t path; Str_Init(&path);
            // Split into paths.
            { const char* c = Str_Text(&fullString);
            while((c = Str_CopyDelim2(&path, c, PATH_DELIMITER_CHAR, CDF_OMIT_DELIMITER))) // Get the next path.
            {
                ADDDOOMWADPATH(&path)
            }}
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
    if(!ArgCheck("-nodoomwaddir") && getenv("DOOMWADDIR"))
    {
        doomWadDir = Str_New(); Str_Set(doomWadDir, getenv("DOOMWADDIR"));
        Str_Strip(doomWadDir);
        if(Str_IsEmpty(doomWadDir) || !F_IsAbsolute(doomWadDir))
        {
            Str_Delete(doomWadDir);
            doomWadDir = 0;
        }
        else
        {
            char last = Str_RAt(doomWadDir, 0);
            if(last != DIR_SEP_CHAR && last != DIR_WRONG_SEP_CHAR)
                Str_AppendChar(doomWadDir, DIR_SEP_CHAR);
        }
    }

    // Construct the search path list.
    searchPathsCount = 2 + doomWadPathsCount + (doomWadDir != 0? 1 : 0);
    if((searchPaths = malloc(sizeof(*searchPaths) * searchPathsCount)) == 0)
        Con_Error("createPackagesResourceNamespace: Failed on allocation of %lu bytes.",
                  (unsigned long) (sizeof(*searchPaths) * searchPathsCount)); 

    idx = 0;
    // Add the default paths.
    searchPaths[idx++] = Uri_NewWithPath2("$(GameInfo.DataPath)/", RC_NULL);
    searchPaths[idx++] = Uri_NewWithPath2("$(App.DataPath)/", RC_NULL);

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

    directory = FileDirectory_New();
    rnamespace = F_CreateResourceNamespace(PACKAGES_RESOURCE_NAMESPACE_NAME, directory,
        F_ComposeHashNameForFilePath, F_HashKeyForFilePathHashName, 0);

    if(searchPathsCount != 0)
    {
        uint i;
        for(i = 0; i < searchPathsCount; ++i)
        {
            ResourceNamespace_AddSearchPath(rnamespace, searchPaths[i], SPG_DEFAULT);
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
        byte flags;
        const char* defaultPaths[NAMESPACEDEF_MAX_SEARCHPATHS];
    } defs[] = {
        { DEFINITIONS_RESOURCE_NAMESPACE_NAME,  NULL,           NULL,           0,
            { "$(GameInfo.DefsPath)/$(GameInfo.IdentityKey)/", "$(GameInfo.DefsPath)/", "$(App.DefsPath)/" } },
        { GRAPHICS_RESOURCE_NAMESPACE_NAME,     "-gfxdir2",     "-gfxdir",      0,
            { "$(App.DataPath)/graphics/" } },
        { MODELS_RESOURCE_NAMESPACE_NAME,       "-modeldir2",   "-modeldir",    RNF_USE_VMAP,
            { "$(GameInfo.DataPath)/models/$(GameInfo.IdentityKey)/", "$(GameInfo.DataPath)/models/" } },
        { SOUNDS_RESOURCE_NAMESPACE_NAME,       "-sfxdir2",     "-sfxdir",      RNF_USE_VMAP,
            { "$(GameInfo.DataPath)/sfx/$(GameInfo.IdentityKey)/", "$(GameInfo.DataPath)/sfx/" } },
        { MUSIC_RESOURCE_NAMESPACE_NAME,        "-musdir2",     "-musdir",      RNF_USE_VMAP,
            { "$(GameInfo.DataPath)/music/$(GameInfo.IdentityKey)/", "$(GameInfo.DataPath)/music/" } },
        { TEXTURES_RESOURCE_NAMESPACE_NAME,     "-texdir2",     "-texdir",      RNF_USE_VMAP,
            { "$(GameInfo.DataPath)/textures/$(GameInfo.IdentityKey)/", "$(GameInfo.DataPath)/textures/" } },
        { FLATS_RESOURCE_NAMESPACE_NAME,        "-flatdir2",    "-flatdir",     RNF_USE_VMAP,
            { "$(GameInfo.DataPath)/flats/$(GameInfo.IdentityKey)/", "$(GameInfo.DataPath)/flats/" } },
        { PATCHES_RESOURCE_NAMESPACE_NAME,      "-patdir2",     "-patdir",      RNF_USE_VMAP,
            { "$(GameInfo.DataPath)/patches/$(GameInfo.IdentityKey)/", "$(GameInfo.DataPath)/patches/" } },
        { LIGHTMAPS_RESOURCE_NAMESPACE_NAME,    "-lmdir2",      "-lmdir",       RNF_USE_VMAP,
            { "$(GameInfo.DataPath)/lightmaps/$(GameInfo.IdentityKey)/", "$(GameInfo.DataPath)/lightmaps/" } },
        { FONTS_RESOURCE_NAMESPACE_NAME,        "-fontdir2",    "-fontdir",     RNF_USE_VMAP,
            { "$(GameInfo.DataPath)/fonts/$(GameInfo.IdentityKey)/", "$(GameInfo.DataPath)/fonts/", "$(App.DataPath)/fonts/" } },
        { NULL }
    };
    Uri* uri = Uri_New();

    // Setup of the Packages namespace is somewhat more involved...
    createPackagesResourceNamespace();

    // Setup the rest...
    { size_t i;
    for(i = 0; defs[i].name; ++i)
    {
        uint j, defaultPathCount;
        struct namespacedef_s* def = &defs[i];
        filedirectory_t* directory = FileDirectory_New();
        resourcenamespace_t* rnamespace = F_CreateResourceNamespace(def->name, directory,
            F_ComposeHashNameForFilePath, F_HashKeyForFilePathHashName, def->flags);

        defaultPathCount = 0;
        while(def->defaultPaths[defaultPathCount] && ++defaultPathCount < NAMESPACEDEF_MAX_SEARCHPATHS)
        {}

        for(j = 0; j < defaultPathCount; ++j)
        {
            Uri_SetUri3(uri, def->defaultPaths[j], RC_NULL);
            ResourceNamespace_AddSearchPath(rnamespace, uri, SPG_DEFAULT);
        }

        if(def->optOverridePath && ArgCheckWith(def->optOverridePath, 1))
        {
            const char* path = ArgNext();
            ddstring_t path2;

            // Override paths are added in reverse order.
            Str_Init(&path2);
            Str_Appendf(&path2, "%s/$(GameInfo.IdentityKey)", path);
            Uri_SetUri3(uri, Str_Text(&path2), RC_NULL);
            ResourceNamespace_AddSearchPath(rnamespace, uri, SPG_OVERRIDE);

            Uri_SetUri3(uri, path, RC_NULL);
            ResourceNamespace_AddSearchPath(rnamespace, uri, SPG_OVERRIDE);

            Str_Free(&path2);
        }

        if(def->optFallbackPath && ArgCheckWith(def->optFallbackPath, 1))
        {
            Uri_SetUri3(uri, ArgNext(), RC_NULL);
            ResourceNamespace_AddSearchPath(rnamespace, uri, SPG_FALLBACK);
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
    ResourceNamespace_Reset(F_ToResourceNamespace(rni));
}

struct resourcenamespace_s* F_ToResourceNamespace(resourcenamespaceid_t rni)
{
    return getNamespaceForId(rni);
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

resourcenamespace_t* F_CreateResourceNamespace(const char* name,
    filedirectory_t* directory, ddstring_t* (*composeHashNameFunc) (const ddstring_t* path),
    resourcenamespace_namehash_key_t (*hashNameFunc) (const ddstring_t* name), byte flags)
{
    assert(name);
    errorIfNotInited("F_CreateResourceNamespace");
    {
    resourcenamespace_t* rn = ResourceNamespace_New2(name, directory, composeHashNameFunc, hashNameFunc, flags);

    // Add this new namespace to the global list.
    namespaces = (resourcenamespace_t**) realloc(namespaces, sizeof(*namespaces) * ++numNamespaces);
    if(namespaces == NULL)
        Con_Error("F_CreateResourceNamespace: Failed on (re)allocation of %lu bytes for new resource namespace\n",
            (unsigned long) (sizeof(*namespaces) * numNamespaces));
    namespaces[numNamespaces-1] = rn;
    return rn;
    }
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
    {Uri** uris;
    if((uris = F_CreateUriListStr(RC_NULL, pathList)) != 0)
    {
        ddstring_t** paths = 0;
        size_t numResolvedPaths = 0;

        { Uri** ptr;
        for(ptr = uris; *ptr; ++ptr)
        {
            if(Uri_Resolved(*ptr) != 0) // Ignore incomplete paths.
                ++numResolvedPaths;
        }}
    
        if(numResolvedPaths != 0)
        {
            uint n = 0;
            paths = malloc(sizeof(*paths) * (numResolvedPaths+1));
            { Uri** ptr;
            for(ptr = uris; *ptr; ++ptr)
            {
                ddstring_t* resolvedPath;
                if((resolvedPath = Uri_Resolved(*ptr)) == 0)
                    continue; // Incomplete path; ignore it.
                // Let's try to make it a relative path.
                F_RemoveBasePath(resolvedPath, resolvedPath);
                paths[n++] = resolvedPath;
            }}
            paths[n] = 0; // Terminate.
        }
        F_DestroyUriList(uris);
        if(count)
            *count = numResolvedPaths;
        return paths;
    }}
    if(count)
        *count = 0;
    return 0;
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

uint F_FindResource4(resourceclass_t rclass, const Uri** searchPaths,
    ddstring_t* foundPath, const ddstring_t* optionalSuffix)
{
    errorIfNotInited("F_FindResource4");
    if(rclass != RC_UNKNOWN && !VALID_RESOURCE_CLASS(rclass))
        Con_Error("F_FindResource: Invalid resource class %i.\n", rclass);
    if(!searchPaths)
        return 0;
    return findResource(rclass, searchPaths, optionalSuffix, foundPath);
}

uint F_FindResourceStr3(resourceclass_t rclass, const ddstring_t* searchPaths,
    ddstring_t* foundPath, const ddstring_t* optionalSuffix)
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

    if((list = F_CreateUriListStr(rclass, searchPaths)) != 0)
    {
        result = findResource(rclass, (const Uri**)list, optionalSuffix, foundPath);
        F_DestroyUriList(list);
    }
    return result;
}

uint F_FindResourceForRecord(resourcerecord_t* rec, ddstring_t* foundPath)
{
    return findResource(ResourceRecord_ResourceClass(rec), ResourceRecord_SearchPaths(rec), 0, foundPath);
}

uint F_FindResourceStr2(resourceclass_t rclass, const ddstring_t* searchPath,
    ddstring_t* foundPath)
{
    return F_FindResourceStr3(rclass, searchPath, foundPath, 0);
}

uint F_FindResourceStr(resourceclass_t rclass, const ddstring_t* searchPath)
{
    return F_FindResourceStr2(rclass, searchPath, 0);
}

uint F_FindResource3(resourceclass_t rclass, const char* _searchPaths,
    ddstring_t* foundPath, const char* _optionalSuffix)
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
    result = F_FindResourceStr3(rclass, &searchPaths, foundPath, hasOptionalSuffix? &optionalSuffix : 0);
    if(hasOptionalSuffix)
        Str_Free(&optionalSuffix);
    Str_Free(&searchPaths);
    return result;
}

uint F_FindResource2(resourceclass_t rclass, const char* searchPaths, ddstring_t* foundPath)
{
    return F_FindResource3(rclass, searchPaths, foundPath, 0);
}

uint F_FindResource(resourceclass_t rclass, const char* searchPaths)
{
    return F_FindResource2(rclass, searchPaths, 0);
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

boolean F_ApplyPathMapping(ddstring_t* path)
{
    assert(path);
    errorIfNotInited("F_ApplyPathMapping");
    { uint i = 0;
    boolean result = false;
    while(i < numNamespaces && !(result = ResourceNamespace_MapPath(namespaces[i++], path)));
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

void F_FileDir(ddstring_t* dst, const ddstring_t* src)
{
    assert(NULL != dst && NULL != src);
    {
    /// \fixme Potentially truncates @a src to FILENAME_T_MAXLEN
    directory_t* dir = Dir_ConstructFromPathDir(Str_Text(src));
    Str_Set(dst, Dir_Path(dir));
    Dir_Delete(dir);
    }
}

void F_FileName(ddstring_t* dst, const char* src)
{
#ifdef WIN32
    char name[_MAX_FNAME];
#else
    char name[NAME_MAX];
#endif
    _splitpath(src, 0, 0, name, 0);
    Str_Set(dst, name);
}

void F_FileNameAndExtension(ddstring_t* dst, const char* src)
{
#ifdef WIN32
    char name[_MAX_FNAME], ext[_MAX_EXT];
#else
    char name[NAME_MAX], ext[NAME_MAX];
#endif
    _splitpath(src, 0, 0, name, ext);
    Str_Clear(dst);
    Str_Appendf(dst, "%s%s", name, ext);
}

int F_FileExists(const char* file)
{
    int result = -1;
    if(NULL != file && file[0])
    {
        ddstring_t buf;
        // Normalize the path into one we can process.
        Str_Init(&buf); Str_Set(&buf, file);
        Str_Strip(&buf);
        F_FixSlashes(&buf, &buf);
        F_ExpandBasePath(&buf, &buf);

        result = !access(Str_Text(&buf), 4); // Read permission?

        Str_Free(&buf);
    }
    return result;
}

boolean F_MakePath(const char* path)
{
#if !defined(WIN32) && !defined(UNIX)
#  error F_MakePath has no implementation for this platform.
#endif

    ddstring_t full, buf;
    char* ptr, *endptr;
    boolean result;

    // Convert all backslashes to normal slashes.
    Str_Init(&full); Str_Set(&full, path);
    Str_Strip(&full);
    F_FixSlashes(&full, &full);

    // Does this path already exist?
    if(0 == access(Str_Text(&full), 0))
    {
        Str_Free(&full);
        return true;
    }

    // Check and create the path in segments.
    ptr = Str_Text(&full);
    Str_Init(&buf);
    do
    {
        endptr = strchr(ptr, DIR_SEP_CHAR);
        if(!endptr)
            Str_Append(&buf, ptr);
        else
            Str_PartAppend(&buf, ptr, 0, endptr - ptr);
        if(0 != access(Str_Text(&buf), 0))
        {
            // Path doesn't exist, create it.
#ifdef WIN32
            mkdir(Str_Text(&buf));
#elif UNIX
            mkdir(Str_Text(&buf), 0775);
#endif
        }
        Str_AppendChar(&buf, DIR_SEP_CHAR);
        ptr = endptr + 1;
    } while(NULL != endptr);

    result = (0 == access(Str_Text(&full), 0));
    Str_Free(&buf);
    Str_Free(&full);
    return result;
}

/// \todo dj: Find a suitable home for this.
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

/// \todo dj: Find a suitable home for this.
boolean F_FixSlashes(ddstring_t* dstStr, const ddstring_t* srcStr)
{
    assert(dstStr && srcStr);
    {
    boolean result = false;
    if(!Str_IsEmpty(srcStr))
    {
        char* dst = Str_Text(dstStr);
        const char* src = Str_Text(srcStr);

        if(dstStr != srcStr)
        {
            Str_Clear(dstStr);
            Str_Reserve(dstStr, Str_Length(srcStr));
        }

        { size_t i;
        for(i = 0; src[i]; ++i)
        {
            if(src[i] != DIR_WRONG_SEP_CHAR)
            {
                if(dstStr != srcStr)
                    Str_AppendChar(dstStr, src[i]);
                continue;
            }

            if(dstStr != srcStr)
                Str_AppendChar(dstStr, DIR_SEP_CHAR);
            else
                dst[i] = DIR_SEP_CHAR;
            result = true;
        }}
    }
    return result;
    }
}

const char* F_FindFileExtension(const char* path)
{
    if(path && path[0])
    {
        size_t len = strlen(path);
        const char* p = path + len - 1;
        if(p - path > 1 && *p != DIR_SEP_CHAR && *p != DIR_WRONG_SEP_CHAR)
        {
            do
            {
                if(*(p - 1) == DIR_SEP_CHAR ||
                   *(p - 1) == DIR_WRONG_SEP_CHAR)
                    break;
                if(*p == '.')
                    return (unsigned) (p - path) < len - 1? p + 1 : NULL;
            } while(--p > path);
        }
    }
    return NULL; // Not found.
}

void F_ExtractFileBase2(char* dest, const char* path, size_t max, int ignore)
{
    const char* src = path + strlen(path) - 1;

    // Back up until a \ or the start.
    while(src != path && *(src - 1) != '\\' && *(src - 1) != '/')
    {
        src--;
    }

    while(*src && *src != '.' && max-- > 0)
    {
        if(ignore-- > 0)
        {
            src++; // Skip chars.
            max++; // Doesn't count.
        }
        else
            *dest++ = toupper((int) *src++);
    }

    if(max > 0) // Room for a null?
    {
        // End with a terminating null.
        *dest++ = 0;
    }
}

void F_ExtractFileBase(char* dest, const char* path, size_t len)
{
    F_ExtractFileBase2(dest, path, len, 0);
}

/// \todo dj: Find a suitable home for this.
void F_ResolveSymbolicPath(ddstring_t* dst, const ddstring_t* src)
{
    assert(dst && src);

    // Src path is base-relative?
    if(Str_At(src, 0) == DIR_SEP_CHAR)
    {
        boolean mustCopy = (dst == src);
        if(mustCopy)
        {
            ddstring_t buf;
            Str_Init(&buf);
            Str_Set(&buf, ddBasePath);
            Str_PartAppend(&buf, Str_Text(src), 1, Str_Length(src)-1);
            Str_Set(dst, Str_Text(&buf));
            return;
        }

        Str_Set(dst, ddBasePath);
        Str_PartAppend(dst, Str_Text(src), 1, Str_Length(src)-1);
        return;
    }

    // Src path is workdir-relative.

    if(dst == src)
    {
        Str_Prepend(dst, ddRuntimePath);
        return;
    }

    Str_Appendf(dst, "%s%s", ddRuntimePath, Str_Text(src));
}

/// \todo dj: Find a suitable home for this.
boolean F_IsRelativeToBasePath(const char* path)
{
    assert(path);
    return !strnicmp(path, ddBasePath, strlen(ddBasePath));
}

/// \todo dj: Find a suitable home for this.
boolean F_RemoveBasePath(ddstring_t* dst, const ddstring_t* absPath)
{
    assert(dst && absPath);

    if(F_IsRelativeToBasePath(Str_Text(absPath)))
    {
        boolean mustCopy = (dst == absPath);
        if(mustCopy)
        {
            ddstring_t buf;
            Str_Init(&buf);
            Str_PartAppend(&buf, Str_Text(absPath), (int)strlen(ddBasePath), Str_Length(absPath) - (int)strlen(ddBasePath));
            Str_Set(dst, Str_Text(&buf));
            Str_Free(&buf);
            return true;
        }

        Str_Clear(dst);
        Str_PartAppend(dst, Str_Text(absPath), (int)strlen(ddBasePath), Str_Length(absPath) - (int)strlen(ddBasePath));
        return true;
    }

    // Do we need to copy anyway?
    if(dst != absPath)
        Str_Set(dst, Str_Text(absPath));

    // This doesn't appear to be the base path.
    return false;
}

/// \todo dj: Find a suitable home for this.
boolean F_IsAbsolute(const ddstring_t* str)
{
    if(!str)
        return false;

    if(Str_At(str, 0) == '\\' || Str_At(str, 0) == '/' || Str_At(str, 1) == ':')
        return true;
#ifdef UNIX
    if(Str_At(str, 0) == '~')
        return true;
#endif
    return false;
}

/// \todo dj: Find a suitable home for this.
boolean F_PrependBasePath(ddstring_t* dst, const ddstring_t* src)
{
    assert(dst && src);

    if(!F_IsAbsolute(src))
    {
        if(dst != src)
            Str_Set(dst, Str_Text(src));
        Str_Prepend(dst, ddBasePath);
        return true;
    }

    // Do we need to copy anyway?
    if(dst != src)
        Str_Set(dst, Str_Text(src));

    return false; // Not done.
}

/// \todo dj: Find a suitable home for this.
boolean F_PrependWorkPath(ddstring_t* dst, const ddstring_t* src)
{
    assert(dst && src);

    if(!F_IsAbsolute(src))
    {
        ddstring_t dir;
        Str_Init(&dir);
        F_FileDir(&dir, src);
        Str_Prepend(dst, Str_Text(&dir));
        Str_Free(&dir);
        return true;
    }

    // Do we need to copy anyway?
    if(dst != src)
        Str_Set(dst, Str_Text(src));

    return false; // Not done.
}

boolean F_MakeAbsolute(ddstring_t* dst, const ddstring_t* src)
{
    if(F_ExpandBasePath(dst, src)) return true;
    // src is equal to dst
    if(F_PrependBasePath(dst, dst)) return true;
    if(F_PrependWorkPath(dst, dst)) return true;
    return false; // Not done.
}

/// \todo dj: Find a suitable home for this.
boolean F_ExpandBasePath(ddstring_t* dst, const ddstring_t* src)
{
    assert(dst && src);
    if(Str_At(src, 0) == '>' || Str_At(src, 0) == '}')
    {
        boolean mustCopy = (dst == src);
        if(mustCopy)
        {
            ddstring_t buf;
            Str_Init(&buf); Str_Set(&buf, ddBasePath);
            Str_PartAppend(&buf, Str_Text(src), 1, Str_Length(src)-1);
            Str_Set(dst, Str_Text(&buf));
            Str_Free(&buf);
            return true;
        }

        Str_Set(dst, ddBasePath);
        Str_PartAppend(dst, Str_Text(src), 1, Str_Length(src)-1);
        return true;
    }
#ifdef UNIX
    else if(Str_At(src, 0) == '~')
    {
        if(Str_At(src, 1) == DIR_SEP_CHAR && getenv("HOME"))
        {   // Replace it with the HOME environment variable.
            ddstring_t buf;
            Str_Init(&buf);

            Str_Set(&buf, getenv("HOME"));
            if(Str_RAt(&buf, 0) != DIR_SEP_CHAR)
                Str_AppendChar(&buf, DIR_SEP_CHAR);

            // Append the rest of the original path.
            Str_PartAppend(&buf, Str_Text(src), 2, Str_Length(src)-2);

            Str_Set(dst, Str_Text(&buf));
            Str_Free(&buf);
            return true;
        }

        // Parse the user's home directory (from passwd).
        { boolean result = false;
        ddstring_t userName;
        const char* p = Str_Text(src)+2;

        Str_Init(&userName);
        if((p = Str_CopyDelim2(&userName, p, '/', CDF_OMIT_DELIMITER)))
        {
            ddstring_t buf;
            struct passwd* pw;

            Str_Init(&buf);
            if((pw = getpwnam(Str_Text(&userName))) != NULL)
            {
                Str_Set(&buf, pw->pw_dir);
                if(Str_RAt(&buf, 0) != DIR_SEP_CHAR)
                    Str_AppendChar(&buf, DIR_SEP_CHAR);
                result = true;
            }

            Str_Append(&buf, Str_Text(src) + 1);
            Str_Set(dst, Str_Text(&buf));
            Str_Free(&buf);
        }
        Str_Free(&userName);
        if(result)
            return result;
        }
    }
#endif

    // Do we need to copy anyway?
    if(dst != src)
        Str_Set(dst, Str_Text(src));

    // No expansion done.
    return false;
}

boolean F_TranslatePath(ddstring_t* dst, const ddstring_t* src)
{
    F_FixSlashes(dst, src); // Will copy src to dst if not equal.
    return F_ExpandBasePath(dst, dst);
}

/// @return  @c true if @a path begins with a known directive.
static boolean pathHasDirective(const char* path)
{
    if(NULL != path && path[0])
    {
#ifdef UNIX
        if('~' == path[0]) return true;
#endif
        if('}' == path[0] || '>' == path[0]) return true;
    }
    return false;
}

/// \todo dj: Find a suitable home for this.
const char* F_PrettyPath(const char* path)
{
#define NUM_BUFS            8

    static ddstring_t buffers[NUM_BUFS]; // \fixme: never free'd!
    static uint index = 0;

    ddstring_t* buf = NULL;
    int len;

    if(NULL == path || 0 == (len = (int)strlen(path)))
        return path;

    // Hide relative directives like '}'
    if(len > 1 && pathHasDirective(path))
    {
        buf = &buffers[index++ % NUM_BUFS];
        Str_Clear(buf);
        Str_PartAppend(buf, path, 1, len-1);
        path = Str_Text(buf);
    }

    // If within our the base directory cut out the base path.
    if(F_IsRelativeToBasePath(path))
    {
        if(NULL == buf)
        {
            buf = &buffers[index++ % NUM_BUFS];
            Str_Set(buf, path);
        }
        F_RemoveBasePath(buf, buf);
        path = Str_Text(buf);
    }

    // Swap directory separators with their system-specific version.
    if(strchr(path, DIR_WRONG_SEP_CHAR))
    {
        if(NULL == buf)
        {
            buf = &buffers[index++ % NUM_BUFS];
            Str_Set(buf, path);
        }
        F_FixSlashes(buf, buf);
        path = Str_Text(buf);
    }

    return path;

#undef NUM_BUFS
}
