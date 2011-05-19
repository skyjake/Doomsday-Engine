/**\file st_stuff.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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
 * Fullscreen HUD code.
 *
 * Does palette indicators as well (red pain/berserk, bright pickup)
 */

 // HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>

#include "jdoom64.h"

#include "d_net.h"
#include "hu_stuff.h"
#include "am_map.h"
#include "p_tick.h" // for P_IsPaused
#include "p_inventory.h"

// MACROS ------------------------------------------------------------------

// Radiation suit, green shift.
#define RADIATIONPAL        13

// Frags pos.
#define ST_FRAGSX           138
#define ST_FRAGSY           171
#define ST_FRAGSWIDTH       2

// TYPES -------------------------------------------------------------------

typedef struct {
    boolean         stopped;
    int             hideTics;
    float           hideAmount;
    float           alpha; // Fullscreen hud alpha value.

    boolean         firstTime;  // ST_Start() has just been called.
    boolean         statusbarActive; // Whether the HUD is on.
    int             currentFragsCount; // Number of frags so far in deathmatch.
} hudstate_t;

typedef enum hotloc_e {
    HOT_TLEFT,
    HOT_TRIGHT,
    HOT_BRIGHT,
    HOT_BLEFT
} hotloc_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

void unhideHUD(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static hudstate_t hudStates[MAXPLAYERS];

// CVARs for the HUD/Statusbar.
cvartemplate_t sthudCVars[] =
{
    // HUD scale
    {"hud-scale", 0, CVT_FLOAT, &cfg.hudScale, 0.1f, 10, unhideHUD},

    // HUD colour + alpha
    {"hud-color-r", 0, CVT_FLOAT, &cfg.hudColor[0], 0, 1, unhideHUD},
    {"hud-color-g", 0, CVT_FLOAT, &cfg.hudColor[1], 0, 1, unhideHUD},
    {"hud-color-b", 0, CVT_FLOAT, &cfg.hudColor[2], 0, 1, unhideHUD},
    {"hud-color-a", 0, CVT_FLOAT, &cfg.hudColor[3], 0, 1, unhideHUD},
    {"hud-icon-alpha", 0, CVT_FLOAT, &cfg.hudIconAlpha, 0, 1, unhideHUD},

    // HUD icons
    {"hud-health", 0, CVT_BYTE, &cfg.hudShown[HUD_HEALTH], 0, 1, unhideHUD},
    {"hud-armor", 0, CVT_BYTE, &cfg.hudShown[HUD_ARMOR], 0, 1, unhideHUD},
    {"hud-ammo", 0, CVT_BYTE, &cfg.hudShown[HUD_AMMO], 0, 1, unhideHUD},
    {"hud-keys", 0, CVT_BYTE, &cfg.hudShown[HUD_KEYS], 0, 1, unhideHUD},
    {"hud-power", 0, CVT_BYTE, &cfg.hudShown[HUD_INVENTORY], 0, 1, unhideHUD},

    // HUD displays
    {"hud-frags", 0, CVT_BYTE, &cfg.hudShown[HUD_FRAGS], 0, 1, unhideHUD},

    {"hud-timer", 0, CVT_FLOAT, &cfg.hudTimer, 0, 60},

    {"hud-unhide-damage", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_DAMAGE], 0, 1},
    {"hud-unhide-pickup-health", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_HEALTH], 0, 1},
    {"hud-unhide-pickup-armor", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_ARMOR], 0, 1},
    {"hud-unhide-pickup-powerup", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_POWER], 0, 1},
    {"hud-unhide-pickup-weapon", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_WEAPON], 0, 1},
    {"hud-unhide-pickup-ammo", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_AMMO], 0, 1},
    {"hud-unhide-pickup-key", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_KEY], 0, 1},

    {"hud-cheat-counter", 0, CVT_BYTE, &cfg.hudShownCheatCounters, 0, 63, unhideHUD},
    {"hud-cheat-counter-scale", 0, CVT_FLOAT, &cfg.hudCheatCounterScale, .1f, 1, unhideHUD},
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
}

/**
 * Unhides the current HUD display if hidden.
 *
 * @param player        The player whoose HUD to (maybe) unhide.
 * @param event         The HUD Update Event type to check for triggering.
 */
void ST_HUDUnHide(int player, hueevent_t ev)
{
    player_t*           plr;

    if(ev < HUE_FORCE || ev > NUMHUDUNHIDEEVENTS)
        return;

    plr = &players[player];
    if(!(plr->plr->inGame && (plr->plr->flags & DDPF_LOCAL)))
        return;

    if(ev == HUE_FORCE || cfg.hudUnHide[ev])
    {
        hudStates[player].hideTics = (cfg.hudTimer * TICSPERSEC);
        hudStates[player].hideAmount = 0;
    }
}

