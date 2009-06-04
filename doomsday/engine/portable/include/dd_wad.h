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

typedef struct {
    char            name[9]; // End in \0.
    DFILE*          handle;
    int             position;
    size_t          size;
    int             sent;
    char            group; // Lump grouping tag (LGT_*).
} lumpinfo_t;

extern char* defaultWads; // A list of wad names, whitespace in between (in .cfg).
extern lumpinfo_t *lumpInfo;
extern int numLumps;

void            DD_RegisterVFS(void);

void            W_InitMultipleFiles(char** fileNames);
void            W_EndStartup(void);
lumpnum_t       W_CheckNumForName(const char* name);
lumpnum_t       W_GetNumForName(const char* name);
size_t          W_LumpLength(lumpnum_t lump);
const char*     W_LumpName(lumpnum_t lump);
void            W_ReadLump(lumpnum_t lump, void* dest);
void            W_ReadLumpSection(lumpnum_t lump, void* dest,
                                  size_t startOffset, size_t length);
void*           W_CacheLumpNum(lumpnum_t lump, int tag);
void*           W_CacheLumpName(const char* name, int tag);
boolean         W_AddFile(const char* fileName,
                          boolean allowDuplicate);
boolean         W_RemoveFile(const char* fileName);
void            W_Reset(void);
lumpnum_t       W_OpenAuxiliary(const char* fileName);
void            W_ChangeCacheTag(lumpnum_t lump, int tag);
void            W_CheckIWAD(void);
boolean         W_IsFromIWAD(lumpnum_t lump);
const char*     W_LumpSourceFile(lumpnum_t lump);
unsigned int    W_CRCNumberForRecord(int idx);
unsigned int    W_CRCNumber(void);
void            W_GetIWADFileName(char* buf, size_t bufSize);
void            W_GetPWADFileNames(char* buf, size_t bufSize,
                                   char separator);

#endif
