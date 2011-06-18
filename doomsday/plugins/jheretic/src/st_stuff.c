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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jheretic.h"

#include "d_net.h"
#include "hu_stuff.h"
#include "p_tick.h" // for P_IsPaused
#include "p_player.h"
#include "p_mapsetup.h"
#include "hu_automap.h"
#include "hu_lib.h"
#include "hu_chat.h"
#include "hu_log.h"
#include "hu_inventory.h"
#include "p_inventory.h"
#include "r_common.h"
#include "am_map.h"

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
    UWG_MAPNAME,
    UWG_TOPLEFT,
    UWG_TOPLEFT2,
    UWG_TOPRIGHT,
    UWG_BOTTOMLEFT,
    UWG_BOTTOMLEFT2,
    UWG_BOTTOMRIGHT,
    UWG_BOTTOM,
    UWG_TOP,
    UWG_COUNTERS,
    UWG_AUTOMAP,
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
    int automapCheatLevel; /// \todo Belongs in player state?

    int widgetGroupIds[NUM_UIWIDGET_GROUPS];
    int automapWidgetId;
    int chatWidgetId;
    int logWidgetId;

    // Statusbar:
    guidata_health_t sbarHealth;
    guidata_armor_t sbarArmor;
    guidata_frags_t sbarFrags;
    guidata_chain_t sbarChain;
    guidata_keyslot_t sbarKeyslots[3];
    guidata_readyitem_t sbarReadyitem;
    guidata_readyammo_t sbarReadyammo;
    guidata_readyammoicon_t sbarReadyammoicon;

    // Fullscreen:
    guidata_health_t health;
    guidata_armor_t armor;
    guidata_keys_t keys;
    guidata_readyammo_t readyammo;
    guidata_readyammoicon_t readyammoicon;
    guidata_frags_t frags;
    guidata_readyitem_t readyitem;

    // Other:
    guidata_automap_t automap;
    guidata_chat_t chat;
    guidata_log_t log;
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

int ST_ChatResponder(int player, event_t* ev);

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

// CODE --------------------------------------------------------------------

/**
 * Register CVARs and CCmds for the HUD/Status bar
 */
void ST_Register(void)
{
    cvartemplate_t cvars[] = {
        { "hud-scale", 0, CVT_FLOAT, &cfg.hudScale, 0.1f, 1, unhideHUD },
        { "hud-wideoffset", 0, CVT_FLOAT, &cfg.hudWideOffset, 0, 1, unhideHUD },

        { "hud-status-size", 0, CVT_FLOAT, &cfg.statusbarScale, 0.1f, 1, updateViewWindow },

        // HUD color + alpha
        { "hud-color-r", 0, CVT_FLOAT, &cfg.hudColor[0], 0, 1, unhideHUD },
        { "hud-color-g", 0, CVT_FLOAT, &cfg.hudColor[1], 0, 1, unhideHUD },
        { "hud-color-b", 0, CVT_FLOAT, &cfg.hudColor[2], 0, 1, unhideHUD },
        { "hud-color-a", 0, CVT_FLOAT, &cfg.hudColor[3], 0, 1, unhideHUD },
        { "hud-icon-alpha", 0, CVT_FLOAT, &cfg.hudIconAlpha, 0, 1, unhideHUD },

        { "hud-status-alpha", 0, CVT_FLOAT, &cfg.statusbarOpacity, 0, 1, unhideHUD },
        { "hud-status-icon-a", 0, CVT_FLOAT, &cfg.statusbarCounterAlpha, 0, 1, unhideHUD },

        // HUD icons
        { "hud-ammo", 0, CVT_BYTE, &cfg.hudShown[HUD_AMMO], 0, 1, unhideHUD },
        { "hud-armor", 0, CVT_BYTE, &cfg.hudShown[HUD_ARMOR], 0, 1, unhideHUD },
        { "hud-keys", 0, CVT_BYTE, &cfg.hudShown[HUD_KEYS], 0, 1, unhideHUD },
        { "hud-health", 0, CVT_BYTE, &cfg.hudShown[HUD_HEALTH], 0, 1, unhideHUD },
        { "hud-currentitem", 0, CVT_BYTE, &cfg.hudShown[HUD_READYITEM], 0, 1, unhideHUD },

        // HUD displays
        { "hud-tome-timer", CVF_NO_MAX, CVT_INT, &cfg.tomeCounter, 0, 0 },
        { "hud-tome-sound", CVF_NO_MAX, CVT_INT, &cfg.tomeSound, 0, 0 },

        { "hud-timer", 0, CVT_FLOAT, &cfg.hudTimer, 0, 60 },

        { "hud-unhide-damage", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_DAMAGE], 0, 1 },
        { "hud-unhide-pickup-health", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_HEALTH], 0, 1 },
        { "hud-unhide-pickup-armor", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_ARMOR], 0, 1 },
        { "hud-unhide-pickup-powerup", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_POWER], 0, 1 },
        { "hud-unhide-pickup-weapon", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_WEAPON], 0, 1 },
        { "hud-unhide-pickup-ammo", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_AMMO], 0, 1 },
        { "hud-unhide-pickup-key", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_KEY], 0, 1 },
        { "hud-unhide-pickup-invitem", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_INVITEM], 0, 1 },

        { "hud-cheat-counter", 0, CVT_BYTE, &cfg.hudShownCheatCounters, 0, 63, unhideHUD },
        { "hud-cheat-counter-scale", 0, CVT_FLOAT, &cfg.hudCheatCounterScale, .1f, 1, unhideHUD },
        { NULL }
    };
    ccmdtemplate_t ccmds[] = {
        { "beginchat",       NULL,   CCmdChatOpen },
        { "chatcancel",      "",     CCmdChatAction },
        { "chatcomplete",    "",     CCmdChatAction },
        { "chatdelete",      "",     CCmdChatAction },
        { "chatsendmacro",   NULL,   CCmdChatSendMacro },
        { NULL }
    };
    int i;
    for(i = 0; cvars[i].name; ++i)
        Con_AddVariable(cvars + i);
    for(i = 0; ccmds[i].name; ++i)
        Con_AddCommand(ccmds + i);

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

void SBarChain_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    assert(NULL != obj);
    {
    guidata_chain_t* chain = (guidata_chain_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    int delta, curHealth = MAX_OF(plr->plr->mo->health, 0);
    if(P_IsPaused() || !GUI_GameTicTriggerIsSharp())
        return;
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

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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
    DGL_DrawRectColor(x + gemXOffset - 11, chainY - 6, 41, 24, rgb[0], rgb[1], rgb[2], gemglow - (1 - iconAlpha));

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_BlendMode(BM_NORMAL);
    DGL_Color4f(1, 1, 1, 1);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef ORIGINX
#undef ORIGINY
}

void SBarChain_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj);

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    /// \fixme Calculate dimensions properly.
    obj->dimensions.width  = (ST_WIDTH - 21 - 28) * cfg.statusbarScale;
    obj->dimensions.height = 8 * cfg.statusbarScale;
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

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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

