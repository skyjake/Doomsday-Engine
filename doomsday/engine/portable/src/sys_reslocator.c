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

#ifdef UNIX
#  include <pwd.h>
#  include <limits.h>
#endif

#include "de_base.h"
#include "de_console.h"
#include "m_args.h"
#include "m_misc.h"

#include "pathdirectory.h"
#include "resourcenamespace.h"
#include "resourcerecord.h"

// MACROS ------------------------------------------------------------------

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

static resourcenamespace_namehash_key_t hashVarLengthNameIgnoreCase(const ddstring_t* name);
static ddstring_t* composeHashNameForFilePath(const ddstring_t* filePath);

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
    /* RT_DEH */        { RC_UNKNOWN,      {"deh", 0} }
};

// Recognized resource types (in order of importance, left to right).
#define MAX_TYPEORDER 6
static const resourcetype_t searchTypeOrder[RESOURCECLASS_COUNT][MAX_TYPEORDER] = {
    /* RC_PACKAGE */    { RT_ZIP, RT_WAD, 0 }, // Favor ZIP over WAD.
    /* RC_DEFINITION */ { RT_DED, 0 }, // Only DED files.
    /* RC_GRAPHIC */    { RT_PNG, RT_TGA, RT_PCX, 0 }, // Favour quality.
    /* RC_MODEL */      { RT_DMD, RT_MD2, 0 }, // Favour DMD over MD2.
    /* RC_SOUND */      { RT_WAV, 0 }, // Only WAV files.
    /* RC_MUSIC */      { RT_OGG, RT_MP3, RT_WAV, RT_MOD, RT_MID, 0 }
};

static const ddstring_t defaultNamespaceForClass[RESOURCECLASS_COUNT] = {
    /* RC_PACKAGE */    { PACKAGES_RESOURCE_NAMESPACE_NAME },
    /* RC_DEFINITION */ { DEFINITIONS_RESOURCE_NAMESPACE_NAME },
    /* RC_GRAPHIC */    { GRAPHICS_RESOURCE_NAMESPACE_NAME },
    /* RC_MODEL */      { MODELS_RESOURCE_NAMESPACE_NAME },
    /* RC_SOUND */      { SOUNDS_RESOURCE_NAMESPACE_NAME },
    /* RC_MUSIC */      { MUSIC_RESOURCE_NAMESPACE_NAME }
};

static resourcenamespace_t** namespaces = 0;
static uint numNamespaces = 0;

/// Local virtual file system paths on the host system.
static pathdirectory_t* fsLocalPaths;

// CODE --------------------------------------------------------------------

static ddstring_t* composeHashNameForFilePath(const ddstring_t* filePath)
{
    ddstring_t* hashName = Str_New(); Str_Init(hashName);
    F_FileName(hashName, filePath);
    return hashName;
}

/**
 * This is a hash function. It uses the resource name to generate a
 * somewhat-random number between 0 and RESOURCENAMESPACE_HASHSIZE.
 *
 * @return              The generated hash key.
 */
static resourcenamespace_namehash_key_t hashVarLengthNameIgnoreCase(const ddstring_t* name)
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

static __inline const resourcetypeinfo_t* getInfoForResourceType(resourcetype_t type)
{
    assert(VALID_RESOURCE_TYPE(type));
    return &typeInfo[((uint)type)-1];
}

static __inline resourcenamespace_t* getNamespaceForId(resourcenamespaceid_t rni)
{
    if(!F_IsValidResourceNamespaceId(rni))
        Con_Error("getNamespaceForId: Invalid namespace id %i.", (int)rni);
    return namespaces[((uint)rni)-1];
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
        ResourceNamespace_Destruct(namespaces[i]);
    }
    free(namespaces);
    namespaces = 0;
}

static void resetAllNamespaces(void)
{
    uint i;
    for(i = 0; i < numNamespaces; ++i)
        ResourceNamespace_Reset(namespaces[i]);
}

