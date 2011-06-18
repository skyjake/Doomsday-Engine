/**\file wi_stuff.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include <assert.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "jdoom64.h"

#include "hu_stuff.h"
#include "d_net.h"
#include "p_start.h"

#define NUMMAPS                 (9)

typedef struct teaminfo_s {
    int playerCount; /// @c 0= team not present.
    int frags[NUMTEAMS];
    int totalFrags; /// Kills minus suicides.
    int items;
    int kills;
    int secret;
} teaminfo_t;

static teaminfo_t teamInfo[NUMTEAMS];

// Used to accelerate or skip a stage.
static boolean advanceState;

static boolean drawYouAreHere = false;

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
static wbstartstruct_t* wbs;
static wbplayerstruct_t* inPlayerInfo;

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
static patchid_t pTeamBackgrounds[NUMTEAMS];
static patchid_t pTeamIcons[NUMTEAMS];

static cvartemplate_t cvars[] = {
    { "inlude-stretch",  0, CVT_BYTE, &cfg.inludeScaleMode, SCALEMODE_FIRST, SCALEMODE_LAST },
    { NULL }
};

void WI_Register(void)
{
    int i;
    for(i = 0; cvars[i].name; ++i)
        Con_AddVariable(cvars + i);
}

static void drawBackground(void)
{
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, 1);
    GL_DrawPatch2(pBackground, 0, 0, ALIGN_TOPLEFT, DPF_NO_OFFSET);
    DGL_Disable(DGL_TEXTURE_2D);
}

static void drawFinishedTitle(void)
{
    int x = SCREENWIDTH/2, y = WI_TITLEY;
    int mapNum = wbs->currentMap;
    char* mapName = (char*) DD_GetVariable(DD_MAP_NAME);
    patchid_t patchId;
    patchinfo_t info;

    // Skip the Map #.
    if(NULL != mapName)
    {
        char* ptr = strchr(mapName, ':');
        if(NULL != ptr)
        {
            mapName = M_SkipWhite(ptr + 1);
        }
    }

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, 1);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(1, 1, 1, 1);

    // Draw <MapName>
    patchId = pMapNames[mapNum];
    WI_DrawPatch3(patchId, Hu_ChoosePatchReplacement2(patchId, mapName, false), x, y, ALIGN_TOP, 0, 0, FID(GF_FONTB));
    if(R_GetPatchInfo(patchId, &info))
        y += (5 * info.height) / 4;
    // Draw "Finished!"
    WI_DrawPatch3(pFinished, Hu_ChoosePatchReplacement(pFinished), x, y, ALIGN_TOP, 0, 0, FID(GF_FONTB));

    DGL_Disable(DGL_TEXTURE_2D);
}

static void drawEnteringTitle(void)
{
    int x = SCREENWIDTH/2, y = WI_TITLEY;
    char* mapName = NULL;
    ddmapinfo_t minfo;
    patchid_t patchId;
    char lumpName[9];
    patchinfo_t info;

    // See if there is a map name.
    P_MapId(wbs->episode, wbs->nextMap, lumpName);
    if(Def_Get(DD_DEF_MAP_INFO, lumpName, &minfo) && minfo.name)
    {
        if(Def_Get(DD_DEF_TEXT, minfo.name, &mapName) == -1)
            mapName = minfo.name;
    }

    // Skip the E#M# or Map #.
    if(NULL != mapName)
    {
        char* ptr = strchr(mapName, ':');
        if(NULL != ptr)
        {
            mapName = M_SkipWhite(ptr + 1);
        }
    }

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, 1);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(1, 1, 1, 1);

    // Draw "Entering"
    WI_DrawPatch3(pEntering, Hu_ChoosePatchReplacement(pEntering), x, y, ALIGN_TOP, 0, 0, FID(GF_FONTB));

    // Draw map.
    if(R_GetPatchInfo(pMapNames[wbs->nextMap], &info))
        y += (5 * info.height) / 4;
    patchId = pMapNames[(wbs->episode * 8) + wbs->nextMap];
    WI_DrawPatch3(patchId, Hu_ChoosePatchReplacement2(patchId, mapName, false), x, y, ALIGN_TOP, 0, 0, FID(GF_FONTB));

    DGL_Disable(DGL_TEXTURE_2D);
}

static void drawPercent(int x, int y, int p)
{
    char buf[20];
    if(p < 0)
        return;
    dd_snprintf(buf, 20, "%i", p);
    FR_DrawChar('%', x, y);
    FR_DrawText3(buf, x, y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);
}

/**
 * Display map completion time and par, or "sucks" message if overflow.
 */
