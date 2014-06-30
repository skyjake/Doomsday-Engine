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

#include <assert.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "jdoom.h"
#include "wi_stuff.h"

#include "hu_stuff.h"
#include "d_net.h"
#include "p_mapsetup.h"
#include "p_start.h"

#define MAX_ANIM_FRAMES         (3)
#define NUMMAPS                 (9)

struct wianimdef_t
{
    int mapNum;        ///< If not @c 0= the logical map-number+1 for which this animation should only be displayed.
    int tics;          ///< Number of tics each frame of the animation lasts for.
    Point2Raw origin;  ///< Location origin of the animation on the map.
    int numFrames;     ///< Number of used frames in the animation.

    /// Names of the patches for each frame of the animation.
    char *patchNames[MAX_ANIM_FRAMES];

    /// State at which this animation begins/becomes visible.
    interludestate_t beginState;
};

struct wianimstate_t
{
    int nextTic;  ///< Next tic on which to progress the animation.
    int frame;    ///< Current frame number (index to patches); otherwise @c -1 (not yet begun).

    /// Graphics for each frame of the animation.
    patchid_t patches[MAX_ANIM_FRAMES];
};

struct teaminfo_t
{
    int playerCount;      ///< @c 0= team not present.
    int frags[NUMTEAMS];
    int totalFrags;       ///< Kills minus suicides.
    int items;
    int kills;
    int secret;
};

static Point2Raw const locations[][NUMMAPS] = {
    {   // Episode 0
        Point2Raw( 185, 164 ),
        Point2Raw( 148, 143 ),
        Point2Raw(  69, 122 ),
        Point2Raw( 209, 102 ),
        Point2Raw( 116,  89 ),
        Point2Raw( 166,  55 ),
        Point2Raw(  71,  56 ),
        Point2Raw( 135,  29 ),
        Point2Raw(  71,  24 )
    },
    {   // Episode 1
        Point2Raw( 254,  25 ),
        Point2Raw(  97,  50 ),
        Point2Raw( 188,  64 ),
        Point2Raw( 128,  78 ),
        Point2Raw( 214,  92 ),
        Point2Raw( 133, 130 ),
        Point2Raw( 208, 136 ),
        Point2Raw( 148, 140 ),
        Point2Raw( 235, 158 )
    },
    {   // Episode 2
        Point2Raw( 156, 168 ),
        Point2Raw(  48, 154 ),
        Point2Raw( 174,  95 ),
        Point2Raw( 265,  75 ),
        Point2Raw( 130,  48 ),
        Point2Raw( 279,  23 ),
        Point2Raw( 198,  48 ),
        Point2Raw( 140,  25 ),
        Point2Raw( 281, 136 )
    }
};

static wianimdef_t const episode0AnimDefs[] = {
    { 0, 11, Point2Raw( 224, 104 ), 3, { "wia00000", "wia00001", "wia00002" }, ILS_SHOW_STATS },
    { 0, 11, Point2Raw( 184, 160 ), 3, { "wia00100", "wia00101", "wia00102" }, ILS_SHOW_STATS },
    { 0, 11, Point2Raw( 112, 136 ), 3, { "wia00200", "wia00201", "wia00202" }, ILS_SHOW_STATS },
    { 0, 11, Point2Raw(  72, 112 ), 3, { "wia00300", "wia00301", "wia00302" }, ILS_SHOW_STATS },
    { 0, 11, Point2Raw(  88,  96 ), 3, { "wia00400", "wia00401", "wia00402" }, ILS_SHOW_STATS },
    { 0, 11, Point2Raw(  64,  48 ), 3, { "wia00500", "wia00501", "wia00502" }, ILS_SHOW_STATS },
    { 0, 11, Point2Raw( 192,  40 ), 3, { "wia00600", "wia00601", "wia00602" }, ILS_SHOW_STATS },
    { 0, 11, Point2Raw( 136,  16 ), 3, { "wia00700", "wia00701", "wia00702" }, ILS_SHOW_STATS },
    { 0, 11, Point2Raw(  80,  16 ), 3, { "wia00800", "wia00801", "wia00802" }, ILS_SHOW_STATS },
    { 0, 11, Point2Raw(  64,  24 ), 3, { "wia00900", "wia00901", "wia00902" }, ILS_SHOW_STATS }
};

