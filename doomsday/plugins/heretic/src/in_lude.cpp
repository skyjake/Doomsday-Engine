/** @file in_lude.cpp  Heretic specific intermission screens.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#include "jheretic.h"
#include "in_lude.h"

#include <cstring>
#include "am_map.h"
#include "d_net.h"
#include "gamesession.h"
#include "hu_stuff.h"
#include "p_mapsetup.h"
#include "p_tick.h"

enum gametype_t
{
    SINGLE,
    COOPERATIVE,
    DEATHMATCH
};

struct teaminfo_t
{
    int members;
    int frags[NUMTEAMS];
    int totalFrags;
};

void IN_DrawOldLevel();
void IN_DrawYAH();
void IN_DrawStatBack();
void IN_DrawSingleStats();
void IN_DrawCoopStats();
void IN_DrawDMStats();

dd_bool intermission;

int interState;
int interTime = -1;

// Used for timing of background animation.
static int bcnt;

// Contains information passed into intermission.
static wbstartstruct_t const *wbs;

static dd_bool skipIntermission;

static int oldInterTime;
static gametype_t gameType;

static int cnt;

static int hours;
static int minutes;
static int seconds;

static int slaughterBoy; // In DM, the player with the most kills.

static int killPercent[NUMTEAMS];
static int bonusPercent[NUMTEAMS];
static int secretPercent[NUMTEAMS];

static int playerTeam[MAXPLAYERS];
static teaminfo_t teamInfo[NUMTEAMS];

static patchid_t dpInterPic;
static patchid_t dpBeenThere;
static patchid_t dpGoingThere;
static patchid_t dpFaceAlive[NUMTEAMS];
static patchid_t dpFaceDead[NUMTEAMS];

static fixed_t dSlideX[NUMTEAMS];
static fixed_t dSlideY[NUMTEAMS];

static char const *killersText = "KILLERS";

static Point2Raw YAHspot[3][9] = {
    {   // Episode 0
        Point2Raw(172, 78),
        Point2Raw(86, 90),
        Point2Raw(73, 66),
        Point2Raw(159, 95),
        Point2Raw(148, 126),
        Point2Raw(132, 54),
        Point2Raw(131, 74),
        Point2Raw(208, 138),
        Point2Raw(52, 10)
    },
    {   // Episode 1
        Point2Raw(218, 57),
        Point2Raw(137, 81),
        Point2Raw(155, 124),
        Point2Raw(171, 68),
        Point2Raw(250, 86),
        Point2Raw(136, 98),
        Point2Raw(203, 90),
        Point2Raw(220, 140),
        Point2Raw(279, 106)
    },
    {   // Episode 2
        Point2Raw(86, 99),
        Point2Raw(124, 103),
        Point2Raw(154, 79),
        Point2Raw(202, 83),
        Point2Raw(178, 59),
        Point2Raw(142, 58),
        Point2Raw(219, 66),
        Point2Raw(247, 57),
        Point2Raw(107, 80)
     }
};

void WI_Register()
{
    C_VAR_BYTE("inlude-stretch",           &cfg.inludeScaleMode, 0, SCALEMODE_FIRST, SCALEMODE_LAST);
    C_VAR_INT ("inlude-patch-replacement", &cfg.inludePatchReplaceMode, 0, 0, 1);
}

void IN_DrawTime(int x, int y, int h, int m, int s, float r, float g, float b, float a)
{
    char buf[20];

    dd_snprintf(buf, 20, "%02d", s);
    M_DrawTextFragmentShadowed(buf, x, y, ALIGN_TOPRIGHT, 0, r, g, b, a);
    x -= FR_TextWidth(buf) + FR_Tracking() * 3;
    M_DrawTextFragmentShadowed(":", x, y, ALIGN_TOPRIGHT, 0, r, g, b, a);
    x -= FR_CharWidth(':') + 3;

    if(m || h)
    {
        dd_snprintf(buf, 20, "%02d", m);
        M_DrawTextFragmentShadowed(buf, x, y, ALIGN_TOPRIGHT, 0, r, g, b, a);
        x -= FR_TextWidth(buf) + FR_Tracking() * 3;
    }

    if(h)
    {
        dd_snprintf(buf, 20, "%02d", h);
        M_DrawTextFragmentShadowed(":", x, y, ALIGN_TOPRIGHT, 0, r, g, b, a);
        x -= FR_CharWidth(':') + FR_Tracking() * 3;
        M_DrawTextFragmentShadowed(buf, x, y, ALIGN_TOPRIGHT, 0, r, g, b, a);
    }
}

void WI_initVariables(wbstartstruct_t const &wbstartstruct)
{
    wbs = &wbstartstruct;

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

    intermission     = true;
    interState       = -1;
    skipIntermission = false;
    interTime        = 0;
    oldInterTime     = 0;
}

void IN_Init(wbstartstruct_t const &wbstartstruct)
{
    WI_initVariables(wbstartstruct);
    IN_LoadPics();

    IN_InitStats();
}

void IN_WaitStop()
{
    if(!--cnt)
    {
        IN_Stop();
        G_IntermissionDone();
    }
}

void IN_Stop()
{
    NetSv_Intermission(IMF_END, 0, 0);

    intermission = false;
    IN_UnloadPics();
}

/**
 * Initializes the stats for single player mode
 */
