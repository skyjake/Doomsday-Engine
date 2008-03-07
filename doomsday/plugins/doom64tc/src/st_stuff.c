/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
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
 * st_stuff.c: Fullscreen HUD code.
 *
 * Does palette indicators as well (red pain/berserk, bright pickup)
 */

 // HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>

#include "doom64tc.h"

#include "d_net.h"
#include "st_lib.h"
#include "hu_stuff.h"
#include "am_map.h"
#include "p_tick.h" // for P_IsPaused

// MACROS ------------------------------------------------------------------

#define FMAKERGBA(r,g,b,a) ( (byte)(0xff*r) + ((byte)(0xff*g)<<8)           \
                            + ((byte)(0xff*b)<<16) + ((byte)(0xff*a)<<24) )

// Radiation suit, green shift.
#define RADIATIONPAL        13

// Frags pos.
#define ST_FRAGSX           138
#define ST_FRAGSY           171
#define ST_FRAGSWIDTH       2

#define HUDBORDERX          20
#define HUDBORDERY          24

// TYPES -------------------------------------------------------------------

typedef enum hotloc_e {
    HOT_TLEFT,
    HOT_TRIGHT,
    HOT_BRIGHT,
    HOT_BLEFT
} hotloc_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void ST_Stop(void);

DEFCC(CCmdHUDShow);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// Whether the HUD is on.
static boolean statusbarActive;
static boolean stopped = true;
static boolean firstTime;

static int hudHideTics;
static float hudHideAmount;

// Fullscreen hud alpha value.
static float hudalpha = 0.0f;
// Whether to use alpha blending.
static boolean blended = false;

// Number of frags so far in deathmatch.
static int currentFragsCount;
static boolean statusbarFragsOn;

static int currentPalette = 0;

// 0-9, tall numbers.
static dpatch_t tallnum[10];

// In deathmatch only, summary of frags stats.
static st_number_t wFrags;

// CVARs for the HUD/Statusbar.
cvar_t sthudCVars[] =
{
    // HUD scale
    {"hud-scale", 0, CVT_FLOAT, &cfg.hudScale, 0.1f, 10},

    // HUD colour + alpha
    {"hud-color-r", 0, CVT_FLOAT, &cfg.hudColor[0], 0, 1},
    {"hud-color-g", 0, CVT_FLOAT, &cfg.hudColor[1], 0, 1},
    {"hud-color-b", 0, CVT_FLOAT, &cfg.hudColor[2], 0, 1},
    {"hud-color-a", 0, CVT_FLOAT, &cfg.hudColor[3], 0, 1},
    {"hud-icon-alpha", 0, CVT_FLOAT, &cfg.hudIconAlpha, 0, 1},

    // HUD icons
    {"hud-health", 0, CVT_BYTE, &cfg.hudShown[HUD_HEALTH], 0, 1},
    {"hud-armor", 0, CVT_BYTE, &cfg.hudShown[HUD_ARMOR], 0, 1},
    {"hud-ammo", 0, CVT_BYTE, &cfg.hudShown[HUD_AMMO], 0, 1},
    {"hud-keys", 0, CVT_BYTE, &cfg.hudShown[HUD_KEYS], 0, 1},
    {"hud-power", 0, CVT_BYTE, &cfg.hudShown[HUD_POWER], 0, 1},

    // HUD displays
    {"hud-frags", 0, CVT_BYTE, &cfg.hudShown[HUD_FRAGS], 0, 1},

    {"hud-frags-all", 0, CVT_BYTE, &huShowAllFrags, 0, 1},

    {"hud-timer", 0, CVT_FLOAT, &cfg.hudTimer, 0, 60},

    {"hud-unhide-damage", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_DAMAGE], 0, 1},
    {"hud-unhide-pickup-health", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_HEALTH], 0, 1},
    {"hud-unhide-pickup-armor", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_ARMOR], 0, 1},
    {"hud-unhide-pickup-powerup", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_POWER], 0, 1},
    {"hud-unhide-pickup-weapon", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_WEAPON], 0, 1},
    {"hud-unhide-pickup-ammo", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_AMMO], 0, 1},
    {"hud-unhide-pickup-key", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_KEY], 0, 1},
    {NULL}
};

