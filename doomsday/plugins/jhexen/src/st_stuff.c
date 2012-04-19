/**\file st_stuff.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jhexen.h"
#include "d_net.h"

#include "p_tick.h" // for P_IsPaused
#include "g_common.h"
#include "p_inventory.h"
#include "p_mapsetup.h"
#include "p_player.h"
#include "hu_automap.h"
#include "hu_chat.h"
#include "hu_lib.h"
#include "hu_log.h"
#include "hu_inventory.h"
#include "hu_stuff.h"
#include "r_common.h"
#include "gl_drawpatch.h"
#include "am_map.h"

// Inventory
#define ST_INVENTORYX       (50)
#define ST_INVENTORYY       (1)

// Current inventory item.
#define ST_INVITEMX         (143)
#define ST_INVITEMY         (1)

// Current inventory item count.
#define ST_INVITEMCWIDTH    (2) // Num digits
#define ST_INVITEMCX        (174)
#define ST_INVITEMCY        (22)

// HEALTH number pos.
#define ST_HEALTHWIDTH          3
#define ST_HEALTHX              64
#define ST_HEALTHY              14

// MANA A
#define ST_MANAAWIDTH           3
#define ST_MANAAX               91
#define ST_MANAAY               19

// MANA A ICON
#define ST_MANAAICONX           77
#define ST_MANAAICONY           2

// MANA A VIAL
#define ST_MANAAVIALX           94
#define ST_MANAAVIALY           2

// MANA B
#define ST_MANABWIDTH           3
#define ST_MANABX               123
#define ST_MANABY               19

// MANA B ICON
#define ST_MANABICONX           110
#define ST_MANABICONY           2

// MANA B VIAL
#define ST_MANABVIALX           102
#define ST_MANABVIALY           2

// ARMOR number pos.
#define ST_ARMORWIDTH           2
#define ST_ARMORX               274
#define ST_ARMORY               14

// Frags pos.
#define ST_FRAGSWIDTH           3
#define ST_FRAGSX               64
#define ST_FRAGSY               14

enum {
    UWG_STATUSBAR = 0,
    UWG_MAPNAME,
    UWG_BOTTOMLEFT,
    UWG_BOTTOMRIGHT,
    UWG_BOTTOMCENTER,
    UWG_BOTTOM,
    UWG_TOP,
    UWG_TOPCENTER,
    UWG_TOPLEFT,
    UWG_TOPLEFT2,
    UWG_TOPLEFT3,
    UWG_TOPRIGHT,
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
    boolean statusbarActive; // Whether the statusbar is active.
    int automapCheatLevel; /// \todo Belongs in player state?
    int readyItemFlashCounter;

    int widgetGroupIds[NUM_UIWIDGET_GROUPS];
    int automapWidgetId;
    int chatWidgetId;
    int logWidgetId;

    // Statusbar:
    guidata_health_t sbarHealth;
    guidata_weaponpieces_t sbarWeaponpieces;
    guidata_bluemanaicon_t sbarBluemanaicon;
    guidata_bluemana_t sbarBluemana;
    guidata_bluemanavial_t sbarBluemanavial;
    guidata_greenmanaicon_t sbarGreenmanaicon;
    guidata_greenmana_t sbarGreenmana;
    guidata_greenmanavial_t sbarGreenmanavial;
    guidata_keys_t sbarKeys;
    guidata_armoricons_t sbarArmoricons;
    guidata_chain_t sbarChain;
    guidata_armor_t sbarArmor;
    guidata_frags_t sbarFrags;
    guidata_readyitem_t sbarReadyitem;

    // Fullscreen:
    guidata_health_t health;
    guidata_frags_t frags;
    guidata_bluemanaicon_t bluemanaicon;
    guidata_bluemana_t bluemana;
    guidata_greenmanaicon_t greenmanaicon;
    guidata_greenmana_t greenmana;
    guidata_readyitem_t readyitem;

    // Other:
    guidata_automap_t automap;
    guidata_chat_t chat;
    guidata_log_t log;
    guidata_flight_t flight;
    guidata_boots_t boots;
    guidata_servant_t servant;
    guidata_defense_t defense;
    guidata_worldtimer_t worldtimer;
} hudstate_t;

void updateViewWindow(void);
void unhideHUD(void);

static hudstate_t hudStates[MAXPLAYERS];

static patchid_t pStatusBar;
static patchid_t pStatusBarTop;
static patchid_t pKills;
static patchid_t pStatBar;
static patchid_t pKeyBar;
static patchid_t pKeySlot[NUM_KEY_TYPES];
static patchid_t pArmorSlot[NUMARMOR];
static patchid_t pManaAVials[2];
static patchid_t pManaBVials[2];
static patchid_t pManaAIcons[2];
static patchid_t pManaBIcons[2];
static patchid_t pInventoryBar;
static patchid_t pWeaponSlot[3]; // [Fighter, Cleric, Mage]
static patchid_t pWeaponFull[3]; // [Fighter, Cleric, Mage]
static patchid_t pLifeGem[3][8]; // [Fighter, Cleric, Mage][color]
static patchid_t pWeaponPiece1[3]; // [Fighter, Cleric, Mage]
static patchid_t pWeaponPiece2[3]; // [Fighter, Cleric, Mage]
static patchid_t pWeaponPiece3[3]; // [Fighter, Cleric, Mage]
static patchid_t pChain[3]; // [Fighter, Cleric, Mage]
static patchid_t pInvItemFlash[5];
static patchid_t pSpinFly[16];
static patchid_t pSpinMinotaur[16];
static patchid_t pSpinSpeed[16];
static patchid_t pSpinDefense[16];

void ST_Register(void)
{
    C_VAR_FLOAT2( "hud-color-r", &cfg.hudColor[0], 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-color-g", &cfg.hudColor[1], 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-color-b", &cfg.hudColor[2], 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-color-a", &cfg.hudColor[3], 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-icon-alpha", &cfg.hudIconAlpha, 0, 0, 1, unhideHUD )
    C_VAR_INT(    "hud-patch-replacement", &cfg.hudPatchReplaceMode, 0, PRM_FIRST, PRM_LAST )
    C_VAR_FLOAT2( "hud-scale", &cfg.hudScale, 0, 0.1f, 1, unhideHUD )
    C_VAR_FLOAT(  "hud-timer", &cfg.hudTimer, 0, 0, 60 )

    // Displays
    C_VAR_BYTE2(  "hud-currentitem", &cfg.hudShown[HUD_READYITEM], 0, 0, 1, unhideHUD )
    C_VAR_BYTE2(  "hud-health", &cfg.hudShown[HUD_HEALTH], 0, 0, 1, unhideHUD )
    C_VAR_BYTE2(  "hud-mana", &cfg.hudShown[HUD_MANA], 0, 0, 1, unhideHUD )

    C_VAR_FLOAT2( "hud-status-alpha", &cfg.statusbarOpacity, 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-status-icon-a", &cfg.statusbarCounterAlpha, 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-status-size", &cfg.statusbarScale, 0, 0.1f, 1, updateViewWindow )

    // Events.
    C_VAR_BYTE(   "hud-unhide-damage", &cfg.hudUnHide[HUE_ON_DAMAGE], 0, 0, 1 )
    C_VAR_BYTE(   "hud-unhide-pickup-ammo", &cfg.hudUnHide[HUE_ON_PICKUP_AMMO], 0, 0, 1 )
    C_VAR_BYTE(   "hud-unhide-pickup-armor", &cfg.hudUnHide[HUE_ON_PICKUP_ARMOR], 0, 0, 1 )
    C_VAR_BYTE(   "hud-unhide-pickup-health", &cfg.hudUnHide[HUE_ON_PICKUP_HEALTH], 0, 0, 1 )
    C_VAR_BYTE(   "hud-unhide-pickup-invitem", &cfg.hudUnHide[HUE_ON_PICKUP_INVITEM], 0, 0, 1 )
    C_VAR_BYTE(   "hud-unhide-pickup-key", &cfg.hudUnHide[HUE_ON_PICKUP_KEY], 0, 0, 1 )
    C_VAR_BYTE(   "hud-unhide-pickup-powerup", &cfg.hudUnHide[HUE_ON_PICKUP_POWER], 0, 0, 1 )
    C_VAR_BYTE(   "hud-unhide-pickup-weapon", &cfg.hudUnHide[HUE_ON_PICKUP_WEAPON], 0, 0, 1 )

    C_CMD("beginchat",       NULL,   ChatOpen )
    C_CMD("chatcancel",      "",     ChatAction )
    C_CMD("chatcomplete",    "",     ChatAction )
    C_CMD("chatdelete",      "",     ChatAction )
    C_CMD("chatsendmacro",   NULL,   ChatSendMacro )

    Hu_InventoryRegister();
}

static int headupDisplayMode(int player)
{
    return (cfg.screenBlocks < 10? 0 : cfg.screenBlocks - 10);
}

void Flight_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_flight_t* flht = (guidata_flight_t*)obj->typedata;
    const player_t* plr = &players[obj->player];

    if(P_IsPaused() || !DD_IsSharpTick()) return;

    flht->patchId = 0;
    if(!plr->powers[PT_FLIGHT]) return;

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
        flht->patchId = pSpinFly[frame];
    }
}

void Flight_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
    guidata_flight_t* flht = (guidata_flight_t*)obj->typedata;
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    if(flht->patchId != 0)
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        if(offset) DGL_Translatef(offset->x, offset->y, 0);
        DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
        DGL_Enable(DGL_TEXTURE_2D);

        DGL_Color4f(1, 1, 1, iconAlpha);
        GL_DrawPatchXY(flht->patchId, 16, 14);

        DGL_Disable(DGL_TEXTURE_2D);
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }
}

void Flight_UpdateGeometry(uiwidget_t* obj)
{
    guidata_flight_t* flht = (guidata_flight_t*)obj->typedata;
    const player_t* plr = &players[obj->player];

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(!plr->powers[PT_FLIGHT]) return;

    Rect_SetWidthHeight(obj->geometry, 32 * cfg.hudScale, 28 * cfg.hudScale);
}

void Boots_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_boots_t* boots = (guidata_boots_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    if(P_IsPaused() || !DD_IsSharpTick()) return;

    boots->patchId = 0;
    if(0 != plr->powers[PT_SPEED] &&
       (plr->powers[PT_SPEED] > BLINKTHRESHOLD || !(plr->powers[PT_SPEED] & 16)))
    {
        boots->patchId = pSpinSpeed[(mapTime / 3) & 15];
    }
}

void Boots_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
    guidata_boots_t* boots = (guidata_boots_t*)obj->typedata;
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(boots->patchId == 0) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(1, 1, 1, iconAlpha);
    GL_DrawPatchXY(boots->patchId, 12, 14);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void Boots_UpdateGeometry(uiwidget_t* obj)
{
    guidata_boots_t* boots = (guidata_boots_t*)obj->typedata;
    const player_t* plr = &players[obj->player];

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(!plr->powers[PT_SPEED]) return;

    Rect_SetWidthHeight(obj->geometry, 24 * cfg.hudScale, 28 * cfg.hudScale);
}

void Defense_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_defense_t* dfns = (guidata_defense_t*)obj->typedata;
    const player_t* plr = &players[obj->player];

    if(P_IsPaused() || !DD_IsSharpTick()) return;
    dfns->patchId = 0;
    if(!plr->powers[PT_INVULNERABILITY]) return;

    if(plr->powers[PT_INVULNERABILITY] > BLINKTHRESHOLD || !(plr->powers[PT_INVULNERABILITY] & 16))
    {
        dfns->patchId = pSpinDefense[(mapTime / 3) & 15];
    }
}

void Defense_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
    guidata_defense_t* dfns = (guidata_defense_t*)obj->typedata;
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(dfns->patchId == 0) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(1, 1, 1, iconAlpha);
    GL_DrawPatchXY(dfns->patchId, 13, 14);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void Defense_UpdateGeometry(uiwidget_t* obj)
{
    guidata_defense_t* dfns = (guidata_defense_t*)obj->typedata;
    const player_t* plr = &players[obj->player];

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(!plr->powers[PT_INVULNERABILITY]) return;

    Rect_SetWidthHeight(obj->geometry, 26 * cfg.hudScale, 28 * cfg.hudScale);
}

void Servant_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_servant_t* svnt = (guidata_servant_t*)obj->typedata;
    const player_t* plr = &players[obj->player];

    if(P_IsPaused() || !DD_IsSharpTick()) return;

    svnt->patchId = 0;
    if(!plr->powers[PT_MINOTAUR]) return;

    if(plr->powers[PT_MINOTAUR] > BLINKTHRESHOLD || !(plr->powers[PT_MINOTAUR] & 16))
    {
        svnt->patchId = pSpinMinotaur[(mapTime / 3) & 15];
    }
}

void Servant_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
    guidata_servant_t* svnt = (guidata_servant_t*)obj->typedata;
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(svnt->patchId == 0) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(1, 1, 1, iconAlpha);
    GL_DrawPatchXY(svnt->patchId, 13, 17);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void Servant_UpdateGeometry(uiwidget_t* obj)
{
    guidata_servant_t* svnt = (guidata_servant_t*)obj->typedata;
    const player_t* plr = &players[obj->player];

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(!plr->powers[PT_MINOTAUR]) return;

    Rect_SetWidthHeight(obj->geometry, 26 * cfg.hudScale, 29 * cfg.hudScale);
}

void WeaponPieces_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_weaponpieces_t* wpn = (guidata_weaponpieces_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    if(P_IsPaused() || !DD_IsSharpTick()) return;

    wpn->pieces = plr->pieces;
}

void SBarWeaponPieces_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
#define ORIGINX (-ST_WIDTH/2)
#define ORIGINY (-ST_HEIGHT*hud->showBar)

    guidata_weaponpieces_t* wpn = (guidata_weaponpieces_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int pClass = cfg.playerClass[obj->player]; // Original player class (i.e. not pig).
    int fullscreen = headupDisplayMode(obj->player);
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(obj->player) || ST_AutomapIsActive(obj->player)) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    if(wpn->pieces == 7)
    {
        DGL_Color4f(1, 1, 1, iconAlpha);
        GL_DrawPatchXY(pWeaponFull[pClass], ORIGINX+190, ORIGINY);
    }
    else
    {
        if(wpn->pieces & WPIECE1)
        {
            DGL_Color4f(1, 1, 1, iconAlpha);
            GL_DrawPatchXY(pWeaponPiece1[pClass], ORIGINX+PCLASS_INFO(pClass)->pieceX[0], ORIGINY);
        }

        if(wpn->pieces & WPIECE2)
        {
            DGL_Color4f(1, 1, 1, iconAlpha);
            GL_DrawPatchXY(pWeaponPiece2[pClass], ORIGINX+PCLASS_INFO(pClass)->pieceX[1], ORIGINY);
        }

        if(wpn->pieces & WPIECE3)
        {
            DGL_Color4f(1, 1, 1, iconAlpha);
            GL_DrawPatchXY(pWeaponPiece3[pClass], ORIGINX+PCLASS_INFO(pClass)->pieceX[2], ORIGINY);
        }
    }

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef ORIGINX
#undef ORIGINY
}

void SBarWeaponPieces_UpdateGeometry(uiwidget_t* obj)
{
    //guidata_weaponpieces_t* wpn = (guidata_weaponpieces_t*)obj->typedata;

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(Hu_InventoryIsOpen(obj->player) || ST_AutomapIsActive(obj->player)) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    Rect_SetWidthHeight(obj->geometry, 57 * cfg.statusbarScale, 30 * cfg.statusbarScale);
}

void SBarChain_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_chain_t* chain = (guidata_chain_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    // Health marker chain animates up to the actual health value.
    int delta, curHealth = MAX_OF(plr->plr->mo->health, 0);
    if(P_IsPaused() || !DD_IsSharpTick()) return;

    if(curHealth < chain->healthMarker)
    {
        delta = MINMAX_OF(1, (chain->healthMarker - curHealth) >> 2, 6);
        chain->healthMarker -= delta;
    }
    else if(curHealth > chain->healthMarker)
    {
        delta = MINMAX_OF(1, (curHealth - chain->healthMarker) >> 2, 6);
        chain->healthMarker += delta;
    }
}

void SBarChain_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
#define ORIGINX (-ST_WIDTH/2)
#define ORIGINY (0)

    static int theirColors[] = {
        157, // Blue
        177, // Red
        137, // Yellow
        198, // Green
        215, // Jade
        32,  // White
        106, // Hazel
        234  // Purple
    };
    guidata_chain_t* chain = (guidata_chain_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int x, y, w, h;
    int pClass, pColor;
    float healthPos;
    int gemXOffset;
    float gemglow, rgb[3];
    int chainYOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = headupDisplayMode(obj->player);
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    patchinfo_t pChainInfo, pGemInfo;

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    // Original player class (i.e. not pig).
    pClass = cfg.playerClass[obj->player];

    if(!IS_NETGAME)
    {
        pColor = 1; // Always use the red life gem (the second gem).
    }
    else
    {
        pColor = cfg.playerColor[obj->player];

        if(pClass == PCLASS_FIGHTER)
        {
            if(pColor == 0)
                pColor = 2;
            else if(pColor == 2)
                pColor = 0;
        }
    }

    if(!R_GetPatchInfo(pChain[pClass], &pChainInfo)) return;
    if(!R_GetPatchInfo(pLifeGem[pClass][pColor], &pGemInfo)) return;

    healthPos = MINMAX_OF(0, chain->healthMarker / 100.f, 100);
    gemglow = healthPos;

    // Draw the chain.
    x = ORIGINX+43;
    y = ORIGINY-7;
    w = ST_WIDTH - 43 - 43;
    h = 7;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, chainYOffset, 0);

    DGL_SetPatch(pChainInfo.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(1, 1, 1, iconAlpha);

    gemXOffset = 7 + ROUND((w - 14) * healthPos) - pGemInfo.geometry.size.width/2;

    if(gemXOffset > 0)
    {   // Left chain section.
        float cw = (float)(pChainInfo.geometry.size.width - gemXOffset) / pChainInfo.geometry.size.width;

        DGL_Begin(DGL_QUADS);
            DGL_TexCoord2f(0, cw, 0);
            DGL_Vertex2f(x, y);

            DGL_TexCoord2f(0, 1, 0);
            DGL_Vertex2f(x + gemXOffset, y);

            DGL_TexCoord2f(0, 1, 1);
            DGL_Vertex2f(x + gemXOffset, y + h);

            DGL_TexCoord2f(0, cw, 1);
            DGL_Vertex2f(x, y + h);
        DGL_End();
    }

    if(gemXOffset + pGemInfo.geometry.size.width < w)
    {   // Right chain section.
        float cw = (w - (float)gemXOffset - pGemInfo.geometry.size.width) / pChainInfo.geometry.size.width;

        DGL_Begin(DGL_QUADS);
            DGL_TexCoord2f(0, 0, 0);
            DGL_Vertex2f(x + gemXOffset + pGemInfo.geometry.size.width, y);

            DGL_TexCoord2f(0, cw, 0);
            DGL_Vertex2f(x + w, y);

            DGL_TexCoord2f(0, cw, 1);
            DGL_Vertex2f(x + w, y + h);

            DGL_TexCoord2f(0, 0, 1);
            DGL_Vertex2f(x + gemXOffset + pGemInfo.geometry.size.width, y + h);
        DGL_End();
    }

    // Draw the life gem.
    {
    int vX = x + MAX_OF(0, gemXOffset);
    int vWidth;
    float s1 = 0, s2 = 1;

    vWidth = pGemInfo.geometry.size.width;
    if(gemXOffset + pGemInfo.geometry.size.width > w)
    {
        vWidth -= gemXOffset + pGemInfo.geometry.size.width - w;
        s2 = (float)vWidth / pGemInfo.geometry.size.width;
    }
    if(gemXOffset < 0)
    {
        vWidth -= -gemXOffset;
        s1 = (float)(-gemXOffset) / pGemInfo.geometry.size.width;
    }

    DGL_SetPatch(pGemInfo.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
    DGL_Begin(DGL_QUADS);
        DGL_TexCoord2f(0, s1, 0);
        DGL_Vertex2f(vX, y);

        DGL_TexCoord2f(0, s2, 0);
        DGL_Vertex2f(vX + vWidth, y);

        DGL_TexCoord2f(0, s2, 1);
        DGL_Vertex2f(vX + vWidth, y + h);

        DGL_TexCoord2f(0, s1, 1);
        DGL_Vertex2f(vX, y + h);
    DGL_End();
    }

    // How about a glowing gem?
    DGL_BlendMode(BM_ADD);
    DGL_Bind(Get(DD_DYNLIGHT_TEXTURE));

    R_GetColorPaletteRGBf(0, theirColors[pColor], rgb, false);
    DGL_DrawRectf2Color(x + gemXOffset + 23, y - 6, 41, 24, rgb[0], rgb[1], rgb[2], gemglow - (1 - iconAlpha));

    DGL_BlendMode(BM_NORMAL);
    DGL_Color4f(1, 1, 1, 1);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef ORIGINX
#undef ORIGINY
}

void SBarChain_UpdateGeometry(uiwidget_t* obj)
{
    assert(obj);

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    Rect_SetWidthHeight(obj->geometry, (ST_WIDTH - 21 - 28) * cfg.statusbarScale,
                                       8 * cfg.statusbarScale);
}

/**
 * Draws the whole statusbar backgound.
 * \todo There is a whole lot of constants in here. What if someone wants to
 * replace the statusbar with new patches?
 */
