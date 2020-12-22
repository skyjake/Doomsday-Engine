/** @file intermission.cpp  DOOM specific intermission screens.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1993-1996 id Software, Inc.
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

#include "jdoom.h"
#include "intermission.h"

#include "d_net.h"
#include "d_netcl.h"
#include "d_netsv.h"
#include "gamesession.h"
#include "hu_stuff.h"
#include "p_mapsetup.h"
#include "p_start.h"

#include <cstdio>
#include <cctype>
#include <cstring>
#include <de/list.h>
#include <de/set.h>

using namespace de;

namespace internal
{
    struct TeamInfo
    {
        int playerCount;      ///< @c 0= team not present.
        int frags[NUMTEAMS];
        int totalFrags;       ///< Kills minus suicides.
        int items;
        int kills;
        int secret;
    };
    static TeamInfo teamInfo[NUMTEAMS];

    struct Animation
    {
        Vec2i origin;
        int tics;                     ///< Number of tics each frame of the animation lasts for.
        StringList patchNames;        ///< For each frame of the animation.
        res::Uri mapUri;               ///< If path is not zero-length the animation should only be displayed on this map.
        interludestate_t beginState;  ///< State at which this animation begins/becomes visible.

        Animation(const Vec2i &origin, int tics, StringList patchNames,
                  const res::Uri &mapUri       = res::makeUri("Maps:"),
                  interludestate_t beginState = ILS_SHOW_STATS)
            : origin    (origin)
            , tics      (tics)
            , patchNames(patchNames)
            , mapUri    (mapUri)
            , beginState(beginState)
        {}
    };
    typedef List<Animation> Animations;
    static Animations episode1Anims;
    static Animations episode2Anims;
    static Animations episode3Anims;

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

    struct wianimstate_t
    {
        int nextTic = 0;  ///< Next tic on which to progress the animation.
        int frame = -1;   ///< Current frame number (index to patches); otherwise @c -1 (not yet begun).

        /// Graphics for each frame of the animation.
        typedef List<patchid_t> Patches;
        Patches patches;
    };
    typedef List<wianimstate_t> AnimationStates;
    static AnimationStates animStates;

    static patchid_t pBackground;
    static patchid_t pYouAreHereRight;
    static patchid_t pYouAreHereLeft;
    static patchid_t pSplat;
    static patchid_t pFinished;
    static patchid_t pEntering;
    static patchid_t pSecret;
    static patchid_t pSecretSP;
    static patchid_t pKills;
    static patchid_t pItems;
    static patchid_t pFrags;
    static patchid_t pTime;
    static patchid_t pPar;
    static patchid_t pSucks;
    static patchid_t pKillers;
    static patchid_t pVictims;
    static patchid_t pTotal;
    static patchid_t pFaceAlive;
    static patchid_t pFaceDead;
    static patchid_t pTeamBackgrounds[NUMTEAMS];
    static patchid_t pTeamIcons[NUMTEAMS];

    /// @todo Revise API to select a replacement mode according to the usage context
    /// and/or domain. Passing an "existing" text string is also a bit awkward... -ds
    static inline String patchReplacementText(patchid_t patchId, const String &text = "")
    {
        return Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.common.inludePatchReplaceMode),
                                         patchId, text);
    }

    static void drawChar(Char const ch, const Vec2i &origin,
                         int alignFlags = ALIGN_TOPLEFT, int textFlags = DTF_NO_TYPEIN)
    {
        const Point2Raw rawOrigin = {{{origin.x, origin.y}}};
        FR_DrawChar3(char(ch), &rawOrigin, alignFlags, textFlags);
    }

    static void drawText(const String &text, const Vec2i &origin,
                         int alignFlags = ALIGN_TOPLEFT, int textFlags = DTF_NO_TYPEIN)
    {
        const Point2Raw rawOrigin = {{{origin.x, origin.y}}};
        FR_DrawText3(text, &rawOrigin, alignFlags, textFlags);
    }

    static void drawPercent(int percent, const Vec2i &origin)
    {
        if(percent < 0) return;
        drawChar('%', origin, ALIGN_TOPLEFT, DTF_NO_TYPEIN);
        drawText(String::asText(percent), origin, ALIGN_TOPRIGHT, DTF_NO_TYPEIN);
    }

    /**
     * Display map completion time and par, or "sucks" message if overflow.
     */
    static void drawTime(Vec2i origin, int t)
    {
        if(t < 0) return;

        if(t <= 61 * 59)
        {
            origin.x -= 22;

            const int seconds = t % 60;
            const int minutes = t / 60 % 60;

            drawChar(':', origin);
            if(minutes > 0)
            {
                drawText(String::asText(minutes), origin, ALIGN_TOPRIGHT);
            }
            drawText(Stringf("%02i", seconds), origin + Vec2i(FR_CharWidth(':'), 0));

            return;
        }

        // "sucks"
        patchinfo_t info;
        if(!R_GetPatchInfo(pSucks, &info)) return;

        WI_DrawPatch(pSucks, patchReplacementText(pSucks), Vec2i(origin.x - info.geometry.size.width, origin.y), ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
    }
}

using namespace internal;