void ST_updateWidgets(int player)
{
    int                 i;
    player_t*           plr = &players[player];
    hudstate_t*         hud = &hudStates[player];

    // Used by wFrags widget.
    hud->currentFragsCount = 0;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->inGame)
            continue;

        hud->currentFragsCount += plr->frags[i] * (i != player ? 1 : -1);
    }
}

void ST_Ticker(timespan_t ticLength)
{
    static trigger_t fixed = {1.0 / TICSPERSEC};

    boolean runFixedTic = M_RunTrigger(&fixed, ticLength);
    int i;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t* plr = &players[i];
        hudstate_t* hud = &hudStates[i];

        if(!(plr->plr->inGame && (plr->plr->flags & DDPF_LOCAL)))
            continue;

        // Fade in/out the fullscreen HUD.
        if(hud->statusbarActive)
        {
            if(hud->alpha > 0.0f)
            {
                hud->statusbarActive = 0;
                hud->alpha-=0.1f;
            }
        }
        else
        {
            if(cfg.screenBlocks == 13)
            {
                if(hud->alpha > 0.0f)
                {
                    hud->alpha-=0.1f;
                }
            }
            else
            {
                if(hud->alpha < 1.0f)
                    hud->alpha += 0.1f;
            }
        }

        // The following is restricted to fixed 35 Hz ticks.
        if(runFixedTic)
        {
            if(!P_IsPaused())
            {
                if(cfg.hudTimer == 0)
                {
                    hud->hideTics = hud->hideAmount = 0;
                }
                else
                {
                    if(hud->hideTics > 0)
                        hud->hideTics--;
                    if(hud->hideTics == 0 && cfg.hudTimer > 0 && hud->hideAmount < 1)
                        hud->hideAmount += 0.1f;
                }

                ST_updateWidgets(i);
            }
        }
    }
}