void SBarBackground_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
#define WIDTH       (ST_WIDTH)
#define HEIGHT      (ST_HEIGHT)
#define ORIGINX     ((int)(-WIDTH/2))
#define ORIGINY     ((int)(-HEIGHT * hud->showBar))

    const hudstate_t* hud = &hudStates[obj->player];
    int x, y, w, h, pClass = cfg.playerClass[obj->player]; // Original class (i.e. not pig).
    int fullscreen = headupDisplayMode(obj->player);
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarOpacity);
    float cw, ch;

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);

    if(!(iconAlpha < 1))
    {
        DGL_Enable(DGL_TEXTURE_2D);

        DGL_Color4f(1, 1, 1, 1);
        GL_DrawPatchXY(pStatusBar, ORIGINX, ORIGINY-28);

        DGL_Disable(DGL_TEXTURE_2D);

        /**
         * \kludge The Hexen statusbar graphic has a chain already in the
         * image, which shows through the modified chain patches.
         * Mask out the chain on the statusbar by drawing a solid black
         * rectangle over it.
         */
        DGL_SetNoMaterial();
        DGL_DrawRectf2Color(ORIGINX+44, ORIGINY+31, 232, 7, .1f, .1f, .1f, 1);
        //// \kludge end

        DGL_Enable(DGL_TEXTURE_2D);

        DGL_Color4f(1, 1, 1, 1);
        GL_DrawPatchXY(pStatusBarTop, ORIGINX, ORIGINY-28);

        if(!Hu_InventoryIsOpen(obj->player))
        {
            // Main interface
            if(!ST_AutomapIsActive(obj->player))
            {
                GL_DrawPatchXY(pStatBar, ORIGINX+38, ORIGINY);

                if(deathmatch)
                {
                    GL_DrawPatchXY(pKills, ORIGINX+38, ORIGINY);
                }

                GL_DrawPatchXY(pWeaponSlot[pClass], ORIGINX+190, ORIGINY);
            }
            else
            {
                GL_DrawPatchXY(pKeyBar, ORIGINX+38, ORIGINY);
            }
        }
        else
        {
            GL_DrawPatchXY(pInventoryBar, ORIGINX+38, ORIGINY);
        }

        DGL_Disable(DGL_TEXTURE_2D);
    }
    else
    {
        DGL_Enable(DGL_TEXTURE_2D);

        DGL_Color4f(1, 1, 1, iconAlpha);
        DGL_SetPatch(pStatusBar, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);

        DGL_Begin(DGL_QUADS);

        // top
        x = ORIGINX;
        y = ORIGINY-27;
        w = ST_WIDTH;
        h = 27;
        ch = 0.41538461538461538461538461538462f;

        DGL_TexCoord2f(0, 0, 0);
        DGL_Vertex2f(x, y);
        DGL_TexCoord2f(0, 1, 0);
        DGL_Vertex2f(x + w, y);
        DGL_TexCoord2f(0, 1, ch);
        DGL_Vertex2f(x + w, y + h);
        DGL_TexCoord2f(0, 0, ch);
        DGL_Vertex2f(x, y + h);

        // left statue
        x = ORIGINX;
        y = ORIGINY;
        w = 38;
        h = 38;
        cw = (float) 38 / ST_WIDTH;
        ch = 0.41538461538461538461538461538462f;

        DGL_TexCoord2f(0, 0, ch);
        DGL_Vertex2f(x, y);
        DGL_TexCoord2f(0, cw, ch);
        DGL_Vertex2f(x + w, y);
        DGL_TexCoord2f(0, cw, 1);
        DGL_Vertex2f(x + w, y + h);
        DGL_TexCoord2f(0, 0, 1);
        DGL_Vertex2f(x, y + h);

        // right statue
        x = ORIGINX+282;
        y = ORIGINY;
        w = 38;
        h = 38;
        cw = (float) (ST_WIDTH - 38) / ST_WIDTH;
        ch = 0.41538461538461538461538461538462f;

        DGL_TexCoord2f(0, cw, ch);
        DGL_Vertex2f(x, y);
        DGL_TexCoord2f(0, 1, ch);
        DGL_Vertex2f(x + w, y);
        DGL_TexCoord2f(0, 1, 1);
        DGL_Vertex2f(x + w, y + h);
        DGL_TexCoord2f(0, cw, 1);
        DGL_Vertex2f(x, y + h);
        DGL_End();

        /**
         * \kludge The Hexen statusbar graphic has a chain already in the
         * image, which shows through the modified chain patches.
         * Mask out the chain on the statusbar by cutting a window out and
         * drawing a solid near-black rectangle to fill the hole.
         */
        DGL_DrawCutRectf2Tiled(ORIGINX+38, ORIGINY+31, 244, 8, 320, 65, 38, 192-134, ORIGINX+44, ORIGINY+31, 232, 7);
        DGL_Disable(DGL_TEXTURE_2D);
        DGL_SetNoMaterial();
        DGL_DrawRectf2Color(ORIGINX+44, ORIGINY+31, 232, 7, .1f, .1f, .1f, iconAlpha);
        DGL_Color4f(1, 1, 1, iconAlpha);
        //// \kludge end

        if(!Hu_InventoryIsOpen(obj->player))
        {
            DGL_Enable(DGL_TEXTURE_2D);

            // Main interface
            if(!ST_AutomapIsActive(obj->player))
            {
                patchinfo_t pStatBarInfo;
                if(R_GetPatchInfo(pStatBar, &pStatBarInfo))
                {
                    x = ORIGINX + (deathmatch ? 68 : 38);
                    y = ORIGINY;
                    w = deathmatch?214:244;
                    h = 31;
                    DGL_SetPatch(pStatBar, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
                    DGL_DrawCutRectf2Tiled(x, y, w, h, pStatBarInfo.geometry.size.width, pStatBarInfo.geometry.size.height, deathmatch?30:0, 0, ORIGINX+190, ORIGINY, 57, 30);
                }

                GL_DrawPatchXY(pWeaponSlot[pClass], ORIGINX+190, ORIGINY);
                if(deathmatch)
                    GL_DrawPatchXY(pKills, ORIGINX+38, ORIGINY);
            }
            else
            {
                GL_DrawPatchXY(pKeyBar, ORIGINX+38, ORIGINY);
            }

            DGL_Disable(DGL_TEXTURE_2D);
        }
        else
        {   // INVBAR

            DGL_SetPatch(pInventoryBar, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
            DGL_Enable(DGL_TEXTURE_2D);

            x = ORIGINX+38;
            y = ORIGINY;
            w = 244;
            h = 30;
            ch = 0.96774193548387096774193548387097f;

            DGL_Begin(DGL_QUADS);
                DGL_TexCoord2f(0, 0, 0);
                DGL_Vertex2f(x, y);
                DGL_TexCoord2f(0, 1, 0);
                DGL_Vertex2f(x + w, y);
                DGL_TexCoord2f(0, 1, ch);
                DGL_Vertex2f(x + w, y + h);
                DGL_TexCoord2f(0, 0, ch);
                DGL_Vertex2f(x, y + h);
            DGL_End();

            DGL_Disable(DGL_TEXTURE_2D);
        }
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef WIDTH
#undef HEIGHT
#undef ORIGINX
#undef ORIGINY
}

void SBarBackground_UpdateGeometry(uiwidget_t* obj)
{
    assert(obj);

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    Rect_SetWidthHeight(obj->geometry, ST_WIDTH  * cfg.statusbarScale,
                                       ST_HEIGHT * cfg.statusbarScale);
}

void SBarInventory_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = headupDisplayMode(obj->player);
    //const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);

    if(!Hu_InventoryIsOpen(obj->player)) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);

    Hu_InventoryDraw2(obj->player, -ST_WIDTH/2 + ST_INVENTORYX, -ST_HEIGHT + yOffset + ST_INVENTORYY, iconAlpha);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void SBarInventory_UpdateGeometry(uiwidget_t* obj)
{
    assert(obj);

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(!Hu_InventoryIsOpen(obj->player)) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    // \fixme calculate dimensions properly!
    Rect_SetWidthHeight(obj->geometry, (ST_WIDTH-(43*2)) * cfg.statusbarScale,
                                       41 * cfg.statusbarScale);
}

void Keys_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_keys_t* keys = (guidata_keys_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    int i;
    if(P_IsPaused() || !DD_IsSharpTick()) return;

    for(i = 0; i < NUM_KEY_TYPES; ++i)
    {
        keys->keyBoxes[i] = (plr->keys & (1 << i));
    }
}

void SBarKeys_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT*hud->showBar)

    guidata_keys_t* keys = (guidata_keys_t*)obj->typedata;
    hudstate_t* hud = &hudStates[obj->player];
    int i, numDrawn;
    int fullscreen = headupDisplayMode(obj->player);
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(obj->player) || !ST_AutomapIsActive(obj->player)) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);

    numDrawn = 0;
    for(i = 0; i < NUM_KEY_TYPES; ++i)
    {
        patchid_t patch;

        if(!keys->keyBoxes[i]) continue;

        patch = pKeySlot[i];
        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconAlpha);
        GL_DrawPatchXY(patch, ORIGINX + 46 + numDrawn * 20, ORIGINY + 1);

        DGL_Disable(DGL_TEXTURE_2D);

        ++numDrawn;
        if(numDrawn == 5) break;
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef ORIGINY
#undef ORIGINX
}

