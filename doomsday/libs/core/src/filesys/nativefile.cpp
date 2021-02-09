/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2009-2021 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/nativefile.h"
#include "de/directoryfeed.h"
#include "de/guard.h"
#include "de/math.h"

#include <the_Foundation/file.h>

namespace de {

DE_PIMPL(NativeFile)
{
    NativePath nativePath;     // Path of the native file in the OS file system.
    iFile *    file = nullptr; // NOTE: One NativeFile shouldn't be accessed by multiple
                               // threads simultaneously (each read/write mutexed).
    Impl(Public *i)
        : Base(i)
    {}

    ~Impl()
    {
        DE_ASSERT(!file);
    }

    iFile *getFile()
    {
        if (!file)
        {
            file = new_File(nativePath.toString());
            const bool isWrite = self().mode().testFlag(Write);
            // Open with Append mode so that missing files will get created.
            // Seek position will be updated anyway when something is written to the file.
            if (!open_File(file, isWrite ? append_FileMode | readWrite_FileMode : readOnly_FileMode))
            {
                iReleasePtr(&file);
                if (isWrite)
                {
                    throw OutputError("NativeFile::getFile",
                                      Stringf("Failed to write (%s)", strerror(errno)));
                }
                else
                {
                    throw InputError("NativeFile::getFile", "Failed to read " + nativePath);
                }
            }
        }
        return file;
    }

    void closeFile()
    {
        iReleasePtr(&file);
    }
};

NativeFile::NativeFile(const String &name, const NativePath &nativePath)
    : ByteArrayFile(name), d(new Impl(this))
{
    d->nativePath = nativePath;
}

NativeFile::~NativeFile()
{
    DE_GUARD(this);

    DE_NOTIFY(Deletion, i) i->fileBeingDeleted(*this);
    audienceForDeletion().clear();

    close();
    deindex();
}

String NativeFile::describe() const
{
    DE_GUARD(this);
    return Stringf("\"%s\"", d->nativePath.pretty().c_str());
}

Block NativeFile::metaId() const
{
    // Special exception: application's persistent data store will be rewritten on every
    // run so there's no point in caching it.
    if (name() == DE_STR("persist.pack"))
    {
        return Block();
    }
    return Block(File::metaId() + d->nativePath.toUtf8()).md5Hash();
}

void NativeFile::close()
{
    DE_GUARD(this);

    flush();
    DE_ASSERT(!d->file);

    d->closeFile();
}

void NativeFile::flush()
{
    DE_GUARD(this);

    d->closeFile();
    DE_ASSERT(!d->file);
}

const NativePath &NativeFile::nativePath() const
{
    DE_GUARD(this);

    return d->nativePath;
}

void NativeFile::clear()
{
    DE_GUARD(this);

    File::clear(); // checks for write access

    d->closeFile();

    if (remove(d->nativePath.toString().c_str()))
    {
        if (errno != ENOENT)
        {
            throw OutputError("NativeFile::clear",
                              "Failed to clear " + d->nativePath +
                                  Stringf(" (%s)", strerror(errno)));
        }
    }

    Status st = status();
    st.size = 0;
    st.modifiedAt = Time();
    setStatus(st);
}

NativeFile::Size NativeFile::size() const
{
    DE_GUARD(this);

    return status().size;
}

void NativeFile::get(Offset at, Byte *values, Size count) const
{
    DE_GUARD(this);

    if (at + count > size())
    {
        d->closeFile();
        /// @throw IByteArray::OffsetError  The region specified for reading extends
        /// beyond the bounds of the file.
        throw OffsetError("NativeFile::get", description() + ": cannot read past end of file " +
                          Stringf("(%zu[+%zu] > %zu)", at, count, size()));
    }

    d->getFile();
    if (pos_File(d->file) != at)
    {
        seek_File(d->file, at);
    }
    readData_File(d->file, count, values);

    // Close the native input file after the full contents have been read.
    if (at + count == size())
    {
        d->closeFile();
    }
}

void NativeFile::set(Offset at, const Byte *values, Size count)
{
    DE_GUARD(this);

    if (at > size())
    {
        /// @throw IByteArray::OffsetError  @a at specified a position beyond the
        /// end of the file.
        throw OffsetError("NativeFile::set", description() + ": cannot write past end of file");
    }

    d->getFile();
    if (pos_File(d->file) != at)
    {
        seek_File(d->file, at);
    }
    if (writeData_File(d->file, values, count) != count)
    {
        throw OutputError("NativeFile::set",
                          description() + Stringf(": error writing to file (%s)", strerror(errno)));
    }

    // Update status.
    Status st = status();
    st.size = max(st.size, at + count);
    st.modifiedAt = Time();
    setStatus(st);
}

NativeFile *NativeFile::newStandalone(const NativePath &nativePath)
{
    std::unique_ptr<NativeFile> file(new NativeFile(nativePath.fileName(), nativePath));
    if (nativePath.exists())
    {
        // Sync status with the real status.
        file->setStatus(DirectoryFeed::fileStatus(nativePath));
    }
    return file.release();
}

void NativeFile::setMode(const Flags &newMode)
{
    DE_GUARD(this);

    close();
    File::setMode(newMode);
}

} // namespace de