void SBarBackground_UpdateDimensions(uiwidget_t* obj)
{
#define WIDTH       (ST_WIDTH)
#define HEIGHT      (ST_HEIGHT)

    assert(NULL != obj);

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    obj->dimensions.width  = WIDTH  * cfg.statusbarScale;
    obj->dimensions.height = HEIGHT * cfg.statusbarScale;

#undef HEIGHT
#undef WIDTH
}

void ST_updateWidgets(int player)
{
    hudstate_t* hud = &hudStates[player];

}

int ST_Responder(event_t* ev)
{
    int i, eaten;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        eaten = ST_ChatResponder(i, ev);
        if(0 != eaten)
            return eaten;
    }
    return false;
}

void ST_Ticker(timespan_t ticLength)
{
    /// \kludge 
    boolean runGameTic = GUI_RunGameTicTrigger(ticLength);
    /// kludge end.
    int i;

    if(runGameTic)
        Hu_InventoryTicker();

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t* plr = &players[i];
        hudstate_t* hud = &hudStates[i];

        if(!plr->plr->inGame)
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
        if(runGameTic && !P_IsPaused())
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
        }

        if(hud->inited)
        {
            int j;
            for(j = 0; j < NUM_UIWIDGET_GROUPS; ++j)
            {
                UIWidget_RunTic(GUI_MustFindObjectById(hud->widgetGroupIds[j]), ticLength);
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
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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

void SBarInventory_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj);

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(!Hu_InventoryIsOpen(obj->player))
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    // \fixme calculate dimensions properly!
    obj->dimensions.width  = (ST_WIDTH-(43*2)) * cfg.statusbarScale;
    obj->dimensions.height = 41 * cfg.statusbarScale;
}