void SBarKeys_UpdateGeometry(uiwidget_t* obj)
{
    guidata_keys_t* keys = (guidata_keys_t*)obj->typedata;
    int i, numVisible, x;
    patchinfo_t pInfo;
    patchid_t patch;

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(Hu_InventoryIsOpen(obj->player) || !ST_AutomapIsActive(obj->player)) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    x = 0;
    numVisible = 0;
    for(i = 0; i < NUM_KEY_TYPES; ++i)
    {
        if(!keys->keyBoxes[i]) continue;
        patch = pKeySlot[i];
        if(!R_GetPatchInfo(patch, &pInfo)) continue;

        pInfo.geometry.origin.x = x;
        pInfo.geometry.origin.y = 0;
        Rect_UniteRaw(obj->geometry, &pInfo.geometry);

        ++numVisible;
        if(numVisible == 5) break;

        x += 20;
    }

    Rect_SetWidthHeight(obj->geometry, Rect_Width(obj->geometry)  * cfg.statusbarScale,
                                       Rect_Height(obj->geometry) * cfg.statusbarScale);
}

void ArmorIcons_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_armoricons_t* icons = (guidata_armoricons_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    int i;

    if(P_IsPaused() || !DD_IsSharpTick()) return;
    for(i = 0; i < NUMARMOR; ++i)
    {
        icons->types[i].value = plr->armorPoints[i];
    }
}

void SBarArmorIcons_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT*hud->showBar)

    guidata_armoricons_t* icons = (guidata_armoricons_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int i, pClass = cfg.playerClass[obj->player]; // Original player class (i.e. not pig).
    int fullscreen = headupDisplayMode(obj->player);
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(obj->player) || !ST_AutomapIsActive(obj->player)) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);

    for(i = 0; i < NUMARMOR; ++i)
    {
        patchid_t patch;
        float alpha;

        if(!icons->types[i].value) continue;

        patch = pArmorSlot[i];
        if(icons->types[i].value <= (PCLASS_INFO(pClass)->armorIncrement[i] >> 2))
            alpha = .3f;
        else if(icons->types[i].value <= (PCLASS_INFO(pClass)->armorIncrement[i] >> 1))
            alpha = .6f;
        else
            alpha = 1;

        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconAlpha * alpha);
        GL_DrawPatchXY(patch, ORIGINX + 150 + 31 * i, ORIGINY + 2);
        DGL_Disable(DGL_TEXTURE_2D);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef ORIGINX
#undef ORIGINY
}

void SBarArmorIcons_UpdateGeometry(uiwidget_t* obj)
{
    guidata_armoricons_t* icons = (guidata_armoricons_t*)obj->typedata;
    patchinfo_t pInfo;
    int i, x;

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(Hu_InventoryIsOpen(obj->player) || !ST_AutomapIsActive(obj->player)) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    x = 0;
    for(i = 0; i < NUMARMOR; ++i, x += 31)
    {
        if(!icons->types[i].value) continue;
        if(!R_GetPatchInfo(pArmorSlot[i], &pInfo)) continue;

        pInfo.geometry.origin.x = x;
        pInfo.geometry.origin.y = 0;
        Rect_UniteRaw(obj->geometry, &pInfo.geometry);
    }

    Rect_SetWidthHeight(obj->geometry, Rect_Width(obj->geometry)  * cfg.statusbarScale,
                                       Rect_Height(obj->geometry) * cfg.statusbarScale);
}

void Frags_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_frags_t* frags = (guidata_frags_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    int i;
    if(P_IsPaused() || !DD_IsSharpTick()) return;

    frags->value = 0;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->inGame) continue;

        frags->value += plr->frags[i] * (i != obj->player ? 1 : -1);
    }
}

