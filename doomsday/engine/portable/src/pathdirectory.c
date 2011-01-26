/**\file pathdirectory.c
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

#include "de_base.h"
#include "de_console.h"

#include "sys_reslocator.h"
#include "sys_timer.h"
#include "sys_file.h"
#include "sys_findfile.h"

#include "pathdirectory.h"

typedef struct pathdirectory_pair_s {
    ddstring_t path;
    void* data;
} pathdirectory_pair_t;

typedef struct pathdirectory_node_s {
    struct pathdirectory_node_s* next;
    struct pathdirectory_node_s* parent;
    pathdirectory_pathtype_t type;
    pathdirectory_pair_t pair;
} pathdirectory_node_t;

static size_t countNodes(pathdirectory_t* pd, pathdirectory_pathtype_t type)
{
    assert(pd);
    {
    pathdirectory_node_t* node;
    size_t n = 0;
    for(node = pd->_head; node; node = node->next)
        if(!VALID_PATHDIRECTORY_PATHTYPE(type) || node->type == type)
            ++n;
    return n;
    }
}

/**
 * @return  [ a new | the ] directory node that matches the name and type and
 * which has the specified parent node.
 */
static pathdirectory_node_t* direcNode(pathdirectory_t* pd, pathdirectory_node_t* parent,
    pathdirectory_pathtype_t type, const ddstring_t* name, void* leafData)
{
    assert(pd && name && !Str_IsEmpty(name));
    {
    pathdirectory_node_t* node;

    // Have we already encountered this? Just iterate through all nodes.
    for(node = pd->_head; node; node = node->next)
    {
        if(node->parent != parent)
            continue;
        if(node->type != type)
            continue;
        if(Str_Length(&node->pair.path) < Str_Length(name))
            continue;
        if(!strnicmp(Str_Text(&node->pair.path), Str_Text(name),
                     Str_Length(&node->pair.path) - (node->type == PT_DIRECTORY? 1:0)))
            return node;
    }

    // Add a new node.
    if((node = malloc(sizeof(*node))) == 0)
        Con_Error("direcNode: failed on allocation of %lu bytes for new node.",
                  (unsigned long) sizeof(*node));

    if(pd->_tail)
        pd->_tail->next = node;
    pd->_tail = node;
    if(!pd->_head)
        pd->_head = node;

    node->type = type;
    Str_Init(&node->pair.path); Str_Copy(&node->pair.path, name);
    node->pair.data = leafData;
    node->parent = parent;
    node->next = 0;

    return node;
    }
}

/**
 * The path is split into as many nodes as necessary. Parent links are set.
 *
 * @return  The node that identifies the given path.
 */
static pathdirectory_node_t* buildDirecNodes(pathdirectory_t* pd,
    const ddstring_t* path, void* leafData, char delimiter)
{
    assert(pd && path);
    {
    pathdirectory_node_t* node = 0, *parent = 0;
    ddstring_t part;
    const char* p;

    // Continue splitting as long as there are parts.
    Str_Init(&part);
    p = Str_Text(path);
    while((p = Str_CopyDelim2(&part, p, delimiter, 0))) // Get the next part.
    {
        node = direcNode(pd, parent, PT_DIRECTORY, &part, 0);
        parent = node;
    }

    if(Str_Length(&part) != 0)
    {
        pathdirectory_pathtype_t type = (Str_RAt(&part, 0) != delimiter? PT_LEAF : PT_DIRECTORY);
        node = direcNode(pd, parent, type, &part, leafData);
    }

    Str_Free(&part);
    return node;
    }
}

static void clearDirectory(pathdirectory_t* pd)
{
    assert(pd);
    // Free the directory nodes.
    while(pd->_head)
    {
        pathdirectory_node_t* next = pd->_head->next;
        Str_Free(&pd->_head->pair.path);
        free(pd->_head);
        pd->_head = next;
    }
    pd->_tail = 0;
}

typedef struct {
    pathdirectory_node_t* foundNode;
    const ddstring_t* searchPath;
    char delimiter;
} findpathworker_paramaters_t;

