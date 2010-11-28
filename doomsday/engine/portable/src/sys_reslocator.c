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
 * r_extres.c: External Resources.
 *
 * Routines for locating external resource files.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_system.h"
#include "de_console.h"
#include "de_refresh.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

#define MAX_EXTENSIONS          (3)
typedef struct {
    char* extensions[MAX_EXTENSIONS];
} resourcetypeinfo_t;

/**
 * Resource Type.
 * @ingroup fs
 */
typedef enum {
    RT_NONE = 0,
    RT_FIRST = 1,
    RT_ZIP = RT_FIRST,
    RT_WAD,
    RT_DED,
    RT_PNG,
    RT_TGA,
    RT_PCX,
    RT_DMD,
    RT_MD2,
    RT_WAV,
    RT_OGG,
    RT_MP3,
    RT_MOD,
    RT_MID,
    RT_LAST_INDEX
} resourcetype_t;

#define NUM_RESOURCE_TYPES          (RT_LAST_INDEX-1)
#define VALID_RESOURCE_TYPE(v)      ((v) >= RT_FIRST && (v) < RT_LAST_INDEX)

/**
 * Search Path Identifier.
 * @ingroup fs
 */
typedef enum {
    SPI_NONE = 0, // Not a real path.
    SPI_FIRST = 1,
    SPI_BASEPATH = SPI_FIRST, // i.e., "}"
    SPI_BASEPATH_DATA, // i.e., "}data/"
    SPI_BASEPATH_DEFS, // i.e., "}defs/"
    SPI_GAMEPATH_DATA, // e.g., "}data/jdoom/"
    SPI_GAMEPATH_DEFS, // e.g., "}defs/jdoom/"
    SPI_GAMEMODEPATH_DATA, // e.g., "}data/jdoom/doom2-plut/"
    SPI_GAMEMODEPATH_DEFS, // e.g., "}defs/jdoom/doom2-plut/" 
    SPI_DOOMWADDIR
} searchpathid_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean inited = false;

static const resourcetypeinfo_t typeInfo[NUM_RESOURCE_TYPES] = {
    /* RT_ZIP */ { {"pk3", "zip", 0} },
    /* RT_WAD */ { {"wad", 0} },
    /* RT_DED */ { {"ded", 0} },
    /* RT_PNG */ { {"png", 0} },
    /* RT_TGA */ { {"tga", 0} },
    /* RT_PCX */ { {"pcx", 0} },
    /* RT_DMD */ { {"dmd", 0} },
    /* RT_MD2 */ { {"md2", 0} },
    /* RT_WAV */ { {"wav", 0} },
    /* RT_OGG */ { {"ogg", 0} },
    /* RT_MP3 */ { {"mp3", 0} },
    /* RT_MOD */ { {"mod", 0} },
    /* RT_MID */ { {"mid", 0} }
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
    /* RC_PACKAGE */    "packages:",
    /* RC_DEFINITION */ "defs:",
    /* RC_GRAPHIC */    "graphics:",
    /* RC_MODEL */      "models:",
    /* RC_SOUND */      "sounds:",
    /* RC_MUSIC */      "music:"
};

static resourcenamespace_t namespaces[NUM_RESOURCE_CLASSES] = {
    { "packages:", 0 },
    { "defs:", 0 },
    { "graphics:", 0 },
    { "models:", 0 },
    { "sounds:", 0 },
    { "music:", 0 }
};

/// Lists of search paths to use when locating file resources.
static ddstring_t searchPathLists[NUM_RESOURCE_CLASSES];

// CODE --------------------------------------------------------------------

static __inline const resourcetypeinfo_t* getInfoForResourceType(resourcetype_t type)
{
    assert(VALID_RESOURCE_TYPE(type));
    return &typeInfo[((uint)type)-1];
}

static __inline void clearSearchPathList(resourceclass_t rclass)
{
    assert(VALID_RESOURCE_CLASS(rclass));
    Str_Free(&searchPathLists[rclass]);
}

