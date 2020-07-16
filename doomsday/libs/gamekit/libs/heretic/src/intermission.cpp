/** @file intermission.cpp  Heretic specific intermission screens.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#include "jheretic.h"
#include "intermission.h"

#include "d_net.h"
#include "d_netcl.h"
#include "d_netsv.h"
#include "gamesession.h"
#include "hu_stuff.h"
#include "p_mapsetup.h"
#include "p_tick.h"
#include "g_defs.h"
#include "menu/widgets/widget.h"

#undef Set
#include <de/Set>

using namespace de;

namespace internal
{
    struct TeamInfo
    {
        int members;
        int frags[NUMTEAMS];
        int totalFrags;
    };
    static TeamInfo teamInfo[NUMTEAMS];

    struct Location
    {
        Vec2i origin;
        res::Uri mapUri;

        Location(const Vec2i &origin, const res::Uri &mapUri)
            : origin(origin)
            , mapUri(mapUri)
        {}
    };
    typedef List<Location> Locations;
    static Locations episode1Locations;
    static Locations episode2Locations;
    static Locations episode3Locations;

    static patchid_t pBackground;
    static patchid_t pBeenThere;
    static patchid_t pGoingThere;
    static patchid_t pFaceAlive[NUMTEAMS];
    static patchid_t pFaceDead[NUMTEAMS];

    static void drawTime(Vec2i origin, int hours, int minutes, int seconds, Vec4f rgba)
    {
        char buf[20];

        dd_snprintf(buf, 20, "%02d", seconds);
        M_DrawTextFragmentShadowed(buf, origin.x, origin.y, ALIGN_TOPRIGHT, 0, rgba.x, rgba.y, rgba.z, rgba.w);
        origin.x -= FR_TextWidth(buf) + FR_Tracking() * 3;
        M_DrawTextFragmentShadowed(":", origin.x, origin.y, ALIGN_TOPRIGHT, 0, rgba.x, rgba.y, rgba.z, rgba.w);
        origin.x -= FR_CharWidth(':') + 3;

        if(minutes || hours)
        {
            dd_snprintf(buf, 20, "%02d", minutes);
            M_DrawTextFragmentShadowed(buf, origin.x, origin.y, ALIGN_TOPRIGHT, 0, rgba.x, rgba.y, rgba.z, rgba.w);
            origin.x -= FR_TextWidth(buf) + FR_Tracking() * 3;
        }

        if(hours)
        {
            dd_snprintf(buf, 20, "%02d", hours);
            M_DrawTextFragmentShadowed(":", origin.x, origin.y, ALIGN_TOPRIGHT, 0, rgba.x, rgba.y, rgba.z, rgba.w);
            origin.x -= FR_CharWidth(':') + FR_Tracking() * 3;
            M_DrawTextFragmentShadowed(buf, origin.x, origin.y, ALIGN_TOPRIGHT, 0, rgba.x, rgba.y, rgba.z, rgba.w);
        }
    }

    enum gametype_t
    {
        SINGLE,
        COOPERATIVE,
        DEATHMATCH
    };
}

using namespace internal;

void IN_Init()
{
    // Already been here?
    if(!episode1Locations.isEmpty()) return;

    episode1Locations
        << Location( Vec2i(172,  78), res::makeUri("Maps:E1M1") )
        << Location( Vec2i( 86,  90), res::makeUri("Maps:E1M2") )
        << Location( Vec2i( 73,  66), res::makeUri("Maps:E1M3") )
        << Location( Vec2i(159,  95), res::makeUri("Maps:E1M4") )
        << Location( Vec2i(148, 126), res::makeUri("Maps:E1M5") )
        << Location( Vec2i(132,  54), res::makeUri("Maps:E1M6") )
        << Location( Vec2i(131,  74), res::makeUri("Maps:E1M7") )
        << Location( Vec2i(208, 138), res::makeUri("Maps:E1M8") )
        << Location( Vec2i( 52,  10), res::makeUri("Maps:E1M9") );

    episode2Locations
        << Location( Vec2i(218,  57), res::makeUri("Maps:E2M1") )
        << Location( Vec2i(137,  81), res::makeUri("Maps:E2M2") )
        << Location( Vec2i(155, 124), res::makeUri("Maps:E2M3") )
        << Location( Vec2i(171,  68), res::makeUri("Maps:E2M4") )
        << Location( Vec2i(250,  86), res::makeUri("Maps:E2M5") )
        << Location( Vec2i(136,  98), res::makeUri("Maps:E2M6") )
        << Location( Vec2i(203,  90), res::makeUri("Maps:E2M7") )
        << Location( Vec2i(220, 140), res::makeUri("Maps:E2M8") )
        << Location( Vec2i(279, 106), res::makeUri("Maps:E2M9") );

    episode3Locations
        << Location( Vec2i( 86,  99), res::makeUri("Maps:E3M1") )
        << Location( Vec2i(124, 103), res::makeUri("Maps:E3M2") )
        << Location( Vec2i(154,  79), res::makeUri("Maps:E3M3") )
        << Location( Vec2i(202,  83), res::makeUri("Maps:E3M4") )
        << Location( Vec2i(178,  59), res::makeUri("Maps:E3M5") )
        << Location( Vec2i(142,  58), res::makeUri("Maps:E3M6") )
        << Location( Vec2i(219,  66), res::makeUri("Maps:E3M7") )
        << Location( Vec2i(247,  57), res::makeUri("Maps:E3M8") )
        << Location( Vec2i(107,  80), res::makeUri("Maps:E3M9") );
}

void IN_Shutdown()
{
    // Nothing to do.
}

static String backgroundPatchForEpisode(const String &episodeId)
{
    bool isNumber;
    const int oldEpisodeNum = episodeId.toInt(&isNumber) - 1; // 1-based
    if(isNumber && oldEpisodeNum >= 0 && oldEpisodeNum <= 2)
    {
        return Stringf("MAPE%d", oldEpisodeNum + 1);
    }
    return ""; // None.
}

static const Locations *locationsForEpisode(const String &episodeId)
{
    if(episodeId == "1") return &episode1Locations;
    if(episodeId == "2") return &episode2Locations;
    if(episodeId == "3") return &episode3Locations;
    return nullptr; // Not found.
}

static const Location *tryFindLocationForMap(const Locations *locations, const res::Uri &mapUri)
{
    if(locations)
    {
        for(const Location &loc : *locations)
        {
            if(loc.mapUri == mapUri) return &loc;
        }
    }
    return nullptr; // Not found.
}

static bool active;

// Used to accelerate or skip a stage.
static bool advanceState;

static int inState;

static int interTime = -1;
static int oldInterTime;
static bool haveLocationMap;

static gametype_t gameType;

static int hours;
static int minutes;
static int seconds;

static int stateCounter;
static int backgroundAnimCounter;

static int cntKills[NUMTEAMS];
static int cntItems[NUMTEAMS];
static int cntSecret[NUMTEAMS];

static int slaughterBoy; // In DM, the player with the most kills.
static int playerTeam[MAXPLAYERS];
static fixed_t dSlideX[NUMTEAMS];
static fixed_t dSlideY[NUMTEAMS];

// Passed into intermission.
static const wbstartstruct_t *wbs;

static common::GameSession::VisitedMaps visitedMaps()
{
    // Newer versions of the savegame format include a breakdown of the maps previously visited
    // during the current game session.
    if(gfw_Session()->allVisitedMaps().isEmpty())
    {
        // For backward compatible intermission behavior we'll have to use a specially prepared
        // version of this information, using the original map progression assumptions.
        bool isNumber;
        int oldEpisodeNum = gfw_Session()->episodeId().toInt(&isNumber) - 1; // 1-based
        DE_ASSERT(isNumber);
        DE_UNUSED(isNumber);

        DE_ASSERT(wbs);
        int lastMapNum = G_MapNumberFor(::wbs->currentMap);
        if(lastMapNum == 8) lastMapNum = G_MapNumberFor(::wbs->nextMap) - 1; // 1-based

        Set<String> visited;
        for(int i = 0; i <= lastMapNum; ++i)
        {
            visited << G_ComposeMapUri(oldEpisodeNum, i);
        }
        if(::wbs->didSecret)
        {
            visited << G_ComposeMapUri(oldEpisodeNum, 8);
        }
        return compose<List<res::Uri>>(visited.begin(), visited.end());
    }
    return gfw_Session()->allVisitedMaps();
}

void IN_End()
{
    NetSv_Intermission(IMF_END, 0, 0);

    active = false;
}

static void tickNoState()
{
    if(!--stateCounter)
    {
        IN_End();
        G_IntermissionDone();
    }
}

static void drawBackground(patchid_t patch)
{
    if(!haveLocationMap) return;

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, 1);

    GL_DrawPatch(patch, Vec2i(0, 0), ALIGN_TOPLEFT, DPF_NO_OFFSET);

    DGL_Disable(DGL_TEXTURE_2D);
}

static void drawFinishedTitle()
{
    if (!haveLocationMap) return;

    if (wbs->currentMap.isEmpty()) return;

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(GF_FONTB));
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    FR_DrawTextXY3(G_MapTitle(wbs->currentMap), 160, 3, ALIGN_TOP, DTF_ONLY_SHADOW);

    FR_SetFont(FID(GF_FONTA));
    FR_SetColor(defFontRGB3[0], defFontRGB3[1],defFontRGB3[2]);
    FR_DrawTextXY3("FINISHED", 160, 25, ALIGN_TOP, DTF_ONLY_SHADOW);

    DGL_Disable(DGL_TEXTURE_2D);
}

static void drawEnteringTitle()
{
    if(!haveLocationMap) return;

    if(wbs->nextMap.isEmpty()) return;

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(GF_FONTA));
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(defFontRGB3[0], defFontRGB3[1], defFontRGB3[2], 1);
    FR_DrawTextXY3("NOW ENTERING:", 160, 10, ALIGN_TOP, DTF_ONLY_SHADOW);

    FR_SetFont(FID(GF_FONTB));
    FR_SetColor(defFontRGB[0], defFontRGB[1], defFontRGB[2]);
    FR_DrawTextXY3(G_MapTitle(wbs->nextMap), 160, 20, ALIGN_TOP, DTF_ONLY_SHADOW);

    DGL_Disable(DGL_TEXTURE_2D);
}

/**
 * Draw a mark on each map location visited during the current game session.
 */
