/**
 * @file p_saveio.h
 * Game save file IO.
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
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

typedef enum gamearchivesegment_e {
    ASEG_MAP_HEADER = 102, // Hexen only
    ASEG_WORLD,
    ASEG_POLYOBJS, // Hexen only
    ASEG_MOBJS, // Hexen < ver 4 only
    ASEG_THINKERS,
    ASEG_SCRIPTS, // Hexen only
    ASEG_PLAYERS,
    ASEG_SOUNDS, // Hexen only
    ASEG_MISC, // Hexen only
    ASEG_END,
    ASEG_MATERIAL_ARCHIVE,
    ASEG_MAP_HEADER2, // Hexen only
    ASEG_PLAYER_HEADER,
    ASEG_GLOBALSCRIPTDATA // Hexen only
} gamearchivesegment_t;

enum {
    SV_OK = 0,
    SV_INVALIDFILENAME
};

void SV_InitIO(void);
void SV_ShutdownIO(void);

void SV_ConfigureSavePaths(void);
const char* SV_SavePath(void);
#if !__JHEXEN__
const char* SV_ClientSavePath(void);
#endif

/*
 * File management
 */
LZFILE* SV_OpenFile(const char* filePath, const char* mode);
void SV_CloseFile(void);
LZFILE* SV_File(void);
boolean SV_ExistingFile(const char* filePath);
int SV_RemoveFile(const Str* filePath);
void SV_CopyFile(const Str* srcPath, const Str* destPath);

#if __JHEXEN__
saveptr_t* SV_HxSavePtr(void);
#endif // __JHEXEN__

/**
 * Exit with a fatal error if the value at the current location in the
 * game-save file does not match that associated with the segment type.
 *
 * @param segType  Segment type identifier to check alignment of.
 */
void SV_AssertSegment(int segType);

void SV_BeginSegment(int segType);

/**
 * Seek forward @a offset bytes in the save file.
 */
void SV_Seek(uint offset);

/*
 * Writing and reading values
 */
void SV_Write(const void* data, int len);
void SV_WriteByte(byte val);
#if __JHEXEN__
void SV_WriteShort(unsigned short val);
#else
void SV_WriteShort(short val);
#endif
#if __JHEXEN__
void SV_WriteLong(unsigned int val);
#else
void SV_WriteLong(long val);
#endif
void SV_WriteFloat(float val);

void SV_Read(void* data, int len);
byte SV_ReadByte(void);
short SV_ReadShort(void);
long SV_ReadLong(void);
float SV_ReadFloat(void);

Writer* SV_NewWriter(void);
Reader* SV_NewReader(void);

#endif /* LIBCOMMON_SAVESTATE_INPUT_OUTPUT_H */
