/**\file
 *\section License
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
 *
 * This program is covered by the HERETIC / HEXEN (LIMITED USE) source
 * code license; you can redistribute it and/or modify it under the terms
 * of the HERETIC / HEXEN source code license as published by Activision.
 *
 * THIS MATERIAL IS NOT MADE OR SUPPORTED BY ACTIVISION.
 *
 * WARRANTY INFORMATION.
 * This program is provided as is. Activision and it's affiliates make no
 * warranties of any kind, whether oral or written , express or implied,
 * including any warranty of merchantability, fitness for a particular
 * purpose or non-infringement, and no other representations or claims of
 * any kind shall be binding on or obligate Activision or it's affiliates.
 *
 * LICENSE CONDITIONS.
 * You shall not:
 *
 * 1) Exploit this Program or any of its parts commercially.
 * 2) Use this Program, or permit use of this Program, on more than one
 *    computer, computer terminal, or workstation at the same time.
 * 3) Make copies of this Program or any part thereof, or make copies of
 *    the materials accompanying this Program.
 * 4) Use the program, or permit use of this Program, in a network,
 *    multi-user arrangement or remote access arrangement, including any
 *    online use, except as otherwise explicitly provided by this Program.
 * 5) Sell, rent, lease or license any copies of this Program, without
 *    the express prior written consent of Activision.
 * 6) Remove, disable or circumvent any proprietary notices or labels
 *    contained on or within the Program.
 *
 * You should have received a copy of the HERETIC / HEXEN source code
 * license along with this program (Ravenlic.txt); if not:
 * http://www.ravensoft.com/
 */

#include <ctype.h>

#include "jhexen.h"

#include "d_net.h"
#include "hu_stuff.h"

// MACROS ------------------------------------------------------------------

#define TEXTSPEED 3
#define TEXTWAIT 140

#define TALLY_EFFECT_TICKS 20
#define TALLY_FINAL_X_DELTA (23*FRACUNIT)
#define TALLY_FINAL_Y_DELTA (13*FRACUNIT)
#define TALLY_START_XPOS (178*FRACUNIT)
#define TALLY_STOP_XPOS (90*FRACUNIT)
#define TALLY_START_YPOS (132*FRACUNIT)
#define TALLY_STOP_YPOS (83*FRACUNIT)
#define TALLY_TOP_X 85
#define TALLY_TOP_Y 9
#define TALLY_LEFT_X 7
#define TALLY_LEFT_Y 71
#define TALLY_TOTALS_X 291

// TYPES -------------------------------------------------------------------

