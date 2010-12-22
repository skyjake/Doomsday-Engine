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

#include "m_misc.h" /// @todo remove dependency
#include "sys_direc.h" /// @todo remove dependency
#include "sys_reslocator.h"
#include "sys_timer.h"

#include "filedirectory.h"

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
    node->count = 0;
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
    filedirectory_node_t* node = 0, *parent;
    char* tokPath, *cursor, *part;
    ddstring_t relPath;

    // Let's try to make it a relative path.
    Str_Init(&relPath);
    F_RemoveBasePath(&relPath, path);

    if((tokPath = malloc(Str_Length(&relPath) + 1)) == 0)
        Con_Error("buildDirecNodes: failed on allocation of %lu bytes.", (unsigned long) (Str_Length(&relPath) + 1));

    strcpy(tokPath, Str_Text(&relPath));
    parent = 0;

    // Continue splitting as long as there are parts.
    cursor = tokPath;
    while(*(part = M_StrTok(&cursor, DIR_SEP_STR)))
    {
        node = direcNode(fd, part, parent);
        parent = node;
    }

    free(tokPath);
    Str_Free(&relPath);
    return node;
    }
}

/**
 * Adds a file into the directory.
 *
 * @param fn            An absolute path.
 */
static int addFileWorker(const ddstring_t* filePath, filetype_t type, void* paramaters)
{
    assert(filePath && VALID_FILE_TYPE(type));
    if(type == FT_NORMAL)
    {
        filedirectory_node_t* node = buildDirecNodes((filedirectory_t*)paramaters, filePath);
        node->type = type;
        // There is now one more file in the directory.
        node->parent->count++;
        VERBOSE2( Con_Message("  File: %s\n", Str_Text(F_PrettyPath(filePath))) );
    }
    return 0; // Continue adding.
}

typedef struct {
    ddstring_t* searchPath;
    ddstring_t* foundPath;
} findfileworker_paramaters_t;

static int findFileWorker(const filedirectory_node_t* direc, void* paramaters)
{
    findfileworker_paramaters_t* params = (findfileworker_paramaters_t*)paramaters;
    if(FileDirectoryNode_MatchDirectory(direc, Str_Text(params->searchPath)))
    {   // A match!
        if(params->foundPath)
            FileDirectoryNode_ComposePath(direc, params->foundPath);
        return true;
    }
    return false; // Continue iteration.
}

/**
 * Process a directory and add its contents to the file directory.
 * If the path is relative, it is relative to the base path.
 */