static int findPathWorker(pathdirectory_node_t* node, void* paramaters)
{
    assert(node && paramaters);
    {
    findpathworker_paramaters_t* p = (findpathworker_paramaters_t*)paramaters;
    if(PathDirectoryNode_MatchDirectory(node, p->searchPath, p->delimiter))
    {
        p->foundNode = node;
        return 1; // A match; stop!
    }
    return 0; // Continue searching.
    }
}

static int iteratePaths(pathdirectory_t* pd, pathdirectory_pathtype_t type,
    pathdirectory_node_t* parent, int (*callback) (pathdirectory_node_t* node, void* paramaters),
    void* paramaters)
{
    assert(pd && callback);
    {
    int result = 0;
    pathdirectory_node_t* node;
    for(node = pd->_head; node; node = node->next)
    {
        if(parent && node->parent != parent)
            continue;
        if(VALID_PATHDIRECTORY_PATHTYPE(type) && node->type != type)
            continue;
        if((result = callback(node, paramaters)) != 0)
            break;
    }
    return result;
    }
}

static int const_iteratePaths(pathdirectory_t* pd, pathdirectory_pathtype_t type,
    pathdirectory_node_t* parent, int (*callback) (const pathdirectory_node_t* node, void* paramaters),
    void* paramaters)
{
    assert(pd && callback);
    {
    int result = 0;
    pathdirectory_node_t* node;
    for(node = pd->_head; node; node = node->next)
    {
        if(parent && node->parent != parent)
            continue;
        if(VALID_PATHDIRECTORY_PATHTYPE(type) && node->type != type)
            continue;
        if((result = callback(node, paramaters)) != 0)
            break;
    }
    return result;
    }
}

pathdirectory_t* PathDirectory_ConstructDefault(void)
{
    pathdirectory_t* pd;
    if((pd = malloc(sizeof(*pd))) == 0)
        Con_Error("PathDirectory::Construct: Failed on allocation of %lu bytes for new PathDirectory.",
                  (unsigned long) sizeof(*pd));
    pd->_head = pd->_tail = 0;
    return pd;
}

void PathDirectory_Destruct(pathdirectory_t* pd)
{
    assert(pd);
    clearDirectory(pd);
    free(pd);
}

void PathDirectory_Clear(pathdirectory_t* pd)
{
    assert(pd);
    clearDirectory(pd);
}

const pathdirectory_node_t* PathDirectory_FindStr(pathdirectory_t* pd, const ddstring_t* searchPath)
{
    assert(pd);
    if(searchPath && !Str_IsEmpty(searchPath))
    {
        findpathworker_paramaters_t p;
        p.searchPath = searchPath;
        p.foundNode = 0;
        iteratePaths(pd, PT_ANY, 0, findPathWorker, (void*)&p);
        return p.foundNode;
    }
    return 0;
}

const pathdirectory_node_t* PathDirectory_Find(pathdirectory_t* pd, const char* _searchPath)
{
    assert(pd);
    if(_searchPath && _searchPath[0])
    {
        const pathdirectory_node_t* result;
        ddstring_t searchPath; Str_Init(&searchPath);
        result = PathDirectory_FindStr(pd, &searchPath);
        Str_Free(&searchPath);
        return result;
    }
    return 0;
}

void PathDirectory_Insert(pathdirectory_t* pd, const ddstring_t* path, void* value,
    char delimiter)
{
    assert(pd);
    if(!path || Str_IsEmpty(path))
        return;
    buildDirecNodes(pd, path, value, delimiter);
/*#if _DEBUG
    PathDirectory_Print(pd);
#endif*/
}

boolean PathDirectory_IsEmpty(pathdirectory_t* pd)
{
    assert(pd);
    return (pd->_head == 0);
}

int PathDirectory_Iterate2(pathdirectory_t* pd, pathdirectory_pathtype_t type,
    pathdirectory_node_t* parent, int (*callback) (const pathdirectory_node_t* node, void* paramaters),
    void* paramaters)
{
    assert(pd && (type == PT_ANY || VALID_PATHDIRECTORY_PATHTYPE(type)));
    return const_iteratePaths(pd, type, parent, callback, paramaters);
}

