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
            FileDirectoryNode_ComposePath(node->data, &path);
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

static void formSearchPathList(ddstring_t* pathList, resourcenamespace_t* rn)
{
    assert(rn && pathList);

    // A command line override path?
    if(rn->_overrideName2 && ArgCheckWith(Str_Text(rn->_overrideName2), 1))
    {
        const char* newPath = ArgNext();
        Str_Appendf(pathList, "%s;%s/$(GameInfo.IdentityKey);", newPath, newPath);
    }

    // Join the extra pathlist from the resource namespace to the final pathlist?
    { uint i;
    for(i = 0; i < rn->_extraSearchPathsCount; ++i)
    {
        ddstring_t* path = Uri_ComposePath(rn->_extraSearchPaths[i]);
        Str_Appendf(pathList, "%s;", Str_Text(path));
        Str_Delete(path);
    }}

    // Join the pathlist from the resource namespace to the final pathlist.
    { uint i;
    for(i = 0; i < rn->_searchPathsCount; ++i)
    {
        ddstring_t* path = Uri_ComposePath(rn->_searchPaths[i]);
        Str_Appendf(pathList, "%s;", Str_Text(path));
        Str_Delete(path);
    }}

    // A command line default path?
    if(rn->_overrideName && ArgCheckWith(Str_Text(rn->_overrideName), 1))
    {
        Str_Appendf(pathList, "%s;", ArgNext());
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
    while(node && !FileDirectoryNode_MatchDirectory(node->data, searchPath, DIR_SEP_CHAR))
        node = node->next;

    // Does the caller want to know the matched path?
    if(node && foundPath)
        FileDirectoryNode_ComposePath(node->data, foundPath);

    return (node == 0? false : true);
    }
}

