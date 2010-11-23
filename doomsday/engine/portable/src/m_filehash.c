/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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

/**
 * m_filehash.c: File Name Hash Table
 *
 * Finding files *fast*.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_platform.h"
#include "de_base.h"
#include "de_system.h"
#include "de_misc.h"

#include <ctype.h>

// MACROS ------------------------------------------------------------------

// Number of entries in the hash table.
#define HASH_SIZE           (512)

// TYPES -------------------------------------------------------------------

typedef struct direcnode_s {
    struct direcnode_s* next;
    struct direcnode_s* parent;
    char*           path;
    uint            count;
    boolean         processed;
    boolean         isOnPath;
} direcnode_t;

typedef struct hashnode_s {
    struct hashnode_s* next;
    direcnode_t*    directory;
    char*           fileName;
} hashnode_t;

typedef struct hashentry_s {
    hashnode_t*     first;
    hashnode_t*     last;
} hashentry_t;

typedef struct _filehash_s {
    /// Copy of the path list specified at creation time.
    char* _pathList;
    direcnode_t*    direcFirst, *direcLast;
    hashentry_t     hashTable[HASH_SIZE];
} _filehash_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void addDirectory(_filehash_t* fh, const char* path);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * @return              [ a new | the ] directory node that matches the name
 *                      and has the specified parent node.
 */
static direcnode_t* direcNode(_filehash_t* fh, const char* name, direcnode_t* parent)
{
    direcnode_t* node;

    // Just iterate through all directory nodes.
    for(node = fh->direcFirst; node; node = node->next)
    {
        if(!stricmp(node->path, name) && node->parent == parent)
            return node;
    }

    // Add a new node.
    if((node = M_Malloc(sizeof(*node))) == NULL)
        Con_Error("direcNode: failed on allocation of %lu bytes for new node.",
                  (unsigned long) sizeof(*node));

    node->next = NULL;
    node->parent = parent;
    if(fh->direcLast)
        fh->direcLast->next = node;
    fh->direcLast = node;
    if(!fh->direcFirst)
        fh->direcFirst = node;

    // Make a copy of the path. Freed in FileHash_Destroy().
    if((node->path = M_Malloc(strlen(name) + 1)) == NULL)
        Con_Error("direcNode: failed on allocation of %lu bytes for path.",
                  (unsigned long) (strlen(name) + 1));

    strcpy(node->path, name);

    // No files yet.
    node->count = 0;
    node->isOnPath = false;
    node->processed = false;

    return node;
}

/**
 * The path is split into as many nodes as necessary. Parent links are set.
 *
 * @return              The node that identifies the given path.
 */
static direcnode_t* buildDirecNodes(_filehash_t* fh, const char* path)
{
    char* tokPath, *cursor, *part;
    direcnode_t* node = 0, *parent;
    filename_t relPath;

    // Let's try to make it a relative path.
    M_RemoveBasePath(relPath, path, FILENAME_T_MAXLEN);

    if((tokPath = cursor = M_Malloc(strlen(relPath) + 1)) == NULL)
        Con_Error("buildDirecNodes: failed on allocation of %lu bytes.",
                  (unsigned long) (strlen(relPath) + 1));

    strcpy(tokPath, relPath);
    parent = NULL;

    // Continue splitting as long as there are parts.
    while(*(part = M_StrTok(&cursor, DIR_SEP_STR)))
    {
        node = direcNode(fh, part, parent);
        parent = node;
    }

    M_Free(tokPath);
    return node;
}

/**
 * This is a hash function. It uses the base part of the file name to
 * generate a somewhat-random number between 0 and HASH_SIZE.
 *
 * @return              The generated hash index.
 */
static uint hashFunction(const char* name)
{
    unsigned short key = 0;
    int i, ch;

    // We stop when the name ends or the extension begins.
    for(i = 0; *name && *name != '.'; i++, name++)
    {
        ch = tolower(*name);

        if(i == 0)
            key ^= ch;
        else if(i == 1)
            key *= ch;
        else if(i == 2)
        {
            key -= ch;
            i = -1;
        }
    }

    return key % HASH_SIZE;
}

/**
 * Creates a file node into a directory.
 */
