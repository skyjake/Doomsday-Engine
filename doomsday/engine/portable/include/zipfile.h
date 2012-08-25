/**
 * @file zipfile.h
 * Specialization of AbstractFile for working with Zip archives.
 *
 * @note Presently only the zlib method (Deflate) of compression is supported.
 *
 * @ingroup fs
 *
 * @see abstractfile.h, AbstractFile
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_FILESYS_ZIPFILE_H
#define LIBDENG_FILESYS_ZIPFILE_H

#include "abstractfile.h"
#include "lumpinfo.h"
#include "lumpdirectory.h"

struct pathdirectorynode_s;
struct zipfile_s;

/**
 * ZipFile instance. Constructed with ZipFile_New().
 */
typedef struct zipfile_s ZipFile;

/**
 * Constructs a new zipfile. The zipfile has to be destroyed with ZipFile_Delete()
 * after it is not needed any more.
 *
 * @param file  Virtual file handle to the underlying file resource.
 * @param path  Virtual file system path to associate with the resultant zipfile.
 * @param info  File info descriptor for the resultant zipfile. A copy is made.
 */
ZipFile* ZipFile_New(DFile* file, const char* path, const LumpInfo* info);

/**
 * Destroys the zipfile.
 */
void ZipFile_Delete(ZipFile* zip);

/**
 * Publish lumps to the end of the specified @a directory.
 *
 * @param zip  ZipFile instance.
 * @param directory  Directory to publish to.
 * @return  Number of lumps published to the directory (not necessarily the same as
 *          ZipFile_LumpCount()).
 */
int ZipFile_PublishLumpsToDirectory(ZipFile* zip, LumpDirectory* directory);

/**
 * Lookup a directory node for a lump contained by this zipfile.
 *
 * @param zip  ZipFile instance.
 * @param lumpIdx  Logical index for the lump within the zipfile's internal directory.
 * @return  Found directory node else @c NULL if @a lumpIdx is not valid.
 */
struct pathdirectorynode_s* ZipFile_LumpDirectoryNode(ZipFile* zip, int lumpIdx);

/**
 * Lookup a lump info descriptor for a lump contained by this zipfile.
 *
 * @param zip  ZipFile instance.
 * @param lumpIdx  Logical index for the lump within the zipfile's internal directory.
 * @return  Found lump info else @c NULL if @a lumpIdx is not valid.
 */
const LumpInfo* ZipFile_LumpInfo(ZipFile* zip, int lumpIdx);

/**
 * Compose the full virtual file system path to a lump contained by this zipfile.
 *
 * @note Always returns a valid string object. In the case of an invalid @a lumpIdx
 *       a zero-length string is returned.
 *
 * @param zip  ZipFile instance.
 * @param lumpIdx  Logical index for the lump.
 * @param delimiter  Delimit directory separators using this character (default: '/').
 * @return  String containing the full path.
 */
AutoStr* ZipFile_ComposeLumpPath(ZipFile* zip, int lumpIdx, char delimiter);

/**
 * Read the data associated with the specified lump index into @a buffer.
 *
 * @param zip  ZipFile instance.
 * @param lumpIdx  Lump index associated with the data being read.
 * @param buffer  Buffer to read into. Must be at least W_LumpLength() bytes.
 * @param tryCache  @c true = try the lump cache first.
 * @return  Number of bytes read.
 */
size_t ZipFile_ReadLump2(ZipFile* zip, int lumpIdx, uint8_t* buffer, boolean tryCache);
size_t ZipFile_ReadLump(ZipFile* zip, int lumpIdx, uint8_t* buffer);

/**
 * Read a subsection of the data associated with the specified lump index into @a buffer.
 *
 * @param zip  ZipFile instance.
 * @param lumpIdx  Lump index associated with the data being read.
 * @param buffer  Buffer to read into. Must be at least W_LumpLength() bytes.
 * @param startOffset  Offset from the beginning of the lump to start reading.
 * @param length  Number of bytes to be read.
 * @param tryCache  @c true = try the lump cache first.
 * @return  Number of bytes read.
 */
