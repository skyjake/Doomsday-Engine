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
#include "saveslots.h"

DENG_EXTERN_C int thingArchiveVersion;
DENG_EXTERN_C uint thingArchiveSize;
DENG_EXTERN_C SaveSlots saveSlots;

#ifdef __cplusplus
extern "C" {
#endif

/// Initialize this module.
void SV_Initialize(void);

/// Shutdown this module.
void SV_Shutdown(void);

dd_bool SV_RecogniseGameState(Str const *path, SaveInfo *info);

#if __JHEXEN__
/**
 * Returns @c true iff a game-save is present and serialized @a map state is
 * present for logical save @a slot.
 */
dd_bool SV_HxHaveMapStateForSlot(int slot, uint map);
#endif

/**
 * Save the current game state to the specified @a slot number.
 *
 * @param name  Textual description to include in the save info. Can be @c 0
 *              in which case a description will be auto-generated.
 *
 * @return  @c true iff the game state was saved successfully.
 */
dd_bool SV_SaveGame(int slot, char const *name);

/**
 * Load the game state associated with the specified @a slot number.
 *
 * @return  @c true iff the game state was loaded successfully.
 */
dd_bool SV_LoadGame(int slot);

#if !__JHEXEN__
/**
 * Saves a snapshot of the world, a still image.
 * No data of movement is included (server sends it).
 */
void SV_SaveGameClient(uint gameId);

void SV_LoadGameClient(uint gameId);
#endif

uint SV_GenerateGameId(void);

/// Unique identifier associated with each archived thing.
#if __JHEXEN__
typedef int ThingSerialId;
#else
typedef ushort ThingSerialId;
#endif

/**
 * To be called when writing a game state to acquire a unique identifier for
 * the specified @a mobj from the thing archive. If the given mobj is already
 * present in the archived, the existing archive Id is returned.
 *
 * @param mobj  Mobj to lookup the archive Id for.
 *
 * @return  Identifier for the specified mobj (may be zero).
 */
ThingSerialId SV_ThingArchiveId(mobj_t const *mobj);

/**
 * To be called after reading a game state has been read to lookup a pointer
 * to the mobj which is associated with the specified @a thingId.
 *
 * @param thingId  Unique identifier for the mobj in the thing archive.
 *
 * @return  Pointer to the associated mobj; otherwise @c 0 (not archived).
 */
mobj_t *SV_GetArchiveThing(ThingSerialId thingid, void *address);

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

#if __JHEXEN__
void SV_HxInitBaseSlot(void);
void SV_HxSaveHubMap(void);
void SV_HxLoadHubMap(void);

typedef struct {
    player_t player;
    uint numInventoryItems[NUM_INVENTORYITEM_TYPES];
    inventoryitemtype_t readyItem;
} playerbackup_t;

void SV_HxBackupPlayersInHub(playerbackup_t playerBackup[MAXPLAYERS]);

/**
 * @param playerBackup  Player state backup.
 * @param entryPoint  Logical identifier for the entry point used to enter the map.
 */
void SV_HxRestorePlayersInHub(playerbackup_t playerBackup[MAXPLAYERS], uint entryPoint);
#endif

void SV_InitThingArchiveForLoad(uint size);
#if __JHEXEN__
void SV_InitTargetPlayers(void);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
class MapStateReader;
class MapStateWriter;

void SV_WriteMobj(thinker_t *th, MapStateWriter *msw);
int SV_ReadMobj(thinker_t *th, MapStateReader *msr);

#if __JHEXEN__
void SV_WriteMovePoly(struct polyevent_s const *movepoly, MapStateWriter *msw);
int SV_ReadMovePoly(struct polyevent_s *movepoly, MapStateReader *msr);
#endif

void SV_WriteLine(Line *line, MapStateWriter *msw);
void SV_ReadLine(Line *line, MapStateReader *msr);

void SV_WriteSector(Sector *sec, MapStateWriter *msw);
void SV_ReadSector(Sector *sec, MapStateReader *msr);

#if __JHEXEN__
void SV_WritePolyObj(polyobj_s *po, MapStateWriter *msw);
int SV_ReadPolyObj(MapStateReader *msr);
#endif
#endif // __cplusplus

#endif // LIBCOMMON_SAVESTATE_H
