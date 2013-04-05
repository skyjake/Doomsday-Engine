/** @file dam_main.h Doomsday Archived Map (DAM), map management.
 *
 * @author Copyright &copy; 2007-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_ARCHIVED_MAP_MAIN_H
#define LIBDENG_ARCHIVED_MAP_MAIN_H

#include "dd_share.h"
#include "uri.hh"

class GameMap;

DENG_EXTERN_C byte mapCache;

/// To be called during init to register the cvars and ccmds for this module.
void DAM_Register();

/// Initialize this module.
void DAM_Init();

/// Shutdown this module.
void DAM_Shutdown();

#if 0
/**
 * Compose the relative path (relative to the runtime directory) to the directory
 * within the archived map cache where maps from the specified source will reside.
 *
 * @param sourcePath  Path to the primary resource file for the original map data.
 * @return  The composed path.
 */
AutoStr* DAM_ComposeCacheDir(char const *sourcePath);
#endif

/**
 * Attempt to load the map associated with the specified identifier.
 */
GameMap *DAM_LoadMap(de::Uri const &uri);

#endif // LIBDENG_ARCHIVED_MAP_MAIN_H
