/**\file p_mapsetup.h
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

/**
 * Common map setup routines
 */

#ifndef LIBCOMMON_PLAYSETUP_H
#define LIBCOMMON_PLAYSETUP_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

// If true we are in the process of setting up a map.
extern boolean mapSetup;

/**
 * Doomsday calls this (before any data is read) for each type of map object
 * at the start of the map load process. This is to allow us (the game) to
 * do any initialization we need. For example if we maintain our own data
 * for lines (the xlines) we'll do all allocation and init here.
 *
 * @param type    (DMU object type) The id of the data type being setup.
 * @param num     The number of elements of "type" Doomsday is creating.
 */
void P_SetupForMapData(int type, uint num);

/**
 * Load the specified map.
 *
 * @param uri      URI e.g., "E1M1".
 * @param episode  Logical episode number.
 * @param map      Logical map number.
 */
void P_SetupMap(Uri* uri, uint episode, uint map);

const char* P_GetMapNiceName(void);
patchid_t P_FindMapTitlePatch(uint episode, uint map);
const char* P_GetMapAuthor(boolean supressGameAuthor);

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
void P_FindSecrets(void);
#endif

void P_SpawnSectorMaterialOriginScrollers(void);
void P_SpawnSideMaterialOriginScrollers(void);
void P_SpawnAllMaterialOriginScrollers(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBCOMMON_PLAYSETUP_H */
