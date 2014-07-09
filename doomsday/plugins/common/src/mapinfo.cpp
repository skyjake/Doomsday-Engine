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
#include <de/Error>
#include "hexlex.h"
#include "g_common.h"

using namespace de;

typedef std::map<std::string, common::MapInfo> MapInfos;
static MapInfos mapInfos;

namespace common {

namespace internal {

static inline String defaultSkyMaterial()
{
#ifdef __JHEXEN__
    if(gameMode == hexen_demo || gameMode == hexen_betademo)
        return "Textures:SKY2";
#endif
    return "Textures:SKY1";
}

} // namespace internal

using namespace internal;

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

namespace internal {

#define MUSIC_STARTUP      "startup"
#define MUSIC_ENDING1      "hall"
#define MUSIC_ENDING2      "orb"
#define MUSIC_ENDING3      "chess"
#define MUSIC_INTERMISSION "hub"
#define MUSIC_TITLE        "hexen"

/**
 * Update the Music definition @a musicId with the specified CD @a track number.
 */
static void setMusicCDTrack(char const *musicId, int track)
{
    LOG_RES_VERBOSE("setMusicCDTrack: musicId=%s, track=%i") << musicId << track;

    int cdTrack = track;
    Def_Set(DD_DEF_MUSIC, Def_Get(DD_DEF_MUSIC, musicId, 0), DD_CD_TRACK, &cdTrack);
}

/**
 * @note In the future it is likely that a MAPINFO parser will only be necessary in order to
 * translate such content into Doomsday-native DED file(s) in a plugin. As such it would be
 * preferable if this could be done in one pass without the need for extra temporary storage.
 */
class MapInfoParser
{
    AutoStr *source;
    HexLex lexer;

public:
    /// Base class for all parse-related errors. @ingroup errors
    DENG2_ERROR(ParseError);

    MapInfoParser(AutoStr *source = 0) : source(source) {}

