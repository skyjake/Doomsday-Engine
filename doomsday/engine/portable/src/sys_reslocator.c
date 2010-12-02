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

#include "m_string.h"
#include "m_filehash.h"

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
 * Resource Namespace.
 * @ingroup fs
 */
typedef struct resourcenamespace_s {
    /// Unique symbolic name of this namespace (e.g., "packages:").
    const char* _name;
 
    /// Command line options for setting the resource path explicitly. Flag2 Takes precendence.
    const char* _overrideFlag, *_overrideFlag2;

    /// Resource search order (in order of greatest-importance, right to left) seperated by semicolon (e.g., "path1;path2;").
    const char* _searchPaths;

    /// Set of "extra" search paths (in order of greatest-importance, right to left) seperated by semicolon (e.g., "path1;path2;").
    ddstring_t _extraSearchPaths;

    /// Auto-inited:
    filehash_t* _fileHash;
} resourcenamespace_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

boolean ResourceNamespace_AddSearchPath(resourcenamespace_t* rnamespace, const char* newPath, boolean append);
void ResourceNamespace_ClearSearchPaths(resourcenamespace_t* rnamespace);
const ddstring_t* ResourceNamespace_ExtraSearchPaths(resourcenamespace_t* rnamespace);

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
    /* RC_PACKAGE */    "packages",
    /* RC_DEFINITION */ "defs",
    /* RC_GRAPHIC */    "graphics",
    /* RC_MODEL */      "models",
    /* RC_SOUND */      "sounds",
    /* RC_MUSIC */      "music"
};

static resourcenamespace_t namespaces[] = {
    { "packages",    "", "",                    "$(GameInfo.DataPath);}data\\;$(DOOMWADDIR)" },
    { "defs",        "-defdir", "-defdir2",     "$(GameInfo.DefsPath)\\$(GameInfo.IdentityKey);$(GameInfo.DefsPath);"DD_BASEPATH_DEFS },
    { "graphics",    "-gfxdir", "-gfxdir2",     "}data\\graphics" },
    { "models",      "-modeldir", "-modeldir2", "$(GameInfo.DataPath)\\models\\$(GameInfo.IdentityKey);$(GameInfo.DataPath)\\models" },
    { "sounds",      "-sfxdir",  "-sfxdir2",    "$(GameInfo.DataPath)\\sfx\\$(GameInfo.IdentityKey);$(GameInfo.DataPath)\\sfx" },
    { "music",       "-musdir",  "-musdir2",    "$(GameInfo.DataPath)\\music\\$(GameInfo.IdentityKey);$(GameInfo.DataPath)\\music" },
    { "textures",    "-texdir",  "-texdir2",    "$(GameInfo.DataPath)\\textures\\$(GameInfo.IdentityKey);$(GameInfo.DataPath)\\textures" },
    { "flats",       "-flatdir", "-flatdir2",   "$(GameInfo.DataPath)\\flats\\$(GameInfo.IdentityKey);$(GameInfo.DataPath)\\flats" },
    { "patches",     "-patdir",  "-patdir2",    "$(GameInfo.DataPath)\\patches\\$(GameInfo.IdentityKey);$(GameInfo.DataPath)\\patches" },
    { "lightmaps",   "-lmdir",   "-lmdir2",     "$(GameInfo.DataPath)\\lightmaps\\$(GameInfo.IdentityKey);$(GameInfo.DataPath)\\lightmaps" },
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
            if(!stricmp(rnamespace->_name, name))
                return (resourcenamespaceid_t)(i+1);
        }
    }
    return 0;
}

static void resetNamespace(resourcenamespace_t* rnamespace)
{
    assert(rnamespace);
    if(rnamespace->_fileHash)
    {
        FileHash_Destroy(rnamespace->_fileHash); rnamespace->_fileHash = 0;
    }
    Str_Free(&rnamespace->_extraSearchPaths);
}

static void resetAllNamespaces(void)
{
    uint i;
    for(i = 0; i < numNamespaces; ++i)
        resetNamespace(&namespaces[i]);
}

