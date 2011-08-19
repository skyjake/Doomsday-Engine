/**\file resourcenamespace.c
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
            PathDirectory_ComposePath(PathDirectoryNode_Directory(node->data), node->data, &path, NULL, FILEDIRECTORY_DELIMITER);
            { ddstring_t* hashName = rn->_composeHashName(&path);
            Con_Printf("  %lu: %lu:\"%s\" -> %s\n", (unsigned long)n, (unsigned long)i,
                       Str_Text(hashName), Str_Text(&path));
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

static void appendSearchPathsInGroup(ddstring_t* pathList, resourcenamespace_t* rn,
    resourcenamespace_searchpathgroup_t group)
{
    assert(pathList && rn && VALID_RESOURCENAMESPACE_SEARCHPATHGROUP(group));
    { uint i;
    for(i = 0; i < rn->_searchPathsCount[group]; ++i)
    {
        ddstring_t* path = Uri_ComposePath(rn->_searchPaths[group][i]);
        Str_Appendf(pathList, "%s;", Str_Text(path));
        Str_Delete(path);
    }}
}

static void formSearchPathList(ddstring_t* pathList, resourcenamespace_t* rn)
{
    assert(pathList && rn);
    appendSearchPathsInGroup(pathList, rn, SPG_OVERRIDE);
    appendSearchPathsInGroup(pathList, rn, SPG_EXTRA);
    appendSearchPathsInGroup(pathList, rn, SPG_DEFAULT);
    appendSearchPathsInGroup(pathList, rn, SPG_FALLBACK);
}

static boolean findPath(resourcenamespace_t* rn, const ddstring_t* hashName,
    const ddstring_t* searchPath, ddstring_t* foundPath)
{
    assert(rn && hashName && !Str_IsEmpty(hashName) && searchPath && !Str_IsEmpty(searchPath));
    {
    resourcenamespace_namehash_key_t key = rn->_hashName(hashName);
    resourcenamespace_namehash_node_t* node = rn->_pathHash[key].first;
    struct pathdirectory_node_s* resultNode = NULL;

    if(NULL != node)
    {
        pathdirectory_t* pd = PathDirectoryNode_Directory(node->data);
        pathdirectory_search_t* search = PathDirectory_BeginSearchStr(pd, 0, searchPath, FILEDIRECTORY_DELIMITER);
        {
            while(NULL != node && !DD_SearchPathDirectoryCompare(node->data, PathDirectory_Search(pd)))
                node = node->next;
        }
        PathDirectory_EndSearch2(pd, &resultNode);
    }

    // Does the caller want to know the matched path?
    if(NULL != foundPath && NULL != resultNode)
        PathDirectory_ComposePath(PathDirectoryNode_Directory(resultNode), resultNode, foundPath, NULL, FILEDIRECTORY_DELIMITER);

    return (NULL == node? false : true);
    }
}

static int addFilePathWorker(const struct pathdirectory_node_s* fdNode, void* paramaters)
{
    assert(fdNode && paramaters);
    {
    resourcenamespace_t* rn = (resourcenamespace_t*) paramaters;
    ddstring_t* hashName, filePath;

    if(PathDirectoryNode_Type(fdNode) != PT_LEAF)
        return 0; // Continue adding.

    // Extract the file name and hash it.
    Str_Init(&filePath);
    PathDirectory_ComposePath(PathDirectoryNode_Directory(fdNode), fdNode, &filePath, NULL, FILEDIRECTORY_DELIMITER);
    hashName = rn->_composeHashName(&filePath);

    // Is this a new resource?
    if(false == findPath(rn, hashName, &filePath, 0))
    {
        resourcenamespace_namehash_node_t* node;

        // Create a new hash node.
        if((node = malloc(sizeof(*node))) == 0)
            Con_Error("ResourceNamespace::addFilePathWorker: Failed on allocation of %lu bytes for "
                "new ResourcenamespaceNamehashNode.", (unsigned long) sizeof(*node));

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
        FileDirectory_Clear(rn->_directory);

        { ddstring_t tmp; Str_Init(&tmp);
        formSearchPathList(&tmp, rn);
        if(Str_Length(&tmp) > 0)
        {
#if _DEBUG
            //uint startTime;
            VERBOSE( Con_Message("Rebuilding rnamespace name hash...\n") )
            VERBOSE2( Con_PrintPathList(Str_Text(&tmp)) );
            //startTime = verbose >= 2? Sys_GetRealTime(): 0;
#endif

            FileDirectory_AddPathList3(rn->_directory, Str_Text(&tmp), addFilePathWorker, rn);

/*#if _DEBUG
            printPathHash(rn);
            VERBOSE2( Con_Message("  Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) );
#endif*/
        }
        Str_Free(&tmp);
        }

        rn->_flags &= ~RNF_IS_DIRTY;
    }
}

