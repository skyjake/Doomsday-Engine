/** @file archive.cpp Collection of named memory blocks stored inside a byte array.
 *
 * Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/archive.h"

namespace de {

DE_PIMPL(Archive)
{
    /// Source data provided at construction.
    const IByteArray *source;

    /// Index maps entry paths to their metadata. Created by concrete subclasses
    /// but we have the ownership.
    PathTree *index;

    /// Contents of the archive have been modified.
    bool modified;

    Impl(Public &a, const IByteArray *src) : Base(a), source(src), index(0), modified(false)
    {}

    ~Impl()
    {
        delete index;
    }

    void readEntry(const Path &path, IBlock &deserializedData) const
    {
        const Entry &entry = static_cast<const Entry &>(
                    index->find(path, PathTree::MatchFull | PathTree::NoBranch));
        if (!entry.size)
        {
            // Empty entry; nothing to do.
            deserializedData.clear();
            return;
        }

        // Do we already have a deserialized copy of this entry?
        if (entry.data)
        {
            deserializedData.copyFrom(*entry.data, 0, entry.data->size());
            return;
        }

        self().readFromSource(entry, path, deserializedData);
    }
};

Archive::Archive() : d(new Impl(*this, 0))
{}

Archive::Archive(const IByteArray &archive) : d(new Impl(*this, &archive))
{}

Archive::~Archive()
{
    clear();
}

const IByteArray *Archive::source() const
{
    return d->source;
}

void Archive::cache(CacheOperation operation)
{
    if (!d->source)
    {
        // Nothing to read from.
        return;
    }
    PathTreeIterator<PathTree> iter(d->index->leafNodes());
    while (iter.hasNext())
    {
        Entry &entry = static_cast<Entry &>(iter.next());
        switch (operation)
        {
        case CacheAndDetachFromSource:
        case CacheAndRemainAttachedToSource:
            if (!entry.data && !entry.dataInArchive)
            {
                entry.dataInArchive.reset(new Block(*d->source, entry.offset, entry.sizeInArchive));
            }
            break;

        case UncacheUnmodifiedEntries:
            if (!entry.maybeChanged)
            {
                entry.data.reset();
                entry.dataInArchive.reset();
            }
            break;
        }
    }
    if (operation == CacheAndDetachFromSource)
    {
        d->source = nullptr;
    }
}

bool Archive::hasEntry(const Path &path) const
{
    DE_ASSERT(d->index != 0);

    return d->index->has(path, PathTree::MatchFull | PathTree::NoBranch);
}

dint Archive::listFiles(Archive::Names &names, const Path &folder) const
{
    DE_ASSERT(d->index != 0);

    names.clear();

    // Find the folder in the index.
    if (const PathTree::Node *parent = d->index->tryFind(folder, PathTree::MatchFull | PathTree::NoLeaf))
    {
        // Traverse the parent's nodes.
        for (PathTreeIterator<PathTree> iter(parent->children().leaves); iter.hasNext(); )
        {
            names.insert(iter.next().name());
        }
    }
    return dint(names.size());
}

dint Archive::listFolders(Archive::Names &names, const Path &folder) const
{
    DE_ASSERT(d->index != 0);

    names.clear();

    // Find the folder in the index.
    if (const PathTree::Node *parent = d->index->tryFind(folder, PathTree::MatchFull | PathTree::NoLeaf))
    {
        // Traverse the parent's nodes.
        for (PathTreeIterator<PathTree> iter(parent->children().branches); iter.hasNext(); )
        {
            names.insert(iter.next().name());
        }
    }
    return dint(names.size());
}

File::Status Archive::entryStatus(const Path &path) const
{
    DE_ASSERT(d->index != 0);

    const Entry &found = static_cast<const Entry &>(d->index->find(path, PathTree::MatchFull));

    return File::Status(
        found.isLeaf()? File::Type::File : File::Type::Folder,
        found.size,
        found.modifiedAt);
}

const Block &Archive::entryBlock(const Path &path) const
{
    DE_ASSERT(d->index != 0);

    // The entry contents will be cached in memory.
    if (Entry *entry = static_cast<Entry *>(d->index->tryFind(path, PathTree::MatchFull | PathTree::NoBranch)))
    {
        if (entry->data)
        {
            // Already got it.
            return *entry->data;
        }
        std::unique_ptr<Block> cached(new Block);
        d->readEntry(path, *cached.get());
        entry->data.reset(cached.release());
        return *entry->data;
    }
    else
    {
        /// @throw NotFoundError Entry with @a path was not found.
        throw NotFoundError("Archive::entryBlock", stringf("'%s' not found", path.c_str()));
    }
}

Block &Archive::entryBlock(const Path &path)
{
    if (!hasEntry(path))
    {
        add(path, Block());
    }

    const Block &block = const_cast<const Archive *>(this)->entryBlock(path);

    // Mark for recompression.
    Entry &entry = static_cast<Entry &>(d->index->find(path, PathTree::MatchFull | PathTree::NoBranch));
    entry.maybeChanged = true;
    entry.modifiedAt   = Time::currentHighPerformanceTime();

    d->modified = true;

    return const_cast<Block &>(block);
}

void Archive::uncacheBlock(const Path &path) const
{
    if (!d->source) return; // Wouldn't be able to re-cache the data.

    if (Entry *entry = static_cast<Entry *>(d->index->tryFind(path, PathTree::MatchFull | PathTree::NoBranch)))
    {
        if (!entry->data && !entry->dataInArchive) return;

        /*qDebug() << "Archive:" << path << "uncached by archive" << this
                 << (entry->data? entry->data->size() : 0)
                 << (entry->dataInArchive? entry->dataInArchive->size() : 0);*/

        if (!entry->maybeChanged)
        {
            entry->data.reset();
        }
        entry->dataInArchive.reset();
    }
    else
    {
        /// @throw NotFoundError Entry with @a path was not found.
        throw NotFoundError("Archive::uncacheBlock", stringf("'%s' not found", path.c_str()));
    }
}

