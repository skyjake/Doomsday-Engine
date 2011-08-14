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

#include "stringpool.h"
#include "pathdirectory.h"
#include "m_misc.h"

typedef struct pathdirectory_node_userdatapair_s {
    stringpool_internid_t internId;
    void* data;
} pathdirectory_node_userdatapair_t;

typedef struct pathdirectory_node_s {
    /// Next node in the hashed path bucket.
    struct pathdirectory_node_s* next;

    /// Parent node in the user's logical hierarchy.
    struct pathdirectory_node_s* _parent;

    /// Symbolic node type.
    pathdirectory_nodetype_t _type;

    /// PathDirectory which owns this node.
    pathdirectory_t* _directory;

    /// User data present at this node.
    pathdirectory_node_userdatapair_t _pair;
} pathdirectory_node_t;

static pathdirectory_node_t* PathDirectoryNode_Construct(pathdirectory_t* directory,
    pathdirectory_nodetype_t type, pathdirectory_node_t* parent,
    stringpool_internid_t internId, void* userData);

static void PathDirectoryNode_Destruct(pathdirectory_node_t* node);

static volatile int numInstances = 0;
// A mutex is used to protect access to the shared fragment info buffer and search state.
static mutex_t fragmentBuffer_Mutex = 0;

#define PATHDIRECTORY_FRAGMENT_BUFFER_SIZE 16

/// \protected by fragmentBuffer_Mutex;
static pathdirectory_fragmentinfo_t smallFragmentBuffer[PATHDIRECTORY_FRAGMENT_BUFFER_SIZE];
static pathdirectory_fragmentinfo_t* largeFragmentBuffer = NULL;
static size_t largeFragmentBufferSize = 0;
static pathdirectory_search_t search;

static volatile boolean searchInProgress = false;

#if 0//_DEBUG
void lockMutex(mutex_t mutex)
{
    //searchInProgress = true;
    Con_Message("Locking PathDirectory.\n");
    Sys_Lock(mutex);
}
#define Sys_Lock lockMutex

void unlockMutex(mutex_t mutex)
{
    //searchInProgress = false;
    Con_Message("Unlocking PathDirectory.\n");
    Sys_Unlock(mutex);
}
#define Sys_Unlock unlockMutex
#endif

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

static void clearInternPool(pathdirectory_t* pd)
{
    assert(NULL != pd);
    if(NULL != pd->_internPool.strings)
    {
        StringPool_Destruct(pd->_internPool.strings), pd->_internPool.strings = NULL;
        free(pd->_internPool.idHashMap), pd->_internPool.idHashMap = NULL;
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
#if _DEBUG
        if(NULL != PathDirectoryNode_UserData((*ph)[i]))
        {
            Con_Error("PathDirectory::clearPathHash: Node %p has non-NULL user data.", (*ph)[i]);
            exit(1); // Unreachable.
        }
#endif
        PathDirectoryNode_Destruct((*ph)[i]);
        (*ph)[i] = next;
    }
}

static size_t countNodes(pathdirectory_t* pd, int flags)
{
    assert(NULL != pd);
    {
    size_t count = 0;
    if(NULL != pd->_pathHash)
    {
        pathdirectory_node_t* node;
        ushort i;

        for(i = 0; i < PATHDIRECTORY_PATHHASH_SIZE; ++i)
        for(node = (pathdirectory_node_t*) (*pd->_pathHash)[i];
            NULL != node; node = node->next)
        {
            if(((flags & PCF_NO_LEAF)   && PT_LEAF   == PathDirectoryNode_Type(node)) ||
               ((flags & PCF_NO_BRANCH) && PT_BRANCH == PathDirectoryNode_Type(node)))
                continue;
            ++count;
        }
    }
    return count;
    }
}

static pathdirectory_node_t* findNode(pathdirectory_t* pd, pathdirectory_node_t* parent,
    pathdirectory_nodetype_t nodeType, stringpool_internid_t internId)
{
    assert(NULL != pd && 0 != internId);
    {
    pathdirectory_node_t* node = NULL;
    if(NULL != pd->_pathHash)
    {
        ushort hash = pd->_internPool.idHashMap[internId-1];
        for(node = (*pd->_pathHash)[hash]; NULL != node; node = node->next)
        {
            if(parent != PathDirectoryNode_Parent(node)) continue;
            if(nodeType != PathDirectoryNode_Type(node)) continue;

            if(internId == PathDirectoryNode_InternId(node))
                return node;
        }
    }
    return node;
    }
}

static ushort hashForInternId(pathdirectory_t* pd, stringpool_internid_t internId)
{
    assert(NULL != pd);
    if(0 == internId)
        Con_Error("PathDirectory::hashForInternId: Invalid internId %u.", internId);
    return pd->_internPool.idHashMap[internId-1];
}