void SBarFrags_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX+ST_FRAGSX)
#define Y                   (ORIGINY+ST_FRAGSY)
#define MAXDIGITS           (ST_FRAGSWIDTH)

    guidata_frags_t* frags = (guidata_frags_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = headupDisplayMode(obj->player);
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    //const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    char buf[20];

    if(!deathmatch || Hu_InventoryIsOpen(obj->player) || ST_AutomapIsActive(obj->player)) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(frags->value == 1994) return;

    dd_snprintf(buf, 20, "%i", frags->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->font);
    FR_SetTracking(0);
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    FR_DrawTextXY3(buf, X, Y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef MAXDIGITS
#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarFrags_UpdateGeometry(uiwidget_t* obj)
{
    guidata_frags_t* frags = (guidata_frags_t*)obj->typedata;
    Size2Raw textSize;
    char buf[20];

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(!deathmatch || Hu_InventoryIsOpen(obj->player) || ST_AutomapIsActive(obj->player)) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(frags->value == 1994) return;

    dd_snprintf(buf, 20, "%i", frags->value);
    FR_SetFont(obj->font);
    FR_SetTracking(0);
    FR_TextSize(&textSize, buf);
    Rect_SetWidthHeight(obj->geometry, textSize.width  * cfg.statusbarScale,
                                       textSize.height * cfg.statusbarScale);
}

void Health_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_health_t* hlth = (guidata_health_t*)obj->typedata;
    const player_t* plr = &players[obj->player];

    if(P_IsPaused() || !DD_IsSharpTick()) return;
    hlth->value = plr->health;
}

void SBarHealth_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX+ST_HEALTHX)
#define Y                   (ORIGINY+ST_HEALTHY)
#define MAXDIGITS           (ST_HEALTHWIDTH)

    guidata_health_t* hlth = (guidata_health_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    char buf[20];
    int fullscreen = headupDisplayMode(obj->player);
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    //const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);

    if(deathmatch || Hu_InventoryIsOpen(obj->player) || ST_AutomapIsActive(obj->player)) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(hlth->value == 1994) return;

    dd_snprintf(buf, 20, "%i", hlth->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->font);
    FR_SetTracking(0);
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    FR_DrawTextXY3(buf, X, Y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef MAXDIGITS
#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarHealth_UpdateGeometry(uiwidget_t* obj)
{
    guidata_health_t* hlth = (guidata_health_t*)obj->typedata;
    Size2Raw textSize;
    char buf[20];

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(deathmatch || Hu_InventoryIsOpen(obj->player) || ST_AutomapIsActive(obj->player)) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(hlth->value == 1994) return;

    dd_snprintf(buf, 20, "%i", hlth->value);
    FR_SetFont(obj->font);
    FR_SetTracking(0);
    FR_TextSize(&textSize, buf);
    Rect_SetWidthHeight(obj->geometry, textSize.width  * cfg.statusbarScale,
                                       textSize.height * cfg.statusbarScale);
}

void SBarArmor_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_armor_t* armor = (guidata_armor_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    int pClass = cfg.playerClass[obj->player]; // Original player class (i.e. not pig).
    if(P_IsPaused() || !DD_IsSharpTick()) return;

    armor->value = FixedDiv(
        PCLASS_INFO(pClass)->autoArmorSave + plr->armorPoints[ARMOR_ARMOR] +
        plr->armorPoints[ARMOR_SHIELD] +
        plr->armorPoints[ARMOR_HELMET] +
        plr->armorPoints[ARMOR_AMULET], 5 * FRACUNIT) >> FRACBITS;
}

void SBarArmor_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX+ST_ARMORX)
#define Y                   (ORIGINY+ST_ARMORY)
#define MAXDIGITS           (ST_ARMORWIDTH)

    guidata_armor_t* armor = (guidata_armor_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = headupDisplayMode(obj->player);
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    char buf[20];

    if(Hu_InventoryIsOpen(obj->player) || ST_AutomapIsActive(obj->player)) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(armor->value == 1994) return;

    dd_snprintf(buf, 20, "%i", armor->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->font);
    FR_SetTracking(0);
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    FR_DrawTextXY3(buf, X, Y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef MAXDIGITS
#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarArmor_UpdateGeometry(uiwidget_t* obj)
{
    guidata_armor_t* armor = (guidata_armor_t*)obj->typedata;
    Size2Raw textSize;
    char buf[20];

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(Hu_InventoryIsOpen(obj->player) || ST_AutomapIsActive(obj->player)) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(armor->value == 1994) return;

    dd_snprintf(buf, 20, "%i", armor->value);
    FR_SetFont(obj->font);
    FR_SetTracking(0);
    FR_TextSize(&textSize, buf);
    Rect_SetWidthHeight(obj->geometry, textSize.width  * cfg.statusbarScale,
                                       textSize.height * cfg.statusbarScale);
}

void BlueMana_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_bluemana_t* mana = (guidata_bluemana_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    if(P_IsPaused() || !DD_IsSharpTick()) return;

    mana->value = plr->ammo[AT_BLUEMANA].owned;
}

void SBarBlueMana_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX+ST_MANAAX)
#define Y                   (ORIGINY+ST_MANAAY)
#define MAXDIGITS           (ST_MANAAWIDTH)

    guidata_bluemana_t* mana = (guidata_bluemana_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = headupDisplayMode(obj->player);
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    char buf[20];

    if(mana->value <= 0 || Hu_InventoryIsOpen(obj->player) || ST_AutomapIsActive(obj->player)) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(mana->value == 1994) return;

    dd_snprintf(buf, 20, "%i", mana->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->font);
    FR_SetTracking(0);
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    FR_DrawTextXY3(buf, X, Y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef MAXDIGITS
#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarBlueMana_UpdateGeometry(uiwidget_t* obj)
{
    guidata_bluemana_t* mana = (guidata_bluemana_t*)obj->typedata;
    Size2Raw textSize;
    char buf[20];

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(mana->value <= 0 || Hu_InventoryIsOpen(obj->player) || ST_AutomapIsActive(obj->player)) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(mana->value == 1994) return;

    dd_snprintf(buf, 20, "%i", mana->value);
    FR_SetFont(obj->font);
    FR_SetTracking(0);
    FR_TextSize(&textSize, buf);
    Rect_SetWidthHeight(obj->geometry, textSize.width  * cfg.statusbarScale,
                                       textSize.height * cfg.statusbarScale);
}

void GreenMana_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_greenmana_t* mana = (guidata_greenmana_t*)obj->typedata;
    const player_t* plr = &players[obj->player];

    if(P_IsPaused() || !DD_IsSharpTick()) return;

    mana->value = plr->ammo[AT_GREENMANA].owned;
}

void SBarGreenMana_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX+ST_MANABX)
#define Y                   (ORIGINY+ST_MANABY)
#define MAXDIGITS           (ST_MANABWIDTH)

    guidata_greenmana_t* mana = (guidata_greenmana_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = headupDisplayMode(obj->player);
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    char buf[20];

    if(mana->value <= 0 || Hu_InventoryIsOpen(obj->player) || ST_AutomapIsActive(obj->player)) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(mana->value == 1994) return;

    dd_snprintf(buf, 20, "%i", mana->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->font);
    FR_SetTracking(0);
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    FR_DrawTextXY3(buf, X, Y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef MAXDIGITS
#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarGreenMana_UpdateGeometry(uiwidget_t* obj)
{
    guidata_greenmana_t* mana = (guidata_greenmana_t*)obj->typedata;
    Size2Raw textSize;
    char buf[20];

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(mana->value <= 0 || Hu_InventoryIsOpen(obj->player) || ST_AutomapIsActive(obj->player)) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(mana->value == 1994) return;

    dd_snprintf(buf, 20, "%i", mana->value);
    FR_SetFont(obj->font);
    FR_SetTracking(0);
    FR_TextSize(&textSize, buf);
    Rect_SetWidthHeight(obj->geometry, textSize.width  * cfg.statusbarScale,
                                       textSize.height * cfg.statusbarScale);
}

void ReadyItem_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_readyitem_t* item = (guidata_readyitem_t*)obj->typedata;
    const int flashCounter = hudStates[obj->player].readyItemFlashCounter;

    if(flashCounter > 0)
    {
        item->patchId = pInvItemFlash[flashCounter % 5];
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

void SBarReadyItem_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)

    guidata_readyitem_t* item = (guidata_readyitem_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    inventoryitemtype_t readyItem;
    patchinfo_t boxInfo;
    int x, y;
    int fullscreen = headupDisplayMode(obj->player);
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(obj->player) || ST_AutomapIsActive(obj->player)) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(item->patchId == 0) return;
    if(!R_GetPatchInfo(pInvItemBox, &boxInfo)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);

    if(hud->readyItemFlashCounter > 0)
    {
        x = ST_INVITEMX + 4;
        y = ST_INVITEMY;
    }
    else
    {
        x = ST_INVITEMX;
        y = ST_INVITEMY;
    }

    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(1, 1, 1, iconAlpha);
    GL_DrawPatchXY(item->patchId, ORIGINX+x, ORIGINY+y);

    readyItem = P_InventoryReadyItem(obj->player);
    if(!(hud->readyItemFlashCounter > 0) && readyItem != IIT_NONE)
    {
        uint count = P_InventoryCount(obj->player, readyItem);
        if(count > 1)
        {
            char buf[20];
            FR_SetFont(obj->font);
            FR_SetTracking(0);
            FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
            dd_snprintf(buf, 20, "%i", count);
            FR_DrawTextXY3(buf, ORIGINX+ST_INVITEMCX, ORIGINY+ST_INVITEMCY, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);
        }
    }

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef ORIGINY
#undef ORIGINX
}

void SBarReadyItem_UpdateGeometry(uiwidget_t* obj)
{
    guidata_readyitem_t* item = (guidata_readyitem_t*)obj->typedata;
    patchinfo_t boxInfo;

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(Hu_InventoryIsOpen(obj->player) || ST_AutomapIsActive(obj->player)) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(item->patchId != 0) return;
    if(!R_GetPatchInfo(pInvItemBox, &boxInfo)) return;

    Rect_SetWidthHeight(obj->geometry, boxInfo.geometry.size.width  * cfg.statusbarScale,
                                       boxInfo.geometry.size.height * cfg.statusbarScale);
}

void BlueManaIcon_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_bluemanaicon_t* icon = (guidata_bluemanaicon_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    if(P_IsPaused() || !DD_IsSharpTick()) return;

    icon->iconIdx = -1;
    if(!(plr->ammo[AT_BLUEMANA].owned > 0))
        icon->iconIdx = 0; // Draw dim Mana icon.
    // Update mana graphics based upon mana count weapon type
    if(plr->readyWeapon == WT_FIRST)
    {
        icon->iconIdx = 0;
    }
    else if(plr->readyWeapon == WT_SECOND)
    {
        // If there is mana for this weapon, make it bright!
        if(icon->iconIdx == -1)
        {
            icon->iconIdx = 1;
        }
    }
    else if(plr->readyWeapon == WT_THIRD)
    {
        icon->iconIdx = 0;
    }
    else
    {
        // If there is mana for this weapon, make it bright!
        if(icon->iconIdx == -1)
        {
            icon->iconIdx = 1;
        }
    }
}

void SBarBlueManaIcon_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX+ST_MANAAICONX)
#define Y                   (ORIGINY+ST_MANAAICONY)

    guidata_bluemanaicon_t* icon = (guidata_bluemanaicon_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = headupDisplayMode(obj->player);
    //const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(obj->player) || ST_AutomapIsActive(obj->player)) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    if(icon->iconIdx >= 0)
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        if(offset) DGL_Translatef(offset->x, offset->y, 0);
        DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
        DGL_Translatef(0, yOffset, 0);
        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconAlpha);

        GL_DrawPatchXY(pManaAIcons[icon->iconIdx], X, Y);

        DGL_Disable(DGL_TEXTURE_2D);
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }

#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarBlueManaIcon_UpdateGeometry(uiwidget_t* obj)
{
    guidata_bluemanaicon_t* icon = (guidata_bluemanaicon_t*)obj->typedata;
    patchinfo_t pInfo;

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(Hu_InventoryIsOpen(obj->player) || ST_AutomapIsActive(obj->player)) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(!R_GetPatchInfo(pManaAIcons[icon->iconIdx%2], &pInfo)) return;

    Rect_SetWidthHeight(obj->geometry, pInfo.geometry.size.width  * cfg.statusbarScale,
                                       pInfo.geometry.size.height * cfg.statusbarScale);
}

void GreenManaIcon_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_greenmanaicon_t* icon = (guidata_greenmanaicon_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    if(P_IsPaused() || !DD_IsSharpTick()) return;
    icon->iconIdx = -1;
    if(!(plr->ammo[AT_GREENMANA].owned > 0))
        icon->iconIdx = 0; // Draw dim Mana icon.

    // Update mana graphics based upon mana count weapon type
    if(plr->readyWeapon == WT_FIRST)
    {
        icon->iconIdx = 0;
    }
    else if(plr->readyWeapon == WT_SECOND)
    {
        icon->iconIdx = 0;
    }
    else if(plr->readyWeapon == WT_THIRD)
    {
        // If there is mana for this weapon, make it bright!
        if(icon->iconIdx == -1)
        {
            icon->iconIdx = 1;
        }
    }
    else
    {
        if(icon->iconIdx == -1)
        {
            icon->iconIdx = 1;
        }
    }
}

void SBarGreenManaIcon_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX+ST_MANABICONX)
#define Y                   (ORIGINY+ST_MANABICONY)

    guidata_greenmanaicon_t* icon = (guidata_greenmanaicon_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = headupDisplayMode(obj->player);
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(obj->player) || ST_AutomapIsActive(obj->player)) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    if(icon->iconIdx >= 0)
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        if(offset) DGL_Translatef(offset->x, offset->y, 0);
        DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
        DGL_Translatef(0, yOffset, 0);
        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconAlpha);

        GL_DrawPatchXY(pManaBIcons[icon->iconIdx], X, Y);

        DGL_Disable(DGL_TEXTURE_2D);
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }

#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarGreenManaIcon_UpdateGeometry(uiwidget_t* obj)
{
    guidata_greenmanaicon_t* icon = (guidata_greenmanaicon_t*)obj->typedata;
    patchinfo_t pInfo;

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(Hu_InventoryIsOpen(obj->player) || ST_AutomapIsActive(obj->player)) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(!R_GetPatchInfo(pManaBIcons[icon->iconIdx%2], &pInfo)) return;

    Rect_SetWidthHeight(obj->geometry, pInfo.geometry.size.width  * cfg.statusbarScale,
                                       pInfo.geometry.size.height * cfg.statusbarScale);
}

void BlueManaVial_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_bluemanavial_t* vial = (guidata_bluemanavial_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    if(P_IsPaused() || !DD_IsSharpTick()) return;
    vial->iconIdx = -1;
    // Update mana graphics based upon mana count weapon type
    if(plr->readyWeapon == WT_FIRST)
    {
        vial->iconIdx = 0;
    }
    else if(plr->readyWeapon == WT_SECOND)
    {
        vial->iconIdx = 1;
    }
    else if(plr->readyWeapon == WT_THIRD)
    {
        vial->iconIdx = 0;
    }
    else
    {
        vial->iconIdx = 1;
    }

    vial->filled = (float)plr->ammo[AT_BLUEMANA].owned / MAX_MANA;
}

void SBarBlueManaVial_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (ST_HEIGHT*(1-hud->showBar))
#define X                   (ORIGINX+ST_MANAAVIALX)
#define Y                   (ORIGINY+ST_MANAAVIALY)
#define VIALHEIGHT          (22)

    guidata_bluemanavial_t* vial = (guidata_bluemanavial_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int fullscreen = headupDisplayMode(obj->player);
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(obj->player) || ST_AutomapIsActive(obj->player)) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, ORIGINY, 0);

    if(vial->iconIdx >= 0)
    {
        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconAlpha);
        GL_DrawPatchXY(pManaAVials[vial->iconIdx], X, Y);
        DGL_Disable(DGL_TEXTURE_2D);
    }

    DGL_SetNoMaterial();
    DGL_DrawRectf2Color(ORIGINX+95, -ST_HEIGHT+3, 3, (int) (VIALHEIGHT * (1-vial->filled) + .5f), 0, 0, 0, iconAlpha);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef VIALHEIGHT
#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarBlueManaVial_UpdateGeometry(uiwidget_t* obj)
{
    guidata_bluemanavial_t* vial = (guidata_bluemanavial_t*)obj->typedata;
    patchinfo_t pInfo;

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(Hu_InventoryIsOpen(obj->player) || ST_AutomapIsActive(obj->player)) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(!R_GetPatchInfo(pManaAVials[vial->iconIdx%2], &pInfo)) return;

    Rect_SetWidthHeight(obj->geometry, pInfo.geometry.size.width  * cfg.statusbarScale,
                                       pInfo.geometry.size.height * cfg.statusbarScale);
}

