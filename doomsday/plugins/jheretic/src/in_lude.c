/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * in_lude.c: Intermission/stat screens.
 */

// HEADER FILES ------------------------------------------------------------

#include "jheretic.h"

#include "hu_stuff.h"
#include "d_net.h"
#include "am_map.h"
#include "p_tick.h"

// MACROS ------------------------------------------------------------------

#define NUMTEAMS            (4) // Four colors, four teams.

// TYPES -------------------------------------------------------------------

typedef enum gametype_e {
    SINGLE,
    COOPERATIVE,
    DEATHMATCH
} gametype_t;

typedef struct teaminfo_s {
    int             members;
    int             frags[NUMTEAMS];
    int             totalFrags;
} teaminfo_t;

typedef struct yahpt_s {
    int             x, y;
} yahpt_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void    IN_DrawOldLevel(void);
void    IN_DrawYAH(void);
void    IN_DrawStatBack(void);
void    IN_DrawSingleStats(void);
void    IN_DrawCoopStats(void);
void    IN_DrawDMStats(void);
void    IN_DrawNumber(int val, int x, int y, int digits, float r, float g, float b, float a);
void    IN_DrawTime(int x, int y, int h, int m, int s, float r, float g, float b, float a);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean intermission;

int     interState = 0;
int     interTime = -1;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean skipIntermission;

static int oldInterTime = 0;
static gametype_t gameType;

static int cnt;

static int time;
static int hours;
static int minutes;
static int seconds;

static int slaughterBoy; // In DM, the player with the most kills.

static int killPercent[NUMTEAMS];
static int bonusPercent[NUMTEAMS];
static int secretPercent[NUMTEAMS];

static int playerTeam[MAXPLAYERS];
static teaminfo_t teamInfo[NUMTEAMS];

static int interPic, beenThere, goingThere, numbers[10], negative, slash,
    percent;

static int patchFaceOkayBase;
static int patchFaceDeadBase;

static fixed_t dSlideX[NUMTEAMS];
static fixed_t dSlideY[NUMTEAMS];

static char *killersText[] = { "K", "I", "L", "L", "E", "R", "S" };

static yahpt_t YAHspot[3][9] = {
    {
     {172, 78},
     {86, 90},
     {73, 66},
     {159, 95},
     {148, 126},
     {132, 54},
     {131, 74},
     {208, 138},
     {52, 101}
     },
    {
     {218, 57},
     {137, 81},
     {155, 124},
     {171, 68},
     {250, 86},
     {136, 98},
     {203, 90},
     {220, 140},
     {279, 106}
     },
    {
     {86, 99},
     {124, 103},
     {154, 79},
     {202, 83},
     {178, 59},
     {142, 58},
     {219, 66},
     {247, 57},
     {107, 80}
     }
};

// CODE --------------------------------------------------------------------

void IN_Start(void)
{
    uint                i;

    NetSv_Intermission(IMF_BEGIN, 0, 0);

    IN_LoadPics();
    IN_InitStats();
    intermission = true;
    interState = -1;
    skipIntermission = false;
    interTime = 0;
    oldInterTime = 0;
    for(i = 0; i < MAXPLAYERS; ++i)
        AM_Open(AM_MapForPlayer(i), false, true);

    S_StartMusic("intr", true);
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

    intermission = false;
    IN_UnloadPics();
}

/**
 * Initializes the stats for single player mode
 */
