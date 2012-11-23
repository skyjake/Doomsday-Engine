/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Archive"

namespace de {

struct Archive::Instance
{
    Archive& self;

    /// Source data provided at construction.
    const IByteArray* source;

    /// Index maps entry paths to their metadata.
    Archive::Index index;

    /// Contents of the archive have been modified.
    bool modified;

    Instance(Archive& a, const IByteArray* src) : self(a), source(src), modified(false)
    {}

    void readEntry(const String& path, IBlock& deserializedData) const
    {
        Archive::Index::const_iterator found = index.find(path);
        if(found == index.end())
        {
            /// @throw NotFoundError @a path was not found in the archive.
            throw NotFoundError("Archive::readEntry",
                "Entry '" + path + "' cannot not found in the archive");
        }

        const Entry& entry = *found.value();

        if(!entry.size)
        {
            // Empty entry; nothing to do.
            deserializedData.clear();
            return;
        }

        // Do we already have a deserialized copy of this entry already?
        if(entry.data)
        {
            deserializedData.copyFrom(*entry.data, 0, entry.data->size());
            return;
        }

        self.readFromSource(&entry, path, deserializedData);
    }
};

Archive::Archive() : d(new Instance(*this, 0))
{}

Archive::Archive(const IByteArray& archive) : d(new Instance(*this, &archive))
{}

Archive::~Archive()
{
    clear();

    delete d;
}

const IByteArray* Archive::source() const
{
    return d->source;
}

void Archive::cache(CacheAttachment attach)
{
    if(!d->source)
    {
        // Nothing to read from.
        return;
    }
    DENG2_FOR_EACH(Index, i, d->index)
    {
        Entry& entry = *i.value();
        if(!entry.data && !entry.dataInArchive)
        {
            entry.dataInArchive = new Block(*d->source, entry.offset, entry.sizeInArchive);
        }
    }
    if(attach == DetachFromSource)
    {
        d->source = 0;
    }
}

bool Archive::has(const String& path) const
{
    return d->index.find(path) != d->index.end();
}

Archive::Names Archive::listFiles(const String& folder) const
{
    Names names;
    
    String prefix = folder.empty()? "" : folder / "";
    DENG2_FOR_EACH_CONST(Index, i, d->index)
    {
        if(i.key().beginsWith(prefix))
        {
            String relative = i.key().substr(prefix.size());
            if(relative.fileNamePath().empty())
            {
                // This is a file in the folder.
                names.insert(relative);
            }
        }
    }
    return names;
}

Archive::Names Archive::listFolders(const String& folder) const
{
    Names names;
    
    String prefix = folder.empty()? "" : folder / "";
    DENG2_FOR_EACH_CONST(Index, i, d->index)
    {
        if(i.key().beginsWith(prefix))
        {
            String relative = String(i.key().substr(prefix.size())).fileNamePath();
            if(!relative.empty() && relative.fileNamePath().empty())
            {
                // This is a subfolder in the folder.
                names.insert(relative);
            }
        }
    }    
    return names;
}

File::Status Archive::status(const String& path) const
{
    Index::const_iterator found = d->index.find(path);
    if(found == d->index.end())
    {
        /// @throw NotFoundError  @a path was not found in the archive.
        throw NotFoundError("Archive::fileStatus",
            "Entry '" + path + "' cannot not found in the archive");
    }

    return File::Status(
        // Is it a folder?
        (!found.value()->size && path.endsWith("/"))? File::Status::FOLDER : File::Status::FILE,
        found.value()->size,
        found.value()->modifiedAt);
}

const Block& Archive::entryBlock(const String& path) const
{
    Index::const_iterator found = d->index.find(path);
    if(found == d->index.end())
    {
        /// @throw NotFoundError  @a path was not found in the archive.
        throw NotFoundError("Archive::block",
            "Entry '" + path + "' cannot not found in the archive");
    }
    
    // We'll need to modify the entry.
    Entry& entry = *const_cast<Entry*>(found.value());
    if(entry.data)
    {
        // Got it.
        return *entry.data;
    }
    std::auto_ptr<Block> cached(new Block);
    d->readEntry(path, *cached.get());
    entry.data = cached.release();
    return *entry.data;
}

Block& Archive::entryBlock(const String& path)
{
    const Block& block = const_cast<const Archive*>(this)->entryBlock(path);
    
    // Mark for recompression.
    d->index.find(path).value()->maybeChanged = true;
    d->modified = true;
    
    return const_cast<Block&>(block);
}

void Archive::add(const String& path, const IByteArray& data)
{
    // Get rid of the earlier entry with this path.
    if(has(path))
    {
        remove(path);
    }

    Entry* entry = newEntry();
    entry->data = new Block(data);
    entry->modifiedAt = Time();
    entry->maybeChanged = true;

    // The rest of the data gets updated when the archive is written.

    d->index[path] = entry;
    d->modified = true;
}

void Archive::remove(const String& path)
{
    Index::iterator found = d->index.find(path);
    if(found != d->index.end())
    {
        delete found.value();
        d->index.erase(found);
        d->modified = true;
        return;
    }
    /// @throw NotFoundError  The path does not exist in the archive.
    throw NotFoundError("Archive::remove", "Entry '" + path + "' not found");
}

void Archive::clear()
{
    // Free deserialized data.
    DENG2_FOR_EACH(Index, i, d->index)
    {
        delete i.value();
    }
    d->index.clear();
    d->modified = true;
}

bool Archive::modified() const
{
    return d->modified;
}

void Archive::insertToIndex(const String& path, Entry* entry)
{
    if(d->index.contains(path))
    {
        delete d->index[path];
    }
    d->index[path] = entry;
}

const Archive::Index& Archive::index() const
{
    return d->index;
}

} // namespace de