static void drawLocationMarks(bool drawYouAreHere = false, bool flashCurrent = true)
{
    const Locations *locations = locationsForEpisode(gfw_Session()->episodeId());
    if(!locations) return;

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, 1);

    const common::GameSession::VisitedMaps visited = visitedMaps();
    for(const res::Uri &visitedMap : visited)
    {
        if(const Location *loc = tryFindLocationForMap(locations, visitedMap))
        {
            if(flashCurrent && visitedMap == wbs->currentMap && (interTime & 16))
            {
                continue;
            }

            GL_DrawPatch(pBeenThere, loc->origin);
        }
    }

    if(drawYouAreHere)
    {
        if(const Location *loc = tryFindLocationForMap(locations, wbs->nextMap))
        {
            GL_DrawPatch(pGoingThere, loc->origin);
        }
    }

    DGL_Disable(DGL_TEXTURE_2D);
}

static void initDeathmatchStats()
{
    gameType = DEATHMATCH;
    slaughterBoy = 0;

    int slaughterfrags = -9999;
    int posNum         = 0;
    int teamCount      = 0;
    int slaughterCount = 0;

    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        const int team = playerTeam[i];

        if(players[i].plr->inGame)
        {
            for(int percent = 0; percent < MAXPLAYERS; ++percent)
            {
                if(players[percent].plr->inGame)
                {
                    teamInfo[team].frags[playerTeam[percent]] += players[i].frags[percent];
                    teamInfo[team].totalFrags                 += players[i].frags[percent];
                }
            }

            // Find out the largest number of frags.
            if(teamInfo[team].totalFrags > slaughterfrags)
                slaughterfrags = teamInfo[team].totalFrags;
        }
    }

    for(int i = 0; i < NUMTEAMS; ++i)
    {
        //posNum++;
        /*if(teamInfo[i].totalFrags > slaughterfrags)
        {
           slaughterBoy = 1<<i;
           slaughterfrags = teamInfo[i].totalFrags;
           slaughterCount = 1;
        }
        else */ if(!teamInfo[i].members)
        {
            continue;
        }

        dSlideX[i] = (43 * posNum * FRACUNIT) / 20;
        dSlideY[i] = (36 * posNum * FRACUNIT) / 20;
        posNum++;

        teamCount++;
        if(teamInfo[i].totalFrags == slaughterfrags)
        {
            slaughterBoy |= 1 << i;
            slaughterCount++;
        }
    }

    if(teamCount == slaughterCount)
    {
        // Don't do the slaughter stuff if everyone is equal.
        slaughterBoy = 0;
    }
}

