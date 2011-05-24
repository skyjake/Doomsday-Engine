/**\file st_stuff.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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
 * Statusbar code - Heretic specific.
 */

// HEADER FILES ------------------------------------------------------------

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "jheretic.h"

#include "d_net.h"
#include "hu_stuff.h"
#include "p_tick.h" // for P_IsPaused
#include "p_player.h"
#include "am_map.h"
#include "hu_lib.h"
#include "hu_chat.h"
#include "hu_inventory.h"
#include "hu_log.h"
#include "p_inventory.h"
#include "r_common.h"

// MACROS ------------------------------------------------------------------

// Current ammo icon(sbbar).
#define ST_AMMOIMGWIDTH     (24)
#define ST_AMMOICONX        (111)
#define ST_AMMOICONY        (14)

// Inventory.
#define ST_INVENTORYX       (50)
#define ST_INVENTORYY       (2)

// Current inventory item.
#define ST_INVITEMX         (179)
#define ST_INVITEMY         (2)

// Current inventory item count.
#define ST_INVITEMCWIDTH    (2) // Num digits
#define ST_INVITEMCX        (208)
#define ST_INVITEMCY        (24)

// AMMO number pos.
#define ST_AMMOWIDTH        (3)
#define ST_AMMOX            (135)
#define ST_AMMOY            (4)

// ARMOR number pos.
#define ST_ARMORWIDTH       (3)
#define ST_ARMORX           (254)
#define ST_ARMORY           (12)

// HEALTH number pos.
#define ST_HEALTHWIDTH      (3)
#define ST_HEALTHX          (85)
#define ST_HEALTHY          (12)

// Key icon positions.
#define ST_KEY0WIDTH        (10)
#define ST_KEY0HEIGHT       (6)
#define ST_KEY0X            (153)
#define ST_KEY0Y            (6)
#define ST_KEY1WIDTH        (ST_KEY0WIDTH)
#define ST_KEY1X            (153)
#define ST_KEY1Y            (14)
#define ST_KEY2WIDTH        (ST_KEY0WIDTH)
#define ST_KEY2X            (153)
#define ST_KEY2Y            (22)

// Frags pos.
#define ST_FRAGSX           (85)
#define ST_FRAGSY           (13)
#define ST_FRAGSWIDTH       (2)

// TYPES -------------------------------------------------------------------

enum {
    UWG_STATUSBAR = 0,
    UWG_TOPLEFT,
    UWG_TOPLEFT2,
    UWG_TOPRIGHT,
    UWG_BOTTOMLEFT,
    UWG_BOTTOMLEFT2,
    UWG_BOTTOMRIGHT,
    UWG_BOTTOM,
    UWG_TOP,
    UWG_COUNTERS,
    NUM_UIWIDGET_GROUPS
};

typedef struct {
    boolean inited;
    boolean stopped;
    int hideTics;
    float hideAmount;
    float alpha; // Fullscreen hud alpha value.
    float showBar; // Slide statusbar amount 1.0 is fully open.
    boolean statusbarActive; // Whether main statusbar is active.

    int widgetGroupIds[NUM_UIWIDGET_GROUPS];

    guidata_health_t sbarHealth;
    guidata_armor_t sbarArmor;
    guidata_frags_t sbarFrags;
    guidata_chain_t sbarChain;
    guidata_keyslot_t sbarKeyslots[3];
    guidata_readyitem_t sbarReadyitem;
    guidata_readyammo_t sbarReadyammo;
    guidata_readyammoicon_t sbarReadyammoicon;

    guidata_health_t health;
    guidata_armor_t armor;
    guidata_keys_t keys;
    guidata_readyammo_t readyammo;
    guidata_readyammoicon_t readyammoicon;
    guidata_frags_t frags;
    guidata_readyitem_t readyitem;

    guidata_flight_t flight;
    guidata_tomeofpower_t tome;
    guidata_secrets_t secrets;
    guidata_items_t items;
    guidata_kills_t kills;
} hudstate_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void updateViewWindow(void);
void unhideHUD(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static hudstate_t hudStates[MAXPLAYERS];

static patchinfo_t pStatusbar;
static patchinfo_t pStatusbarTopLeft;
static patchinfo_t pStatusbarTopRight;
static patchinfo_t pChain;
static patchinfo_t pStatBar;
static patchinfo_t pLifeBar;
static patchinfo_t pInvBar;
static patchinfo_t pLifeGems[4];
static patchinfo_t pAmmoIcons[11];
static patchinfo_t pInvItemFlash[5];
static patchinfo_t pSpinTome[16];
static patchinfo_t pSpinFly[16];
static patchinfo_t pKeys[NUM_KEY_TYPES];
static patchinfo_t pGodLeft;
static patchinfo_t pGodRight;

// CVARs for the HUD/Statusbar
cvartemplate_t sthudCVars[] = {
    // HUD scale
    {"hud-scale", 0, CVT_FLOAT, &cfg.hudScale, 0.1f, 1, unhideHUD},
    {"hud-wideoffset", 0, CVT_FLOAT, &cfg.hudWideOffset, 0, 1, unhideHUD},

    {"hud-status-size", 0, CVT_FLOAT, &cfg.statusbarScale, 0.1f, 1, updateViewWindow},

    // HUD colour + alpha
    {"hud-color-r", 0, CVT_FLOAT, &cfg.hudColor[0], 0, 1, unhideHUD},
    {"hud-color-g", 0, CVT_FLOAT, &cfg.hudColor[1], 0, 1, unhideHUD},
    {"hud-color-b", 0, CVT_FLOAT, &cfg.hudColor[2], 0, 1, unhideHUD},
    {"hud-color-a", 0, CVT_FLOAT, &cfg.hudColor[3], 0, 1, unhideHUD},
    {"hud-icon-alpha", 0, CVT_FLOAT, &cfg.hudIconAlpha, 0, 1, unhideHUD},

    {"hud-status-alpha", 0, CVT_FLOAT, &cfg.statusbarOpacity, 0, 1, unhideHUD},
    {"hud-status-icon-a", 0, CVT_FLOAT, &cfg.statusbarCounterAlpha, 0, 1, unhideHUD},

    // HUD icons
    {"hud-ammo", 0, CVT_BYTE, &cfg.hudShown[HUD_AMMO], 0, 1, unhideHUD},
    {"hud-armor", 0, CVT_BYTE, &cfg.hudShown[HUD_ARMOR], 0, 1, unhideHUD},
    {"hud-keys", 0, CVT_BYTE, &cfg.hudShown[HUD_KEYS], 0, 1, unhideHUD},
    {"hud-health", 0, CVT_BYTE, &cfg.hudShown[HUD_HEALTH], 0, 1, unhideHUD},
    {"hud-currentitem", 0, CVT_BYTE, &cfg.hudShown[HUD_READYITEM], 0, 1, unhideHUD},

    // HUD displays
    {"hud-tome-timer", CVF_NO_MAX, CVT_INT, &cfg.tomeCounter, 0, 0},
    {"hud-tome-sound", CVF_NO_MAX, CVT_INT, &cfg.tomeSound, 0, 0},

    {"hud-timer", 0, CVT_FLOAT, &cfg.hudTimer, 0, 60},

    {"hud-unhide-damage", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_DAMAGE], 0, 1},
    {"hud-unhide-pickup-health", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_HEALTH], 0, 1},
    {"hud-unhide-pickup-armor", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_ARMOR], 0, 1},
    {"hud-unhide-pickup-powerup", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_POWER], 0, 1},
    {"hud-unhide-pickup-weapon", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_WEAPON], 0, 1},
    {"hud-unhide-pickup-ammo", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_AMMO], 0, 1},
    {"hud-unhide-pickup-key", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_KEY], 0, 1},
    {"hud-unhide-pickup-invitem", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_INVITEM], 0, 1},

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
    int i;

    for(i = 0; sthudCVars[i].name; ++i)
        Con_AddVariable(sthudCVars + i);

    Hu_InventoryRegister();
}

static int fullscreenMode(void)
{
    return (cfg.screenBlocks < 10? 0 : cfg.screenBlocks - 10);
}

static void shadeChain(int x, int y, float alpha)
{
    DGL_Begin(DGL_QUADS);
        // Left shadow.
        DGL_Color4f(0, 0, 0, alpha);
        DGL_Vertex2f(x+20, y+ST_HEIGHT);
        DGL_Vertex2f(x+20, y+ST_HEIGHT-10);
        DGL_Color4f(0, 0, 0, 0);
        DGL_Vertex2f(x+35, y+ST_HEIGHT-10);
        DGL_Vertex2f(x+35, y+ST_HEIGHT);

        // Right shadow.
        DGL_Vertex2f(x+ST_WIDTH-43, y+ST_HEIGHT);
        DGL_Vertex2f(x+ST_WIDTH-43, y+ST_HEIGHT-10);
        DGL_Color4f(0, 0, 0, alpha);
        DGL_Vertex2f(x+ST_WIDTH-27, y+ST_HEIGHT-10);
        DGL_Vertex2f(x+ST_WIDTH-27, y+ST_HEIGHT);
    DGL_End();
}

void SBarChain_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_chain_t* chain = (guidata_chain_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    int delta, curHealth = MAX_OF(plr->plr->mo->health, 0);

    // Health marker chain animates up to the actual health value.
    if(curHealth < chain->healthMarker)
    {
        delta = MINMAX_OF(1, (chain->healthMarker - curHealth) >> 2, 4);
        chain->healthMarker -= delta;
    }
    else if(curHealth > chain->healthMarker)
    {
        delta = MINMAX_OF(1, (curHealth - chain->healthMarker) >> 2, 4);
        chain->healthMarker += delta;
    }

    if(chain->healthMarker != curHealth && (mapTime & 1))
    {
        chain->wiggle = P_Random() & 1;
    }
    else
    {
        chain->wiggle = 0;
    }
    }
}