void IN_InitStats(void)
{
    signed int          slaughterfrags;

    int                 i, j;
    int                 posNum;
    int                 slaughterCount;
    int                 teamCount, team;

    // Init team info.
    if(IS_NETGAME)
    {
        memset(teamInfo, 0, sizeof(teamInfo));
        memset(playerTeam, 0, sizeof(playerTeam));
        for(i = 0; i < MAXPLAYERS; ++i)
        {
            if(!players[i].plr->inGame)
                continue;

            playerTeam[i] = gs.players[i].color;
            teamInfo[playerTeam[i]].members++;
        }
    }

    time = mapTime / 35;
    hours = time / 3600;
    time -= hours * 3600;
    minutes = time / 60;
    time -= minutes * 60;
    seconds = time;

#ifdef _DEBUG
    Con_Printf("%i %i %i\n", hours, minutes, seconds);
#endif

    if(!IS_NETGAME)
    {
        gameType = SINGLE;
    }
    else if( /*IS_NETGAME && */ !deathmatch)
    {
        gameType = COOPERATIVE;
        memset(killPercent, 0, sizeof(killPercent));
        memset(bonusPercent, 0, sizeof(bonusPercent));
        memset(secretPercent, 0, sizeof(secretPercent));
        for(i = 0; i < MAXPLAYERS; ++i)
        {
            if(players[i].plr->inGame)
            {
                if(totalKills)
                {
                    j = players[i].killCount * 100 / totalKills;
                    if(j > killPercent[playerTeam[i]])
                        killPercent[playerTeam[i]] = j;
                }

                if(totalItems)
                {
                    j = players[i].itemCount * 100 / totalItems;
                    if(j > bonusPercent[playerTeam[i]])
                        bonusPercent[playerTeam[i]] = j;
                }

                if(totalSecret)
                {
                    j = players[i].secretCount * 100 / totalSecret;
                    if(j > secretPercent[playerTeam[i]])
                        secretPercent[playerTeam[i]] = j;
                }
            }
        }
    }
    else
    {
        gameType = DEATHMATCH;
        slaughterBoy = 0;
        slaughterfrags = -9999;
        posNum = 0;
        teamCount = 0;
        slaughterCount = 0;
        for(i = 0; i < MAXPLAYERS; ++i)
        {
            team = playerTeam[i];
            if(players[i].plr->inGame)
            {
                for(j = 0; j < MAXPLAYERS; ++j)
                {
                    if(players[j].plr->inGame)
                    {
                        teamInfo[team].frags[playerTeam[j]] +=
                            players[i].frags[j];
                        teamInfo[team].totalFrags += players[i].frags[j];
                    }
                }

                // Find out the largest number of frags.
                if(teamInfo[team].totalFrags > slaughterfrags)
                    slaughterfrags = teamInfo[team].totalFrags;
            }
        }

        for(i = 0; i < NUMTEAMS; ++i)
        {
            //posNum++;
            /*if(teamInfo[i].totalFrags > slaughterfrags)
               {
               slaughterBoy = 1<<i;
               slaughterfrags = teamInfo[i].totalFrags;
               slaughterCount = 1;
               }
               else */
            if(!teamInfo[i].members)
                continue;

            dSlideX[i] = (43 * posNum * FRACUNIT) / 20;
            dSlideY[i] = (36 * posNum * FRACUNIT) / 20;
            posNum++;

            teamCount++;
            if(teamInfo[i].totalFrags == slaughterfrags)
            {
                slaughterBoy |= 1 << i;
                slaughterCount++;
            }
        }

        if(teamCount == slaughterCount)
        {   // Don't do the slaughter stuff if everyone is equal.
            slaughterBoy = 0;
        }
    }
}

void IN_LoadPics(void)
{
    int                 i;

    switch(gameEpisode)
    {
    case 1:
        interPic = W_GetNumForName("MAPE1");
        break;
    case 2:
        interPic = W_GetNumForName("MAPE2");
        break;
    case 3:
        interPic = W_GetNumForName("MAPE3");
        break;
    default:
        break;
    }

    beenThere = W_GetNumForName("IN_X");
    goingThere = W_GetNumForName("IN_YAH");

    for(i = 0; i < 10; ++i)
    {
        numbers[i] = huFontB[i+15].lump;
    }
    negative = huFontB[13].lump;

    slash = huFontB[14].lump;
    percent = huFontB[5].lump;

    patchFaceOkayBase = W_GetNumForName("FACEA0");
    patchFaceDeadBase = W_GetNumForName("FACEB0");
}

void IN_UnloadPics(void)
{
    /* Nothing to do...*/
}

void IN_Ticker(void)
{
    if(!intermission)
    {
        return;
    }

    if(!IS_CLIENT)
    {
        if(interState == 3)
        {
            IN_WaitStop();
            return;
        }
        IN_CheckForSkip();
    }

    interTime++;
    if(oldInterTime < interTime)
    {
        interState++;
        if(gameEpisode > 3 && interState >= 1)
        {
            // Extended Wad levels:  skip directly to the next level
            interState = 3;
        }

        switch(interState)
        {
        case 0:
            oldInterTime = interTime + 300;
            if(gameEpisode > 3)
            {
                oldInterTime = interTime + 1200;
            }
            break;

        case 1:
            oldInterTime = interTime + 200;
            break;

        case 2:
            oldInterTime = MAXINT;
            break;

        case 3:
            cnt = 10;
            break;

        default:
            break;
        }
    }

    if(skipIntermission)
    {
        if(interState == 0 && interTime < 150)
        {
            interTime = 150;
            skipIntermission = false;
            NetSv_Intermission(IMF_TIME, 0, interTime);
            return;
        }
        else if(interState < 2 && gameEpisode < 4)
        {
            interState = 2;
            skipIntermission = false;
            S_StartSound(SFX_DORCLS, NULL);
            NetSv_Intermission(IMF_STATE, interState, 0);
            return;
        }

        interState = 3;
        cnt = 10;
        skipIntermission = false;
        S_StartSound(SFX_DORCLS, NULL);
        NetSv_Intermission(IMF_STATE, interState, 0);
    }
}

