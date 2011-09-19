/**\file abstractfile.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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

#include <stdio.h>

#include "sys_file.h"
#include "lumpinfo.h"

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
        uint open:1; // Presently open.
        uint startup:1; // Loaded during the startup process.
        uint iwad:1; // Owned by or is an "iwad" resource.
    } _flags;

    /// File stream abstraction wrapper/handle.
    streamfile_t _stream;

    /// Offset from start of owning package.
    size_t _baseOffset;

    /// Info descriptor (file metadata).
    lumpinfo_t _info;

    /// Load order depth index.
    uint _order;
} abstractfile_t;

/**
 * Initialize this resource.
 *
 * @param type  File type identifier.
 * @param baseOffset  Offset from the start of the file in bytes to begin.
 * @param info  Lump info descriptor for the file. A copy is made.
 * @param sf  Stream file handle/wrapper to the file being interpreted.
 *      A copy is made unless @c NULL in which case a default-stream is initialized.
 * @return  Same as @a file for convenience (chaining).
 */
abstractfile_t* AbstractFile_Init(abstractfile_t* file, filetype_t type, size_t baseOffset,
    const lumpinfo_t* info, streamfile_t* sf);

/// @return  Type of this resource @see filetype_t
filetype_t AbstractFile_Type(const abstractfile_t* file);

/// @return  Immutable copy of the info descriptor for this resource.
const lumpinfo_t* AbstractFile_Info(abstractfile_t* file);

/// @return  Owning package else @c NULL if not contained.
abstractfile_t* AbstractFile_Container(const abstractfile_t* file);

/**
 * Accessors:
 */

/// @return  Absolute (i.e., resolved but possibly virtual/mapped) path to this resource.
const ddstring_t* AbstractFile_Path(const abstractfile_t* file);

/// @return  Load order index for this resource.
uint AbstractFile_LoadOrderIndex(const abstractfile_t* file);

/// @return  "Last modified" timestamp of the resource.
uint AbstractFile_LastModified(const abstractfile_t* file);

/// @return  @c true if the resource is marked "startup".
boolean AbstractFile_HasStartup(const abstractfile_t* file);

/// Mark this resource as "startup".
void AbstractFile_SetStartup(abstractfile_t* file, boolean yes);

/// @return  @c true if the resource is marked "IWAD".
boolean AbstractFile_HasIWAD(const abstractfile_t* file);

/// Mark this resource as "IWAD".
void AbstractFile_SetIWAD(abstractfile_t* file, boolean yes);

size_t AbstractFile_BaseOffset(const abstractfile_t* file);

/**
 * Abstract interface (minimal, data caching interface not expected):
 */

/// Close this resource if open and release any acquired identifiers.
void AbstractFile_Close(abstractfile_t* file);

/**
 * Read the data associated with the specified lump index into @a buffer.
 *
 * @param lumpIdx  Lump index associated with the data being read.
 * @param buffer  Buffer to read into. Must be at least W_LumpLength() bytes.
 * @return  Number of bytes read.
 */
size_t AbstractFile_ReadLump(abstractfile_t* file, int lumpIdx, uint8_t* buffer);

/**
 * Retrieve the low-level file handle used for direct manipulation of a stream.
 * \note Higher-level derivatives of AbstractFile should not expose this method
 *      publicly if they are designed to abstract access to the underlying stream.
 */
streamfile_t* AbstractFile_Handle(abstractfile_t* file);

/**
 * Accessors:
 */

/// @return  Number of "lumps" contained within this resource.
int AbstractFile_LumpCount(abstractfile_t* file);

#endif /* LIBDENG_FILESYS_ABSTRACTFILE_H */
