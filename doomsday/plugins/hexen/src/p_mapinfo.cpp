/** @file p_mapinfo.cpp  Hexen MAPINFO parsing.
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
#include "hexlex.h"
#include <cstdio>
#include <cstring>

#define MUSIC_STARTUP      "startup"
#define MUSIC_ENDING1      "hall"
#define MUSIC_ENDING2      "orb"
#define MUSIC_ENDING3      "chess"
#define MUSIC_INTERMISSION "hub"
#define MUSIC_TITLE        "hexen"

static mapinfo_t MapInfo[99];
static uint mapCount;

static uint qualifyMap(uint map)
{
    return (map >= mapCount) ? 0 : map;
}

static void setMusicCDTrack(char const *musicId, int track)
{
    App_Log(DE2_DEV_RES_VERBOSE, "setSongCDTrack: musicId=%s, track=%i", musicId, track);

    // Update the corresponding Music definition.
    int cdTrack = track;
    Def_Set(DD_DEF_MUSIC, Def_Get(DD_DEF_MUSIC, musicId, 0), DD_CD_TRACK, &cdTrack);
}

void MapInfoParser(Str const *path)
{
    memset(&MapInfo, 0, sizeof(MapInfo));

    // Configure the defaults
    mapinfo_t defMapInfo;
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

    for(uint map = 0; map < 99; ++map)
    {
        MapInfo[map].warpTrans = 0;
    }
    uint mapMax = 0;

    AutoStr *script = M_ReadFileIntoString(path, 0);

    if(script && !Str_IsEmpty(script))
    {
        App_Log(DE2_RES_VERBOSE, "Parsing \"%s\"...", F_PrettyPath(Str_Text(path)));

        HexLex lexer(script, path);

        while(lexer.readToken())
        {
            if(!Str_CompareIgnoreCase(lexer.token(), "cd_start_track"))
            {
                setMusicCDTrack(MUSIC_STARTUP, lexer.readNumber());
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "cd_end1_track"))
            {
                setMusicCDTrack(MUSIC_ENDING1, lexer.readNumber());
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "cd_end2_track"))
            {
                setMusicCDTrack(MUSIC_ENDING2, lexer.readNumber());
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "cd_end3_track"))
            {
                setMusicCDTrack(MUSIC_ENDING3, lexer.readNumber());
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "cd_intermission_track"))
            {
                setMusicCDTrack(MUSIC_INTERMISSION, lexer.readNumber());
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "cd_title_track"))
            {
                setMusicCDTrack(MUSIC_TITLE, lexer.readNumber());
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "map"))
            {
                int tmap = lexer.readNumber();
                if(tmap < 1 || tmap > 99)
                {
                    Con_Error("MapInfoParser: Invalid map number '%s' in \"%s\" on line #%i",
                              lexer.token(), F_PrettyPath(Str_Text(path)), lexer.lineNumber());
                }

                uint map = (unsigned) tmap - 1;
                mapinfo_t *info = &MapInfo[map];

                // Save song lump name.
                char songMulch[10];
                strcpy(songMulch, info->songLump);

                // Copy defaults to current map definition.
                memcpy(info, &defMapInfo, sizeof(*info));

                // Restore song lump name.
                strcpy(info->songLump, songMulch);

                // This information has been parsed from MAPINFO.
                info->usingDefaults = false;

                // The warp translation defaults to the map number.
                info->warpTrans = map;

                // Map name must follow the number.
                strcpy(info->title, Str_Text(lexer.readString()));

                // Process optional tokens.
                while(lexer.readToken())
                {
                    if(!Str_CompareIgnoreCase(lexer.token(), "sky1"))
                    {
                        ddstring_t path; Str_InitStd(&path);
                        Str_PercentEncode(Str_Copy(&path, lexer.readString()));

                        Uri *uri = Uri_NewWithPath2("Textures:", RC_NULL);
                        Uri_SetPath(uri, Str_Text(&path));
                        info->sky1Material = Materials_ResolveUri(uri);
                        Uri_Delete(uri);
                        Str_Free(&path);

                        info->sky1ScrollDelta = (float) lexer.readNumber() / 256;
                        continue;
                    }
                    if(!Str_CompareIgnoreCase(lexer.token(), "sky2"))
                    {
                        ddstring_t path; Str_InitStd(&path);
                        Str_PercentEncode(Str_Copy(&path, lexer.readString()));

                        Uri *uri = Uri_NewWithPath2("Textures:", RC_NULL);
                        Uri_SetPath(uri, Str_Text(&path));
                        info->sky2Material = Materials_ResolveUri(uri);
                        Uri_Delete(uri);
                        Str_Free(&path);

                        info->sky2ScrollDelta = (float) lexer.readNumber() / 256;
                        continue;
                    }
                    if(!Str_CompareIgnoreCase(lexer.token(), "doublesky"))
                    {
                        info->doubleSky = true;
                        continue;
                    }
                    if(!Str_CompareIgnoreCase(lexer.token(), "lightning"))
                    {
                        info->lightning = true;
                        continue;
                    }
                    if(!Str_CompareIgnoreCase(lexer.token(), "fadetable"))
                    {
                        info->fadeTable = W_GetLumpNumForName(Str_Text(lexer.readString()));
                        continue;
                    }
                    if(!Str_CompareIgnoreCase(lexer.token(), "cluster"))
                    {
                        info->cluster = lexer.readNumber();
                        if(info->cluster < 1)
                        {
                            Con_Error("MapInfoParser: Invalid cluster number '%s' in \"%s\" on line #%i",
                                      lexer.token(), F_PrettyPath(Str_Text(path)), lexer.lineNumber());
                        }
                        continue;
                    }
                    if(!Str_CompareIgnoreCase(lexer.token(), "warptrans"))
                    {
                        int mapWarpNum =  lexer.readNumber();
                        if(mapWarpNum < 1 || mapWarpNum > 99)
                        {
                            Con_Error("MapInfoParser: Invalid map warp-number '%s' in \"%s\" on line #%i",
                                      lexer.token(), F_PrettyPath(Str_Text(path)), lexer.lineNumber());
                        }

                        info->warpTrans = (unsigned) mapWarpNum - 1;
                        continue;
                    }
                    if(!Str_CompareIgnoreCase(lexer.token(), "next"))
                    {
                        int map = lexer.readNumber();
                        if(map < 1 || map > 99)
                        {
                            Con_Error("MapInfoParser: Invalid map number '%s' in \"%s\" on line #%i",
                                      lexer.token(), F_PrettyPath(Str_Text(path)), lexer.lineNumber());
                        }
                        info->nextMap = (unsigned) map - 1;
                        continue;
                    }
                    if(!Str_CompareIgnoreCase(lexer.token(), "cdtrack"))
                    {
                        info->cdTrack = lexer.readNumber();
                        continue;
                    }

                    lexer.unreadToken();
                    break;
                }

                App_Log(DE2_DEV_RES_MSG, "MAPINFO: map%i title: \"%s\" warp: %i", map, info->title, info->warpTrans);
                mapMax = map > mapMax ? map : mapMax;

                continue;
            }

            // Found an unexpected token.
            Con_Error("MapInfoParser: Unexpected token '%s' in \"%s\" on line #%i",
                      lexer.token(), F_PrettyPath(Str_Text(path)), lexer.lineNumber());
        }
    }
    else
    {
        App_Log(DE2_RES_WARNING, "MapInfoParser: Failed to open definition/script file \"%s\" for reading", F_PrettyPath(Str_Text(path)));
    }

    mapCount = mapMax + 1;
}

void P_InitMapInfo()
{
    for(uint i = 0; i < 99; ++i)
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
    uint matchedWithoutCluster = P_INVALID_LOGICAL_MAP;

    for(uint i = 0; i < 99; ++i)
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

            App_Log(DE2_DEV_MAP_VERBOSE, "Warp %i matches logical map %i, but it has no cluster", map, i);
            matchedWithoutCluster = i;
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