/**
 * Check to see if any player hit a key.
 */
void IN_CheckForSkip(void)
{
    int                 i;
    player_t           *player;

    if(IS_CLIENT)
        return;

    for(i = 0, player = players; i < MAXPLAYERS; ++i, player++)
    {
        if(players->plr->inGame)
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
}

void IN_Drawer(void)
{
    static int          oldInterState;

    if(!intermission || interState < 0 || interState > 3)
    {
        return;
    }
    if(interState == 3)
    {
        return;
    }

    if(oldInterState != 2 && interState == 2)
    {
        S_LocalSound(SFX_PSTOP, NULL);
    }

    oldInterState = interState;
    switch(interState)
    {
    case 0: // Draw stats.
        IN_DrawStatBack();
        switch (gameType)
        {
        case SINGLE:
            IN_DrawSingleStats();
            break;
        case COOPERATIVE:
            IN_DrawCoopStats();
            break;
        case DEATHMATCH:
            IN_DrawDMStats();
            break;
        }
        break;

    case 1: // Leaving old level.
        if(gameEpisode < 4)
        {
            GL_DrawPatch(0, 0, interPic);
            IN_DrawOldLevel();
        }
        break;

    case 2: // Going to the next level.
        if(gameEpisode < 4)
        {
            GL_DrawPatch(0, 0, interPic);
            IN_DrawYAH();
        }
        break;

    case 3: // Waiting before going to the next level.
        if(gameEpisode < 4)
        {
            GL_DrawPatch(0, 0, interPic);
        }
        break;

    default:
        Con_Error("IN_lude:  Intermission state out of range.\n");
        break;
    }
}

void IN_DrawStatBack(void)
{
    DGL_Color4f(1, 1, 1, 1);
    DGL_SetMaterial(P_ToPtr(DMU_MATERIAL,
        P_MaterialNumForName("FLOOR16", MN_FLATS)));
    DGL_DrawRectTiled(0, 0, SCREENWIDTH, SCREENHEIGHT, 64, 64);
}

void IN_DrawOldLevel(void)
{
    int                 i, x;
    char               *levelname;

    levelname = P_GetShortMapName(gameEpisode, prevMap);

    x = 160 - M_StringWidth(levelname, huFontB) / 2;
    M_WriteText2(x, 3, levelname, huFontB, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    x = 160 - M_StringWidth("FINISHED", huFontA) / 2;
    M_WriteText2(x, 25, "FINISHED", huFontA, defFontRGB2[0], defFontRGB2[1],defFontRGB2[2], 1);

    if(prevMap == 9)
    {
        for(i = 0; i < gameMap - 1; ++i)
        {
            GL_DrawPatch(YAHspot[gameEpisode - 1][i].x,
                         YAHspot[gameEpisode - 1][i].y, beenThere);
        }

        if(!(interTime & 16))
        {
            GL_DrawPatch(YAHspot[gameEpisode - 1][8].x,
                         YAHspot[gameEpisode - 1][8].y, beenThere);
        }
    }
    else
    {
        for(i = 0; i < prevMap - 1; ++i)
        {
            GL_DrawPatch(YAHspot[gameEpisode - 1][i].x,
                         YAHspot[gameEpisode - 1][i].y, beenThere);
        }

        if(players[CONSOLEPLAYER].didSecret)
        {
            GL_DrawPatch(YAHspot[gameEpisode - 1][8].x,
                         YAHspot[gameEpisode - 1][8].y, beenThere);
        }

        if(!(interTime & 16))
        {
            GL_DrawPatch(YAHspot[gameEpisode - 1][prevMap - 1].x,
                         YAHspot[gameEpisode - 1][prevMap - 1].y, beenThere);
        }
    }
}

void IN_DrawYAH(void)
{
    int                 i, x;
    char               *levelname;

    levelname = P_GetShortMapName(gameEpisode, gameMap);

    x = 160 - M_StringWidth("NOW ENTERING:", huFontA) / 2;
    M_WriteText2(x, 10, "NOW ENTERING:", huFontA, defFontRGB2[0], defFontRGB2[1], defFontRGB2[2], 1);

    x = 160 - M_StringWidth(levelname, huFontB) / 2;
    M_WriteText2(x, 20, levelname, huFontB, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    if(prevMap == 9)
    {
        prevMap = gameMap - 1;
    }

    for(i = 0; i < prevMap; ++i)
    {
        GL_DrawPatch(YAHspot[gameEpisode - 1][i].x,
                     YAHspot[gameEpisode - 1][i].y, beenThere);
    }

    if(players[CONSOLEPLAYER].didSecret)
    {
        GL_DrawPatch(YAHspot[gameEpisode - 1][8].x,
                     YAHspot[gameEpisode - 1][8].y, beenThere);
    }

    if(!(interTime & 16) || interState == 3)
    {   // Draw the destination 'X'
        GL_DrawPatch(YAHspot[gameEpisode - 1][gameMap - 1].x,
                     YAHspot[gameEpisode - 1][gameMap - 1].y, goingThere);
    }
}

void IN_DrawSingleStats(void)
{
    static int          sounds;

    int                 x;
    char               *levelname;

    levelname = P_GetShortMapName(gameEpisode, prevMap);

    M_WriteText2(50, 65, "KILLS", huFontB, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
    M_WriteText2(50, 90, "ITEMS", huFontB, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
    M_WriteText2(50, 115, "SECRETS", huFontB, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    x = 160 - M_StringWidth(levelname, huFontB) / 2;
    M_WriteText2(x, 3, levelname, huFontB, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    x = 160 - M_StringWidth("FINISHED", huFontA) / 2;
    M_WriteText2(x, 25, "FINISHED", huFontA, defFontRGB2[0], defFontRGB2[1], defFontRGB2[2], 1);

    if(interTime < 30)
    {
        sounds = 0;
        return;
    }

    if(sounds < 1 && interTime >= 30)
    {
        S_LocalSound(SFX_DORCLS, NULL);
        sounds++;
    }

    IN_DrawNumber(players[CONSOLEPLAYER].killCount, 200, 65, 3, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
    GL_DrawPatchLitAlpha(250, 67, 0, .4f, slash);
    DGL_Color4f(defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
    GL_DrawPatch_CS(248, 65, slash);
    IN_DrawNumber(totalKills, 248, 65, 3, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    if(interTime < 60)
    {
        return;
    }

    if(sounds < 2 && interTime >= 60)
    {
        S_LocalSound(SFX_DORCLS, NULL);
        sounds++;
    }

    IN_DrawNumber(players[CONSOLEPLAYER].itemCount, 200, 90, 3, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
    GL_DrawPatchLitAlpha(250, 92, 0, .4f, slash);
    DGL_Color4f(defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
    GL_DrawPatch_CS(248, 90, slash);
    IN_DrawNumber(totalItems, 248, 90, 3, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    if(interTime < 90)
    {
        return;
    }

    if(sounds < 3 && interTime >= 90)
    {
        S_LocalSound(SFX_DORCLS, NULL);
        sounds++;
    }

    IN_DrawNumber(players[CONSOLEPLAYER].secretCount, 200, 115, 3, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
    GL_DrawPatchLitAlpha(250, 117, 0, .4f, slash);
    DGL_Color4f(defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
    GL_DrawPatch_CS(248, 115, slash);
    IN_DrawNumber(totalSecret, 248, 115, 3, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    if(interTime < 150)
    {
        return;
    }

    if(sounds < 4 && interTime >= 150)
    {
        S_LocalSound(SFX_DORCLS, NULL);
        sounds++;
    }

    if(gameMode != extended || gameEpisode < 4)
    {
        M_WriteText2(85, 160, "TIME", huFontB, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
        IN_DrawTime(155, 160, hours, minutes, seconds, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
    }
    else
    {
        x = 160 - M_StringWidth("NOW ENTERING:", huFontA) / 2;
        M_WriteText2(x, 160, "NOW ENTERING:", huFontA, defFontRGB2[0], defFontRGB2[1], defFontRGB2[2], 1);

        levelname = P_GetShortMapName(gameEpisode, gameMap);

        x = 160 - M_StringWidth(levelname, huFontB) / 2;
        M_WriteText2(x, 170, levelname, huFontB, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

        skipIntermission = false;
    }
}

void IN_DrawCoopStats(void)
{
    static int          sounds;

    int                 i;
    int                 x;
    int                 ypos;
    char               *levelname;

    levelname = P_GetShortMapName(gameEpisode, prevMap);

    M_WriteText2(95, 35, "KILLS", huFontB, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
    M_WriteText2(155, 35, "BONUS", huFontB, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
    M_WriteText2(232, 35, "SECRET", huFontB, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    x = 160 - M_StringWidth(levelname, huFontB) / 2;
    M_WriteText2(x, 3, levelname, huFontB, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    x = 160 - M_StringWidth("FINISHED", huFontA) / 2;
    M_WriteText2(x, 25, "FINISHED", huFontA, defFontRGB2[0], defFontRGB2[1], defFontRGB2[2], 1);

    ypos = 50;
    for(i = 0; i < NUMTEAMS; ++i)
    {
        if(teamInfo[i].members)
        {
            GL_DrawPatchLitAlpha(27, ypos+2, 0, .4f, patchFaceOkayBase + i);
            DGL_Color4f(defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
            GL_DrawPatch_CS(25, ypos, patchFaceOkayBase + i);

            if(interTime < 40)
            {
                sounds = 0;
                ypos += 37;
                continue;
            }
            else if(interTime >= 40 && sounds < 1)
            {
                S_LocalSound(SFX_DORCLS, NULL);
                sounds++;
            }

            IN_DrawNumber(killPercent[i], 85, ypos + 10, 3, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
            GL_DrawPatchLitAlpha(123, ypos + 12, 0, .4f, percent);

            DGL_Color4f(defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
            GL_DrawPatch_CS(121, ypos + 10, percent);
            IN_DrawNumber(bonusPercent[i], 160, ypos + 10, 3, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
            GL_DrawPatchLitAlpha(198, ypos + 12, 0, .4f, percent);

            DGL_Color4f(defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
            GL_DrawPatch_CS(196, ypos + 10, percent);
            IN_DrawNumber(secretPercent[i], 237, ypos + 10, 3, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
            GL_DrawPatchLitAlpha(275, ypos + 12, 0, .4f, percent);

            DGL_Color4f(defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
            GL_DrawPatch_CS(273, ypos + 10, percent);
            ypos += 37;
        }
    }
}

void IN_DrawDMStats(void)
{
    static int          sounds;

    int                 i, j;
    int                 ypos, xpos, kpos;

    xpos = 90;
    ypos = 55;

    M_WriteText2(265, 30, "TOTAL", huFontB, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
    M_WriteText2(140, 8, "VICTIMS", huFontA, defFontRGB2[0], defFontRGB2[1], defFontRGB2[2], 1);

    for(i = 0; i < 7; ++i)
    {
        M_WriteText2(10, 80 + 9 * i, killersText[i], huFontA, defFontRGB2[0], defFontRGB2[1], defFontRGB2[2], 1);
    }

    if(interTime < 20)
    {
        for(i = 0; i < NUMTEAMS; ++i)
        {
            if(teamInfo[i].members)
            {
                GL_DrawShadowedPatch(40,
                                     ((ypos << FRACBITS) +
                                      dSlideY[i] * interTime) >> FRACBITS,
                                     patchFaceOkayBase + i);
                GL_DrawShadowedPatch(((xpos << FRACBITS) +
                                      dSlideX[i] * interTime) >> FRACBITS, 18,
                                     patchFaceDeadBase + i);
            }
        }

        sounds = 0;
        return;
    }

    if(interTime >= 20 && sounds < 1)
    {
        S_LocalSound(SFX_DORCLS, NULL);
        sounds++;
    }

    if(interTime >= 100 && slaughterBoy && sounds < 2)
    {
        S_LocalSound(SFX_WPNUP, NULL);
        sounds++;
    }

    for(i = 0; i < NUMTEAMS; ++i)
    {
        if(teamInfo[i].members)
        {
            if(interTime < 100 || i == playerTeam[CONSOLEPLAYER])
            {
                GL_DrawShadowedPatch(40, ypos, patchFaceOkayBase + i);
                GL_DrawShadowedPatch(xpos, 18, patchFaceDeadBase + i);
            }
            else
            {
                GL_DrawFuzzPatch(40, ypos, patchFaceOkayBase + i);
                GL_DrawFuzzPatch(xpos, 18, patchFaceDeadBase + i);
            }

            kpos = 86;
            for(j = 0; j < NUMTEAMS; ++j)
            {
                if(teamInfo[j].members)
                {
                    IN_DrawNumber(teamInfo[i].frags[j]
                                  /*players[i].frags[j] */ , kpos, ypos + 10,
                                  3, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
                    kpos += 43;
                }
            }

            if(slaughterBoy & (1 << i))
            {
                if(!(interTime & 16))
                {
                    IN_DrawNumber(teamInfo[i].totalFrags, 263, ypos + 10, 3,
                            defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
                }
            }
            else
            {
                IN_DrawNumber(teamInfo[i].totalFrags, 263, ypos + 10, 3,
                            defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
            }

            ypos += 36;
            xpos += 43;
        }
    }
}

void IN_DrawTime(int x, int y, int h, int m, int s, float r, float g,
                 float b, float a)
{
    if(h)
    {
        IN_DrawNumber(h, x, y, 2, r, g, b, a);

        M_WriteText2(x + 26, y, ":", huFontB, r, g, b, a);
    }

    x += 34;
    if(m || h)
    {
        IN_DrawNumber(m, x, y, 2, r, g, b, a);
    }

    x += 34;
    M_WriteText2(x-8, y, ":", huFontB, r, g, b, a);
    IN_DrawNumber(s, x, y, 2, r, g, b, a);
}

void IN_DrawNumber(int val, int x, int y, int digits, float r, float g,
                   float b, float a)
{
    int                 xpos;
    int                 oldval;
    int                 realdigits;
    boolean             neg;

    oldval = val;
    xpos = x;
    neg = false;
    realdigits = 1;

    if(val < 0)
    {   //...this should reflect negative frags.
        val = -val;
        neg = true;
        if(val > 99)
        {
            val = 99;
        }
    }

    if(val > 9)
    {
        realdigits++;
        if(digits < realdigits)
        {
            realdigits = digits;
            val = 9;
        }
    }

    if(val > 99)
    {
        realdigits++;
        if(digits < realdigits)
        {
            realdigits = digits;
            val = 99;
        }
    }

    if(val > 999)
    {
        realdigits++;
        if(digits < realdigits)
        {
            realdigits = digits;
            val = 999;
        }
    }

    if(digits == 4)
    {
        GL_DrawPatchLitAlpha(xpos + 8 - huFontB[val / 1000].width / 2 - 12, y + 2, 0, .4f, numbers[val / 1000]);
        DGL_Color4f(r, g, b, a);
        GL_DrawPatch_CS(xpos + 6 - huFontB[val / 1000].width / 2 - 12, y, numbers[val / 1000]);
    }

    if(digits > 2)
    {
        if(realdigits > 2)
        {
            GL_DrawPatchLitAlpha(xpos + 8 - huFontB[val / 100].width / 2, y+2, 0, .4f, numbers[val / 100]);
            DGL_Color4f(r, g, b, a);
            GL_DrawPatch_CS(xpos + 6 - huFontB[val / 100].width / 2, y, numbers[val / 100]);
        }
        xpos += 12;
    }

    val = val % 100;
    if(digits > 1)
    {
        if(val > 9)
        {
            GL_DrawPatchLitAlpha(xpos + 8 - huFontB[val / 10].width / 2, y+2, 0, .4f, numbers[val / 10]);
            DGL_Color4f(r, g, b, a);
            GL_DrawPatch_CS(xpos + 6 - huFontB[val / 10].width / 2, y, numbers[val / 10]);
        }
        else if(digits == 2 || oldval > 99)
        {
            GL_DrawPatchLitAlpha(xpos+2, y+2, 0, .4f, numbers[0]);
            DGL_Color4f(r, g, b, a);
            GL_DrawPatch_CS(xpos, y, numbers[0]);
        }
        xpos += 12;
    }

    val = val % 10;
    GL_DrawPatchLitAlpha(xpos + 8 - huFontB[val].width / 2, y+2, 0, .4f, numbers[val]);
    DGL_Color4f(r, g, b, a);
    GL_DrawPatch_CS(xpos + 6 - huFontB[val].width / 2, y, numbers[val]);
    if(neg)
    {
        GL_DrawPatchLitAlpha(xpos + 8 - huFontB[negative].width / 2 - 12 * (realdigits), y+2, 0, .4f, negative);
        DGL_Color4f(r, g, b, a);
        GL_DrawPatch_CS(xpos + 6 - huFontB[negative].width / 2 - 12 * (realdigits), y, negative);
    }
}
