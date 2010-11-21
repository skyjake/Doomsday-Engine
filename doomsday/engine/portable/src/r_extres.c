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

#define MAX_EXTENSIONS          (10)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// Recognized extensions (in order of importance, left to right).
static const char* resourceTypeFileExtensions[NUM_RESOURCE_TYPES][MAX_EXTENSIONS] = {
    { "pk3", "zip", "wad", 0 }, // Packages, favor ZIP over WAD.
    { "png", "tga", "pcx", 0 }, // Graphic, favor quality.
    { "dmd", "md2", 0 }, // Model, favour DMD over MD2.
    { "wav", 0 }, // Sound, only WAV files.
    { "ogg", "mp3", "wav", "mod", "mid", 0 } // Music
};

// Default resource classes for resource types.
static const ddresourceclass_t resourceTypeDefaultClasses[NUM_RESOURCE_TYPES] = {
    DDRC_PACKAGE,
    DDRC_GRAPHIC,
    DDRC_MODEL,
    DDRC_SOUND,
    DDRC_MUSIC
};

static filehash_t* fileHashes[NUM_RESOURCE_CLASSES];
static boolean inited = false;

// CODE --------------------------------------------------------------------

static __inline ddresourceclass_t defaultResourceClassForType(resourcetype_t resType)
{
    assert(VALID_RESOURCE_TYPE(resType));
    return resourceTypeDefaultClasses[(int)resType];
}

static boolean tryFindFile(const char* searchPath, char* foundPath, size_t foundPathLen,
    filehash_t* fileHash)
{
    assert(inited && searchPath && searchPath[0]);

    if(fileHash)
    {
        return FileHash_Find(fileHash, foundPath, searchPath, foundPathLen);
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
 * @param resType       Type of resource being searched for @see resourceType.
 * @param searchPath    File name/path to search for.
 * @param foundPath     Located path if found will be written back here.
 *                      Can be @c NULL, in which case this is just a boolean query.
 * @param foundPathLen  Length of the @a foundFileName buffer in bytes.
 * @param fileHash      Optional FileHash to use for faster searching.
 *                      Can be @c NULL, in which case the File Stream Abstraction Layer's
 *                      own search algorithm is used directly.
 *
 * @return              @c true, if it's found.
 */
static boolean tryResourceFile(resourcetype_t resType, const char* searchPath,
    char* foundPath, size_t foundPathLen, filehash_t* fileHash)
{
    assert(inited && VALID_RESOURCE_TYPE(resType) && searchPath && searchPath[0]);
    {
    boolean found = false;
    char* ptr;

    // Has an extension been specified?
    ptr = M_FindFileExtension((char*)searchPath);
    if(ptr && *ptr != '*') // Try this first.
        found = tryFindFile(searchPath, foundPath, foundPathLen, fileHash);

    if(!found)
    {
        ddstring_t path2, tmp;
        const char** ext;

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

        { int i;
        for(i = 0, ext = resourceTypeFileExtensions[resType]; *ext; ext++)
        {
            Str_Copy(&tmp, &path2);
            Str_Appendf(&tmp, "%s", *ext);

            if((found = tryFindFile(Str_Text(&tmp), foundPath, foundPathLen, fileHash)))
                break;
        }}

        Str_Free(&path2);
        Str_Free(&tmp);
    }

    return found;
    }
}

static boolean findResource(resourcetype_t resType, const char* searchPath,
    const char* optionalSuffix, char* foundPath, size_t foundPathLen, filehash_t* fileHash)
{
    assert(inited && VALID_RESOURCE_TYPE(resType) && searchPath && searchPath[0]);
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

        if(tryResourceFile(resType, Str_Text(&fn), foundPath, foundPathLen, fileHash))
            found = true;

        Str_Free(&fn);
    }

    // Try without a suffix.
    if(!found)
    {
        if(tryResourceFile(resType, searchPath, foundPath, foundPathLen, fileHash))
            found = true;
    }

    return found;
    }
}