void GreenManaVial_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_greenmanavial_t* vial = (guidata_greenmanavial_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    if(P_IsPaused() || !DD_IsSharpTick()) return;

    vial->iconIdx = -1;
    // Update mana graphics based upon mana count weapon type
    if(plr->readyWeapon == WT_FIRST)
    {
        vial->iconIdx = 0;
    }
    else if(plr->readyWeapon == WT_SECOND)
    {
        vial->iconIdx = 0;
    }
    else if(plr->readyWeapon == WT_THIRD)
    {
        vial->iconIdx = 1;
    }
    else
    {
        vial->iconIdx = 1;
    }

    vial->filled = (float)plr->ammo[AT_GREENMANA].owned / MAX_MANA;
}

void SBarGreenManaVial_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (ST_HEIGHT*(1-hud->showBar))
#define X                   (ORIGINX+ST_MANABVIALX)
#define Y                   (ORIGINY+ST_MANABVIALY)
#define VIALHEIGHT          (22)

    guidata_greenmanavial_t* vial = (guidata_greenmanavial_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int fullscreen = headupDisplayMode(obj->player);
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(obj->player) || ST_AutomapIsActive(obj->player)) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, ORIGINY, 0);

    if(vial->iconIdx >= 0)
    {
        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconAlpha);
        GL_DrawPatchXY(pManaBVials[vial->iconIdx], X, Y);
        DGL_Disable(DGL_TEXTURE_2D);
    }

    DGL_SetNoMaterial();
    DGL_DrawRectf2Color(ORIGINX+103, -ST_HEIGHT+3, 3, (int) (VIALHEIGHT * (1-vial->filled) + .5f), 0, 0, 0, iconAlpha);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef VIALHEIGHT
#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarGreenManaVial_UpdateGeometry(uiwidget_t* obj)
{
    guidata_greenmanavial_t* vial = (guidata_greenmanavial_t*)obj->typedata;
    patchinfo_t pInfo;

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(Hu_InventoryIsOpen(obj->player) || ST_AutomapIsActive(obj->player)) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(!R_GetPatchInfo(pManaBVials[vial->iconIdx%2], &pInfo)) return;

    Rect_SetWidthHeight(obj->geometry, pInfo.geometry.size.width  * cfg.statusbarScale,
                                       pInfo.geometry.size.height * cfg.statusbarScale);
}

void ST_HUDUnHide(int player, hueevent_t ev)
{
    player_t* plr;

    if(player < 0 || player >= MAXPLAYERS)
    {
#if _DEBUG
        Con_Message("Warning:ST_HUDUnHide: Invalid player #%i, ignoring.", player);
#endif
        return;
    }
    if(ev < HUE_FORCE || ev > NUMHUDUNHIDEEVENTS)
    {
#if _DEBUG
        Con_Message("Warning:ST_HUDUnHide: Invalid event type %i, ignoring.", (int) ev);
#endif
        return;
    }

    plr = &players[player];
    if(!plr->plr->inGame) return;

    if(ev == HUE_FORCE || cfg.hudUnHide[ev])
    {
        hudStates[player].hideTics = (cfg.hudTimer * TICSPERSEC);
        hudStates[player].hideAmount = 0;
    }
}

void Health_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
#define TRACKING                (1)

    guidata_health_t* hlth = (guidata_health_t*)obj->typedata;
    int value = MAX_OF(hlth->value, 0);
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    char buf[20];

    if(!cfg.hudShown[HUD_HEALTH]) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(hlth->value == 1994) return;

    dd_snprintf(buf, 20, "%i", value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->font);
    FR_SetTracking(TRACKING);
    FR_SetColorAndAlpha(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    FR_DrawTextXY(buf, -1, -1);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef TRACKING
}

void Health_UpdateGeometry(uiwidget_t* obj)
{
#define TRACKING                (1)

    guidata_health_t* hlth = (guidata_health_t*)obj->typedata;
    int value = MAX_OF(hlth->value, 0);
    Size2Raw textSize;
    char buf[20];

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(!cfg.hudShown[HUD_HEALTH]) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(hlth->value == 1994) return;

    dd_snprintf(buf, 20, "%i", value);
    FR_SetFont(obj->font);
    FR_SetTracking(TRACKING);
    FR_TextSize(&textSize, buf);
    Rect_SetWidthHeight(obj->geometry, textSize.width  * cfg.hudScale,
                                       textSize.height * cfg.hudScale);

#undef TRACKING
}

void BlueManaIcon_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
    guidata_bluemanaicon_t* icon = (guidata_bluemanaicon_t*)obj->typedata;
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;

    if(!cfg.hudShown[HUD_MANA]) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    if(icon->iconIdx >= 0)
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        if(offset) DGL_Translatef(offset->x, offset->y, 0);
        DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconAlpha);

        GL_DrawPatchXY(pManaAIcons[icon->iconIdx], 0, 0);

        DGL_Disable(DGL_TEXTURE_2D);
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }
}

void BlueManaIcon_UpdateGeometry(uiwidget_t* obj)
{
    guidata_bluemanaicon_t* icon = (guidata_bluemanaicon_t*)obj->typedata;
    patchinfo_t pInfo;

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(!cfg.hudShown[HUD_MANA]) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(!R_GetPatchInfo(pManaAIcons[icon->iconIdx%2], &pInfo)) return;

    Rect_SetWidthHeight(obj->geometry, pInfo.geometry.size.width  * cfg.hudScale,
                                       pInfo.geometry.size.height * cfg.hudScale);
}

void BlueMana_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
#define TRACKING                (1)

    guidata_bluemana_t* mana = (guidata_bluemana_t*)obj->typedata;
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    char buf[20];

    if(!cfg.hudShown[HUD_MANA]) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(mana->value == 1994) return;

    dd_snprintf(buf, 20, "%i", mana->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->font);
    FR_SetTracking(TRACKING);
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    FR_DrawTextXY3(buf, 0, 0, ALIGN_TOPLEFT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef TRACKING
}

void BlueMana_UpdateGeometry(uiwidget_t* obj)
{
#define TRACKING                (1)

    guidata_bluemana_t* mana = (guidata_bluemana_t*)obj->typedata;
    Size2Raw textSize;
    char buf[20];

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(!cfg.hudShown[HUD_MANA]) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(mana->value == 1994) return;

    dd_snprintf(buf, 20, "%i", mana->value);
    FR_SetFont(obj->font);
    FR_SetTracking(TRACKING);
    FR_TextSize(&textSize, buf);
    Rect_SetWidthHeight(obj->geometry, textSize.width  * cfg.hudScale,
                                       textSize.height * cfg.hudScale);

#undef TRACKING
}

void GreenManaIcon_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
    guidata_greenmanaicon_t* icon = (guidata_greenmanaicon_t*)obj->typedata;
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;

    if(!cfg.hudShown[HUD_MANA]) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    if(icon->iconIdx >= 0)
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        if(offset) DGL_Translatef(offset->x, offset->y, 0);
        DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconAlpha);

        GL_DrawPatchXY(pManaBIcons[icon->iconIdx], 0, 0);

        DGL_Disable(DGL_TEXTURE_2D);
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }
}

void GreenManaIcon_UpdateGeometry(uiwidget_t* obj)
{
    guidata_greenmanaicon_t* icon = (guidata_greenmanaicon_t*)obj->typedata;
    patchinfo_t pInfo;

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(!cfg.hudShown[HUD_MANA]) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(!R_GetPatchInfo(pManaBIcons[icon->iconIdx%2], &pInfo)) return;

    Rect_SetWidthHeight(obj->geometry, pInfo.geometry.size.width  * cfg.hudScale,
                                       pInfo.geometry.size.height * cfg.hudScale);
}

void GreenMana_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
#define TRACKING                (1)

    guidata_greenmana_t* mana = (guidata_greenmana_t*)obj->typedata;
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    char buf[20];

    if(!cfg.hudShown[HUD_MANA]) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(mana->value == 1994) return;

    dd_snprintf(buf, 20, "%i", mana->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->font);
    FR_SetTracking(TRACKING);
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    FR_DrawTextXY3(buf, 0, 0, ALIGN_TOPLEFT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef TRACKING
}

void GreenMana_UpdateGeometry(uiwidget_t* obj)
{
#define TRACKING                (1)

    guidata_greenmana_t* mana = (guidata_greenmana_t*)obj->typedata;
    Size2Raw textSize;
    char buf[20];

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(!cfg.hudShown[HUD_MANA]) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(mana->value == 1994) return;

    dd_snprintf(buf, 20, "%i", mana->value);

    FR_SetFont(obj->font);
    FR_SetTracking(TRACKING);
    FR_TextSize(&textSize, buf);
    Rect_SetWidthHeight(obj->geometry, textSize.width  * cfg.hudScale,
                                       textSize.height * cfg.hudScale);

#undef TRACKING
}

void Frags_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
#define TRACKING                (1)

    guidata_frags_t* frags = (guidata_frags_t*)obj->typedata;
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    char buf[20];

    if(!deathmatch) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(frags->value == 1994) return;

    dd_snprintf(buf, 20, "%i", frags->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->font);
    FR_SetTracking(TRACKING);
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    FR_DrawTextXY3(buf, 0, -13, ALIGN_TOPLEFT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef TRACKING
}

void Frags_UpdateGeometry(uiwidget_t* obj)
{
#define TRACKING                (1)

    guidata_frags_t* frags = (guidata_frags_t*)obj->typedata;
    Size2Raw textSize;
    char buf[20];

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(!deathmatch) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(frags->value == 1994) return;

    dd_snprintf(buf, 20, "%i", frags->value);
    FR_SetFont(obj->font);
    FR_SetTracking(TRACKING);
    FR_TextSize(&textSize, buf);
    Rect_SetWidthHeight(obj->geometry, textSize.width  * cfg.hudScale,
                                       textSize.height * cfg.hudScale);

#undef TRACKING
}

void ReadyItem_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
    guidata_readyitem_t* item = (guidata_readyitem_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;
    inventoryitemtype_t readyItem;
    int xOffset, yOffset;
    patchinfo_t boxInfo;

    if(!cfg.hudShown[HUD_READYITEM]) return;
    if(Hu_InventoryIsOpen(obj->player)) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(item->patchId == 0) return;
    if(!R_GetPatchInfo(pInvItemBox, &boxInfo)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(1, 1, 1, iconAlpha/2);
    GL_DrawPatchXY(pInvItemBox, 0, 0);

    if(hud->readyItemFlashCounter > 0)
    {
        xOffset = 3;
        yOffset = 0;
    }
    else
    {
        xOffset = -2;
        yOffset = -1;
    }

    DGL_Color4f(1, 1, 1, iconAlpha);
    GL_DrawPatchXY(item->patchId, xOffset, yOffset);

    readyItem = P_InventoryReadyItem(obj->player);
    if(hud->readyItemFlashCounter == 0 && readyItem != IIT_NONE)
    {
        uint count = P_InventoryCount(obj->player, readyItem);
        if(count > 1)
        {
            char buf[20];
            FR_SetFont(obj->font);
            FR_SetTracking(0);
            FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
            dd_snprintf(buf, 20, "%i", count);
            FR_DrawTextXY2(buf, boxInfo.geometry.size.width-1, boxInfo.geometry.size.height-3, ALIGN_BOTTOMRIGHT);
        }
    }

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void ReadyItem_UpdateGeometry(uiwidget_t* obj)
{
    patchinfo_t boxInfo;
    assert(obj);

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(!cfg.hudShown[HUD_READYITEM]) return;
    if(Hu_InventoryIsOpen(obj->player)) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(!R_GetPatchInfo(pInvItemBox, &boxInfo)) return;

    Rect_SetWidthHeight(obj->geometry, boxInfo.geometry.size.width  * cfg.hudScale,
                                       boxInfo.geometry.size.height * cfg.hudScale);
}

void Inventory_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
#define INVENTORY_HEIGHT    29
#define EXTRA_SCALE         .75f

    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;
    assert(obj);

    if(!Hu_InventoryIsOpen(obj->player)) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(EXTRA_SCALE * cfg.hudScale, EXTRA_SCALE * cfg.hudScale, 1);

    Hu_InventoryDraw(obj->player, 0, -INVENTORY_HEIGHT, textAlpha, iconAlpha);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef EXTRA_SCALE
#undef INVENTORY_HEIGHT
}

void Inventory_UpdateGeometry(uiwidget_t* obj)
{
#define INVENTORY_HEIGHT    29
#define EXTRA_SCALE         .75f

    assert(obj);

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(!Hu_InventoryIsOpen(obj->player)) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    Rect_SetWidthHeight(obj->geometry, (31 * 7 + 16 * 2) * EXTRA_SCALE * cfg.hudScale,
                                       INVENTORY_HEIGHT  * EXTRA_SCALE * cfg.hudScale);

#undef EXTRA_SCALE
#undef INVENTORY_HEIGHT
}

void WorldTimer_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_worldtimer_t* time = (guidata_worldtimer_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    int worldTime = plr->worldTimer / TICRATE;
    if(P_IsPaused() || !DD_IsSharpTick()) return;

    time->days    = worldTime / 86400; worldTime -= time->days * 86400;
    time->hours   = worldTime / 3600;  worldTime -= time->hours * 3600;
    time->minutes = worldTime / 60;    worldTime -= time->minutes * 60;
    time->seconds = worldTime;
}

void WorldTimer_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
#define ORIGINX         (0)
#define ORIGINY         (0)
#define LEADING         (.5)

    guidata_worldtimer_t* time = (guidata_worldtimer_t*)obj->typedata;
    int counterWidth, spacerWidth, lineHeight, x, y;
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    char buf[20];

    if(!ST_AutomapIsActive(obj->player)) return;

    FR_SetFont(obj->font);
    FR_SetTracking(0);
    FR_SetColorAndAlpha(1, 1, 1, textAlpha);
    counterWidth = FR_TextWidth("00");
    lineHeight   = FR_TextHeight("00");
    spacerWidth  = FR_TextWidth(" : ");

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);

    DGL_Enable(DGL_TEXTURE_2D);

    x = ORIGINX - counterWidth;
    y = ORIGINY;
    dd_snprintf(buf, 20, "%.2d", time->seconds);
    FR_DrawTextXY(buf, x, y);
    x -= spacerWidth;

    FR_DrawCharXY2(':', x + spacerWidth/2, y, ALIGN_TOP);
    x -= counterWidth;

    dd_snprintf(buf, 20, "%.2d", time->minutes);
    FR_DrawTextXY(buf, x, y);
    x -= spacerWidth;

    FR_DrawCharXY2(':', x + spacerWidth/2, y, ALIGN_TOP);
    x -= counterWidth;

    dd_snprintf(buf, 20, "%.2d", time->hours);
    FR_DrawTextXY(buf, x, y);
    y += lineHeight;

    if(time->days)
    {
        y += lineHeight * LEADING;
        dd_snprintf(buf, 20, "%.2d %s", time->days, time->days == 1? "day" : "days");
        FR_DrawTextXY(buf, ORIGINX, y);
        y += lineHeight;

        if(time->days >= 5)
        {
            y += lineHeight * LEADING;
            strncpy(buf, "You Freak!!!", 20);
            FR_DrawTextXY(buf, ORIGINX, y);
            x = -MAX_OF(abs(x), FR_TextWidth(buf));
            y += lineHeight;
        }
    }

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef LEADING
#undef ORIGINY
#undef ORIGINX
}

