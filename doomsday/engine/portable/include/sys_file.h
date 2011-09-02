/**\file sys_file.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009-2011 Daniel Swanson <danij@dengine.net>
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

/**
 * File (Input) Stream Abstraction Layer.
 *
 * File input. Can read from real files or WAD lumps. Note that
 * reading from WAD lumps means that a copy is taken of the lump when
 * the corresponding 'file' is opened. With big files this uses
 * considerable memory and time.
 */

#ifndef LIBDENG_FILESYS_FILE_IO_H
#define LIBDENG_FILESYS_FILE_IO_H

#include <stdio.h>

#include "abstractfile.h"

abstractfile_t* F_NewFile(const char* absolutePath);
void F_Delete(abstractfile_t* file);

/**
 * Close the current file if open, removing all references to it in the open file list.
 */
void F_Close(abstractfile_t* file);

/// @return  The length of the file, in bytes.
size_t F_Length(abstractfile_t* file);

/// @return  "Last modified" timestamp of the file.
unsigned int F_LastModified(abstractfile_t* file);

/**
 * @return  Number of bytes read (at most @a count bytes will be read).
 */
size_t F_Read(abstractfile_t* file, uint8_t* buffer, size_t count);

/**
 * Read a character from the stream, advancing the read position in the process.
 */
unsigned char F_GetC(abstractfile_t* file);

/**
 * @return  @c true iff the stream has reached the end of the file.
 */
boolean F_AtEnd(abstractfile_t* file);

/**
 * @return  Current position in the stream as an offset from the beginning of the file.
 */
size_t F_Tell(abstractfile_t* file);

/**
 * @return  The current position in the file, before the move, as an
 *      offset from the beginning of the file.
 */
size_t F_Seek(abstractfile_t* file, size_t offset, int whence);

/**
 * Rewind the stream to the start of the file.
 */
void F_Rewind(abstractfile_t* file);

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

/**
 * This is a case-insensitive test.
 * I do hope this algorithm works like it should...
 *
 * @return  @c true, if the string matches the pattern.
 */
int F_MatchFileName(const char* string, const char* pattern);

#endif /* LIBDENG_FILESYS_FILE_IO_H */
