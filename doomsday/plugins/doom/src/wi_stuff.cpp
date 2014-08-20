/** @file wi_stuff.cpp  DOOM specific intermission screens.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "wi_stuff.h"

#include <cstdio>
#include <cctype>
#include <cstring>
#include <QList>
#include "d_net.h"
#include "gamesession.h"
#include "hu_stuff.h"
#include "p_mapsetup.h"
#include "p_start.h"

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
        Vector2i origin;
        int tics;                     ///< Number of tics each frame of the animation lasts for.
        StringList patchNames;        ///< For each frame of the animation.
        de::Uri mapUri;               ///< If path is not zero-length the animation should only be displayed on this map.
        interludestate_t beginState;  ///< State at which this animation begins/becomes visible.

        Animation(Vector2i const &origin, int tics, StringList patchNames,
                  de::Uri const &mapUri       = de::Uri("Maps:", RC_NULL),
                  interludestate_t beginState = ILS_SHOW_STATS)
            : origin    (origin)
            , tics      (tics)
            , patchNames(patchNames)
            , mapUri    (mapUri)
            , beginState(beginState)
        {}
    };
    typedef QList<Animation> Animations;
    static Animations episode1Anims;
    static Animations episode2Anims;
    static Animations episode3Anims;

    struct Location
    {
        Vector2i origin;
        de::Uri mapUri;

        Location(Vector2i const &origin, de::Uri const &mapUri)
            : origin(origin)
            , mapUri(mapUri)
        {}
    };
    typedef QList<Location> Locations;
    static Locations episode1Locations;
    static Locations episode2Locations;
    static Locations episode3Locations;

    struct wianimstate_t
    {
        int nextTic;  ///< Next tic on which to progress the animation.
        int frame;    ///< Current frame number (index to patches); otherwise @c -1 (not yet begun).

        /// Graphics for each frame of the animation.
        typedef QList<patchid_t> Patches;
        Patches patches;
    };
    static wianimstate_t *animStates;

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
    static inline char const *patchReplacementText(patchid_t patchId, char const *text = 0)
    {
        return Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.inludePatchReplaceMode),
                                         patchId, text);
    }

    static void drawPercent(int x, int y, int p)
    {
        if(p < 0) return;

        Point2Raw origin(x, y);
        char buf[20]; dd_snprintf(buf, 20, "%i", p);
        FR_DrawChar3('%', &origin, ALIGN_TOPLEFT, DTF_NO_TYPEIN);
        FR_DrawText3(buf, &origin, ALIGN_TOPRIGHT, DTF_NO_TYPEIN);
    }

    /**
     * Display map completion time and par, or "sucks" message if overflow.
     */
    static void drawTime(Vector2i origin, int t)
    {
        if(t < 0) return;

        if(t <= 61 * 59)
        {
            origin.x -= 22;

            int const seconds = t % 60;
            int const minutes = t / 60 % 60;

            char buf[20];
            FR_DrawCharXY3(':', origin.x, origin.y, ALIGN_TOPLEFT, DTF_NO_TYPEIN);
            if(minutes > 0)
            {
                dd_snprintf(buf, 20, "%d", minutes);
                FR_DrawTextXY3(buf, origin.x, origin.y, ALIGN_TOPRIGHT, DTF_NO_TYPEIN);
            }

            dd_snprintf(buf, 20, "%02d", seconds);
            FR_DrawTextXY3(buf, origin.x + FR_CharWidth(':'), origin.y, ALIGN_TOPLEFT, DTF_NO_TYPEIN);

            return;
        }

        // "sucks"
        patchinfo_t info;
        if(!R_GetPatchInfo(pSucks, &info)) return;

        WI_DrawPatch(pSucks, patchReplacementText(pSucks), Vector2i(origin.x - info.geometry.size.width, origin.y), ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
    }
}

using namespace internal;

