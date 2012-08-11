/**
 * @file pathdirectory.cpp
 * @ingroup base
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include <ctype.h>
//#include "de_console.h"

#include <de/Error>
#include <de/Log>
#include <de/stringpool.h>
#include <de/memory.h>
#include "blockset.h"
#include "m_misc.h" // For M_NumDigits()
#if 0
#include "de_system.h"
#endif

#include "pathdirectory.h"

#if 0
static volatile uint pathDirectoryInstanceCount;

/// A mutex is used to prevent Data races in the node allocator.
static mutex_t nodeAllocator_Mutex;

/// Threaded access to the following Data is protected by nodeAllocator_Mutex:
/// Nodes are block-allocated from this set.
static blockset_t* NodeBlockSet;
/// Linked list of used directory nodes for re-use. Linked with de::PathDirectoryNode::next
static de::PathDirectoryNode* UsedNodes;
#endif

#ifdef LIBDENG_STACK_MONITOR
static void* stackStart;
static size_t maxStackDepth;
#endif

typedef struct pathconstructorparams_s {
    de::PathDirectory* pd;
    size_t length;
    Str* dest;
    char delimiter;
    size_t delimiterLen;
} pathconstructorparams_t;

/**
 * Recursive path constructor. First finds the root and the full length of the
 * path (when descending), then allocates memory for the string, and finally
 * copies each fragment with the delimiters (on the way out).
 */
static void pathConstructor(pathconstructorparams_t* parm, const de::PathDirectoryNode* trav)
{
    DENG2_ASSERT(parm);

    const Str* fragment = parm->pd->pathFragment(trav);

#ifdef LIBDENG_STACK_MONITOR
    maxStackDepth = MAX_OF(maxStackDepth, stackStart - (void*)&fragment);
#endif

    parm->length += Str_Length(fragment);

    if(trav->parent())
    {
        // There also needs to be a separator.
        parm->length += parm->delimiterLen;

        // Descend to parent level.
        pathConstructor(parm, trav->parent());

        // Append the separator.
        if(parm->delimiter)
            Str_AppendCharWithoutAllocs(parm->dest, parm->delimiter);
    }
    else
    {
        // We've arrived at the deepest level. The full length is now known.
        // Ensure there's enough memory for the string.
        Str_ReserveNotPreserving(parm->dest, parm->length);
    }

    // Assemble the path by appending the fragment.
    Str_AppendWithoutAllocs(parm->dest, fragment);
}

struct de::PathDirectory::Instance
{
    de::PathDirectory* self;
    /// Path name fragment intern pool.
    StringPool* stringPool;

    int flags; /// @see pathDirectoryFlags

    /// Path node hashes.
    de::PathDirectory::PathNodes* pathLeafHash;
    de::PathDirectory::PathNodes* pathBranchHash;

    /// Total number of unique paths in the directory.
    uint size;

    Instance(de::PathDirectory* d, int flags_)
        : self(d), stringPool(0), flags(flags_), pathLeafHash(0), pathBranchHash(0), size(0)
    {
#if 0
        // We'll block-allocate nodes and maintain a list of unused ones
        // to accelerate directory construction/population.
        if(!nodeAllocator_Mutex)
        {
            nodeAllocator_Mutex = Sys_CreateMutex("PathDirectoryNodeAllocator_MUTEX");

            Sys_Lock(nodeAllocator_Mutex);
            NodeBlockSet = BlockSet_New(sizeof(de::PathDirectoryNode), 128);
            UsedNodes = NULL;
            Sys_Unlock(nodeAllocator_Mutex);
        }

        pathDirectoryInstanceCount += 1;
#endif
    }

    ~Instance()
    {
        if(pathLeafHash)
        {
            delete pathLeafHash;
        }
        if(pathBranchHash)
        {
            delete pathBranchHash;
        }

#if 0
        if(--pathDirectoryInstanceCount == 0)
        {
            Sys_Lock(nodeAllocator_Mutex);
            BlockSet_Delete(NodeBlockSet);
            NodeBlockSet = NULL;
            UsedNodes = NULL;
            Sys_DestroyMutex(nodeAllocator_Mutex);
            nodeAllocator_Mutex = 0;
        }
#endif
    }

    void clearInternPool()
    {
        if(stringPool)
        {
            StringPool_Delete(stringPool);
            stringPool = NULL;
        }
    }

    de::PathDirectoryNode* findNode(de::PathDirectoryNode* parent,
        PathDirectoryNodeType nodeType, StringPoolId internId)
    {
        de::PathDirectory::PathNodes* ph = (nodeType == PT_LEAF? pathLeafHash : pathBranchHash);
        if(ph)
        {
            ushort hash = StringPool_UserValue(stringPool, internId);
            de::PathDirectory::PathNodes::const_iterator i = ph->find(hash);
            while(i != ph->end() && i.key() == hash)
            {
                if(parent == (*i)->parent() && internId == (*i)->internId())
                {
                    return *i;
                }
                ++i;
            }
        }
        return 0; // Not found.
    }

    StringPoolId internNameAndUpdateIdHashMap(const ddstring_t* name, ushort hash)
    {
        if(!stringPool)
        {
            stringPool = StringPool_New();
        }

        StringPoolId internId = StringPool_Intern(stringPool, name);
        StringPool_SetUserValue(stringPool, internId, hash);
        return internId;
    }

