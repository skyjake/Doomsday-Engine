/**\file p_saveio.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_SAVESTATE_INPUT_OUTPUT_H
#define LIBCOMMON_SAVESTATE_INPUT_OUTPUT_H

#include "materialarchive.h"
#include "lzss.h"
#include "p_savedef.h"

typedef struct saveheader_s {
    int             magic;
    int             version;
    gamemode_t      gameMode;
    char            name[SAVESTRINGSIZE];
    byte            skill;
    byte            episode;
    byte            map;
    byte            deathmatch;
    byte            noMonsters;
#if __JHEXEN__
    byte            randomClasses;
#else
    byte            respawnMonsters;
    int             mapTime;
    byte            players[MAXPLAYERS];
#endif
    unsigned int    gameId;
} saveheader_t;

typedef struct gamesaveinfo_s {
    ddstring_t filePath;
    ddstring_t name;
} gamesaveinfo_t;

typedef struct savegameparam_s {
    const ddstring_t* path;
    const char* name;
    int slot;
} savegameparam_t;

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
LZFILE* SV_OpenFile(const char* fileName, const char* mode);
void SV_CloseFile(void);
LZFILE* SV_File(void);

/**
 * Force an update of the cached game-save info. To be called (sparingly)
 * at strategic points when an update is necessary (e.g., the game-save
 * paths have changed).
 *
 * \note It is not necessary to call this after a game-save is made,
 * this module will do so automatically.
 */
void SV_UpdateGameSaveInfo(void);

/**
 * Lookup a save slot by searching for a match on game-save name.
 * Search is in ascending logical slot order 0...N (where N is the number
 * of available save slots in the current game).
 *
 * @param name  Name of the game-save to look for. Case insensitive.
 * @return  Logical slot number of the found game-save else @c -1
 */
int SV_FindGameSaveSlotForName(const char* name);

/**
 * Parse the given string and determine whether it references a logical
 * game-save slot.
 *
 * @param str  String to be parsed. Parse is divided into three passes.
 *             Pass 1: Check for a known game-save name which matches this.
 *                 Search is in ascending logical slot order 0..N (where N is the
 *                 number of available save slots in the current game).
 *             Pass 2: Check for keyword identifiers.
 *                 <last>  = The last used slot.
 *                 <quick> = The currently nominated "quick save" slot.
 *                 <auto>  = The autosave slot.
 *             Pass 3: Check for a logical save slot number.
 *
 * @return  Save slot identifier of the slot else @c -1
 */
int SV_ParseGameSaveSlot(const char* str);

/// @return  @c true iff @a slot is a valid logical save slot.
boolean SV_IsValidSlot(int slot);

/// @return  @c true iff @a slot is user-writable save slot (not "auto" or similar).
boolean SV_IsUserWritableSlot(int slot);

/// @return  @c true iff a game-save is present for logical save @a slot.
boolean SV_IsGameSaveSlotUsed(int slot);

#if __JHEXEN__
/**
 * @return  @c true iff a game-save is present and serialized @a map state is
 *      is present for logical save @a slot.
 */
boolean SV_HxGameSaveSlotHasMapState(int slot, uint map);
#endif

/**
 * @return  Game-save info for logical save @a slot. Always returns valid
 *      info even if supplied with an invalid or unused slot identifer.
 */
const gamesaveinfo_t* SV_GameSaveInfoForSlot(int slot);

boolean SV_GameSavePathForSlot(int slot, ddstring_t* path);
#if __JHEXEN__
boolean SV_GameSavePathForMapSlot(uint map, int slot, ddstring_t* path);
#else
boolean SV_ClientGameSavePathForGameId(uint gameId, ddstring_t* path);
#endif

/**
 * Deletes all save game files associated with a slot number.
 */
void SV_ClearSaveSlot(int slot);

/**
 * Copies all the save game files from one slot to another.
 */
void SV_CopySaveSlot(int sourceSlot, int destSlot);

#if __JHEXEN__
saveptr_t* SV_HxSavePtr(void);
#endif // __JHEXEN__

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

void SV_SaveInfo_Write(saveheader_t* info);
void SV_SaveInfo_Read(saveheader_t* info);

void SV_MaterialArchive_Write(MaterialArchive* arc);
void SV_MaterialArchive_Read(MaterialArchive* arc, int version);

#endif /* LIBCOMMON_SAVESTATE_INPUT_OUTPUT_H */
