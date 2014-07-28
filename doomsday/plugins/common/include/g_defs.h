/** @file g_defs.h Game definition lookup utilities.
 *
 * @authors Copyright © 1999-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCOMMON_DEFINITION_UTILS_H
#define LIBCOMMON_DEFINITION_UTILS_H

#ifdef __cplusplus
#include <doomsday/defs/ded.h>
#include <doomsday/uri.h>

/**
 * Provides access to the engine's definition database (DED).
 */
ded_t &Defs();

/**
 * Translates a warp map number to unique map identifier. Always returns a valid
 * map identifier.
 *
 * @note This should only be used where necessary for compatibility reasons as
 * the "warp translation" mechanic is redundant in the context of Doomsday's
 * altogether better handling of map resources and their references. Instead,
 * use the map URI mechanism.
 *
 * @param map  The warp map number to translate.
 *
 * @return The unique identifier of the map given a warp map number. If the map
 * is not found a URI to the first available map is returned (i.e., Maps:MAP01)
 */
de::Uri P_TranslateMap(uint map);

extern "C" {
#endif

/**
 * @return  The default for a value (retrieved from Doomsday).
 */
int GetDefInt(char const *def, int *returnVal);

void GetDefState(char const *def, int *returnVal);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBCOMMON_DEFINITION_UTILS_H
