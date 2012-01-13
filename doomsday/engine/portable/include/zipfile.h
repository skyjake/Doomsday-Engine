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
 * Publish lumps to the end of the specified @a directory and prune any duplicates
 * (see LumpDirectory_PruneDuplicateRecords()).
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
 * @return  String containing the full path. Has to be destroyed with Str_Delete()
 *          once it is no longer needed.
 */
ddstring_t* ZipFile_ComposeLumpPath(ZipFile* zip, int lumpIdx, char delimiter);

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

/**
 * Accessors:
 */

/// @return  Number of lumps contained within this file.
int ZipFile_LumpCount(ZipFile* zip);

/**
 * Static members:
 */

/**
 * Does the specified file appear to be in a format recognised by ZipFile?
 *
 * @param file  Stream file handle/wrapper to the file being interpreted.
 * @return  @c true iff this is a file that can be represented using ZipFile.
 */
boolean ZipFile_Recognise(DFile* file);

#endif // LIBDENG_FILESYS_ZIPFILE_H
