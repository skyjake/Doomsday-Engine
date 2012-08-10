/**
 * @file filedirectory.c
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

typedef struct filedirectory_nodeinfo_s {
    boolean processed;
} filedirectory_nodeinfo_t;

struct filedirectory_s {
    /// Path hash table.
    PathDirectory* _pathDirectory;

    /// Used with relative path directories.
    ddstring_t* _basePath;

    /// Used with relative path directories.
    PathDirectoryNode* _baseNode;
};

typedef struct {
    FileDirectory* fileDirectory; ///< FileDirectory instance.

    int flags; ///< @ref searchPathFlags

    /// If not @c NULL the callback's logic dictates whether iteration continues.
    int (*callback) (PathDirectoryNode* node, void* paramaters);

    /// Passed to the callback.
    void* paramaters;
} addpathworker_paramaters_t;

static int addPathNodesAndMaybeDescendBranch(int flags, const ddstring_t* filePath,
    pathdirectorynode_type_t nodeType, void* paramaters);

static PathDirectoryNode* attachMissingNodeInfo(PathDirectoryNode* node)
{
    filedirectory_nodeinfo_t* info;

    if(!node) return NULL;

    // Has this already been processed?
    info = (filedirectory_nodeinfo_t*) PathDirectoryNode_UserData(node);
    if(!info)
    {
        // Clearly not. Attach our node info.
        info = (filedirectory_nodeinfo_t*) malloc(sizeof *info);
        if(!info) Con_Error("FileDirectory::addMissingNodeInfo: Failed on allocation of %lu bytes for new FileDirectory::NodeInfo.", (unsigned long) sizeof *info);

        info->processed = false;
        PathDirectoryNode_AttachUserData(node, info);
    }

    return node;
}

static PathDirectoryNode* addPathNodes(FileDirectory* fd, const ddstring_t* rawPath)
{
    const ddstring_t* path;
    PathDirectoryNode* node;
    ddstring_t buf;

    assert(fd);

    if(!rawPath || Str_IsEmpty(rawPath)) return NULL;

    if(fd->_basePath)
    {
        // Try to make it a relative path?
        if(F_IsAbsolute(rawPath))
        {
            F_RemoveBasePath2(Str_InitStd(&buf), rawPath, fd->_basePath);
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
            if(!fd->_baseNode)
            {
                fd->_baseNode = PathDirectory_Insert(fd->_pathDirectory, "./", '/');
                attachMissingNodeInfo(fd->_baseNode);
            }

            if(path == &buf) Str_Free(&buf);
            return fd->_baseNode;
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

    node = PathDirectory_Insert(fd->_pathDirectory, Str_Text(path), '/');
    attachMissingNodeInfo(node);

    if(path == &buf) Str_Free(&buf);

    return node;
}

static int addPathWorker(const ddstring_t* filePath, pathdirectorynode_type_t nodeType,
    void* paramaters)
{
    addpathworker_paramaters_t* p = (addpathworker_paramaters_t*)paramaters;
    return addPathNodesAndMaybeDescendBranch(p->flags, filePath, nodeType, paramaters);
}

static int addChildNodes(FileDirectory* fd, PathDirectoryNode* node, int flags,
    int (*callback) (PathDirectoryNode* node, void* paramaters), void* paramaters)
{
    int result = 0; // Continue iteration.

    assert(fd);

    if(node && PT_BRANCH == PathDirectoryNode_Type(node))
    {
        addpathworker_paramaters_t p;
        ddstring_t searchPattern;

        // Compose the search pattern.
        Str_Init(&searchPattern);
        PathDirectory_ComposePath(PathDirectoryNode_Directory(node), node, &searchPattern, NULL, '/');
        // We're interested in *everything*.
        Str_AppendChar(&searchPattern, '*');

        // Take a copy of the caller's iteration paramaters.
        p.fileDirectory = fd;
        p.callback = callback;
        p.flags = flags;
        p.paramaters = paramaters;

        // Process this search.
        result = F_AllResourcePaths2(Str_Text(&searchPattern), flags, addPathWorker, (void*)&p);

        Str_Free(&searchPattern);
    }

    return result;
}

/**
 * @param filePath  Possibly-relative path to an element in the virtual file system.
 * @param nodeType  Type of element, either a branch (directory) or a leaf (file).
 * @param paramaters  Caller's iteration paramaters.
 * @return  Non-zero if iteration should stop else @c 0.
 */