// Console commands for the HUD/Status bar
ccmd_t  sthudCCmds[] = {
    {"showhud",     "",     CCmdHUDShow},
    {NULL}
};

// CODE --------------------------------------------------------------------

/**
 * Register CVARs and CCmds for the HUD/Status bar
 */
void ST_Register(void)
{
    uint                i;

    for(i = 0; sthudCVars[i].name; ++i)
        Con_AddVariable(sthudCVars + i);
    for(i = 0; sthudCCmds[i].name; ++i)
        Con_AddCommand(sthudCCmds + i);
}

/**
 * Unhides the current HUD display if hidden.
 *
 * @param event         The HUD Update Event type to check for triggering.
 */
void ST_HUDUnHide(hueevent_t event)
{
    if(event < HUE_FORCE || event > NUMHUDUNHIDEEVENTS)
        return;

    if(event == HUE_FORCE || cfg.hudUnHide[event])
    {
        hudHideTics = (cfg.hudTimer * TICSPERSEC);
        hudHideAmount = 0;
    }
}


void ST_updateWidgets(void)
{
    int                 i;
    player_t            *plr = &players[CONSOLEPLAYER];

    // Used by wFrags widget.
    statusbarFragsOn = deathmatch && statusbarActive;
    currentFragsCount = 0;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->inGame)
            continue;

        currentFragsCount += plr->frags[i] * (i != CONSOLEPLAYER ? 1 : -1);
    }
}

void ST_Ticker(void)
{
    player_t           *plyr = &players[CONSOLEPLAYER];

    if(!P_IsPaused())
    {
        if(cfg.hudTimer == 0)
        {
            hudHideTics = hudHideAmount = 0;
        }
        else
        {
            if(hudHideTics > 0)
                hudHideTics--;
            if(hudHideTics == 0 && cfg.hudTimer > 0 && hudHideAmount < 1)
                hudHideAmount += 0.1f;
        }
    }

    ST_updateWidgets();
}

int R_GetFilterColor(int filter)
{
    int                 rgba = 0;

    // We have to choose the right color and alpha.
    if(filter >= STARTREDPALS && filter < STARTREDPALS + NUMREDPALS)
        // Red.
        rgba = FMAKERGBA(1, 0, 0, filter / 9.0);
    else if(filter >= STARTBONUSPALS && filter < STARTBONUSPALS + NUMBONUSPALS)
        // Gold.
        rgba = FMAKERGBA(1, .8, .5, (filter - STARTBONUSPALS + 1) / 16.0);
    else if(filter == RADIATIONPAL)
        // Green.
        rgba = FMAKERGBA(0, .7, 0, .15f);
    else if(filter)
        Con_Error("R_GetFilterColor: Real strange filter number: %d.\n", filter);

    return rgba;
}

void ST_doPaletteStuff(void)
{
    int                 palette;
    int                 cnt;
    int                 bzc;
    player_t           *plyr = &players[CONSOLEPLAYER];

    cnt = plyr->damageCount;

    if(plyr->powers[PT_STRENGTH])
    {
        // Slowly fade the berzerk out.
        bzc = 12 - (plyr->powers[PT_STRENGTH] >> 6);

        if(bzc > cnt)
            cnt = bzc;
    }

    if(cnt)
    {
        palette = (cnt + 7) >> 3;

        if(palette >= NUMREDPALS)
            palette = NUMREDPALS - 1;

        palette += STARTREDPALS;
    }

    else if(plyr->bonusCount)
    {
        palette = (plyr->bonusCount + 7) >> 3;

        if(palette >= NUMBONUSPALS)
            palette = NUMBONUSPALS - 1;

        palette += STARTBONUSPALS;
    }

    else if(plyr->powers[PT_IRONFEET] > 4 * 32 ||
            plyr->powers[PT_IRONFEET] & 8)
        palette = RADIATIONPAL;
    else
        palette = 0;

    if(palette != currentPalette)
    {
        currentPalette = palette;
        plyr->plr->filter = R_GetFilterColor(palette); // $democam
    }
}