/**
 * @param searchOrder       Resource path search order (in order of least-importance, left to right).
 * @param overrideFlag      Command line options for setting the resource path explicitly.
 * @param overrideFlag2     Takes precendence.
 */
static void formSearchPathList(resourceclass_t rclass, const ddstring_t* identityKey,
    const ddstring_t* defsPath, const ddstring_t* dataPath, const searchpathid_t* searchOrder,
    const char* defaultPath, const char* overrideFlag, const char* overrideFlag2)
{
    assert(VALID_RESOURCE_CLASS(rclass) && searchOrder);
    {
    ddstring_t* pathList = &searchPathLists[rclass];
    boolean usingGameModePathData = false;
    boolean usingGameModePathDefs = false;

    Str_Clear(pathList);

    if(overrideFlag && overrideFlag[0] && ArgCheckWith(overrideFlag, 1))
    {   // A command line override of the default path.
        filename_t newPath;
        M_TranslatePath(newPath, ArgNext(), FILENAME_T_MAXLEN);
        Dir_ValidDir(newPath, FILENAME_T_MAXLEN);
        Str_Prepend(pathList, ";"); Str_Prepend(pathList, newPath);
    }

    { size_t i;
    for(i = 0; searchOrder[i] != SPI_NONE; ++i)
    {
        switch(searchOrder[i])
        {
        case SPI_BASEPATH:
            { filename_t newPath;
            M_TranslatePath(newPath, "}", FILENAME_T_MAXLEN);
            F_AddResourceSearchPath(rclass, newPath, false);
            break;
            }
        case SPI_BASEPATH_DATA:
            { filename_t newPath;
            if(defaultPath && defaultPath[0])
            {
                filename_t other;
                dd_snprintf(other, FILENAME_T_MAXLEN, DD_BASEPATH_DATA"%s", defaultPath);
                M_TranslatePath(newPath, other, FILENAME_T_MAXLEN);
            }
            else
            {
                M_TranslatePath(newPath, DD_BASEPATH_DATA, FILENAME_T_MAXLEN);
            }
            F_AddResourceSearchPath(rclass, newPath, false);
            break;
            }
        case SPI_BASEPATH_DEFS:
            { filename_t newPath;
            if(defaultPath && defaultPath[0])
            {
                filename_t other;
                dd_snprintf(other, FILENAME_T_MAXLEN, DD_BASEPATH_DEFS"%s", defaultPath);
                M_TranslatePath(newPath, other, FILENAME_T_MAXLEN);
            }
            else
            {
                M_TranslatePath(newPath, DD_BASEPATH_DEFS, FILENAME_T_MAXLEN);
            }
            F_AddResourceSearchPath(rclass, newPath, false);
            break;
            }
        case SPI_GAMEPATH_DATA:
            if(defaultPath && defaultPath[0])
            {
                filename_t newPath;
                dd_snprintf(newPath, FILENAME_T_MAXLEN, "%s%s", Str_Text(dataPath), defaultPath);
                F_AddResourceSearchPath(rclass, newPath, false);
            }
            else
            {
                F_AddResourceSearchPath(rclass, Str_Text(dataPath), false);
            }
            break;
        case SPI_GAMEPATH_DEFS:
            if(defaultPath && defaultPath[0])
            {
                filename_t newPath;
                dd_snprintf(newPath, FILENAME_T_MAXLEN, "%s%s", Str_Text(defsPath), defaultPath);
                F_AddResourceSearchPath(rclass, newPath, false);
            }
            else
            {
                F_AddResourceSearchPath(rclass, Str_Text(defsPath), false);
            }
            break;
        case SPI_GAMEMODEPATH_DATA:
            usingGameModePathData = true;
            if(defaultPath && defaultPath[0] && Str_Length(identityKey))
            {
                filename_t newPath;
                dd_snprintf(newPath, FILENAME_T_MAXLEN, "%s%s%s", Str_Text(dataPath), defaultPath, Str_Text(identityKey));
                F_AddResourceSearchPath(rclass, newPath, false);
            }
            break;
        case SPI_GAMEMODEPATH_DEFS:
            usingGameModePathDefs = true;
            if(Str_Length(identityKey))
            {
                if(defaultPath && defaultPath[0])
                {
                    filename_t newPath;
                    dd_snprintf(newPath, FILENAME_T_MAXLEN, "%s%s", Str_Text(defsPath), Str_Text(identityKey));
                    F_AddResourceSearchPath(rclass, newPath, false);
                }
                else
                {
                    filename_t newPath;
                    dd_snprintf(newPath, FILENAME_T_MAXLEN, "%s%s%s", Str_Text(defsPath), defaultPath, Str_Text(identityKey));
                    F_AddResourceSearchPath(rclass, newPath, false);
                }
            }
            break;
        case SPI_DOOMWADDIR:
            if(!ArgCheck("-nowaddir") && getenv("DOOMWADDIR"))
            {
                filename_t newPath;
                M_TranslatePath(newPath, getenv("DOOMWADDIR"), FILENAME_T_MAXLEN);
                F_AddResourceSearchPath(rclass, newPath, false);
            }
            break;

        default: Con_Error("formSearchPathList: Invalid path ident %i.", searchOrder[i]);
        };
    }}

    // The overriding path.
    if(overrideFlag2 && overrideFlag2[0] && ArgCheckWith(overrideFlag2, 1))
    {
        filename_t newPath;
        M_TranslatePath(newPath, ArgNext(), FILENAME_T_MAXLEN);
        F_AddResourceSearchPath(rclass, newPath, false);

        if((usingGameModePathData || usingGameModePathDefs) && Str_Length(identityKey))
        {
            filename_t other;
            dd_snprintf(other, FILENAME_T_MAXLEN, "%s/%s", newPath, Str_Text(identityKey));
            F_AddResourceSearchPath(rclass, other, false);
        }
    }
    }
}