static int addPathNodesAndMaybeDescendBranch(int flags, const ddstring_t* filePath,
    pathdirectorynode_type_t nodeType, void* paramaters)
{
    addpathworker_paramaters_t* p = (addpathworker_paramaters_t*)paramaters;
    FileDirectory* fd = p->fileDirectory;
    PathDirectoryNode* node;
    int result = 0; // Continue iteration.

    assert(VALID_PATHDIRECTORYNODE_TYPE(nodeType) && p);

    // Add this path to the directory.
    node = addPathNodes(p->fileDirectory, filePath);
    if(node)
    {
        filedirectory_nodeinfo_t* info = (filedirectory_nodeinfo_t*) PathDirectoryNode_UserData(node);

        if(PT_BRANCH == PathDirectoryNode_Type(node))
        {
            // Descend into this subdirectory?
            if(!(flags & SPF_NO_DESCEND))
            {
                if(info->processed)
                {
                    // Does caller want to process it again?
                    if(p->callback)
                    {
                        result = PathDirectory_Iterate2(fd->_pathDirectory, PCF_MATCH_PARENT, node,
                                                        PATHDIRECTORY_NOHASH, p->callback, p->paramaters);
                    }
                }
                else
                {
                    result = addChildNodes(fd, node, p->flags, p->callback, p->paramaters);

                    // This node is now considered processed.
                    info->processed = true;
                }
            }
        }
        // Node is a leaf.
        else if(p->callback)
        {
            result = p->callback(node, p->paramaters);

            // This node is now considered processed (if it wasn't already).
            info->processed = true;
        }
    }

    return result;
}

static void resolveSearchPathsAndAddNodes(FileDirectory* fd,
    int flags, const Uri* const* searchPaths, uint searchPathsCount,
    int (*callback) (PathDirectoryNode* node, void* paramaters), void* paramaters)
{
    uint i;
    assert(fd);

    for(i = 0; i < searchPathsCount; ++i)
    {
        const ddstring_t* searchPath = Uri_ResolvedConst(searchPaths[i]);
        addpathworker_paramaters_t p;
        if(!searchPath) continue;

        // Take a copy of the caller's iteration paramaters.
        p.fileDirectory = fd;
        p.callback = callback;
        p.paramaters = paramaters;
        p.flags = flags;

        // Add new nodes on this path and/or re-process previously seen nodes.
        addPathNodesAndMaybeDescendBranch(0/*do descend*/, searchPath, PT_BRANCH, (void*)&p);
    }

/*#if _DEBUG
    FileDirectory_Print(fd);
#endif*/
}

static void printUriList(const Uri* const* pathList, size_t pathCount, int indent)
{
    const Uri* const* pathsIt = pathList;
    size_t i;
    if(!pathList) return;

    for(i = 0; i < pathCount && NULL != *pathsIt; ++i, pathsIt++)
    {
        Uri_Print(*pathsIt, indent);
    }
}

FileDirectory* FileDirectory_NewWithPathListStr(const char* basePath,
    const ddstring_t* pathList, int flags)
{
    FileDirectory* fd = (FileDirectory*) malloc(sizeof *fd);
    if(!fd)
        Con_Error("FileDirectory::Construct: Failed on allocation of %lu bytes for new FileDirectory.", (unsigned long) sizeof *fd);

    fd->_pathDirectory = PathDirectory_New();
    fd->_baseNode = NULL;

    if(basePath && basePath[0])
    {
        fd->_basePath = Str_Set(Str_NewStd(), basePath);
        // Ensure path is correctly formed.
        F_AppendMissingSlash(fd->_basePath);
    }
    else
    {
        fd->_basePath = NULL;
    }

    if(pathList)
    {
        size_t count;
        Uri** uris = F_CreateUriListStr2(RC_NULL, pathList, &count);
        resolveSearchPathsAndAddNodes(fd, flags, (const Uri**)uris, (uint)count, 0, 0);
        F_DestroyUriList(uris);
    }
    return fd;
}

