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

#include "pathdirectory.h"

typedef struct pathdirectory_internname_s {
    ddstring_t name;
    ushort hash;
} pathdirectory_internname_t;

typedef struct pathdirectory_node_userdatapair_s {
    pathdirectory_internnameid_t nameId;
    void* data;
} pathdirectory_node_userdatapair_t;

typedef struct pathdirectory_node_s {
    /// Next node in the hashed path bucket.
    struct pathdirectory_node_s* next;

    /// Parent node in the user's logical hierarchy.
    struct pathdirectory_node_s* _parent;

    /// Symbolic node type.
    pathdirectory_nodetype_t _type;

    /// Path directory which owns this node.
    pathdirectory_t* _directory;

    /// User data present at this node.
    pathdirectory_node_userdatapair_t pair;
} pathdirectory_node_t;

static pathdirectory_node_t* PathDirectoryNode_Construct(pathdirectory_t* directory,
    pathdirectory_nodetype_t type, pathdirectory_node_t* parent,
    pathdirectory_internnameid_t nameId, void* userData);

void PathDirectoryNode_Destruct(pathdirectory_node_t* node);

static const pathdirectory_internname_t* getInternNameById(pathdirectory_t* pd,
    pathdirectory_internnameid_t id)
{
    assert(NULL != pd);
    if(0 != id && id <= pd->_internNamesCount)
        return &pd->_internNames[id-1];
    return NULL;
}

static pathdirectory_internnameid_t findInternNameIdForName(pathdirectory_t* pd,
    const ddstring_t* name)
{
    assert(NULL != pd && NULL != name);
    {
    pathdirectory_internnameid_t nameId = 0; // Not a valid id.
    if(!Str_IsEmpty(name) && 0 != pd->_internNamesCount)
    {
        uint bottomIdx = 0, topIdx = pd->_internNamesCount - 1, pivot;
        const pathdirectory_internname_t* intern;
        boolean isDone = false;
        int result;
        while(bottomIdx <= topIdx && !isDone)
        {
            pivot = bottomIdx + (topIdx - bottomIdx)/2;
            intern = getInternNameById(pd, pd->_sortedNameIdMap[pivot]);
            assert(NULL != intern);

            result = Str_CompareIgnoreCase(&intern->name, Str_Text(name));
            if(result == 0)
            {   // Found.
                nameId = pd->_sortedNameIdMap[pivot];
                isDone = true;
            }
            else if(result > 0)
            {
                if(pivot == 0)
                    isDone = true; // Not present.
                else
                    topIdx = pivot - 1;
            }
            else
            {
                bottomIdx = pivot + 1;
            }
        }
    }
    return nameId;
    }
}

static pathdirectory_internnameid_t internName(pathdirectory_t* pd, const ddstring_t* name,
    ushort hash)
{
    assert(NULL != pd && NULL != name);
    {
    pathdirectory_internname_t* intern;
    uint sortedMapIndex = 0;

    if(((uint)-1) == pd->_internNamesCount)
        return 0; // No can do sir, we're out of indices!

    // Add the name to the intern name list.
    pd->_internNames = (pathdirectory_internname_t*) realloc(pd->_internNames,
        sizeof(*pd->_internNames) * ++pd->_internNamesCount);
    if(NULL == pd->_internNames)
        Con_Error("PathDirectory::internName: Failed on (re)allocation of %lu bytes for "
            "intern name list.", (unsigned long) (sizeof(*pd->_internNames) * pd->_internNamesCount));

    intern = &pd->_internNames[pd->_internNamesCount-1];
    Str_Init(&intern->name);
    Str_Copy(&intern->name, name);
    intern->hash = hash;

    // Update the intern name identifier map.
    pd->_sortedNameIdMap = (pathdirectory_internnameid_t*) realloc(pd->_sortedNameIdMap,
        sizeof(*pd->_sortedNameIdMap) * pd->_internNamesCount);
    if(NULL == pd->_sortedNameIdMap)
        Con_Error("PathDirectory::internName: Failed on (re)allocation of %lu bytes for "
            "intern name identifier map.", (unsigned long) (sizeof(*pd->_sortedNameIdMap) * pd->_internNamesCount));

    // Find the insertion point.
    if(1 != pd->_internNamesCount)
    {
        uint i;
        for(i = 0; i < pd->_internNamesCount - 1; ++i)
        {
            const pathdirectory_internname_t* other = getInternNameById(pd, pd->_sortedNameIdMap[i]);
            if(Str_CompareIgnoreCase(&other->name, Str_Text(&intern->name)) > 0)
                break;
        }
        if(i != pd->_internNamesCount - 1)
            memmove(pd->_sortedNameIdMap + i + 1, pd->_sortedNameIdMap + i,
                sizeof(*pd->_sortedNameIdMap) * (pd->_internNamesCount - 1 - i));
        sortedMapIndex = i;
    }
    pd->_sortedNameIdMap[sortedMapIndex] = pd->_internNamesCount; // 1-based index.

    return pd->_internNamesCount; // 1-based index.
    }
}

