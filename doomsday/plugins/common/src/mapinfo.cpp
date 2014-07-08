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

#include "mapinfo.h"
#include <cstdio>
#include <sstream>
#include <cstring>
#include <map>
#include "hexlex.h"
#include "g_common.h"

using namespace de;

#define MUSIC_STARTUP      "startup"
#define MUSIC_ENDING1      "hall"
#define MUSIC_ENDING2      "orb"
#define MUSIC_ENDING3      "chess"
#define MUSIC_INTERMISSION "hub"
#define MUSIC_TITLE        "hexen"

typedef std::map<std::string, MapInfo> MapInfos;
static MapInfos mapInfos;

static inline String defaultSkyMaterial()
{
#ifdef __JHEXEN__
    if(gameMode == hexen_demo || gameMode == hexen_betademo)
        return "Textures:SKY2";
#endif
    return "Textures:SKY1";
}

MapInfo::MapInfo() : Record()
{
    resetToDefaults();
}

void MapInfo::resetToDefaults()
{
    // Add all expected fields with their default values.
    addText   ("map", "Maps:"); // URI. Unknown.
    addNumber ("hub", 0);
    addNumber ("warpTrans", 0);
    addNumber ("nextMap", 0);   // Warp trans number. Always go to map 0 if not specified.
    addNumber ("cdTrack", 1);
    addText   ("title", "Untitled");
    addText   ("sky1Material", defaultSkyMaterial());
    addText   ("sky2Material", defaultSkyMaterial());
    addNumber ("sky1ScrollDelta", 0);
    addNumber ("sky2ScrollDelta", 0);
    addBoolean("doubleSky", false);
    addBoolean("lightning", false);
    addText   ("fadeTable", "COLORMAP");
    addText   ("songLump", "DEFSONG");
}

/**
 * Update the Music definition @a musicId with the specified CD @a track number.
 */
static void setMusicCDTrack(char const *musicId, int track)
{
    LOG_RES_VERBOSE("setMusicCDTrack: musicId=%s, track=%i") << musicId << track;

    int cdTrack = track;
    Def_Set(DD_DEF_MUSIC, Def_Get(DD_DEF_MUSIC, musicId, 0), DD_CD_TRACK, &cdTrack);
}

