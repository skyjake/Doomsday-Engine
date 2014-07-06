/** @file wi_stuff.cpp  DOOM64 specific intermission screens.
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

#include "jdoom64.h"
#include "wi_stuff.h"

#include <cassert>
#include <cstdio>
#include <cctype>
#include <cstring>

#include "hu_stuff.h"
#include "d_net.h"
#include "p_mapsetup.h"
#include "p_start.h"

#define NUMMAPS                 (9)

struct teaminfo_t
{
    int playerCount;      ///< @c 0= team not present.
    int frags[NUMTEAMS];
    int totalFrags;       ///< Kills minus suicides.
    int items;
    int kills;
    int secret;
};

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
static wbstartstruct_t const *wbs;
static wbplayerstruct_t const *inPlayerInfo;

static patchid_t pBackground;
//static patchid_t pYouAreHereRight;
//static patchid_t pYouAreHereLeft;
//static patchid_t pSplat;
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

static void drawBackground()
{
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, 1);
    GL_DrawPatchXY3(pBackground, 0, 0, ALIGN_TOPLEFT, DPF_NO_OFFSET);
    DGL_Disable(DGL_TEXTURE_2D);
}

static void drawFinishedTitle(int x = SCREENWIDTH / 2, int y = WI_TITLEY)
{
    de::String const mapTitle = G_MapTitle(); // current map

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, 1);
    FR_SetFont(FID(GF_FONTB));
    FR_LoadDefaultAttrib();

    // Draw <MapName>
    patchid_t const mapTitlePatch = G_MapTitlePatch(&wbs->currentMap);
    WI_DrawPatchXY3(mapTitlePatch, patchReplacementText(mapTitlePatch, mapTitle.toUtf8().constData()), x, y, ALIGN_TOP, 0, DTF_NO_TYPEIN);
    patchinfo_t info;
    if(R_GetPatchInfo(mapTitlePatch, &info))
        y += (5 * info.geometry.size.height) / 4;

    // Draw "Finished!"
    WI_DrawPatchXY2(pFinished, patchReplacementText(pFinished), x, y, ALIGN_TOP);

    DGL_Disable(DGL_TEXTURE_2D);
}

static void drawEnteringTitle(int x = SCREENWIDTH / 2, int y = WI_TITLEY)
{
    // See if there is a map name...
    char *mapName = 0;
    ddmapinfo_t minfo;
    if(Def_Get(DD_DEF_MAP_INFO, wbs->nextMap.compose().toUtf8().constData(), &minfo) && minfo.name)
    {
        if(Def_Get(DD_DEF_TEXT, minfo.name, &mapName) == -1)
            mapName = minfo.name;
    }

    // Skip the E#M# or Map #.
    if(mapName)
    {
        char *ptr = strchr(mapName, ':');
        if(ptr)
        {
            mapName = M_SkipWhite(ptr + 1);
        }
    }

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, 1);
    FR_SetFont(FID(GF_FONTB));
    FR_LoadDefaultAttrib();

    // Draw "Entering"
    WI_DrawPatchXY2(pEntering, patchReplacementText(pEntering), x, y, ALIGN_TOP);

    patchinfo_t info;
    patchid_t const mapTitlePatch = G_MapTitlePatch(&wbs->nextMap);
    if(R_GetPatchInfo(mapTitlePatch, &info))
        y += (5 * info.geometry.size.height) / 4;

    // Draw map.
    WI_DrawPatchXY2(mapTitlePatch, patchReplacementText(mapTitlePatch, mapName), x, y, ALIGN_TOP);

    DGL_Disable(DGL_TEXTURE_2D);
}

static void drawPercent(int x, int y, int p)
{
    if(p < 0) return;

    Point2Raw origin(x, y);
    char buf[20]; dd_snprintf(buf, 20, "%i", p);
    FR_DrawChar('%', &origin);
    FR_DrawText3(buf, &origin, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);
}

/**
 * Display map completion time and par, or "sucks" message if overflow.
 */
