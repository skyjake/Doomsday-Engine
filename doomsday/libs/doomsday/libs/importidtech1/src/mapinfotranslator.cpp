/** @file mapinfotranslator.cpp  Hexen-format MAPINFO definition translator.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "mapinfotranslator.h"

#include <cstdio>
#include <cstring>
#include <sstream>
#include <map>

#include <doomsday/game.h>
#include <doomsday/doomsdayapp.h>
#include <de/app.h>
#include <de/error.h>
#include <de/record.h>
#include "hexlex.h"

using namespace de;

namespace idtech1 {
namespace internal {

    static inline String defaultSkyMaterial()
    {
        const String gameIdKey = DoomsdayApp::game().id();
        if(gameIdKey == "hexen-demo" || gameIdKey == "hexen-betademo")
        {
            return "Textures:SKY2";
        }
        return "Textures:SKY1";
    }

    /**
     * Determines whether to interpret cluster numbers as episode ids. This is necessary for
     * ZDoom-compatible interpretation of MAPINFO.
     */
    static bool interpretHubNumberAsEpisodeId()
    {
        const String gameIdKey = DoomsdayApp::game().id();
        return (gameIdKey.beginsWith("doom1") || gameIdKey.beginsWith("heretic") ||
                gameIdKey.beginsWith("chex"));
    }

    static inline String toMapId(const res::Uri &mapUri)
    {
        return mapUri.scheme().compareWithoutCase("Maps") ? mapUri.compose() : mapUri.path().toString();
    }

    class Music : public de::Record
    {
    public:
        Music() : Record() {
            resetToDefaults();
        }
        Music &operator = (const Music &other) {
            static_cast<Record &>(*this) = other;
            return *this;
        }

        void resetToDefaults() {
            addBoolean("custom", false);

            // Add all expected fields with their default values.
            addText   ("id", "");
            addNumber ("cdTrack", 1);
        }
    };

    class MapInfo : public de::Record
    {
    public:
        MapInfo() : Record() {
            resetToDefaults();
        }
        MapInfo &operator = (const MapInfo &other) {
            static_cast<Record &>(*this) = other;
            return *this;
        }

        void resetToDefaults() {
            addBoolean("custom", false);

            // Add all expected fields with their default values.
            addNumber ("cdTrack", 1);
            addBoolean("doubleSky", false);
            addText   ("fadeTable", "COLORMAP");
            addNumber ("hub", 0);
            addText   ("id", "Maps:");        // URI. Unknown.
            addBoolean("lightning", false);
            addText   ("music", "");
            addBoolean("nointermission", false);
            addText   ("nextMap", "");        // URI. None. (If scheme is "@wt" then the path is a warp trans number).
            addNumber ("par", 0);
            addText   ("secretNextMap", "");  // URI. None. (If scheme is "@wt" then the path is a warp trans number).
            addText   ("sky1Material", defaultSkyMaterial());
            addNumber ("sky1ScrollDelta", 0);
            addText   ("sky2Material", defaultSkyMaterial());
            addNumber ("sky2ScrollDelta", 0);
            addText   ("title", "Untitled");
            addText   ("titleImage", "");     // URI. None.
            addNumber ("warpTrans", 0);
        }
    };

    class EpisodeInfo : public de::Record
    {
    public:
        EpisodeInfo() : Record() {
            resetToDefaults();
        }
        EpisodeInfo &operator = (const EpisodeInfo &other) {
            static_cast<Record &>(*this) = other;
            return *this;
        }

        void resetToDefaults() {
            addBoolean("custom", false);

            // Add all expected fields with their default values.
            addText("id", "");            // Unknown.
            addText("menuHelpInfo", "");  // None.
            addText("menuImage", "");     // URI. None.
            addText("menuShortcut", "");  // Key name. None.
            addText("startMap", "Maps:"); // URI. Unknown.
            addText("title", "Untitled");
        }
    };

    /**
     * Central database of definitions read from Hexen-derived definition formats.
     */
    struct HexDefs {
        typedef KeyMap<String, Music>       Musics;
        typedef KeyMap<String, EpisodeInfo> EpisodeInfos;
        typedef KeyMap<String, MapInfo>     MapInfos;

        Musics       musics;
        EpisodeInfos episodeInfos;
        MapInfos     mapInfos;

        void clear()
        {
            musics.clear();
            episodeInfos.clear();
            mapInfos.clear();
        }

        /**
         * @param id  Identifier of the music to lookup info for.
         *
         * @return  Music for the specified @a id; otherwise @c 0 (not found).
         */
        Music *getMusic(String id)
        {
            if(!id.isEmpty())
            {
                Musics::iterator found = musics.find(id.lower());
                if(found != musics.end())
                {
                    return &found->second;
                }
            }
            return 0; // Not found.
        }

        /**
         * @param id  Identifier of the episode to lookup info for.
         *
         * @return  EpisodeInfo for the specified @a id; otherwise @c 0 (not found).
         */
        EpisodeInfo *getEpisodeInfo(String id)
        {
            if(!id.isEmpty())
            {
                EpisodeInfos::iterator found = episodeInfos.find(id.lower());
                if(found != episodeInfos.end())
                {
                    return &found->second;
                }
            }
            return 0; // Not found.
        }

        /**
         * @param mapUri  Identifier of the map to lookup info for.
         *
         * @return  MapInfo for the specified @a mapUri; otherwise @c 0 (not found).
         */
        MapInfo *getMapInfo(const res::Uri &mapUri)
        {
            if(!mapUri.scheme().compareWithoutCase("Maps"))
            {
                MapInfos::iterator found = mapInfos.find(mapUri.path().toString().lower());
                if(found != mapInfos.end())
                {
                    return &found->second;
                }
            }
            return 0; // Not found.
        }
    };

    static res::Uri composeMapUri(uint episode, uint map)
    {
        const String gameIdKey = DoomsdayApp::game().id();
        if(gameIdKey.beginsWith("doom1") || gameIdKey.beginsWith("heretic"))
        {
            return res::makeUri(Stringf("Maps:E%iM%i", episode+1, map+1));
        }
        return res::makeUri(Stringf("Maps:MAP%02i", map+1));
    }

    static uint mapWarpNumberFor(const res::Uri &mapUri)
    {
        String path = mapUri.path();
        if(!path.isEmpty())
        {
            if(path.first().lower() == 'e' && path.at(CharPos(2)).lower() == 'm')
            {
                return de::max(path.substr(CharPos(3)).toInt(0, 10, String::AllowSuffix), 1);
            }
            if(path.beginsWith("map", CaseInsensitive))
            {
                return de::max(path.substr(BytePos(3)).toInt(0, 10, String::AllowSuffix), 1);
            }
        }
        return 0;
    }

    /**
     * Parser for Hexen's MAPINFO definition lumps.
     */
    class MapInfoParser
    {
    public:
        /// Base class for all parse-related errors. @ingroup errors
        DE_ERROR(ParseError);

        /// Mappings from symbolic song name to music id.
        typedef KeyMap<String, String> MusicMappings;
        MusicMappings musicMap;

        bool reportErrors = true;
        bool sourceIsCustom = false;

        MapInfoParser(HexDefs &db) : db(db), defaultMap(0)
        {
            // Init the music id mappings.
            musicMap.insert("end1",         "hall");
            musicMap.insert("end2",         "orb");
            musicMap.insert("end3",         "chess");
            musicMap.insert("intermission", "hub");
            musicMap.insert("title",        "title");
            musicMap.insert("start",        "startup");
        }
        ~MapInfoParser() { clearDefaultMap(); }

        /**
         * Clear any custom default MapInfo definition currently in use. MapInfos
         * read after this is called will use the games' default definition as a
         * basis (unless specified otherwise).
         */
        void clearDefaultMap()
        {
            delete defaultMap; defaultMap = 0;
        }

        void tryParse(const AutoStr &buffer, String sourceFile, bool sourceIsCustom)
        {
            try
            {
                parse(buffer, sourceFile, sourceIsCustom);
            }
            catch (const ParseError &)
            {
                if (reportErrors)
                {
                    throw;
                }
            }
        }

        void parse(const AutoStr &buffer, String /*sourceFile*/, bool sourceIsCustom)
        {
            LOG_AS("MapInfoParser");

            // Nothing to parse?
            if(Str_IsEmpty(&buffer))
                return;

            this->sourceIsCustom = sourceIsCustom;
            
            // May opt out of error reporting.
            {
                const String bufText(Str_Text(&buffer));
                if (bufText.contains("// Doomsday: Ignore errors!", CaseInsensitive))
                {
                    reportErrors = false;
                }
                /// @todo Better to look for all comment lines instead.
                if (bufText.contains("// ZDaemon"))
                {
                    // Wrong format.
                    return;
                }
            }

            lexer.parse(&buffer);
            while(lexer.readToken())
            {
                String tok = Str_Text(lexer.token());
                if(tok.beginsWith("cd_", CaseInsensitive) &&
                   tok.endsWith("_track", CaseInsensitive))
                {
                    const String pubName = tok.substr(BytePos(3), tok.size() - 6 - 3);
                    MusicMappings::const_iterator found = musicMap.constFind(pubName);
                    if(found != musicMap.end())
                    {
                        // Lookup an existing music from the database.
                        const String songId = found->second;
                        Music *music = db.getMusic(songId);
                        if(!music)
                        {
                            // A new music.
                            music = &db.musics[songId];
                            music->set("id", songId);
                        }
                        music->set("cdTrack", (int)lexer.readNumber());
                        if(sourceIsCustom) music->set("custom", true);
                        continue;
                    }
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "clearepisodes")) // ZDoom
                {
                    reportProblem("MAPINFO ClearEpisodes directives are not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "clearskills")) // ZDoom
                {
                    reportProblem("MAPINFO ClearSkills directives are not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "clusterdef")) // ZDoom
                {
                    parseCluster();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "episode")) // ZDoom
                {
                    parseEpisode();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "map"))
                {
                    parseMap();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "defaultmap")) // ZDoom
                {
                    // Custom default MapInfo definition to be used as the basis for subsequent defs.
                    addDefaultMapIfNeeded();
                    parseMap(defaultMap);
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "adddefaultmap")) // ZDoom
                {
                    // As per 'defaultmap' but additive.
                    addDefaultMapIfNeeded(false/*don't reset*/);
                    parseMap(defaultMap);
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "gamedefaults")) // ZDoom
                {
                    // Custom default MapInfo definition which is seemingly only used by ZDoom itself
                    // as a way to get around their changes to/repurposing of the MAPINFO mechanism.
                    // We probably don't need to support this. -ds
                    MapInfo tempMap;
                    parseMap(&tempMap);
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "skill")) // ZDoom
                {
                    parseSkill();
                    continue;
                }

                // Unexpected token encountered.
                throw ParseError(stringf("Unexpected token '%s' on line #%i",
                                         Str_Text(lexer.token()),
                                         lexer.lineNumber()));
            }
        }

    private:
        HexDefs &db;
        HexLex lexer;
        MapInfo *defaultMap;

        void addDefaultMapIfNeeded(bool resetToDefaultsIfPresent = true)
        {
            if(!defaultMap)
            {
                defaultMap = new MapInfo;
            }
            else if(resetToDefaultsIfPresent)
            {
                defaultMap->resetToDefaults();
            }
        }

        void parseCluster() // ZDoom
        {
            reportProblem("MAPINFO Cluster definitions are not supported.");

            /*const int clusterId = (int)*/lexer.readNumber();

            // Process optional tokens.
            while(lexer.readToken())
            {
                if(!Str_CompareIgnoreCase(lexer.token(), "entertext"))
                {
                    String enterText = Str_Text(lexer.readString());

                    // Lookup the enter text from a Text definition?
                    if(!enterText.compareWithoutCase("lookup"))
                    {
                        enterText = Str_Text(lexer.readString());
                    }
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "exittext"))
                {
                    String exitText = Str_Text(lexer.readString());

                    // Lookup the exit text from a Text definition?
                    if(!exitText.compareWithoutCase("lookup"))
                    {
                        exitText = Str_Text(lexer.readString());
                    }
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "music"))
                {
                    lexer.readString();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "flat"))
                {
                    lexer.readString();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "pic"))
                {
                    lexer.readString();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "hub"))
                {
                    continue;
                }

                lexer.unreadToken();
                break;
            }
        }

        void parseEpisode() // ZDoom
        {
            res::Uri mapUri(Str_Text(lexer.readString()), RC_NULL);
            if(mapUri.scheme().isEmpty()) mapUri.setScheme("Maps");

            // A new episode info.
            const String id = String::asText(db.episodeInfos.size() + 1);
            EpisodeInfo *info = &db.episodeInfos[id.toStdString()];

            if(sourceIsCustom) info->set("custom", true);
            info->set("id", id);
            info->set("startMap", mapUri.compose());

            // Process optional tokens.
            while(lexer.readToken())
            {
                if(!Str_CompareIgnoreCase(lexer.token(), "name"))
                {
                    info->set("title", Str_Text(lexer.readString()));
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "lookup"))
                {
                    info->set("title", Str_Text(lexer.readString()));
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "picname"))
                {
                    info->set("menuImage", lexer.readUri("Patches").compose());
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "key"))
                {
                    info->set("menuShortcut", Str_Text(lexer.readString()));
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "remove"))
                {
                    reportProblem("MAPINFO Episode.remove is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "noskillmenu"))
                {
                    reportProblem("MAPINFO Episode.noskillmenu is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "optional"))
                {
                    // All episodes are "optional".
                    //LOG_MAP_WARNING("MAPINFO Episode.optional is not supported.");
                    continue;
                }

                lexer.unreadToken();
                break;
            }
        }

        /**
         * @note EndGame definitions appear inside a Map definition and unlike all other definition
         * block types are scoped with curly-braces.
         *
         * @param mapInfo  MapInfo definition for which the EndGame subblock applies.
         */
        void parseEndGame(MapInfo & /*mapInfo*/) // ZDoom
        {
            reportProblem("MAPINFO Map.next[EndGame] definitions are not supported.");

            lexer.readToken();
            if (Str_CompareIgnoreCase(lexer.token(), "{"))
                throw ParseError(stringf("Expected '{' but found '%s' on line #%i",
                                         Str_Text(lexer.token()),
                                         lexer.lineNumber()));

            while(lexer.readToken())
            {
                if(!Str_CompareIgnoreCase(lexer.token(), "}"))
                {
                    break;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "cast"))
                {
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "hscroll"))
                {
                    lexer.readString();
                    lexer.readString();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "music"))
                {
                    lexer.readString();
                    lexer.readNumber(); // Optional?
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "pic"))
                {
                    lexer.readString();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "vscroll"))
                {
                    lexer.readString();
                    lexer.readString();
                    continue;
                }

                lexer.unreadToken();
                break;
            }
        }

        /**
         * @param isSecret  @c true= this is the secret next map (from ZDoom).
         */
        void parseMapNext(MapInfo &mapInfo, bool isSecret = false)
        {
            const ddstring_s *tok = lexer.readString();

            // Perhaps a ZDoom EndGame directive?
            if(!Str_CompareIgnoreCase(tok, "endpic"))
            {
                reportProblem("MAPINFO Map.next EndGame directives are not supported.");
                lexer.readString();
                return;
            }
            if(!Str_CompareIgnoreCase(tok, "endbunny") ||
               !Str_CompareIgnoreCase(tok, "enddemon") ||
               !Str_CompareIgnoreCase(tok, "endgame1") ||
               !Str_CompareIgnoreCase(tok, "endgame2") ||
               !Str_CompareIgnoreCase(tok, "endgame3") ||
               !Str_CompareIgnoreCase(tok, "endgame4") ||
               !Str_CompareIgnoreCase(tok, "endgamec") ||
               !Str_CompareIgnoreCase(tok, "endgames") ||
               !Str_CompareIgnoreCase(tok, "endgamew") ||
               !Str_CompareIgnoreCase(tok, "endtitle"))
            {
                reportProblem("MAPINFO Map.next EndGame directives are not supported.");
                return;
            }
            if(!Str_CompareIgnoreCase(tok, "endgame"))
            {
                parseEndGame(mapInfo);
                return;
            }

            res::Uri mapUri;
            bool isNumber;
            int mapNumber = String(Str_Text(tok)).toInt(&isNumber); // 1-based
            if(!isNumber)
            {
                mapUri = res::makeUri(Str_Text(tok));
                if(mapUri.scheme().isEmpty()) mapUri.setScheme("Maps");
                mapInfo.set((isSecret? "secretNextMap" : "nextMap"), mapUri.compose());
            }
            else
            {
                mapInfo.set((isSecret? "secretNextMap" : "nextMap"), Stringf("@wt:%i", mapNumber));
            }
        }

        void reportProblem(const String &msg)
        {
            if (reportErrors)
            {
                LOG_MAP_WARNING(msg);
            }
        }

        /**
         * @param info  If non-zero parse the definition to this record. Otherwise the relevant
         *              MapInfo record will be located/created in the main database.
         */
        void parseMap(MapInfo *info = 0)
        {
            res::Uri mapUri;
            if(!info)
            {
                const String mapRef = String(Str_Text(lexer.readString()));

                bool isNumber;
                int mapNumber = mapRef.toInt(&isNumber); // 1-based
                if(!isNumber)
                {
                    mapUri = res::makeUri(mapRef);
                    if(mapUri.scheme().isEmpty()) mapUri.setScheme("Maps");
                }
                else
                {
                    if(mapNumber < 1)
                    {
                        throw ParseError(stringf(
                            "Invalid map number '%i' on line #%i", mapNumber, lexer.lineNumber()));
                    }
                    mapUri = composeMapUri(0, mapNumber - 1);
                }

                // Lookup an existing map info from the database.
                info = db.getMapInfo(mapUri);

                if(!info)
                {
                    // A new map info.
                    info = &db.mapInfos[mapUri.path().asText().lower()];

                    // Initialize with custom default values?
                    if(defaultMap)
                    {
                        *info = *defaultMap;
                    }

                    info->set("id", mapUri.compose());

                    // Attempt to extract the map "warp number".
                    info->set("warpTrans", mapWarpNumberFor(mapUri));
                }

                // Map title follows the number.
                String title = Str_Text(lexer.readString());

                // Lookup the title from a Text definition? (ZDoom)
                if(!title.compareWithoutCase("lookup"))
                {
                    title = Str_Text(lexer.readString());
                }
                info->set("title", title);
            }

            if(sourceIsCustom) info->set("custom", true);

            // Process optional tokens.
            while(lexer.readToken())
            {
                if(!Str_CompareIgnoreCase(lexer.token(), "allowcrouch")) // ZDoom
                {
                    reportProblem("MAPINFO Map.allowCrouch is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "allowjump")) // ZDoom
                {
                    reportProblem("MAPINFO Map.allowJump is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "allowmonstertelefrags")) // ZDoom
                {
                    reportProblem("MAPINFO Map.allowMonsterTelefrags is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "allowrespawn")) // ZDoom
                {
                    reportProblem("MAPINFO Map.allowRespawn is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "aircontrol")) // ZDoom
                {
                    reportProblem("MAPINFO Map.airControl is not supported.");
                    lexer.readNumber();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "airsupply")) // ZDoom
                {
                    reportProblem("MAPINFO Map.airSupply is not supported.");
                    lexer.readNumber();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "autosequences")) // ZDoom
                {
                    reportProblem("MAPINFO Map.autosequences is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "baronspecial")) // ZDoom
                {
                    reportProblem("MAPINFO Map.baronSpecial is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "bordertexture")) // ZDoom
                {
                    reportProblem("MAPINFO Map.borderTexture is not supported.");
                    lexer.readString();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "cdid")) // ZDoom
                {
                    reportProblem("MAPINFO Map.cdid is not supported.");
                    lexer.readString();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "cdtrack"))
                {
                    info->set("cdTrack", (int)lexer.readNumber());
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "checkswitchrange")) // ZDoom
                {
                    reportProblem("MAPINFO Map.checkSwitchRange is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "clipmidtextures")) // ZDoom
                {
                    reportProblem("MAPINFO Map.clipMidtextures is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "cluster"))
                {
                    const int hubNum = (int)lexer.readNumber();
                    if(hubNum < 1)
                    {
                        throw ParseError(stringf("Invalid 'cluster' (i.e., hub) number '%s' on line #%i",
                                                 Str_Text(lexer.token()), lexer.lineNumber()));
                    }
                    info->set("hub", hubNum);
                    continue;
                }
                if(String(Str_Text(lexer.token())).beginsWith("compat_", CaseInsensitive)) // ZDoom
                {
                    reportProblem(String::format("MAPINFO Map.%s is not supported.", Str_Text(lexer.token())));
                    lexer.readNumber();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "cyberdemonspecial")) // ZDoom
                {
                    reportProblem("MAPINFO Map.cyberdemonSpecial is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "doublesky"))
                {
                    info->set("doubleSky", true);
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "enterpic")) // ZDoom
                {
                    reportProblem("MAPINFO Map.enterPic is not supported.");
                    lexer.readString();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "evenlighting")) // ZDoom
                {
                    reportProblem("MAPINFO Map.evenlighting is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "exitpic")) // ZDoom
                {
                    reportProblem("MAPINFO Map.exitPic is not supported.");
                    lexer.readString();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "f1")) // ZDoom
                {
                    reportProblem("MAPINFO Map.f1 is not supported.");
                    lexer.readString();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "fadetable"))
                {
                    info->set("fadeTable", Str_Text(lexer.readString()));
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "fade")) // ZDoom
                {
                    reportProblem("MAPINFO Map.fade is not supported.");
                    lexer.readString();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "fallingdamage")) // ZDoom
                {
                    reportProblem("MAPINFO Map.fallingdamage is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "filterstarts")) // ZDoom
                {
                    reportProblem("MAPINFO Map.filterStarts is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "forceFallingDamage")) // ZDoom
                {
                    reportProblem("MAPINFO Map.forceFallingDamage is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "forceNoSkyStretch")) // ZDoom
                {
                    reportProblem("MAPINFO Map.forceNoSkyStretch is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "gravity")) // ZDoom
                {
                    reportProblem("MAPINFO Map.gravity is not supported.");
                    lexer.readNumber();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "horizwallshade")) // ZDoom
                {
                    reportProblem("MAPINFO Map.horizwallShade is not supported.");
                    lexer.readNumber();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "infiniteflightpowerup")) // ZDoom
                {
                    reportProblem("MAPINFO Map.infiniteFlightPowerup is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "intermusic")) // ZDoom
                {
                    reportProblem("MAPINFO Map.interMusic is not supported.");
                    lexer.readString();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "keepfullinventory")) // ZDoom
                {
                    reportProblem("MAPINFO Map.keepFullInventory is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "laxmonsteractivation")) // ZDoom
                {
                    reportProblem("MAPINFO Map.laxMonsterActivation is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "lightning"))
                {
                    info->set("lightning", true);
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "map07special")) // ZDoom
                {
                    reportProblem("MAPINFO Map.map07Special is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "monsterfallingdamage")) // ZDoom
                {
                    reportProblem("MAPINFO Map.monsterFallingDamage is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "missilesactivateimpactlines")) // ZDoom
                {
                    reportProblem("MAPINFO Map.missilesActivateImpactLines is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "missileshootersactivateimpactlines")) // ZDoom
                {
                    reportProblem("MAPINFO Map.missileshootersActivateImpactLines is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "music")) // ZDoom
                {
                    info->set("music", Str_Text(lexer.readString()));
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "next"))
                {
                    parseMapNext(*info);
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "noautosequences")) // ZDoom
                {
                    reportProblem("MAPINFO Map.noAutoSequences is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "nocheckswitchrange")) // ZDoom
                {
                    reportProblem("MAPINFO Map.noCheckSwitchRange is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "nocrouch")) // ZDoom
                {
                    reportProblem("MAPINFO Map.noCrouch is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "nofallingdamage")) // ZDoom
                {
                    reportProblem("MAPINFO Map.noFallingDamage is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "noinfighting")) // ZDoom
                {
                    reportProblem("MAPINFO Map.noInfighting is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "nointermission")) // ZDoom
                {
                    info->set("nointermission", true);
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "noinventorybar")) // ZDoom
                {
                    reportProblem("MAPINFO Map.noInventorybar is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "nojump")) // ZDoom
                {
                    reportProblem("MAPINFO Map.noJump is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "normalinfighting")) // ZDoom
                {
                    reportProblem("MAPINFO Map.normalInfighting is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "nosoundclipping")) // ZDoom
                {
                    reportProblem("MAPINFO Map.noSoundClipping is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "oldfallingdamage")) // ZDoom
                {
                    reportProblem("MAPINFO Map.oldFallingDamage is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "outsidefog")) // ZDoom
                {
                    reportProblem("MAPINFO Map.outsideFog is not supported.");
                    lexer.readString();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "par")) // ZDoom
                {
                    info->set("par", lexer.readNumber());
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "secretnext")) // ZDoom
                {
                    parseMapNext(*info, true/*is-secret*/);
                    continue;
                }
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
                if(!Str_CompareIgnoreCase(lexer.token(), "skystretch")) // ZDoom
                {
                    reportProblem("MAPINFO Map.skyStretch is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "specialaction_exitlevel")) // ZDoom
                {
                    reportProblem("MAPINFO Map.specialaction_exitlevel is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "specialaction_killmonsters")) // ZDoom
                {
                    reportProblem("MAPINFO Map.specialaction_killmonsters is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "specialaction_lowerfloor")) // ZDoom
                {
                    reportProblem("MAPINFO Map.specialaction_lowerfloor is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "specialaction_opendoor")) // ZDoom
                {
                    reportProblem("MAPINFO Map.specialaction_opendoor is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "spidermastermindspecial")) // ZDoom
                {
                    reportProblem("MAPINFO Map.spidermastermindSpecial is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "smoothlighting")) // ZDoom
                {
                    reportProblem("MAPINFO Map.smoothlighting is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "strictmonsteractivation")) // ZDoom
                {
                    reportProblem("MAPINFO Map.strictMonsterActivation is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "strifefallingdamage")) // ZDoom
                {
                    reportProblem("MAPINFO Map.strifeFallingDamage is not supported.");
                    continue;
                }
                if (!Str_CompareIgnoreCase(lexer.token(), "sucktime")) // ZDoom?
                {
                    reportProblem("MAPINFO Map.suckTime is not supported.");
                    lexer.readNumber();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "teamdamage")) // ZDoom
                {
                    reportProblem("MAPINFO Map.teamDamage is not supported.");
                    lexer.readNumber();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "teamplayoff")) // ZDoom
                {
                    reportProblem("MAPINFO Map.teamplayOff is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "teamplayon")) // ZDoom
                {
                    reportProblem("MAPINFO Map.teamplayOn is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "titlepatch")) // ZDoom
                {
                    info->set("titleImage", lexer.readUri("Patches").compose());
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "totalinfighting")) // ZDoom
                {
                    reportProblem("MAPINFO Map.totalInfighting is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "translator")) // ZDoom
                {
                    reportProblem("MAPINFO Map.translator is not supported.");
                    lexer.readString();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "unfreezesingleplayerconversations")) // ZDoom
                {
                    reportProblem("MAPINFO Map.unfreezeSingleplayerConversations is not supported.");
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "vertwallshade")) // ZDoom
                {
                    reportProblem("MAPINFO Map.vertwallShade is not supported.");
                    lexer.readNumber();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "warptrans") ||
                   !Str_CompareIgnoreCase(lexer.token(), "levelnum") /* ZDoom */)
                {
                    info->set("warpTrans", (int)lexer.readNumber());
                    continue;
                }

                lexer.unreadToken();
                break;
            }
        }

        void parseSkill() // ZDoom
        {
            reportProblem("MAPINFO Skill definitions are not supported.");

            /*const ddstring_s *id =*/ lexer.readString();

            // Process optional tokens.
            while(lexer.readToken())
            {
                if(!Str_CompareIgnoreCase(lexer.token(), "acsreturn"))
                {
                    lexer.readNumber();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "aggressiveness"))
                {
                    lexer.readNumber();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "ammofactor"))
                {
                    lexer.readNumber();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "autousehealth"))
                {
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "damagefactor"))
                {
                    lexer.readNumber();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "disablecheats"))
                {
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "doubleammofactor"))
                {
                    lexer.readNumber();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "dropammofactor"))
                {
                    lexer.readNumber();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "easybossbrain"))
                {
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "fastmonsters"))
                {
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "key"))
                {
                    lexer.readString();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "mustconfirm"))
                {
                    lexer.readString(); // Optional?
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "name"))
                {
                    lexer.readString();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "picname"))
                {
                    lexer.readString();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "playerclassname"))
                {
                    lexer.readString();
                    lexer.readString();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "respawnlimit"))
                {
                    lexer.readNumber();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "respawntime"))
                {
                    lexer.readNumber();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "spawnfilter"))
                {
                    lexer.readString();
                    continue;
                }
                if(!Str_CompareIgnoreCase(lexer.token(), "textcolor"))
                {
                    lexer.readString();
                    continue;
                }

                lexer.unreadToken();
                break;
            }
        }
    };

} // namespace internal

using namespace internal;

DE_PIMPL_NOREF(MapInfoTranslator)
{
    HexDefs defs;
    StringList translatedFiles;

    typedef KeyMap<int, List<const MapInfo *>> MapInfos;

    MapInfos buildHubMapInfoTable(const String &episodeId)
    {
        const bool hubNumberIsEpisodeId = interpretHubNumberAsEpisodeId();

        MapInfos set;
        for (HexDefs::MapInfos::const_iterator it = defs.mapInfos.begin();
             it != defs.mapInfos.end();
             ++it)
        {
            const MapInfo &mapInfo = it->second;

            int hub = mapInfo.geti("hub");
            if (hubNumberIsEpisodeId)
            {
                if (String::asText(hub) != episodeId) continue;

                /// @todo Once hubs are supported in DOOM and Heretic, whether or not this
                /// map should be grouped into a DED Episode.Hub definition is determined
                /// by whether or not the ZDoom ClusterDef.hub property is true.
                hub = 0;
            }

            set[hub] << &mapInfo;
        }
        return set;
    }

    res::Uri xlatWarpNumber(uint map)
    {
        res::Uri matchedWithoutHub("Maps:", RC_NULL);

        for (HexDefs::MapInfos::iterator i = defs.mapInfos.begin(); i != defs.mapInfos.end(); ++i)
        {
            const MapInfo &info = i->second;

            if (info.getui("warpTrans") == map)
            {
                if (info.geti("hub"))
                {
                    LOGDEV_MAP_VERBOSE("Warp %u translated to map %s, hub %i")
                            << map << info.gets("id") << info.geti("hub");
                    return res::makeUri(info.gets("id"));
                }

                LOGDEV_MAP_VERBOSE("Warp %u matches map %s, but it has no hub")
                        << map << info.gets("id");
                matchedWithoutHub = res::makeUri(info.gets("id"));
            }
        }

        LOGDEV_MAP_NOTE("Could not find warp %i, translating to map %s (without hub)")
                << map << matchedWithoutHub;

        return matchedWithoutHub;
    }

    /**
     * To be called once all definitions have been parsed to translate Hexen's
     * map "warp numbers" to URIs where used as map definition references.
     */
    void translateWarpNumbers()
    {
        for (HexDefs::EpisodeInfos::iterator i = defs.episodeInfos.begin();
             i != defs.episodeInfos.end();
             ++i)
        {
            EpisodeInfo &info = i->second;
            res::Uri     startMap(info.gets("startMap", ""), RC_NULL);
            if (!startMap.scheme().compareWithoutCase("@wt"))
            {
                info.set("startMap", xlatWarpNumber(startMap.path().toString().toInt()).compose());
            }
        }
        for (HexDefs::MapInfos::iterator i = defs.mapInfos.begin(); i != defs.mapInfos.end(); ++i)
        {
            MapInfo &info = i->second;
            res::Uri nextMap(info.gets("nextMap", ""), RC_NULL);
            if (!nextMap.scheme().compareWithoutCase("@wt"))
            {
                info.set("nextMap", xlatWarpNumber(nextMap.path().toString().toInt()).compose());
            }
            res::Uri secretNextMap(info.gets("secretNextMap", ""), RC_NULL);
            if (!secretNextMap.scheme().compareWithoutCase("@wt"))
            {
                info.set("secretNextMap",
                         xlatWarpNumber(secretNextMap.path().toString().toInt()).compose());
            }
        }
    }

    void preprocess()
    {
        // Warp numbers may be used as internal map references (doh!)
        translateWarpNumbers();

/*#ifdef DE_IMPORTIDTECH1_DEBUG
        for(HexDefs::MapInfos::const_iterator i = defs.mapInfos.begin(); i != defs.mapInfos.end(); ++i)
        {
            const MapInfo &info = i->second;
            LOG_RES_MSG("MAPINFO %s { title: \"%s\" hub: %i map: %s warp: %i nextMap: %s }")
                    << i->first.c_str() << info.gets("title")
                    << info.geti("hub") << info.gets("id") << info.geti("warpTrans") << info.gets("nextMap");
        }
#endif*/
    }

    void translate(String &output, bool custom)
    {
        std::ostringstream os;

        os << "# Translated definitions from:";
        // List the files we translated in input order (for debug).
        for(int i = 0; i < translatedFiles.sizei(); ++i)
        {
            String sourceFile = translatedFiles[i];
            os << "\n# " + Stringf("%i: %s", i, NativePath(sourceFile).pretty().c_str());
        }

        // Output the header block.
        os << "\n\nHeader { Version = 6; }";

        // Output episode defs.
        for (const auto &pair : defs.episodeInfos)
        {
            const String episodeId  = pair.first;
            const EpisodeInfo &info = pair.second;

            res::Uri startMapUri(info.gets("startMap"), RC_NULL);
            if(startMapUri.path().isEmpty()) continue;

            // Find all the hubs for this episode.
            MapInfos mapInfos = buildHubMapInfoTable(episodeId);

            bool episodeIsCustom = info.getb("custom");
            // If one of the maps is custom then so too is the episode.
            if (!episodeIsCustom)
            {
                for (const auto &mapHub : mapInfos)
                {
                    for (const auto *mapInfo : mapHub.second)
                    {
                        if (mapInfo->getb("custom"))
                        {
                            episodeIsCustom = true;
                            break;
                        }
                    }
                }
            }
            if(custom != episodeIsCustom) continue;

            os << "\n\nEpisode {"
               << "\n  ID = \"" + episodeId + "\";"
               << "\n  Title = \"" + info.gets("title") + "\";"
               << "\n  Start Map = \"" + toMapId(startMapUri) + "\";";
            String menuHelpInfo = info.gets("menuHelpInfo");
            if(!menuHelpInfo.isEmpty())
            {
                os << "\n  Menu Help Info = \"" + menuHelpInfo + "\";";
            }
            res::Uri menuImageUri(info.gets("menuImage"), RC_NULL);
            if(!menuImageUri.path().isEmpty())
            {
                os << "\n  Menu Image = \"" + menuImageUri.compose() + "\";";
            }
            String menuShortcut = info.gets("menuShortcut");
            if(!menuShortcut.isEmpty())
            {
                os << "\n  Menu Shortcut = \"" + menuShortcut + "\";";
            }

//            List<int> hubs = mapInfos.uniqueKeys();
            for (const auto &mapHub : mapInfos)
            {
                const int   hub            = mapHub.first;
                const auto &mapInfosForHub = mapHub.second;

                if(mapInfosForHub.isEmpty()) continue;

                // Extra whitespace between hubs, for neatness.
                os << "\n";

                // #0 is not actually a hub.
                if(hub != 0)
                {
                    // Begin the hub definition.
                    os << "\n  Hub {"
                       << "\n    ID = \"" + String::asText(hub) + "\";";
                }

                // Output each map for this hub (in reverse insertion order).
                dsize n = mapInfosForHub.size();
                while (n-- > 0)
                {
                    const MapInfo *mapInfo = mapInfosForHub.at(n);
                    res::Uri       mapUri(mapInfo->gets("id"), RC_NULL);

                    if (!mapUri.path().isEmpty())
                    {
                        os << "\n    Map {"
                           << "\n      ID = \"" + toMapId(mapUri) + "\";";
                        res::Uri nextMapUri(mapInfo->gets("nextMap"), RC_NULL);
                        if (!nextMapUri.path().isEmpty())
                        {
                            os << "\n      Exit { ID = \"next\"; Target Map = \"" +
                                      toMapId(nextMapUri) + "\"; }";
                        }
                        res::Uri secretNextMapUri(mapInfo->gets("secretNextMap"), RC_NULL);
                        if (!secretNextMapUri.path().isEmpty())
                        {
                            os << "\n      Exit { ID = \"secret\"; Target Map = \"" +
                                      toMapId(secretNextMapUri) + "\"; }";
                        }
                        os << "\n      Warp Number = " +
                                  String::asText(mapInfo->geti("warpTrans")) + ";";
                        os << "\n    }";
                    }
                }

                // #0 is not actually a hub.
                if(hub != 0)
                {
                    // End the hub definition.
                    os << "\n  }";
                }
            }
            os << "\n} # Episode '" << episodeId << "'";
        }

        GameInfo gameInfo;
        DD_GameInfo(&gameInfo);

        // Output mapinfo defs.
        for (const auto &pair : defs.mapInfos)
        {
            const MapInfo &info = pair.second;
            res::Uri mapUri(info.gets("id"), RC_NULL);

            const bool isCustomMapInfo = info.getb("custom");

            if (custom != isCustomMapInfo) continue;

            if (mapUri.path().isEmpty()) continue;

            const String mapId         = toMapId(mapUri);
            const String musicId       = mapId + "_dd_xlt"; // doomsday translated
            const String musicLumpName = info.gets("music");
            bool         addedMusicDef = false;

            if (isCustomMapInfo && (!musicLumpName.isEmpty() || info.geti("cdTrack")))
            {
                addedMusicDef = true;

                // Add a music def for this custom music.
                os << "\n\nMusic {"
                   << "\n  ID = \"" + musicId + "\";";
                if (!musicLumpName.isEmpty())
                {
                   os << "\n  Lump = \"" + musicLumpName + "\";";
                }
                os << "\n  CD Track = " << info.geti("cdTrack") << ";"
                   << "\n}";
            }

            const bool doubleSky = info.getb("doubleSky");

            os << "\n\nMap Info {"
               << "\n  ID = \"" + mapId + "\";"
               << "\n  Title = \"" + info.gets("title") + "\";";
            if (!isCustomMapInfo)
            {
               os << "\n  Author = \"" + String(Str_Text(gameInfo.author)) + "\";";
            }
            os << "\n  Fade Table = \"" + info.gets("fadeTable") + "\";";
            if (addedMusicDef)
            {
                os << "\n  Music = \"" + musicId + "\";";
            }
            res::Uri titleImageUri(info.gets("titleImage"), RC_NULL);
            if(!titleImageUri.path().isEmpty())
            {
                os << "\n  Title image = \"" + titleImageUri.compose() + "\";";
            }
            dfloat parTime = info.getf("par");
            if(parTime > 0)
            {
                os << "\n  Par time = " + String::asText(parTime) + ";";
            }
            StringList allFlags;
            if(info.getb("lightning"))      allFlags << "lightning";
            if(info.getb("nointermission")) allFlags << "nointermission";
            if(!allFlags.isEmpty())
            {
                os << "\n  Flags = " + String::join(allFlags, " | ") + ";";
            }
            res::Uri skyLayer1MaterialUri(info.gets(doubleSky? "sky2Material" : "sky1Material"), RC_NULL);
            if(!skyLayer1MaterialUri.path().isEmpty())
            {
                os << "\n  Sky Layer 1 {"
                   << "\n    Flags = enable;"
                   << "\n    Material = \"" + skyLayer1MaterialUri.compose() + "\";";
                dfloat scrollDelta = info.getf(doubleSky? "sky2ScrollDelta" : "sky1ScrollDelta") * 35 /*TICSPERSEC*/;
                if(!de::fequal(scrollDelta, 0))
                {
                    os << "\n    Offset Speed = " + String::asText(scrollDelta) + ";";
                }
                os << "\n  }";
            }
            res::Uri skyLayer2MaterialUri(info.gets(doubleSky? "sky1Material" : "sky2Material"), RC_NULL);
            if(!skyLayer2MaterialUri.path().isEmpty())
            {
                os << "\n  Sky Layer 2 {";
                if(doubleSky)
                {
                    os << "\n    Flags = enable | mask;";
                }
                os << "\n    Material = \"" + skyLayer2MaterialUri.compose() + "\";";
                dfloat scrollDelta = info.getf(doubleSky? "sky1ScrollDelta" : "sky2ScrollDelta") * 35 /*TICSPERSEC*/;
                if(!de::fequal(scrollDelta, 0))
                {
                    os << "\n    Offset Speed = " + String::asText(scrollDelta) + ";";
                }
                os << "\n  }";
            }
            os << "\n}";
        }

        // Output music modification defs for the non-map musics.
        for (const auto &pair : defs.musics)
        {
            const Music &music = pair.second;
            if(custom != music.getb("custom")) continue;

            os << "\n\nMusic Mods \"" + music.gets("id") + "\" {"
               << "\n  CD Track = " + String::asText(music.geti("cdTrack")) + ";"
               << "\n}";
        }

        output = os.str();
    }
};

MapInfoTranslator::MapInfoTranslator() : d(new Impl)
{}

void MapInfoTranslator::reset()
{
    d->defs.clear();
    d->translatedFiles.clear();
}

void MapInfoTranslator::merge(const ddstring_s &definitions,
                              const String &    sourcePath,
                              bool              sourceIsCustom)
{
    LOG_AS("MapInfoTranslator");

    if (Str_IsEmpty(&definitions)) return;

    const String source = sourcePath.isEmpty() ? "[definition-data]"
                                              : ("\"" + NativePath(sourcePath).pretty() + "\"");
    try
    {
        if (!sourcePath.isEmpty())
        {
            LOG_RES_VERBOSE("Parsing %s...") << source;
            d->translatedFiles << sourcePath;
        }

        MapInfoParser parser(d->defs);
        parser.tryParse(definitions, sourcePath, sourceIsCustom);
    }
    catch (const MapInfoParser::ParseError &er)
    {
        LOG_MAP_WARNING("Failed to parse %s as MAPINFO:\n") << source << er.asText();
    }
}

void MapInfoTranslator::translate(String &translated, String &translatedCustom)
{
    LOG_AS("MapInfoTranslator");

    // Perform necessary preprocessing (must be done before translation).
    d->preprocess();
    d->translate(translated, false/*not custom*/);
    d->translate(translatedCustom, true/*custom*/);
    
    debug("translated:%s", translated.c_str());

    reset(); // The definition database was modified.
}

} // namespace idtech1