void ST_drawWidgets(boolean refresh)
{
    // Used by wFrags widget.
    statusbarFragsOn = deathmatch && statusbarActive;

    STlib_updateNum(&wFrags, refresh);
}

void ST_doRefresh(void)
{
    firstTime = false;

    // And refresh all widgets.
    ST_drawWidgets(true);
}

void ST_HUDSpriteSize(int sprite, int *w, int *h)
{
    spriteinfo_t        sprInfo;

    R_GetSpriteInfo(sprite, 0, &sprInfo);
    *w = sprInfo.width;
    *h = sprInfo.height;

    if(sprite == SPR_ROCK)
    {
        // Must scale it a bit.
        *w /= 1.5;
        *h /= 1.5;
    }
}

void ST_drawHUDSprite(int sprite, int x, int y, int hotspot, float alpha)
{
    int                 w, h;
    spriteinfo_t        sprInfo;

    CLAMP(alpha, 0.0f, 1.0f);
    if(alpha == 0.0f)
        return;

    R_GetSpriteInfo(sprite, 0, &sprInfo);
    w = sprInfo.width;
    h = sprInfo.height;
    if(sprite == SPR_ROCK)
    {
        w /= 1.5;
        h /= 1.5;
    }
    switch (hotspot)
    {
    case HOT_BRIGHT:
        y -= h;

    case HOT_TRIGHT:
        x -= w;
        break;

    case HOT_BLEFT:
        y -= h;
        break;
    }
    DGL_Color4f(1, 1, 1, alpha );
    GL_DrawPSprite(x, y, sprite == SPR_ROCK ? 1 / 1.5 : 1, false,
                   sprInfo.lump);
}