void IN_Init()
{
    // Already been here?
    if(!episode1Anims.isEmpty()) return;

    if(gameModeBits & GM_ANY_DOOM2) return;

    episode1Anims
        << Animation( Vec2i(224, 104), 11, StringList() << String("wia00000") << String("wia00001") << String("wia00002") )
        << Animation( Vec2i(184, 160), 11, StringList() << String("wia00100") << String("wia00101") << String("wia00102") )
        << Animation( Vec2i(112, 136), 11, StringList() << String("wia00200") << String("wia00201") << String("wia00202") )
        << Animation( Vec2i( 72, 112), 11, StringList() << String("wia00300") << String("wia00301") << String("wia00302") )
        << Animation( Vec2i( 88,  96), 11, StringList() << String("wia00400") << String("wia00401") << String("wia00402") )
        << Animation( Vec2i( 64,  48), 11, StringList() << String("wia00500") << String("wia00501") << String("wia00502") )
        << Animation( Vec2i(192,  40), 11, StringList() << String("wia00600") << String("wia00601") << String("wia00602") )
        << Animation( Vec2i(136,  16), 11, StringList() << String("wia00700") << String("wia00701") << String("wia00702") )
        << Animation( Vec2i( 80,  16), 11, StringList() << String("wia00800") << String("wia00801") << String("wia00802") )
        << Animation( Vec2i( 64,  24), 11, StringList() << String("wia00900") << String("wia00901") << String("wia00902") );

    episode1Locations
        << Location( Vec2i(185, 164), res::makeUri("Maps:E1M1") )
        << Location( Vec2i(148, 143), res::makeUri("Maps:E1M2") )
        << Location( Vec2i( 69, 122), res::makeUri("Maps:E1M3") )
        << Location( Vec2i(209, 102), res::makeUri("Maps:E1M4") )
        << Location( Vec2i(116,  89), res::makeUri("Maps:E1M5") )
        << Location( Vec2i(166,  55), res::makeUri("Maps:E1M6") )
        << Location( Vec2i( 71,  56), res::makeUri("Maps:E1M7") )
        << Location( Vec2i(135,  29), res::makeUri("Maps:E1M8") )
        << Location( Vec2i( 71,  24), res::makeUri("Maps:E1M9") );

    episode2Anims
        << Animation( Vec2i(128, 136),  0, StringList() << String("wia10000"), res::makeUri("Maps:E2M2") )
        << Animation( Vec2i(128, 136),  0, StringList() << String("wia10100"), res::makeUri("Maps:E2M3") )
        << Animation( Vec2i(128, 136),  0, StringList() << String("wia10200"), res::makeUri("Maps:E2M4") )
        << Animation( Vec2i(128, 136),  0, StringList() << String("wia10300"), res::makeUri("Maps:E2M5") )
        << Animation( Vec2i(128, 136),  0, StringList() << String("wia10400"), res::makeUri("Maps:E2M6") )
        << Animation( Vec2i(128, 136),  0, StringList() << String("wia10400"), res::makeUri("Maps:E2M9") )
        << Animation( Vec2i(128, 136),  0, StringList() << String("wia10500"), res::makeUri("Maps:E2M7") )
        << Animation( Vec2i(128, 136),  0, StringList() << String("wia10600"), res::makeUri("Maps:E2M8") )
        << Animation( Vec2i(192, 144), 11, StringList() << String("wia10700") << String("wia10701") << String("wia10702"), res::makeUri("Maps:E2M9"), ILS_SHOW_NEXTMAP );

    episode2Locations
        << Location( Vec2i(254,  25), res::makeUri("Maps:E2M1") )
        << Location( Vec2i( 97,  50), res::makeUri("Maps:E2M2") )
        << Location( Vec2i(188,  64), res::makeUri("Maps:E2M3") )
        << Location( Vec2i(128,  78), res::makeUri("Maps:E2M4") )
        << Location( Vec2i(214,  92), res::makeUri("Maps:E2M5") )
        << Location( Vec2i(133, 130), res::makeUri("Maps:E2M6") )
        << Location( Vec2i(208, 136), res::makeUri("Maps:E2M7") )
        << Location( Vec2i(148, 140), res::makeUri("Maps:E2M8") )
        << Location( Vec2i(235, 158), res::makeUri("Maps:E2M9") );

    episode3Anims
        << Animation( Vec2i(104, 168), 11, StringList() << String("wia20000") << String("wia20001") << String("wia20002") )
        << Animation( Vec2i( 40, 136), 11, StringList() << String("wia20100") << String("wia20101") << String("wia20102") )
        << Animation( Vec2i(160,  96), 11, StringList() << String("wia20200") << String("wia20201") << String("wia20202") )
        << Animation( Vec2i(104,  80), 11, StringList() << String("wia20300") << String("wia20301") << String("wia20302") )
        << Animation( Vec2i(120,  32), 11, StringList() << String("wia20400") << String("wia20401") << String("wia20402") )
        << Animation( Vec2i( 40,   0),  8, StringList() << String("wia20500") << String("wia20501") << String("wia20502") );

    episode3Locations
        << Location( Vec2i(156, 168), res::makeUri("Maps:E3M1") )
        << Location( Vec2i( 48, 154), res::makeUri("Maps:E3M2") )
        << Location( Vec2i(174,  95), res::makeUri("Maps:E3M3") )
        << Location( Vec2i(265,  75), res::makeUri("Maps:E3M4") )
        << Location( Vec2i(130,  48), res::makeUri("Maps:E3M5") )
        << Location( Vec2i(279,  23), res::makeUri("Maps:E3M6") )
        << Location( Vec2i(198,  48), res::makeUri("Maps:E3M7") )
        << Location( Vec2i(140,  25), res::makeUri("Maps:E3M8") )
        << Location( Vec2i(281, 136), res::makeUri("Maps:E3M9") );
}

void IN_Shutdown()
{
    animStates.clear();
}

static String backgroundPatchForEpisode(const String &episodeId)
{
    if(!(gameModeBits & GM_ANY_DOOM2))
    {
        bool isNumber;
        const int oldEpisodeNum = episodeId.toInt(&isNumber) - 1; // 1-based
        if(isNumber && oldEpisodeNum >= 0 && oldEpisodeNum <= 2)
        {
            return Stringf("WIMAP%i", oldEpisodeNum);
        }
    }
    return "INTERPIC";
}

static const Animations *animationsForEpisode(const String &episodeId)
{
    if(!(gameModeBits & GM_ANY_DOOM2))
    {
        if(episodeId == "1") return &episode1Anims;
        if(episodeId == "2") return &episode2Anims;
        if(episodeId == "3") return &episode3Anims;
    }
    return nullptr; // Not found.
}

