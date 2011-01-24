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

typedef struct filedirectory_node_s {
    struct filedirectory_node_s* next;
    struct filedirectory_node_s* parent;
    filedirectory_pathtype_t type;
    boolean processed;
    ddstring_t path;
} filedirectory_node_t;

static size_t countNodes(filedirectory_t* fd, filedirectory_pathtype_t type)
{
    assert(fd);
    {
    filedirectory_node_t* node;
    size_t n = 0;
    for(node = fd->_head; node; node = node->next)
        if(!VALID_FILEDIRECTORY_PATHTYPE(type) || node->type == type)
            ++n;
    return n;
    }
}

/**
 * @return  [ a new | the ] directory node that matches the name and type and
 * which has the specified parent node.
 */
static filedirectory_node_t* direcNode(filedirectory_t* fd, filedirectory_node_t* parent,
    filedirectory_pathtype_t type, const ddstring_t* name, void* leafData)
{
    assert(fd && name && !Str_IsEmpty(name));
    {
    filedirectory_node_t* node;

    // Have we already encountered this? Just iterate through all nodes.
    for(node = fd->_head; node; node = node->next)
    {
        if(node->parent != parent)
            continue;
        if(node->type != type)
            continue;
        if(Str_Length(&node->path) < Str_Length(name))
            continue;
        if(!strnicmp(Str_Text(&node->path), Str_Text(name), Str_Length(&node->path) - (node->type == FDT_DIRECTORY? 1:0)))
            return node;
    }

    // Add a new node.
    if((node = malloc(sizeof(*node))) == 0)
        Con_Error("direcNode: failed on allocation of %lu bytes for new node.", (unsigned long) sizeof(*node));

    if(fd->_tail)
        fd->_tail->next = node;
    fd->_tail = node;
    if(!fd->_head)
        fd->_head = node;

    node->type = type;
    Str_Init(&node->path); Str_Copy(&node->path, name);
    node->parent = parent;
    node->next = 0;
    node->processed = false;

    return node;
    }
}

/**
 * The path is split into as many nodes as necessary. Parent links are set.
 *
 * @return  The node that identifies the given path.
 */
static filedirectory_node_t* buildDirecNodes(filedirectory_t* fd,
    const ddstring_t* path, void* leafData)
{
    assert(fd && path);
    {
    filedirectory_node_t* node = 0, *parent = 0;
    ddstring_t part;
    const char* p;

    // Continue splitting as long as there are parts.
    Str_Init(&part);
    p = Str_Text(path);
    while((p = Str_CopyDelim2(&part, p, DIR_SEP_CHAR, 0))) // Get the next part.
    {
        node = direcNode(fd, parent, FDT_DIRECTORY, &part, 0);
        parent = node;
    }

    if(Str_Length(&part) != 0)
    {
        filedirectory_pathtype_t type = (Str_RAt(&part, 0) != DIR_SEP_CHAR? FDT_FILE : FDT_DIRECTORY);
        node = direcNode(fd, parent, type, &part, leafData);
    }

    Str_Free(&part);
    return node;
    }
}

static void clearDirectory(filedirectory_t* fd)
{
    assert(fd);
    // Free the directory nodes.
    while(fd->_head)
    {
        filedirectory_node_t* next = fd->_head->next;
        Str_Free(&fd->_head->path);
        free(fd->_head);
        fd->_head = next;
    }
    fd->_tail = 0;
}

typedef struct {
    filedirectory_t* fileDirectory;
    int (*callback) (const filedirectory_node_t* node, void* paramaters);
    void* paramaters;
} addpathworker_paramaters_t;

static int addPathWorker(const ddstring_t* filePath, filedirectory_pathtype_t type, void* paramaters)
{
    assert(filePath && VALID_FILEDIRECTORY_PATHTYPE(type) && paramaters);
    {
    addpathworker_paramaters_t* p = (addpathworker_paramaters_t*)paramaters;
    int result = 0; // Continue adding.
    if(type == FDT_FILE)
    {
        filedirectory_node_t* node;
        ddstring_t relPath;

        // Let's try to make it a relative path.
        Str_Init(&relPath);
        F_RemoveBasePath(&relPath, filePath);

        node = buildDirecNodes(p->fileDirectory, &relPath, 0);
        assert(node->type == FDT_FILE);

        if(p->callback)
            result = p->callback(node, p->paramaters);

        Str_Free(&relPath);
    }
    return result;
    }
}