static int addFilePathWorker(const struct filedirectory_node_s* fdNode, void* paramaters)
{
    assert(fdNode && paramaters);
    {
    resourcenamespace_t* rn = (resourcenamespace_t*) paramaters;
    ddstring_t* hashName, filePath;

    if(FileDirectoryNode_Type(fdNode) != FDT_FILE)
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

        { ddstring_t tmp; Str_Init(&tmp);
        formSearchPathList(&tmp, rn);
        if(Str_Length(&tmp) > 0)
        {
#if _DEBUG
            //uint startTime;
            VERBOSE( Con_Message("Rebuilding rnamespace name hash ...\n") );
            VERBOSE2( Con_PrintPathList(Str_Text(&tmp)) );
            //startTime = verbose >= 2? Sys_GetRealTime(): 0;
#endif

            FileDirectory_AddPathList3(F_LocalPaths(), Str_Text(&tmp), addFilePathWorker, rn);

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

resourcenamespace_t* ResourceNamespace_Construct5(const char* name,
    ddstring_t* (*composeHashNameFunc) (const ddstring_t* path),
    resourcenamespace_namehash_key_t (*hashNameFunc) (const ddstring_t* name),
    const dduri_t* const* searchPaths, uint searchPathsCount, byte flags,
    const char* overrideName, const char* overrideName2)
{
    assert(name && composeHashNameFunc && hashNameFunc);
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

    rn->_composeHashName = composeHashNameFunc;
    rn->_hashName = hashNameFunc;
    memset(rn->_pathHash, 0, sizeof(rn->_pathHash));

    rn->_extraSearchPaths = 0;
    rn->_extraSearchPathsCount = 0;

    if(searchPaths && searchPathsCount != 0)
    {
        rn->_searchPathsCount = searchPathsCount;
        rn->_searchPaths = (dduri_t**) malloc(sizeof(*rn->_searchPaths) * rn->_searchPathsCount);
        if(NULL == rn->_searchPaths)
            Con_Error("ResourceNamespace::Construct: Failed on allocation of %lu bytes for "
                "searchPath list.", (unsigned long) (sizeof(*rn->_searchPaths) * rn->_searchPathsCount));
        { uint i;
        for(i = 0; i < rn->_searchPathsCount; ++i)
            rn->_searchPaths[i] = Uri_ConstructCopy(searchPaths[i]);
        }
    }
    else
    {
        rn->_searchPathsCount = 0;
        rn->_searchPaths = 0;
    }

    if(overrideName && strlen(overrideName) != 0)
    {
        rn->_overrideName = Str_New();
        Str_Init(rn->_overrideName); Str_Set(rn->_overrideName, overrideName);
    }
    else
        rn->_overrideName = 0;
    if(overrideName2 && strlen(overrideName2) != 0)
    {
        rn->_overrideName2 = Str_New();
        Str_Init(rn->_overrideName2); Str_Set(rn->_overrideName2, overrideName2);
    }
    else
        rn->_overrideName2 = 0;
    return rn;
    }
}

resourcenamespace_t* ResourceNamespace_Construct4(const char* name,
    ddstring_t* (*composeHashNameFunc) (const ddstring_t* path),
    resourcenamespace_namehash_key_t (*hashNameFunc) (const ddstring_t* name),
    const dduri_t* const* searchPaths, uint searchPathsCount, byte flags,
    const char* overrideName)
{
    return ResourceNamespace_Construct5(name, composeHashNameFunc, hashNameFunc,
        searchPaths, searchPathsCount, flags, overrideName, 0);
}

resourcenamespace_t* ResourceNamespace_Construct3(const char* name,
    ddstring_t* (*composeHashNameFunc) (const ddstring_t* path),
    resourcenamespace_namehash_key_t (*hashNameFunc) (const ddstring_t* name),
    const dduri_t* const* searchPaths, uint searchPathsCount, byte flags)
{
    return ResourceNamespace_Construct4(name, composeHashNameFunc, hashNameFunc,
        searchPaths, searchPathsCount, flags, 0);
}

resourcenamespace_t* ResourceNamespace_Construct2(const char* name,
    ddstring_t* (*composeHashNameFunc) (const ddstring_t* path),
    resourcenamespace_namehash_key_t (*hashNameFunc) (const ddstring_t* name),
    const dduri_t* const* searchPaths, uint searchPathsCount)
{
    return ResourceNamespace_Construct3(name, composeHashNameFunc, hashNameFunc,
        searchPaths, searchPathsCount, 0);
}

resourcenamespace_t* ResourceNamespace_Construct(const char* name,
    ddstring_t* (*composeHashNameFunc) (const ddstring_t* path),
    resourcenamespace_namehash_key_t (*hashNameFunc) (const ddstring_t* name))
{
    return ResourceNamespace_Construct2(name, composeHashNameFunc, hashNameFunc, 0, 0);
}

void ResourceNamespace_Destruct(resourcenamespace_t* rn)
{
    assert(rn);
    ResourceNamespace_ClearSearchPaths(rn);
    ResourceNamespace_ClearExtraSearchPaths(rn);
    clearPathHash(rn);
    Str_Free(&rn->_name);
    if(rn->_overrideName)
        Str_Delete(rn->_overrideName);
    if(rn->_overrideName2)
        Str_Delete(rn->_overrideName2);
    free(rn);
}

void ResourceNamespace_Reset(resourcenamespace_t* rn)
{
    assert(rn);
    ResourceNamespace_ClearExtraSearchPaths(rn);
    clearPathHash(rn);
    rn->_flags |= RNF_IS_DIRTY;
}

boolean ResourceNamespace_AddExtraSearchPath(resourcenamespace_t* rn, const dduri_t* newUri)
{
    assert(rn && newUri);

    if(Str_IsEmpty(Uri_Path(newUri)) || !Str_CompareIgnoreCase(Uri_Path(newUri), DIR_SEP_STR))
        return false; // Not suitable.

    // Have we seen this path already (we don't want duplicates)?
    { uint i;
    for(i = 0; i < rn->_extraSearchPathsCount; ++i)
    {
        if(Uri_Equality(rn->_extraSearchPaths[i], newUri))
            return true;
    }}
    { uint i;
    for(i = 0; i < rn->_searchPathsCount; ++i)
    {
        if(Uri_Equality(rn->_searchPaths[i], newUri))
            return true;
    }}

    rn->_extraSearchPaths = (dduri_t**) realloc(rn->_extraSearchPaths,
        sizeof(*rn->_extraSearchPaths) * ++rn->_extraSearchPathsCount);
    if(NULL == rn->_extraSearchPaths)
        Con_Error("ResourceNamespace::AddExtraSearchPath: Failed on reallocation of %lu bytes for "
            "extraSearchPath list.", (unsigned long) sizeof(*rn->_extraSearchPaths) * (rn->_extraSearchPathsCount-1));

    // Prepend to the path list - newer paths have priority.
    if(rn->_extraSearchPathsCount > 1)
        memmove(rn->_extraSearchPaths + 1, rn->_extraSearchPaths,
            sizeof(*rn->_extraSearchPaths) * (rn->_extraSearchPathsCount-1));
    rn->_extraSearchPaths[0] = Uri_ConstructCopy(newUri);

    rn->_flags |= RNF_IS_DIRTY;
    return true;
}

void ResourceNamespace_ClearSearchPaths(resourcenamespace_t* rn)
{
    assert(rn);
    if(!rn->_searchPaths)
        return;
    { uint i;
    for(i = 0; i < rn->_searchPathsCount; ++i)
        Uri_Destruct(rn->_searchPaths[i]);
    }
    free(rn->_searchPaths);
    rn->_searchPaths = 0;
    rn->_searchPathsCount = 0;
    rn->_flags |= RNF_IS_DIRTY;
}

void ResourceNamespace_ClearExtraSearchPaths(resourcenamespace_t* rn)
{
    assert(rn);
    if(!rn->_extraSearchPaths)
        return;
    { uint i;
    for(i = 0; i < rn->_extraSearchPathsCount; ++i)
        Uri_Destruct(rn->_extraSearchPaths[i]);
    }
    free(rn->_extraSearchPaths);
    rn->_extraSearchPaths = 0;
    rn->_extraSearchPathsCount = 0;
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