static const Locations *locationsForEpisode(const String &episodeId)
{
    if(!(gameModeBits & GM_ANY_DOOM2))
    {
        if(episodeId == "1") return &episode1Locations;
        if(episodeId == "2") return &episode2Locations;
        if(episodeId == "3") return &episode3Locations;
    }
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

// Used to accelerate or skip a stage.
static bool advanceState;

static bool drawYouAreHere;

static int spState, dmState, ngState;

static interludestate_t inState;

static int dmFrags[NUMTEAMS][NUMTEAMS];
static int dmTotals[NUMTEAMS];

static int doFrags;

static int inPlayerNum;
static int inPlayerTeam;

static int stateCounter;
static int backgroundAnimCounter;

static int cntKills[NUMTEAMS];
static int cntItems[NUMTEAMS];
static int cntSecret[NUMTEAMS];
static int cntFrags[NUMTEAMS];
static int cntTime;
static int cntPar;
static int cntPause;

// Passed into intermission.
static const wbstartstruct_t *wbs;
static const wbplayerstruct_t *inPlayerInfo;

static common::GameSession::VisitedMaps visitedMaps()
{
    // Newer versions of the savegame format include a breakdown of the maps previously visited
    // during the current game session.
    if(gfw_Session()->allVisitedMaps().isEmpty())
    {
        // For backward compatible intermission behavior we'll have to use a specially prepared
        // version of this information, using the original map progression assumptions.
        if(!(gameModeBits & GM_ANY_DOOM2))
        {
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
            return compose<common::GameSession::VisitedMaps>(visited.begin(), visited.end());
        }
    }
    return gfw_Session()->allVisitedMaps();
}

static void drawBackground()
{
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, 1);

    GL_DrawPatch(pBackground, Vec2i(0, 0), ALIGN_TOPLEFT, DPF_NO_OFFSET);

    if(const Animations *anims = animationsForEpisode(gfw_Session()->episodeId()))
    {
        FR_SetFont(FID(GF_FONTB));
        FR_LoadDefaultAttrib();

        for(int i = 0; i < anims->count(); ++i)
        {
            const Animation &def = (*anims)[i];
            wianimstate_t &state = animStates[i];

            // Has the animation begun yet?
            if(state.frame < 0) continue;

            patchid_t patchId = state.patches[state.frame];
            WI_DrawPatch(patchId, patchReplacementText(patchId), def.origin, ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
        }
    }

    DGL_Disable(DGL_TEXTURE_2D);
}

static void drawFinishedTitle(Vec2i origin = Vec2i(SCREENWIDTH / 2, WI_TITLEY))
{
    DE_ASSERT(!wbs->currentMap.isEmpty());

    patchid_t titlePatchId = 0;

    const String title       = G_MapTitle(wbs->currentMap);
    const res::Uri titleImage = G_MapTitleImage(wbs->currentMap);
    if(!titleImage.isEmpty())
    {
        if(!titleImage.scheme().compareWithoutCase("Patches"))
        {
            titlePatchId = R_DeclarePatch(titleImage.path());
        }
    }

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, 1);
    FR_SetFont(FID(GF_FONTB));
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(defFontRGB[CR], defFontRGB[CG], defFontRGB[CB], 1);

    const String text = patchReplacementText(titlePatchId, title);
    if (text)
    {
        // Draw title text.
        drawText(text, origin, ALIGN_TOP, DTF_NO_TYPEIN);
        origin.y += 4 * FR_TextHeight(text) / 5;
    }
    else
    {
        // Draw title image.
        GL_DrawPatch(titlePatchId, origin, ALIGN_TOP);
        patchinfo_t info;
        if(R_GetPatchInfo(titlePatchId, &info)) origin.y += (5 * info.geometry.size.height) / 4;
    }

    // Draw "Finished!"
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], 1);
    WI_DrawPatch(pFinished, patchReplacementText(pFinished), origin, ALIGN_TOP, 0, DTF_NO_TYPEIN);

    DGL_Disable(DGL_TEXTURE_2D);
}

static void drawEnteringTitle(Vec2i origin = Vec2i(SCREENWIDTH / 2, WI_TITLEY))
{
    if(wbs->nextMap.isEmpty()) return;

    /// @kludge We need to properly externalize the map progression.
    if((gameModeBits & (GM_DOOM2|GM_DOOM2_PLUT|GM_DOOM2_TNT)) &&
       wbs->nextMap.path() == "MAP31")
    {
        return;
    }
    /// kludge end.

    patchid_t patchId = 0;

    const String title       = G_MapTitle(wbs->nextMap);
    const res::Uri titleImage = G_MapTitleImage(wbs->nextMap);
    if(!titleImage.isEmpty())
    {
        if(!titleImage.scheme().compareWithoutCase("Patches"))
        {
            patchId = R_DeclarePatch(titleImage.path());
        }
    }

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, 1);

    FR_SetFont(FID(GF_FONTB));
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], 1);

    // Determine height of the title.
    int titleHeight = 0;
    {
        patchinfo_t info;
        if(R_GetPatchInfo(patchId, &info))
        {
            titleHeight = 5 * info.geometry.size.height / 4;
        }
        else
        {
            titleHeight = 4 * FR_TextHeight(title) / 5;
        }
    }

    // Draw "Entering".
    WI_DrawPatch(pEntering, patchReplacementText(pEntering), origin, ALIGN_TOP, 0, DTF_NO_TYPEIN);

    // Draw map title.
    origin.y += titleHeight;
    FR_SetColorAndAlpha(defFontRGB[CR], defFontRGB[CG], defFontRGB[CB], 1);
    WI_DrawPatch(patchId, patchReplacementText(patchId, title),
                 origin, ALIGN_TOP, 0, DTF_NO_TYPEIN);

    DGL_Disable(DGL_TEXTURE_2D);
}

static bool patchFits(patchid_t patchId, const Vec2i &origin)
{
    patchinfo_t info;
    if(!R_GetPatchInfo(patchId, &info)) return false;

    const int left   = origin.x + info.geometry.origin.x;
    const int top    = origin.y + info.geometry.origin.y;
    const int right  = left + info.geometry.size.width;
    const int bottom = top + info.geometry.size.height;
    return (left >= 0 && right < SCREENWIDTH && top >= 0 && bottom < SCREENHEIGHT);
}

static patchid_t chooseYouAreHerePatch(const Vec2i &origin)
{
    if(patchFits(pYouAreHereRight, origin))
        return pYouAreHereRight;

    if(patchFits(pYouAreHereLeft, origin))
        return pYouAreHereLeft;

    return 0; // None fits.
}