static void destroyInternNames(pathdirectory_t* pd)
{
    assert(NULL != pd);

    if(0 == pd->_internNamesCount) return;

    { uint i;
    for(i = 0; i < pd->_internNamesCount; ++i)
    {
        pathdirectory_internname_t* intern = &pd->_internNames[i];
        Str_Free(&intern->name);
    }}
    free(pd->_internNames); pd->_internNames = NULL;
    pd->_internNamesCount = 0;

    free(pd->_sortedNameIdMap); pd->_sortedNameIdMap = NULL;
}

/**
 * This is a hash function. It uses the path fragment string to generate
 * a somewhat-random number between @c 0 and @c PATHDIRECTORY_PATHHASH_SIZE
 *
 * @return  The generated hash key.
 */
static ushort hashName(const char* path, size_t len, char delimiter)
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
    return key % PATHDIRECTORY_PATHHASH_SIZE;
    }
}

static void initPathHash(pathdirectory_t* pd)
{
    assert(NULL != pd);
    pd->_pathHash = (pathdirectory_pathhash_t*) calloc(1, sizeof(*pd->_pathHash));
    if(NULL == pd->_pathHash)
        Con_Error("PathDirectory::initPathHash: Failed on allocation of %lu bytes for "
            "new PathDirectory::PathHash.", (unsigned long) sizeof(*pd->_pathHash));
}

static void destroyPathHash(pathdirectory_t* pd)
{
    assert(NULL != pd);
    if(NULL != pd->_pathHash)
    {
        free(pd->_pathHash);
        pd->_pathHash = NULL;
    }
}

static void clearPathHash(pathdirectory_pathhash_t* ph)
{
    ushort i;

    if(NULL == ph) return;

    for(i = 0; i < PATHDIRECTORY_PATHHASH_SIZE; ++i)
    while(NULL != (*ph)[i])
    {
        pathdirectory_node_t* next = (*ph)[i]->next;
        PathDirectoryNode_Destruct((*ph)[i]);
        (*ph)[i] = next;
    }
}

static size_t countNodes(pathdirectory_t* pd, pathdirectory_nodetype_t nodeType)
{
    assert(NULL != pd);
    {
    size_t n = 0;
    if(NULL != pd->_pathHash)
    {
        boolean compareType = VALID_PATHDIRECTORY_NODETYPE(nodeType);
        pathdirectory_node_t* node;
        ushort i;

        for(i = 0; i < PATHDIRECTORY_PATHHASH_SIZE; ++i)
        for(node = (pathdirectory_node_t*) (*pd->_pathHash)[i];
            NULL != node; node = node->next)
        {
            if(!compareType || nodeType == PathDirectoryNode_Type(node))
                ++n;
        }
    }
    return n;
    }
}