void ST_doPaletteStuff(int player)
{
    int palette = 0, cnt, bzc;
    player_t* plr = &players[player];

    cnt = plr->damageCount;

    if(plr->powers[PT_STRENGTH])
    {   // Slowly fade the berzerk out.
        bzc = 12 - (plr->powers[PT_STRENGTH] >> 6);

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
    else if(plr->bonusCount)
    {
        palette = (plr->bonusCount + 7) >> 3;
        if(palette >= NUMBONUSPALS)
            palette = NUMBONUSPALS - 1;
        palette += STARTBONUSPALS;
    }
    else if(plr->powers[PT_IRONFEET] > 4 * 32 ||
            plr->powers[PT_IRONFEET] & 8)
    {
        palette = RADIATIONPAL;
    }

    // $democam
    if(palette)
    {
        plr->plr->flags |= DDPF_VIEW_FILTER;
        R_GetFilterColor(plr->plr->filterColor, palette);
    }
    else
        plr->plr->flags &= ~DDPF_VIEW_FILTER;
}

static void drawWidgets(hudstate_t* hud)
{
#define MAXDIGITS           ST_FRAGSWIDTH

    if(deathmatch)
    {
        char buf[20];
        if(hud->currentFragsCount == 1994)
            return;
        dd_snprintf(buf, 20, "%i", hud->currentFragsCount);

        DGL_Enable(DGL_TEXTURE_2D);

        FR_SetFont(FID(GF_STATUS));
        DGL_Color4f(1, 1, 1, hud->alpha);

        FR_DrawTextFragment2(buf, ST_FRAGSX, ST_FRAGSY, DTF_ALIGN_TOPRIGHT|DTF_NO_EFFECTS);

        DGL_Disable(DGL_TEXTURE_2D);
    }

#undef MAXDIGITS
}

void ST_doRefresh(int player)
{
    hudstate_t* hud;

    if(player < 0 || player > MAXPLAYERS)
        return;

    hud = &hudStates[player];
    hud->firstTime = false;

    drawWidgets(hud);
}

void ST_drawHUDSprite(int sprite, float x, float y, hotloc_t hotspot,
    float scale, float alpha, boolean flip, int* drawnWidth, int* drawnHeight)
{
    spriteinfo_t info;

    if(!(alpha > 0))
        return;

    alpha = MINMAX_OF(0.f, alpha, 1.f);
    R_GetSpriteInfo(sprite, 0, &info);

    switch(hotspot)
    {
    case HOT_BRIGHT:
        y -= info.height * scale;

    case HOT_TRIGHT:
        x -= info.width * scale;
        break;

    case HOT_BLEFT:
        y -= info.height * scale;
        break;
    default:
        break;
    }

    DGL_SetPSprite(info.material);
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(1, 1, 1, alpha);
    DGL_Begin(DGL_QUADS);
        DGL_TexCoord2f(0, flip * info.texCoord[0], 0);
        DGL_Vertex2f(x, y);

        DGL_TexCoord2f(0, !flip * info.texCoord[0], 0);
        DGL_Vertex2f(x + info.width * scale, y);

        DGL_TexCoord2f(0, !flip * info.texCoord[0], info.texCoord[1]);
        DGL_Vertex2f(x + info.width * scale, y + info.height * scale);

        DGL_TexCoord2f(0, flip * info.texCoord[0], info.texCoord[1]);
        DGL_Vertex2f(x, y + info.height * scale);
    DGL_End();

    DGL_Disable(DGL_TEXTURE_2D);

    if(drawnWidth)  *drawnWidth  = info.width  * scale;
    if(drawnHeight) *drawnHeight = info.height * scale;
}

void ST_doFullscreenStuff(int player)
{
    static const int    ammo_sprite[NUM_AMMO_TYPES] = {
        SPR_AMMO,
        SPR_SBOX,
        SPR_CELL,
        SPR_RCKT
    };

    hudstate_t*         hud = &hudStates[player];
    player_t*           plr = &players[player];
    char                buf[20];
    int                 w, h, pos = 0, oldPos = 0, spr,i;
    int                 h_width = 320 / cfg.hudScale;
    int                 h_height = 200 / cfg.hudScale;
    float               textalpha =
        hud->alpha - hud->hideAmount - ( 1 - cfg.hudColor[3]);
    float               iconalpha =
        hud->alpha - hud->hideAmount - ( 1 - cfg.hudIconAlpha);

    textalpha = MINMAX_OF(0.f, textalpha, 1.f);
    iconalpha = MINMAX_OF(0.f, iconalpha, 1.f);

    if(IS_NETGAME && deathmatch && cfg.hudShown[HUD_FRAGS])
    {
        // Display the frag counter.
        i = 199 - HUDBORDERY;
        if(cfg.hudShown[HUD_HEALTH])
        {
            i -= 18 * cfg.hudScale;
        }

        sprintf(buf, "FRAGS:%i", hud->currentFragsCount);

        DGL_Enable(DGL_TEXTURE_2D);
        FR_SetFont(FID(GF_FONTA));
        DGL_Color4f(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textalpha);

        FR_DrawTextFragment2(buf, HUDBORDERX, i, DTF_ALIGN_TOPLEFT|DTF_NO_EFFECTS);

        DGL_Disable(DGL_TEXTURE_2D);
    }

    // Setup the scaling matrix.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);

    // Draw the visible HUD data, first health.
    if(cfg.hudShown[HUD_HEALTH])
    {

        sprintf(buf, "HEALTH");

        DGL_Enable(DGL_TEXTURE_2D);

        FR_SetFont(FID(GF_FONTA));
        DGL_Color4f(1, 1, 1, iconalpha);

        pos = FR_TextFragmentWidth(buf)/2;
        FR_DrawTextFragment2(buf, HUDBORDERX, h_height - HUDBORDERY - 4, DTF_ALIGN_BOTTOM|DTF_NO_EFFECTS);

        sprintf(buf, "%i", plr->health);

        FR_SetFont(FID(GF_FONTB));
        DGL_Color4f(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textalpha);
        FR_DrawTextFragment2(buf, HUDBORDERX + pos, h_height - HUDBORDERY, DTF_ALIGN_BOTTOM|DTF_NO_EFFECTS);

        DGL_Disable(DGL_TEXTURE_2D);

        oldPos = pos;
        pos = HUDBORDERX * 2 + FR_TextFragmentWidth(buf);
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
                ST_drawHUDSprite(spr, pos, h_height - 2, HOT_BLEFT, 1,
                    iconalpha, false, &w, &h);
                pos += w + 2;
            }
        }
Draw_EndZoom();
    }
    pos = oldPos;

    // Inventory
    if(cfg.hudShown[HUD_INVENTORY])
    {
        if(P_InventoryCount(player, IIT_DEMONKEY1))
        {
            spr = SPR_ART1;
            ST_drawHUDSprite(spr, HUDBORDERX + pos -w/2, h_height - 44,
                HOT_BLEFT, 1, iconalpha, false, &w, &h);
        }

        if(P_InventoryCount(player, IIT_DEMONKEY2))
        {
            spr = SPR_ART2;
            ST_drawHUDSprite(spr, HUDBORDERX + pos -w/2, h_height - 84,
                HOT_BLEFT, 1, iconalpha, false, &w, &h);
        }

        if(P_InventoryCount(player, IIT_DEMONKEY3))
        {
            spr = SPR_ART3;
            ST_drawHUDSprite(spr, HUDBORDERX + pos -w/2, h_height - 124,
                HOT_BLEFT, 1, iconalpha, false, &w, &h);
        }
    }

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
            pos = h_width/2;

            DGL_Enable(DGL_TEXTURE_2D);

            FR_SetFont(FID(GF_FONTB));
            DGL_Color4f(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textalpha);
            FR_DrawTextFragment2(buf, pos, h_height - HUDBORDERY, DTF_ALIGN_TOP|DTF_NO_EFFECTS);

            DGL_Disable(DGL_TEXTURE_2D);
            break;
        }
    }

    pos = h_width - 1;
    if(cfg.hudShown[HUD_ARMOR])
    {
        sprintf(buf, "ARMOR");

        DGL_Enable(DGL_TEXTURE_2D);
        FR_SetFont(FID(GF_FONTA));
        DGL_Color4f(1, 1, 1, iconalpha);
        w = FR_TextFragmentWidth(buf);
        FR_DrawTextFragment2(buf, h_width - HUDBORDERX, h_height - HUDBORDERY - 4, DTF_ALIGN_BOTTOMRIGHT|DTF_NO_EFFECTS);

        sprintf(buf, "%i", plr->armorPoints);
        FR_SetFont(FID(GF_FONTB));
        DGL_Color4f(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textalpha);
        FR_DrawTextFragment2(buf, h_width - (w/2) - HUDBORDERX, h_height - HUDBORDERY, DTF_ALIGN_BOTTOMRIGHT|DTF_NO_EFFECTS);

        DGL_Disable(DGL_TEXTURE_2D);
    }

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void ST_Drawer(int player)
{
    hudstate_t* hud;
    player_t* plr;
    int fullscreenMode = (cfg.screenBlocks < 10? 0 : cfg.screenBlocks - 10);

    if(player < 0 || player >= MAXPLAYERS)
        return;

    plr = &players[player];
    if(!((plr->plr->flags & DDPF_LOCAL) && plr->plr->inGame))
        return;

    hud = &hudStates[player];

    hud->firstTime = hud->firstTime;
    hud->statusbarActive = (fullscreenMode < 2) || (AM_IsActive(AM_MapForPlayer(player)) && (cfg.automapHudDisplay == 0 || cfg.automapHudDisplay == 2));

    // Do palette shifts.
    ST_doPaletteStuff(player);

    ST_doFullscreenStuff(player);
}