static void collateSearchPathSet(gameinfo_t* info, resourceclass_t rclass)
{
#define MAX_PATHS                   4

    assert(info && VALID_RESOURCE_CLASS(rclass));
    {
    struct searchpath_s {
        resourceclass_t rclass;
        searchpathid_t searchOrder[MAX_PATHS];
        const char* defaultPath;
        const char* overrideFlag, *overrideFlag2;
    } static const paths[] = {
        { RC_PACKAGE,    { SPI_DOOMWADDIR, SPI_BASEPATH_DATA, SPI_GAMEPATH_DATA, 0 }, "", "", "" },
        { RC_DEFINITION, { SPI_BASEPATH_DEFS, SPI_GAMEPATH_DEFS, SPI_GAMEMODEPATH_DEFS, 0 }, "", "-defdir", "-defdir2" },
        { RC_GRAPHIC,    { SPI_BASEPATH_DATA, 0 }, "graphics\\", "-gfxdir", "-gfxdir2" },
        { RC_MODEL,      { SPI_GAMEPATH_DATA, SPI_GAMEMODEPATH_DATA, 0 }, "models\\", "-modeldir", "-modeldir2" },
        { RC_SOUND,      { SPI_GAMEPATH_DATA, SPI_GAMEMODEPATH_DATA, 0 }, "sfx\\", "-sfxdir", "-sfxdir2" },
        { RC_MUSIC,      { SPI_GAMEPATH_DATA, SPI_GAMEMODEPATH_DATA, 0 }, "music\\", "-musdir", "-musdir2" },
        /*
        { RC_GRAPHIC,   { SPI_GAMEPATH_DATA, SPI_GAMEMODEPATH_DATA, 0 }, "textures\\", "-texdir", "-texdir2" },
        { RC_GRAPHIC,   { SPI_GAMEPATH_DATA, SPI_GAMEMODEPATH_DATA, 0 }, "flats\\", "-flatdir", "-flatdir2" },
        { RC_GRAPHIC,   { SPI_GAMEPATH_DATA, SPI_GAMEMODEPATH_DATA, 0 }, "patches\\", "-patdir", "-patdir2" },
        { RC_GRAPHIC,   { SPI_GAMEPATH_DATA, SPI_GAMEMODEPATH_DATA, 0 }, "lightmaps\\", "-lmdir", "-lmdir2" },
        { RC_GRAPHIC,   { SPI_GAMEPATH_DATA, SPI_GAMEMODEPATH_DATA, 0 }, "flares\\", "-flaredir", "-flaredir2" },
        */
        { RC_UNKNOWN }
    };
    const ddstring_t* identityKey = GameInfo_IdentityKey(info);
    const ddstring_t* defsPath = GameInfo_DefsPath(info);
    const ddstring_t* dataPath = GameInfo_DataPath(info);
    size_t i;
    for(i = 0; paths[i].rclass != RC_UNKNOWN; ++i)
    {
        if(paths[i].rclass == rclass)
            formSearchPathList(rclass, identityKey, defsPath, dataPath, paths[i].searchOrder, paths[i].defaultPath, paths[i].overrideFlag, paths[i].overrideFlag2);
    }
    }

#undef MAX_PATHS
}

