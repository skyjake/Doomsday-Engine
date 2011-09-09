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
 * File input. Can read from real files or WAD lumps. Note that reading
 * from WAD lumps means that a copy is taken of the lump when the
 * corresponding 'file' is opened. With big files this uses considerable
 * memory and time.
 */

#ifndef LIBDENG_FILESYS_FILE_IO_H
#define LIBDENG_FILESYS_FILE_IO_H

#include <stdio.h>

/**
 * DFILE is a subcomponent of abstractfile_t. Objects of this type should
 * never be instantiated, rather, they are used as an implementation
 * mechanism to separate the lower level file stream abstraction from the
 * higher level abstractfile_t derived objects.
 */
typedef struct {
    boolean eof;
    size_t size;
    FILE* hndl;
    uint8_t* data;
    uint8_t* pos;
} DFILE;

/// @return  The length of the file, in bytes.
size_t F_Length(DFILE* file);

/**
 * @return  Number of bytes read (at most @a count bytes will be read).
 */
size_t F_Read(DFILE* file, uint8_t* buffer, size_t count);

/**
 * Read a character from the stream, advancing the read position in the process.
 */
unsigned char F_GetC(DFILE* file);

/**
 * @return  @c true iff the stream has reached the end of the file.
 */
boolean F_AtEnd(DFILE* file);

/**
 * @return  Current position in the stream as an offset from the beginning of the file.
 */
size_t F_Tell(DFILE* file);

/**
 * @return  The current position in the file, before the move, as an
 *      offset from the beginning of the file.
 */
size_t F_Seek(DFILE* file, size_t offset, int whence);

/**
 * Rewind the stream to the start of the file.
 */
void F_Rewind(DFILE* file);

#endif /* LIBDENG_FILESYS_FILE_IO_H */
