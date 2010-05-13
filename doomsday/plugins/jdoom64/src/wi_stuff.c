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
#define NG_STATSX           (32 + star.width /2 + 32*!doFrags)
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

static patchinfo_t bg; // Background (map of levels).
static patchinfo_t percent;
static patchinfo_t colon;
static patchinfo_t num[10]; // 0-9 graphic.
static patchinfo_t minus; // Minus sign.
static patchinfo_t finished; // "Finished!"
static patchinfo_t entering; // "Entering"
static patchinfo_t sp_secret; // "secret"
static patchinfo_t kills; // "Kills"
static patchinfo_t secret; // "Scrt"
static patchinfo_t items; // "Items"
static patchinfo_t frags; // "Frags"
static patchinfo_t time; // "Time"
static patchinfo_t par; // "Par"
static patchinfo_t sucks; // "Sucks!"
static patchinfo_t killers; // "killers"
static patchinfo_t victims; // "victims"
static patchinfo_t total; // "Total"
static patchinfo_t star; // Player icon (alive).
static patchinfo_t bstar; // Player icon (dead).
static patchinfo_t p[MAXPLAYERS]; // "red P[1..MAXPLAYERS]"
static patchinfo_t bp[MAXPLAYERS]; // "gray P[1..MAXPLAYERS]"

// CODE --------------------------------------------------------------------

void WI_slamBackground(void)
{
    GL_DrawPatch(bg.id, 0, 0);
}

/**
 * Draws "<Levelname> Finished!"
 */
void WI_drawLF(void)
{
    int                 y = WI_TITLEY;
    int                 mapnum;
    char               *lname, *ptr;

    lname = (char *) DD_GetVariable(DD_MAP_NAME);

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
    WI_DrawPatch(SCREENWIDTH / 2, y, 1, 1, 1, 1,
                 &mapNamePatches[mapnum], lname, false, ALIGN_CENTER);

    // Draw "Finished!"
    y += (5 * mapNamePatches[mapnum].height) / 4;

    WI_DrawPatch(SCREENWIDTH / 2, y, 1, 1, 1, 1,
                 &finished, NULL, false, ALIGN_CENTER);
}

/**
 * Draws "Entering <LevelName>"
 */
void WI_drawEL(void)
{
    int                 y = WI_TITLEY;
    char               *lname = "", *ptr;
    ddmapinfo_t         minfo;
    char                levid[10];

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
    WI_DrawPatch(SCREENWIDTH / 2, y, 1, 1, 1, 1, &entering,
                 NULL, false, ALIGN_CENTER);

    // Draw level.
    y += (5 * mapNamePatches[wbs->nextMap].height) / 4;

    WI_DrawPatch(SCREENWIDTH / 2, y, 1, 1, 1, 1,
                 &mapNamePatches[(wbs->episode * 8) + wbs->nextMap],
                 lname, false, ALIGN_CENTER);
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
    int                 fontwidth = num[0].width;
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
        WI_DrawPatch(x, y, 1, 1, 1, 1, &num[n % 10], NULL, false, ALIGN_LEFT);
        n /= 10;
    }

    // Draw a minus sign if necessary.
    if(neg)
        WI_DrawPatch(x -= 8, y, 1, 1, 1, 1, &minus, NULL, false, ALIGN_LEFT);

    return x;
}

void WI_drawPercent(int x, int y, int p)
{
    if(p < 0)
        return;

    WI_DrawPatch(x, y, 1, 1, 1, 1, &percent, NULL, false, ALIGN_LEFT);
    WI_drawNum(x, y, p, -1);
}

/**
 * Display level completion time and par, or "sucks" message if overflow.
 */
