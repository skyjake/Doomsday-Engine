/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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

void            P_InitMapInfo(void);
void            P_InitMapMusicInfo(void);

int             P_GetMapCluster(int map);
int             P_TranslateMap(int map);
int             P_GetMapCDTrack(int map);
int             P_GetMapWarpTrans(int map);
int             P_GetMapNextMap(int map);
materialnum_t   P_GetMapSky1Material(int map);
materialnum_t   P_GetMapSky2Material(int map);
char           *P_GetMapName(int map);
float           P_GetMapSky1ScrollDelta(int map);
float           P_GetMapSky2ScrollDelta(int map);
boolean         P_GetMapDoubleSky(int map);
boolean         P_GetMapLightning(int map);
int             P_GetMapFadeTable(int map);
char           *P_GetMapSongLump(int map);
void            P_PutMapSongLump(int map, char *lumpName);
int             P_GetCDStartTrack(void);
int             P_GetCDEnd1Track(void);
int             P_GetCDEnd2Track(void);
int             P_GetCDEnd3Track(void);
int             P_GetCDIntermissionTrack(void);
int             P_GetCDTitleTrack(void);

#endif
