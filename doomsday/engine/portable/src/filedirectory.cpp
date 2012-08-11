/**
 * @file filedirectory.cpp
 * FileDirectory implementation. @ingroup fs
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

#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"

#include "timer.h"
#include "filedirectory.h"
#include <de/memory.h>

struct FileDirectoryNodeInfo
{
    bool processed;
    FileDirectoryNodeInfo() : processed(false) {}
};

static int addPathNodesAndMaybeDescendBranch(int flags, const ddstring_t* filePath,
    pathdirectorynode_type_t nodeType, void* parameters);

static de::PathDirectoryNode* attachMissingNodeInfo(de::PathDirectoryNode* node)
{
    if(!node) return NULL;
    // Has this already been processed?
    FileDirectoryNodeInfo* info = reinterpret_cast<FileDirectoryNodeInfo*>(node->userData());
    if(!info)
    {
        // Clearly not. Attach new node info.
        node->setUserData(new FileDirectoryNodeInfo());
    }
    return node;
}

de::FileDirectory::FileDirectory(const char* _basePath)
    : PathDirectory(), basePath(0), baseNode(0)
{
    if(_basePath && _basePath[0])
    {
        basePath = Str_Set(Str_NewStd(), _basePath);
        // Ensure path is correctly formed.
        F_AppendMissingSlash(basePath);
    }
}

de::FileDirectory::~FileDirectory()
{
    clearNodeInfo();
    if(basePath) Str_Delete(basePath);
}

void de::FileDirectory::clearNodeInfo()
{
    NodeHash* nodes = nodeHash(PT_LEAF);
    if(nodes)
    DENG2_FOR_EACH(i, *nodes, NodeHash::iterator)
    {
        FileDirectoryNodeInfo* info = reinterpret_cast<FileDirectoryNodeInfo*>((*i)->userData());
        if(info)
        {
            // Detach our user data from this node.
            (*i)->setUserData(0);
            delete info;
        }
    }

    nodes = nodeHash(PT_BRANCH);
    if(nodes)
    DENG2_FOR_EACH(i, *nodes, NodeHash::iterator)
    {
        FileDirectoryNodeInfo* info = reinterpret_cast<FileDirectoryNodeInfo*>((*i)->userData());
        if(info)
        {
            // Detach our user data from this node.
            (*i)->setUserData(0);
            delete info;
        }
    }
}

void de::FileDirectory::clear()
{
    clearNodeInfo();
    PathDirectory::clear();
    baseNode = NULL;
}

bool de::FileDirectory::find(pathdirectorynode_type_t nodeType,
    const char* _searchPath, char searchDelimiter, ddstring_t* foundPath,
    char foundDelimiter)
{
    if(foundPath)
    {
        Str_Clear(foundPath);
    }

    if(!_searchPath || !_searchPath[0]) return false;

    // Convert the raw path into one we can process.
    ddstring_t searchPath;
    Str_Init(&searchPath); Str_Set(&searchPath, _searchPath);
    F_FixSlashes(&searchPath, &searchPath);

    // Try to make it a relative path?
    if(basePath && !F_IsAbsolute(&searchPath))
    {
        F_RemoveBasePath2(&searchPath, &searchPath, basePath);
    }

    // Perform the search.
    int flags = (nodeType == PT_LEAF? PCF_NO_BRANCH : PCF_NO_LEAF) | PCF_MATCH_FULL;
    const de::PathDirectoryNode* foundNode = PathDirectory::find(flags, Str_Text(&searchPath), searchDelimiter);
    Str_Free(&searchPath);

    // Does caller want to know the full path?
    if(foundPath && foundNode)
    {
        foundNode->directory()->composePath(foundNode, foundPath, NULL, foundDelimiter);
    }

    return !!foundNode;
}

de::PathDirectoryNode* de::FileDirectory::addPathNodes(const ddstring_t* rawPath)
{
    if(!rawPath || Str_IsEmpty(rawPath)) return NULL;

    const ddstring_t* path;
    ddstring_t buf;
    if(basePath)
    {
        // Try to make it a relative path?
        if(F_IsAbsolute(rawPath))
        {
            F_RemoveBasePath2(Str_InitStd(&buf), rawPath, basePath);
            path = &buf;
        }
        else
        {
            path = rawPath;
        }

        // If this is equal to the base path, return that node.
        if(Str_IsEmpty(path))
        {
            // Time to construct the relative base node?
            // This node is purely symbolic, its only necessary for our internal use.
            if(!baseNode)
            {
                baseNode = insert("./", '/');
                attachMissingNodeInfo(baseNode);
            }

            if(path == &buf) Str_Free(&buf);
            return baseNode;
        }
    }
    else
    {
        // Do not add relative paths.
        if(!F_IsAbsolute(rawPath))
        {
            return NULL;
        }
    }

    de::PathDirectoryNode* node = insert(Str_Text(path), '/');
    attachMissingNodeInfo(node);

    if(path == &buf) Str_Free(&buf);

    return node;
}

typedef struct {
    de::FileDirectory* fileDirectory; ///< FileDirectory instance.

    int flags; ///< @ref searchPathFlags

    /// If not @c NULL the callback's logic dictates whether iteration continues.
    int (*callback) (struct pathdirectorynode_s* node, void* parameters);

    /// Passed to the callback.
    void* parameters;
} addpathworker_paramaters_t;

static int addPathWorker(const ddstring_t* filePath, pathdirectorynode_type_t nodeType,
    void* parameters)
{
    addpathworker_paramaters_t* p = (addpathworker_paramaters_t*)parameters;
    return addPathNodesAndMaybeDescendBranch(p->flags, filePath, nodeType, parameters);
}

int de::FileDirectory::addChildNodes(de::PathDirectoryNode* node, int flags,
    int (*callback) (struct pathdirectorynode_s* node, void* parameters), void* parameters)
{
    int result = 0; // Continue iteration.

    if(node && PT_BRANCH == node->type())
    {
        addpathworker_paramaters_t p;
        ddstring_t searchPattern;

        // Compose the search pattern.
        Str_Init(&searchPattern);
        node->directory()->composePath(node, &searchPattern, NULL, '/');
        // We're interested in *everything*.
        Str_AppendChar(&searchPattern, '*');

        // Take a copy of the caller's iteration parameters.
        p.fileDirectory = this;
        p.callback = callback;
        p.flags = flags;
        p.parameters = parameters;

        // Process this search.
        result = F_AllResourcePaths2(Str_Text(&searchPattern), flags, addPathWorker, (void*)&p);

        Str_Free(&searchPattern);
    }

    return result;
}

int de::FileDirectory::addPathNodesAndMaybeDescendBranch(bool descendBranches,
    const ddstring_t* filePath, pathdirectorynode_type_t nodeType,
    int flags, int (*callback) (struct pathdirectorynode_s* node, void* parameters),
    void* parameters)
{
    DENG2_ASSERT(VALID_PATHDIRECTORYNODE_TYPE(nodeType));

    int result = 0; // Continue iteration.

    // Add this path to the directory.
    de::PathDirectoryNode* node = addPathNodes(filePath);
    if(node)
    {
        FileDirectoryNodeInfo* info = reinterpret_cast<FileDirectoryNodeInfo*>(node->userData());

        if(PT_BRANCH == node->type())
        {
            // Descend into this subdirectory?
            if(descendBranches)
            {
                if(info->processed)
                {
                    // Does caller want to process it again?
                    if(callback)
                    {
                        NodeHash* nodes = nodeHash(PT_LEAF);
                        if(nodes)
                        DENG2_FOR_EACH(i, *nodes, NodeHash::iterator)
                        {
                            if(node == (*i)->parent())
                            {
                                result = callback(reinterpret_cast<struct pathdirectorynode_s*>(*i), parameters);
                                if(result) break;
                            }
                        }

                        if(!result)
                        {
                            nodes = nodeHash(PT_BRANCH);
                            if(nodes)
                            DENG2_FOR_EACH(i, *nodes, NodeHash::iterator)
                            {
                                if(node == (*i)->parent())
                                {
                                    result = callback(reinterpret_cast<struct pathdirectorynode_s*>(*i), parameters);
                                    if(result) break;
                                }
                            }
                        }
                    }
                }
                else
                {
                    result = addChildNodes(node, flags, callback, parameters);

                    // This node is now considered processed.
                    info->processed = true;
                }
            }
        }
        // Node is a leaf.
        else if(callback)
        {
            result = callback(reinterpret_cast<struct pathdirectorynode_s*>(node), parameters);

            // This node is now considered processed (if it wasn't already).
            info->processed = true;
        }
    }

    return result;
}

/**
 * @param filePath  Possibly-relative path to an element in the virtual file system.
 * @param nodeType  Type of element, either a branch (directory) or a leaf (file).
 * @param parameters  Caller's iteration parameters.
 * @return  Non-zero if iteration should stop else @c 0.
 */
