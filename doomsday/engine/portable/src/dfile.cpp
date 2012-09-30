/**
 * @file dfile.cpp
 *
 * @ingroup fs
 *
 * @author Copyright &copy; 2012 Daniel Swanson <danij@dengine.net>
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

#include <cctype>
#include <ctime>
#include <sys/stat.h>

#include "de_platform.h"
#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"

#include "blockset.h"
#include "dfile.h"

#include <de/memory.h>

struct dfile_s
{
    /// The referenced abstract file (if any).
    AbstractFile* file;

    /// Either the FileList which owns this or the next DFile in the used object pool.
    void* list;

    struct dfile_flags_s {
        uint open:1; /// Presently open.
        uint eof:1; /// Reader has reached the end of the stream.
        uint reference; /// This handle is a reference to another dfile instance.
    } flags;

    /// Offset from start of owning package.
    size_t baseOffset;

    FILE* hndl;
    size_t size;
    uint8_t* data;
    uint8_t* pos;
};

// Has the handle builder been initialized yet?
static bool inited;

// A mutex is used to protect access to the shared handle block allocator and object pool.
static mutex_t mutex;

// File handles are allocated by this block allocator.
static blockset_t* handleBlockSet;

// Head of the llist of used file handles, for recycling.
static DFile* usedHandles;

static void errorIfNotValid(DFile const* file, const char* callerName)
{
    if(DFile_IsValid(file)) return;
    Con_Error("%s: Instance %p has not yet been initialized.", callerName, (void*)file);
    exit(1); // Unreachable.
}

void DFileBuilder_Init(void)
{
    if(!inited)
    {
        mutex = Sys_CreateMutex(0);
        inited = true;
        return;
    }
    Con_Error("DFileBuilder_Init: Already initialized.");
}

void DFileBuilder_Shutdown(void)
{
    if(inited)
    {
        Sys_Lock(mutex);
        BlockSet_Delete(handleBlockSet); handleBlockSet = 0;
        usedHandles = 0;
        Sys_Unlock(mutex);
        Sys_DestroyMutex(mutex); mutex = 0;
        inited = false;
        return;
    }
#if _DEBUG
    Con_Error("DFileBuilder_Shutdown: Not presently initialized.");
#endif
}

DFile* DFileBuilder_NewFromAbstractFileLump(AbstractFile* container, int lumpIdx, boolean dontBuffer)
{
    LumpInfo const* info = F_LumpInfo(container, lumpIdx);
    DFile* file;
    if(!info) return NULL;

    file = DFile_New();
    // Init and load in the lump data.
    file->flags.open = true;
    if(!dontBuffer)
    {
        file->size = info->size;
        file->pos = file->data = (uint8_t*) M_Malloc(file->size);
        if(!file->data)
            Con_Error("DFileBuilder_NewFromAbstractFileLump: Failed on allocation of %lu bytes for data buffer.",
                (unsigned long) file->size);
#if _DEBUG
        VERBOSE2(
            AutoStr* path = F_ComposeLumpPath(container, lumpIdx);
            Con_Printf("DFile [%p] buffering \"%s:%s\"...\n", (void*)file,
                       F_PrettyPath(Str_Text(AbstractFile_Path(container))),
                       F_PrettyPath(Str_Text(path)));
        )
#endif
        F_ReadLumpSection(container, lumpIdx, (uint8_t*)file->data, 0, info->size);
    }
    return file;
}

DFile* DFileBuilder_NewFromAbstractFile(AbstractFile* af)
{
    DFile* file = DFile_New();
    DENG_ASSERT(af);
    file->file = af;
    file->flags.open = true;
    file->flags.reference = true;
    return file;
}

DFile* DFileBuilder_NewFromFile(FILE* hndl, size_t baseOffset)
{
    DFile* file = DFile_New();
    DENG_ASSERT(hndl);
    file->flags.open = true;
    file->hndl = hndl;
    file->baseOffset = baseOffset;
    return file;
}

DFile* DFileBuilder_NewCopy(const DFile* file)
{
    DENG_ASSERT(inited && file);

    DFile* clone = DFile_New();
    clone->flags.open = true;
    clone->flags.reference = true;
    clone->file = DFile_File_const(file);
    return clone;
}

DFile* DFile_New(void)
{
    Sys_Lock(mutex);
    DFile* file;
    if(usedHandles)
    {
        file = usedHandles;
        usedHandles = (DFile*) file->list;
    }
    else
    {
        if(!handleBlockSet)
        {
            handleBlockSet = BlockSet_New(sizeof(DFile), 64);
        }
        file = (DFile*) BlockSet_Allocate(handleBlockSet);
    }
    Sys_Unlock(mutex);

    file->list = NULL;
    file->file = NULL;
    file->flags.eof  = false;
    file->flags.open = false;
    file->flags.reference = false;
    file->baseOffset = 0;
    file->size = 0;
    file->hndl = NULL;
    file->data = NULL;
    file->pos  = 0;
    return file;
}

void DFile_Delete(DFile* file, boolean recycle)
{
    DENG_ASSERT(inited && file);

    DFile_Close(file);
    file->file = NULL;
    file->list = NULL;

    if(!recycle)
    {
        // Memory for file will be free'd along with handleBlockSet
        return;
    }
    // Copy this file to the used object pool for recycling.
    Sys_Lock(mutex);
    file->list = usedHandles;
    usedHandles = file;
    Sys_Unlock(mutex);
}

void DFile_Close(DFile* file)
{
    DENG_ASSERT(file);
    if(!file->flags.open) return;
    if(file->hndl)
    {
        fclose(file->hndl);
        file->hndl = NULL;
    }
    else
    {   // Free the stored data.
        if(file->data)
        {
            M_Free(file->data); file->data = NULL;
        }
    }
    file->pos = NULL;
    file->flags.open = false;
}

boolean DFile_IsValid(DFile const* file)
{
    DENG_ASSERT(file);
    /// @todo write me.
    return true;
}

FileList* DFile_List(DFile* file)
{
    errorIfNotValid(file, "DFile_List");
    return (FileList*)file->list;
}

DFile* DFile_SetList(DFile* file, FileList* list)
{
    DENG_ASSERT(file);
    file->list = list;
    return file;
}

AbstractFile* DFile_File(DFile* file)
{
    errorIfNotValid(file, "DFile_File");
    return file->file;
}

AbstractFile* DFile_File_const(DFile const* file)
{
    errorIfNotValid(file, "DFile_File_const");
    return file->file;
}

size_t DFile_BaseOffset(DFile const* file)
{
    DENG_ASSERT(file);
    if(file->flags.reference)
    {
        return DFile_BaseOffset(AbstractFile_Handle(file->file));
    }
    return file->baseOffset;
}

size_t DFile_Length(DFile* file)
{
    errorIfNotValid(file, "DFile_Length");
    if(file->flags.reference)
    {
        return DFile_Length(AbstractFile_Handle(file->file));
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
    errorIfNotValid(file, "DFile_Read");
    if(file->flags.reference)
    {
        return DFile_Read(AbstractFile_Handle(file->file), buffer, count);
    }
    else
    {
        if(file->hndl)
        {
            // Normal file.
            count = fread(buffer, 1, count, file->hndl);
            if(feof(file->hndl))
                file->flags.eof = true;
            return count;
        }

        // Is there enough room in the file?
        size_t bytesleft = file->size - (file->pos - file->data);
        if(count > bytesleft)
        {
            count = bytesleft;
            file->flags.eof = true;
        }
        if(count)
        {
            memcpy(buffer, file->pos, count);
            file->pos += count;
        }
        return count;
    }
}

boolean DFile_AtEnd(DFile* file)
{
    errorIfNotValid(file, "DFile_AtEnd");
    if(file->flags.reference)
    {
        return DFile_AtEnd(AbstractFile_Handle(file->file));
    }
    return (file->flags.eof != 0);
}

unsigned char DFile_GetC(DFile* file)
{
    errorIfNotValid(file, "DFile_GetC");

    unsigned char ch = 0;
    DFile_Read(file, (uint8_t*)&ch, 1);
    return ch;
}

size_t DFile_Tell(DFile* file)
{
    errorIfNotValid(file, "DFile_Tell");
    if(file->flags.reference)
    {
        return DFile_Tell(AbstractFile_Handle(file->file));
    }
    else
    {
        if(file->hndl)
            return (size_t) ftell(file->hndl);
        return file->pos - file->data;
    }
}

size_t DFile_Seek(DFile* file, size_t offset, int whence)
{
    if(file->flags.reference)
    {
        return DFile_Seek(AbstractFile_Handle(file->file), offset, whence);
    }
    else
    {
        size_t oldpos = DFile_Tell(file);

        file->flags.eof = false;
        if(file->hndl)
        {
            fseek(file->hndl, (long) (file->baseOffset + offset), whence);
        }
        else
        {
            if(whence == SEEK_SET)
                file->pos = file->data + offset;
            else if(whence == SEEK_END)
                file->pos = file->data + (file->size + offset);
            else if(whence == SEEK_CUR)
                file->pos += offset;
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
    errorIfNotValid(file, "DFile_Print");

    byte id[16];
    F_GenerateFileId(Str_Text(AbstractFile_Path(DFile_File_const(file))), id);
    F_PrintFileId(id);
    Con_Printf(" - \"%s\" [%p]\n", F_PrettyPath(Str_Text(AbstractFile_Path(DFile_File_const(file)))), (void*)file);
}
#endif