static void drawTime(int x, int y, int t)
{
    if(t < 0)
        return;

    if(t <= 61 * 59)
    {
        int seconds = t % 60, minutes = t / 60 % 60;
        char buf[20];

        x -= 22;

        FR_DrawChar(':', x, y);
        if(minutes > 0)
        {
            dd_snprintf(buf, 20, "%d", minutes);
            FR_DrawText3(buf, x, y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);
        }
        dd_snprintf(buf, 20, "%02d", seconds);
        FR_DrawText(buf, x+FR_CharWidth(':'), y);
        return;
    }

    // "sucks"
    {
    patchinfo_t info;
    if(!R_GetPatchInfo(pSucks, &info))
        return;
    WI_DrawPatch3(pSucks, Hu_ChoosePatchReplacement(pSucks), x - info.width, y, ALIGN_TOPLEFT, 0, DTF_NO_EFFECTS, FID(GF_SMALL));
    }
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
        if(IS_CLIENT)
            return;
        WI_End();
        G_WorldDone();
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
        memset(dmFrags[i], 0, sizeof(dmFrags[i]));
}

static void updateDeathmatchStats(void)
{
    int i, j;
    boolean stillTicking;

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

static void drawDeathmatchStats(void)
{
    int i, j, x, y, w, lh = WI_SPACINGY; // Line height.

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, 1);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], 1);
    FR_LoadDefaultAttrib();

    // Draw stat titles (top line).
    { patchinfo_t info;
    if(R_GetPatchInfo(pTotal, &info))
        WI_DrawPatch(pTotal, Hu_ChoosePatchReplacement(pTotal), DM_TOTALSX - info.width / 2, DM_MATRIXY - WI_SPACINGY + 10); }

    WI_DrawPatch(pKillers, Hu_ChoosePatchReplacement(pKillers), DM_KILLERSX, DM_KILLERSY);
    WI_DrawPatch(pVictims, Hu_ChoosePatchReplacement(pVictims), DM_VICTIMSX, DM_VICTIMSY);

    x = DM_MATRIXX + DM_SPACINGX;
    y = DM_MATRIXY;

    for(i = 0; i < NUMTEAMS; ++i)
    {
        if(teamInfo[i].playerCount > 0)
        {
            patchid_t patchId = pTeamBackgrounds[i];
            const char* replacement = Hu_ChoosePatchReplacement(patchId);
            patchinfo_t info;

            R_GetPatchInfo(patchId, &info);

            WI_DrawPatch(patchId, replacement, x - info.width / 2, DM_MATRIXY - WI_SPACINGY);
            WI_DrawPatch(patchId, replacement, DM_MATRIXX - info.width / 2, y);

            /*if(i == inPlayerTeam)
            {
                WI_DrawPatch(pFaceDead, Hu_ChoosePatchReplacement(pFaceDead), x - info.width / 2, DM_MATRIXY - WI_SPACINGY);
                WI_DrawPatch(pFaceAlive, Hu_ChoosePatchReplacement(pFaceAlive), DM_MATRIXX - info.width / 2, y);
            }*/

            // If more than 1 member, show the member count.
            if(1 != teamInfo[i].playerCount)
            {
                char tmp[20];

                sprintf(tmp, "%i", teamInfo[i].playerCount);

                FR_SetFont(FID(GF_FONTA));
                FR_DrawText(tmp, x - info.width / 2 + 1, DM_MATRIXY - WI_SPACINGY + info.height - 8);
                FR_DrawText(tmp, DM_MATRIXX - info.width / 2 + 1, y + info.height - 8);
            }
        }
        else
        {
            patchid_t patchId = pTeamIcons[i];
            const char* replacement = Hu_ChoosePatchReplacement(patchId);
            patchinfo_t info;
            R_GetPatchInfo(patchId, &info);
            WI_DrawPatch(patchId, replacement, x - info.width / 2, DM_MATRIXY - WI_SPACINGY);
            WI_DrawPatch(patchId, replacement, DM_MATRIXX - info.width / 2, y);
        }

        x += DM_SPACINGX;
        y += WI_SPACINGY;
    }

    // Draw stats.
    y = DM_MATRIXY + 10;
    FR_SetFont(FID(GF_SMALL));
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
                    FR_DrawText3(buf, x + w, y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);
                }
                x += DM_SPACINGX;
            }
            dd_snprintf(buf, 20, "%i", dmTotals[i]);
            FR_DrawText3(buf, DM_TOTALSX + w, y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);
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
}