static int addPathNodesAndMaybeDescendBranch(int flags, const ddstring_t* filePath,
    pathdirectorynode_type_t nodeType, void* parameters)
{
    addpathworker_paramaters_t* p = (addpathworker_paramaters_t*)parameters;
    DENG2_ASSERT(p);
    return p->fileDirectory->addPathNodesAndMaybeDescendBranch(!(flags & SPF_NO_DESCEND),
                                                               filePath, nodeType,
                                                               p->flags, p->callback, p->parameters);
}

DENG_DEBUG_ONLY(
static void printUriList(const Uri* const* pathList, size_t pathCount, int indent)
{
    if(!pathList) return;

    const Uri* const* pathsIt = pathList;
    for(size_t i = 0; i < pathCount && (*pathsIt); ++i, pathsIt++)
    {
        Uri_Print(*pathsIt, indent);
    }
})

void de::FileDirectory::addPaths(int flags,
    const Uri* const* searchPaths, uint searchPathsCount,
    int (*callback) (struct pathdirectorynode_s* node, void* parameters), void* parameters)
{
    if(!searchPaths || searchPathsCount == 0)
    {
        DEBUG_Message(("Warning: FileDirectory::AddPaths: Attempt to add zero-sized path list, ignoring.\n"));
        return;
    }

#if _DEBUG
    VERBOSE2( Con_Message("Adding paths to FileDirectory...\n");
              printUriList(searchPaths, searchPathsCount, 2/*indent*/) )
#endif

    for(uint i = 0; i < searchPathsCount; ++i)
    {
        const ddstring_t* searchPath = Uri_ResolvedConst(searchPaths[i]);
        if(!searchPath) continue;

        // Add new nodes on this path and/or re-process previously seen nodes.
        addPathNodesAndMaybeDescendBranch(true/*do descend*/, searchPath, PT_BRANCH,
                                          flags, callback, parameters);
    }

/*#if _DEBUG
    debugPrint(fd);
#endif*/
}