static void drawPatchIfFits(patchid_t patchId, const Vec2i &origin)
{
    if(patchFits(patchId, origin))
    {
        WI_DrawPatch(patchId, patchReplacementText(patchId), origin, ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
    }
}

/**
 * Begin any animations that were previously waiting on a state.
 * To be called upon changing the value of @var inState.
 */
static void beginAnimations()
{
    const Animations *anims = animationsForEpisode(gfw_Session()->episodeId());
    if(!anims) return;

    for(int i = 0; i < anims->count(); ++i)
    {
        const Animation &def = (*anims)[i];
        wianimstate_t &state = animStates[i];

        // Is the animation active for the next map?
        if(!def.mapUri.path().isEmpty() && wbs->nextMap != def.mapUri)
            continue;

        // Already begun?
        if(state.frame >= 0) continue;

        // Is it time to begin the animation?
        if(def.beginState != inState) continue;

        state.frame = 0;

        // Determine when to animate the first frame.
        if(!def.mapUri.path().isEmpty())
        {
            state.nextTic = backgroundAnimCounter + 1 + def.tics;
        }
        else
        {
            state.nextTic = backgroundAnimCounter + 1 + (M_Random() % def.tics);
        }
    }
}

static void animateBackground()
{
    const Animations *anims = animationsForEpisode(gfw_Session()->episodeId());
    if(!anims) return;

    for(int i = 0; i < anims->count(); ++i)
    {
        const Animation &def = (*anims)[i];
        wianimstate_t &state = animStates[i];

        // Is the animation active for the next map?
        if(!def.mapUri.path().isEmpty() && wbs->nextMap != def.mapUri)
            continue;

        // Has the animation begun yet
        if(state.frame < 0) continue;

        // Time to progress the animation?
        if(backgroundAnimCounter != state.nextTic) continue;

        ++state.frame;
        if(state.frame >= def.patchNames.count())
        {
            if(!def.mapUri.path().isEmpty())
            {
                // Loop.
                state.frame = def.patchNames.count() - 1;
            }
            else
            {
                // Restart.
                state.frame = 0;
            }
        }

        state.nextTic = backgroundAnimCounter + de::max(def.tics, 1);
    }
}

void IN_End()
{
    NetSv_Intermission(IMF_END, 0, 0);
}

static void initNoState()
{
    inState      = ILS_NONE;
    advanceState = false;
    stateCounter = 10;

    NetSv_Intermission(IMF_STATE, inState, 0);
}

static void tickNoState()
{
    --stateCounter;
    if(0 == stateCounter)
    {
        if(IS_CLIENT) return;

        IN_End();
        G_IntermissionDone();
    }
}

static void initShowNextMap()
{
    inState      = ILS_SHOW_NEXTMAP;
    advanceState = false;
    stateCounter = SHOWNEXTLOCDELAY * TICRATE;

    beginAnimations();

    NetSv_Intermission(IMF_STATE, inState, 0);
}

static void tickShowNextMap()
{
    --stateCounter;
    if(0 == stateCounter || advanceState)
    {
        initNoState();
        return;
    }

    drawYouAreHere = (stateCounter & 31) < 20;
}

/**
 * Draw a mark on each map location visited during the current game session.
 */
static void drawLocationMarks()
{
    const Locations *locations = locationsForEpisode(gfw_Session()->episodeId());
    if(!locations) return;

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, 1);
    FR_SetFont(FID(GF_FONTB));
    FR_LoadDefaultAttrib();

    for (const res::Uri &visitedMap : visitedMaps())
    {
        if(const Location *loc = tryFindLocationForMap(locations, visitedMap))
        {
            drawPatchIfFits(pSplat, loc->origin);
        }
    }

    if(drawYouAreHere)
    {
        if(const Location *loc = tryFindLocationForMap(locations, wbs->nextMap))
        {
            const patchid_t yahPatchId = chooseYouAreHerePatch(loc->origin);
            WI_DrawPatch(yahPatchId, patchReplacementText(yahPatchId),
                         loc->origin, ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
        }
    }

    DGL_Disable(DGL_TEXTURE_2D);
}

static void initDeathmatchStats()
{
    inState      = ILS_SHOW_STATS;
    advanceState = false;
    dmState      = 1;
    cntPause     = TICRATE;

    // Clear the on-screen counters.
    std::memset(dmTotals, 0, sizeof(dmTotals));
    for(int i = 0; i < NUMTEAMS; ++i)
    {
        std::memset(dmFrags[i], 0, sizeof(dmFrags[i]));
    }

    beginAnimations();
}

static void updateDeathmatchStats()
{
    if(advanceState && dmState != 4)
    {
        advanceState = false;
        for(int i = 0; i < NUMTEAMS; ++i)
        {
            for(int k = 0; k < NUMTEAMS; ++k)
            {
                dmFrags[i][k] = teamInfo[i].frags[k];
            }

            dmTotals[i] = teamInfo[i].totalFrags;
        }

        S_LocalSound(SFX_BAREXP, 0);
        dmState = 4;
    }

    if(dmState == 2)
    {
        if(!(backgroundAnimCounter & 3))
        {
            S_LocalSound(SFX_PISTOL, 0);
        }

        bool stillTicking = false;
        for(int i = 0; i < NUMTEAMS; ++i)
        {
            for(int k = 0; k < NUMTEAMS; ++k)
            {
                if(dmFrags[i][k] != teamInfo[i].frags[k])
                {
                    if(teamInfo[i].frags[k] < 0)
                        dmFrags[i][k]--;
                    else
                        dmFrags[i][k]++;

                    if(dmFrags[i][k] > 99)
                        dmFrags[i][k] = 99;

                    if(dmFrags[i][k] < -99)
                        dmFrags[i][k] = -99;

                    stillTicking = true;
                }
            }

            dmTotals[i] = teamInfo[i].totalFrags;

            if(dmTotals[i] > 99)
                dmTotals[i] = 99;

            if(dmTotals[i] < -99)
                dmTotals[i] = -99;
        }

        if(!stillTicking)
        {
            S_LocalSound(SFX_BAREXP, 0);
            dmState++;
        }
    }
    else if(dmState == 4)
    {
        if(advanceState)
        {
            S_LocalSound(SFX_SLOP, 0);
            if(gameModeBits & GM_ANY_DOOM2)
            {
                initNoState();
            }
            else
            {
                initShowNextMap();
            }
        }
    }
    else if(dmState & 1)
    {
        if(!--cntPause)
        {
            dmState++;
            cntPause = TICRATE;
        }
    }
}

static void drawDeathmatchStats(Vec2i origin = Vec2i(DM_MATRIXX + DM_SPACINGX, DM_MATRIXY))
{
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, 1);

    FR_SetFont(FID(GF_FONTB));
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], 1);

    // Draw stat titles (top line).
    patchinfo_t info;
    if(R_GetPatchInfo(pTotal, &info))
    {
        WI_DrawPatch(pTotal, patchReplacementText(pTotal), Vec2i(DM_TOTALSX - info.geometry.size.width / 2, DM_MATRIXY - WI_SPACINGY + 10), ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
    }

    WI_DrawPatch(pKillers, patchReplacementText(pKillers), Vec2i(DM_KILLERSX, DM_KILLERSY), ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
    WI_DrawPatch(pVictims, patchReplacementText(pVictims), Vec2i(DM_VICTIMSX, DM_VICTIMSY), ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);

    for(int i = 0; i < NUMTEAMS; ++i)
    {
        if(teamInfo[i].playerCount > 0)
        {
            FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], 1);

            const patchid_t patchId  = pTeamBackgrounds[i];
            const String replacement = patchReplacementText(patchId);

            patchinfo_t info;
            R_GetPatchInfo(patchId, &info);
            WI_DrawPatch(patchId, replacement, Vec2i(origin.x - info.geometry.size.width / 2, DM_MATRIXY - WI_SPACINGY), ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
            WI_DrawPatch(patchId, replacement, Vec2i(DM_MATRIXX - info.geometry.size.width / 2, origin.y), ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);

            if(i == inPlayerTeam)
            {
                WI_DrawPatch(pFaceDead, patchReplacementText(pFaceDead), Vec2i(origin.x - info.geometry.size.width / 2, DM_MATRIXY - WI_SPACINGY), ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
                WI_DrawPatch(pFaceAlive, patchReplacementText(pFaceAlive), Vec2i(DM_MATRIXX - info.geometry.size.width / 2, origin.y), ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
            }

            // If more than 1 member, show the member count.
            if(1 > teamInfo[i].playerCount)
            {
                const String count = String::asText(teamInfo[i].playerCount);

                FR_SetFont(FID(GF_FONTA));
                drawText(count, Vec2i(origin.x   - info.geometry.size.width / 2 + 1, DM_MATRIXY - WI_SPACINGY + info.geometry.size.height - 8));
                drawText(count, Vec2i(DM_MATRIXX - info.geometry.size.width / 2 + 1, origin.y + info.geometry.size.height - 8));
            }
        }
        else
        {
            FR_SetColorAndAlpha(defFontRGB[CR], defFontRGB[CG], defFontRGB[CB], 1);

            const patchid_t patchId  = pTeamIcons[i];
            const String replacement = patchReplacementText(patchId);

            patchinfo_t info;
            R_GetPatchInfo(patchId, &info);
            WI_DrawPatch(patchId, replacement, Vec2i(origin.x - info.geometry.size.width / 2, DM_MATRIXY - WI_SPACINGY + 10), ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
            WI_DrawPatch(patchId, replacement, Vec2i(DM_MATRIXX - info.geometry.size.width / 2, origin.y + 10), ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
        }

        origin.x += DM_SPACINGX;
        origin.y += WI_SPACINGY;
    }

    // Draw stats.
    origin.y = DM_MATRIXY + 10;
    FR_SetFont(FID(GF_SMALL));
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], 1);
    const int w = FR_CharWidth('0');

    for(int i = 0; i < NUMTEAMS; ++i)
    {
        origin.x = DM_MATRIXX + DM_SPACINGX;
        if(teamInfo[i].playerCount > 0)
        {
            for(int k = 0; k < NUMTEAMS; ++k)
            {
                if(teamInfo[k].playerCount > 0)
                {
                    drawText(String::asText(dmFrags[i][k]), origin + Vec2i(w, 0), ALIGN_TOPRIGHT);
                }
                origin.x += DM_SPACINGX;
            }
            drawText(String::asText(dmTotals[i]), Vec2i(DM_TOTALSX + w, origin.y), ALIGN_TOPRIGHT);
        }

        origin.y += WI_SPACINGY;
    }

    DGL_Disable(DGL_TEXTURE_2D);
}

static void initNetgameStats()
{
    inState      = ILS_SHOW_STATS;
    advanceState = false;
    ngState      = 1;
    cntPause     = TICRATE;

    std::memset(cntKills,  0, sizeof(cntKills));
    std::memset(cntItems,  0, sizeof(cntItems));
    std::memset(cntSecret, 0, sizeof(cntSecret));
    std::memset(cntFrags,  0, sizeof(cntFrags));
    doFrags = 0;

    for(int i = 0; i < NUMTEAMS; ++i)
    {
        doFrags += teamInfo[i].totalFrags;
    }
    doFrags = !doFrags;

    beginAnimations();
}

static void updateNetgameStats()
{
    if(advanceState && ngState != 10)
    {
        advanceState = false;
        for(int i = 0; i < NUMTEAMS; ++i)
        {
            cntKills[i]  = (teamInfo[i].kills  * 100) / wbs->maxKills;
            cntItems[i]  = (teamInfo[i].items  * 100) / wbs->maxItems;
            cntSecret[i] = (teamInfo[i].secret * 100) / wbs->maxSecret;

            if(doFrags)
                cntFrags[i] = teamInfo[i].totalFrags;
        }

        S_LocalSound(SFX_BAREXP, 0);
        ngState = 10;
    }

    if(ngState == 2)
    {
        if(!(backgroundAnimCounter & 3))
        {
            S_LocalSound(SFX_PISTOL, 0);
        }

        bool stillTicking = false;

        for(int i = 0; i < NUMTEAMS; ++i)
        {
            cntKills[i] += 2;

            if(cntKills[i] >= (teamInfo[i].kills * 100) / wbs->maxKills)
                cntKills[i] = (teamInfo[i].kills * 100) / wbs->maxKills;
            else
                stillTicking = true;
        }

        if(!stillTicking)
        {
            S_LocalSound(SFX_BAREXP, 0);
            ngState++;
        }
    }
    else if(ngState == 4)
    {
        if(!(backgroundAnimCounter & 3))
        {
            S_LocalSound(SFX_PISTOL, 0);
        }

        bool stillTicking = false;

        for(int i = 0; i < NUMTEAMS; ++i)
        {
            cntItems[i] += 2;
            if(cntItems[i] >= (teamInfo[i].items * 100) / wbs->maxItems)
                cntItems[i] = (teamInfo[i].items * 100) / wbs->maxItems;
            else
                stillTicking = true;
        }

        if(!stillTicking)
        {
            S_LocalSound(SFX_BAREXP, 0);
            ngState++;
        }
    }
    else if(ngState == 6)
    {
        if(!(backgroundAnimCounter & 3))
        {
            S_LocalSound(SFX_PISTOL, 0);
        }

        bool stillTicking = false;

        for(int i = 0; i < NUMTEAMS; ++i)
        {
            cntSecret[i] += 2;

            if(cntSecret[i] >= (teamInfo[i].secret * 100) / wbs->maxSecret)
                cntSecret[i] = (teamInfo[i].secret * 100) / wbs->maxSecret;
            else
                stillTicking = true;
        }

        if(!stillTicking)
        {
            S_LocalSound(SFX_BAREXP, 0);
            ngState += 1 + 2 * !doFrags;
        }
    }
    else if(ngState == 8)
    {
        if(!(backgroundAnimCounter & 3))
        {
            S_LocalSound(SFX_PISTOL, 0);
        }

        bool stillTicking = false;

        for(int i = 0; i < NUMTEAMS; ++i)
        {
            cntFrags[i] += 1;

            const int fsum = teamInfo[i].totalFrags;
            if(cntFrags[i] >= fsum)
                cntFrags[i] = fsum;
            else
                stillTicking = true;
        }

        if(!stillTicking)
        {
            S_LocalSound(SFX_PLDETH, 0);
            ngState++;
        }
    }
    else if(ngState == 10)
    {
        if(advanceState)
        {
            S_LocalSound(SFX_SGCOCK, 0);
            if(gameModeBits & GM_ANY_DOOM2)
            {
                initNoState();
            }
            else
            {
                initShowNextMap();
            }
        }
    }
    else if(ngState & 1)
    {
        if(!--cntPause)
        {
            ngState++;
            cntPause = TICRATE;
        }
    }
}

static void drawNetgameStats()
{
#define ORIGINX             (NG_STATSX + starWidth/2 + NG_STATSX * !doFrags)

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, 1);

    FR_SetFont(FID(GF_FONTB));
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], 1);

    const int pwidth = FR_CharWidth('%');
    patchinfo_t info;
    R_GetPatchInfo(pFaceAlive, &info);
    const int starWidth = info.geometry.size.width;

    // Draw stat titles (top line).
    R_GetPatchInfo(pKills, &info);
    WI_DrawPatch(pKills, patchReplacementText(pKills), Vec2i(ORIGINX + NG_SPACINGX, NG_STATSY), ALIGN_TOPRIGHT, 0, DTF_NO_TYPEIN);
    int y = NG_STATSY + info.geometry.size.height;

    WI_DrawPatch(pItems, patchReplacementText(pItems), Vec2i(ORIGINX + 2 * NG_SPACINGX, NG_STATSY), ALIGN_TOPRIGHT, 0, DTF_NO_TYPEIN);
    WI_DrawPatch(pSecret, patchReplacementText(pSecret), Vec2i(ORIGINX + 3 * NG_SPACINGX, NG_STATSY), ALIGN_TOPRIGHT, 0, DTF_NO_TYPEIN);
    if(doFrags)
    {
        WI_DrawPatch(pFrags, patchReplacementText(pFrags), Vec2i(ORIGINX + 4 * NG_SPACINGX, NG_STATSY), ALIGN_TOPRIGHT, 0, DTF_NO_TYPEIN);
    }

    // Draw stats.
    for(int i = 0; i < NUMTEAMS; ++i)
    {
        if(!teamInfo[i].playerCount)
            continue;

        FR_SetFont(FID(GF_FONTA));
        FR_SetColorAndAlpha(1, 1, 1, 1);

        int x = ORIGINX;

        patchinfo_t info;
        R_GetPatchInfo(pTeamBackgrounds[i], &info);
        WI_DrawPatch(pTeamBackgrounds[i], patchReplacementText(pTeamBackgrounds[i]), Vec2i(x - info.geometry.size.width, y), ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);

        // If more than 1 member, show the member count.
        if(1 != teamInfo[i].playerCount)
        {
            drawText(String::asText(teamInfo[i].playerCount),
                     Vec2i(x - info.geometry.size.width + 1,
                              y + info.geometry.size.height - 8), ALIGN_TOPLEFT);
        }

        FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], 1);

        if(i == inPlayerTeam)
        {
            WI_DrawPatch(pFaceAlive, patchReplacementText(pFaceAlive), Vec2i(x - info.geometry.size.width, y), ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
        }
        x += NG_SPACINGX;

        FR_SetFont(FID(GF_SMALL));
        drawPercent(cntKills[i], Vec2i(x - pwidth, y + 10));
        x += NG_SPACINGX;

        drawPercent(cntItems[i], Vec2i(x - pwidth, y + 10));
        x += NG_SPACINGX;

        drawPercent(cntSecret[i], Vec2i(x - pwidth, y + 10));
        x += NG_SPACINGX;

        if(doFrags)
        {
            drawText(String::asText(cntFrags[i]), Vec2i(x, y + 10), ALIGN_TOPRIGHT);
        }

        y += WI_SPACINGY;
    }

    DGL_Disable(DGL_TEXTURE_2D);

