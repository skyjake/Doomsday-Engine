/**\file pathdirectory.c
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

#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"

#include "stringpool.h"
#include "m_misc.h"

#include "pathdirectory.h"

typedef struct pathdirectorynode_userdatapair_s {
    StringPoolInternId internId;
    void* data;
} pathdirectorynode_userdatapair_t;

struct pathdirectorynode_s {
    /// Next node in the hashed path bucket.
    PathDirectoryNode* next;

    /// Parent node in the user's logical hierarchy.
    PathDirectoryNode* _parent;

    /// Symbolic node type.
    pathdirectorynode_type_t _type;

    /// PathDirectory which owns this node.
    PathDirectory* _directory;

    /// User data present at this node.
    pathdirectorynode_userdatapair_t _pair;
};

static PathDirectoryNode* PathDirectoryNode_New(PathDirectory* directory,
    pathdirectorynode_type_t type, PathDirectoryNode* parent,
    StringPoolInternId internId, void* userData);

static void PathDirectoryNode_Delete(PathDirectoryNode* node);

/// @return  Intern id for the string fragment owned by the PathDirectory of which this node is a child of.
StringPoolInternId PathDirectoryNode_InternId(const PathDirectoryNode* node);

typedef struct {
    PathDirectoryNode* head;
} pathdirectory_nodelist_t;
typedef pathdirectory_nodelist_t pathdirectory_pathhash_t[PATHDIRECTORY_PATHHASH_SIZE];

struct pathdirectory_s {
    /// Path name fragment intern pool.
    struct pathdirectory_internpool_s {
        StringPool* strings;
        ushort* idHashMap; // Index by @c StringPoolInternId-1
    } _internPool;

    /// Path hash map.
    pathdirectory_pathhash_t* _pathLeafHash;
    pathdirectory_pathhash_t* _pathBranchHash;

    /// Number of unique paths in the directory.
    uint _size;
};

ushort PathDirectory_HashName(const char* path, size_t len, char delimiter)
{
    const char* c = path + len - 1;
    ushort key = 0;
    int op = 0;
    size_t i;

    assert(path);

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

static __inline pathdirectory_pathhash_t** hashAddressForNodeType(PathDirectory* pd,
    pathdirectorynode_type_t type)
{
    assert(pd && VALID_PATHDIRECTORYNODE_TYPE(type));
    return (type == PT_LEAF? &pd->_pathLeafHash : &pd->_pathBranchHash);
}

static __inline const pathdirectory_pathhash_t** hashAddressForNodeType_Const(const PathDirectory* pd,
    pathdirectorynode_type_t type)
{
    return (const pathdirectory_pathhash_t**) hashAddressForNodeType((PathDirectory*)pd, type);
}

static void initPathHash(PathDirectory* pd, pathdirectorynode_type_t type)
{
    pathdirectory_pathhash_t** hashAdr = hashAddressForNodeType(pd, type);
    *hashAdr = (pathdirectory_pathhash_t*) calloc(1, sizeof **hashAdr);
    if(!*hashAdr)
        Con_Error("PathDirectory::initPathHash: Failed on allocation of %lu bytes for new PathDirectory::PathHash.", (unsigned long) sizeof **hashAdr);
}

static void destroyPathHash(PathDirectory* pd, pathdirectorynode_type_t type)
{
    pathdirectory_pathhash_t** hashAdr = hashAddressForNodeType(pd, type);
    if(!*hashAdr) return;
    free(*hashAdr), *hashAdr = NULL;
}

static void clearInternPool(PathDirectory* pd)
{
    assert(pd);
    if(pd->_internPool.strings)
    {
        StringPool_Delete(pd->_internPool.strings), pd->_internPool.strings = NULL;
        free(pd->_internPool.idHashMap), pd->_internPool.idHashMap = NULL;
    }
}

static void clearNodeList(PathDirectoryNode** list)
{
    PathDirectoryNode* next;
    if(!*list) return;

    do
    {
        next = (*list)->next;
#if _DEBUG
        if(PathDirectoryNode_UserData(*list))
        {
            Con_Error("PathDirectory::clearNodeList: Node %p has non-NULL user data.", *list);
            exit(1); // Unreachable.
        }
#endif
        PathDirectoryNode_Delete(*list);
    } while(NULL != (*list = next));
}

static void clearPathHash(pathdirectory_pathhash_t* ph)
{
    ushort hash = 0;
    if(!ph) return;
    for(; hash < PATHDIRECTORY_PATHHASH_SIZE; ++hash)
        clearNodeList(&(*ph)[hash].head);
}

static size_t countNodesInPathHash(pathdirectory_pathhash_t* ph)
{
    size_t count = 0;
    if(ph)
    {
        PathDirectoryNode* node;
        ushort hash = 0;
        for(; hash < PATHDIRECTORY_PATHHASH_SIZE; ++hash)
        for(node = (PathDirectoryNode*) (*ph)[hash].head; node; node = node->next)
            ++count;
    }
    return count;
}

static size_t countNodes(PathDirectory* pd, int flags)
{
    size_t count = 0;
    assert(pd);
    if(!(flags & PCF_NO_LEAF))
        count += countNodesInPathHash(*hashAddressForNodeType(pd, PT_LEAF));
    if(!(flags & PCF_NO_BRANCH))
        count += countNodesInPathHash(*hashAddressForNodeType(pd, PT_BRANCH));
    return count;
}

static PathDirectoryNode* findNode(PathDirectory* pd, PathDirectoryNode* parent,
    pathdirectorynode_type_t nodeType, StringPoolInternId internId)
{
    pathdirectory_pathhash_t* ph = *hashAddressForNodeType(pd, nodeType);
    PathDirectoryNode* node = NULL;
    if(ph)
    {
        ushort hash = pd->_internPool.idHashMap[internId-1];
        for(node = (*ph)[hash].head; node; node = node->next)
        {
            if(parent != PathDirectoryNode_Parent(node)) continue;
            if(internId == PathDirectoryNode_InternId(node))
                return node;
        }
    }
    return node;
}

static ushort hashForInternId(PathDirectory* pd, StringPoolInternId internId)
{
    assert(pd);
    if(0 == internId)
        Con_Error("PathDirectory::hashForInternId: Invalid internId %u.", internId);
    return pd->_internPool.idHashMap[internId-1];
}

static StringPoolInternId internNameAndUpdateIdHashMap(PathDirectory* pd,
    const ddstring_t* name, ushort hash)
{
    StringPool* pool;
    StringPoolInternId internId;
    uint oldSize;
    assert(pd);

    pool = pd->_internPool.strings;
    if(!pool)
    {
        pool = pd->_internPool.strings = StringPool_New();
    }
    oldSize = StringPool_Size(pool);

    internId = StringPool_Intern(pool, name);
    if(oldSize != StringPool_Size(pool))
    {
        // A new string was added to the pool.
        pd->_internPool.idHashMap = (ushort*) realloc(pd->_internPool.idHashMap, sizeof *pd->_internPool.idHashMap * StringPool_Size(pool));
        if(!pd->_internPool.idHashMap)
            Con_Error("PathDirectory::internNameAndUpdateIdHashMap: Failed on (re)allocation of %lu bytes for the IdHashMap", (unsigned long) (sizeof *pd->_internPool.idHashMap * StringPool_Size(pool)));

        if(internId < StringPool_Size(pool))
        {
            memmove(pd->_internPool.idHashMap + internId, pd->_internPool.idHashMap + (internId-1), sizeof *pd->_internPool.idHashMap * (StringPool_Size(pool) - internId));
        }
        pd->_internPool.idHashMap[internId-1] = hash;
    }
    return internId;
}

/**
 * @return  [ a new | the ] directory node that matches the name and type and
 * which has the specified parent node.
 */
