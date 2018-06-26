/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/ArchiveEntryFile"
#include "de/ArchiveFeed"
#include "de/Archive"
#include "de/Block"
#include "de/Guard"

namespace de {

DE_PIMPL_NOREF(ArchiveEntryFile)
{
    Archive *archive;

    /// Path of the entry within the archive.
    Path entryPath;

    /// Pointer to the data of the entry within the archive.
    Block const *readBlock = nullptr;

    Block const &entryData()
    {
        if (!readBlock)
        {
#if 0
            {
                static Lockable dbg;
                DE_GUARD(dbg);
                qDebug() << "--------\nAEF being read" << entryPath;
                DE_PRINT_BACKTRACE();
            }
#endif
            readBlock = &const_cast<Archive const *>(archive)->entryBlock(entryPath);
        }
        return *readBlock;
    }
};

ArchiveEntryFile::ArchiveEntryFile(String const &name, Archive &archive, String const &entryPath)
    : ByteArrayFile(name)
    , d(new Impl)
{
    d->archive   = &archive;
    d->entryPath = entryPath;
}

ArchiveEntryFile::~ArchiveEntryFile()
{
    DE_GUARD(this);

    DE_FOR_AUDIENCE2(Deletion, i) i->fileBeingDeleted(*this);
    audienceForDeletion().clear();

    deindex();
}

String ArchiveEntryFile::entryPath() const
{
    return d->entryPath;
}

String ArchiveEntryFile::describe() const
{
    DE_GUARD(this);

    return String::format("archive entry \"%s\"", d->entryPath.c_str());
}

void ArchiveEntryFile::clear()
{
    DE_GUARD(this);

    verifyWriteAccess();

    File::clear();

    archive().entryBlock(d->entryPath).clear();

    // Update status.
    Status st = status();
    st.size = 0;
    st.modifiedAt = Time();
    setStatus(st);
}

void ArchiveEntryFile::flush()
{
    ByteArrayFile::flush();
    if (ArchiveFeed *feed = maybeAs<ArchiveFeed>(originFeed()))
    {
        feed->rewriteFile();
    }
}

Block ArchiveEntryFile::metaId() const
{
    Block data = File::metaId() + d->entryPath.toUtf8();
    if (File const *sourceFile = maybeAs<File const>(archive().source()))
    {
        data += sourceFile->metaId();
    }
    return data.md5Hash();
}

Archive &ArchiveEntryFile::archive()
{
    return *d->archive;
}

Archive const &ArchiveEntryFile::archive() const
{
    return *d->archive;
}

void ArchiveEntryFile::uncache() const
{
    DE_GUARD(this);

    if (d->readBlock)
    {
        archive().uncacheBlock(d->entryPath);
        d->readBlock = nullptr;
    }
}

IByteArray::Size ArchiveEntryFile::size() const
{
    DE_GUARD(this);

    return archive().entryStatus(d->entryPath).size;
}

void ArchiveEntryFile::get(Offset at, Byte *values, Size count) const
{
    DE_GUARD(this);

    d->entryData().get(at, values, count);
}

void ArchiveEntryFile::set(Offset at, Byte const *values, Size count)
{
    DE_GUARD(this);

    verifyWriteAccess();

    // The entry will be marked for recompression (due to non-const access).
    Block &entryBlock = archive().entryBlock(d->entryPath);
    entryBlock.set(at, values, count);

    // Update status.
    Status st = status();
    st.size = entryBlock.size();
    // Timestamps must match, otherwise would be pruned needlessly.
    st.modifiedAt = archive().entryStatus(d->entryPath).modifiedAt;
    setStatus(st);
}

} // namespace de
