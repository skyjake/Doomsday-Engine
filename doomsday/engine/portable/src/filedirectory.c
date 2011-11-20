/**\file filedirectory.c
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
#include "de_filesys.h"

#include "sys_timer.h"

#include "filedirectory.h"

typedef struct filedirectory_nodeinfo_s {
    boolean processed;
} filedirectory_nodeinfo_t;

struct filedirectory_s {
    /// Path hash table.
    PathDirectory* _pathDirectory;
};

typedef struct {
    FileDirectory* fileDirectory;
    int (*callback) (PathDirectoryNode* node, void* paramaters);
    void* paramaters;
} addpathworker_paramaters_t;

static int addPathWorker(const ddstring_t* filePath, pathdirectorynode_type_t nodeType,
    void* paramaters)
{
    addpathworker_paramaters_t* p = (addpathworker_paramaters_t*)paramaters;
    int result = 0; // Continue adding.
    assert(filePath && VALID_PATHDIRECTORYNODE_TYPE(nodeType) && p);

    if(!Str_IsEmpty(filePath))
    {
        PathDirectoryNode* node;
        filedirectory_nodeinfo_t* info;
        ddstring_t relPath;

        // Let's try to make it a relative path.
        Str_Init(&relPath);
        F_RemoveBasePath(&relPath, filePath);

        node = PathDirectory_Insert(p->fileDirectory->_pathDirectory, Str_Text(&relPath), FILEDIRECTORY_DELIMITER);
        //assert(PT_LEAF == PathDirectoryNode_Type(node));

        // Has this already been processed?
        info = (filedirectory_nodeinfo_t*) PathDirectoryNode_UserData(node);
        if(!info)
        {
            // Clearly not. Attach our node info.
            info = (filedirectory_nodeinfo_t*) malloc(sizeof *info);
            if(!info)
                Con_Error("FileDirectory::addPathWorker: Failed on allocation of %lu bytes for new FileDirectory::NodeInfo.", (unsigned long) sizeof *info);

            info->processed = false;
            PathDirectoryNode_AttachUserData(node, info);
        }

        if(info->processed)
        {
            // Does caller want to process it again?
            if(p->callback)
            {
                if(PT_BRANCH == PathDirectoryNode_Type(node))
                {
                    result = PathDirectory_Iterate2(p->fileDirectory->_pathDirectory, PCF_MATCH_PARENT, node, PATHDIRECTORY_NOHASH, p->callback, p->paramaters);
                }
                else
                {
                    result = p->callback(node, p->paramaters);
                }
            }
        }
        else
        {
            if(PT_BRANCH == PathDirectoryNode_Type(node))
            {
                ddstring_t searchPattern;

                // Compose the search pattern. Resolve relative to the base path
                // if not already absolute. We're interested in *everything*.
                Str_Init(&searchPattern);
                Str_Appendf(&searchPattern, "%s*", Str_Text(filePath));
                //F_PrependBasePath(&searchPattern, &searchPattern);

                // Process this search.
                F_AllResourcePaths2(Str_Text(&searchPattern), addPathWorker, (void*)p);
                Str_Free(&searchPattern);
            }
            else
            {
                if(p->callback)
                    p->callback(node, p->paramaters);
            }

            info->processed = true;
        }
        Str_Free(&relPath);
    }
    return result;
}

static FileDirectory* addPaths(FileDirectory* fd, const ddstring_t* const* paths,
    size_t pathsCount, int (*callback) (PathDirectoryNode* node, void* paramaters),
    void* paramaters)
{
//    uint startTime = verbose >= 2? Sys_GetRealTime(): 0;
    const ddstring_t* const* ptr = paths;
    size_t i;

    assert(fd && paths && pathsCount);

    for(i = 0; i < pathsCount && *ptr; ++i, ptr++)
    {
        const ddstring_t* searchPath = *ptr;
        PathDirectoryNode* node;
        filedirectory_nodeinfo_t* info;

        if(Str_IsEmpty(searchPath)) continue;

        node = PathDirectory_Insert(fd->_pathDirectory, Str_Text(searchPath), FILEDIRECTORY_DELIMITER);

        // Has this already been processed?
        info = (filedirectory_nodeinfo_t*) PathDirectoryNode_UserData(node);
        if(!info)
        {
            // Clearly not. Attach our node info.
            info = (filedirectory_nodeinfo_t*) malloc(sizeof *info);
            if(!info)
                Con_Error("FileDirectory::addPaths: Failed on allocation of %lu bytes for new FileDirectory::NodeInfo.", (unsigned long) sizeof *info);

            info->processed = false;
            PathDirectoryNode_AttachUserData(node, info);
        }

        if(info->processed)
        {
            // Does caller want to process it again?
            if(callback)
            {
                if(PT_BRANCH == PathDirectoryNode_Type(node))
                {
                    PathDirectory_Iterate2(fd->_pathDirectory, PCF_MATCH_PARENT, node, PATHDIRECTORY_NOHASH, callback, paramaters);
                }
                else
                {
                    callback(node, paramaters);
                }
            }
        }
        else
        {
            if(PT_BRANCH == PathDirectoryNode_Type(node))
            {
                addpathworker_paramaters_t p;
                ddstring_t searchPattern;

                // Compose the search pattern. Resolve relative to the base path
                // if not already absolute. We're interested in *everything*.
                Str_Init(&searchPattern);
                Str_Appendf(&searchPattern, "%s*", Str_Text(searchPath));
                //F_PrependBasePath(&searchPattern, &searchPattern);

                // Process this search.
                p.fileDirectory = fd;
                p.callback = callback;
                p.paramaters = paramaters;
                F_AllResourcePaths2(Str_Text(&searchPattern), addPathWorker, (void*)&p);
                Str_Free(&searchPattern);
            }
            else
            {
                if(callback)
                    callback(node, paramaters);
            }

            info->processed = true;
        }
    }

/*#if _DEBUG
    FileDirectory_Print(fd);
#endif*/
//    VERBOSE2( Con_Message("FileDirectory::addPaths: Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) );

    return fd;
}

