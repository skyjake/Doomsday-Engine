/**
 * @file file.h
 *
 * Base for all classes which represent opened files.
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

#ifndef LIBDENG_FILESYS_FILE_H
#define LIBDENG_FILESYS_FILE_H

#ifdef __cplusplus

#include <de/Error>
#include <de/str.h>
#include "filehandle.h"
#include "fileinfo.h"
#include "pathtree.h"

namespace de {

class LumpIndex;

/**
 * File.  File is a core component of the filesystem intended for use as the base
 * for all types of (pseudo-)file resources.
 *
 * @ingroup fs
 */
class File1
{
public:
    /// Categorization flags.
    enum Flag
    {
        /// Flagged as having been loaded during the game startup process.
        Startup     = 0x1,

        /// Flagged as a non-original game resource.
        Custom      = 0x2,

        /// All resources are by default flagged as "custom".
        DefaultFlags = Custom
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    /// This file is not contained. @ingroup errors
    DENG2_ERROR(NotContainedError);

private:
    File1();

public:
    /**
     * @param hndl          Handle to the file. Ownership of the handle is given to this instance.
     * @param path          Path to this file in the virtual file system.
     * @param info          Info descriptor for the file. A copy is made.
     * @param container     Container of this file. Can be @c NULL.
     */
    File1(FileHandle& hndl, char const* _path, FileInfo const& _info, File1* container = 0);

    /**
     * Release all memory acquired for objects linked with this resource.
     */
    virtual ~File1();

    /// @return  Name of this file.
    virtual ddstring_t const* name() const;

    /**
     * Compose the absolute VFS path to this file.
     *
     * @param delimiter     Delimit directory using this character.
     *
     * @return String containing the absolute path.
     */
    virtual AutoStr* composePath(char delimiter = '/') const;

    /// @return  @c true iff this file is contained by another.
    bool isContained() const;

    /// @return  The file instance which contains this.
    File1& container() const;

    /// @return  Load order index for this resource.
    uint loadOrderIndex() const;

    /**
     * @return  Immutable copy of the info descriptor for this resource.
     */
    FileInfo const& info() const;

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
    File1& setStartup(bool yes);

    /// @return  @c true if the resource is marked "custom".
    bool hasCustom() const;

    /// Mark this resource as "custom".
    File1& setCustom(bool yes);

    FileHandle& handle();

    /**
     * Retrieve the directory node for this file.
     *
     * @return  Directory node for this file.
     */
    virtual PathTree::Node const& directoryNode() const
    {
        throw de::Error("File1::directoryNode", "No owner directory");
    }

    /*
     * Access interfaces:
     *
     * @todo Extract these into one or more interface classes/subcomponents?
     */

    /**
     * @return @c true= @a lumpIdx is a valid logical index for a lump in this file.
     *
     * @attention This default implementation assumes there is only one lump in
     * the file. Subclasses with multiple lumps should override this function
     * accordingly.
     */
    virtual bool isValidIndex(int lumpIdx) const { return lumpIdx == 0; }

    /**
     * @return Logical index of the last lump in this file's directory or @c -1 if empty.
     *
     * @attention This default implementation assumes there is only one lump in
     * the file. Subclasses with multiple lumps should override this function
     * accordingly.
     */
    virtual int lastIndex() const { return 0; }

    /**
     * @return  Number of "lumps" contained within this resource.
     *
     * @attention This default implementation assumes there is only one lump in
     * the file. Subclasses with multiple lumps should override this function
     * accordingly.
     */
    virtual int lumpCount() const { return 1; }

    /**
     * Retrieve a lump contained by this file.
     *
     * @param lumpIdx       Logical index for the lump in this file's directory.
     *
     * @return The lump.
     *
     * @throws de::Error    If @a lumpIdx is not valid.
     *
     * @attention This default implementation assumes there is only one lump in
     * the file and therefore *this* is that lump. Subclasses with multiple lumps
     * should override this function accordingly.
     */
    virtual File1& lump(int /*lumpIdx*/) { return *this; }

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
    virtual size_t read(uint8_t* buffer, bool tryCache = true);

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
    virtual size_t read(uint8_t* buffer, size_t startOffset, size_t length,
                        bool tryCache = true);

    /*
     * Caching interface:
     */

    /**
     * Read this file into the local cache.
     *
     * @return Pointer to the cached copy of the associated data.
     */
    virtual uint8_t const* cache();

    /**
     * Remove a lock on the locally cached data.
     *
     * @return This instance.
     */
    virtual File1& unlock();

    /**
     * Clear any data in the local cache.
     *
     * @param retCleared    If not @c NULL write @c true to this address if data was
     *                      present and subsequently cleared from the cache.
     *
     * @return This instance.
     */
    virtual File1& clearCache(bool* retCleared = 0);

protected:
    /// File stream handle.
    FileHandle* handle_;

    /// Info descriptor (file metadata).
    FileInfo info_;

    /// The container file (if any).
    File1* container_;

private:
    /// Categorization flags.
    Flags flags;

    /// Absolute variable-length path in the vfs.
    ddstring_t path_;

    /// Load order depth index.
    uint order;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(File1::Flags)

} // namespace de

extern "C" {
#endif // __cplusplus

struct file1_s; // The file instance (opaque)
typedef struct file1_s File1;

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_FILESYS_FILE_H */
