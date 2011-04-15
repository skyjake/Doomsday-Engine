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

/**
 * This is a hash function. It uses the resource name to generate a
 * somewhat-random number between 0 and PATHDIRECTORY_HASHSIZE.
 *
 * @return  The generated hash key.
 */
static ushort hashPath(const char* path, size_t len, char delimiter)
{
    assert(path);
    {
    const char* c = path + len - 1;
    ushort key = 0;
    int op = 0;
    size_t i;

    // Skip over any trailing delimiters.
    for(i = len; *c && *c == delimiter && i-- > 0; c--) {}

    // Compose the hash.
    for(; *c && *c != delimiter && i-- > 0; c--)
    {
        switch(op)
        {
        case 0: key ^= tolower(*c); ++op;   break;
        case 1: key *= tolower(*c); ++op;   break;
        case 2: key -= tolower(*c);   op=0; break;
        }
    }
    return key % PATHDIRECTORY_HASHSIZE;
    }
}

static size_t countNodes(pathdirectory_t* pd, pathdirectory_pathtype_t type)
{
    assert(NULL != pd);
    {
    boolean compareType = VALID_PATHDIRECTORY_PATHTYPE(type);
    pathdirectory_node_t* node;
    size_t n = 0;
    ushort i;
    for(i = 0; i < PATHDIRECTORY_HASHSIZE; ++i)
    {
        for(node = (pathdirectory_node_t*) pd->_hashTable[i];
            NULL != node; node = node->next)
            if(!compareType || node->type == type)
                ++n;
    }
    return n;
    }
}

/**
 * @return  [ a new | the ] directory node that matches the name and type and
 * which has the specified parent node.
 */
static pathdirectory_node_t* direcNode(pathdirectory_t* pd, char delimiter,
    pathdirectory_node_t* parent, pathdirectory_pathtype_t type,
    const ddstring_t* name, void* leafData)
{
    assert(NULL != pd && NULL != name && !Str_IsEmpty(name));
    {
    pathdirectory_node_t* node;
    ushort hash = hashPath(Str_Text(name), Str_Length(name), delimiter);

    // Have we already encountered this?
    for(node = pd->_hashTable[hash]; NULL != node; node = node->next)
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
    node = (pathdirectory_node_t*) malloc(sizeof(*node));
    if(NULL == node)
        Con_Error("PathDirectory::direcNode: failed on allocation of %lu bytes for "
            "new PathDirectory::Node.", (unsigned long) sizeof(*node));

    node->next = pd->_hashTable[hash];
    pd->_hashTable[hash] = node;

    node->type = type;
    Str_Init(&node->pair.path); Str_Copy(&node->pair.path, name);
    node->pair.data = leafData;
    node->parent = parent;

    return node;
    }
}

/**
 * The path is split into as many nodes as necessary. Parent links are set.
 *
 * @return  The node that identifies the given path.
 */
static pathdirectory_node_t* buildDirecNodes(pathdirectory_t* pd,
    char delimiter, const ddstring_t* path, void* leafData)
{
    assert(NULL != pd && NULL != path);
    {
    pathdirectory_node_t* node = NULL, *parent = NULL;
    ddstring_t part;
    const char* p;

    // Continue splitting as long as there are parts.
    Str_Init(&part);
    p = Str_Text(path);
    while(NULL != (p = Str_CopyDelim2(&part, p, delimiter, 0))) // Get the next part.
    {
        node = direcNode(pd, delimiter, parent, PT_DIRECTORY, &part, 0);
        parent = node;
    }

    if(Str_Length(&part) != 0)
    {
        pathdirectory_pathtype_t type = (Str_RAt(&part, 0) != delimiter? PT_LEAF : PT_DIRECTORY);
        node = direcNode(pd, delimiter, parent, type, &part, leafData);
    }

    Str_Free(&part);
    return node;
    }
}

static pathdirectory_node_t* addPath(pathdirectory_t* fd, char delimiter,
    const ddstring_t* path, void* leafData)
{
    assert(NULL != fd && NULL != path);
    return buildDirecNodes(fd, delimiter, path, leafData);
}

static void clearDirectory(pathdirectory_t* pd)
{
    assert(NULL != pd);
    { ushort i;
    for(i = 0; i < PATHDIRECTORY_HASHSIZE; ++i)
    while(NULL != pd->_hashTable[i])
    {
        pathdirectory_node_t* next = pd->_hashTable[i]->next;
        Str_Free(&pd->_hashTable[i]->pair.path);
        free(pd->_hashTable[i]);
        pd->_hashTable[i] = next;
    }}
}

typedef struct {
    const ddstring_t* searchPath;
    pathdirectory_node_t* foundNode;
    char delimiter;
} findpathworker_paramaters_t;

static int findPathWorker(pathdirectory_node_t* node, void* paramaters)
{
    assert(NULL != node && NULL != paramaters);
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
    assert(NULL != pd && NULL != callback);
    {
    boolean compareType = VALID_PATHDIRECTORY_PATHTYPE(type);
    pathdirectory_node_t* node;
    int result = 0;
    ushort i;
    for(i = 0; i < PATHDIRECTORY_HASHSIZE; ++i)
    for(node = pd->_hashTable[i]; NULL != node; node = node->next)
    {
        if(NULL != parent && node->parent != parent)
            continue;
        if(compareType && node->type != type)
            continue;
        if(0 != (result = callback(node, paramaters)))
            break;
    }
    return result;
    }
}

