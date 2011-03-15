/**\file in_lude.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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
 * Intermission/stat screens - jHeretic specific.
 */

// HEADER FILES ------------------------------------------------------------

#include <string.h>

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

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean intermission;

int     interState = 0;
int     interTime = -1;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// Used for timing of background animation.
static int bcnt;

// Contains information passed into intermission.
static wbstartstruct_t *wbs;

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

static patchinfo_t dpInterPic;
static patchinfo_t dpBeenThere;
static patchinfo_t dpGoingThere;
static patchinfo_t dpFaceAlive[NUMTEAMS];
static patchinfo_t dpFaceDead[NUMTEAMS];

static fixed_t dSlideX[NUMTEAMS];
static fixed_t dSlideY[NUMTEAMS];

static char* killersText[] = { "K", "I", "L", "L", "E", "R", "S" };

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

void IN_DrawTime(int x, int y, int h, int m, int s, int fontIdx, int tracking, float r, float g, float b, float a)
{
    char buf[20];

    dd_snprintf(buf, 20, "%02d", s);
    M_DrawTextFragmentShadowed(buf, x, y, fontIdx, DTF_ALIGN_TOPRIGHT, tracking, r, g, b, a);
    FR_SetFont(FID(fontIdx));
    x -= FR_TextFragmentWidth2(buf, tracking) + tracking * 3;
    M_DrawTextFragmentShadowed(":", x, y, fontIdx, DTF_ALIGN_TOPRIGHT, tracking, r, g, b, a);
    FR_SetFont(FID(fontIdx));
    x -= FR_CharWidth(':') + 3;

    if(m || h)
    {
        dd_snprintf(buf, 20, "%02d", m);
        M_DrawTextFragmentShadowed(buf, x, y, fontIdx, DTF_ALIGN_TOPRIGHT, tracking, r, g, b, a);
        FR_SetFont(FID(fontIdx));
        x -= FR_TextFragmentWidth2(buf, tracking) + tracking * 3;
    }
   
    if(h)
    {
        dd_snprintf(buf, 20, "%02d", h);
        M_DrawTextFragmentShadowed(":", x, y, fontIdx, DTF_ALIGN_TOPRIGHT, tracking, r, g, b, a);
        FR_SetFont(FID(fontIdx));
        x -= FR_CharWidth(':') + tracking * 3;
        M_DrawTextFragmentShadowed(buf, x, y, fontIdx, DTF_ALIGN_TOPRIGHT, tracking, r, g, b, a);
    }
}

