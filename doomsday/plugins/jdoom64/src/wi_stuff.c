/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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

/**
 * wi_stuff.c: Intermission/stat screens - jDoom64 specific.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "jdoom64.h"

#include "hu_stuff.h"
#include "d_net.h"
#include "p_start.h"

// MACROS ------------------------------------------------------------------

#define NUM_TEAMS           (4) // Color = team.

// GLOBAL LOCATIONS
#define WI_TITLEY           (2)
#define WI_SPACINGY         (33)

// SINGPLE-PLAYER STUFF
#define SP_STATSX           (50)
#define SP_STATSY           (50)
#define SP_TIMEX            (16)
#define SP_TIMEY            (SCREENHEIGHT-32)

// NET GAME STUFF
#define NG_STATSY           (50)
#define NG_STATSX           (32)
#define NG_SPACINGX         (64)

// DEATHMATCH STUFF
#define DM_MATRIXX          (42)
#define DM_MATRIXY          (68)
#define DM_SPACINGX         (40)
#define DM_TOTALSX          (269)
#define DM_KILLERSX         (10)
#define DM_KILLERSY         (100)
#define DM_VICTIMSX         (5)
#define DM_VICTIMSY         (50)

// TYPES -------------------------------------------------------------------

typedef struct teaminfo_s {
    int             members; // 0 if team not present.
    int             frags[NUM_TEAMS];
    int             totalFrags; // Kills minus suicides.
    int             items;
    int             kills;
    int             secret;
} teaminfo_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static teaminfo_t teamInfo[NUM_TEAMS];

// Used to accelerate or skip a stage.
static int accelerateStage;

static int spState, dmState, ngState;

static int dmFrags[NUM_TEAMS][NUM_TEAMS];
static int dmTotals[NUM_TEAMS];

static int doFrags;

// wbs->pnum
static int me, myTeam;

// Specifies current state.
static interludestate_t state;

// Contains information passed into intermission.
static wbstartstruct_t *wbs;

static wbplayerstruct_t *plrs; // wbs->plyr[]

// Used for general timing.
static int cnt;

// Used for timing of background animation.
static int bcnt;

static int cntKills[NUM_TEAMS];
static int cntItems[NUM_TEAMS];
static int cntSecret[NUM_TEAMS];
static int cntFrags[NUM_TEAMS];
static int cntTime;
static int cntPar;

static int cntPause;

static patchid_t bg; // Background (map of levels).
static patchid_t finished; // "Finished!"
static patchid_t entering; // "Entering"
static patchid_t sp_secret; // "secret"
static patchid_t kills; // "Kills"
static patchid_t secret; // "Scrt"
static patchid_t items; // "Items"
static patchid_t frags; // "Frags"
static patchid_t time; // "Time"
static patchid_t par; // "Par"
static patchid_t sucks; // "Sucks!"
static patchid_t killers; // "killers"
static patchid_t victims; // "victims"
static patchid_t total; // "Total"
static patchid_t star; // Player icon (alive).
static patchid_t bstar; // Player icon (dead).
static patchid_t p[NUM_TEAMS]; // "red P[1..NUM_TEAMS]"
static patchid_t bp[NUM_TEAMS]; // "gray P[1..NUM_TEAMS]"

// CODE --------------------------------------------------------------------

void WI_slamBackground(void)
{
    DGL_Color4f(1, 1, 1, 1);
    GL_DrawPatch2(bg, 0, 0, DPF_ALIGN_TOPLEFT|DPF_NO_OFFSET);
}

/**
 * Draws "<Levelname> Finished!"
 */
void WI_drawLF(void)
{
    int y = WI_TITLEY, mapnum;
    char* lname, *ptr;

    lname = (char*) DD_GetVariable(DD_MAP_NAME);

    if(gameMode == commercial)
        mapnum = wbs->currentMap;
    else
        mapnum = (wbs->episode * 8) + wbs->currentMap;

    ptr = strchr(lname, ':'); // Skip the E#M# or Level #.
    if(ptr)
    {
        lname = ptr + 1;
        while(*lname && isspace(*lname))
            lname++;
    }

    // Draw <LevelName>
    WI_DrawPatch2(mapNamePatches[mapnum], SCREENWIDTH / 2, y, lname, false, 0);

    // Draw "Finished!"
    /// \fixme WI_DrawPatch2 should return the visible height.
    {
    patchinfo_t info;
    if(R_GetPatchInfo(mapNamePatches[mapnum], &info))
        y += (5 * info.height) / 4;
    }

    WI_DrawPatch2(finished, SCREENWIDTH / 2, y, NULL, false, DPF_ALIGN_TOP);
}

