/**\file abstractfile.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef LIBDENG_FILESYS_ABSTRACTFILE_H
#define LIBDENG_FILESYS_ABSTRACTFILE_H

#include "dfile.h"
#include "lumpinfo.h"

#ifdef __cplusplus
extern "C" {
#endif

// File types.
typedef enum {
    FT_UNKNOWNFILE,
    FT_ZIPFILE,
    FT_WADFILE,
    FT_LUMPFILE,
    FILETYPE_COUNT
} filetype_t;

#define VALID_FILETYPE(v)       ((v) >= FT_UNKNOWNFILE && (v) < FILETYPE_COUNT)

/**
 * Abstract File.  Abstract File is a core component of the filesystem
 * intended for use as the base for all types of (pseudo-)file resources.
 *
 * @ingroup fs
 */
typedef struct abstractfile_s {
    /// @see filetype_t
    filetype_t _type;

    struct abstractfile_flags_s {
        uint startup:1; ///< Loaded during the startup process.
        uint custom:1; /// < Not an original game resource.
    } _flags;

    /// protected: File stream handle/wrapper.
    DFile* _file;

    /// Absolute variable-length path in the vfs.
    ddstring_t _path;

    /// Info descriptor (file metadata).
    LumpInfo _info;

    /// Load order depth index.
    uint _order;
} abstractfile_t;

/**
 * Initialize this resource.
 *
 * @param af    AbstractFile instance.
 * @param type  File type identifier.
 * @param path  Path to this file in the virtual file system.
 * @param file  Handle to the file. AbstractFile takes ownership of the handle.
 * @param info  Lump info descriptor for the file. A copy is made.
 *
 * @return  @a af for convenience (chaining).
 */
abstractfile_t* AbstractFile_Init(abstractfile_t* af, filetype_t type,
    const char* path, DFile* file, const LumpInfo* info);

/**
 * Release all memory acquired for objects linked with this resource.
 * @param af    AbstractFile instance.
 */
void AbstractFile_Destroy(abstractfile_t* af);

/**
 * @param af    AbstractFile instance.
 * @return  Type of this resource @see filetype_t
 */
filetype_t AbstractFile_Type(const abstractfile_t* af);

/**
 * @param af    AbstractFile instance.
 * @return  Immutable copy of the info descriptor for this resource.
 */
const LumpInfo* AbstractFile_Info(abstractfile_t* af);

/**
 * @param af    AbstractFile instance.
 * @return  Owning package else @c NULL if not contained.
 */
abstractfile_t* AbstractFile_Container(const abstractfile_t* af);

/**
 * Accessors:
 */

/// @return  Absolute (i.e., resolved but possibly virtual/mapped) path to this resource.
const ddstring_t* AbstractFile_Path(const abstractfile_t* af);

/// @return  Load order index for this resource.
uint AbstractFile_LoadOrderIndex(const abstractfile_t* af);

/// @return  "Last modified" timestamp of the resource.
uint AbstractFile_LastModified(const abstractfile_t* af);

/// @return  @c true if the resource is marked "startup".
boolean AbstractFile_HasStartup(const abstractfile_t* af);

/// Mark this resource as "startup".
void AbstractFile_SetStartup(abstractfile_t* af, boolean yes);

/// @return  @c true if the resource is marked "custom".
boolean AbstractFile_HasCustom(const abstractfile_t* af);

/// Mark this resource as "custom".
void AbstractFile_SetCustom(abstractfile_t* af, boolean yes);

size_t AbstractFile_BaseOffset(const abstractfile_t* af);

DFile* AbstractFile_Handle(abstractfile_t* af);

/**
 * Abstract interface (minimal, data caching interface not expected):
 */

/**
 * Read the data associated with the specified lump index into @a buffer.
 *
 * @param lumpIdx  Lump index associated with the data being read.
 * @param buffer  Buffer to read into. Must be at least W_LumpLength() bytes.
 * @return  Number of bytes read.
 */
size_t AbstractFile_ReadLump(abstractfile_t* af, int lumpIdx, uint8_t* buffer);

/**
 * Accessors:
 */

/// @return  Number of "lumps" contained within this resource.
int AbstractFile_LumpCount(abstractfile_t* af);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_FILESYS_ABSTRACTFILE_H */