void WorldTimer_UpdateGeometry(uiwidget_t* obj)
{
#define ORIGINX         (0)
#define ORIGINY         (0)
#define LEADING         (.5)

    guidata_worldtimer_t* time = (guidata_worldtimer_t*)obj->typedata;
    int counterWidth, spacerWidth, lineHeight, x, y;
    char buf[20];

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(!ST_AutomapIsActive(obj->player)) return;

    FR_SetFont(obj->font);
    FR_SetTracking(0);
    counterWidth = FR_TextWidth("00");
    lineHeight   = FR_TextHeight("00");
    spacerWidth  = FR_TextWidth(" : ");

    x = ORIGINX;
    y = ORIGINY;
    dd_snprintf(buf, 20, "%.2d", time->seconds);
    x -= counterWidth + spacerWidth;

    dd_snprintf(buf, 20, "%.2d", time->minutes);
    x -= counterWidth + spacerWidth;

    dd_snprintf(buf, 20, "%.2d", time->hours);
    x -= counterWidth;
    y += lineHeight;

    if(0 != time->days)
    {
        y += lineHeight * LEADING;
        dd_snprintf(buf, 20, "%.2d %s", time->days, time->days == 1? "day" : "days");
        y += lineHeight;

        if(time->days >= 5)
        {
            y += lineHeight * LEADING;
            strncpy(buf, "You Freak!!!", 20);
            x = -MAX_OF(abs(x), FR_TextWidth(buf));
            y += lineHeight;
        }
    }

    Rect_SetWidthHeight(obj->geometry, (x - ORIGINX) * cfg.hudScale,
                                       (y - ORIGINY) * cfg.hudScale);

#undef DRAWFLAGS
#undef LEADING
#undef ORIGINY
#undef ORIGINX
}

void MapName_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
    const float scale = .75f;
    const float textAlpha = uiRendState->pageAlpha;
    const char* text = P_GetMapNiceName();
    assert(obj->type == GUI_MAPNAME);

    if(!text) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(scale, scale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->font);
    FR_SetTracking(0);
    FR_SetColorAndAlpha(defFontRGB3[0], defFontRGB3[1], defFontRGB3[2], textAlpha);
    FR_DrawTextXY(text, 0, 0);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void MapName_UpdateGeometry(uiwidget_t* obj)
{
    const char* text = P_GetMapNiceName();
    const float scale = .75f;
    Size2Raw textSize;
    assert(obj->type == GUI_MAPNAME);

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(!text) return;

    FR_SetFont(obj->font);
    FR_SetTracking(0);
    FR_TextSize(&textSize, text);
    Rect_SetWidthHeight(obj->geometry, textSize.width * scale, textSize.height * scale);
}

void ST_loadGraphics(void)
{
    char namebuf[9];
    int i;

    pStatusBar = R_DeclarePatch("H2BAR");
    pStatusBarTop = R_DeclarePatch("H2TOP");
    pInventoryBar = R_DeclarePatch("INVBAR");
    pStatBar = R_DeclarePatch("STATBAR");
    pKeyBar = R_DeclarePatch("KEYBAR");

    pManaAVials[0] = R_DeclarePatch("MANAVL1D");
    pManaBVials[0] = R_DeclarePatch("MANAVL2D");
    pManaAVials[1] = R_DeclarePatch("MANAVL1");
    pManaBVials[1] = R_DeclarePatch("MANAVL2");

    pManaAIcons[0] = R_DeclarePatch("MANADIM1");
    pManaBIcons[0] = R_DeclarePatch("MANADIM2");
    pManaAIcons[1] = R_DeclarePatch("MANABRT1");
    pManaBIcons[1] = R_DeclarePatch("MANABRT2");

    pKills = R_DeclarePatch("KILLS");

    for(i = 0; i < NUM_KEY_TYPES; ++i)
    {
        sprintf(namebuf, "KEYSLOT%X", i + 1);
        pKeySlot[i] = R_DeclarePatch(namebuf);
    }

    for(i = 0; i < NUMARMOR; ++i)
    {
        sprintf(namebuf, "ARMSLOT%d", i + 1);
        pArmorSlot[i] = R_DeclarePatch(namebuf);
    }

    for(i = 0; i < 16; ++i)
    {
        sprintf(namebuf, "SPFLY%d", i);
        pSpinFly[i] = R_DeclarePatch(namebuf);

        sprintf(namebuf, "SPMINO%d", i);
        pSpinMinotaur[i] = R_DeclarePatch(namebuf);

        sprintf(namebuf, "SPBOOT%d", i);
        pSpinSpeed[i] = R_DeclarePatch(namebuf);

        sprintf(namebuf, "SPSHLD%d", i);
        pSpinDefense[i] = R_DeclarePatch(namebuf);
    }

    // Fighter:
    pWeaponPiece1[PCLASS_FIGHTER] = R_DeclarePatch("WPIECEF1");
    pWeaponPiece2[PCLASS_FIGHTER] = R_DeclarePatch("WPIECEF2");
    pWeaponPiece3[PCLASS_FIGHTER] = R_DeclarePatch("WPIECEF3");
    pChain[PCLASS_FIGHTER] = R_DeclarePatch("CHAIN");
    pWeaponSlot[PCLASS_FIGHTER] = R_DeclarePatch("WPSLOT0");
    pWeaponFull[PCLASS_FIGHTER] = R_DeclarePatch("WPFULL0");
    pLifeGem[PCLASS_FIGHTER][0] = R_DeclarePatch("LIFEGEM");
    for(i = 1; i < 8; ++i)
    {
        sprintf(namebuf, "LIFEGMF%d", i + 1);
        pLifeGem[PCLASS_FIGHTER][i] = R_DeclarePatch(namebuf);
    }

    // Cleric:
    pWeaponPiece1[PCLASS_CLERIC] = R_DeclarePatch("WPIECEC1");
    pWeaponPiece2[PCLASS_CLERIC] = R_DeclarePatch("WPIECEC2");
    pWeaponPiece3[PCLASS_CLERIC] = R_DeclarePatch("WPIECEC3");
    pChain[PCLASS_CLERIC] = R_DeclarePatch("CHAIN2");
    pWeaponSlot[PCLASS_CLERIC] = R_DeclarePatch("WPSLOT1");
    pWeaponFull[PCLASS_CLERIC] = R_DeclarePatch("WPFULL1");
    for(i = 0; i < 8; ++i)
    {
        sprintf(namebuf, "LIFEGMC%d", i + 1);
        pLifeGem[PCLASS_CLERIC][i] = R_DeclarePatch(namebuf);
    }

    // Mage:
    pWeaponPiece1[PCLASS_MAGE] = R_DeclarePatch("WPIECEM1");
    pWeaponPiece2[PCLASS_MAGE] = R_DeclarePatch("WPIECEM2");
    pWeaponPiece3[PCLASS_MAGE] = R_DeclarePatch("WPIECEM3");
    pChain[PCLASS_MAGE] = R_DeclarePatch("CHAIN3");
    pWeaponSlot[PCLASS_MAGE] = R_DeclarePatch("WPSLOT2");
    pWeaponFull[PCLASS_MAGE] = R_DeclarePatch("WPFULL2");
    for(i = 0; i < 8; ++i)
    {
        sprintf(namebuf, "LIFEGMM%d", i + 1);
        pLifeGem[PCLASS_MAGE][i] = R_DeclarePatch(namebuf);
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
        pInvItemFlash[i] = R_DeclarePatch(invItemFlashAnim[i]);
    }
    }
}

void ST_loadData(void)
{
    ST_loadGraphics();
}

static void initData(hudstate_t* hud)
{
    int i, player = hud - hudStates;

    hud->statusbarActive = true;
    hud->stopped = true;
    hud->showBar = 1;
    hud->readyItemFlashCounter = 0;

    // Statusbar:
    hud->sbarHealth.value = 1994;
    hud->sbarWeaponpieces.pieces = 0;
    hud->sbarFrags.value = 1994;
    hud->sbarArmor.value = 1994;
    hud->sbarChain.healthMarker = 0;
    hud->sbarChain.wiggle = 0;
    hud->sbarBluemanaicon.iconIdx = -1;
    hud->sbarBluemana.value = 1994;
    hud->sbarBluemanavial.iconIdx = -1;
    hud->sbarBluemanavial.filled = 0;
    hud->sbarGreenmanaicon.iconIdx = -1;
    hud->sbarGreenmana.value = 1994;
    hud->sbarGreenmanavial.iconIdx = -1;
    hud->sbarGreenmanavial.filled = 0;
    hud->sbarReadyitem.patchId = 0;
    for(i = 0; i < NUM_KEY_TYPES; ++i)
    {
        hud->sbarKeys.keyBoxes[i] = false;
    }
    for(i = (int)ARMOR_FIRST; i < (int)NUMARMOR; ++i)
    {
        hud->sbarArmoricons.types[i].value = 0;
    }

    // Fullscreen:
    hud->health.value = 1994;
    hud->frags.value = 1994;
    hud->bluemanaicon.iconIdx = -1;
    hud->bluemana.value = 1994;
    hud->greenmanaicon.iconIdx = -1;
    hud->greenmana.value = 1994;
    hud->readyitem.patchId = 0;

    // Other:
    hud->flight.patchId = 0;
    hud->flight.hitCenterFrame = false;
    hud->boots.patchId = 0;
    hud->servant.patchId = 0;
    hud->defense.patchId = 0;
    hud->worldtimer.days = hud->worldtimer.hours = hud->worldtimer.minutes = hud->worldtimer.seconds = 0;

    hud->log._msgCount = 0;
    hud->log._nextUsedMsg = 0;
    hud->log._pvisMsgCount = 0;
    memset(hud->log._msgs, 0, sizeof(hud->log._msgs));

    ST_HUDUnHide(player, HUE_FORCE);
}

static void setAutomapCheatLevel(uiwidget_t* obj, int level)
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