/**
 * Draws "Entering <LevelName>"
 */
void WI_drawEL(void)
{
    int y = WI_TITLEY;
    char* lname = "", *ptr;
    ddmapinfo_t minfo;
    char levid[10];

    P_MapId(wbs->episode, wbs->nextMap, levid);

    // See if there is a level name.
    if(Def_Get(DD_DEF_MAP_INFO, levid, &minfo) && minfo.name)
    {
        if(Def_Get(DD_DEF_TEXT, minfo.name, &lname) == -1)
            lname = minfo.name;
    }

    ptr = strchr(lname, ':'); // Skip the E#M# or Level #.
    if(ptr)
    {
        lname = ptr + 1;
        while(*lname && isspace(*lname))
            lname++;
    }

    // Draw "Entering"
    WI_DrawPatch2(entering, SCREENWIDTH / 2, y, NULL, false, DPF_ALIGN_TOP);

    // Draw level.
    {
    patchinfo_t info;
    if(R_GetPatchInfo(mapNamePatches[wbs->nextMap], &info))
        y += (5 * info.height) / 4;
    }

    WI_DrawPatch2(mapNamePatches[(wbs->episode * 8) + wbs->nextMap], SCREENWIDTH / 2, y, lname, false, 0);
}

void WI_initAnimatedBack(void)
{
    // Nothing to do.
}

void WI_updateAnimatedBack(void)
{
    // Nothing to do.
}

void WI_drawAnimatedBack(void)
{
    // Nothing to do.
}

/**
 * Draws a number.
 * If digits > 0, then use that many digits minimum, otherwise only use as
 * many as necessary.
 *
 * @return              New x position.
 */
int WI_drawNum(int x, int y, int n, int digits)
{
    int                 fontwidth = GL_CharWidth('0', GF_SMALL);
    int                 neg, temp;

    if(digits < 0)
    {
        if(!n)
        {
            // Make variable-length zeros 1 digit long.
            digits = 1;
        }
        else
        {
            // Figure out # of digits in #
            digits = 0;
            temp = n;
            while(temp)
            {
                temp /= 10;
                digits++;
            }
        }
    }

    neg = n < 0;
    if(neg)
        n = -n;

    // If non-number, do not draw it.
    if(n == 1994)
        return 0;

    // Draw the new number.
    while(digits--)
    {
        x -= fontwidth;
        GL_DrawChar2('0' + (n % 10), x, y, GF_SMALL);
        n /= 10;
    }

    // Draw a minus sign if necessary.
    if(neg)
    {
        GL_DrawChar2('-', x - 8, y, GF_SMALL);
        x -= 8;
    }

    return x;
}

void WI_drawPercent(int x, int y, int p)
{
    char buf[20];

    if(p < 0)
        return;

    DGL_Color4f(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], 1);
    GL_DrawChar2('%', x, y, GF_SMALL);

    dd_snprintf(buf, 20, "%i", p);
    GL_DrawTextFragment3(buf, x, y, GF_SMALL, DTF_ALIGN_TOPRIGHT|DTF_NO_EFFECTS);
}

/**
 * Display map completion time and par, or "sucks" message if overflow.
 */
