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
#include "de_filesys.h"

#include "m_args.h"
#include "filedirectory.h"

#include "resourcenamespace.h"

static void clearPathHash(resourcenamespace_t* rn)
{
    uint i;
    assert(rn);

    for(i = 0; i < RESOURCENAMESPACE_HASHSIZE; ++i)
    {
        resourcenamespace_hashentry_t* entry = &rn->_pathHash[i];
        while(entry->first)
        {
            resourcenamespace_namehash_node_t* nextNode = entry->first->next;
#if _DEBUG
            Str_Free(&entry->first->name);
#endif
            free(entry->first);
            entry->first = nextNode;
        }
        entry->last = 0;
    }
}

static void appendSearchPathsInGroup(resourcenamespace_t* rn,
    resourcenamespace_searchpathgroup_t group, char delimiter, ddstring_t* pathList)
{
    uint i;
    assert(rn && VALID_RESOURCENAMESPACE_SEARCHPATHGROUP(group) && pathList);

    for(i = 0; i < rn->_searchPathsCount[group]; ++i)
    {
        ddstring_t* path = Uri_ComposePath(rn->_searchPaths[group][i]);
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
    resourcenamespace_namehash_key_t key, const struct pathdirectory_node_s* pdNode)
{
    resourcenamespace_namehash_node_t* node;
    assert(rn && pdNode);
    node = rn->_pathHash[key].first;
    while(node && node->userData != pdNode)
    {
        node = node->next;
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

    rn->_hashName = hashNameFunc;
    memset(rn->_pathHash, 0, sizeof(rn->_pathHash));

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
    clearPathHash(rn);
    free(rn);
}

void ResourceNamespace_Clear(resourcenamespace_t* rn)
{
    clearPathHash(rn);
}

boolean ResourceNamespace_AddSearchPath(resourcenamespace_t* rn, const Uri* newUri,
    resourcenamespace_searchpathgroup_t group)
{
    uint i, j;
    assert(rn && newUri && VALID_RESOURCENAMESPACE_SEARCHPATHGROUP(group));

    if(Str_IsEmpty(Uri_Path(newUri)) || !Str_CompareIgnoreCase(Uri_Path(newUri), DIR_SEP_STR))
        return false; // Not suitable.

    // Have we seen this path already (we don't want duplicates)?

    for(i = 0; i < SEARCHPATHGROUP_COUNT; ++i)
    for(j = 0; j < rn->_searchPathsCount[i]; ++j)
    {
        if(Uri_Equality(rn->_searchPaths[i][j], newUri))
            return true;
    }

    rn->_searchPaths[group] = (Uri**) realloc(rn->_searchPaths[group],
        sizeof **rn->_searchPaths * ++rn->_searchPathsCount[group]);
    if(!rn->_searchPaths[group])
        Con_Error("ResourceNamespace::AddExtraSearchPath: Failed on reallocation of %lu bytes for "
            "searchPath list.", (unsigned long) sizeof **rn->_searchPaths * (rn->_searchPathsCount[group]-1));

    // Prepend to the path list - newer paths have priority.
    if(rn->_searchPathsCount[group] > 1)
        memmove(rn->_searchPaths[group] + 1, rn->_searchPaths[group],
            sizeof(*rn->_searchPaths[group]) * (rn->_searchPathsCount[group]-1));
    rn->_searchPaths[group][0] = Uri_NewCopy(newUri);

    return true;
}

void ResourceNamespace_ClearSearchPaths(resourcenamespace_t* rn, resourcenamespace_searchpathgroup_t group)
{
    uint i;
    assert(rn && VALID_RESOURCENAMESPACE_SEARCHPATHGROUP(group));

    if(!rn->_searchPaths[group]) return;

    for(i = 0; i < rn->_searchPathsCount[group]; ++i)
    {
        Uri_Delete(rn->_searchPaths[group][i]);
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

int ResourceNamespace_Iterate2(resourcenamespace_t* rn, const ddstring_t* name,
    int (*callback) (struct pathdirectory_node_s* node, void* paramaters), void* paramaters)
{
    int result = 0;
    assert(rn);
    if(rn->_pathHash && callback)
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
            node = rn->_pathHash[key].first;
            while(node)
            {
                next = node->next;
                result = callback(node->userData, paramaters);
                if(result) break;
                node = next;
            }
            if(result) break;
        }
    }
    return result;
}

int ResourceNamespace_Iterate(resourcenamespace_t* rn, const ddstring_t* name,
    int (*callback) (struct pathdirectory_node_s* node, void* paramaters))
{
    return ResourceNamespace_Iterate2(rn, name, callback, NULL);
}

boolean ResourceNamespace_Add(resourcenamespace_t* rn, const ddstring_t* name,
    const struct pathdirectory_node_s* pdNode)
{
    resourcenamespace_namehash_node_t* node;
    resourcenamespace_namehash_key_t key;
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
        // Create a new node for this name.
        resourcenamespace_hashentry_t* slot;

        isNewNode = true;
        node = (resourcenamespace_namehash_node_t*)malloc(sizeof *node);
        if(!node)
            Con_Error("ResourceNamespace::Add: Failed on allocation of %lu bytes for "
                "new ResourceNamespace::NameHash::Node.", (unsigned long) sizeof *node);

#if _DEBUG
        Str_Init(&node->name); Str_Set(&node->name, Str_Text(name));
#endif
        node->userData = NULL;
        node->next = NULL;

        // Link it to the list for this bucket.
        slot = &rn->_pathHash[key];
        if(slot->last) slot->last->next = node;
        slot->last = node;
        if(!slot->first) slot->first = node;
    }

    // (Re)configure this node.
    node->userData = (void*)pdNode;

    return isNewNode;
}

#if _DEBUG
void ResourceNamespace_Print(resourcenamespace_t* rn)
{
    ddstring_t path;
    size_t n;
    uint i;
    assert(rn);

    Con_Printf("ResourceNamespace [%p]:\n", (void*)rn);
    n = 0;
    Str_Init(&path);
    for(i = 0; i < RESOURCENAMESPACE_HASHSIZE; ++i)
    {
        resourcenamespace_hashentry_t* entry = &rn->_pathHash[i];
        resourcenamespace_namehash_node_t* node = entry->first;
        while(node)
        {
            Str_Clear(&path);
            PathDirectory_ComposePath(PathDirectoryNode_Directory(node->userData), node->userData, &path, NULL, FILEDIRECTORY_DELIMITER);
            Con_Printf("  %lu: %lu:\"%s\" -> %s\n", (unsigned long)n, (unsigned long)i,
                Str_Text(&node->name), Str_Text(&path));
            ++n;
            node = node->next;
        }
    }
    Con_Printf("  %lu %s in namespace.\n", (unsigned long) n, (n==1? "resource":"resources"));
    Str_Free(&path);
}
#endif