static pathdirectory_node_t* findNode(pathdirectory_t* pd, pathdirectory_node_t* parent,
    pathdirectory_nodetype_t nodeType, pathdirectory_internnameid_t nameId)
{
    assert(NULL != pd && 0 != nameId);
    {
    pathdirectory_node_t* node = NULL;
    if(NULL != pd->_pathHash)
    {
        const pathdirectory_internname_t* intern = getInternNameById(pd, nameId);
        assert(NULL != intern);
        for(node = (*pd->_pathHash)[intern->hash]; NULL != node; node = node->next)
        {
            if(parent != PathDirectoryNode_Parent(node)) continue;
            if(nodeType != PathDirectoryNode_Type(node)) continue;

            if(nameId == node->pair.nameId)
                return node;
        }
    }
    return node;
    }
}

/**
 * @return  [ a new | the ] directory node that matches the name and type and
 * which has the specified parent node.
 */
static pathdirectory_node_t* direcNode(pathdirectory_t* pd, pathdirectory_node_t* parent,
    pathdirectory_nodetype_t nodeType, const ddstring_t* name, char delimiter, void* userData)
{
    assert(NULL != pd && NULL != name && !Str_IsEmpty(name));
    {
    const pathdirectory_internname_t* intern;
    pathdirectory_internnameid_t nameId;
    pathdirectory_node_t* node;

    // Have we already encountered this?
    nameId = findInternNameIdForName(pd, name);
    if(0 != nameId)
    {
        // The name is known. Perhaps we have.
        node = findNode(pd, parent, nodeType, nameId);
        if(NULL != node) return node;
    }

    /**
     * A new node is needed.
     */

    // Do we need a new name identifier (and hash)?
    if(0 == nameId)
    {
        nameId = internName(pd, name, hashName(Str_Text(name), Str_Length(name), delimiter));
    }
    // Are we out of name indices?
    if(0 == nameId)
        return NULL;

    node = PathDirectoryNode_Construct(pd, nodeType, parent, nameId, userData);

    // Do we need to init the path hash?
    if(NULL == pd->_pathHash)
        initPathHash(pd);

    // Insert the new node into the path hash.
    intern = getInternNameById(pd, nameId);
    assert(NULL != intern);
    node->next = (*pd->_pathHash)[intern->hash];
    (*pd->_pathHash)[intern->hash] = node;

    return node;
    }
}

/**
 * The path is split into as many nodes as necessary. Parent links are set.
 *
 * @return  The node that identifies the given path.
 */
static pathdirectory_node_t* buildDirecNodes(pathdirectory_t* pd, const char* path,
    char delimiter, void* userData)
{
    assert(NULL != pd && NULL != path);
    {
    pathdirectory_node_t* node = NULL, *parent = NULL;
    ddstring_t part;
    const char* p;

    // Continue splitting as long as there are parts.
    Str_Init(&part);
    p = path;
    while(NULL != (p = Str_CopyDelim2(&part, p, delimiter, CDF_OMIT_DELIMITER))) // Get the next part.
    {
        node = direcNode(pd, parent, PT_BRANCH, &part, delimiter, NULL);
        /// \todo Do not error here. If we're out of storage undo this action and return.
        if(NULL == node)
            Con_Error("PathDirectory::buildDirecNodes: Exhausted storage while attempting to "
                "insert nodes for path \"%s\".", path);
        parent = node;
    }

    if(Str_Length(&part) != 0)
    {
        node = direcNode(pd, parent, PT_LEAF, &part, delimiter, NULL);
        /// \todo Do not error here. If we're out of storage undo this action and return.
        if(NULL == node)
            Con_Error("PathDirectory::buildDirecNodes: Exhausted storage while attempting to "
                "insert nodes for path \"%s\".", path);
    }

    Str_Free(&part);
    return node;
    }
}

static pathdirectory_node_t* addPath(pathdirectory_t* fd, const char* path,
    char delimiter, void* userData)
{
    assert(NULL != fd && NULL != path);
    {
    pathdirectory_node_t* node = buildDirecNodes(fd, path, delimiter, NULL);
    if(NULL != node && NULL != userData)
        PathDirectoryNode_AttachUserData(node, userData);
    return node;
    }
}