void WI_Init()
{
    // Already been here?
    if(!episode1Anims.isEmpty()) return;

    if(gameModeBits & GM_ANY_DOOM2) return;

    episode1Anims
        << Animation( Vector2i(224, 104), 11, StringList() << String("wia00000") << String("wia00001") << String("wia00002") )
        << Animation( Vector2i(184, 160), 11, StringList() << String("wia00100") << String("wia00101") << String("wia00102") )
        << Animation( Vector2i(112, 136), 11, StringList() << String("wia00200") << String("wia00201") << String("wia00202") )
        << Animation( Vector2i( 72, 112), 11, StringList() << String("wia00300") << String("wia00301") << String("wia00302") )
        << Animation( Vector2i( 88,  96), 11, StringList() << String("wia00400") << String("wia00401") << String("wia00402") )
        << Animation( Vector2i( 64,  48), 11, StringList() << String("wia00500") << String("wia00501") << String("wia00502") )
        << Animation( Vector2i(192,  40), 11, StringList() << String("wia00600") << String("wia00601") << String("wia00602") )
        << Animation( Vector2i(136,  16), 11, StringList() << String("wia00700") << String("wia00701") << String("wia00702") )
        << Animation( Vector2i( 80,  16), 11, StringList() << String("wia00800") << String("wia00801") << String("wia00802") )
        << Animation( Vector2i( 64,  24), 11, StringList() << String("wia00900") << String("wia00901") << String("wia00902") );

    episode1Locations
        << Location( Vector2i(185, 164), de::Uri("Maps:E1M1", RC_NULL) )
        << Location( Vector2i(148, 143), de::Uri("Maps:E1M2", RC_NULL) )
        << Location( Vector2i( 69, 122), de::Uri("Maps:E1M3", RC_NULL) )
        << Location( Vector2i(209, 102), de::Uri("Maps:E1M4", RC_NULL) )
        << Location( Vector2i(116,  89), de::Uri("Maps:E1M5", RC_NULL) )
        << Location( Vector2i(166,  55), de::Uri("Maps:E1M6", RC_NULL) )
        << Location( Vector2i( 71,  56), de::Uri("Maps:E1M7", RC_NULL) )
        << Location( Vector2i(135,  29), de::Uri("Maps:E1M8", RC_NULL) )
        << Location( Vector2i( 71,  24), de::Uri("Maps:E1M9", RC_NULL) );

    episode2Anims
        << Animation( Vector2i(128, 136),  0, StringList() << String("wia10000"), de::Uri("Maps:E2M1", RC_NULL) )
        << Animation( Vector2i(128, 136),  0, StringList() << String("wia10100"), de::Uri("Maps:E2M2", RC_NULL) )
        << Animation( Vector2i(128, 136),  0, StringList() << String("wia10200"), de::Uri("Maps:E2M3", RC_NULL) )
        << Animation( Vector2i(128, 136),  0, StringList() << String("wia10300"), de::Uri("Maps:E2M4", RC_NULL) )
        << Animation( Vector2i(128, 136),  0, StringList() << String("wia10400"), de::Uri("Maps:E2M5", RC_NULL) )
        << Animation( Vector2i(128, 136),  0, StringList() << String("wia10500"), de::Uri("Maps:E2M6", RC_NULL) )
        << Animation( Vector2i(128, 136),  0, StringList() << String("wia10600"), de::Uri("Maps:E2M7", RC_NULL) )
        << Animation( Vector2i(192, 144), 11, StringList() << String("wia10700") << String("wia10701") << String("wia10702"), de::Uri("Maps:E2M8", RC_NULL), ILS_SHOW_NEXTMAP )
        << Animation( Vector2i(128, 136),  0, StringList() << String("wia10400"), de::Uri("Maps:E2M8", RC_NULL) );

    episode2Locations
        << Location( Vector2i(254,  25), de::Uri("Maps:E2M1", RC_NULL) )
        << Location( Vector2i( 97,  50), de::Uri("Maps:E2M2", RC_NULL) )
        << Location( Vector2i(188,  64), de::Uri("Maps:E2M3", RC_NULL) )
        << Location( Vector2i(128,  78), de::Uri("Maps:E2M4", RC_NULL) )
        << Location( Vector2i(214,  92), de::Uri("Maps:E2M5", RC_NULL) )
        << Location( Vector2i(133, 130), de::Uri("Maps:E2M6", RC_NULL) )
        << Location( Vector2i(208, 136), de::Uri("Maps:E2M7", RC_NULL) )
        << Location( Vector2i(148, 140), de::Uri("Maps:E2M8", RC_NULL) )
        << Location( Vector2i(235, 158), de::Uri("Maps:E2M9", RC_NULL) );

    episode3Anims
        << Animation( Vector2i(104, 168), 11, StringList() << String("wia20000") << String("wia20001") << String("wia20002") )
        << Animation( Vector2i( 40, 136), 11, StringList() << String("wia20100") << String("wia20101") << String("wia20102") )
        << Animation( Vector2i(160,  96), 11, StringList() << String("wia20200") << String("wia20201") << String("wia20202") )
        << Animation( Vector2i(104,  80), 11, StringList() << String("wia20300") << String("wia20301") << String("wia20302") )
        << Animation( Vector2i(120,  32), 11, StringList() << String("wia20400") << String("wia20401") << String("wia20402") )
        << Animation( Vector2i( 40,   0),  8, StringList() << String("wia20500") << String("wia20501") << String("wia20502") );

    episode3Locations
        << Location( Vector2i(156, 168), de::Uri("Maps:E3M1", RC_NULL) )
        << Location( Vector2i( 48, 154), de::Uri("Maps:E3M2", RC_NULL) )
        << Location( Vector2i(174,  95), de::Uri("Maps:E3M3", RC_NULL) )
        << Location( Vector2i(265,  75), de::Uri("Maps:E3M4", RC_NULL) )
        << Location( Vector2i(130,  48), de::Uri("Maps:E3M5", RC_NULL) )
        << Location( Vector2i(279,  23), de::Uri("Maps:E3M6", RC_NULL) )
        << Location( Vector2i(198,  48), de::Uri("Maps:E3M7", RC_NULL) )
        << Location( Vector2i(140,  25), de::Uri("Maps:E3M8", RC_NULL) )
        << Location( Vector2i(281, 136), de::Uri("Maps:E3M9", RC_NULL) );
}

