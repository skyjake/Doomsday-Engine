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

/**
 * Force an update of the cached game-save info. To be called (sparingly)
 * at strategic points when an update is necessary (e.g., the game-save
 * paths have changed).
 *
 * \note It is not necessary to call this after a game-save is made,
 * this module will do so automatically.
 */
void SV_UpdateAllSaveInfo(void);

/**
 * Lookup a save slot by searching for a match on game-save name.
 * Search is in ascending logical slot order 0...N (where N is the number
 * of available save slots in the current game).
 *
 * @param name  Name of the game-save to look for. Case insensitive.
 * @return  Logical slot number of the found game-save else @c -1
 */
int SV_SlotForSaveName(const char* name);

/**
 * Parse the given string and determine whether it references a logical
 * game-save slot.
 *
 * @param str  String to be parsed. Parse is divided into three passes.
 *             Pass 1: Check for a known game-save name which matches this.
 *                 Search is in ascending logical slot order 0..N (where N
 *                 is the number of available save slots).
 *             Pass 2: Check for keyword identifiers.
 *                 <last>  = The last used slot.
 *                 <quick> = The currently nominated "quick save" slot.
 *                 <auto>  = The "auto save" slot.
 *             Pass 3: Check for a logical save slot number.
 *
 * @return  Save slot identifier of the slot else @c -1
 */
int SV_ParseSlotIdentifier(const char* str);

/// @return  @c true iff @a slot is a valid logical save slot.
boolean SV_IsValidSlot(int slot);

/// @return  @c true iff @a slot is user-writable save slot (not "auto" or similar).
boolean SV_IsUserWritableSlot(int slot);

/// @return  @c true iff a game-save is present for logical save @a slot.
boolean SV_IsSlotUsed(int slot);

#if __JHEXEN__
/**
 * @return  @c true iff a game-save is present and serialized @a map state is
 *      is present for logical save @a slot.
 */
boolean SV_HxHaveMapSaveForSlot(int slot, uint map);
#endif

/**
 * @return  Game-save info for logical save @a slot. Always returns valid
 *      info even if supplied with an invalid or unused slot identifer.
 */
saveinfo_t* SV_SaveInfoForSlot(int slot);

boolean SV_ComposeSavePathForSlot(int slot, ddstring_t* path);

#if __JHEXEN__
boolean SV_ComposeSavePathForMapSlot(uint map, int slot, ddstring_t* path);
#endif

#if !__JHEXEN__
/**
 * Compose the (possibly relative) path to the game-save associated
 * with @a gameId. If the game-save path is unreachable then @a path
 * will be made empty.
 *
 * @param gameId  Unique game identifier.
 * @param path  String buffer to populate with the game save path.
 * @return  @c true if @a path was set.
 */
boolean SV_ComposeSavePathForClientGameId(uint gameId, ddstring_t* path);
#endif

/**
 * Deletes all save game files associated with a slot number.
 */
void SV_ClearSlot(int slot);

/**
 * Copies all the save game files from one slot to another.
 */
void SV_CopySlot(int sourceSlot, int destSlot);

boolean SV_SaveGame(int slot, const char* name);

boolean SV_LoadGame(int slot);

#if __JHEXEN__
void SV_HxInitBaseSlot(void);
void SV_HxMapTeleport(uint map, uint position);
#endif

#if !__JHEXEN__
/**
 * Saves a snapshot of the world, a still image.
 * No data of movement is included (server sends it).
 */
void SV_SaveGameClient(uint gameId);

void SV_LoadGameClient(uint gameId);
#endif

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
int SV_ThingArchiveNum(mobj_t* mo);
#else
unsigned short SV_ThingArchiveNum(mobj_t* mo);
#endif
mobj_t* SV_GetArchiveThing(int thingid, void* address);

MaterialArchive* SV_MaterialArchive(void);
material_t* SV_GetArchiveMaterial(materialarchive_serialid_t serialId, int group);

/**
 * Update mobj flag values from those used in legacy game-save formats
 * to their current values.
 *
 * To be called after loading a legacy game-save for each mobj loaded.
 *
 * @param mo  Mobj whoose flags are to be updated.
 * @param ver  The MOBJ save version to update from.
 */
void SV_TranslateLegacyMobjFlags(mobj_t* mo, int ver);

#endif /* LIBCOMMON_SAVESTATE_H */