#undef ORIGINX
}

static void drawSinglePlayerStats()
{
    const int lh = (3 * FR_CharHeight('0')) / 2; // Line height.

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, 1);

    FR_SetFont(FID(GF_FONTB));
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], 1);

    WI_DrawPatch(pKills   , patchReplacementText(pKills)   , Vec2i(SP_STATSX, SP_STATSY)         , ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
    WI_DrawPatch(pItems   , patchReplacementText(pItems)   , Vec2i(SP_STATSX, SP_STATSY + lh)    , ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
    WI_DrawPatch(pSecretSP, patchReplacementText(pSecretSP), Vec2i(SP_STATSX, SP_STATSY + 2 * lh), ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
    WI_DrawPatch(pTime    , patchReplacementText(pTime)    , Vec2i(SP_TIMEX, SP_TIMEY)           , ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
    if(wbs->parTime != -1)
    {
        WI_DrawPatch(pPar, patchReplacementText(pPar), Vec2i(SCREENWIDTH / 2 + SP_TIMEX, SP_TIMEY), ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
    }

    FR_SetFont(FID(GF_SMALL));
    drawPercent(cntKills[0] , Vec2i(SCREENWIDTH - SP_STATSX, SP_STATSY));
    drawPercent(cntItems[0] , Vec2i(SCREENWIDTH - SP_STATSX, SP_STATSY + lh));
    drawPercent(cntSecret[0], Vec2i(SCREENWIDTH - SP_STATSX, SP_STATSY + 2 * lh));

    if(cntTime >= 0)
    {
        drawTime(Vec2i(SCREENWIDTH / 2 - SP_TIMEX, SP_TIMEY), cntTime / TICRATE);
    }

    if(wbs->parTime != -1 && cntPar >= 0)
    {
        drawTime(Vec2i(SCREENWIDTH - SP_TIMEX, SP_TIMEY), cntPar / TICRATE);
    }

    DGL_Disable(DGL_TEXTURE_2D);
}

static void initShowStats()
{
    inState      = ILS_SHOW_STATS;
    advanceState = false;
    spState      = 1;
    cntKills[0]  = cntItems[0] = cntSecret[0] = -1;
    cntTime      = cntPar = -1;
    cntPause     = TICRATE;

    beginAnimations();
}

static void tickShowStats()
{
    if(gfw_Rule(deathmatch))
    {
        updateDeathmatchStats();
        return;
    }
    else if(IS_NETGAME)
    {
        updateNetgameStats();
        return;
    }

    if(advanceState && spState != 10)
    {
        advanceState = false;
        cntKills[0]  = (inPlayerInfo[inPlayerNum].kills  * 100) / wbs->maxKills;
        cntItems[0]  = (inPlayerInfo[inPlayerNum].items  * 100) / wbs->maxItems;
        cntSecret[0] = (inPlayerInfo[inPlayerNum].secret * 100) / wbs->maxSecret;
        cntTime = inPlayerInfo[inPlayerNum].time;
        if(wbs->parTime != -1)
            cntPar = wbs->parTime;
        S_LocalSound(SFX_BAREXP, 0);
        spState = 10;
    }

    if(spState == 2)
    {
        cntKills[0] += 2;

        if(!(backgroundAnimCounter & 3))
            S_LocalSound(SFX_PISTOL, 0);

        if(cntKills[0] >= (inPlayerInfo[inPlayerNum].kills * 100) / wbs->maxKills)
        {
            cntKills[0] = (inPlayerInfo[inPlayerNum].kills * 100) / wbs->maxKills;
            S_LocalSound(SFX_BAREXP, 0);
            spState++;
        }
    }
    else if(spState == 4)
    {
        cntItems[0] += 2;

        if(!(backgroundAnimCounter & 3))
            S_LocalSound(SFX_PISTOL, 0);

        if(cntItems[0] >= (inPlayerInfo[inPlayerNum].items * 100) / wbs->maxItems)
        {
            cntItems[0] = (inPlayerInfo[inPlayerNum].items * 100) / wbs->maxItems;
            S_LocalSound(SFX_BAREXP, 0);
            spState++;
        }
    }
    else if(spState == 6)
    {
        cntSecret[0] += 2;

        if(!(backgroundAnimCounter & 3))
            S_LocalSound(SFX_PISTOL, 0);

        if(cntSecret[0] >= (inPlayerInfo[inPlayerNum].secret * 100) / wbs->maxSecret)
        {
            cntSecret[0] = (inPlayerInfo[inPlayerNum].secret * 100) / wbs->maxSecret;
            S_LocalSound(SFX_BAREXP, 0);
            spState++;
        }
    }
    else if(spState == 8)
    {
        if(!(backgroundAnimCounter & 3))
            S_LocalSound(SFX_PISTOL, 0);

        if(cntTime == -1)
            cntTime = 0;
        cntTime += TICRATE * 3;

        // Par time might not be defined so count up and stop on play time instead.
        if(cntTime >= inPlayerInfo[inPlayerNum].time)
        {
            cntTime = inPlayerInfo[inPlayerNum].time;
            cntPar = wbs->parTime;
            S_LocalSound(SFX_BAREXP, 0);
            spState++;
        }

        if(wbs->parTime != -1)
        {
            if(cntPar == -1)
                cntPar = 0;
            cntPar += TICRATE * 3;

            if(cntPar >= wbs->parTime)
                cntPar = wbs->parTime;
        }
    }
    else if(spState == 10)
    {
        if(advanceState)
        {
            S_LocalSound(SFX_SGCOCK, 0);

            if(gameModeBits & GM_ANY_DOOM2)
                initNoState();
            else
                initShowNextMap();
        }
    }
    else if(spState & 1)
    {
        if(!--cntPause)
        {
            spState++;
            cntPause = TICRATE;
        }
    }
}

static void drawStats()
{
    if(gfw_Rule(deathmatch))
    {
        drawDeathmatchStats();
    }
    else if(IS_NETGAME)
    {
        drawNetgameStats();
    }
    else
    {
        drawSinglePlayerStats();
    }
}

/// Check for button presses to skip delays.
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
                    NetCl_PlayerActionRequest(player, GPA_FIRE, 0);
                else
                    IN_SkipToNext();
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
                    NetCl_PlayerActionRequest(player, GPA_USE, 0);
                else
                    IN_SkipToNext();
            }
            player->useDown = true;
        }
        else
        {
            player->useDown = false;
        }
    }
}