int PathDirectory_Iterate(pathdirectory_t* pd, pathdirectory_pathtype_t type,
    pathdirectory_node_t* parent, int (*callback) (const pathdirectory_node_t* node, void* paramaters))
{
    return PathDirectory_Iterate2(pd, type, parent, callback, 0);
}

ddstring_t* PathDirectory_AllPaths(pathdirectory_t* pd, pathdirectory_pathtype_t type,
    size_t* count)
{
    assert(pd && count);
    {
    ddstring_t* paths = 0;
    *count = countNodes(pd, type);
    if(*count != 0)
    {
        if((paths = Z_Malloc(sizeof(*paths) * (*count), PU_APPSTATIC, 0)) == 0)
            Con_Error("PathDirectory::AllPaths: failed on allocation of %lu bytes for list.",
                      (unsigned long) sizeof(*paths));

        { pathdirectory_node_t* node;
        ddstring_t* path = paths;
        for(node = pd->_head; node; node = node->next)
        {
            if(VALID_PATHDIRECTORY_PATHTYPE(type) && node->type != type)
                continue;
            Str_Init(path);
            PathDirectoryNode_ComposePath(node, path);
            path++;
        }}
    }
    return paths;
    }
}

#if _DEBUG
static int C_DECL comparePaths(const void* a, const void* b)
{
    return stricmp(Str_Text((ddstring_t*)a), Str_Text((ddstring_t*)b));
}

void PathDirectory_Print(pathdirectory_t* pd)
{
    assert(pd);
    {
    size_t numLeafs, n = 0;
    ddstring_t* pathList;

    Con_Printf("PathDirectory:\n");
    if((pathList = PathDirectory_AllPaths(pd, PT_LEAF, &numLeafs)) != 0)
    {
        qsort(pathList, numLeafs, sizeof(*pathList), comparePaths);
        do
        {
            Con_Printf("  %s\n", Str_Text(F_PrettyPath(pathList + n)));
            Str_Free(pathList + n);
        } while(++n < numLeafs);
        Z_Free(pathList);
    }
    Con_Printf("  %lu %s in directory.\n", (unsigned long)numLeafs, (numLeafs==1? "path":"paths"));
    }
}
#endif

boolean PathDirectoryNode_MatchDirectory(const pathdirectory_node_t* node,
    const ddstring_t* searchPath, char delimiter)
{
    assert(node && searchPath);
    {
    filename_t dir;

    // Where does the directory searchPath begin? We'll do this in reverse order.
    strncpy(dir, Str_Text(searchPath), FILENAME_T_MAXLEN);
    { char* pos;
    while((pos = strrchr(dir, delimiter)) != 0)
    {
        // Does this match?
        if(Str_Length(&node->pair.path) < Str_Length(searchPath) ||
           strnicmp(Str_Text(&node->pair.path), Str_Text(searchPath),
                    Str_Length(&node->pair.path) - (node->type == PT_DIRECTORY? 1:0)))
            return false;

        // Are there no more parent directories?
        if(!node->parent)
            return false;

        // So far so good. Move one directory level upwards.
        node = node->parent;
        // The string now ends here.
        *pos = 0;
    }}

    // Anything remaining is the root directory or file searchPath - does it match?
    if(Str_Length(&node->pair.path) < Str_Length(searchPath) ||
       strnicmp(Str_Text(&node->pair.path), Str_Text(searchPath),
                Str_Length(&node->pair.path) - (node->type == PT_DIRECTORY? 1:0)))
        return false;

    // We must have now arrived at the search target.
    return true;
    }
}

void PathDirectoryNode_ComposePath(const pathdirectory_node_t* node, ddstring_t* foundPath)
{
    assert(node && foundPath);
    Str_Prepend(foundPath, Str_Text(&node->pair.path));
    node = node->parent;
    while(node)
    {
        Str_Prepend(foundPath, Str_Text(&node->pair.path));
        node = node->parent;
    }
}