static de::String labelString(const char *text)
{
    return common::menu::Widget::labelText(text, "Intermission Label");
}

static void drawDeathmatchStats()
{
#define TRACKING                (1)

    static int sounds;
    static const char *killersText = "KILLERS";

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(GF_FONTB));
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
    FR_DrawTextXY3(labelString("TOTAL"), 265, 30, ALIGN_TOPLEFT, DTF_ONLY_SHADOW);

    FR_SetFont(FID(GF_FONTA));
    FR_SetColor(defFontRGB3[0], defFontRGB3[1], defFontRGB3[2]);
    FR_DrawTextXY3(labelString("VICTIMS"), 140, 8, ALIGN_TOPLEFT, DTF_ONLY_SHADOW);

    for(int i = 0; i < 7; ++i)
    {
        FR_DrawCharXY3(killersText[i], 10, 80 + 9 * i, ALIGN_TOPLEFT, DTF_ONLY_SHADOW);
    }

    DGL_Disable(DGL_TEXTURE_2D);

    int ypos = 55, xpos = 90;
    if(interTime < 20)
    {
        DGL_Enable(DGL_TEXTURE_2D);

        for(int i = 0; i < NUMTEAMS; ++i)
        {
            if(teamInfo[i].members)
            {
                M_DrawShadowedPatch(pFaceAlive[i], 40, ((ypos << FRACBITS) + dSlideY[i] * interTime) >> FRACBITS);
                M_DrawShadowedPatch(pFaceDead[i], ((xpos << FRACBITS) + dSlideX[i] * interTime) >> FRACBITS, 18);
            }
        }

        DGL_Disable(DGL_TEXTURE_2D);

        sounds = 0;
        return;
    }

    if(interTime >= 20 && sounds < 1)
    {
        S_LocalSound(SFX_DORCLS, NULL);
        sounds++;
    }

    if(interTime >= 100 && slaughterBoy && sounds < 2)
    {
        S_LocalSound(SFX_WPNUP, NULL);
        sounds++;
    }

    for(int i = 0; i < NUMTEAMS; ++i)
    {
        if(teamInfo[i].members)
        {
            char buf[20];

            DGL_Enable(DGL_TEXTURE_2D);

            if(interTime < 100 || i == playerTeam[CONSOLEPLAYER])
            {
                M_DrawShadowedPatch(pFaceAlive[i], 40, ypos);
                M_DrawShadowedPatch(pFaceDead[i], xpos, 18);
            }
            else
            {
                DGL_Color4f(1, 1, 1, .333f);
                GL_DrawPatch(pFaceAlive[i], Vec2i(40, ypos));
                GL_DrawPatch(pFaceDead[i],  Vec2i(xpos, 18));
            }

            FR_SetFont(FID(GF_FONTB));
            FR_SetTracking(TRACKING);
            int kpos = 122;
            for(int k = 0; k < NUMTEAMS; ++k)
            {
                if(teamInfo[k].members)
                {
                    dd_snprintf(buf, 20, "%i", teamInfo[i].frags[k]);
                    M_DrawTextFragmentShadowed(buf, kpos, ypos + 10, ALIGN_TOPRIGHT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
                    kpos += 43;
                }
            }

            if(slaughterBoy & (1 << i))
            {
                if(!(interTime & 16))
                {
                    dd_snprintf(buf, 20, "%i", teamInfo[i].totalFrags);
                    M_DrawTextFragmentShadowed(buf, 263, ypos + 10, ALIGN_TOPRIGHT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
                }
            }
            else
            {
                dd_snprintf(buf, 20, "%i", teamInfo[i].totalFrags);
                M_DrawTextFragmentShadowed(buf, 263, ypos + 10, ALIGN_TOPRIGHT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
            }

            DGL_Disable(DGL_TEXTURE_2D);

            ypos += 36;
            xpos += 43;
        }
    }

#undef TRACKING
}

static void initNetgameStats()
{
    gameType = COOPERATIVE;

    std::memset(cntKills,  0, sizeof(cntKills));
    std::memset(cntItems,  0, sizeof(cntItems));
    std::memset(cntSecret, 0, sizeof(cntSecret));

    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->inGame) continue;

        if(totalKills)
        {
            const int percent = players[i].killCount * 100 / totalKills;
            if(percent > cntKills[playerTeam[i]])
                cntKills[playerTeam[i]] = percent;
        }

        if(totalItems)
        {
            const int percent = players[i].itemCount * 100 / totalItems;
            if(percent > cntItems[playerTeam[i]])
                cntItems[playerTeam[i]] = percent;
        }

        if(totalSecret)
        {
            const int percent = players[i].secretCount * 100 / totalSecret;
            if(percent > cntSecret[playerTeam[i]])
                cntSecret[playerTeam[i]] = percent;
        }
    }
}

