/**
 * @file abstractfile.h
 *
 * Abstract base for all classes which represent opened files.
 *
 * @ingroup fs
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

#ifndef LIBDENG_FILESYS_ABSTRACTFILE_H
#define LIBDENG_FILESYS_ABSTRACTFILE_H

#ifdef __cplusplus
extern "C" {
#endif

// File types.
/// @todo Refactor away.
typedef enum {
    FT_GENERICFILE,
    FT_ZIPFILE,
    FT_WADFILE,
    FT_LUMPFILE,
    FILETYPE_COUNT
} filetype_t;

#define VALID_FILETYPE(v)       ((v) >= FT_GENERICFILE && (v) < FILETYPE_COUNT)

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

#include <de/str.h>
#include "dfile.h"
#include "lumpinfo.h"

namespace de {

class LumpIndex;
class PathDirectoryNode;

/**
 * Abstract File.  Abstract File is a core component of the filesystem
 * intended for use as the base for all types of (pseudo-)file resources.
 *
 * @ingroup fs
 */
class AbstractFile
{
private:
    AbstractFile();

public:
    /**
     * @param type  File type identifier.
     * @param path  Path to this file in the virtual file system.
     * @param file  Handle to the file. Ownership of the handle is given to this instance.
     * @param info  Lump info descriptor for the file. A copy is made.
     */
    AbstractFile(filetype_t _type, char const* _path, DFile& file, LumpInfo const& _info);

    /**
     * Release all memory acquired for objects linked with this resource.
     */
    virtual ~AbstractFile();

    /**
     * @return  Type of this resource @see filetype_t
     */
    filetype_t type() const;

    /// @return  Absolute (i.e., resolved but possibly virtual/mapped) path to this resource.
    ddstring_t const* path() const;

    /// @return @c true= this file is contained within another.
    bool isContained() const;

    /// @return The file instance which contains this.
    AbstractFile& container() const;

    /// @return  Load order index for this resource.
    uint loadOrderIndex() const;

    /**
     * @return  Immutable copy of the info descriptor for this resource.
     */
    LumpInfo const& info() const;

    // Convenient lookup method for when only the last-modified property is needed from info().
    /// @return  "Last modified" timestamp of the resource.
    inline uint lastModified() const {
        return info().lastModified;
    }

    // Convenient lookup method for when only the size property is needed from info().
    /// @return  Size of the uncompressed resource.
    inline uint size() const {
        return info().size;
    }

    // Convenient lookup method for when only the is-compressed property is needed from info().
    /// @return  Size of the uncompressed resource.
    inline bool isCompressed() const {
        return info().isCompressed();
    }

    /// @return  @c true if the resource is marked "startup".
    bool hasStartup() const;

    /// Mark this resource as "startup".
    AbstractFile& setStartup(bool yes);

    /// @return  @c true if the resource is marked "custom".
    bool hasCustom() const;

    /// Mark this resource as "custom".
    AbstractFile& setCustom(bool yes);

    DFile* handle();

    /**
     * Access interfaces:
     *
     * @todo Extract these into one or more interface classes/subcomponents.
     */

    /**
     * @return @c true= @a lumpIdx is a valid logical index for a lump in this file.
     *
     * @attention This default implementation assumes there is only one lump in
     * the file. Subclasses with multiple lumps should override this function
     * accordingly.
     */
    virtual bool isValidIndex(int lumpIdx) { return lumpIdx == 0; }

    /**
     * @return Logical index of the last lump in this file's directory or @c -1 if empty.
     *
     * @attention This default implementation assumes there is only one lump in
     * the file. Subclasses with multiple lumps should override this function
     * accordingly.
     */
    virtual int lastIndex() { return 0; }

    /**
     * @return  Number of "lumps" contained within this resource.
     *
     * @attention This default implementation assumes there is only one lump in
     * the file. Subclasses with multiple lumps should override this function
     * accordingly.
     */
    virtual int lumpCount() { return 1; }

