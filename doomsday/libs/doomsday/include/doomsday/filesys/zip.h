/** @file zip.h  ZIP Archive (File).
 *
 * @author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDOOMSDAY_FILESYS_ZIP_H
#define LIBDOOMSDAY_FILESYS_ZIP_H

#include "../libdoomsday.h"
#include "../filesys/lumpindex.h"
#include "file.h"
#include "fileinfo.h"
#include <de/error.h>
#include <de/pathtree.h>

namespace res {

/**
 * ZIP archive file format.
 * @ingroup resource
 *
 * @note Presently only the zlib method (Deflate) of compression is supported.
 *
 * @see file.h, File1
 *
 * @todo This is obsolete: should use ZipArchive/ArchiveFolder in libcore.
 */
class LIBDOOMSDAY_PUBLIC Zip : public File1, public LumpIndex
{
protected:
    struct Entry; // forward

public:
    /// Base class for format-related errors. @ingroup errors
    DE_ERROR(FormatError);

    /**
     * File system object for a lump in the ZIP.
     *
     * The purpose of this abstraction is to redirect various File1 methods to the
     * containing Zip file. Such a mechanism would be unnecessary in a file system
     * in which proper OO design is used for the package / file abstraction. -ds
     */
    class LumpFile : public File1
    {
    public:
        LumpFile(Entry &entry, FileHandle *hndl, String path, const FileInfo &info,
                 File1 *container);

        /// @return  Name of this file.
        const String &name() const;

        /**
         * Compose an absolute URI to this file.
         *
         * @param delimiter     Delimit directory using this character.
         *
         * @return The absolute URI.
         */
        Uri composeUri(Char delimiter = '/') const;

        /**
         * Retrieve the directory node for this file.
         *
         * @return  Directory node for this file.
         */
        PathTree::Node &directoryNode() const;

        /**
         * Read the file data into @a buffer.
         *
         * @param buffer        Buffer to read into. Must be at least large enough to
         *                      contain the whole file.
         * @param tryCache      @c true= try the lump cache first.
         *
         * @return Number of bytes read.
         *
         * @see size() or info() to determine the size of buffer needed.
         */
        size_t read(uint8_t *buffer, bool tryCache = true);

        /**
         * Read a subsection of the file data into @a buffer.
         *
         * @param buffer        Buffer to read into. Must be at least @a length bytes.
         * @param startOffset   Offset from the beginning of the file to start reading.
         * @param length        Number of bytes to read.
         * @param tryCache      If @c true try the local data cache first.
         *
         * @return Number of bytes read.
         */
        size_t read(uint8_t *buffer, size_t startOffset, size_t length, bool tryCache = true);

        /**
         * Read this lump into the local cache.

         * @return Pointer to the cached copy of the associated data.
         */
        const uint8_t *cache();

        /**
         * Remove a lock on the locally cached data.
         *
         * @return This instance.
         */
        LumpFile &unlock();

        /**
         * Convenient method returning the containing Zip file instance.
         */
        Zip &zip() const;

    private:
        Entry &entry;
    };

public:
    Zip(FileHandle &hndl, String path, const FileInfo &info, File1 *container = 0);
    virtual ~Zip();

    /**
     * Read the data associated with lump @a lumpIndex into @a buffer.
     *
     * @param lumpIndex  Lump index associated with the data to be read.
     * @param buffer     Buffer to read into. Must be at least large enough to
     *                   contain the whole lump.
     * @param tryCache   @c true= try the lump cache first.
     *
     * @return Number of bytes read.
     *
     * @throws NotFoundError  If @a lumpIndex is not valid.
     *
     * @see lumpSize() or lumpInfo() to determine the size of buffer needed.
     */
    size_t readLump(int lumpIndex, uint8_t *buffer, bool tryCache = true);

    /**
     * Read a subsection of the data associated with lump @a lumpIndex into @a buffer.
     *
     * @param lumpIndex    Lump index associated with the data to be read.
     * @param buffer       Buffer to read into. Must be at least @a length bytes.
     * @param startOffset  Offset from the beginning of the lump to start reading.
     * @param length       Number of bytes to read.
     * @param tryCache     @c true= try the lump cache first.
     *
     * @return Number of bytes read.
     *
     * @throws NotFoundError  If @a lumpIndex is not valid.
     */
    size_t readLump(int lumpIndex, uint8_t *buffer, size_t startOffset, size_t length,
                    bool tryCache = true);

