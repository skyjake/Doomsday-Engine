/*
 * The Doomsday Engine Project -- libcore
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

#ifndef LIBCORE_ARCHIVE_H
#define LIBCORE_ARCHIVE_H

#include "de/libcore.h"
#include "de/ibytearray.h"
#include "de/iwritable.h"
#include "de/string.h"
#include "de/time.h"
#include "de/file.h"
#include "de/pathtree.h"

#include <set>

namespace de {

class IBlock;
class Block;

/**
 * Collection of named memory blocks stored inside a byte array.
 *
 * An archive consists of a collection of Block instances that are
 * identified using a path tree structure. Blocks can be added and removed
 * at any time.
 *
 * Archive is merely IWritable instead of ISerializable because of its
 * memory management model. The basic assumption is that even though the
 * archive is kept in serialized form, individual entries can still be
 * accessed without processing the entire serialization. Therefore, Archive
 * only operates on existing, serialized archives that live in externally
 * owned byte arrays; Archive does not take ownership of the source data.
 * This also means that read-only access to very large byte arrays can be
 * done without loading all the source data into memory (e.g., Archive
 * that reads from a large NativeFile).
 *
 * It is also assumed that accessing the source data and extracting a
 * particular entry is potentially a slow operation: when individual
 * entries are read from the source, the entries are cached in memory so
 * that subsequent access is fast.
 *
 * An Archive instance expects that the source byte array is never changed
 * by third parties while it is the source of the Archive.
 *
 * It is possible to detach an Archive instance from its source byte array
 * by calling @c cache(DetachFromSource). This forces all entries to be
 * copied to Archive-owned memory (in original serialized form).
 *
 * @see ArchiveFeed, ArchiveEntryFile
 *
 * @ingroup data
 */
class DE_PUBLIC Archive : public IWritable
{
public:
    /// Base class for format-related errors. @ingroup errors
    DE_ERROR(FormatError);

    /// Provided path was not valid. @ingroup errors
    DE_ERROR(InvalidPathError);

    /// The requested entry does not exist in the archive. @ingroup errors
    DE_ERROR(NotFoundError);

    /// There is an error related to content processing. @ingroup errors
    DE_ERROR(ContentError);

    typedef std::set<String> Names; // alphabetical order

public:
    /**
     * Constructs an empty Archive.
     */
    Archive();

    /**
     * Constructs a new Archive instance. The content index contained in
     * @a data is read during construction.
     *
     * @param data  Data of the source archive. No copy of the
     *              data is made, so the caller must make sure the
     *              byte array remains in existence for the lifetime
     *              of the Archive instance.
     */
    Archive(const IByteArray &data);

    virtual ~Archive();

    /**
     * Returns the source byte array. @c NULL, if the archive was
     * constructed without a source (as empty) or has been detached from
     * its original source.
     */
    const IByteArray *source() const;

    enum CacheOperation {
        CacheAndRemainAttachedToSource = 0,
        CacheAndDetachFromSource = 1,
        UncacheUnmodifiedEntries = 2,
    };

    /**
     * Caches or uncaches archive entries.
     *
     * The archive must have a source for any of the caching operations to be possible.
     *
     * @param operation  One of the following:
     *      - CacheAndRemainAttachedToSource: Loads a copy of the serialized data into
     *          memory for all the entries that don't already have deserialized data stored.
     *      - CacheAndDetachFromSource: Loads a copy of all the serialized data into memory,
     *          and the archive becomes a standalone archive that no longer needs the
     *          source byte array to remain in existence.
     *      - UncacheUnmodifiedEntries: All entries that have not been modified will be
     *          released from memory. Accessing them in the future will require reloading
     *          the entry contents into memory.
     */
    void cache(CacheOperation operation = CacheAndDetachFromSource);

    /**
     * Determines whether the archive contains an entry (not a folder).
     *
     * @param path  Path of the entry.
     *
     * @return  @c true or @c false.
     */
    bool hasEntry(const Path &path) const;

    /**
     * List the files in a specific folder of the archive.
     *
     * @param folder  Folder path to look in. By default looks in the root.
     * @param names   Entry names collected in a set. The names are relative to a
     *                @a folder and are in alphabetical order.
     *
     * @return Number of names returned in @a names.
     */
    dint listFiles(Names &names, const Path &folder = Path()) const;

    /**
     * List the folders in a specific folder of the archive.
     *
     * @param folder  Folder path to look in. By default looks in the root.
     * @param names   Folder entry names collected in a set. The names are
     *                relative to @a folder and are in alphabetical order.
     *
     * @return Number of names returned in @a names.
     */
    dint listFolders(Names &names, const Path &folder = Path()) const;