void WI_drawTime(int x, int y, int t)
{
    if(t < 0)
        return;

    if(t <= 61 * 59)
    {
        int seconds = t % 60, minutes = t / 60 % 60;
        char buf[20];

        x -= 22;
        
        DGL_Color4f(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], 1);
        GL_DrawChar2(':', x, y, GF_SMALL);
        if(minutes > 0)
        {
            dd_snprintf(buf, 20, "%d", minutes);
            GL_DrawTextFragment3(buf, x, y, GF_SMALL, DTF_ALIGN_TOPRIGHT|DTF_NO_EFFECTS);
        }
        dd_snprintf(buf, 20, "%02d", seconds);
        GL_DrawTextFragment3(buf, x+GL_CharWidth(':', GF_SMALL), y, GF_SMALL, DTF_ALIGN_TOPLEFT|DTF_NO_EFFECTS);
        return;
    }

    // "sucks"
    {
    patchinfo_t info;
    if(!R_GetPatchInfo(sucks, &info))
        return;
    WI_DrawPatch4(sucks, x - info.width, y, NULL, GF_SMALL, false, DTF_ALIGN_TOPLEFT|DTF_NO_EFFECTS, defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], 1);
    }
}

void WI_End(void)
{
    NetSv_Intermission(IMF_END, 0, 0);
}

void WI_initNoState(void)
{
    state = ILS_NONE;
    accelerateStage = 0;
    cnt = 10;

    NetSv_Intermission(IMF_STATE, state, 0);
}

void WI_updateNoState(void)
{
    WI_updateAnimatedBack();

    if(!--cnt)
    {
        if(IS_CLIENT)
            return;
        WI_End();
        G_WorldDone();
    }
}

void WI_drawNoState(void)
{
    // Nothing to do.
}

int WI_fragSum(int teamnum)
{
    return teamInfo[teamnum].totalFrags;
}

void WI_initDeathmatchStats(void)
{
    int                 i;

    state = ILS_SHOW_STATS;
    accelerateStage = 0;
    dmState = 1;

    cntPause = TICRATE;

    // Clear the on-screen counters.
    memset(dmTotals, 0, sizeof(dmTotals));
    for(i = 0; i < NUM_TEAMS; ++i)
        memset(dmFrags[i], 0, sizeof(dmFrags[i]));

    WI_initAnimatedBack();
}