static void drawTime(int x, int y, int t)
{
    if(t < 0) return;

    if(t <= 61 * 59)
    {
        x -= 22;

        int const seconds = t % 60;
        int const minutes = t / 60 % 60;

        char buf[20];
        FR_DrawCharXY(':', x, y);
        if(minutes > 0)
        {
            dd_snprintf(buf, 20, "%d", minutes);
            FR_DrawTextXY3(buf, x, y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);
        }

        dd_snprintf(buf, 20, "%02d", seconds);
        FR_DrawTextXY(buf, x+FR_CharWidth(':'), y);

        return;
    }

    // "sucks"
    patchinfo_t info;
    if(!R_GetPatchInfo(pSucks, &info)) return;

    WI_DrawPatchXY3(pSucks, patchReplacementText(pSucks), x - info.geometry.size.width, y, ALIGN_TOPLEFT, 0, DTF_NO_EFFECTS);
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
    if(!stateCounter)
    {
        if(IS_CLIENT) return;

        WI_End();
        G_IntermissionDone();
    }
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
            initNoState();
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

static void drawDeathmatchStats(int x = DM_MATRIXX + DM_SPACINGX, int y = DM_MATRIXY)
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
        WI_DrawPatchXY(pTotal, patchReplacementText(pTotal), DM_TOTALSX - info.geometry.size.width / 2, DM_MATRIXY - WI_SPACINGY + 10);
    }

    WI_DrawPatchXY(pKillers, patchReplacementText(pKillers), DM_KILLERSX, DM_KILLERSY);
    WI_DrawPatchXY(pVictims, patchReplacementText(pVictims), DM_VICTIMSX, DM_VICTIMSY);

    for(int i = 0; i < NUMTEAMS; ++i)
    {
        if(teamInfo[i].playerCount > 0)
        {
            patchid_t const patchId = pTeamBackgrounds[i];
            char const *replacement = patchReplacementText(patchId);

            patchinfo_t info;
            R_GetPatchInfo(patchId, &info);

            WI_DrawPatchXY(patchId, replacement, x - info.geometry.size.width / 2, DM_MATRIXY - WI_SPACINGY);
            WI_DrawPatchXY(patchId, replacement, DM_MATRIXX - info.geometry.size.width / 2, y);

            /*if(i == inPlayerTeam)
            {
                WI_DrawPatchXY(pFaceDead, Hu_ChoosePatchReplacement(pFaceDead), x - info.geometry.size.width / 2, DM_MATRIXY - WI_SPACINGY);
                WI_DrawPatchXY(pFaceAlive, Hu_ChoosePatchReplacement(pFaceAlive), DM_MATRIXX - info.width / 2, y);
            }*/

            // If more than 1 member, show the member count.
            if(1 != teamInfo[i].playerCount)
            {
                char tmp[20]; sprintf(tmp, "%i", teamInfo[i].playerCount);

                FR_SetFont(FID(GF_FONTA));
                FR_DrawTextXY(tmp, x - info.geometry.size.width / 2 + 1, DM_MATRIXY - WI_SPACINGY + info.geometry.size.height - 8);
                FR_DrawTextXY(tmp, DM_MATRIXX - info.geometry.size.width / 2 + 1, y + info.geometry.size.height - 8);
            }
        }
        else
        {
            patchid_t const patchId = pTeamIcons[i];
            char const *replacement = patchReplacementText(patchId);

            patchinfo_t info;
            R_GetPatchInfo(patchId, &info);

            WI_DrawPatchXY(patchId, replacement, x - info.geometry.size.width / 2, DM_MATRIXY - WI_SPACINGY);
            WI_DrawPatchXY(patchId, replacement, DM_MATRIXX - info.geometry.size.width / 2, y);
        }

        x += DM_SPACINGX;
        y += WI_SPACINGY;
    }

    // Draw stats.
    y = DM_MATRIXY + 10;
    FR_SetFont(FID(GF_SMALL));
    int const w = FR_CharWidth('0');

    for(int i = 0; i < NUMTEAMS; ++i)
    {
        x = DM_MATRIXX + DM_SPACINGX;
        if(teamInfo[i].playerCount > 0)
        {
            char buf[20];
            for(int k = 0; k < NUMTEAMS; ++k)
            {
                if(teamInfo[k].playerCount > 0)
                {
                    dd_snprintf(buf, 20, "%i", dmFrags[i][k]);
                    FR_DrawTextXY3(buf, x + w, y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);
                }
                x += DM_SPACINGX;
            }
            dd_snprintf(buf, 20, "%i", dmTotals[i]);
            FR_DrawTextXY3(buf, DM_TOTALSX + w, y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);
        }

        y += WI_SPACINGY;
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

            int fsum = teamInfo[i].totalFrags;
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
            initNoState();
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
#define ORIGINX             (NG_STATSX + NG_STATSX * !doFrags)

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, 1);
    FR_SetFont(FID(GF_SMALL));
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], 1);

    int const pwidth = FR_CharWidth('%');

    // Draw stat titles (top line).
    patchinfo_t info;
    R_GetPatchInfo(pKills, &info);
    WI_DrawPatchXY(pKills, patchReplacementText(pKills), ORIGINX + NG_SPACINGX - info.geometry.size.width, NG_STATSY);
    int y = NG_STATSY + info.geometry.size.height;

    R_GetPatchInfo(pItems, &info);
    WI_DrawPatchXY(pItems, patchReplacementText(pItems), ORIGINX + 2 * NG_SPACINGX - info.geometry.size.width, NG_STATSY);

    R_GetPatchInfo(pSecret, &info);
    WI_DrawPatchXY(pSecret, patchReplacementText(pSecret), ORIGINX + 3 * NG_SPACINGX - info.geometry.size.width, NG_STATSY);

    if(doFrags)
    {
        R_GetPatchInfo(pFrags, &info);
        WI_DrawPatchXY(pFrags, patchReplacementText(pFrags), ORIGINX + 4 * NG_SPACINGX - info.geometry.size.width, NG_STATSY);
    }

    // Draw stats.
    for(int i = 0; i < NUMTEAMS; ++i)
    {
        if(!teamInfo[i].playerCount)
            continue;

        int x = ORIGINX;
        patchid_t patchId = pTeamBackgrounds[i];

        patchinfo_t info;
        R_GetPatchInfo(patchId, &info);
        WI_DrawPatchXY(patchId, patchReplacementText(patchId), x - info.geometry.size.width, y);

        // If more than 1 member, show the member count.
        if(1 != teamInfo[i].playerCount)
        {
            char tmp[40]; sprintf(tmp, "%i", teamInfo[i].playerCount);

            FR_SetFont(FID(GF_FONTA));
            FR_SetColorAndAlpha(1, 1, 1, 1);
            FR_DrawTextXY(tmp, x - info.geometry.size.width + 1, y + info.geometry.size.height - 8);
            FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], 1);
        }

        //if(i == inPlayerTeam)
        //    WI_DrawPatchXY(pFaceAlive, Hu_ChoosePatchReplacement(pFaceAlive), x - info.geometry.size.width, y);
        //x += NG_SPACINGX;

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

            FR_DrawTextXY3(buf, x, y + 10, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);
        }

        y += WI_SPACINGY;
    }

    DGL_Disable(DGL_TEXTURE_2D);