static PathDirectoryNode* direcNode(PathDirectory* pd, PathDirectoryNode* parent,
    pathdirectorynode_type_t nodeType, const ddstring_t* name, char delimiter, void* userData)
{
    StringPoolInternId internId = 0;
    pathdirectory_pathhash_t** phAdr;
    PathDirectoryNode* node;
    ushort hash;
    assert(pd && name);

    // Have we already encountered this?
    if(pd->_internPool.strings)
    {
        internId = StringPool_IsInterned(pd->_internPool.strings, name);
        if(0 != internId)
        {
            // The name is known. Perhaps we have.
            node = findNode(pd, parent, nodeType, internId);
            if(node) return node;
        }
    }

    /**
     * A new node is needed.
     */

    // Do we need a new name identifier (and hash)?
    if(!internId)
    {
        hash = PathDirectory_HashName(Str_Text(name), Str_Length(name), delimiter);
        internId = internNameAndUpdateIdHashMap(pd, name, hash);
    }
    else
    {
        hash = hashForInternId(pd, internId);
    }

    // Are we out of name indices?
    if(!internId) return NULL;

    node = PathDirectoryNode_New(pd, nodeType, parent, internId, userData);

    // Do we need to init the path hash?
    phAdr = hashAddressForNodeType(pd, nodeType);
    if(!*phAdr)
    {
        initPathHash(pd, nodeType);
    }

    // Insert the new node into the path hash.
    node->next = (**phAdr)[hash].head;
    (**phAdr)[hash].head = node;

    return node;
}

/**
 * The path is split into as many nodes as necessary. Parent links are set.
 *
 * @return  The node that identifies the given path.
 */
static PathDirectoryNode* buildDirecNodes(PathDirectory* pd, const char* path,
    char delimiter, void* userData)
{
    PathDirectoryNode* node = NULL, *parent = NULL;
    ddstring_t part;
    const char* p;
    assert(pd && path);

    // Continue splitting as long as there are parts.
    Str_Init(&part);
    p = path;
    while((p = Str_CopyDelim2(&part, p, delimiter, CDF_OMIT_DELIMITER))) // Get the next part.
    {
        node = direcNode(pd, parent, PT_BRANCH, &part, delimiter, NULL);
        /// \todo Do not error here. If we're out of storage undo this action and return.
        if(!node)
            Con_Error("PathDirectory::buildDirecNodes: Exhausted storage while attempting to insert nodes for path \"%s\".", path);
        parent = node;
    }

    if(Str_Length(&part) != 0)
    {
        node = direcNode(pd, parent, PT_LEAF, &part, delimiter, NULL);
        /// \todo Do not error here. If we're out of storage undo this action and return.
        if(!node)
            Con_Error("PathDirectory::buildDirecNodes: Exhausted storage while attempting to insert nodes for path \"%s\".", path);
    }

    Str_Free(&part);
    return node;
}

static PathDirectoryNode* addPath(PathDirectory* pd, const char* path,
    char delimiter, void* userData)
{
    PathDirectoryNode* node = buildDirecNodes(pd, path, delimiter, NULL);
    if(node)
    {
        // There is now one more unique path in the directory.
        ++pd->_size;
        if(userData)
            PathDirectoryNode_AttachUserData(node, userData);
    }
    return node;
}

static int iteratePathsInHash(pathdirectory_pathhash_t* ph, int flags, PathDirectoryNode* parent,
    ushort hash, int (*callback) (PathDirectoryNode* node, void* paramaters),
    void* paramaters)
{
    PathDirectoryNode* node, *next;
    int result = 0;

    if(ph)
    if(hash != PATHDIRECTORY_NOHASH)
    {
        if(hash >= PATHDIRECTORY_PATHHASH_SIZE)
            Con_Error("PathDirectory:iteratePathsInHash: Invalid hash %u (valid range is [0...%u]).",
                      hash, PATHDIRECTORY_PATHHASH_SIZE-1);

        node = (PathDirectoryNode*)(*ph)[hash].head;
        while(node)
        {
            next = node->next;
            if(!((flags & PCF_MATCH_PARENT) && parent == PathDirectoryNode_Parent(node)))
            {
                result = callback(node, paramaters);
                if(result) break;
            }
            node = next;
        }
    }
    else
    {
        for(hash = 0; hash < PATHDIRECTORY_PATHHASH_SIZE; ++hash)
        {
            node = (PathDirectoryNode*) (*ph)[hash].head;
            while(node)
            {
                next = node->next;
                if(!((flags & PCF_MATCH_PARENT) && parent == PathDirectoryNode_Parent(node)))
                {
                    result = callback(node, paramaters);
                    if(result) break;
                }
                node = next;
            }
            if(result) break;
        }
    }
    return result;
}

