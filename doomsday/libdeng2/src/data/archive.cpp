/** @file archive.cpp Collection of named memory blocks stored inside a byte array.
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
    Archive &self;

    /// Source data provided at construction.
    IByteArray const *source;

    /// Index maps entry paths to their metadata. Created by concrete subclasses
    /// but we have the ownership.
    PathTree *index;

    /// Contents of the archive have been modified.
    bool modified;

    Instance(Archive &a, IByteArray const *src) : self(a), source(src), index(0), modified(false)
    {}

    ~Instance()
    {
        delete index;
    }

    void readEntry(Path const &path, IBlock &deserializedData) const
    {
        Entry const &entry = static_cast<Entry const &>(
                    index->find(path, PathTree::MatchFull | PathTree::NoBranch));
        if(!entry.size)
        {
            // Empty entry; nothing to do.
            deserializedData.clear();
            return;
        }

        // Do we already have a deserialized copy of this entry?
        if(entry.data)
        {
            deserializedData.copyFrom(*entry.data, 0, entry.data->size());
            return;
        }

        self.readFromSource(entry, path, deserializedData);
    }
};

Archive::Archive() : d(new Instance(*this, 0))
{}

Archive::Archive(IByteArray const &archive) : d(new Instance(*this, &archive))
{}

Archive::~Archive()
{
    clear();

    delete d;
}

IByteArray const *Archive::source() const
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
    PathTreeIterator<PathTree> iter(d->index->leafNodes());
    while(iter.hasNext())
    {
        Entry &entry = static_cast<Entry &>(iter.next());
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

bool Archive::has(Path const &path) const
{
    DENG2_ASSERT(d->index != 0);

    return d->index->has(path, PathTree::MatchFull | PathTree::NoBranch);
}

dint Archive::listFiles(Archive::Names& names, Path const &folder) const
{
    DENG2_ASSERT(d->index != 0);

    names.clear();

    // Find the folder in the index.
    PathTree::Node const &parent = d->index->find(folder, PathTree::MatchFull | PathTree::NoLeaf);

    // Traverse the parent's nodes.
    for(PathTreeIterator<PathTree> iter(parent.children()); iter.hasNext(); )
    {
        PathTree::Node const &node = iter.next();
        if(node.isLeaf()) names.insert(node.name());
    }

    return names.size();
}

dint Archive::listFolders(Archive::Names &names, Path const &folder) const
{
    DENG2_ASSERT(d->index != 0);

    names.clear();

    // Find the folder in the index.
    PathTree::Node const &parent = d->index->find(folder, PathTree::MatchFull | PathTree::NoLeaf);

    // Traverse the parent's nodes.
    for(PathTreeIterator<PathTree> iter(parent.children()); iter.hasNext(); )
    {
        PathTree::Node const &node = iter.next();
        if(node.isBranch()) names.insert(node.name());
    }

    return names.size();
}

File::Status Archive::status(Path const &path) const
{
    DENG2_ASSERT(d->index != 0);

    Entry const &found = static_cast<Entry const &>(d->index->find(path, PathTree::MatchFull));

    return File::Status(
        found.isLeaf()? File::Status::FILE : File::Status::FOLDER,
        found.size,
        found.modifiedAt);
}

Block const &Archive::entryBlock(Path const &path) const
{
    DENG2_ASSERT(d->index != 0);

    // We'll need to modify the entry.
    Entry &entry = static_cast<Entry &>(d->index->find(path, PathTree::MatchFull | PathTree::NoBranch));
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

Block &Archive::entryBlock(Path const &path)
{
    Block const &block = const_cast<Archive const *>(this)->entryBlock(path);
    
    // Mark for recompression.
    static_cast<Entry &>(d->index->find(path, PathTree::MatchFull | PathTree::NoBranch)).maybeChanged = true;
    d->modified = true;
    
    return const_cast<Block &>(block);
}

void Archive::add(Path const &path, IByteArray const &data)
{
    if(path.isEmpty())
    {
        /// @throws InvalidPathError  Provided path was not a valid path.
        throw InvalidPathError("Archive::add",
                               QString("'%1' is an invalid path for an entry").arg(path));
    }

    // Get rid of the earlier entry with this path.
    remove(path);

    DENG2_ASSERT(d->index != 0);

    Entry *entry = &static_cast<Entry &>(d->index->insert(path));

    DENG2_ASSERT(entry != 0); // does this actually happen and when?

    entry->data         = new Block(data);
    entry->modifiedAt   = Time();
    entry->maybeChanged = true;

    // The rest of the data gets updated when the archive is written.

    d->modified = true;
}

void Archive::remove(Path const &path)
{
    DENG2_ASSERT(d->index != 0);

    if(d->index->remove(path, PathTree::MatchFull | PathTree::NoBranch))
    {
        d->modified = true;
    }
}

void Archive::clear()
{
    DENG2_ASSERT(d->index != 0);

    d->index->clear();
    d->modified = true;
}

bool Archive::modified() const
{
    return d->modified;
}

void Archive::setIndex(PathTree *tree)
{
    d->index = tree;
}

Archive::Entry &Archive::insertEntry(Path const &path)
{
    LOG_AS("Archive");
    DENG2_ASSERT(d->index != 0);

    LOG_DEBUG("Inserting %s") << path;

    // Remove any existing node at this path.
    d->index->remove(path, PathTree::MatchFull | PathTree::NoBranch);

    return static_cast<Entry &>(d->index->insert(path));
}

PathTree const &Archive::index() const
{
    DENG2_ASSERT(d->index != 0);

    return *d->index;
}

} // namespace de
