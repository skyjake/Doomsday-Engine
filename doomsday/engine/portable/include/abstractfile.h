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

// File types.
typedef enum {
    FT_UNKNOWNFILE,
    FT_ZIPFILE,
    FT_WADFILE,
    FT_LUMPFILE
} filetype_t;

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
        uint startup:1; // Loaded during the startup process.
        uint iwad:1; // Owned by or is an "iwad" resource.
    } _flags;

    /// File stream abstraction wrapper/handle for this resource.
    DFILE _dfile;

    /// Absolute path to this resource in the vfs.
    ddstring_t _path;

    /// Load order depth index.
    uint _order;
} abstractfile_t;

/// Initialize this resource.
void AbstractFile_Init(abstractfile_t* file, filetype_t type, const char* absolutePath);

/// @return  Type of this resource @see filetype_t
filetype_t AbstractFile_Type(const abstractfile_t* file);

/**
 * Accessors:
 */

/// @return  Absolute (i.e., resolved but possibly virtual/mapped) path to this resource.
const ddstring_t* AbstractFile_Path(abstractfile_t* file);

/// @return  Load order index for this resource.
uint AbstractFile_LoadOrderIndex(abstractfile_t* file);

/// @return  "Last modified" timestamp of the resource.
uint AbstractFile_LastModified(abstractfile_t* file);

/// @return  @c true if the resource is marked "startup".
boolean AbstractFile_HasStartup(abstractfile_t* file);

/// Mark this resource as "startup".
void AbstractFile_SetStartup(abstractfile_t* file, boolean yes);

/// @return  @c true if the resource is marked "IWAD".
boolean AbstractFile_HasIWAD(abstractfile_t* file);

/// Mark this resource as "IWAD".
void AbstractFile_SetIWAD(abstractfile_t* file, boolean yes);

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
DFILE* AbstractFile_Handle(abstractfile_t* file);

/**
 * Accessors:
 */

/// @return  Number of "lumps" contained within this resource.
int AbstractFile_LumpCount(abstractfile_t* file);

/**
 * \todo The following function declarations do not belong here:
 */

abstractfile_t* F_NewFile(const char* absolutePath);

#endif /* LIBDENG_FILESYS_ABSTRACTFILE_H */