static int iteratePaths(PathDirectory* pd, int flags, PathDirectoryNode* parent,
    ushort hash, int (*callback) (PathDirectoryNode* node, void* paramaters),
    void* paramaters)
{
    int result = 0;
    assert(pd && callback);

    if(!(flags & PCF_NO_LEAF))
        result = iteratePathsInHash(*hashAddressForNodeType(pd, PT_LEAF), flags, parent, hash, callback, paramaters);

    if(!result && !(flags & PCF_NO_BRANCH))
        result = iteratePathsInHash(*hashAddressForNodeType(pd, PT_BRANCH), flags, parent, hash, callback, paramaters);

    return result;
}

static int iteratePathsInHash_Const(const pathdirectory_pathhash_t* ph, int flags,
    const PathDirectoryNode* parent, ushort hash,
    int (*callback) (const PathDirectoryNode* node, void* paramaters), void* paramaters)
{
    PathDirectoryNode* node, *next;
    int result = 0;

    if(ph)
    if(hash != PATHDIRECTORY_NOHASH)
    {
        if(hash >= PATHDIRECTORY_PATHHASH_SIZE)
            Con_Error("PathDirectory:iteratePathsInHash: Invalid hash %u (valid range is [0...%u]).",
                      hash, PATHDIRECTORY_PATHHASH_SIZE-1);

        node = (PathDirectoryNode*)(*ph)[hash].head;
        while(node)
        {
            next = node->next;
            if(!((flags & PCF_MATCH_PARENT) && parent == PathDirectoryNode_Parent(node)))
            {
                result = callback(node, paramaters);
                if(result) break;
            }
            node = next;
        }
    }
    else
    {
        for(hash = 0; hash < PATHDIRECTORY_PATHHASH_SIZE; ++hash)
        {
            node = (PathDirectoryNode*)(*ph)[hash].head;
            while(node)
            {
                next = node->next;
                if(!((flags & PCF_MATCH_PARENT) && parent == PathDirectoryNode_Parent(node)))
                {
                    result = callback(node, paramaters);
                    if(result) break;
                }
                node = next;
            }
            if(result) break;
        }
    }
    return result;
}

static int iteratePaths_Const(const PathDirectory* pd, int flags, const PathDirectoryNode* parent,
    ushort hash, int (*callback) (const PathDirectoryNode* node, void* paramaters),
    void* paramaters)
{
    int result = 0;
    assert(pd && callback);

    if(!(flags & PCF_NO_LEAF))
        result = iteratePathsInHash_Const(*hashAddressForNodeType_Const(pd, PT_LEAF),
                                          flags, parent, hash, callback, paramaters);

    if(!result && !(flags & PCF_NO_BRANCH))
        result = iteratePathsInHash_Const(*hashAddressForNodeType_Const(pd, PT_BRANCH),
                                          flags, parent, hash, callback, paramaters);

    return result;
}

PathDirectory* PathDirectory_New(void)
{
    PathDirectory* pd = (PathDirectory*) malloc(sizeof *pd);
    if(!pd)
        Con_Error("PathDirectory::Construct: Failed on allocation of %lu bytes for new PathDirectory.", (unsigned long) sizeof *pd);

    pd->_internPool.strings = NULL;
    pd->_internPool.idHashMap = NULL;
    pd->_pathLeafHash = NULL;
    pd->_pathBranchHash = NULL;
    pd->_size = 0;
    return pd;
}

void PathDirectory_Delete(PathDirectory* pd)
{
    if(!pd) return;

    clearPathHash(*hashAddressForNodeType(pd, PT_LEAF));
    destroyPathHash(pd, PT_LEAF);

    clearPathHash(*hashAddressForNodeType(pd, PT_BRANCH));
    destroyPathHash(pd, PT_BRANCH);

    clearInternPool(pd);
    free(pd);
}

uint PathDirectory_Size(PathDirectory* pd)
{
    assert(pd);
    return pd->_size;
}

void PathDirectory_Clear(PathDirectory* pd)
{
    assert(pd);
    clearPathHash(*hashAddressForNodeType(pd, PT_LEAF));
    clearPathHash(*hashAddressForNodeType(pd, PT_BRANCH));
    clearInternPool(pd);
    pd->_size = 0;
}

PathDirectoryNode* PathDirectory_Insert2(PathDirectory* pd, const char* path, char delimiter,
    void* value)
{
    if(!path || !path[0]) return NULL;
    return addPath(pd, path, delimiter, value);
}

PathDirectoryNode* PathDirectory_Insert(PathDirectory* pd, const char* path, char delimiter)
{
    return PathDirectory_Insert2(pd, path, delimiter, NULL);
}

int PathDirectory_Iterate2(PathDirectory* pd, int flags, PathDirectoryNode* parent,
    ushort hash, pathdirectory_iteratecallback_t callback, void* paramaters)
{
    if(!pd) return false;
    return iteratePaths(pd, flags, parent, hash, callback, paramaters);
}

int PathDirectory_Iterate(PathDirectory* pd, int flags, PathDirectoryNode* parent,
    ushort hash, pathdirectory_iteratecallback_t callback)
{
    return PathDirectory_Iterate2(pd, flags, parent, hash, callback, NULL);
}

int PathDirectory_Iterate2_Const(const PathDirectory* pd, int flags, const PathDirectoryNode* parent,
    ushort hash, pathdirectory_iterateconstcallback_t callback, void* paramaters)
{
    return iteratePaths_Const(pd, flags, parent, hash, callback, paramaters);
}

