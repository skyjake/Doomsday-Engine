/**
 * @file dfile.cpp
 * Reference/handle to a unique file in the engine's virtual file system.
 *
 * @ingroup fs
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
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

struct de::DFile::Instance
{
    /// The referenced file (if any).
    de::AbstractFile* file;

    /// Either the FileList which owns this or the next DFile in the used object pool.
    void* list;

    struct dfile_flags_s {
        uint open:1; /// Presently open.
        uint eof:1; /// Reader has reached the end of the stream.
        uint reference:1; /// This handle is a reference to another dfile instance.
    } flags;

    /// Offset from start of owning package.
    size_t baseOffset;

    FILE* hndl;
    size_t size;
    uint8_t* data;
    uint8_t* pos;

    Instance() : file(0), list(0), baseOffset(0), hndl(0), size(0), data(0), pos(0)
    {
        flags.eof  = false;
        flags.open = false;
        flags.reference = false;
    }
};

#if 0
// Has the handle builder been initialized yet?
static bool inited;

// A mutex is used to protect access to the shared handle block allocator and object pool.
static mutex_t mutex;

// File handles are allocated by this block allocator.
static blockset_t* handleBlockSet;

// Head of the llist of used file handles, for recycling.
static DFile* usedHandles;
#endif

using de::DFileBuilder;

static void errorIfNotValid(de:: DFile const& file, char const* callerName)
{
    if(file.isValid()) return;
    Con_Error("%s: Instance %p has not yet been initialized.", callerName, (void*)&file);
    exit(1); // Unreachable.
}

void DFileBuilder::init(void)
{
#if 0
    if(!inited)
    {
        mutex = Sys_CreateMutex(0);
        inited = true;
        return;
    }
    Con_Error("DFileBuilder::init: Already initialized.");
#endif
}

void DFileBuilder::shutdown(void)
{
#if 0
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
    Con_Error("DFileBuilder::shutdown: Not presently initialized.");
#endif
#endif
}

de::DFile* DFileBuilder::fromAbstractFileLump(de::AbstractFile& container, int lumpIdx, bool dontBuffer)
{
    LumpInfo const* info = F_LumpInfo(&reinterpret_cast<struct abstractfile_s&>(container), lumpIdx);
    if(!info) return NULL;

    de::DFile* file = new de::DFile();
    // Init and load in the lump data.
    file->d->flags.open = true;
    if(!dontBuffer)
    {
        file->d->size = info->size;
        file->d->pos = file->d->data = (uint8_t*) M_Malloc(file->d->size);
        if(!file->d->data)
            Con_Error("DFileBuilder::newFromAbstractFileLump: Failed on allocation of %lu bytes for data buffer.",
                (unsigned long) file->d->size);
#if _DEBUG
        VERBOSE2(
            AutoStr* path = F_ComposeLumpPath(&reinterpret_cast<struct abstractfile_s&>(container), lumpIdx);
            Con_Printf("DFile [%p] buffering \"%s:%s\"...\n", (void*)file,
                       F_PrettyPath(Str_Text(container.path())),
                       F_PrettyPath(Str_Text(path)));
        )
#endif
        F_ReadLumpSection(&reinterpret_cast<struct abstractfile_s&>(container), lumpIdx, (uint8_t*)file->d->data, 0, info->size);
    }
    return file;
}

de::DFile* DFileBuilder::fromAbstractFile(de::AbstractFile& af)
{
    de::DFile* file = new de::DFile();
    file->d->file = &af;
    file->d->flags.open = true;
    file->d->flags.reference = true;
    return file;
}

de::DFile* DFileBuilder::fromFile(FILE& hndl, size_t baseOffset)
{
    de::DFile* file = new de::DFile();
    file->d->flags.open = true;
    file->d->hndl = &hndl;
    file->d->baseOffset = baseOffset;
    return file;
}

de::DFile* DFileBuilder::dup(de::DFile const& hndl)
{
    de::DFile* clone = new de::DFile();
    clone->d->flags.open = true;
    clone->d->flags.reference = true;
    clone->d->file = reinterpret_cast<de::AbstractFile*>(DFile_File_const(&reinterpret_cast<struct dfile_s const&>(hndl)));
    return clone;
}

de::DFile::DFile(void)
{
#if 0
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
#endif

    d = new Instance();
}

de::DFile::~DFile()
{
    close();

#if 0
    // Copy this file to the used object pool for recycling.
    Sys_Lock(mutex);
    d->file = 0;
    d->list = usedHandles;
    usedHandles = this;
    Sys_Unlock(mutex);
#endif

    delete d;
}

de::DFile& de::DFile::close()
{
    if(!d->flags.open) return *this;
    if(d->hndl)
    {
        fclose(d->hndl); d->hndl = NULL;
    }
    else
    {
        // Free the stored data.
        if(d->data)
        {
            M_Free(d->data); d->data = NULL;
        }
    }
    d->pos = NULL;
    d->flags.open = false;
    return *this;
}

bool de::DFile::isValid() const
{
    /// @todo write me.
    return true;
}

struct filelist_s* de::DFile::list()
{
    errorIfNotValid(*this, "DFile::list");
    return (struct filelist_s*)d->list;
}

de::DFile& de::DFile::setList(struct filelist_s* list)
{
    d->list = list;
    return *this;
}

de::AbstractFile* de::DFile::file()
{
    errorIfNotValid(*this, "DFile::file");
    return d->file;
}

de::AbstractFile* de::DFile::file() const
{
    errorIfNotValid(*this, "DFile::file const");
    return d->file;
}

size_t de::DFile::baseOffset() const
{
    if(d->flags.reference)
    {
        return d->file->handle()->baseOffset();
    }
    return d->baseOffset;
}

size_t de::DFile::length()
{
    errorIfNotValid(*this, "DFile::Length");
    if(d->flags.reference)
    {
        return d->file->handle()->length();
    }
    else
    {
        size_t currentPosition = seek(0, SeekEnd);
        size_t length = tell();
        seek(currentPosition, SeekSet);
        return length;
    }
}

size_t de::DFile::read(uint8_t* buffer, size_t count)
{
    errorIfNotValid(*this, "DFile::read");
    if(d->flags.reference)
    {
        return d->file->handle()->read(buffer, count);
    }
    else
    {
        if(d->hndl)
        {
            // Normal file.
            count = fread(buffer, 1, count, d->hndl);
            if(feof(d->hndl))
                d->flags.eof = true;
            return count;
        }

        // Is there enough room in the file?
        size_t bytesleft = d->size - (d->pos - d->data);
        if(count > bytesleft)
        {
            count = bytesleft;
            d->flags.eof = true;
        }
        if(count)
        {
            memcpy(buffer, d->pos, count);
            d->pos += count;
        }
        return count;
    }
}

bool de::DFile::atEnd()
{
    errorIfNotValid(*this, "DFile::atEnd");
    if(d->flags.reference)
    {
        return d->file->handle()->atEnd();
    }
    return (d->flags.eof != 0);
}

unsigned char de::DFile::getC()
{
    errorIfNotValid(*this, "DFile::getC");

    unsigned char ch = 0;
    read((uint8_t*)&ch, 1);
    return ch;
}

size_t de::DFile::tell()
{
    errorIfNotValid(*this, "DFile::tell");
    if(d->flags.reference)
    {
        return d->file->handle()->tell();
    }
    else
    {
        if(d->hndl)
            return (size_t) ftell(d->hndl);
        return d->pos - d->data;
    }
}

size_t de::DFile::seek(size_t offset, SeekMethod whence)
{
    if(d->flags.reference)
    {
        return d->file->handle()->seek(offset, whence);
    }
    else
    {
        size_t oldpos = tell();

        d->flags.eof = false;
        if(d->hndl)
        {
            int fwhence = whence == SeekSet? SEEK_SET :
                          whence == SeekCur? SEEK_CUR : SEEK_END;

            fseek(d->hndl, (long) (d->baseOffset + offset), fwhence);
        }
        else
        {
            if(whence == SeekSet)
                d->pos = d->data + offset;
            else if(whence == SeekEnd)
                d->pos = d->data + (d->size + offset);
            else if(whence == SeekCur)
                d->pos += offset;
        }

        return oldpos;
    }
}

de::DFile& de::DFile::rewind()
{
    seek(0, SeekSet);
    return *this;
}

#if _DEBUG
void de::DFile::debugPrint(de::DFile const& hndl)
{
    byte id[16];
    F_GenerateFileId(Str_Text(hndl.file()->path()), id);
    F_PrintFileId(id);
    Con_Printf(" - \"%s\" [%p]\n", F_PrettyPath(Str_Text(hndl.file()->path())), (void*)&hndl);
}
#endif

/**
 * C Wrapper API:
 */