void ST_doFullscreenStuff(void)
{
    static const int    ammo_sprite[NUM_AMMO_TYPES] = {
        SPR_AMMO,
        SPR_SBOX,
        SPR_CELL,
        SPR_ROCK
    };

    player_t           *plr = &players[DISPLAYPLAYER];
    char                buf[20];
    int                 w, h, pos = 0, spr, i;
    int                 h_width = 320 / cfg.hudScale;
    int                 h_height = 200 / cfg.hudScale;
    float               textalpha =
        hudalpha - hudHideAmount - ( 1 - cfg.hudColor[3]);
    float               iconalpha =
        hudalpha - hudHideAmount - ( 1 - cfg.hudIconAlpha);

    CLAMP(textalpha, 0.0f, 1.0f);
    CLAMP(iconalpha, 0.0f, 1.0f);

    if(IS_NETGAME && deathmatch && cfg.hudShown[HUD_FRAGS])
    {
        // Display the frag counter.
        i = 199 - HUDBORDERY;
        if(cfg.hudShown[HUD_HEALTH])
        {
            i -= 18 * cfg.hudScale;
        }
        sprintf(buf, "FRAGS:%i", currentFragsCount);
        M_WriteText2(HUDBORDERX, i, buf, huFontA, cfg.hudColor[0], cfg.hudColor[1],
                     cfg.hudColor[2], textalpha);
    }

    // Setup the scaling matrix.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);

    // Draw the visible HUD data, first health.
    if(cfg.hudShown[HUD_HEALTH])
    {
        sprintf(buf, "HEALTH");
        pos = M_StringWidth(buf, huFontA)/2;
        M_WriteText2(HUDBORDERX, h_height - HUDBORDERY - huFont[0].height - 4,
                     buf, huFontA, 1, 1, 1, iconalpha);

        sprintf(buf, "%i", plr->health);
        M_WriteText2(HUDBORDERX + pos - (M_StringWidth(buf, huFontB)/2),
                     h_height - HUDBORDERY, buf, huFontB,
                     cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textalpha);
    }

    // d64tc > Laser artifacts
    if(cfg.hudShown[HUD_POWER])
    {
        if(plr->laserIcon1 == 1)
        {
            spr = SPR_POW1;
            ST_HUDSpriteSize(spr, &w, &h);
            pos -= w/2;
            ST_drawHUDSprite(spr, HUDBORDERX + pos, h_height - 44,
                             HOT_BLEFT, iconalpha);
        }
        if(plr->laserIcon2 == 1)
        {
            spr = SPR_POW2;
            ST_HUDSpriteSize(spr, &w, &h);
            ST_drawHUDSprite(spr, HUDBORDERX + pos, h_height - 84,
                             HOT_BLEFT, iconalpha);
        }
        if(plr->laserIcon3 == 1)
        {
            spr = SPR_POW3;
            ST_HUDSpriteSize(spr, &w, &h);
            ST_drawHUDSprite(spr, HUDBORDERX + pos, h_height - 124,
                             HOT_BLEFT, iconalpha);
        }

        if(plr->artifacts[it_helltime])
        {
            ST_drawHUDSprite(SPR_POW5, HUDBORDERX + pos, h_height - 164,
                             HOT_BLEFT, iconalpha);
            ST_HUDSpriteSize(SPR_POW5, &w, &h);
            if(plr->hellTime)
            {
                for(i = 0; i < plr->hellTime; ++i)
                {
                    ST_drawHUDSprite(SPR_STHT, HUDBORDERX + 48 + i,
                                     h_height - 44, HOT_BLEFT, iconalpha);
                }
            }
        }

        if(plr->artifacts[it_float])
        {
            ST_drawHUDSprite(SPR_POW4, HUDBORDERX, h_height - 184, HOT_BLEFT, iconalpha);
            ST_HUDSpriteSize(SPR_POW4, &w, &h);

            if(plr->deviceTime &&
               ((plr->outcastCycle == 2 && plr->artifacts[it_float]) ||
                (plr->outcastCycle == 1 && plr->artifacts[it_float] &&
                 !(plr->artifacts[it_helltime]))))
            {
                for(i = 0; i < plr->deviceTime; ++i)
                {
                    ST_drawHUDSprite(SPR_STDT, HUDBORDERX + 48 + i, h_height - 32,
                                     HOT_BLEFT, iconalpha);
                }
            }
        }
    }
    // < d64tc

    if(cfg.hudShown[HUD_AMMO])
    {
        ammotype_t ammotype;

        //// \todo Only supports one type of ammo per weapon.
        //// for each type of ammo this weapon takes.
        for(ammotype=0; ammotype < NUM_AMMO_TYPES; ++ammotype)
        {
            if(!weaponInfo[plr->readyWeapon][plr->class].mode[0].ammoType[ammotype])
                continue;

            sprintf(buf, "%i", plr->ammo[ammotype]);
            pos = (h_width/2) - (M_StringWidth(buf, huFontB)/2);
            M_WriteText2(pos, h_height - HUDBORDERY, buf, huFontB,
                         cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textalpha);
            break;
        }
    }

    pos = h_width - 1;
    if(cfg.hudShown[HUD_ARMOR])
    {
        sprintf(buf, "ARMOR");
        w = M_StringWidth(buf, huFontA);
        M_WriteText2(h_width - w - HUDBORDERX,
                     h_height - HUDBORDERY - huFont[0].height - 4,
                     buf, huFontA, 1, 1, 1, iconalpha);

        sprintf(buf, "%i", plr->armorPoints);
        M_WriteText2(h_width - (w/2) - (M_StringWidth(buf, huFontB)/2) - HUDBORDERX,
                     h_height - HUDBORDERY,
                     buf, huFontB, cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2],
                     textalpha);
        pos = h_width * 0.25;
    }

    // Keys  | use a bit of extra scale.
    if(cfg.hudShown[HUD_KEYS])
    {
Draw_BeginZoom(0.75f, pos , h_height - HUDBORDERY);
        for(i = 0; i < 3; ++i)
        {
            spr = 0;
            if(plr->
               keys[i == 0 ? KT_REDCARD : i ==
                     1 ? KT_YELLOWCARD : KT_BLUECARD])
                spr = i == 0 ? SPR_RKEY : i == 1 ? SPR_YKEY : SPR_BKEY;
            if(plr->
               keys[i == 0 ? KT_REDSKULL : i ==
                     1 ? KT_YELLOWSKULL : KT_BLUESKULL])
                spr = i == 0 ? SPR_RSKU : i == 1 ? SPR_YSKU : SPR_BSKU;
            if(spr)
            {
                ST_drawHUDSprite(spr, pos, h_height - 2, HOT_BLEFT, iconalpha);
                ST_HUDSpriteSize(spr, &w, &h);
                pos -= w + 2;
            }
        }
Draw_EndZoom();
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void ST_Drawer(int fullscreenmode, boolean refresh)
{
    firstTime = firstTime || refresh;
    statusbarActive = (fullscreenmode < 2) || (AM_IsMapActive(CONSOLEPLAYER) &&
                     (cfg.automapHudDisplay == 0 || cfg.automapHudDisplay == 2));

    // Do palette shifts.
    ST_doPaletteStuff();

    // Fade in/out the fullscreen HUD.
    if(statusbarActive)
    {
        if(hudalpha > 0.0f)
        {
            statusbarActive = 0;
            hudalpha-=0.1f;
        }
    }
    else
    {
        if(fullscreenmode == 3)
        {
            if(hudalpha > 0.0f)
            {
                hudalpha-=0.1f;
                fullscreenmode = 2;
            }
        }
        else
        {
            if(hudalpha < 1.0f)
                hudalpha+=0.1f;
        }
    }

    // Always try to render statusbar with alpha in fullscreen modes.
    if(fullscreenmode)
        blended = 1;
    else
        blended = 0;

    ST_doFullscreenStuff();
}

void ST_loadGraphics(void)
{
    int                 i;
    char                namebuf[9];

    // Load the numbers, tall and short.
    for(i = 0; i < 10; ++i)
    {
        sprintf(namebuf, "STTNUM%d", i);
        R_CachePatch(&tallnum[i], namebuf);
    }
}

void ST_updateGraphics(void)
{
    // Nothing to do.
}

void ST_loadData(void)
{
    ST_loadGraphics();
}

void ST_initData(void)
{
    player_t           *plyr = &players[CONSOLEPLAYER];

    firstTime = true;
    statusbarActive = true;
    currentPalette = -1;
    STlib_init();

    ST_HUDUnHide(HUE_FORCE);
}

void ST_createWidgets(void)
{
    // Frags sum.
    STlib_initNum(&wFrags, ST_FRAGSX, ST_FRAGSY, tallnum, &currentFragsCount,
                  &statusbarFragsOn, ST_FRAGSWIDTH, &hudalpha);
}

void ST_Start(void)
{
    if(!stopped)
        ST_Stop();

    ST_initData();
    ST_createWidgets();
    stopped = false;
}

void ST_Stop(void)
{
    if(stopped)
        return;
    stopped = true;
}

void ST_Init(void)
{
    ST_loadData();
}

/**
 * Console command to show the hud if hidden.
 */
DEFCC(CCmdHUDShow)
{
    ST_HUDUnHide(HUE_FORCE);
    return true;
}
