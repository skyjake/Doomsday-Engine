/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "de/ArchiveEntryFile"
#include "de/Archive"
#include "de/Block"

namespace de {

ArchiveEntryFile::ArchiveEntryFile(String const &name, Archive &archive, String const &entryPath)
    : ByteArrayFile(name), _archive(archive), _entryPath(entryPath)
{}

ArchiveEntryFile::~ArchiveEntryFile()
{
    DENG2_FOR_AUDIENCE(Deletion, i) i->fileBeingDeleted(*this);
    audienceForDeletion.clear();
    
    deindex();
}

void ArchiveEntryFile::clear()
{
    File::clear();
    
    archive().entryBlock(_entryPath).clear();
    
    // Update status.
    Status st = status();
    st.size = 0;
    st.modifiedAt = Time();
    setStatus(st);
}

IByteArray::Size ArchiveEntryFile::size() const
{
    return archive().entryBlock(_entryPath).size();
}

void ArchiveEntryFile::get(Offset at, Byte *values, Size count) const
{
    archive().entryBlock(_entryPath).get(at, values, count);
}

void ArchiveEntryFile::set(Offset at, Byte const *values, Size count)
{
    verifyWriteAccess();
    
    // The entry will be marked for recompression (due to non-const access).
    Block &entryBlock = archive().entryBlock(_entryPath);
    entryBlock.set(at, values, count);
    
    // Update status.
    Status st = status();
    st.size = entryBlock.size();
    st.modifiedAt = Time();
    setStatus(st);
}

} // namespace de