#define TOINTERNAL(inst) \
    (inst) != 0? reinterpret_cast<de::DFile*>(inst) : NULL

#define TOINTERNAL_CONST(inst) \
    (inst) != 0? reinterpret_cast<de::DFile const*>(inst) : NULL

#define SELF(inst) \
    DENG2_ASSERT(inst); \
    de::DFile* self = TOINTERNAL(inst)

#define SELF_CONST(inst) \
    DENG2_ASSERT(inst); \
    de::DFile const* self = TOINTERNAL_CONST(inst)

void DFile_Close(DFile* hndl)
{
    SELF(hndl);
    self->close();
}

boolean DFile_IsValid(DFile const* hndl)
{
    SELF_CONST(hndl);
    return self->isValid();
}

size_t DFile_Length(DFile* hndl)
{
    SELF(hndl);
    return self->length();
}

size_t DFile_BaseOffset(DFile const* hndl)
{
    SELF_CONST(hndl);
    return self->baseOffset();
}

size_t DFile_Read(DFile* hndl, uint8_t* buffer, size_t count)
{
    SELF(hndl);
    return self->read(buffer, count);
}

unsigned char DFile_GetC(DFile* hndl)
{
    SELF(hndl);
    return self->getC();
}

boolean DFile_AtEnd(DFile* hndl)
{
    SELF(hndl);
    return self->atEnd();
}

size_t DFile_Tell(DFile* hndl)
{
    SELF(hndl);
    return self->tell();
}

size_t DFile_Seek(DFile* hndl, size_t offset, SeekMethod whence)
{
    SELF(hndl);
    return self->seek(offset, whence);
}

void DFile_Rewind(DFile* hndl)
{
    SELF(hndl);
    self->rewind();
}

AbstractFile* DFile_File(DFile* hndl)
{
    SELF(hndl);
    return reinterpret_cast<AbstractFile*>(self->file());
}

AbstractFile* DFile_File_const(DFile const* hndl)
{
    SELF_CONST(hndl);
    return reinterpret_cast<AbstractFile*>(self->file());
}

struct filelist_s* DFile_List(DFile* hndl)
{
    SELF(hndl);
    return self->list();
}

DFile* DFile_SetList(DFile* hndl, struct filelist_s* list)
{
    SELF(hndl);
    return reinterpret_cast<DFile*>(&self->setList(list));
}

#if _DEBUG
void DFile_DebugPrint(DFile const* hndl)
{
    if(!hndl) return;
    de::DFile::debugPrint(*reinterpret_cast<de::DFile const*>(hndl));
}
#endif
