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
    dd_bool      usingDefaults; ///< @c true= this definition was @em not read from MAPINFO.

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
 * Initializes the MapInfo database.
 * All MAPINFO lumps are then parsed and stored into the database.
 */
void P_InitMapInfo(void);

/**
 * Special early initializer needed to start sound before R_InitRefresh()
 */
void P_InitMapMusicInfo(void);

/**
 * Returns MAPINFO data for the specified @a map, or the default if not valid.
 */
mapinfo_t *P_MapInfo(uint map);

/**
 * Returns the total number of MapInfo definitions.
 */
uint P_MapInfoCount();

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

/**
 * Retrieve the song lump name for the given map.
 *
 * @param map  The map (logical number) to be queried.
 *
 * @return  @c NULL, if the map is set to use the default song lump, else a ptr
 * to a string containing the name of the song lump.
 */
char *P_GetMapSongLump(uint map);

/**
 * Retrieve the CD start track number.
 *
 * @return  The CD start track number
 */
int P_GetCDStartTrack(void);

/**
 * Retrieve the CD end1 track number.
 *
 * @return  The CD end1 track number.
 */
int P_GetCDEnd1Track(void);

/**
 * Retrieve the CD end2 track number.
 *
 * @return  The CD end2 track number.
 */
int P_GetCDEnd2Track(void);

/**
 * Retrieve the CD end3 track number.
 *
 * @return  The CD end3 track number.
 */
int P_GetCDEnd3Track(void);

/**
 * Retrieve the CD intermission track number.
 *
 * @return  The CD intermission track number.
 */
int P_GetCDIntermissionTrack(void);

/**
 * Retrieve the CD title track number.
 *
 * @return  The CD title track number.
 */
int P_GetCDTitleTrack(void);

#if __cplusplus
} // extern "C"
#endif

#endif // JHEXEN_DEF_MAPINFO_H