void Archive::add(const Path &path, const IByteArray &data)
{
    if (path.isEmpty())
    {
        /// @throws InvalidPathError  Provided path was not a valid path.
        throw InvalidPathError("Archive::add",
                               stringf("'%s' is an invalid path for an entry", path.c_str()));
    }

    // Get rid of the earlier entry with this path.
    remove(path);

    DE_ASSERT(d->index != 0);

    Entry &entry = static_cast<Entry &>(d->index->insert(path));
    entry.data.reset(new Block(data));
    entry.modifiedAt   = Time::currentHighPerformanceTime();
    entry.maybeChanged = true;

    // The rest of the data gets updated when the archive is written.

    d->modified = true;
}

void Archive::remove(const Path &path)
{
    DE_ASSERT(d->index != 0);

    if (d->index->remove(path, PathTree::MatchFull | PathTree::NoBranch))
    {
        d->modified = true;
    }
}

void Archive::clear()
{
    DE_ASSERT(d->index != 0);

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

Archive::Entry &Archive::insertEntry(const Path &path)
{
    LOG_AS("Archive");
    DE_ASSERT(d->index != 0);

    // Remove any existing node at this path.
    d->index->remove(path, PathTree::MatchFull | PathTree::NoBranch);

    return static_cast<Entry &>(d->index->insert(path));
}

const PathTree &Archive::index() const
{
    DE_ASSERT(d->index != 0);

    return *d->index;
}

Archive::Entry::Entry(const PathTree::NodeArgs &args)
    : Node(args)
    , offset(0)
    , size(0)
    , sizeInArchive(0)
    , modifiedAt(Time::invalidTime())
    , maybeChanged(false)
{}

Archive::Entry::~Entry()
{}

} // namespace de