void WI_drawTime(int x, int y, int t)
{
    int                 div, n;

    if(t < 0)
        return;

    if(t <= 61 * 59)
    {
        div = 1;
        do
        {
            n = (t / div) % 60;
            x = WI_drawNum(x, y, n, 2) - colon.width;
            div *= 60;

            if(div == 60 || t / div)
                WI_DrawPatch(x, y, 1, 1, 1, 1, &colon, NULL, false,
                             ALIGN_LEFT);

        } while(t / div);
    }
    else
    {
        // "sucks"
        WI_DrawPatch(x - sucks.width, y, 1, 1, 1, 1, &sucks,
                     NULL, false, ALIGN_LEFT);
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
    int                 i, j;
    int                 x, y;
    int                 w;
    int                 lh; // Line height.

    lh = WI_SPACINGY;

    WI_slamBackground();

    // Draw animated background.
    WI_drawAnimatedBack();
    WI_drawLF();

    // Draw stat titles (top line).
    WI_DrawPatch(DM_TOTALSX - total.width / 2,
                 DM_MATRIXY - WI_SPACINGY + 10, 1, 1, 1, 1, &total, NULL,
                 false, ALIGN_LEFT);

    WI_DrawPatch(DM_KILLERSX, DM_KILLERSY, 1, 1, 1, 1, &killers, NULL,
                 false, ALIGN_LEFT);
    WI_DrawPatch(DM_VICTIMSX, DM_VICTIMSY, 1, 1, 1, 1, &victims, NULL,
                 false, ALIGN_LEFT);

    // Draw P?
    x = DM_MATRIXX + DM_SPACINGX;
    y = DM_MATRIXY;

    for(i = 0; i < NUM_TEAMS; ++i)
    {
        if(teamInfo[i].members)
        {
            WI_DrawPatch(x - p[i].width / 2, DM_MATRIXY - WI_SPACINGY,
                         1, 1, 1, 1, &p[i], NULL, false, ALIGN_LEFT);

            WI_DrawPatch(DM_MATRIXX - p[i].width / 2, y, 1, 1, 1, 1,
                         &p[i], NULL, false, ALIGN_LEFT);

            if(i == myTeam)
            {
                WI_DrawPatch(x - p[i].width / 2,
                             DM_MATRIXY - WI_SPACINGY, 1, 1, 1, 1, &bstar,
                             NULL, false, ALIGN_LEFT);

                WI_DrawPatch(DM_MATRIXX - p[i].width / 2, y, 1, 1, 1, 1,
                             &star, NULL, false, ALIGN_LEFT);
            }

            // If more than 1 member, show the member count.
            if(teamInfo[i].members > 1)
            {
                char                tmp[20];

                sprintf(tmp, "%i", teamInfo[i].members);
                M_WriteText2(x - p[i].width / 2 + 1,
                             DM_MATRIXY - WI_SPACINGY + p[i].height - 8, tmp,
                             GF_FONTA, 1, 1, 1, 1);
                M_WriteText2(DM_MATRIXX - p[i].width / 2 + 1,
                             y + p[i].height - 8, tmp, GF_FONTA, 1, 1, 1, 1);
            }
        }
        else
        {
            WI_DrawPatch(x - bp[i].width / 2, DM_MATRIXY - WI_SPACINGY, 1, 1, 1, 1,
                         &bp[i], NULL, false, ALIGN_LEFT);

            WI_DrawPatch(DM_MATRIXX - bp[i].width / 2, y, 1, 1, 1, 1,
                         &bp[i], NULL, false, ALIGN_LEFT);
        }

        x += DM_SPACINGX;
        y += WI_SPACINGY;
    }

    // Draw stats.
    y = DM_MATRIXY + 10;
    w = num[0].width;

    for(i = 0; i < NUM_TEAMS; ++i)
    {
        x = DM_MATRIXX + DM_SPACINGX;
        if(teamInfo[i].members)
        {
            for(j = 0; j < NUM_TEAMS; ++j)
            {
                if(teamInfo[j].members)
                    WI_drawNum(x + w, y, dmFrags[i][j], 2);

                x += DM_SPACINGX;
            }

            WI_drawNum(DM_TOTALSX + w, y, dmTotals[i], 2);
        }

        y += WI_SPACINGY;
    }
}

void WI_initNetgameStats(void)
{
    int                 i;

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
    int                 i;
    int                 x, y;
    int                 pwidth = percent.width;

    WI_slamBackground();

    // Draw animated background.
    WI_drawAnimatedBack();

    WI_drawLF();

    // Draw stat titles (top line).
    WI_DrawPatch(NG_STATSX + NG_SPACINGX - kills.width, NG_STATSY,
                 1, 1, 1, 1, &kills, NULL, false, ALIGN_LEFT);

    WI_DrawPatch(NG_STATSX + 2 * NG_SPACINGX - items.width, NG_STATSY,
                 1, 1, 1, 1, &items, NULL, false, ALIGN_LEFT);

    WI_DrawPatch(NG_STATSX + 3 * NG_SPACINGX - secret.width, NG_STATSY,
                 1, 1, 1, 1, &secret, NULL, false, ALIGN_LEFT);

    if(doFrags)
        WI_DrawPatch(NG_STATSX + 4 * NG_SPACINGX - frags.width,
                     1, 1, 1, 1, NG_STATSY, &frags, NULL, false, ALIGN_LEFT);

    // Draw stats.
    y = NG_STATSY + kills.height;

    for(i = 0; i < NUM_TEAMS; ++i)
    {
        if(!teamInfo[i].members)
            continue;

        x = NG_STATSX;
        WI_DrawPatch(x - p[i].width, y, 1, 1, 1, 1, &p[i], NULL,
                     false, ALIGN_LEFT);
        // If more than 1 member, show the member count.
        if(teamInfo[i].members > 1)
        {
            char                tmp[40];

            sprintf(tmp, "%i", teamInfo[i].members);
            M_WriteText2(x - p[i].width + 1, y + p[i].height - 8, tmp,
                         GF_FONTA, 1, 1, 1, 1);
        }

        if(i == myTeam)
            WI_DrawPatch(x - p[i].width, y, 1, 1, 1, 1, &star, NULL,
                         false, ALIGN_LEFT);

        x += NG_SPACINGX;
        WI_drawPercent(x - pwidth, y + 10, cntKills[i]);
        x += NG_SPACINGX;
        WI_drawPercent(x - pwidth, y + 10, cntItems[i]);
        x += NG_SPACINGX;
        WI_drawPercent(x - pwidth, y + 10, cntSecret[i]);
        x += NG_SPACINGX;

        if(doFrags)
            WI_drawNum(x, y + 10, cntFrags[i], -1);

        y += WI_SPACINGY;
    }
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
    int                 lh; // Line height.

    lh = (3 * num[0].height) / 2;

    WI_slamBackground();

    // Draw animated background.
    WI_drawAnimatedBack();

    WI_drawLF();

    WI_DrawPatch(SP_STATSX, SP_STATSY, 1, 1, 1, 1, &kills, NULL,
                 false, ALIGN_LEFT);
    WI_drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY, cntKills[0]);

    WI_DrawPatch(SP_STATSX, SP_STATSY + lh, 1, 1, 1, 1, &items, NULL,
                 false, ALIGN_LEFT);
    WI_drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY + lh, cntItems[0]);

    WI_DrawPatch(SP_STATSX, SP_STATSY + 2 * lh, 1, 1, 1, 1, &sp_secret,
                 NULL, false, ALIGN_LEFT);
    WI_drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY + 2 * lh, cntSecret[0]);

    WI_DrawPatch(SP_TIMEX, SP_TIMEY, 1, 1, 1, 1, &time, NULL, false,
                 ALIGN_LEFT);
    if(cntTime >= 0)
        WI_drawTime(SCREENWIDTH / 2 - SP_TIMEX, SP_TIMEY, cntTime / TICRATE);

    if(wbs->parTime != -1)
    {
        WI_DrawPatch(SCREENWIDTH / 2 + SP_TIMEX, SP_TIMEY, 1, 1, 1, 1, &par,
                     NULL, false, ALIGN_LEFT);
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

    // Background.
    R_PrecachePatch("INTERPIC", &bg);
    // Minus sign.
    R_PrecachePatch("WIMINUS", &minus);

    // Numbers 0-9
    for(i = 0; i < 10; ++i)
    {
        sprintf(name, "WINUM%d", i);
        R_PrecachePatch(name, &num[i]);
    }
    // Percent sign.
    R_PrecachePatch("WIPCNT", &percent);
    // "finished"
    R_PrecachePatch("WIF", &finished);
    // "entering"
    R_PrecachePatch("WIENTER", &entering);
    // "kills"
    R_PrecachePatch("WIOSTK", &kills);
    // "scrt"
    R_PrecachePatch("WIOSTS", &secret);
    // "secret"
    R_PrecachePatch("WISCRT2", &sp_secret);
    //items
    R_PrecachePatch("WIOSTI", &items);
    // "frgs"
    R_PrecachePatch("WIFRGS", &frags);
    // ":"
    R_PrecachePatch("WICOLON", &colon);
    // "time"
    R_PrecachePatch("WITIME", &time);
    // "sucks"
    R_PrecachePatch("WISUCKS", &sucks);
    // "par"
    R_PrecachePatch("WIPAR", &par);
    // "killers" (vertical)
    R_PrecachePatch("WIKILRS", &killers);
    // "victims" (horiz)
    R_PrecachePatch("WIVCTMS", &victims);
    // "total"
    R_PrecachePatch("WIMSTT", &total);

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        sprintf(name, "STPB%d", i);
        R_PrecachePatch(name, &p[i]);

        sprintf(name, "WIBP%d", i + 1);
        R_PrecachePatch(name, &bp[i]);
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