size_t ZipFile_ReadLumpSection2(ZipFile* zip, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length, boolean tryCache);
size_t ZipFile_ReadLumpSection(ZipFile* zip, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length);

/**
 * Read the data associated with the specified lump index into the cache.
 *
 * @param zip  ZipFile instance.
 * @param lumpIdx  Lump index associated with the data being read.
 * @param tag  Zone purge level/cache tag to use.
 * @return  Ptr to the cached copy of the associated data.
 */
const uint8_t* ZipFile_CacheLump(ZipFile* zip, int lumpIdx, int tag);

/**
 * Change the Zone purge level/cache tag associated with a cached data lump.
 *
 * @param zip  ZipFile instance.
 * @param lumpIdx  Lump index associated with the cached data being changed.
 * @param tag  Zone purge level/cache tag to use.
 */
void ZipFile_ChangeLumpCacheTag(ZipFile* zip, int lumpIdx, int tag);

void ZipFile_ClearLumpCache(ZipFile* zip);

// Accessors --------------------------------------------------------------

/// @return  Number of lumps contained within this file.
int ZipFile_LumpCount(ZipFile* zip);

// Static members ----------------------------------------------------------

/**
 * Determines whether the specified file appears to be in a format recognised by
 * ZipFile.
 * @param file  Stream file handle/wrapper to the file being interpreted.
 *
 * @return  @c true iff this is a file that can be represented using ZipFile.
 */
boolean ZipFile_Recognise(DFile* file);

/**
 * Inflates a block of data compressed using ZipFile_Compress() (i.e., zlib
 * deflate algorithm).
 *
 * @param in       Pointer to compressed data.
 * @param inSize   Size of the compressed data.
 * @param outSize  Size of the uncompressed data is written here. Must not
 *                 be @c NULL.
 *
 * @return  Pointer to the uncompressed data. Caller gets ownership of the
 * returned memory and must free it with M_Free().
 *
 * @see ZipFile_Compress()
 */
uint8_t* ZipFile_Uncompress(uint8_t* in, size_t inSize, size_t* outSize);

/**
 * Inflates a compressed block of data using zlib. The caller must figure out
 * the uncompressed size of the data before calling this.
 *
 * zlib will expect raw deflate data, not looking for a zlib or gzip header,
 * not generating a check value, and not looking for any check values for
 * comparison at the end of the stream.
 *
 * @param in       Pointer to compressed data.
 * @param inSize   Size of the compressed data.
 * @param out      Pointer to output buffer.
 * @param outSize  Size of the output buffer. This must match the size of the
 *                 decompressed data.
 *
 * @return  @c true if successful.
 */
boolean ZipFile_UncompressRaw(uint8_t* in, size_t inSize, uint8_t* out, size_t outSize);

/**
 * Compresses a block of data using zlib with the default/balanced
 * compression level.
 *
 * @param in       Pointer to input data to compress.
 * @param inSize   Size of the input data.
 * @param outSize  Pointer where the size of the compressed data will be written.
 *                 Cannot be @c NULL.
 *
 * @return  Compressed data. The caller gets ownership of this memory and must
 *          free it with M_Free(). If an error occurs, returns @c NULL and
 *          @a outSize is set to zero.
 */
uint8_t* ZipFile_Compress(uint8_t* in, size_t inSize, size_t* outSize);

/**
 * Compresses a block of data using zlib.
 *
 * @param in       Pointer to input data to compress.
 * @param inSize   Size of the input data.
 * @param outSize  Pointer where the size of the compressed data will be written.
 *                 Cannot be @c NULL.
 * @param level    Compression level: 0=none/fastest ... 9=maximum/slowest.
 *
 * @return  Compressed data. The caller gets ownership of this memory and must
 *          free it with M_Free(). If an error occurs, returns @c NULL and
 *          @a outSize is set to zero.
 */
uint8_t* ZipFile_CompressAtLevel(uint8_t* in, size_t inSize, size_t* outSize, int level);

#endif // LIBDENG_FILESYS_ZIPFILE_H