static void clearExtraSearchPaths(resourcenamespaceid_t rni)
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

static boolean translateBasePath(ddstring_t* dest, ddstring_t* src)
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
            Str_PartAppend(&buf, Str_Text(str), 2, Str_Length(src)-2);

            Str_Copy(&dest, &buf);
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
        return result;
        }
    }
#endif
    return false;
}

#if 0
    { size_t i;
    for(i = 0; rnamespace->_searchOrder[i] != SPI_NONE; ++i)
    {
        switch(rnamespace->_searchOrder[i])
        {
        case SPI_BASEPATH:
            { filename_t newPath;
            M_TranslatePath(newPath, "}", FILENAME_T_MAXLEN);
            ResourceNamespace_AddSearchPath(rnamespace, newPath, false);
            break;
            }
        case SPI_BASEPATH_DATA:
            { filename_t newPath;
            if(rnamespace->_defaultPath && rnamespace->_defaultPath[0])
            {
                filename_t other;
                dd_snprintf(other, FILENAME_T_MAXLEN, DD_BASEPATH_DATA"%s", rnamespace->_defaultPath);
                M_TranslatePath(newPath, other, FILENAME_T_MAXLEN);
            }
            else
            {
                M_TranslatePath(newPath, DD_BASEPATH_DATA, FILENAME_T_MAXLEN);
            }
            ResourceNamespace_AddSearchPath(rnamespace, newPath, false);
            break;
            }
        case SPI_BASEPATH_DEFS:
            { filename_t newPath;
            if(rnamespace->_defaultPath && rnamespace->_defaultPath[0])
            {
                filename_t other;
                dd_snprintf(other, FILENAME_T_MAXLEN, DD_BASEPATH_DEFS"%s", rnamespace->_defaultPath);
                M_TranslatePath(newPath, other, FILENAME_T_MAXLEN);
            }
            else
            {
                M_TranslatePath(newPath, DD_BASEPATH_DEFS, FILENAME_T_MAXLEN);
            }
            ResourceNamespace_AddSearchPath(rnamespace, newPath, false);
            break;
            }
        case SPI_GAMEPATH_DATA:
            if(rnamespace->_defaultPath && rnamespace->_defaultPath[0])
            {
                filename_t newPath;
                dd_snprintf(newPath, FILENAME_T_MAXLEN, "%s%s", Str_Text(dataPath), rnamespace->_defaultPath);
                ResourceNamespace_AddSearchPath(rnamespace, newPath, false);
            }
            else
            {
                ResourceNamespace_AddSearchPath(rnamespace, Str_Text(dataPath), false);
            }
            break;
        case SPI_GAMEPATH_DEFS:
            if(rnamespace->_defaultPath && rnamespace->_defaultPath[0])
            {
                filename_t newPath;
                dd_snprintf(newPath, FILENAME_T_MAXLEN, "%s%s", Str_Text(defsPath), rnamespace->_defaultPath);
                ResourceNamespace_AddSearchPath(rnamespace, newPath, false);
            }
            else
            {
                ResourceNamespace_AddSearchPath(rnamespace, Str_Text(defsPath), false);
            }
            break;
        case SPI_GAMEMODEPATH_DATA:
            usingGameModePathData = true;
            if(rnamespace->_defaultPath && rnamespace->_defaultPath[0] && Str_Length(identityKey))
            {
                filename_t newPath;
                dd_snprintf(newPath, FILENAME_T_MAXLEN, "%s%s%s", Str_Text(dataPath), rnamespace->_defaultPath, Str_Text(identityKey));
                ResourceNamespace_AddSearchPath(rnamespace, newPath, false);
            }
            break;
        case SPI_GAMEMODEPATH_DEFS:
            usingGameModePathDefs = true;
            if(Str_Length(identityKey))
            {
                if(rnamespace->_defaultPath && rnamespace->_defaultPath[0])
                {
                    filename_t newPath;
                    dd_snprintf(newPath, FILENAME_T_MAXLEN, "%s%s", Str_Text(defsPath), Str_Text(identityKey));
                    ResourceNamespace_AddSearchPath(rnamespace, newPath, false);
                }
                else
                {
                    filename_t newPath;
                    dd_snprintf(newPath, FILENAME_T_MAXLEN, "%s%s%s", Str_Text(defsPath), rnamespace->_defaultPath, Str_Text(identityKey));
                    ResourceNamespace_AddSearchPath(rnamespace, newPath, false);
                }
            }
            break;
        case SPI_DOOMWADDIR:
            if(!ArgCheck("-nowaddir") && getenv("DOOMWADDIR"))
            {
                filename_t newPath;
                M_TranslatePath(newPath, getenv("DOOMWADDIR"), FILENAME_T_MAXLEN);
                ResourceNamespace_AddSearchPath(rnamespace, newPath, false);
            }
            break;

        default: Con_Error("formSearchPathList: Invalid path ident %i.", rnamespace->_searchOrder[i]);
        };
    }}
