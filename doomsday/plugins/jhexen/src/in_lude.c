/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
 * in_lude.c:
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>
#include <assert.h>

#include "jhexen.h"

#include "d_net.h"
#include "hu_stuff.h"
#include "hu_menu.h"
#include "g_common.h"
#include "am_map.h"

// MACROS ------------------------------------------------------------------

#define TEXTSPEED               (3)
#define TEXTWAIT                (140)

#define TALLY_EFFECT_TICKS      (20)
#define TALLY_FINAL_X_DELTA     (23 * FRACUNIT)
#define TALLY_FINAL_Y_DELTA     (13 * FRACUNIT)
#define TALLY_START_XPOS        (178 * FRACUNIT)
#define TALLY_STOP_XPOS         (90 * FRACUNIT)
#define TALLY_START_YPOS        (132 * FRACUNIT)
#define TALLY_STOP_YPOS         (83 * FRACUNIT)
#define TALLY_TOP_X             (85)
#define TALLY_TOP_Y             (9)
#define TALLY_LEFT_X            (7)
#define TALLY_LEFT_Y            (71)
#define TALLY_TOTALS_X          (291)

#define MAX_INTRMSN_MESSAGE_SIZE (1024)

// TYPES -------------------------------------------------------------------

typedef enum gametype_e {
    SINGLE,
    COOPERATIVE,
    DEATHMATCH
} gametype_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void IN_WaitStop(void);
static void loadPics(void);
static void unloadPics(void);
static void CheckForSkip(void);
static void initStats(void);
static void drawDeathTally(void);
static void drawNumber(int val, int x, int y, int wrapThresh);
static void drawNumberBold(int val, int x, int y, int wrapThresh);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DECLARATIONS ------------------------------------------------

boolean intermission;
int interState = 0;
int overrideHubMsg = 0; // Override the hub transition message when 1.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// Used for timing of background animation.
static int bcnt;

static boolean skipIntermission;
static int interTime = -1;
static gametype_t gameType;
static int cnt;
static int slaughterBoy; // In DM, the player with the most kills.
static signed int totalFrags[MAXPLAYERS];

static int hubCount;

static patchinfo_t dpTallyTop;
static patchinfo_t dpTallyLeft;

// CODE --------------------------------------------------------------------

void WI_initVariables(void /* wbstartstruct_t* wbstartstruct */)
{
/*    wbs = wbstartstruct;

#ifdef RANGECHECK
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
    cnt =*/ bcnt = 0; /*
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

    intermission = true;
    interState = 0;
    skipIntermission = false;
    interTime = 0;
}

void IN_Init(void)
{
    assert(deathmatch);

    WI_initVariables();
    loadPics();
    initStats();
}

void IN_WaitStop(void)
{
    if(!--cnt)
    {
        IN_Stop();
        G_WorldDone();
    }
}

void IN_Stop(void)
{
    NetSv_Intermission(IMF_END, 0, 0);
    unloadPics();
    intermission = false;
}

/**
 * Initializes the stats for single player mode.
 */
static void initStats(void)
{
    int i, j, slaughterFrags, posNum, slaughterCount, playerCount;

    gameType = DEATHMATCH;
    slaughterBoy = 0;
    slaughterFrags = -9999;
    posNum = 0;
    playerCount = 0;
    slaughterCount = 0;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        totalFrags[i] = 0;
        if(players[i].plr->inGame)
        {
            playerCount++;
            for(j = 0; j < MAXPLAYERS; ++j)
            {
                if(players[i].plr->inGame)
                {
                    totalFrags[i] += players[i].frags[j];
                }
            }
            posNum++;
        }

        if(totalFrags[i] > slaughterFrags)
        {
            slaughterBoy = 1 << i;
            slaughterFrags = totalFrags[i];
            slaughterCount = 1;
        }
        else if(totalFrags[i] == slaughterFrags)
        {
            slaughterBoy |= 1 << i;
            slaughterCount++;
        }
    }

    if(playerCount == slaughterCount)
    {   // don't do the slaughter stuff if everyone is equal
        slaughterBoy = 0;
    }
}

static void loadPics(void)
{
    if(gameType != SINGLE)
    {
        R_PrecachePatch("TALLYTOP", &dpTallyTop);
        R_PrecachePatch("TALLYLFT", &dpTallyLeft);
    }
}

static void unloadPics(void)
{
    // Nothing to do.
}

void IN_Ticker(void)
{
    if(!intermission)
    {
        return;
    }

    if(interState)
    {
        IN_WaitStop();
        return;
    }
    skipIntermission = false;
    CheckForSkip();

    // Counter for general background animation.
    bcnt++;

    interTime++;
    if(skipIntermission || (gameType == SINGLE && !hubCount))
    {
        interState = 1;
        NetSv_Intermission(IMF_STATE, interState, 0);
        cnt = 10;
        skipIntermission = false;
    }
}

/**
 * Check to see if any player hit a key.
 */
