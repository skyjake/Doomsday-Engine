/** @file p_mapsetup.h  Common map setup routines.
 *
 * Management of extended map data objects (e.g., xlines) is done here.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_PLAYSIM_MAPSETUP_H
#define LIBCOMMON_PLAYSIM_MAPSETUP_H

#include "common.h"

// If true we are in the process of setting up a map.
DENG_EXTERN_C dd_bool mapSetup;

#ifdef __cplusplus

/**
 * Change the current map to that referenced by @a mapUri.
 */
void P_SetupMap(de::Uri const &mapUri);

extern "C" {
#endif

/**
 * Doomsday calls this once a map change has completed to allow us (the game)
 * to do any finalization we need (such as spawning thinkers or cataloguing
 * secret areas).
 */
void P_FinalizeMapChange(Uri const *uri);

/**
 * To be called to reset the local world state (e.g., when leaving a networked game).
 * Note that @ref P_SetupMap() calls this automatically when the current map changes.
 */
void P_ResetWorldState();

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
void P_FindSecrets(void);
#endif

void P_SpawnSectorMaterialOriginScrollers(void);

void P_SpawnSideMaterialOriginScrollers(void);

void P_SpawnAllMaterialOriginScrollers(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBCOMMON_PLAYSIM_MAPSETUP_H */