void WI_updateDeathmatchStats(void)
{
    int                 i, j;
    boolean             stillTicking;

    WI_updateAnimatedBack();

    if(accelerateStage && dmState != 4)
    {
        accelerateStage = 0;
        for(i = 0; i < NUM_TEAMS; ++i)
        {
            for(j = 0; j < NUM_TEAMS; ++j)
            {
                dmFrags[i][j] = teamInfo[i].frags[j];
            }

            dmTotals[i] = WI_fragSum(i);
        }

        S_LocalSound(SFX_BAREXP, 0);
        dmState = 4;
    }

    if(dmState == 2)
    {
        if(!(bcnt & 3))
            S_LocalSound(SFX_PISTOL, 0);

        stillTicking = false;
        for(i = 0; i < NUM_TEAMS; ++i)
        {
            for(j = 0; j < NUM_TEAMS; ++j)
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

            dmTotals[i] = WI_fragSum(i);

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
        if(accelerateStage)
        {
            S_LocalSound(SFX_SLOP, 0);
            WI_initNoState();
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

void WI_drawDeathmatchStats(void)
{
    int i, j, x, y, w, lh; // Line height.

    lh = WI_SPACINGY;

    WI_slamBackground();

    // Draw animated background.
    WI_drawAnimatedBack();
    WI_drawLF();

    // Draw stat titles (top line).
    {
    patchinfo_t info;
    if(R_GetPatchInfo(total, &info))
        WI_DrawPatch(total, DM_TOTALSX - info.width / 2, DM_MATRIXY - WI_SPACINGY + 10);
    }

    WI_DrawPatch(killers, DM_KILLERSX, DM_KILLERSY);
    WI_DrawPatch(victims, DM_VICTIMSX, DM_VICTIMSY);

    x = DM_MATRIXX + DM_SPACINGX;
    y = DM_MATRIXY;

    for(i = 0; i < NUM_TEAMS; ++i)
    {
        if(teamInfo[i].members)
        {
            patchid_t patch = p[i];
            patchinfo_t info;

            R_GetPatchInfo(patch, &info);

            WI_DrawPatch(patch, x - info.width / 2, DM_MATRIXY - WI_SPACINGY);
            WI_DrawPatch(patch, DM_MATRIXX - info.width / 2, y);

            if(i == myTeam)
            {
                WI_DrawPatch(bstar, x - info.width / 2, DM_MATRIXY - WI_SPACINGY);
                WI_DrawPatch(star, DM_MATRIXX - info.width / 2, y);
            }

            // If more than 1 member, show the member count.
            if(teamInfo[i].members > 1)
            {
                char tmp[20];

                sprintf(tmp, "%i", teamInfo[i].members);
                DGL_Color4f(1, 1, 1, 1);
                GL_DrawTextFragment3(tmp, x - info.width / 2 + 1, DM_MATRIXY - WI_SPACINGY + info.height - 8, GF_FONTA, DTF_ALIGN_TOPLEFT|DTF_NO_EFFECTS);
                GL_DrawTextFragment3(tmp, DM_MATRIXX - info.width / 2 + 1, y + info.height - 8, GF_FONTA, DTF_ALIGN_TOPLEFT|DTF_NO_EFFECTS);
            }
        }
        else
        {
            patchid_t patch = bp[i];
            patchinfo_t info;
            R_GetPatchInfo(patch, &info);
            WI_DrawPatch(patch, x - info.width / 2, DM_MATRIXY - WI_SPACINGY);
            WI_DrawPatch(patch, DM_MATRIXX - info.width / 2, y);
        }

        x += DM_SPACINGX;
        y += WI_SPACINGY;
    }

    // Draw stats.
    y = DM_MATRIXY + 10;
    w = GL_CharWidth('0', GF_SMALL);

    for(i = 0; i < NUM_TEAMS; ++i)
    {
        x = DM_MATRIXX + DM_SPACINGX;
        if(teamInfo[i].members)
        {
            char buf[20];
            for(j = 0; j < NUM_TEAMS; ++j)
            {
                if(teamInfo[j].members)
                {
                    dd_snprintf(buf, 20, "%i", dmFrags[i][j]);
                    GL_DrawTextFragment3(buf, x + w, y, GF_SMALL, DTF_ALIGN_TOPRIGHT|DTF_NO_EFFECTS);
                }
                x += DM_SPACINGX;
            }
            dd_snprintf(buf, 20, "%i", dmTotals[i]);
            GL_DrawTextFragment3(buf, DM_TOTALSX + w, y, GF_SMALL, DTF_ALIGN_TOPRIGHT|DTF_NO_EFFECTS);
        }

        y += WI_SPACINGY;
    }
}

void WI_initNetgameStats(void)
{
    int i;

    state = ILS_SHOW_STATS;
    accelerateStage = 0;
    ngState = 1;
    cntPause = TICRATE;

    memset(cntKills, 0, sizeof(cntKills));
    memset(cntItems, 0, sizeof(cntItems));
    memset(cntSecret, 0, sizeof(cntSecret));
    memset(cntFrags, 0, sizeof(cntFrags));

    doFrags = 0;
    for(i = 0; i < NUM_TEAMS; ++i)
    {
        doFrags += teamInfo[i].totalFrags;
    }
    doFrags = !doFrags;

    WI_initAnimatedBack();
}

void WI_updateNetgameStats(void)
{
    int                 i, fsum;
    boolean             stillTicking;

    WI_updateAnimatedBack();

    if(accelerateStage && ngState != 10)
    {
        accelerateStage = 0;
        for(i = 0; i < NUM_TEAMS; ++i)
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
        if(!(bcnt & 3))
            S_LocalSound(SFX_PISTOL, 0);
        stillTicking = false;

        for(i = 0; i < NUM_TEAMS; ++i)
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
        if(!(bcnt & 3))
            S_LocalSound(SFX_PISTOL, 0);
        stillTicking = false;

        for(i = 0; i < NUM_TEAMS; ++i)
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
        if(!(bcnt & 3))
            S_LocalSound(SFX_PISTOL, 0);

        stillTicking = false;

        for(i = 0; i < NUM_TEAMS; ++i)
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
        if(!(bcnt & 3))
            S_LocalSound(SFX_PISTOL, 0);

        stillTicking = false;

        for(i = 0; i < NUM_TEAMS; ++i)
        {
            cntFrags[i] += 1;

            if(cntFrags[i] >= (fsum = WI_fragSum(i)))
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
        if(accelerateStage)
        {
            S_LocalSound(SFX_SGCOCK, 0);
            WI_initNoState();
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

void WI_drawNetgameStats(void)
{
#define ORIGINX             (NG_STATSX + starWidth/2 + NG_STATSX*!doFrags)

    int i, x, y, starWidth, pwidth = GL_CharWidth('%', GF_SMALL);
    patchinfo_t info;

    R_GetPatchInfo(star, &info);
    starWidth = info.width;

    WI_slamBackground();

    // Draw animated background.
    WI_drawAnimatedBack();

    WI_drawLF();

    // Draw stat titles (top line).
    R_GetPatchInfo(kills, &info);
    WI_DrawPatch(kills, ORIGINX + NG_SPACINGX - info.width, NG_STATSY);
    y = NG_STATSY + info.height;

    R_GetPatchInfo(items, &info);
    WI_DrawPatch(items, ORIGINX + 2 * NG_SPACINGX - info.width, NG_STATSY);

    R_GetPatchInfo(secret, &info);
    WI_DrawPatch(secret, ORIGINX + 3 * NG_SPACINGX - info.width, NG_STATSY);

    if(doFrags)
    {
        R_GetPatchInfo(frags, &info);
        WI_DrawPatch(frags, ORIGINX + 4 * NG_SPACINGX - info.width, NG_STATSY);
    }

    // Draw stats.
    for(i = 0; i < NUM_TEAMS; ++i)
    {
        patchinfo_t info;

        if(!teamInfo[i].members)
            continue;

        x = ORIGINX;
        R_GetPatchInfo(p[i], &info);
        WI_DrawPatch(p[i], x - info.width, y);

        // If more than 1 member, show the member count.
        if(teamInfo[i].members > 1)
        {
            char tmp[40];

            sprintf(tmp, "%i", teamInfo[i].members);
            DGL_Color4f(1, 1, 1, 1);
            GL_DrawTextFragment3(tmp, x - info.width + 1, y + info.height - 8, GF_FONTA, DTF_ALIGN_TOPLEFT|DTF_NO_EFFECTS);
        }

        if(i == myTeam)
            WI_DrawPatch(star, x - info.width, y);
        x += NG_SPACINGX;

        WI_drawPercent(x - pwidth, y + 10, cntKills[i]);
        x += NG_SPACINGX;

        WI_drawPercent(x - pwidth, y + 10, cntItems[i]);
        x += NG_SPACINGX;

        WI_drawPercent(x - pwidth, y + 10, cntSecret[i]);
        x += NG_SPACINGX;

        if(doFrags)
        {
            char buf[20];
            dd_snprintf(buf, 20, "%i", cntFrags[i]);
            GL_DrawTextFragment3(buf, x, y + 10, GF_SMALL, DTF_ALIGN_TOPRIGHT|DTF_NO_EFFECTS);
        }

        y += WI_SPACINGY;
    }

#undef ORIGINX
}

void WI_initStats(void)
{
    state = ILS_SHOW_STATS;
    accelerateStage = 0;
    spState = 1;
    cntKills[0] = cntItems[0] = cntSecret[0] = -1;
    cntTime = cntPar = -1;
    cntPause = TICRATE;
    WI_initAnimatedBack();
}

void WI_updateStats(void)
{
    WI_updateAnimatedBack();

    if(accelerateStage && spState != 10)
    {
        accelerateStage = 0;
        cntKills[0] = (plrs[me].kills * 100) / wbs->maxKills;
        cntItems[0] = (plrs[me].items * 100) / wbs->maxItems;
        cntSecret[0] = (plrs[me].secret * 100) / wbs->maxSecret;
        cntTime = plrs[me].time;
        if(wbs->parTime != -1)
            cntPar = wbs->parTime;
        S_LocalSound(SFX_BAREXP, 0);
        spState = 10;
    }

    if(spState == 2)
    {
        cntKills[0] += 2;

        if(!(bcnt & 3))
            S_LocalSound(SFX_PISTOL, 0);

        if(cntKills[0] >= (plrs[me].kills * 100) / wbs->maxKills)
        {
            cntKills[0] = (plrs[me].kills * 100) / wbs->maxKills;
            S_LocalSound(SFX_BAREXP, 0);
            spState++;
        }
    }
    else if(spState == 4)
    {
        cntItems[0] += 2;

        if(!(bcnt & 3))
            S_LocalSound(SFX_PISTOL, 0);

        if(cntItems[0] >= (plrs[me].items * 100) / wbs->maxItems)
        {
            cntItems[0] = (plrs[me].items * 100) / wbs->maxItems;
            S_LocalSound(SFX_BAREXP, 0);
            spState++;
        }
    }
    else if(spState == 6)
    {
        cntSecret[0] += 2;

        if(!(bcnt & 3))
            S_LocalSound(SFX_PISTOL, 0);

        if(cntSecret[0] >= (plrs[me].secret * 100) / wbs->maxSecret)
        {
            cntSecret[0] = (plrs[me].secret * 100) / wbs->maxSecret;
            S_LocalSound(SFX_BAREXP, 0);
            spState++;
        }
    }
    else if(spState == 8)
    {
        if(!(bcnt & 3))
            S_LocalSound(SFX_PISTOL, 0);

        if(cntTime == -1)
            cntTime = 0;
        cntTime += TICRATE * 3;

        // Par time might not be defined so count up and stop on play time instead.
        if(cntTime >= plrs[me].time)
        {
            cntTime = plrs[me].time;
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
        if(accelerateStage)
        {
            S_LocalSound(SFX_SGCOCK, 0);
            WI_initNoState();
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

void WI_drawStats(void)
{
    int lh = (3 * GL_CharHeight('0', GF_SMALL)) / 2; // Line height.

    WI_slamBackground();

    // Draw animated background.
    WI_drawAnimatedBack();

    WI_drawLF();

    WI_DrawPatch(kills, SP_STATSX, SP_STATSY);
    WI_drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY, cntKills[0]);

    WI_DrawPatch(items, SP_STATSX, SP_STATSY + lh);
    WI_drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY + lh, cntItems[0]);

    WI_DrawPatch(sp_secret, SP_STATSX, SP_STATSY + 2 * lh);
    WI_drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY + 2 * lh, cntSecret[0]);

    WI_DrawPatch(time, SP_TIMEX, SP_TIMEY);
    if(cntTime >= 0)
        WI_drawTime(SCREENWIDTH / 2 - SP_TIMEX, SP_TIMEY, cntTime / TICRATE);

    if(wbs->parTime != -1)
    {
        WI_DrawPatch(par, SCREENWIDTH / 2 + SP_TIMEX, SP_TIMEY);
        if(cntPar >= 0)
            WI_drawTime(SCREENWIDTH - SP_TIMEX, SP_TIMEY, cntPar / TICRATE);
    }
}

void WI_checkForAccelerate(void)
{
    int                 i;
    player_t           *player;

    // Check for button presses to skip delays.
    for(i = 0, player = players; i < MAXPLAYERS; ++i, player++)
    {
        if(players[i].plr->inGame)
        {
            if(player->brain.attack)
            {
                if(!player->attackDown)
                    accelerateStage = 1;
                player->attackDown = true;
            }
            else
                player->attackDown = false;

            if(player->brain.use)
            {
                if(!player->useDown)
                    accelerateStage = 1;
                player->useDown = true;
            }
            else
                player->useDown = false;
        }
    }
}

/**
 * Updates stuff each tick.
 */
void WI_Ticker(void)
{
    // Counter for general background animation.
    bcnt++;

    WI_checkForAccelerate();

    switch(state)
    {
    case ILS_SHOW_STATS:
        if(deathmatch)
            WI_updateDeathmatchStats();
        else if(IS_NETGAME)
            WI_updateNetgameStats();
        else
            WI_updateStats();
        break;

    case ILS_NONE:
    default:
        WI_updateNoState();
        break;
    }
}

void WI_loadData(void)
{
    char name[9];
    int i;

    bg = R_PrecachePatch("INTERPIC", NULL); // Background.
    finished = R_PrecachePatch("WIF", NULL); // "finished"
    entering = R_PrecachePatch("WIENTER", NULL); // "entering"
    kills = R_PrecachePatch("WIOSTK", NULL); // "kills"
    secret = R_PrecachePatch("WIOSTS", NULL); // "scrt"
    sp_secret = R_PrecachePatch("WISCRT2", NULL); // "secret"
    items = R_PrecachePatch("WIOSTI", NULL); //items
    frags = R_PrecachePatch("WIFRGS", NULL); // "frgs"
    time = R_PrecachePatch("WITIME", NULL); // "time"
    sucks = R_PrecachePatch("WISUCKS", NULL); // "sucks"
    par = R_PrecachePatch("WIPAR", NULL); // "par"
    killers = R_PrecachePatch("WIKILRS", NULL); // "killers" (vertical)
    victims = R_PrecachePatch("WIVCTMS", NULL); // "victims" (horiz)
    total = R_PrecachePatch("WIMSTT", NULL); // "total"

    for(i = 0; i < NUM_TEAMS; ++i)
    {
        sprintf(name, "STPB%d", i);
        p[i] = R_PrecachePatch(name, NULL);

        sprintf(name, "WIBP%d", i + 1);
        bp[i] = R_PrecachePatch(name, NULL);
    }
}

void WI_Drawer(void)
{
    switch(state)
    {
    case ILS_SHOW_STATS:
        if(deathmatch)
            WI_drawDeathmatchStats();
        else if(IS_NETGAME)
            WI_drawNetgameStats();
        else
            WI_drawStats();
        break;

    case ILS_NONE:
    default:
        WI_drawNoState();
        break;
    }
}

void WI_initVariables(wbstartstruct_t *wbstartstruct)
{
    wbs = wbstartstruct;

#ifdef RANGECHECK
    if(gameMode != commercial)
    {
        if(gameMode == retail)
            RNGCHECK(wbs->episode, 0, 3);
        else
            RNGCHECK(wbs->episode, 0, 2);
    }
    else
    {
        RNGCHECK(wbs->currentMap, 0, 8);
        RNGCHECK(wbs->nextMap, 0, 8);
    }
    RNGCHECK(wbs->pnum, 0, MAXPLAYERS);
    RNGCHECK(wbs->pnum, 0, MAXPLAYERS);
#endif

    accelerateStage = 0;
    cnt = bcnt = 0;
    me = wbs->pNum;
    myTeam = cfg.playerColor[wbs->pNum];
    plrs = wbs->plyr;

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

    WI_initVariables(wbstartstruct);
    WI_loadData();

    // Calculate team stats.
    memset(teamInfo, 0, sizeof(teamInfo));
    for(i = 0, tin = teamInfo; i < NUM_TEAMS; ++i, tin++)
    {
        for(j = 0; j < MAXPLAYERS; j++)
        {
            // Is the player in this team?
            if(!plrs[j].inGame || cfg.playerColor[j] != i)
                continue;

            tin->members++;

            // Check the frags.
            for(k = 0; k < MAXPLAYERS; ++k)
                tin->frags[cfg.playerColor[k]] += plrs[j].frags[k];

            // Counters.
            if(plrs[j].items > tin->items)
                tin->items = plrs[j].items;

            if(plrs[j].kills > tin->kills)
                tin->kills = plrs[j].kills;

            if(plrs[j].secret > tin->secret)
                tin->secret = plrs[j].secret;
        }

        // Calculate team's total frags.
        for(j = 0; j < NUM_TEAMS; ++j)
        {
            if(j == i) // Suicides are negative frags.
                tin->totalFrags -= tin->frags[j];
            else
                tin->totalFrags += tin->frags[j];
        }
    }

    if(deathmatch)
        WI_initDeathmatchStats();
    else if(IS_NETGAME)
        WI_initNetgameStats();
    else
        WI_initStats();
}

void WI_SetState(interludestate_t st)
{
    if(st == ILS_SHOW_STATS)
        WI_initStats();
    if(st == ILS_NONE)
        WI_initNoState();
}