FileDirectory* FileDirectory_NewWithPathList(const char* basePath,
    const char* pathList, int flags)
{
    FileDirectory* fd;
    ddstring_t _pathList, *paths = NULL;
    size_t len = (pathList != NULL? strlen(pathList) : 0);
    if(len != 0)
    {
        Str_Init(&_pathList);
        Str_Set(&_pathList, pathList);
        paths = &_pathList;
    }
    fd = FileDirectory_NewWithPathListStr(basePath, paths, flags);
    if(len != 0) Str_Free(paths);
    return fd;
}

FileDirectory* FileDirectory_New(const char* basePath)
{
    return FileDirectory_NewWithPathListStr(basePath, NULL, 0);
}

static int deleteNodeInfo(PathDirectoryNode* node, void* paramaters)
{
    filedirectory_nodeinfo_t* info = PathDirectoryNode_DetachUserData(node);
    if(info) free(info);
    return 0; // Continue iteration.
}

static void clearNodeInfo(FileDirectory* fd)
{
    assert(fd);
    if(!fd->_pathDirectory) return;
    PathDirectory_Iterate(fd->_pathDirectory, 0, NULL, PATHDIRECTORY_NOHASH, deleteNodeInfo);
}

void FileDirectory_Delete(FileDirectory* fd)
{
    assert(fd);
    clearNodeInfo(fd);
    if(fd->_pathDirectory) PathDirectory_Delete(fd->_pathDirectory);
    if(fd->_basePath) Str_Delete(fd->_basePath);
    free(fd);
}

void FileDirectory_Clear(FileDirectory* fd)
{
    assert(fd);
    clearNodeInfo(fd);
    PathDirectory_Clear(fd->_pathDirectory);
    fd->_baseNode = NULL;
}

void FileDirectory_AddPaths3(FileDirectory* fd, int flags, const Uri* const* paths,
    uint pathsCount, int (*callback) (PathDirectoryNode*, void*),
    void* paramaters)
{
    assert(fd);
    if(!paths || pathsCount == 0)
    {
        DEBUG_Message(("Warning: FileDirectory::AddPaths: Attempt to add zero-sized path list, ignoring.\n"));
        return;
    }

#if _DEBUG
    VERBOSE2( Con_Message("Adding paths to FileDirectory...\n");
              printUriList(paths, pathsCount, 2/*indent*/) )
#endif

    resolveSearchPathsAndAddNodes(fd, flags, paths, pathsCount, callback, paramaters);
}

void FileDirectory_AddPaths2(FileDirectory* fd, int flags, const Uri* const* paths,
    uint pathsCount, int (*callback) (PathDirectoryNode*, void*))
{
    FileDirectory_AddPaths3(fd, flags, paths, pathsCount, callback, NULL);
}

void FileDirectory_AddPaths(FileDirectory* fd, int flags, const Uri* const* paths,
    uint pathsCount)
{
    FileDirectory_AddPaths2(fd, flags, paths, pathsCount, NULL);
}

void FileDirectory_AddPathList3(FileDirectory* fd, int flags, const char* pathList,
    int (*callback) (PathDirectoryNode*, void*), void* paramaters)
{
    Uri** paths = NULL;
    size_t pathsCount = 0;
    assert(fd);

    if(pathList && pathList[0])
        paths = F_CreateUriList2(RC_UNKNOWN, pathList, &pathsCount);

    FileDirectory_AddPaths3(fd, flags, (const Uri**)paths, (uint)pathsCount, callback, paramaters);
    if(paths) F_DestroyUriList(paths);
}

void FileDirectory_AddPathList2(FileDirectory* fd, int flags, const char* pathList,
    int (*callback) (PathDirectoryNode*, void*))
{
    FileDirectory_AddPathList3(fd, flags, pathList, callback, NULL);
}

void FileDirectory_AddPathList(FileDirectory* fd, int flags, const char* pathList)
{
    FileDirectory_AddPathList2(fd, flags, pathList, NULL);
}

int FileDirectory_Iterate2(FileDirectory* fd, pathdirectorynode_type_t nodeType,
    PathDirectoryNode* parent, ushort hash, filedirectory_iteratecallback_t callback,
    void* paramaters)
{
    int flags = (nodeType == PT_LEAF? PCF_NO_BRANCH : PCF_NO_LEAF);
    assert(fd);
    return PathDirectory_Iterate2(fd->_pathDirectory, flags, parent, hash, callback, paramaters);
}