static void clearNamespaceSearchPaths(resourcenamespaceid_t rni)
{
    if(rni == 0)
    {
        uint i;
        for(i = 0; i < numNamespaces; ++i)
            ResourceNamespace_ClearExtraSearchPaths(namespaces[i]);
        return;
    }
    ResourceNamespace_ClearExtraSearchPaths(getNamespaceForId(rni));
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

    if(F_Access(Str_Text(searchPath)))
    {
        if(foundPath)
            Str_Copy(foundPath, searchPath);
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
    char* ptr;

    // Has an extension been specified?
    ptr = M_FindFileExtension(Str_Text(searchPath));
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
    VERBOSE2( Con_Message("Using rnamespace \"%s\" ...\n", rnamespace? Str_Text(ResourceNamespace_Name(rnamespace)) : "None") );
#endif

    // First try with the optional suffix.
    if(optionalSuffix)
    {
        ddstring_t fn;

        Str_Init(&fn);

        // Has an extension been specified?
        { char* ptr = M_FindFileExtension(Str_Text(searchPath));
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

static int findResource(resourceclass_t rclass, const dduri_t** list,
    const ddstring_t* optionalSuffix, ddstring_t* foundPath)
{
    assert(inited && list && (rclass == RC_UNKNOWN || VALID_RESOURCE_CLASS(rclass)));
    {
    uint result = 0, n = 1;
    const dduri_t** ptr;

    for(ptr = list; *ptr; ptr++, n++)
    {
        const dduri_t* searchPath = *ptr;
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

static resourcenamespace_t* createResourceNamespace(const char* name,
    ddstring_t* (*composeHashNameFunc) (const ddstring_t* path),
    resourcenamespace_namehash_key_t (*hashNameFunc) (const ddstring_t* name),
    const dduri_t** searchPaths, int numSearchPaths, byte flags, const char* overrideName,
    const char* overrideName2)
{
    assert(name);
    {
    resourcenamespace_t* rn = ResourceNamespace_Construct5(name, composeHashNameFunc,
        hashNameFunc, searchPaths, numSearchPaths, flags, overrideName, overrideName2);
    namespaces = realloc(namespaces, sizeof(*namespaces) * ++numNamespaces);
    namespaces[numNamespaces-1] = rn;
    return rn;
    }
}

static void createResourceNamespaces(void)
{
#define NAMESPACEDEF_MAX_SEARCHPATHS        5

    struct namespacedef_s {
        const char* name;
        const char* searchPaths[NAMESPACEDEF_MAX_SEARCHPATHS];
        byte flags;
        const char* overrideName;
        const char* overrideName2;
    } defs[] =
    {
        { PACKAGES_RESOURCE_NAMESPACE_NAME,    { "$(GameInfo.DataPath)", "$(App.DataPath)", "$(DOOMWADDIR)" } },
        { DEFINITIONS_RESOURCE_NAMESPACE_NAME, { "$(GameInfo.DefsPath)/$(GameInfo.IdentityKey)", "$(GameInfo.DefsPath)", "$(App.DefsPath)" } },
        { GRAPHICS_RESOURCE_NAMESPACE_NAME,    { "$(App.DataPath)/graphics" }, 0, "-gfxdir",  "-gfxdir2" },
        { MODELS_RESOURCE_NAMESPACE_NAME,      { "$(GameInfo.DataPath)/models/$(GameInfo.IdentityKey)", "$(GameInfo.DataPath)/models" },        RNF_USE_VMAP,  "-modeldir", "-modeldir2" },
        { SOUNDS_RESOURCE_NAMESPACE_NAME,      { "$(GameInfo.DataPath)/sfx/$(GameInfo.IdentityKey)", "$(GameInfo.DataPath)/sfx" },              RNF_USE_VMAP,  "-sfxdir",   "-sfxdir2" },
        { MUSIC_RESOURCE_NAMESPACE_NAME,       { "$(GameInfo.DataPath)/music/$(GameInfo.IdentityKey)", "$(GameInfo.DataPath)/music" },          RNF_USE_VMAP,  "-musdir",   "-musdir2" },
        { TEXTURES_RESOURCE_NAMESPACE_NAME,    { "$(GameInfo.DataPath)/textures/$(GameInfo.IdentityKey)", "$(GameInfo.DataPath)/textures" },    RNF_USE_VMAP,  "-texdir",   "-texdir2" },
        { FLATS_RESOURCE_NAMESPACE_NAME,       { "$(GameInfo.DataPath)/flats/$(GameInfo.IdentityKey)", "$(GameInfo.DataPath)/flats" },          RNF_USE_VMAP,  "-flatdir",  "-flatdir2" },
        { PATCHES_RESOURCE_NAMESPACE_NAME,     { "$(GameInfo.DataPath)/patches/$(GameInfo.IdentityKey)", "$(GameInfo.DataPath)/patches" },      RNF_USE_VMAP,  "-patdir",   "-patdir2" },
        { LIGHTMAPS_RESOURCE_NAMESPACE_NAME,   { "$(GameInfo.DataPath)/lightmaps/$(GameInfo.IdentityKey)", "$(GameInfo.DataPath)/lightmaps" },  RNF_USE_VMAP,  "-lmdir",    "-lmdir2" },
        { NULL }
    };
    size_t i;
    for(i = 0; defs[i].name; ++i)
    {
        struct namespacedef_s* def = &defs[i];
        dduri_t** searchPaths = 0;
        uint j, searchPathsCount = 0;

        for(searchPathsCount = 0; searchPathsCount < NAMESPACEDEF_MAX_SEARCHPATHS; ++searchPathsCount)
            if(!def->searchPaths[searchPathsCount])
                break;

        if((searchPaths = malloc(sizeof(*searchPaths) * searchPathsCount)) == 0)
            Con_Error("Error:createResourceNamespaces: Failed on allocation of %lu bytes.",
                      (unsigned long) (sizeof(*searchPaths) * searchPathsCount)); 

        for(j = 0; j < searchPathsCount; ++j)
            searchPaths[j] = Uri_Construct2(def->searchPaths[j], RC_NULL);

        createResourceNamespace(def->name, composeHashNameForFilePath, hashVarLengthNameIgnoreCase,
            searchPaths, searchPathsCount, def->flags, def->overrideName, def->overrideName2);

        for(j = 0; j < searchPathsCount; ++j)
            Uri_Destruct(searchPaths[j]);
        free(searchPaths);
    }

#undef NAMESPACEDEF_MAX_SEARCHPATHS
}

void F_InitResourceLocator(void)
{
    if(!inited)
    {   // First init.
        createResourceNamespaces();

        // Create the initial (empty) local file system path directory now.
        fsLocalPaths = PathDirectory_ConstructDefault();
    }

    // Allow re-init.
    resetAllNamespaces();
    inited = true;
}

void F_ShutdownResourceLocator(void)
{
    if(!inited)
        return;
    destroyAllNamespaces();
    PathDirectory_Destruct(fsLocalPaths); fsLocalPaths = 0;
    inited = false;
}

pathdirectory_t* F_LocalPaths(void)
{
    assert(inited);
    return fsLocalPaths;
}

struct resourcenamespace_s* F_ToResourceNamespace(resourcenamespaceid_t rni)
{
    assert(inited);
    return getNamespaceForId(rni);
}

resourcenamespaceid_t F_SafeResourceNamespaceForName(const char* name)
{
    assert(inited);
    return findNamespaceForName(name);
}

resourcenamespaceid_t F_ResourceNamespaceForName(const char* name)
{
    assert(inited);
    {
    resourcenamespaceid_t result;
    if((result = F_SafeResourceNamespaceForName(name)) == 0)
        Con_Error("resourceNamespaceForName: Failed to locate resource namespace \"%s\".", name);
    return result;
    }
}

uint F_NumResourceNamespaces(void)
{
    assert(inited);
    return numNamespaces;
}

boolean F_IsValidResourceNamespaceId(int val)
{
    assert(inited);
    return (boolean)(val>0 && (unsigned)val < (F_NumResourceNamespaces()+1)? 1 : 0);
}

dduri_t** F_CreateUriList(resourceclass_t rclass, const ddstring_t* _searchPaths)
{
    assert(rclass == RC_UNKNOWN || VALID_RESOURCE_CLASS(rclass));
    {
    const ddstring_t* searchPaths = _searchPaths;
    dduri_t* path, **list;
    const char* result = 0, *p;
    size_t numPaths, n;
    ddstring_t temp;

    if(!_searchPaths || Str_IsEmpty(_searchPaths))
        return 0;

    // Ensure the path list has the required terminating semicolon.
    if(Str_RAt(_searchPaths, 0) != ';')
    {
        Str_Init(&temp);
        Str_Appendf(&temp, "%s;", Str_Text(_searchPaths));
        searchPaths = &temp;
    }

    path = Uri_ConstructDefault();

    numPaths = 0;
    p = Str_Text(searchPaths);
    while((p = F_ParseSearchPath2(path, p, ';', rclass))) { ++numPaths; }

    n = 0;
    p = Str_Text(searchPaths);
    list = malloc((numPaths+1) * sizeof(*list));
    while((p = F_ParseSearchPath2(path, p, ';', rclass)))
    {
        list[n++] = Uri_ConstructCopy(path);
    }
    list[n] = 0; // Terminate.

    Uri_Destruct(path);
    if(searchPaths != _searchPaths)
        Str_Free(&temp);
    return list;
    }
}

void F_DestroyUriList(dduri_t** list)
{
    if(list)
    {
        dduri_t** ptr;
        for(ptr = list; *ptr; ptr++)
            Uri_Destruct(*ptr);
        free(list);
    }
}

uint F_FindResource4(resourceclass_t rclass, const dduri_t** searchPaths,
    ddstring_t* foundPath, const ddstring_t* optionalSuffix)
{
    assert(inited);
    if(rclass != RC_UNKNOWN && !VALID_RESOURCE_CLASS(rclass))
        Con_Error("F_FindResource: Invalid resource class %i.\n", rclass);
    if(!searchPaths)
        return 0;
    return findResource(rclass, searchPaths, optionalSuffix, foundPath);
}

uint F_FindResourceStr3(resourceclass_t rclass, const ddstring_t* searchPaths,
    ddstring_t* foundPath, const ddstring_t* optionalSuffix)
{
    assert(inited);
    {
    dduri_t** list;
    int result = 0;

    if(rclass != RC_UNKNOWN && !VALID_RESOURCE_CLASS(rclass))
        Con_Error("F_FindResource: Invalid resource class %i.\n", rclass);

    if(!searchPaths || Str_IsEmpty(searchPaths))
    {
#if _DEBUG
        Con_Message("F_FindResource: Invalid (NULL) search path, returning not-found.\n");
#endif
        return 0;
    }

    if((list = F_CreateUriList(rclass, searchPaths)) != 0)
    {
        result = findResource(rclass, list, optionalSuffix, foundPath);
        F_DestroyUriList(list);
    }
    return result;
    }
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
    assert(inited);
    if(type == RT_NONE)
        return RC_UNKNOWN;
    return getInfoForResourceType(type)->defaultClass;
}

resourcenamespaceid_t F_DefaultResourceNamespaceForClass(resourceclass_t rclass)
{
    assert(inited && VALID_RESOURCE_CLASS(rclass));
    return F_ResourceNamespaceForName(Str_Text(&defaultNamespaceForClass[rclass]));
}

resourcetype_t F_GuessResourceTypeByName(const char* path)
{
    assert(inited);

    if(!path || !path[0])
        return RT_NONE; // Unrecognizable.

    // We require a file extension for this.
    {char* ext;
    if((ext = strrchr(path, '.')))
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
    assert(inited && path);
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
        "RC_MUSIC"
    };
    return resourceClassNames[(int)rclass];
    }
}

void F_FileDir(const ddstring_t* str, directory2_t* dir)
{
    assert(str && dir);
    {
    filename_t fullPath, pth, drive;

    strncpy(pth, Str_Text(str), FILENAME_T_MAXLEN);
    _fullpath(fullPath, pth, FILENAME_T_MAXLEN);
    _splitpath(fullPath, drive, pth, 0, 0);

    Str_Clear(&dir->path);
    Str_Appendf(&dir->path, "%s%s", drive, pth);
#ifdef WIN32
    dir->drive = toupper(Str_At(&dir->path, 0)) - 'A' + 1;
#else
    dir->drive = 0;
#endif
    }
}

void F_FileName(ddstring_t* dest, const ddstring_t* src)
{
#ifdef WIN32
    char name[_MAX_FNAME];
#else
    char name[NAME_MAX];
#endif
    _splitpath(Str_Text(src), 0, 0, name, 0);
    Str_Set(dest, name);
}

void F_FileNameAndExtension(ddstring_t* dest, const ddstring_t* src)
{
#ifdef WIN32
    char name[_MAX_FNAME], ext[_MAX_EXT];
#else
    char name[NAME_MAX], ext[NAME_MAX];
#endif
    _splitpath(Str_Text(src), 0, 0, name, ext);
    Str_Clear(dest);
    Str_Appendf(dest, "%s%s", name, ext);
}

/// \todo dj: Find a suitable home for this.
const char* F_ParseSearchPath2(dduri_t* dest, const char* src, char delim,
    resourceclass_t defaultResourceClass)
{
    Uri_Clear(dest);
    if(src)
    {
        ddstring_t buf; Str_Init(&buf);
        for(; *src && *src != delim; ++src)
        {
            Str_PartAppend(&buf, src, 0, 1);
        }
        Uri_SetUri3(dest, Str_Text(&buf), defaultResourceClass);
        Str_Free(&buf);
    }
    if(!*src)
        return 0; // It ended.
    // Skip past the delimiter.
    return src + 1;
}

const char* F_ParseSearchPath(dduri_t* dest, const char* src, char delim)
{
    return F_ParseSearchPath2(dest, src, delim, RC_UNKNOWN);
}

/// \todo dj: Find a suitable home for this.
boolean F_FixSlashes(ddstring_t* str)
{
    assert(str);
    {
    boolean result = false;
    size_t len = Str_Length(str);
    if(len != 0)
    {
        char* path = Str_Text(str);
        size_t i;
        for(i = 0; i < len && path[i]; ++i)
        {
            if(path[i] != DIR_WRONG_SEP_CHAR)
                continue;
            path[i] = DIR_SEP_CHAR;
            result = true;
        }
    }
    return result;
    }
}

/// \todo dj: Find a suitable home for this.
void F_ResolveSymbolicPath(ddstring_t* dest, const ddstring_t* src)
{
    assert(dest && src);

    // Src path is base-relative?
    if(Str_At(src, 0) == DIR_SEP_CHAR)
    {
        boolean mustCopy = (dest == src);
        if(mustCopy)
        {
            ddstring_t buf;
            Str_Init(&buf);
            Str_Set(&buf, ddBasePath);
            Str_PartAppend(&buf, Str_Text(src), 1, Str_Length(src)-1);
            Str_Copy(dest, &buf);
            return;
        }

        Str_Set(dest, ddBasePath);
        Str_PartAppend(dest, Str_Text(src), 1, Str_Length(src)-1);
        return;
    }

    // Src path is workdir-relative.

    if(dest == src)
    {
        Str_Prepend(dest, ddRuntimeDir.path);
        return;
    }

    Str_Appendf(dest, "%s%s", ddRuntimeDir.path, Str_Text(src));
}

/// \todo dj: Find a suitable home for this.
boolean F_IsRelativeToBasePath(const ddstring_t* path)
{
    assert(path);
    return !strnicmp(Str_Text(path), ddBasePath, strlen(ddBasePath));
}

/// \todo dj: Find a suitable home for this.
boolean F_RemoveBasePath(ddstring_t* dest, const ddstring_t* absPath)
{
    assert(dest && absPath);

    if(F_IsRelativeToBasePath(absPath))
    {
        boolean mustCopy = (dest == absPath);
        if(mustCopy)
        {
            ddstring_t buf;
            Str_Init(&buf);
            Str_PartAppend(&buf, Str_Text(absPath), strlen(ddBasePath), Str_Length(absPath) - strlen(ddBasePath));
            Str_Copy(dest, &buf);
            Str_Free(&buf);
            return true;
        }

        Str_Clear(dest);
        Str_PartAppend(dest, Str_Text(absPath), strlen(ddBasePath), Str_Length(absPath) - strlen(ddBasePath));
        return true;
    }

    // Do we need to copy anyway?
    if(dest != absPath)
        Str_Copy(dest, absPath);

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
boolean F_PrependBasePath(ddstring_t* dest, const ddstring_t* src)
{
    assert(dest && src);

    if(!F_IsAbsolute(src))
    {
        if(dest != src)
            Str_Copy(dest, src);
        Str_Prepend(dest, ddBasePath);
        return true;
    }

    // Do we need to copy anyway?
    if(dest != src)
        Str_Copy(dest, src);

    return false; // Not done.
}

/// \todo dj: Find a suitable home for this.
boolean F_ExpandBasePath(ddstring_t* dest, const ddstring_t* src)
{
    assert(dest && src);
    if(Str_At(src, 0) == '>' || Str_At(src, 0) == '}')
    {
        boolean mustCopy = (dest == src);
        if(mustCopy)
        {
            ddstring_t buf;
            Str_Init(&buf); Str_Set(&buf, ddBasePath);
            Str_PartAppend(&buf, Str_Text(src), 1, Str_Length(src)-1);
            Str_Copy(dest, &buf);
            Str_Free(&buf);
            return true;
        }

        Str_Set(dest, ddBasePath);
        Str_PartAppend(dest, Str_Text(src), 1, Str_Length(src)-1);
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

            Str_Copy(dest, &buf);
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
            Str_Copy(dest, &buf);
            Str_Free(&buf);
        }
        Str_Free(&userName);
        if(result)
            return result;
        }
    }
#endif

    // Do we need to copy anyway?
    if(dest != src)
        Str_Copy(dest, src);

    // No expansion done.
    return false;
}

const ddstring_t* F_PrettyPath(const ddstring_t* path)
{
#define NUM_BUFS            8

    static ddstring_t buffers[NUM_BUFS]; // \fixme: never free'd!
    static uint index = 0;

    size_t basePathLen = strlen(ddBasePath), len = path != 0? Str_Length(path) : 0;
    if(len > basePathLen && !strnicmp(ddBasePath, Str_Text(path), basePathLen))
    {
        ddstring_t* str = &buffers[index++ % NUM_BUFS];
        Str_Free(str);
        F_RemoveBasePath(str, path);
        F_FixSlashes(str);
        return str;
    }
    else if(len > 1 && (Str_At(path, 0) == '}' || Str_At(path, 0) == '>'))
    {   // Skip over this special character.
        ddstring_t* str = &buffers[index++ % NUM_BUFS];
        Str_Free(str);
        Str_PartAppend(str, Str_Text(path), 1, Str_Length(path)-1);
        F_FixSlashes(str);
        return str;
    }

    return path; // We don't know how to make this prettier.

#undef NUM_BUFS
}