void Frags_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    assert(NULL != obj);
    {
    guidata_frags_t* frags = (guidata_frags_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    int i;
    if(P_IsPaused() || !GUI_GameTicTriggerIsSharp())
        return;
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
#define TRACKING            (1)

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
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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
    FR_SetTracking(TRACKING);
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    FR_DrawText3(buf, X, Y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);

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

void SBarFrags_UpdateDimensions(uiwidget_t* obj)
{
#define TRACKING            (1)
    assert(NULL != obj);
    {
    guidata_frags_t* frags = (guidata_frags_t*)obj->typedata;
    char buf[20];

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(!deathmatch || Hu_InventoryIsOpen(obj->player))
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(frags->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", frags->value);
    FR_SetFont(obj->fontId);
    FR_SetTracking(TRACKING);
    FR_TextDimensions(&obj->dimensions.width, &obj->dimensions.height, buf);
    obj->dimensions.width  *= cfg.statusbarScale;
    obj->dimensions.height *= cfg.statusbarScale;
    }
#undef TRACKING
}

void Health_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    assert(NULL != obj);
    {
    guidata_health_t* hlth = (guidata_health_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    if(P_IsPaused() || !GUI_GameTicTriggerIsSharp())
        return;
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
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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
    FR_SetTracking(TRACKING);
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    FR_DrawText3(buf, X, Y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);

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

void SBarHealth_UpdateDimensions(uiwidget_t* obj)
{
#define TRACKING            (1)

    assert(NULL != obj);
    {
    guidata_health_t* hlth = (guidata_health_t*)obj->typedata;
    char buf[20];

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(deathmatch || Hu_InventoryIsOpen(obj->player))
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(hlth->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", hlth->value);
    FR_SetFont(obj->fontId);
    FR_SetTracking(TRACKING);
    FR_TextDimensions(&obj->dimensions.width, &obj->dimensions.height, buf);
    obj->dimensions.width  *= cfg.statusbarScale;
    obj->dimensions.height *= cfg.statusbarScale;
    }
#undef TRACKING
}

void Armor_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    assert(NULL != obj);
    {
    guidata_armor_t* armor = (guidata_armor_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    if(P_IsPaused() || !GUI_GameTicTriggerIsSharp())
        return;
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
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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
    FR_SetTracking(TRACKING);
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    FR_DrawText3(buf, X, Y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);

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

void SBarArmor_UpdateDimensions(uiwidget_t* obj)
{
#define TRACKING            (1)

    assert(NULL != obj);
    {
    guidata_armor_t* armor = (guidata_armor_t*)obj->typedata;
    char buf[20];

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(Hu_InventoryIsOpen(obj->player))
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(armor->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", armor->value);
    FR_SetFont(obj->fontId);
    FR_SetTracking(TRACKING);
    FR_TextDimensions(&obj->dimensions.width, &obj->dimensions.height, buf);
    obj->dimensions.width  *= cfg.statusbarScale;
    obj->dimensions.height *= cfg.statusbarScale;
    }
#undef TRACKING
}

void KeySlot_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    assert(NULL != obj);
    {
    guidata_keyslot_t* kslt = (guidata_keyslot_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    if(P_IsPaused() || !GUI_GameTicTriggerIsSharp())
        return;
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
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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
    DGL_Color4f(1, 1, 1, iconAlpha);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(1, 1, 1, iconAlpha);

    WI_DrawPatch(kslt->patchId, Hu_ChoosePatchReplacement(kslt->patchId), loc->x, loc->y);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef ORIGINY
#undef ORIGINX
}

void KeySlot_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_keyslot_t* kslt = (guidata_keyslot_t*)obj->typedata;
    patchinfo_t info;

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(Hu_InventoryIsOpen(obj->player))
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(kslt->patchId == 0 || !R_GetPatchInfo(kslt->patchId, &info))
        return;

    obj->dimensions.width  = info.width  * cfg.statusbarScale;
    obj->dimensions.height = info.height * cfg.statusbarScale;
    }
}

void ReadyAmmo_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    assert(NULL != obj);
    {
    guidata_readyammo_t* ammo = (guidata_readyammo_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    int lvl = (plr->powers[PT_WEAPONLEVEL2]? 1 : 0);
    ammotype_t ammoType;
    if(P_IsPaused() || !GUI_GameTicTriggerIsSharp())
        return;
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
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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
    FR_SetTracking(TRACKING);
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], iconAlpha);
    FR_DrawText3(buf, X, Y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);

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

void SBarReadyAmmo_UpdateDimensions(uiwidget_t* obj)
{
#define TRACKING            (1)

    assert(NULL != obj);
    {
    guidata_readyammo_t* ammo = (guidata_readyammo_t*)obj->typedata;
    char buf[20];

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(Hu_InventoryIsOpen(obj->player))
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(ammo->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", ammo->value);
    FR_SetFont(obj->fontId);
    FR_SetTracking(TRACKING);
    FR_TextDimensions(&obj->dimensions.width, &obj->dimensions.height, buf);
    obj->dimensions.width  *= cfg.statusbarScale;
    obj->dimensions.height *= cfg.statusbarScale;
    }
#undef TRACKING
}

void ReadyAmmoIcon_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    assert(NULL != obj);
    {
    guidata_readyammoicon_t* icon = (guidata_readyammoicon_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    int lvl = (plr->powers[PT_WEAPONLEVEL2]? 1 : 0);
    ammotype_t ammoType;
    if(P_IsPaused() || !GUI_GameTicTriggerIsSharp())
        return;
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
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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
    DGL_Color4f(1, 1, 1, iconAlpha);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(1, 1, 1, iconAlpha);

    WI_DrawPatch(icon->patchId, Hu_ChoosePatchReplacement(icon->patchId), X, Y);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarReadyAmmoIcon_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_readyammoicon_t* icon = (guidata_readyammoicon_t*)obj->typedata;
    patchinfo_t info;

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(Hu_InventoryIsOpen(obj->player))
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(icon->patchId == 0)
        return;
    if(!R_GetPatchInfo(icon->patchId, &info))
        return;

    obj->dimensions.width  = info.width  * cfg.statusbarScale;
    obj->dimensions.height = info.height * cfg.statusbarScale;
    }
}

void ReadyItem_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    assert(NULL != obj);
    {
    guidata_readyitem_t* item = (guidata_readyitem_t*)obj->typedata;
    if(P_IsPaused() || !GUI_GameTicTriggerIsSharp())
        return;
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
#define TRACKING (2)

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
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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
                FR_SetTracking(TRACKING);
                FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
                FR_DrawText3(buf, ORIGINX+ST_INVITEMCX, ORIGINY+ST_INVITEMCY, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);
            }
        }

        DGL_Disable(DGL_TEXTURE_2D);
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }
    }
#undef TRACKING
#undef ORIGINY
#undef ORIGINX
}

void SBarReadyItem_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_readyitem_t* item = (guidata_readyitem_t*)obj->typedata;
    patchinfo_t info;

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(Hu_InventoryIsOpen(obj->player))
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(item->patchId == 0)
        return;
    if(!R_GetPatchInfo(item->patchId, &info))
        return;
    // \fixme Calculate dimensions properly!
    obj->dimensions.width  = info.width  * cfg.statusbarScale;
    obj->dimensions.height = info.height * cfg.statusbarScale;
    }
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

void Flight_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    assert(NULL != obj);
    {
    guidata_flight_t* flht = (guidata_flight_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    if(P_IsPaused() || !GUI_GameTicTriggerIsSharp())
        return;
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

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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

void Flight_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_flight_t* flht = (guidata_flight_t*)obj->typedata;

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(0 == flht->patchId)
        return;

    /// \fixme Calculate dimensions properly!
    obj->dimensions.width  = 32 * cfg.hudScale;
    obj->dimensions.height = 32 * cfg.hudScale;
    }
}

void Tome_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    assert(NULL != obj);
    {
    guidata_tomeofpower_t* tome = (guidata_tomeofpower_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    const int ticsRemain = plr->powers[PT_WEAPONLEVEL2];

    if(P_IsPaused() || !GUI_GameTicTriggerIsSharp())
        return;

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

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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
        FR_SetTracking(TRACKING);
        FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
        FR_DrawText3(buf, 0, 25 + 2, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);

        DGL_Disable(DGL_TEXTURE_2D);

#undef TRACKING
#undef COUNT_Y
#undef COUNT_X
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void Tome_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_tomeofpower_t* tome = (guidata_tomeofpower_t*)obj->typedata;

    obj->dimensions.width = 0;
    obj->dimensions.height = 0;

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(tome->patchId == 0 && tome->countdownSeconds == 0)
        return;

    if(tome->patchId != 0)
    {
        // \fixme Determine the actual center point of the animation at widget creation time.
        obj->dimensions.width  += 25;
        obj->dimensions.height += 25;
    }

    if(tome->countdownSeconds != 0)
    {
#define TRACKING            (2)

        char buf[20];
        int w;
        dd_snprintf(buf, 20, "%i", tome->countdownSeconds);
        FR_SetFont(obj->fontId);
        FR_SetTracking(TRACKING);
        w = FR_TextWidth(buf);
        if(w > obj->dimensions.width)
            obj->dimensions.width += w;
        obj->dimensions.height += FR_TextHeight(buf) + 2;

#undef TRACKING
    }

    obj->dimensions.width  *= cfg.hudScale;
    obj->dimensions.height *= cfg.hudScale;
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

    if(!cfg.hudShown[HUD_AMMO])
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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
    GL_DrawPatch2(icon->patchId, 0, 0, ALIGN_TOPLEFT, DPF_NO_OFFSET);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void ReadyAmmoIcon_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_readyammoicon_t* icon = (guidata_readyammoicon_t*)obj->typedata;
    patchinfo_t info;

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(!cfg.hudShown[HUD_AMMO])
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(icon->patchId == 0)
        return;
    if(!R_GetPatchInfo(icon->patchId, &info))
        return;

    obj->dimensions.width  = info.width  * cfg.hudScale;
    obj->dimensions.height = info.height * cfg.hudScale;
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

    if(!cfg.hudShown[HUD_AMMO])
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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
    FR_SetTracking(TRACKING);
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    FR_DrawText3(buf, 0, -2, ALIGN_TOPLEFT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef TRACKING
}

void ReadyAmmo_UpdateDimensions(uiwidget_t* obj)
{
#define TRACKING                (1)

    assert(NULL != obj);
    {
    guidata_readyammo_t* ammo = (guidata_readyammo_t*)obj->typedata;
    char buf[20];

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(!cfg.hudShown[HUD_AMMO])
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(ammo->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", ammo->value);
    FR_SetFont(obj->fontId);
    FR_SetTracking(TRACKING);
    FR_TextDimensions(&obj->dimensions.width, &obj->dimensions.height, buf);
    obj->dimensions.width  *= cfg.hudScale;
    obj->dimensions.height *= cfg.hudScale;
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

    if(!cfg.hudShown[HUD_HEALTH])
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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
    FR_SetTracking(TRACKING);
    FR_SetColorAndAlpha(0, 0, 0, textAlpha * .4f);
    FR_DrawText3(buf, 2, 1, ALIGN_BOTTOMLEFT, DTF_NO_EFFECTS);

    FR_SetColorAndAlpha(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    FR_DrawText3(buf, 0, -1, ALIGN_BOTTOMLEFT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef TRACKING
}

void Health_UpdateDimensions(uiwidget_t* obj)
{
#define TRACKING                (1)

    assert(NULL != obj);
    {
    guidata_health_t* hlth = (guidata_health_t*)obj->typedata;
    int health = MAX_OF(hlth->value, 0);
    char buf[20];

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(!cfg.hudShown[HUD_HEALTH])
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    dd_snprintf(buf, 5, "%i", health);
    FR_SetFont(obj->fontId);
    FR_SetTracking(TRACKING);
    FR_TextDimensions(&obj->dimensions.width, &obj->dimensions.height, buf);
    obj->dimensions.width  *= cfg.hudScale;
    obj->dimensions.height *= cfg.hudScale;
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

    if(!cfg.hudShown[HUD_ARMOR])
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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
    FR_SetTracking(TRACKING);
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    FR_DrawText3(buf, -1, -11, ALIGN_TOPLEFT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef TRACKING
}

void Armor_UpdateDimensions(uiwidget_t* obj)
{
#define TRACKING                (1)

    assert(NULL != obj);
    {
    guidata_armor_t* armor = (guidata_armor_t*)obj->typedata;
    char buf[20];

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(!cfg.hudShown[HUD_ARMOR])
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(armor->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", armor->value);
    FR_SetFont(obj->fontId);
    FR_SetTracking(TRACKING);
    FR_TextDimensions(&obj->dimensions.width, &obj->dimensions.height, buf);
    obj->dimensions.width  *= cfg.hudScale;
    obj->dimensions.height *= cfg.hudScale;
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

    if(!cfg.hudShown[HUD_KEYS])
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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
        GL_DrawPatch2(pKeys[0].id, x, -pKeys[0].height, ALIGN_TOPLEFT, DPF_NO_OFFSET);
        DGL_Disable(DGL_TEXTURE_2D);

        x += pKeys[0].width + 1;
    }

    if(keys->keyBoxes[KT_GREEN])
    {
        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconAlpha);
        GL_DrawPatch2(pKeys[1].id, x, -pKeys[1].height, ALIGN_TOPLEFT, DPF_NO_OFFSET);
        DGL_Disable(DGL_TEXTURE_2D);

        x += pKeys[1].width + 1;
    }

    if(keys->keyBoxes[KT_BLUE])
    {
        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconAlpha);
        GL_DrawPatch2(pKeys[2].id, x, -pKeys[2].height, ALIGN_TOPLEFT, DPF_NO_OFFSET);
        DGL_Disable(DGL_TEXTURE_2D);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void Keys_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    assert(NULL != obj);
    {
    guidata_keys_t* keys = (guidata_keys_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    int i;
    if(P_IsPaused() || !GUI_GameTicTriggerIsSharp())
        return;
    for(i = 0; i < 3; ++i)
    {
        keys->keyBoxes[i] = plr->keys[i] ? true : false;
    }
    }
}

void Keys_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_keys_t* keys = (guidata_keys_t*)obj->typedata;
    int x = 0, numVisible = 0; 

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(!cfg.hudShown[HUD_KEYS])
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    if(keys->keyBoxes[KT_YELLOW])
    {
        x += pKeys[0].width + 1;
        obj->dimensions.width += pKeys[0].width;
        if(pKeys[0].height > obj->dimensions.height)
            obj->dimensions.height = pKeys[0].height;
        ++numVisible;
    }

    if(keys->keyBoxes[KT_GREEN])
    {
        x += pKeys[1].width + 1;
        obj->dimensions.width += pKeys[1].width;
        if(pKeys[1].height > obj->dimensions.height)
            obj->dimensions.height = pKeys[1].height;
        ++numVisible;
    }

    if(keys->keyBoxes[KT_BLUE])
    {
        x += pKeys[2].width;
        obj->dimensions.width += pKeys[1].width;
        if(pKeys[2].height > obj->dimensions.height)
            obj->dimensions.height = pKeys[2].height;
        ++numVisible;
    }
    obj->dimensions.width += (numVisible-1) * 1;

    obj->dimensions.width  *= cfg.hudScale;
    obj->dimensions.height *= cfg.hudScale;
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
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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
    FR_SetTracking(TRACKING);
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    FR_DrawText3(buf, 0, 0, ALIGN_BOTTOMLEFT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef TRACKING
}

void Frags_UpdateDimensions(uiwidget_t* obj)
{
#define TRACKING                (1)

    assert(NULL != obj);
    {
    guidata_frags_t* frags = (guidata_frags_t*)obj->typedata;
    char buf[20];

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(!deathmatch)
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(frags->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", frags->value);

    FR_SetFont(obj->fontId);
    FR_SetTracking(TRACKING);
    FR_TextDimensions(&obj->dimensions.width, &obj->dimensions.height, buf);
    obj->dimensions.width  *= cfg.hudScale;
    obj->dimensions.height *= cfg.hudScale;
    }
#undef TRACKING
}

void ReadyItem_Drawer(uiwidget_t* obj, int x, int y)
{
#define TRACKING                (2)
    assert(NULL != obj);
    {
    guidata_readyitem_t* item = (guidata_readyitem_t*)obj->typedata;
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;
    inventoryitemtype_t readyItem;
    patchinfo_t boxInfo;

    if(!cfg.hudShown[HUD_READYITEM])
        return;
    if(Hu_InventoryIsOpen(obj->player))
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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
        GL_DrawPatch2(pInvItemBox, 0, 0, ALIGN_BOTTOMRIGHT, DPF_NO_OFFSET);
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
            FR_SetTracking(TRACKING);
            FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
            dd_snprintf(buf, 20, "%i", count);
            FR_DrawText3(buf, -1, -7, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);
            DGL_Disable(DGL_TEXTURE_2D);
        }
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef TRACKING
}

void ReadyItem_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    patchinfo_t boxInfo;

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(!cfg.hudShown[HUD_READYITEM])
        return;
    if(Hu_InventoryIsOpen(obj->player))
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(!R_GetPatchInfo(pInvItemBox, &boxInfo))
        return;

    obj->dimensions.width  = boxInfo.width  * cfg.hudScale;
    obj->dimensions.height = boxInfo.height * cfg.hudScale;
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
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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

void Inventory_UpdateDimensions(uiwidget_t* obj)
{
#define INVENTORY_HEIGHT    29
#define EXTRA_SCALE         .75f

    assert(NULL != obj);

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(!Hu_InventoryIsOpen(obj->player))
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    /// \fixme Calculate the visual dimensions properly!
    obj->dimensions.width  = (31*7+16*2) * EXTRA_SCALE * cfg.hudScale;
    obj->dimensions.height = INVENTORY_HEIGHT * EXTRA_SCALE * cfg.hudScale;

#undef EXTRA_SCALE
#undef INVENTORY_HEIGHT
}

void Kills_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    assert(NULL != obj);
    {
    guidata_kills_t* kills = (guidata_kills_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    if(P_IsPaused() || !GUI_GameTicTriggerIsSharp())
        return;
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
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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
    FR_SetColorAndAlpha(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    FR_DrawText3(buf, 0, 0, ALIGN_BOTTOMLEFT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void Kills_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_kills_t* kills = (guidata_kills_t*)obj->typedata;
    char buf[40], tmp[20];

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(!(cfg.hudShownCheatCounters & (CCH_KILLS | CCH_KILLS_PRCNT)))
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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
    FR_TextDimensions(&obj->dimensions.width, &obj->dimensions.height, buf);
    obj->dimensions.width  *= cfg.hudCheatCounterScale;
    obj->dimensions.height *= cfg.hudCheatCounterScale;
    }
}

void Items_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    assert(NULL != obj);
    {
    guidata_items_t* items = (guidata_items_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    if(P_IsPaused() || !GUI_GameTicTriggerIsSharp())
        return;
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
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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
    FR_SetColorAndAlpha(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    FR_DrawText3(buf, 0, 0, ALIGN_BOTTOMLEFT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void Items_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_items_t* items = (guidata_items_t*)obj->typedata;
    char buf[40], tmp[20];

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(!(cfg.hudShownCheatCounters & (CCH_ITEMS | CCH_ITEMS_PRCNT)))
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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
    FR_TextDimensions(&obj->dimensions.width, &obj->dimensions.height, buf);
    obj->dimensions.width  *= cfg.hudCheatCounterScale;
    obj->dimensions.height *= cfg.hudCheatCounterScale;
    }
}

void Secrets_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    assert(NULL != obj);
    {
    guidata_secrets_t* scrt = (guidata_secrets_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    if(P_IsPaused() || !GUI_GameTicTriggerIsSharp())
        return;
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
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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
    FR_SetColorAndAlpha(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    FR_DrawText3(buf, 0, 0, ALIGN_BOTTOMLEFT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void Secrets_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_secrets_t* scrt = (guidata_secrets_t*)obj->typedata;
    char buf[40], tmp[20];

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(!(cfg.hudShownCheatCounters & (CCH_SECRETS | CCH_SECRETS_PRCNT)))
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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
    FR_TextDimensions(&obj->dimensions.width, &obj->dimensions.height, buf);
    obj->dimensions.width  *= cfg.hudCheatCounterScale;
    obj->dimensions.height *= cfg.hudCheatCounterScale;
    }
}

void MapName_Drawer(uiwidget_t* obj, int x, int y)
{
    assert(NULL != obj && obj->type == GUI_MAPNAME);
    {
    const float scale = .75f;
    const float textAlpha = uiRendState->pageAlpha;
    const char* text = P_GetMapNiceName();

    if(NULL == text)
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(scale, scale, 1);

    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(obj->fontId);
    FR_SetColorAndAlpha(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    FR_DrawText3(text, 0, 0, ALIGN_BOTTOMLEFT, DTF_NO_EFFECTS);
    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void MapName_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj && obj->type == GUI_MAPNAME);
    {
    const char* text = P_GetMapNiceName();
    const float scale = .75f;

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(NULL == text)
        return;

    FR_SetFont(obj->fontId);
    FR_TextDimensions(&obj->dimensions.width, &obj->dimensions.height, text);
    obj->dimensions.width  *= scale;
    obj->dimensions.height *= scale;
    }
}

typedef struct {
    guiwidgettype_t type;
    int group;
    gamefontid_t fontIdx;
    void (*updateDimensions) (uiwidget_t* obj); 
    void (*drawer) (uiwidget_t* obj, int x, int y);
    void (*ticker) (uiwidget_t* obj, timespan_t ticLength);
    void* typedata;
} uiwidgetdef_t;

typedef struct {
    int group;
    int alignFlags;
    int groupFlags;
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
    hud->statusbarActive = (fullscreen < 2) || (ST_AutomapIsActive(player) && (cfg.automapHudDisplay == 0 || cfg.automapHudDisplay == 2) );

    // Do palette shifts
    ST_doPaletteStuff(player);

    GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_AUTOMAP]), 0, 0, SCREENWIDTH, SCREENHEIGHT, ST_AutomapOpacity(player), NULL, NULL);

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

        int posX, posY, availWidth, availHeight, drawnWidth = 0, drawnHeight = 0;

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

        if(!hud->statusbarActive)
        {
            int h = 0;
            GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOMLEFT]), x, y, width, height, alpha, &drawnWidth, &drawnHeight);
            if(drawnHeight > h) h = drawnHeight;
            GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOMRIGHT]), x, y, width, height, alpha, &drawnWidth, &drawnHeight);
            if(drawnHeight > h) h = drawnHeight;
            GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOM]), x, y, width, height, alpha, &drawnWidth, &drawnHeight);
            if(drawnHeight > h) h = drawnHeight;
            drawnHeight = h;
        }

        if(!hud->statusbarActive)
        {
            int h = drawnHeight;
            availHeight = height - (drawnHeight > 0 ? drawnHeight : 0);
            GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOMLEFT2]), x, y, width, availHeight, alpha, &drawnWidth, &drawnHeight);
            drawnHeight += h;
        }

        availHeight = height - (drawnHeight > 0 ? drawnHeight : 0);
        GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_MAPNAME]), x, y, width, availHeight, ST_AutomapOpacity(player), NULL, NULL);

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
    player_t* plr = &players[player];

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

    hud->log._msgCount = 0;
    hud->log._nextUsedMsg = 0;
    hud->log._pvisMsgCount = 0;
    memset(hud->log._msgs, 0, sizeof(hud->log._msgs));

    ST_HUDUnHide(player, HUE_FORCE);
}

static void findMapBounds(float* lowX, float* hiX, float* lowY, float* hiY)
{
    assert(NULL != lowX && NULL != hiX && NULL != lowY && NULL != hiY);
    {
    float pos[2];
    uint i;

    *lowX = *lowY =  DDMAXFLOAT;
    *hiX  = *hiY  = -DDMAXFLOAT;

    for(i = 0; i < numvertexes; ++i)
    {
        P_GetFloatv(DMU_VERTEX, i, DMU_XY, pos);

        if(pos[VX] < *lowX)
            *lowX = pos[VX];
        if(pos[VX] > *hiX)
            *hiX  = pos[VX];

        if(pos[VY] < *lowY)
            *lowY = pos[VY];
        if(pos[VY] > *hiY)
            *hiY  = pos[VY];
    }
    }
}

static void setAutomapCheatLevel(uiwidget_t* obj, int level)
{
    assert(NULL != obj);
    {
    hudstate_t* hud = &hudStates[UIWidget_Player(obj)];
    int flags;

    hud->automapCheatLevel = level;

    flags = UIAutomap_Flags(obj) & ~(AMF_REND_ALLLINES|AMF_REND_THINGS|AMF_REND_SPECIALLINES|AMF_REND_VERTEXES|AMF_REND_LINE_NORMALS);
    if(hud->automapCheatLevel >= 1)
        flags |= AMF_REND_ALLLINES;
    if(hud->automapCheatLevel == 2)
        flags |= AMF_REND_THINGS | AMF_REND_SPECIALLINES;
    if(hud->automapCheatLevel > 2)
        flags |= (AMF_REND_VERTEXES | AMF_REND_LINE_NORMALS);
    UIAutomap_SetFlags(obj, flags);
    }
}

static void initAutomapForCurrentMap(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    hudstate_t* hud = &hudStates[UIWidget_Player(obj)];
    float lowX, hiX, lowY, hiY;
    automapcfg_t* mcfg;

    UIAutomap_Reset(obj);

    findMapBounds(&lowX, &hiX, &lowY, &hiY);
    UIAutomap_SetMinScale(obj, 2 * PLAYERRADIUS);
    UIAutomap_SetWorldBounds(obj, lowX, hiX, lowY, hiY);

    mcfg = UIAutomap_Config(obj);

    // Determine the obj view scale factors.
    UIAutomap_SetScale(obj, UIAutomap_ZoomMax(obj)? 0 : .45f);
    UIAutomap_ClearPoints(obj);

#if !__JHEXEN__
    if(gameSkill == SM_BABY && cfg.automapBabyKeys)
    {
        int flags = UIAutomap_Flags(obj);
        UIAutomap_SetFlags(obj, flags|AMF_REND_KEYS);
    }

    if(!IS_NETGAME && hud->automapCheatLevel)
        AM_SetVectorGraphic(mcfg, AMO_THINGPLAYER, VG_CHEATARROW);
#endif

    // Are we re-centering on a followed mobj?
    { mobj_t* mo = UIAutomap_FollowMobj(obj);
    if(NULL != mo)
    {
        UIAutomap_SetCameraOrigin(obj, mo->pos[VX], mo->pos[VY]);
    }}

    if(IS_NETGAME)
    {
        setAutomapCheatLevel(obj, 0);
    }

    UIAutomap_SetReveal(obj, false);

    // Add all immediately visible lines.
    { uint i;
    for(i = 0; i < numlines; ++i)
    {
        xline_t* xline = &xlines[i];
        if(!(xline->flags & ML_MAPPED)) continue;
        P_SetLinedefAutomapVisibility(UIWidget_Player(obj), i, true);
    }}
    }
}

void ST_Start(int player)
{
    const int winWidth  = Get(DD_WINDOW_WIDTH);
    const int winHeight = Get(DD_WINDOW_HEIGHT);
    uiwidget_t* obj;
    hudstate_t* hud;
    int flags;
    if(player < 0 || player >= MAXPLAYERS)
    {
        Con_Error("ST_Start: Invalid player #%i.", player);
        exit(1); // Unreachable.
    }
    hud = &hudStates[player];

    if(!hud->stopped)
        ST_Stop(player);

    initData(hud);
    // If the map has been left open; close it.
    ST_AutomapOpen(player, false, true);

    /**
     * Initialize widgets according to player preferences.
     */

    obj = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOP]);
    flags = UIWidget_Alignment(obj);
    flags &= ~(ALIGN_LEFT|ALIGN_RIGHT);
    if(cfg.msgAlign == 0)
        flags |= ALIGN_LEFT;
    else if(cfg.msgAlign == 2)
        flags |= ALIGN_RIGHT;
    UIWidget_SetAlignment(obj, flags);

    obj = GUI_MustFindObjectById(hud->automapWidgetId);
    initAutomapForCurrentMap(obj);
    UIAutomap_SetScale(obj, 1);
    UIAutomap_SetCameraRotation(obj, cfg.automapRotate);
    UIAutomap_SetOrigin(obj, 0, 0);
    UIAutomap_SetDimensions(obj, winWidth, winHeight);

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

void ST_BuildWidgets(int player)
{
#define PADDING 2 // In fixed 320x200 units.

    hudstate_t* hud = hudStates + player;
    const uiwidgetgroupdef_t widgetGroupDefs[] = {
        { UWG_STATUSBAR,    ALIGN_BOTTOM },
        { UWG_MAPNAME,      ALIGN_BOTTOMLEFT },
        { UWG_TOPLEFT,      ALIGN_TOPLEFT,     UWGF_LEFTTORIGHT, PADDING },
        { UWG_TOPLEFT2,     ALIGN_TOPLEFT,     UWGF_LEFTTORIGHT, PADDING },
        { UWG_TOPRIGHT,     ALIGN_TOPRIGHT,    UWGF_RIGHTTOLEFT, PADDING },
        { UWG_BOTTOMLEFT,   ALIGN_BOTTOMLEFT,  UWGF_VERTICAL|UWGF_RIGHTTOLEFT, PADDING },
        { UWG_BOTTOMLEFT2,  ALIGN_BOTTOMLEFT,  UWGF_LEFTTORIGHT, PADDING },
        { UWG_BOTTOMRIGHT,  ALIGN_BOTTOMRIGHT, UWGF_RIGHTTOLEFT, PADDING },
        { UWG_BOTTOM,       ALIGN_BOTTOM,      UWGF_VERTICAL|UWGF_RIGHTTOLEFT, PADDING },
        { UWG_TOP,          ALIGN_TOPLEFT,     UWGF_VERTICAL|UWGF_LEFTTORIGHT, PADDING },
        { UWG_COUNTERS,     ALIGN_LEFT,        UWGF_VERTICAL|UWGF_RIGHTTOLEFT, PADDING },
        { UWG_AUTOMAP,      ALIGN_TOPLEFT }
    };
    const uiwidgetdef_t widgetDefs[] = {
        { GUI_BOX,          UWG_STATUSBAR,    0,            SBarBackground_UpdateDimensions, SBarBackground_Drawer },
        { GUI_INVENTORY,    UWG_STATUSBAR,    GF_SMALLIN,   SBarInventory_UpdateDimensions, SBarInventory_Drawer },
        { GUI_FRAGS,        UWG_STATUSBAR,    GF_STATUS,    SBarFrags_UpdateDimensions, SBarFrags_Drawer, Frags_Ticker, &hud->sbarFrags },
        { GUI_HEALTH,       UWG_STATUSBAR,    GF_STATUS,    SBarHealth_UpdateDimensions, SBarHealth_Drawer, Health_Ticker, &hud->sbarHealth },
        { GUI_ARMOR,        UWG_STATUSBAR,    GF_STATUS,    SBarArmor_UpdateDimensions, SBarArmor_Drawer, Armor_Ticker, &hud->sbarArmor },
        { GUI_KEYSLOT,      UWG_STATUSBAR,    0,            KeySlot_UpdateDimensions, KeySlot_Drawer, KeySlot_Ticker, &hud->sbarKeyslots[0] },
        { GUI_KEYSLOT,      UWG_STATUSBAR,    0,            KeySlot_UpdateDimensions, KeySlot_Drawer, KeySlot_Ticker, &hud->sbarKeyslots[1] },
        { GUI_KEYSLOT,      UWG_STATUSBAR,    0,            KeySlot_UpdateDimensions, KeySlot_Drawer, KeySlot_Ticker, &hud->sbarKeyslots[2] },
        { GUI_READYAMMO,    UWG_STATUSBAR,    GF_STATUS,    SBarReadyAmmo_UpdateDimensions, SBarReadyAmmo_Drawer, ReadyAmmo_Ticker, &hud->sbarReadyammo },
        { GUI_READYAMMOICON, UWG_STATUSBAR,   0,            SBarReadyAmmoIcon_UpdateDimensions, SBarReadyAmmoIcon_Drawer, ReadyAmmoIcon_Ticker, &hud->sbarReadyammoicon },
        { GUI_READYITEM,    UWG_STATUSBAR,    GF_SMALLIN,   SBarReadyItem_UpdateDimensions, SBarReadyItem_Drawer, ReadyItem_Ticker, &hud->sbarReadyitem },
        { GUI_CHAIN,        UWG_STATUSBAR,    0,            SBarChain_UpdateDimensions, SBarChain_Drawer, SBarChain_Ticker, &hud->sbarChain },
        { GUI_MAPNAME,      UWG_MAPNAME,      GF_FONTB,     MapName_UpdateDimensions, MapName_Drawer },
        { GUI_READYAMMOICON, UWG_TOPLEFT,     0,            ReadyAmmoIcon_UpdateDimensions, ReadyAmmoIcon_Drawer, ReadyAmmoIcon_Ticker, &hud->readyammoicon },
        { GUI_READYAMMO,    UWG_TOPLEFT,      GF_STATUS,    ReadyAmmo_UpdateDimensions, ReadyAmmo_Drawer, ReadyAmmo_Ticker, &hud->readyammo },
        { GUI_FLIGHT,       UWG_TOPLEFT2,     0,            Flight_UpdateDimensions, Flight_Drawer, Flight_Ticker, &hud->flight },
        { GUI_TOME,         UWG_TOPRIGHT,     GF_SMALLIN,   Tome_UpdateDimensions, Tome_Drawer, Tome_Ticker, &hud->tome },
        { GUI_HEALTH,       UWG_BOTTOMLEFT,   GF_FONTB,     Health_UpdateDimensions, Health_Drawer, Health_Ticker, &hud->health },
        { GUI_KEYS,         UWG_BOTTOMLEFT,   0,            Keys_UpdateDimensions, Keys_Drawer, Keys_Ticker, &hud->keys },
        { GUI_ARMOR,        UWG_BOTTOMLEFT,   GF_STATUS,    Armor_UpdateDimensions, Armor_Drawer, Armor_Ticker, &hud->armor },
        { GUI_FRAGS,        UWG_BOTTOMLEFT2,  GF_STATUS,    Frags_UpdateDimensions, Frags_Drawer, Frags_Ticker, &hud->frags },
        { GUI_READYITEM,    UWG_BOTTOMRIGHT,  GF_SMALLIN,   ReadyItem_UpdateDimensions, ReadyItem_Drawer, ReadyItem_Ticker, &hud->readyitem },
        { GUI_INVENTORY,    UWG_BOTTOM,       GF_SMALLIN,   Inventory_UpdateDimensions, Inventory_Drawer },
        { GUI_SECRETS,      UWG_COUNTERS,     GF_FONTA,     Secrets_UpdateDimensions, Secrets_Drawer, Secrets_Ticker, &hud->secrets },
        { GUI_ITEMS,        UWG_COUNTERS,     GF_FONTA,     Items_UpdateDimensions, Items_Drawer, Items_Ticker, &hud->items },
        { GUI_KILLS,        UWG_COUNTERS,     GF_FONTA,     Kills_UpdateDimensions, Kills_Drawer, Kills_Ticker, &hud->kills },
        { GUI_NONE }
    };
    size_t i;

    if(player < 0 || player >= MAXPLAYERS)
    {
        Con_Error("ST_BuildWidgets: Invalid player #%i.", player);
        exit(1); // Unreachable.
    }

    for(i = 0; i < sizeof(widgetGroupDefs)/sizeof(widgetGroupDefs[0]); ++i)
    {
        const uiwidgetgroupdef_t* def = &widgetGroupDefs[i];
        hud->widgetGroupIds[def->group] = GUI_CreateGroup(player, def->groupFlags, def->alignFlags, def->padding);
    }

    for(i = 0; widgetDefs[i].type != GUI_NONE; ++i)
    {
        const uiwidgetdef_t* def = &widgetDefs[i];
        uiwidgetid_t id = GUI_CreateWidget(def->type, player, FID(def->fontIdx), def->updateDimensions, def->drawer, def->ticker, def->typedata);
        UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[def->group]), GUI_FindObjectById(id));
    }

    hud->logWidgetId = GUI_CreateWidget(GUI_LOG, player, FID(GF_FONTA), UILog_UpdateDimensions, UILog_Drawer, UILog_Ticker, &hud->log);
    UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOP]), GUI_FindObjectById(hud->logWidgetId));

    hud->chatWidgetId = GUI_CreateWidget(GUI_CHAT, player, FID(GF_FONTA), UIChat_UpdateDimensions, UIChat_Drawer, NULL, &hud->chat);
    UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOP]), GUI_FindObjectById(hud->chatWidgetId));

    hud->automapWidgetId = GUI_CreateWidget(GUI_AUTOMAP, player, FID(GF_FONTA), UIAutomap_UpdateDimensions, UIAutomap_Drawer, UIAutomap_Ticker, &hud->automap);
    UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_AUTOMAP]), GUI_FindObjectById(hud->automapWidgetId));