void SBarChain_Drawer(uiwidget_t* obj, int xOffset, int yOffset)
{
#define ORIGINX (-ST_WIDTH/2)
#define ORIGINY (0)

    assert(NULL != obj);
    {
    static int theirColors[] = {
        144, // Green.
        197, // Yellow.
        150, // Red.
        220  // Blue.
    };
    guidata_chain_t* chain = (guidata_chain_t*)obj->typedata;
    int chainY;
    float healthPos, gemXOffset, gemglow;
    int x, y, w, h, gemNum;
    float cw, rgb[3];
    const hudstate_t* hud = &hudStates[obj->player];
    int chainYOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = fullscreenMode();
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    chainY = -9 + chain->wiggle;

    healthPos = MINMAX_OF(0, chain->healthMarker / 100.f, 1);

    if(!IS_NETGAME)
        gemNum = 2; // Always use the red gem in single player.
    else
        gemNum = cfg.playerColor[obj->player];
    gemglow = healthPos;

    // Draw the chain.
    x = ORIGINX+21;
    y = ORIGINY+chainY;
    w = ST_WIDTH - 21 - 28;
    h = 8;
    cw = (float) w / pChain.width;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(xOffset, yOffset, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, chainYOffset, 0);

    DGL_SetPatch(pChain.id, DGL_REPEAT, DGL_CLAMP);
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(1, 1, 1, iconAlpha);

    gemXOffset = (w - pLifeGems[gemNum].width) * healthPos;

    if(gemXOffset > 0)
    {   // Left chain section.
        float cw = gemXOffset / pChain.width;

        DGL_Begin(DGL_QUADS);
            DGL_TexCoord2f(0, 1 - cw, 0);
            DGL_Vertex2f(x, y);

            DGL_TexCoord2f(0, 1, 0);
            DGL_Vertex2f(x + gemXOffset, y);

            DGL_TexCoord2f(0, 1, 1);
            DGL_Vertex2f(x + gemXOffset, y + h);

            DGL_TexCoord2f(0, 1 - cw, 1);
            DGL_Vertex2f(x, y + h);
        DGL_End();
    }

    if(gemXOffset + pLifeGems[gemNum].width < w)
    {   // Right chain section.
        float cw = (w - gemXOffset - pLifeGems[gemNum].width) / pChain.width;

        DGL_Begin(DGL_QUADS);
            DGL_TexCoord2f(0, 0, 0);
            DGL_Vertex2f(x + gemXOffset + pLifeGems[gemNum].width, y);

            DGL_TexCoord2f(0, cw, 0);
            DGL_Vertex2f(x + w, y);

            DGL_TexCoord2f(0, cw, 1);
            DGL_Vertex2f(x + w, y + h);

            DGL_TexCoord2f(0, 0, 1);
            DGL_Vertex2f(x + gemXOffset + pLifeGems[gemNum].width, y + h);
        DGL_End();
    }

    // Draw the life gem.
    DGL_Color4f(1, 1, 1, iconAlpha);
    GL_DrawPatch(pLifeGems[gemNum].id, x + gemXOffset, chainY);

    DGL_Disable(DGL_TEXTURE_2D);

    shadeChain(ORIGINX, ORIGINY-ST_HEIGHT, iconAlpha/2);

    // How about a glowing gem?
    DGL_BlendMode(BM_ADD);
    DGL_Bind(Get(DD_DYNLIGHT_TEXTURE));
    DGL_Enable(DGL_TEXTURE_2D);

    R_GetColorPaletteRGBf(0, theirColors[gemNum], rgb, false);
    DGL_DrawRect(x + gemXOffset - 11, chainY - 6, 41, 24, rgb[0], rgb[1], rgb[2], gemglow - (1 - iconAlpha));

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_BlendMode(BM_NORMAL);
    DGL_Color4f(1, 1, 1, 1);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef ORIGINX
#undef ORIGINY
}

void SBarChain_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    /// \fixme Calculate dimensions properly.
    if(NULL != width)  *width  = (ST_WIDTH - 21 - 28) * cfg.statusbarScale;
    if(NULL != height) *height = 8 * cfg.statusbarScale;
}

/**
 * Draws the whole statusbar backgound.
 * \todo There is a whole lot of constants in here. What if someone wants to
 * replace the statusbar with new patches?
 */
void SBarBackground_Drawer(uiwidget_t* obj, int x, int y)
{
#define WIDTH       (ST_WIDTH)
#define HEIGHT      (ST_HEIGHT)
#define ORIGINX     ((int)(-WIDTH/2))
#define ORIGINY     ((int)(-HEIGHT * hud->showBar))

    assert(NULL != obj);
    {
    const hudstate_t* hud = &hudStates[obj->player];
    int fullscreen = fullscreenMode();
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarOpacity);
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarOpacity);

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);

    if(!(iconAlpha < 1))
    {   // We can just render the full thing as normal.
        // Top bits.
        DGL_Enable(DGL_TEXTURE_2D);

        DGL_Color4f(1, 1, 1, 1);
        GL_DrawPatch(pStatusbarTopLeft.id, ORIGINX, ORIGINY-10);
        GL_DrawPatch(pStatusbarTopRight.id, ORIGINX+290, ORIGINY-10);

        // Faces.
        GL_DrawPatch(pStatusbar.id, ORIGINX, ORIGINY);

        if(P_GetPlayerCheats(&players[obj->player]) & CF_GODMODE)
        {
            GL_DrawPatch(pGodLeft.id, ORIGINX+16, ORIGINY+9);
            GL_DrawPatch(pGodRight.id, ORIGINX+287, ORIGINY+9);
        }

        if(!Hu_InventoryIsOpen(obj->player))
        {
            if(deathmatch)
                GL_DrawPatch(pStatBar.id, ORIGINX+34, ORIGINY+2);
            else
                GL_DrawPatch(pLifeBar.id, ORIGINX+34, ORIGINY+2);
        }
        else
        {
            GL_DrawPatch(pInvBar.id, ORIGINX+34, ORIGINY+2);
        }

        DGL_Disable(DGL_TEXTURE_2D);
    }
    else
    {
        DGL_Enable(DGL_TEXTURE_2D);

        DGL_Color4f(1, 1, 1, iconAlpha);

        // Top bits.
        GL_DrawPatch(pStatusbarTopLeft.id, ORIGINX, ORIGINY-10);
        GL_DrawPatch(pStatusbarTopRight.id, ORIGINX+290, ORIGINY-10);

        DGL_SetPatch(pStatusbar.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);

        // Top border.
        DGL_DrawCutRectTiled(ORIGINX+34, ORIGINY, 248, 2, 320, 42, 34, 0, ORIGINX, ORIGINY, 0, 0);

        // Chain background.
        DGL_DrawCutRectTiled(ORIGINX+34, ORIGINY+33, 248, 9, 320, 42, 34, 33, ORIGINX, ORIGINY+191, 16, 8);

        // Faces.
        if(P_GetPlayerCheats(&players[obj->player]) & CF_GODMODE)
        {
            // If GOD mode we need to cut windows
            DGL_DrawCutRectTiled(ORIGINX, ORIGINY, 34, 42, 320, 42, 0, 0, ORIGINX+16, ORIGINY+9, 16, 8);
            DGL_DrawCutRectTiled(ORIGINX+282, ORIGINY, 38, 42, 320, 42, 282, 0, ORIGINX+287, ORIGINY+9, 16, 8);

            GL_DrawPatch(pGodLeft.id, ORIGINX+16, ORIGINY+9);
            GL_DrawPatch(pGodRight.id, ORIGINX+287, ORIGINY+9);
        }
        else
        {
            DGL_DrawCutRectTiled(ORIGINX, ORIGINY, 34, 42, 320, 42, 0, 0, ORIGINX, ORIGINY, 0, 0);
            DGL_DrawCutRectTiled(ORIGINX+282, ORIGINY, 38, 42, 320, 42, 282, 0, ORIGINX, ORIGINY, 0, 0);
        }

        if(!Hu_InventoryIsOpen(obj->player))
        {
            if(deathmatch)
                GL_DrawPatch(pStatBar.id, ORIGINX+34, ORIGINY+2);
            else
                GL_DrawPatch(pLifeBar.id, ORIGINX+34, ORIGINY+2);
        }
        else
        {
            GL_DrawPatch(pInvBar.id, ORIGINX+34, ORIGINY+2);
        }

        DGL_Disable(DGL_TEXTURE_2D);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef WIDTH
#undef HEIGHT
#undef ORIGINX
#undef ORIGINY
}

void SBarBackground_Dimensions(uiwidget_t* obj, int* width, int* height)
{
#define WIDTH       (ST_WIDTH)
#define HEIGHT      (ST_HEIGHT)

    assert(NULL != obj);

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    if(NULL != width)  *width  = WIDTH  * cfg.statusbarScale;
    if(NULL != height) *height = HEIGHT * cfg.statusbarScale;

#undef WIDTH
#undef HEIGHT
}

void ST_updateWidgets(int player)
{
    hudstate_t* hud = &hudStates[player];
    if(hud->inited)
    {
        int i;
        for(i = 0; i < NUM_UIWIDGET_GROUPS; ++i)
        {
            GUI_TickWidget(GUI_MustFindObjectById(hud->widgetGroupIds[i]));
        }
    }
}