void de::FileDirectory::addPathList(int flags, const char* pathList,
    int (*callback) (struct pathdirectorynode_s*, void*), void* parameters)
{
    Uri** paths = NULL;
    size_t pathsCount = 0;

    if(pathList && pathList[0])
        paths = F_CreateUriList2(RC_UNKNOWN, pathList, &pathsCount);

    addPaths(flags, (const Uri**)paths, (uint)pathsCount, callback, parameters);
    if(paths) F_DestroyUriList(paths);
}

ddstring_t* de::FileDirectory::collectPaths(int flags, char delimiter, size_t* count)
{
    return PathDirectory::collectPaths(flags, delimiter, count);
}

#if _DEBUG
/// @return Lexicographical delta between the two ddstring_ts @a and @a b.
static int C_DECL comparePaths(const void* a, const void* b)
{
    return stricmp(Str_Text((ddstring_t*)a), Str_Text((ddstring_t*)b));
}

static void printPathList(const ddstring_t* pathList, size_t numPaths, int indent)
{
    if(!pathList) return;
    for(size_t n = 0; n < numPaths; ++n)
    {
        const ddstring_t* path = pathList + n;
        Con_Printf("%*s\n", indent, Str_Text(path));
    }
}

static void deletePathList(ddstring_t* pathList, size_t numPaths)
{
    if(!pathList) return;
    for(size_t n = 0; n < numPaths; ++n)
    {
        ddstring_t* path = pathList + n;
        Str_Free(path);
    }
    M_Free(pathList);
}

void de::FileDirectory::debugPrint(FileDirectory* inst)
{
    Con_Printf("FileDirectory [%p]:\n", (void*)inst);

    size_t numFiles;
    ddstring_t* pathList = inst->collectPaths(0, DIR_SEP_CHAR, &numFiles);
    if(pathList)
    {
        qsort(pathList, numFiles, sizeof *pathList, comparePaths);

        printPathList(pathList, numFiles, 2/*indent*/);

        deletePathList(pathList, numFiles);
    }
    Con_Printf("  %lu %s in directory.\n", (unsigned long)numFiles, (numFiles==1? "path":"paths"));
}

void de::FileDirectory::debugPrintHashDistribution(FileDirectory* inst)
{
    PathDirectory::debugPrintHashDistribution(inst);
}

#endif

/**
 * C Wrapper API:
 */

#define TOINTERNAL(inst) \
    (inst) != 0? reinterpret_cast<de::FileDirectory*>(inst) : NULL

#define TOINTERNAL_CONST(inst) \
    (inst) != 0? reinterpret_cast<const de::FileDirectory*>(inst) : NULL