typedef struct {
    ddstring_t* searchPath;
    filedirectory_node_t* foundNode;
} findpathworker_paramaters_t;

static int findPathWorker(filedirectory_node_t* node, void* paramaters)
{
    assert(node && paramaters);
    {
    findpathworker_paramaters_t* p = (findpathworker_paramaters_t*)paramaters;
    if(FileDirectoryNode_MatchDirectory(node, p->searchPath, DIR_SEP_CHAR))
    {
        p->foundNode = node;
        return 1; // A match; stop!
    }
    return 0; // Continue searching.
    }
}

static int iteratePaths(filedirectory_t* fd, filedirectory_pathtype_t type,
    filedirectory_node_t* parent, int (*callback) (filedirectory_node_t* node, void* paramaters),
    void* paramaters)
{
    assert(fd && callback);
    {
    int result = 0;
    filedirectory_node_t* node;
    for(node = fd->_head; node; node = node->next)
    {
        if(parent && node->parent != parent)
            continue;
        if(VALID_FILEDIRECTORY_PATHTYPE(type) && node->type != type)
            continue;
        if((result = callback(node, paramaters)) != 0)
            break;
    }
    return result;
    }
}

static filedirectory_t* addPaths(filedirectory_t* fd, const ddstring_t** paths,
    size_t pathsCount, int (*callback) (const filedirectory_node_t* node, void* paramaters),
    void* paramaters)
{
    assert(fd && paths && pathsCount != 0);
    {
//    uint startTime = verbose >= 2? Sys_GetRealTime(): 0;
    const ddstring_t** ptr = paths;
    { size_t i;
    for(i = 0; i < pathsCount && *ptr; ++i, ptr++)
    {
        const ddstring_t* searchPath = *ptr;
        filedirectory_node_t* leaf;

        // Build direc nodes for this path.
        leaf = buildDirecNodes(fd, searchPath, 0);
        assert(leaf);

        // Has this directory already been processed?
        if(leaf->processed)
        {
            // Does caller want to process it again?
            if(callback)
            {
                if(leaf->type == FDT_DIRECTORY)
                {
                    iteratePaths(fd, FDT_FILE, leaf, callback, paramaters);
                }
                else
                {
                    callback(leaf, paramaters);
                }
            }
        }
        else
        {
            if(leaf->type == FDT_DIRECTORY)
            {
                // Compose the search pattern. We're interested in *everything*.
                addpathworker_paramaters_t p;
                ddstring_t searchPattern;

                Str_Init(&searchPattern); Str_Appendf(&searchPattern, "%s*", Str_Text(searchPath));
                F_PrependBasePath(&searchPattern, &searchPattern);

                // Process this search.
                p.fileDirectory = fd;
                p.callback = callback;
                p.paramaters = paramaters;
                F_AllResourcePaths2(&searchPattern, addPathWorker, (void*)&p);
                Str_Free(&searchPattern);
            }
            else
            {
                if(callback)
                    callback(leaf, paramaters);
            }

            leaf->processed = true;
        }
    }}

/*#if _DEBUG
    FileDirectory_Print(fd);
#endif*/
//    VERBOSE2( Con_Message("FileDirectory::addPaths: Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) );

    return fd;
    }
}

static dduri_t* findSearchPath(filedirectory_t* fd, const dduri_t* path)
{
    assert(fd && path);
    { uint i;
    for(i = 0; i < fd->_searchPathsCount; ++i)
    {
        if(Uri_Equality(fd->_searchPaths[i], path))
            return fd->_searchPaths[i];
    }}
    return 0;
}