#endif

static boolean parseAndReplaceSymbols(ddstring_t* dest, const ddstring_t* src, gameinfo_t* info)
{
    assert(dest && src && info);
    {
    ddstring_t part;
    const char* p;
    boolean successful = false;

    Str_Init(&part);

    // Copy the first part of the string as-is up to first '$' if present.
    if((p = Str_CopyDelim(dest, Str_Text(src), '$')))
    {
        int depth = 0;

        if(*p != '(')
        {
            Con_Message("Invalid character '%c' in \"%s\" at %lu.\n", *p, Str_Text(src), p - Str_Text(src));
            goto parseEnded;
        }
        // Skip over the opening brace.
        p++;
        depth++;

        // Now grab everything up to the closing ')' (it *should* be present).
        while((p = Str_CopyDelim(&part, p, ')')))
        {
            // First, try external symbols like environment variable names - they are quick to reject.
            if(!Str_CompareIgnoreCase(&part, "DOOMWADDIR"))
            {
                if(!ArgCheck("-nowaddir") && getenv("DOOMWADDIR"))
                {
                    filename_t newPath;
                    /// \todo dj: This translation is not necessary at this point. It should be enough to
                    /// fix slashes and ensure a well-formed path (potentially relative).
                    M_TranslatePath(newPath, getenv("DOOMWADDIR"), FILENAME_T_MAXLEN);
                    Str_Append(dest, newPath);
                }
            }
            // Now try internal symbols.
            else if(!Str_CompareIgnoreCase(&part, "GameInfo.DataPath"))
            {
                if(DD_IsNullGameInfo(info))
                    goto parseEnded;

                // DataPath already has ending @c DIR_SEP_CHAR.
                Str_PartAppend(dest, Str_Text(GameInfo_DataPath(info)), 0, Str_Length(GameInfo_DataPath(info))-1);
            }
            else if(!Str_CompareIgnoreCase(&part, "GameInfo.DefsPath"))
            {
                if(DD_IsNullGameInfo(info))
                    goto parseEnded;

                // DefsPath already has ending @c DIR_SEP_CHAR.
                Str_PartAppend(dest, Str_Text(GameInfo_DefsPath(info)), 0, Str_Length(GameInfo_DefsPath(info))-1);
            }
            else if(!Str_CompareIgnoreCase(&part, "GameInfo.IdentityKey"))
            {
                if(DD_IsNullGameInfo(info))
                    goto parseEnded;

                Str_Append(dest, Str_Text(GameInfo_IdentityKey(info)));
            }
            else
            {
                Con_Message("Unknown identifier '%s' in \"%s\".\n", Str_Text(&part), Str_Text(src));
                goto parseEnded;
            }
            depth--;

            // Is there another '$' present?
            if(!(p = Str_CopyDelim(&part, p, '$')))
                break;
            // Copy everything up to the next '$'.
            Str_Append(dest, Str_Text(&part));
            if(*p != '(')
            {
                Con_Message("Invalid character '%c' in \"%s\" at %lu.\n", *p, Str_Text(src), p - Str_Text(src));
                goto parseEnded;
            }
            // Skip over the opening brace.
            p++;
            depth++;
        }

        if(depth != 0)
        {
            goto parseEnded;
        }

        // Copy anything remaining.
        Str_Append(dest, Str_Text(&part));
    }

    // Ensure we have a terminating DIR_SEP_CHAR
    if(Str_RAt(dest, 0) != DIR_SEP_CHAR)
        Str_AppendChar(dest, DIR_SEP_CHAR);

    // No errors detected.
    successful = true;

parseEnded:
    Str_Free(&part);
    return successful;
    }
}

