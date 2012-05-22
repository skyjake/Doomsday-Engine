/**\file resourcenamespace.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
#include "de_filesys.h"

#include "pathdirectory.h"
#include "resourcenamespace.h"

typedef struct {
    /// Directory node for this resource in the owning PathDirectory
    PathDirectoryNode* directoryNode;

#if _DEBUG
    /// Symbolic name of this resource.
    ddstring_t name;
#endif

    /// User data associated with this resource.
    void* userData;
} resourcenamespace_record_t;

typedef struct resourcenamespace_namehash_node_s {
    struct resourcenamespace_namehash_node_s* next;
    resourcenamespace_record_t resource;
} resourcenamespace_namehash_node_t;

/**
 * Name search hash.
 */
typedef struct {
    resourcenamespace_namehash_node_t* first;
    resourcenamespace_namehash_node_t* last;
} resourcenamespace_hashentry_t;

typedef struct resourcenamespace_namehash_s {
    resourcenamespace_hashentry_t hash[RESOURCENAMESPACE_HASHSIZE];
} resourcenamespace_namehash_t;

static resourcenamespace_record_t* initResourceRecord(resourcenamespace_record_t* res)
{
    assert(res);
    res->directoryNode = NULL;
#if _DEBUG
    Str_Init(&res->name);
#endif
    res->userData = NULL;
    return res;
}

static void destroyResourceRecord(resourcenamespace_record_t* res)
{
    assert(res);
#if _DEBUG
    Str_Free(&res->name);
#endif
}

static void clearNameHash(resourcenamespace_t* rn)
{
    uint i;
    assert(rn);
    if(!rn->_nameHash) return;

    for(i = 0; i < RESOURCENAMESPACE_HASHSIZE; ++i)
    {
        resourcenamespace_hashentry_t* entry = &rn->_nameHash->hash[i];
        while(entry->first)
        {
            resourcenamespace_namehash_node_t* nextNode = entry->first->next;
#if _DEBUG
            destroyResourceRecord(&entry->first->resource);
#endif
            free(entry->first);
            entry->first = nextNode;
        }
        entry->last = 0;
    }
}

static void destroyNameHash(resourcenamespace_t* rn)
{
    if(!rn->_nameHash) return;
    clearNameHash(rn);
    free(rn->_nameHash), rn->_nameHash = NULL;
}

static void appendSearchPathsInGroup(resourcenamespace_t* rn,
    resourcenamespace_searchpathgroup_t group, char delimiter, ddstring_t* pathList)
{
    uint i;
    assert(rn && VALID_RESOURCENAMESPACE_SEARCHPATHGROUP(group) && pathList);

    for(i = 0; i < rn->_searchPathsCount[group]; ++i)
    {
        ddstring_t* path = Uri_Compose(rn->_searchPaths[group][i].uri);
        Str_Appendf(pathList, "%s%c", Str_Text(path), delimiter);
        Str_Delete(path);
    }
}

static ddstring_t* formSearchPathList(resourcenamespace_t* rn, ddstring_t* pathList, char delimiter)
{
    appendSearchPathsInGroup(rn, SPG_OVERRIDE, delimiter, pathList);
    appendSearchPathsInGroup(rn, SPG_EXTRA, delimiter, pathList);
    appendSearchPathsInGroup(rn, SPG_DEFAULT, delimiter, pathList);
    appendSearchPathsInGroup(rn, SPG_FALLBACK, delimiter, pathList);
    return pathList;
}

static resourcenamespace_namehash_node_t* findPathNodeInHash(resourcenamespace_t* rn,
    resourcenamespace_namehash_key_t key, const PathDirectoryNode* pdNode)
{
    resourcenamespace_namehash_node_t* node = NULL;
    assert(rn && pdNode);
    if(rn->_nameHash)
    {
        node = rn->_nameHash->hash[key].first;
        while(node && node->resource.directoryNode != pdNode)
        {
            node = node->next;
        }
    }
    return node;
}

resourcenamespace_t* ResourceNamespace_New(
    resourcenamespace_namehash_key_t (*hashNameFunc) (const ddstring_t* name))
{
    resourcenamespace_t* rn;
    uint i;
    assert(hashNameFunc);

    rn = (resourcenamespace_t*) malloc(sizeof *rn);
    if(!rn)
        Con_Error("ResourceNamespace::Construct: Failed on allocation of %lu bytes.", (unsigned long) sizeof *rn);

    for(i = 0; i < SEARCHPATHGROUP_COUNT; ++i)
    {
        rn->_searchPathsCount[i] = 0;
        rn->_searchPaths[i] = NULL;
    }

    rn->_nameHash = NULL;
    rn->_hashName = hashNameFunc;

    return rn;
}

