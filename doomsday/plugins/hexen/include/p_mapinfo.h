/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
 * p_mapinfo.h: MAPINFO definitions.
 */

#ifndef __P_MAPINFO_H__
#define __P_MAPINFO_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#if __cplusplus
extern "C" {
#endif

void            P_InitMapInfo(void);
void            P_InitMapMusicInfo(void);

int             P_GetMapCluster(uint map);

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

int             P_GetMapCDTrack(uint map);
uint            P_GetMapWarpTrans(uint map);
uint            P_GetMapNextMap(uint map);
materialid_t    P_GetMapSky1Material(uint map);
materialid_t    P_GetMapSky2Material(uint map);
char*           P_GetMapName(uint map);
float           P_GetMapSky1ScrollDelta(uint map);
float           P_GetMapSky2ScrollDelta(uint map);
boolean         P_GetMapDoubleSky(uint map);
boolean         P_GetMapLightning(uint map);
int             P_GetMapFadeTable(uint map);
char*           P_GetMapSongLump(uint map);
void            P_PutMapSongLump(uint map, char* lumpName);
int             P_GetCDStartTrack(void);
int             P_GetCDEnd1Track(void);
int             P_GetCDEnd2Track(void);
int             P_GetCDEnd3Track(void);
int             P_GetCDIntermissionTrack(void);
int             P_GetCDTitleTrack(void);

#if __cplusplus
} // extern "C"
#endif

#endif