static void updateNetgameStats(void)
{
    boolean stillTicking;
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

static void drawNetgameStats(void)
{
#define ORIGINX             (NG_STATSX + NG_STATSX*!doFrags)

    int i, x, y, pwidth;
    patchinfo_t info;

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, 1);
    FR_SetFont(FID(GF_SMALL));
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], 1);

    pwidth = FR_CharWidth('%');

    // Draw stat titles (top line).
    R_GetPatchInfo(pKills, &info);
    WI_DrawPatch(pKills, Hu_ChoosePatchReplacement(pKills), ORIGINX + NG_SPACINGX - info.width, NG_STATSY);
    y = NG_STATSY + info.height;

    R_GetPatchInfo(pItems, &info);
    WI_DrawPatch(pItems, Hu_ChoosePatchReplacement(pItems), ORIGINX + 2 * NG_SPACINGX - info.width, NG_STATSY);

    R_GetPatchInfo(pSecret, &info);
    WI_DrawPatch(pSecret, Hu_ChoosePatchReplacement(pSecret), ORIGINX + 3 * NG_SPACINGX - info.width, NG_STATSY);

    if(doFrags)
    {
        R_GetPatchInfo(pFrags, &info);
        WI_DrawPatch(pFrags, Hu_ChoosePatchReplacement(pFrags), ORIGINX + 4 * NG_SPACINGX - info.width, NG_STATSY);
    }

    // Draw stats.
    for(i = 0; i < NUMTEAMS; ++i)
    {
        patchid_t patchId;
        patchinfo_t info;

        if(0 == teamInfo[i].playerCount)
            continue;

        x = ORIGINX;
        patchId = pTeamBackgrounds[i];
        R_GetPatchInfo(patchId, &info);
        WI_DrawPatch(patchId, Hu_ChoosePatchReplacement(patchId), x - info.width, y);

        // If more than 1 member, show the member count.
        if(1 != teamInfo[i].playerCount)
        {
            char tmp[40];

            sprintf(tmp, "%i", teamInfo[i].playerCount);
            FR_SetFont(FID(GF_FONTA));
            FR_SetColorAndAlpha(1, 1, 1, 1);
            FR_DrawText(tmp, x - info.width + 1, y + info.height - 8);
            FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], 1);
        }

        //if(i == inPlayerTeam)
        //    WI_DrawPatch(pFaceAlive, Hu_ChoosePatchReplacement(pFaceAlive), x - info.width, y);
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
            char buf[20];
            dd_snprintf(buf, 20, "%i", cntFrags[i]);
            FR_DrawText3(buf, x, y + 10, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);
        }

        y += WI_SPACINGY;
    }

    DGL_Disable(DGL_TEXTURE_2D);

#undef ORIGINX
}

