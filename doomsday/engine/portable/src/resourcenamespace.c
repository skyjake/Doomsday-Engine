/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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
#include "de_console.h"
#include "m_args.h"
#include "filedirectory.h"

#include "resourcenamespace.h"

static void clearPathHash(resourcenamespace_t* rn)
{
    assert(rn);
    { uint i;
    for(i = 0; i < RESOURCENAMESPACE_HASHSIZE; ++i)
    {
        resourcenamespace_hashentry_t* entry = &rn->_pathHash[i];
        while(entry->first)
        {
            resourcenamespace_namehash_node_t* nextNode = entry->first->next;
            free(entry->first);
            entry->first = nextNode;
        }
        entry->last = 0;
    }}
}

#if _DEBUG
static void printPathHash(resourcenamespace_t* rn)
{
    assert(rn);
    {
    ddstring_t path;
    size_t n = 0;

    Str_Init(&path);

    { uint i;
    for(i = 0; i < RESOURCENAMESPACE_HASHSIZE; ++i)
    {
        resourcenamespace_hashentry_t* entry = &rn->_pathHash[i];
        resourcenamespace_namehash_node_t* node = entry->first;
        while(node)
        {
            Str_Clear(&path);
            FileDirectoryNode_ComposePath((filedirectory_node_t*)node->data, &path);
            { ddstring_t* hashName = rn->_composeHashName(&path);
            Con_Printf("  %lu: %lu:\"%s\" -> %s\n", (unsigned long)n, i, Str_Text(hashName), Str_Text(&path));
            Str_Delete(hashName);
            }
            ++n;
            node = node->next;
        }
    }}

    Str_Free(&path);
    }
}
#endif

static void formSearchPathList(ddstring_t* pathList, resourcenamespace_t* rn)
{
    assert(rn && pathList);

    // A command line override path?
    if(rn->_overrideName2 && rn->_overrideName2[0] && ArgCheckWith(rn->_overrideName2, 1))
    {
        const char* newPath = ArgNext();
        Str_Append(pathList, newPath); Str_Append(pathList, ";");

        Str_Append(pathList, newPath); Str_Append(pathList, "/$(GameInfo.IdentityKey)"); Str_Append(pathList, ";");
    }

    // Join the extra pathlist from the resource namespace to the final pathlist?
    if(Str_Length(&rn->_extraSearchPaths) > 0)
    {
        Str_Append(pathList, Str_Text(&rn->_extraSearchPaths));
        // Ensure we have the required terminating semicolon.
        if(Str_RAt(pathList, 0) != ';')
            Str_AppendChar(pathList, ';');
    }

    // Join the pathlist from the resource namespace to the final pathlist.
    Str_Append(pathList, rn->_searchPaths);
    // Ensure we have the required terminating semicolon.
    if(Str_RAt(pathList, 0) != ';')
        Str_AppendChar(pathList, ';');

    // A command line default path?
    if(rn->_overrideName && rn->_overrideName[0] && ArgCheckWith(rn->_overrideName, 1))
    {
        Str_Append(pathList, ArgNext()); Str_Append(pathList, ";");
    }
}

static boolean findPath(resourcenamespace_t* rn, const ddstring_t* hashName,
    const ddstring_t* searchPath, ddstring_t* foundPath)
{
    assert(rn && hashName && !Str_IsEmpty(hashName) && searchPath && !Str_IsEmpty(searchPath));
    {
    resourcenamespace_namehash_key_t key = rn->_hashName(hashName);
    resourcenamespace_namehash_node_t* node;

    // Go through the candidates.
    node = rn->_pathHash[key].first;
    while(node && !FileDirectoryNode_MatchDirectory((filedirectory_node_t*)node->data, searchPath))
        node = node->next;

    // Does the caller want to know the matched path?
    if(node && foundPath)
        FileDirectoryNode_ComposePath((filedirectory_node_t*)node->data, foundPath);

    return (node == 0? false : true);
    }
}

static int addFilePathToResourceNamespaceWorker(const filedirectory_node_t* fdNode, void* paramaters)
{
    assert(fdNode && paramaters);
    {
    resourcenamespace_t* rn = (resourcenamespace_t*) paramaters;
    ddstring_t* hashName, filePath;

    if(fdNode->type != FT_NORMAL)
        return 0; // Continue adding.

    // Extract the file name and hash it.
    Str_Init(&filePath);
    FileDirectoryNode_ComposePath(fdNode, &filePath);
    hashName = rn->_composeHashName(&filePath);

    // Is this a new resource?
    if(false == findPath(rn, hashName, &filePath, 0))
    {
        resourcenamespace_namehash_node_t* node;

        // Create a new hash node.
        if((node = malloc(sizeof(*node))) == 0)
            Con_Error("addFilePathToResourceNamespaceWorker: failed on allocation of %lu bytes for node.", (unsigned long) sizeof(*node));
        node->data = (void*) fdNode;
        node->next = 0;

        // Link it to the list for this bucket.
        { resourcenamespace_hashentry_t* slot = &rn->_pathHash[rn->_hashName(hashName)];
        if(slot->last)
            slot->last->next = node;
        slot->last = node;
        if(!slot->first)
            slot->first = node;
        }
    }

    Str_Delete(hashName);
    Str_Free(&filePath);
    return 0; // Continue adding.
    }
}