    /**
     * @return  [ a new | the ] directory node that matches the name and type and
     * which has the specified parent node.
     */
    de::PathDirectoryNode* direcNode(de::PathDirectoryNode* parent,
        PathDirectoryNodeType nodeType, const ddstring_t* name, char delimiter,
        void* userData)
    {
        DENG2_ASSERT(name);

        // Have we already encountered this?
        StringPoolId internId = 0;
        if(stringPool)
        {
            internId = StringPool_IsInterned(stringPool, name);
            if(internId)
            {
                // The name is known. Perhaps we have.
                de::PathDirectoryNode* node = findNode(parent, nodeType, internId);
                if(node)
                {
                    if(nodeType == PT_BRANCH || !(flags & PDF_ALLOW_DUPLICATE_LEAF))
                        return node;
                }
            }
        }

        /**
         * A new node is needed.
         */

        // Do we need a new name identifier (and hash)?
        ushort hash;
        if(!internId)
        {
            hash = hashPathFragment(Str_Text(name), Str_Length(name), delimiter);
            internId = internNameAndUpdateIdHashMap(name, hash);
        }
        else
        {
            hash = self->hashForInternId(internId);
        }

        // Are we out of name indices?
        if(!internId) return NULL;

        de::PathDirectoryNode* node = newNode(self, nodeType, parent, internId, userData);

        // Insert the new node into the path hash.
        if(nodeType == PT_LEAF)
        {
            // Do we need to init the path hash?
            if(!pathLeafHash)
            {
                pathLeafHash = new PathNodes;
            }
            pathLeafHash->insert(hash, node);
        }
        else // PT_BRANCH
        {
            // Do we need to init the path hash?
            if(!pathBranchHash)
            {
                pathBranchHash = new PathNodes;
            }
            pathBranchHash->insert(hash, node);
        }

        return node;
    }

    /**
     * The path is split into as many nodes as necessary. Parent links are set.
     *
     * @return  The node that identifies the given path.
     */
    de::PathDirectoryNode* buildDirecNodes(const char* path, char delimiter)
    {
        DENG2_ASSERT(path);

        de::PathDirectoryNode* node = NULL, *parent = NULL;

        // Continue splitting as long as there are parts.
        AutoStr* part = AutoStr_NewStd();
        const char* p = path;
        while((p = Str_CopyDelim2(part, p, delimiter, CDF_OMIT_DELIMITER))) // Get the next part.
        {
            node = direcNode(parent, PT_BRANCH, part, delimiter, NULL);
            /// @todo Do not error here. If we're out of storage undo this action and return.
            if(!node)
            {
                throw de::Error("PathDirectory::buildDirecNodes",
                                de::String("Exhausted storage while attempting to insert nodes for path \"%1\".")
                                    .arg(path));
            }
            parent = node;
        }

        if(!Str_IsEmpty(part))
        {
            node = direcNode(parent, PT_LEAF, part, delimiter, NULL);
            /// @todo Do not error here. If we're out of storage undo this action and return.
            if(!node)
            {
                throw de::Error("PathDirectory::buildDirecNodes",
                                de::String("Exhausted storage while attempting to insert nodes for path \"%1\".")
                                    .arg(path));
            }
        }

        return node;
    }

    /**
     * @param node              Node whose path to construct.
     * @param constructedPath   The constructed path is written here. Previous contents discarded.
     * @param delimiter         Character to use for separating fragments.
     *
     * @return @a constructedPath
     *
     * @todo This is a good candidate for result caching: the constructed path
     * could be saved and returned on subsequent calls. Are there any circumstances
     * in which the cached result becomes obsolete? -jk
     *
     * The only times the result becomes obsolete is when the delimiter is changed
     * or when the directory itself is rebuilt (in which case the nodes themselves
     * will be free'd). Note that any caching mechanism should not counteract one
     * of the primary goals of this class, i.e., optimal memory usage for the whole
     * directory. Caching constructed paths for every root node in the directory
     * would completely negate the benefits of the design of this class.
     *
     * Perhaps a fixed size MRU cache? -ds
     */
    ddstring_t* constructPath(const de::PathDirectoryNode* node,
                              ddstring_t* constructedPath, char delimiter)
    {
        pathconstructorparams_t parm;

#ifdef LIBDENG_STACK_MONITOR
        stackStart = &parm;
#endif

        DENG2_ASSERT(node && constructedPath);

        parm.dest = constructedPath;
        parm.length = 0;
        parm.pd = self;
        parm.delimiter = delimiter;
        parm.delimiterLen = (delimiter? 1 : 0);

        // Include a terminating path separator for branches (directories).
        if(node->type() == PT_BRANCH)
            parm.length += parm.delimiterLen;

        // Recursively construct the path from fragments and delimiters.
        Str_Clear(constructedPath);
        pathConstructor(&parm, node);

        // Terminating delimiter for branches.
        if(delimiter && node->type() == PT_BRANCH)
            Str_AppendCharWithoutAllocs(constructedPath, delimiter);

        DENG2_ASSERT(Str_Length(constructedPath) == parm.length);

#ifdef LIBDENG_STACK_MONITOR
        LOG_AS("pathConstructor");
        LOG_INFO("Max stack depth: %1 bytes") << maxStackDepth;
#endif

        return constructedPath;
    }

    static de::PathDirectoryNode*
    newNode(de::PathDirectory* directory, PathDirectoryNodeType type,
            de::PathDirectoryNode* parent, StringPoolId internId, void* userData)
    {
        de::PathDirectoryNode* node;

        DENG2_ASSERT(directory);

#if 0
        // Acquire a new node, either from the used list or the block allocator.
        Sys_Lock(nodeAllocator_Mutex);
        if(UsedNodes)
        {
            node = UsedNodes;
            UsedNodes = node->next;

            // Reconfigure the node.
            node->next = NULL;
            node->directory_ = directory;
            node->type_ = type;
            node->parent_ = parent;
            node->pair.internId = internId;
            node->pair.data = userData;
        }
        else
#endif
        {
            //void* element = BlockSet_Allocate(NodeBlockSet);
            node = /*new (element)*/ new de::PathDirectoryNode(*directory, type, internId,
                                                               parent, userData);
        }
#if 0
        Sys_Unlock(nodeAllocator_Mutex);
#endif

        return node;
    }

    static void collectPathsInHash(de::PathDirectory::PathNodes& ph, char delimiter,
        ddstring_t** pathListAdr)
    {
        DENG2_FOR_EACH(i, ph, de::PathDirectory::PathNodes::const_iterator)
        {
            Str_Init(*pathListAdr);
            (*i)->directory()->composePath(*i, (*pathListAdr), NULL, delimiter);
            (*pathListAdr)++;
        }
    }

