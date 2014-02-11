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

#include "saveinfo.h"
#include "api_materialarchive.h"
#include "lzss.h"
#include "p_savedef.h"

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

enum {
    SV_OK = 0,
    SV_INVALIDFILENAME
};

#ifdef __cplusplus
extern "C" {
#endif

void SV_InitIO(void);
void SV_ShutdownIO(void);

void SV_ConfigureSavePaths(void);
char const *SV_SavePath(void);
#if !__JHEXEN__
char const *SV_ClientSavePath(void);
#endif

/*
 * File management
 */
LZFILE *SV_OpenFile(Str const *filePath, char const *mode);
void SV_CloseFile(void);
LZFILE *SV_File(void);
dd_bool SV_ExistingFile(Str const *filePath);
int SV_RemoveFile(Str const *filePath);
void SV_CopyFile(Str const *srcPath, Str const *destPath);

#if __JHEXEN__
saveptr_t *SV_HxSavePtr(void);
void SV_HxSetSaveEndPtr(void *endPtr);
dd_bool SV_HxBytesLeft(void);
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

void SV_WriteConsistencyBytes(void);
void SV_ReadConsistencyBytes(void);

/**
 * Seek forward @a offset bytes in the save file.
 */
void SV_Seek(uint offset);

Writer *SV_NewWriter(void);
Reader *SV_NewReader(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBCOMMON_SAVESTATE_INPUT_OUTPUT_H