static void drawNetgameStats()
{
#define TRACKING                    (1)

    static int sounds;

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(GF_FONTB));
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    FR_DrawTextXY3(labelString("KILLS"), 95, 35, ALIGN_TOPLEFT, DTF_ONLY_SHADOW);
    FR_DrawTextXY3(labelString("BONUS"), 155, 35, ALIGN_TOPLEFT, DTF_ONLY_SHADOW);
    FR_DrawTextXY3(labelString("SECRET"), 232, 35, ALIGN_TOPLEFT, DTF_ONLY_SHADOW);
    FR_DrawTextXY3(G_MapTitle(wbs->currentMap), SCREENWIDTH/2, 3, ALIGN_TOP, DTF_ONLY_SHADOW);

    FR_SetFont(FID(GF_FONTA));
    FR_SetColor(defFontRGB3[0], defFontRGB3[1], defFontRGB3[2]);
    FR_DrawTextXY3(labelString("FINISHED"), SCREENWIDTH/2, 25, ALIGN_TOP, DTF_ONLY_SHADOW);

    FR_SetFont(FID(GF_FONTB));
    FR_SetTracking(TRACKING);

    int ypos = 50;
    for(int i = 0; i < NUMTEAMS; ++i)
    {
        if(teamInfo[i].members)
        {
            DGL_Color4f(0, 0, 0, .4f);
            GL_DrawPatch(pFaceAlive[i], Vec2i(27, ypos + 2));

            DGL_Color4f(defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
            GL_DrawPatch(pFaceAlive[i], Vec2i(25, ypos));

            if(interTime < 40)
            {
                sounds = 0;
                ypos += 37;
                continue;
            }
            else if(interTime >= 40 && sounds < 1)
            {
                S_LocalSound(SFX_DORCLS, NULL);
                sounds++;
            }

            char buf[20];
            dd_snprintf(buf, 20, "%i", cntKills[i]);
            M_DrawTextFragmentShadowed(buf, 121, ypos + 10, ALIGN_TOPRIGHT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
            M_DrawTextFragmentShadowed("%", 121, ypos + 10, ALIGN_TOPLEFT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

            dd_snprintf(buf, 20, "%i", cntItems[i]);
            M_DrawTextFragmentShadowed(buf, 196, ypos + 10, ALIGN_TOPRIGHT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
            M_DrawTextFragmentShadowed("%", 196, ypos + 10, ALIGN_TOPLEFT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

            dd_snprintf(buf, 20, "%i", cntSecret[i]);
            M_DrawTextFragmentShadowed(buf, 273, ypos + 10, ALIGN_TOPRIGHT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
            M_DrawTextFragmentShadowed("%", 273, ypos + 10, ALIGN_TOPLEFT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
            ypos += 37;
        }
    }

    DGL_Disable(DGL_TEXTURE_2D);

#undef TRACKING
}

static void drawSinglePlayerStats()
{
#define TRACKING                (1)

    static int sounds;

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(GF_FONTB));
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    FR_DrawTextXY3(labelString("KILLS"), 50, 65, ALIGN_TOPLEFT, DTF_ONLY_SHADOW);
    FR_DrawTextXY3(labelString("ITEMS"), 50, 90, ALIGN_TOPLEFT, DTF_ONLY_SHADOW);
    FR_DrawTextXY3(labelString("SECRETS"), 50, 115, ALIGN_TOPLEFT, DTF_ONLY_SHADOW);
    FR_DrawTextXY3(G_MapTitle(wbs->currentMap), 160, 3, ALIGN_TOP, DTF_ONLY_SHADOW);

    FR_SetFont(FID(GF_FONTA));
    FR_SetColor(defFontRGB3[0], defFontRGB3[1], defFontRGB3[2]);

    FR_DrawTextXY3(labelString("FINISHED"), 160, 25, ALIGN_TOP, DTF_ONLY_SHADOW);

    DGL_Disable(DGL_TEXTURE_2D);

    if(interTime < 30)
    {
        sounds = 0;
        return;
    }

    if(sounds < 1 && interTime >= 30)
    {
        S_LocalSound(SFX_DORCLS, nullptr);
        sounds++;
    }

    DGL_Enable(DGL_TEXTURE_2D);

    char buf[20];
    dd_snprintf(buf, 20, "%i", players[CONSOLEPLAYER].killCount);
    FR_SetFont(FID(GF_FONTB));
    FR_SetTracking(TRACKING);
    M_DrawTextFragmentShadowed(buf, 236, 65, ALIGN_TOPRIGHT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    M_DrawTextFragmentShadowed("/", 241, 65, ALIGN_TOPLEFT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    dd_snprintf(buf, 20, "%i", totalKills);
    M_DrawTextFragmentShadowed(buf, 284, 65, ALIGN_TOPRIGHT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    DGL_Disable(DGL_TEXTURE_2D);

    if(interTime < 60)
        return;

    if(sounds < 2 && interTime >= 60)
    {
        S_LocalSound(SFX_DORCLS, nullptr);
        sounds++;
    }

    DGL_Enable(DGL_TEXTURE_2D);

    dd_snprintf(buf, 20, "%i", players[CONSOLEPLAYER].itemCount);
    FR_SetFont(FID(GF_FONTB));
    M_DrawTextFragmentShadowed(buf, 236, 90, ALIGN_TOPRIGHT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    M_DrawTextFragmentShadowed("/", 241, 90, ALIGN_TOPLEFT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    dd_snprintf(buf, 20, "%i", totalItems);
    M_DrawTextFragmentShadowed(buf, 284, 90, ALIGN_TOPRIGHT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    DGL_Disable(DGL_TEXTURE_2D);

    if(interTime < 90)
        return;

    if(sounds < 3 && interTime >= 90)
    {
        S_LocalSound(SFX_DORCLS, nullptr);
        sounds++;
    }

    DGL_Enable(DGL_TEXTURE_2D);

    dd_snprintf(buf, 20, "%i", players[CONSOLEPLAYER].secretCount);
    FR_SetFont(FID(GF_FONTB));
    M_DrawTextFragmentShadowed(buf, 236, 115, ALIGN_TOPRIGHT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    M_DrawTextFragmentShadowed("/", 241, 115, ALIGN_TOPLEFT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    dd_snprintf(buf, 20, "%i", totalSecret);
    M_DrawTextFragmentShadowed(buf, 284, 115, ALIGN_TOPRIGHT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    DGL_Disable(DGL_TEXTURE_2D);

    if(interTime < 150)
    {
        return;
    }

    if(sounds < 4 && interTime >= 150)
    {
        S_LocalSound(SFX_DORCLS, nullptr);
        sounds++;
    }

    // Map play time.
    {
        DGL_Enable(DGL_TEXTURE_2D);

        FR_SetFont(FID(GF_FONTB));
        FR_SetColorAndAlpha(defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
        FR_DrawTextXY3(labelString("TIME"), 50, 140, ALIGN_TOPLEFT, DTF_ONLY_SHADOW);

        drawTime(Vec2i(284, 160), hours, minutes, seconds, Vec4f(defFontRGB[0], defFontRGB[1], defFontRGB[2], 1));

        DGL_Disable(DGL_TEXTURE_2D);
    }

    // Without a location map, show at least here on the stats screen what the next map will be.
    if (!haveLocationMap && interTime > 220)
    {
        if (!wbs->nextMap.isEmpty())
        {
            DGL_Enable(DGL_TEXTURE_2D);

            FR_SetFont(FID(GF_FONTA));
            FR_SetColorAndAlpha(defFontRGB3[0], defFontRGB3[1], defFontRGB3[2], 1);
            FR_DrawTextXY3(labelString("NOW ENTERING:"), SCREENWIDTH/2, 160, ALIGN_TOP, DTF_ONLY_SHADOW);

            FR_SetFont(FID(GF_FONTB));
            FR_SetColorAndAlpha(defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
            FR_DrawTextXY3(G_MapTitle(wbs->nextMap), 160, 170, ALIGN_TOP, DTF_ONLY_SHADOW);

            DGL_Disable(DGL_TEXTURE_2D);
        }

        advanceState = false;
    }

#undef TRACKING
}

static void initShowStats()
{
    gameType = SINGLE;
}

static void drawStats()
{
    de::String bgMaterial = "Flats:FLOOR16";

    // Intermission background can be defined via DED.
    {
        const String defined = gfw_Session()->mapInfo().gets("intermissionBg", "");
        if (!defined.empty())
        {
            bgMaterial = defined;
        }
    }

    if (bgMaterial.beginsWith("Flats:") || bgMaterial.beginsWith("flats:"))
    {
        // Draw a background flat.
        DGL_SetMaterialUI(
            (world_Material *) P_ToPtr(DMU_MATERIAL, Materials_ResolveUriCString(bgMaterial)),
            DGL_REPEAT,
            DGL_REPEAT);
        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, 1);
        DGL_DrawRectf2Tiled(0, 0, SCREENWIDTH, SCREENHEIGHT, 64, 64);
        DGL_Disable(DGL_TEXTURE_2D);
    }
    else if (bgMaterial.beginsWith("Patches:") || bgMaterial.beginsWith("patches:"))
    {
        drawBackground(R_DeclarePatch(bgMaterial.c_str() + strlen("Patches:")));
    }

    switch (gameType)
    {
        case SINGLE: drawSinglePlayerStats(); break;
        case COOPERATIVE: drawNetgameStats(); break;
        case DEATHMATCH: drawDeathmatchStats(); break;
    }
}

static void maybeAdvanceState()
{
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        player_t *player = &players[i];

        if(!players[i].plr->inGame) continue;

        if(player->brain.attack)
        {
            if(!player->attackDown)
            {
                if(IS_CLIENT)
                {
                    NetCl_PlayerActionRequest(player, GPA_FIRE, 0);
                }
                else
                {
                    IN_SkipToNext();
                }
            }
            player->attackDown = true;
        }
        else
        {
            player->attackDown = false;
        }

        if(player->brain.use)
        {
            if(!player->useDown)
            {
                if(IS_CLIENT)
                {
                    NetCl_PlayerActionRequest(player, GPA_USE, 0);
                }
                else
                {
                    IN_SkipToNext();
                }
            }
            player->useDown = true;
        }
        else
        {
            player->useDown = false;
        }
    }
}

static void endIntermissionGoToNextLevel()
{
    BusyMode_FreezeGameForBusyMode();
    inState = 3;
}

void IN_Ticker()
{
    if (!active) return;

    if (!IS_CLIENT)
    {
        if (inState == 3)
        {
            tickNoState();
            return;
        }
    }
    maybeAdvanceState();

    backgroundAnimCounter++;
    interTime++;

    if (oldInterTime < interTime)
    {
        // Only show stats if no location map is available.
        if (haveLocationMap)
        {
            if (inState == 2)
            {
                // Prepare for busy mode.
                BusyMode_FreezeGameForBusyMode();
            }
            inState++;
        }
        else
        {
            inState = 0;
        }

        switch (inState)
        {
            case 0:
                oldInterTime = interTime + 300;
                if (!haveLocationMap)
                {
                    oldInterTime = interTime + 1200;
                }
                break;
            case 1: oldInterTime = interTime + 200; break;
            case 2: oldInterTime = MAXINT; break;
            case 3: stateCounter = 10; break;
            default: break;
        }
    }

    if (advanceState)
    {
        if (inState == 0 && interTime < 150)
        {
            interTime    = 150;
            advanceState = false;
            NetSv_Intermission(IMF_TIME, 0, interTime);
            return;
        }
        if (inState < 2 && haveLocationMap)
        {
            inState      = 2;
            advanceState = false;
            S_StartSound(SFX_DORCLS, nullptr);
            NetSv_Intermission(IMF_STATE, inState, 0);
            return;
        }

        endIntermissionGoToNextLevel();
        stateCounter = 10;
        advanceState = false;
        S_StartSound(SFX_DORCLS, nullptr);
        NetSv_Intermission(IMF_STATE, inState, 0);
    }
}

static void loadData()
{
    const String episodeId = gfw_Session()->episodeId();

    // Determine which patch to use for the background.
    pBackground = R_DeclarePatch(backgroundPatchForEpisode(episodeId));

    pBeenThere  = R_DeclarePatch("IN_X");
    pGoingThere = R_DeclarePatch("IN_YAH");

    char name[9];
    for(int i = 0; i < NUMTEAMS; ++i)
    {
        dd_snprintf(name, 9, "FACEA%i", i);
        pFaceAlive[i] = R_DeclarePatch(name);

        dd_snprintf(name, 9, "FACEB%i", i);
        pFaceDead[i] = R_DeclarePatch(name);
    }
}

void IN_Drawer()
{
    static int oldInterState;

    if(!active || inState > 3)
    {
        return;
    }
    /*if(interState == 3)
    {
        return;
    }*/

    if(oldInterState != 2 && inState == 2)
    {
        S_LocalSound(SFX_PSTOP, nullptr);
    }

    if(inState != -1)
    {
        oldInterState = inState;
    }

    dgl_borderedprojectionstate_t bp;
    GL_ConfigureBorderedProjection(&bp, BPF_OVERDRAW_MASK|BPF_OVERDRAW_CLIP, SCREENWIDTH, SCREENHEIGHT,
                                   Get(DD_WINDOW_WIDTH), Get(DD_WINDOW_HEIGHT), scalemode_t(cfg.common.inludeScaleMode));
    GL_BeginBorderedProjection(&bp);

    switch(inState)
    {
    case -1:
    case 0:
        drawStats();
        break;

    case 1: // Leaving old level.
        drawBackground(pBackground);
        drawLocationMarks();
        drawFinishedTitle();
        break;

    case 2: // Going to the next level.
        drawBackground(pBackground);
        drawLocationMarks(!(interTime & 16) || inState == 3, false /*don't flash current location mark*/);
        drawEnteringTitle();
        break;

    case 3: // Waiting before going to the next level.
        drawBackground(pBackground);
        break;

    default:
        DE_ASSERT_FAIL("IN_Drawer: Unknown intermission state");
        break;
    }

    GL_EndBorderedProjection(&bp);
}

static void initVariables(const wbstartstruct_t &wbstartstruct)
{
    wbs = &wbstartstruct;

/*#ifdef RANGECHECK
    if(gameMode != commercial)
    {
        if(gameMode == retail)
            RNGCHECK(wbs->epsd, 0, 3);
        else
            RNGCHECK(wbs->epsd, 0, 2);
    }
    else
    {
        RNGCHECK(wbs->last, 0, 8);
        RNGCHECK(wbs->next, 0, 8);
    }
    RNGCHECK(wbs->pnum, 0, MAXPLAYERS);
    RNGCHECK(wbs->pnum, 0, MAXPLAYERS);
#endif

    accelerateStage = 0;
    */ backgroundAnimCounter = 0; /*
    firstRefresh = 1;
    me = wbs->pNum;
    myTeam = cfg.playerColor[wbs->pNum];
    plrs = wbs->plyr;

    if(!wbs->maxKills)
        wbs->maxKills = 1;
    if(!wbs->maxItems)
        wbs->maxItems = 1;
    if(!wbs->maxSecret)
        wbs->maxSecret = 1;

    if(gameMode != retail)
        if(wbs->epsd > 2)
            wbs->epsd -= 3;*/

    active          = true;
    inState         = -1;
    advanceState    = false;
    interTime       = 0;
    oldInterTime    = 0;
    haveLocationMap = locationsForEpisode(gfw_Session()->episodeId()) != nullptr;
}

void IN_Begin(const wbstartstruct_t &wbstartstruct)
{
    initVariables(wbstartstruct);
    loadData();

    // Init team info.
    if(IS_NETGAME)
    {
        std::memset(teamInfo,   0, sizeof(teamInfo));
        std::memset(playerTeam, 0, sizeof(playerTeam));

        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            if(!players[i].plr->inGame)
                continue;

            playerTeam[i] = cfg.playerColor[i];
            teamInfo[playerTeam[i]].members++;
        }
    }

    int time = mapTime / 35;
    ::hours   = time / 3600; time -= hours * 3600;
    ::minutes = time / 60;   time -= minutes * 60;
    ::seconds = time;

    if(!IS_NETGAME)
    {
        initShowStats();
    }
    else if( /*IS_NETGAME && */ !gfw_Rule(deathmatch))
    {
        initNetgameStats();
    }
    else
    {
        initDeathmatchStats();
    }
}

void IN_SetState(int stateNum)
{
    inState = stateNum;
}

void IN_SetTime(int time)
{
    interTime = time;
}

void IN_SkipToNext()
{
    advanceState = 1;
}

void IN_ConsoleRegister()
{
    C_VAR_BYTE("inlude-stretch",           &cfg.common.inludeScaleMode, 0, SCALEMODE_FIRST, SCALEMODE_LAST);
    C_VAR_INT ("inlude-patch-replacement", &cfg.common.inludePatchReplaceMode, 0, 0, 1);
}
