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

#include <assert.h>

#include "de_base.h"
#include "de_console.h"
#include "m_args.h"
#include "sys_direc.h"
#include "filedirectory.h"

#include "resourcenamespace.h"

/**
 * This is a hash function. It uses the base part of the file name to generate a
 * somewhat-random number between 0 and RESOURCENAMESPACE_HASHSIZE.
 *
 * @return              The generated hash index.
 */
static uint hashFunction(const char* name)
{
    assert(name);
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

    return key % RESOURCENAMESPACE_HASHSIZE;
    }
}

static void clearPathHash(resourcenamespace_t* rn)
{
    assert(rn);
    { uint i;
    for(i = 0; i < RESOURCENAMESPACE_HASHSIZE; ++i)
    {
        resourcenamespace_hashentry_t* entry = &rn->_pathHash[i];
        while(entry->first)
        {
            resourcenamespace_hashnode_t* nextNode = entry->first->next;
            free(entry->first->name);
            free(entry->first);
            entry->first = nextNode;
        }
        entry->last = 0;
    }}
}

#if _DEBUG
static void printPathHash(resourcenamespace_t* rn)
{
    assert(rn);
    {
    ddstring_t path;
    size_t n = 0;

    Str_Init(&path);

    { uint i;
    for(i = 0; i < RESOURCENAMESPACE_HASHSIZE; ++i)
    {
        resourcenamespace_hashentry_t* entry = &rn->_pathHash[i];
        resourcenamespace_hashnode_t* node = entry->first;
        while(node)
        {
            Str_Clear(&path);
            FileDirectoryNode_ComposePath((filedirectory_node_t*)node->data, &path);
            Con_Printf("  %lu: %lu:\"%s\" -> %s\n", (unsigned long)n, i, node->name, Str_Text(&path));
            ++n;
            node = node->next;
        }
    }}

    Str_Free(&path);
    }
}
#endif

static void formSearchPathList(ddstring_t* pathList, resourcenamespace_t* rn)
{
    assert(rn && pathList);

    // A command line override path?
    if(rn->_overrideName2 && rn->_overrideName2[0] && ArgCheckWith(rn->_overrideName2, 1))
    {
        const char* newPath = ArgNext();
        Str_Append(pathList, newPath); Str_Append(pathList, ";");

        Str_Append(pathList, newPath); Str_Append(pathList, "\\$(GameInfo.IdentityKey)"); Str_Append(pathList, ";");
    }

    // Join the extra pathlist from the resource namespace to the final pathlist?
    if(Str_Length(&rn->_extraSearchPaths) > 0)
    {
        Str_Append(pathList, Str_Text(&rn->_extraSearchPaths));
        // Ensure we have the required terminating semicolon.
        if(Str_RAt(pathList, 0) != ';')
            Str_AppendChar(pathList, ';');
    }

    // Join the pathlist from the resource namespace to the final pathlist.
    Str_Append(pathList, rn->_searchPaths);
    // Ensure we have the required terminating semicolon.
    if(Str_RAt(pathList, 0) != ';')
        Str_AppendChar(pathList, ';');

    // A command line default path?
    if(rn->_overrideName && rn->_overrideName[0] && ArgCheckWith(rn->_overrideName, 1))
    {
        Str_Append(pathList, ArgNext()); Str_Append(pathList, ";");
    }
}

void ResourceNamespace_Reset(resourcenamespace_t* rn)
{
    assert(rn);
    clearPathHash(rn);
    if(rn->_fileDirectory)
    {
        FileDirectory_Destroy(rn->_fileDirectory); rn->_fileDirectory = 0;
    }
    Str_Free(&rn->_extraSearchPaths);
    rn->_flags |= RNF_IS_DIRTY;
}

boolean ResourceNamespace_AddSearchPath(resourcenamespace_t* rn, const char* newPath)
{
    assert(rn);
    {
    ddstring_t* pathList;

    if(!newPath || !newPath[0] || !strcmp(newPath, DIR_SEP_STR))
        return false; // Not suitable.

    // Have we seen this path already?
    pathList = &rn->_extraSearchPaths;
    if(Str_Length(pathList))
    {
        const char* p = Str_Text(pathList);
        ddstring_t curPath;
        boolean ignore = false;

        Str_Init(&curPath);
        while(!ignore && (p = Str_CopyDelim(&curPath, p, ';')))
        {
            if(!Str_CompareIgnoreCase(&curPath, newPath))
                ignore = true;
        }
        Str_Free(&curPath);

        if(ignore) return true; // We don't want duplicates.
    }

    // Prepend to the path list - newer paths have priority.
    Str_Prepend(pathList, ";");
    Str_Prepend(pathList, newPath);

    rn->_flags |= RNF_IS_DIRTY;
    return true;
    }
}

void ResourceNamespace_ClearSearchPaths(resourcenamespace_t* rn)
{
    assert(rn);
    if(Str_Length(&rn->_extraSearchPaths) == 0)
        return;
    Str_Free(&rn->_extraSearchPaths);
    rn->_flags |= RNF_IS_DIRTY;
}

