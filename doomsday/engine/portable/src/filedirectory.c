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

#include "sys_reslocator.h"
#include "sys_timer.h"
#include "sys_file.h"
#include "sys_findfile.h"

#include "filedirectory.h"

typedef struct filedirectory_nodeinfo_s {
    boolean processed;
} filedirectory_nodeinfo_t;

typedef struct {
    filedirectory_t* fileDirectory;
    int (*callback) (const struct pathdirectory_node_s* node, void* paramaters);
    void* paramaters;
} addpathworker_paramaters_t;

static int addPathWorker(const ddstring_t* filePath, pathdirectory_nodetype_t nodeType,
    void* paramaters)
{
    assert(NULL != filePath && VALID_PATHDIRECTORY_NODETYPE(nodeType) && NULL != paramaters);
    {
    addpathworker_paramaters_t* p = (addpathworker_paramaters_t*)paramaters;
    int result = 0; // Continue adding.
    if(PT_LEAF == nodeType && !Str_IsEmpty(filePath))
    {
        struct pathdirectory_node_s* node;
        filedirectory_nodeinfo_t* info;
        ddstring_t relPath;

        // Let's try to make it a relative path.
        Str_Init(&relPath);
        F_RemoveBasePath(&relPath, filePath);

        node = PathDirectory_Insert(p->fileDirectory->_pathDirectory, Str_Text(&relPath), FILEDIRECTORY_DELIMITER);
        assert(PT_LEAF == PathDirectoryNode_Type(node));

        // Has this already been processed?
        info = (filedirectory_nodeinfo_t*) PathDirectoryNode_UserData(node);
        if(NULL == info)
        {
            // Clearly not. Attach our node info.
            info = (filedirectory_nodeinfo_t*) malloc(sizeof(*info));
            if(NULL == info)
                Con_Error("FileDirectory::addPathWorker: Failed on allocation of %lu bytes for "
                    "new FileDirectory::NodeInfo.", (unsigned long) sizeof(*info));
            info->processed = false;
            PathDirectoryNode_AttachUserData(node, info);
        }

        if(NULL != p->callback)
            result = p->callback(node, p->paramaters);

        Str_Free(&relPath);
    }
    return result;
    }
}