static stringpool_internid_t internNameAndUpdateIdHashMap(pathdirectory_t* pd,
    const ddstring_t* name, ushort hash)
{
    assert(NULL != pd);
    {
    stringpool_t* pool = pd->_internPool.strings;
    stringpool_internid_t internId;
    uint oldSize;

    if(NULL == pool)
    {
        pool = pd->_internPool.strings = StringPool_ConstructDefault();
    }
    oldSize = StringPool_Size(pool);

    internId = StringPool_Intern(pool, name);
    if(oldSize != StringPool_Size(pool))
    {
        // A new string was added to the pool.
        pd->_internPool.idHashMap = (ushort*) realloc(pd->_internPool.idHashMap, sizeof(*pd->_internPool.idHashMap) * StringPool_Size(pool));
        if(NULL == pd->_internPool.idHashMap)
            Con_Error("PathDirectory::internNameAndUpdateIdHashMap: Failed on (re)allocation of "
                "%lu bytes for the IdHashMap", (unsigned long) (sizeof(*pd->_internPool.idHashMap) * StringPool_Size(pool)));
        if(internId < StringPool_Size(pool))
        {
            memmove(pd->_internPool.idHashMap + internId, pd->_internPool.idHashMap + (internId-1), sizeof(*pd->_internPool.idHashMap) * (StringPool_Size(pool) - internId));
        }
        pd->_internPool.idHashMap[internId-1] = hash;
    }
    return internId;
    }
}

/**
 * @return  [ a new | the ] directory node that matches the name and type and
 * which has the specified parent node.
 */
