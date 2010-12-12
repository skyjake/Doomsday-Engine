/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009-2010 Daniel Swanson <danij@dengine.net>
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
 * sys_file.h: File Stream Abstraction Layer
 *
 * Data can be read from memory, virtual files or actual files.
 */

#ifndef LIBDENG_FILE_IO_H
#define LIBDENG_FILE_IO_H

#include <stdio.h>

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

typedef enum filetype_e {
    FT_NONE = -1,
    FT_NORMAL,
    FT_DIRECTORY
} filetype_t;

#define VALID_FILE_TYPE(t)          ((t) == FT_NORMAL || (t) == FT_DIRECTORY)

typedef int     (*f_forall_func_t) (const char* fn, filetype_t type, void* parm);

void F_ResetFileIDs(void);
boolean F_CheckFileID(const char* path);

void F_InitMapping(void);
void F_AddMapping(const char* source, const char* destination);
void F_InitDirec(void);
void F_ShutdownDirec(void);
int F_Access(const char* path);
DFILE* F_Open(const char* path, const char* mode);
void F_Close(DFILE* file);
size_t F_Length(DFILE* file);
size_t F_Read(void* dest, size_t count, DFILE* file);
unsigned char F_GetC(DFILE* file);
size_t F_Tell(DFILE* file);
size_t F_Seek(DFILE* file, size_t offset, int whence);
void F_Rewind(DFILE* file);
int F_ForAll(const char* filespec, void* parm, f_forall_func_t func);
unsigned int F_LastModified(const char* fileName);

#endif
