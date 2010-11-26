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
    { "ded", 0 }, // Definitions, only DED files.
    { "png", "tga", "pcx", 0 }, // Graphic, favor quality.
    { "dmd", "md2", 0 }, // Model, favour DMD over MD2.
    { "wav", 0 }, // Sound, only WAV files.
    { "ogg", "mp3", "wav", "mod", "mid", 0 } // Music
};

static const char* defaultResourceNamespaceForType[NUM_RESOURCE_TYPES] = {
    { "packages:" },
    { "defs:" },
    { "graphics:" },
    { "models:" },
    { "sounds:" },
    { "music:" },
};

static resourcenamespace_t resourceNamespaces[NUM_RESOURCE_TYPES] = {
    { "packages:" },
    { "defs:" },
    { "graphics:" },
    { "models:" },
    { "sounds:" },
    { "music:" }
};

static boolean inited = false;

// CODE --------------------------------------------------------------------

static boolean tryFindFile(const char* searchPath, char* foundPath, size_t foundPathLen,
    resourcenamespaceid_t rni)
{
    assert(inited && searchPath && searchPath[0]);

    { resourcenamespace_t* rnamespace;
    if(rni != 0 && (rnamespace = DD_ResourceNamespace(rni)))
    {
        if(!rnamespace->_fileHash)
            rnamespace->_fileHash = FileHash_Create(Str_Text(DD_ResourceSearchPaths(rni)));
#if _DEBUG
        VERBOSE2( Con_Message("Using filehash for rnamespace \"%s\" ...\n", rnamespace->_name) );
#endif
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
 * @param resType       Type of resource being searched for @see resourceType.
 * @param searchPath    File name/path to search for.
 * @param foundPath     Located path if found will be written back here.
 *                      Can be @c NULL, in which case this is just a boolean query.
 * @param foundPathLen  Length of the @a foundFileName buffer in bytes.
 * @param rni           If non-zero, the namespace to use when searching.
 *
 * @return              @c true, if it's found.
 */
static boolean tryResourceFile(resourcetype_t type, const char* searchPath,
    char* foundPath, size_t foundPathLen, resourcenamespaceid_t rni)
{
    assert(inited && VALID_RESOURCE_TYPE(type) && searchPath && searchPath[0]);
    {
    boolean found = false;
    char* ptr;

    // Has an extension been specified?
    ptr = M_FindFileExtension((char*)searchPath);
    if(ptr && *ptr != '*') // Try this first.
        found = tryFindFile(searchPath, foundPath, foundPathLen, rni);

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
        for(i = 0, ext = resourceTypeFileExtensions[type]; *ext; ext++)
        {
            Str_Copy(&tmp, &path2);
            Str_Appendf(&tmp, "%s", *ext);

            if((found = tryFindFile(Str_Text(&tmp), foundPath, foundPathLen, rni)))
                break;
        }}

        Str_Free(&path2);
        Str_Free(&tmp);
    }

    return found;
    }
}

static boolean findResource(resourcetype_t type, const char* searchPath, const char* optionalSuffix,
    char* foundPath, size_t foundPathLen, resourcenamespaceid_t rni)
{
    assert(inited && VALID_RESOURCE_TYPE(type) && searchPath && searchPath[0]);
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

        if(tryResourceFile(type, Str_Text(&fn), foundPath, foundPathLen, rni))
            found = true;

        Str_Free(&fn);
    }

    // Try without a suffix.
    if(!found)
    {
        if(tryResourceFile(type, searchPath, foundPath, foundPathLen, rni))
            found = true;
    }

    return found;
    }
}

static boolean tryLocateResource(resourcetype_t type, resourcenamespaceid_t rni, const char* searchPath,
    const char* optionalSuffix, char* foundPath, size_t foundPathLen)
{
    assert(inited && VALID_RESOURCE_TYPE(type) && searchPath && searchPath[0]);
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
        found = findResource(type, Str_Text(&name), optionalSuffix, foundPath, foundPathLen, 0);
    }
    else
    {   // Else, prepend the base path and try that.
        ddstring_t fn;
        const char* path;

        // Make this an absolute, base-relative path.
        // If only checking the base path and not the expected location
        // for the resource type (below); re-use the current string.
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
        found = findResource(type, path, optionalSuffix, foundPath, foundPathLen, 0);

        if(rni != 0)
            Str_Free(&fn);
    }

    // Try expected location for this resource type.
    if(!found && rni != 0)
    {
        found = findResource(type, Str_Text(&name), optionalSuffix, foundPath, foundPathLen, rni);
    }

    Str_Free(&name);

    return found;
    }
}

static void resetResourceNamespace(resourcenamespaceid_t rni)
{
    assert(F_IsValidResourceNamespaceId(rni));
    {
    resourcenamespace_t* rnamespace = &resourceNamespaces[rni-1];
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

resourcenamespace_t* F_ToResourceNamespace(resourcenamespaceid_t rni)
{
    if(!F_IsValidResourceNamespaceId(rni))
        Con_Error("DD_ResourceNamespace: Invalid resource namespace id %i.", (int)rni);
    return &resourceNamespaces[rni-1];
}

resourcenamespaceid_t F_SafeResourceNamespaceForName(const char* name)
{
    if(name && name[0])
    {
        uint i, numResourceNamespaces = F_NumResourceNamespaces();
        for(i = 1; i < numResourceNamespaces+1; ++i)
        {
            resourcenamespace_t* rnamespace = &resourceNamespaces[i-1];
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

const char* F_ResourceTypeStr(resourcetype_t resType)
{
    assert(VALID_RESOURCE_TYPE(resType));
    {
    static const char* resourceTypeNames[NUM_RESOURCE_TYPES] = {
        { "RT_PACKAGE" },
        { "RT_DEFINITION" },
        { "RT_GRAPHIC" },
        { "RT_MODEL" },
        { "RT_SOUND" },
        { "RT_MUSIC" }
    };
    return resourceTypeNames[(int)resType];
    }
}

void F_InitResourceLocator(void)
{
    resetResourceNamespaces();
    inited = true;
}

void F_ShutdownResourceLocator(void)
{
    if(!inited)
        return;
    resetResourceNamespaces();
    inited = false;
}

uint F_NumResourceNamespaces(void)
{
    return NUM_RESOURCE_TYPES;
}

boolean F_IsValidResourceNamespaceId(int val)
{
    return (boolean)(val>0 && (unsigned)val < (F_NumResourceNamespaces()+1)? 1 : 0);
}

resourcenamespaceid_t F_DefaultResourceNamespaceForType(resourcetype_t type)
{
    assert(VALID_RESOURCE_TYPE(type));
    return F_ResourceNamespaceForName(defaultResourceNamespaceForType[type]);
}

boolean F_FindResource(resourcetype_t type, char* foundPath, const char* searchPath, const char* optionalSuffix, size_t foundPathLen)
{
    if(!searchPath || !searchPath[0])
        return false;
    if(!VALID_RESOURCE_TYPE(type))
        Con_Error("F_FindResource: Invalid resource type %i.\n", type);
    { resourcenamespaceid_t rni;
    if((rni = F_ParseResourceNamespace(searchPath)) != 0)
        return tryLocateResource(type, rni, searchPath, optionalSuffix, foundPath, foundPathLen);
    }
    return tryLocateResource(type, F_ResourceNamespaceForName(defaultResourceNamespaceForType[type]), searchPath, optionalSuffix, foundPath, foundPathLen);
}