void MapInfoParser(ddstring_s const *path)
{
    mapInfos.clear();

    AutoStr *script = M_ReadFileIntoString(path, 0);
    if(!script || Str_IsEmpty(script))
    {
        LOG_RES_WARNING("MapInfoParser: Failed to open definition/script file \"%s\" for reading")
                << NativePath(Str_Text(path)).pretty();
        return;
    }

    LOG_RES_VERBOSE("Parsing \"%s\"...") << NativePath(Str_Text(path)).pretty();

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
            de::Uri mapUri;
            String const mapRef = String(Str_Text(lexer.readString()));

            bool isNumber;
            int mapNumber = mapRef.toInt(&isNumber); // 1-based
            if(!isNumber)
            {
                mapUri = de::Uri(mapRef, RC_NULL);
                if(mapUri.scheme().isEmpty()) mapUri.setScheme("Maps");
            }
            else
            {
                if(mapNumber < 1)
                {
                    Con_Error("MapInfoParser: Invalid map number '%s' in \"%s\" on line #%i",
                              mapNumber, F_PrettyPath(Str_Text(path)), lexer.lineNumber());
                }
                mapUri = G_ComposeMapUri(0, mapNumber - 1);
            }

            MapInfo *info = P_MapInfo(&mapUri);
            if(!info)
            {
                // A new map info.
                info = &mapInfos[mapUri.path().asText().toLower().toUtf8().constData()];

                // Initialize with the default values.
                info->resetToDefaults();

                info->set("map", mapUri.compose());

                // Attempt to extract the "warp translation" number.
                /// @todo Define a sensible default should the map identifier not
                /// follow the default MAPXX form (what does ZDoom do here?) -ds
                info->set("warpTrans", G_MapNumberFor(mapUri));
            }

            // Map title must follow the number.
            info->set("title", Str_Text(lexer.readString()));

            // Process optional tokens.
            while(lexer.readToken())
            {
                if(!Str_CompareIgnoreCase(lexer.token(), "sky1"))
                {
                    info->set("sky1Material", lexer.readUri("Textures").compose());
                    info->set("sky1ScrollDelta", lexer.readNumber() / 256.f);
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "sky2"))
                {
                    info->set("sky2Material", lexer.readUri("Textures").compose());
                    info->set("sky2ScrollDelta", lexer.readNumber() / 256.f);
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "doublesky"))
                {
                    info->set("doubleSky", true);
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "lightning"))
                {
                    info->set("lightning", true);
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "fadetable"))
                {
                    info->set("fadeTable", Str_Text(lexer.readString()));
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "cluster"))
                {
                    int const hubNum = lexer.readNumber();
                    if(hubNum < 1)
                    {
                        Con_Error("MapInfoParser: Invalid 'cluster' (i.e., hub) number '%s' in \"%s\" on line #%i",
                                  lexer.token(), F_PrettyPath(Str_Text(path)), lexer.lineNumber());
                    }
                    info->set("hub", hubNum);
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "warptrans"))
                {
                    int const mapWarpNum = lexer.readNumber();
                    if(mapWarpNum < 1)
                    {
                        Con_Error("MapInfoParser: Invalid map warp-number '%s' in \"%s\" on line #%i",
                                  lexer.token(), F_PrettyPath(Str_Text(path)), lexer.lineNumber());
                    }
                    info->set("warpTrans", mapWarpNum - 1);
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "next"))
                {
                    int const map = lexer.readNumber();
                    if(map < 1)
                    {
                        Con_Error("MapInfoParser: Invalid map number '%s' in \"%s\" on line #%i",
                                  lexer.token(), F_PrettyPath(Str_Text(path)), lexer.lineNumber());
                    }
                    info->set("nextMap", map - 1);
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "cdtrack"))
                {
                    info->set("cdTrack", lexer.readNumber());
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

#ifdef DENG_DEBUG
    for(MapInfos::const_iterator i = mapInfos.begin(); i != mapInfos.end(); ++i)
    {
        MapInfo const &info = i->second;
        LOG_RES_MSG("MAPINFO %s { title: \"%s\" hub: %i map: %s warp: %i }")
                << i->first.c_str() << info.gets("title")
                << info.geti("hub") << info.gets("map") << info.geti("warpTrans");
    }
#endif
}

MapInfo *P_MapInfo(de::Uri const *mapUri)
{
    if(!mapUri) mapUri = &gameMapUri;

    if(mapUri->scheme().compareWithoutCase("Maps")) return 0;

    MapInfos::iterator found = mapInfos.find(mapUri->path().toString().toLower().toStdString());
    if(found != mapInfos.end())
    {
        return &found->second;
    }
    //App_Log(DE2_DEV_MAP_NOTE, "Unknown MAPINFO definition '%s'", Str_Text(mapUriStr));
    return 0;
}

de::Uri P_TranslateMapIfExists(uint map)
{
    de::Uri matchedWithoutHub("Maps:", RC_NULL);

    for(MapInfos::const_iterator i = mapInfos.begin(); i != mapInfos.end(); ++i)
    {
        MapInfo const &info = i->second;

        if(info.geti("warpTrans") == map)
        {
            if(info.geti("hub"))
            {
                LOGDEV_MAP_VERBOSE("Warp %u translated to map %s, hub %i")
                        << map << info.gets("map") << info.geti("hub");
                return de::Uri(info.gets("map"), RC_NULL);
            }

            LOGDEV_MAP_VERBOSE("Warp %u matches map %s, but it has no hub")
                    << map << info.gets("map");
            matchedWithoutHub = de::Uri(info.gets("map"), RC_NULL);
        }
    }

    LOGDEV_MAP_NOTE("Could not find warp %i, translating to map %s (without hub)")
            << map << matchedWithoutHub;

    return matchedWithoutHub;
}

de::Uri P_TranslateMap(uint map)
{
    de::Uri translated = P_TranslateMapIfExists(map);
    if(translated.path().isEmpty())
    {
        // This function always returns a valid map.
        return G_ComposeMapUri(0, 0); // Always available?
    }
    return translated;
}