void IN_Ticker()
{
    ++backgroundAnimCounter;
    animateBackground();

    maybeAdvanceState();
    switch(inState)
    {
    case ILS_SHOW_STATS:    tickShowStats();   break;
    case ILS_SHOW_NEXTMAP:  tickShowNextMap(); break;
    case ILS_NONE:          tickNoState();     break;

    default:
        DE_ASSERT_FAIL("IN_Ticker: Unknown intermission state");
        break;
    }
}

static void loadData()
{
    const String episodeId = gfw_Session()->episodeId();

    // Determine which patch to use for the background.
    pBackground = R_DeclarePatch(backgroundPatchForEpisode(episodeId));

    // Are there any animation states to initialize?
    animStates.clear();
    if(const Animations *anims = animationsForEpisode(episodeId))
    {
        animStates.reserve(anims->count());
        for(const Animation &def : *anims)
        {
            animStates.append(wianimstate_t());
            wianimstate_t &state = animStates.last();
            for(const String &patchName : def.patchNames)
            {
                state.patches << R_DeclarePatch(patchName);
            }
        }
    }

    pFinished  = R_DeclarePatch("WIF");
    pEntering  = R_DeclarePatch("WIENTER");
    pKills     = R_DeclarePatch("WIOSTK");
    pSecret    = R_DeclarePatch("WIOSTS");
    pSecretSP  = R_DeclarePatch("WISCRT2");
    pItems     = R_DeclarePatch("WIOSTI");
    pFrags     = R_DeclarePatch("WIFRGS");
    pTime      = R_DeclarePatch("WITIME");
    pSucks     = R_DeclarePatch("WISUCKS");
    pPar       = R_DeclarePatch("WIPAR");
    pKillers   = R_DeclarePatch("WIKILRS");
    pVictims   = R_DeclarePatch("WIVCTMS");
    pTotal     = R_DeclarePatch("WIMSTT");
    pFaceAlive = R_DeclarePatch("STFST01");
    pFaceDead  = R_DeclarePatch("STFDEAD0");

    if(!(gameModeBits & GM_ANY_DOOM2))
    {
        pYouAreHereRight = R_DeclarePatch("WIURH0");
        pYouAreHereLeft  = R_DeclarePatch("WIURH1");
        pSplat           = R_DeclarePatch("WISPLAT");
    }

    char name[9];
    for(int i = 0; i < NUMTEAMS; ++i)
    {
        sprintf(name, "STPB%d", i);
        pTeamBackgrounds[i] = R_DeclarePatch(name);

        sprintf(name, "WIBP%d", i + 1);
        pTeamIcons[i] = R_DeclarePatch(name);
    }
}

