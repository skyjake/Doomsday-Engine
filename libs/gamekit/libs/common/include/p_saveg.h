/** @file p_saveg.h  Common game-save state management.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#ifdef __cplusplus

#include "common.h"

class MapStateWriter;
class MapStateReader;

DE_EXTERN_C int saveToRealPlayerNum[MAXPLAYERS];

#if __JHEXEN__
typedef struct targetplraddress_s {
    void **address;
    struct targetplraddress_s *next;
} targetplraddress_t;

DE_EXTERN_C targetplraddress_t *targetPlayerAddrs;

void SV_InitTargetPlayers();
void SV_ClearTargetPlayers();
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
    void write(Writer1 *writer);
    void read(Reader1 *reader, int saveVersion);
#endif
} playerheader_t;

void SV_WriteLine(Line *line, MapStateWriter *msw);
void SV_ReadLine(Line *line, MapStateReader *msr);

void SV_WriteSector(Sector *sec, MapStateWriter *msw);
void SV_ReadSector(Sector *sec, MapStateReader *msr);

#if !__JHEXEN__
/**
 * Saves a snapshot of the world, a still image.
 * No data of movement is included (server sends it).
 */
void SV_SaveGameClient(uint gameId);

void SV_LoadGameClient(uint gameId);
#endif

#endif // __cplusplus
#endif // LIBCOMMON_SAVESTATE_H