static void addFileToDirec(_filehash_t* fh, const char* filePath, direcnode_t* dir)
{
    filename_t name;
    hashnode_t* node;
    hashentry_t* slot;

    // Extract the file name.
    Dir_FileName(name, filePath, FILENAME_T_MAXLEN);

    // Create a new node and link it to the hash table.
    if((node = M_Malloc(sizeof(hashnode_t))) == NULL)
        Con_Error("addFileToDirec: failed on allocation of %lu bytes for node.",
                  (unsigned long) sizeof(hashnode_t));

    node->directory = dir;

    if((node->fileName = M_Malloc(strlen(name) + 1)) == NULL)
        Con_Error("addFileToDirec: failed on allocation of %lu bytes for fileName.",
                  (unsigned long) (strlen(name) + 1));

    strcpy(node->fileName, name);
    node->next = NULL;

    // Calculate the key.
    slot = &fh->hashTable[hashFunction(name)];
    if(slot->last)
        slot->last->next = node;
    slot->last = node;
    if(!slot->first)
        slot->first = node;

    // There's now one more file in the directory.
    dir->count++;
}

/**
 * Adds a file/directory to a directory.
 *
 * @param fn            An absolute path.
 */
static int addFile(const char* fn, filetype_t type, void* parm)
{
    filename_t path;
    char* pos;
    _filehash_t* fh = (_filehash_t*) parm;

    if(type != FT_NORMAL)
        return true;

    // Extract the path from the full file name.
    strncpy(path, fn, FILENAME_T_MAXLEN);
    if((pos = strrchr(path, DIR_SEP_CHAR)))
        *pos = 0;
    // Add a node for this file.
    addFileToDirec(fh, fn, buildDirecNodes(fh, path));
    return true;
}

/**
 * Process a directory and add its contents to the file hash.
 * If the path is relative, it is relative to the base path.
 */
static void addDirectory(_filehash_t* fh, const char* path)
{
    direcnode_t* direc = buildDirecNodes(fh, path);
    filename_t searchPattern;

    // This directory is now on the search path.
    direc->isOnPath = true;

    if(direc->processed)
    {
        // This directory has already been processed. It means the
        // given path was a duplicate. We won't process it again.
        return;
    }

    // Compose the search pattern.
    M_PrependBasePath(searchPattern, path, FILENAME_T_MAXLEN);

    // We're interested in *everything*.
    strncat(searchPattern, "*", FILENAME_T_MAXLEN);
    F_ForAll(searchPattern, fh, addFile);

    // Mark this directory processed.
    direc->processed = true;
}

/**
 * @param name          A relative path.
 *
 * @return              @c true, if the path specified in the name begins
 *                      from a directory in the search path.
 */
static boolean matchDirectory(hashnode_t* node, const char* name)
{
    direcnode_t* direc = node->directory;
    filename_t dir;
    char* pos;

    // We'll do this in reverse order.
    strncpy(dir, name, FILENAME_T_MAXLEN);
    while((pos = strrchr(dir, DIR_SEP_CHAR)) != NULL)
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

        // Does this match the node's directory?
        if(stricmp(direc->path, pos))
        {
            // Mismatch! This is not it.
            return false;
        }

        // So far so good. Move one directory level upwards.
        direc = direc->parent;
    }

    // We must have now arrived at a directory on the search path.
    return direc && direc->isOnPath;
}

/**
 * Composes an absolute path name for the node.
 */
static void composePath(hashnode_t* node, char* foundPath, size_t len)
{
    direcnode_t* direc = node->directory;
    filename_t buf;

    strncpy(foundPath, node->fileName, len);
    while(direc)
    {
        dd_snprintf(buf, FILENAME_T_MAXLEN, "%s" DIR_SEP_STR "%s", direc->path, foundPath);
        strncpy(foundPath, buf, FILENAME_T_MAXLEN);
        direc = direc->parent;
    }

    // Add the base path.
    M_PrependBasePath(foundPath, foundPath, len);
}