#undef PADDING
}

void ST_Init(void)
{
    int i;
    ST_InitAutomapConfig();
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        hudstate_t* hud = &hudStates[i];
        ST_BuildWidgets(i);
        hud->inited = true;
    }
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

uiwidget_t* ST_UIChatForPlayer(int player)
{
    if(player >= 0 && player < MAXPLAYERS)
    {
        hudstate_t* hud = &hudStates[player];
        return GUI_FindObjectById(hud->chatWidgetId);
    }
    Con_Error("ST_UIChatForPlayer: Invalid player #%i.", player);
    exit(1); // Unreachable.
}

uiwidget_t* ST_UILogForPlayer(int player)
{
    if(player >= 0 && player < MAXPLAYERS)
    {
        hudstate_t* hud = &hudStates[player];
        return GUI_FindObjectById(hud->logWidgetId);
    }
    Con_Error("ST_UILogForPlayer: Invalid player #%i.", player);
    exit(1); // Unreachable.
}

uiwidget_t* ST_UIAutomapForPlayer(int player)
{
    if(player >= 0 && player < MAXPLAYERS)
    {
        hudstate_t* hud = &hudStates[player];
        return GUI_FindObjectById(hud->automapWidgetId);
    }
    Con_Error("ST_UIAutomapForPlayer: Invalid player #%i.", player);
    exit(1); // Unreachable.
}

