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
#include <QMultiMap>

#include <doomsday/Game>
#include <doomsday/doomsdayapp.h>
#include <de/App>
#include <de/Error>
#include <de/Record>
#include "hexlex.h"

using namespace de;

namespace idtech1 {
namespace internal {

    static inline String defaultSkyMaterial()
    {
        String const gameIdKey = DoomsdayApp::game().id();
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
        String const gameIdKey = DoomsdayApp::game().id();
        return (gameIdKey.beginsWith("doom1") || gameIdKey.beginsWith("heretic") ||
                gameIdKey.beginsWith("chex"));
    }

    static inline String toMapId(de::Uri const &mapUri)
    {
        return mapUri.scheme().compareWithoutCase("Maps") ? mapUri.compose() : mapUri.path().toString();
    }

    class Music : public de::Record
    {
    public:
        Music() : Record() {
            resetToDefaults();
        }
        Music &operator = (Music const &other) {
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
        MapInfo &operator = (MapInfo const &other) {
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
        EpisodeInfo &operator = (EpisodeInfo const &other) {
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
    struct HexDefs
    {
        typedef std::map<std::string, Music> Musics;
        Musics musics;
        typedef std::map<std::string, EpisodeInfo> EpisodeInfos;
        EpisodeInfos episodeInfos;
        typedef std::map<std::string, MapInfo> MapInfos;
        MapInfos mapInfos;

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
                Musics::iterator found = musics.find(id.toLower().toStdString());
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
                EpisodeInfos::iterator found = episodeInfos.find(id.toLower().toStdString());
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
        MapInfo *getMapInfo(de::Uri const &mapUri)
        {
            if(!mapUri.scheme().compareWithoutCase("Maps"))
            {
                MapInfos::iterator found = mapInfos.find(mapUri.path().toString().toLower().toStdString());
                if(found != mapInfos.end())
                {
                    return &found->second;
                }
            }
            return 0; // Not found.
        }
    };

    static de::Uri composeMapUri(uint episode, uint map)
    {
        String const gameIdKey = DoomsdayApp::game().id();
        if(gameIdKey.beginsWith("doom1") || gameIdKey.beginsWith("heretic"))
        {
            return de::makeUri(String("Maps:E%1M%2").arg(episode+1).arg(map+1));
        }
        return de::makeUri(String("Maps:MAP%1").arg(map+1, 2, 10, QChar('0')));
    }

    static uint mapWarpNumberFor(de::Uri const &mapUri)
    {
        String path = mapUri.path();
        if(!path.isEmpty())
        {
            if(path.at(0).toLower() == 'e' && path.at(2).toLower() == 'm')
            {
                return de::max(path.substr(3).toInt(0, 10, String::AllowSuffix), 1);
            }
            if(path.beginsWith("map", String::CaseInsensitive))
            {
                return de::max(path.substr(3).toInt(0, 10, String::AllowSuffix), 1);
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
        DENG2_ERROR(ParseError);

        /// Mappings from symbolic song name to music id.
        typedef QMap<String, String> MusicMappings;
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

        void parse(AutoStr const &buffer, String /*sourceFile*/, bool sourceIsCustom)
        {
            LOG_AS("MapInfoParser");

            // Nothing to parse?
            if(Str_IsEmpty(&buffer))
                return;

            this->sourceIsCustom = sourceIsCustom;

            // May opt out of error reporting.
            {
                const String bufText(Str_Text(&buffer));
                if (bufText.contains("// Doomsday: Ignore errors!", Qt::CaseInsensitive))
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
                if(tok.beginsWith("cd_", String::CaseInsensitive) &&
                   tok.endsWith("_track", String::CaseInsensitive))
                {
                    String const pubName = tok.substr(3, tok.length() - 6 - 3);
                    MusicMappings::const_iterator found = musicMap.constFind(pubName);
                    if(found != musicMap.end())
                    {
                        // Lookup an existing music from the database.
                        String const songId = found.value();
                        Music *music = db.getMusic(songId);
                        if(!music)
                        {
                            // A new music.
                            music = &db.musics[songId.toUtf8().constData()];
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
                throw ParseError(String("Unexpected token '%1' on line #%2").arg(Str_Text(lexer.token())).arg(lexer.lineNumber()));
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

            /*int const clusterId = (int)*/lexer.readNumber();

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
            de::Uri mapUri(Str_Text(lexer.readString()), RC_NULL);
            if(mapUri.scheme().isEmpty()) mapUri.setScheme("Maps");

            // A new episode info.
            String const id = String::number(db.episodeInfos.size() + 1);
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
            if(Str_CompareIgnoreCase(lexer.token(), "{"))
                throw ParseError(String("Expected '{' but found '%1' on line #%2").arg(Str_Text(lexer.token())).arg(lexer.lineNumber()));

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
            ddstring_s const *tok = lexer.readString();

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

            de::Uri mapUri;
            bool isNumber;
            int mapNumber = String(Str_Text(tok)).toInt(&isNumber); // 1-based
            if(!isNumber)
            {
                mapUri = de::makeUri(Str_Text(tok));
                if(mapUri.scheme().isEmpty()) mapUri.setScheme("Maps");
                mapInfo.set((isSecret? "secretNextMap" : "nextMap"), mapUri.compose());
            }
            else
            {
                mapInfo.set((isSecret? "secretNextMap" : "nextMap"), String("@wt:%1").arg(mapNumber));
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
            de::Uri mapUri;
            if(!info)
            {
                String const mapRef = String(Str_Text(lexer.readString()));

                bool isNumber;
                int mapNumber = mapRef.toInt(&isNumber); // 1-based
                if(!isNumber)
                {
                    mapUri = de::makeUri(mapRef);
                    if(mapUri.scheme().isEmpty()) mapUri.setScheme("Maps");
                }
                else
                {
                    if(mapNumber < 1)
                    {
                        throw ParseError(String("Invalid map number '%1' on line #%2").arg(mapNumber).arg(lexer.lineNumber()));
                    }
                    mapUri = composeMapUri(0, mapNumber - 1);
                }

                // Lookup an existing map info from the database.
                info = db.getMapInfo(mapUri);

                if(!info)
                {
                    // A new map info.
                    info = &db.mapInfos[mapUri.path().asText().toLower().toStdString()];

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
                    int const hubNum = (int)lexer.readNumber();
                    if(hubNum < 1)
                    {
                        throw ParseError(String("Invalid 'cluster' (i.e., hub) number '%1' on line #%2").arg(Str_Text(lexer.token())).arg(lexer.lineNumber()));
                    }
                    info->set("hub", hubNum);
                    continue;
                }
                if(String(Str_Text(lexer.token())).beginsWith("compat_", String::CaseInsensitive)) // ZDoom
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

            /*ddstring_s const *id =*/ lexer.readString();

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

DENG2_PIMPL_NOREF(MapInfoTranslator)
{
    HexDefs defs;
    StringList translatedFiles;

    typedef QMultiMap<int, MapInfo *> MapInfos;
    MapInfos buildHubMapInfoTable(String episodeId)
    {
        bool const hubNumberIsEpisodeId = interpretHubNumberAsEpisodeId();

        MapInfos set;
        for(HexDefs::MapInfos::const_iterator it = defs.mapInfos.begin(); it != defs.mapInfos.end(); ++it)
        {
            MapInfo const &mapInfo = it->second;

            int hub = mapInfo.geti("hub");
            if(hubNumberIsEpisodeId)
            {
                if(String::number(hub) != episodeId)
                    continue;

                /// @todo Once hubs are supported in DOOM and Heretic, whether or not this
                /// map should be grouped into a DED Episode.Hub definition is determined
                /// by whether or not the ZDoom ClusterDef.hub property is true.
                hub = 0;
            }

            set.insert(hub, const_cast<MapInfo *>(&mapInfo));
        }

        return set;
    }

    de::Uri xlatWarpNumber(uint map)
    {
        de::Uri matchedWithoutHub("Maps:", RC_NULL);

        for(HexDefs::MapInfos::iterator i = defs.mapInfos.begin(); i != defs.mapInfos.end(); ++i)
        {
            MapInfo const &info = i->second;

            if((unsigned)info.geti("warpTrans") == map)
            {
                if(info.geti("hub"))
                {
                    LOGDEV_MAP_VERBOSE("Warp %u translated to map %s, hub %i")
                            << map << info.gets("id") << info.geti("hub");
                    return de::makeUri(info.gets("id"));
                }

                LOGDEV_MAP_VERBOSE("Warp %u matches map %s, but it has no hub")
                        << map << info.gets("id");
                matchedWithoutHub = de::makeUri(info.gets("id"));
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
        for(HexDefs::EpisodeInfos::iterator i = defs.episodeInfos.begin(); i != defs.episodeInfos.end(); ++i)
        {
            EpisodeInfo &info = i->second;
            de::Uri startMap(info.gets("startMap", ""), RC_NULL);
            if(!startMap.scheme().compareWithoutCase("@wt"))
            {
                info.set("startMap", xlatWarpNumber(startMap.path().toStringRef().toInt()).compose());
            }
        }
        for(HexDefs::MapInfos::iterator i = defs.mapInfos.begin(); i != defs.mapInfos.end(); ++i)
        {
            MapInfo &info = i->second;
            de::Uri nextMap(info.gets("nextMap", ""), RC_NULL);
            if(!nextMap.scheme().compareWithoutCase("@wt"))
            {
                info.set("nextMap", xlatWarpNumber(nextMap.path().toStringRef().toInt()).compose());
            }
            de::Uri secretNextMap(info.gets("secretNextMap", ""), RC_NULL);
            if(!secretNextMap.scheme().compareWithoutCase("@wt"))
            {
                info.set("secretNextMap", xlatWarpNumber(secretNextMap.path().toStringRef().toInt()).compose());
            }
        }
    }

    void preprocess()
    {
        // Warp numbers may be used as internal map references (doh!)
        translateWarpNumbers();

/*#ifdef DENG_IMPORTIDTECH1_DEBUG
        for(HexDefs::MapInfos::const_iterator i = defs.mapInfos.begin(); i != defs.mapInfos.end(); ++i)
        {
            MapInfo const &info = i->second;
            LOG_RES_MSG("MAPINFO %s { title: \"%s\" hub: %i map: %s warp: %i nextMap: %s }")
                    << i->first.c_str() << info.gets("title")
                    << info.geti("hub") << info.gets("id") << info.geti("warpTrans") << info.gets("nextMap");
        }
#endif*/
    }

    void translate(String &output, bool custom)
    {
        QTextStream os(&output);

        os << "# Translated definitions from:";
        // List the files we translated in input order (for debug).
        for(int i = 0; i < translatedFiles.size(); ++i)
        {
            String sourceFile = translatedFiles[i];
            os << "\n# " + QString("%1: %2").arg(i).arg(NativePath(sourceFile).pretty());
        }

        // Output the header block.
        os << "\n\nHeader { Version = 6; }";

        // Output episode defs.
        for(auto const pair : defs.episodeInfos)
        {
            String const episodeId  = String::fromStdString(pair.first);
            EpisodeInfo const &info = pair.second;

            de::Uri startMapUri(info.gets("startMap"), RC_NULL);
            if(startMapUri.path().isEmpty()) continue;

            // Find all the hubs for this episode.
            MapInfos mapInfos = buildHubMapInfoTable(episodeId);

            bool episodeIsCustom = info.getb("custom");
            // If one of the maps is custom then so too is the episode.
            if(!episodeIsCustom)
            {
                for(MapInfo const *mapInfo : mapInfos)
                {
                    if(mapInfo->getb("custom"))
                    {
                        episodeIsCustom = true;
                        break;
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
            de::Uri menuImageUri(info.gets("menuImage"), RC_NULL);
            if(!menuImageUri.path().isEmpty())
            {
                os << "\n  Menu Image = \"" + menuImageUri.compose() + "\";";
            }
            String menuShortcut = info.gets("menuShortcut");
            if(!menuShortcut.isEmpty())
            {
                os << "\n  Menu Shortcut = \"" + menuShortcut + "\";";
            }

            QList<int> hubs = mapInfos.uniqueKeys();
            for(int hub : hubs)
            {
                QList<MapInfo *> const mapInfosForHub = mapInfos.values(hub);
                if(mapInfosForHub.isEmpty()) continue;

                // Extra whitespace between hubs, for neatness.
                os << "\n";

                // #0 is not actually a hub.
                if(hub != 0)
                {
                    // Begin the hub definition.
                    os << "\n  Hub {"
                       << "\n    ID = \"" + String::number(hub) + "\";";
                }

                // Output each map for this hub (in reverse insertion order).
                int n = mapInfosForHub.size();
                while(n-- > 0)
                {
                    MapInfo const *mapInfo = mapInfosForHub.at(n);
                    de::Uri mapUri(mapInfo->gets("id"), RC_NULL);
                    if(!mapUri.path().isEmpty())
                    {
                        os << "\n    Map {"
                           << "\n      ID = \"" + toMapId(mapUri) + "\";";
                        de::Uri nextMapUri(mapInfo->gets("nextMap"), RC_NULL);
                        if(!nextMapUri.path().isEmpty())
                        {
                            os << "\n      Exit { ID = \"next\"; Target Map = \"" + toMapId(nextMapUri) + "\"; }";
                        }
                        de::Uri secretNextMapUri(mapInfo->gets("secretNextMap"), RC_NULL);
                        if(!secretNextMapUri.path().isEmpty())
                        {
                            os << "\n      Exit { ID = \"secret\"; Target Map = \"" + toMapId(secretNextMapUri) + "\"; }";
                        }
                        os << "\n      Warp Number = " + String::number(mapInfo->geti("warpTrans")) + ";";
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
        for(auto const pair : defs.mapInfos)
        {
            MapInfo const &info = pair.second;

            const bool isCustomMapInfo = info.getb("custom");

            if(custom != isCustomMapInfo) continue;

            de::Uri mapUri(info.gets("id"), RC_NULL);
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
            de::Uri titleImageUri(info.gets("titleImage"), RC_NULL);
            if(!titleImageUri.path().isEmpty())
            {
                os << "\n  Title image = \"" + titleImageUri.compose() + "\";";
            }
            dfloat parTime = info.getf("par");
            if(parTime > 0)
            {
                os << "\n  Par time = " + String::number(parTime) + ";";
            }
            QStringList allFlags;
            if(info.getb("lightning"))      allFlags << "lightning";
            if(info.getb("nointermission")) allFlags << "nointermission";
            if(!allFlags.isEmpty())
            {
                os << "\n  Flags = " + allFlags.join(" | ") + ";";
            }
            if (DoomsdayApp::game().id().beginsWith("hexen"))
            {
                os << "\n  Sky height = 0.75;\n";
            }
            de::Uri skyLayer1MaterialUri(info.gets(doubleSky? "sky2Material" : "sky1Material"), RC_NULL);
            if(!skyLayer1MaterialUri.path().isEmpty())
            {
                os << "\n  Sky Layer 1 {"
                   << "\n    Flags = enable;"
                   << "\n    Material = \"" + skyLayer1MaterialUri.compose() + "\";";
                dfloat scrollDelta = info.getf(doubleSky? "sky2ScrollDelta" : "sky1ScrollDelta") * 35 /*TICSPERSEC*/;
                if(!de::fequal(scrollDelta, 0))
                {
                    os << "\n    Offset Speed = " + String::number(scrollDelta) + ";";
                }
                os << "\n  }";
            }
            de::Uri skyLayer2MaterialUri(info.gets(doubleSky? "sky1Material" : "sky2Material"), RC_NULL);
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
                    os << "\n    Offset Speed = " + String::number(scrollDelta) + ";";
                }
                os << "\n  }";
            }
            os << "\n}";
        }

        // Output music modification defs for the non-map musics.
        for(auto const pair : defs.musics)
        {
            Music const &music = pair.second;
            if(custom != music.getb("custom")) continue;

            os << "\n\nMusic Mods \"" + music.gets("id") + "\" {"
               << "\n  CD Track = " + String::number(music.geti("cdTrack")) + ";"
               << "\n}";
        }
    }
};

MapInfoTranslator::MapInfoTranslator() : d(new Impl)
{}

void MapInfoTranslator::reset()
{
    d->defs.clear();
    d->translatedFiles.clear();
}

void MapInfoTranslator::merge(ddstring_s const &definitions, String sourcePath, bool sourceIsCustom)
{
    LOG_AS("MapInfoTranslator");

    if(Str_IsEmpty(&definitions)) return;

    String const source = sourcePath.isEmpty()? "[definition-data]"
                                              : ("\"" + NativePath(sourcePath).pretty() + "\"");
    try
    {
        if(!sourcePath.isEmpty())
        {
            LOG_RES_VERBOSE("Parsing %s...") << source;
            d->translatedFiles << sourcePath;
        }

        MapInfoParser parser(d->defs);
        parser.tryParse(definitions, sourcePath, sourceIsCustom);
    }
    catch(MapInfoParser::ParseError const &er)
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

    reset(); // The definition database was modified.
}

} // namespace idtech1