static void CheckForSkip(void)
{
    static boolean      triedToSkip;

    int                 i;
    player_t           *player;

    for(i = 0, player = players; i < MAXPLAYERS; ++i, player++)
    {
        if(players[i].plr->inGame)
        {
            if(player->brain.attack)
            {
                if(!player->attackDown)
                {
                    skipIntermission = 1;
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
                    skipIntermission = 1;
                }
                player->useDown = true;
            }
            else
            {
                player->useDown = false;
            }
        }
    }

    if(deathmatch && interTime < 140)
    {   // Wait for 4 seconds before allowing a skip.
        if(skipIntermission == 1)
        {
            triedToSkip = true;
            skipIntermission = 0;
        }
    }
    else
    {
        if(triedToSkip)
        {
            skipIntermission = 1;
            triedToSkip = false;
        }
    }
}

void IN_Drawer(void)
{
    lumpnum_t lump;

    if(!intermission || interState)
        return;

    if((lump = W_GetNumForName("INTERPIC")) != -1)
    {
        DGL_Color4f(1, 1, 1, 1);
        DGL_DrawRawScreen(lump, 0, 0);
    }

    if(gameType != SINGLE)
    {
        drawDeathTally();
    }
}

static void drawDeathTally(void)
{
    static boolean showTotals;

    fixed_t xPos, yPos, xDelta, yDelta, xStart, scale;
    int i, j, x, y, temp;
    boolean bold;

    DGL_Color4f(1, 1, 1, 1);
    DGL_DrawPatch(dpTallyTop.id, TALLY_TOP_X, TALLY_TOP_Y);
    DGL_DrawPatch(dpTallyLeft.id, TALLY_LEFT_X, TALLY_LEFT_Y);

    if(interTime < TALLY_EFFECT_TICKS)
    {
        showTotals = false;
        scale = (interTime * FRACUNIT) / TALLY_EFFECT_TICKS;
        xDelta = FixedMul(scale, TALLY_FINAL_X_DELTA);
        yDelta = FixedMul(scale, TALLY_FINAL_Y_DELTA);
        xStart = TALLY_START_XPOS - FixedMul(scale, TALLY_START_XPOS - TALLY_STOP_XPOS);
        yPos   = TALLY_START_YPOS - FixedMul(scale, TALLY_START_YPOS - TALLY_STOP_YPOS);
    }
    else
    {
        xDelta = TALLY_FINAL_X_DELTA;
        yDelta = TALLY_FINAL_Y_DELTA;
        xStart = TALLY_STOP_XPOS;
        yPos   = TALLY_STOP_YPOS;
    }

    if(interTime >= TALLY_EFFECT_TICKS && showTotals == false)
    {
        showTotals = true;
        S_StartSound(SFX_PLATFORM_STOP, NULL);
    }
    y = yPos >> FRACBITS;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        xPos = xStart;
        for(j = 0; j < MAXPLAYERS; ++j, xPos += xDelta)
        {
            x = xPos >> FRACBITS;
            bold = (i == CONSOLEPLAYER || j == CONSOLEPLAYER);
            if(players[i].plr->inGame && players[j].plr->inGame)
            {
                if(bold)
                {
                    drawNumberBold(players[i].frags[j], x, y, 100);
                }
                else
                {
                    drawNumber(players[i].frags[j], x, y, 100);
                }
            }
            else
            {
                temp = M_StringWidth("--", GF_FONTA) / 2;
                if(bold)
                {
                    M_WriteText2(x - temp, y, "--", GF_FONTA, 1, 0.7f, 0.3f, 1);
                }
                else
                {
                    M_WriteText2(x - temp, y, "--", GF_FONTA, 1, 1, 1, 1);
                }
            }
        }

        if(showTotals && players[i].plr->inGame &&
           !((slaughterBoy & (1 << i)) && !(interTime & 16)))
        {
            drawNumber(totalFrags[i], TALLY_TOTALS_X, y, 1000);
        }

        yPos += yDelta;
        y = yPos >> FRACBITS;
    }
}

static void drawNumber(int val, int x, int y, int wrapThresh)
{
    char buff[8] = "XX";

    if(!(val < -9 && wrapThresh < 1000))
    {
        sprintf(buff, "%d", val >= wrapThresh ? val % wrapThresh : val);
    }

    M_WriteText2(x - M_StringWidth(buff, GF_FONTA) / 2, y, buff,
                 GF_FONTA, 1, 1, 1, 1);
}

static void drawNumberBold(int val, int x, int y, int wrapThresh)
{
    char                buff[8] = "XX";

    if(!(val < -9 && wrapThresh < 1000))
    {
        sprintf(buff, "%d", val >= wrapThresh ? val % wrapThresh : val);
    }

    M_WriteText2(x - M_StringWidth(buff, GF_FONTA) / 2, y, buff,
                 GF_FONTA, 1, 0.7f, 0.3f, 1);
}