int PathDirectory_Iterate_Const(const PathDirectory* pd, int flags, const PathDirectoryNode* parent,
    ushort hash, pathdirectory_iterateconstcallback_t callback)
{
    return PathDirectory_Iterate2_Const(pd, flags, parent, hash, callback, NULL);
}

typedef struct {
    int flags; /// @see pathComparisonFlags
    PathMap* mappedSearchPath;
    void* paramaters;
    pathdirectory_searchcallback_t callback;
    PathDirectoryNode* foundNode;
} pathdirectorysearchworker_params_t;

static int PathDirectory_SearchWorker(PathDirectoryNode* node, void* paramaters)
{
    pathdirectorysearchworker_params_t* p = (pathdirectorysearchworker_params_t*)paramaters;
    assert(node && paramaters);
    if(p->callback(node, p->flags, p->mappedSearchPath, p->paramaters))
    {
        p->foundNode = node;
        return 1; // Stop iteration.
    }
    return 0; // Continue iteration.
}

PathDirectoryNode* PathDirectory_Search2(PathDirectory* pd, int flags,
    PathMap* mappedSearchPath, pathdirectory_searchcallback_t callback, void* paramaters)
{
    pathdirectorysearchworker_params_t p;
    int result;
    p.flags = flags;
    p.mappedSearchPath = mappedSearchPath;
    p.paramaters = paramaters;
    p.callback = callback;
    p.foundNode = NULL;
    result = iteratePaths(pd, flags, NULL, PathMap_Fragment(mappedSearchPath, 0)->hash,
                 PathDirectory_SearchWorker, (void*)&p);
    return (result? p.foundNode : NULL);
}

PathDirectoryNode* PathDirectory_Search(PathDirectory* pd, int flags,
    PathMap* mappedSearchPath, pathdirectory_searchcallback_t callback)
{
    return PathDirectory_Search2(pd, flags, mappedSearchPath, callback, NULL);
}

PathDirectoryNode* PathDirectory_Find(PathDirectory* pd, int flags,
    const char* searchPath, char delimiter)
{
    PathDirectoryNode* foundNode = NULL;
    if(searchPath && searchPath[0] && PathDirectory_Size(pd))
    {
        PathMap mappedSearchPath;
        PathMap_Initialize(&mappedSearchPath, searchPath, delimiter);
        foundNode = PathDirectory_Search(pd, flags, &mappedSearchPath, PathDirectoryNode_MatchDirectory);
        PathMap_Destroy(&mappedSearchPath);
    }
    return foundNode;
}

const ddstring_t* PathDirectory_GetFragment(PathDirectory* pd, const PathDirectoryNode* node)
{
    assert(pd);
    return StringPool_String(pd->_internPool.strings, PathDirectoryNode_InternId(node));
}

/// Calculate the total length of the final composed path.
static int PathDirectory_CalcPathLength(PathDirectory* pd, const PathDirectoryNode* node, char delimiter)
{
    const int delimiterLen = delimiter? 1 : 0;
    int requiredLen = 0;
    assert(pd && node);

    if(PT_BRANCH == PathDirectoryNode_Type(node))
        requiredLen += delimiterLen;

    requiredLen += Str_Length(PathDirectory_GetFragment(pd, node));
    if(PathDirectoryNode_Parent(node))
    {
        const PathDirectoryNode* trav = PathDirectoryNode_Parent(node);
        requiredLen += delimiterLen;
        do
        {
            requiredLen += Str_Length(PathDirectory_GetFragment(pd, trav));
            if(PathDirectoryNode_Parent(trav))
                requiredLen += delimiterLen;
        } while((trav = PathDirectoryNode_Parent(trav)));
    }

    return requiredLen;
}

/// \assume @a foundPath already has sufficent characters reserved to hold the fully composed path.
static ddstring_t* PathDirectory_ConstructPath(PathDirectory* pd, const PathDirectoryNode* node,
    ddstring_t* foundPath, char delimiter)
{
    const int delimiterLen = delimiter? 1 : 0;
    const PathDirectoryNode* trav;
    const ddstring_t* fragment;
    assert(pd && node && foundPath);

    if(PT_BRANCH == PathDirectoryNode_Type(node) && 0 != delimiterLen)
        Str_AppendChar(foundPath, delimiter);

    trav = node;
    do
    {
        fragment = PathDirectory_GetFragment(pd, trav);
        Str_Prepend(foundPath, Str_Text(fragment));
        if(NULL != PathDirectoryNode_Parent(trav) && 0 != delimiterLen)
            Str_PrependChar(foundPath, delimiter);
    } while((trav = PathDirectoryNode_Parent(trav)));
    return foundPath;
}

ddstring_t* PathDirectory_ComposePath(PathDirectory* pd, const PathDirectoryNode* node,
    ddstring_t* foundPath, int* length, char delimiter)
{
    assert(pd && node);
    if(!foundPath && length)
    {
        *length = PathDirectory_CalcPathLength(pd, node, delimiter);
        return foundPath;
    }
    else
    {
        int fullLength;

        PathDirectory_ComposePath(pd, node, NULL, &fullLength, delimiter);
        if(length) *length = fullLength;

        if(!foundPath) return foundPath;

        Str_Clear(foundPath);
        Str_Reserve(foundPath, fullLength);
        return PathDirectory_ConstructPath(pd, node, foundPath, delimiter);
    }
}

static void PathDirectory_CollectPathsInHash(pathdirectory_pathhash_t* ph, char delimiter,
    ddstring_t** pathListAdr)
{
    PathDirectoryNode* node;
    ushort hash;

    if(ph)
    for(hash = 0; hash < PATHDIRECTORY_PATHHASH_SIZE; ++hash)
    for(node = (PathDirectoryNode*) (*ph)[hash].head; node; node = node->next)
    {
        Str_Init(*pathListAdr);
        PathDirectory_ComposePath(PathDirectoryNode_Directory(node), node, (*pathListAdr), NULL, delimiter);
        (*pathListAdr)++;
    }
}

