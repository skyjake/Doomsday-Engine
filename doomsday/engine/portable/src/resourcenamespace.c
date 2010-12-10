/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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

#include <assert.h>

#include "de_base.h"
#include "m_args.h"
#include "m_filehash.h"
#include "m_misc.h" /// \todo dj: replace dep

#include "resourcenamespace.h"

/**
 * \todo dj: We don't really want absolute paths output here, paths should stay relative
 * until search-time where possible.
 */
static void formSearchPathList(resourcenamespace_t* rnamespace, ddstring_t* pathList)
{
    assert(pathList && rnamespace);
    {
    ddstring_t pathTemplate;

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
        if(!F_ResolveURI(&translatedPath, &path))
        {   // Path cannot be completed due to incomplete symbol definitions. 
#if _DEBUG
            Con_Message("formSearchPathList: Ignoring incomplete path \"%s\" for rnamespace %s.\n", Str_Text(&path), rnamespace->_name);
#endif
            continue; // Ignore this path.
        }
        // Do we need to expand any path directives?
        F_ExpandBasePath(&translatedPath, &translatedPath);

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

void ResourceNamespace_Reset(resourcenamespace_t* rnamespace)
{
    assert(rnamespace);
    if(rnamespace->_fileHash)
    {
        FileHash_Destroy(rnamespace->_fileHash); rnamespace->_fileHash = 0;
    }
    Str_Free(&rnamespace->_extraSearchPaths);
    rnamespace->_flags |= RNF_IS_DIRTY;
}

boolean ResourceNamespace_AddSearchPath(resourcenamespace_t* rnamespace, const char* newPath)
{
    assert(rnamespace);
    {
    ddstring_t* pathList;
    filename_t absNewPath;

    if(!newPath || !newPath[0] || !strcmp(newPath, DIR_SEP_STR))
        return false; // Not suitable.

    // Convert all slashes to the host OS's directory separator, for compatibility
    // with the sys_filein routines.
    strncpy(absNewPath, newPath, FILENAME_T_MAXLEN);
    // \todo dj: Do not make paths absolute at this time, defer until filehash construction.
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

    // Prepend to the path list - newer paths have priority.
    Str_Prepend(pathList, ";");
    Str_Prepend(pathList, absNewPath);

    rnamespace->_flags |= RNF_IS_DIRTY;
    return true;
    }
}

void ResourceNamespace_ClearSearchPaths(resourcenamespace_t* rnamespace)
{
    assert(rnamespace);
    if(Str_Length(&rnamespace->_extraSearchPaths) == 0)
        return;
    Str_Free(&rnamespace->_extraSearchPaths);
    rnamespace->_flags |= RNF_IS_DIRTY;
}

struct filehash_s* ResourceNamespace_Hash(resourcenamespace_t* rnamespace)
{
    assert(rnamespace);
    if(rnamespace->_flags & RNF_IS_DIRTY)
    {
        ddstring_t tmp;
        Str_Init(&tmp);
        formSearchPathList(rnamespace, &tmp);
        if(Str_Length(&tmp) > 0)
        {
            rnamespace->_fileHash = FileHash_Create(Str_Text(&tmp));
        }
        Str_Free(&tmp);
        rnamespace->_flags &= ~RNF_IS_DIRTY;
    }
    return rnamespace->_fileHash;
}

boolean ResourceNamespace_MapPath(resourcenamespace_t* rnamespace, ddstring_t* path)
{
    assert(rnamespace && path);

    if(rnamespace->_flags & RNF_USE_VMAP)
    {
        size_t nameLen = strlen(rnamespace->_name), pathLen = Str_Length(path);
        if(nameLen <= pathLen && Str_At(path, nameLen) == DIR_SEP_CHAR && !strnicmp(rnamespace->_name, Str_Text(path), nameLen))
        {
            Str_Prepend(path, Str_Text(GameInfo_DataPath(DD_GameInfo())));
            return true;
        }
    }
    return false;
}

const char* ResourceNamespace_Name(const resourcenamespace_t* rnamespace)
{
    assert(rnamespace);
    return rnamespace->_name;
}

const ddstring_t* ResourceNamespace_ExtraSearchPaths(resourcenamespace_t* rnamespace)
{
    assert(rnamespace);
    return &rnamespace->_extraSearchPaths;
}