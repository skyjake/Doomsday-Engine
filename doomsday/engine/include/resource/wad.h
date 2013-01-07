/** @file wad.h WAD Archive (File).
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RESOURCE_WAD_H
#define LIBDENG_RESOURCE_WAD_H

#include "filesys/file.h"
#include "filesys/fileinfo.h"
#include <de/PathTree>

namespace de {

/**
 * WAD archive file format.
 * @ingroup resource
 *
 * @see file.h, File1
 */
class Wad : public File1
{
public:
    /// Base class for format-related errors. @ingroup errors
    DENG2_ERROR(FormatError);

    /// The requested entry does not exist in the wad. @ingroup errors
    DENG2_ERROR(NotFoundError);

public:
    Wad(FileHandle &hndl, String path, FileInfo const &info, File1 *container = 0);
    ~Wad();

    /// @return @c true= @a lumpIdx is a valid logical index for a lump in this file.
    bool isValidIndex(int lumpIdx) const;

    /// @return Logical index of the last lump in this file's directory or @c -1 if empty.
    int lastIndex() const;

    /// @return Number of lumps contained by this file or @c 0 if empty.
    int lumpCount() const;

    /// @return @c true= There are no lumps in this file's directory.
    bool empty();

    /**
     * Retrieve the directory node for a lump contained by this file.
     *
     * @param lumpIdx       Logical index for the lump in this file's directory.
     *
     * @return  Directory node for this lump.
     *
     * @throws NotFoundError  If @a lumpIdx is not valid.
     */
    PathTree::Node &lumpDirectoryNode(int lumpIdx) const;

    /**
     * Retrieve a lump contained by this file.
     *
     * @param lumpIdx       Logical index for the lump in this file's directory.
     *
     * @return The lump.
     *
     * @throws NotFoundError  If @a lumpIdx is not valid.
     */
    File1 &lump(int lumpIdx);

    /**
     * Read the data associated with lump @a lumpIdx into @a buffer.
     *
     * @param lumpIdx       Lump index associated with the data to be read.
     * @param buffer        Buffer to read into. Must be at least large enough to
     *                      contain the whole lump.
     * @param tryCache      @c true= try the lump cache first.
     *
     * @return Number of bytes read.
     *
     * @throws NotFoundError  If @a lumpIdx is not valid.
     *
     * @see lumpSize() or lumpInfo() to determine the size of buffer needed.
     */
    size_t readLump(int lumpIdx, uint8_t *buffer, bool tryCache = true);

    /**
     * Read a subsection of the data associated with lump @a lumpIdx into @a buffer.
     *
     * @param lumpIdx       Lump index associated with the data to be read.
     * @param buffer        Buffer to read into. Must be at least @a length bytes.
     * @param startOffset   Offset from the beginning of the lump to start reading.
     * @param length        Number of bytes to read.
     * @param tryCache      @c true= try the lump cache first.
     *
     * @return Number of bytes read.
     *
     * @throws NotFoundError  If @a lumpIdx is not valid.
     */
    size_t readLump(int lumpIdx, uint8_t *buffer, size_t startOffset, size_t length,
                    bool tryCache = true);

    /**
     * Read the data associated with lump @a lumpIdx into the cache.
     *
     * @param lumpIdx   Lump index associated with the data to be cached.
     *
     * @return Pointer to the cached copy of the associated data.
     *
     * @throws NotFoundError  If @a lumpIdx is not valid.
     */
    uint8_t const *cacheLump(int lumpIdx);

    /**
     * Remove a lock on a cached data lump.
     *
     * @param lumpIdx   Lump index associated with the cached data to be changed.
     */
    void unlockLump(int lumpIdx);

    /**
     * Clear any cached data for lump @a lumpIdx from the lump cache.
     *
     * @param lumpIdx       Lump index associated with the cached data to be cleared.
     * @param retCleared    If not @c NULL write @c true to this address if data was
     *                      present and subsequently cleared from the cache.
     */
    void clearCachedLump(int lumpIdx, bool *retCleared = 0);

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

private:
    struct Instance;
    Instance *d;
};

} // namespace de

#endif /* LIBDENG_RESOURCE_WAD_H */