    /**
     * Read the data associated with lump @a lumpIndex into the cache.
     *
     * @param lumpIndex  Lump index associated with the data to be cached.
     *
     * @return Pointer to the cached copy of the associated data.
     */
    const uint8_t *cacheLump(int lumpIndex);

    /**
     * Remove a lock on a cached data lump.
     *
     * @param lumpIndex  Lump index associated with the cached data to be changed.
     */
    void unlockLump(int lumpIndex);

    /**
     * Clear any cached data for lump @a lumpIndex from the lump cache.
     *
     * @param lumpIndex   Lump index associated with the cached data to be cleared.
     * @param retCleared  If not @c NULL write @c true to this address if data was
     *                    present and subsequently cleared from the cache.
     */
    void clearCachedLump(int lumpIndex, bool *retCleared = 0);

    /**
     * Purge the lump cache, clearing all cached data lumps.
     */
    void clearLumpCache();

public:

    /**
     * Determines whether the specified file appears to be in a format recognised by
     * Zip.
     *
     * @param file  Stream file handle/wrapper to the file being interpreted.
     *
     * @return  @c true= this is a file that can be represented using Zip.
     */
    static bool recognise(FileHandle &file);

    /**
     * Inflates a block of data compressed using ZipFile_Compress() (i.e., zlib
     * deflate algorithm).
     *
     * @param in        Pointer to compressed data.
     * @param inSize    Size of the compressed data.
     * @param outSize   Size of the uncompressed data is written here. Must not be @c NULL.
     *
     * @return  Pointer to the uncompressed data. Caller gets ownership of the
     * returned memory and must free it with M_Free().
     *
     * @see compress()
     */
    static uint8_t *uncompress(uint8_t *in, size_t inSize, size_t *outSize);

    /**
     * Inflates a compressed block of data using zlib. The caller must figure out
     * the uncompressed size of the data before calling this.
     *
     * zlib will expect raw deflate data, not looking for a zlib or gzip header,
     * not generating a check value, and not looking for any check values for
     * comparison at the end of the stream.
     *
     * @param in        Pointer to compressed data.
     * @param inSize    Size of the compressed data.
     * @param out       Pointer to output buffer.
     * @param outSize   Size of the output buffer. This must match the size of the
     *                  decompressed data.
     *
     * @return  @c true if successful.
     */
    static bool uncompressRaw(uint8_t *in, size_t inSize, uint8_t *out, size_t outSize);

    /**
     * Compresses a block of data using zlib with the default/balanced compression level.
     *
     * @param in        Pointer to input data to compress.
     * @param inSize    Size of the input data.
     * @param outSize   Pointer where the size of the compressed data will be written.
     *                  Cannot be @c NULL.
     *
     * @return  Compressed data. The caller gets ownership of this memory and must
     *          free it with M_Free(). If an error occurs, returns @c NULL and
     *          @a outSize is set to zero.
     */
    static uint8_t *compress(uint8_t *in, size_t inSize, size_t *outSize);

    /**
     * Compresses a block of data using zlib.
     *
     * @param in        Pointer to input data to compress.
     * @param inSize    Size of the input data.
     * @param outSize   Pointer where the size of the compressed data will be written.
     *                  Cannot be @c NULL.
     * @param level     Compression level: 0=none/fastest ... 9=maximum/slowest.
     *
     * @return  Compressed data. The caller gets ownership of this memory and must
     *          free it with M_Free(). If an error occurs, returns @c NULL and
     *          @a outSize is set to zero.
     */
    static uint8_t *compressAtLevel(uint8_t *in, size_t inSize, size_t *outSize, int level);

protected:
    /**
     * Models an entry in the internal lump tree.
     */
    struct Entry : public PathTree::Node
    {
        dsize offset;
        dsize size;
        dsize compressedSize;
        std::unique_ptr<LumpFile> lumpFile;  ///< File system object for the lump data.

        Entry(const PathTree::NodeArgs &args)
            : Node(args), offset(0), size(0), compressedSize(0)
        {}

        LumpFile &file() const;
    };
    typedef PathTreeT<Entry> LumpTree;

    /**
     * Provides access to the internal LumpTree, for efficient traversal.
     */
    const LumpTree &lumpTree() const;

private:
    DE_PRIVATE(d)
};

} // namespace res

#endif // LIBDOOMSDAY_FILESYS_ZIP_H