static void resolveAndAddSearchPathsToDirectory(FileDirectory* fd,
    const Uri* const* searchPaths, uint searchPathsCount,
    int (*callback) (PathDirectoryNode* node, void* paramaters), void* paramaters)
{
    uint i;
    assert(fd);

    for(i = 0; i < searchPathsCount; ++i)
    {
        ddstring_t* searchPath = Uri_Resolved(searchPaths[i]);
        if(!searchPath) continue;

        // Let's try to make it a relative path.
        F_RemoveBasePath(searchPath, searchPath);
        if(Str_RAt(searchPath, 0) != FILEDIRECTORY_DELIMITER)
            Str_AppendChar(searchPath, FILEDIRECTORY_DELIMITER);

        addPaths(fd, (const ddstring_t**)&searchPath, 1, callback, paramaters);
        Str_Delete(searchPath);
    }
}

static void printPaths(const Uri* const* paths, size_t pathsCount)
{
    const Uri* const* ptr = paths;
    size_t i;
    assert(paths);

    for(i = 0; i < pathsCount && NULL != *ptr; ++i, ptr++)
    {
        const Uri* path = *ptr;
        ddstring_t* rawPath = Uri_ToString(path);
        ddstring_t* resolvedPath = Uri_Resolved(path);

        Con_Printf("  \"%s\" %s%s\n", Str_Text(rawPath), (resolvedPath != 0? "-> " : "--(!)incomplete"),
                   resolvedPath != 0? F_PrettyPath(Str_Text(resolvedPath)) : "");

        Str_Delete(rawPath);
        if(resolvedPath) Str_Delete(resolvedPath);
    }
}

FileDirectory* FileDirectory_NewWithPathListStr(const ddstring_t* pathList)
{
    FileDirectory* fd = (FileDirectory*) malloc(sizeof *fd);
    if(!fd)
        Con_Error("FileDirectory::Construct: Failed on allocation of %lu bytes for new FileDirectory.", (unsigned long) sizeof *fd);

    fd->_pathDirectory = PathDirectory_New();
    if(pathList)
    {
        size_t count;
        Uri** uris = F_CreateUriListStr2(RC_NULL, pathList, &count);
        resolveAndAddSearchPathsToDirectory(fd, (const Uri**)uris, (uint)count, 0, 0);
        F_DestroyUriList(uris);
    }
    return fd;
}

FileDirectory* FileDirectory_NewWithPathList(const char* pathList)
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
    fd = FileDirectory_NewWithPathListStr(paths);
    if(len != 0) Str_Free(paths);
    return fd;
}

FileDirectory* FileDirectory_New(void)
{
    return FileDirectory_NewWithPathListStr(NULL);
}

static int freeNodeInfo(PathDirectoryNode* node, void* paramaters)
{
    filedirectory_nodeinfo_t* info = PathDirectoryNode_DetachUserData(node);
    if(info) free(info);
    return 0; // Continue iteration.
}

static void clearNodeInfo(FileDirectory* fd)
{
    assert(fd);
    if(!fd->_pathDirectory) return;
    PathDirectory_Iterate(fd->_pathDirectory, 0, NULL, PATHDIRECTORY_NOHASH, freeNodeInfo);
}

void FileDirectory_Delete(FileDirectory* fd)
{
    assert(fd);
    clearNodeInfo(fd);
    if(fd->_pathDirectory) PathDirectory_Delete(fd->_pathDirectory);
    free(fd);
}

void FileDirectory_Clear(FileDirectory* fd)
{
    assert(fd);
    clearNodeInfo(fd);
    PathDirectory_Clear(fd->_pathDirectory);
}

