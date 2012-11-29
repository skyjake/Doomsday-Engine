/**
 * @file filehandle.cpp
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

#include "filehandle.h"

#include <de/memory.h>
#include <de/memoryblockset.h>
#include <de/NativePath>

namespace de {

struct FileHandle::Instance
{
    /// The referenced file (if any).
    File1* file;

    /// Either the FileList which owns this or the next FileHandle in the used object pool.
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
static de::FileHandle* usedHandles;
#endif

static void errorIfNotValid(de::FileHandle const& file, char const* callerName)
{
    if(file.isValid()) return;
    Con_Error("%s: Instance %p has not yet been initialized.", callerName, (void*)&file);
    exit(1); // Unreachable.
}

void FileHandleBuilder::init(void)
{
#if 0
    if(!inited)
    {
        mutex = Sys_CreateMutex(0);
        inited = true;
        return;
    }
    Con_Error("FileHandleBuilder::init: Already initialized.");
#endif
}

void FileHandleBuilder::shutdown(void)
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
    Con_Error("FileHandleBuilder::shutdown: Not presently initialized.");
#endif
#endif
}

FileHandle* FileHandleBuilder::fromLump(File1& lump, bool dontBuffer)
{
    LOG_AS("FileHandle::fromLump");

    de::FileHandle* hndl = new de::FileHandle();
    // Init and load in the lump data.
    hndl->d->file = &lump;
    hndl->d->flags.open = true;
    if(!dontBuffer)
    {
        hndl->d->size = lump.size();
        hndl->d->pos = hndl->d->data = (uint8_t*) M_Malloc(hndl->d->size);
        if(!hndl->d->data) Con_Error("FileHandleBuilder::fromFileLump: Failed on allocation of %lu bytes for data buffer.", (unsigned long) hndl->d->size);

#if _DEBUG
        LOG_VERBOSE("[%p] Buffering \"%s:%s\"...")
            << dintptr(hndl) << NativePath(lump.container().composePath()).pretty() << NativePath(lump.composePath()).pretty();
#endif
        lump.read((uint8_t*)hndl->d->data, 0, lump.size());
    }
    return hndl;
}

FileHandle* FileHandleBuilder::fromFile(File1& file)
{
    de::FileHandle* hndl = new de::FileHandle();
    hndl->d->file = &file;
    hndl->d->flags.open = true;
    hndl->d->flags.reference = true;
    return hndl;
}

FileHandle* FileHandleBuilder::fromNativeFile(FILE& file, size_t baseOffset)
{
    de::FileHandle* hndl = new de::FileHandle();
    hndl->d->flags.open = true;
    hndl->d->hndl = &file;
    hndl->d->baseOffset = baseOffset;
    return hndl;
}

FileHandle* FileHandleBuilder::dup(de::FileHandle const& hndl)
{
    de::FileHandle* clone = new de::FileHandle();
    clone->d->flags.open = true;
    clone->d->flags.reference = true;
    clone->d->file = &hndl.file();
    return clone;
}

FileHandle::FileHandle(void)
{
#if 0
    Sys_Lock(mutex);
    de::FileHandle* file;
    if(usedHandles)
    {
        file = usedHandles;
        usedHandles = (de::FileHandle*) file->list;
    }
    else
    {
        if(!handleBlockSet)
        {
            handleBlockSet = BlockSet_New(sizeof(de::FileHandle), 64);
        }
        file = (de::FileHandle*) BlockSet_Allocate(handleBlockSet);
    }
    Sys_Unlock(mutex);
#endif

    d = new Instance();
}

FileHandle::~FileHandle()
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

FileHandle& FileHandle::close()
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

bool FileHandle::isValid() const
{
    /// @todo write me.
    return true;
}

struct filelist_s* FileHandle::list()
{
    errorIfNotValid(*this, "FileHandle::list");
    return (struct filelist_s*)d->list;
}

FileHandle& FileHandle::setList(struct filelist_s* list)
{
    d->list = list;
    return *this;
}

bool FileHandle::hasFile() const
{
    errorIfNotValid(*this, "FileHandle::file");
    return !!d->file;
}

File1& FileHandle::file()
{
    errorIfNotValid(*this, "FileHandle::file");
    return *d->file;
}

File1& FileHandle::file() const
{
    errorIfNotValid(*this, "FileHandle::file const");
    return *d->file;
}

size_t FileHandle::baseOffset() const
{
    if(d->flags.reference)
    {
        return d->file->handle().baseOffset();
    }
    return d->baseOffset;
}

size_t FileHandle::length()
{
    errorIfNotValid(*this, "FileHandle::Length");
    if(d->flags.reference)
    {
        return d->file->handle().length();
    }
    else
    {
        size_t currentPosition = seek(0, SeekEnd);
        size_t length = tell();
        seek(currentPosition, SeekSet);
        return length;
    }
}

size_t FileHandle::read(uint8_t* buffer, size_t count)
{
    errorIfNotValid(*this, "FileHandle::read");
    if(d->flags.reference)
    {
        return d->file->handle().read(buffer, count);
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

bool FileHandle::atEnd()
{
    errorIfNotValid(*this, "FileHandle::atEnd");
    if(d->flags.reference)
    {
        return d->file->handle().atEnd();
    }
    return (d->flags.eof != 0);
}

unsigned char FileHandle::getC()
{
    errorIfNotValid(*this, "FileHandle::getC");

    unsigned char ch = 0;
    read((uint8_t*)&ch, 1);
    return ch;
}

size_t FileHandle::tell()
{
    errorIfNotValid(*this, "FileHandle::tell");
    if(d->flags.reference)
    {
        return d->file->handle().tell();
    }
    else
    {
        if(d->hndl)
            return (size_t) ftell(d->hndl);
        return d->pos - d->data;
    }
}

size_t FileHandle::seek(size_t offset, SeekMethod whence)
{
    if(d->flags.reference)
    {
        return d->file->handle().seek(offset, whence);
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

de::FileHandle& FileHandle::rewind()
{
    seek(0, SeekSet);
    return *this;
}

} // namespace de

/**
 * C Wrapper API:
 */

