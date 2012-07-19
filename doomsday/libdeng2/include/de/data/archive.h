/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <vector>
#include <set>
#include <map>

namespace de
{
    class IBlock;
    class Block;
    
    /**
     * Provides read and write access to data inside a compressed ZIP archive.
     * 
     * @see ArchiveFeed, ArchiveFile
     *
     * @ingroup data
     */
    class DENG2_PUBLIC Archive : public IWritable
    {
    public:
        /// Base class for format-related errors. @ingroup errors
        DENG2_ERROR(FormatError);
        
        /// The central directory of the ZIP archive cannot be located. Maybe it's not 
        /// a ZIP archive after all? @ingroup errors
        DENG2_SUB_ERROR(FormatError, MissingCentralDirectoryError);
        
        /// The source archive belongs to a multipart archive. @ingroup errors
        DENG2_SUB_ERROR(FormatError, MultiPartError);
        
        /// An entry in the archive uses a compression algorithm not supported by the
        /// implementation. @ingroup errors
        DENG2_SUB_ERROR(FormatError, UnknownCompressionError);
        
        /// An entry is encrypted. Decrypting is not supported. @ingroup errors
        DENG2_SUB_ERROR(FormatError, EncryptionError);
        
        /// The requested entry does not exist in the archive. @ingroup errors
        DENG2_ERROR(NotFoundError);
        
        /// There is an error during decompression. @ingroup errors
        DENG2_ERROR(InflateError);
        
        /// There is an error during compression. @ingroup errors
        DENG2_ERROR(DeflateError);
        
        typedef std::set<String> Names;
        
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
        Archive(const IByteArray& data);
        
        virtual ~Archive();

        enum CacheAttachment {
            RemainAttachedToSource = 0,
            DetachFromSource = 1
        };

        /**
         * Loads a copy of the compressed data into memory for all the entries that
         * don't already have uncompressed data stored. 
         *
         * @param op  If DetachFromSource, the archive becomes a standalone
         *            archive that no longer needs the source byte array to
         *            remain in existence.
         */
        void cache(CacheAttachment attach = DetachFromSource);

        /**
         * Determines whether the archive contains an entry.
         *
         * @param path  Path of the entry.
         *
         * @return  @c true or @c false.
         */
        bool has(const String& path) const;

        /**
         * List the files in a specific folder of the archive.
         *
         * @param names   Entry names collected to this set. The names are
          *               relative to @a folder.
         * @param folder  Folder path to look in.
         */
        void listFiles(Names& names, const String& folder = "") const;

        /**
         * List the folders in a specific folder of the archive.
         *
         * @param names   Folder entry names collected to this set. The names are
         *                relative to @a folder.
         * @param folder  Folder path to look in.
         */
        void listFolders(Names& names, const String& folder = "") const;

        /**
         * Returns information about the specified path. 
         *
         * @param path  Path of the entry within the archive.
         *
         * @return Type, size, and other metadata about the entry.
         */
        File::Status status(const String& path) const;

        /**
         * Returns the uncompressed data of an entry for read-only access. 
         * The data is uncompressed and cached if a cached copy doesn't already exist. 
         * The cached data will not need to be recompressed if the archive is written.
         *
         * @param path  Entry path.
         */
        const Block& entryBlock(const String& path) const;

        /**
         * Returns the uncompressed data of an entry for read and write access. 
         * The data is uncompressed and cached if a cached copy doesn't already exist. 
         * The data is marked for recompression if the archive is written (since
         * the caller is free to modify the contents of the returned block).
         *
         * @param path  Entry path. The entry must already exist in the archive.
         */
        Block& entryBlock(const String& path);

        /**
         * Reads and decompresses an entry from the archive. The compressed data
         * for the entry is kept cached after the call.
         *
         * @param path              Path of the entry within the archive.
         * @param uncompressedData  Data is written here.
         */
        void read(const String& path, IBlock& uncompressedData) const;

        /**
         * Adds an entry to the archive. It will remain uncompressed in memory.
         * 
         * @param path  Path of the entry within the archive.
         * @param data  Data of the entry.
         */
        void add(const String& path, const IByteArray& data);

        /**
         * Removes an entry from the archive. If there is uncompressed data for
         * the entry in memory, it will be deleted.
         *
         * @param path  Path of the entry.
         */
        void remove(const String& path);
        
        /**
         * Clears the index of the archive. All entries are deleted.
         */
        void clear();

        /**
         * Determines if the archive has been modified.
         */
        bool modified() const { return _modified; }

        /**
         * Writes the archive to a Writer. Uncompressed entries are compressed 
         * just-in-time before writing.
         *
         * @param to  Where to write.
         */
        void operator >> (Writer& to) const;
        
    public:
        /**
         * Determines whether a File looks like it could be accessed using 
         * Archive.
         *
         * @param file  File to check.
         *
         * @return @c true, if the file looks like an archive.
         */
        static bool recognize(const File& file);
        
    private:
        struct Entry {
            dsize offset;           ///< Offset from the start of the archive file.
            dsize size;             ///< Uncompressed size.
            dsize sizeInArchive;    ///< Size within the archive.
            duint16 compression;    ///< Type of compression employed by the entry.
            duint32 crc32;          ///< CRC32 checksum.
            dsize localHeaderOffset;///< Offset of the local file header.
            Time modifiedAt;        ///< Latest modification timestamp.
            bool mustCompress;      ///< True if the data must be compressed when writing.
            Block* data;            ///< Uncompressed data. Can be @c NULL.
            mutable Block* compressedData; ///< Cached copy of the compressed data. Can be @c NULL.
            
            Entry() : offset(0), size(0), sizeInArchive(0), compression(0), crc32(0),
                localHeaderOffset(0), mustCompress(false), data(0), compressedData(0) {}
        };
        typedef std::map<String, Entry> Index;

        /// Recalculates CRC32 of @a entry.
        void updateEntry(Entry& entry) const;
        
    private:
        /// Source data provided at construction.
        const IByteArray* _source;
        
        /// Index maps entry paths to their metadata.
        Index _index;
        
        /// Contents of the archive has been modified.
        bool _modified;
    };
}

#endif /* LIBDENG2_ARCHIVE_H */