    /**
     * Retrieve the directory node for a lump contained by this file.
     *
     * @param lumpIdx       Logical index for the lump in this file's directory.
     *
     * @return  Directory node for this lump.
     *
     * @throws de::Error    If @a lumpIdx is not valid.
     */
    virtual PathDirectoryNode const& lumpDirectoryNode(int lumpIdx) = 0;

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
    virtual AutoStr* composeLumpPath(int lumpIdx, char delimiter = '/') = 0;

    /**
     * Retrieve the LumpInfo descriptor for a lump contained by this file.
     *
     * @param lumpIdx       Logical index for the lump in this file's directory.
     *
     * @return Lump info descriptor for the lump.
     *
     * @throws de::Error    If @a lumpIdx is not valid.
     *
     * @attention This default implementation assumes there is only one lump in
     * the file and therefore its decriptor is that of the file itself. Subclasses
     * with multiple lumps should override this function accordingly.
     */
    virtual LumpInfo const& lumpInfo(int /*lumpIdx*/) { return info(); }

    /**
     * Lookup the uncompressed size of lump contained by this file.
     *
     * @param lumpIdx       Logical index for the lump in this file's directory.
     *
     * @return Size of the lump in bytes.
     *
     * @note This method is intended mainly for convenience. @see lumpInfo() for
     *       a better method of looking up multiple @ref LumpInfo properties.
     *
     * @attention This default implementation assumes there is only one lump in
     * the file and therefore its decriptor is that of the file itself. Subclasses
     * with multiple lumps should override this function accordingly.
     *
     */
    virtual size_t lumpSize(int lumpIdx) = 0;

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
    virtual size_t readLump(int lumpIdx, uint8_t* buffer, bool tryCache = true) = 0;

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
    virtual size_t readLump(int lumpIdx, uint8_t* buffer, size_t startOffset, size_t length,
                            bool tryCache = true) = 0;

    /**
     * Publish this lump to the end of the specified @a index.
     *
     * @param index  Index to publish to.
     *
     * @return Number of lumps published to the index.
     */
    virtual int publishLumpsToIndex(LumpIndex& index) = 0;

    /**
     * Lump caching interface:
     */

    /**
     * Read the data associated with lump @a lumpIdx into the cache.
     *
     * @param lumpIdx   Lump index associated with the data to be cached.
     *
     * @return Pointer to the cached copy of the associated data.
     */
    virtual uint8_t const* cacheLump(int lumpIdx) = 0;

    /**
     * Remove a lock on a cached data lump.
     *
     * @param lumpIdx   Lump index associated with the cached data to be changed.
     *
     * @return This instance.
     */
    virtual AbstractFile& unlockLump(int lumpIdx) = 0;

    /**
     * Clear any cached data for lump @a lumpIdx from the lump cache.
     *
     * @param lumpIdx       Lump index associated with the cached data to be cleared.
     * @param retCleared    If not @c NULL write @c true to this address if data was
     *                      present and subsequently cleared from the cache.
     *
     * @return This instance.
     */
    //virtual AbstractFile& clearCachedLump(int lumpIdx, bool* retCleared = 0) = 0;

    /**
     * Purge the lump cache, clearing all cached data lumps.
     *
     * @return This instance.
     */
    //virtual AbstractFile& clearLumpCache() = 0;

protected:
    /// File stream handle/wrapper.
    DFile* file;

private:
    /// @see filetype_t
    filetype_t type_;

    struct abstractfile_flags_s
    {
        uint startup:1; ///< Loaded during the startup process.
        uint custom:1; /// < Not an original game resource.
    } flags;

    /// Absolute variable-length path in the vfs.
    ddstring_t path_;

    /// Info descriptor (file metadata).
    LumpInfo info_;

    /// Load order depth index.
    uint order;
};

} // namespace de

extern "C" {
#endif // __cplusplus

struct abstractfile_s; // The abstractfile instance (opaque)
typedef struct abstractfile_s AbstractFile;

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_FILESYS_ABSTRACTFILE_H */