static int iteratePaths(pathdirectory_t* pd, pathdirectory_nodetype_t nodeType,
    pathdirectory_node_t* parent, int (*callback) (pathdirectory_node_t* node, void* paramaters),
    void* paramaters)
{
    assert(NULL != pd && NULL != callback);
    {
    int result = 0;
    if(NULL != pd->_pathHash)
    {
        boolean compareType = VALID_PATHDIRECTORY_NODETYPE(nodeType);
        pathdirectory_node_t* node;
        ushort i;

        for(i = 0; i < PATHDIRECTORY_PATHHASH_SIZE; ++i)
        for(node = (*pd->_pathHash)[i]; NULL != node; node = node->next)
        {
            if(compareType && nodeType != PathDirectoryNode_Type(node)) continue;
            if(parent != PathDirectoryNode_Parent(node)) continue;

            if(0 != (result = callback(node, paramaters)))
                break;
        }
    }
    return result;
    }
}

static int iteratePaths_const(const pathdirectory_t* pd, pathdirectory_nodetype_t nodeType,
    const pathdirectory_node_t* parent,
    int (*callback) (const pathdirectory_node_t* node, void* paramaters), void* paramaters)
{
    assert(NULL != pd && NULL != callback);
    {
    int result = 0;
    if(NULL != pd->_pathHash)
    {
        boolean compareType = VALID_PATHDIRECTORY_NODETYPE(nodeType);
        pathdirectory_node_t* node;
        ushort i;

        for(i = 0; i < PATHDIRECTORY_PATHHASH_SIZE; ++i)
        for(node = (*pd->_pathHash)[i]; NULL != node; node = node->next)
        {
            if(compareType && nodeType != PathDirectoryNode_Type(node)) continue;
            if(parent != PathDirectoryNode_Parent(node)) continue;

            if(0 != (result = callback(node, paramaters)))
                break;
        }
    }
    return result;
    }
}

pathdirectory_t* PathDirectory_Construct(void)
{
    pathdirectory_t* pd = (pathdirectory_t*) malloc(sizeof(*pd));
    if(NULL == pd)
        Con_Error("PathDirectory::Construct: Failed on allocation of %lu bytes for "
            "new PathDirectory.", (unsigned long) sizeof(*pd));
    pd->_internNames = NULL;
    pd->_internNamesCount = 0;
    pd->_sortedNameIdMap = NULL;
    pd->_pathHash = NULL;
    return pd;
}

void PathDirectory_Destruct(pathdirectory_t* pd)
{
    assert(NULL != pd);
    clearPathHash(pd->_pathHash);
    destroyPathHash(pd);
    destroyInternNames(pd);
    free(pd);
}

void PathDirectory_Clear(pathdirectory_t* pd)
{
    assert(NULL != pd);
    clearPathHash(pd->_pathHash);
}

pathdirectory_node_t* PathDirectory_Insert2(pathdirectory_t* pd, const char* path, char delimiter,
    void* value)
{
    assert(NULL != pd);
    if(NULL == path || !path[0])
        return NULL;
    return addPath(pd, path, delimiter, value);
}

pathdirectory_node_t* PathDirectory_Insert(pathdirectory_t* pd, const char* path, char delimiter)
{
    return PathDirectory_Insert2(pd, path, delimiter, NULL);
}

int PathDirectory_Iterate2(pathdirectory_t* pd, pathdirectory_nodetype_t nodeType,
    pathdirectory_node_t* parent, int (*callback) (pathdirectory_node_t* node, void* paramaters),
    void* paramaters)
{
    assert(NULL != pd && (nodeType == PT_ANY || VALID_PATHDIRECTORY_NODETYPE(nodeType)));
    return iteratePaths(pd, nodeType, parent, callback, paramaters);
}

int PathDirectory_Iterate(pathdirectory_t* pd, pathdirectory_nodetype_t nodeType,
    pathdirectory_node_t* parent, int (*callback) (pathdirectory_node_t* node, void* paramaters))
{
    return PathDirectory_Iterate2(pd, nodeType, parent, callback, NULL);
}

