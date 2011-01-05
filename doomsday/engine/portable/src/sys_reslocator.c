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
 * Routines for locating external resources.
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

#include "filedirectory.h"
#include "resourcenamespace.h"

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
static const resourcetype_t searchTypeOrder[NUM_RESOURCE_CLASSES][MAX_TYPEORDER] = {
    /* RC_PACKAGE */    { RT_ZIP, RT_WAD, 0 }, // Favor ZIP over WAD.
    /* RC_DEFINITION */ { RT_DED, 0 }, // Only DED files.
    /* RC_GRAPHIC */    { RT_PNG, RT_TGA, RT_PCX, 0 }, // Favour quality.
    /* RC_MODEL */      { RT_DMD, RT_MD2, 0 }, // Favour DMD over MD2.
    /* RC_SOUND */      { RT_WAV, 0 }, // Only WAV files.
    /* RC_MUSIC */      { RT_OGG, RT_MP3, RT_WAV, RT_MOD, RT_MID, 0 }
};

static const ddstring_t defaultNamespaceForClass[NUM_RESOURCE_CLASSES] = {
    /* RC_PACKAGE */    { "Packages" },
    /* RC_DEFINITION */ { "Defs" },
    /* RC_GRAPHIC */    { "Graphics" },
    /* RC_MODEL */      { "Models" },
    /* RC_SOUND */      { "Sfx" },
    /* RC_MUSIC */      { "Music" }
};

static resourcenamespace_t namespaces[] = {
    { "Packages",    0,             "",         "",             "$(GameInfo.DataPath);}data/;$(DOOMWADDIR)", composeHashNameForFilePath, hashVarLengthNameIgnoreCase },
    { "Defs",        0,             "",         "",             "$(GameInfo.DefsPath)/$(GameInfo.IdentityKey);$(GameInfo.DefsPath);}defs/", composeHashNameForFilePath, hashVarLengthNameIgnoreCase },
    { "Graphics",    0,             "-gfxdir",  "-gfxdir2",     "}data/graphics", composeHashNameForFilePath, hashVarLengthNameIgnoreCase },
    { "Models",      RNF_USE_VMAP,  "-modeldir","-modeldir2",   "$(GameInfo.DataPath)/models/$(GameInfo.IdentityKey);$(GameInfo.DataPath)/models", composeHashNameForFilePath, hashVarLengthNameIgnoreCase },
    { "Sfx",         RNF_USE_VMAP,  "-sfxdir",  "-sfxdir2",     "$(GameInfo.DataPath)/sfx/$(GameInfo.IdentityKey);$(GameInfo.DataPath)/sfx", composeHashNameForFilePath, hashVarLengthNameIgnoreCase },
    { "Music",       RNF_USE_VMAP,  "-musdir",  "-musdir2",     "$(GameInfo.DataPath)/music/$(GameInfo.IdentityKey);$(GameInfo.DataPath)/music", composeHashNameForFilePath, hashVarLengthNameIgnoreCase },
    { "Textures",    RNF_USE_VMAP,  "-texdir",  "-texdir2",     "$(GameInfo.DataPath)/textures/$(GameInfo.IdentityKey);$(GameInfo.DataPath)/textures", composeHashNameForFilePath, hashVarLengthNameIgnoreCase },
    { "Flats",       RNF_USE_VMAP,  "-flatdir", "-flatdir2",    "$(GameInfo.DataPath)/flats/$(GameInfo.IdentityKey);$(GameInfo.DataPath)/flats", composeHashNameForFilePath, hashVarLengthNameIgnoreCase },
    { "Patches",     RNF_USE_VMAP,  "-patdir",  "-patdir2",     "$(GameInfo.DataPath)/patches/$(GameInfo.IdentityKey);$(GameInfo.DataPath)/patches", composeHashNameForFilePath, hashVarLengthNameIgnoreCase },
    { "LightMaps",   RNF_USE_VMAP,  "-lmdir",   "-lmdir2",      "$(GameInfo.DataPath)/lightmaps/$(GameInfo.IdentityKey);$(GameInfo.DataPath)/lightmaps", composeHashNameForFilePath, hashVarLengthNameIgnoreCase }
};
static uint numNamespaces = sizeof(namespaces)/sizeof(namespaces[0]);