static void initAutomapForCurrentMap(uiwidget_t* obj)
{
    hudstate_t* hud = &hudStates[UIWidget_Player(obj)];
    automapcfg_t* mcfg;
    mobj_t* followMobj;
    uint i;

    UIAutomap_Reset(obj);

    UIAutomap_SetMinScale(obj, 2 * PLAYERRADIUS);
    UIAutomap_SetWorldBounds(obj, *((coord_t*) DD_GetVariable(DD_MAP_MIN_X)),
                                  *((coord_t*) DD_GetVariable(DD_MAP_MAX_X)),
                                  *((coord_t*) DD_GetVariable(DD_MAP_MIN_Y)),
                                  *((coord_t*) DD_GetVariable(DD_MAP_MAX_Y)));

    mcfg = UIAutomap_Config(obj);

    // Determine the obj view scale factors.
    if(UIAutomap_ZoomMax(obj))
        UIAutomap_SetScale(obj, 0);

    UIAutomap_ClearPoints(obj);

#if !__JHEXEN__
    if(gameSkill == SM_BABY && cfg.automapBabyKeys)
    {
        int flags = UIAutomap_Flags(obj);
        UIAutomap_SetFlags(obj, flags|AMF_REND_KEYS);
    }
#endif

#if __JDOOM__
    if(!IS_NETGAME && hud->automapCheatLevel)
        AM_SetVectorGraphic(mcfg, AMO_THINGPLAYER, VG_CHEATARROW);
#endif

    // Are we re-centering on a followed mobj?
    followMobj = UIAutomap_FollowMobj(obj);
    if(followMobj)
    {
        UIAutomap_SetCameraOrigin(obj, followMobj->origin[VX], followMobj->origin[VY]);
    }

    if(IS_NETGAME)
    {
        setAutomapCheatLevel(obj, 0);
    }

    UIAutomap_SetReveal(obj, false);

    // Add all immediately visible lines.
    for(i = 0; i < numlines; ++i)
    {
        xline_t* xline = &xlines[i];
        if(!(xline->flags & ML_MAPPED)) continue;

        P_SetLinedefAutomapVisibility(UIWidget_Player(obj), i, true);
    }
}

void ST_Start(int player)
{
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
    {
        ST_Stop(player);
    }

    initData(hud);

    /**
     * Initialize widgets according to player preferences.
     */

    obj = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPCENTER]);
    flags = UIWidget_Alignment(obj);
    flags &= ~(ALIGN_LEFT|ALIGN_RIGHT);
    if(cfg.msgAlign == 0)
        flags |= ALIGN_LEFT;
    else if(cfg.msgAlign == 2)
        flags |= ALIGN_RIGHT;
    UIWidget_SetAlignment(obj, flags);

    obj = GUI_MustFindObjectById(hud->automapWidgetId);
    // If the automap was left open; close it.
    UIAutomap_Open(obj, false, true);
    initAutomapForCurrentMap(obj);
    UIAutomap_SetCameraRotation(obj, cfg.automapRotate);

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
#define PADDING             (2) /// Units in fixed 320x200 screen space.

typedef struct {
    int group;
    int alignFlags;
    order_t order;
    int groupFlags;
    int padding; // In fixed 320x200 pixels.
} uiwidgetgroupdef_t;

typedef struct {
    guiwidgettype_t type;
    int alignFlags;
    int group;
    gamefontid_t fontIdx;
    void (*updateGeometry) (uiwidget_t* obj);
    void (*drawer) (uiwidget_t* obj, const Point2Raw* offset);
    void (*ticker) (uiwidget_t* obj, timespan_t ticLength);
    void* typedata;
} uiwidgetdef_t;

    hudstate_t* hud = hudStates + player;
    const uiwidgetgroupdef_t widgetGroupDefs[] = {
        { UWG_STATUSBAR,    ALIGN_BOTTOM },
        { UWG_MAPNAME,      ALIGN_BOTTOMLEFT },
        { UWG_BOTTOMLEFT,   ALIGN_BOTTOMLEFT,  ORDER_LEFTTORIGHT, 0, PADDING },
        { UWG_BOTTOMRIGHT,  ALIGN_BOTTOMRIGHT, ORDER_RIGHTTOLEFT, 0, PADDING },
        { UWG_BOTTOMCENTER, ALIGN_BOTTOM,      ORDER_RIGHTTOLEFT, UWGF_VERTICAL, PADDING },
        { UWG_BOTTOM,       ALIGN_BOTTOM,      ORDER_LEFTTORIGHT },
        { UWG_TOP,          ALIGN_TOPLEFT,     ORDER_LEFTTORIGHT },
        { UWG_TOPCENTER,    ALIGN_TOP,         ORDER_LEFTTORIGHT, UWGF_VERTICAL, PADDING },
        { UWG_TOPLEFT,      ALIGN_TOPLEFT,     ORDER_LEFTTORIGHT, 0, PADDING },
        { UWG_TOPLEFT2,     ALIGN_TOPLEFT,     ORDER_LEFTTORIGHT, 0, PADDING },
        { UWG_TOPLEFT3,     ALIGN_TOPLEFT,     ORDER_LEFTTORIGHT, 0, PADDING },
        { UWG_TOPRIGHT,     ALIGN_TOPRIGHT,    ORDER_RIGHTTOLEFT, 0, PADDING },
        { UWG_AUTOMAP,      ALIGN_TOPLEFT }
    };
    const uiwidgetdef_t widgetDefs[] = {
        { GUI_BOX,          ALIGN_TOPLEFT,    UWG_STATUSBAR,    0,            SBarBackground_UpdateGeometry, SBarBackground_Drawer },
        { GUI_WEAPONPIECES, ALIGN_TOPLEFT,    UWG_STATUSBAR,    0,            SBarWeaponPieces_UpdateGeometry, SBarWeaponPieces_Drawer, WeaponPieces_Ticker, &hud->sbarWeaponpieces },
        { GUI_CHAIN,        ALIGN_TOPLEFT,    UWG_STATUSBAR,    0,            SBarChain_UpdateGeometry, SBarChain_Drawer, SBarChain_Ticker, &hud->sbarChain },
        { GUI_INVENTORY,    ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_SMALLIN,   SBarInventory_UpdateGeometry, SBarInventory_Drawer },
        { GUI_KEYS,         ALIGN_TOPLEFT,    UWG_STATUSBAR,    0,            SBarKeys_UpdateGeometry, SBarKeys_Drawer, Keys_Ticker, &hud->sbarKeys },
        { GUI_ARMORICONS,   ALIGN_TOPLEFT,    UWG_STATUSBAR,    0,            SBarArmorIcons_UpdateGeometry, SBarArmorIcons_Drawer, ArmorIcons_Ticker, &hud->sbarArmoricons },
        { GUI_FRAGS,        ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_STATUS,    SBarFrags_UpdateGeometry, SBarFrags_Drawer, Frags_Ticker, &hud->sbarFrags },
        { GUI_HEALTH,       ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_STATUS,    SBarHealth_UpdateGeometry, SBarHealth_Drawer, Health_Ticker, &hud->sbarHealth },
        { GUI_ARMOR,        ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_STATUS,    SBarArmor_UpdateGeometry, SBarArmor_Drawer, SBarArmor_Ticker, &hud->sbarArmor },
        { GUI_READYITEM,    ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_SMALLIN,   SBarReadyItem_UpdateGeometry, SBarReadyItem_Drawer, ReadyItem_Ticker, &hud->sbarReadyitem },
        { GUI_BLUEMANAICON, ALIGN_TOPLEFT,    UWG_STATUSBAR,    0,            SBarBlueManaIcon_UpdateGeometry, SBarBlueManaIcon_Drawer, BlueManaIcon_Ticker, &hud->sbarBluemanaicon },
        { GUI_BLUEMANA,     ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_SMALLIN,   SBarBlueMana_UpdateGeometry, SBarBlueMana_Drawer, BlueMana_Ticker, &hud->sbarBluemana },
        { GUI_BLUEMANAVIAL, ALIGN_TOPLEFT,    UWG_STATUSBAR,    0,            SBarBlueManaVial_UpdateGeometry, SBarBlueManaVial_Drawer, BlueManaVial_Ticker, &hud->sbarBluemanavial },
        { GUI_GREENMANAICON, ALIGN_TOPLEFT,   UWG_STATUSBAR,    0,            SBarGreenManaIcon_UpdateGeometry, SBarGreenManaIcon_Drawer, GreenManaIcon_Ticker, &hud->sbarGreenmanaicon },
        { GUI_GREENMANA,    ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_SMALLIN,   SBarGreenMana_UpdateGeometry, SBarGreenMana_Drawer, GreenMana_Ticker, &hud->sbarGreenmana },
        { GUI_GREENMANAVIAL, ALIGN_TOPLEFT,   UWG_STATUSBAR,    0,            SBarGreenManaVial_UpdateGeometry, SBarGreenManaVial_Drawer, GreenManaVial_Ticker, &hud->sbarGreenmanavial },
        { GUI_MAPNAME,      ALIGN_TOPLEFT,    UWG_MAPNAME,      GF_FONTA,     MapName_UpdateGeometry, MapName_Drawer },
        { GUI_BLUEMANAICON, ALIGN_TOPLEFT,    UWG_TOPLEFT,      0,            BlueManaIcon_UpdateGeometry, BlueManaIcon_Drawer, BlueManaIcon_Ticker, &hud->bluemanaicon },
        { GUI_BLUEMANA,     ALIGN_TOPLEFT,    UWG_TOPLEFT,      GF_STATUS,    BlueMana_UpdateGeometry, BlueMana_Drawer, BlueMana_Ticker, &hud->bluemana },
        { GUI_GREENMANAICON, ALIGN_TOPLEFT,   UWG_TOPLEFT2,     0,            GreenManaIcon_UpdateGeometry, GreenManaIcon_Drawer, GreenManaIcon_Ticker, &hud->greenmanaicon },
        { GUI_GREENMANA,    ALIGN_TOPLEFT,    UWG_TOPLEFT2,     GF_STATUS,    GreenMana_UpdateGeometry, GreenMana_Drawer, GreenMana_Ticker, &hud->greenmana },
        { GUI_FLIGHT,       ALIGN_TOPLEFT,    UWG_TOPLEFT3,     0,            Flight_UpdateGeometry, Flight_Drawer, Flight_Ticker, &hud->flight },
        { GUI_BOOTS,        ALIGN_TOPLEFT,    UWG_TOPLEFT3,     0,            Boots_UpdateGeometry, Boots_Drawer, Boots_Ticker, &hud->boots },
        { GUI_SERVANT,      ALIGN_TOPRIGHT,   UWG_TOPRIGHT,     0,            Servant_UpdateGeometry, Servant_Drawer, Servant_Ticker, &hud->servant },
        { GUI_DEFENSE,      ALIGN_TOPRIGHT,   UWG_TOPRIGHT,     0,            Defense_UpdateGeometry, Defense_Drawer, Defense_Ticker, &hud->defense },
        { GUI_WORLDTIMER,   ALIGN_TOPRIGHT,   UWG_TOPRIGHT,     GF_FONTA,     WorldTimer_UpdateGeometry, WorldTimer_Drawer, WorldTimer_Ticker, &hud->worldtimer },
        { GUI_HEALTH,       ALIGN_BOTTOMLEFT, UWG_BOTTOMLEFT,   GF_FONTB,     Health_UpdateGeometry, Health_Drawer, Health_Ticker, &hud->health },
        { GUI_FRAGS,        ALIGN_BOTTOMLEFT, UWG_BOTTOMLEFT,   GF_STATUS,    Frags_UpdateGeometry, Frags_Drawer, Frags_Ticker, &hud->frags },
        { GUI_READYITEM,    ALIGN_BOTTOMRIGHT, UWG_BOTTOMRIGHT, GF_SMALLIN,   ReadyItem_UpdateGeometry, ReadyItem_Drawer, ReadyItem_Ticker, &hud->readyitem },
        { GUI_INVENTORY,    ALIGN_TOPLEFT,    UWG_BOTTOMCENTER, GF_SMALLIN,   Inventory_UpdateGeometry, Inventory_Drawer },
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
        hud->widgetGroupIds[def->group] = GUI_CreateGroup(def->groupFlags, player, def->alignFlags, def->order, def->padding);
    }

    for(i = 0; widgetDefs[i].type != GUI_NONE; ++i)
    {
        const uiwidgetdef_t* def = &widgetDefs[i];
        uiwidgetid_t id = GUI_CreateWidget(def->type, player, def->alignFlags, FID(def->fontIdx), 1, def->updateGeometry, def->drawer, def->ticker, def->typedata);
        UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[def->group]), GUI_FindObjectById(id));
    }

    UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOM]),
                      GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOMLEFT]));
    UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOM]),
                      GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOMCENTER]));
    UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOM]),
                      GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOMRIGHT]));

    //UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOP]),
    //                  GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPLEFT]));
    UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOP]),
                      GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPCENTER]));
    UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOP]),
                      GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPRIGHT]));

    hud->logWidgetId = GUI_CreateWidget(GUI_LOG, player, ALIGN_TOPLEFT, FID(GF_FONTA), 1, UILog_UpdateGeometry, UILog_Drawer, UILog_Ticker, &hud->log);
    UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPCENTER]), GUI_FindObjectById(hud->logWidgetId));

    hud->chatWidgetId = GUI_CreateWidget(GUI_CHAT, player, ALIGN_TOPLEFT, FID(GF_FONTA), 1, UIChat_UpdateGeometry, UIChat_Drawer, NULL, &hud->chat);
    UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPCENTER]), GUI_FindObjectById(hud->chatWidgetId));

    hud->automapWidgetId = GUI_CreateWidget(GUI_AUTOMAP, player, ALIGN_TOPLEFT, FID(GF_FONTA), 1, UIAutomap_UpdateGeometry, UIAutomap_Drawer, UIAutomap_Ticker, &hud->automap);
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
    if(!obj) return false;
    return UIChat_Responder(obj, ev);
}