static boolean tryFindFile(resourceclass_t rclass, const char* searchPath,
    char* foundPath, size_t foundPathLen, resourcenamespaceid_t rni)
{
    assert(inited && searchPath && searchPath[0]);

    { resourcenamespace_t* rnamespace;
    if(rni != 0 && (rnamespace = F_ToResourceNamespace(rni)))
    {
#if _DEBUG
        VERBOSE2( Con_Message("Using filehash for rnamespace \"%s\" ...\n", rnamespace->_name) );
#endif
        if(!rnamespace->_fileHash)
        {
            collateSearchPathSet(DD_GameInfo(), rclass);
            rnamespace->_fileHash = FileHash_Create(Str_Text(&searchPathLists[rclass]));
        }
        return FileHash_Find(rnamespace->_fileHash, foundPath, searchPath, foundPathLen);
    }}

    if(F_Access(searchPath))
    {
        if(foundPath && foundPathLen > 0)
            strncpy(foundPath, searchPath, foundPathLen);
        return true;
    }
    return false;
}

/**
 * Check all possible extensions to see if the resource exists.
 *
 * @param rclass        Class of resource being searched for.
 * @param searchPath    File name/path to search for.
 * @param foundPath     Located path if found will be written back here.
 *                      Can be @c NULL, in which case this is just a boolean query.
 * @param foundPathLen  Length of the @a foundFileName buffer in bytes.
 * @param rni           If non-zero, the namespace to use when searching.
 *
 * @return              @c true, if it's found.
 */
static boolean tryResourceFile(resourceclass_t rclass, const char* searchPath,
    char* foundPath, size_t foundPathLen, resourcenamespaceid_t rni)
{
    assert(inited && VALID_RESOURCE_CLASS(rclass) && searchPath && searchPath[0]);
    {
    boolean found = false;
    char* ptr;

    // Has an extension been specified?
    ptr = M_FindFileExtension((char*)searchPath);
    if(ptr && *ptr != '*') // Try this first.
        found = tryFindFile(rclass, searchPath, foundPath, foundPathLen, rni);

    if(!found)
    {
        ddstring_t path2, tmp;

        Str_Init(&path2);
        Str_Init(&tmp);

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
            do
            {
                const resourcetypeinfo_t* info = getInfoForResourceType(*type);
                if(info->extensions[0])
                {
                    char* const* ext = info->extensions;
                    do
                    {
                        Str_Copy(&tmp, &path2);
                        Str_Appendf(&tmp, "%s", *ext);
                        found = tryFindFile(rclass, Str_Text(&tmp), foundPath, foundPathLen, rni);
                    } while(!found && *(++ext));
                }
            } while(!found && *(++type) != RT_NONE);
        }

        Str_Free(&path2);
        Str_Free(&tmp);
    }

    return found;
    }
}