int ST_ChatResponder(int player, event_t* ev)
{
    uiwidget_t* obj = ST_UIChatForPlayer(player);
    if(NULL != obj)
    {
        return UIChat_Responder(obj, ev);
    }
    return false;
}

boolean ST_ChatIsActive(int player)
{    
    uiwidget_t* obj = ST_UIChatForPlayer(player);
    if(NULL == obj) return false;
    return UIChat_IsActive(obj);
}

void ST_LogPost(int player, byte flags, const char* msg)
{
    uiwidget_t* obj = ST_UILogForPlayer(player);
    if(NULL == obj) return;
    UILog_Post(obj, flags, msg);
}

void ST_LogRefresh(int player)
{
    uiwidget_t* obj = ST_UILogForPlayer(player);
    if(NULL == obj) return;
    UILog_Refresh(obj);
}

void ST_LogEmpty(int player)
{
    uiwidget_t* obj = ST_UILogForPlayer(player);
    if(NULL == obj) return;
    UILog_Empty(obj);
}

void ST_LogPostVisibilityChangeNotification(void)
{
    int i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        ST_LogPost(i, LMF_NOHIDE, !cfg.hudShown[HUD_LOG] ? MSGOFF : MSGON);
    }
}

void ST_LogUpdateAlignment(void)
{
    int i, flags;
    uiwidget_t* obj;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        hudstate_t* hud = &hudStates[i];
        if(!hud->inited) continue;

        obj = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOP]);
        flags = UIWidget_Alignment(obj);
        flags &= ~(ALIGN_LEFT|ALIGN_RIGHT);
        if(cfg.msgAlign == 0)
            flags |= ALIGN_LEFT;
        else if(cfg.msgAlign == 2)
            flags |= ALIGN_RIGHT;
        UIWidget_SetAlignment(obj, flags);
    }
}