#define SELF(inst) \
    DENG2_ASSERT(inst); \
    de::FileDirectory* self = TOINTERNAL(inst)

#define SELF_CONST(inst) \
    DENG2_ASSERT(inst); \
    const de::FileDirectory* self = TOINTERNAL_CONST(inst)

FileDirectory* FileDirectory_New(const char* basePath)
{
    return reinterpret_cast<FileDirectory*>(new de::FileDirectory(basePath));
}

void FileDirectory_Delete(FileDirectory* fd)
{
    if(fd)
    {
        SELF(fd);
        delete self;
    }
}

void FileDirectory_Clear(FileDirectory* fd)
{
    SELF(fd);
    self->clear();
}

void FileDirectory_AddPaths3(FileDirectory* fd, int flags, const Uri* const* paths,
    uint pathsCount, int (*callback) (PathDirectoryNode*, void*),
    void* parameters)
{
    SELF(fd);
    self->addPaths(flags, paths, pathsCount, callback, parameters);
}

void FileDirectory_AddPaths2(FileDirectory* fd, int flags, const Uri* const* paths,
    uint pathsCount, int (*callback) (PathDirectoryNode*, void*))
{
    SELF(fd);
    self->addPaths(flags, paths, pathsCount, callback);
}

void FileDirectory_AddPaths(FileDirectory* fd, int flags, const Uri* const* paths,
    uint pathsCount)
{
    SELF(fd);
    self->addPaths(flags, paths, pathsCount);
}

void FileDirectory_AddPathList3(FileDirectory* fd, int flags, const char* pathList,
    int (*callback) (PathDirectoryNode*, void*), void* parameters)
{
    SELF(fd);
    self->addPathList(flags, pathList, callback, parameters);
}

void FileDirectory_AddPathList2(FileDirectory* fd, int flags, const char* pathList,
    int (*callback) (PathDirectoryNode*, void*))
{
    SELF(fd);
    self->addPathList(flags, pathList, callback);
}

void FileDirectory_AddPathList(FileDirectory* fd, int flags, const char* pathList)
{
    SELF(fd);
    self->addPathList(flags, pathList);
}

int FileDirectory_Iterate2(FileDirectory* fd, pathdirectorynode_type_t nodeType,
    PathDirectoryNode* parent, ushort hash, filedirectory_iteratecallback_t callback,
    void* parameters)
{
    SELF(fd);

    int flags = (nodeType == PT_LEAF? PCF_NO_BRANCH : PCF_NO_LEAF);
    return PathDirectory_Iterate2(reinterpret_cast<PathDirectory*>(self),
                                  flags, parent, hash, callback, parameters);
}

int FileDirectory_Iterate(FileDirectory* fd, pathdirectorynode_type_t nodeType,
    PathDirectoryNode* parent, ushort hash, filedirectory_iteratecallback_t callback)
{
    return FileDirectory_Iterate2(fd, nodeType, parent, hash, callback, NULL);
}

int FileDirectory_Iterate2_Const(const FileDirectory* fd, pathdirectorynode_type_t nodeType,
    const PathDirectoryNode* parent, ushort hash,
    filedirectory_iterateconstcallback_t callback, void* parameters)
{
    SELF_CONST(fd);

    int flags = (nodeType == PT_LEAF? PCF_NO_BRANCH : PCF_NO_LEAF);
    return PathDirectory_Iterate2_Const(reinterpret_cast<const PathDirectory*>(self),
                                        flags, parent, hash, callback, parameters);
}

int FileDirectory_Iterate_Const(const FileDirectory* fd, pathdirectorynode_type_t nodeType,
    const PathDirectoryNode* parent, ushort hash, filedirectory_iterateconstcallback_t callback)
{
    return FileDirectory_Iterate2_Const(fd, nodeType, parent, hash, callback, NULL);
}

boolean FileDirectory_Find(FileDirectory* fd, pathdirectorynode_type_t nodeType,
    const char* searchPath, char searchDelimiter, ddstring_t* foundPath,
    char foundDelimiter)
{
    SELF(fd);
    return CPP_BOOL(self->find(nodeType, searchPath, searchDelimiter,
                               foundPath, foundDelimiter));
}

#if _DEBUG
void FileDirectory_DebugPrint(FileDirectory* fd)
{
    de::FileDirectory::debugPrint(TOINTERNAL(fd));
}

void FileDirectory_DebugPrintHashDistribution(FileDirectory* fd)
{
    de::FileDirectory::debugPrintHashDistribution(TOINTERNAL(fd));
}
#endif
