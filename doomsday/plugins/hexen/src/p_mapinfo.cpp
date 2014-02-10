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
#include "g_common.h"
#include "p_mapinfo.h"
#include "hexlex.h"
#include <cstdio>
#include <sstream>
#include <string.h>
#include <map>

#define MUSIC_STARTUP      "startup"
#define MUSIC_ENDING1      "hall"
#define MUSIC_ENDING2      "orb"
#define MUSIC_ENDING3      "chess"
#define MUSIC_INTERMISSION "hub"
#define MUSIC_TITLE        "hexen"

typedef std::map<std::string, mapinfo_t> MapInfos;
static MapInfos mapInfos;

/**
 * Update the Music definition @a musicId with the specified CD @a track number.
 */
static void setMusicCDTrack(char const *musicId, int track)
{
    App_Log(DE2_DEV_RES_VERBOSE, "setMusicCDTrack: musicId=%s, track=%i", musicId, track);

    int cdTrack = track;
    Def_Set(DD_DEF_MUSIC, Def_Get(DD_DEF_MUSIC, musicId, 0), DD_CD_TRACK, &cdTrack);
}

void MapInfoParser(Str const *path)
{
    mapInfos.clear();

    AutoStr *script = M_ReadFileIntoString(path, 0);

    if(script && !Str_IsEmpty(script))
    {
        App_Log(DE2_RES_VERBOSE, "Parsing \"%s\"...", F_PrettyPath(Str_Text(path)));

        // Prepare a default-configured definition, for one-shot initialization.
        mapinfo_t defMapInfo;
        defMapInfo.map             = -1; // Unknown.
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
        strcpy(defMapInfo.songLump, "DEFSONG"); // Unknown.

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
                if(tmap < 1)
                {
                    Con_Error("MapInfoParser: Invalid map number '%s' in \"%s\" on line #%i",
                              lexer.token(), F_PrettyPath(Str_Text(path)), lexer.lineNumber());
                }

                Uri *mapUri = G_ComposeMapUri(0, tmap - 1);
                mapinfo_t *info = P_MapInfo(mapUri);
                if(!info)
                {
                    // A new map info.
                    info = &mapInfos[Str_Text(Uri_Compose(mapUri))];

                    // Initialize with the default values.
                    memcpy(info, &defMapInfo, sizeof(*info));

                    // Assign a logical map index.
                    info->map = tmap - 1;

                    // The warp translation defaults to the logical map index.
                    info->warpTrans = tmap - 1;
                }
                Uri_Delete(mapUri);

                // Map title must follow the number.
                strcpy(info->title, Str_Text(lexer.readString()));

                // Process optional tokens.
                while(lexer.readToken())
                {
                    if(!Str_CompareIgnoreCase(lexer.token(), "sky1"))
                    {
                        Uri *uri = lexer.readUri("Textures");

                        info->sky1Material    = Materials_ResolveUri(uri);
                        info->sky1ScrollDelta = (float) lexer.readNumber() / 256;

                        Uri_Delete(uri);
                        continue;
                    }
                    if(!Str_CompareIgnoreCase(lexer.token(), "sky2"))
                    {
                        Uri *uri = lexer.readUri("Textures");

                        info->sky2Material    = Materials_ResolveUri(uri);
                        info->sky2ScrollDelta = (float) lexer.readNumber() / 256;

                        Uri_Delete(uri);
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
                        if(mapWarpNum < 1)
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
                        if(map < 1)
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

#ifdef DENG_DEBUG
    for(MapInfos::const_iterator i = mapInfos.begin(); i != mapInfos.end(); ++i)
    {
        mapinfo_t const &info = i->second;
        App_Log(DE2_DEV_RES_MSG, "MAPINFO %s { title: \"%s\" cluster: %i map: %i warp: %i }",
                                 i->first.c_str(), info.title, info.cluster, info.map, info.warpTrans);
    }
#endif
}

mapinfo_t *P_MapInfo(Uri const *mapUri)
{
    if(!mapUri) mapUri = gameMapUri;

    std::string mapPath(Str_Text(Uri_Compose(mapUri)));
    MapInfos::iterator found = mapInfos.find(mapPath);
    if(found != mapInfos.end())
    {
        return &found->second;
    }
    return 0;
}

uint P_TranslateMapIfExists(uint map)
{
    uint matchedWithoutCluster = P_INVALID_LOGICAL_MAP;

    for(MapInfos::const_iterator i = mapInfos.begin(); i != mapInfos.end(); ++i)
    {
        mapinfo_t const &info = i->second;

        if(info.warpTrans == map)
        {
            if(info.cluster)
            {
                App_Log(DE2_DEV_MAP_VERBOSE, "Warp %i translated to logical map %i, cluster %i", map, info.map, info.cluster);
                return info.map;
            }

            App_Log(DE2_DEV_MAP_VERBOSE, "Warp %i matches logical map %i, but it has no cluster", map, info.map);
            matchedWithoutCluster = info.map;
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