void WI_initVariables(wbstartstruct_t * wbstartstruct)
{
    wbs = wbstartstruct;

/*#ifdef RANGECHECK
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
    interState = -1;
    skipIntermission = false;
    interTime = 0;
    oldInterTime = 0;
}

void IN_Init(wbstartstruct_t * wbstartstruct)
{
    WI_initVariables(wbstartstruct);
    IN_LoadPics();

    IN_InitStats();
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

            playerTeam[i] = cfg.playerColor[i];
            teamInfo[playerTeam[i]].members++;
        }
    }

    time = mapTime / 35;
    hours = time / 3600;
    time -= hours * 3600;
    minutes = time / 60;
    time -= minutes * 60;
    seconds = time;

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
    char buf[9];
    int i;

    if(wbs->episode < 3)
        R_PrecachePatch(wbs->episode == 0? "MAPE1" : wbs->episode == 1? "MAPE2" : "MAPE3", &dpInterPic);

    R_PrecachePatch("IN_X", &dpBeenThere);
    R_PrecachePatch("IN_YAH", &dpGoingThere);

    for(i = 0; i < NUMTEAMS; ++i)
    {
        dd_snprintf(buf, 9, "FACEA%i", i);
        R_PrecachePatch(buf, &dpFaceAlive[i]);
        dd_snprintf(buf, 9, "FACEB%i", i);
        R_PrecachePatch(buf, &dpFaceDead[i]);
    }
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

    // Counter for general background animation.
    bcnt++;

    interTime++;
    if(oldInterTime < interTime)
    {
        interState++;
        if(wbs->episode > 2 && interState >= 1)
        {
            // Extended Wad levels:  skip directly to the next level
            interState = 3;
        }

        switch(interState)
        {
        case 0:
            oldInterTime = interTime + 300;
            if(wbs->episode > 2)
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
        else if(interState < 2 && wbs->episode < 3)
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

    if(!intermission || interState > 3)
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

    if(interState != -1)
        oldInterState = interState;

    switch(interState)
    {
    case -1:
    case 0: // Draw stats.
        IN_DrawStatBack();
        switch(gameType)
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
        if(wbs->episode < 3)
        {
            DGL_Enable(DGL_TEXTURE_2D);

            DGL_Color4f(1, 1, 1, 1);
            GL_DrawPatch(dpInterPic.id, 0, 0);

            DGL_Disable(DGL_TEXTURE_2D);

            IN_DrawOldLevel();
        }
        break;

    case 2: // Going to the next level.
        if(wbs->episode < 3)
        {
            DGL_Enable(DGL_TEXTURE_2D);

            DGL_Color4f(1, 1, 1, 1);
            GL_DrawPatch(dpInterPic.id, 0, 0);
            IN_DrawYAH();

            DGL_Disable(DGL_TEXTURE_2D);
        }
        break;

    case 3: // Waiting before going to the next level.
        if(wbs->episode < 3)
        {
            DGL_Enable(DGL_TEXTURE_2D);

            DGL_Color4f(1, 1, 1, 1);
            GL_DrawPatch(dpInterPic.id, 0, 0);

            DGL_Disable(DGL_TEXTURE_2D);
        }
        break;

    default:
        Con_Error("IN_lude:  Intermission state out of range.\n");
        break;
    }
}

void IN_DrawStatBack(void)
{
    DGL_SetMaterial(P_ToPtr(DMU_MATERIAL, Materials_IndexForName(MN_FLATS_NAME":FLOOR16")));
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(1, 1, 1, 1);
    DGL_DrawRectTiled(0, 0, SCREENWIDTH, SCREENHEIGHT, 64, 64);

    DGL_Disable(DGL_TEXTURE_2D);
}

void IN_DrawOldLevel(void)
{
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(GF_FONTB));
    DGL_Color4f(defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    FR_DrawTextFragment2(P_GetShortMapName(wbs->episode, wbs->currentMap), 160, 3, DTF_ALIGN_TOP|DTF_NO_TYPEIN);

    FR_SetFont(FID(GF_FONTA));
    DGL_Color4f(defFontRGB2[0], defFontRGB2[1],defFontRGB2[2], 1);
    FR_DrawTextFragment2("FINISHED", 160, 25, DTF_ALIGN_TOP|DTF_NO_TYPEIN);

    if(wbs->currentMap == 8)
    {
        uint i;
        DGL_Color4f(1, 1, 1, 1);
        for(i = 0; i < wbs->nextMap; ++i)
        {
            GL_DrawPatch(dpBeenThere.id, YAHspot[wbs->episode][i].x, YAHspot[wbs->episode][i].y);
        }

        if(!(interTime & 16))
        {
            GL_DrawPatch(dpBeenThere.id, YAHspot[wbs->episode][8].x, YAHspot[wbs->episode][8].y);
        }
    }
    else
    {
        uint i;
        DGL_Color4f(1, 1, 1, 1);
        for(i = 0; i < wbs->currentMap; ++i)
        {
            GL_DrawPatch(dpBeenThere.id, YAHspot[wbs->episode][i].x, YAHspot[wbs->episode][i].y);
        }

        if(players[CONSOLEPLAYER].didSecret)
        {
            GL_DrawPatch(dpBeenThere.id, YAHspot[wbs->episode][8].x, YAHspot[wbs->episode][8].y);
        }

        if(!(interTime & 16))
        {
            GL_DrawPatch(dpBeenThere.id, YAHspot[wbs->episode][wbs->currentMap].x, YAHspot[wbs->episode][wbs->currentMap].y);
        }
    }

    DGL_Disable(DGL_TEXTURE_2D);
}

void IN_DrawYAH(void)
{
    uint i;

    FR_SetFont(FID(GF_FONTA));
    DGL_Color4f(defFontRGB2[0], defFontRGB2[1], defFontRGB2[2], 1);
    FR_DrawTextFragment2("NOW ENTERING:", 160, 10, DTF_ALIGN_TOP|DTF_NO_TYPEIN);

    FR_SetFont(FID(GF_FONTB));
    DGL_Color4f(defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
    FR_DrawTextFragment2(P_GetShortMapName(wbs->episode, wbs->nextMap), 160, 20, DTF_ALIGN_TOP|DTF_NO_TYPEIN);

    DGL_Color4f(1, 1, 1, 1);
    for(i = 0; i < wbs->nextMap; ++i)
    {
        GL_DrawPatch(dpBeenThere.id, YAHspot[wbs->episode][i].x, YAHspot[wbs->episode][i].y);
    }

    if(players[CONSOLEPLAYER].didSecret)
    {
        GL_DrawPatch(dpBeenThere.id, YAHspot[wbs->episode][8].x, YAHspot[wbs->episode][8].y);
    }

    if(!(interTime & 16) || interState == 3)
    {   // Draw the destination 'X'
        GL_DrawPatch(dpGoingThere.id, YAHspot[wbs->episode][wbs->nextMap].x, YAHspot[wbs->episode][wbs->nextMap].y);
    }
}

void IN_DrawSingleStats(void)
{
#define TRACKING                (1)

    static int sounds;
    char buf[20];

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(GF_FONTB));
    DGL_Color4f(defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    FR_DrawTextFragment("KILLS", 50, 65);
    FR_DrawTextFragment("ITEMS", 50, 90);
    FR_DrawTextFragment("SECRETS", 50, 115);
    FR_DrawTextFragment2(P_GetShortMapName(wbs->episode, wbs->currentMap), 160, 3, DTF_ALIGN_TOP|DTF_NO_TYPEIN);

    FR_SetFont(FID(GF_FONTA));
    DGL_Color4f(defFontRGB2[0], defFontRGB2[1], defFontRGB2[2], 1);

    FR_DrawTextFragment2("FINISHED", 160, 25, DTF_ALIGN_TOP|DTF_NO_TYPEIN);

    DGL_Disable(DGL_TEXTURE_2D);

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

    DGL_Enable(DGL_TEXTURE_2D);

    dd_snprintf(buf, 20, "%i", players[CONSOLEPLAYER].killCount);
    M_DrawTextFragmentShadowed(buf, 236, 65, GF_FONTB, DTF_ALIGN_TOPRIGHT, TRACKING, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    M_DrawTextFragmentShadowed("/", 241, 65, GF_FONTB, DTF_ALIGN_TOPLEFT, TRACKING, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    dd_snprintf(buf, 20, "%i", totalKills);
    M_DrawTextFragmentShadowed(buf, 284, 65, GF_FONTB, DTF_ALIGN_TOPRIGHT, TRACKING, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    DGL_Disable(DGL_TEXTURE_2D);

    if(interTime < 60)
        return;

    if(sounds < 2 && interTime >= 60)
    {
        S_LocalSound(SFX_DORCLS, NULL);
        sounds++;
    }

    DGL_Enable(DGL_TEXTURE_2D);

    dd_snprintf(buf, 20, "%i", players[CONSOLEPLAYER].itemCount);
    M_DrawTextFragmentShadowed(buf, 236, 90, GF_FONTB, DTF_ALIGN_TOPRIGHT, TRACKING, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    M_DrawTextFragmentShadowed("/", 241, 90, GF_FONTB, DTF_ALIGN_TOPLEFT, TRACKING, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    dd_snprintf(buf, 20, "%i", totalItems);
    M_DrawTextFragmentShadowed(buf, 284, 90, GF_FONTB, DTF_ALIGN_TOPRIGHT, TRACKING, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    DGL_Disable(DGL_TEXTURE_2D);

    if(interTime < 90)
        return;

    if(sounds < 3 && interTime >= 90)
    {
        S_LocalSound(SFX_DORCLS, NULL);
        sounds++;
    }

    DGL_Enable(DGL_TEXTURE_2D);

    dd_snprintf(buf, 20, "%i", players[CONSOLEPLAYER].secretCount);
    M_DrawTextFragmentShadowed(buf, 236, 115, GF_FONTB, DTF_ALIGN_TOPRIGHT, TRACKING, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    M_DrawTextFragmentShadowed("/", 241, 115, GF_FONTB, DTF_ALIGN_TOPLEFT, TRACKING, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    dd_snprintf(buf, 20, "%i", totalSecret);
    M_DrawTextFragmentShadowed(buf, 284, 115, GF_FONTB, DTF_ALIGN_TOPRIGHT, TRACKING, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    DGL_Disable(DGL_TEXTURE_2D);

    if(interTime < 150)
    {
        return;
    }

    if(sounds < 4 && interTime >= 150)
    {
        S_LocalSound(SFX_DORCLS, NULL);
        sounds++;
    }

    if(gameMode != heretic_extended || wbs->episode < 3)
    {
        DGL_Enable(DGL_TEXTURE_2D);

        FR_SetFont(FID(GF_FONTB));
        DGL_Color4f(defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
        FR_DrawTextFragment("TIME", 85, 160);

        IN_DrawTime(284, 160, hours, minutes, seconds, GF_FONTB, TRACKING, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

        DGL_Disable(DGL_TEXTURE_2D);
    }
    else
    {
        DGL_Enable(DGL_TEXTURE_2D);

        FR_SetFont(FID(GF_FONTA));
        DGL_Color4f(defFontRGB2[0], defFontRGB2[1], defFontRGB2[2], 1);
        FR_DrawTextFragment2("NOW ENTERING:", SCREENWIDTH/2, 160, DTF_ALIGN_TOP|DTF_NO_TYPEIN);

        FR_SetFont(FID(GF_FONTB));
        DGL_Color4f(defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
        FR_DrawTextFragment2(P_GetShortMapName(wbs->episode, wbs->nextMap), 160, 170, DTF_ALIGN_TOP|DTF_NO_TYPEIN);

        DGL_Disable(DGL_TEXTURE_2D);

        skipIntermission = false;
    }

#undef TRACKING
}

void IN_DrawCoopStats(void)
{
    static int sounds;

    int i, ypos;

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(GF_FONTB));
    DGL_Color4f(defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    FR_DrawTextFragment("KILLS", 95, 35);
    FR_DrawTextFragment("BONUS", 155, 35);
    FR_DrawTextFragment("SECRET", 232, 35);
    FR_DrawTextFragment2(P_GetShortMapName(wbs->episode, wbs->currentMap), SCREENWIDTH/2, 3, DTF_ALIGN_TOP|DTF_NO_TYPEIN);

    FR_SetFont(FID(GF_FONTA));
    DGL_Color4f(defFontRGB2[0], defFontRGB2[1], defFontRGB2[2], 1);
    FR_DrawTextFragment2("FINISHED", SCREENWIDTH/2, 25, DTF_ALIGN_TOP|DTF_NO_TYPEIN);

    DGL_Disable(DGL_TEXTURE_2D);

    ypos = 50;
    for(i = 0; i < NUMTEAMS; ++i)
    {
        if(teamInfo[i].members)
        {
#define TRACKING                    (1)

            char buf[20];

            DGL_Enable(DGL_TEXTURE_2D);

            DGL_Color4f(0, 0, 0, .4f);
            GL_DrawPatch(dpFaceAlive[i].id, 27, ypos+2);

            DGL_Color4f(defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
            GL_DrawPatch(dpFaceAlive[i].id, 25, ypos);

            DGL_Disable(DGL_TEXTURE_2D);

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

            DGL_Enable(DGL_TEXTURE_2D);

            dd_snprintf(buf, 20, "%i", killPercent[i]);
            M_DrawTextFragmentShadowed(buf, 121, ypos + 10, GF_FONTB, DTF_ALIGN_TOPRIGHT, TRACKING, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
            M_DrawTextFragmentShadowed("%", 121, ypos + 10, GF_FONTB, DTF_ALIGN_TOPLEFT, TRACKING, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

            dd_snprintf(buf, 20, "%i", bonusPercent[i]);
            M_DrawTextFragmentShadowed(buf, 196, ypos + 10, GF_FONTB, DTF_ALIGN_TOPRIGHT, TRACKING, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
            M_DrawTextFragmentShadowed("%", 196, ypos + 10, GF_FONTB, DTF_ALIGN_TOPLEFT, TRACKING, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

            dd_snprintf(buf, 20, "%i", secretPercent[i]);
            M_DrawTextFragmentShadowed(buf, 273, ypos + 10, GF_FONTB, DTF_ALIGN_TOPRIGHT, TRACKING, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
            M_DrawTextFragmentShadowed("%", 273, ypos + 10, GF_FONTB, DTF_ALIGN_TOPLEFT, TRACKING, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

            DGL_Disable(DGL_TEXTURE_2D);

            ypos += 37;

#undef TRACKING
        }
    }
}

void IN_DrawDMStats(void)
{
    static int sounds;

    int i, j, ypos = 55, xpos = 90, kpos;

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(GF_FONTB));
    DGL_Color4f(defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
    FR_DrawTextFragment("TOTAL", 265, 30);

    FR_SetFont(FID(GF_FONTA));
    DGL_Color4f(defFontRGB2[0], defFontRGB2[1], defFontRGB2[2], 1);
    FR_DrawTextFragment("VICTIMS", 140, 8);

    for(i = 0; i < 7; ++i)
    {
        FR_DrawTextFragment(killersText[i], 10, 80 + 9 * i);
    }

    DGL_Disable(DGL_TEXTURE_2D);

    if(interTime < 20)
    {
        DGL_Enable(DGL_TEXTURE_2D);

        for(i = 0; i < NUMTEAMS; ++i)
        {
            if(teamInfo[i].members)
            {
                M_DrawShadowedPatch(dpFaceAlive[i].id, 40, ((ypos << FRACBITS) + dSlideY[i] * interTime) >> FRACBITS);
                M_DrawShadowedPatch(dpFaceDead[i].id, ((xpos << FRACBITS) + dSlideX[i] * interTime) >> FRACBITS, 18);
            }
        }

        DGL_Disable(DGL_TEXTURE_2D);

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
#define TRACKING                (1)

            char buf[20];

            DGL_Enable(DGL_TEXTURE_2D);

            if(interTime < 100 || i == playerTeam[CONSOLEPLAYER])
            {
                M_DrawShadowedPatch(dpFaceAlive[i].id, 40, ypos);
                M_DrawShadowedPatch(dpFaceDead[i].id, xpos, 18);
            }
            else
            {
                DGL_Color4f(1, 1, 1, .333f);
                GL_DrawPatch(dpFaceAlive[i].id, 40, ypos);
                GL_DrawPatch(dpFaceDead[i].id, xpos, 18);
            }

            kpos = 122;
            for(j = 0; j < NUMTEAMS; ++j)
            {
                if(teamInfo[j].members)
                {
                    dd_snprintf(buf, 20, "%i", teamInfo[i].frags[j]);
                    M_DrawTextFragmentShadowed(buf, kpos, ypos + 10, GF_FONTB, DTF_ALIGN_TOPRIGHT, TRACKING, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
                    kpos += 43;
                }
            }

            if(slaughterBoy & (1 << i))
            {
                if(!(interTime & 16))
                {
                    dd_snprintf(buf, 20, "%i", teamInfo[i].totalFrags);
                    M_DrawTextFragmentShadowed(buf, 263, ypos + 10, GF_FONTB, DTF_ALIGN_TOPRIGHT, TRACKING, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
                }
            }
            else
            {
                dd_snprintf(buf, 20, "%i", teamInfo[i].totalFrags);
                M_DrawTextFragmentShadowed(buf, 263, ypos + 10, GF_FONTB, DTF_ALIGN_TOPRIGHT, TRACKING, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
            }

            DGL_Disable(DGL_TEXTURE_2D);

            ypos += 36;
            xpos += 43;

#undef TRACKING
        }
    }
}