void WI_Shutdown()
{
    Z_Free(animStates); animStates = 0;
}

static String backgroundPatchForEpisode(String const &episodeId)
{
    int const oldEpisodeNum = episodeId.toInt();
    if((gameModeBits & GM_ANY_DOOM2) || (gameMode == doom_ultimate && oldEpisodeNum > 2))
    {
        return "INTERPIC";
    }
    return String("WIMAP%1").arg(oldEpisodeNum);
}

static Animations const *animationsForEpisode(String const &episodeId)
{
    if(episodeId == "1") return &episode1Anims;
    if(episodeId == "2") return &episode2Anims;
    if(episodeId == "3") return &episode3Anims;
    return nullptr; // Not found.
}

static Locations const *locationsForEpisode(String const &episodeId)
{
    if(episodeId == "1") return &episode1Locations;
    if(episodeId == "2") return &episode2Locations;
    if(episodeId == "3") return &episode3Locations;
    return nullptr; // Not found.
}

static Location const *tryFindLocationForMap(Locations const *locations, de::Uri const &mapUri)
{
    if(locations)
    {
        for(Location const &loc : *locations)
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
static wbstartstruct_t const *wbs;
static wbplayerstruct_t const *inPlayerInfo;

static void drawBackground()
{
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, 1);

    GL_DrawPatchXY3(pBackground, 0, 0, ALIGN_TOPLEFT, DPF_NO_OFFSET);

    if(Animations const *anims = animationsForEpisode(COMMON_GAMESESSION->episodeId()))
    {
        FR_SetFont(FID(GF_FONTB));
        FR_LoadDefaultAttrib();

        for(int i = 0; i < anims->count(); ++i)
        {
            Animation const &def = (*anims)[i];
            wianimstate_t *state = &animStates[i];

            // Has the animation begun yet?
            if(state->frame < 0) continue;

            patchid_t patchId = state->patches[state->frame];
            WI_DrawPatch(patchId, patchReplacementText(patchId), def.origin, ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
        }
    }

    DGL_Disable(DGL_TEXTURE_2D);
}

static void drawFinishedTitle(Vector2i origin = Vector2i(SCREENWIDTH / 2, WI_TITLEY))
{
    DENG2_ASSERT(!wbs->currentMap.isEmpty());

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, 1);

    FR_SetFont(FID(GF_FONTB));
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(defFontRGB[CR], defFontRGB[CG], defFontRGB[CB], 1);

    // Draw map title.
    String const title         = G_MapTitle(wbs->currentMap);
    patchid_t const titlePatch = G_MapTitlePatch(wbs->currentMap);
    WI_DrawPatch(titlePatch, patchReplacementText(titlePatch, title.toUtf8().constData()), origin, ALIGN_TOP, 0, DTF_NO_TYPEIN);
    patchinfo_t info;
    if(R_GetPatchInfo(titlePatch, &info))
    {
        origin.y += (5 * info.geometry.size.height) / 4;
    }

    // Draw "Finished!"
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], 1);
    WI_DrawPatch(pFinished, patchReplacementText(pFinished), origin, ALIGN_TOP, 0, DTF_NO_TYPEIN);

    DGL_Disable(DGL_TEXTURE_2D);
}