static pathdirectory_node_t* direcNode(pathdirectory_t* pd, pathdirectory_node_t* parent,
    pathdirectory_nodetype_t nodeType, const ddstring_t* name, char delimiter, void* userData)
{
    assert(NULL != pd && NULL != name);
    {
    stringpool_internid_t internId = 0;
    pathdirectory_node_t* node;
    ushort hash;

    // Have we already encountered this?
    if(NULL != pd->_internPool.strings)
    {
        internId = StringPool_IsInterned(pd->_internPool.strings, name);
        if(0 != internId)
        {
            // The name is known. Perhaps we have.
            node = findNode(pd, parent, nodeType, internId);
            if(NULL != node) return node;
        }
    }

    /**
     * A new node is needed.
     */

    // Do we need a new name identifier (and hash)?
    if(0 == internId)
    {
        hash = hashName(Str_Text(name), Str_Length(name), delimiter);
        internId = internNameAndUpdateIdHashMap(pd, name, hash);
    }
    else
    {
        hash = hashForInternId(pd, internId);
    }

    // Are we out of name indices?
    if(0 == internId)
        return NULL;

    node = PathDirectoryNode_Construct(pd, nodeType, parent, internId, userData);

    // Do we need to init the path hash?
    if(NULL == pd->_pathHash)
        initPathHash(pd);

    // Insert the new node into the path hash.
    node->next = (*pd->_pathHash)[hash];
    (*pd->_pathHash)[hash] = node;

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

static int iteratePaths(pathdirectory_t* pd, int flags, pathdirectory_node_t* parent,
    ushort hash, int (*callback) (pathdirectory_node_t* node, void* paramaters),
    void* paramaters)
{
    assert(NULL != pd && NULL != callback);
    {
    int result = 0;
    if(NULL != pd->_pathHash)
    {
        pathdirectory_node_t* node;
        ushort i;
        if(hash < PATHDIRECTORY_PATHHASH_SIZE)
        {
            for(node = (*pd->_pathHash)[hash]; NULL != node; node = node->next)
            {
                if(((flags & PCF_NO_LEAF)   && PT_LEAF   == PathDirectoryNode_Type(node)) ||
                   ((flags & PCF_NO_BRANCH) && PT_BRANCH == PathDirectoryNode_Type(node)))
                    continue;
                if((flags & PCF_MATCH_PARENT) && parent != PathDirectoryNode_Parent(node))
                    continue;

                if(0 != (result = callback(node, paramaters)))
                    break;
            }
        }
        else
        {
            for(i = 0; i < PATHDIRECTORY_PATHHASH_SIZE; ++i)
            for(node = (*pd->_pathHash)[i]; NULL != node; node = node->next)
            {
                if(((flags & PCF_NO_LEAF)   && PT_LEAF   == PathDirectoryNode_Type(node)) ||
                   ((flags & PCF_NO_BRANCH) && PT_BRANCH == PathDirectoryNode_Type(node)))
                    continue;
                if((flags & PCF_MATCH_PARENT) && parent != PathDirectoryNode_Parent(node))
                    continue;

                if(0 != (result = callback(node, paramaters)))
                    break;
            }
        }
    }
    return result;
    }
}

static int iteratePaths_const(const pathdirectory_t* pd, int flags, const pathdirectory_node_t* parent,
    ushort hash, int (*callback) (const pathdirectory_node_t* node, void* paramaters), void* paramaters)
{
    assert(NULL != pd && NULL != callback);
    {
    int result = 0;
    if(NULL != pd->_pathHash)
    {
        pathdirectory_node_t* node;
        ushort i;
        if(hash < PATHDIRECTORY_PATHHASH_SIZE)
        {
            for(node = (*pd->_pathHash)[hash]; NULL != node; node = node->next)
            {
                if(((flags & PCF_NO_LEAF)   && PT_LEAF   == PathDirectoryNode_Type(node)) ||
                   ((flags & PCF_NO_BRANCH) && PT_BRANCH == PathDirectoryNode_Type(node)))
                    continue;
                if((flags & PCF_MATCH_PARENT) && parent != PathDirectoryNode_Parent(node))
                    continue;

                if(0 != (result = callback(node, paramaters)))
                    break;
            }
        }
        else
        {
            for(i = 0; i < PATHDIRECTORY_PATHHASH_SIZE; ++i)
            for(node = (*pd->_pathHash)[i]; NULL != node; node = node->next)
            {
                if(((flags & PCF_NO_LEAF)   && PT_LEAF   == PathDirectoryNode_Type(node)) ||
                   ((flags & PCF_NO_BRANCH) && PT_BRANCH == PathDirectoryNode_Type(node)))
                    continue;
                if((flags & PCF_MATCH_PARENT) && parent != PathDirectoryNode_Parent(node))
                    continue;

                if(0 != (result = callback(node, paramaters)))
                    break;
            }
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
    pd->_internPool.strings = NULL;
    pd->_internPool.idHashMap = NULL;
    pd->_pathHash = NULL;

    if(numInstances == 0)
    {
        fragmentBuffer_Mutex = Sys_CreateMutex("PathDirectory::fragmentBuffer_MUTEX");
    }
    ++numInstances;
    return pd;
}

void PathDirectory_Destruct(pathdirectory_t* pd)
{
    assert(NULL != pd);
    clearPathHash(pd->_pathHash);
    destroyPathHash(pd);
    clearInternPool(pd);
    free(pd);

    --numInstances;
    if(numInstances == 0)
    {
        Sys_DestroyMutex(fragmentBuffer_Mutex);
        fragmentBuffer_Mutex = 0;
    }
}

const ddstring_t* PathDirectory_NodeTypeName(pathdirectory_nodetype_t type)
{
    static const ddstring_t nodeNames[PATHDIRECTORY_NODETYPES_COUNT] = {
        { "branch" },
        { "leaf" },
    };
    assert(VALID_PATHDIRECTORY_NODETYPE(type));
    return &nodeNames[type - PATHDIRECTORY_NODETYPES_FIRST];
}

void PathDirectory_Clear(pathdirectory_t* pd)
{
    assert(NULL != pd);
    clearPathHash(pd->_pathHash);
    clearInternPool(pd);
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

int PathDirectory_Iterate2(pathdirectory_t* pd, int flags, pathdirectory_node_t* parent,
    ushort hash, int (*callback) (pathdirectory_node_t* node, void* paramaters), void* paramaters)
{
    return iteratePaths(pd, flags, parent, hash, callback, paramaters);
}

int PathDirectory_Iterate(pathdirectory_t* pd, int flags, pathdirectory_node_t* parent,
    ushort hash, int (*callback) (pathdirectory_node_t* node, void* paramaters))
{
    return PathDirectory_Iterate2(pd, flags, parent, hash, callback, NULL);
}

int PathDirectory_Iterate2_Const(const pathdirectory_t* pd, int flags, const pathdirectory_node_t* parent,
    ushort hash, int (*callback) (const pathdirectory_node_t* node, void* paramaters),
    void* paramaters)
{
    return iteratePaths_const(pd, flags, parent, hash, callback, paramaters);
}

int PathDirectory_Iterate_Const(const pathdirectory_t* pd, int flags, const pathdirectory_node_t* parent,
    ushort hash, int (*callback) (const pathdirectory_node_t* node, void* paramaters))
{
    return PathDirectory_Iterate2_Const(pd, flags, parent, hash, callback, NULL);
}

const ddstring_t* PathDirectory_GetFragment(pathdirectory_t* pd, const pathdirectory_node_t* node)
{
    assert(NULL != pd);
    return StringPool_String(pd->_internPool.strings, PathDirectoryNode_InternId(node));
}

/**
 * Calculate the total length of the final composed path.
 */
static int PathDirectory_CalcPathLength(pathdirectory_t* pd, const pathdirectory_node_t* node, char delimiter)
{
    assert(NULL != pd && NULL != node);
    {
    const int delimiterLen = delimiter? 1 : 0;
    int requiredLen = 0;

    if(PT_BRANCH == PathDirectoryNode_Type(node))
        requiredLen += delimiterLen;

    requiredLen += Str_Length(PathDirectory_GetFragment(pd, node));
    if(NULL != PathDirectoryNode_Parent(node))
    {
        const pathdirectory_node_t* trav = PathDirectoryNode_Parent(node);
        requiredLen += delimiterLen;
        do
        {
            requiredLen += Str_Length(PathDirectory_GetFragment(pd, trav));
            if(NULL != PathDirectoryNode_Parent(trav))
                requiredLen += delimiterLen;
        } while(NULL != (trav = PathDirectoryNode_Parent(trav)));
    }

    return requiredLen;
    }
}

/// \assume @a foundPath already has sufficent characters reserved to hold the fully composed path.
static ddstring_t* PathDirectory_ConstructPath(pathdirectory_t* pd, const pathdirectory_node_t* node,
    ddstring_t* foundPath, char delimiter)
{
    assert(NULL != pd && NULL != node && foundPath != NULL);
    {
    const int delimiterLen = delimiter? 1 : 0;
    const pathdirectory_node_t* trav;
    const ddstring_t* fragment;
    if(PT_BRANCH == PathDirectoryNode_Type(node) && 0 != delimiterLen)
        Str_AppendChar(foundPath, delimiter);
    trav = node;
    do
    {
        fragment = PathDirectory_GetFragment(pd, trav);
        Str_Prepend(foundPath, Str_Text(fragment));
        if(NULL != PathDirectoryNode_Parent(trav) && 0 != delimiterLen)
            Str_PrependChar(foundPath, delimiter);
    } while(NULL != (trav = PathDirectoryNode_Parent(trav)));
    return foundPath;
    }
}

ddstring_t* PathDirectory_ComposePath(pathdirectory_t* pd, const pathdirectory_node_t* node,
    ddstring_t* foundPath, int* length, char delimiter)
{
    assert(NULL != pd && NULL != node);
    if(NULL == foundPath && length != NULL)
    {
        PathDirectory_CalcPathLength(pd, node, delimiter);
        return foundPath;
    }
    else
    {
        int fullLength;

        PathDirectory_ComposePath(pd, node, NULL, &fullLength, delimiter);
        if(NULL != length)
            *length = fullLength;

        if(NULL == foundPath)
            return foundPath;

        Str_Clear(foundPath);
        Str_Reserve(foundPath, fullLength);
        return PathDirectory_ConstructPath(pd, node, foundPath, delimiter);
    }
}

static size_t splitSearchPath(const char* searchPath, size_t searchPathLen, char delimiter,
    pathdirectory_fragmentinfo_t* storage)
{
    assert(searchPath && searchPath[0] && searchPathLen != 0);
    {
    size_t i, fragments;
    const char* begin = searchPath;
    const char* to = begin + searchPathLen - 1;
    const char* from;

    // Skip over any trailing delimiters.
    for(i = searchPathLen; *to && *to == delimiter && i-- > 0; to--) {}

    // In reverse order scan for distinct fragments in the search term.
    fragments = 0;
    for(;;)
    {
        // Find the start of the next path fragment.
        for(from = to; from > begin && !(*from == delimiter); from--) {}

        // One more.
        fragments++;

        // Are we storing info?
        if(storage)
        {
            storage->from = (*from == delimiter? from + 1 : from);
            storage->to   = to;
            // Hashing is deferred; means not-hashed yet.
            storage->hash = PATHDIRECTORY_PATHHASH_SIZE;
        }

        // Are there no more parent directories?
        if(from == begin) break;

        // So far so good. Move one directory level upwards.
        // The next fragment ends here.
        to = from-1;

        if(storage) storage++;
    }

    return fragments;
    }
}

static pathdirectory_fragmentinfo_t* enlargeFragmentBuffer(size_t fragments)
{
    if(fragments <= PATHDIRECTORY_FRAGMENT_BUFFER_SIZE)
    {
        return smallFragmentBuffer;
    }
    if(largeFragmentBuffer == NULL || fragments > largeFragmentBufferSize)
    {
        largeFragmentBufferSize = fragments;
        largeFragmentBuffer = (pathdirectory_fragmentinfo_t*)realloc(largeFragmentBuffer,
            sizeof(*largeFragmentBuffer) * largeFragmentBufferSize);
        if(largeFragmentBuffer == NULL)
            Con_Error("PathDirectory::enlargeFragmentBuffer: Failed on reallocation of %lu bytes.",
                (unsigned long)(sizeof(*largeFragmentBuffer) * largeFragmentBufferSize));
    }
    return largeFragmentBuffer;
}

static void freeFragmentBuffer(void)
{
    if(largeFragmentBuffer == NULL)
        return;
    free(largeFragmentBuffer), largeFragmentBuffer = NULL;
    largeFragmentBufferSize = 0;
}

pathdirectory_search_t* PathDirectory_BeginSearchStr(pathdirectory_t* pd, int flags,
    const ddstring_t* searchPath, char delimiter)
{
    assert(NULL != pd);

    if(searchInProgress)
        Con_Error("PathDirectory::BeginSearch: Search already in progress.");

    Sys_Lock(fragmentBuffer_Mutex);

    search.resultNode = NULL;
    search.result = 0;
    search.flags = flags;
    search.delimiter = delimiter;
    Str_Init(&search.path);
    Str_Reserve(&search.path, Str_Length(searchPath));
    Str_Set(&search.path, Str_Text(searchPath));
    search.fragments = splitSearchPath(Str_Text(&search.path), Str_Length(&search.path), delimiter, NULL);
    searchInProgress = true;
    search.info  = enlargeFragmentBuffer(search.fragments);
    splitSearchPath(Str_Text(&search.path), Str_Length(&search.path), delimiter, search.info);
    // Hash the first (i.e., rightmost) fragment now.
    search.info[0].hash = hashName(search.info[0].from, (search.info[0].to - search.info[0].from) + 1, delimiter);

    return &search;
}

pathdirectory_search_t* PathDirectory_BeginSearch(pathdirectory_t* pd, int flags,
    const char* searchPath, char delimiter)
{
    pathdirectory_search_t* result;
    ddstring_t temp; Str_Init(&temp); Str_Set(&temp, searchPath);
    result = PathDirectory_BeginSearchStr(pd, flags, &temp, delimiter);
    Str_Free(&temp);
    return result;
}

int PathDirectory_EndSearch2(pathdirectory_t* pd, pathdirectory_node_t** resultNode)
{
    assert(NULL != pd);
    {
    int result = search.result;
    if(resultNode) *resultNode = search.resultNode;

    if(!searchInProgress)
        Con_Error("PathDirectory::EndSearch: Attempted with no search in progress.\n");

    freeFragmentBuffer();
    Str_Free(&search.path);
    Sys_Unlock(fragmentBuffer_Mutex);
    searchInProgress = false;
    return result;
    }
}

int PathDirectory_EndSearch(pathdirectory_t* pd)
{
    return PathDirectory_EndSearch2(pd, NULL);
}

pathdirectory_search_t* PathDirectory_Search(pathdirectory_t* pd)
{
    assert(NULL != pd);

    if(!searchInProgress)
        Con_Error("PathDirectory::Search: Attempted with no search in progress.\n");
    return &search;
}

ddstring_t* PathDirectory_CollectPaths(pathdirectory_t* pd, int flags, char delimiter,
    size_t* retCount)
{
    assert(NULL != pd);
    {
    ddstring_t* paths = NULL;
    size_t count = countNodes(pd, flags);
    if(0 != count)
    {
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
            if(((flags & PCF_NO_LEAF)   && PT_LEAF   == PathDirectoryNode_Type(node)) ||
               ((flags & PCF_NO_BRANCH) && PT_BRANCH == PathDirectoryNode_Type(node)))
                continue;

            Str_Init(*pathPtr);
            PathDirectory_ComposePath(PathDirectoryNode_Directory(node), node, *pathPtr, NULL, delimiter);
            pathPtr++;
        }
        *pathPtr = NULL;
    }
    if(retCount)
        *retCount = count;
    return paths;
    }
}

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

    Con_Printf("PathDirectory: %p\n", pd);
    if(NULL != (pathList = PathDirectory_CollectPaths(pd, PT_LEAF, delimiter, &numLeafs)))
    {
        qsort(pathList, numLeafs, sizeof(*pathList), comparePaths);
        do
        {
            Con_Printf("  %s\n", F_PrettyPath(Str_Text(pathList + n)));
            Str_Free(pathList + n);
        } while(++n < numLeafs);
        free(pathList);
    }
    Con_Printf("  %lu %s in directory.\n", (unsigned long)numLeafs, (numLeafs==1? "path":"paths"));
    }
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

static void printDistributionOverview(pathdirectory_t* pd,
    size_t nodeCountSum[PATHDIRECTORY_NODETYPES_COUNT],
    size_t nodeCountTotal[PATHDIRECTORY_NODETYPES_COUNT],
    size_t nodeBucketCollisions[PATHDIRECTORY_NODETYPES_COUNT], size_t nodeBucketCollisionsTotal,
    size_t nodeBucketCollisionsMax[PATHDIRECTORY_NODETYPES_COUNT], size_t nodeBucketCollisionsMaxTotal,
    size_t nodeBucketEmpty[PATHDIRECTORY_NODETYPES_COUNT], size_t nodeBucketEmptyTotal, size_t nodeBucketHeight,
    size_t nodeCount[PATHDIRECTORY_NODETYPES_COUNT])
{
#define NUMCOLS             10/*type+count+used:+empty+collideMax+collideCount+collidePercent+coverage+variance+maxheight*/
    assert(NULL != pd);
    {
    size_t collisionsMax = 0, countSum = 0, countTotal = 0;
    int i, nodeCountDigits, colWidths[NUMCOLS], spans[4][2], *span, *col;

    for(i = 0; i < PATHDIRECTORY_NODETYPES_COUNT; ++i)
    {
        if(nodeBucketCollisionsMax[i] > collisionsMax)
            collisionsMax = nodeBucketCollisionsMax[i];
        countSum += nodeCountSum[i];
        countTotal += nodeCountTotal[i];
    }
    nodeCountDigits = M_NumDigits(countTotal);

    // Calculate minimum field widths:
    col = colWidths;
    *col = 0;
    for(i = 0; i < PATHDIRECTORY_NODETYPES_COUNT; ++i)
    {
        if(Str_Length(PathDirectory_NodeTypeName(i)) > *col)
            *col = Str_Length(PathDirectory_NodeTypeName(i));
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

    Con_FPrintf(CBLF_YELLOW, "Directory Distribution (p:%p):\n", pd);

    // Level1 headings:
    span = &spans[0][0];
    Con_Printf("%*s%-*s|",  *span++ +  5/2, "nodes",      *span++ -  5/2, "");
    Con_Printf("%*s%-*s|",  *span++ +  4/2, "hash",       *span++ -  4/2, "");
    Con_Printf("%*s%-*s|",  *span++ + 10/2, "collisions", *span++ - 10/2, "");
    Con_Printf("%*s%-*s\n", *span++ +  5/2, "other",      *span++ -  5/2, "");

    // Level2 headings:
    col = colWidths;
    Con_FPrintf(CBLF_LIGHT, "%*s ",   *col++, "type");
    Con_FPrintf(CBLF_LIGHT, "%-*s|",  *col++, "#");
    Con_FPrintf(CBLF_LIGHT, "%*s:",   *col++, "used");
    Con_FPrintf(CBLF_LIGHT, "%-*s|",  *col++, "empty");
    Con_FPrintf(CBLF_LIGHT, "%*s ",   *col++, "max");
    Con_FPrintf(CBLF_LIGHT, "%*s ",   *col++, "num#");
    Con_FPrintf(CBLF_LIGHT, "%-*s|",  *col++, "percent%");
    Con_FPrintf(CBLF_LIGHT, "%*s ",   *col++, "coverage%");
    Con_FPrintf(CBLF_LIGHT, "%*s ",   *col++, "variance");
    Con_FPrintf(CBLF_LIGHT, "%-*s\n", *col++, "maxheight");

    if(countTotal != 0)
    {
        for(i = 0; i < PATHDIRECTORY_NODETYPES_COUNT; ++i)
        {
            printDistributionOverviewElement(colWidths, Str_Text(PathDirectory_NodeTypeName(i)),
                nodeBucketEmpty[i], (i == PT_LEAF? nodeBucketHeight : 0),
                nodeBucketCollisions[i], nodeBucketCollisionsMax[i],
                nodeCountSum[i], nodeCountTotal[i]);
        }
        Con_FPrintf(CBLF_RULER, "");
    }

    printDistributionOverviewElement(colWidths, "total", 
        nodeBucketEmptyTotal, nodeBucketHeight,
        nodeBucketCollisionsTotal, collisionsMax,
        countSum / PATHDIRECTORY_NODETYPES_COUNT, countTotal);
    }
#undef NUMCOLS
}

void printDistributionHistogram(pathdirectory_t* pd, ushort size,
    size_t nodeCountTotal[PATHDIRECTORY_NODETYPES_COUNT])
{
#define NUMCOLS             4/*range+total+PATHDIRECTORY_NODETYPES_COUNT*/
    assert(NULL != pd);
    {
    size_t totalForRange, total, nodeCount[PATHDIRECTORY_NODETYPES_COUNT];
    int hashIndexDigits, col, colWidths[2+/*range+total*/PATHDIRECTORY_NODETYPES_COUNT];
    pathdirectory_node_t* node;
    int i, j;

    total = 0;
    for(i = 0; i < PATHDIRECTORY_NODETYPES_COUNT; ++i)
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
    for(i = 0; i < PATHDIRECTORY_NODETYPES_COUNT; ++i)
        if(nodeCountTotal[i] > max)
            max = nodeCountTotal[i];
    colWidths[col++] = MAX_OF(M_NumDigits((int)max), 5/*total*/);
    }

    { int i;
    for(i = 0; i < PATHDIRECTORY_NODETYPES_COUNT; ++i, ++col)
        colWidths[col] = Str_Length(PathDirectory_NodeTypeName(i));
    }

    // Apply formatting:
    for(i = 1; i < NUMCOLS; ++i) { colWidths[i] += 1; }

    Con_FPrintf(CBLF_YELLOW, "Histogram (p:%p):\n", pd);
    // Print heading:
    col = 0;
    Con_Printf("%*s", colWidths[col++], "range");
    Con_Printf("%*s", colWidths[col++], "total");
    { int i;
    for(i = 0; i < PATHDIRECTORY_NODETYPES_COUNT; ++i)
        Con_Printf("%*s", colWidths[col++], Str_Text(PathDirectory_NodeTypeName(i)));
    }
    Con_Printf("\n");
    Con_FPrintf(CBLF_RULER, "");

    { ushort i, from = 0, n = 0, range = (size != 0? PATHDIRECTORY_PATHHASH_SIZE / size: 0);
    memset(nodeCount, 0, sizeof(nodeCount));

    for(i = 0; i < PATHDIRECTORY_PATHHASH_SIZE; ++i)
    {
        if(NULL != pd->_pathHash)
        {
            for(node = (*pd->_pathHash)[i]; NULL != node; node = node->next)
                ++nodeCount[PathDirectoryNode_Type(node) - PATHDIRECTORY_NODETYPES_FIRST];
        }

        if(size != 0 && (++n != range && i != PATHDIRECTORY_PATHHASH_SIZE-1))
            continue;

        totalForRange = 0;
        for(j = 0; j < PATHDIRECTORY_NODETYPES_COUNT; ++j)
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
            for(j = 0; j < PATHDIRECTORY_NODETYPES_COUNT; ++j, ++col)
            {
                if(0 != nodeCount[j])
                {
                    Con_Printf("%*lu", colWidths[col], (unsigned long) nodeCount[j]);
                }
                else if(j < PATHDIRECTORY_NODETYPES_COUNT-1 || 0 == size)
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
    Con_FPrintf(CBLF_RULER, "");

    // Sums:
    col = 0;
    Con_Printf("%*s",  colWidths[col++], "Sum");
    Con_Printf("%*lu", colWidths[col++], (unsigned long) total);
    if(0 != total)
    {
        int i;
        for(i = 0; i < PATHDIRECTORY_NODETYPES_COUNT; ++i, ++col)
        {
            if(0 != nodeCountTotal[i])
            {
                Con_Printf("%*lu", colWidths[col], (unsigned long) nodeCountTotal[i]);
            }
            else if(i < PATHDIRECTORY_NODETYPES_COUNT-1)
            {
                Con_Printf("%*s", colWidths[col], "");
            }
        }
    }
    Con_Printf("\n");
    }
#undef NUMCOLS
}

void PathDirectory_PrintHashDistribution(pathdirectory_t* pd)
{
    assert(NULL != pd);
    {
    size_t nodeCountSum[PATHDIRECTORY_NODETYPES_COUNT],
           nodeCountTotal[PATHDIRECTORY_NODETYPES_COUNT], nodeBucketHeight = 0,
           nodeBucketCollisions[PATHDIRECTORY_NODETYPES_COUNT], nodeBucketCollisionsTotal = 0,
           nodeBucketCollisionsMax[PATHDIRECTORY_NODETYPES_COUNT], nodeBucketCollisionsMaxTotal = 0,
           nodeBucketEmpty[PATHDIRECTORY_NODETYPES_COUNT], nodeBucketEmptyTotal = 0,
           nodeCount[PATHDIRECTORY_NODETYPES_COUNT];
    size_t totalForRange;
    pathdirectory_node_t* node;
    int j;

    memset(nodeCountTotal, 0, sizeof(nodeCountTotal));
    if(NULL != pd->_pathHash)
    {
        ushort i;
        for(i = 0; i < PATHDIRECTORY_PATHHASH_SIZE; ++i)
            for(node = (*pd->_pathHash)[i]; NULL != node; node = node->next)
                ++nodeCountTotal[PathDirectoryNode_Type(node) - PATHDIRECTORY_NODETYPES_FIRST];
    }

    memset(nodeCountSum, 0, sizeof(nodeCountSum));
    memset(nodeBucketCollisions, 0, sizeof(nodeBucketCollisions));
    memset(nodeBucketCollisionsMax, 0, sizeof(nodeBucketCollisionsMax));
    memset(nodeBucketEmpty, 0, sizeof(nodeBucketEmpty));

    { ushort i;
    for(i = 0; i < PATHDIRECTORY_PATHHASH_SIZE; ++i)
    {
        memset(nodeCount, 0, sizeof(nodeCount));
        totalForRange = 0;
        if(NULL != pd->_pathHash)
        {
            size_t chainHeight = 0;
            for(node = (*pd->_pathHash)[i]; NULL != node; node = node->next)
            {
                size_t height = 0;
                if(PT_LEAF == PathDirectoryNode_Type(node))
                {
                    pathdirectory_node_t* other = node;
                    while(NULL != (other = PathDirectoryNode_Parent(other)))
                    { ++height; }

                    if(height > chainHeight)
                        chainHeight = height;
                }
                ++nodeCount[PathDirectoryNode_Type(node) - PATHDIRECTORY_NODETYPES_FIRST];
            }

            if(chainHeight > nodeBucketHeight)
                nodeBucketHeight = chainHeight;

            for(j = 0; j < PATHDIRECTORY_NODETYPES_COUNT; ++j)
            {
                totalForRange += nodeCount[j];
                nodeCountSum[j] += nodeCount[j];
            }
        }

        for(j = 0; j < PATHDIRECTORY_NODETYPES_COUNT; ++j)
        {
            if(nodeCount[j] != 0)
            {
                if(nodeCount[j] > 1)
                    nodeBucketCollisions[j] += nodeCount[j]-1;
            }
            else
            {
                ++nodeBucketEmpty[j];
            }
            if(nodeCount[j] > nodeBucketCollisionsMax[j])
                nodeBucketCollisionsMax[j] = nodeCount[j];
        }

        { size_t max = 0;
        for(j = 0; j < PATHDIRECTORY_NODETYPES_COUNT; ++j)
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
}

static pathdirectory_node_t* PathDirectoryNode_Construct(pathdirectory_t* directory,
    pathdirectory_nodetype_t type, pathdirectory_node_t* parent,
    stringpool_internid_t internId, void* userData)
{
    assert(NULL != directory);
    {
    pathdirectory_node_t* node = (pathdirectory_node_t*) malloc(sizeof(*node));
    if(NULL == node)
        Con_Error("PathDirectory::direcNode: Failed on allocation of %lu bytes for "
            "new PathDirectory::Node.", (unsigned long) sizeof(*node));

    node->_directory = directory;
    node->_type = type;
    node->_parent = parent;
    node->_pair.internId = internId;
    node->_pair.data = userData;
    return node;
    }
}

static void PathDirectoryNode_Destruct(pathdirectory_node_t* node)
{
    assert(NULL != node);
    free(node);
}

pathdirectory_t* PathDirectoryNode_Directory(const struct pathdirectory_node_s* node)
{
    assert(NULL != node);
    return node->_directory;
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

stringpool_internid_t PathDirectoryNode_InternId(const pathdirectory_node_t* node)
{
    assert(NULL != node);
    return node->_pair.internId;
}

boolean PathDirectoryNode_MatchDirectory(const pathdirectory_node_t* node, pathdirectory_search_t* s)
{
    assert(NULL != node && NULL != s);
    {
    pathdirectory_t* pd = PathDirectoryNode_Directory(node);
    pathdirectory_fragmentinfo_t* info;
    const ddstring_t* fragment;
    size_t i;

    if(NULL == s->info || 0 == s->fragments)
        return false;

    if(((s->flags & PCF_NO_LEAF)   && PT_LEAF   == PathDirectoryNode_Type(node)) ||
       ((s->flags & PCF_NO_BRANCH) && PT_BRANCH == PathDirectoryNode_Type(node)))
        return false;

    // In reverse order, compare path s.fragments in the search term.
    info = s->info;
    for(i = 0; i < s->fragments; ++i)
    {
        // Is it time to compute the hash for this path fragment?
        if(info->hash == PATHDIRECTORY_PATHHASH_SIZE)
        {
            info->hash = hashName(info->from, (info->to - info->from) + 1, s->delimiter);
        }

        // If the hashes don't match it can't possibly be this.
        if(info->hash != hashForInternId(pd, PathDirectoryNode_InternId(node)))
            return false;

        fragment = PathDirectory_GetFragment(pd, node);
        if(Str_Length(fragment) < (info->to - info->from)+1 ||
           strnicmp(Str_Text(fragment), info->from, Str_Length(fragment)))
            return false;

        // Have we arrived at the search target?
        if(i == s->fragments-1)
            return (!(s->flags & PCF_MATCH_FULL) || NULL == PathDirectoryNode_Parent(node));

        // Are there no more parent directories?
        if(NULL == PathDirectoryNode_Parent(node))
            return false;

        // So far so good. Move one directory level upwards.
        node = PathDirectoryNode_Parent(node);
        info++;
    }

    return false;
    }
}

void PathDirectoryNode_AttachUserData(pathdirectory_node_t* node, void* data)
{
    assert(NULL != node);
#if _DEBUG
    if(NULL != node->_pair.data)
    {
        Con_Message("Warning:PathDirectoryNode::AttachUserData: Data is already associated "
            "with this node, will be replaced.");
    }
#endif
    node->_pair.data = data;
}

void* PathDirectoryNode_DetachUserData(pathdirectory_node_t* node)
{
    assert(NULL != node);
    {
    void* data = node->_pair.data;
    node->_pair.data = NULL;
    return data;
    }
}

void* PathDirectoryNode_UserData(const pathdirectory_node_t* node)
{
    assert(NULL != node);
    return node->_pair.data;
}
