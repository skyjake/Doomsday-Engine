/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "de_console.h"
#include "de_misc.h"

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

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean inited = false;

static const resourcetypeinfo_t typeInfo[NUM_RESOURCE_TYPES] = {
    /* RT_ZIP */ { RC_PACKAGE,      {"pk3", "zip", 0} },
    /* RT_WAD */ { RC_PACKAGE,      {"wad", 0} },
    /* RT_DED */ { RC_DEFINITION,   {"ded", 0} },
    /* RT_PNG */ { RC_GRAPHIC,      {"png", 0} },
    /* RT_TGA */ { RC_GRAPHIC,      {"tga", 0} },
    /* RT_PCX */ { RC_GRAPHIC,      {"pcx", 0} },
    /* RT_DMD */ { RC_MODEL,        {"dmd", 0} },
    /* RT_MD2 */ { RC_MODEL,        {"md2", 0} },
    /* RT_WAV */ { RC_SOUND,        {"wav", 0} },
    /* RT_OGG */ { RC_MUSIC,        {"ogg", 0} },
    /* RT_MP3 */ { RC_MUSIC,        {"mp3", 0} },
    /* RT_MOD */ { RC_MUSIC,        {"mod", 0} },
    /* RT_MID */ { RC_MUSIC,        {"mid", 0} },
    /* RT_DEH */ { RC_UNKNOWN,      {"deh", 0} }
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

static const char* defaultNamespaceForClass[NUM_RESOURCE_CLASSES] = {
    /* RC_PACKAGE */    "Packages",
    /* RC_DEFINITION */ "Defs",
    /* RC_GRAPHIC */    "Graphics",
    /* RC_MODEL */      "Models",
    /* RC_SOUND */      "Sfx",
    /* RC_MUSIC */      "Music"
};

static resourcenamespace_t namespaces[] = {
    { "Packages",    0,             "",         "",             "$(GameInfo.DataPath);}data\\;$(DOOMWADDIR)" },
    { "Defs",        0,             "",         "",             "$(GameInfo.DefsPath)\\$(GameInfo.IdentityKey);$(GameInfo.DefsPath);}defs\\" },
    { "Graphics",    0,             "-gfxdir",  "-gfxdir2",     "}data\\graphics" },
    { "Models",      RNF_USE_VMAP,  "-modeldir","-modeldir2",   "$(GameInfo.DataPath)\\models\\$(GameInfo.IdentityKey);$(GameInfo.DataPath)\\models" },
    { "Sfx",         RNF_USE_VMAP,  "-sfxdir",  "-sfxdir2",     "$(GameInfo.DataPath)\\sfx\\$(GameInfo.IdentityKey);$(GameInfo.DataPath)\\sfx" },
    { "Music",       RNF_USE_VMAP,  "-musdir",  "-musdir2",     "$(GameInfo.DataPath)\\music\\$(GameInfo.IdentityKey);$(GameInfo.DataPath)\\music" },
    { "Textures",    RNF_USE_VMAP,  "-texdir",  "-texdir2",     "$(GameInfo.DataPath)\\textures\\$(GameInfo.IdentityKey);$(GameInfo.DataPath)\\textures" },
    { "Flats",       RNF_USE_VMAP,  "-flatdir", "-flatdir2",    "$(GameInfo.DataPath)\\flats\\$(GameInfo.IdentityKey);$(GameInfo.DataPath)\\flats" },
    { "Patches",     RNF_USE_VMAP,  "-patdir",  "-patdir2",     "$(GameInfo.DataPath)\\patches\\$(GameInfo.IdentityKey);$(GameInfo.DataPath)\\patches" },
    { "LightMaps",   RNF_USE_VMAP,  "-lmdir",   "-lmdir2",      "$(GameInfo.DataPath)\\lightmaps\\$(GameInfo.IdentityKey);$(GameInfo.DataPath)\\lightmaps" }
};
static uint numNamespaces = sizeof(namespaces)/sizeof(namespaces[0]);

// CODE --------------------------------------------------------------------

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

static boolean tryFindFile(resourceclass_t rclass, const char* searchPath,
    char* foundPath, size_t foundPathLen, resourcenamespace_t* rnamespace)
{
    assert(inited && searchPath && searchPath[0]);

    // Is there a name hash we should use?
    { filehash_t* hash;
    if(rnamespace && (hash = ResourceNamespace_Hash(rnamespace)))
    {
        return FileHash_Find(hash, foundPath, searchPath, foundPathLen);
    }}

#if _DEBUG
    Con_Message("tryFindFile: Locating resource without name hash.\n");
#endif

    if(F_Access(searchPath))
    {
        if(foundPath && foundPathLen > 0)
            strncpy(foundPath, searchPath, foundPathLen);
        return true;
    }
    return false;
}

static boolean tryResourceFile(resourceclass_t rclass, const char* searchPath,
    char* foundPath, size_t foundPathLen, resourcenamespace_t* rnamespace)
{
    assert(inited && VALID_RESOURCE_CLASS(rclass) && searchPath && searchPath[0]);
    {
    boolean found = false;
    char* ptr;

    // Has an extension been specified?
    ptr = M_FindFileExtension((char*)searchPath);
    if(ptr && *ptr != '*') // Try this first.
        found = tryFindFile(rclass, searchPath, foundPath, foundPathLen, rnamespace);

    if(!found)
    {
        ddstring_t path2;
        Str_Init(&path2);

        // Create a copy of the searchPath minus file extension.
        if(ptr)
        {
            Str_PartAppend(&path2, searchPath, 0, ptr - searchPath);
        }
        else
        {
            Str_Set(&path2, searchPath);
            Str_AppendChar(&path2, '.');
        }

        if(searchTypeOrder[rclass][0] != RT_NONE)
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
                        Str_Copy(&tmp, &path2);
                        Str_Appendf(&tmp, "%s", *ext);
                        found = tryFindFile(rclass, Str_Text(&tmp), foundPath, foundPathLen, rnamespace);
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

static boolean findResource(resourceclass_t rclass, const char* searchPath, const char* optionalSuffix,
    char* foundPath, size_t foundPathLen, resourcenamespace_t* rnamespace)
{
    assert(inited && VALID_RESOURCE_CLASS(rclass) && searchPath && searchPath[0]);
    {
    boolean found = false;

    // First try with the optional suffix.
    if(optionalSuffix)
    {
        ddstring_t fn;
        Str_Init(&fn);

        // Has an extension been specified?
        { char* ptr = M_FindFileExtension((char*)searchPath);
        if(ptr && *ptr != '*')
        {
            char ext[10];
            strncpy(ext, ptr, 10);
            Str_PartAppend(&fn, searchPath, 0, ptr - 1 - searchPath);
            Str_Append(&fn, optionalSuffix);
            Str_Append(&fn, ext);
        }
        else
        {
            Str_Set(&fn, searchPath);
            Str_Append(&fn, optionalSuffix);
        }}

        if(tryResourceFile(rclass, Str_Text(&fn), foundPath, foundPathLen, rnamespace))
            found = true;

        Str_Free(&fn);
    }

    // Try without a suffix.
    if(!found)
    {
        if(tryResourceFile(rclass, searchPath, foundPath, foundPathLen, rnamespace))
            found = true;
    }

    return found;
    }
}

static boolean tryLocateResource2(resourceclass_t rclass, resourcenamespace_t* rnamespace, const char* searchPath,
    const char* optionalSuffix, char* foundPath, size_t foundPathLen)
{
    assert(inited && VALID_RESOURCE_CLASS(rclass) && searchPath && searchPath[0]);
    {
    ddstring_t name;
    boolean found = false;

    // Fix directory seperators early.
    Str_Init(&name);
    Str_Set(&name, searchPath);
    Dir_FixSlashes(Str_Text(&name), Str_Length(&name));

#if _DEBUG
    if(rnamespace)
        VERBOSE2( Con_Message("Using rnamespace \"%s\" ...\n", ResourceNamespace_Name(rnamespace)) );
#endif

    // If this is an absolute path, locate using it.
    if(Dir_IsAbsolute(Str_Text(&name)))
    {
        found = findResource(rclass, Str_Text(&name), optionalSuffix, foundPath, foundPathLen, 0);
    }
    /*else
    {   // Else, prepend the base path and try that.
        ddstring_t fn;
        const char* path;

        // Make this an absolute, base-relative path.
        // If only checking the base path and not the expected location
        // for the resource class (below); re-use the current string.
        if(rnamespace)
        {
            Str_Init(&fn);
            Str_Copy(&fn, &name);
            Str_Prepend(&fn, ddBasePath);
            path = Str_Text(&fn);
        }
        else
        {
            Str_Prepend(&name, ddBasePath);
            path = Str_Text(&name);
        }

        // Try loading using the base path as the starting point.
        found = findResource(rclass, path, optionalSuffix, foundPath, foundPathLen, 0);

        if(rnamespace)
            Str_Free(&fn);
    }*/

    // Try expected location for this resource class.
    if(!found && rnamespace)
    {
        found = findResource(rclass, Str_Text(&name), optionalSuffix, foundPath, foundPathLen, rnamespace);
    }

    Str_Free(&name);

    return found;
    }
}

static boolean tryLocateResource(resourceclass_t rclass, const char* searchPath,
    const char* optionalSuffix, char* foundPath, size_t foundPathLen)
{
    ddstring_t tmp;
    Str_Init(&tmp);

    // Has a namespace identifier been included?
    { const char* p;
    if((p = Str_CopyDelim(&tmp, searchPath, ':')))
    {
        resourcenamespaceid_t rni;
        if((rni = F_SafeResourceNamespaceForName(Str_Text(&tmp))) != 0)
        {
            boolean result = tryLocateResource2(rclass, getNamespaceForId(rni), p, optionalSuffix, foundPath, foundPathLen);
            Str_Free(&tmp);
            return result;
        }
        else
        {
            Con_Message("tryLocateResource: Unknown namespace \"%s\" in searchPath \"%s\", using default for %s.\n", Str_Text(&tmp), searchPath, F_ResourceClassStr(rclass));
        }
    }}
    Str_Free(&tmp);

    // Use the default namespace for this resource class.
    return tryLocateResource2(rclass, getNamespaceForId(F_ResourceNamespaceForName(defaultNamespaceForClass[rclass])), searchPath, optionalSuffix, foundPath, foundPathLen);
}

void F_InitResourceLocator(void)
{
    // Allow re-init.
    resetAllNamespaces();
    inited = true;
}

void F_ShutdownResourceLocator(void)
{
    if(!inited)
        return;
    resetAllNamespaces();
    inited = false;
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

boolean F_FindResource(resourceclass_t rclass, char* foundPath, const char* _searchPaths, const char* optionalSuffix, size_t foundPathLen)
{
    assert(inited);
    if(!_searchPaths || !_searchPaths[0])
        return false;
    if(!VALID_RESOURCE_CLASS(rclass))
        Con_Error("F_FindResource: Invalid resource class %i.\n", rclass);

    // Add an auto-identification file name list to the info record.
    { ddstring_t searchPaths, path;
    boolean found = false;

    // Ensure the searchPath list has the required terminating semicolon.
    Str_Init(&searchPaths); Str_Set(&searchPaths, _searchPaths);
    if(Str_RAt(&searchPaths, 0) != ';')
        Str_Append(&searchPaths, ";");

    Str_Init(&path);
    { const char* p = Str_Text(&searchPaths);
    while((p = Str_CopyDelim(&path, p, ';')) &&
          !(found = tryLocateResource(rclass, Str_Text(&path), optionalSuffix, foundPath, foundPathLen)));
    }

    Str_Free(&path);
    Str_Free(&searchPaths);

    return found;
    }
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
    return F_ResourceNamespaceForName(defaultNamespaceForClass[rclass]);
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
            if(Str_RAt(&buf, 0) != '/')
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
        if((p = Str_CopyDelim(&userName, p, DIR_SEP_CHAR)))
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
