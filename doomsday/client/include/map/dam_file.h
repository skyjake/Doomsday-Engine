/**\file dam_file.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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
 * Doomsday Archived Map (DAM), reader/writer.
 */

#ifndef LIBDENG_ARCHIVED_MAP_FILE_H
#define LIBDENG_ARCHIVED_MAP_FILE_H

#include "de_play.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Check if a map in the archive is up to date. The source data must not be
 * newer than the cached map data.
 */
boolean DAM_MapIsValid(const char* cachedMapPath, lumpnum_t markerLumpNum);

/**
 * Write the current state of the map into a Doomsday Archived Map.
 */
boolean DAM_MapWrite(GameMap* map, const char* path);

/**
 * Load a map from a Doomsday Archived Map.
 */
boolean DAM_MapRead(GameMap* map, const char* path);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_ARCHIVED_MAP_FILE_H */