static void addPathToSearchPaths(filedirectory_t* fd, const dduri_t* path)
{
    assert(fd && path);
    fd->_searchPaths = realloc(fd->_searchPaths, sizeof(*fd->_searchPaths) * ++fd->_searchPathsCount);
    fd->_searchPaths[fd->_searchPathsCount-1] = Uri_ConstructCopy(path);
}

static void addPathsToSearchPaths(filedirectory_t* fd, const dduri_t** paths, uint pathsCount)
{
    assert(fd && paths && pathsCount != 0);
    { uint i;
    for(i = 0; *paths && i < pathsCount; ++i, paths++)
    {
        if(!findSearchPath(fd, *paths))
        {   // Add this new path to the list.
            addPathToSearchPaths(fd, *paths);
        }
    }}
}

static void resolveAndAddSearchPathsToDirectory(filedirectory_t* fd,
    int (*callback) (const filedirectory_node_t* node, void* paramaters), void* paramaters)
{
    assert(fd);
    { uint i;
    for(i = 0; i < fd->_searchPathsCount; ++i)
    {
        ddstring_t* searchPath;
        if((searchPath = Uri_Resolved(fd->_searchPaths[i])) != 0)
        {
            // Let's try to make it a relative path.
            F_RemoveBasePath(searchPath, searchPath);
            if(Str_RAt(searchPath, 0) != DIR_SEP_CHAR)
                Str_AppendChar(searchPath, DIR_SEP_CHAR);

            addPaths(fd, &searchPath, 1, callback, paramaters);
            Str_Delete(searchPath);
        }
    }}
    fd->_builtRecordSet = true;
}

static void printPaths(const dduri_t** paths, size_t pathsCount)
{
    assert(paths);
    {
    const dduri_t** ptr = paths;
    size_t i;
    for(i = 0; i < pathsCount && *ptr; ++i, ptr++)
    {
        const dduri_t* path = *ptr;
        ddstring_t* rawPath = Uri_ToString(path);
        ddstring_t* resolvedPath = Uri_Resolved(path);

        Con_Printf("  \"%s\" %s%s\n", Str_Text(rawPath), (resolvedPath != 0? "-> " : "--(!)incomplete"),
                   resolvedPath != 0? Str_Text(F_PrettyPath(resolvedPath)) : "");

        Str_Delete(rawPath);
        if(resolvedPath)
            Str_Delete(resolvedPath);
    }
    }
}

filedirectory_t* FileDirectory_ConstructStr2(const ddstring_t* pathList, char delimiter)
{
    filedirectory_t* fd;

    if((fd = malloc(sizeof(*fd))) == 0)
        Con_Error("FileDirectory::Construct: Failed on allocation of %lu bytes for "
                  "new FileDirectory.", (unsigned long) sizeof(*fd));

    fd->_head = fd->_tail = 0;
    fd->_builtRecordSet = false;
    if(pathList)
    {
        uint count;
        dduri_t** uris = F_CreateUriListStr2(RC_NULL, pathList, &count);
        addPathsToSearchPaths(fd, uris, count);
        resolveAndAddSearchPathsToDirectory(fd, 0, 0);
        F_DestroyUriList(uris);
    }
    else
    {
        fd->_searchPaths = 0;
        fd->_searchPathsCount = 0;
    }
    return fd;
}

filedirectory_t* FileDirectory_ConstructStr(const ddstring_t* pathList)
{
    return FileDirectory_ConstructStr2(pathList, DIR_SEP_CHAR);
}

