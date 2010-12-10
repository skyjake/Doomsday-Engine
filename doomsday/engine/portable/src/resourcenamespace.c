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

#include "resourcenamespace.h"

static void formSearchPathList(ddstring_t* pathList, resourcenamespace_t* rnamespace)
{
    assert(rnamespace && pathList);

    // A command line override path?
    if(rnamespace->_overrideFlag2 && rnamespace->_overrideFlag2[0] && ArgCheckWith(rnamespace->_overrideFlag2, 1))
    {
        const char* newPath = ArgNext();
        Str_Append(pathList, newPath); Str_Append(pathList, ";");

        Str_Append(pathList, newPath); Str_Append(pathList, "\\$(GameInfo.IdentityKey)"); Str_Append(pathList, ";");
    }

    // Join the extra pathlist from the resource namespace to the final pathlist?
    if(Str_Length(&rnamespace->_extraSearchPaths) > 0)
    {
        Str_Append(pathList, Str_Text(&rnamespace->_extraSearchPaths));
        // Ensure we have the required terminating semicolon.
        if(Str_RAt(pathList, 0) != ';')
            Str_AppendChar(pathList, ';');
    }

    // Join the pathlist from the resource namespace to the final pathlist.
    Str_Append(pathList, rnamespace->_searchPaths);
    // Ensure we have the required terminating semicolon.
    if(Str_RAt(pathList, 0) != ';')
        Str_AppendChar(pathList, ';');

    // A command line default path?
    if(rnamespace->_overrideFlag && rnamespace->_overrideFlag[0] && ArgCheckWith(rnamespace->_overrideFlag, 1))
    {
        Str_Append(pathList, ArgNext()); Str_Append(pathList, ";");
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

    if(!newPath || !newPath[0] || !strcmp(newPath, DIR_SEP_STR))
        return false; // Not suitable.

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
            if(!Str_CompareIgnoreCase(&curPath, newPath))
                ignore = true;
        }
        Str_Free(&curPath);

        if(ignore) return true; // We don't want duplicates.
    }

    // Prepend to the path list - newer paths have priority.
    Str_Prepend(pathList, ";");
    Str_Prepend(pathList, newPath);

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
        formSearchPathList(&tmp, rnamespace);
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