void FileDirectory_AddPaths3(FileDirectory* fd, const Uri* const* paths, uint pathsCount,
    int (*callback) (PathDirectoryNode* node, void* paramaters), void* paramaters)
{
    assert(fd);
    if(!paths || pathsCount == 0)
    {
#if _DEBUG
        Con_Message("Warning:FileDirectory::AddPaths: Attempt to add zero-sized path list, ignoring.\n");
#endif
        return;
    }

#if _DEBUG
    VERBOSE( Con_Message("Adding paths to FileDirectory...\n") );
    VERBOSE2( printPaths(paths, pathsCount) );
#endif
    resolveAndAddSearchPathsToDirectory(fd, paths, pathsCount, callback, paramaters);
}

void FileDirectory_AddPaths2(FileDirectory* fd, const Uri* const* paths, uint pathsCount,
    int (*callback) (PathDirectoryNode* node, void* paramaters))
{
    FileDirectory_AddPaths3(fd, paths, pathsCount, callback, NULL);
}

void FileDirectory_AddPaths(FileDirectory* fd, const Uri* const* paths, uint pathsCount)
{
    FileDirectory_AddPaths2(fd, paths, pathsCount, NULL);
}

void FileDirectory_AddPathList3(FileDirectory* fd, const char* pathList,
    int (*callback) (PathDirectoryNode* node, void* paramaters), void* paramaters)
{
    Uri** paths = NULL;
    size_t pathsCount = 0;
    assert(fd);

    if(pathList && pathList[0])
        paths = F_CreateUriList2(RC_UNKNOWN, pathList, &pathsCount);

    FileDirectory_AddPaths3(fd, (const Uri**)paths, (uint)pathsCount, callback, paramaters);
    if(paths) F_DestroyUriList(paths);
}

void FileDirectory_AddPathList2(FileDirectory* fd, const char* pathList,
    int (*callback) (PathDirectoryNode* node, void* paramaters))
{
    FileDirectory_AddPathList3(fd, pathList, callback, NULL);
}

void FileDirectory_AddPathList(FileDirectory* fd, const char* pathList)
{
    FileDirectory_AddPathList2(fd, pathList, NULL);
}

int FileDirectory_Iterate2(FileDirectory* fd, pathdirectorynode_type_t nodeType,
    PathDirectoryNode* parent, ushort hash, filedirectory_iteratecallback_t callback, void* paramaters)
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
    const PathDirectoryNode* parent, ushort hash, filedirectory_iterateconstcallback_t callback, void* paramaters)
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
    const char* _searchPath, ddstring_t* foundName)
{
    const PathDirectoryNode* foundNode;
    ddstring_t searchPath;
    int flags;
    assert(fd);

    if(foundName)
    {
        Str_Clear(foundName);
    }

    if(!_searchPath || !_searchPath[0]) return false;

    // Convert the raw path into one we can process.
    Str_Init(&searchPath); Str_Set(&searchPath, _searchPath);
    F_FixSlashes(&searchPath, &searchPath);

    // Perform the search.
    flags = (nodeType == PT_LEAF? PCF_NO_BRANCH : PCF_NO_LEAF) | PCF_MATCH_FULL;
    foundNode = PathDirectory_Find(fd->_pathDirectory, flags, Str_Text(&searchPath), FILEDIRECTORY_DELIMITER);
    Str_Free(&searchPath);

    // Does caller want to know the full path?
    if(foundName && foundNode)
    {
        PathDirectory_ComposePath(PathDirectoryNode_Directory(foundNode), foundNode, foundName, NULL, FILEDIRECTORY_DELIMITER);
    }

    return !!foundNode;
}

static int C_DECL comparePaths(const void* a, const void* b)
{
    return stricmp(Str_Text((ddstring_t*)a), Str_Text((ddstring_t*)b));
}

#if _DEBUG
void FileDirectory_Print(FileDirectory* fd)
{
    size_t numFiles, n = 0;
    ddstring_t* fileList;
    assert(fd);

    Con_Printf("FileDirectory [%p]:\n", (void*)fd);
    fileList = PathDirectory_CollectPaths(fd->_pathDirectory, PT_LEAF, FILEDIRECTORY_DELIMITER, &numFiles);
    if(fileList)
    {
        qsort(fileList, numFiles, sizeof *fileList, comparePaths);
        do
        {
            Con_Printf("  %s\n", F_PrettyPath(Str_Text(fileList + n)));
            Str_Free(fileList + n);
        } while(++n < numFiles);
        free(fileList);
    }
    Con_Printf("  %lu %s in directory.\n", (unsigned long)numFiles, (numFiles==1? "file":"files"));
}

void FileDirectory_PrintHashDistribution(FileDirectory* fd)
{
    assert(fd);
    PathDirectory_PrintHashDistribution(fd->_pathDirectory);
}
#endif