void ResourceNamespace_Delete(resourcenamespace_t* rn)
{
    uint i;
    assert(rn);
    for(i = 0; i < SEARCHPATHGROUP_COUNT; ++i)
    {
        ResourceNamespace_ClearSearchPaths(rn, (resourcenamespace_searchpathgroup_t)i);
    }
    destroyNameHash(rn);
    free(rn);
}

void ResourceNamespace_Clear(resourcenamespace_t* rn)
{
    clearNameHash(rn);
}

boolean ResourceNamespace_AddSearchPath(resourcenamespace_t* rn, int flags,
    const Uri* _searchPath, resourcenamespace_searchpathgroup_t group)
{
    Uri* searchPath;
    uint i, j;
    assert(rn);

    // Is this suitable?
    if(!_searchPath || Str_IsEmpty(Uri_Path(_searchPath)) ||
       !Str_CompareIgnoreCase(Uri_Path(_searchPath), "/"))
        return false;

    if(!VALID_RESOURCENAMESPACE_SEARCHPATHGROUP(group))
    {
#if _DEBUG
        Con_Message("ResourceNamespace::AddSearchPath: Invalid SearchPathGroup %i, ignoring.\n", (int)group);
#endif
        return false;
    }

    // Ensure this is a well formed path.
    { ddstring_t* path = Uri_Compose(_searchPath);

    // Only directories can be search paths.
    F_AppendMissingSlash(path);

    searchPath = Uri_NewWithPath2(Str_Text(path), RC_NULL);
    Str_Delete(path);
    }

    // Have we seen this path already (we don't want duplicates)?

    for(i = 0; i < SEARCHPATHGROUP_COUNT; ++i)
    for(j = 0; j < rn->_searchPathsCount[i]; ++j)
    {
        if(Uri_Equality(rn->_searchPaths[i][j].uri, searchPath))
        {
            rn->_searchPaths[i][j].flags = flags;
            Uri_Delete(searchPath);
            return true;
        }
    }

    rn->_searchPaths[group] = realloc(rn->_searchPaths[group],
        sizeof(**rn->_searchPaths) * ++rn->_searchPathsCount[group]);
    if(!rn->_searchPaths[group])
        Con_Error("ResourceNamespace::AddSearchPath: Failed on reallocation of %lu bytes for "
            "searchPath list.", (unsigned long) sizeof **rn->_searchPaths * (rn->_searchPathsCount[group]-1));

    // Prepend to the path list - newer paths have priority.
    if(rn->_searchPathsCount[group] > 1)
        memmove(rn->_searchPaths[group] + 1, rn->_searchPaths[group],
            sizeof(*rn->_searchPaths[group]) * (rn->_searchPathsCount[group]-1));
    rn->_searchPaths[group][0].uri = searchPath;
    rn->_searchPaths[group][0].flags = flags;

    return true;
}

void ResourceNamespace_ClearSearchPaths(resourcenamespace_t* rn, resourcenamespace_searchpathgroup_t group)
{
    uint i;
    assert(rn);

    if(!VALID_RESOURCENAMESPACE_SEARCHPATHGROUP(group))
    {
#if _DEBUG
        Con_Message("ResourceNamespace::ClearSearchPaths: Invalid SearchPathGroup %i, ignoring.\n", (int)group);
#endif
        return;
    }

    if(!rn->_searchPaths[group]) return;

    for(i = 0; i < rn->_searchPathsCount[group]; ++i)
    {
        Uri_Delete(rn->_searchPaths[group][i].uri);
    }
    free(rn->_searchPaths[group]);
    rn->_searchPaths[group] = 0;
    rn->_searchPathsCount[group] = 0;
}

ddstring_t* ResourceNamespace_ComposeSearchPathList2(resourcenamespace_t* rn, char delimiter)
{
    return formSearchPathList(rn, Str_New(), delimiter);
}

ddstring_t* ResourceNamespace_ComposeSearchPathList(resourcenamespace_t* rn)
{
    return ResourceNamespace_ComposeSearchPathList2(rn, ';');
}

int ResourceNamespace_IterateSearchPaths2(resourcenamespace_t* rn,
    int (*callback) (const Uri* uri, int flags, void* paramaters), void* paramaters)
{
    int result = 0;
    assert(rn);
    if(callback)
    {
        uint i, j;
        for(i = 0; i < SEARCHPATHGROUP_COUNT; ++i)
        for(j = 0; j < rn->_searchPathsCount[i]; ++j)
        {
            resourcenamespace_searchpath_t* path = &rn->_searchPaths[i][j];
            result = callback(path->uri, path->flags, paramaters);
            if(result) return result;
        }
    }
    return result;
}

int ResourceNamespace_IterateSearchPaths(resourcenamespace_t* rn,
    int (*callback) (const Uri* uri, int flags, void* paramaters))
{
    return ResourceNamespace_IterateSearchPaths2(rn, callback, NULL);
}