static boolean tryLocateResource(resourcetype_t resType, ddresourceclass_t resClass,
    const char* searchPath, const char* optionalSuffix, char* foundPath, size_t foundPathLen)
{
    assert(inited && VALID_RESOURCE_TYPE(resType) && searchPath && searchPath[0]);
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
        found = findResource(resType, Str_Text(&name), optionalSuffix, foundPath, foundPathLen, 0);
    }
    else
    {   // Else, prepend the base path and try that.
        ddstring_t fn;
        const char* path;

        // Make this an absolute, base-relative path.
        // If only checking the base path and not the expected location
        // for the resource type (below); re-use the current string.
        if(resClass != DDRC_NONE)
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
        found = findResource(resType, path, optionalSuffix, foundPath, foundPathLen, 0);

        if(resClass != DDRC_NONE)
            Str_Free(&fn);
    }

    // Try expected location for this resource type and class?
    if(!found && resClass != DDRC_NONE)
    {
        // Do we need to (re)build a hash for this resource class?
        if(!fileHashes[(int)resClass])
        {
            uint startTime = verbose >= 1? Sys_GetRealTime(): 0;
            fileHashes[(int)resClass] = FileHash_Create(Str_Text(GameInfo_ResourceSearchPaths(DD_GameInfo(), resClass)));
            if(verbose >= 1)
            {
                Con_Message("%s filehash rebuilt in %.2f seconds.\n", F_ResourceClassStr(resClass), (Sys_GetRealTime() - startTime) / 1000.0f);
                M_PrintPathList(FileHash_PathList(fileHashes[(int)resClass]));
/*#if _DEBUG
                FileHash_Print(fileHashes[(int)resClass]);
#endif*/
            }
        }
        found = findResource(resType, Str_Text(&name), optionalSuffix, foundPath, foundPathLen, fileHashes[(int)resClass]);
    }

    Str_Free(&name);

    return found;
    }
}

static void destroyFileHashes(void)
{
    { int i;
    for(i = 0; i < NUM_RESOURCE_CLASSES; ++i)
    {
        if(fileHashes[i])
            FileHash_Destroy(fileHashes[i]);
        fileHashes[i] = 0;
    }}
}

const char* F_ResourceClassStr(ddresourceclass_t rc)
{
    assert(VALID_RESOURCE_CLASS(rc));
    {
    static const char* resourceClassNames[NUM_RESOURCE_CLASSES] = {
        { "DDRC_PACKAGE" },
        { "DDRC_TEXTURE" },
        { "DDRC_FLAT" },
        { "DDRC_PATCH" },
        { "DDRC_LIGHTMAP" },
        { "DDRC_FLAREMAP" },
        { "DDRC_MUSIC" },
        { "DDRC_SOUND" },
        { "DDRC_GRAPHIC" },
        { "DDRC_MODEL" }
    };
    return resourceClassNames[(int)rc];
    }
}

void F_InitResourceLocator(void)
{
    destroyFileHashes();
    inited = true;
}

void F_ShutdownResourceLocator(void)
{
    if(!inited)
        return;
    destroyFileHashes();
    inited = false;
}

boolean F_FindResource2(resourcetype_t resType, ddresourceclass_t resClass,
    char* foundPath, const char* searchPath, const char* optionalSuffix, size_t foundPathLen)
{
    if(!searchPath || !searchPath[0])
        return false;
    if(!VALID_RESOURCE_TYPE(resType))
        Con_Error("F_FindResource2: Invalid resource type %i.\n", resType);
    // No resource class means use the base path.
    if(resClass != DDRC_NONE && !VALID_RESOURCE_CLASS(resClass))
        Con_Error("F_FindResource2: Invalid resource class %i.\n", resClass);
    return tryLocateResource(resType, resClass, searchPath, optionalSuffix, foundPath, foundPathLen);
}

boolean F_FindResource(resourcetype_t resType, char* foundPath, const char* searchPath,
    const char* optionalSuffix, size_t foundPathLen)
{
    if(!searchPath || !searchPath[0])
        return false;
    if(!VALID_RESOURCE_TYPE(resType))
        Con_Error("F_FindResource: Invalid resource type %i.\n", resType);
    return tryLocateResource(resType, defaultResourceClassForType(resType), searchPath, optionalSuffix, foundPath, foundPathLen);
}