/**
 * \todo dj: We don't really want absolute paths output here, paths should stay relative
 * until search-time where possible.
 */
static void formSearchPathList(ddstring_t* pathList, resourcenamespace_t* rnamespace, gameinfo_t* info)
{
    assert(pathList && rnamespace && info);
    {
    ddstring_t pathTemplate;
    boolean usingGameModePathData = false;
    boolean usingGameModePathDefs = false;

    Str_Clear(pathList);

    if(rnamespace->_overrideFlag && rnamespace->_overrideFlag[0] && ArgCheckWith(rnamespace->_overrideFlag, 1))
    {   // A command line override of the default path.
        filename_t newPath;
        M_TranslatePath(newPath, ArgNext(), FILENAME_T_MAXLEN);
        Dir_ValidDir(newPath, FILENAME_T_MAXLEN);
        Str_Prepend(pathList, ";"); Str_Prepend(pathList, newPath);
    }

    Str_Init(&pathTemplate); Str_Set(&pathTemplate, rnamespace->_searchPaths);

    // Convert all slashes to the host OS's directory separator, for compatibility with the sys_filein routines.
    Dir_FixSlashes(Str_Text(&pathTemplate), Str_Length(&pathTemplate));

    // Ensure we have the required terminating semicolon.
    if(Str_RAt(&pathTemplate, 0) != ';')
        Str_AppendChar(&pathTemplate, ';');

    // Tokenize the template into discreet paths and process individually.
    { const char* p = Str_Text(&pathTemplate);
    ddstring_t path, translatedPath;
    Str_Init(&path);
    Str_Init(&translatedPath);
    while((p = Str_CopyDelim(&path, p, ';')))
    {
        // Substitute known symbols in the templated path.
        if(!parseAndReplaceSymbols(&translatedPath, &path, info))
        {
            // Path cannot be completed due to incomplete symbol definitions. Ignore this path.
#if _DEBUG
            Con_Message("formSearchPathList: Ignoring incomplete path \"%s\" for rnamespace %s.\n", Str_Text(&path), rnamespace->_name);
#endif
            continue;
        }

        // Do we need to expand platform-specific path directives?
        translateBasePath(&translatedPath, &translatedPath);

        // Add it to the list of paths.
        Str_Append(pathList, Str_Text(&translatedPath)); Str_Append(pathList, ";");
    }
    Str_Free(&path);
    Str_Free(&translatedPath);
    }

    // The overriding path.
#pragma message("!!!WARNING: Resource namespace default path override2 not presently implemented (e.g., -texdir2)!!!")
    /*if(rnamespace->_overrideFlag2 && rnamespace->_overrideFlag2[0] && ArgCheckWith(rnamespace->_overrideFlag2, 1))
    {
        filename_t newPath;
        M_TranslatePath(newPath, ArgNext(), FILENAME_T_MAXLEN);
        Dir_ValidDir(newPath, FILENAME_T_MAXLEN);
        Str_Prepend(pathList, ";"); Str_Prepend(pathList, newPath);

        if((usingGameModePathData || usingGameModePathDefs) && Str_Length(identityKey))
        {
            filename_t other;
            dd_snprintf(other, FILENAME_T_MAXLEN, "%s/%s", newPath, Str_Text(identityKey));
            ResourceNamespace_AddSearchPath(rnamespace, other, false);
        }
    }*/
    }
}