ddstring_t* PathDirectory_CollectPaths(PathDirectory* pd, int flags, char delimiter,
    size_t* retCount)
{
    ddstring_t* paths = NULL;
    size_t count = countNodes(pd, flags);

    if(count)
    {
        ddstring_t* pathPtr;

        paths = (ddstring_t*) malloc(sizeof *paths * count);
        if(!paths)
            Con_Error("PathDirectory::AllPaths: Failed on allocation of %lu bytes for "
                "new path list.", (unsigned long) (sizeof *paths * count));
        pathPtr = paths;

        if(!(flags & PCF_NO_BRANCH))
            PathDirectory_CollectPathsInHash(*hashAddressForNodeType(pd, PT_BRANCH), delimiter, &pathPtr);

        if(!(flags & PCF_NO_LEAF))
            PathDirectory_CollectPathsInHash(*hashAddressForNodeType(pd, PT_LEAF), delimiter, &pathPtr);
    }

    if(retCount) *retCount = count;
    return paths;
}

static int C_DECL comparePaths(const void* a, const void* b)
{
    return stricmp(Str_Text((ddstring_t*)a), Str_Text((ddstring_t*)b));
}

#if _DEBUG
void PathDirectory_Print(PathDirectory* pd, char delimiter)
{
    size_t numLeafs, n = 0;
    ddstring_t* pathList;
    assert(pd);

    Con_Printf("PathDirectory [%p]:\n", (void*)pd);
    pathList = PathDirectory_CollectPaths(pd, PT_LEAF, delimiter, &numLeafs);
    if(pathList)
    {
        qsort(pathList, numLeafs, sizeof *pathList, comparePaths);
        do
        {
            Con_Printf("  %s\n", F_PrettyPath(Str_Text(pathList + n)));
            Str_Free(pathList + n);
        } while(++n < numLeafs);
        free(pathList);
    }
    Con_Printf("  %lu %s in directory.\n", (unsigned long)numLeafs, (numLeafs==1? "path":"paths"));
}

static void printDistributionOverviewElement(const int* colWidths, const char* name,
    size_t numEmpty, size_t maxHeight, size_t numCollisions, size_t maxCollisions,
    size_t sum, size_t total)
{
    float coverage, collision, variance;
    const int* col;

    if(0 != total)
    {
        size_t sumSqr = sum*sum;
        float mean = (signed)sum / total;
        variance = ((signed)sumSqr - (signed)sum * mean) / (((signed)total)-1);

        coverage  = 100 / (float)PATHDIRECTORY_PATHHASH_SIZE * (PATHDIRECTORY_PATHHASH_SIZE - numEmpty);
        collision = 100 / (float) total * numCollisions;
    }
    else
    {
        variance = coverage = collision = 0;
    }

    assert(NULL != colWidths);
    col = colWidths;
    Con_Printf("%*s ",    *col++, name);
    Con_Printf("%*lu ",   *col++, (unsigned long)total);
    Con_Printf("%*lu",    *col++, PATHDIRECTORY_PATHHASH_SIZE - (unsigned long)numEmpty);
    Con_Printf(":%-*lu ", *col++, (unsigned long)numEmpty);
    Con_Printf("%*lu ",   *col++, (unsigned long)maxCollisions);
    Con_Printf("%*lu ",   *col++, (unsigned long)numCollisions);
    Con_Printf("%*.2f ",  *col++, collision);
    Con_Printf("%*.2f ",  *col++, coverage);
    Con_Printf("%*.2f ",  *col++, variance);
    Con_Printf("%*lu\n",  *col++, (unsigned long)maxHeight);
}