void IN_InitStats()
{
    // Init team info.
    if(IS_NETGAME)
    {
        std::memset(teamInfo,   0, sizeof(teamInfo));
        std::memset(playerTeam, 0, sizeof(playerTeam));

        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            if(!players[i].plr->inGame)
                continue;

            playerTeam[i] = cfg.playerColor[i];
            teamInfo[playerTeam[i]].members++;
        }
    }

    int time = mapTime / 35;
    int const hours   = time / 3600; time -= hours * 3600;
    int const minutes = time / 60;   time -= minutes * 60;
    //int const seconds = time;

    if(!IS_NETGAME)
    {
        gameType = SINGLE;
    }
    else if( /*IS_NETGAME && */ !COMMON_GAMESESSION->rules().deathmatch)
    {
        gameType = COOPERATIVE;

        std::memset(killPercent,   0, sizeof(killPercent));
        std::memset(bonusPercent,  0, sizeof(bonusPercent));
        std::memset(secretPercent, 0, sizeof(secretPercent));

        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            if(players[i].plr->inGame)
            {
                if(totalKills)
                {
                    int const percent = players[i].killCount * 100 / totalKills;
                    if(percent > killPercent[playerTeam[i]])
                        killPercent[playerTeam[i]] = percent;
                }

                if(totalItems)
                {
                    int const percent = players[i].itemCount * 100 / totalItems;
                    if(percent > bonusPercent[playerTeam[i]])
                        bonusPercent[playerTeam[i]] = percent;
                }

                if(totalSecret)
                {
                    int const percent = players[i].secretCount * 100 / totalSecret;
                    if(percent > secretPercent[playerTeam[i]])
                        secretPercent[playerTeam[i]] = percent;
                }
            }
        }
    }
    else
    {
        gameType = DEATHMATCH;
        slaughterBoy = 0;

        int slaughterfrags = -9999;
        int posNum         = 0;
        int teamCount      = 0;
        int slaughterCount = 0;

        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            int const team = playerTeam[i];

            if(players[i].plr->inGame)
            {
                for(int percent = 0; percent < MAXPLAYERS; ++percent)
                {
                    if(players[percent].plr->inGame)
                    {
                        teamInfo[team].frags[playerTeam[percent]] += players[i].frags[percent];
                        teamInfo[team].totalFrags                 += players[i].frags[percent];
                    }
                }

                // Find out the largest number of frags.
                if(teamInfo[team].totalFrags > slaughterfrags)
                    slaughterfrags = teamInfo[team].totalFrags;
            }
        }

        for(int i = 0; i < NUMTEAMS; ++i)
        {
            //posNum++;
            /*if(teamInfo[i].totalFrags > slaughterfrags)
            {
               slaughterBoy = 1<<i;
               slaughterfrags = teamInfo[i].totalFrags;
               slaughterCount = 1;
            }
            else */ if(!teamInfo[i].members)
            {
                continue;
            }

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

void IN_LoadPics()
{
    if(::gameEpisode < 3)
    {
        dpInterPic = R_DeclarePatch(::gameEpisode == 0? "MAPE1" : ::gameEpisode == 1? "MAPE2" : "MAPE3");
    }

    dpBeenThere  = R_DeclarePatch("IN_X");
    dpGoingThere = R_DeclarePatch("IN_YAH");

    char buf[9];
    for(int i = 0; i < NUMTEAMS; ++i)
    {
        dd_snprintf(buf, 9, "FACEA%i", i);
        dpFaceAlive[i] = R_DeclarePatch(buf);
        dd_snprintf(buf, 9, "FACEB%i", i);
        dpFaceDead[i] = R_DeclarePatch(buf);
    }
}

void IN_UnloadPics()
{
    // Nothing to do.
}

static void nextIntermissionState()
{
    if(interState == 2)
    {
        // Prepare for busy mode.
        BusyMode_FreezeGameForBusyMode();
    }
    interState++;
}

static void endIntermissionGoToNextLevel()
{
    BusyMode_FreezeGameForBusyMode();
    interState = 3;
}

void IN_Ticker()
{
    if(!intermission) return;

    if(!IS_CLIENT)
    {
        if(interState == 3)
        {
            IN_WaitStop();
            return;
        }
    }
    IN_CheckForSkip();

    // Counter for general background animation.
    bcnt++;

    interTime++;
    if(oldInterTime < interTime)
    {
        nextIntermissionState();

        if(::gameEpisode > 2 && interState >= 1)
        {
            // Extended Wad levels:  skip directly to the next level
            endIntermissionGoToNextLevel();
        }

        switch(interState)
        {
        case 0:
            oldInterTime = interTime + 300;
            if(::gameEpisode > 2)
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
        if(interState < 2 && ::gameEpisode < 3)
        {
            interState = 2;
            skipIntermission = false;
            S_StartSound(SFX_DORCLS, NULL);
            NetSv_Intermission(IMF_STATE, interState, 0);
            return;
        }

        endIntermissionGoToNextLevel();
        cnt = 10;
        skipIntermission = false;
        S_StartSound(SFX_DORCLS, NULL);
        NetSv_Intermission(IMF_STATE, interState, 0);
    }
}

void IN_SkipToNext()
{
    skipIntermission = 1;
}

/**
 * Check to see if any player hit a key.
 */
void IN_CheckForSkip()
{
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
}

void IN_Drawer()
{
    static int oldInterState;

    if(!intermission || interState > 3)
    {
        return;
    }
    /*if(interState == 3)
    {
        return;
    }*/

    if(oldInterState != 2 && interState == 2)
    {
        S_LocalSound(SFX_PSTOP, NULL);
    }

    if(interState != -1)
        oldInterState = interState;

    dgl_borderedprojectionstate_t bp;
    GL_ConfigureBorderedProjection(&bp, BPF_OVERDRAW_MASK|BPF_OVERDRAW_CLIP, SCREENWIDTH, SCREENHEIGHT,
                                   Get(DD_WINDOW_WIDTH), Get(DD_WINDOW_HEIGHT), scalemode_t(cfg.inludeScaleMode));
    GL_BeginBorderedProjection(&bp);

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
        if(::gameEpisode < 3)
        {
            DGL_Enable(DGL_TEXTURE_2D);

            DGL_Color4f(1, 1, 1, 1);
            GL_DrawPatchXY(dpInterPic, 0, 0);

            DGL_Disable(DGL_TEXTURE_2D);

            IN_DrawOldLevel();
        }
        break;

    case 2: // Going to the next level.
        if(::gameEpisode < 3)
        {
            DGL_Enable(DGL_TEXTURE_2D);

            DGL_Color4f(1, 1, 1, 1);
            GL_DrawPatchXY(dpInterPic, 0, 0);
            IN_DrawYAH();

            DGL_Disable(DGL_TEXTURE_2D);
        }
        break;

    case 3: // Waiting before going to the next level.
        if(::gameEpisode < 3)
        {
            DGL_Enable(DGL_TEXTURE_2D);

            DGL_Color4f(1, 1, 1, 1);
            GL_DrawPatchXY(dpInterPic, 0, 0);

            DGL_Disable(DGL_TEXTURE_2D);
        }
        break;

    default:
        DENG2_ASSERT(!"IN_Drawer: Unknown intermission state");
        break;
    }

    GL_EndBorderedProjection(&bp);
}

void IN_DrawStatBack()
{
    DGL_SetMaterialUI((Material *)P_ToPtr(DMU_MATERIAL, Materials_ResolveUriCString("Flats:FLOOR16")), DGL_REPEAT, DGL_REPEAT);
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(1, 1, 1, 1);
    DGL_DrawRectf2Tiled(0, 0, SCREENWIDTH, SCREENHEIGHT, 64, 64);

    DGL_Disable(DGL_TEXTURE_2D);
}

void IN_DrawOldLevel()
{
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(GF_FONTB));
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    FR_DrawTextXY3(G_MapTitle(&wbs->currentMap).toUtf8().constData(), 160, 3, ALIGN_TOP, DTF_ONLY_SHADOW);

    FR_SetFont(FID(GF_FONTA));
    FR_SetColor(defFontRGB3[0], defFontRGB3[1],defFontRGB3[2]);
    FR_DrawTextXY3("FINISHED", 160, 25, ALIGN_TOP, DTF_ONLY_SHADOW);

    if(wbs->currentMap.path() == "E1M9" ||
       wbs->currentMap.path() == "E2M9" ||
       wbs->currentMap.path() == "E3M9" ||
       wbs->currentMap.path() == "E4M9" ||
       wbs->currentMap.path() == "E5M9")
    {
        DGL_Color4f(1, 1, 1, 1);
        uint const nextMap = G_MapNumberFor(wbs->nextMap);
        for(uint i = 0; i < nextMap; ++i)
        {
            GL_DrawPatch(dpBeenThere, &YAHspot[::gameEpisode][i]);
        }

        if(!(interTime & 16))
        {
            GL_DrawPatch(dpBeenThere, &YAHspot[::gameEpisode][8]);
        }
    }
    else
    {
        DGL_Color4f(1, 1, 1, 1);
        uint const currentMap = G_MapNumberFor(wbs->currentMap);
        for(uint i = 0; i < currentMap; ++i)
        {
            GL_DrawPatch(dpBeenThere, &YAHspot[::gameEpisode][i]);
        }

        if(players[CONSOLEPLAYER].didSecret)
        {
            GL_DrawPatch(dpBeenThere, &YAHspot[::gameEpisode][8]);
        }

        if(!(interTime & 16))
        {
            GL_DrawPatch(dpBeenThere, &YAHspot[::gameEpisode][currentMap]);
        }
    }

    DGL_Disable(DGL_TEXTURE_2D);
}

