/** @file filehandle.cpp
 *
 * Reference/handle to a unique file in the engine's virtual file system.
 *
 * @ingroup fs
 *
 * @authors Copyright &copy; 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/filesys/file.h"

#include <cctype>
#include <ctime>
#include <sys/stat.h>

#include <de/legacy/memory.h>
#include <de/legacy/memoryblockset.h>
#include <de/logbuffer.h>
#include <de/nativepath.h>

namespace res {

using namespace de;

DE_PIMPL_NOREF(FileHandle)
{
    /// The referenced file (if any).
    File1 *file;

    /// Either the FileList which owns this or the next FileHandle in the used object pool.
    void *list;

    struct dfile_flags_s {
        uint open:1;       ///< Presently open.
        uint eof:1;        ///< Reader has reached the end of the stream.
        uint reference:1;  ///< This handle is a reference to another dfile instance.
    } flags;

    /// Offset from start of owning package.
    size_t baseOffset;

    FILE *hndl;
    size_t size;
    uint8_t *data;
    uint8_t *pos;

    Impl() : file(0), list(0), baseOffset(0), hndl(0), size(0), data(0), pos(0)
    {
        flags.eof  = false;
        flags.open = false;
        flags.reference = false;
    }
};

static void errorIfNotValid(const FileHandle &file, const char * /*callerName*/)
{
    DE_ASSERT(file.isValid());
    if (!file.isValid()) exit(1);
}

FileHandle::FileHandle()
    : d(new Impl)
{}

FileHandle::~FileHandle()
{
    close();

    // Free any cached data.
    if (d->data)
    {
        M_Free(d->data); d->data = 0;
    }
}

FileHandle &FileHandle::close()
{
    if (!d->flags.open) return *this;
    if (d->hndl)
    {
        fclose(d->hndl); d->hndl = 0;
    }
    // Free any cached data.
    if (d->data)
    {
        M_Free(d->data); d->data = 0;
    }
    d->pos        = 0;
    d->flags.open = false;
    return *this;
}

bool FileHandle::isValid() const
{
    return true;
}

FileList *FileHandle::list()
{
    errorIfNotValid(*this, "FileHandle::list");
    return reinterpret_cast<FileList *>(d->list);
}

FileHandle &FileHandle::setList(FileList *list)
{
    d->list = list;
    return *this;
}

bool FileHandle::hasFile() const
{
    errorIfNotValid(*this, "FileHandle::file");
    return !!d->file;
}

File1 &FileHandle::file()
{
    errorIfNotValid(*this, "FileHandle::file");
    return *d->file;
}

File1 &FileHandle::file() const
{
    errorIfNotValid(*this, "FileHandle::file const");
    return *d->file;
}

size_t FileHandle::baseOffset() const
{
    if (d->flags.reference)
    {
        return d->file->handle().baseOffset();
    }
    return d->baseOffset;
}

size_t FileHandle::length()
{
    errorIfNotValid(*this, "FileHandle::Length");
    if (d->flags.reference)
    {
        return d->file->handle().length();
    }
    else
    {
        const size_t currentPosition = seek(0, SeekEnd);
        const size_t length = tell();
        seek(currentPosition, SeekSet);
        return length;
    }
}

size_t FileHandle::read(uint8_t *buffer, size_t count)
{
    errorIfNotValid(*this, "FileHandle::read");
    if (d->flags.reference)
    {
        return d->file->handle().read(buffer, count);
    }
    else
    {
        if (d->hndl)
        {
            // Normal file.
            count = fread(buffer, 1, count, d->hndl);
            if (feof(d->hndl))
            {
                d->flags.eof = true;
            }
            return count;
        }

        // Is there enough room in the file?
        size_t bytesleft = d->size - (d->pos - d->data);
        if (count > bytesleft)
        {
            count = bytesleft;
            d->flags.eof = true;
        }
        if (count)
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
    if (d->flags.reference)
    {
        return d->file->handle().atEnd();
    }
    return (d->flags.eof != 0);
}

unsigned char FileHandle::getC()
{
    errorIfNotValid(*this, "FileHandle::getC");

    unsigned char ch = 0;
    read((uint8_t *)&ch, 1);
    return ch;
}

size_t FileHandle::tell()
{
    errorIfNotValid(*this, "FileHandle::tell");
    if (d->flags.reference)
    {
        return d->file->handle().tell();
    }
    else
    {
        if (d->hndl)
        {
            return (size_t) ftell(d->hndl);
        }
        return d->pos - d->data;
    }
}

size_t FileHandle::seek(size_t offset, SeekMethod whence)
{
    if (d->flags.reference)
    {
        return d->file->handle().seek(offset, whence);
    }
    else
    {
        size_t oldpos = tell();

        d->flags.eof = false;
        if (d->hndl)
        {
            int fwhence = whence == SeekSet? SEEK_SET :
                          whence == SeekCur? SEEK_CUR : SEEK_END;

            fseek(d->hndl, (long) (d->baseOffset + offset), fwhence);
        }
        else
        {
            if (whence == SeekSet)
                d->pos = d->data + offset;
            else if (whence == SeekEnd)
                d->pos = d->data + (d->size + offset);
            else if (whence == SeekCur)
                d->pos += offset;
        }

        return oldpos;
    }
}

FileHandle &FileHandle::rewind()
{
    seek(0, SeekSet);
    return *this;
}

FileHandle *FileHandle::fromFile(File1 &file) // static
{
    FileHandle *hndl = new FileHandle();
    hndl->d->file            = &file;
    hndl->d->flags.open      = true;
    hndl->d->flags.reference = true;
    return hndl;
}

FileHandle *FileHandle::fromNativeFile(FILE &file, size_t baseOffset) // static
{
    FileHandle *hndl = new FileHandle();
    hndl->d->flags.open = true;
    hndl->d->hndl       = &file;
    hndl->d->baseOffset = baseOffset;
    return hndl;
}

FileHandle *FileHandle::fromLump(File1 &lump, bool dontBuffer) // static
{
    LOG_AS("FileHandle::fromLump");

    FileHandle *hndl = new FileHandle();
    // Init and load in the lump data.
    hndl->d->file       = &lump;
    hndl->d->flags.open = true;
    if (!dontBuffer)
    {
        hndl->d->size = lump.size();
        hndl->d->pos  = hndl->d->data = (uint8_t *) M_Malloc(hndl->d->size);

        LOGDEV_RES_XVERBOSE_DEBUGONLY("[%p] Buffering \"%s:%s\"...", dintptr(hndl)
                                     << NativePath(lump.container().composePath()).pretty()
                                     << NativePath(lump.composePath()).pretty());

        lump.read((uint8_t *)hndl->d->data, 0, lump.size());
    }
    return hndl;
}

} // namespace res