static void printDistributionOverview(PathDirectory* pd,
    size_t nodeCountSum[PATHDIRECTORYNODE_TYPE_COUNT],
    size_t nodeCountTotal[PATHDIRECTORYNODE_TYPE_COUNT],
    size_t nodeBucketCollisions[PATHDIRECTORYNODE_TYPE_COUNT], size_t nodeBucketCollisionsTotal,
    size_t nodeBucketCollisionsMax[PATHDIRECTORYNODE_TYPE_COUNT], size_t nodeBucketCollisionsMaxTotal,
    size_t nodeBucketEmpty[PATHDIRECTORYNODE_TYPE_COUNT], size_t nodeBucketEmptyTotal, size_t nodeBucketHeight,
    size_t nodeCount[PATHDIRECTORYNODE_TYPE_COUNT])
{
#define NUMCOLS             10/*type+count+used:+empty+collideMax+collideCount+collidePercent+coverage+variance+maxheight*/

    size_t collisionsMax = 0, countSum = 0, countTotal = 0;
    int i, nodeCountDigits, colWidths[NUMCOLS], spans[4][2], *span, *col;
    assert(pd);

    for(i = 0; i < PATHDIRECTORYNODE_TYPE_COUNT; ++i)
    {
        if(nodeBucketCollisionsMax[i] > collisionsMax)
            collisionsMax = nodeBucketCollisionsMax[i];
        countSum += nodeCountSum[i];
        countTotal += nodeCountTotal[i];
    }
    nodeCountDigits = M_NumDigits((int)countTotal);

    // Calculate minimum field widths:
    col = colWidths;
    *col = 0;
    for(i = 0; i < PATHDIRECTORYNODE_TYPE_COUNT; ++i)
    {
        if(Str_Length(PathDirectoryNode_TypeName(i)) > *col)
            *col = Str_Length(PathDirectoryNode_TypeName(i));
    }
    col++;
    *col++ = MAX_OF(nodeCountDigits, 1); /*#*/
    *col++ = MAX_OF(nodeCountDigits, 4); /*used*/
    *col++ = MAX_OF(nodeCountDigits, 5); /*empty*/
    *col++ = MAX_OF(nodeCountDigits, 3); /*max*/
    *col++ = MAX_OF(nodeCountDigits, 4); /*num*/
    *col++ = MAX_OF(3+1+2, 8);           /*percent*/
    *col++ = MAX_OF(3+1+2, 9);           /*coverage*/
    *col++ = MAX_OF(nodeCountDigits, 8); /*variance*/
    *col   = MAX_OF(nodeCountDigits, 9); /*maxheight*/

    // Calculate span widths:
    spans[0][0] = colWidths[0] + 1/* */ + colWidths[1];
    spans[1][0] = colWidths[2] + 1/*:*/ + colWidths[3];
    spans[2][0] = colWidths[4] + 1/* */ + colWidths[5] + 1/* */ + colWidths[6];
    spans[3][0] = colWidths[7] + 1/* */ + colWidths[8] + 1/* */ + colWidths[9];
    for(i = 0; i < 4; ++i)
    {
        int remainder = spans[i][0] % 2;
        spans[i][1] = remainder + (spans[i][0] /= 2);
    }

    Con_FPrintf(CPF_YELLOW, "Directory Distribution (p:%p):\n", pd);

    // Level1 headings:
    span = &spans[0][0];
    Con_Printf("%*s", *span++ +  5/2, "nodes"), Con_Printf("%-*s|",      *span++ -  5/2, "");
    Con_Printf("%*s", *span++ +  4/2, "hash"), Con_Printf("%-*s|",       *span++ -  4/2, "");
    Con_Printf("%*s", *span++ + 10/2, "collisions"), Con_Printf("%-*s|", *span++ - 10/2, "");
    Con_Printf("%*s", *span++ +  5/2, "other"), Con_Printf("%-*s\n",     *span++ -  5/2, "");

    // Level2 headings:
    col = colWidths;
    Con_FPrintf(CPF_LIGHT, "%*s ",   *col++, "type");
    Con_FPrintf(CPF_LIGHT, "%-*s|",  *col++, "#");
    Con_FPrintf(CPF_LIGHT, "%*s:",   *col++, "used");
    Con_FPrintf(CPF_LIGHT, "%-*s|",  *col++, "empty");
    Con_FPrintf(CPF_LIGHT, "%*s ",   *col++, "max");
    Con_FPrintf(CPF_LIGHT, "%*s ",   *col++, "num#");
    Con_FPrintf(CPF_LIGHT, "%-*s|",  *col++, "percent%");
    Con_FPrintf(CPF_LIGHT, "%*s ",   *col++, "coverage%");
    Con_FPrintf(CPF_LIGHT, "%*s ",   *col++, "variance");
    Con_FPrintf(CPF_LIGHT, "%-*s\n", *col++, "maxheight");

    if(countTotal != 0)
    {
        for(i = 0; i < PATHDIRECTORYNODE_TYPE_COUNT; ++i)
        {
            printDistributionOverviewElement(colWidths, Str_Text(PathDirectoryNode_TypeName(i)),
                nodeBucketEmpty[i], (i == PT_LEAF? nodeBucketHeight : 0),
                nodeBucketCollisions[i], nodeBucketCollisionsMax[i],
                nodeCountSum[i], nodeCountTotal[i]);
        }
        Con_PrintRuler();
    }

    printDistributionOverviewElement(colWidths, "total",
        nodeBucketEmptyTotal, nodeBucketHeight,
        nodeBucketCollisionsTotal, collisionsMax,
        countSum / PATHDIRECTORYNODE_TYPE_COUNT, countTotal);

#undef NUMCOLS
}

