/** @file mapinfo.h  Hexen-format MAPINFO definition parsing.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_MAPINFO_H
#define LIBCOMMON_MAPINFO_H
#ifdef __cplusplus

#include "common.h"

class MapInfo
{
public:
    uint map; ///< Logical map number.
    int hub;
    uint warpTrans;
    uint nextMap;
    int cdTrack;
    de::String title;
    de::Uri sky1Material;
    de::Uri sky2Material;
    float sky1ScrollDelta;
    float sky2ScrollDelta;
    bool doubleSky;
    bool lightning;
    de::String fadeTable;
    de::String songLump;

public:
    MapInfo();

    void resetToDefaults();
};

/**
 * Populate the MapInfo database by parsing the MAPINFO lump.
 */
void MapInfoParser(Str const *path);

/**
 * @param mapUri  Identifier of the map to lookup info for. Can be @c 0 in which
 *                case the info for the @em current map will be returned (if set).
 *
 * @return  MAPINFO data for the specified @a mapUri; otherwise @c 0 (not found).
 */
MapInfo *P_MapInfo(de::Uri const *mapUri = 0);

#define P_INVALID_LOGICAL_MAP 0xffffffff

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

#endif // __cplusplus
#endif // LIBCOMMON_MAPINFO_H