static wianimdef_t const episode1AnimDefs[] = {
    { 1,  0, Point2Raw( 128, 136 ), 1, { "wia10000" }, ILS_SHOW_STATS },
    { 2,  0, Point2Raw( 128, 136 ), 1, { "wia10100" }, ILS_SHOW_STATS },
    { 3,  0, Point2Raw( 128, 136 ), 1, { "wia10200" }, ILS_SHOW_STATS },
    { 4,  0, Point2Raw( 128, 136 ), 1, { "wia10300" }, ILS_SHOW_STATS },
    { 5,  0, Point2Raw( 128, 136 ), 1, { "wia10400" }, ILS_SHOW_STATS },
    { 6,  0, Point2Raw( 128, 136 ), 1, { "wia10500" }, ILS_SHOW_STATS },
    { 7,  0, Point2Raw( 128, 136 ), 1, { "wia10600" }, ILS_SHOW_STATS },
    { 8, 11, Point2Raw( 192, 144 ), 3, { "wia10700", "wia10701", "wia10702" }, ILS_SHOW_NEXTMAP },
    { 8,  0, Point2Raw( 128, 136 ), 1, { "wia10400" }, ILS_SHOW_STATS }
};

static wianimdef_t const episode2AnimDefs[] = {
    { 0, 11, Point2Raw( 104, 168 ), 3, { "wia20000", "wia20001", "wia20002" }, ILS_SHOW_STATS },
    { 0, 11, Point2Raw(  40, 136 ), 3, { "wia20100", "wia20101", "wia20102" }, ILS_SHOW_STATS },
    { 0, 11, Point2Raw( 160,  96 ), 3, { "wia20200", "wia20201", "wia20202" }, ILS_SHOW_STATS },
    { 0, 11, Point2Raw( 104,  80 ), 3, { "wia20300", "wia20301", "wia20302" }, ILS_SHOW_STATS },
    { 0, 11, Point2Raw( 120,  32 ), 3, { "wia20400", "wia20401", "wia20402" }, ILS_SHOW_STATS },
    { 0,  8, Point2Raw(  40,   0 ), 3, { "wia20500", "wia20501", "wia20502" }, ILS_SHOW_STATS }
};

static int animCounts[] = {
    sizeof(episode0AnimDefs) / sizeof(wianimdef_t),
    sizeof(episode1AnimDefs) / sizeof(wianimdef_t),
    sizeof(episode2AnimDefs) / sizeof(wianimdef_t)
};

static wianimdef_t const *animDefs[] = {
    episode0AnimDefs,
    episode1AnimDefs,
    episode2AnimDefs
};

static wianimstate_t *animStates;

static teaminfo_t teamInfo[NUMTEAMS];

// Used to accelerate or skip a stage.
static dd_bool advanceState;

static dd_bool drawYouAreHere;

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
static wbstartstruct_t *wbs;
static wbplayerstruct_t *inPlayerInfo;

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
    return Hu_ChoosePatchReplacement2(patchreplacemode_t(cfg.inludePatchReplaceMode),
                                      patchId, text);
}

void WI_Register()
{
    C_VAR_BYTE("inlude-stretch",            &cfg.inludeScaleMode,           0, SCALEMODE_FIRST, SCALEMODE_LAST);
    C_VAR_INT ("inlude-patch-replacement",  &cfg.inludePatchReplaceMode,    0, PRM_FIRST, PRM_LAST);
}

void IN_SkipToNext()
{
    advanceState = true;
}

static void drawBackground()
{
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, 1);
    GL_DrawPatchXY3(pBackground, 0, 0, ALIGN_TOPLEFT, DPF_NO_OFFSET);

    if(!(gameModeBits & GM_ANY_DOOM2) && wbs->episode < 3)
    {
        patchid_t patchId;
        int i;

        FR_SetFont(FID(GF_FONTB));
        FR_LoadDefaultAttrib();

        for(i = 0; i < animCounts[wbs->episode]; ++i)
        {
            wianimdef_t const *def = &animDefs[wbs->episode][i];
            wianimstate_t *state = &animStates[i];

            // Has the animation begun yet?
            if(state->frame < 0) continue;

            patchId = state->patches[state->frame];
            WI_DrawPatch3(patchId, patchReplacementText(patchId), &def->origin, ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
        }
    }
    DGL_Disable(DGL_TEXTURE_2D);
}

static void drawFinishedTitle(int x = SCREENWIDTH / 2, int y = WI_TITLEY)
{
    uint mapNum;
    if(gameModeBits & (GM_ANY_DOOM2|GM_DOOM_CHEX))
        mapNum = wbs->currentMap;
    else
        mapNum = (wbs->episode * 9) + wbs->currentMap;

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, 1);

    FR_SetFont(FID(GF_FONTB));
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(defFontRGB[CR], defFontRGB[CG], defFontRGB[CB], 1);

    // Draw <MapName>
    patchid_t const patchId   = (mapNum < pMapNamesSize? pMapNames[mapNum] : 0);
    de::String const mapTitle = G_MapTitle(0/*current map*/);
    WI_DrawPatchXY3(patchId, patchReplacementText(patchId, mapTitle.toUtf8().constData()), x, y, ALIGN_TOP, 0, DTF_NO_TYPEIN);
    patchinfo_t info;
    if(R_GetPatchInfo(patchId, &info))
        y += (5 * info.geometry.size.height) / 4;

    // Draw "Finished!"
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], 1);
    WI_DrawPatchXY3(pFinished, patchReplacementText(pFinished), x, y, ALIGN_TOP, 0, DTF_NO_TYPEIN);

    DGL_Disable(DGL_TEXTURE_2D);
}