void ST_AutomapOpen(int player, boolean yes, boolean fast)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(NULL == obj) return;
    UIAutomap_Open(obj, yes, fast);
}

boolean ST_AutomapIsActive(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(NULL == obj) return false;
    return UIAutomap_Active(obj);
}

boolean ST_AutomapWindowObscures2(int player, const rectanglei_t* region)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(NULL == obj) return false;
    if(UIAutomap_Active(obj))
    {
        if(cfg.automapOpacity * ST_AutomapOpacity(player) >= ST_AUTOMAP_OBSCURE_TOLERANCE)
        {
            /*if(UIAutomap_Fullscreen(obj))
            {*/
                return true;
            /*}
            else
            {
                // We'll have to compare the dimensions.
                const int scrwidth  = Get(DD_WINDOW_WIDTH);
                const int scrheight = Get(DD_WINDOW_HEIGHT);
                const rectanglei_t* dims = UIWidget_Dimensions(obj);
                float fx = FIXXTOSCREENX(region->x);
                float fy = FIXYTOSCREENY(region->y);
                float fw = FIXXTOSCREENX(region->width);
                float fh = FIXYTOSCREENY(region->height);

                if(dims->x >= fx && dims->y >= fy && dims->width >= fw && dims->height >= fh)
                    return true;
            }*/
        }
    }
    return false;
}