void ST_Ticker(timespan_t ticLength)
{
    static trigger_t fixed = {1.0 / TICSPERSEC};

    boolean runFixedTic = M_RunTrigger(&fixed, ticLength);
    int i;

    if(runFixedTic)
        Hu_InventoryTicker();

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t* plr = &players[i];
        hudstate_t* hud = &hudStates[i];

        if(!(plr->plr->inGame && (plr->plr->flags & DDPF_LOCAL)))
            continue;

        // Either slide the status bar in or fade out the fullscreen HUD.
        if(hud->statusbarActive)
        {
            if(hud->alpha > 0.0f)
            {
                hud->statusbarActive = 0;
                hud->alpha -= 0.1f;
            }
            else if(hud->showBar < 1.0f)
            {
                hud->showBar += 0.1f;
            }
        }
        else
        {
            if(cfg.screenBlocks == 13)
            {
                if(hud->alpha > 0.0f)
                {
                    hud->alpha -= 0.1f;
                }
            }
            else
            {
                if(hud->showBar > 0.0f)
                {
                    hud->showBar -= 0.1f;
                    hud->statusbarActive = 1;
                }
                else if(hud->alpha < 1.0f)
                {
                    hud->alpha += 0.1f;
                }
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

/**
 * Sets the new palette based upon current values of player->damageCount
 * and player->bonusCount
 */
void ST_doPaletteStuff(int player)
{
    int palette = 0;
    player_t* plr = &players[player];

    if(plr->damageCount)
    {
        palette = (plr->damageCount + 7) >> 3;
        if(palette >= NUMREDPALS)
        {
            palette = NUMREDPALS - 1;
        }
        palette += STARTREDPALS;
    }
    else if(plr->bonusCount)
    {
        palette = (plr->bonusCount + 7) >> 3;
        if(palette >= NUMBONUSPALS)
        {
            palette = NUMBONUSPALS - 1;
        }
        palette += STARTBONUSPALS;
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

void SBarInventory_Drawer(uiwidget_t* obj, int x, int y)
{
    assert(NULL != obj);
    {
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = fullscreenMode();
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);

    if(!Hu_InventoryIsOpen(obj->player))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);

    Hu_InventoryDraw2(obj->player, -ST_WIDTH/2 + ST_INVENTORYX, -ST_HEIGHT + yOffset + ST_INVENTORYY, iconAlpha);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void SBarInventory_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(!Hu_InventoryIsOpen(obj->player))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    // \fixme calculate dimensions properly!
    if(NULL != width)  *width  = (ST_WIDTH-(43*2)) * cfg.statusbarScale;
    if(NULL != height) *height = 41 * cfg.statusbarScale;
}

void Frags_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_frags_t* frags = (guidata_frags_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    int i;
    frags->value = 0;
    for(i = 0; i < MAXPLAYERS; ++i)
        if(players[i].plr->inGame)
            frags->value += plr->frags[i] * (i != obj->player ? 1 : -1);
    }
}

void SBarFrags_Drawer(uiwidget_t* obj, int x, int y)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX+ST_FRAGSX)
#define Y                   (ORIGINY+ST_FRAGSY)
#define MAXDIGITS           (ST_FRAGSWIDTH)

    assert(NULL != obj);
    {
    guidata_frags_t* frags = (guidata_frags_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int fullscreen = fullscreenMode();
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    char buf[20];

    if(!deathmatch || Hu_InventoryIsOpen(obj->player))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(frags->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", frags->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->fontId);
    DGL_Color4f(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    FR_DrawTextFragment2(buf, X, Y, DTF_ALIGN_TOPRIGHT|DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef MAXDIGITS
#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarFrags_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_frags_t* frags = (guidata_frags_t*)obj->typedata;
    char buf[20];

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(!deathmatch || Hu_InventoryIsOpen(obj->player))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(frags->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", frags->value);
    FR_SetFont(obj->fontId);
    if(NULL != width)  *width  = FR_TextFragmentWidth(buf)  * cfg.statusbarScale;
    if(NULL != height) *height = FR_TextFragmentHeight(buf) * cfg.statusbarScale;
    }
}

void Health_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_health_t* hlth = (guidata_health_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    hlth->value = plr->health;
    }
}

void SBarHealth_Drawer(uiwidget_t* obj, int x, int y)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX+ST_HEALTHX)
#define Y                   (ORIGINY+ST_HEALTHY)
#define MAXDIGITS           (ST_HEALTHWIDTH)
#define TRACKING            (1)

    assert(NULL != obj);
    {
    guidata_health_t* hlth = (guidata_health_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = fullscreenMode();
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    char buf[20];

    if(deathmatch || Hu_InventoryIsOpen(obj->player))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(hlth->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", hlth->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->fontId);
    DGL_Color4f(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    FR_DrawTextFragment3(buf, X, Y, DTF_ALIGN_TOPRIGHT|DTF_NO_EFFECTS, TRACKING);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef TRACKING
#undef MAXDIGITS
#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarHealth_Dimensions(uiwidget_t* obj, int* width, int* height)
{
#define TRACKING            (1)

    assert(NULL != obj);
    {
    guidata_health_t* hlth = (guidata_health_t*)obj->typedata;
    char buf[20];

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(deathmatch || Hu_InventoryIsOpen(obj->player))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(hlth->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", hlth->value);
    FR_SetFont(obj->fontId);
    if(NULL != width)  *width  = FR_TextFragmentWidth2(buf, TRACKING) * cfg.statusbarScale;
    if(NULL != height) *height = FR_TextFragmentHeight(buf) * cfg.statusbarScale;
    }
#undef TRACKING
}

void Armor_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_armor_t* armor = (guidata_armor_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    armor->value = plr->armorPoints;
    }
}

void SBarArmor_Drawer(uiwidget_t* obj, int x, int y)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX+ST_ARMORX)
#define Y                   (ORIGINY+ST_ARMORY)
#define MAXDIGITS           (ST_ARMORWIDTH)
#define TRACKING            (1)

    assert(NULL != obj);
    {
    guidata_armor_t* armor = (guidata_armor_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = fullscreenMode();
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    char buf[20];

    if(Hu_InventoryIsOpen(obj->player))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(armor->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", armor->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->fontId);
    DGL_Color4f(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    FR_DrawTextFragment3(buf, X, Y, DTF_ALIGN_TOPRIGHT|DTF_NO_EFFECTS, TRACKING);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef TRACKING
#undef MAXDIGITS
#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarArmor_Dimensions(uiwidget_t* obj, int* width, int* height)
{
#define TRACKING            (1)

    assert(NULL != obj);
    {
    guidata_armor_t* armor = (guidata_armor_t*)obj->typedata;
    char buf[20];

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(Hu_InventoryIsOpen(obj->player))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(armor->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", armor->value);
    FR_SetFont(obj->fontId);
    if(NULL != width)  *width  = FR_TextFragmentWidth2(buf, TRACKING) * cfg.statusbarScale;
    if(NULL != height) *height = FR_TextFragmentHeight(buf) * cfg.statusbarScale;
    }
#undef TRACKING
}

void KeySlot_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_keyslot_t* kslt = (guidata_keyslot_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    kslt->patchId = plr->keys[kslt->keytypeA] ? pKeys[kslt->keytypeA].id : 0;
    }
}

void KeySlot_Drawer(uiwidget_t* obj, int x, int y)
{
#define ORIGINX (-ST_WIDTH/2)
#define ORIGINY (-ST_HEIGHT)

    typedef struct {
        int x, y;
    } loc_t;
    assert(NULL != obj);
    {
    static const loc_t elements[] = {
        { ORIGINX+ST_KEY0X, ORIGINY+ST_KEY0Y },
        { ORIGINX+ST_KEY1X, ORIGINY+ST_KEY1Y },
        { ORIGINX+ST_KEY2X, ORIGINY+ST_KEY2Y }
    };
    guidata_keyslot_t* kslt = (guidata_keyslot_t*)obj->typedata;
    const loc_t* loc = &elements[kslt->keytypeA];
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = fullscreenMode();
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(obj->player))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(kslt->patchId == 0)
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    WI_DrawPatch4(kslt->patchId, Hu_ChoosePatchReplacement(kslt->patchId), loc->x, loc->y, DPF_ALIGN_TOPLEFT, FID(GF_FONTB), 1, 1, 1, iconAlpha);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef ORIGINY
#undef ORIGINX
}

void KeySlot_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_keyslot_t* kslt = (guidata_keyslot_t*)obj->typedata;
    patchinfo_t info;

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(Hu_InventoryIsOpen(obj->player))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(kslt->patchId == 0 || !R_GetPatchInfo(kslt->patchId, &info))
        return;

    if(NULL != width)  *width  = info.width  * cfg.statusbarScale;
    if(NULL != height) *height = info.height * cfg.statusbarScale;
    }
}

void ReadyAmmo_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_readyammo_t* ammo = (guidata_readyammo_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    int lvl = (plr->powers[PT_WEAPONLEVEL2]? 1 : 0);
    ammotype_t ammoType;

    ammo->value = 1994; // Means n/a.
    if(!(plr->readyWeapon > 0 && plr->readyWeapon < 7))
        return;
    for(ammoType = 0; ammoType < NUM_AMMO_TYPES; ++ammoType)
    {
        if(!weaponInfo[plr->readyWeapon][plr->class].mode[lvl].ammoType[ammoType])
            continue; // Weapon does not use this type of ammo.
        ammo->value = plr->ammo[ammoType].owned;
        break;
    }
    }
}

void SBarReadyAmmo_Drawer(uiwidget_t* obj, int x, int y)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX+ST_AMMOX)
#define Y                   (ORIGINY+ST_AMMOY)
#define MAXDIGITS           (ST_AMMOWIDTH)
#define TRACKING            (1)

    assert(NULL != obj);
    {
    guidata_readyammo_t* ammo = (guidata_readyammo_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = fullscreenMode();
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    char buf[20];

    if(Hu_InventoryIsOpen(obj->player))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(ammo->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", ammo->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->fontId);
    DGL_Color4f(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], iconAlpha);
    FR_DrawTextFragment3(buf, X, Y, DTF_ALIGN_TOPRIGHT|DTF_NO_EFFECTS, TRACKING);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef TRACKING
#undef MAXDIGITS
#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarReadyAmmo_Dimensions(uiwidget_t* obj, int* width, int* height)
{
#define TRACKING            (1)

    assert(NULL != obj);
    {
    guidata_readyammo_t* ammo = (guidata_readyammo_t*)obj->typedata;
    char buf[20];

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(Hu_InventoryIsOpen(obj->player))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(ammo->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", ammo->value);
    FR_SetFont(obj->fontId);
    if(NULL != width)  *width  = FR_TextFragmentWidth2(buf, TRACKING) * cfg.statusbarScale;
    if(NULL != height) *height = FR_TextFragmentHeight(buf) * cfg.statusbarScale;
    }
#undef TRACKING
}

void ReadyAmmoIcon_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_readyammoicon_t* icon = (guidata_readyammoicon_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    int lvl = (plr->powers[PT_WEAPONLEVEL2]? 1 : 0);
    ammotype_t ammoType;

    icon->patchId = 0;
    if(!(plr->readyWeapon > 0 && plr->readyWeapon < 7))
        return;
    for(ammoType = 0; ammoType < NUM_AMMO_TYPES; ++ammoType)
    {
        if(!weaponInfo[plr->readyWeapon][plr->class].mode[lvl].ammoType[ammoType])
            continue; // Weapon does not use this type of ammo.
        icon->patchId = pAmmoIcons[ammoType].id;
        break;
    }
    }
}

void SBarReadyAmmoIcon_Drawer(uiwidget_t* obj, int x, int y)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX+ST_AMMOICONX)
#define Y                   (ORIGINY+ST_AMMOICONY)

    assert(NULL != obj);
    {
    guidata_readyammoicon_t* icon = (guidata_readyammoicon_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = fullscreenMode();
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(obj->player))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(icon->patchId == 0)
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    WI_DrawPatch4(icon->patchId, Hu_ChoosePatchReplacement(icon->patchId), X, Y, DPF_ALIGN_TOPLEFT, FID(GF_FONTB), 1, 1, 1, iconAlpha);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarReadyAmmoIcon_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_readyammoicon_t* icon = (guidata_readyammoicon_t*)obj->typedata;
    patchinfo_t info;

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(Hu_InventoryIsOpen(obj->player))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(icon->patchId == 0)
        return;
    if(!R_GetPatchInfo(icon->patchId, &info))
        return;

    if(NULL != width)  *width  = info.width  * cfg.statusbarScale;
    if(NULL != height) *height = info.height * cfg.statusbarScale;
    }
}

void ReadyItem_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_readyitem_t* item = (guidata_readyitem_t*)obj->typedata;
    if(item->flashCounter > 0)
        --item->flashCounter;
    if(item->flashCounter > 0)
    {
        item->patchId = pInvItemFlash[item->flashCounter % 5].id;
    }
    else
    {
        inventoryitemtype_t readyItem = P_InventoryReadyItem(obj->player);
        if(readyItem != IIT_NONE)
        {
            item->patchId = P_GetInvItem(readyItem-1)->patchId;
        }
        else
        {
            item->patchId = 0;
        }
    }
    }
}

void SBarReadyItem_Drawer(uiwidget_t* obj, int xOffset, int yOffset)
{
#define ORIGINX (-ST_WIDTH/2)
#define ORIGINY (-ST_HEIGHT*hud->showBar)

    assert(NULL != obj);
    {
    guidata_readyitem_t* item = (guidata_readyitem_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int fullscreen = fullscreenMode();
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    inventoryitemtype_t readyItem;

    if(Hu_InventoryIsOpen(obj->player))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    if(item->patchId != 0)
    {
        int x, y;
       
        if(item->flashCounter > 0)
        {
            x = ORIGINX+ST_INVITEMX + 2;
            y = ORIGINY+ST_INVITEMY + 1;
        }
        else
        {
            x = ORIGINX+ST_INVITEMX;
            y = ORIGINY+ST_INVITEMY;
        }

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        DGL_Translatef(xOffset, yOffset, 0);
        DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
        DGL_Enable(DGL_TEXTURE_2D);

        DGL_Color4f(1, 1, 1, iconAlpha);
        GL_DrawPatch(item->patchId, x, y);

        readyItem = P_InventoryReadyItem(obj->player);
        if(!(item->flashCounter > 0) && IIT_NONE != readyItem)
        {
            uint count = P_InventoryCount(obj->player, readyItem);
            if(count > 1)
            {
                char buf[20];
                dd_snprintf(buf, 20, "%i", count);

                FR_SetFont(obj->fontId);
                DGL_Color4f(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
                FR_DrawTextFragment3(buf, ORIGINX+ST_INVITEMCX, ORIGINY+ST_INVITEMCY, DTF_ALIGN_TOPRIGHT|DTF_NO_EFFECTS, 2);
            }
        }

        DGL_Disable(DGL_TEXTURE_2D);
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }
    }
#undef ORIGINY
#undef ORIGINX
}

void SBarReadyItem_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(Hu_InventoryIsOpen(obj->player))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    // \fixme Calculate dimensions!
    //if(NULL != width)  *width  = ? * cfg.statusbarScale;
    //if(NULL != height) *height = ? * cfg.statusbarScale;
}

void ST_FlashCurrentItem(int player)
{
    player_t* plr;
    hudstate_t* hud;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    plr = &players[player];
    if(!((plr->plr->flags & DDPF_LOCAL) && plr->plr->inGame))
        return;

    hud = &hudStates[player];
    hud->sbarReadyitem.flashCounter = 4;
    hud->readyitem.flashCounter = 4;
}

/**
 * Unhides the current HUD display if hidden.
 *
 * @param player        The player whoose HUD to (maybe) unhide.
 * @param event         The HUD Update Event type to check for triggering.
 */
void ST_HUDUnHide(int player, hueevent_t ev)
{
    player_t* plr;

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

void Flight_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_flight_t* flht = (guidata_flight_t*)obj->typedata;
    const player_t* plr = &players[obj->player];

    flht->patchId = 0;
    if(0 == plr->powers[PT_FLIGHT])
        return;

    if(plr->powers[PT_FLIGHT] > BLINKTHRESHOLD || !(plr->powers[PT_FLIGHT] & 16))
    {
        int frame = (mapTime / 3) & 15;
        if(plr->plr->mo->flags2 & MF2_FLY)
        {
            if(flht->hitCenterFrame && (frame != 15 && frame != 0))
                frame = 15;
            else               
                flht->hitCenterFrame = false;
        }
        else
        {
            if(!flht->hitCenterFrame && (frame != 15 && frame != 0))
            {
                flht->hitCenterFrame = false;
            }
            else
            {
                frame = 15;
                flht->hitCenterFrame = true;
            }
        }
        flht->patchId = pSpinFly[frame].id;
    }
    }
}

void Flight_Drawer(uiwidget_t* obj, int x, int y)
{
    assert(NULL != obj);
    {
    guidata_flight_t* flht = (guidata_flight_t*)obj->typedata;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    if(0 != flht->patchId)
    {
        const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        DGL_Translatef(x, y, 0);
        DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
        DGL_Enable(DGL_TEXTURE_2D);

        DGL_Color4f(1, 1, 1, iconAlpha);
        GL_DrawPatch(flht->patchId, 16, 14);

        DGL_Disable(DGL_TEXTURE_2D);
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }
    }
}

void Flight_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_flight_t* flht = (guidata_flight_t*)obj->typedata;

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(0 == flht->patchId)
        return;

    /// \fixme Calculate dimensions properly!
    if(NULL != width)  *width  = 32 * cfg.hudScale;
    if(NULL != height) *height = 32 * cfg.hudScale;
    }
}

void Tome_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_tomeofpower_t* tome = (guidata_tomeofpower_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    const int ticsRemain = plr->powers[PT_WEAPONLEVEL2];

    tome->patchId = 0;
    tome->countdownSeconds = 0;

    if(0 == ticsRemain || 0 != plr->morphTics)
        return;

    // Time to player the countdown sound?
    if(0 != ticsRemain && ticsRemain < cfg.tomeSound * TICSPERSEC)
    {
        int timeleft = ticsRemain / TICSPERSEC;
        if(tome->play != timeleft)
        {
            tome->play = timeleft;
            S_LocalSound(SFX_KEYUP, NULL);
        }
    }

    if(cfg.tomeCounter > 0 || ticsRemain > BLINKTHRESHOLD || !(ticsRemain & 16))
    {
        int frame = (mapTime / 3) & 15;
        tome->patchId = pSpinTome[frame].id;
    }

    if(cfg.tomeCounter > 0 && ticsRemain < cfg.tomeCounter * TICSPERSEC)
    {
        tome->countdownSeconds = 1 + ticsRemain / TICSPERSEC;
    }
    }
}

void Tome_Drawer(uiwidget_t* obj, int x, int y)
{
    assert(NULL != obj);
    {
    guidata_tomeofpower_t* tome = (guidata_tomeofpower_t*)obj->typedata;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(tome->patchId == 0 && tome->countdownSeconds == 0)
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);

    if(tome->patchId != 0)
    {
        float alpha = uiRendState->pageAlpha * cfg.hudIconAlpha;
        if(tome->countdownSeconds != 0)
            alpha *= tome->countdownSeconds / (float)cfg.tomeCounter;

        DGL_Enable(DGL_TEXTURE_2D);

        DGL_Color4f(1, 1, 1, alpha);
        GL_DrawPatch(tome->patchId, -13, 13);

        DGL_Disable(DGL_TEXTURE_2D);
    }

    if(tome->countdownSeconds != 0)
    {
#define COUNT_X             (303)
#define COUNT_Y             (30)
#define TRACKING            (2)

        const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
        char buf[20];
        dd_snprintf(buf, 20, "%i", tome->countdownSeconds);

        DGL_Enable(DGL_TEXTURE_2D);

        FR_SetFont(obj->fontId);
        DGL_Color4f(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
        FR_DrawTextFragment3(buf, 0, 25 + 2, DTF_ALIGN_TOPRIGHT|DTF_NO_EFFECTS, TRACKING);

        DGL_Disable(DGL_TEXTURE_2D);

#undef TRACKING
#undef COUNT_Y
#undef COUNT_X
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void Tome_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_tomeofpower_t* tome = (guidata_tomeofpower_t*)obj->typedata;

    if(NULL != width)  *width = 0;
    if(NULL != height) *height = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(tome->patchId == 0 && tome->countdownSeconds == 0)
        return;

    if(tome->patchId != 0)
    {
        // \fixme Determine the actual center point of the animation at widget creation time.
        if(NULL != width)  *width  += 25;
        if(NULL != height) *height += 25;
    }

    if(tome->countdownSeconds != 0)
    {
#define TRACKING            (2)

        char buf[20];
        int w;
        dd_snprintf(buf, 20, "%i", tome->countdownSeconds);
        FR_SetFont(obj->fontId);
        w = FR_TextFragmentWidth2(buf, TRACKING);
        if(NULL != width && w > *width)
            *width += w;
        if(NULL != height)
            *height += FR_TextFragmentHeight(buf) + 2;

#undef TRACKING
    }

    if(NULL != width)  *width  = *width  * cfg.hudScale;
    if(NULL != height) *height = *height * cfg.hudScale;
    }
}

/*
static boolean pickStatusbarScalingStrategy(int viewportWidth, int viewportHeight)
{
    float a = (float)viewportWidth/viewportHeight;
    float b = (float)SCREENWIDTH/SCREENHEIGHT;

    if(INRANGE_OF(a, b, .001f))
        return true; // The same, so stretch.
    if(Con_GetByte("rend-hud-stretch") || !INRANGE_OF(a, b, .38f))
        return false; // No stretch; translate and scale to fit.
    // Otherwise stretch.
    return true;
}

static void old_drawStatusbar(int player, int x, int y, int viewW, int viewH)
{
    hudstate_t* hud = &hudStates[player];
    int needWidth;
    float scaleX, scaleY;

    if(!hud->statusbarActive)
        return;

    needWidth = ((viewW >= viewH)? (float)viewH/SCREENHEIGHT : (float)viewW/SCREENWIDTH) * ST_WIDTH;
    scaleX = scaleY = cfg.statusbarScale;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);

    if(pickStatusbarScalingStrategy(viewW, viewH))
    {
        scaleX *= (float)viewW/needWidth;
    }
    else
    {
        if(needWidth > viewW)
        {
            scaleX *= (float)viewW/needWidth;
            scaleY *= (float)viewW/needWidth;
        }
    }

    DGL_Scalef(scaleX, scaleY, 1);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}
*/

void ReadyAmmoIcon_Drawer(uiwidget_t* obj, int x, int y)
{
    assert(NULL != obj);
    {
    guidata_readyammoicon_t* icon = (guidata_readyammoicon_t*)obj->typedata;
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(icon->patchId == 0)
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(1, 1, 1, iconAlpha);
    GL_DrawPatch2(icon->patchId, 0, 0, DPF_ALIGN_TOPLEFT|DPF_NO_OFFSET);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void ReadyAmmoIcon_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_readyammoicon_t* icon = (guidata_readyammoicon_t*)obj->typedata;
    patchinfo_t info;

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(icon->patchId == 0)
        return;
    if(!R_GetPatchInfo(icon->patchId, &info))
        return;

    if(NULL != width) *width   = info.width  * cfg.hudScale;
    if(NULL != height) *height = info.height * cfg.hudScale;
    }
}

void ReadyAmmo_Drawer(uiwidget_t* obj, int x, int y)
{
#define TRACKING                (1)

    assert(NULL != obj);
    {
    guidata_readyammo_t* ammo = (guidata_readyammo_t*)obj->typedata;
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    char buf[20];

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(ammo->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", ammo->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->fontId);
    DGL_Color4f(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    FR_DrawTextFragment3(buf, 0, -2, DTF_ALIGN_TOPLEFT|DTF_NO_EFFECTS, TRACKING);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef TRACKING
}

void ReadyAmmo_Dimensions(uiwidget_t* obj, int* width, int* height)
{
#define TRACKING                (1)

    assert(NULL != obj);
    {
    guidata_readyammo_t* ammo = (guidata_readyammo_t*)obj->typedata;
    char buf[20];

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(ammo->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", ammo->value);
    FR_SetFont(obj->fontId);
    if(NULL != width)  *width  = FR_TextFragmentWidth2(buf, TRACKING) * cfg.hudScale;
    if(NULL != height) *height = FR_TextFragmentHeight(buf) * cfg.hudScale;
    }
#undef TRACKING
}

void Health_Drawer(uiwidget_t* obj, int x, int y)
{
#define TRACKING                (1)

    assert(NULL != obj);
    {
    guidata_health_t* hlth = (guidata_health_t*)obj->typedata;
    int health = MAX_OF(hlth->value, 0);
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    char buf[20];

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);

    dd_snprintf(buf, 5, "%i", health);

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->fontId);
    DGL_Color4f(0, 0, 0, textAlpha * .4f);
    FR_DrawTextFragment3(buf, 2, 1, DTF_ALIGN_BOTTOMLEFT|DTF_NO_EFFECTS, TRACKING);

    DGL_Color4f(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    FR_DrawTextFragment3(buf, 0, -1, DTF_ALIGN_BOTTOMLEFT|DTF_NO_EFFECTS, TRACKING);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef TRACKING
}

void Health_Dimensions(uiwidget_t* obj, int* width, int* height)
{
#define TRACKING                (1)

    assert(NULL != obj);
    {
    guidata_health_t* hlth = (guidata_health_t*)obj->typedata;
    int health = MAX_OF(hlth->value, 0);
    char buf[20];

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    dd_snprintf(buf, 5, "%i", health);
    FR_SetFont(obj->fontId);
    if(NULL != width)  *width  = FR_TextFragmentWidth2(buf, TRACKING) * cfg.hudScale;
    if(NULL != height) *height = FR_TextFragmentHeight(buf) * cfg.hudScale;
    }
#undef TRACKING
}

void Armor_Drawer(uiwidget_t* obj, int x, int y)
{
#define TRACKING                (1)

    assert(NULL != obj);
    {
    guidata_armor_t* armor = (guidata_armor_t*)obj->typedata;
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    char buf[20];

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(armor->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", armor->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->fontId);
    DGL_Color4f(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    FR_DrawTextFragment3(buf, -1, -11, DTF_ALIGN_TOPLEFT|DTF_NO_EFFECTS, TRACKING);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef TRACKING
}

void Armor_Dimensions(uiwidget_t* obj, int* width, int* height)
{
#define TRACKING                (1)

    assert(NULL != obj);
    {
    guidata_armor_t* armor = (guidata_armor_t*)obj->typedata;
    char buf[20];

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(armor->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", armor->value);
    FR_SetFont(obj->fontId);
    if(NULL != width)  *width  = FR_TextFragmentWidth2(buf, TRACKING) * cfg.hudScale;
    if(NULL != height) *height = FR_TextFragmentHeight(buf) * cfg.hudScale;
    }
#undef TRACKING
}

void Keys_Drawer(uiwidget_t* obj, int xOffset, int yOffset)
{
    assert(NULL != obj);
    {
    guidata_keys_t* keys = (guidata_keys_t*)obj->typedata;
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;
    int x = 0; 

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(xOffset, yOffset, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);

    if(keys->keyBoxes[KT_YELLOW])
    {
        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconAlpha);
        GL_DrawPatch2(pKeys[0].id, x, -pKeys[0].height, DPF_ALIGN_TOPLEFT|DPF_NO_OFFSET);
        DGL_Disable(DGL_TEXTURE_2D);

        x += pKeys[0].width + 1;
    }

    if(keys->keyBoxes[KT_GREEN])
    {
        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconAlpha);
        GL_DrawPatch2(pKeys[1].id, x, -pKeys[1].height, DPF_ALIGN_TOPLEFT|DPF_NO_OFFSET);
        DGL_Disable(DGL_TEXTURE_2D);

        x += pKeys[1].width + 1;
    }

    if(keys->keyBoxes[KT_BLUE])
    {
        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconAlpha);
        GL_DrawPatch2(pKeys[2].id, x, -pKeys[2].height, DPF_ALIGN_TOPLEFT|DPF_NO_OFFSET);
        DGL_Disable(DGL_TEXTURE_2D);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void Keys_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_keys_t* keys = (guidata_keys_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    int i;
    for(i = 0; i < 3; ++i)
    {
        keys->keyBoxes[i] = plr->keys[i] ? true : false;
    }
    }
}

void Keys_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_keys_t* keys = (guidata_keys_t*)obj->typedata;
    int x = 0, numVisible = 0; 

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    if(keys->keyBoxes[KT_YELLOW])
    {
        x += pKeys[0].width + 1;
        if(NULL != width)
            *width += pKeys[0].width;
        if(NULL != height && pKeys[0].height > *height)
            *height = pKeys[0].height;
        ++numVisible;
    }

    if(keys->keyBoxes[KT_GREEN])
    {
        x += pKeys[1].width + 1;
        if(NULL != width)
            *width += pKeys[1].width;
        if(NULL != height && pKeys[1].height > *height)
            *height = pKeys[1].height;
        ++numVisible;
    }

    if(keys->keyBoxes[KT_BLUE])
    {
        x += pKeys[2].width;
        if(NULL != width)
            *width += pKeys[1].width;
        if(NULL != height && pKeys[2].height > *height)
            *height = pKeys[2].height;
        ++numVisible;
    }
    if(NULL != width)
        *width += (numVisible-1) * 1;

    if(NULL != width)  *width  = *width  * cfg.hudScale;
    if(NULL != height) *height = *height * cfg.hudScale;
    }
}

void Frags_Drawer(uiwidget_t* obj, int x, int y)
{
#define TRACKING                (1)

    assert(NULL != obj);
    {
    guidata_frags_t* frags = (guidata_frags_t*)obj->typedata;
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    char buf[20];

    if(!deathmatch)
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(frags->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", frags->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->fontId);
    DGL_Color4f(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    FR_DrawTextFragment3(buf, 0, 0, DTF_ALIGN_BOTTOMLEFT|DTF_NO_EFFECTS, TRACKING);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef TRACKING
}

void Frags_Dimensions(uiwidget_t* obj, int* width, int* height)
{
#define TRACKING                (1)

    assert(NULL != obj);
    {
    guidata_frags_t* frags = (guidata_frags_t*)obj->typedata;
    char buf[20];

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(!deathmatch)
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(frags->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", frags->value);

    FR_SetFont(obj->fontId);
    if(NULL != width)  *width  = FR_TextFragmentWidth2(buf, TRACKING) * cfg.hudScale;
    if(NULL != height) *height = FR_TextFragmentHeight(buf) * cfg.hudScale;
    }
#undef TRACKING
}

void ReadyItem_Drawer(uiwidget_t* obj, int x, int y)
{
    assert(NULL != obj);
    {
    guidata_readyitem_t* item = (guidata_readyitem_t*)obj->typedata;
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;
    inventoryitemtype_t readyItem;
    patchinfo_t boxInfo;

    if(Hu_InventoryIsOpen(obj->player))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(!R_GetPatchInfo(pInvItemBox, &boxInfo))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);

    if(item->patchId != 0)
    {
        int xOffset = 0, yOffset = 0;
        if(item->flashCounter > 0)
        {
            xOffset = 2;
            yOffset = 1;
        }

        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconAlpha/2);
        GL_DrawPatch2(pInvItemBox, 0, 0, DPF_ALIGN_BOTTOMRIGHT|DPF_NO_OFFSET);
        DGL_Color4f(1, 1, 1, iconAlpha);
        GL_DrawPatch(item->patchId, -boxInfo.width + xOffset, -boxInfo.height + yOffset);
        DGL_Disable(DGL_TEXTURE_2D);
    }

    readyItem = P_InventoryReadyItem(obj->player);
    if(!(item->flashCounter > 0) && readyItem != IIT_NONE)
    {
        uint count;
        if((count = P_InventoryCount(obj->player, readyItem)) > 1)
        {
            char buf[20];
            DGL_Enable(DGL_TEXTURE_2D);
            FR_SetFont(obj->fontId);
            DGL_Color4f(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
            dd_snprintf(buf, 20, "%i", count);
            FR_DrawTextFragment3(buf, -1, -7, DTF_ALIGN_TOPRIGHT|DTF_NO_EFFECTS, 2);
            DGL_Disable(DGL_TEXTURE_2D);
        }
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void ReadyItem_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    patchinfo_t boxInfo;

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(Hu_InventoryIsOpen(obj->player))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(!R_GetPatchInfo(pInvItemBox, &boxInfo))
        return;

    if(NULL != width)  *width  = boxInfo.width  * cfg.hudScale;
    if(NULL != height) *height = boxInfo.height * cfg.hudScale;
    }
}

void Inventory_Drawer(uiwidget_t* obj, int x, int y)
{
#define INVENTORY_HEIGHT    29
#define EXTRA_SCALE         .75f

    assert(NULL != obj);
    {
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;

    if(!Hu_InventoryIsOpen(obj->player))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(EXTRA_SCALE * cfg.hudScale, EXTRA_SCALE * cfg.hudScale, 1);

    Hu_InventoryDraw(obj->player, 0, -INVENTORY_HEIGHT, textAlpha, iconAlpha);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef EXTRA_SCALE
#undef INVENTORY_HEIGHT
}

void Inventory_Dimensions(uiwidget_t* obj, int* width, int* height)
{
#define INVENTORY_HEIGHT    29
#define EXTRA_SCALE         .75f

    assert(NULL != obj);

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(!Hu_InventoryIsOpen(obj->player))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    /// \fixme Calculate the visual dimensions properly!
    if(NULL != width)  *width  = (31*7+16*2) * EXTRA_SCALE * cfg.hudScale;
    if(NULL != height) *height = INVENTORY_HEIGHT * EXTRA_SCALE * cfg.hudScale;

#undef EXTRA_SCALE
#undef INVENTORY_HEIGHT
}

void Kills_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_kills_t* kills = (guidata_kills_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    kills->value = plr->killCount;
    }
}

void Kills_Drawer(uiwidget_t* obj, int x, int y)
{
    assert(NULL != obj);
    {
    guidata_kills_t* kills = (guidata_kills_t*)obj->typedata;
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    char buf[40], tmp[20];

    if(!(cfg.hudShownCheatCounters & (CCH_KILLS | CCH_KILLS_PRCNT)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(kills->value == 1994)
        return;

    strcpy(buf, "Kills: ");
    if(cfg.hudShownCheatCounters & CCH_KILLS)
    {
        sprintf(tmp, "%i/%i ", kills->value, totalKills);
        strcat(buf, tmp);
    }
    if(cfg.hudShownCheatCounters & CCH_KILLS_PRCNT)
    {
        sprintf(tmp, "%s%i%%%s", (cfg.hudShownCheatCounters & CCH_KILLS ? "(" : ""), totalKills ? kills->value * 100 / totalKills : 100, (cfg.hudShownCheatCounters & CCH_KILLS ? ")" : ""));
        strcat(buf, tmp);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.hudCheatCounterScale, cfg.hudCheatCounterScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->fontId);
    DGL_Color4f(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    FR_DrawTextFragment2(buf, 0, 0, DTF_ALIGN_BOTTOMLEFT|DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void Kills_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_kills_t* kills = (guidata_kills_t*)obj->typedata;
    char buf[40], tmp[20];

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(!(cfg.hudShownCheatCounters & (CCH_KILLS | CCH_KILLS_PRCNT)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(kills->value == 1994)
        return;

    strcpy(buf, "Kills: ");
    if(cfg.hudShownCheatCounters & CCH_KILLS)
    {
        sprintf(tmp, "%i/%i ", kills->value, totalKills);
        strcat(buf, tmp);
    }
    if(cfg.hudShownCheatCounters & CCH_KILLS_PRCNT)
    {
        sprintf(tmp, "%s%i%%%s", (cfg.hudShownCheatCounters & CCH_KILLS ? "(" : ""), totalKills ? kills->value * 100 / totalKills : 100, (cfg.hudShownCheatCounters & CCH_KILLS ? ")" : ""));
        strcat(buf, tmp);
    }

    FR_SetFont(obj->fontId);
    if(NULL != width)  *width  = FR_TextFragmentWidth(buf)  * cfg.hudCheatCounterScale;
    if(NULL != height) *height = FR_TextFragmentHeight(buf) * cfg.hudCheatCounterScale;
    }
}

void Items_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_items_t* items = (guidata_items_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    items->value = plr->itemCount;
    }
}

void Items_Drawer(uiwidget_t* obj, int x, int y)
{
    assert(NULL != obj);
    {
    guidata_items_t* items = (guidata_items_t*)obj->typedata;
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    char buf[40], tmp[20];

    if(!(cfg.hudShownCheatCounters & (CCH_ITEMS | CCH_ITEMS_PRCNT)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(items->value == 1994)
        return;

    strcpy(buf, "Items: ");
    if(cfg.hudShownCheatCounters & CCH_ITEMS)
    {
        sprintf(tmp, "%i/%i ", items->value, totalItems);
        strcat(buf, tmp);
    }
    if(cfg.hudShownCheatCounters & CCH_ITEMS_PRCNT)
    {
        sprintf(tmp, "%s%i%%%s", (cfg.hudShownCheatCounters & CCH_ITEMS ? "(" : ""), totalItems ? items->value * 100 / totalItems : 100, (cfg.hudShownCheatCounters & CCH_ITEMS ? ")" : ""));
        strcat(buf, tmp);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.hudCheatCounterScale, cfg.hudCheatCounterScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->fontId);
    DGL_Color4f(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    FR_DrawTextFragment2(buf, 0, 0, DTF_ALIGN_BOTTOMLEFT|DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void Items_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_items_t* items = (guidata_items_t*)obj->typedata;
    char buf[40], tmp[20];

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(!(cfg.hudShownCheatCounters & (CCH_ITEMS | CCH_ITEMS_PRCNT)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(items->value == 1994)
        return;

    strcpy(buf, "Items: ");
    if(cfg.hudShownCheatCounters & CCH_ITEMS)
    {
        sprintf(tmp, "%i/%i ", items->value, totalItems);
        strcat(buf, tmp);
    }
    if(cfg.hudShownCheatCounters & CCH_ITEMS_PRCNT)
    {
        sprintf(tmp, "%s%i%%%s", (cfg.hudShownCheatCounters & CCH_ITEMS ? "(" : ""), totalItems ? items->value * 100 / totalItems : 100, (cfg.hudShownCheatCounters & CCH_ITEMS ? ")" : ""));
        strcat(buf, tmp);
    }

    FR_SetFont(obj->fontId);
    if(NULL != width)  *width  = FR_TextFragmentWidth(buf)  * cfg.hudCheatCounterScale;
    if(NULL != height) *height = FR_TextFragmentHeight(buf) * cfg.hudCheatCounterScale;
    }
}

void Secrets_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_secrets_t* scrt = (guidata_secrets_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    scrt->value = plr->secretCount;
    }
}

void Secrets_Drawer(uiwidget_t* obj, int x, int y)
{
    assert(NULL != obj);
    {
    guidata_secrets_t* scrt = (guidata_secrets_t*)obj->typedata;
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    char buf[40], tmp[20];

    if(!(cfg.hudShownCheatCounters & (CCH_SECRETS | CCH_SECRETS_PRCNT)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(scrt->value == 1994)
        return;

    strcpy(buf, "Secret: ");
    if(cfg.hudShownCheatCounters & CCH_SECRETS)
    {
        sprintf(tmp, "%i/%i ", scrt->value, totalSecret);
        strcat(buf, tmp);
    }
    if(cfg.hudShownCheatCounters & CCH_SECRETS_PRCNT)
    {
        sprintf(tmp, "%s%i%%%s", (cfg.hudShownCheatCounters & CCH_SECRETS ? "(" : ""), totalSecret ? scrt->value * 100 / totalSecret : 100, (cfg.hudShownCheatCounters & CCH_SECRETS ? ")" : ""));
        strcat(buf, tmp);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.hudCheatCounterScale, cfg.hudCheatCounterScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->fontId);
    DGL_Color4f(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    FR_DrawTextFragment2(buf, 0, 0, DTF_ALIGN_BOTTOMLEFT|DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void Secrets_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_secrets_t* scrt = (guidata_secrets_t*)obj->typedata;
    char buf[40], tmp[20];

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(!(cfg.hudShownCheatCounters & (CCH_SECRETS | CCH_SECRETS_PRCNT)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(scrt->value == 1994)
        return;

    strcpy(buf, "Secret: ");
    if(cfg.hudShownCheatCounters & CCH_SECRETS)
    {
        sprintf(tmp, "%i/%i ", scrt->value, totalSecret);
        strcat(buf, tmp);
    }
    if(cfg.hudShownCheatCounters & CCH_SECRETS_PRCNT)
    {
        sprintf(tmp, "%s%i%%%s", (cfg.hudShownCheatCounters & CCH_SECRETS ? "(" : ""), totalSecret ? scrt->value * 100 / totalSecret : 100, (cfg.hudShownCheatCounters & CCH_SECRETS ? ")" : ""));
        strcat(buf, tmp);
    }

    FR_SetFont(obj->fontId);
    if(NULL != width)  *width  = FR_TextFragmentWidth(buf)  * cfg.hudCheatCounterScale;
    if(NULL != height) *height = FR_TextFragmentHeight(buf) * cfg.hudCheatCounterScale;
    }
}

void Log_Drawer2(uiwidget_t* obj, int x, int y)
{
    assert(NULL != obj);
    {
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;

    /// \kludge Do not draw message logs while the map title is being displayed.
    if(cfg.mapTitle && actualMapTime < 6 * 35)
        return;
    /// kludge end.

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.msgScale, cfg.msgScale, 1);

    Hu_LogDrawer(obj->player, textAlpha);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void Log_Dimensions2(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    if(NULL != width)  *width = 0;
    if(NULL != height) *height = 0;

    /// \kludge Do not draw message logs while the map title is being displayed.
    if(cfg.mapTitle && actualMapTime < 6 * 35)
        return;
    /// kludge end.

    Hu_LogDimensions(obj->player, width, height);

    if(NULL != width)  *width  = *width  * cfg.msgScale;
    if(NULL != height) *height = *height * cfg.msgScale;
}

void Chat_Drawer2(uiwidget_t* obj, int x, int y)
{
    assert(NULL != obj);
    {
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.msgScale, cfg.msgScale, 1);

    Chat_Drawer(obj->player, textAlpha, iconAlpha);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void Chat_Dimensions2(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    if(NULL != width)  *width = 0;
    if(NULL != height) *height = 0;

    Chat_Dimensions(obj->player, width, height);

    if(NULL != width)  *width  = *width  * cfg.msgScale;
    if(NULL != height) *height = *height * cfg.msgScale;
}

typedef struct {
    guiwidgettype_t type;
    int group;
    int hideId;
    gamefontid_t fontIdx;
    void (*dimensions) (uiwidget_t* obj, int* width, int* height); 
    void (*drawer) (uiwidget_t* obj, int x, int y);
    void (*ticker) (uiwidget_t* obj);
    void* typedata;
} uiwidgetdef_t;

typedef struct {
    int group;
    short flags;
    int padding; // In fixed 320x200 pixels.
} uiwidgetgroupdef_t;

void ST_Drawer(int player)
{
    hudstate_t* hud;
    player_t* plr;
    int fullscreen = fullscreenMode();
    boolean blended = (fullscreen!=0);

    if(player < 0 || player >= MAXPLAYERS)
        return;

    plr = &players[player];
    if(!((plr->plr->flags & DDPF_LOCAL) && plr->plr->inGame))
        return;

    hud = &hudStates[player];
    if(!hud->inited)
    {
#define PADDING 2 // In fixed 320x200 units.

        const uiwidgetgroupdef_t widgetGroupDefs[] = {
            { UWG_STATUSBAR,    UWGF_ALIGN_BOTTOM },
            { UWG_TOPLEFT,      UWGF_ALIGN_TOP|UWGF_ALIGN_LEFT|UWGF_LEFTTORIGHT, PADDING },
            { UWG_TOPLEFT2,     UWGF_ALIGN_TOP|UWGF_ALIGN_LEFT|UWGF_LEFTTORIGHT, PADDING },
            { UWG_TOPRIGHT,     UWGF_ALIGN_TOP|UWGF_ALIGN_RIGHT|UWGF_RIGHTTOLEFT, PADDING },
            { UWG_BOTTOMLEFT,   UWGF_ALIGN_BOTTOM|UWGF_ALIGN_LEFT|UWGF_VERTICAL|UWGF_RIGHTTOLEFT, PADDING },
            { UWG_BOTTOMLEFT2,  UWGF_ALIGN_BOTTOM|UWGF_ALIGN_LEFT|UWGF_LEFTTORIGHT, PADDING },
            { UWG_BOTTOMRIGHT,  UWGF_ALIGN_BOTTOM|UWGF_ALIGN_RIGHT|UWGF_RIGHTTOLEFT, PADDING },
            { UWG_BOTTOM,       UWGF_ALIGN_BOTTOM|UWGF_VERTICAL|UWGF_RIGHTTOLEFT, PADDING },
            { UWG_TOP,          UWGF_ALIGN_TOP|UWGF_ALIGN_LEFT|UWGF_VERTICAL|UWGF_LEFTTORIGHT, PADDING },
            { UWG_COUNTERS,     UWGF_ALIGN_LEFT|UWGF_VERTICAL|UWGF_RIGHTTOLEFT, PADDING }
        };
        const uiwidgetdef_t widgetDefs[] = {
            { GUI_BOX,          UWG_STATUSBAR,    -1,         0,            SBarBackground_Dimensions, SBarBackground_Drawer },
            { GUI_INVENTORY,    UWG_STATUSBAR,    -1,         GF_SMALLIN,   SBarInventory_Dimensions, SBarInventory_Drawer },
            { GUI_FRAGS,        UWG_STATUSBAR,    -1,         GF_STATUS,    SBarFrags_Dimensions, SBarFrags_Drawer, Frags_Ticker, &hud->sbarFrags },
            { GUI_HEALTH,       UWG_STATUSBAR,    -1,         GF_STATUS,    SBarHealth_Dimensions, SBarHealth_Drawer, Health_Ticker, &hud->sbarHealth },
            { GUI_ARMOR,        UWG_STATUSBAR,    -1,         GF_STATUS,    SBarArmor_Dimensions, SBarArmor_Drawer, Armor_Ticker, &hud->sbarArmor },
            { GUI_KEYSLOT,      UWG_STATUSBAR,    -1,         0,            KeySlot_Dimensions, KeySlot_Drawer, KeySlot_Ticker, &hud->sbarKeyslots[0] },
            { GUI_KEYSLOT,      UWG_STATUSBAR,    -1,         0,            KeySlot_Dimensions, KeySlot_Drawer, KeySlot_Ticker, &hud->sbarKeyslots[1] },
            { GUI_KEYSLOT,      UWG_STATUSBAR,    -1,         0,            KeySlot_Dimensions, KeySlot_Drawer, KeySlot_Ticker, &hud->sbarKeyslots[2] },
            { GUI_READYAMMO,    UWG_STATUSBAR,    -1,         GF_STATUS,    SBarReadyAmmo_Dimensions, SBarReadyAmmo_Drawer, ReadyAmmo_Ticker, &hud->sbarReadyammo },
            { GUI_READYAMMOICON, UWG_STATUSBAR,   -1,         0,            SBarReadyAmmoIcon_Dimensions, SBarReadyAmmoIcon_Drawer, ReadyAmmoIcon_Ticker, &hud->sbarReadyammoicon },
            { GUI_READYITEM,    UWG_STATUSBAR,    -1,         GF_SMALLIN,   SBarReadyItem_Dimensions, SBarReadyItem_Drawer, ReadyItem_Ticker, &hud->sbarReadyitem },
            { GUI_CHAIN,        UWG_STATUSBAR,    -1,         0,            SBarChain_Dimensions, SBarChain_Drawer, SBarChain_Ticker, &hud->sbarChain },
            { GUI_READYAMMOICON, UWG_TOPLEFT,     HUD_AMMO,   0,            ReadyAmmoIcon_Dimensions, ReadyAmmoIcon_Drawer, ReadyAmmoIcon_Ticker, &hud->readyammoicon },
            { GUI_READYAMMO,    UWG_TOPLEFT,      HUD_AMMO,   GF_STATUS,    ReadyAmmo_Dimensions, ReadyAmmo_Drawer, ReadyAmmo_Ticker, &hud->readyammo },
            { GUI_FLIGHT,       UWG_TOPLEFT2,     -1,         0,            Flight_Dimensions, Flight_Drawer, Flight_Ticker, &hud->flight },
            { GUI_TOME,         UWG_TOPRIGHT,     -1,         GF_SMALLIN,   Tome_Dimensions, Tome_Drawer, Tome_Ticker, &hud->tome },
            { GUI_HEALTH,       UWG_BOTTOMLEFT,   HUD_HEALTH, GF_FONTB,     Health_Dimensions, Health_Drawer, Health_Ticker, &hud->health },
            { GUI_KEYS,         UWG_BOTTOMLEFT,   HUD_KEYS,   0,            Keys_Dimensions, Keys_Drawer, Keys_Ticker, &hud->keys },
            { GUI_ARMOR,        UWG_BOTTOMLEFT,   HUD_ARMOR,  GF_STATUS,    Armor_Dimensions, Armor_Drawer, Armor_Ticker, &hud->armor },
            { GUI_FRAGS,        UWG_BOTTOMLEFT2,  -1,         GF_STATUS,    Frags_Dimensions, Frags_Drawer, Frags_Ticker, &hud->frags },
            { GUI_READYITEM,    UWG_BOTTOMRIGHT,  HUD_READYITEM, GF_SMALLIN,ReadyItem_Dimensions, ReadyItem_Drawer, ReadyItem_Ticker, &hud->readyitem },
            { GUI_INVENTORY,    UWG_BOTTOM,       -1,         GF_SMALLIN,   Inventory_Dimensions, Inventory_Drawer },
            { GUI_LOG,          UWG_TOP,          -1,         GF_FONTA,     Log_Dimensions2, Log_Drawer2 },
            { GUI_CHAT,         UWG_TOP,          -1,         GF_FONTA,     Chat_Dimensions2, Chat_Drawer2 },
            { GUI_SECRETS,      UWG_COUNTERS,     -1,         GF_FONTA,     Secrets_Dimensions, Secrets_Drawer, Secrets_Ticker, &hud->secrets },
            { GUI_ITEMS,        UWG_COUNTERS,     -1,         GF_FONTA,     Items_Dimensions, Items_Drawer, Items_Ticker, &hud->items },
            { GUI_KILLS,        UWG_COUNTERS,     -1,         GF_FONTA,     Kills_Dimensions, Kills_Drawer, Kills_Ticker, &hud->kills },
            { GUI_NONE }
        };
        size_t i;

        for(i = 0; i < sizeof(widgetGroupDefs)/sizeof(widgetGroupDefs[0]); ++i)
        {
            const uiwidgetgroupdef_t* def = &widgetGroupDefs[i];
            hud->widgetGroupIds[def->group] = GUI_CreateGroup(player, def->flags, def->padding);
        }

        for(i = 0; widgetDefs[i].type != GUI_NONE; ++i)
        {
            const uiwidgetdef_t* def = &widgetDefs[i];
            uiwidgetid_t id = GUI_CreateWidget(def->type, player, def->hideId, FID(def->fontIdx), def->dimensions, def->drawer, def->ticker, def->typedata);
            UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[def->group]), GUI_FindObjectById(id));
        }

        // Initialize widgets according to player preferences.
        {
        short flags = UIGroup_Flags(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOP]));
        flags &= ~(UWGF_ALIGN_LEFT|UWGF_ALIGN_RIGHT);
        if(cfg.msgAlign == 0)
            flags |= UWGF_ALIGN_LEFT;
        else if(cfg.msgAlign == 2)
            flags |= UWGF_ALIGN_RIGHT;
        UIGroup_SetFlags(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOP]), flags);
        }

        hud->inited = true;

#undef PADDING
    }

    hud->statusbarActive = (fullscreen < 2) || (AM_IsActive(AM_MapForPlayer(player)) && (cfg.automapHudDisplay == 0 || cfg.automapHudDisplay == 2) );

    // Do palette shifts
    ST_doPaletteStuff(player);

    if(hud->statusbarActive || (fullscreen < 3 || hud->alpha > 0))
    {
        int viewW, viewH, x, y, width, height;
        float alpha = (hud->statusbarActive? (blended? (1-hud->hideAmount) : 1.0f) : hud->alpha * (1-hud->hideAmount));
        float scale;

        R_GetViewPort(player, NULL, NULL, &viewW, &viewH);

        if(viewW >= viewH)
            scale = (float)viewH/SCREENHEIGHT;
        else
            scale = (float)viewW/SCREENWIDTH;

        x = y = 0;
        width = viewW / scale;
        height = viewH / scale;

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();

        DGL_Scalef(scale, scale, 1);

        /**
         * Draw widgets.
         */
        {
#define PADDING 2 // In fixed 320x200 units.

        int posX, posY, availWidth, availHeight, drawnWidth, drawnHeight;

        if(hud->statusbarActive)
        {
            alpha = (1 - hud->hideAmount) * hud->showBar;
            GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_STATUSBAR]), x, y, width, height, alpha, &drawnWidth, &drawnHeight);
        }

        /**
         * Wide offset scaling.
         * Used with ultra-wide/tall resolutions to move the uiwidgets into
         * the viewer's primary field of vision (without this, uiwidgets
         * would be positioned at the very edges of the view window and
         * likely into the viewer's peripheral vision range).
         *
         * \note Statusbar is exempt because it is intended to extend over
         * the entire width of the view window and as such, uses another
         * special-case scale-positioning calculation.
         */
        if(cfg.hudWideOffset != 1)
        {
            if(viewW > viewH)
            {
                x = (viewW/2/scale -  SCREENWIDTH/2) * (1-cfg.hudWideOffset);
                width -= x*2;
            }
            else
            {
                y = (viewH/2/scale - SCREENHEIGHT/2) * (1-cfg.hudWideOffset);
                height -= y*2;
            }
        }

        alpha = hud->alpha * (1-hud->hideAmount);
        x += PADDING;
        y += PADDING;
        width -= PADDING*2;
        height -= PADDING*2;

        GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOP]), x, y, width, height, alpha, &drawnWidth, &drawnHeight);
        if(!hud->statusbarActive)
        {
            GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPLEFT]), x, y, width, height, alpha, &drawnWidth, &drawnHeight);
        }
        else
        {
            drawnWidth = 0;
        }

        posX = x + (drawnWidth > 0 ? drawnWidth + PADDING : 0);
        posY = y;
        availWidth = width - (drawnWidth > 0 ? drawnWidth + PADDING : 0);
        availHeight = height;
        GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPLEFT2]), posX, posY, availWidth, availHeight, alpha, &drawnWidth, &drawnHeight);

        GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPRIGHT]), x, y, width, height, alpha, &drawnWidth, &drawnHeight);
        if(!hud->statusbarActive)
        {
            GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOMLEFT]), x, y, width, height, alpha, &drawnWidth, &drawnHeight);

            posX = x + (drawnWidth > 0 ? drawnWidth + PADDING : 0);
            posY = y;
            availWidth = width - (drawnWidth > 0 ? drawnWidth + PADDING : 0);
            availHeight = height;
            GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOMLEFT2]), posX, posY, availWidth, availHeight, alpha, &drawnWidth, &drawnHeight);

            GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOMRIGHT]), x, y, width, height, alpha, &drawnWidth, &drawnHeight);
            GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOM]), x, y, width, height, alpha, &drawnWidth, &drawnHeight);
        }

        GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_COUNTERS]), x, y, width, height, alpha, &drawnWidth, &drawnHeight);
