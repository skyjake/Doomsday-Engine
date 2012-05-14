/**\file dfile.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2012 Daniel Swanson <danij@dengine.net>
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

#include <ctype.h>
#include <time.h>
#include <sys/stat.h>

#include "de_platform.h"
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
    /// The referenced abstract file (if any).
    abstractfile_t* _file;

    /// Either the FileList which owns this or the next DFile in the used object pool.
    void* _list;

    struct dfile_flags_s {
        uint open:1; /// Presently open.
        uint eof:1; /// Reader has reached the end of the stream.
        uint reference; /// This handle is a reference to another dfile instance.
    } _flags;

    /// Offset from start of owning package.
    size_t _baseOffset;

    FILE* _hndl;
    size_t _size;
    uint8_t* _data;
    uint8_t* _pos;
};

// Has the handle builder been initialized yet?
static boolean inited = false;
// A mutex is used to protect access to the shared handle block allocator and object pool.
static mutex_t mutex;
// File handles are allocated by this block allocator.
static blockset_t* handleBlockSet;
// Head of the llist of used file handles, for recycling.
static DFile* usedHandles;

static void errorIfNotValid(const DFile* file, const char* callerName)
{
    if(DFile_IsValid(file)) return;
    Con_Error("%s: Instance %p has not yet been initialized.", callerName, (void*)file);
    exit(1); // Unreachable.
}

void DFileBuilder_Init(void)
{
    if(!inited)
    {
        mutex = Sys_CreateMutex("DFileBuilder_MUTEX");
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

DFile* DFileBuilder_NewFromAbstractFileLump(abstractfile_t* container, int lumpIdx, boolean dontBuffer)
{
    const LumpInfo* info = F_LumpInfo(container, lumpIdx);
    DFile* file;
    if(!info) return NULL;

    file = DFile_New();
    // Init and load in the lump data.
    file->_flags.open = true;
    if(!dontBuffer)
    {
        file->_size = info->size;
        file->_pos = file->_data = (uint8_t*)malloc(file->_size);
        if(!file->_data)
            Con_Error("DFileBuilder::NewFromAbstractFileLump: Failed on allocation of %lu bytes for data buffer.",
                (unsigned long) file->_size);
#if _DEBUG
        VERBOSE2(
            ddstring_t* path = F_ComposeLumpPath(container, lumpIdx);
            Con_Printf("DFile [%p] buffering \"%s:%s\"...\n", (void*)file,
                       F_PrettyPath(Str_Text(AbstractFile_Path(container))),
                       F_PrettyPath(Str_Text(path)));
            Str_Delete(path);
        )
#endif
        F_ReadLumpSection(container, lumpIdx, (uint8_t*)file->_data, 0, info->size);
    }
    return file;
}

DFile* DFileBuilder_NewFromAbstractFile(abstractfile_t* af)
{
    DFile* file = DFile_New();
    assert(af);
    file->_file = af;
    file->_flags.open = true;
    file->_flags.reference = true;
    return file;
}

DFile* DFileBuilder_NewFromFile(FILE* hndl, size_t baseOffset)
{
    DFile* file = DFile_New();
    assert(hndl);
    file->_flags.open = true;
    file->_hndl = hndl;
    file->_baseOffset = baseOffset;
    return file;
}

DFile* DFileBuilder_NewCopy(const DFile* file)
{
    assert(inited && file);
    {
    DFile* clone = DFile_New();
    clone->_flags.open = true;
    clone->_flags.reference = true;
    clone->_file = DFile_File_Const(file);
    return clone;
    }
}

DFile* DFile_New(void)
{
    DFile* file;

    Sys_Lock(mutex);
    if(usedHandles)
    {
        file = usedHandles;
        usedHandles = file->_list;
    }
    else
    {
        if(!handleBlockSet)
        {
            handleBlockSet = BlockSet_New(sizeof(DFile), 64);
        }
        file = BlockSet_Allocate(handleBlockSet);
    }
    Sys_Unlock(mutex);

    file->_list = NULL;
    file->_file = NULL;
    file->_flags.eof  = false;
    file->_flags.open = false;
    file->_flags.reference = false;
    file->_baseOffset = 0;
    file->_size = 0;
    file->_hndl = NULL;
    file->_data = NULL;
    file->_pos  = 0;
    return file;
}

void DFile_Delete(DFile* file, boolean recycle)
{
    assert(inited && file);

    DFile_Close(file);
    file->_file = NULL;
    file->_list = NULL;

    if(!recycle)
    {
        // Memory for file will be free'd along with handleBlockSet
        return;
    }
    // Copy this file to the used object pool for recycling.
    Sys_Lock(mutex);
    file->_list = usedHandles;
    usedHandles = file;
    Sys_Unlock(mutex);
}

void DFile_Close(DFile* file)
{
    assert(file);
    if(!file->_flags.open) return;
    if(file->_hndl)
    {
        fclose(file->_hndl);
        file->_hndl = NULL;
    }
    else
    {   // Free the stored data.
        if(file->_data)
        {
            free(file->_data), file->_data = NULL;
        }
    }
    file->_pos = NULL;
    file->_flags.open = false;
}

boolean DFile_IsValid(const DFile* file)
{
    assert(file);
    /// \todo write me.
    return true;
}

FileList* DFile_List(DFile* file)
{
    errorIfNotValid(file, "DFile::List");
    return (FileList*)file->_list;
}

void DFile_SetList(DFile* file, FileList* list)
{
    assert(file);
    file->_list = list;
}

abstractfile_t* DFile_File(DFile* file)
{
    errorIfNotValid(file, "DFile::File");
    return file->_file;
}

abstractfile_t* DFile_File_Const(const DFile* file)
{
    errorIfNotValid(file, "DFile::File const");
    return file->_file;
}

size_t DFile_BaseOffset(const DFile* file)
{
    assert(file);
    if(file->_flags.reference)
    {
        return DFile_BaseOffset(AbstractFile_Handle(file->_file));
    }
    return file->_baseOffset;
}

size_t DFile_Length(DFile* file)
{
    errorIfNotValid(file, "DFile::Length");
    if(file->_flags.reference)
    {
        return DFile_Length(AbstractFile_Handle(file->_file));
    }
    else
    {
        size_t currentPosition = DFile_Seek(file, 0, SEEK_END);
        size_t length = DFile_Tell(file);
        DFile_Seek(file, currentPosition, SEEK_SET);
        return length;
    }
}

size_t DFile_Read(DFile* file, uint8_t* buffer, size_t count)
{
    errorIfNotValid(file, "DFile::Read");
    if(file->_flags.reference)
    {
        return DFile_Read(AbstractFile_Handle(file->_file), buffer, count);
    }
    else
    {
        size_t bytesleft;

        if(file->_hndl)
        {
            // Normal file.
            count = fread(buffer, 1, count, file->_hndl);
            if(feof(file->_hndl))
                file->_flags.eof = true;
            return count;
        }

        // Is there enough room in the file?
        bytesleft = file->_size - (file->_pos - file->_data);
        if(count > bytesleft)
        {
            count = bytesleft;
            file->_flags.eof = true;
        }
        if(count)
        {
            memcpy(buffer, file->_pos, count);
            file->_pos += count;
        }
        return count;
    }
}

boolean DFile_AtEnd(DFile* file)
{
    errorIfNotValid(file, "DFile::AtEnd");
    if(file->_flags.reference)
    {
        return DFile_AtEnd(AbstractFile_Handle(file->_file));
    }
    return (file->_flags.eof != 0);
}

unsigned char DFile_GetC(DFile* file)
{
    errorIfNotValid(file, "DFile::GetC");
    {
    unsigned char ch = 0;
    DFile_Read(file, (uint8_t*)&ch, 1);
    return ch;
    }
}

size_t DFile_Tell(DFile* file)
{
    errorIfNotValid(file, "DFile::Tell");
    if(file->_flags.reference)
    {
        return DFile_Tell(AbstractFile_Handle(file->_file));
    }
    else
    {
        if(file->_hndl)
            return (size_t) ftell(file->_hndl);
        return file->_pos - file->_data;
    }
}

size_t DFile_Seek(DFile* file, size_t offset, int whence)
{
    if(file->_flags.reference)
    {
        return DFile_Seek(AbstractFile_Handle(file->_file), offset, whence);
    }
    else
    {
        size_t oldpos = DFile_Tell(file);

        file->_flags.eof = false;
        if(file->_hndl)
        {
            fseek(file->_hndl, (long) (file->_baseOffset + offset), whence);
        }
        else
        {
            if(whence == SEEK_SET)
                file->_pos = file->_data + offset;
            else if(whence == SEEK_END)
                file->_pos = file->_data + (file->_size + offset);
            else if(whence == SEEK_CUR)
                file->_pos += offset;
        }

        return oldpos;
    }
}

void DFile_Rewind(DFile* file)
{
    DFile_Seek(file, 0, SEEK_SET);
}

#if _DEBUG
void DFile_Print(const DFile* file)
{
    errorIfNotValid(file, "DFile::Print");
    {
    byte id[16];
    F_GenerateFileId(Str_Text(AbstractFile_Path(DFile_File_Const(file))), id);
    F_PrintFileId(id);
    Con_Printf(" - \"%s\" [%p]\n", F_PrettyPath(Str_Text(AbstractFile_Path(DFile_File_Const(file)))), (void*)file);
    }
}
#endif