boolean ST_AutomapWindowObscures(int player, int x, int y, int width, int height)
{
    rectanglei_t rect;
    rect.x = x;
    rect.y = y;
    rect.width  = width;
    rect.height = height;
    return ST_AutomapWindowObscures2(player, &rect);
}

void ST_AutomapClearPoints(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(NULL == obj) return;
    UIAutomap_ClearPoints(obj);
}

/**
 * Adds a marker at the specified X/Y location.
 */
int ST_AutomapAddPoint(int player, float x, float y, float z)
{
    static char buffer[20];
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    int newPoint;
    if(NULL == obj) return - 1;

    if(UIAutomap_PointCount(obj) == MAX_MAP_POINTS)
        return -1;

    newPoint = UIAutomap_AddPoint(obj, x, y, z);
    sprintf(buffer, "%s %d", AMSTR_MARKEDSPOT, newPoint);
    P_SetMessage(&players[player], buffer, false);

    return newPoint;
}

boolean ST_AutomapPointOrigin(int player, int point, float* x, float* y, float* z)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(NULL == obj) return false;
    return UIAutomap_PointOrigin(obj, point, x, y, z);
}

void ST_ToggleAutomapMaxZoom(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(NULL == obj) return;
    if(UIAutomap_SetZoomMax(obj, !UIAutomap_ZoomMax(obj)))
    {
        Con_Printf("Maximum zoom %s in automap.\n", UIAutomap_ZoomMax(obj)? "ON":"OFF");
    }
}