#undef PADDING
        }

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }
}

void ST_loadGraphics(void)
{
    char nameBuf[9];
    int i;

    R_PrecachePatch("BARBACK", &pStatusbar);
    R_PrecachePatch("INVBAR", &pInvBar);
    R_PrecachePatch("CHAIN", &pChain);

    R_PrecachePatch("STATBAR", &pStatBar);
    R_PrecachePatch("LIFEBAR", &pLifeBar);

    // Order of lifeGems changed to match player color index.
    R_PrecachePatch("LIFEGEM1", &pLifeGems[0]);
    R_PrecachePatch("LIFEGEM3", &pLifeGems[1]);
    R_PrecachePatch("LIFEGEM2", &pLifeGems[2]);
    R_PrecachePatch("LIFEGEM0", &pLifeGems[3]);

    R_PrecachePatch("GOD1", &pGodLeft);
    R_PrecachePatch("GOD2", &pGodRight);
    R_PrecachePatch("LTFCTOP", &pStatusbarTopLeft);
    R_PrecachePatch("RTFCTOP", &pStatusbarTopRight);
    for(i = 0; i < 16; ++i)
    {
        sprintf(nameBuf, "SPINBK%d", i);
        R_PrecachePatch(nameBuf, &pSpinTome[i]);

        sprintf(nameBuf, "SPFLY%d", i);
        R_PrecachePatch(nameBuf, &pSpinFly[i]);
    }

    // Inventory item flash anim.
    {
    const char invItemFlashAnim[5][9] = {
        {"USEARTIA"},
        {"USEARTIB"},
        {"USEARTIC"},
        {"USEARTID"},
        {"USEARTIE"}
    };

    for(i = 0; i < 5; ++i)
    {
        R_PrecachePatch(invItemFlashAnim[i], &pInvItemFlash[i]);
    }
    }

    // Ammo icons.
    {
    const char ammoPic[NUM_AMMO_TYPES][9] = {
        {"INAMGLD"},
        {"INAMBOW"},
        {"INAMBST"},
        {"INAMRAM"},
        {"INAMPNX"},
        {"INAMLOB"}
    };

    for(i = 0; i < NUM_AMMO_TYPES; ++i)
    {
        R_PrecachePatch(ammoPic[i], &pAmmoIcons[i]);
    }
    }

    // Key cards.
    R_PrecachePatch("YKEYICON", &pKeys[0]);
    R_PrecachePatch("GKEYICON", &pKeys[1]);
    R_PrecachePatch("BKEYICON", &pKeys[2]);
}