static filedirectory_t* addPaths(filedirectory_t* fd, const ddstring_t* const* paths,
    size_t pathsCount, int (*callback) (const struct pathdirectory_node_s* node, void* paramaters),
    void* paramaters)
{
    assert(NULL != fd && NULL != paths && pathsCount != 0);
    {
//    uint startTime = verbose >= 2? Sys_GetRealTime(): 0;
    const ddstring_t* const* ptr = paths;
    { size_t i;
    for(i = 0; i < pathsCount && *ptr; ++i, ptr++)
    {
        const ddstring_t* searchPath = *ptr;
        struct pathdirectory_node_s* node;
        filedirectory_nodeinfo_t* info;

        if(Str_IsEmpty(searchPath)) continue;

        node = PathDirectory_Insert(fd->_pathDirectory, Str_Text(searchPath), FILEDIRECTORY_DELIMITER);

        // Has this already been processed?
        info = (filedirectory_nodeinfo_t*) PathDirectoryNode_UserData(node);
        if(NULL == info)
        {
            // Clearly not. Attach our node info.
            info = (filedirectory_nodeinfo_t*) malloc(sizeof(*info));
            if(NULL == info)
                Con_Error("FileDirectory::addPaths: Failed on allocation of %lu bytes for "
                    "new FileDirectory::NodeInfo.", (unsigned long) sizeof(*info));
            info->processed = false;
            PathDirectoryNode_AttachUserData(node, info);
        }

        if(info->processed)
        {
            // Does caller want to process it again?
            if(NULL != callback)
            {
                if(PT_BRANCH == PathDirectoryNode_Type(node))
                {
                    PathDirectory_Iterate2_Const(fd->_pathDirectory, PCF_MATCH_PARENT, node, callback, paramaters);
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
                if(NULL != callback)
                    callback(node, paramaters);
            }

            info->processed = true;
        }
    }}

/*#if _DEBUG
    FileDirectory_Print(fd);
#endif*/
//    VERBOSE2( Con_Message("FileDirectory::addPaths: Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) );

    return fd;
    }
}

static void resolveAndAddSearchPathsToDirectory(filedirectory_t* fd,
    const dduri_t* const* searchPaths, uint searchPathsCount,
    int (*callback) (const struct pathdirectory_node_s* node, void* paramaters),
    void* paramaters)
{
    assert(NULL != fd);
    { uint i;
    for(i = 0; i < searchPathsCount; ++i)
    {
        ddstring_t* searchPath = Uri_Resolved(searchPaths[i]);
        if(NULL == searchPath) continue;

        // Let's try to make it a relative path.
        F_RemoveBasePath(searchPath, searchPath);
        if(Str_RAt(searchPath, 0) != FILEDIRECTORY_DELIMITER)
            Str_AppendChar(searchPath, FILEDIRECTORY_DELIMITER);

        addPaths(fd, &searchPath, 1, callback, paramaters);
        Str_Delete(searchPath);
    }}
}

static void printPaths(const dduri_t* const* paths, size_t pathsCount)
{
    assert(NULL != paths);
    {
    const dduri_t* const* ptr = paths;
    size_t i;
    for(i = 0; i < pathsCount && NULL != *ptr; ++i, ptr++)
    {
        const dduri_t* path = *ptr;
        ddstring_t* rawPath = Uri_ToString(path);
        ddstring_t* resolvedPath = Uri_Resolved(path);

        Con_Printf("  \"%s\" %s%s\n", Str_Text(rawPath), (resolvedPath != 0? "-> " : "--(!)incomplete"),
                   resolvedPath != 0? F_PrettyPath(Str_Text(resolvedPath)) : "");

        Str_Delete(rawPath);
        if(NULL != resolvedPath)
            Str_Delete(resolvedPath);
    }
    }
}

filedirectory_t* FileDirectory_ConstructStr(const ddstring_t* pathList)
{
    filedirectory_t* fd = (filedirectory_t*) malloc(sizeof(*fd));
    if(NULL == fd)
        Con_Error("FileDirectory::Construct: Failed on allocation of %lu bytes for "
            "new FileDirectory.", (unsigned long) sizeof(*fd));

    fd->_pathDirectory = PathDirectory_Construct();
    if(NULL != pathList)
    {
        size_t count;
        dduri_t** uris = F_CreateUriListStr2(RC_NULL, pathList, &count);
        resolveAndAddSearchPathsToDirectory(fd, uris, (uint)count, 0, 0);
        F_DestroyUriList(uris);
    }
    return fd;
}

filedirectory_t* FileDirectory_Construct(const char* pathList)
{
    filedirectory_t* fd;
    ddstring_t _pathList, *paths = NULL;
    size_t len = pathList != NULL? strlen(pathList) : 0;
    if(len != 0)
    {
        Str_Init(&_pathList);
        Str_Set(&_pathList, pathList);
        paths = &_pathList;
    }
    fd = FileDirectory_ConstructStr(paths);
    if(len != 0)
        Str_Free(paths);
    return fd;
}

filedirectory_t* FileDirectory_ConstructEmpty(void)
{
    return FileDirectory_ConstructStr(NULL);
}

filedirectory_t* FileDirectory_ConstructDefault(void)
{
    return FileDirectory_ConstructEmpty();
}

static int freeNodeInfo(struct pathdirectory_node_s* node, void* paramaters)
{
    assert(NULL != node);
    {
    filedirectory_nodeinfo_t* info = PathDirectoryNode_DetachUserData(node);
    if(NULL != info)
        free(info);
    return 0; // Continue iteration.
    }
}

static void clearNodeInfo(filedirectory_t* fd)
{
    assert(NULL != fd);
    if(NULL == fd->_pathDirectory) return;
    PathDirectory_Iterate(fd->_pathDirectory, 0, NULL, freeNodeInfo);
}

void FileDirectory_Destruct(filedirectory_t* fd)
{
    assert(NULL != fd);
    clearNodeInfo(fd);
    if(NULL != fd->_pathDirectory)
        PathDirectory_Destruct(fd->_pathDirectory);
    free(fd);
}

void FileDirectory_Clear(filedirectory_t* fd)
{
    assert(NULL != fd);
    clearNodeInfo(fd);
    PathDirectory_Clear(fd->_pathDirectory);
}

void FileDirectory_AddPaths3(filedirectory_t* fd, const dduri_t* const* paths, uint pathsCount,
    int (*callback) (const struct pathdirectory_node_s* node, void* paramaters), void* paramaters)
{
    assert(NULL != fd);
    if(NULL == paths || pathsCount == 0)
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

void FileDirectory_AddPaths2(filedirectory_t* fd, const dduri_t* const* paths, uint pathsCount,
    int (*callback) (const struct pathdirectory_node_s* node, void* paramaters))
{
    FileDirectory_AddPaths3(fd, paths, pathsCount, callback, NULL);
}

void FileDirectory_AddPaths(filedirectory_t* fd, const dduri_t* const* paths, uint pathsCount)
{
    FileDirectory_AddPaths2(fd, paths, pathsCount, NULL);
}

void FileDirectory_AddPathList3(filedirectory_t* fd, const char* pathList,
    int (*callback) (const struct pathdirectory_node_s* node, void* paramaters), void* paramaters)
{
    assert(NULL != fd);
    {
    dduri_t** paths = NULL;
    size_t pathsCount = 0;
    if(NULL != pathList && pathList[0])
        paths = F_CreateUriList2(RC_UNKNOWN, pathList, &pathsCount);
    FileDirectory_AddPaths3(fd, paths, (uint)pathsCount, callback, paramaters);
    if(NULL != paths)
        F_DestroyUriList(paths);
    }
}

void FileDirectory_AddPathList2(filedirectory_t* fd, const char* pathList,
    int (*callback) (const struct pathdirectory_node_s* node, void* paramaters))
{
    FileDirectory_AddPathList3(fd, pathList, callback, NULL);
}

void FileDirectory_AddPathList(filedirectory_t* fd, const char* pathList)
{
    FileDirectory_AddPathList2(fd, pathList, NULL);
}

int FileDirectory_Iterate2(filedirectory_t* fd, pathdirectory_nodetype_t nodeType, 
    const struct pathdirectory_node_s* parent,
    int (*callback) (const struct pathdirectory_node_s* node, void* paramaters),
    void* paramaters)
{
    assert(NULL != fd);
    {
    int flags = (nodeType == PT_LEAF? PCF_NO_BRANCH : PCF_NO_LEAF);
    return PathDirectory_Iterate2_Const(fd->_pathDirectory, flags, parent, callback, paramaters);
    }
}

int FileDirectory_Iterate(filedirectory_t* fd, pathdirectory_nodetype_t nodeType,
    const struct pathdirectory_node_s* parent,
    int (*callback) (const struct pathdirectory_node_s* node, void* paramaters))
{
    return FileDirectory_Iterate2(fd, nodeType, parent, callback, NULL);
}

boolean FileDirectory_Find(filedirectory_t* fd, pathdirectory_nodetype_t nodeType,
    const char* _searchPath, ddstring_t* foundName)
{
    assert(NULL != fd);
    {
    const struct pathdirectory_node_s* foundNode;
    ddstring_t searchPath;
    int flags;

    if(NULL != foundName)
        Str_Clear(foundName);

    if(NULL == _searchPath || !_searchPath[0])
        return false;

    // Convert the raw path into one we can process.
    Str_Init(&searchPath); Str_Set(&searchPath, _searchPath);
    F_FixSlashes(&searchPath, &searchPath);

    // Perform the search.
    flags = (nodeType == PT_LEAF? PCF_NO_BRANCH : PCF_NO_LEAF);
    foundNode = PathDirectory_Find(fd->_pathDirectory, flags, Str_Text(&searchPath), FILEDIRECTORY_DELIMITER);
    Str_Free(&searchPath);

    // Does caller want to know the full path?
    if(NULL != foundName && NULL != foundNode)
    {
        PathDirectoryNode_ComposePath(foundNode, foundName, FILEDIRECTORY_DELIMITER);
    }

    return (NULL != foundNode);
    }
}

#if _DEBUG
static int C_DECL comparePaths(const void* a, const void* b)
{
    return stricmp(Str_Text((ddstring_t*)a), Str_Text((ddstring_t*)b));
}

void FileDirectory_Print(filedirectory_t* fd)
{
    assert(NULL != fd);
    {
    size_t numFiles, n = 0;
    ddstring_t* fileList;

    Con_Printf("FileDirectory:\n");
    if(NULL != (fileList = PathDirectory_CollectPaths(fd->_pathDirectory, PT_LEAF, FILEDIRECTORY_DELIMITER, &numFiles)))
    {
        qsort(fileList, numFiles, sizeof(*fileList), comparePaths);
        do
        {
            Con_Printf("  %s\n", F_PrettyPath(Str_Text(fileList + n)));
            Str_Free(fileList + n);
        } while(++n < numFiles);
        free(fileList);
    }
    Con_Printf("  %lu %s in directory.\n", (unsigned long)numFiles, (numFiles==1? "file":"files"));
    }
}
#endif
