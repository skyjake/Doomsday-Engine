/**\file
 *\section Copyright and License Summary
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
 *
 * This program is covered by the HERETIC / HEXEN (LIMITED USE) source
 * code license; you can redistribute it and/or modify it under the terms
 * of the HERETIC / HEXEN source code license as published by Activision.
 *
 * THIS MATERIAL IS NOT MADE OR SUPPORTED BY ACTIVISION.
 *
 * WARRANTY INFORMATION.
 * This program is provided as is. Activision and it's affiliates make no
 * warranties of any kind, whether oral or written , express or implied,
 * including any warranty of merchantability, fitness for a particular
 * purpose or non-infringement, and no other representations or claims of
 * any kind shall be binding on or obligate Activision or it's affiliates.
 *
 * LICENSE CONDITIONS.
 * You shall not:
 *
 * 1) Exploit this Program or any of its parts commercially.
 * 2) Use this Program, or permit use of this Program, on more than one
 *    computer, computer terminal, or workstation at the same time.
 * 3) Make copies of this Program or any part thereof, or make copies of
 *    the materials accompanying this Program.
 * 4) Use the program, or permit use of this Program, in a network,
 *    multi-user arrangement or remote access arrangement, including any
 *    online use, except as otherwise explicitly provided by this Program.
 * 5) Sell, rent, lease or license any copies of this Program, without
 *    the express prior written consent of Activision.
 * 6) Remove, disable or circumvent any proprietary notices or labels
 *    contained on or within the Program.
 *
 * You should have received a copy of the HERETIC / HEXEN source code
 * license along with this program (Ravenlic.txt); if not:
 * http://www.ravensoft.com/
 */

/*
 * Mapinfo support.
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
int             P_GetMapSky1Texture(int map);
int             P_GetMapSky2Texture(int map);
char           *P_GetMapName(int map);
fixed_t         P_GetMapSky1ScrollDelta(int map);
fixed_t         P_GetMapSky2ScrollDelta(int map);
boolean         P_GetMapDoubleSky(int map);
boolean         P_GetMapLightning(int map);
boolean         P_GetMapFadeTable(int map);
char           *P_GetMapSongLump(int map);
void            P_PutMapSongLump(int map, char *lumpName);
int             P_GetCDStartTrack(void);
int             P_GetCDEnd1Track(void);
int             P_GetCDEnd2Track(void);
int             P_GetCDEnd3Track(void);
int             P_GetCDIntermissionTrack(void);
int             P_GetCDTitleTrack(void);

#endif