    void parse()
    {
        ::mapInfos.clear();

        // Nothing to parse?
        if(!source || Str_IsEmpty(source))
            return;

        lexer.parse(source);
        while(lexer.readToken())
        {
            if(!Str_CompareIgnoreCase(lexer.token(), "cd_start_track"))
            {
                setMusicCDTrack(MUSIC_STARTUP, (int)lexer.readNumber());
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "cd_end1_track"))
            {
                setMusicCDTrack(MUSIC_ENDING1, (int)lexer.readNumber());
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "cd_end2_track"))
            {
                setMusicCDTrack(MUSIC_ENDING2, (int)lexer.readNumber());
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "cd_end3_track"))
            {
                setMusicCDTrack(MUSIC_ENDING3, (int)lexer.readNumber());
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "cd_intermission_track"))
            {
                setMusicCDTrack(MUSIC_INTERMISSION, (int)lexer.readNumber());
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "cd_title_track"))
            {
                setMusicCDTrack(MUSIC_TITLE, (int)lexer.readNumber());
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "clearepisodes")) // ZDoom
            {
                LOG_WARNING("MAPINFO ClearEpisodes directives are not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "clearskills")) // ZDoom
            {
                LOG_WARNING("MAPINFO ClearSkills directives are not supported.");
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
    void parseCluster() // ZDoom
    {
        LOG_WARNING("MAPINFO Cluster definitions are not supported.");

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
                    /*char *found = 0;
                    if(Def_Get(DD_DEF_TEXT, enterText.toUtf8().constData(), &found) >= 0)
                    {
                        enterText = String(found);
                    }*/
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
                    /*char *found = 0;
                    if(Def_Get(DD_DEF_TEXT, exitText.toUtf8().constData(), &found) >= 0)
                    {
                        exitText = String(found);
                    }*/
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
                lexer.readNumber();
                continue;
            }

            lexer.unreadToken();
            break;
        }
    }

    void parseEpisode() // ZDoom
    {
        LOG_WARNING("MAPINFO Episode definitions are not supported.");
        de::Uri mapUri(Str_Text(lexer.readString()), RC_NULL);
        if(mapUri.scheme().isEmpty()) mapUri.setScheme("Maps");

        // Process optional tokens.
        while(lexer.readToken())
        {
            if(!Str_CompareIgnoreCase(lexer.token(), "name"))
            {
                lexer.readString();
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "lookup"))
            {
                lexer.readString();
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "picname"))
            {
                lexer.readString();
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "key"))
            {
                lexer.readString();
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "remove"))
            {
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "noskillmenu"))
            {
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "optional"))
            {
                continue;
            }

            lexer.unreadToken();
            break;
        }
    }

    /**
     * @note EndGame definitions appear inside a Map definition and  unlike all
     * other definition block types are scoped with curly-braces.
     *
     * @param mapInfo  MapInfo definition for which the EndGame subblock applies.
     */
    void parseEndGame(MapInfo & /*mapInfo*/) // ZDoom
    {
        LOG_WARNING("MAPINFO Map.next[EndGame] definitions are not supported.");

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
     * @param isSecret  @c true= this is the secret next map (from ZDoom) and should be ignored.
     */
    void parseMapNext(MapInfo &mapInfo, bool isSecret = false)
    {
        ddstring_s const *tok = lexer.readString();

        // Perhaps a ZDoom EndGame directive?
        if(!Str_CompareIgnoreCase(tok, "endpic"))
        {
            LOG_WARNING("MAPINFO Map.next EndGame directives are not supported.");
            lexer.readString();
            return;
        }
        if(!Str_CompareIgnoreCase(tok, "enddemon") ||
           !Str_CompareIgnoreCase(tok, "endgame1") ||
           !Str_CompareIgnoreCase(tok, "endgame2") ||
           !Str_CompareIgnoreCase(tok, "endgame3") ||
           !Str_CompareIgnoreCase(tok, "endgame4") ||
           !Str_CompareIgnoreCase(tok, "endgamec") ||
           !Str_CompareIgnoreCase(tok, "endgames") ||
           !Str_CompareIgnoreCase(tok, "endgamew"))
        {
            LOG_WARNING("MAPINFO Map.next EndGame directives are not supported.");
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
            mapUri = de::Uri(Str_Text(tok), RC_NULL);
            if(mapUri.scheme().isEmpty()) mapUri.setScheme("Maps");
        }
        else
        {
            if(mapNumber < 1)
            {
                throw ParseError(String("Invalid map number '%1' on line #%2").arg(mapNumber).arg(lexer.lineNumber()));
            }
            mapUri = G_ComposeMapUri(0, mapNumber - 1);
        }

        if(!isSecret)
        {
            mapInfo.set("nextMap", G_MapNumberFor(mapUri));
        }
    }

    void parseMap()
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
                throw ParseError(String("Invalid map number '%1' on line #%2").arg(mapNumber).arg(lexer.lineNumber()));
            }
            mapUri = G_ComposeMapUri(0, mapNumber - 1);
        }

        MapInfo *info = P_MapInfo(&mapUri);
        if(!info)
        {
            // A new map info.
            info = &::mapInfos[mapUri.path().asText().toLower().toUtf8().constData()];

            // Initialize with the default values.
            info->resetToDefaults();

            info->set("map", mapUri.compose());

            // Attempt to extract the "warp translation" number.
            /// @todo Define a sensible default should the map identifier not
            /// follow the default MAPXX form (what does ZDoom do here?) -ds
            info->set("warpTrans", G_MapNumberFor(mapUri));
        }

        // Map title must follow the number.
        String title = Str_Text(lexer.readString());

        // Lookup the title from a Text definition? (ZDoom)
        if(!title.compareWithoutCase("lookup"))
        {
            title = Str_Text(lexer.readString());
            char *found = 0;
            if(Def_Get(DD_DEF_TEXT, title.toUtf8().constData(), &found) >= 0)
            {
                title = String(found);
            }
        }
        info->set("title", title);

        // Process optional tokens.
        while(lexer.readToken())
        {
            if(!Str_CompareIgnoreCase(lexer.token(), "allowcrouch")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.allowCrouch is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "allowjump")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.allowJump is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "allowmonstertelefrags")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.allowMonsterTelefrags is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "allowrespawn")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.allowRespawn is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "aircontrol")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.airControl is not supported.");
                lexer.readNumber();
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "airsupply")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.airSupply is not supported.");
                lexer.readNumber();
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "autosequences")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.autosequences is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "baronspecial")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.baronSpecial is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "bordertexture")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.borderTexture is not supported.");
                lexer.readString();
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "cdid")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.cdid is not supported.");
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
                LOG_WARNING("MAPINFO Map.checkSwitchRange is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "clipmidtextures")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.clipMidtextures is not supported.");
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
            if(String(Str_Text(lexer.token())).beginsWith("compat_", Qt::CaseInsensitive)) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.%s is not supported.") << lexer.token();
                lexer.readNumber();
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "cyberdemonspecial")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.cyberdemonSpecial is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "doublesky"))
            {
                info->set("doubleSky", true);
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "enterpic")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.enterPic is not supported.");
                lexer.readString();
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "evenlighting")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.evenlighting is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "exitpic")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.exitPic is not supported.");
                lexer.readString();
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "f1")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.f1 is not supported.");
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
                LOG_WARNING("MAPINFO Map.fade is not supported.");
                lexer.readString();
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "fallingdamage")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.fallingdamage is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "filterstarts")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.filterStarts is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "forceFallingDamage")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.forceFallingDamage is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "forceNoSkyStretch")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.forceNoSkyStretch is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "gravity")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.gravity is not supported.");
                lexer.readNumber();
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "horizwallshade")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.horizwallShade is not supported.");
                lexer.readNumber();
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "infiniteflightpowerup")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.infiniteFlightPowerup is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "intermusic")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.interMusic is not supported.");
                lexer.readString();
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "keepfullinventory")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.keepFullInventory is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "laxmonsteractivation")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.laxMonsterActivation is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "lightning"))
            {
                info->set("lightning", true);
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "map07special")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.map07Special is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "monsterfallingdamage")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.monsterFallingDamage is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "missilesactivateimpactlines")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.missilesActivateImpactLines is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "missileshootersactivateimpactlines")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.missileshootersActivateImpactLines is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "music")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.music is not supported.");
                lexer.readString();
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "next"))
            {
                parseMapNext(*info);
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "noautosequences")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.noAutoSequences is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "nocheckswitchrange")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.noCheckSwitchRange is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "nocrouch")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.noCrouch is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "nofallingdamage")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.noFallingDamage is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "noinfighting")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.noInfighting is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "nointermission")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.noIntermission is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "noinventorybar")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.noInventorybar is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "nojump")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.noJump is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "normalinfighting")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.normalInfighting is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "nosoundclipping")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.noSoundClipping is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "oldfallingdamage")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.oldFallingDamage is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "outsidefog")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.outsideFog is not supported.");
                lexer.readString();
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "par")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.par is not supported.");
                lexer.readNumber();
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "secretnext")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.secretNext is not supported.");
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
                LOG_WARNING("MAPINFO Map.skyStretch is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "specialaction_exitlevel")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.specialaction_exitlevel is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "specialaction_killmonsters")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.specialaction_killmonsters is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "specialaction_lowerfloor")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.specialaction_lowerfloor is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "specialaction_opendoor")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.specialaction_opendoor is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "spidermastermindspecial")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.spidermastermindSpecial is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "smoothlighting")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.smoothlighting is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "strictmonsteractivation")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.strictMonsterActivation is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "strifefallingdamage")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.strifeFallingDamage is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "teamdamage")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.teamDamage is not supported.");
                lexer.readNumber();
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "teamplayoff")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.teamplayOff is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "teamplayon")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.teamplayOn is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "titlepatch")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.titlePatch is not supported.");
                lexer.readString();
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "totalinfighting")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.totalInfighting is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "translator")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.translator is not supported.");
                lexer.readString();
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "unfreezesingleplayerconversations")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.unfreezeSingleplayerConversations is not supported.");
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "vertwallshade")) // ZDoom
            {
                LOG_WARNING("MAPINFO Map.vertwallShade is not supported.");
                lexer.readNumber();
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "warptrans") ||
               !Str_CompareIgnoreCase(lexer.token(), "levelnum") /* ZDoom */)
            {
                int const mapWarpNum = (int)lexer.readNumber();
                if(mapWarpNum < 1)
                {
                    throw ParseError(String("Invalid map warp-number '%1' on line #%2").arg(Str_Text(lexer.token())).arg(lexer.lineNumber()));
                }
                info->set("warpTrans", mapWarpNum - 1);
                continue;
            }

            lexer.unreadToken();
            break;
        }
    }

    void parseSkill() // ZDoom
    {
        LOG_WARNING("MAPINFO Skill definitions are not supported.");

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

void MapInfoParser(ddstring_s const *path)
{
    AutoStr *source = M_ReadFileIntoString(path, 0);
    if(!source || Str_IsEmpty(source))
    {
        LOG_RES_WARNING("MapInfoParser: Failed to open definition/script file \"%s\" for reading")
                << NativePath(Str_Text(path)).pretty();
        return;
    }

    LOG_RES_VERBOSE("Parsing \"%s\"...") << NativePath(Str_Text(path)).pretty();

    try
    {
        internal::MapInfoParser(source).parse();
    }
    catch(internal::MapInfoParser::ParseError const &er)
    {
        LOG_RES_WARNING("Failed parsing \"%s\" as MAPINFO:\n%s")
                << NativePath(Str_Text(path)).pretty() << er.asText();
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

} // namespace common
