/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_ARCHIVE_H
#define LIBDENG2_ARCHIVE_H

#include "../libdeng2.h"
#include "../IByteArray"
#include "../IWritable"
#include "../String"
#include "../Time"
#include "../File"
#include "../PathTree"

#include <set>

namespace de
{
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
    class DENG2_PUBLIC Archive : public IWritable
    {
    public:
        /// Base class for format-related errors. @ingroup errors
        DENG2_ERROR(FormatError);

        /// Provided path was not valid. @ingroup errors
        DENG2_ERROR(InvalidPathError);

        /// The requested entry does not exist in the archive. @ingroup errors
        DENG2_ERROR(NotFoundError);
        
        /// There is an error related to content processing. @ingroup errors
        DENG2_ERROR(ContentError);
        
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
        Archive(IByteArray const &data);
        
        virtual ~Archive();

        /**
         * Returns the source byte array. @c NULL, if the archive was
         * constructed without a source (as empty) or has been detached from
         * its original source.
         */
        IByteArray const *source() const;

        enum CacheAttachment {
            RemainAttachedToSource = 0,
            DetachFromSource = 1
        };

        /**
         * Loads a copy of the serialized data into memory for all the entries that
         * don't already have deserialized data stored.
         *
         * @param attach  If DetachFromSource, the archive becomes a standalone
         *                archive that no longer needs the source byte array to
         *                remain in existence.
         */
        void cache(CacheAttachment attach = DetachFromSource);

        /**
         * Determines whether the archive contains an entry (not a folder).
         *
         * @param path  Path of the entry.
         *
         * @return  @c true or @c false.
         */
        bool hasEntry(Path const &path) const;

        /**
         * List the files in a specific folder of the archive.
         *
         * @param folder  Folder path to look in. By default looks in the root.
         * @param names   Entry names collected in a set. The names are relative to a
         *                @a folder and are in alphabetical order.
         *
         * @return Number of names returned in @a names.
         */
        dint listFiles(Names &names, Path const &folder = Path()) const;

        /**
         * List the folders in a specific folder of the archive.
         *
         * @param folder  Folder path to look in. By default looks in the root.
         * @param names   Folder entry names collected in a set. The names are
         *                relative to @a folder and are in alphabetical order.
         *
         * @return Number of names returned in @a names.
         */
        dint listFolders(Names &names, Path const &folder = Path()) const;

        /**
         * Returns information about the specified path. 
         *
         * @param path  Path of the entry within the archive.
         *
         * @return Type, size, and other metadata about the entry.
         */
        File::Status entryStatus(Path const &path) const;

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
        Block const &entryBlock(Path const &path) const;

        inline Block const &constEntryBlock(Path const &path) const {
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
        Block &entryBlock(Path const &path);

        /**
         * Adds an entry to the archive. The entry will not be committed to the
         * source, but instead remains as-is in memory.
         *
         * @param path  Path of the entry within the archive.
         * @param data  Data of the entry.
         */
        void add(Path const &path, IByteArray const &data);

        /**
         * Removes an entry from the archive. If there is deserialized data for
         * the entry in memory, it will be deleted.
         *
         * @param path  Path of the entry.
         */
        void remove(Path const &path);
        
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
        struct Entry : public PathTree::Node
        {
            dsize offset;           ///< Offset from the start of the source array.
            dsize size;             ///< Deserialized size.
            dsize sizeInArchive;    ///< Size within the archive (serialized).
            Time modifiedAt;        ///< Latest modification timestamp.
            bool maybeChanged;      ///< @c true, if the data must be re-serialized when writing.

            /// Deserialized data. Can be @c NULL. Entry has ownership.
            Block *data;

            /// Cached copy of the serialized data. Can be @c NULL. Entry has ownership.
            Block mutable *dataInArchive;

            Entry(PathTree::NodeArgs const &args) : Node(args),
                  offset(0),
                  size(0),
                  sizeInArchive(0),
                  maybeChanged(false),
                  data(0),
                  dataInArchive(0)
            {}

            virtual ~Entry()
            {
                // Entry has ownership of the cached data.
                delete data;
                delete dataInArchive;
            }
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
        virtual void readFromSource(Entry const &entry, Path const &path, IBlock &data) const = 0;

        /**
         * Inserts an entry into the archive's index. If the path already
         * exists in the index, the old entry is deleted first.
         *
         * @param path   Path of the entry.
         *
         * @return Inserted entry.
         */
        Entry &insertEntry(Path const &path);

        /**
         * Returns the full entry index so that derived classes can iterate the
         * entries.
         *
         * @return Entry index.
         */
        PathTree const &index() const;

    private:
        struct Instance;
        Instance *d;
    };
}

#endif /* LIBDENG2_ARCHIVE_H */