static void drawSinglePlayerStats(void)
{
    int lh;

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, 1);
    FR_SetFont(FID(GF_SMALL));
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], 1);

    lh = (3 * FR_CharHeight('0')) / 2; // Line height.

    WI_DrawPatch(pKills, Hu_ChoosePatchReplacement(pKills), SP_STATSX, SP_STATSY);
    drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY, cntKills[0]);

    WI_DrawPatch(pItems, Hu_ChoosePatchReplacement(pItems), SP_STATSX, SP_STATSY + lh);
    drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY + lh, cntItems[0]);

    WI_DrawPatch(pSecretSP, Hu_ChoosePatchReplacement(pSecretSP), SP_STATSX, SP_STATSY + 2 * lh);
    drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY + 2 * lh, cntSecret[0]);

    WI_DrawPatch(pTime, Hu_ChoosePatchReplacement(pTime), SP_TIMEX, SP_TIMEY);
    if(cntTime >= 0)
    {
        drawTime(SCREENWIDTH / 2 - SP_TIMEX, SP_TIMEY, cntTime / TICRATE);
    }

    if(wbs->parTime != -1)
    {
        WI_DrawPatch(pPar, Hu_ChoosePatchReplacement(pPar), SCREENWIDTH / 2 + SP_TIMEX, SP_TIMEY);
        if(cntPar >= 0)
        {
            drawTime(SCREENWIDTH - SP_TIMEX, SP_TIMEY, cntPar / TICRATE);
        }
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
}

static void tickShowStats(void)
{
    if(deathmatch)
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

static void drawStats(void)
{
    if(deathmatch)
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
                advanceState = true;
            player->attackDown = true;
        }
        else
        {
            player->attackDown = false;
        }

        if(player->brain.use)
        {
            if(!player->useDown)
                advanceState = true;
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

    maybeAdvanceState();
    switch(inState)
    {
    case ILS_SHOW_STATS:    tickShowStats(); break;
    case ILS_UNUSED:
    case ILS_NONE:          tickNoState(); break;
    default:
#if _DEBUG
        Con_Error("WI_Ticker: Invalid state %i.", (int) inState);
#endif
    }
}

static void loadData(void)
{
    char name[9];

    pBackground = R_PrecachePatch("INTERPIC", NULL);
    pFinished   = R_PrecachePatch("WIF", NULL);
    pEntering   = R_PrecachePatch("WIENTER", NULL);
    pKills      = R_PrecachePatch("WIOSTK", NULL);
    pSecret     = R_PrecachePatch("WIOSTS", NULL);
    pSecretSP   = R_PrecachePatch("WISCRT2", NULL);
    pItems      = R_PrecachePatch("WIOSTI", NULL);
    pFrags      = R_PrecachePatch("WIFRGS", NULL);
    pTime       = R_PrecachePatch("WITIME", NULL);
    pSucks      = R_PrecachePatch("WISUCKS", NULL);
    pPar        = R_PrecachePatch("WIPAR", NULL);
    pKillers    = R_PrecachePatch("WIKILRS", NULL);
    pVictims    = R_PrecachePatch("WIVCTMS", NULL);
    pTotal      = R_PrecachePatch("WIMSTT", NULL);

    { int i;
    for(i = 0; i < NUMTEAMS; ++i)
    {
        sprintf(name, "STPB%d", i);
        pTeamBackgrounds[i] = R_PrecachePatch(name, NULL);

        sprintf(name, "WIBP%d", i + 1);
        pTeamIcons[i] = R_PrecachePatch(name, NULL);
    }}
}

void WI_Drawer(void)
{
    borderedprojectionstate_t bp;

    /// \kludge dj: Clearly a kludge but why?
    if(ILS_NONE == inState)
    {
        drawYouAreHere = true;
    }
    /// kludge end.

    GL_ConfigureBorderedProjection(&bp, BPF_OVERDRAW_MASK|BPF_OVERDRAW_CLIP, SCREENWIDTH, SCREENHEIGHT, Get(DD_WINDOW_WIDTH), Get(DD_WINDOW_HEIGHT), cfg.inludeScaleMode);
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

    if(deathmatch)
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
    case ILS_NONE:          initNoState(); break;
    default:
#if _DEBUG
        Con_Error("WI_SetState: Invalid state %i.", (int) st);
#endif
    }
}