static void addDirectory(filedirectory_t* fd, const ddstring_t* path)
{
    assert(fd && path);
    {
    filedirectory_node_t* direc = buildDirecNodes(fd, path);
    ddstring_t searchPattern;
    assert(direc);

    if(direc->processed)
    {
        // This directory has already been processed. It means the
        // given path was a duplicate. We won't process it again.
        return;
    }

    // Compose the search pattern. We're interested in *everything*.
    Str_Init(&searchPattern); Str_Appendf(&searchPattern, "%s*", Str_Text(path));
    F_PrependBasePath(&searchPattern, &searchPattern);

    // Process this search.
    F_ForAll2(&searchPattern, addFileWorker, fd);

    // Mark this directory processed.
    direc->processed = true;

    Str_Free(&searchPattern);
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

static void printPathList(filedirectory_t* fd)
{
    assert(fd);
    {
    ddstring_t path, translatedPath;

    Str_Init(&path);
    Str_Init(&translatedPath);

    // Tokenize the into discreet paths and process individually.
    { const char* p = Str_Text(&fd->_pathList); size_t n = 0;
    while((p = Str_CopyDelim(&path, p, ';'))) // Get the next path.
    {
        boolean incomplete = !F_ResolveURI(&translatedPath, &path);
        Con_Message("  %i: \"%s\" %s%s\n", n++, Str_Text(&path), (incomplete? "--(!)incomplete" : "-> "), !incomplete? M_PrettyPath(Str_Text(&translatedPath)) : "");
    }}

    Str_Free(&path);
    Str_Free(&translatedPath);
    }
}

static filedirectory_t* buildFileDirectory(filedirectory_t* fd)
{
    assert(fd);
    {
    uint startTime = verbose >= 2? Sys_GetRealTime(): 0;
    ddstring_t path, translatedPath;

    Str_Init(&path);
    Str_Init(&translatedPath);

    VERBOSE( printPathList(fd); Con_Message("Rebuilding file directory ...\n") );

    // Tokenize the into discreet paths and process individually.
    { const char* p = Str_Text(&fd->_pathList); size_t n = 0;
    while((p = Str_CopyDelim(&path, p, ';'))) // Get the next path.
    {
        if(!F_ResolveURI(&translatedPath, &path))
            continue; // Incomplete path; ignore it.

        // Expand any base-relative path directives.
        F_ExpandBasePath(&translatedPath, &translatedPath);

        // Add this path to the directory.
        addDirectory(fd, &translatedPath);
    }}
    fd->_builtRecordSet = true;

    Str_Free(&path);
    Str_Free(&translatedPath);

    VERBOSE2( Con_Message("  Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) );
    return fd;
    }
}

filedirectory_t* FileDirectory_Create(const char* pathList)
{
    assert(pathList && pathList[0]);
    {
    filedirectory_t* fd;

    if((fd = malloc(sizeof(*fd))) == 0)
        Con_Error("FileDirectory_Create: Failed on allocation of %lu bytes for new FileDirectory.", (unsigned long) sizeof(*fd));

    fd->_builtRecordSet = false;
    fd->_direcFirst = fd->_direcLast = 0;
    Str_Init(&fd->_pathList);
    Str_Set(&fd->_pathList, pathList);
    return fd;
    }
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

int FileDirectory_Iterate2(filedirectory_t* fd, filetype_t type, filedirectory_node_t* parent,
    int (*callback) (const filedirectory_node_t* node, void* paramaters), void* paramaters)
{
    assert(fd && callback);
    {
    int result = 0;
    filedirectory_node_t* node;

    // Time to build the record set?
    if(!fd->_builtRecordSet)
        buildFileDirectory(fd);

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
    findfileworker_paramaters_t p;
    ddstring_t searchPath;
    int result;

    // Convert the raw path into one we can process.
    Str_Init(&searchPath); Str_Set(&searchPath, _searchPath);
    Dir_FixSlashes(Str_Text(&searchPath), Str_Length(&searchPath));

    p.foundPath = foundPath;
    p.searchPath = &searchPath;
    result = FileDirectory_Iterate2(fd, FT_NORMAL, 0, findFileWorker, (void*)&p);
    Str_Free(&searchPath);
    return result;
    }
}

boolean FileDirectory_Find(filedirectory_t* fd, const char* searchPath)
{
    return FileDirectory_Find2(fd, searchPath, 0);
}

boolean FileDirectoryNode_MatchDirectory(const filedirectory_node_t* direc, const char* name)
{
    assert(direc && name && name[0]);
    {
    filename_t dir;
    char* pos;

    // We'll do this in reverse order.
    strncpy(dir, name, FILENAME_T_MAXLEN);
    while((pos = strrchr(dir, DIR_SEP_CHAR)) != 0)
    {
        // The string now ends here.
        *pos = 0;

        // Where does the directory name begin?
        pos = strrchr(dir, DIR_SEP_CHAR);
        if(!pos)
            pos = dir;
        else
            pos++;

        // Is there no more parent directories?
        if(!direc)
            return false;

        // Does this match the directory?
        if(stricmp(direc->path, pos))
        {
            // Mismatch! This is not it.
            return false;
        }

        // So far so good. Move one directory level upwards.
        direc = direc->parent;
    }

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