static void clearHash(_filehash_t* fh)
{
    if(fh->direcFirst)
    {
        direcnode_t* next;
        hashentry_t* entry;
        hashnode_t* nextNode;
        uint i;

        // Free the directory nodes.
        do
        {
            next = fh->direcFirst->next;
            M_Free(fh->direcFirst->path);
            M_Free(fh->direcFirst);
            fh->direcFirst = next;
        } while(fh->direcFirst);

        // Free the hash table.
        for(i = 0, entry = fh->hashTable; i < HASH_SIZE; ++i, entry++)
        {
            while(entry->first)
            {
                nextNode = entry->first->next;
                M_Free(entry->first->fileName);
                M_Free(entry->first);
                entry->first = nextNode;
            }
        }
    }
    fh->direcFirst = fh->direcLast = NULL;

    // Clear the entire table.
    memset(fh->hashTable, 0, sizeof(fh->hashTable));
}

static _filehash_t* buildFileHash(_filehash_t* fh)
{
    assert(fh);
    {
    char* tokenPaths = M_Malloc(strlen(fh->_pathList)+1);
    strcpy(tokenPaths, fh->_pathList);

    { char* path;
    if((path = strtok(tokenPaths, ";")))
    {
        do
        {
            // Convert all slashes to backslashes (sys_file compatibility).
            Dir_FixSlashes(path, strlen(path));
            addDirectory(fh, path); // Add this path to the hash.
        } while((path = strtok(NULL, ";"))); // Get the next path.
    }}
    M_Free(tokenPaths);
    return fh;
    }
}

filehash_t* FileHash_Create(const char* pathList)
{
    assert(pathList && pathList[0]);
    {
    size_t pathListLen = strlen(pathList);
    _filehash_t* fh = M_Calloc(sizeof(*fh));
    fh->_pathList = M_Malloc(pathListLen+1);
    strcpy(fh->_pathList, pathList);
    return (filehash_t*) buildFileHash(fh);
    }
}

void FileHash_Destroy(filehash_t* fileHash)
{
    assert(fileHash);
    {
    _filehash_t* fh = (_filehash_t*) fileHash;
    clearHash(fh);
    M_Free(fh->_pathList);
    M_Free(fh);
    }
}

const char* FileHash_PathList(filehash_t* fileHash)
{
    assert(fileHash);
    {
    _filehash_t* fh = (_filehash_t*) fileHash;
    return fh->_pathList;
    }
}

#if _DEBUG
void FileHash_Print(filehash_t* fileHash)
{
    assert(fileHash);
    {
    _filehash_t* fh = (_filehash_t*) fileHash;
    filename_t filePath;
    size_t i;
    for(i = 0; i < HASH_SIZE; ++i)
    {
        hashentry_t* slot = &fh->hashTable[i];
        hashnode_t* node;

        // Paths in the hash are relative to their directory node.
        // There is one direcnode per search path directory.
        // Go through the candidates.
        for(node = slot->first; node; node = node->next)
        {
            composePath(node, filePath, FILENAME_T_MAXLEN);
            Con_Message("  File: %s\n", M_PrettyPath(filePath));
        }
    }
    }
}
#endif

boolean FileHash_Find(filehash_t* fileHash, char* foundPath, const char* name, size_t len)
{
    assert(fileHash && foundPath && name && name[0] && len > 0);
    {
    filename_t validName, baseName;
    hashentry_t* slot;
    hashnode_t* node;
    _filehash_t* fh = (_filehash_t*) fileHash;

    // Absolute paths are not in the hash (no need to put them there).
    if(Dir_IsAbsolute(name))
        return false;

    // Convert the given file name into a file name we can process.
    strncpy(validName, name, FILENAME_T_MAXLEN);
    Dir_FixSlashes(validName, FILENAME_T_MAXLEN);

    // Extract the base name.
    Dir_FileName(baseName, validName, FILENAME_T_MAXLEN);

    // Which slot in the hash table?
    slot = &fh->hashTable[hashFunction(baseName)];

    // Paths in the hash are relative to their directory node.
    // There is one direcnode per search path directory.

    // Go through the candidates.
    for(node = slot->first; node; node = node->next)
    {
        // The file name in the node has no path.
        if(stricmp(node->fileName, baseName))
            continue;

        // If the directory compare passes, this is the match.
        // The directory must be on the search path for the test to pass.
        if(matchDirectory(node, validName))
        {
            composePath(node, foundPath, len);
            return true;
        }
    }

    // Nothing suitable was found.
    return false;
    }
}

boolean FileHash_HasRecordSet(filehash_t* fileHash)
{
    assert(fileHash);
    {
    _filehash_t* fh = (_filehash_t*) fileHash;
    return (fh->direcFirst != 0);
    }
}