static int const_iteratePaths(pathdirectory_t* pd, pathdirectory_pathtype_t type,
    pathdirectory_node_t* parent, int (*callback) (const pathdirectory_node_t* node, void* paramaters),
    void* paramaters)
{
    assert(NULL != pd && NULL != callback);
    {
    boolean compareType = VALID_PATHDIRECTORY_PATHTYPE(type);
    pathdirectory_node_t* node;
    int result = 0;
    ushort i;
    for(i = 0; i < PATHDIRECTORY_HASHSIZE; ++i)
    for(node = pd->_hashTable[i]; NULL != node; node = node->next)
    {
        if(NULL != parent && node->parent != parent)
            continue;
        if(compareType && node->type != type)
            continue;
        if(0 != (result = callback(node, paramaters)))
            break;
    }
    return result;
    }
}

pathdirectory_t* PathDirectory_ConstructDefault(void)
{
    pathdirectory_t* pd = (pathdirectory_t*) malloc(sizeof(*pd));
    if(NULL == pd)
        Con_Error("PathDirectory::Construct: Failed on allocation of %lu bytes for"
            "new PathDirectory.", (unsigned long) sizeof(*pd));
    memset(pd->_hashTable, 0, sizeof(pd->_hashTable));
    return pd;
}

void PathDirectory_Destruct(pathdirectory_t* pd)
{
    assert(NULL != pd);
    clearDirectory(pd);
    free(pd);
}

void PathDirectory_Clear(pathdirectory_t* pd)
{
    assert(NULL != pd);
    clearDirectory(pd);
}

const pathdirectory_node_t* PathDirectory_FindStr(pathdirectory_t* pd, const ddstring_t* searchPath)
{
    assert(NULL != pd);
    if(NULL != searchPath && !Str_IsEmpty(searchPath))
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
    assert(NULL != pd);
    if(NULL != _searchPath && _searchPath[0])
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
    assert(NULL != pd);
    if(NULL == path || Str_IsEmpty(path))
        return;
    addPath(pd, delimiter, path, value);
/*#if _DEBUG
    PathDirectory_Print(pd);
#endif*/
}

int PathDirectory_Iterate2(pathdirectory_t* pd, pathdirectory_pathtype_t type,
    pathdirectory_node_t* parent, int (*callback) (const pathdirectory_node_t* node, void* paramaters),
    void* paramaters)
{
    assert(NULL != pd && (type == PT_ANY || VALID_PATHDIRECTORY_PATHTYPE(type)));
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
    assert(NULL != pd && 0 != count);
    {
    ddstring_t* paths = NULL;
    *count = countNodes(pd, type);
    if(*count != 0)
    {
        boolean compareType = VALID_PATHDIRECTORY_PATHTYPE(type);
        pathdirectory_node_t* node;
        ddstring_t* path;
        ushort i;

        paths = (ddstring_t*) malloc(sizeof(*paths) * (*count));
        if(NULL == paths)
            Con_Error("PathDirectory::AllPaths: Failed on allocation of %lu bytes for "
                "new path list.", (unsigned long) sizeof(*paths));

        path = paths;
        for(i = 0; i < PATHDIRECTORY_HASHSIZE; ++i)
        for(node = pd->_hashTable[i]; NULL != node; node = node->next)
        {
            if(compareType && node->type != type) continue;

            Str_Init(path);
            PathDirectoryNode_ComposePath(node, path);
            path++;
        }
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
    assert(NULL != pd);
    {
    size_t numLeafs, n = 0;
    ddstring_t* pathList;

    Con_Printf("PathDirectory:\n");
    if(NULL != (pathList = PathDirectory_AllPaths(pd, PT_LEAF, &numLeafs)))
    {
        qsort(pathList, numLeafs, sizeof(*pathList), comparePaths);
        do
        {
            Con_Printf("  %s\n", Str_Text(F_PrettyPath(pathList + n)));
            Str_Free(pathList + n);
        } while(++n < numLeafs);
        free(pathList);
    }
    Con_Printf("  %lu %s in directory.\n", (unsigned long)numLeafs, (numLeafs==1? "path":"paths"));
    }
}
#endif

boolean PathDirectoryNode_MatchDirectory(const pathdirectory_node_t* node,
    const ddstring_t* searchPath, char delimiter)
{
    assert(NULL != node && NULL != searchPath);
    {
    filename_t dir;

    // Where does the directory searchPath begin? We'll do this in reverse order.
    strncpy(dir, Str_Text(searchPath), FILENAME_T_MAXLEN);
    { char* pos;
    while(NULL != (pos = strrchr(dir, delimiter)))
    {
        // Does this match?
        if(Str_Length(&node->pair.path) < Str_Length(searchPath) ||
           strnicmp(Str_Text(&node->pair.path), Str_Text(searchPath),
                    Str_Length(&node->pair.path) - (node->type == PT_DIRECTORY? 1:0)))
            return false;

        // Are there no more parent directories?
        if(NULL == node->parent)
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
    assert(NULL != node && NULL != foundPath);
    Str_Prepend(foundPath, Str_Text(&node->pair.path));
    node = node->parent;
    while(NULL != node)
    {
        Str_Prepend(foundPath, Str_Text(&node->pair.path));
        node = node->parent;
    }
}
