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

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

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

static boolean inited = false;

// CODE --------------------------------------------------------------------

static __inline const resourcetypeinfo_t* getInfoForResourceType(resourcetype_t type)
{
    assert(VALID_RESOURCE_TYPE(type));
    return &typeInfo[((uint)type)-1];
}

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
        found = tryFindFile(searchPath, foundPath, foundPathLen, rni);

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
                        found = tryFindFile(Str_Text(&tmp), foundPath, foundPathLen, rni);
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