resourcenamespace_t* ResourceNamespace_New2(const char* name,
    filedirectory_t* directory, ddstring_t* (*composeHashNameFunc) (const ddstring_t* path),
    resourcenamespace_namehash_key_t (*hashNameFunc) (const ddstring_t* name), byte flags)
{
    assert(name && directory && composeHashNameFunc && hashNameFunc);
    {
    resourcenamespace_t* rn;

    if(strlen(name) < RESOURCENAMESPACE_MINNAMELENGTH)
        Con_Error("ResourceNamespace::Construct: Invalid name '%s' (min length:%i)",
            name, (int)RESOURCENAMESPACE_MINNAMELENGTH);

    if(NULL == (rn = (resourcenamespace_t*) malloc(sizeof(*rn))))
        Con_Error("ResourceNamespace::Construct: Failed on allocation of %lu bytes.",
            (unsigned long) sizeof(*rn));

    rn->_flags = flags;
    Str_Init(&rn->_name); Str_Set(&rn->_name, name);

    { uint i;
    for(i = 0; i < SEARCHPATHGROUP_COUNT; ++i)
    {
        rn->_searchPathsCount[i] = 0;
        rn->_searchPaths[i] = NULL;
    }}

    rn->_directory = directory;
    rn->_composeHashName = composeHashNameFunc;
    rn->_hashName = hashNameFunc;
    memset(rn->_pathHash, 0, sizeof(rn->_pathHash));

    return rn;
    }
}

resourcenamespace_t* ResourceNamespace_New(const char* name,
    filedirectory_t* directory, ddstring_t* (*composeHashNameFunc) (const ddstring_t* path),
    resourcenamespace_namehash_key_t (*hashNameFunc) (const ddstring_t* name))
{
    return ResourceNamespace_New2(name, directory, composeHashNameFunc, hashNameFunc, 0);
}

void ResourceNamespace_Delete(resourcenamespace_t* rn)
{
    assert(rn);
    ResourceNamespace_ClearSearchPaths(rn, SPG_OVERRIDE);
    ResourceNamespace_ClearSearchPaths(rn, SPG_DEFAULT);
    ResourceNamespace_ClearSearchPaths(rn, SPG_EXTRA);
    ResourceNamespace_ClearSearchPaths(rn, SPG_FALLBACK);
    clearPathHash(rn);
    Str_Free(&rn->_name);
    free(rn);
}

void ResourceNamespace_Reset(resourcenamespace_t* rn)
{
    assert(rn);
    ResourceNamespace_ClearSearchPaths(rn, SPG_EXTRA);
    clearPathHash(rn);
    FileDirectory_Clear(rn->_directory);
    rn->_flags |= RNF_IS_DIRTY;
}

boolean ResourceNamespace_AddSearchPath(resourcenamespace_t* rn, const dduri_t* newUri,
    resourcenamespace_searchpathgroup_t group)
{
    assert(rn && newUri && VALID_RESOURCENAMESPACE_SEARCHPATHGROUP(group));

    if(Str_IsEmpty(Uri_Path(newUri)) || !Str_CompareIgnoreCase(Uri_Path(newUri), DIR_SEP_STR))
        return false; // Not suitable.

    // Have we seen this path already (we don't want duplicates)?
    { uint i, j;
    for(i = 0; i < SEARCHPATHGROUP_COUNT; ++i)
    for(j = 0; j < rn->_searchPathsCount[i]; ++j)
    {
        if(Uri_Equality(rn->_searchPaths[i][j], newUri))
            return true;
    }}

    rn->_searchPaths[group] = (dduri_t**) realloc(rn->_searchPaths[group],
        sizeof(*rn->_searchPaths[group]) * ++rn->_searchPathsCount[group]);
    if(NULL == rn->_searchPaths[group])
        Con_Error("ResourceNamespace::AddExtraSearchPath: Failed on reallocation of %lu bytes for "
            "searchPath list.", (unsigned long) sizeof(*rn->_searchPaths[group]) * (rn->_searchPathsCount[group]-1));

    // Prepend to the path list - newer paths have priority.
    if(rn->_searchPathsCount[group] > 1)
        memmove(rn->_searchPaths[group] + 1, rn->_searchPaths[group],
            sizeof(*rn->_searchPaths[group]) * (rn->_searchPathsCount[group]-1));
    rn->_searchPaths[group][0] = Uri_NewCopy(newUri);

    rn->_flags |= RNF_IS_DIRTY;
    return true;
}

void ResourceNamespace_ClearSearchPaths(resourcenamespace_t* rn, resourcenamespace_searchpathgroup_t group)
{
    assert(rn && VALID_RESOURCENAMESPACE_SEARCHPATHGROUP(group));
    if(!rn->_searchPaths[group])
        return;
    { uint i;
    for(i = 0; i < rn->_searchPathsCount[group]; ++i)
        Uri_Delete(rn->_searchPaths[group][i]);
    }
    free(rn->_searchPaths[group]);
    rn->_searchPaths[group] = 0;
    rn->_searchPathsCount[group] = 0;
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
        Str_Init(&searchPath);
        F_FixSlashes(&searchPath, _searchPath);
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
        int nameLen = Str_Length(&rn->_name), pathLen = Str_Length(path);
        if(nameLen <= pathLen && Str_At(path, nameLen) == DIR_SEP_CHAR &&
           !strnicmp(Str_Text(&rn->_name), Str_Text(path), nameLen))
        {
            Str_Prepend(path, Str_Text(GameInfo_DataPath(DD_GameInfo())));
            return true;
        }
    }
    return false;
}

const ddstring_t* ResourceNamespace_Name(const resourcenamespace_t* rn)
{
    assert(rn);
    return &rn->_name;
}

filedirectory_t* ResourceNamespace_Directory(const resourcenamespace_t* rn)
{
    assert(rn);
    return rn->_directory;
}