void IN_Drawer()
{
    /// @todo Kludge: dj: Clearly a kludge but why?
    if(ILS_NONE == inState)
    {
        drawYouAreHere = true;
    }
    /// kludge end.

    dgl_borderedprojectionstate_t bp;
    GL_ConfigureBorderedProjection(&bp, BPF_OVERDRAW_MASK | BPF_OVERDRAW_CLIP,
        SCREENWIDTH, SCREENHEIGHT, Get(DD_WINDOW_WIDTH), Get(DD_WINDOW_HEIGHT), scalemode_t(cfg.common.inludeScaleMode));
    GL_BeginBorderedProjection(&bp);

    drawBackground();

    if(ILS_SHOW_STATS != inState)
    {
        drawLocationMarks();
        drawEnteringTitle();
    }
    else
    {
        drawFinishedTitle();
        drawStats();
    }

    GL_EndBorderedProjection(&bp);
}

static void initVariables(const wbstartstruct_t &wbstartstruct)
{
    wbs = &wbstartstruct;

    advanceState = false;
    stateCounter = backgroundAnimCounter = 0;
    inPlayerNum  = wbs->pNum;
    inPlayerTeam = cfg.playerColor[wbs->pNum];
    inPlayerInfo = wbs->plyr;
}

void IN_Begin(const wbstartstruct_t &wbstartstruct)
{
    initVariables(wbstartstruct);
    loadData();

    // Calculate team stats.
    std::memset(teamInfo, 0, sizeof(teamInfo));
    for(int i = 0; i < NUMTEAMS; ++i)
    {
        TeamInfo *tin = &teamInfo[i];

        for(int k = 0; k < MAXPLAYERS; ++k)
        {
            // Is the player in this team?
            if(!inPlayerInfo[k].inGame || cfg.playerColor[k] != i)
                continue;

            ++tin->playerCount;

            // Check the frags.
            for(int m = 0; m < MAXPLAYERS; ++m)
            {
                tin->frags[cfg.playerColor[m]] += inPlayerInfo[k].frags[m];
            }

            // Counters.
            if(inPlayerInfo[k].items > tin->items)
                tin->items = inPlayerInfo[k].items;
            if(inPlayerInfo[k].kills > tin->kills)
                tin->kills = inPlayerInfo[k].kills;
            if(inPlayerInfo[k].secret > tin->secret)
                tin->secret = inPlayerInfo[k].secret;
        }

        // Calculate team's total frags.
        for(int k = 0; k < NUMTEAMS; ++k)
        {
            if(k == i) // Suicides are negative frags.
                tin->totalFrags -= tin->frags[k];
            else
                tin->totalFrags += tin->frags[k];
        }
    }

    if(gfw_Rule(deathmatch))
    {
        initDeathmatchStats();
        beginAnimations();
    }
    else if(IS_NETGAME)
    {
        initNetgameStats();
        beginAnimations();
    }
    else
    {
        initShowStats();
    }
}

void IN_SetState(interludestate_t st)
{
    switch(st)
    {
    case ILS_SHOW_STATS:    initShowStats();   break;
    case ILS_SHOW_NEXTMAP:  initShowNextMap(); break;
    case ILS_NONE:          initNoState();     break;

    default:
        DE_ASSERT_FAIL("IN_SetState: Unknown intermission state");
        break;
    }
}

void IN_SkipToNext()
{
    advanceState = true;
}

void IN_ConsoleRegister()
{
    C_VAR_BYTE("inlude-stretch",            &cfg.common.inludeScaleMode,           0, SCALEMODE_FIRST, SCALEMODE_LAST);
    C_VAR_INT ("inlude-patch-replacement",  &cfg.common.inludePatchReplaceMode,    0, 0, 1);
}