int ResourceNamespace_Iterate2(resourcenamespace_t* rn, const ddstring_t* name,
    int (*callback) (PathDirectoryNode* node, void* paramaters), void* paramaters)
{
    int result = 0;
    assert(rn);
    if(rn->_nameHash && callback)
    {
        resourcenamespace_namehash_node_t* node, *next;
        resourcenamespace_namehash_key_t from, to, key;
        if(name)
        {
            from = rn->_hashName(name);
            if(from >= RESOURCENAMESPACE_HASHSIZE)
            {
                Con_Error("ResourceNamespace::Iterate: Hashing of name '%s' in [%p] produced invalid key %u.", Str_Text(name), (void*)rn, (unsigned short)from);
                exit(1); // Unreachable.
            }
            to = from;
        }
        else
        {
            from = 0;
            to = RESOURCENAMESPACE_HASHSIZE-1;
        }

        for(key = from; key < to+1; ++key)
        {
            node = rn->_nameHash->hash[key].first;
            while(node)
            {
                next = node->next;
                result = callback(node->resource.directoryNode, paramaters);
                if(result) break;
                node = next;
            }
            if(result) break;
        }
    }
    return result;
}

int ResourceNamespace_Iterate(resourcenamespace_t* rn, const ddstring_t* name,
    int (*callback) (PathDirectoryNode* node, void* paramaters))
{
    return ResourceNamespace_Iterate2(rn, name, callback, NULL);
}

boolean ResourceNamespace_Add(resourcenamespace_t* rn, const ddstring_t* name,
    PathDirectoryNode* pdNode, void* userData)
{
    resourcenamespace_namehash_node_t* node;
    resourcenamespace_namehash_key_t key;
    resourcenamespace_record_t* res;
    boolean isNewNode = false;
    assert(rn && name && pdNode);

    key = rn->_hashName(name);
    if(key >= RESOURCENAMESPACE_HASHSIZE)
    {
        Con_Error("ResourceNamespace::Add: Hashing of name '%s' in [%p] produced invalid key %u.", Str_Text(name), (void*)rn, (unsigned short)key);
        exit(1); // Unreachable.
    }

    // Is this a new resource name & path pair?
    node = findPathNodeInHash(rn, key, pdNode);
    if(!node)
    {
        resourcenamespace_hashentry_t* slot;

        // Is it time to allocate the name hash?
        if(!rn->_nameHash)
        {
            rn->_nameHash = (resourcenamespace_namehash_t*)calloc(1, sizeof *rn->_nameHash);
            if(!rn->_nameHash)
                Con_Error("ResourceNamespace::Add: Failed on allocation of %lu bytes for new ResourceNamespace::NameHash.", (unsigned long) sizeof *rn->_nameHash);
        }

        // Create a new node for this.
        node = (resourcenamespace_namehash_node_t*)malloc(sizeof *node);
        node->next = NULL;
        if(!node)
            Con_Error("ResourceNamespace::Add: Failed on allocation of %lu bytes for new ResourceNamespace::NameHash::Node.", (unsigned long) sizeof *node);
        isNewNode = true;

        // Link it to the list for this bucket.
        slot = &rn->_nameHash->hash[key];
        if(slot->last) slot->last->next = node;
        slot->last = node;
        if(!slot->first) slot->first = node;

        res = initResourceRecord(&node->resource);
#if _DEBUG
        Str_Set(&res->name, Str_Text(name));
#endif
    }
    else
    {
        res = &node->resource;
    }

    // (Re)configure this record.
    res->directoryNode = pdNode;
    res->userData = userData;

    return isNewNode;
}

#if _DEBUG
static void printResourceRecord(resourcenamespace_record_t* res)
{
    ddstring_t path;
    Str_Init(&path);
    PathDirectory_ComposePath(PathDirectoryNode_Directory(res->directoryNode), res->directoryNode, &path, NULL, DIR_SEP_CHAR);
    Con_Printf("\"%s\" -> %s\n", Str_Text(&res->name), Str_Text(&path));
    Str_Free(&path);
}

void ResourceNamespace_Print(resourcenamespace_t* rn)
{
    resourcenamespace_hashentry_t* entry;
    resourcenamespace_namehash_node_t* node;
    size_t n;
    uint i;
    assert(rn);

    Con_Printf("ResourceNamespace [%p]:\n", (void*)rn);
    n = 0;
    if(rn->_nameHash)
    for(i = 0; i < RESOURCENAMESPACE_HASHSIZE; ++i)
    {
        entry = &rn->_nameHash->hash[i];
        node = entry->first;

        while(node)
        {
            Con_Printf("  %lu: %lu:", (unsigned long)n, (unsigned long)i);
            printResourceRecord(&node->resource);
            node = node->next;
            ++n;
        }
    }
    Con_Printf("  %lu %s in namespace.\n", (unsigned long) n, (n==1? "resource":"resources"));
}
#endif