void IN_DrawYAH()
{
    FR_SetFont(FID(GF_FONTA));
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(defFontRGB3[0], defFontRGB3[1], defFontRGB3[2], 1);
    FR_DrawTextXY3("NOW ENTERING:", 160, 10, ALIGN_TOP, DTF_ONLY_SHADOW);

    FR_SetFont(FID(GF_FONTB));
    FR_SetColor(defFontRGB[0], defFontRGB[1], defFontRGB[2]);
    FR_DrawTextXY3(G_MapTitle(&wbs->nextMap).toUtf8().constData(), 160, 20, ALIGN_TOP, DTF_ONLY_SHADOW);

    DGL_Color4f(1, 1, 1, 1);

    uint const nextMap = G_MapNumberFor(wbs->nextMap);

    for(uint i = 0; i < nextMap; ++i)
    {
        GL_DrawPatch(dpBeenThere, &YAHspot[::gameEpisode][i]);
    }

    if(players[CONSOLEPLAYER].didSecret)
    {
        GL_DrawPatch(dpBeenThere, &YAHspot[::gameEpisode][8]);
    }

    if(!(interTime & 16) || interState == 3)
    {
        // Draw the destination 'X'
        GL_DrawPatch(dpGoingThere, &YAHspot[::gameEpisode][nextMap]);
    }
}