void ST_loadData(void)
{
    // Nothing to do.
}

static void initData(hudstate_t* hud)
{
    int player = hud - hudStates;

    hud->firstTime = true;
    hud->statusbarActive = true;
    hud->stopped = true;
    hud->alpha = 0.f;

    ST_HUDUnHide(player, HUE_FORCE);
}

void ST_Start(int player)
{
    hudstate_t* hud;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    hud = &hudStates[player];

    if(!hud->stopped)
        ST_Stop(player);

    initData(hud);

    hud->stopped = false;
}

void ST_Stop(int player)
{
    hudstate_t* hud;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    hud = &hudStates[player];
    if(hud->stopped)
        return;

    hud->stopped = true;
}

void ST_Init(void)
{
    ST_loadData();
}

void ST_Shutdown(void)
{
    /*int i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        hudstate_t* hud = &hudStates[i];
        hud->inited = false;
    }*/
}

void ST_UpdateLogAlignment(void)
{
    // Stub.
#if 0
    short flags;
    int i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        hudstate_t* hud = &hudStates[i];
        if(!hud->inited) continue;

        flags = GUI_GroupFlags(GUI_GroupByIndex(hud->widgetGroupNames[UWG_TOP]));
        flags &= ~(UWGF_ALIGN_LEFT|UWGF_ALIGN_RIGHT);
        if(cfg.msgAlign == 0)
            flags |= UWGF_ALIGN_LEFT;
        else if(cfg.msgAlign == 2)
            flags |= UWGF_ALIGN_RIGHT;
        GUI_GroupSetFlags(GUI_GroupByIndex(hud->widgetGroupNames[UWG_TOP]), flags);
    }
#endif
}

/**
 * Called when a cvar changes that affects the look/behavior of the HUD in order to unhide it.
 */
void unhideHUD(void)
{
    int i;
    for(i = 0; i < MAXPLAYERS; ++i)
        ST_HUDUnHide(i, HUE_FORCE);
}