void printDistributionHistogram(PathDirectory* pd, ushort size,
    size_t nodeCountTotal[PATHDIRECTORYNODE_TYPE_COUNT])
{
#define NUMCOLS             4/*range+total+PATHDIRECTORYNODE_TYPE_COUNT*/
    size_t totalForRange, total, nodeCount[PATHDIRECTORYNODE_TYPE_COUNT];
    int hashIndexDigits, col, colWidths[2+/*range+total*/PATHDIRECTORYNODE_TYPE_COUNT];
    pathdirectory_pathhash_t** phAdr;
    PathDirectoryNode* node;
    int i, j;
    assert(pd);

    total = 0;
    for(i = 0; i < PATHDIRECTORYNODE_TYPE_COUNT; ++i)
        total += nodeCountTotal[i];
    if(0 == total) return;

    // Calculate minimum field widths:
    hashIndexDigits = M_NumDigits(PATHDIRECTORY_PATHHASH_SIZE);
    col = 0;
    if(size != 0)
        colWidths[col] = 2/*braces*/+hashIndexDigits*2+3/*elipses*/;
    else
        colWidths[col] = 2/*braces*/+hashIndexDigits;
    colWidths[col] = MAX_OF(colWidths[col], 5/*range*/);
    ++col;

    { size_t max = 0;
    for(i = 0; i < PATHDIRECTORYNODE_TYPE_COUNT; ++i)
        if(nodeCountTotal[i] > max)
            max = nodeCountTotal[i];
    colWidths[col++] = MAX_OF(M_NumDigits((int)max), 5/*total*/);
    }

    { int i;
    for(i = 0; i < PATHDIRECTORYNODE_TYPE_COUNT; ++i, ++col)
        colWidths[col] = Str_Length(PathDirectoryNode_TypeName(i));
    }

    // Apply formatting:
    for(i = 1; i < NUMCOLS; ++i) { colWidths[i] += 1; }

    Con_FPrintf(CPF_YELLOW, "Histogram (p:%p):\n", pd);
    // Print heading:
    col = 0;
    Con_Printf("%*s", colWidths[col++], "range");
    Con_Printf("%*s", colWidths[col++], "total");
    { int i;
    for(i = 0; i < PATHDIRECTORYNODE_TYPE_COUNT; ++i)
        Con_Printf("%*s", colWidths[col++], Str_Text(PathDirectoryNode_TypeName(i)));
    }
    Con_Printf("\n");
    Con_PrintRuler();

    { ushort i, from = 0, n = 0, range = (size != 0? PATHDIRECTORY_PATHHASH_SIZE / size: 0);
    memset(nodeCount, 0, sizeof(nodeCount));

    for(i = 0; i < PATHDIRECTORY_PATHHASH_SIZE; ++i)
    {
        phAdr = hashAddressForNodeType(pd, PT_BRANCH);
        if(*phAdr)
        for(node = (**phAdr)[i].head; node; node = node->next)
            ++nodeCount[PT_BRANCH];

        phAdr = hashAddressForNodeType(pd, PT_LEAF);
        if(*phAdr)
        for(node = (**phAdr)[i].head; node; node = node->next)
            ++nodeCount[PT_LEAF];

        if(size != 0 && (++n != range && i != PATHDIRECTORY_PATHHASH_SIZE-1))
            continue;

        totalForRange = 0;
        for(j = 0; j < PATHDIRECTORYNODE_TYPE_COUNT; ++j)
            totalForRange += nodeCount[j];

        col = 0;
        if(size != 0)
        {
            ddstring_t range; Str_Init(&range);
            Str_Appendf(&range, "%*u...%*u", hashIndexDigits, from, hashIndexDigits, from+n-1);
            Con_Printf("[%*s]", colWidths[col++]-2/*braces*/, Str_Text(&range));
            Str_Free(&range);
        }
        else
        {
            Con_Printf("[%*u]", colWidths[col++]-2/*braces*/, i);
        }

        Con_Printf("%*lu", colWidths[col++], (unsigned long) totalForRange);
        if(0 != totalForRange)
        {
            for(j = 0; j < PATHDIRECTORYNODE_TYPE_COUNT; ++j, ++col)
            {
                if(0 != nodeCount[j])
                {
                    Con_Printf("%*lu", colWidths[col], (unsigned long) nodeCount[j]);
                }
                else if(j < PATHDIRECTORYNODE_TYPE_COUNT-1 || 0 == size)
                {
                    Con_Printf("%*s", colWidths[col], "");
                }
            }
        }

        // Are we printing a "graphical" representation?
        if(0 != totalForRange)
        {
            size_t max = MAX_OF(1, ROUND(total/(float)size/10));
            size_t scale = totalForRange / (float)max;

            scale = MAX_OF(scale, 1);
            Con_Printf(" ");
            for(n = 0; n < scale; ++n)
                Con_Printf("*");
        }

        Con_Printf("\n");
        from = i+1;
        n = 0;
        memset(nodeCount, 0, sizeof(nodeCount));
    }}
    Con_PrintRuler();

    // Sums:
    col = 0;
    Con_Printf("%*s",  colWidths[col++], "Sum");
    Con_Printf("%*lu", colWidths[col++], (unsigned long) total);
    if(0 != total)
    {
        int i;
        for(i = 0; i < PATHDIRECTORYNODE_TYPE_COUNT; ++i, ++col)
        {
            if(0 != nodeCountTotal[i])
            {
                Con_Printf("%*lu", colWidths[col], (unsigned long) nodeCountTotal[i]);
            }
            else if(i < PATHDIRECTORYNODE_TYPE_COUNT-1)
            {
                Con_Printf("%*s", colWidths[col], "");
            }
        }
    }
    Con_Printf("\n");

#undef NUMCOLS
}

void PathDirectory_PrintHashDistribution(PathDirectory* pd)
{
    size_t nodeCountSum[PATHDIRECTORYNODE_TYPE_COUNT],
           nodeCountTotal[PATHDIRECTORYNODE_TYPE_COUNT], nodeBucketHeight = 0,
           nodeBucketCollisions[PATHDIRECTORYNODE_TYPE_COUNT], nodeBucketCollisionsTotal = 0,
           nodeBucketCollisionsMax[PATHDIRECTORYNODE_TYPE_COUNT], nodeBucketCollisionsMaxTotal = 0,
           nodeBucketEmpty[PATHDIRECTORYNODE_TYPE_COUNT], nodeBucketEmptyTotal = 0,
           nodeCount[PATHDIRECTORYNODE_TYPE_COUNT];
    pathdirectory_pathhash_t** phAdr;
    size_t totalForRange;
    PathDirectoryNode* node;
    int j;
    assert(pd);

    nodeCountTotal[PT_BRANCH] = countNodesInPathHash(*hashAddressForNodeType(pd, PT_BRANCH));
    nodeCountTotal[PT_LEAF]   = countNodesInPathHash(*hashAddressForNodeType(pd, PT_LEAF));

    memset(nodeCountSum, 0, sizeof(nodeCountSum));
    memset(nodeBucketCollisions, 0, sizeof(nodeBucketCollisions));
    memset(nodeBucketCollisionsMax, 0, sizeof(nodeBucketCollisionsMax));
    memset(nodeBucketEmpty, 0, sizeof(nodeBucketEmpty));

    { ushort i;
    for(i = 0; i < PATHDIRECTORY_PATHHASH_SIZE; ++i)
    {
        phAdr = hashAddressForNodeType(pd, PT_BRANCH);
        nodeCount[PT_BRANCH] = 0;
        if(*phAdr)
        for(node = (**phAdr)[i].head; node; node = node->next)
            ++nodeCount[PT_BRANCH];

        phAdr = hashAddressForNodeType(pd, PT_LEAF);
        nodeCount[PT_LEAF] = 0;
        if(*phAdr)
        {
            size_t chainHeight = 0;
            for(node = (**phAdr)[i].head; node; node = node->next)
            {
                size_t height = 0;
                PathDirectoryNode* other = node;

                ++nodeCount[PT_LEAF];

                while((other = PathDirectoryNode_Parent(other))) { ++height; }

                if(height > chainHeight)
                    chainHeight = height;
            }

            if(chainHeight > nodeBucketHeight)
                nodeBucketHeight = chainHeight;
        }

        totalForRange = nodeCount[PT_BRANCH] + nodeCount[PT_LEAF];

        nodeCountSum[PT_BRANCH] += nodeCount[PT_BRANCH];
        nodeCountSum[PT_LEAF]   += nodeCount[PT_LEAF];

        for(j = 0; j < PATHDIRECTORYNODE_TYPE_COUNT; ++j)
        {
            if(nodeCount[j] != 0)
            {
                if(nodeCount[j] > 1)
                    nodeBucketCollisions[j] += nodeCount[j]-1;
            }
            else
            {
                ++(nodeBucketEmpty[j]);
            }
            if(nodeCount[j] > nodeBucketCollisionsMax[j])
                nodeBucketCollisionsMax[j] = nodeCount[j];
        }

        { size_t max = 0;
        for(j = 0; j < PATHDIRECTORYNODE_TYPE_COUNT; ++j)
        {
            max += nodeCount[j];
        }
        if(max > nodeBucketCollisionsMaxTotal)
            nodeBucketCollisionsMaxTotal = max;
        }

        if(totalForRange != 0)
        {
            if(totalForRange > 1)
                nodeBucketCollisionsTotal += totalForRange-1;
        }
        else
        {
            ++nodeBucketEmptyTotal;
        }
        if(totalForRange > nodeBucketCollisionsMaxTotal)
            nodeBucketCollisionsMaxTotal = totalForRange;
    }}

    printDistributionOverview(pd, nodeCountSum, nodeCountTotal,
        nodeBucketCollisions,    nodeBucketCollisionsTotal,
        nodeBucketCollisionsMax, nodeBucketCollisionsMaxTotal,
        nodeBucketEmpty, nodeBucketEmptyTotal,
        nodeBucketHeight, nodeCount);
    Con_Printf("\n");
    printDistributionHistogram(pd, 16, nodeCountTotal);
}
#endif

