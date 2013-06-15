/**\file p_mapsetup.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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
 * Doomsday calls this once a map change has completed to allow us (the game)
 * to do any finalization we need (such as spawning thinkers or cataloguing
 * secret areas).
 */
void P_FinalizeMapChange(Uri const *uri);

/**
 * Load the specified map.
 *
 * @param uri  URI e.g., "E1M1".
 */
void P_SetupMap(Uri *uri);

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
