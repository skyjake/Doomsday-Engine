/** @file p_saveio.h  Game save file IO.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBCOMMON_SAVESTATE_INPUT_OUTPUT_H
#define LIBCOMMON_SAVESTATE_INPUT_OUTPUT_H

#include "api_materialarchive.h"
#include "p_savedef.h"
#include <de/Path>
#include "lzss.h"

typedef enum savestatesegment_e {
    ASEG_MAP_HEADER = 102,  // Hexen only
    ASEG_MAP_ELEMENTS,
    ASEG_POLYOBJS,          // Hexen only
    ASEG_MOBJS,             // Hexen < ver 4 only
    ASEG_THINKERS,
    ASEG_SCRIPTS,           // Hexen only
    ASEG_PLAYERS,
    ASEG_SOUNDS,            // Hexen only
    ASEG_MISC,              // Hexen only
    ASEG_END,               // = 111
    ASEG_MATERIAL_ARCHIVE,
    ASEG_MAP_HEADER2,
    ASEG_PLAYER_HEADER,
    ASEG_WORLDSCRIPTDATA   // Hexen only
} savestatesegment_t;

#if __JHEXEN__
typedef union saveptr_u {
    byte *b;
    short *w;
    int *l;
    float *f;
} saveptr_t;
#endif

void SV_InitIO();
void SV_ShutdownIO();

/**
 * Create the saved game directories.
 */
void SV_SetupSaveDirectory(de::Path);

de::Path SV_SavePath();

#if !__JHEXEN__
de::Path SV_ClientSavePath();
#endif

/*
 * File management
 */

bool SV_ExistingFile(de::Path filePath);

int SV_RemoveFile(de::Path filePath);

void SV_CopyFile(de::Path srcFilePath, de::Path destFilePath);

bool SV_OpenFile(de::Path filePath, bool write);

void SV_CloseFile();

#if __JHEXEN__
saveptr_t *SV_HxSavePtr();

void SV_HxSetSaveEndPtr(void *endPtr);

size_t SV_HxBytesLeft();

void SV_HxReleaseSaveBuffer();
#endif // __JHEXEN__

/**
 * Exit with a fatal error if the value at the current location in the
 * game-save file does not match that associated with the segment id.
 *
 * @param segmentId  Identifier of the segment to check alignment of.
 */
void SV_AssertSegment(int segmentId);

void SV_BeginSegment(int segmentId);

void SV_EndSegment();

void SV_WriteConsistencyBytes();

void SV_ReadConsistencyBytes();

/**
 * Seek forward @a offset bytes in the save file.
 */
void SV_Seek(uint offset);

Writer *SV_NewWriter();

Reader *SV_NewReader();

#endif // LIBCOMMON_SAVESTATE_INPUT_OUTPUT_H
