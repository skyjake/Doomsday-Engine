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
 * File Stream Abstraction Layer
 *
 * Data can be read from memory, virtual files or actual files.
 */

#ifndef LIBDENG_FILESYS_FILE_IO_H
#define LIBDENG_FILESYS_FILE_IO_H

#include <stdio.h>

#include "dd_string.h"
#include "filedirectory.h"

#define deof(file) ((file)->flags.eof != 0)

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

/// Register the console commands, variables, etc..., of this module.
void F_Register(void);

/**
 * Initialize the file system databases.
 */
void F_Init(void);

/**
 * Shutdown the file system databases.
 */
void F_Shutdown(void);

/**
 * \post No more files will be loaded in startup mode.
 */
void F_EndStartup(void);

/**
 * Files with a .wad extension are archived data files with multiple 'lumps',
 * other files are single lumps whose base filename will become the lump name.
 *
 * \note Lump names can appear multiple times. The name searcher looks backwards,
 * so a later file can override an earlier one.
 *
 * @return  @c true, if the operation is successful.
 */
boolean F_AddFile(const char* fileName, boolean allowDuplicate);
boolean F_AddFiles(const char* const* filenames, size_t num, boolean allowDuplicate);

boolean F_RemoveFile(const char* fileName);
boolean F_RemoveFiles(const char* const* filenames, size_t num);

/**
 * Remove all file records flagged Runtime.
 * @return  Number of records removed.
 */
int F_Reset(void);

int F_Access(const char* path);

/**
 * Opens the given file (will be translated) for reading.
 * "t" = text mode (with real files, lumps are always binary)
 * "b" = binary
 * "f" = must be a real file in the local file system.
 * "x" = just test for access (don't buffer anything)
 */
DFILE* F_Open(const char* path, const char* mode);

void F_Close(DFILE* file);

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
 * \note Stream position is not affected.
 * @return  The length of the file, in bytes.
 */
size_t F_Length(DFILE* file);

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
 * @return  The time when the file was last modified, as seconds since
 * the Epoch else zero if the file is not found.
 */
unsigned int F_LastModified(const char* fileName);

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
 * This is a case-insensitive test.
 * I do hope this algorithm works like it should...
 *
 * @return  @c true, if the string matches the pattern.
 */
int F_MatchName(const char* string, const char* pattern);

void F_InitializeResourcePathMap(void);

/**
 * The path names are converted to full paths before adding to the table.
 * Files in the source directory are mapped to the target directory.
 */
void F_AddResourcePathMapping(const char* source, const char* destination);

/**
 * Initialize the lump directory > vfs translations.
 * \note Should be called after WADs have been processed.
 */
void F_InitDirec(void);

void F_ShutdownDirec(void);

/**
 * Parm is passed on to the callback, which is called for each file
 * matching the filespec. Absolute path names are given to the callback.
 * Zip directory, DD_DIREC and the real files are scanned.
 */
int F_AllResourcePaths2(const char* searchPath,
    int (*callback) (const ddstring_t* path, pathdirectory_nodetype_t type, void* paramaters),
    void* paramaters);
int F_AllResourcePaths(const char* searchPath,
    int (*callback) (const ddstring_t* path, pathdirectory_nodetype_t type, void* paramaters));

#endif /* LIBDENG_FILESYS_FILE_IO_H */