static boolean findResource(resourceclass_t rclass, const char* searchPath, const char* optionalSuffix,
    char* foundPath, size_t foundPathLen, resourcenamespaceid_t rni)
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

        if(tryResourceFile(rclass, Str_Text(&fn), foundPath, foundPathLen, rni))
            found = true;

        Str_Free(&fn);
    }

    // Try without a suffix.
    if(!found)
    {
        if(tryResourceFile(rclass, searchPath, foundPath, foundPathLen, rni))
            found = true;
    }

    return found;
    }
}

static boolean tryLocateResource(resourceclass_t rclass, resourcenamespaceid_t rni, const char* searchPath,
    const char* optionalSuffix, char* foundPath, size_t foundPathLen)
{
    assert(inited && VALID_RESOURCE_CLASS(rclass) && searchPath && searchPath[0]);
    {
    ddstring_t name;
    boolean found;

    // Fix directory seperators early.
    Str_Init(&name);
    Str_Set(&name, searchPath);
    Dir_FixSlashes(Str_Text(&name), Str_Length(&name));

    // If this is an absolute path, locate using it.
    if(Dir_IsAbsolute(Str_Text(&name)))
    {
        found = findResource(rclass, Str_Text(&name), optionalSuffix, foundPath, foundPathLen, 0);
    }
    else
    {   // Else, prepend the base path and try that.
        ddstring_t fn;
        const char* path;

        // Make this an absolute, base-relative path.
        // If only checking the base path and not the expected location
        // for the resource class (below); re-use the current string.
        if(rni != 0)
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

        if(rni != 0)
            Str_Free(&fn);
    }

    // Try expected location for this resource class.
    if(!found && rni != 0)
    {
        found = findResource(rclass, Str_Text(&name), optionalSuffix, foundPath, foundPathLen, rni);
    }

    Str_Free(&name);

    return found;
    }
}

static void resetResourceNamespace(resourcenamespaceid_t rni)
{
    assert(F_IsValidResourceNamespaceId(rni));
    {
    resourcenamespace_t* rnamespace = &namespaces[rni-1];
    if(rnamespace->_fileHash)
    {
        FileHash_Destroy(rnamespace->_fileHash); rnamespace->_fileHash = 0;
    }
    }
}

static void resetResourceNamespaces(void)
{
    uint i, numResourceNamespaces = F_NumResourceNamespaces();
    for(i = 1; i < numResourceNamespaces+1; ++i)
        resetResourceNamespace((resourcenamespaceid_t)i);
}

void F_InitResourceLocator(void)
{
    uint i;
    for(i = 0; i < NUM_RESOURCE_CLASSES; ++i)
        Str_Init(&searchPathLists[i]);
    resetResourceNamespaces();
    inited = true;
}

void F_ShutdownResourceLocator(void)
{
    if(!inited)
        return;
    { uint i;
    for(i = 0; i < NUM_RESOURCE_CLASSES; ++i)
        Str_Free(&searchPathLists[i]);
    }
    resetResourceNamespaces();
    inited = false;
}

const ddstring_t* F_ResourceSearchPaths(resourceclass_t rclass)
{
    assert(VALID_RESOURCE_CLASS(rclass));
    return &searchPathLists[rclass];
}

void F_ClearResourceSearchPaths2(resourceclass_t rclass)
{
    if(rclass == NUM_RESOURCE_CLASSES)
    {
        uint i;
        for(i = 0; i < NUM_RESOURCE_CLASSES; ++i)
        {
            clearSearchPathList((resourceclass_t)i);
        }
        return;
    }
    clearSearchPathList(rclass);
}

void F_ClearResourceSearchPaths(void)
{
    F_ClearResourceSearchPaths2(0);
}