boolean ST_ChatIsActive(int player)
{
    uiwidget_t* obj = ST_UIChatForPlayer(player);
    if(!obj) return false;
    return UIChat_IsActive(obj);
}

void ST_LogPost(int player, byte flags, const char* msg)
{
    uiwidget_t* obj = ST_UILogForPlayer(player);
    if(!obj) return;
    UILog_Post(obj, flags, msg);
}

void ST_LogRefresh(int player)
{
    uiwidget_t* obj = ST_UILogForPlayer(player);
    if(!obj) return;
    UILog_Refresh(obj);
}

void ST_LogEmpty(int player)
{
    uiwidget_t* obj = ST_UILogForPlayer(player);
    if(!obj) return;
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

        obj = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPCENTER]);
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
    if(!obj) return;
    UIAutomap_Open(obj, yes, fast);
}

boolean ST_AutomapIsActive(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return false;
    return UIAutomap_Active(obj);
}

boolean ST_AutomapObscures2(int player, const RectRaw* region)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return false;
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
                const Rect* rect = UIWidget_Geometry(obj);
                float fx = FIXXTOSCREENX(region->origin.x);
                float fy = FIXYTOSCREENY(region->origin.y);
                float fw = FIXXTOSCREENX(region->size.width);
                float fh = FIXYTOSCREENY(region->size.height);

                if(dims->origin.x >= fx && dims->origin.y >= fy && dims->size.width >= fw && dims->size.height >= fh)
                    return true;
            }*/
        }
    }
    return false;
}

boolean ST_AutomapObscures(int player, int x, int y, int width, int height)
{
    RectRaw rect;
    rect.origin.x = x;
    rect.origin.y = y;
    rect.size.width  = width;
    rect.size.height = height;
    return ST_AutomapObscures2(player, &rect);
}

void ST_AutomapClearPoints(int player)
{
    uiwidget_t* ob = ST_UIAutomapForPlayer(player);
    if(!ob) return;

    UIAutomap_ClearPoints(ob);
    P_SetMessage(&players[player], AMSTR_MARKSCLEARED, false);
}

/**
 * Adds a marker at the specified X/Y location.
 */
int ST_AutomapAddPoint(int player, coord_t x, coord_t y, coord_t z)
{
    static char buffer[20];
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    int newPoint;
    if(!obj) return - 1;

    if(UIAutomap_PointCount(obj) == MAX_MAP_POINTS)
        return -1;

    newPoint = UIAutomap_AddPoint(obj, x, y, z);
    sprintf(buffer, "%s %d", AMSTR_MARKEDSPOT, newPoint);
    P_SetMessage(&players[player], buffer, false);

    return newPoint;
}

boolean ST_AutomapPointOrigin(int player, int point, coord_t* x, coord_t* y, coord_t* z)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return false;
    return UIAutomap_PointOrigin(obj, point, x, y, z);
}

void ST_ToggleAutomapMaxZoom(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return;
    if(UIAutomap_SetZoomMax(obj, !UIAutomap_ZoomMax(obj)))
    {
        Con_Printf("Maximum zoom %s in automap.\n", UIAutomap_ZoomMax(obj)? "ON":"OFF");
    }
}

float ST_AutomapOpacity(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return 0;
    return UIAutomap_Opacity(obj);
}

void ST_SetAutomapCameraRotation(int player, boolean on)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return;
    UIAutomap_SetCameraRotation(obj, on);
}

void ST_ToggleAutomapPanMode(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return;
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
    if(!obj) return;
    setAutomapCheatLevel(obj, level);
}

void ST_RevealAutomap(int player, boolean on)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return;
    UIAutomap_SetReveal(obj, on);
}

boolean ST_AutomapHasReveal(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return false;
    return UIAutomap_Reveal(obj);
}

void ST_RebuildAutomap(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return;
    UIAutomap_Rebuild(obj);
}

int ST_AutomapCheatLevel(int player)
{
    if(player >=0 && player < MAXPLAYERS)
        return hudStates[player].automapCheatLevel;
    return 0;
}

void ST_FlashCurrentItem(int player)
{
    player_t* plr;
    hudstate_t* hud;

    if(player < 0 || player >= MAXPLAYERS) return;

    plr = &players[player];
    if(!(/*(plr->plr->flags & DDPF_LOCAL) &&*/ plr->plr->inGame)) return;

    hud = &hudStates[player];
    hud->readyItemFlashCounter = 4;
}

int ST_Responder(event_t* ev)
{
    int i, eaten = false;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        eaten = ST_ChatResponder(i, ev);
        if(eaten) break;
    }
    return eaten;
}

void ST_Ticker(timespan_t ticLength)
{
    const boolean isSharpTic = DD_IsSharpTick();
    int i;

    if(isSharpTic)
        Hu_InventoryTicker();

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t* plr = &players[i];
        hudstate_t* hud = &hudStates[i];

        if(!plr->plr->inGame)
            continue;

        // Either slide the statusbar in or fade out the fullscreen HUD.
        if(hud->statusbarActive)
        {
            if(hud->alpha > 0.0f)
            {
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
                }
                else if(hud->alpha < 1.0f)
                {
                    hud->alpha += 0.1f;
                }
            }
        }

        // The following is restricted to fixed 35 Hz ticks.
        if(isSharpTic && !P_IsPaused())
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

            if(hud->readyItemFlashCounter > 0)
                --hud->readyItemFlashCounter;
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

static void drawUIWidgetsForPlayer(player_t* plr)
{
/// Units in fixed 320x200 screen space.
#define DISPLAY_BORDER      (2)
#define PADDING             (2)

    const int playerNum = plr - players;
    const int displayMode = headupDisplayMode(playerNum);
    hudstate_t* hud = hudStates + playerNum;
    Size2Raw size, portSize;
    uiwidget_t* obj;
    float scale;
    assert(plr);

    R_ViewPortSize(playerNum, &portSize);

    // The automap is drawn in a viewport scaled coordinate space (of viewwindow dimensions).
    obj = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_AUTOMAP]);
    UIWidget_SetOpacity(obj, ST_AutomapOpacity(playerNum));
    UIWidget_SetMaximumSize(obj, &portSize);
    GUI_DrawWidgetXY(obj, 0, 0);

    // The rest of the UI is drawn in a fixed 320x200 coordinate space.
    // Determine scale factors.
    R_ChooseAlignModeAndScaleFactor(&scale, SCREENWIDTH, SCREENHEIGHT,
        portSize.width, portSize.height, SCALEMODE_SMART_STRETCH);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Scalef(scale, scale, 1);

    if(hud->statusbarActive || (displayMode < 3 || hud->alpha > 0))
    {
        float opacity = /**\kludge: clamp*/MIN_OF(1.0f, hud->alpha)/**kludge end*/ * (1-hud->hideAmount);
        Size2Raw drawnSize = { 0, 0 };
        RectRaw displayRegion;
        int posX, posY, availWidth, availHeight;

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Scalef(1, 1.2f/*aspect correct*/, 1);

        displayRegion.origin.x = displayRegion.origin.y = 0;
        displayRegion.size.width  = .5f + portSize.width  / scale;
        displayRegion.size.height = .5f + portSize.height / (scale * 1.2f /*aspect correct*/);

        if(hud->statusbarActive)
        {
            const float statusbarOpacity = (1 - hud->hideAmount) * hud->showBar;

            obj = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_STATUSBAR]);
            UIWidget_SetOpacity(obj, statusbarOpacity);
            UIWidget_SetMaximumSize(obj, &displayRegion.size);

            GUI_DrawWidget(obj, &displayRegion.origin);

            Size2_Raw(Rect_Size(UIWidget_Geometry(obj)), &drawnSize);
        }

        displayRegion.origin.x += DISPLAY_BORDER;
        displayRegion.origin.y += DISPLAY_BORDER;
        displayRegion.size.width  -= DISPLAY_BORDER*2;
        displayRegion.size.height -= DISPLAY_BORDER*2;

        if(!hud->statusbarActive)
        {
            obj = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOM]);
            UIWidget_SetOpacity(obj, opacity);
            UIWidget_SetMaximumSize(obj, &displayRegion.size);

            GUI_DrawWidget(obj, &displayRegion.origin);

            Size2_Raw(Rect_Size(UIWidget_Geometry(obj)), &drawnSize);
        }

        availHeight = displayRegion.size.height - (drawnSize.height > 0 ? drawnSize.height : 0);
        obj = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_MAPNAME]);
        UIWidget_SetOpacity(obj, ST_AutomapOpacity(playerNum));
        size.width = displayRegion.size.width; size.height = availHeight;
        UIWidget_SetMaximumSize(obj, &size);

        GUI_DrawWidget(obj, &displayRegion.origin);

        // The other displays are always visible except when using the "no-hud" mode.
        if(hud->statusbarActive || displayMode < 3)
            opacity = 1.0f;

        obj = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOP]);
        UIWidget_SetOpacity(obj, opacity);
        UIWidget_SetMaximumSize(obj, &displayRegion.size);

        GUI_DrawWidget(obj, &displayRegion.origin);

        Size2_Raw(Rect_Size(UIWidget_Geometry(obj)), &drawnSize);

        if(!hud->statusbarActive)
        {
            Size2Raw tlDrawnSize;

            obj = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPLEFT]);
            UIWidget_SetOpacity(obj, opacity);
            UIWidget_SetMaximumSize(obj, &displayRegion.size);

            GUI_DrawWidget(obj, &displayRegion.origin);

            Size2_Raw(Rect_Size(UIWidget_Geometry(obj)), &drawnSize);
            posY = displayRegion.origin.y + (drawnSize.height > 0 ? drawnSize.height + PADDING : 0);

            obj = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPLEFT2]);
            UIWidget_SetOpacity(obj, opacity);
            UIWidget_SetMaximumSize(obj, &displayRegion.size);

            GUI_DrawWidgetXY(obj, displayRegion.origin.x, posY);

            Size2_Raw(Rect_Size(UIWidget_Geometry(obj)), &tlDrawnSize);
            if(tlDrawnSize.width > drawnSize.width)
                drawnSize.width = tlDrawnSize.width;
        }
        else
        {
            drawnSize.width = 0;
        }

        posX = displayRegion.origin.x + (drawnSize.width > 0 ? drawnSize.width + PADDING : 0);
        availWidth = displayRegion.size.width - (drawnSize.width > 0 ? drawnSize.width + PADDING : 0);
        obj = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPLEFT3]);
        UIWidget_SetOpacity(obj, opacity);
        size.width = availWidth; size.height = displayRegion.size.height;
        UIWidget_SetMaximumSize(obj, &size);

        GUI_DrawWidgetXY(obj, posX, displayRegion.origin.y);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef PADDING
#undef DISPLAY_BORDER
}

void ST_Drawer(int player)
{
    hudstate_t* hud;

    if(player < 0 || player >= MAXPLAYERS)
    {
#if _DEBUG
        Con_Message("Warning:ST_Drawer: Invalid player #%i, ignoring.\n", player);
#endif
        return;
    }
    if(!players[player].plr->inGame) return;

    R_UpdateViewFilter(player);

    hud = &hudStates[player];
    hud->statusbarActive = (headupDisplayMode(player) < 2) || (ST_AutomapIsActive(player) && (cfg.automapHudDisplay == 0 || cfg.automapHudDisplay == 2));

    drawUIWidgetsForPlayer(players + player);
}

/**
 * Called when the statusbar scale cvar changes.
 */
void updateViewWindow(void)
{
    int i;
    R_ResizeViewWindow(RWF_FORCE);
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

    if(G_GameAction() == GA_QUIT)
    {
        return false;
    }

    obj = ST_UIChatForPlayer(player);
    if(!obj)
    {
        return false;
    }

    if(argc == 2)
    {
        destination = UIChat_ParseDestination(argv[1]);
        if(destination < 0)
        {
            Con_Message("Invalid team number #%i (valid range: 0..%i).\n", destination, NUMTEAMS);
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

    if(G_GameAction() == GA_QUIT)
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

    if(G_GameAction() == GA_QUIT)
        return false;

    if(argc < 2 || argc > 3)
    {
        Con_Message("Usage: %s (team) (macro number)\n", argv[0]);
        Con_Message("Send a chat macro to other player(s).\n"
                    "If (team) is omitted, the message will be sent to all players.\n");
        return true;
    }

    obj = ST_UIChatForPlayer(player);
    if(!obj)
    {
        return false;
    }

    if(argc == 3)
    {
        destination = UIChat_ParseDestination(argv[1]);
        if(destination < 0)
        {
            Con_Message("Invalid team number #%i (valid range: 0..%i).\n", destination, NUMTEAMS);
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