#define TOINTERNAL(inst) \
    (inst) != 0? reinterpret_cast<de::FileHandle*>(inst) : NULL

#define TOINTERNAL_CONST(inst) \
    (inst) != 0? reinterpret_cast<de::FileHandle const*>(inst) : NULL

#define SELF(inst) \
    DENG2_ASSERT(inst); \
    de::FileHandle* self = TOINTERNAL(inst)

#define SELF_CONST(inst) \
    DENG2_ASSERT(inst); \
    de::FileHandle const* self = TOINTERNAL_CONST(inst)

void FileHandle_Delete(struct filehandle_s* hndl)
{
    if(!hndl) return;
    SELF(hndl);
    delete self;
}

void FileHandle_Close(struct filehandle_s* hndl)
{
    SELF(hndl);
    self->close();
}

boolean FileHandle_IsValid(struct filehandle_s const* hndl)
{
    SELF_CONST(hndl);
    return self->isValid();
}

size_t FileHandle_Length(struct filehandle_s* hndl)
{
    SELF(hndl);
    return self->length();
}

size_t FileHandle_BaseOffset(struct filehandle_s const* hndl)
{
    SELF_CONST(hndl);
    return self->baseOffset();
}

size_t FileHandle_Read(struct filehandle_s* hndl, uint8_t* buffer, size_t count)
{
    SELF(hndl);
    return self->read(buffer, count);
}

unsigned char FileHandle_GetC(struct filehandle_s* hndl)
{
    SELF(hndl);
    return self->getC();
}

boolean FileHandle_AtEnd(struct filehandle_s* hndl)
{
    SELF(hndl);
    return self->atEnd();
}

size_t FileHandle_Tell(struct filehandle_s* hndl)
{
    SELF(hndl);
    return self->tell();
}

size_t FileHandle_Seek(struct filehandle_s* hndl, size_t offset, SeekMethod whence)
{
    SELF(hndl);
    return self->seek(offset, whence);
}

void FileHandle_Rewind(struct filehandle_s* hndl)
{
    SELF(hndl);
    self->rewind();
}

struct file1_s* FileHandle_File(struct filehandle_s* hndl)
{
    SELF(hndl);
    return reinterpret_cast<struct file1_s*>(&self->file());
}

struct file1_s* FileHandle_File_const(struct filehandle_s const* hndl)
{
    SELF_CONST(hndl);
    return reinterpret_cast<struct file1_s*>(&self->file());
}