float ST_AutomapOpacity(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(NULL == obj) return 0;
    return UIAutomap_Opacity(obj);
}

void ST_SetAutomapCameraRotation(int player, boolean on)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(NULL == obj) return;
    UIAutomap_SetCameraRotation(obj, on);
}

void ST_ToggleAutomapPanMode(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(NULL == obj) return;
    if(UIAutomap_SetPanMode(obj, !UIAutomap_PanMode(obj)))
    {
        P_SetMessage(&players[player], (UIAutomap_PanMode(obj)? AMSTR_FOLLOWON : AMSTR_FOLLOWOFF), true);
    }
}

void ST_CycleAutomapCheatLevel(int player)
{
    if(player >= 0 && player < MAXPLAYERS)
    {
        hudstate_t* hud = &hudStates[player];
        ST_SetAutomapCheatLevel(player, (hud->automapCheatLevel + 1) % 3);
    }
}

void ST_SetAutomapCheatLevel(int player, int level)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(NULL == obj) return;
    setAutomapCheatLevel(obj, level);
}

void ST_RevealAutomap(int player, boolean on)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(NULL == obj) return;
    UIAutomap_SetReveal(obj, on);
}

boolean ST_AutomapHasReveal(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(NULL == obj) return false;
    return UIAutomap_Reveal(obj);
}

void ST_RebuildAutomap(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(NULL == obj) return;
    UIAutomap_Rebuild(obj);
}

int ST_AutomapCheatLevel(int player)
{
    if(player >=0 && player < MAXPLAYERS)
        return hudStates[player].automapCheatLevel;
    return 0;
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

D_CMD(ChatOpen)
{
    int player = CONSOLEPLAYER, destination = 0;
    uiwidget_t* obj;

    if(G_GetGameAction() == GA_QUIT)
    {
        return false;
    }

    obj = ST_UIChatForPlayer(player);
    if(NULL == obj)
    {
        return false;
    }

    if(argc == 2)
    {
        destination = UIChat_ParseDestination(argv[1]);
        if(destination < 0)
        {
            Con_Message("Invalid team number #%i (valid range: 0...%i).\n", destination, NUMTEAMS);
            return false;
        }
    }
    UIChat_SetDestination(obj, destination);
    UIChat_Activate(obj, true);
    return true;
}

D_CMD(ChatAction)
{
    int player = CONSOLEPLAYER;
    const char* cmd = argv[0] + 4;
    uiwidget_t* obj;

    if(G_GetGameAction() == GA_QUIT)
    {
        return false;
    }

    obj = ST_UIChatForPlayer(player);
    if(NULL == obj || !UIChat_IsActive(obj))
    {
        return false;
    }
    if(!stricmp(cmd, "complete")) // Send the message.
    {
        return UIChat_CommandResponder(obj, MCMD_SELECT);
    }
    else if(!stricmp(cmd, "cancel")) // Close chat.
    {
        return UIChat_CommandResponder(obj, MCMD_CLOSE);
    }
    else if(!stricmp(cmd, "delete"))
    {
        return UIChat_CommandResponder(obj, MCMD_DELETE);
    }
    return true;
}

D_CMD(ChatSendMacro)
{
    int player = CONSOLEPLAYER, macroId, destination = 0;
    uiwidget_t* obj;

    if(G_GetGameAction() == GA_QUIT)
        return false;

    if(argc < 2 || argc > 3)
    {
        Con_Message("Usage: %s (team) (macro number)\n", argv[0]);
        Con_Message("Send a chat macro to other player(s).\n"
                    "If (team) is omitted, the message will be sent to all players.\n");
        return true;
    }

    obj = ST_UIChatForPlayer(player);
    if(NULL == obj)
    {
        return false;
    }

    if(argc == 3)
    {
        destination = UIChat_ParseDestination(argv[1]);
        if(destination < 0)
        {
            Con_Message("Invalid team number #%i (valid range: 0...%i).\n", destination, NUMTEAMS);
            return false;
        }
    }

    macroId = UIChat_ParseMacroId(argc == 3? argv[2] : argv[1]);
    if(-1 == macroId)
    {
        Con_Message("Invalid macro id.\n");
        return false;
    }

    UIChat_Activate(obj, true);
    UIChat_SetDestination(obj, destination);
    UIChat_LoadMacro(obj, macroId);
    UIChat_CommandResponder(obj, MCMD_SELECT);
    UIChat_Activate(obj, false);
    return true;
}