static void drawEnteringTitle(int x = SCREENWIDTH / 2, int y = WI_TITLEY)
{
    /// @kludge We need to properly externalize the map progression.
    if((gameModeBits & (GM_DOOM2|GM_DOOM2_PLUT|GM_DOOM2_TNT)) && wbs->nextMap == 30)
    {
        return;
    }
    /// kludge end.

    // See if there is a map name...
    char *mapName = 0;
    ddmapinfo_t minfo;
    if(Def_Get(DD_DEF_MAP_INFO, G_ComposeMapUri(wbs->episode, wbs->nextMap).compose().toUtf8().constData(), &minfo) && minfo.name)
    {
        if(Def_Get(DD_DEF_TEXT, minfo.name, &mapName) == -1)
            mapName = minfo.name;
    }

    // Skip the E#M# or Map #.
    if(mapName)
    {
        char* ptr = strchr(mapName, ':');
        if(ptr)
        {
            mapName = M_SkipWhite(ptr + 1);
        }
    }

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, 1);

    FR_SetFont(FID(GF_FONTB));
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], 1);

    // Draw "Entering"
    WI_DrawPatchXY3(pEntering, patchReplacementText(pEntering), x, y, ALIGN_TOP, 0, DTF_NO_TYPEIN);

    patchinfo_t info;
    if(R_GetPatchInfo(pMapNames[wbs->nextMap], &info))
        y += (5 * info.geometry.size.height) / 4;

    // Draw map.
    uint const mapNum       = (wbs->episode * 9) + wbs->nextMap;
    patchid_t const patchId = (mapNum < pMapNamesSize? pMapNames[mapNum] : 0);
    FR_SetColorAndAlpha(defFontRGB[CR], defFontRGB[CG], defFontRGB[CB], 1);
    WI_DrawPatchXY3(patchId, patchReplacementText(patchId, mapName), x, y, ALIGN_TOP, 0, DTF_NO_TYPEIN);

    DGL_Disable(DGL_TEXTURE_2D);
}

static dd_bool patchFits(patchid_t patchId, int x, int y)
{
    int left, top, right, bottom;
    patchinfo_t info;
    if(!R_GetPatchInfo(patchId, &info)) return false;

    left = x + info.geometry.origin.x;
    top  = y + info.geometry.origin.y;
    right = left + info.geometry.size.width;
    bottom = top + info.geometry.size.height;
    return (left >= 0 && right < SCREENWIDTH && top >= 0 && bottom < SCREENHEIGHT);
}

static patchid_t chooseYouAreHerePatch(const Point2Raw* origin)
{
    assert(origin);
    if(patchFits(pYouAreHereRight, origin->x, origin->y))
        return pYouAreHereRight;
    if(patchFits(pYouAreHereLeft, origin->x, origin->y))
        return pYouAreHereLeft;
    return 0; // None fits.
}

