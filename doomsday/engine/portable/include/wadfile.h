/**
 * @file wadfile.h
 * Specialization of AbstractFile for working with Wad archives.
 *
 * @ingroup fs
 *
 * @see abstractfile.h, AbstractFile
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_FILESYS_WADFILE_H
#define LIBDENG_FILESYS_WADFILE_H

#ifdef __cplusplus

#include "lumpinfo.h"
#include "abstractfile.h"

namespace de {

class DFile;
class LumpIndex;
class PathDirectoryNode;

/**
 * WadFile. Runtime representation of an opened WAD file.
 */
class WadFile : public AbstractFile
{
public:
    /// Base class for format-related errors. @ingroup errors
    DENG2_ERROR(FormatError);

    /// The requested entry does not exist in the wad. @ingroup errors
    DENG2_ERROR(NotFoundError);

public:
    WadFile(DFile& file, char const* path, LumpInfo const& info);
    ~WadFile();

    /// @return @c true= @a lumpIdx is a valid logical index for a lump in this file.
    bool isValidIndex(int lumpIdx);

    /// @return Logical index of the last lump in this file's directory or @c -1 if empty.
    int lastIndex();

    /// @return Number of lumps contained by this file or @c 0 if empty.
    int lumpCount();

    /// @return @c true= There are no lumps in this file's directory.
    bool empty();

    /**
     * Retrieve the directory node for a lump contained by this file.
     *
     * @param lumpIdx       Logical index for the lump in this file's directory.
     *
     * @return  Directory node for this lump.
     *
     * @throws de::Error    If @a lumpIdx is not valid.
     */
    PathDirectoryNode& lumpDirectoryNode(int lumpIdx);

    /**
     * Compose the absolute VFS path to a lump contained by this file.
     *
     * @note Always returns a valid string object. If @a lumpIdx is not valid a
     *       zero-length string is returned.
     *
     * @param lumpIdx       Logical index for the lump.
     * @param delimiter     Delimit directory separators using this character.
     *
     * @return String containing the absolute path.
     */
    AutoStr* composeLumpPath(int lumpIdx, char delimiter = '/');

    /**
     * Retrieve the LumpInfo descriptor for a lump contained by this file.
     *
     * @param lumpIdx       Logical index for the lump in this file's directory.
     *
     * @return Lump info descriptor for the lump.
     *
     * @throws de::Error    If @a lumpIdx is not valid.
     */
    LumpInfo const& lumpInfo(int lumpIdx);

    /**
     * Lookup the uncompressed size of lump contained by this file.
     *
     * @param lumpIdx       Logical index for the lump in this file's directory.
     *
     * @return Size of the lump in bytes.
     *
     * @throws de::Error    If @a lumpIdx is not valid.
     *
     * @note This method is intended mainly for convenience. @see lumpInfo() for
     *       a better method of looking up multiple @ref LumpInfo properties.
     */
    size_t lumpSize(int lumpIdx);

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
     * @see lumpSize() or lumpInfo() to determine the size of buffer needed.
     */
    size_t readLump(int lumpIdx, uint8_t* buffer, bool tryCache = true);

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
     */
    size_t readLump(int lumpIdx, uint8_t* buffer, size_t startOffset, size_t length,
                    bool tryCache = true);

    /**
     * Read the data associated with lump @a lumpIdx into the cache.
     *
     * @param lumpIdx   Lump index associated with the data to be cached.
     *
     * @return Pointer to the cached copy of the associated data.
     */
    uint8_t const* cacheLump(int lumpIdx);

    /**
     * Remove a lock on a cached data lump.
     *
     * @param lumpIdx   Lump index associated with the cached data to be changed.
     *
     * @return This instance.
     */
    WadFile& unlockLump(int lumpIdx);

    /**
     * Clear any cached data for lump @a lumpIdx from the lump cache.
     *
     * @param lumpIdx       Lump index associated with the cached data to be cleared.
     * @param retCleared    If not @c NULL write @c true to this address if data was
     *                      present and subsequently cleared from the cache.
     *
     * @return This instance.
     */
    WadFile& clearCachedLump(int lumpIdx, bool* retCleared = 0);

    /**
     * Purge the lump cache, clearing all cached data lumps.
     *
     * @return This instance.
     */
    WadFile& clearLumpCache();

    /**
     * Publish lumps to the end of the specified @a index.
     *
     * @param index  Index to publish to.
     *
     * @return Number of lumps published to the index. Note that this is not
     *         necessarily equal to the the number of lumps in the file.
     */
    int publishLumpsToIndex(LumpIndex& index);

    /**
     * @attention Uses an extremely simple formula which does not conform to any CRC
     *            standard. Should not be used for anything critical.
     */
    uint calculateCRC();

// Static members ------------------------------------------------------------------

    /**
     * Does @a file appear to be in a format which can be represented with WadFile?
     *
     * @param file      Stream file handle/wrapper to be recognised.
     *
     * @return @c true= @a file can be represented with WadFile.
     */
    static bool recognise(DFile& file);

private:
    struct Instance;
    Instance* d;
};

} // namespace de

extern "C" {
#endif // __cplusplus

struct wadfile_s; // The wadfile instance (opaque)
//typedef struct wadfile_s WadFile;

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_FILESYS_WADFILE_H */