int addFilePathToResourceNamespaceWorker(const filedirectory_node_t* node, void* paramaters)
{
    assert(node && paramaters);
    {
    resourcenamespace_t* rn = (resourcenamespace_t*) paramaters;
    resourcenamespace_hashnode_t* hashNode;
    ddstring_t filePath;
    filename_t name;

    Str_Init(&filePath);
    FileDirectoryNode_ComposePath(node, &filePath);

    // Extract the file name.
    Dir_FileName(name, Str_Text(&filePath), FILENAME_T_MAXLEN);

    // Create a new hashNode and link it to the hash table.
    if((hashNode = malloc(sizeof(*hashNode))) == 0)
        Con_Error("addFilePathToResourceNamespaceWorker: failed on allocation of %lu bytes for hashNode.", (unsigned long) sizeof(*hashNode));

    hashNode->data = (void*) node;

    // Calculate the name key and add to the hash.
    { resourcenamespace_hashentry_t* slot = &rn->_pathHash[hashFunction(name)];
    if(slot->last)
        slot->last->next = hashNode;
    slot->last = hashNode;
    if(!slot->first)
        slot->first = hashNode;}

    if((hashNode->name = malloc(strlen(name) + 1)) == 0)
        Con_Error("addFilePathToResourceNamespaceWorker: failed on allocation of %lu bytes for name.", (unsigned long) (strlen(name) + 1));

    strcpy(hashNode->name, name);
    hashNode->next = 0;

    Str_Free(&filePath);
    return 0; // Continue iteration.
    }
}

static void rebuild(resourcenamespace_t* rn)
{
    assert(rn);
    if(rn->_flags & RNF_IS_DIRTY)
    {
        clearPathHash(rn);
        if(rn->_fileDirectory)
            FileDirectory_Destroy(rn->_fileDirectory);

        { ddstring_t tmp; Str_Init(&tmp);
        formSearchPathList(&tmp, rn);
        if(Str_Length(&tmp) > 0)
        {
            uint startTime;

            rn->_fileDirectory = FileDirectory_Create(Str_Text(&tmp));

            VERBOSE( Con_Message("Rebuilding rnamespace name hash ...\n") );
            startTime = verbose >= 2? Sys_GetRealTime(): 0;
            FileDirectory_Iterate2(rn->_fileDirectory, FT_NORMAL, 0, addFilePathToResourceNamespaceWorker, rn);
#if _DEBUG
            printPathHash(rn);
#endif
            VERBOSE2( Con_Message("  Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) );
        }
        Str_Free(&tmp);
        }

        rn->_flags &= ~RNF_IS_DIRTY;
    }
}

boolean ResourceNamespace_Find2(resourcenamespace_t* rn, const char* _searchPath, ddstring_t* foundPath)
{
    assert(rn && _searchPath && _searchPath[0]);
    {
    resourcenamespace_hashnode_t* node;
    ddstring_t searchPath;
    filename_t baseName;
    byte result = 0;

    // Convert the search path into one we can process.
    Str_Init(&searchPath); Str_Set(&searchPath, _searchPath);
    Dir_FixSlashes(Str_Text(&searchPath), Str_Length(&searchPath));

    // Extract the base name.
    Dir_FileName(baseName, Str_Text(&searchPath), FILENAME_T_MAXLEN);

    // Ensure the namespace is clean.
    rebuild(rn);

    // Go through the candidates.
    { resourcenamespace_hashentry_t* slot = &rn->_pathHash[hashFunction(baseName)];
    for(node = slot->first; node; node = node->next)
    {
        if(stricmp(node->name, baseName))
            continue;

        // If the directory compare passes, this is the match.
        // The directory must be on the search path for the test to pass.
        if(FileDirectoryNode_MatchDirectory((filedirectory_node_t*) node->data, Str_Text(&searchPath)))
        {
            if(foundPath)
            {
                FileDirectoryNode_ComposePath((filedirectory_node_t*) node->data, foundPath);
            }
            result = 1;
            break;
        }
    }}

    Str_Free(&searchPath);

    return (result == 0? false : true);
    }
}

boolean ResourceNamespace_Find(resourcenamespace_t* rn, const char* searchPath)
{
    return ResourceNamespace_Find2(rn, searchPath, 0);
}

boolean ResourceNamespace_MapPath(resourcenamespace_t* rn, ddstring_t* path)
{
    assert(rn && path);

    if(rn->_flags & RNF_USE_VMAP)
    {
        size_t nameLen = strlen(rn->_name), pathLen = Str_Length(path);
        if(nameLen <= pathLen && Str_At(path, nameLen) == DIR_SEP_CHAR && !strnicmp(rn->_name, Str_Text(path), nameLen))
        {
            Str_Prepend(path, Str_Text(GameInfo_DataPath(DD_GameInfo())));
            return true;
        }
    }
    return false;
}

const char* ResourceNamespace_Name(const resourcenamespace_t* rn)
{
    assert(rn);
    return rn->_name;
}

const ddstring_t* ResourceNamespace_ExtraSearchPaths(resourcenamespace_t* rn)
{
    assert(rn);
    return &rn->_extraSearchPaths;
}