static void collateSearchPathSet(ddstring_t* pathList, resourcenamespace_t* rnamespace, gameinfo_t* info)
{
    formSearchPathList(pathList, rnamespace, info);
}

static boolean tryFindFile(resourceclass_t rclass, const char* searchPath,
    char* foundPath, size_t foundPathLen, resourcenamespace_t* rnamespace)
{
    assert(inited && searchPath && searchPath[0]);

    if(rnamespace)
    {
        if(!rnamespace->_fileHash)
        {
            ddstring_t tmp;
            Str_Init(&tmp);
            collateSearchPathSet(&tmp, rnamespace, DD_GameInfo());
            rnamespace->_fileHash = FileHash_Create(Str_Text(&tmp));
            Str_Free(&tmp);
        }
        return FileHash_Find(rnamespace->_fileHash, foundPath, searchPath, foundPathLen);
    }

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
 * @param rnamespace    The namespace to use when searching else @c NULL.
 *
 * @return              @c true, if it's found.
 */
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
                        found = tryFindFile(rclass, Str_Text(&tmp), foundPath, foundPathLen, rnamespace);
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
    boolean found;

    // Fix directory seperators early.
    Str_Init(&name);
    Str_Set(&name, searchPath);
    Dir_FixSlashes(Str_Text(&name), Str_Length(&name));

#if _DEBUG
    if(rnamespace)
        VERBOSE2( Con_Message("Using rnamespace \"%s\" ...\n", rnamespace->_name) );
#endif

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
    }

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
    // Has a namespace identifier been included?
    resourcenamespaceid_t rni;
    const char* p;
    ddstring_t tmp;

    Str_Init(&tmp);
    p = Str_CopyDelim(&tmp, searchPath, ':');
    if(p && (rni = F_SafeResourceNamespaceForName(Str_Text(&tmp)) != 0))
    {
        boolean result = tryLocateResource2(rclass, getNamespaceForId(rni), Str_Text(&tmp), optionalSuffix, foundPath, foundPathLen);
        Str_Free(&tmp);
        return result;
    }

    // Use the default namespace for this resource class.
    return tryLocateResource2(rclass, getNamespaceForId(F_ResourceNamespaceForName(defaultNamespaceForClass[rclass])), searchPath, optionalSuffix, foundPath, foundPathLen);
}

void F_InitResourceLocator(void)
{
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
    return getNamespaceForId(rni);
}

resourcenamespaceid_t F_SafeResourceNamespaceForName(const char* name)
{
    return findNamespaceForName(name);
}

resourcenamespaceid_t F_ResourceNamespaceForName(const char* name)
{
    resourcenamespaceid_t result;
    if((result = F_SafeResourceNamespaceForName(name)) == 0)
        Con_Error("resourceNamespaceForName: Failed to locate resource namespace \"%s\".", name);
    return result;
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
    return numNamespaces;
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

void F_ClearResourceSearchPaths(void)
{
    clearExtraSearchPaths(0);
}

boolean F_FindResource(resourceclass_t rclass, char* foundPath, const char* _searchPaths, const char* optionalSuffix, size_t foundPathLen)
{
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

/**
 * Add a new path to the list of "extra" search paths in this namespace.
 */
boolean ResourceNamespace_AddSearchPath(resourcenamespace_t* rnamespace, const char* newPath, boolean append)
{
    assert(rnamespace);
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
    pathList = &rnamespace->_extraSearchPaths;
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

/**
 * Clear "extra" resource search paths in this namespace.
 */
void ResourceNamespace_ClearSearchPaths(resourcenamespace_t* rnamespace)
{
    assert(rnamespace);
    Str_Free(&rnamespace->_extraSearchPaths);
}

/**
 * @return              Ptr to a string containing the list of "extra" search paths.
 */
const ddstring_t* ResourceNamespace_ExtraSearchPaths(resourcenamespace_t* rnamespace)
{
    assert(rnamespace);
    return &rnamespace->_extraSearchPaths;
}