static void drawPatchIfFits(patchid_t patchId, const Point2Raw* origin)
{
    assert(origin);
    if(patchFits(patchId, origin->x, origin->y))
    {
        WI_DrawPatch3(patchId, patchReplacementText(patchId), origin, ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
    }
}

/**
 * Begin any animations that were previously waiting on a state.
 * To be called upon changing the value of @var inState.
 */
static void beginAnimations()
{
    if(gameModeBits & GM_ANY_DOOM2) return;
    if(wbs->episode > 2) return;

    for(int i = 0; i < animCounts[wbs->episode]; ++i)
    {
        wianimdef_t const *def = &animDefs[wbs->episode][i];
        wianimstate_t *state   = &animStates[i];

        // Is the animation active for the current map?
        if(def->mapNum && wbs->nextMap != (unsigned)def->mapNum)
            continue;

        // Already begun?
        if(state->frame >= 0) continue;

        // Is it time to begin the animation?
        if(def->beginState != inState) continue;

        state->frame = 0;

        // Determine when to animate the first frame.
        if(def->mapNum)
        {
            state->nextTic = backgroundAnimCounter + 1 + def->tics;
        }
        else
        {
            state->nextTic = backgroundAnimCounter + 1 + (M_Random() % def->tics);
        }
    }
}

static void animateBackground()
{
    if(gameModeBits & GM_ANY_DOOM2) return;
    if(wbs->episode > 2) return;

    for(int i = 0; i < animCounts[wbs->episode]; ++i)
    {
        wianimdef_t const *def = &animDefs[wbs->episode][i];
        wianimstate_t *state   = &animStates[i];

        // Is the animation active for the current map?
        if(def->mapNum && wbs->nextMap != (unsigned)def->mapNum)
            continue;

        // Has the animation begun yet
        if(state->frame < 0) continue;

        // Time to progress the animation?
        if(backgroundAnimCounter != state->nextTic) continue;

        ++state->frame;
        if(state->frame >= def->numFrames)
        {
            if(def->mapNum)
            {
                // Loop.
                state->frame = def->numFrames - 1;
            }
            else
            {
                // Restart.
                state->frame = 0;
            }
        }

        state->nextTic = backgroundAnimCounter + MAX_OF(def->tics, 1);
    }
}

static void drawPercent(int x, int y, int p)
{
    Point2Raw origin;
    char buf[20];
    if(p < 0) return;

    origin.x = x;
    origin.y = y;
    dd_snprintf(buf, 20, "%i", p);
    FR_DrawChar3('%', &origin, ALIGN_TOPLEFT, DTF_NO_TYPEIN);
    FR_DrawText3(buf, &origin, ALIGN_TOPRIGHT, DTF_NO_TYPEIN);
}

/**
 * Display map completion time and par, or "sucks" message if overflow.
 */
static void drawTime(int x, int y, int t)
{
    patchinfo_t info;
    if(t < 0) return;

    if(t <= 61 * 59)
    {
        int seconds = t % 60, minutes = t / 60 % 60;
        char buf[20];

        x -= 22;

        FR_DrawCharXY3(':', x, y, ALIGN_TOPLEFT, DTF_NO_TYPEIN);
        if(minutes > 0)
        {
            dd_snprintf(buf, 20, "%d", minutes);
            FR_DrawTextXY3(buf, x, y, ALIGN_TOPRIGHT, DTF_NO_TYPEIN);
        }
        dd_snprintf(buf, 20, "%02d", seconds);
        FR_DrawTextXY3(buf, x+FR_CharWidth(':'), y, ALIGN_TOPLEFT, DTF_NO_TYPEIN);
        return;
    }

    // "sucks"
    if(!R_GetPatchInfo(pSucks, &info)) return;

    WI_DrawPatchXY3(pSucks, patchReplacementText(pSucks), x - info.geometry.size.width, y, ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
}

void WI_End(void)
{
    NetSv_Intermission(IMF_END, 0, 0);
}

static void initNoState(void)
{
    inState = ILS_NONE;
    advanceState = false;
    stateCounter = 10;

    NetSv_Intermission(IMF_STATE, inState, 0);
}

static void tickNoState(void)
{
    --stateCounter;
    if(0 == stateCounter)
    {
        if(IS_CLIENT) return;

        WI_End();
        G_IntermissionDone();
    }
}

static void initShowNextMap(void)
{
    inState = ILS_SHOW_NEXTMAP;
    advanceState = false;
    stateCounter = SHOWNEXTLOCDELAY * TICRATE;

    beginAnimations();

    NetSv_Intermission(IMF_STATE, inState, 0);
}

static void tickShowNextMap(void)
{
    --stateCounter;
    if(0 == stateCounter || advanceState)
    {
        initNoState();
        return;
    }

    drawYouAreHere = (stateCounter & 31) < 20;
}

static void drawLocationMarks(void)
{
    if((gameModeBits & GM_ANY_DOOM) && wbs->episode < 3)
    {
        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, 1);
        FR_SetFont(FID(GF_FONTB));
        FR_LoadDefaultAttrib();

        // Draw a splat on taken cities.
        { int i, last = (wbs->currentMap == 8) ? wbs->nextMap-1 : wbs->currentMap;
        for(i = 0; i <= last; ++i)
        {
            drawPatchIfFits(pSplat, &locations[wbs->episode][i]);
        }}

        // Splat the secret map?
        if(wbs->didSecret)
        {
            drawPatchIfFits(pSplat, &locations[wbs->episode][8]);
        }

        if(drawYouAreHere)
        {
            const Point2Raw* origin = &locations[wbs->episode][wbs->nextMap];
            patchid_t patchId = chooseYouAreHerePatch(origin);
            if(0 != patchId)
            {
                WI_DrawPatch3(patchId, patchReplacementText(patchId), origin, ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
            }
        }

        DGL_Disable(DGL_TEXTURE_2D);
    }
}

static void initDeathmatchStats(void)
{
    int i;

    inState = ILS_SHOW_STATS;
    advanceState = false;
    dmState = 1;

    cntPause = TICRATE;

    // Clear the on-screen counters.
    memset(dmTotals, 0, sizeof(dmTotals));
    for(i = 0; i < NUMTEAMS; ++i)
    {
        memset(dmFrags[i], 0, sizeof(dmFrags[i]));
    }

    beginAnimations();
}

static void updateDeathmatchStats(void)
{
    int i, j;
    dd_bool stillTicking;

    if(advanceState && dmState != 4)
    {
        advanceState = false;
        for(i = 0; i < NUMTEAMS; ++i)
        {
            for(j = 0; j < NUMTEAMS; ++j)
            {
                dmFrags[i][j] = teamInfo[i].frags[j];
            }

            dmTotals[i] = teamInfo[i].totalFrags;
        }

        S_LocalSound(SFX_BAREXP, 0);
        dmState = 4;
    }

    if(dmState == 2)
    {
        if(!(backgroundAnimCounter & 3))
            S_LocalSound(SFX_PISTOL, 0);

        stillTicking = false;
        for(i = 0; i < NUMTEAMS; ++i)
        {
            for(j = 0; j < NUMTEAMS; ++j)
            {
                if(dmFrags[i][j] != teamInfo[i].frags[j])
                {
                    if(teamInfo[i].frags[j] < 0)
                        dmFrags[i][j]--;
                    else
                        dmFrags[i][j]++;

                    if(dmFrags[i][j] > 99)
                        dmFrags[i][j] = 99;

                    if(dmFrags[i][j] < -99)
                        dmFrags[i][j] = -99;

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
                initNoState();
            else
                initShowNextMap();
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

static void drawDeathmatchStats(void)
{
    int i, j, x, y, w;// lh = WI_SPACINGY; // Line height.

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, 1);

    FR_SetFont(FID(GF_FONTB));
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], 1);

    // Draw stat titles (top line).
    { patchinfo_t info;
    if(R_GetPatchInfo(pTotal, &info))
        WI_DrawPatchXY3(pTotal, patchReplacementText(pTotal), DM_TOTALSX - info.geometry.size.width / 2, DM_MATRIXY - WI_SPACINGY + 10, ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN); }

    WI_DrawPatchXY3(pKillers, patchReplacementText(pKillers), DM_KILLERSX, DM_KILLERSY, ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
    WI_DrawPatchXY3(pVictims, patchReplacementText(pVictims), DM_VICTIMSX, DM_VICTIMSY, ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);

    x = DM_MATRIXX + DM_SPACINGX;
    y = DM_MATRIXY;

    for(i = 0; i < NUMTEAMS; ++i)
    {
        if(teamInfo[i].playerCount > 0)
        {
            patchid_t patchId = pTeamBackgrounds[i];
            const char* replacement;
            patchinfo_t info;

            FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], 1);

            R_GetPatchInfo(patchId, &info);
            replacement = patchReplacementText(patchId);
            WI_DrawPatchXY3(patchId, replacement, x - info.geometry.size.width / 2, DM_MATRIXY - WI_SPACINGY, ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
            WI_DrawPatchXY3(patchId, replacement, DM_MATRIXX - info.geometry.size.width / 2, y, ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);

            if(i == inPlayerTeam)
            {
                WI_DrawPatchXY3(pFaceDead, patchReplacementText(pFaceDead), x - info.geometry.size.width / 2, DM_MATRIXY - WI_SPACINGY, ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
                WI_DrawPatchXY3(pFaceAlive, patchReplacementText(pFaceAlive), DM_MATRIXX - info.geometry.size.width / 2, y, ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
            }

            // If more than 1 member, show the member count.
            if(1 > teamInfo[i].playerCount)
            {
                char tmp[20];
                sprintf(tmp, "%i", teamInfo[i].playerCount);

                FR_SetFont(FID(GF_FONTA));
                FR_DrawTextXY3(tmp, x - info.geometry.size.width / 2 + 1, DM_MATRIXY - WI_SPACINGY + info.geometry.size.height - 8, ALIGN_TOPLEFT, DTF_NO_TYPEIN);
                FR_DrawTextXY3(tmp, DM_MATRIXX - info.geometry.size.width / 2 + 1, y + info.geometry.size.height - 8, ALIGN_TOPLEFT, DTF_NO_TYPEIN);
            }
        }
        else
        {
            patchid_t patchId = pTeamIcons[i];
            const char* replacement = patchReplacementText(patchId);
            patchinfo_t info;

            FR_SetColorAndAlpha(defFontRGB[CR], defFontRGB[CG], defFontRGB[CB], 1);

            R_GetPatchInfo(patchId, &info);
            WI_DrawPatchXY3(patchId, replacement, x - info.geometry.size.width / 2, DM_MATRIXY - WI_SPACINGY + 10, ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
            WI_DrawPatchXY3(patchId, replacement, DM_MATRIXX - info.geometry.size.width / 2, y + 10, ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
        }

        x += DM_SPACINGX;
        y += WI_SPACINGY;
    }

    // Draw stats.
    y = DM_MATRIXY + 10;
    FR_SetFont(FID(GF_SMALL));
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], 1);
    w = FR_CharWidth('0');

    for(i = 0; i < NUMTEAMS; ++i)
    {
        x = DM_MATRIXX + DM_SPACINGX;
        if(teamInfo[i].playerCount > 0)
        {
            char buf[20];
            for(j = 0; j < NUMTEAMS; ++j)
            {
                if(teamInfo[j].playerCount > 0)
                {
                    dd_snprintf(buf, 20, "%i", dmFrags[i][j]);
                    FR_DrawTextXY3(buf, x + w, y, ALIGN_TOPRIGHT, DTF_NO_TYPEIN);
                }
                x += DM_SPACINGX;
            }
            dd_snprintf(buf, 20, "%i", dmTotals[i]);
            FR_DrawTextXY3(buf, DM_TOTALSX + w, y, ALIGN_TOPRIGHT, DTF_NO_TYPEIN);
        }

        y += WI_SPACINGY;
    }

    DGL_Disable(DGL_TEXTURE_2D);
}

static void initNetgameStats(void)
{
    int i;

    inState = ILS_SHOW_STATS;
    advanceState = false;
    ngState = 1;
    cntPause = TICRATE;

    memset(cntKills, 0, sizeof(cntKills));
    memset(cntItems, 0, sizeof(cntItems));
    memset(cntSecret, 0, sizeof(cntSecret));
    memset(cntFrags, 0, sizeof(cntFrags));
    doFrags = 0;

    for(i = 0; i < NUMTEAMS; ++i)
    {
        doFrags += teamInfo[i].totalFrags;
    }
    doFrags = !doFrags;

    beginAnimations();
}

static void updateNetgameStats(void)
{
    dd_bool stillTicking;
    int i, fsum;

    if(advanceState && ngState != 10)
    {
        advanceState = false;
        for(i = 0; i < NUMTEAMS; ++i)
        {
            cntKills[i] = (teamInfo[i].kills * 100) / wbs->maxKills;
            cntItems[i] = (teamInfo[i].items * 100) / wbs->maxItems;
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
            S_LocalSound(SFX_PISTOL, 0);
        stillTicking = false;

        for(i = 0; i < NUMTEAMS; ++i)
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
            S_LocalSound(SFX_PISTOL, 0);
        stillTicking = false;

        for(i = 0; i < NUMTEAMS; ++i)
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
            S_LocalSound(SFX_PISTOL, 0);

        stillTicking = false;

        for(i = 0; i < NUMTEAMS; ++i)
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
            S_LocalSound(SFX_PISTOL, 0);

        stillTicking = false;

        for(i = 0; i < NUMTEAMS; ++i)
        {
            cntFrags[i] += 1;

            fsum = teamInfo[i].totalFrags;
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
                initNoState();
            else
                initShowNextMap();
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

static void drawNetgameStats(void)
{
#define ORIGINX             (NG_STATSX + starWidth/2 + NG_STATSX*!doFrags)

    int i, x, y, starWidth, pwidth;
    patchinfo_t info;

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, 1);

    FR_SetFont(FID(GF_FONTB));
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], 1);

    pwidth = FR_CharWidth('%');
    R_GetPatchInfo(pFaceAlive, &info);
    starWidth = info.geometry.size.width;

    // Draw stat titles (top line).
    R_GetPatchInfo(pKills, &info);
    WI_DrawPatchXY3(pKills, patchReplacementText(pKills), ORIGINX + NG_SPACINGX, NG_STATSY, ALIGN_TOPRIGHT, 0, DTF_NO_TYPEIN);
    y = NG_STATSY + info.geometry.size.height;

    WI_DrawPatchXY3(pItems, patchReplacementText(pItems), ORIGINX + 2 * NG_SPACINGX, NG_STATSY, ALIGN_TOPRIGHT, 0, DTF_NO_TYPEIN);
    WI_DrawPatchXY3(pSecret, patchReplacementText(pSecret), ORIGINX + 3 * NG_SPACINGX, NG_STATSY, ALIGN_TOPRIGHT, 0, DTF_NO_TYPEIN);
    if(doFrags)
    {
        WI_DrawPatchXY3(pFrags, patchReplacementText(pFrags), ORIGINX + 4 * NG_SPACINGX, NG_STATSY, ALIGN_TOPRIGHT, 0, DTF_NO_TYPEIN);
    }

    // Draw stats.
    for(i = 0; i < NUMTEAMS; ++i)
    {
        patchinfo_t info;

        if(0 == teamInfo[i].playerCount)
            continue;

        FR_SetFont(FID(GF_FONTA));
        FR_SetColorAndAlpha(1, 1, 1, 1);

        x = ORIGINX;
        R_GetPatchInfo(pTeamBackgrounds[i], &info);
        WI_DrawPatchXY3(pTeamBackgrounds[i], patchReplacementText(pTeamBackgrounds[i]), x - info.geometry.size.width, y, ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);

        // If more than 1 member, show the member count.
        if(1 != teamInfo[i].playerCount)
        {
            char tmp[40];

            sprintf(tmp, "%i", teamInfo[i].playerCount);
            FR_DrawTextXY3(tmp, x - info.geometry.size.width + 1, y + info.geometry.size.height - 8, ALIGN_TOPLEFT, DTF_NO_TYPEIN);
        }

        FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], 1);

        if(i == inPlayerTeam)
            WI_DrawPatchXY3(pFaceAlive, patchReplacementText(pFaceAlive), x - info.geometry.size.width, y, ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
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
            char buf[20];
            dd_snprintf(buf, 20, "%i", cntFrags[i]);
            FR_DrawTextXY3(buf, x, y + 10, ALIGN_TOPRIGHT, DTF_NO_TYPEIN);
        }

        y += WI_SPACINGY;
    }

    DGL_Disable(DGL_TEXTURE_2D);

#undef ORIGINX
}

static void drawSinglePlayerStats(void)
{
    int lh;
    lh = (3 * FR_CharHeight('0')) / 2; // Line height.

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, 1);

    FR_SetFont(FID(GF_FONTB));
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], 1);

    WI_DrawPatchXY3(pKills, patchReplacementText(pKills), SP_STATSX, SP_STATSY, ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
    WI_DrawPatchXY3(pItems, patchReplacementText(pItems), SP_STATSX, SP_STATSY + lh, ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
    WI_DrawPatchXY3(pSecretSP, patchReplacementText(pSecretSP), SP_STATSX, SP_STATSY + 2 * lh, ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
    WI_DrawPatchXY3(pTime, patchReplacementText(pTime), SP_TIMEX, SP_TIMEY, ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
    if(wbs->parTime != -1)
    {
        WI_DrawPatchXY3(pPar, patchReplacementText(pPar), SCREENWIDTH / 2 + SP_TIMEX, SP_TIMEY, ALIGN_TOPLEFT, 0, DTF_NO_TYPEIN);
    }

    FR_SetFont(FID(GF_SMALL));
    drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY, cntKills[0]);
    drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY + lh, cntItems[0]);
    drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY + 2 * lh, cntSecret[0]);

    if(cntTime >= 0)
    {
        drawTime(SCREENWIDTH / 2 - SP_TIMEX, SP_TIMEY, cntTime / TICRATE);
    }

    if(wbs->parTime != -1 && cntPar >= 0)
    {
        drawTime(SCREENWIDTH - SP_TIMEX, SP_TIMEY, cntPar / TICRATE);
    }

    DGL_Disable(DGL_TEXTURE_2D);
}

static void initShowStats(void)
{
    inState = ILS_SHOW_STATS;
    advanceState = false;
    spState = 1;
    cntKills[0] = cntItems[0] = cntSecret[0] = -1;
    cntTime = cntPar = -1;
    cntPause = TICRATE;

    beginAnimations();
}

static void tickShowStats(void)
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
        cntKills[0] = (inPlayerInfo[inPlayerNum].kills * 100) / wbs->maxKills;
        cntItems[0] = (inPlayerInfo[inPlayerNum].items * 100) / wbs->maxItems;
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

static void drawStats(void)
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
static void maybeAdvanceState(void)
{
    player_t* player;
    int i;

    for(i = 0, player = players; i < MAXPLAYERS; ++i, player++)
    {
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

void WI_Ticker(void)
{
    ++backgroundAnimCounter;
    animateBackground();

    maybeAdvanceState();
    switch(inState)
    {
    case ILS_SHOW_STATS:    tickShowStats(); break;
    case ILS_SHOW_NEXTMAP:  tickShowNextMap(); break;
    case ILS_NONE:          tickNoState(); break;
    default:
#if _DEBUG
        Con_Error("WI_Ticker: Invalid state %i.", (int) inState);
#endif
        break;
    }
}

static void loadData(void)
{
    char name[9];
    int i;

    if((gameModeBits & GM_ANY_DOOM2) || (gameMode == doom_ultimate && wbs->episode > 2))
    {
        pBackground = R_DeclarePatch("INTERPIC");
    }
    else
    {
        sprintf(name, "WIMAP%u", wbs->episode);
        pBackground = R_DeclarePatch(name);
    }

    if((gameModeBits & GM_ANY_DOOM) && wbs->episode < 3)
    {
        pYouAreHereRight = R_DeclarePatch("WIURH0");
        pYouAreHereLeft  = R_DeclarePatch("WIURH1");
        pSplat = R_DeclarePatch("WISPLAT");

        animStates = (wianimstate_t *)Z_Realloc(animStates, sizeof(*animStates) * animCounts[wbs->episode], PU_GAMESTATIC);
        if(!animStates) Con_Error("WI_Stuff::loadData: Failed on (re)allocation of %lu bytes for animStates.", (unsigned long) (sizeof(*animStates) * animCounts[wbs->episode]));
        memset(animStates, 0, sizeof(*animStates) * animCounts[wbs->episode]);

        for(i = 0; i < animCounts[wbs->episode]; ++i)
        {
            wianimdef_t const *def = &animDefs[wbs->episode][i];
            wianimstate_t *state = &animStates[i];
            int j;

            state->frame = -1; // Not yet begun.

            for(j = 0; j < def->numFrames; ++j)
            {
                state->patches[j] = R_DeclarePatch(def->patchNames[j]);
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

    for(i = 0; i < NUMTEAMS; ++i)
    {
        sprintf(name, "STPB%d", i);
        pTeamBackgrounds[i] = R_DeclarePatch(name);

        sprintf(name, "WIBP%d", i + 1);
        pTeamIcons[i] = R_DeclarePatch(name);
    }
}

void WI_Drawer(void)
{
    dgl_borderedprojectionstate_t bp;

    /// @todo Kludge: dj: Clearly a kludge but why?
    if(ILS_NONE == inState)
    {
        drawYouAreHere = true;
    }
    /// kludge end.

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

static void initVariables(wbstartstruct_t* wbstartstruct)
{
    wbs = wbstartstruct;

    advanceState = false;
    stateCounter = backgroundAnimCounter = 0;
    inPlayerNum = wbs->pNum;
    inPlayerTeam = cfg.playerColor[wbs->pNum];
    inPlayerInfo = wbs->plyr;

    if(!wbs->maxKills)
        wbs->maxKills = 1;
    if(!wbs->maxItems)
        wbs->maxItems = 1;
    if(!wbs->maxSecret)
        wbs->maxSecret = 1;
}

void WI_Init(wbstartstruct_t* wbstartstruct)
{
    int i, j, k;
    teaminfo_t* tin;

    initVariables(wbstartstruct);
    loadData();

    // Calculate team stats.
    memset(teamInfo, 0, sizeof(teamInfo));
    for(i = 0, tin = teamInfo; i < NUMTEAMS; ++i, tin++)
    {
        for(j = 0; j < MAXPLAYERS; ++j)
        {
            // Is the player in this team?
            if(!inPlayerInfo[j].inGame || cfg.playerColor[j] != i)
                continue;

            ++tin->playerCount;

            // Check the frags.
            for(k = 0; k < MAXPLAYERS; ++k)
                tin->frags[cfg.playerColor[k]] += inPlayerInfo[j].frags[k];

            // Counters.
            if(inPlayerInfo[j].items > tin->items)
                tin->items = inPlayerInfo[j].items;
            if(inPlayerInfo[j].kills > tin->kills)
                tin->kills = inPlayerInfo[j].kills;
            if(inPlayerInfo[j].secret > tin->secret)
                tin->secret = inPlayerInfo[j].secret;
        }

        // Calculate team's total frags.
        for(j = 0; j < NUMTEAMS; ++j)
        {
            if(j == i) // Suicides are negative frags.
                tin->totalFrags -= tin->frags[j];
            else
                tin->totalFrags += tin->frags[j];
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

void WI_Shutdown(void)
{
    if(animStates)
    {
        Z_Free(animStates);
        animStates = NULL;
    }
}

void WI_SetState(interludestate_t st)
{
    switch(st)
    {
    case ILS_SHOW_STATS:    initShowStats(); break;
    case ILS_SHOW_NEXTMAP:  initShowNextMap(); break;
    case ILS_NONE:          initNoState(); break;
    default:
#if _DEBUG
        Con_Error("WI_SetState: Invalid state %i.", (int) st);
#endif
        break;
    }
}