    /**
     * Returns information about the specified path.
     *
     * @param path  Path of the entry within the archive.
     *
     * @return Type, size, and other metadata about the entry.
     */
    File::Status entryStatus(const Path &path) const;

    /**
     * Returns the deserialized data of an entry for read-only access. The
     * data is deserialized and cached if a cached copy doesn't already
     * exist.
     *
     * This method operates on the Archive in immutable mode: the user is
     * not expected to modify the contents of the returned Block, and the
     * existing serialized data of the entry can be used as-is when the
     * archive is written.
     *
     * @param path  Entry path. The entry must already exist in the archive.
     *
     * @return Immutable contents of the entry.
     */
    const Block &entryBlock(const Path &path) const;

    inline const Block &constEntryBlock(const Path &path) const {
        return entryBlock(path);
    }

    /**
     * Returns the deserialized data of an entry for read and write access.
     * The data is deserialized and cached if a cached copy doesn't already
     * exist.
     *
     * The user is allowed to make changes to the returned block. The
     * entry's data is automatically marked for re-serialization in case
     * the archive is written.
     *
     * @param path  Entry path. If doesn't exist, a new entry will be added.
     *
     * @return Modifiable contents of the entry.
     */
    Block &entryBlock(const Path &path);

    /**
     * Release all cached data of a block. Unmodified blocks cannot be uncached.
     * The archive must have a source for uncaching to be possible.
     */
    void uncacheBlock(const Path &path) const;

    /**
     * Adds an entry to the archive. The entry will not be committed to the
     * source, but instead remains as-is in memory.
     *
     * @param path  Path of the entry within the archive.
     * @param data  Data of the entry.
     */
    void add(const Path &path, const IByteArray &data);

    /**
     * Removes an entry from the archive. If there is deserialized data for
     * the entry in memory, it will be deleted.
     *
     * @param path  Path of the entry.
     */
    void remove(const Path &path);

    /**
     * Clears the index of the archive. All entries are deleted.
     */
    void clear();

    /**
     * Determines if the archive has been modified.
     */
    bool modified() const;

    /**
     * Writes the archive to a Writer. Deserialized entries are
     * re-serialized just-in-time before writing if they have been
     * modified.
     *
     * @note If overwriting the source array, be sure to either first write
     * to a temporary array and then replace the source, or alternatively
     * detach the source from the archive beforehand. Otherwise the
     * unchanged entries may become corrupted as they are reused from their
     * old location in the source, which may have been already overwritten.
     *
     * @param to  Where to write.
     */
    virtual void operator >> (Writer &to) const = 0;

protected:
    /*
     * Interface for derived classes:
     */
    /// Base class for archive entries.
    struct DE_PUBLIC Entry : public PathTree::Node
    {
        dsize offset;           ///< Offset from the start of the source array.
        dsize size;             ///< Deserialized size.
        dsize sizeInArchive;    ///< Size within the archive (serialized).
        Time modifiedAt;        ///< Latest modification timestamp.
        bool maybeChanged;      ///< @c true, if the data must be re-serialized when writing.

        /// Deserialized data. Can be @c nullptr. Entry has ownership.
        std::unique_ptr<Block> data;

        /// Cached copy of the serialized data. Can be @c nullptr. Entry has ownership.
        mutable std::unique_ptr<Block> dataInArchive;

        Entry(const PathTree::NodeArgs &args);
        virtual ~Entry();

        // Must be constructed with args.
        Entry() = delete;
    };

    /**
     * Sets the index used by the Archive. A concrete subclass must call
     * this in their constructor; Archive does not create an index on its
     * own.
     *
     * @param tree  PathTree with entries of suitable type. Ownership given to Archive.
     */
    void setIndex(PathTree *tree);

    /**
     * Reads an entry from the source archive. The implementation of this
     * method is expected to cache the read data of the entry in its
     * original, serialized format in Entry::dataInArchive.
     *
     * @param entry  Entry that is being read.
     * @param path   Path of the entry within the archive.
     * @param data   Data is written here.
     */
    virtual void readFromSource(const Entry &entry, const Path &path, IBlock &data) const = 0;

    /**
     * Inserts an entry into the archive's index. If the path already
     * exists in the index, the old entry is deleted first.
     *
     * @param path   Path of the entry.
     *
     * @return Inserted entry.
     */
    Entry &insertEntry(const Path &path);

    /**
     * Returns the full entry index so that derived classes can iterate the
     * entries.
     *
     * @return Entry index.
     */
    const PathTree &index() const;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif /* LIBCORE_ARCHIVE_H */
