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

#include "dd_types.h"

#define deof(file) ((file)->flags.eof != 0)

struct abstractfile_s;

typedef struct {
    struct DFILE_flags_s {
        unsigned char open:1;
        unsigned char file:1;
        unsigned char eof:1;
    } flags;
    size_t size;
    void* data;
    char* pos;
    unsigned int lastModified;
} DFILE;

void F_CloseAll(void);

/**
 * Reset known fileId records so that the next time F_CheckFileId() is
 * called on a file, it will pass.
 */
void F_ResetFileIds(void);

/**
 * Calculate an identifier for the file based on its full path name.
 * The identifier is the MD5 hash of the path.
 */
void F_GenerateFileId(const char* str, byte identifier[16]);

/**
 * Maintains a list of identifiers already seen.
 *
 * @return @c true if the given file can be read, or
 *         @c false, if it has already been read.
 */
boolean F_CheckFileId(const char* path);

/// @return  @c true if the FileId associated with @a path was released.
boolean F_ReleaseFileId(const char* path);

/**
 * @return  @c true if the file can be opened for reading.
 */
int F_Access(const char* path);

/**
 * Frees the memory allocated to the handle.
 */
void F_Release(DFILE* file);

void F_Close(DFILE* file);

/**
 * \note Stream position is not affected.
 * @return  The length of the file, in bytes.
 */
size_t F_Length(DFILE* file);

/**
 * @return  "Last modified" timestamp of the file.
 */
unsigned int F_LastModified(DFILE* file);

/**
 * Try to locate the specified lump for reading.
 *
 * @param lump  Index of the lump to open.
 * @param dontBuffer  Just test for access (don't buffer anything).
 *
 * @return  Non-zero if a lump was found and opened successfully.
 */
DFILE* F_OpenLump(lumpnum_t lumpNum, boolean dontBuffer);

/**
 * Open a new stream on the specified lump for reading.
 *
 * @param fsObject  File system record for the file containing the lump to be read.
 * @param lump  Index of the lump to open.
 * @param dontBuffer  Just test for access (don't buffer anything).
 *
 * @return  Non-zero if a lump was found and opened successfully.
 */
DFILE* F_OpenStreamLump(struct abstractfile_s* fsObject, int lumpIdx, boolean dontBuffer);

DFILE* F_OpenStreamFile(FILE* hndl, const char* path);

/**
 * @return  Number of bytes read (at most @a count bytes will be read).
 */
size_t F_Read(DFILE* file, void* dest, size_t count);

unsigned char F_GetC(DFILE* file);
size_t F_Tell(DFILE* file);

/**
 * @return  The current position in the file, before the move, as an
 * offset from the beginning of the file.
 */
size_t F_Seek(DFILE* file, size_t offset, int whence);
void F_Rewind(DFILE* file);

/**
 * This is a case-insensitive test.
 * I do hope this algorithm works like it should...
 *
 * @return  @c true, if the string matches the pattern.
 */
int F_MatchFileName(const char* string, const char* pattern);

#endif /* LIBDENG_FILESYS_FILE_IO_H */