int PathDirectory_Iterate2_Const(const pathdirectory_t* pd, pathdirectory_nodetype_t nodeType,
    const pathdirectory_node_t* parent,
    int (*callback) (const pathdirectory_node_t* node, void* paramaters),
    void* paramaters)
{
    assert(NULL != pd && (nodeType == PT_ANY || VALID_PATHDIRECTORY_NODETYPE(nodeType)));
    return iteratePaths_const(pd, nodeType, parent, callback, paramaters);
}

int PathDirectory_Iterate_Const(const pathdirectory_t* pd, pathdirectory_nodetype_t nodeType,
    const pathdirectory_node_t* parent,
    int (*callback) (const pathdirectory_node_t* node, void* paramaters))
{
    return PathDirectory_Iterate2_Const(pd, nodeType, parent, callback, NULL);
}

pathdirectory_node_t* PathDirectory_Find(pathdirectory_t* pd, const char* searchPath, char delimiter)
{
    assert(NULL != pd);
    if(NULL != searchPath && searchPath[0] && NULL != pd->_pathHash)
    {
        size_t searchPathLen = strlen(searchPath);
        ushort hash = hashName(searchPath, searchPathLen, delimiter);
        pathdirectory_node_t* node = (*pd->_pathHash)[hash];
        // Perform the search.
        while(NULL != node && !PathDirectoryNode_MatchDirectory(node, searchPath, searchPathLen, delimiter))
            node = node->next;
        return node;
    }
    return NULL;
}

ddstring_t* PathDirectory_AllPaths(pathdirectory_t* pd, pathdirectory_nodetype_t nodeType,
    char delimiter, size_t* retCount)
{
    assert(NULL != pd);
    {
    ddstring_t* paths = NULL;
    size_t count = countNodes(pd, nodeType);
    if(0 != count)
    {
        boolean compareType = VALID_PATHDIRECTORY_NODETYPE(nodeType);
        pathdirectory_node_t* node;
        ddstring_t** pathPtr;
        ushort i;

        paths = (ddstring_t*) malloc(sizeof(*paths) * (count + 1));
        if(NULL == paths)
            Con_Error("PathDirectory::AllPaths: Failed on allocation of %lu bytes for "
                "new path list.", (unsigned long) sizeof(*paths));

        pathPtr = &paths;
        for(i = 0; i < PATHDIRECTORY_PATHHASH_SIZE; ++i)
        for(node = (*pd->_pathHash)[i]; NULL != node; node = node->next)
        {
            if(compareType && nodeType != PathDirectoryNode_Type(node)) continue;

            Str_Init(*pathPtr);
            PathDirectoryNode_ComposePath(node, *pathPtr, delimiter);
            pathPtr++;
        }
        *pathPtr = NULL;
    }
    if(retCount)
        *retCount = count;
    return paths;
    }
}

#if _DEBUG
static int C_DECL comparePaths(const void* a, const void* b)
{
    return stricmp(Str_Text((ddstring_t*)a), Str_Text((ddstring_t*)b));
}

