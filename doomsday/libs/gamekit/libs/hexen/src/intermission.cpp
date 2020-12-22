/** @file intermission.cpp  Hexen specific intermission screens.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "jhexen.h"
#include "intermission.h"

#include <cstdio>
#include "d_net.h"
#include "d_netcl.h"
#include "d_netsv.h"
#include "hu_stuff.h"
#include "hu_menu.h"
#include "g_common.h"

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

using namespace de;

enum gametype_t
{
    SINGLE,
    COOPERATIVE,
    DEATHMATCH
};

static void IN_WaitStop();
static void loadPics();
static void unloadPics();
static void CheckForSkip();
static void initStats();
static void drawDeathTally();
static void drawNumber(int val, int x, int y, int wrapThresh);
static void drawNumberBold(int val, int x, int y, int wrapThresh);

dd_bool intermission;
int interState;
int overrideHubMsg; // Override the hub transition message when 1.

// Used for timing of background animation.
static int bcnt;

static dd_bool skipIntermission;
static int interTime = -1;
static gametype_t gameType;
static int cnt;
static int slaughterBoy; // In DM, the player with the most kills.
static int totalFrags[MAXPLAYERS];

static int hubCount;

static patchid_t dpTallyTop;
static patchid_t dpTallyLeft;

void WI_initVariables(/*wbstartstruct_t *wbstartstruct */)
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

    intermission     = true;
    interState       = 0;
    skipIntermission = false;
    interTime        = 0;
}

void IN_Begin(const wbstartstruct_t & /*wbstartstruct*/)
{
    DE_ASSERT(gfw_Rule(deathmatch));

    WI_initVariables();
    loadPics();
    initStats();
}

void IN_WaitStop()
{
    if(!--cnt)
    {
        IN_End();
        G_IntermissionDone();
    }
}

void IN_End()
{
    NetSv_Intermission(IMF_END, 0, 0);
    unloadPics();
    intermission = false;
}

/**
 * Initializes the stats for single player mode.
 */
static void initStats()
{
    gameType = DEATHMATCH;
    slaughterBoy = 0;

    int slaughterFrags = -9999;
    int posNum         = 0;
    int playerCount    = 0;
    int slaughterCount = 0;

    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        totalFrags[i] = 0;
        if(players[i].plr->inGame)
        {
            playerCount++;
            for(int k = 0; k < MAXPLAYERS; ++k)
            {
                if(players[i].plr->inGame)
                {
                    totalFrags[i] += players[i].frags[k];
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
    {
        // Don't do the slaughter stuff if everyone is equal.
        slaughterBoy = 0;
    }
}

static void loadPics()
{
    if(gameType != SINGLE)
    {
        dpTallyTop  = R_DeclarePatch("TALLYTOP");
        dpTallyLeft = R_DeclarePatch("TALLYLFT");
    }
}

static void unloadPics()
{
    // Nothing to do.
}

void IN_Ticker()
{
    if(!intermission) return;

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

static void CheckForSkip()
{
    static bool triedToSkip;

    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        player_t *player = &players[i];

        if(player->plr->inGame)
        {
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

    if(gfw_Rule(deathmatch) && interTime < 140)
    {
        // Wait for 4 seconds before allowing a skip.
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

void IN_Drawer()
{
    if(!intermission || interState)
        return;

    dgl_borderedprojectionstate_t bp;
    GL_ConfigureBorderedProjection(&bp, BPF_OVERDRAW_MASK|BPF_OVERDRAW_CLIP, SCREENWIDTH, SCREENHEIGHT,
                                   Get(DD_WINDOW_WIDTH), Get(DD_WINDOW_HEIGHT), scalemode_t(cfg.common.inludeScaleMode));
    GL_BeginBorderedProjection(&bp);

    lumpnum_t lumpNum = CentralLumpIndex().findLast("INTERPIC.lmp");
    if(lumpNum >= 0)
    {
        DGL_Color4f(1, 1, 1, 1);
        DGL_SetRawImage(lumpNum, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_Enable(DGL_TEXTURE_2D);
        DGL_DrawRectf2(0, 0, SCREENWIDTH, SCREENHEIGHT);
        DGL_Disable(DGL_TEXTURE_2D);
    }

    if(gameType != SINGLE)
    {
        drawDeathTally();
    }

    GL_EndBorderedProjection(&bp);
}

static void drawDeathTally()
{
    static dd_bool showTotals;

    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(1, 1, 1, 1);
    GL_DrawPatch(dpTallyTop,  Vec2i(TALLY_TOP_X, TALLY_TOP_Y));
    GL_DrawPatch(dpTallyLeft, Vec2i(TALLY_LEFT_X, TALLY_LEFT_Y));

    fixed_t xPos, yPos, xDelta, yDelta, xStart, scale;
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
    int y = yPos >> FRACBITS;

    FR_SetFont(FID(GF_FONTA));
    FR_LoadDefaultAttrib();

    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        xPos = xStart;
        for(int j = 0; j < MAXPLAYERS; ++j, xPos += xDelta)
        {
            int x = xPos >> FRACBITS;
            dd_bool bold = (i == CONSOLEPLAYER || j == CONSOLEPLAYER);
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
                if(bold)
                {
                    FR_SetColorAndAlpha(1, 0.7f, 0.3f, 1);
                    FR_DrawTextXY3("--", x, y, ALIGN_TOP, DTF_NO_EFFECTS);
                }
                else
                {
                    FR_SetColorAndAlpha(1, 1, 1, 1);
                    FR_DrawTextXY("--", x, y);
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

    DGL_Disable(DGL_TEXTURE_2D);
}

static void drawNumber(int val, int x, int y, int wrapThresh)
{
    char buf[8] = "XX";

    if(!(val < -9 && wrapThresh < 1000))
    {
        sprintf(buf, "%d", val >= wrapThresh ? val % wrapThresh : val);
    }

    FR_SetColorAndAlpha(1, 1, 1, 1);
    FR_DrawTextXY3(buf, x, y, ALIGN_TOP, DTF_NO_EFFECTS);
}

static void drawNumberBold(int val, int x, int y, int wrapThresh)
{
    char buf[8] = "XX";

    if(!(val < -9 && wrapThresh < 1000))
    {
        sprintf(buf, "%d", val >= wrapThresh ? val % wrapThresh : val);
    }

    FR_SetColorAndAlpha(1, 0.7f, 0.3f, 1);
    FR_DrawTextXY3(buf, x, y, ALIGN_TOP, DTF_NO_EFFECTS);
}

void IN_SetState(int stateNum)
{
    interState = stateNum;
}

void IN_SkipToNext()
{
    skipIntermission = 1;
}

void IN_ConsoleRegister()
{
    C_VAR_BYTE("inlude-stretch",           &cfg.common.inludeScaleMode, 0, SCALEMODE_FIRST, SCALEMODE_LAST);
    C_VAR_INT ("inlude-patch-replacement", &cfg.common.inludePatchReplaceMode, 0, 0, 1);
}