/// File system directories on the host system.
static filedirectory_t* fileDirectory;

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
    return &namespaces[((uint)rni)-1];
}

static resourcenamespaceid_t findNamespaceForName(const char* name)
{
    if(name && name[0])
    {
        uint i;
        for(i = 0; i < numNamespaces; ++i)
        {
            resourcenamespace_t* rnamespace = &namespaces[i];
            if(!stricmp(ResourceNamespace_Name(rnamespace), name))
                return (resourcenamespaceid_t)(i+1);
        }
    }
    return 0;
}

static void resetAllNamespaces(void)
{
    uint i;
    for(i = 0; i < numNamespaces; ++i)
        ResourceNamespace_Reset(&namespaces[i]);
}

static void clearNamespaceSearchPaths(resourcenamespaceid_t rni)
{
    if(rni == 0)
    {
        uint i;
        for(i = 0; i < numNamespaces; ++i)
            ResourceNamespace_ClearSearchPaths(&namespaces[i]);
        return;
    }
    ResourceNamespace_ClearSearchPaths(getNamespaceForId(rni));
}

static boolean tryFindResource2(resourceclass_t rclass, const ddstring_t* searchPath,
    ddstring_t* foundPath, resourcenamespace_t* rnamespace)
{
    assert(inited && searchPath && !Str_IsEmpty(searchPath));

    // Is there a namespace we should use?
    if(rnamespace)
    {
        boolean result;
        if((result = ResourceNamespace_Find2(rnamespace, searchPath, foundPath)))
            F_PrependBasePath(foundPath, foundPath);
        return result;
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
    VERBOSE2( Con_Message("Using rnamespace \"%s\" ...\n", rnamespace? ResourceNamespace_Name(rnamespace) : "None") );
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

static const char* findResource(resourceclass_t rclass, const ddstring_t* searchPaths,
    const ddstring_t* optionalSuffix, ddstring_t* foundPath)
{
    assert(inited && (rclass == RC_UNKNOWN || VALID_RESOURCE_CLASS(rclass)));
    assert(searchPaths && !Str_IsEmpty(searchPaths));
    {
    dduri_t* searchPath;
    const char* result = 0;

    // Ensure the searchPath list has the required terminating semicolon.
    searchPath = Uri_ConstructDefault();

    { const char* p = Str_Text(searchPaths);
    size_t numParsedCharacters = 0;
    while((p = F_ParseSearchPath2(searchPath, p, ';', rclass)))
    {
        ddstring_t* resolvedPath;
        boolean found = false;

        if((resolvedPath = Uri_Resolved(searchPath)) == 0)
            continue; // Incomplete path; ignore it.

        // If this is an absolute path, locate using it.
        if(F_IsAbsolute(resolvedPath))
        {
            found = findResource2(rclass, resolvedPath, optionalSuffix, foundPath, 0);
        }
        else
        {   // Probably a relative path.
            // Has a namespace identifier been included?
            if(!Str_IsEmpty(Uri_Scheme(searchPath)))
            {
                resourcenamespaceid_t rni;
                if((rni = F_SafeResourceNamespaceForName(Str_Text(Uri_Scheme(searchPath)))) != 0)
                {
                    found = findResource2(rclass, resolvedPath, optionalSuffix, foundPath, getNamespaceForId(rni));
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

        if(found)
        {
            result = Str_Text(searchPaths) + numParsedCharacters;
            break;
        }
        numParsedCharacters = (p - Str_Text(searchPaths));
    }}

    if(searchPath)
        Uri_Destruct(searchPath);

    return result;
    }
}

void F_InitResourceLocator(void)
{
    if(!inited)
    {   // First init.
        uint i;
        for(i = 0; i < numNamespaces; ++i)
        {
            resourcenamespace_t* rnamespace = &namespaces[i];
            Str_Init(&rnamespace->_extraSearchPaths);
            memset(rnamespace->_pathHash, 0, sizeof(rnamespace->_pathHash));
        }

        // Create the initial (empty) FileDirectory now.
        fileDirectory = FileDirectory_ConstructDefault();
    }

    // Allow re-init.
    resetAllNamespaces();
    inited = true;
}

void F_ShutdownResourceLocator(void)
{
    if(!inited)
        return;
    resetAllNamespaces();
    FileDirectory_Destruct(fileDirectory); fileDirectory = 0;
    inited = false;
}

filedirectory_t* F_FileDirectory(void)
{
    assert(inited);
    return fileDirectory;
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

const char* F_FindResourceStr3(resourceclass_t rclass, const ddstring_t* _searchPaths,
    ddstring_t* foundPath, const ddstring_t* optionalSuffix)
{
    assert(inited);
    {
    ddstring_t searchPaths;
    const char* result;

    if(rclass != RC_UNKNOWN && !VALID_RESOURCE_CLASS(rclass))
        Con_Error("F_FindResource: Invalid resource class %i.\n", rclass);

    if(!_searchPaths || Str_IsEmpty(_searchPaths))
    {
#if _DEBUG
        Con_Message("F_FindResource: Invalid (NULL) search path, returning not-found.\n");
#endif
        return 0;
    }

    // Ensure the searchPath list has the required terminating semicolon.
    Str_Init(&searchPaths); Str_Copy(&searchPaths, _searchPaths);
    if(Str_RAt(&searchPaths, 0) != ';')
        Str_AppendChar(&searchPaths, ';');

    if(0 != (result = findResource(rclass, &searchPaths, optionalSuffix, foundPath)))
    {
        result = Str_Text(_searchPaths) + (result - Str_Text(&searchPaths));
    }
    Str_Free(&searchPaths);
    return result;
    }
}

const char* F_FindResourceStr2(resourceclass_t rclass, const ddstring_t* searchPath,
    ddstring_t* foundPath)
{
    return F_FindResourceStr3(rclass, searchPath, foundPath, 0);
}

const char* F_FindResourceStr(resourceclass_t rclass, const ddstring_t* searchPath)
{
    return F_FindResourceStr2(rclass, searchPath, 0);
}

const char* F_FindResource3(resourceclass_t rclass, const char* _searchPaths,
    ddstring_t* foundPath, const char* _optionalSuffix)
{
    ddstring_t searchPaths, optionalSuffix;
    boolean hasOptionalSuffix = false;
    const char* result;
    Str_Init(&searchPaths); Str_Set(&searchPaths, _searchPaths);
    if(_optionalSuffix && _optionalSuffix[0])
    {
        Str_Init(&optionalSuffix); Str_Set(&optionalSuffix, _optionalSuffix);
        hasOptionalSuffix = true;
    }
    if(0 != (result = F_FindResourceStr3(rclass, &searchPaths, foundPath, hasOptionalSuffix? &optionalSuffix : 0)))
    {
        result = _searchPaths + (result - Str_Text(&searchPaths));
    }
    if(hasOptionalSuffix)
        Str_Free(&optionalSuffix);
    Str_Free(&searchPaths);
    return result;
}

const char* F_FindResource2(resourceclass_t rclass, const char* searchPaths, ddstring_t* foundPath)
{
    return F_FindResource3(rclass, searchPaths, foundPath, 0);
}

const char* F_FindResource(resourceclass_t rclass, const char* searchPaths)
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
    while(i < numNamespaces && !(result = ResourceNamespace_MapPath(&namespaces[i++], path)));
    return result;
    }
}

const char* F_ResourceClassStr(resourceclass_t rclass)
{
    assert(VALID_RESOURCE_CLASS(rclass));
    {
    static const char* resourceClassNames[NUM_RESOURCE_CLASSES] = {
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
