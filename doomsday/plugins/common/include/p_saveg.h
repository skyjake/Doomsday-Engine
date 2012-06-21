/**\file p_saveg.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
 * Common save game handling.
 */

#ifndef LIBCOMMON_SAVESTATE_H
#define LIBCOMMON_SAVESTATE_H

#include "p_saveio.h"

/// Register the console commands and variables of this module.
void SV_Register(void);

/// Initialize this module.
void SV_Init(void);

/// Shutdown this module.
void SV_Shutdown(void);

#ifdef __JHEXEN__
void SV_SetSaveVersion(int version);
int SV_SaveVersion(void);
#else
saveheader_t* SV_SaveHeader(void);
#endif

boolean SV_SaveGame(int slot, const char* name);

boolean SV_LoadGame(int slot);

#if __JHEXEN__
void SV_HxInitBaseSlot(void);
void SV_HxMapTeleport(uint map, uint position);
#else
/**
 * Saves a snapshot of the world, a still image.
 * No data of movement is included (server sends it).
 */
void SV_SaveClient(uint gameId);

void SV_LoadClient(uint gameId);
#endif

typedef enum gamearchivesegment_e {
    ASEG_GAME_HEADER = 101, //jhexen only
    ASEG_MAP_HEADER, //jhexen only
    ASEG_WORLD,
    ASEG_POLYOBJS, //jhexen only
    ASEG_MOBJS, //jhexen < ver 4 only
    ASEG_THINKERS,
    ASEG_SCRIPTS, //jhexen only
    ASEG_PLAYERS,
    ASEG_SOUNDS, //jhexen only
    ASEG_MISC, //jhexen only
    ASEG_END,
    ASEG_MATERIAL_ARCHIVE,
    ASEG_MAP_HEADER2, //jhexen only
    ASEG_PLAYER_HEADER,
    ASEG_GLOBALSCRIPTDATA //jhexen only
} gamearchivesegment_t;

/**
 * Original indices must remain unchanged!
 * Added new think classes to the end.
 */
typedef enum thinkclass_e {
    TC_NULL = -1,
    TC_END,
    TC_MOBJ,
    TC_XGMOVER,
    TC_CEILING,
    TC_DOOR,
    TC_FLOOR,
    TC_PLAT,
#if __JHEXEN__
    TC_INTERPRET_ACS,
    TC_FLOOR_WAGGLE,
    TC_LIGHT,
    TC_PHASE,
    TC_BUILD_PILLAR,
    TC_ROTATE_POLY,
    TC_MOVE_POLY,
    TC_POLY_DOOR,
#else
    TC_FLASH,
    TC_STROBE,
# if __JDOOM__ || __JDOOM64__
    TC_GLOW,
    TC_FLICKER,
#  if __JDOOM64__
    TC_BLINK,
#  endif
# else
    TC_GLOW,
# endif
#endif
    TC_MATERIALCHANGER,
    NUMTHINKERCLASSES
} thinkerclass_t;

#if __JHEXEN__
int             SV_ThingArchiveNum(mobj_t* mo);
#else
unsigned short  SV_ThingArchiveNum(mobj_t* mo);
#endif
mobj_t*         SV_GetArchiveThing(int thingid, void* address);

MaterialArchive* SV_MaterialArchive(void);
material_t*     SV_GetArchiveMaterial(materialarchive_serialid_t serialId, int group);

/**
 * Exit with a fatal error if the value at the current location in the
 * game-save file does not match that associated with the segment type.
 *
 * @param segType  Segment type identifier to check alignment of.
 */
void SV_AssertSegment(int segType);

void SV_BeginSegment(int segType);

/**
 * Update mobj flag values from those used in legacy game-save formats
 * to their current values.
 *
 * To be called after loading a legacy game-save for each mobj loaded.
 *
 * @param mo  Mobj whoose flags are to be updated.
 * @param ver  The MOBJ save version to update from.
 */
void SV_UpdateReadMobjFlags(mobj_t* mo, int ver);

#endif /* LIBCOMMON_SAVESTATE_H */