boolean F_AddResourceSearchPath(resourceclass_t rclass, const char* newPath, boolean append)
{
    assert(VALID_RESOURCE_CLASS(rclass));
    {
    ddstring_t* pathList;
    filename_t absNewPath;

    if(!newPath || !newPath[0] || !strcmp(newPath, DIR_SEP_STR))
        return false; // Not suitable.

    // Convert all slashes to the host OS's directory separator,
    // for compatibility with the sys_filein routines.
    strncpy(absNewPath, newPath, FILENAME_T_MAXLEN);
    Dir_ValidDir(absNewPath, FILENAME_T_MAXLEN);
    M_PrependBasePath(absNewPath, absNewPath, FILENAME_T_MAXLEN);

    // Have we seen this path already?
    pathList = &searchPathLists[rclass];
    if(Str_Length(pathList))
    {
        const char* p = Str_Text(pathList);
        ddstring_t curPath;
        boolean ignore = false;

        Str_Init(&curPath);
        while(!ignore && (p = Str_CopyDelim(&curPath, p, ';')))
        {
            if(!Str_CompareIgnoreCase(&curPath, absNewPath))
                ignore = true;
        }
        Str_Free(&curPath);

        if(ignore) return true; // We don't want duplicates.
    }

    // Add the new search path.
    if(append)
    {
        Str_Append(pathList, absNewPath);
        Str_Append(pathList, ";");
    }
    else
    {
        Str_Prepend(pathList, ";");
        Str_Prepend(pathList, absNewPath);
    }
    return true;
    }
}

resourcenamespace_t* F_ToResourceNamespace(resourcenamespaceid_t rni)
{
    if(!F_IsValidResourceNamespaceId(rni))
        Con_Error("DD_ResourceNamespace: Invalid resource namespace id %i.", (int)rni);
    return &namespaces[rni-1];
}

resourcenamespaceid_t F_SafeResourceNamespaceForName(const char* name)
{
    if(name && name[0])
    {
        uint i, numResourceNamespaces = F_NumResourceNamespaces();
        for(i = 1; i < numResourceNamespaces+1; ++i)
        {
            resourcenamespace_t* rnamespace = &namespaces[i-1];
            if(Str_Length(&rnamespace->_name) > 0 && !Str_CompareIgnoreCase(&rnamespace->_name, name))
                return (resourcenamespaceid_t)i;
        }
    }
    return 0;
}

resourcenamespaceid_t F_ResourceNamespaceForName(const char* name)
{
    resourcenamespaceid_t result;
    if((result = F_SafeResourceNamespaceForName(name)) == 0)
        Con_Error("resourceNamespaceForName: Failed to locate resource namespace \"%s\".", name);
    return result;
}

resourcenamespaceid_t F_ParseResourceNamespace(const char* str)
{
    assert(str);
    return F_SafeResourceNamespaceForName(str);
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

uint F_NumResourceNamespaces(void)
{
    return NUM_RESOURCE_CLASSES;
}

boolean F_IsValidResourceNamespaceId(int val)
{
    return (boolean)(val>0 && (unsigned)val < (F_NumResourceNamespaces()+1)? 1 : 0);
}

resourcenamespaceid_t F_DefaultResourceNamespaceForClass(resourceclass_t rclass)
{
    assert(VALID_RESOURCE_CLASS(rclass));
    return F_ResourceNamespaceForName(defaultNamespaceForClass[rclass]);
}

boolean F_FindResource(resourceclass_t rclass, char* foundPath, const char* searchPath, const char* optionalSuffix, size_t foundPathLen)
{
    if(!searchPath || !searchPath[0])
        return false;
    if(!VALID_RESOURCE_CLASS(rclass))
        Con_Error("F_FindResource: Invalid resource class %i.\n", rclass);
    { resourcenamespaceid_t rni;
    if((rni = F_ParseResourceNamespace(searchPath)) != 0)
        return tryLocateResource(rclass, rni, searchPath, optionalSuffix, foundPath, foundPathLen);
    }
    return tryLocateResource(rclass, F_ResourceNamespaceForName(defaultNamespaceForClass[rclass]), searchPath, optionalSuffix, foundPath, foundPathLen);
}