typedef enum gametype_e {
    SINGLE,
    COOPERATIVE,
    DEATHMATCH
} gametype_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern void AM_Stop(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void    IN_Start(void);
void    IN_Ticker(void);
void    IN_Drawer(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void WaitStop(void);
static void LoadPics(void);
static void UnloadPics(void);
static void CheckForSkip(void);
static void InitStats(void);
static void DrDeathTally(void);
static void DrNumber(int val, int x, int y, int wrapThresh);
static void DrNumberBold(int val, int x, int y, int wrapThresh);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

int     overrideHubMsg = 0;     // override the hub transition message when 1

// PUBLIC DATA DECLARATIONS ------------------------------------------------

boolean intermission;
char    ClusterMessage[MAX_INTRMSN_MESSAGE_SIZE];
int     interstate = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean skipintermission;
static int intertime = -1;
static gametype_t gametype;
static int cnt;
static int slaughterboy;        // in DM, the player with the most kills
static int patchINTERPICLumpRS; // A raw screen.
static int FontBNumbersLump[10];
static int FontBNegativeLump;
static int FontBSlashLump;
static int FontBPercentLump;

static int FontABaseLump;
static int FontBLump;
static int FontBLumpBase;

static signed int totalFrags[MAXPLAYERS];

static int HubCount;

#if 0
static char *ClusMsgLumpNames[] = {
    "clus1msg",
    "clus2msg",
    "clus3msg",
    "clus4msg",
    "clus5msg"
};
#endif

// CODE --------------------------------------------------------------------

void IN_Start(void)
{
    AM_Stop();
    SN_StopAllSequences();

    // InFine handles the text.
    if(!deathmatch)
    {
        gameaction = GA_LEAVEMAP;
        return;
    }

    GL_SetFilter(0);
    InitStats();
    LoadPics();
    intermission = true;
    interstate = 0;
    skipintermission = false;
    intertime = 0;
}

void WaitStop(void)
{
    if(!--cnt)
    {
        IN_Stop();
        //      gamestate = GS_LEVEL;
        //      G_DoLoadLevel();
        gameaction = GA_LEAVEMAP;
        //      G_WorldDone();
    }
}

void IN_Stop(void)
{
    NetSv_Intermission(IMF_END, 0, 0);
    intermission = false;
    UnloadPics();
    SB_state = -1;
}

/**
 * Initializes the stats for single player mode
 */
static void InitStats(void)
{
    int         i, j;
    signed int  slaughterfrags;
    int         posnum;
    int         slaughtercount;
    int         playercount;

    if(!deathmatch)
    {
#if 0
        int     oldCluster;
        char   *msgLumpName;
        size_t  msgSize;
        int     msgLump;
        extern int LeaveMap;

        gametype = SINGLE;
        HubCount = 0;
        oldCluster = P_GetMapCluster(gamemap);
        if(oldCluster != P_GetMapCluster(LeaveMap))
        {
            if(oldCluster >= 1 && oldCluster <= 5)
            {
                if(!overrideHubMsg)
                {
                    msgLumpName = ClusMsgLumpNames[oldCluster - 1];
                    msgLump = W_CheckNumForName(msgLumpName);
                    if(msgLump < 0)
                    {
                        HubText = "correct hub message not found";
                    }
                    else
                    {
                        msgSize = W_LumpLength(msgLump);
                        if(msgSize >= MAX_INTRMSN_MESSAGE_SIZE)
                        {
                            Con_Error("Cluster message too long (%s)",
                                      msgLumpName);
                        }
                        gi.W_ReadLump(msgLump, ClusterMessage);
                        ClusterMessage[msgSize] = 0;    // Append terminator
                        HubText = ClusterMessage;
                    }
                }
                else
                {
                    HubText = "null hub message";
                }
                HubCount = strlen(HubText) * TEXTSPEED + TEXTWAIT;
                S_StartSongName("hub", true);
            }
        }
#endif
    }
    else
    {
        gametype = DEATHMATCH;
        slaughterboy = 0;
        slaughterfrags = -9999;
        posnum = 0;
        playercount = 0;
        slaughtercount = 0;
        for(i = 0; i < MAXPLAYERS; i++)
        {
            totalFrags[i] = 0;
            if(players[i].plr->ingame)
            {
                playercount++;
                for(j = 0; j < MAXPLAYERS; j++)
                {
                    if(players[i].plr->ingame)
                    {
                        totalFrags[i] += players[i].frags[j];
                    }
                }
                posnum++;
            }
            if(totalFrags[i] > slaughterfrags)
            {
                slaughterboy = 1 << i;
                slaughterfrags = totalFrags[i];
                slaughtercount = 1;
            }
            else if(totalFrags[i] == slaughterfrags)
            {
                slaughterboy |= 1 << i;
                slaughtercount++;
            }
        }
        if(playercount == slaughtercount)
        {   // don't do the slaughter stuff if everyone is equal
            slaughterboy = 0;
        }
        S_StartMusic("hub", true);
    }
}

static void LoadPics(void)
{
    int         i;

    if(HubCount || gametype == DEATHMATCH)
    {
        patchINTERPICLumpRS = W_GetNumForName("INTERPIC");
        FontBLumpBase = W_GetNumForName("FONTB16");
        for(i = 0; i < 10; ++i)
        {
            FontBNumbersLump[i] = FontBLumpBase + i;
        }
        FontBLump = W_GetNumForName("FONTB_S") + 1;
        FontBNegativeLump = W_GetNumForName("FONTB13");
        FontABaseLump = W_GetNumForName("FONTA_S") + 1;

        FontBSlashLump = W_GetNumForName("FONTB15");
        FontBPercentLump = W_GetNumForName("FONTB05");
    }
}

static void UnloadPics(void)
{
    // Nothing to do.
}

void IN_Ticker(void)
{
    if(!intermission)
    {
        return;
    }
    if(interstate)
    {
        WaitStop();
        return;
    }
    skipintermission = false;
    CheckForSkip();
    intertime++;
    if(skipintermission || (gametype == SINGLE && !HubCount))
    {
        interstate = 1;
        NetSv_Intermission(IMF_STATE, interstate, 0);
        cnt = 10;
        skipintermission = false;
    }
}

/**
 * Check to see if any player hit a key
 */
static void CheckForSkip(void)
{
    int         i;
    player_t   *player;
    static boolean triedToSkip;

    for(i = 0, player = players; i < MAXPLAYERS; ++i, player++)
    {
        if(players[i].plr->ingame)
        {
            if(player->brain.attack)
            {
                if(!player->attackdown)
                {
                    skipintermission = 1;
                }
                player->attackdown = true;
            }
            else
            {
                player->attackdown = false;
            }
            if(player->brain.use)
            {
                if(!player->usedown)
                {
                    skipintermission = 1;
                }
                player->usedown = true;
            }
            else
            {
                player->usedown = false;
            }
        }
    }
    if(deathmatch && intertime < 140)
    {   // wait for 4 seconds before allowing a skip
        if(skipintermission == 1)
        {
            triedToSkip = true;
            skipintermission = 0;
        }
    }
    else
    {
        if(triedToSkip)
        {
            skipintermission = 1;
            triedToSkip = false;
        }
    }
}

void IN_Drawer(void)
{
    if(!intermission)
    {
        return;
    }
    if(interstate)
    {
        return;
    }
    GL_DrawRawScreen(patchINTERPICLumpRS, 0, 0);

    if(gametype != SINGLE)
    {
        DrDeathTally();
    }
}

static void DrDeathTally(void)
{
    int         i, j;
    fixed_t     xPos, yPos;
    fixed_t     xDelta, yDelta;
    fixed_t     xStart, scale;
    int         x, y;
    boolean     bold;
    static boolean showTotals;
    int         temp;

    GL_DrawPatch(TALLY_TOP_X, TALLY_TOP_Y, W_GetNumForName("tallytop"));
    GL_DrawPatch(TALLY_LEFT_X, TALLY_LEFT_Y, W_GetNumForName("tallylft"));

    if(intertime < TALLY_EFFECT_TICKS)
    {
        showTotals = false;
        scale = (intertime * FRACUNIT) / TALLY_EFFECT_TICKS;
        xDelta = FixedMul(scale, TALLY_FINAL_X_DELTA);
        yDelta = FixedMul(scale, TALLY_FINAL_Y_DELTA);
        xStart =
            TALLY_START_XPOS - FixedMul(scale,
                                        TALLY_START_XPOS - TALLY_STOP_XPOS);
        yPos =
            TALLY_START_YPOS - FixedMul(scale,
                                        TALLY_START_YPOS - TALLY_STOP_YPOS);
    }
    else
    {
        xDelta = TALLY_FINAL_X_DELTA;
        yDelta = TALLY_FINAL_Y_DELTA;
        xStart = TALLY_STOP_XPOS;
        yPos = TALLY_STOP_YPOS;
    }
    if(intertime >= TALLY_EFFECT_TICKS && showTotals == false)
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
            bold = (i == consoleplayer || j == consoleplayer);
            if(players[i].plr->ingame && players[j].plr->ingame)
            {
                if(bold)
                {
                    DrNumberBold(players[i].frags[j], x, y, 100);
                }
                else
                {
                    DrNumber(players[i].frags[j], x, y, 100);
                }
            }
            else
            {
                temp = M_StringWidth("--", hu_font_a) / 2;
                if(bold)
                {
                    M_WriteText2(x - temp, y, "--", hu_font_a, 1, 0.7f, 0.3f, 1);
                }
                else
                {
                    M_WriteText2(x - temp, y, "--", hu_font_a, 1, 1, 1, 1);
                }
            }
        }
        if(showTotals && players[i].plr->ingame &&
           !((slaughterboy & (1 << i)) && !(intertime & 16)))
        {
            DrNumber(totalFrags[i], TALLY_TOTALS_X, y, 1000);
        }
        yPos += yDelta;
        y = yPos >> FRACBITS;
    }
}

static void DrNumber(int val, int x, int y, int wrapThresh)
{
    char    buff[8] = "XX";

    if(!(val < -9 && wrapThresh < 1000))
    {
        sprintf(buff, "%d", val >= wrapThresh ? val % wrapThresh : val);
    }

    M_WriteText2(x - M_StringWidth(buff, hu_font_a) / 2, y, buff,
                 hu_font_a, 1, 1, 1, 1);
}

static void DrNumberBold(int val, int x, int y, int wrapThresh)
{
    char    buff[8] = "XX";

    if(!(val < -9 && wrapThresh < 1000))
    {
        sprintf(buff, "%d", val >= wrapThresh ? val % wrapThresh : val);
    }

    M_WriteText2(x - M_StringWidth(buff, hu_font_a) / 2, y, buff,
                 hu_font_a, 1, 0.7f, 0.3f, 1);
}
