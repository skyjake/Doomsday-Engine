/**
 * @file zip.h
 * Specialization of AbstractFile for working with Zip archives.
 *
 * @note Presently only the zlib method (Deflate) of compression is supported.
 *
 * @ingroup fs
 *
 * @see abstractfile.h, AbstractFile
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_FILESYS_ZIP_H
#define LIBDENG_FILESYS_ZIP_H

#ifdef __cplusplus

#include "abstractfile.h"
#include "fileinfo.h"

namespace de {

class DFile;
class LumpIndex;
class PathDirectoryNode;

/**
 * Zip. Runtime representation of an opened ZIP file.
 */
class Zip : public AbstractFile
{
public:
    Zip(DFile& file, char const* path, FileInfo const& info);
    ~Zip();

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
     * Retrieve the FileInfo descriptor for a lump contained by this file.
     *
     * @param lumpIdx       Logical index for the lump in this file's directory.
     *
     * @return Lump info descriptor for the lump.
     *
     * @throws de::Error    If @a lumpIdx is not valid.
     */
    FileInfo const& lumpInfo(int lumpIdx);

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
     *       a better method of looking up multiple @ref FileInfo properties.
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
    Zip& unlockLump(int lumpIdx);

    /**
     * Clear any cached data for lump @a lumpIdx from the lump cache.
     *
     * @param lumpIdx       Lump index associated with the cached data to be cleared.
     * @param retCleared    If not @c NULL write @c true to this address if data was
     *                      present and subsequently cleared from the cache.
     *
     * @return This instance.
     */
    Zip& clearCachedLump(int lumpIdx, bool* retCleared = 0);

    /**
     * Purge the lump cache, clearing all cached data lumps.
     *
     * @return This instance.
     */
    Zip& clearLumpCache();

// Static members ------------------------------------------------------------------

    /**
     * Determines whether the specified file appears to be in a format recognised by
     * Zip.
     *
     * @param file      Stream file handle/wrapper to the file being interpreted.
     *
     * @return  @c true= this is a file that can be represented using Zip.
     */
    static bool recognise(DFile& file);

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
    static uint8_t* uncompress(uint8_t* in, size_t inSize, size_t* outSize);

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
    static bool uncompressRaw(uint8_t* in, size_t inSize, uint8_t* out, size_t outSize);

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
    static uint8_t* compress(uint8_t* in, size_t inSize, size_t* outSize);

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
    static uint8_t* compressAtLevel(uint8_t* in, size_t inSize, size_t* outSize, int level);

private:
    struct Instance;
    Instance* d;
};

} // namespace de

extern "C" {
#endif // __cplusplus

struct zipfile_s; // The  zipfile instance (opaque)
//typedef struct zipfile_s Zip;

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_FILESYS_ZIP_H */