    static void clearPathHash(de::PathDirectory::PathNodes& ph)
    {
        DENG2_FOR_EACH(i, ph, de::PathDirectory::PathNodes::iterator)
        {
#if _DEBUG
            if((*i)->userData())
            {
                LOG_AS("PathDirectory::clearPathHash");
                LOG_ERROR("Node %p has non-NULL user data.") << (void*)(*i);
            }
#endif
            delete (*i);
        }
        ph.clear();
    }
};

ushort de::PathDirectory::hashPathFragment(const char* fragment, size_t len, char delimiter)
{
    ushort key = 0;

    DENG2_ASSERT(fragment);

    // Skip over any trailing delimiters.
    const char* c = fragment + len - 1;
    while(c >= fragment && *c && *c == delimiter) c--;

    // Compose the hash.
    int op = 0;
    for(; c >= fragment && *c && *c != delimiter; c--)
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

ushort de::PathDirectory::hashForInternId(StringPoolId internId)
{
    DENG2_ASSERT(internId > 0);
    return StringPool_UserValue(d->stringPool, internId);
}

de::PathDirectoryNode*
de::PathDirectory::insert(const char* path, char delimiter, void* userData)
{
    de::PathDirectoryNode* node = d->buildDirecNodes(path, delimiter);
    if(node)
    {
        // There is now one more unique path in the directory.
        d->size += 1;

        if(userData)
        {
            node->setUserData(userData);
        }
    }
    return node;
}

de::PathDirectory::PathDirectory(int flags)
{
    d = new Instance(this, flags);
}

de::PathDirectory::~PathDirectory()
{
    clear();
    delete d;
}

uint de::PathDirectory::size() const
{
    return d->size;
}

void de::PathDirectory::clear()
{
    if(d->pathLeafHash)
    {
        d->clearPathHash(*d->pathLeafHash);
    }
    if(d->pathBranchHash)
    {
        d->clearPathHash(*d->pathBranchHash);
    }
    d->clearInternPool();
    d->size = 0;
}

de::PathDirectoryNode* de::PathDirectory::find(int flags,
    const char* searchPath, char delimiter)
{
    PathDirectoryNode* foundNode = NULL;
    if(searchPath && searchPath[0] && d->size)
    {
        PathMap mappedSearchPath;
        PathMap_Initialize2(&mappedSearchPath, PathDirectory_HashPathFragment2, searchPath, delimiter);

        ushort hash = PathMap_Fragment(&mappedSearchPath, 0)->hash;
        if(!(flags & PCF_NO_LEAF) && d->pathLeafHash)
        {
            de::PathDirectory::PathNodes* nodes = d->pathLeafHash;
            de::PathDirectory::PathNodes::iterator i = nodes->find(hash);
            for(; i != nodes->end() && i.key() == hash; ++i)
            {
                if((*i)->matchDirectory(flags, &mappedSearchPath))
                {
                    // This is the node we're looking for - stop iteration.
                    foundNode = *i;
                    break;
                }
            }
        }

        if(!foundNode)
        if(!(flags & PCF_NO_BRANCH) && d->pathBranchHash)
        {
            de::PathDirectory::PathNodes* nodes = d->pathBranchHash;
            de::PathDirectory::PathNodes::iterator i = nodes->find(hash);
            for(; i != nodes->end() && i.key() == hash; ++i)
            {
                if((*i)->matchDirectory(flags, &mappedSearchPath))
                {
                    // This is the node we're looking for - stop iteration.
                    foundNode = *i;
                    break;
                }
            }
        }

        PathMap_Destroy(&mappedSearchPath);
    }
    return foundNode;
}

/**
 * C wrapper API:
 */

#define D_TOINTERNAL(inst) \
    (inst) != 0? reinterpret_cast<de::PathDirectory*>(inst) : NULL

#define D_TOINTERNAL_CONST(inst) \
    (inst) != 0? reinterpret_cast<const de::PathDirectory*>(inst) : NULL

#define D_SELF(inst) \
    DENG2_ASSERT(inst); \
    de::PathDirectory* self = D_TOINTERNAL(inst)

#define D_SELF_CONST(inst) \
    DENG2_ASSERT(inst); \
    const de::PathDirectory* self = D_TOINTERNAL_CONST(inst)

PathDirectory* PathDirectory_NewWithFlags(int flags)
{
    return reinterpret_cast<PathDirectory*>(new de::PathDirectory(flags));
}

PathDirectory* PathDirectory_New(void)
{
    return reinterpret_cast<PathDirectory*>(new de::PathDirectory());
}

void PathDirectory_Delete(PathDirectory* pd)
{
    if(pd)
    {
        D_SELF(pd);
        delete self;
    }
}

uint PathDirectory_Size(PathDirectory* pd)
{
    D_SELF(pd);
    return self->size();
}

void PathDirectory_Clear(PathDirectory* pd)
{
    D_SELF(pd);
    self->clear();
}

PathDirectoryNode* PathDirectory_Insert3(PathDirectory* pd, const char* path, char delimiter, void* userData)
{
    D_SELF(pd);
    return reinterpret_cast<PathDirectoryNode*>(self->insert(path, delimiter, userData));
}

PathDirectoryNode* PathDirectory_Insert2(PathDirectory* pd, const char* path, char delimiter)
{
    D_SELF(pd);
    return reinterpret_cast<PathDirectoryNode*>(self->insert(path, delimiter));
}

PathDirectoryNode* PathDirectory_Insert(PathDirectory* pd, const char* path)
{
    D_SELF(pd);
    return reinterpret_cast<PathDirectoryNode*>(self->insert(path));
}

static int iteratePathsInHash(PathDirectory* pd,
    ushort hash, PathDirectoryNodeType type, int flags, PathDirectoryNode* parent_,
    int (*callback) (PathDirectoryNode* node, void* parameters), void* parameters)
{
    int result = 0;

    D_SELF(pd);

    if(hash != PATHDIRECTORY_NOHASH && hash >= PATHDIRECTORY_PATHHASH_SIZE)
    {
        throw de::Error("iteratePathsInHash",
                        de::String("Invalid hash %1 (valid range is [0..%2]).")
                            .arg(hash).arg(PATHDIRECTORY_PATHHASH_SIZE-1));
    }

    const de::PathDirectory::PathNodes* nodes = self->pathNodes(type);
    if(nodes)
    {
        de::PathDirectoryNode* parent = reinterpret_cast<de::PathDirectoryNode*>(parent_);

        // Are we iterating nodes with a known hash?
        if(hash != PATHDIRECTORY_NOHASH)
        {
            // Yes.
            de::PathDirectory::PathNodes::const_iterator i = nodes->constFind(hash);
            for(; i != nodes->end() && i.key() == hash; ++i)
            {
                if(!((flags & PCF_MATCH_PARENT) && parent != (*i)->parent()))
                {
                    result = callback(reinterpret_cast<PathDirectoryNode*>(*i), parameters);
                    if(result) break;
                }
            }
        }
        else
        {
            // No - iterate all nodes.
            DENG2_FOR_EACH(i, *nodes, de::PathDirectory::PathNodes::const_iterator)
            {
                if(!((flags & PCF_MATCH_PARENT) && parent != (*i)->parent()))
                {
                    result = callback(reinterpret_cast<PathDirectoryNode*>(*i), parameters);
                    if(result) break;
                }
            }
        }
    }
    return result;
}

static int iteratePathsInHash_Const(const PathDirectory* pd,
    ushort hash, PathDirectoryNodeType type, int flags, const PathDirectoryNode* parent_,
    int (*callback) (const PathDirectoryNode* node, void* parameters), void* parameters)
{
    int result = 0;

    D_SELF_CONST(pd);

    if(hash != PATHDIRECTORY_NOHASH && hash >= PATHDIRECTORY_PATHHASH_SIZE)
    {
        throw de::Error("iteratePathsInHash_Const",
                        de::String("Invalid hash %1 (valid range is [0..%2]).")
                            .arg(hash).arg(PATHDIRECTORY_PATHHASH_SIZE-1));
    }

    const de::PathDirectory::PathNodes* nodes = self->pathNodes(type);
    if(nodes)
    {
        const de::PathDirectoryNode* parent = reinterpret_cast<const de::PathDirectoryNode*>(parent_);

        // Are we iterating nodes with a known hash?
        if(hash != PATHDIRECTORY_NOHASH)
        {
            // Yes.
            de::PathDirectory::PathNodes::const_iterator i = nodes->find(hash);
            for(; i != nodes->end() && i.key() == hash; ++i)
            {
                if(!((flags & PCF_MATCH_PARENT) && parent != (*i)->parent()))
                {
                    result = callback(reinterpret_cast<const PathDirectoryNode*>(*i), parameters);
                    if(result) break;
                }
            }
        }
        else
        {
            // No - iterate all nodes.
            DENG2_FOR_EACH(i, *nodes, de::PathDirectory::PathNodes::const_iterator)
            {
                if(!((flags & PCF_MATCH_PARENT) && parent != (*i)->parent()))
                {
                    result = callback(reinterpret_cast<const PathDirectoryNode*>(*i), parameters);
                    if(result) break;
                }
            }
        }
    }
    return result;
}

int PathDirectory_Iterate2(PathDirectory* pd, int flags, PathDirectoryNode* parent,
    ushort hash, pathdirectory_iteratecallback_t callback, void* parameters)
{
    int result = 0;
    if(callback)
    {
        if(!(flags & PCF_NO_LEAF))
            result = iteratePathsInHash(pd, hash, PT_LEAF, flags, parent,
                                        callback, parameters);

        if(!result && !(flags & PCF_NO_BRANCH))
            result = iteratePathsInHash(pd, hash, PT_BRANCH, flags, parent,
                                        callback, parameters);
    }
    return result;
}

int PathDirectory_Iterate(PathDirectory* pd, int flags, PathDirectoryNode* parent,
    ushort hash, pathdirectory_iteratecallback_t callback)
{
    return PathDirectory_Iterate2(pd, flags, parent, hash, callback, NULL);
}

int PathDirectory_Iterate2_Const(const PathDirectory* pd, int flags, const PathDirectoryNode* parent,
    ushort hash, pathdirectory_iterateconstcallback_t callback, void* parameters)
{
    int result = 0;
    if(callback)
    {
        if(!(flags & PCF_NO_LEAF))
            result = iteratePathsInHash_Const(pd, hash, PT_LEAF, flags, parent,
                                              callback, parameters);

        if(!result && !(flags & PCF_NO_BRANCH))
            result = iteratePathsInHash_Const(pd, hash, PT_BRANCH, flags, parent,
                                              callback, parameters);
    }
    return result;
}

int PathDirectory_Iterate_Const(const PathDirectory* pd, int flags, const PathDirectoryNode* parent,
    ushort hash, pathdirectory_iterateconstcallback_t callback)
{
    return PathDirectory_Iterate2_Const(pd, flags, parent, hash, callback, NULL);
}

ddstring_t* PathDirectory_ComposePath2(PathDirectory* pd, const PathDirectoryNode* node,
    ddstring_t* path, int* length, char delimiter)
{
    D_SELF(pd);
    return self->composePath(reinterpret_cast<const de::PathDirectoryNode*>(node), path, length, delimiter);
}

const ddstring_t* PathDirectory_GetFragment(PathDirectory* pd, const PathDirectoryNode* node)
{
    D_SELF(pd);
    return self->pathFragment(reinterpret_cast<const de::PathDirectoryNode*>(node));
}

ddstring_t* PathDirectory_CollectPaths2(PathDirectory* pd, size_t* count, int flags, char delimiter)
{
    D_SELF(pd);
    return self->collectPaths(count, flags, delimiter);
}

ddstring_t* PathDirectory_CollectPaths(PathDirectory* pd, size_t* count, int flags)
{
    D_SELF(pd);
    return self->collectPaths(count, flags);
}

typedef struct {
    int flags; /// @see pathComparisonFlags
    PathMap* mappedSearchPath;
    void* parameters;
    pathdirectory_searchcallback_t callback;
    PathDirectoryNode* foundNode;
} pathdirectorysearchworker_params_t;

static int PathDirectory_SearchWorker(PathDirectoryNode* node, void* parameters)
{
    pathdirectorysearchworker_params_t* p = (pathdirectorysearchworker_params_t*)parameters;
    DENG2_ASSERT(node && parameters);
    if(p->callback(node, p->flags, p->mappedSearchPath, p->parameters))
    {
        p->foundNode = node;
        return 1; // Stop iteration.
    }
    return 0; // Continue iteration.
}

PathDirectoryNode* PathDirectory_Search2(PathDirectory* pd, int flags,
    PathMap* mappedSearchPath, pathdirectory_searchcallback_t callback, void* parameters)
{
    DENG2_ASSERT(pd);
    if(callback)
    {
        pathdirectorysearchworker_params_t p;
        p.flags = flags;
        p.mappedSearchPath = mappedSearchPath;
        p.parameters = parameters;
        p.callback = callback;
        p.foundNode = NULL;

        ushort hash = PathMap_Fragment(mappedSearchPath, 0)->hash;
        if(!(flags & PCF_NO_LEAF))
        {
            if(iteratePathsInHash(pd, hash, PT_LEAF, flags, NULL,
                                  PathDirectory_SearchWorker, (void*)&p))
            {
                return p.foundNode;
            }
        }

        if(!(flags & PCF_NO_BRANCH))
        {
            if(iteratePathsInHash(pd, hash, PT_BRANCH, flags, NULL,
                                  PathDirectory_SearchWorker, (void*)&p))
            {
                return p.foundNode;
            }
        }
    }
    return 0; // Not found.
}

PathDirectoryNode* PathDirectory_Search(PathDirectory* pd, int flags,
    PathMap* mappedSearchPath, pathdirectory_searchcallback_t callback)
{
    return PathDirectory_Search2(pd, flags, mappedSearchPath, callback, NULL);
}

PathDirectoryNode* PathDirectory_Find2(PathDirectory* pd, int flags,
    const char* searchPath, char delimiter)
{
    D_SELF(pd);
    return reinterpret_cast<PathDirectoryNode*>(self->find(flags, searchPath, delimiter));
}

PathDirectoryNode* PathDirectory_Find(PathDirectory* pd, int flags,
    const char* searchPath)
{
    D_SELF(pd);
    return reinterpret_cast<PathDirectoryNode*>(self->find(flags, searchPath));
}

ushort PathDirectory_HashPathFragment2(const char* path, size_t len, char delimiter)
{
    return de::PathDirectory::hashPathFragment(path, len, delimiter);
}

#if _DEBUG
void PathDirectory_DebugPrint(PathDirectory* pd, char delimiter)
{
    if(!pd) return;
    de::PathDirectory::debugPrint(D_TOINTERNAL(pd), delimiter);
}
#endif

const ddstring_t* de::PathDirectory::pathFragment(const de::PathDirectoryNode* node)
{
    DENG2_ASSERT(node);
    return StringPool_String(d->stringPool, node->internId());
}

ddstring_t* de::PathDirectory::composePath(const de::PathDirectoryNode* node,
    ddstring_t* foundPath, int* length, char delimiter)
{
    if(!foundPath)
    {
        if(length) composePath(node, NULL, length, delimiter);
        return 0;
    }
    return d->constructPath(node, foundPath, delimiter);
}

const de::PathDirectory::PathNodes* const
de::PathDirectory::pathNodes(PathDirectoryNodeType type) const
{
    return (type == PT_LEAF? d->pathLeafHash : d->pathBranchHash);
}

ddstring_t* de::PathDirectory::collectPaths(size_t* retCount, int flags, char delimiter)
{
    ddstring_t* paths = NULL;
    size_t count = 0;

    if(!(flags & PCF_NO_LEAF))
        count += (d->pathLeafHash  ? d->pathLeafHash->size()   : 0);
    if(!(flags & PCF_NO_BRANCH))
        count += (d->pathBranchHash? d->pathBranchHash->size() : 0);

    if(count)
    {
        // Uses malloc here because this is returned with the C wrapper.
        paths = static_cast<ddstring_t*>(M_Malloc(sizeof *paths * count));
        if(!paths)
        {
            throw de::Error("PathDirectory::collectPaths:",
                            de::String("Failed on allocation of %1 bytes for new path list.")
                                .arg(sizeof *paths * count));
        }

        ddstring_t* pathPtr = paths;
        if(!(flags & PCF_NO_BRANCH) && d->pathBranchHash)
            Instance::collectPathsInHash(*d->pathBranchHash, delimiter, &pathPtr);

        if(!(flags & PCF_NO_LEAF) && d->pathLeafHash)
            Instance::collectPathsInHash(*d->pathLeafHash, delimiter, &pathPtr);
    }

    if(retCount) *retCount = count;
    return paths;
}

#if _DEBUG
static int C_DECL comparePaths(const void* a, const void* b)
{
    return qstricmp(Str_Text((Str*)a), Str_Text((Str*)b));
}

void de::PathDirectory::debugPrint(de::PathDirectory* pd, char delimiter)
{
    if(!pd) return;

    LOG_AS("PathDirectory");
    LOG_INFO("Directory [%p]:") << (void*)pd;
    size_t numLeafs;
    ddstring_t* pathList = pd->collectPaths(&numLeafs, PT_LEAF, delimiter);
    if(pathList)
    {
        size_t n = 0;
        qsort(pathList, numLeafs, sizeof *pathList, comparePaths);
        do
        {
            LOG_INFO("  %s") << Str_Text(pathList + n);
            Str_Free(pathList + n);
        } while(++n < numLeafs);
        M_Free(pathList);
    }
    LOG_INFO("  %lu %s in directory.") << numLeafs << (numLeafs==1? "path":"paths");
}

static void printDistributionOverviewElement(const int* colWidths, const char* name,
    size_t numEmpty, size_t maxHeight, size_t numCollisions, size_t maxCollisions,
    size_t sum, size_t total)
{
#if 0
    DENG2_ASSERT(colWidths);

    float coverage, collision, variance;
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

    const int* col = colWidths;
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
#endif
}

static void printDistributionOverview(PathDirectory* pd,
    size_t nodeCountSum[PATHDIRECTORYNODE_TYPE_COUNT],
    size_t nodeCountTotal[PATHDIRECTORYNODE_TYPE_COUNT],
    size_t nodeBucketCollisions[PATHDIRECTORYNODE_TYPE_COUNT], size_t nodeBucketCollisionsTotal,
    size_t nodeBucketCollisionsMax[PATHDIRECTORYNODE_TYPE_COUNT], size_t /*nodeBucketCollisionsMaxTotal*/,
    size_t nodeBucketEmpty[PATHDIRECTORYNODE_TYPE_COUNT], size_t nodeBucketEmptyTotal, size_t nodeBucketHeight,
    size_t /*nodeCount*/[PATHDIRECTORYNODE_TYPE_COUNT])
{
#if 0
#define NUMCOLS             10/*type+count+used:+empty+collideMax+collideCount+collidePercent+coverage+variance+maxheight*/

    DENG2_ASSERT(pd);

    size_t collisionsMax = 0, countSum = 0, countTotal = 0;
    for(int i = 0; i < PATHDIRECTORYNODE_TYPE_COUNT; ++i)
    {
        if(nodeBucketCollisionsMax[i] > collisionsMax)
            collisionsMax = nodeBucketCollisionsMax[i];
        countSum += nodeCountSum[i];
        countTotal += nodeCountTotal[i];
    }

    int nodeCountDigits = M_NumDigits((int)countTotal);

    // Calculate minimum field widths:
    int colWidths[NUMCOLS];
    int* col = colWidths;
    *col = 0;
    for(int i = 0; i < PATHDIRECTORYNODE_TYPE_COUNT; ++i)
    {
        PathDirectoryNodeType type = PathDirectoryNodeType(i);
        if(Str_Length(de::PathDirectoryNode::typeName(type)) > *col)
            *col = Str_Length(de::PathDirectoryNode::typeName(type));
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
    int spans[4][2];
    spans[0][0] = colWidths[0] + 1/* */ + colWidths[1];
    spans[1][0] = colWidths[2] + 1/*:*/ + colWidths[3];
    spans[2][0] = colWidths[4] + 1/* */ + colWidths[5] + 1/* */ + colWidths[6];
    spans[3][0] = colWidths[7] + 1/* */ + colWidths[8] + 1/* */ + colWidths[9];
    for(int i = 0; i < 4; ++i)
    {
        int remainder = spans[i][0] % 2;
        spans[i][1] = remainder + (spans[i][0] /= 2);
    }

    Con_FPrintf(CPF_YELLOW, "Directory Distribution (p:%p):\n", pd);

    // Level1 headings:
    int* span = &spans[0][0];
    Con_Printf("%*s", *span++ +  5/2, "nodes");         Con_Printf("%-*s|", *span++ -  5/2, "");
    Con_Printf("%*s", *span++ +  4/2, "hash");          Con_Printf("%-*s|", *span++ -  4/2, "");
    Con_Printf("%*s", *span++ + 10/2, "collisions");    Con_Printf("%-*s|", *span++ - 10/2, "");
    Con_Printf("%*s", *span++ +  5/2, "other");         Con_Printf("%-*s\n",*span++ -  5/2, "");

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
        for(int i = 0; i < PATHDIRECTORYNODE_TYPE_COUNT; ++i)
        {
            PathDirectoryNodeType type = PathDirectoryNodeType(i);
            printDistributionOverviewElement(colWidths, Str_Text(de::PathDirectoryNode::typeName(type)),
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
#endif
}

static void printDistributionHistogram(PathDirectory* pd, ushort size,
    size_t nodeCountTotal[PATHDIRECTORYNODE_TYPE_COUNT])
{
#if 0
#define NUMCOLS             4/*range+total+PATHDIRECTORYNODE_TYPE_COUNT*/

    size_t totalForRange, total, nodeCount[PATHDIRECTORYNODE_TYPE_COUNT];
    int hashIndexDigits, col, colWidths[2+/*range+total*/PATHDIRECTORYNODE_TYPE_COUNT];
    PathDirectoryNode* node;
    int j;
    DENG2_ASSERT(pd);

    total = 0;
    for(int i = 0; i < PATHDIRECTORYNODE_TYPE_COUNT; ++i)
    {
        total += nodeCountTotal[i];
    }
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
    for(int i = 0; i < PATHDIRECTORYNODE_TYPE_COUNT; ++i)
    {
        if(nodeCountTotal[i] > max)
            max = nodeCountTotal[i];
    }
    colWidths[col++] = MAX_OF(M_NumDigits((int)max), 5/*total*/);
    }

    for(int i = 0; i < PATHDIRECTORYNODE_TYPE_COUNT; ++i, ++col)
    {
        PathDirectoryNodeType type = PathDirectoryNodeType(i);
        colWidths[col] = Str_Length(de::PathDirectoryNode::typeName(type));
    }

    // Apply formatting:
    for(int i = 1; i < NUMCOLS; ++i) { colWidths[i] += 1; }

    Con_FPrintf(CPF_YELLOW, "Histogram (p:%p):\n", pd);
    // Print heading:
    col = 0;
    Con_Printf("%*s", colWidths[col++], "range");
    Con_Printf("%*s", colWidths[col++], "total");
    for(int i = 0; i < PATHDIRECTORYNODE_TYPE_COUNT; ++i)
    {
        PathDirectoryNodeType type = PathDirectoryNodeType(i);
        Con_Printf("%*s", colWidths[col++], Str_Text(de::PathDirectoryNode::typeName(type)));
    }
    Con_Printf("\n");
    Con_PrintRuler();

    { ushort from = 0, n = 0, range = (size != 0? PATHDIRECTORY_PATHHASH_SIZE / size: 0);
    memset(nodeCount, 0, sizeof(nodeCount));

    for(ushort i = 0; i < PATHDIRECTORY_PATHHASH_SIZE; ++i)
    {
        pathdirectory_pathhash_t** phAdr;
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
            Str range; Str_Init(&range);
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
#endif
}

void de::PathDirectory::debugPrintHashDistribution(de::PathDirectory* pd)
{
    if(!pd) return;

#if 0
    size_t nodeCountSum[PATHDIRECTORYNODE_TYPE_COUNT],
           nodeCountTotal[PATHDIRECTORYNODE_TYPE_COUNT], nodeBucketHeight = 0,
           nodeBucketCollisions[PATHDIRECTORYNODE_TYPE_COUNT], nodeBucketCollisionsTotal = 0,
           nodeBucketCollisionsMax[PATHDIRECTORYNODE_TYPE_COUNT], nodeBucketCollisionsMaxTotal = 0,
           nodeBucketEmpty[PATHDIRECTORYNODE_TYPE_COUNT], nodeBucketEmptyTotal = 0,
           nodeCount[PATHDIRECTORYNODE_TYPE_COUNT];
    size_t totalForRange;
    de::PathDirectoryNode* node;
    DENG2_ASSERT(pd);

    nodeCountTotal[PT_BRANCH] = countNodesInPathHash(*hashAddressForNodeType(pd, PT_BRANCH));
    nodeCountTotal[PT_LEAF]   = countNodesInPathHash(*hashAddressForNodeType(pd, PT_LEAF));

    memset(nodeCountSum, 0, sizeof(nodeCountSum));
    memset(nodeBucketCollisions, 0, sizeof(nodeBucketCollisions));
    memset(nodeBucketCollisionsMax, 0, sizeof(nodeBucketCollisionsMax));
    memset(nodeBucketEmpty, 0, sizeof(nodeBucketEmpty));

    for(ushort i = 0; i < PATHDIRECTORY_PATHHASH_SIZE; ++i)
    {
        pathdirectory_pathhash_t** phAdr;
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

        for(int j = 0; j < PATHDIRECTORYNODE_TYPE_COUNT; ++j)
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

        size_t max = 0;
        for(int j = 0; j < PATHDIRECTORYNODE_TYPE_COUNT; ++j)
        {
            max += nodeCount[j];
        }
        if(max > nodeBucketCollisionsMaxTotal)
            nodeBucketCollisionsMaxTotal = max;

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
    }

    printDistributionOverview(pd, nodeCountSum, nodeCountTotal,
        nodeBucketCollisions,    nodeBucketCollisionsTotal,
        nodeBucketCollisionsMax, nodeBucketCollisionsMaxTotal,
        nodeBucketEmpty, nodeBucketEmptyTotal,
        nodeBucketHeight, nodeCount);
    Con_Printf("\n");
    printDistributionHistogram(pd, 16, nodeCountTotal);
#endif
}

void PathDirectory_DebugPrintHashDistribution(PathDirectory* pd)
{
    de::PathDirectory::debugPrintHashDistribution(D_TOINTERNAL(pd));
}
#endif

struct de::PathDirectoryNode::Instance
{
    de::PathDirectoryNode* self;

    /// Parent node in the user's logical hierarchy.
    PathDirectoryNode* parent;

    /// Symbolic node type.
    PathDirectoryNodeType type;

    /// PathDirectory which owns this node.
    PathDirectory* directory;

    /// User data present at this node.
    typedef struct userdatapair_s {
        StringPoolId internId;
        void* data;
    } userdatapair_t;
    userdatapair_t pair;

    Instance(de::PathDirectoryNode* d, de::PathDirectory& _directory,
             PathDirectoryNodeType _type, StringPoolId internId,
             de::PathDirectoryNode* _parent)
        : self(d), parent(_parent), type(_type), directory(&_directory)
    {
        pair.internId = internId;
        pair.data = 0;
    }
};

de::PathDirectoryNode::PathDirectoryNode(PathDirectory& directory,
    PathDirectoryNodeType type, StringPoolId internId, de::PathDirectoryNode* parent,
    void* userData)
{
    d = new Instance(this, directory, type, internId, parent);
    setUserData(userData);
}

de::PathDirectoryNode::~PathDirectoryNode()
{
    delete d;

#if 0
    // Add this node to the list of used nodes for re-use.
    Sys_Lock(nodeAllocator_Mutex);
    next = UsedNodes;
    UsedNodes = this;
    Sys_Unlock(nodeAllocator_Mutex);
#endif
}

const Str* de::PathDirectoryNode::typeName(PathDirectoryNodeType type)
{
    static const de::Str nodeNames[1+PATHDIRECTORYNODE_TYPE_COUNT] = {
        "(invalidtype)",
        "branch",
        "leaf"
    };
    if(!VALID_PATHDIRECTORYNODE_TYPE(type)) return nodeNames[0];
    return nodeNames[1 + (type - PATHDIRECTORYNODE_TYPE_FIRST)];
}

/// @return  PathDirectory which owns this node.
de::PathDirectory* de::PathDirectoryNode::directory() const
{
    return d->directory;
}

/// @return  Parent of this directory node else @c NULL
de::PathDirectoryNode* de::PathDirectoryNode::parent() const
{
    return d->parent;
}

/// @return  Type of this directory node.
PathDirectoryNodeType de::PathDirectoryNode::type() const
{
    return d->type;
}

StringPoolId de::PathDirectoryNode::internId() const
{
    return d->pair.internId;
}

ushort de::PathDirectoryNode::hash() const
{
    return d->directory->hashForInternId(d->pair.internId);
}

static int matchPathFragment(const char* string, const char* pattern)
{
    const char* in = string, *st = pattern;

    while(*in)
    {
        if(*st == '*')
        {
            st++;
            continue;
        }

        if(*st != '?' && (tolower((unsigned char) *st) != tolower((unsigned char) *in)))
        {
            // A mismatch. Hmm. Go back to a previous '*'.
            while(st >= pattern && *st != '*')
                st--;
            if(st < pattern)
                return false; // No match!
            // The asterisk lets us continue.
        }

        // This character of the pattern is OK.
        st++;
        in++;
    }

    // Match is good if the end of the pattern was reached.
    while(*st == '*')
        st++; // Skip remaining asterisks.

    return *st == 0;
}

/// @note This routine is also used as an iteration callback, so only return
///       a non-zero value when the node is a match for the search term.
int de::PathDirectoryNode::matchDirectory(int flags, PathMap* searchPattern)
{
    if(((flags & PCF_NO_LEAF)   && PT_LEAF   == type()) ||
       ((flags & PCF_NO_BRANCH) && PT_BRANCH == type()))
        return false;

    const PathMapFragment* sfragment = PathMap_Fragment(searchPattern, 0);
    if(!sfragment) return false; // Hmm...

//#ifdef _DEBUG
//#  define EXIT_POINT(ep) fprintf(stderr, "MatchDirectory exit point %i\n", ep)
//#else
#  define EXIT_POINT(ep)
//#endif

    // In reverse order, compare path fragments in the search term.
    PathDirectory* pd = directory();
    uint fragmentCount = PathMap_Size(searchPattern);

    de::PathDirectoryNode* node = this;
    for(uint i = 0; i < fragmentCount; ++i)
    {
        if(i == 0 && node->type() == PT_LEAF)
        {
            char buf[256];
            qsnprintf(buf, 256, "%*s", sfragment->to - sfragment->from + 1, sfragment->from);

            const ddstring_t* fragment = pd->pathFragment(node);
            DENG2_ASSERT(fragment);

            if(!matchPathFragment(Str_Text(fragment), buf))
            {
                EXIT_POINT(1);
                return false;
            }
        }
        else
        {
            const bool isWild = (sfragment->to == sfragment->from && *sfragment->from == '*');
            if(!isWild)
            {
                // If the hashes don't match it can't possibly be this.
                if(sfragment->hash != pd->hashForInternId(node->internId()))
                {
                    EXIT_POINT(2);
                    return false;
                }

                // Determine length of the sfragment.
                int sfraglen;
                if(!strcmp(sfragment->to, "") && !strcmp(sfragment->from, ""))
                    sfraglen = 0;
                else
                    sfraglen = (sfragment->to - sfragment->from) + 1;

                // Compare the path fragment to that of the search term.
                const ddstring_t* fragment = pd->pathFragment(node);
                if(Str_Length(fragment) < sfraglen ||
                   qstrnicmp(Str_Text(fragment), sfragment->from, Str_Length(fragment)))
                {
                    EXIT_POINT(3);
                    return false;
                }
            }
        }

        // Have we arrived at the search target?
        if(i == fragmentCount-1)
        {
            EXIT_POINT(4);
            return (!(flags & PCF_MATCH_FULL) || !node->parent());
        }

        // Are there no more parent directories?
        if(!node->parent())
        {
            EXIT_POINT(5);
            return false;
        }

        // So far so good. Move one directory level upwards.
        node = node->parent();
        sfragment = PathMap_Fragment(searchPattern, i+1);
    }
    EXIT_POINT(6);
    return false;

#undef EXIT_POINT
}

de::PathDirectoryNode& de::PathDirectoryNode::setUserData(void* userData)
{
    d->pair.data = userData;
    return *this;
}

void* de::PathDirectoryNode::userData() const
{
    return d->pair.data;
}

/**
 * C wrapper API
 */

#define TOINTERNAL(inst) \
    (inst) != 0? reinterpret_cast<de::PathDirectoryNode*>(inst) : NULL

#define TOINTERNAL_CONST(inst) \
    (inst) != 0? reinterpret_cast<const de::PathDirectoryNode*>(inst) : NULL

#define SELF(inst) \
    DENG2_ASSERT(inst); \
    de::PathDirectoryNode* self = TOINTERNAL(inst)

#define SELF_CONST(inst) \
    DENG2_ASSERT(inst); \
    const de::PathDirectoryNode* self = TOINTERNAL_CONST(inst)

PathDirectory* PathDirectoryNode_Directory(const PathDirectoryNode* node)
{
    SELF_CONST(node);
    return reinterpret_cast<PathDirectory*>(self->directory());
}

PathDirectoryNode* PathDirectoryNode_Parent(const PathDirectoryNode* node)
{
    SELF_CONST(node);
    return reinterpret_cast<PathDirectoryNode*>(self->parent());
}

PathDirectoryNodeType PathDirectoryNode_Type(const PathDirectoryNode* node)
{
    SELF_CONST(node);
    return self->type();
}

StringPoolId PathDirectoryNode_InternId(const PathDirectoryNode* node)
{
    SELF_CONST(node);
    return self->internId();
}

ushort PathDirectoryNode_Hash(const PathDirectoryNode* node)
{
    SELF_CONST(node);
    return self->hash();
}

int PathDirectoryNode_MatchDirectory(PathDirectoryNode* node, int flags,
    PathMap* searchPattern, void* /*parameters*/)
{
    SELF(node);
    return self->matchDirectory(flags, searchPattern);
}

void* PathDirectoryNode_UserData(const PathDirectoryNode* node)
{
    SELF_CONST(node);
    return self->userData();
}

void PathDirectoryNode_SetUserData(PathDirectoryNode* node, void* userData)
{
    SELF(node);
    self->setUserData(userData);
}

const Str* PathDirectoryNodeType_Name(PathDirectoryNodeType type)
{
    return de::PathDirectoryNode::typeName(type);
}
