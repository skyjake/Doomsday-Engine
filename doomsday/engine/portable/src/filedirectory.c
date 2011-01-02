/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2010 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include "filedirectory.h"

static size_t countNodes(filedirectory_t* fd, filetype_t type)
{
    assert(fd);
    {
    filedirectory_node_t* node;
    size_t n = 0;
    for(node = fd->_direcFirst; node; node = node->next)
        if(!VALID_FILE_TYPE(type) || node->type == type)
            ++n;
    return n;
    }
}

/**
 * @return                  [ a new | the ] directory node that matches the name
 *                          and has the specified parent node.
 */
static filedirectory_node_t* direcNode(filedirectory_t* fd, const char* name,
    filedirectory_node_t* parent)
{
    assert(fd && name && name[0]);
    {
    filedirectory_node_t* node;

    // Have we already encountered this directory? Just iterate through all nodes.
    for(node = fd->_direcFirst; node; node = node->next)
    {
        if(!stricmp(node->path, name) && node->parent == parent)
            return node;
    }

    // Add a new node.
    if((node = malloc(sizeof(*node))) == 0)
        Con_Error("direcNode: failed on allocation of %lu bytes for new node.", (unsigned long) sizeof(*node));

    node->next = 0;
    node->parent = parent;
    if(fd->_direcLast)
        fd->_direcLast->next = node;
    fd->_direcLast = node;
    if(!fd->_direcFirst)
        fd->_direcFirst = node;

    // Make a copy of the path. Freed in FileDirectory_Clear().
    if((node->path = malloc(strlen(name) + 1)) == 0)
        Con_Error("direcNode: failed on allocation of %lu bytes for path.", (unsigned long) (strlen(name) + 1));

    strcpy(node->path, name);

    // No files yet.
    node->type = FT_DIRECTORY;
    node->processed = false;

    return node;
    }
}

/**
 * The path is split into as many nodes as necessary. Parent links are set.
 *
 * @return                  The node that identifies the given path.
 */
static filedirectory_node_t* buildDirecNodes(filedirectory_t* fd, const ddstring_t* path)
{
    assert(fd && path);
    {
    filedirectory_node_t* node = 0, *parent = 0;
    ddstring_t relPath, part;
    const char* p;

    // Let's try to make it a relative path.
    Str_Init(&relPath);
    F_RemoveBasePath(&relPath, path);

    // Continue splitting as long as there are parts.
    Str_Init(&part);
    p = Str_Text(&relPath);
    while((p = Str_CopyDelim2(&part, p, DIR_SEP_CHAR, CDF_OMIT_DELIMITER))) // Get the next part.
    {
        node = direcNode(fd, Str_Text(&part), parent);
        parent = node;
    }
    if(Str_Length(&part) != 0)
        node = direcNode(fd, Str_Text(&part), parent);

    Str_Free(&part);
    Str_Free(&relPath);
    return node;
    }
}

static void clearDirectory(filedirectory_t* fd)
{
    assert(fd);
    // Free the directory nodes.
    while(fd->_direcFirst)
    {
        filedirectory_node_t* next = fd->_direcFirst->next;
        free(fd->_direcFirst->path);
        free(fd->_direcFirst);
        fd->_direcFirst = next;
    }
    fd->_direcLast = 0;
    fd->_builtRecordSet = false;
}

static void printPathList(const char* pathList)
{
    assert(pathList);
    {
    dduri_t* uri;
    if(strlen(pathList) == 0)
        return;

    uri = Uri_ConstructDefault();
    // Tokenize into discrete paths and process individually.
    { const char* p = pathList;
    while((p = F_ParseSearchPath(uri, p, ';'))) // Get the next path.
    {
        ddstring_t* rawPath = Uri_ToString(uri);
        ddstring_t* resolvedPath = Uri_Resolved(uri);

        Con_Printf("  \"%s\" %s%s\n", Str_Text(rawPath), (resolvedPath != 0? "-> " : "--(!)incomplete"), resolvedPath != 0? Str_Text(F_PrettyPath(resolvedPath)) : "");

        Str_Delete(rawPath);
        if(resolvedPath)
            Str_Delete(resolvedPath);
    }}
    Uri_Destruct(uri);
    }
}

typedef struct {
    filedirectory_t* fd;
    filetype_t type;
    int (*callback) (const filedirectory_node_t* node, void* paramaters);
    void* paramaters;
} addpathworker_paramaters_t;

static int addPathWorker(const ddstring_t* filePath, filetype_t type, void* paramaters)
{
    assert(filePath && VALID_FILE_TYPE(type) && paramaters);
    {
    addpathworker_paramaters_t* p = (addpathworker_paramaters_t*)paramaters;
    int result = 0; // Continue adding.
    if(p->type == FT_NORMAL)
    {
        filedirectory_node_t* node = buildDirecNodes(p->fd, filePath);
        if(type == FT_NORMAL)
            node->type = FT_NORMAL;
        if(p->callback)
            result = p->callback(node, p->paramaters);
    }
    return result;
    }
}

typedef struct {
    ddstring_t* searchPath;
    ddstring_t* foundPath;
} findpathworker_paramaters_t;

