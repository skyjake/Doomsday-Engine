/**\file filehandle.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2011 Daniel Swanson <danij@dengine.net>
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
#include "de_system.h"

#include "blockset.h"
#include "filelist.h"
#include "abstractfile.h"
#include "dfile.h"

struct dfile_s
{
    /// The referenced file.
    abstractfile_t* _file;
    /// Either the FileList which owns this or the next DFile in the used object pool.
    void* _list;
};

// Has the handle builder been initialized yet?
static boolean inited = false;
// A mutex is used to protect access to the shared handle block allocator and object pool.
static mutex_t mutex;
// File handles are allocated by this block allocator.
static blockset_t* handleBlockSet;
// Head of the llist of used file handles, for recycling.
static DFile* usedHandles;

static void errorIfNotValid(const DFile* hndl, const char* callerName)
{
    if(DFile_IsValid(hndl)) return;
    Con_Error("%s: Instance %p has not yet been initialized.", callerName, (void*)hndl);
    exit(1); // Unreachable.
}

void DFileBuilder_Init(void)
{
    if(!inited)
    {
        mutex = Sys_CreateMutex("DFile_MUTEX");
        inited = true;
        return;
    }
    Con_Error("DFileBuilder::Init: Already initialized.");
}

void DFileBuilder_Shutdown(void)
{
    if(inited)
    {
        Sys_Lock(mutex);
        BlockSet_Delete(handleBlockSet), handleBlockSet = NULL;
        usedHandles = NULL;
        Sys_Unlock(mutex);
        Sys_DestroyMutex(mutex), mutex = 0;
        inited = false;
        return;
    }
#if _DEBUG
    Con_Error("DFileBuilder::Shutdown: Not presently initialized.");
#endif
}

DFile* DFileBuilder_CreateNew(abstractfile_t* file, FileList* list)
{
    assert(inited && file && list);
    {
    DFile* hndl = DFile_New();
    DFile_SetFile(hndl, file);
    DFile_SetList(hndl, list);
    return hndl;
    }
}

DFile* DFile_New(void)
{
    DFile* hndl;

    Sys_Lock(mutex);
    if(usedHandles)
    {
        hndl = usedHandles;
        usedHandles = hndl->_list;
    }
    else
    {
        if(!handleBlockSet)
        {
            handleBlockSet = BlockSet_New(sizeof(DFile), 64);
        }
        hndl = BlockSet_Allocate(handleBlockSet);
    }
    Sys_Unlock(mutex);

    return hndl;
}

void DFile_Delete(DFile* hndl, boolean recycle)
{
    assert(inited && hndl);

    hndl->_file = NULL;
    hndl->_list = NULL;

    if(!recycle)
    {
        // Memory for hndl will be free'd along with handleBlockSet
        return;
    }
    // Copy this hndl to the used object pool for recycling.
    Sys_Lock(mutex);
    hndl->_list = usedHandles;
    usedHandles = hndl;
    Sys_Unlock(mutex);
}

boolean DFile_IsValid(const DFile* hndl)
{
    assert(hndl);
    return (hndl && hndl->_file && hndl->_list);
}

FileList* DFile_List(DFile* hndl)
{
    errorIfNotValid(hndl, "DFile::List");
    return (FileList*)hndl->_list;
}

void DFile_SetList(DFile* hndl, FileList* list)
{
    assert(hndl);
    hndl->_list = list;
}

abstractfile_t* DFile_File(DFile* hndl)
{
    errorIfNotValid(hndl, "DFile::File");
    return hndl->_file;
}

const abstractfile_t* DFile_File_Const(const DFile* hndl)
{
    errorIfNotValid(hndl, "DFile::File const");
    return hndl->_file;
}

void DFile_SetFile(DFile* hndl, struct abstractfile_s* file)
{
    assert(hndl);
    hndl->_file = file;
}

#if _DEBUG
void DFile_Print(const DFile* hndl)
{
    assert(hndl);
    {
    byte id[16];
    F_GenerateFileId(Str_Text(AbstractFile_Path(DFile_File_Const(hndl))), id);               
    F_PrintFileId(id);
    Con_Printf(" - \"%s\"\n", F_PrettyPath(Str_Text(AbstractFile_Path(DFile_File_Const(hndl)))));
    }
}
#endif