filedirectory_t* FileDirectory_Construct2(const char* pathList, char delimiter)
{
    filedirectory_t* fd;
    ddstring_t _pathList, *paths = 0;
    size_t len = pathList? strlen(pathList) : 0;
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

filedirectory_t* FileDirectory_Construct(const char* pathList)
{
    return FileDirectory_Construct2(pathList, DIR_SEP_CHAR);
}

filedirectory_t* FileDirectory_ConstructEmpty2(char delimiter)
{
    return FileDirectory_ConstructStr2(0, delimiter);
}

filedirectory_t* FileDirectory_ConstructEmpty(void)
{
    return FileDirectory_ConstructEmpty2(DIR_SEP_CHAR);
}

filedirectory_t* FileDirectory_ConstructDefault(void)
{
    return FileDirectory_ConstructEmpty();
}

void FileDirectory_Destruct(filedirectory_t* fd)
{
    assert(fd);
    if(fd->_searchPaths)
    {
        uint i;
        for(i = 0; i < fd->_searchPathsCount; ++i)
            Uri_Destruct(fd->_searchPaths[i]);
        free(fd->_searchPaths);
    }
    clearDirectory(fd);
    free(fd);
}

void FileDirectory_Clear(filedirectory_t* fd)
{
    assert(fd);
    clearDirectory(fd);
    fd->_builtRecordSet = false;
}

void FileDirectory_AddPaths3(filedirectory_t* fd, const dduri_t** paths, uint pathsCount,
    int (*callback) (const filedirectory_node_t* node, void* paramaters), void* paramaters)
{
    assert(fd);
    if(!paths || pathsCount == 0)
    {
#if _DEBUG
        Con_Message("Warning:FileDirectory::AddPaths: Attempt to add zero-sized path list, ignoring.\n");
#endif
        return;
    }

    VERBOSE( Con_Message("Adding paths to FileDirectory ...\n") );
    VERBOSE2( printPaths(paths, pathsCount) );
    addPathsToSearchPaths(fd, paths, pathsCount);
    resolveAndAddSearchPathsToDirectory(fd, callback, paramaters);
}

void FileDirectory_AddPaths2(filedirectory_t* fd, const dduri_t** paths, uint pathsCount,
    int (*callback) (const filedirectory_node_t* node, void* paramaters))
{
    FileDirectory_AddPaths3(fd, paths, pathsCount, callback, 0);
}

void FileDirectory_AddPaths(filedirectory_t* fd, const dduri_t** paths, uint pathsCount)
{
    FileDirectory_AddPaths2(fd, paths, pathsCount, 0);
}

void FileDirectory_AddPathList3(filedirectory_t* fd, const char* pathList,
    int (*callback) (const filedirectory_node_t* node, void* paramaters), void* paramaters)
{
    assert(fd);
    {
    dduri_t** paths = 0;
    uint pathsCount = 0;
    if(pathList && pathList[0])
        paths = F_CreateUriList2(RC_UNKNOWN, pathList, &pathsCount);
    FileDirectory_AddPaths3(fd, paths, pathsCount, callback, paramaters);
    if(paths)
        F_DestroyUriList(paths);
    }
}

void FileDirectory_AddPathList2(filedirectory_t* fd, const char* pathList,
    int (*callback) (const filedirectory_node_t* node, void* paramaters))
{
    FileDirectory_AddPathList3(fd, pathList, callback, 0);
}

void FileDirectory_AddPathList(filedirectory_t* fd, const char* pathList)
{
    FileDirectory_AddPathList2(fd, pathList, 0);
}

int FileDirectory_Iterate2(filedirectory_t* fd, filedirectory_pathtype_t type,
    filedirectory_node_t* parent, int (*callback) (const filedirectory_node_t* node, void* paramaters),
    void* paramaters)
{
    assert(fd && (type == FDT_ANY || VALID_FILEDIRECTORY_PATHTYPE(type)));
    // Time to build the record set?
    if(!fd->_builtRecordSet && fd->_searchPathsCount != 0)
    {
        resolveAndAddSearchPathsToDirectory(fd, 0, 0);
    }
    return iteratePaths(fd, type, parent, callback, paramaters);
}

int FileDirectory_Iterate(filedirectory_t* fd, filedirectory_pathtype_t type,
    filedirectory_node_t* parent, int (*callback) (const filedirectory_node_t* node, void* paramaters))
{
    return FileDirectory_Iterate2(fd, type, parent, callback, 0);
}

boolean FileDirectory_FindPath(filedirectory_t* fd, const char* _searchPath,
    ddstring_t* foundName)
{
    assert(fd);
    {
    findpathworker_paramaters_t p;
    ddstring_t searchPath;
    int result;

    if(!_searchPath || !_searchPath[0])
        return false;

    // Time to build the record set?
    if(!fd->_builtRecordSet && fd->_searchPathsCount != 0)
    {
        resolveAndAddSearchPathsToDirectory(fd, 0, 0);
    }

    // Convert the raw path into one we can process.
    Str_Init(&searchPath); Str_Set(&searchPath, _searchPath);
    F_FixSlashes(&searchPath);

    // Perform the search.
    p.searchPath = &searchPath;
    p.foundNode = 0;
    result = iteratePaths(fd, FDT_FILE, 0, findPathWorker, (void*)&p);
    Str_Free(&searchPath);

    // Does caller want to know the full path?
    if(foundName)
    {
        if(p.foundNode)
            FileDirectoryNode_ComposePath(p.foundNode, foundName);
        else
            Str_Clear(foundName);
    }

    return result;
    }
}

ddstring_t* FileDirectory_AllPaths(filedirectory_t* fd, filedirectory_pathtype_t type,
    size_t* count)
{
    assert(fd && count);
    {
    ddstring_t* paths = 0;
    *count = countNodes(fd, type);
    if(*count != 0)
    {
        if((paths = Z_Malloc(sizeof(*paths) * (*count), PU_APPSTATIC, 0)) == 0)
            Con_Error("FileDirectory::AllPaths: Failed on allocation of %lu bytes for list.",
                      (unsigned long) sizeof(*paths));

        { filedirectory_node_t* node;
        ddstring_t* path = paths;
        for(node = fd->_head; node; node = node->next)
        {
            if(VALID_FILEDIRECTORY_PATHTYPE(type) && node->type != type)
                continue;
            Str_Init(path);
            FileDirectoryNode_ComposePath(node, path);
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

void FileDirectory_Print(filedirectory_t* fd)
{
    assert(fd);
    {
    size_t numFiles, n = 0;
    ddstring_t* fileList;

    Con_Printf("FileDirectory:\n");
    if((fileList = FileDirectory_AllPaths(fd, FDT_FILE, &numFiles)) != 0)
    {
        qsort(fileList, numFiles, sizeof(*fileList), comparePaths);
        do
        {
            Con_Printf("  %s\n", Str_Text(F_PrettyPath(fileList + n)));
            Str_Free(fileList + n);
        } while(++n < numFiles);
        Z_Free(fileList);
    }
    Con_Printf("  %lu %s in directory.\n", (unsigned long)numFiles, (numFiles==1? "file":"files"));
    }
}
#endif

boolean FileDirectoryNode_MatchDirectory(const filedirectory_node_t* node,
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
        if(Str_Length(&node->path) < Str_Length(searchPath) ||
           strnicmp(Str_Text(&node->path), Str_Text(searchPath),
                    Str_Length(&node->path) - (node->type == FDT_DIRECTORY? 1:0)))
            return false;

        // Are there no more parent directories?
        if(!node->parent)
            return false;

        // So far so good. Move one directory level upwards.
        node = node->parent;
        // The string now ends here.
        *pos = 0;
    }}

    // Anything remaining is the root directory or file; but does it match?
    if(Str_Length(&node->path) < Str_Length(searchPath) ||
       strnicmp(Str_Text(&node->path), Str_Text(searchPath),
                Str_Length(&node->path) - (node->type == FDT_DIRECTORY? 1:0)))
        return false;

    // We must have now arrived at the search target.
    return true;
    }
}

void FileDirectoryNode_ComposePath(const filedirectory_node_t* node, ddstring_t* foundPath)
{
    assert(node && foundPath);
    Str_Prepend(foundPath, Str_Text(&node->path));
    node = node->parent;
    while(node)
    {
        Str_Prepend(foundPath, Str_Text(&node->path));
        node = node->parent;
    }
}

filedirectory_pathtype_t FileDirectoryNode_Type(const filedirectory_node_t* node)
{
    assert(node);
    return node->type;
}
