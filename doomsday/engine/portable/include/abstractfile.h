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

struct lumpdirectory_s;

// File types.
typedef enum {
    FT_UNKNOWNFILE,
    FT_ZIPFILE,
    FT_WADFILE,
    FT_LUMPFILE
} filetype_t;

/**
 * Abstract File base. To be used as the basis for all types of files.
 * @ingroup fs
 */
typedef struct abstractfile_s {
    filetype_t _type;
    DFILE _dfile;
    ddstring_t _absolutePath;
    /// Load order depth index.
    uint _order;
} abstractfile_t;

/// Initialize this file.
void AbstractFile_Init(abstractfile_t* file, filetype_t type, FILE* handle, const char* absolutePath);

/// @return  Type of this file.
filetype_t AbstractFile_Type(const abstractfile_t* file);

/**
 * Accessors:
 */

/// @return  Resolved (possibly virtual/mapped) path to this file.
const ddstring_t* AbstractFile_AbsolutePath(abstractfile_t* file);

/// @return  Load order index for this file.
uint AbstractFile_LoadOrderIndex(abstractfile_t* file);

/// @return  "Last modified" timestamp of the file.
uint AbstractFile_LastModified(abstractfile_t* file);

/**
 * Abstract interface (minimal, data caching interface not expected):
 */

/// Close this file if open and release any acquired file identifiers.
void AbstractFile_Close(abstractfile_t* file);

/**
 * Read the data associated with the specified lump index into @a buffer.
 *
 * @param lumpIdx  Lump index associated with the data being read.
 * @param dest  Buffer to read into. Must be at least W_LumpLength() bytes.
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

/// @return  Number of lumps contained within this file.
int AbstractFile_LumpCount(abstractfile_t* file);

/// @return  @c true if the file is marked as an "IWAD".
boolean AbstractFile_IsIWAD(abstractfile_t* file);

/**
 * \todo The following function declarations do not belong here:
 */

abstractfile_t* F_NewFile(const char* absolutePath);
void F_Delete(abstractfile_t* file);

/**
 * Open a new stream on the specified lump for reading.
 *
 * @param file  File system handle used to reference the lump once read.
 * @param container  File system record for the file containing the lump to be read.
 * @param lump  Index of the lump to open.
 * @param dontBuffer  Just test for access (don't buffer anything).
 *
 * @return  Same as @a file for convenience.
 */
abstractfile_t* F_OpenStreamLump(abstractfile_t* file, abstractfile_t* container, int lumpIdx, boolean dontBuffer);

/**
 * Open a new stream on the specified lump for reading.
 *
 * @param file  File system handle used to reference the file once read.
 * @param hndl  Handle to the file containing the data to be read.
 *
 * @return  Same as @a file for convenience.
 */
abstractfile_t* F_OpenStreamFile(abstractfile_t* file, FILE* hndl, const char* path);

#endif /* LIBDENG_FILESYS_ABSTRACTFILE_H */