static void rebuild(resourcenamespace_t* rn)
{
    assert(rn);
    if(rn->_flags & RNF_IS_DIRTY)
    {
        clearPathHash(rn);

        { ddstring_t tmp; Str_Init(&tmp);
        formSearchPathList(&tmp, rn);
        if(Str_Length(&tmp) > 0)
        {
            uint startTime;
            VERBOSE( Con_Message("Rebuilding rnamespace name hash ...\n") );
            startTime = verbose >= 2? Sys_GetRealTime(): 0;
            FileDirectory_AddPaths3(F_FileDirectory(), Str_Text(&tmp), addFilePathToResourceNamespaceWorker, rn);
/*#if _DEBUG
            printPathHash(rn);
#endif*/
            VERBOSE2( Con_Message("  Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) );
        }
        Str_Free(&tmp);
        }

        rn->_flags &= ~RNF_IS_DIRTY;
    }
}

void ResourceNamespace_Reset(resourcenamespace_t* rn)
{
    assert(rn);
    clearPathHash(rn);
    Str_Free(&rn->_extraSearchPaths);
    rn->_flags |= RNF_IS_DIRTY;
}

boolean ResourceNamespace_AddSearchPath(resourcenamespace_t* rn, const char* newPath)
{
    assert(rn);
    {
    ddstring_t* pathList;

    if(!newPath || !newPath[0] || !strcmp(newPath, DIR_SEP_STR))
        return false; // Not suitable.

    // Have we seen this path already?
    pathList = &rn->_extraSearchPaths;
    if(Str_Length(pathList))
    {
        const char* p = Str_Text(pathList);
        ddstring_t curPath;
        boolean ignore = false;

        Str_Init(&curPath);
        while(!ignore && (p = Str_CopyDelim2(&curPath, p, ';', CDF_OMIT_DELIMITER)))
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

    rn->_flags |= RNF_IS_DIRTY;
    return true;
    }
}

void ResourceNamespace_ClearSearchPaths(resourcenamespace_t* rn)
{
    assert(rn);
    if(Str_Length(&rn->_extraSearchPaths) == 0)
        return;
    Str_Free(&rn->_extraSearchPaths);
    rn->_flags |= RNF_IS_DIRTY;
}

boolean ResourceNamespace_Find2(resourcenamespace_t* rn, const ddstring_t* _searchPath,
    ddstring_t* foundPath)
{
    assert(rn && _searchPath);
    {
    boolean result = false;
    if(!Str_IsEmpty(_searchPath))
    {
        ddstring_t* hashName, searchPath;

        // Ensure the namespace is clean.
        rebuild(rn);

        // Extract the file name and hash it.
        Str_Init(&searchPath); Str_Copy(&searchPath, _searchPath);
        F_FixSlashes(&searchPath);
        hashName = rn->_composeHashName(&searchPath);

        result = findPath(rn, hashName, &searchPath, foundPath);
        Str_Free(&searchPath);
        Str_Delete(hashName);
    }
    return result;
    }
}

boolean ResourceNamespace_Find(resourcenamespace_t* rn, const ddstring_t* searchPath)
{
    return ResourceNamespace_Find2(rn, searchPath, 0);
}

boolean ResourceNamespace_MapPath(resourcenamespace_t* rn, ddstring_t* path)
{
    assert(rn && path);

    if(rn->_flags & RNF_USE_VMAP)
    {
        size_t nameLen = strlen(rn->_name), pathLen = Str_Length(path);
        if(nameLen <= pathLen && Str_At(path, nameLen) == DIR_SEP_CHAR && !strnicmp(rn->_name, Str_Text(path), nameLen))
        {
            Str_Prepend(path, Str_Text(GameInfo_DataPath(DD_GameInfo())));
            return true;
        }
    }
    return false;
}

const char* ResourceNamespace_Name(const resourcenamespace_t* rn)
{
    assert(rn);
    return rn->_name;
}

const ddstring_t* ResourceNamespace_ExtraSearchPaths(resourcenamespace_t* rn)
{
    assert(rn);
    return &rn->_extraSearchPaths;
}
