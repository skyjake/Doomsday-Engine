/** @file wad.h  WAD Archive (File).
 *
 * @author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDOOMSDAY_FILESYS_WAD_H
#define LIBDOOMSDAY_FILESYS_WAD_H

#include "../libdoomsday.h"
#include "../filesys/lumpindex.h"
#include "file.h"
#include "fileinfo.h"
#include <de/error.h>
#include <de/pathtree.h>

namespace res {

/**
 * WAD archive file format.
 * @ingroup fs
 *
 * @see file.h, File1
 *
 * @todo This should be replaced with a FS2 based WadFolder class.
 */
class LIBDOOMSDAY_PUBLIC Wad : public File1, public LumpIndex
{
protected:
    struct Entry; // forward

public:
    /// Base class for format-related errors. @ingroup errors
    DE_ERROR(FormatError);

    /**
     * File system object for a lump in the WAD.
     *
     * The purpose of this abstraction is to redirect various File1 methods to the
     * containing Wad file. Such a mechanism would be unnecessary in a file system
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
        res::Uri composeUri(Char delimiter = '/') const;

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
         *
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
         * Convenient method returning the containing Wad file instance.
         */
        Wad &wad() const;

    private:
        Entry &entry;
    };

public:
    Wad(FileHandle &hndl, String path, const FileInfo &info, File1 *container = 0);
    virtual ~Wad();

    /**
     * Read the data associated with lump @a lumpIndex into @a buffer.
     *
     * @param lumpIndex     Lump index associated with the data to be read.
     * @param buffer        Buffer to read into. Must be at least large enough to
     *                      contain the whole lump.
     * @param tryCache      @c true= try the lump cache first.
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
     * @param lumpIndex     Lump index associated with the data to be read.
     * @param buffer        Buffer to read into. Must be at least @a length bytes.
     * @param startOffset   Offset from the beginning of the lump to start reading.
     * @param length        Number of bytes to read.
     * @param tryCache      @c true= try the lump cache first.
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
     * @param lumpIndex   Lump index associated with the data to be cached.
     *
     * @return Pointer to the cached copy of the associated data.
     *
     * @throws NotFoundError  If @a lumpIndex is not valid.
     */
    const uint8_t *cacheLump(int lumpIndex);

    /**
     * Remove a lock on a cached data lump.
     *
     * @param lumpIndex   Lump index associated with the cached data to be changed.
     */
    void unlockLump(int lumpIndex);

    /**
     * Clear any cached data for lump @a lumpIndex from the lump cache.
     *
     * @param lumpIndex     Lump index associated with the cached data to be cleared.
     * @param retCleared    If not @c NULL write @c true to this address if data was
     *                      present and subsequently cleared from the cache.
     */
    void clearCachedLump(int lumpIndex, bool *retCleared = 0);

    /**
     * Purge the lump cache, clearing all cached data lumps.
     *
     * @return This instance.
     */
    void clearLumpCache();

    /**
     * @attention Uses an extremely simple formula which does not conform to any CRC
     *            standard. Should not be used for anything critical.
     */
    uint calculateCRC();

public:
    /**
     * Determines whether a File looks like it could be accessed using Wad.
     *
     * @param file  File to check.
     *
     * @return @c true, if the file looks like a WAD.
     */
    static bool recognise(FileHandle &file);

protected:
    /**
     * Models an entry in the internal lump tree.
     */
    struct Entry : public PathTree::Node
    {
        dint32 offset;
        dint32 size;
        std::unique_ptr<LumpFile> lumpFile;  ///< File system object for the lump data.
        uint crc;                           ///< CRC for the lump data.

        Entry(const PathTree::NodeArgs &args)
            : Node(args), offset(0), size(0), crc(0)
        {}

        LumpFile &file() const;

        /// Recalculates CRC of the entry.
        void update();
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

#endif // LIBDOOMSDAY_FILESYS_WAD_H
