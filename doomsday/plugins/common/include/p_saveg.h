/** @file p_saveg.h Common game-save state management.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_SAVESTATE_H
#define LIBCOMMON_SAVESTATE_H

#include "common.h"
#include "p_savedef.h" /// @todo remove me
#include "saveinfo.h"

DENG_EXTERN_C int saveToRealPlayerNum[MAXPLAYERS];

#if __JHEXEN__
DENG_EXTERN_C targetplraddress_t *targetPlayerAddrs;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/// Initialize this module.
void SV_Initialize(void);

/// Shutdown this module.
void SV_Shutdown(void);

/**
 * Save the current game state to the specified @a slotNumber.
 *
 * @param description  Textual description to include in the save info. Can be @c 0
 *                     in which case a description will be auto-generated.
 *
 * @return  @c true iff the game state was saved successfully.
 */
dd_bool SV_SaveGame(int slotNumber, char const *description);

/**
 * Load the game state associated with the specified @a slotNumber.
 *
 * @return  @c true iff the game state was loaded successfully.
 */
dd_bool SV_LoadGame(int slotNumber);

#if !__JHEXEN__
/**
 * Saves a snapshot of the world, a still image.
 * No data of movement is included (server sends it).
 */
void SV_SaveGameClient(uint gameId);

void SV_LoadGameClient(uint gameId);
#endif

#if __JHEXEN__
/**
 * Returns @c true iff a game-save is present and serialized @a map state is
 * present for logical save @a slotNumber.
 */
dd_bool SV_HxHaveMapStateForSlot(int slotNumber, uint map);

void SV_HxSaveHubMap(void);

void SV_HxLoadHubMap(void);
#endif

/**
 * Update mobj flag values from those used in legacy game-save formats
 * to their current values.
 *
 * To be called after loading a legacy game-save for each mobj loaded.
 *
 * @param mo  Mobj whoose flags are to be updated.
 * @param ver  The MOBJ save version to update from.
 */
void SV_TranslateLegacyMobjFlags(mobj_t *mo, int ver);

typedef struct playerheader_s {
    int numPowers;
    int numKeys;
    int numFrags;
    int numWeapons;
    int numAmmoTypes;
    int numPSprites;
#if __JDOOM64__ || __JHERETIC__ || __JHEXEN__
    int numInvItemTypes;
#endif
#if __JHEXEN__
    int numArmorTypes;
#endif

#ifdef __cplusplus
    void write(Writer *writer);
    void read(Reader *reader, int saveVersion);
#endif
} playerheader_t;

#if __JHEXEN__
void SV_InitTargetPlayers(void);
void SV_ClearTargetPlayers(void);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
#include "gamestatereader.h"
#include <de/Path>

class SaveSlots;

/**
 * Returns the game's SaveSlots.
 */
SaveSlots &SV_SaveSlots();

/**
 * Declare a new saved game state reader/interpreter.
 *
 * @param recognizer  Format recognizer function.
 * @param maker       Reader instantiator function.
 */
void SV_DeclareGameStateReader(GameStateRecognizeFunc recognizer, GameStateReaderMakeFunc maker);

/**
 * Determines whether the game session associated with save @a info is interpretable as a
 * potentially loadable savegame state.
 *
 * @param info  SaveInfo to attempt to read game session header into.
 */
bool SV_RecognizeGameState(SaveInfo &info);

class MapStateReader;
class MapStateWriter;

void SV_WriteLine(Line *line, MapStateWriter *msw);
void SV_ReadLine(Line *line, MapStateReader *msr);

void SV_WriteSector(Sector *sec, MapStateWriter *msw);
void SV_ReadSector(Sector *sec, MapStateReader *msr);
#endif // __cplusplus

#endif // LIBCOMMON_SAVESTATE_H
