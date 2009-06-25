/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * dd_wad.h: WAD Files and Data Lump Cache
 */

#ifndef __DOOMSDAY_WAD_H__
#define __DOOMSDAY_WAD_H__

#include "sys_file.h"
#include "con_decl.h"

#define RECORD_FILENAMELEN  FILENAME_T_MAXLEN

// File record flags.
#define FRF_RUNTIME         0x1 // Loaded at runtime (for reset).

#define AUXILIARY_BASE      100000000

// Lump Grouping Tags
typedef enum {
    LGT_NONE = 0,
    LGT_FLATS,
    LGT_SPRITES,
    NUM_LGTAGS
} lumpgrouptag_t;

typedef struct {
    filename_t      fileName; // Full filename (every '\' -> '/').
    int             numLumps; // Number of lumps.
    int             flags;
    DFILE*          handle; // File handle.
    char            iwad;
} filerecord_t;

extern char* defaultWads; // A list of wad names, whitespace in between (in .cfg).
extern int numLumps;

void            DD_RegisterVFS(void);

void            W_InitMultipleFiles(char** fileNames);
void            W_EndStartup(void);
boolean         W_AddFile(const char* fileName,
                          boolean allowDuplicate);
boolean         W_RemoveFile(const char* fileName);
void            W_Reset(void);
void            W_CheckIWAD(void);
unsigned int    W_CRCNumberForRecord(int idx);
void            W_GetIWADFileName(char* buf, size_t bufSize);
void            W_GetPWADFileNames(char* buf, size_t bufSize,
                                   char separator);
boolean         W_DumpLump(lumpnum_t, const char* fileName);
void            W_DumpLumpDir(void);
#endif