#undef ORIGINX
}

static void drawSinglePlayerStats()
{
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, 1);
    FR_SetFont(FID(GF_SMALL));
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], 1);

    int const lh = (3 * FR_CharHeight('0')) / 2; // Line height.

    WI_DrawPatchXY(pKills, patchReplacementText(pKills), SP_STATSX, SP_STATSY);
    drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY, cntKills[0]);

    WI_DrawPatchXY(pItems, patchReplacementText(pItems), SP_STATSX, SP_STATSY + lh);
    drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY + lh, cntItems[0]);

    WI_DrawPatchXY(pSecretSP, patchReplacementText(pSecretSP), SP_STATSX, SP_STATSY + 2 * lh);
    drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY + 2 * lh, cntSecret[0]);

    WI_DrawPatchXY(pTime, patchReplacementText(pTime), SP_TIMEX, SP_TIMEY);
    if(cntTime >= 0)
    {
        drawTime(SCREENWIDTH / 2 - SP_TIMEX, SP_TIMEY, cntTime / TICRATE);
    }

    if(wbs->parTime != -1)
    {
        WI_DrawPatchXY(pPar, patchReplacementText(pPar), SCREENWIDTH / 2 + SP_TIMEX, SP_TIMEY);
        if(cntPar >= 0)
        {
            drawTime(SCREENWIDTH - SP_TIMEX, SP_TIMEY, cntPar / TICRATE);
        }
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
        cntTime      = inPlayerInfo[inPlayerNum].time;
        if(wbs->parTime != -1)
            cntPar   = wbs->parTime;

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
            initNoState();
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

void IN_SkipToNext()
{
    advanceState = true;
}

/**
 * Updates stuff each tick.
 */
void WI_Ticker()
{
    ++backgroundAnimCounter;

    maybeAdvanceState();
    switch(inState)
    {
    case ILS_SHOW_STATS:    tickShowStats(); break;
    case ILS_UNUSED:
    case ILS_NONE:          tickNoState();   break;

    default:
        DENG2_ASSERT(!"WI_Ticker: Unknown intermission state");
        break;
    }
}

static void loadData()
{
    pBackground = R_DeclarePatch("INTERPIC");
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
    /// @todo Clearly a kludge but why? -dj
    if(ILS_NONE == inState)
    {
        drawYouAreHere = true;
    }
    /// kludge end.

    dgl_borderedprojectionstate_t bp;
    GL_ConfigureBorderedProjection(&bp, BPF_OVERDRAW_MASK|BPF_OVERDRAW_CLIP, SCREENWIDTH, SCREENHEIGHT, Get(DD_WINDOW_WIDTH), Get(DD_WINDOW_HEIGHT), scalemode_t(cfg.inludeScaleMode));
    GL_BeginBorderedProjection(&bp);

    drawBackground();

    if(ILS_SHOW_STATS != inState)
    {
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

void WI_Init(wbstartstruct_t const &wbstartstruct)
{
    initVariables(wbstartstruct);
    loadData();

    // Calculate team stats.
    std::memset(teamInfo, 0, sizeof(teamInfo));
    for(int i = 0; i < NUMTEAMS; ++i)
    {
        teaminfo_t *tin = &teamInfo[i];

        for(int k = 0; k < MAXPLAYERS; ++k)
        {
            // Is the player in this team?
            if(!inPlayerInfo[k].inGame || cfg.playerColor[k] != i)
                continue;

            ++tin->playerCount;

            // Check the frags.
            for(k = 0; k < MAXPLAYERS; ++k)
                tin->frags[cfg.playerColor[k]] += inPlayerInfo[k].frags[k];

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
    }
    else if(IS_NETGAME)
    {
        initNetgameStats();
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
    case ILS_SHOW_STATS:    initShowStats(); break;
    case ILS_UNUSED:
    case ILS_NONE:          initNoState();   break;

    default:
        DENG2_ASSERT(!"WI_SetState: Unknown intermission state");
        break;
    }
}