void IN_DrawSingleStats()
{
#define TRACKING                (1)

    static int sounds;

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(GF_FONTB));
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    FR_DrawTextXY3("KILLS", 50, 65, ALIGN_TOPLEFT, DTF_ONLY_SHADOW);
    FR_DrawTextXY3("ITEMS", 50, 90, ALIGN_TOPLEFT, DTF_ONLY_SHADOW);
    FR_DrawTextXY3("SECRETS", 50, 115, ALIGN_TOPLEFT, DTF_ONLY_SHADOW);
    FR_DrawTextXY3(G_MapTitle(&wbs->currentMap).toUtf8().constData(), 160, 3, ALIGN_TOP, DTF_ONLY_SHADOW);

    FR_SetFont(FID(GF_FONTA));
    FR_SetColor(defFontRGB3[0], defFontRGB3[1], defFontRGB3[2]);

    FR_DrawTextXY3("FINISHED", 160, 25, ALIGN_TOP, DTF_ONLY_SHADOW);

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

    char buf[20];
    dd_snprintf(buf, 20, "%i", players[CONSOLEPLAYER].killCount);
    FR_SetFont(FID(GF_FONTB));
    FR_SetTracking(TRACKING);
    M_DrawTextFragmentShadowed(buf, 236, 65, ALIGN_TOPRIGHT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    M_DrawTextFragmentShadowed("/", 241, 65, ALIGN_TOPLEFT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    dd_snprintf(buf, 20, "%i", totalKills);
    M_DrawTextFragmentShadowed(buf, 284, 65, ALIGN_TOPRIGHT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

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
    FR_SetFont(FID(GF_FONTB));
    M_DrawTextFragmentShadowed(buf, 236, 90, ALIGN_TOPRIGHT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    M_DrawTextFragmentShadowed("/", 241, 90, ALIGN_TOPLEFT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    dd_snprintf(buf, 20, "%i", totalItems);
    M_DrawTextFragmentShadowed(buf, 284, 90, ALIGN_TOPRIGHT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

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
    FR_SetFont(FID(GF_FONTB));
    M_DrawTextFragmentShadowed(buf, 236, 115, ALIGN_TOPRIGHT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    M_DrawTextFragmentShadowed("/", 241, 115, ALIGN_TOPLEFT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    dd_snprintf(buf, 20, "%i", totalSecret);
    M_DrawTextFragmentShadowed(buf, 284, 115, ALIGN_TOPRIGHT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

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

    if(gameMode != heretic_extended || ::gameEpisode < 3)
    {
        DGL_Enable(DGL_TEXTURE_2D);

        FR_SetFont(FID(GF_FONTB));
        FR_SetColorAndAlpha(defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
        FR_DrawTextXY3("TIME", 85, 160, ALIGN_TOPLEFT, DTF_ONLY_SHADOW);

        IN_DrawTime(284, 160, hours, minutes, seconds, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

        DGL_Disable(DGL_TEXTURE_2D);
    }
    else
    {
        DGL_Enable(DGL_TEXTURE_2D);

        FR_SetFont(FID(GF_FONTA));
        FR_SetColorAndAlpha(defFontRGB3[0], defFontRGB3[1], defFontRGB3[2], 1);
        FR_DrawTextXY3("NOW ENTERING:", SCREENWIDTH/2, 160, ALIGN_TOP, DTF_ONLY_SHADOW);

        FR_SetFont(FID(GF_FONTB));
        FR_SetColorAndAlpha(defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
        FR_DrawTextXY3(G_MapTitle(&wbs->nextMap).toUtf8().constData(), 160, 170, ALIGN_TOP, DTF_ONLY_SHADOW);

        DGL_Disable(DGL_TEXTURE_2D);

        skipIntermission = false;
    }

#undef TRACKING
}

void IN_DrawCoopStats()
{
#define TRACKING                    (1)

    static int sounds;

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(GF_FONTB));
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

    FR_DrawTextXY3("KILLS", 95, 35, ALIGN_TOPLEFT, DTF_ONLY_SHADOW);
    FR_DrawTextXY3("BONUS", 155, 35, ALIGN_TOPLEFT, DTF_ONLY_SHADOW);
    FR_DrawTextXY3("SECRET", 232, 35, ALIGN_TOPLEFT, DTF_ONLY_SHADOW);
    FR_DrawTextXY3(G_MapTitle(&wbs->currentMap).toUtf8().constData(), SCREENWIDTH/2, 3, ALIGN_TOP, DTF_ONLY_SHADOW);

    FR_SetFont(FID(GF_FONTA));
    FR_SetColor(defFontRGB3[0], defFontRGB3[1], defFontRGB3[2]);
    FR_DrawTextXY3("FINISHED", SCREENWIDTH/2, 25, ALIGN_TOP, DTF_ONLY_SHADOW);

    FR_SetFont(FID(GF_FONTB));
    FR_SetTracking(TRACKING);

    int ypos = 50;
    for(int i = 0; i < NUMTEAMS; ++i)
    {
        if(teamInfo[i].members)
        {
            DGL_Color4f(0, 0, 0, .4f);
            GL_DrawPatchXY(dpFaceAlive[i], 27, ypos+2);

            DGL_Color4f(defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
            GL_DrawPatchXY(dpFaceAlive[i], 25, ypos);

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

            char buf[20];
            dd_snprintf(buf, 20, "%i", killPercent[i]);
            M_DrawTextFragmentShadowed(buf, 121, ypos + 10, ALIGN_TOPRIGHT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
            M_DrawTextFragmentShadowed("%", 121, ypos + 10, ALIGN_TOPLEFT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

            dd_snprintf(buf, 20, "%i", bonusPercent[i]);
            M_DrawTextFragmentShadowed(buf, 196, ypos + 10, ALIGN_TOPRIGHT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
            M_DrawTextFragmentShadowed("%", 196, ypos + 10, ALIGN_TOPLEFT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);

            dd_snprintf(buf, 20, "%i", secretPercent[i]);
            M_DrawTextFragmentShadowed(buf, 273, ypos + 10, ALIGN_TOPRIGHT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
            M_DrawTextFragmentShadowed("%", 273, ypos + 10, ALIGN_TOPLEFT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
            ypos += 37;
        }
    }

    DGL_Disable(DGL_TEXTURE_2D);

#undef TRACKING
}

void IN_DrawDMStats()
{
#define TRACKING                (1)

    static int sounds;

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(GF_FONTB));
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
    FR_DrawTextXY3("TOTAL", 265, 30, ALIGN_TOPLEFT, DTF_ONLY_SHADOW);

    FR_SetFont(FID(GF_FONTA));
    FR_SetColor(defFontRGB3[0], defFontRGB3[1], defFontRGB3[2]);
    FR_DrawTextXY3("VICTIMS", 140, 8, ALIGN_TOPLEFT, DTF_ONLY_SHADOW);

    for(int i = 0; i < 7; ++i)
    {
        FR_DrawCharXY3(killersText[i], 10, 80 + 9 * i, ALIGN_TOPLEFT, DTF_ONLY_SHADOW);
    }

    DGL_Disable(DGL_TEXTURE_2D);

    int ypos = 55, xpos = 90;
    if(interTime < 20)
    {
        DGL_Enable(DGL_TEXTURE_2D);

        for(int i = 0; i < NUMTEAMS; ++i)
        {
            if(teamInfo[i].members)
            {
                M_DrawShadowedPatch(dpFaceAlive[i], 40, ((ypos << FRACBITS) + dSlideY[i] * interTime) >> FRACBITS);
                M_DrawShadowedPatch(dpFaceDead[i], ((xpos << FRACBITS) + dSlideX[i] * interTime) >> FRACBITS, 18);
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

    for(int i = 0; i < NUMTEAMS; ++i)
    {
        if(teamInfo[i].members)
        {
            char buf[20];

            DGL_Enable(DGL_TEXTURE_2D);

            if(interTime < 100 || i == playerTeam[CONSOLEPLAYER])
            {
                M_DrawShadowedPatch(dpFaceAlive[i], 40, ypos);
                M_DrawShadowedPatch(dpFaceDead[i], xpos, 18);
            }
            else
            {
                DGL_Color4f(1, 1, 1, .333f);
                GL_DrawPatchXY(dpFaceAlive[i], 40, ypos);
                GL_DrawPatchXY(dpFaceDead[i], xpos, 18);
            }

            FR_SetFont(FID(GF_FONTB));
            FR_SetTracking(TRACKING);
            int kpos = 122;
            for(int k = 0; k < NUMTEAMS; ++k)
            {
                if(teamInfo[k].members)
                {
                    dd_snprintf(buf, 20, "%i", teamInfo[i].frags[k]);
                    M_DrawTextFragmentShadowed(buf, kpos, ypos + 10, ALIGN_TOPRIGHT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
                    kpos += 43;
                }
            }

            if(slaughterBoy & (1 << i))
            {
                if(!(interTime & 16))
                {
                    dd_snprintf(buf, 20, "%i", teamInfo[i].totalFrags);
                    M_DrawTextFragmentShadowed(buf, 263, ypos + 10, ALIGN_TOPRIGHT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
                }
            }
            else
            {
                dd_snprintf(buf, 20, "%i", teamInfo[i].totalFrags);
                M_DrawTextFragmentShadowed(buf, 263, ypos + 10, ALIGN_TOPRIGHT, 0, defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
            }

            DGL_Disable(DGL_TEXTURE_2D);

            ypos += 36;
            xpos += 43;
        }
    }

#undef TRACKING
}