static void drawEnteringTitle(Vector2i origin = Vector2i(SCREENWIDTH / 2, WI_TITLEY))
{
    if(wbs->nextMap.isEmpty()) return;

    /// @kludge We need to properly externalize the map progression.
    if((gameModeBits & (GM_DOOM2|GM_DOOM2_PLUT|GM_DOOM2_TNT)) &&
       wbs->nextMap.path() == "MAP31")
    {
        return;
    }
    /// kludge end.

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, 1);

    FR_SetFont(FID(GF_FONTB));
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], 1);

    // Draw "Entering".
    WI_DrawPatch(pEntering, patchReplacementText(pEntering), origin, ALIGN_TOP, 0, DTF_NO_TYPEIN);

    // Draw map title.
    String const title         = G_MapTitle(wbs->nextMap);
    patchid_t const titlePatch = G_MapTitlePatch(wbs->nextMap);
    patchinfo_t info;
    if(R_GetPatchInfo(titlePatch, &info))
    {
        origin.y += (5 * info.geometry.size.height) / 4;
    }
    FR_SetColorAndAlpha(defFontRGB[CR], defFontRGB[CG], defFontRGB[CB], 1);
    WI_DrawPatch(titlePatch, patchReplacementText(titlePatch, title.toUtf8().constData()),
                 origin, ALIGN_TOP, 0, DTF_NO_TYPEIN);

    DGL_Disable(DGL_TEXTURE_2D);
}

static bool patchFits(patchid_t patchId, Vector2i const &origin)
{
    patchinfo_t info;
    if(!R_GetPatchInfo(patchId, &info)) return false;

    int const left   = origin.x + info.geometry.origin.x;
    int const top    = origin.y + info.geometry.origin.y;
    int const right  = left + info.geometry.size.width;
    int const bottom = top + info.geometry.size.height;
    return (left >= 0 && right < SCREENWIDTH && top >= 0 && bottom < SCREENHEIGHT);
}

static patchid_t chooseYouAreHerePatch(Vector2i const &origin)
{
    if(patchFits(pYouAreHereRight, origin))
        return pYouAreHereRight;

    if(patchFits(pYouAreHereLeft, origin))
        return pYouAreHereLeft;

    return 0; // None fits.
}

