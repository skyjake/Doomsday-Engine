/** @file p_mapinfo.c  Hexen MAPINFO parsing.
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

#include "jhexen.h"
#include "p_mapinfo.h"
#include "r_common.h"
#include <stdio.h>
#include <string.h>

enum {
    CD_START,
    CD_END1,
    CD_END2,
    CD_END3,
    CD_INTERLUDE,
    CD_TITLE,
    NUM_CD_TRACKS
};

static int cdTracks[NUM_CD_TRACKS]; // Non-map specific song cd track numbers

static mapinfo_t MapInfo[99];
static uint mapCount;

static uint qualifyMap(uint map)
{
    return (map >= mapCount) ? 0 : map;
}

static void setSongCDTrack(int index, int track)
{
    static char *defaultTracks[] = {
        "startup",
        "hall",
        "orb",
        "chess",
        "hub",
        "hexen"
    };
    int cdTrack = track;

    App_Log(DE2_DEV_RES_VERBOSE, "setSongCDTrack: index=%i, track=%i", index, track);

    // Set the internal array.
    cdTracks[index] = cdTrack;

    // Update the corresponding Doomsday definition.
    Def_Set(DD_DEF_MUSIC, Def_Get(DD_DEF_MUSIC, defaultTracks[index], 0),
            DD_CD_TRACK, &cdTrack);
}

void P_InitMapInfo(void)
{
    uint map, mapMax = 0;
    char songMulch[10];
    mapinfo_t defMapInfo;
    mapinfo_t *info;

    memset(&MapInfo, 0, sizeof(MapInfo));

    // Configure the defaults
    defMapInfo.usingDefaults   = true;

    defMapInfo.cluster         = 0;
    defMapInfo.warpTrans       = 0;
    defMapInfo.nextMap         = 0; // Always go to map 0 if not specified.
    defMapInfo.cdTrack         = 1;
    defMapInfo.sky1Material    = Materials_ResolveUriCString(gameMode == hexen_demo || gameMode == hexen_betademo? "Textures:SKY2" : "Textures:SKY1");
    defMapInfo.sky2Material    = defMapInfo.sky1Material;
    defMapInfo.sky1ScrollDelta = 0;
    defMapInfo.sky2ScrollDelta = 0;
    defMapInfo.doubleSky       = false;
    defMapInfo.lightning       = false;
    defMapInfo.fadeTable       = W_GetLumpNumForName("COLORMAP");
    strcpy(defMapInfo.title, "DEVELOPMENT MAP"); // Unknown.

    for(map = 0; map < 99; ++map)
    {
        MapInfo[map].warpTrans = 0;
    }

    SC_Open("MAPINFO");
    while(SC_GetString())
    {
        if(SC_Compare("MAP") == false)
        {
            SC_ScriptError(NULL);
        }
        SC_MustGetNumber();
        if(sc_Number < 1 || sc_Number > 99)
            SC_ScriptError(NULL);

        map = (unsigned) sc_Number - 1;
        info = &MapInfo[map];

        // Save song lump name
        strcpy(songMulch, info->songLump);

        // Copy defaults to current map definition
        memcpy(info, &defMapInfo, sizeof(*info));

        // Restore song lump name
        strcpy(info->songLump, songMulch);

        // This information has been parsed from MAPINFO.
        info->usingDefaults = false;

        // The warp translation defaults to the map number
        info->warpTrans = map;

        // Map name must follow the number
        SC_MustGetString();
        strcpy(info->title, sc_String);

        // Process optional tokens
        while(SC_GetString())
        {
            if(SC_Compare("MAP"))
            {
                // Start of the next map definition.
                SC_UnGet();
                break;
            }

            if(SC_Compare("SKY1"))
            {
                ddstring_t path;
                Uri *uri;

                SC_MustGetString();
                Str_Init(&path);
                Str_PercentEncode(Str_Set(&path, sc_String));

                uri = Uri_NewWithPath2("Textures:", RC_NULL);
                Uri_SetPath(uri, Str_Text(&path));
                info->sky1Material = Materials_ResolveUri(uri);
                Uri_Delete(uri);
                Str_Free(&path);

                SC_MustGetNumber();
                info->sky1ScrollDelta = (float) sc_Number / 256;
                continue;
            }
            if(SC_Compare("SKY2"))
            {
                ddstring_t path;
                Uri *uri;

                SC_MustGetString();
                Str_Init(&path);
                Str_PercentEncode(Str_Set(&path, sc_String));

                uri = Uri_NewWithPath2("Textures:", RC_NULL);
                Uri_SetPath(uri, Str_Text(&path));
                info->sky2Material = Materials_ResolveUri(uri);
                Uri_Delete(uri);
                Str_Free(&path);

                SC_MustGetNumber();
                info->sky2ScrollDelta = (float) sc_Number / 256;
                continue;
            }
            if(SC_Compare("DOUBLESKY"))
            {
                info->doubleSky = true;
                continue;
            }
            if(SC_Compare("LIGHTNING"))
            {
                info->lightning = true;
                continue;
            }
            if(SC_Compare("FADETABLE"))
            {
                SC_MustGetString();
                info->fadeTable = W_GetLumpNumForName(sc_String);
                continue;
            }
            if(SC_Compare("CLUSTER"))
            {
                SC_MustGetNumber();
                if(sc_Number < 1)
                {
                    char buf[40];
                    dd_snprintf(buf, 40, "Invalid cluster %i", sc_Number);
                    SC_ScriptError(buf);
                }
                info->cluster = sc_Number;
                continue;
            }
            if(SC_Compare("WARPTRANS"))
            {
                SC_MustGetNumber();
                if(sc_Number < 1 || sc_Number > 99)
                    SC_ScriptError(NULL);
                info->warpTrans = (unsigned) sc_Number - 1;
                continue;
            }
            if(SC_Compare("NEXT"))
            {
                SC_MustGetNumber();
                if(sc_Number < 1 || sc_Number > 99)
                    SC_ScriptError(NULL);
                info->nextMap = (unsigned) sc_Number - 1;
                continue;
            }
            if(SC_Compare("CDTRACK"))
            {
                SC_MustGetNumber();
                info->cdTrack = sc_Number;
                continue;
            }

            if(SC_Compare("CD_START_TRACK"))
            {
                SC_MustGetNumber();
                setSongCDTrack(CD_START, sc_Number);
                continue;
            }
            if(SC_Compare("CD_END1_TRACK"))
            {
                SC_MustGetNumber();
                setSongCDTrack(CD_END1, sc_Number);
                continue;
            }
            if(SC_Compare("CD_END2_TRACK"))
            {
                SC_MustGetNumber();
                setSongCDTrack(CD_END2, sc_Number);
                continue;
            }
            if(SC_Compare("CD_END3_TRACK"))
            {
                SC_MustGetNumber();
                setSongCDTrack(CD_END3, sc_Number);
                continue;
            }
            if(SC_Compare("CD_INTERMISSION_TRACK"))
            {
                SC_MustGetNumber();
                setSongCDTrack(CD_INTERLUDE, sc_Number);
                continue;
            }
            if(SC_Compare("CD_TITLE_TRACK"))
            {
                SC_MustGetNumber();
                setSongCDTrack(CD_TITLE, sc_Number);
                continue;
            }

            // An unrecognized label.
            SC_ScriptError(NULL);
        }

        App_Log(DE2_DEV_RES_MSG, "MAPINFO: map%i \"%s\" warp:%i", map, info->title, info->warpTrans);

        mapMax = map > mapMax ? map : mapMax;
    }

    SC_Close();
    mapCount = mapMax+1;
}

void P_InitMapMusicInfo(void)
{
    uint i;

    for(i = 0; i < 99; ++i)
    {
        strcpy(MapInfo[i].songLump, "DEFSONG");
    }

    mapCount = 98;
}

mapinfo_t *P_MapInfo(uint map)
{
    return &MapInfo[qualifyMap(map)];
}

uint P_MapInfoCount()
{
    return mapCount;
}

uint P_TranslateMapIfExists(uint map)
{
    uint i;
    uint matchedWithoutCluster = P_INVALID_LOGICAL_MAP;

    for(i = 0; i < 99; ++i)
    {
        mapinfo_t const *info = &MapInfo[i];
        if(info->usingDefaults) continue; // Ignoring, undefined values.
        if(info->warpTrans == map)
        {
            if(info->cluster)
            {
                App_Log(DE2_DEV_MAP_VERBOSE, "Warp %i translated to logical map %i, cluster %i", map, i, info->cluster);
                return i;
            }
            else
            {
                App_Log(DE2_DEV_MAP_VERBOSE, "Warp %i matches logical map %i, but it has no cluster", map, i);
                matchedWithoutCluster = i;
            }
        }
    }

    App_Log(DE2_DEV_MAP_NOTE, "Could not find warp %i, translating to logical map %i (without cluster)",
            map, matchedWithoutCluster);

    return matchedWithoutCluster;
}

uint P_TranslateMap(uint map)
{
    uint translated = P_TranslateMapIfExists(map);
    if(translated == P_INVALID_LOGICAL_MAP)
    {
        // This function always returns a valid logical map.
        return 0;
    }
    return translated;
}

char *P_GetMapSongLump(uint map)
{
    if(!strcasecmp(MapInfo[qualifyMap(map)].songLump, "DEFSONG"))
    {
        return NULL;
    }
    else
    {
        return MapInfo[qualifyMap(map)].songLump;
    }
}

int P_GetCDStartTrack(void)
{
    return cdTracks[CD_START];
}

int P_GetCDEnd1Track(void)
{
    return cdTracks[CD_END1];
}

int P_GetCDEnd2Track(void)
{
    return cdTracks[CD_END2];
}

int P_GetCDEnd3Track(void)
{
    return cdTracks[CD_END3];
}

int P_GetCDIntermissionTrack(void)
{
    return cdTracks[CD_INTERLUDE];
}

int P_GetCDTitleTrack(void)
{
    return cdTracks[CD_TITLE];
}
