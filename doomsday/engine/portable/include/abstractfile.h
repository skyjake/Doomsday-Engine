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

#include "dfile.h"
#include "lumpinfo.h"

#ifdef __cplusplus
extern "C" {
#endif

// File types.
/// @todo Refactor away.
typedef enum {
    FT_UNKNOWNFILE,
    FT_ZIPFILE,
    FT_WADFILE,
    FT_LUMPFILE,
    FILETYPE_COUNT
} filetype_t;

#define VALID_FILETYPE(v)       ((v) >= FT_UNKNOWNFILE && (v) < FILETYPE_COUNT)

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
namespace de {

/**
 * Abstract File.  Abstract File is a core component of the filesystem
 * intended for use as the base for all types of (pseudo-)file resources.
 *
 * @ingroup fs
 */
class AbstractFile
{
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
    ~AbstractFile();

    /**
     * @return  Type of this resource @see filetype_t
     */
    filetype_t type() const;

    /**
     * @return  Immutable copy of the info descriptor for this resource.
     */
    LumpInfo const* info() const;

    /**
     * @return  Owning package else @c NULL if not contained.
     */
    AbstractFile* container() const;

    /**
     * Accessors:
     */

    /// @return  Absolute (i.e., resolved but possibly virtual/mapped) path to this resource.
    ddstring_t const* path() const;

    /// @return  Load order index for this resource.
    uint loadOrderIndex() const;

    /// @return  "Last modified" timestamp of the resource.
    uint lastModified() const;

    /// @return  @c true if the resource is marked "startup".
    bool hasStartup() const;

    /// Mark this resource as "startup".
    void setStartup(bool yes);

    /// @return  @c true if the resource is marked "custom".
    bool hasCustom() const;

    /// Mark this resource as "custom".
    void setCustom(bool yes);

    size_t baseOffset() const;

    DFile* handle();

    /**
     * Abstract interface (minimal, data caching interface not expected):
     */

    /**
     * Read the data associated with the specified lump index into @a buffer.
     *
     * @param lumpIdx  Lump index associated with the data being read.
     * @param buffer  Buffer to read into. Must be at least W_LumpLength() bytes.
     * @return  Number of bytes read.
     *
     * @todo Should be virtual
     */
    size_t readLump(int lumpIdx, uint8_t* buffer);

    /**
     * @return  Number of "lumps" contained within this resource.
     *
     * @todo Should be virtual
     */
    int lumpCount();

private:
    /// @see filetype_t
    filetype_t type_;

    struct abstractfile_flags_s
    {
        uint startup:1; ///< Loaded during the startup process.
        uint custom:1; /// < Not an original game resource.
    } flags;

protected:
    /// File stream handle/wrapper.
    DFile* file;

private:
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

/**
 * C wrapper API:
 */

struct abstractfile_s; // The abstractfile instance (opaque)
typedef struct abstractfile_s AbstractFile;

filetype_t AbstractFile_Type(AbstractFile const* af);

LumpInfo const* AbstractFile_Info(AbstractFile const* af);

AbstractFile* AbstractFile_Container(AbstractFile const* af);

ddstring_t const* AbstractFile_Path(AbstractFile const* af);

uint AbstractFile_LoadOrderIndex(AbstractFile const* af);

uint AbstractFile_LastModified(AbstractFile const* af);

boolean AbstractFile_HasStartup(AbstractFile const* af);

void AbstractFile_SetStartup(AbstractFile* af, boolean yes);

boolean AbstractFile_HasCustom(AbstractFile const* af);

void AbstractFile_SetCustom(AbstractFile* af, boolean yes);

size_t AbstractFile_BaseOffset(AbstractFile const* af);

DFile* AbstractFile_Handle(AbstractFile* af);

size_t AbstractFile_ReadLump(AbstractFile* af, int lumpIdx, uint8_t* buffer);

int AbstractFile_LumpCount(AbstractFile* af);

/// @todo Refactor away:
AbstractFile* UnknownFile_New(DFile* file, char const* path, LumpInfo const* info);

void UnknownFile_Delete(AbstractFile* af);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_FILESYS_ABSTRACTFILE_H */