void PathDirectory_Print(pathdirectory_t* pd, char delimiter)
{
    assert(NULL != pd);
    {
    size_t numLeafs, n = 0;
    ddstring_t* pathList;

    Con_Printf("PathDirectory:\n");
    if(NULL != (pathList = PathDirectory_AllPaths(pd, PT_LEAF, delimiter, &numLeafs)))
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

static pathdirectory_node_t* PathDirectoryNode_Construct(pathdirectory_t* directory,
    pathdirectory_nodetype_t type, pathdirectory_node_t* parent,
    pathdirectory_internnameid_t nameId, void* userData)
{
    pathdirectory_node_t* node = (pathdirectory_node_t*) malloc(sizeof(*node));
    if(NULL == node)
        Con_Error("PathDirectory::direcNode: Failed on allocation of %lu bytes for "
            "new PathDirectory::Node.", (unsigned long) sizeof(*node));

    node->_directory = directory;
    node->_type = type;
    node->_parent = parent;
    node->pair.nameId = nameId;
    node->pair.data = userData;
    return node;
}

static void PathDirectoryNode_Destruct(pathdirectory_node_t* node)
{
    assert(NULL != node);
    free(node);
}

boolean PathDirectoryNode_MatchDirectory(const pathdirectory_node_t* node,
    const char* searchPath, size_t searchPathLen, char delimiter)
{
    assert(NULL != node);
    {
    const pathdirectory_internname_t* intern;
    const char* begin, *from, *to;
    size_t i;

    if(NULL == searchPath || 0 == searchPathLen)
        return false;

    begin = searchPath;
    to = begin + searchPathLen - 1;

    // Skip over any trailing delimiters.
    for(i = searchPathLen; *to && *to == delimiter && i-- > 0; to--) {}

    // In reverse order, compare path fragments in the search term.
    for(;;)
    {
        // Find the start of the next path fragment.
        for(from = to; from > begin && !(*from == delimiter); from--) {}

        // Does this match?
        intern = getInternNameById(node->_directory, node->pair.nameId);
        assert(NULL != intern);

        if(Str_Length(&intern->name) < (to - from) + (from == begin && *from != delimiter? 1 : 0))
            return false;
        if(strnicmp(Str_Text(&intern->name), *from == delimiter? from + 1 : from, Str_Length(&intern->name)))
            return false;

        // Have we arrived at the search target?
        if(from == begin)
            return true;

        // Are there no more parent directories?
        if(NULL == node->_parent)
            return false;

        // So far so good. Move one directory level upwards.
        node = node->_parent;

        // The string now ends here.
        to = from-1;
    }
    // Unreachable.
    return false;
    }
}

void PathDirectoryNode_ComposePath(const pathdirectory_node_t* node, ddstring_t* foundPath,
    char delimiter)
{
    assert(NULL != node && NULL != foundPath);
    {
    const int delimiterLen = delimiter? 1 : 0;
    const pathdirectory_node_t* trav;
    const pathdirectory_internname_t* intern;
    int requiredLen = 0;

    // Calculate the total length of the final composed path.
    if(PT_BRANCH == node->_type)
        requiredLen += delimiterLen;
    trav = node;
    do
    {
        intern = getInternNameById(trav->_directory, trav->pair.nameId);
        assert(NULL != intern);

        requiredLen += Str_Length(&intern->name);
        if(NULL != trav->_parent && delimiterLen != 0)
            requiredLen += delimiterLen;
    } while(NULL != (trav = trav->_parent));

    // Compose the path.
    Str_Clear(foundPath);
    Str_Reserve(foundPath, requiredLen);
    if(PT_BRANCH == node->_type)
        Str_PrependChar(foundPath, delimiter);
    trav = node;
    do
    {
        intern = getInternNameById(trav->_directory, trav->pair.nameId);
        assert(NULL != intern);

        Str_Prepend(foundPath, Str_Text(&intern->name));
        if(NULL != trav->_parent && delimiterLen != 0)
            Str_PrependChar(foundPath, delimiter);
    } while(NULL != (trav = trav->_parent));
    }
}

struct pathdirectory_node_s* PathDirectoryNode_Parent(const struct pathdirectory_node_s* node)
{
    assert(NULL != node);
    return node->_parent;
}

pathdirectory_nodetype_t PathDirectoryNode_Type(const pathdirectory_node_t* node)
{
    assert(NULL != node);
    return node->_type;
}

void PathDirectoryNode_AttachUserData(pathdirectory_node_t* node, void* data)
{
    assert(NULL != node);
#if _DEBUG
    if(NULL != node->pair.data)
    {
        Con_Message("Warning:PathDirectoryNode::AttachUserData: Data is already associated "
            "with this node, will be replaced.");
    }
#endif
    node->pair.data = data;
}

void* PathDirectoryNode_DetachUserData(pathdirectory_node_t* node)
{
    assert(NULL != node);
    {
    void* data = node->pair.data;
    node->pair.data = NULL;
    return data;
    }
}

void* PathDirectoryNode_UserData(const pathdirectory_node_t* node)
{
    assert(NULL != node);
    return node->pair.data;
}