static void drawPatchIfFits(patchid_t patchId, Vector2i const &origin)
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
    Animations const *anims = animationsForEpisode(COMMON_GAMESESSION->episodeId());
    if(!anims) return;

    for(int i = 0; i < anims->count(); ++i)
    {
        Animation const &def = (*anims)[i];
        wianimstate_t *state = &animStates[i];

        // Is the animation active for the current map?
        if(!def.mapUri.path().isEmpty() && wbs->nextMap != def.mapUri)
            continue;

        // Already begun?
        if(state->frame >= 0) continue;

        // Is it time to begin the animation?
        if(def.beginState != inState) continue;

        state->frame = 0;

        // Determine when to animate the first frame.
        if(!def.mapUri.path().isEmpty())
        {
            state->nextTic = backgroundAnimCounter + 1 + def.tics;
        }
        else
        {
            state->nextTic = backgroundAnimCounter + 1 + (M_Random() % def.tics);
        }
    }
}

static void animateBackground()
{
    Animations const *anims = animationsForEpisode(COMMON_GAMESESSION->episodeId());
    if(!anims) return;

    for(int i = 0; i < anims->count(); ++i)
    {
        Animation const &def = (*anims)[i];
        wianimstate_t &state = animStates[i];

        // Is the animation active for the current map?
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

void WI_End()
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

        WI_End();
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

static void drawLocationMarks()
{
    Locations const *locations = locationsForEpisode(COMMON_GAMESESSION->episodeId());
    if(!locations) return;

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, 1);
    FR_SetFont(FID(GF_FONTB));
    FR_LoadDefaultAttrib();

    // Draw a splat on taken cities.
    int last = G_MapNumberFor(wbs->currentMap);
    if(last == 8) last = G_MapNumberFor(wbs->nextMap) - 1;

    for(int i = 0; i <= last; ++i)
    {
        Location const &loc = (*locations)[i];
        drawPatchIfFits(pSplat, loc.origin);
    }

    // Splat the secret map?
    if(wbs->didSecret)
    {
        drawPatchIfFits(pSplat, (*locations)[8].origin);
    }

    if(drawYouAreHere)
    {
        if(Location const *loc = tryFindLocationForMap(locations, wbs->nextMap))
        {
            patchid_t const yahPatchId = chooseYouAreHerePatch(loc->origin);
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

static void drawDeathmatchStats(Vector2i origin = Vector2i(DM_MATRIXX + DM_SPACINGX, DM_MATRIXY))
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
        WI_DrawPatch(pTotal, patchReplacementText(pTotal), Vector2i(DM_TOTALSX - info.geometry.size.width / 2, DM_MATRIXY - WI_SPACINGY + 10), ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
    }

    WI_DrawPatch(pKillers, patchReplacementText(pKillers), Vector2i(DM_KILLERSX, DM_KILLERSY), ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
    WI_DrawPatch(pVictims, patchReplacementText(pVictims), Vector2i(DM_VICTIMSX, DM_VICTIMSY), ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);

    for(int i = 0; i < NUMTEAMS; ++i)
    {
        if(teamInfo[i].playerCount > 0)
        {
            FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], 1);

            patchid_t const patchId = pTeamBackgrounds[i];
            char const *replacement = patchReplacementText(patchId);

            patchinfo_t info;
            R_GetPatchInfo(patchId, &info);
            WI_DrawPatch(patchId, replacement, Vector2i(origin.x - info.geometry.size.width / 2, DM_MATRIXY - WI_SPACINGY), ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
            WI_DrawPatch(patchId, replacement, Vector2i(DM_MATRIXX - info.geometry.size.width / 2, origin.y), ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);

            if(i == inPlayerTeam)
            {
                WI_DrawPatch(pFaceDead, patchReplacementText(pFaceDead), Vector2i(origin.x - info.geometry.size.width / 2, DM_MATRIXY - WI_SPACINGY), ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
                WI_DrawPatch(pFaceAlive, patchReplacementText(pFaceAlive), Vector2i(DM_MATRIXX - info.geometry.size.width / 2, origin.y), ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
            }

            // If more than 1 member, show the member count.
            if(1 > teamInfo[i].playerCount)
            {
                char tmp[20]; sprintf(tmp, "%i", teamInfo[i].playerCount);

                FR_SetFont(FID(GF_FONTA));
                FR_DrawTextXY3(tmp, origin.x - info.geometry.size.width / 2 + 1, DM_MATRIXY - WI_SPACINGY + info.geometry.size.height - 8, ALIGN_TOPLEFT, DTF_NO_TYPEIN);
                FR_DrawTextXY3(tmp, DM_MATRIXX - info.geometry.size.width / 2 + 1, origin.y + info.geometry.size.height - 8, ALIGN_TOPLEFT, DTF_NO_TYPEIN);
            }
        }
        else
        {
            FR_SetColorAndAlpha(defFontRGB[CR], defFontRGB[CG], defFontRGB[CB], 1);

            patchid_t const patchId = pTeamIcons[i];
            char const *replacement = patchReplacementText(patchId);

            patchinfo_t info;
            R_GetPatchInfo(patchId, &info);
            WI_DrawPatch(patchId, replacement, Vector2i(origin.x - info.geometry.size.width / 2, DM_MATRIXY - WI_SPACINGY + 10), ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
            WI_DrawPatch(patchId, replacement, Vector2i(DM_MATRIXX - info.geometry.size.width / 2, origin.y + 10), ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
        }

        origin.x += DM_SPACINGX;
        origin.y += WI_SPACINGY;
    }

    // Draw stats.
    origin.y = DM_MATRIXY + 10;
    FR_SetFont(FID(GF_SMALL));
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], 1);
    int const w = FR_CharWidth('0');

    for(int i = 0; i < NUMTEAMS; ++i)
    {
        origin.x = DM_MATRIXX + DM_SPACINGX;
        if(teamInfo[i].playerCount > 0)
        {
            char buf[20];
            for(int k = 0; k < NUMTEAMS; ++k)
            {
                if(teamInfo[k].playerCount > 0)
                {
                    dd_snprintf(buf, 20, "%i", dmFrags[i][k]);
                    FR_DrawTextXY3(buf, origin.x + w, origin.y, ALIGN_TOPRIGHT, DTF_NO_TYPEIN);
                }
                origin.x += DM_SPACINGX;
            }
            dd_snprintf(buf, 20, "%i", dmTotals[i]);
            FR_DrawTextXY3(buf, DM_TOTALSX + w, origin.y, ALIGN_TOPRIGHT, DTF_NO_TYPEIN);
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

            int const fsum = teamInfo[i].totalFrags;
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

    int const pwidth = FR_CharWidth('%');
    patchinfo_t info;
    R_GetPatchInfo(pFaceAlive, &info);
    int const starWidth = info.geometry.size.width;

    // Draw stat titles (top line).
    R_GetPatchInfo(pKills, &info);
    WI_DrawPatch(pKills, patchReplacementText(pKills), Vector2i(ORIGINX + NG_SPACINGX, NG_STATSY), ALIGN_TOPRIGHT, 0, DTF_NO_TYPEIN);
    int y = NG_STATSY + info.geometry.size.height;

    WI_DrawPatch(pItems, patchReplacementText(pItems), Vector2i(ORIGINX + 2 * NG_SPACINGX, NG_STATSY), ALIGN_TOPRIGHT, 0, DTF_NO_TYPEIN);
    WI_DrawPatch(pSecret, patchReplacementText(pSecret), Vector2i(ORIGINX + 3 * NG_SPACINGX, NG_STATSY), ALIGN_TOPRIGHT, 0, DTF_NO_TYPEIN);
    if(doFrags)
    {
        WI_DrawPatch(pFrags, patchReplacementText(pFrags), Vector2i(ORIGINX + 4 * NG_SPACINGX, NG_STATSY), ALIGN_TOPRIGHT, 0, DTF_NO_TYPEIN);
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
        WI_DrawPatch(pTeamBackgrounds[i], patchReplacementText(pTeamBackgrounds[i]), Vector2i(x - info.geometry.size.width, y), ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);

        // If more than 1 member, show the member count.
        if(1 != teamInfo[i].playerCount)
        {
            char tmp[40]; sprintf(tmp, "%i", teamInfo[i].playerCount);
            FR_DrawTextXY3(tmp, x - info.geometry.size.width + 1, y + info.geometry.size.height - 8, ALIGN_TOPLEFT, DTF_NO_TYPEIN);
        }

        FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], 1);

        if(i == inPlayerTeam)
        {
            WI_DrawPatch(pFaceAlive, patchReplacementText(pFaceAlive), Vector2i(x - info.geometry.size.width, y), ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
        }
        x += NG_SPACINGX;

        FR_SetFont(FID(GF_SMALL));
        drawPercent(x - pwidth, y + 10, cntKills[i]);
        x += NG_SPACINGX;

        drawPercent(x - pwidth, y + 10, cntItems[i]);
        x += NG_SPACINGX;

        drawPercent(x - pwidth, y + 10, cntSecret[i]);
        x += NG_SPACINGX;

        if(doFrags)
        {
            char buf[20]; dd_snprintf(buf, 20, "%i", cntFrags[i]);
            FR_DrawTextXY3(buf, x, y + 10, ALIGN_TOPRIGHT, DTF_NO_TYPEIN);
        }

        y += WI_SPACINGY;
    }

    DGL_Disable(DGL_TEXTURE_2D);

#undef ORIGINX
}

static void drawSinglePlayerStats()
{
    int const lh = (3 * FR_CharHeight('0')) / 2; // Line height.

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, 1);

    FR_SetFont(FID(GF_FONTB));
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], 1);

    WI_DrawPatch(pKills, patchReplacementText(pKills), Vector2i(SP_STATSX, SP_STATSY), ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
    WI_DrawPatch(pItems, patchReplacementText(pItems), Vector2i(SP_STATSX, SP_STATSY + lh), ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
    WI_DrawPatch(pSecretSP, patchReplacementText(pSecretSP), Vector2i(SP_STATSX, SP_STATSY + 2 * lh), ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
    WI_DrawPatch(pTime, patchReplacementText(pTime), Vector2i(SP_TIMEX, SP_TIMEY), ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
    if(wbs->parTime != -1)
    {
        WI_DrawPatch(pPar, patchReplacementText(pPar), Vector2i(SCREENWIDTH / 2 + SP_TIMEX, SP_TIMEY), ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
    }

    FR_SetFont(FID(GF_SMALL));
    drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY, cntKills[0]);
    drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY + lh, cntItems[0]);
    drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY + 2 * lh, cntSecret[0]);

    if(cntTime >= 0)
    {
        drawTime(Vector2i(SCREENWIDTH / 2 - SP_TIMEX, SP_TIMEY), cntTime / TICRATE);
    }

    if(wbs->parTime != -1 && cntPar >= 0)
    {
        drawTime(Vector2i(SCREENWIDTH - SP_TIMEX, SP_TIMEY), cntPar / TICRATE);
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
    if(G_Ruleset_Deathmatch())
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
    if(G_Ruleset_Deathmatch())
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

void WI_Ticker()
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
        DENG2_ASSERT(!"WI_Ticker: Unknown intermission state");
        break;
    }
}

static void loadData()
{
    String const episodeId = COMMON_GAMESESSION->episodeId();

    // Determine which patch to use for the background.
    pBackground = R_DeclarePatch(backgroundPatchForEpisode(episodeId).toUtf8().constData());

    // Are there any animations to initialize?
    if(Animations const *anims = animationsForEpisode(episodeId))
    {
        pYouAreHereRight = R_DeclarePatch("WIURH0");
        pYouAreHereLeft  = R_DeclarePatch("WIURH1");
        pSplat           = R_DeclarePatch("WISPLAT");

        animStates = (wianimstate_t *)Z_Realloc(animStates, sizeof(*animStates) * anims->count(), PU_GAMESTATIC);
        std::memset(animStates, 0, sizeof(*animStates) * anims->count());

        for(int i = 0; i < anims->count(); ++i)
        {
            Animation const &def = (*anims)[i];
            wianimstate_t &state = animStates[i];

            state.frame = -1; // Not yet begun.

            for(String const &patchName : def.patchNames)
            {
                state.patches << R_DeclarePatch(patchName.toUtf8().constData());
            }
        }
    }

    pFinished   = R_DeclarePatch("WIF");
    pEntering   = R_DeclarePatch("WIENTER");
    pKills      = R_DeclarePatch("WIOSTK");
    pSecret     = R_DeclarePatch("WIOSTS");
    pSecretSP   = R_DeclarePatch("WISCRT2");
    pItems      = R_DeclarePatch("WIOSTI");
    pFrags      = R_DeclarePatch("WIFRGS");
    pTime       = R_DeclarePatch("WITIME");
    pSucks      = R_DeclarePatch("WISUCKS");
    pPar        = R_DeclarePatch("WIPAR");
    pKillers    = R_DeclarePatch("WIKILRS");
    pVictims    = R_DeclarePatch("WIVCTMS");
    pTotal      = R_DeclarePatch("WIMSTT");
    pFaceAlive  = R_DeclarePatch("STFST01");
    pFaceDead   = R_DeclarePatch("STFDEAD0");

    char name[9];
    for(int i = 0; i < NUMTEAMS; ++i)
    {
        sprintf(name, "STPB%d", i);
        pTeamBackgrounds[i] = R_DeclarePatch(name);

        sprintf(name, "WIBP%d", i + 1);
        pTeamIcons[i] = R_DeclarePatch(name);
    }
}

void WI_Drawer()
{
    /// @todo Kludge: dj: Clearly a kludge but why?
    if(ILS_NONE == inState)
    {
        drawYouAreHere = true;
    }
    /// kludge end.

    dgl_borderedprojectionstate_t bp;
    GL_ConfigureBorderedProjection(&bp, BPF_OVERDRAW_MASK | BPF_OVERDRAW_CLIP,
        SCREENWIDTH, SCREENHEIGHT, Get(DD_WINDOW_WIDTH), Get(DD_WINDOW_HEIGHT), scalemode_t(cfg.inludeScaleMode));
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

static void initVariables(wbstartstruct_t const &wbstartstruct)
{
    wbs = &wbstartstruct;

    advanceState = false;
    stateCounter = backgroundAnimCounter = 0;
    inPlayerNum  = wbs->pNum;
    inPlayerTeam = cfg.playerColor[wbs->pNum];
    inPlayerInfo = wbs->plyr;
}

void WI_Begin(wbstartstruct_t const &wbstartstruct)
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

    if(G_Ruleset_Deathmatch())
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

void WI_SetState(interludestate_t st)
{
    switch(st)
    {
    case ILS_SHOW_STATS:    initShowStats();   break;
    case ILS_SHOW_NEXTMAP:  initShowNextMap(); break;
    case ILS_NONE:          initNoState();     break;

    default:
        DENG2_ASSERT(!"WI_SetState: Unknown intermission state");
        break;
    }
}

void IN_SkipToNext()
{
    advanceState = true;
}

void WI_ConsoleRegister()
{
    C_VAR_BYTE("inlude-stretch",            &cfg.inludeScaleMode,           0, SCALEMODE_FIRST, SCALEMODE_LAST);
    C_VAR_INT ("inlude-patch-replacement",  &cfg.inludePatchReplaceMode,    0, 0, 1);
}
