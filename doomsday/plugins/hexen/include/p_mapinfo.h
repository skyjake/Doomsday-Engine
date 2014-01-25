/** @file p_mapinfo.h  Hexen MAPINFO parsing.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Activision
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

#ifndef JHEXEN_DEF_MAPINFO_H
#define JHEXEN_DEF_MAPINFO_H

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#include "doomsday.h"

typedef struct mapinfo_s {
    uint         map; ///< Logical map number.
    short        cluster;
    uint         warpTrans;
    uint         nextMap;
    short        cdTrack;
    char         title[32];
    materialid_t sky1Material;
    materialid_t sky2Material;
    float        sky1ScrollDelta;
    float        sky2ScrollDelta;
    dd_bool      doubleSky;
    dd_bool      lightning;
    int          fadeTable;
    char         songLump[10];
} mapinfo_t;

#if __cplusplus
extern "C" {
#endif

/**
 * Populate the MapInfo database by parsing the MAPINFO lump.
 */
void MapInfoParser(Str const *path);

/**
 * Returns MAPINFO data for the specified @a mapUri; otherwise @c 0 (not found).
 */
mapinfo_t *P_MapInfo(Uri const *mapUri);

#define P_INVALID_LOGICAL_MAP   0xffffffff

/**
 * Translates a warp map number to logical map number, if possible.
 *
 * @param map  The warp map number to translate.
 *
 * @return The logical map number given a warp map number. If the map is not
 * found, returns P_INVALID_LOGICAL_MAP.
 */
uint P_TranslateMapIfExists(uint map);

/**
 * Translates a warp map number to logical map number. Always returns a valid
 * logical map.
 *
 * @param map  The warp map number to translate.
 *
 * @return The logical map number given a warp map number. If the map is not
 * found, returns 0 (first available logical map).
 */
uint P_TranslateMap(uint map);

#if __cplusplus
} // extern "C"
#endif

#endif // JHEXEN_DEF_MAPINFO_H