static int findPathWorker(const filedirectory_node_t* direc, void* paramaters)
{
    assert(direc && paramaters);
    {
    findpathworker_paramaters_t* p = (findpathworker_paramaters_t*)paramaters;
    if(FileDirectoryNode_MatchDirectory(direc, Str_Text(p->searchPath)))
    {
        if(p->foundPath)
            FileDirectoryNode_ComposePath(direc, p->foundPath);
        return 1; // A match; stop!
    }
    return 0; // Continue searching.
    }
}

static filedirectory_t* parsePathsAndBuildNodes(filedirectory_t* fd, const char* _pathList,
    int (*callback) (const filedirectory_node_t* node, void* paramaters), void* paramaters)
{
    assert(fd);
    {
    const char* pathList = _pathList? _pathList : Str_Text(&fd->_pathList);
    uint startTime = verbose >= 2? Sys_GetRealTime(): 0;
    ddstring_t searchPattern;
    dduri_t* searchPath;

    VERBOSE( Con_Message("Adding paths to file directory ...\n") );
    VERBOSE2( printPathList(pathList) );

    Str_Init(&searchPattern);

    searchPath = Uri_ConstructDefault();

    // Tokenize into discrete paths and process individually.
    { const char* p = pathList; size_t n = 0;
    while((p = F_ParseSearchPath(searchPath, p, ';'))) // Get the next path.
    {
        filedirectory_node_t* direc;
        ddstring_t* resolvedPath;

        if((resolvedPath = Uri_Resolved(searchPath)) == 0)
            continue; // Incomplete path; ignore it.

        if(Str_RAt(resolvedPath, 0) != DIR_SEP_CHAR)
            Str_AppendChar(resolvedPath, DIR_SEP_CHAR);

        // Build direc nodes for this path.
        direc = buildDirecNodes(fd, resolvedPath);
        assert(direc);

        // Ignore duplicate paths (already processed).
        if(!direc->processed && pathList != _pathList)
        {
            // Add this new path to the list.
            ddstring_t* path = Uri_ComposePath(searchPath);
            Str_Append(&fd->_pathList, Str_Text(path));
            if(Str_RAt(&fd->_pathList, 0) != ';')
                Str_AppendChar(&fd->_pathList, ';');
            Str_Delete(path);
        }

        // Has this directory already been processed?
        if(direc->processed)
        {
            // Does caller want to process it again?
            if(callback)
                FileDirectory_Iterate2(fd, FT_NORMAL, direc, callback, paramaters);
        }
        else
        {
            // Compose the search pattern. We're interested in *everything*.
            addpathworker_paramaters_t p;
 
            Str_Clear(&searchPattern); Str_Appendf(&searchPattern, "%s*", Str_Text(resolvedPath));
            F_PrependBasePath(&searchPattern, &searchPattern);

            // Process this search.
            p.fd = fd;
            p.type = FT_NORMAL;
            p.callback = callback;
            p.paramaters = paramaters;
            F_ForAll2(&searchPattern, addPathWorker, (void*)&p);
            direc->processed = true;
        }

        Str_Free(resolvedPath);
    }}
    fd->_builtRecordSet = true;

    Str_Free(&searchPattern);
    if(searchPath)
        Uri_Destruct(searchPath);

/*#if _DEBUG
    FileDirectory_PrintFileList(fd);
#endif*/
    VERBOSE2( Con_Message("  Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) );

    return fd;
    }
}

filedirectory_t* FileDirectory_Create2(const char* pathList)
{
    filedirectory_t* fd;

    if((fd = malloc(sizeof(*fd))) == 0)
        Con_Error("FileDirectory_Create: Failed on allocation of %lu bytes for new FileDirectory.", (unsigned long) sizeof(*fd));

    fd->_builtRecordSet = false;
    fd->_direcFirst = fd->_direcLast = 0;
    Str_Init(&fd->_pathList);
    if(pathList)
    {
        Str_Set(&fd->_pathList, pathList);
        if(Str_RAt(&fd->_pathList, 0) != ';')
            Str_AppendChar(&fd->_pathList, ';');
    }
    return fd;
}

filedirectory_t* FileDirectory_Create(void)
{
    return FileDirectory_Create2(0);
}

void FileDirectory_Destroy(filedirectory_t* fd)
{
    assert(fd);
    clearDirectory(fd);
    Str_Free(&fd->_pathList);
    free(fd);
}

void FileDirectory_Clear(filedirectory_t* fd)
{
    assert(fd);
    clearDirectory(fd);
}

void FileDirectory_AddPaths3(filedirectory_t* fd, const char* pathList,
    int (*callback) (const filedirectory_node_t* node, void* paramaters), void* paramaters)
{
    parsePathsAndBuildNodes(fd, pathList, callback, paramaters);
}

void FileDirectory_AddPaths2(filedirectory_t* fd, const char* pathList,
    int (*callback) (const filedirectory_node_t* node, void* paramaters))
{
    FileDirectory_AddPaths3(fd, pathList, callback, 0);
}

void FileDirectory_AddPaths(filedirectory_t* fd, const char* pathList)
{
    FileDirectory_AddPaths2(fd, pathList, 0);
}

int FileDirectory_Iterate2(filedirectory_t* fd, filetype_t type, filedirectory_node_t* parent,
    int (*callback) (const filedirectory_node_t* node, void* paramaters), void* paramaters)
{
    assert(fd && callback);
    {
    int result = 0;
    filedirectory_node_t* node;

    // Time to build the record set?
    if(!fd->_builtRecordSet)
        parsePathsAndBuildNodes(fd, 0, 0, 0);

    for(node = fd->_direcFirst; node; node = node->next)
    {
        if(parent && node->parent != parent)
            continue;
        if(VALID_FILE_TYPE(type) && node->type != type)
            continue;
        if((result = callback(node, paramaters)) != 0)
            break;
    }
    return result;
    }
}

int FileDirectory_Iterate(filedirectory_t* fd, filetype_t type, filedirectory_node_t* parent,
    int (*callback) (const filedirectory_node_t* node, void* paramaters))
{
    return FileDirectory_Iterate2(fd, type, parent, callback, 0);
}

boolean FileDirectory_Find2(filedirectory_t* fd, const char* _searchPath, ddstring_t* foundPath)
{
    assert(fd && _searchPath && _searchPath[0]);
    {
    findpathworker_paramaters_t p;
    ddstring_t searchPath;
    int result;

    // Convert the raw path into one we can process.
    Str_Init(&searchPath); Str_Set(&searchPath, _searchPath);
    F_FixSlashes(&searchPath);

    p.foundPath = foundPath;
    p.searchPath = &searchPath;
    result = FileDirectory_Iterate2(fd, FT_NORMAL, 0, findPathWorker, (void*)&p);
    Str_Free(&searchPath);
    return result;
    }
}

boolean FileDirectory_Find(filedirectory_t* fd, const char* searchPath)
{
    return FileDirectory_Find2(fd, searchPath, 0);
}

ddstring_t* FileDirectory_AllPaths(filedirectory_t* fd, filetype_t type, size_t* count)
{
    assert(fd && count);
    {
    ddstring_t* filePaths = 0;
    *count = countNodes(fd, type);
    if(*count != 0)
    {
        if((filePaths = Z_Malloc(sizeof(*filePaths) * (*count), PU_APPSTATIC, 0)) == 0)
            Con_Error("FileDirectory_AllPaths: failed on allocation of %lu bytes for list.", (unsigned long) sizeof(*filePaths));

        { filedirectory_node_t* node;
        ddstring_t* path = filePaths;
        for(node = fd->_direcFirst; node; node = node->next)
        {
            if(VALID_FILE_TYPE(type) && node->type != type)
                continue;
            Str_Init(path);
            FileDirectoryNode_ComposePath(node, path);
            path++;
        }}
    }
    return filePaths;
    }
}

#if _DEBUG
static int C_DECL compareFilePaths(const void* a, const void* b)
{
    return stricmp(Str_Text((ddstring_t*)a), Str_Text((ddstring_t*)b));
}

void FileDirectory_PrintFileList(filedirectory_t* fd)
{
    assert(fd);
    {
    size_t numFilePaths, n = 0;
    ddstring_t* filePaths;

    Con_Printf("FileDirectory:\n");
    if((filePaths = FileDirectory_AllPaths(fd, FT_NORMAL, &numFilePaths)) != 0)
    {
        qsort(filePaths, numFilePaths, sizeof(*filePaths), compareFilePaths);
        do
        {
            Con_Printf("  %s\n", Str_Text(F_PrettyPath(filePaths + n)));
            Str_Free(filePaths + n);
        } while(++n < numFilePaths);
        Z_Free(filePaths);
    }
    Con_Printf("  %lu %s in directory.\n", (unsigned long)numFilePaths, (numFilePaths==1? "file":"files"));
    }
}
#endif

boolean FileDirectoryNode_MatchDirectory(const filedirectory_node_t* direc, const char* name)
{
    assert(direc && name && name[0]);
    {
    filename_t dir;

    // Where does the directory name begin? We'll do this in reverse order.
    strncpy(dir, name, FILENAME_T_MAXLEN);
    { char* pos;
    while((pos = strrchr(dir, DIR_SEP_CHAR)) != 0)
    {
        // Does this match?
        if(stricmp(direc->path, pos + 1))
            return false;

        // Are there no more parent directories?
        if(!direc->parent)
            return false;

        // So far so good. Move one directory level upwards.
        direc = direc->parent;
        // The string now ends here.
        *pos = 0;
    }}

    // Anything remaining is the root directory name - does it match?
    if(stricmp(direc->path, dir))
        return false;

    // We must have now arrived at a directory on the search path.
    return true;
    }
}

void FileDirectoryNode_ComposePath(const filedirectory_node_t* direc, ddstring_t* foundPath)
{
    assert(direc && foundPath);
    Str_Prepend(foundPath, direc->path);
    direc = direc->parent;
    while(direc)
    {
        Str_Prepend(foundPath, DIR_SEP_STR);
        Str_Prepend(foundPath, direc->path);
        direc = direc->parent;
    }
}