int FileDirectory_Iterate(FileDirectory* fd, pathdirectorynode_type_t nodeType,
    PathDirectoryNode* parent, ushort hash, filedirectory_iteratecallback_t callback)
{
    return FileDirectory_Iterate2(fd, nodeType, parent, hash, callback, NULL);
}

int FileDirectory_Iterate2_Const(const FileDirectory* fd, pathdirectorynode_type_t nodeType,
    const PathDirectoryNode* parent, ushort hash,
    filedirectory_iterateconstcallback_t callback, void* paramaters)
{
    int flags = (nodeType == PT_LEAF? PCF_NO_BRANCH : PCF_NO_LEAF);
    assert(fd);
    return PathDirectory_Iterate2_Const(fd->_pathDirectory, flags, parent, hash, callback, paramaters);
}

int FileDirectory_Iterate_Const(const FileDirectory* fd, pathdirectorynode_type_t nodeType,
    const PathDirectoryNode* parent, ushort hash, filedirectory_iterateconstcallback_t callback)
{
    return FileDirectory_Iterate2_Const(fd, nodeType, parent, hash, callback, NULL);
}

boolean FileDirectory_Find(FileDirectory* fd, pathdirectorynode_type_t nodeType,
    const char* _searchPath, char searchDelimiter, ddstring_t* foundPath,
    char foundDelimiter)
{
    const PathDirectoryNode* foundNode;
    ddstring_t searchPath;
    int flags;
    assert(fd);

    if(foundPath)
    {
        Str_Clear(foundPath);
    }

    if(!_searchPath || !_searchPath[0]) return false;

    // Convert the raw path into one we can process.
    Str_Init(&searchPath); Str_Set(&searchPath, _searchPath);
    F_FixSlashes(&searchPath, &searchPath);

    // Try to make it a relative path?
    if(fd->_basePath && !F_IsAbsolute(&searchPath))
    {
        F_RemoveBasePath2(&searchPath, &searchPath, fd->_basePath);
    }

    // Perform the search.
    flags = (nodeType == PT_LEAF? PCF_NO_BRANCH : PCF_NO_LEAF) | PCF_MATCH_FULL;
    foundNode = PathDirectory_Find(fd->_pathDirectory, flags, Str_Text(&searchPath), searchDelimiter);
    Str_Free(&searchPath);

    // Does caller want to know the full path?
    if(foundPath && foundNode)
    {
        PathDirectory_ComposePath(PathDirectoryNode_Directory(foundNode), foundNode,
                                  foundPath, NULL, foundDelimiter);
    }

    return !!foundNode;
}

/// @return Lexicographical delta between the two ddstring_ts @a and @a b.
static int C_DECL comparePaths(const void* a, const void* b)
{
    return stricmp(Str_Text((ddstring_t*)a), Str_Text((ddstring_t*)b));
}

#if _DEBUG
static void printPathList(const ddstring_t* pathList, size_t numPaths, int indent)
{
    size_t n = 0;
    if(!pathList) return;
    for(n = 0; n < numPaths; ++n)
    {
        const ddstring_t* path = pathList + n;
        Con_Printf("%*s\n", indent, Str_Text(path));
    }
}

static void deletePathList(ddstring_t* pathList, size_t numPaths)
{
    size_t n = 0;
    if(!pathList) return;
    for(n = 0; n < numPaths; ++n)
    {
        ddstring_t* path = pathList + n;
        Str_Free(path);
    }
    free(pathList);
}

void FileDirectory_Print(FileDirectory* fd)
{
    size_t numFiles, n = 0;
    ddstring_t* pathList;
    assert(fd);

    Con_Printf("FileDirectory [%p]:\n", (void*)fd);
    pathList = PathDirectory_CollectPaths(fd->_pathDirectory, 0, DIR_SEP_CHAR, &numFiles);
    if(pathList)
    {
        qsort(pathList, numFiles, sizeof *pathList, comparePaths);

        printPathList(pathList, numFiles, 2/*indent*/);

        deletePathList(pathList, numFiles);
    }
    Con_Printf("  %lu %s in directory.\n", (unsigned long)numFiles, (numFiles==1? "path":"paths"));
}

void FileDirectory_PrintHashDistribution(FileDirectory* fd)
{
    assert(fd);
    PathDirectory_DebugPrintHashDistribution(fd->_pathDirectory);
}
#endif