static PathDirectoryNode* PathDirectoryNode_New(PathDirectory* directory,
    pathdirectorynode_type_t type, PathDirectoryNode* parent,
    StringPoolInternId internId, void* userData)
{
    PathDirectoryNode* node;
    assert(directory);

    node = (PathDirectoryNode*) malloc(sizeof *node);
    if(!node)
        Con_Error("PathDirectory::direcNode: Failed on allocation of %lu bytes for "
            "new PathDirectory::Node.", (unsigned long) sizeof *node);

    node->_directory = directory;
    node->_type = type;
    node->_parent = parent;
    node->_pair.internId = internId;
    node->_pair.data = userData;
    return node;
}

static void PathDirectoryNode_Delete(PathDirectoryNode* node)
{
    assert(node);
    free(node);
}

PathDirectory* PathDirectoryNode_Directory(const PathDirectoryNode* node)
{
    assert(node);
    return node->_directory;
}

PathDirectoryNode* PathDirectoryNode_Parent(const PathDirectoryNode* node)
{
    assert(node);
    return node->_parent;
}

pathdirectorynode_type_t PathDirectoryNode_Type(const PathDirectoryNode* node)
{
    assert(node);
    return node->_type;
}

const ddstring_t* PathDirectoryNode_TypeName(pathdirectorynode_type_t type)
{
    static const ddstring_t nodeNames[1+PATHDIRECTORYNODE_TYPE_COUNT] = {
        { "(invalidtype)" },
        { "branch" },
        { "leaf" }
    };
    if(!VALID_PATHDIRECTORYNODE_TYPE(type)) return &nodeNames[0];
    return &nodeNames[1 + (type - PATHDIRECTORYNODE_TYPE_FIRST)];
}

StringPoolInternId PathDirectoryNode_InternId(const PathDirectoryNode* node)
{
    assert(node);
    return node->_pair.internId;
}

/// \note This routine is also used as an iteration callback, so only return
/// a non-zero value when the node is a match for the search term.
int PathDirectoryNode_MatchDirectory(PathDirectoryNode* node, int flags,
    PathMap* searchPattern, void* paramaters)
{
    PathDirectory* pd = PathDirectoryNode_Directory(node);
    const PathMapFragment* sfragment;
    const ddstring_t* fragment;
    uint i, fragmentCount;

    if(((flags & PCF_NO_LEAF)   && PT_LEAF   == PathDirectoryNode_Type(node)) ||
       ((flags & PCF_NO_BRANCH) && PT_BRANCH == PathDirectoryNode_Type(node)))
        return false;

    sfragment = PathMap_Fragment(searchPattern, 0);
    if(!sfragment) return false; // Hmm...

    // In reverse order, compare path fragments in the search term.
    fragmentCount = PathMap_Size(searchPattern);
    for(i = 0; i < fragmentCount; ++i)
    {
        // If the hashes don't match it can't possibly be this.
        if(sfragment->hash != hashForInternId(pd, PathDirectoryNode_InternId(node)))
            return false;

        // Compare the path fragment to that of the search term.
        fragment = PathDirectory_GetFragment(pd, node);
        if(Str_Length(fragment) < (sfragment->to - sfragment->from)+1 ||
           strnicmp(Str_Text(fragment), sfragment->from, Str_Length(fragment)))
            return false;

        // Have we arrived at the search target?
        if(i == fragmentCount-1)
            return (!(flags & PCF_MATCH_FULL) || !PathDirectoryNode_Parent(node));

        // Are there no more parent directories?
        if(!PathDirectoryNode_Parent(node))
            return false;

        // So far so good. Move one directory level upwards.
        node = PathDirectoryNode_Parent(node);
        sfragment = PathMap_Fragment(searchPattern, i+1);
    }
    return false;
}

void PathDirectoryNode_AttachUserData(PathDirectoryNode* node, void* userData)
{
    assert(node);
#if _DEBUG
    if(node->_pair.data)
    {
        Con_Message("Warning:PathDirectoryNode::AttachUserData: Data is already associated "
            "with this node, will be replaced.\n");
    }
#endif
    node->_pair.data = userData;
}

void* PathDirectoryNode_DetachUserData(PathDirectoryNode* node)
{
    void* userData;
    assert(node);
    userData = node->_pair.data;
    node->_pair.data = NULL;
    return userData;
}

void* PathDirectoryNode_UserData(const PathDirectoryNode* node)
{
    assert(node);
    return node->_pair.data;
}