void ST_loadData(void)
{
    ST_loadGraphics();
}

static void initData(hudstate_t* hud)
{
    int player = hud - hudStates;

    hud->statusbarActive = true;
    hud->stopped = true;
    hud->showBar = 1;

    // Fullscreen:
    hud->health.value = 1994;
    hud->armor.value = 1994;
    hud->readyammo.value = 1994;
    hud->readyammoicon.patchId = 0;
    hud->frags.value = 1994;
    hud->readyitem.patchId = 0;
    hud->readyitem.flashCounter = 0;
    { int i;
    for(i = 0; i < 3; ++i)
    {
        hud->keys.keyBoxes[i] = false;
    }}

    // Statusbar:
    hud->sbarHealth.value = 1994;
    hud->sbarFrags.value = 1994;
    hud->sbarArmor.value = 1994;
    hud->sbarReadyammo.value = 1994;
    hud->sbarReadyammoicon.patchId = 0;
    hud->sbarReadyitem.patchId = 0;
    hud->sbarReadyitem.flashCounter = 0;
    hud->sbarChain.wiggle = 0;
    hud->sbarChain.healthMarker = 0;
    { int i;
    for(i = 0; i < 3; ++i)
    {
        hud->sbarKeyslots[i].slot = i;
        hud->sbarKeyslots[i].keytypeA = (keytype_t)i;
        hud->sbarKeyslots[i].patchId = 0;
    }}

    // Other:
    hud->flight.patchId = 0;
    hud->flight.hitCenterFrame = false;
    hud->tome.patchId = 0;
    hud->tome.play = 0;
    hud->secrets.value = 1994;
    hud->items.value = 1994;
    hud->kills.value = 1994;

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
    int i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        hudstate_t* hud = &hudStates[i];
        hud->inited = false;
    }
}

void ST_UpdateLogAlignment(void)
{
    short flags;
    int i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        hudstate_t* hud = &hudStates[i];
        if(!hud->inited) continue;

        flags = UIGroup_Flags(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOP]));
        flags &= ~(UWGF_ALIGN_LEFT|UWGF_ALIGN_RIGHT);
        if(cfg.msgAlign == 0)
            flags |= UWGF_ALIGN_LEFT;
        else if(cfg.msgAlign == 2)
            flags |= UWGF_ALIGN_RIGHT;
        UIGroup_SetFlags(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOP]), flags);
    }
}

/**
 * Called when the statusbar scale cvar changes.
 */
void updateViewWindow(void)
{
    int i;
    R_UpdateViewWindow(true);
    for(i = 0; i < MAXPLAYERS; ++i)
        ST_HUDUnHide(i, HUE_FORCE); // So the user can see the change.
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
