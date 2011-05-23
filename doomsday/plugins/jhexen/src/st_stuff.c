/**\file st_stuff.c
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

// HEADER FILES ------------------------------------------------------------

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "jhexen.h"
#include "d_net.h"

#include "p_tick.h" // for P_IsPaused
#include "am_map.h"
#include "g_common.h"
#include "p_inventory.h"
#include "hu_lib.h"
#include "hu_chat.h"
#include "hu_inventory.h"
#include "hu_log.h"
#include "hu_stuff.h"
#include "r_common.h"
#include "gl_drawpatch.h"

// MACROS ------------------------------------------------------------------

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

// TYPES -------------------------------------------------------------------

enum {
    UWG_STATUSBAR = 0,
    UWG_BOTTOMLEFT,
    UWG_BOTTOMRIGHT,
    UWG_BOTTOM,
    UWG_TOP,
    UWG_TOPLEFT,
    UWG_TOPLEFT2,
    UWG_TOPLEFT3,
    UWG_TOPRIGHT,
    UWG_TOPRIGHT2,
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

    int widgetGroupIds[NUM_UIWIDGET_GROUPS];

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
    guidata_flight_t flight;
    guidata_boots_t boots;
    guidata_servant_t servant;
    guidata_defense_t defense;
    guidata_worldtimer_t worldtimer;
} hudstate_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void ST_updateWidgets(int player);
void updateViewWindow(void);
void unhideHUD(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DECLARATIONS ------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static hudstate_t hudStates[MAXPLAYERS];

static patchinfo_t pStatusBar;
static patchinfo_t pStatusBarTop;
static patchinfo_t pKills;
static patchinfo_t pStatBar;
static patchinfo_t pKeyBar;
static patchinfo_t pKeySlot[NUM_KEY_TYPES];
static patchinfo_t pArmorSlot[NUMARMOR];
static patchinfo_t pManaAVials[2];
static patchinfo_t pManaBVials[2];
static patchinfo_t pManaAIcons[2];
static patchinfo_t pManaBIcons[2];
static patchinfo_t pInventoryBar;
static patchinfo_t pWeaponSlot[3]; // [Fighter, Cleric, Mage]
static patchinfo_t pWeaponFull[3]; // [Fighter, Cleric, Mage]
static patchinfo_t pLifeGem[3][8]; // [Fighter, Cleric, Mage][color]
static patchinfo_t pWeaponPiece1[3]; // [Fighter, Cleric, Mage]
static patchinfo_t pWeaponPiece2[3]; // [Fighter, Cleric, Mage]
static patchinfo_t pWeaponPiece3[3]; // [Fighter, Cleric, Mage]
static patchinfo_t pChain[3]; // [Fighter, Cleric, Mage]
static patchinfo_t pInvItemFlash[5];
static patchinfo_t pSpinFly[16];
static patchinfo_t pSpinMinotaur[16];
static patchinfo_t pSpinSpeed[16];
static patchinfo_t pSpinDefense[16];

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
    {"hud-mana", 0, CVT_BYTE, &cfg.hudShown[HUD_MANA], 0, 2, unhideHUD},
    {"hud-health", 0, CVT_BYTE, &cfg.hudShown[HUD_HEALTH], 0, 1, unhideHUD},
    {"hud-currentitem", 0, CVT_BYTE, &cfg.hudShown[HUD_READYITEM], 0, 1, unhideHUD},

    // HUD displays
    {"hud-timer", 0, CVT_FLOAT, &cfg.hudTimer, 0, 60},

    {"hud-unhide-damage", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_DAMAGE], 0, 1},
    {"hud-unhide-pickup-health", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_HEALTH], 0, 1},
    {"hud-unhide-pickup-armor", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_ARMOR], 0, 1},
    {"hud-unhide-pickup-powerup", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_POWER], 0, 1},
    {"hud-unhide-pickup-weapon", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_WEAPON], 0, 1},
    {"hud-unhide-pickup-ammo", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_AMMO], 0, 1},
    {"hud-unhide-pickup-key", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_KEY], 0, 1},
    {"hud-unhide-pickup-invitem", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_INVITEM], 0, 1},
    {NULL}
};

// CODE --------------------------------------------------------------------

/**
 * Register CVARs and CCmds for the HUD/Status bar.
 */
void ST_Register(void)
{
    int                 i;

    for(i = 0; sthudCVars[i].name; ++i)
        Con_AddVariable(sthudCVars + i);

    Hu_InventoryRegister();
}

static int fullscreenMode(void)
{
    return (cfg.screenBlocks < 10? 0 : cfg.screenBlocks - 10);
}

void Flight_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_flight_t* flht = (guidata_flight_t*)obj->typedata;
    const player_t* plr = &players[obj->player];

    flht->patchId = 0;
    if(!plr->powers[PT_FLIGHT])
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
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    if(flht->patchId != 0)
    {
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
    if(flht->patchId == 0)
        return;

    if(NULL != width)  *width  = 32 * cfg.hudScale;
    if(NULL != height) *height = 28 * cfg.hudScale;
    }
}

void Boots_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_boots_t* boots = (guidata_boots_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    boots->patchId = 0;
    if(0 != plr->powers[PT_SPEED] && 
       (plr->powers[PT_SPEED] > BLINKTHRESHOLD || !(plr->powers[PT_SPEED] & 16)))
    {
        boots->patchId = pSpinSpeed[(mapTime / 3) & 15].id;
    }
    }
}

void Boots_Drawer(uiwidget_t* obj, int x, int y)
{
    assert(NULL != obj);
    {
    guidata_boots_t* boots = (guidata_boots_t*)obj->typedata;
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(boots->patchId == 0)
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(1, 1, 1, iconAlpha);
    GL_DrawPatch(boots->patchId, 12, 14);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void Boots_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_boots_t* boots = (guidata_boots_t*)obj->typedata;

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(boots->patchId == 0)
        return;

    if(NULL != width)  *width  = 24 * cfg.hudScale;
    if(NULL != height) *height = 28 * cfg.hudScale;
    }
}

void Defense_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_defense_t* dfns = (guidata_defense_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    dfns->patchId = 0;
    if(!plr->powers[PT_INVULNERABILITY])
        return;
    if(plr->powers[PT_INVULNERABILITY] > BLINKTHRESHOLD || !(plr->powers[PT_INVULNERABILITY] & 16))
    {
        dfns->patchId = pSpinDefense[(mapTime / 3) & 15].id;
    }
    }
}

void Defense_Drawer(uiwidget_t* obj, int x, int y)
{
    assert(NULL != obj);
    {
    guidata_defense_t* dfns = (guidata_defense_t*)obj->typedata;
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(dfns->patchId == 0)
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(1, 1, 1, iconAlpha);
    GL_DrawPatch(dfns->patchId, -13, 14);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void Defense_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_defense_t* dfns = (guidata_defense_t*)obj->typedata;
    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(dfns->patchId == 0)
        return;

    if(NULL != width)  *width  = 26 * cfg.hudScale;
    if(NULL != height) *height = 28 * cfg.hudScale;
    }
}

void Servant_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_servant_t* svnt = (guidata_servant_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    svnt->patchId = 0;
    if(!plr->powers[PT_MINOTAUR])
        return;
    if(plr->powers[PT_MINOTAUR] > BLINKTHRESHOLD || !(plr->powers[PT_MINOTAUR] & 16))
    {
        svnt->patchId = pSpinMinotaur[(mapTime / 3) & 15].id;
    }
    }
}

void Servant_Drawer(uiwidget_t* obj, int x, int y)
{
    assert(NULL != obj);
    {
    guidata_servant_t* svnt = (guidata_servant_t*)obj->typedata;
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(svnt->patchId == 0)
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(1, 1, 1, iconAlpha);
    GL_DrawPatch(svnt->patchId, -13, 17);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void Servant_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_servant_t* svnt = (guidata_servant_t*)obj->typedata;

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(svnt->patchId == 0)
        return;

    if(NULL != width)  *width  = 26 * cfg.hudScale;
    if(NULL != height) *height = 29 * cfg.hudScale;
    }
}

void WeaponPieces_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_weaponpieces_t* wpn = (guidata_weaponpieces_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    wpn->pieces = plr->pieces;
    }
}

void SBarWeaponPieces_Drawer(uiwidget_t* obj, int x, int y)
{
#define ORIGINX (-ST_WIDTH/2)
#define ORIGINY (-ST_HEIGHT*hud->showBar)

    assert(NULL != obj);
    {
    guidata_weaponpieces_t* wpn = (guidata_weaponpieces_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int pClass = cfg.playerClass[obj->player]; // Original player class (i.e. not pig).
    int fullscreen = fullscreenMode();
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(obj->player) || AM_IsActive(AM_MapForPlayer(obj->player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    if(wpn->pieces == 7)
    {
        DGL_Color4f(1, 1, 1, iconAlpha);
        GL_DrawPatch(pWeaponFull[pClass].id, ORIGINX+190, ORIGINY);
    }
    else
    {
        if(wpn->pieces & WPIECE1)
        {
            DGL_Color4f(1, 1, 1, iconAlpha);
            GL_DrawPatch(pWeaponPiece1[pClass].id, ORIGINX+PCLASS_INFO(pClass)->pieceX[0], ORIGINY);
        }

        if(wpn->pieces & WPIECE2)
        {
            DGL_Color4f(1, 1, 1, iconAlpha);
            GL_DrawPatch(pWeaponPiece2[pClass].id, ORIGINX+PCLASS_INFO(pClass)->pieceX[1], ORIGINY);
        }

        if(wpn->pieces & WPIECE3)
        {
            DGL_Color4f(1, 1, 1, iconAlpha);
            GL_DrawPatch(pWeaponPiece3[pClass].id, ORIGINX+PCLASS_INFO(pClass)->pieceX[2], ORIGINY);
        }
    }

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef ORIGINX
#undef ORIGINY
}

void SBarWeaponPieces_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_weaponpieces_t* wpn = (guidata_weaponpieces_t*)obj->typedata;

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(Hu_InventoryIsOpen(obj->player) || AM_IsActive(AM_MapForPlayer(obj->player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    if(NULL != width)  *width  = 57 * cfg.statusbarScale;
    if(NULL != height) *height = 30 * cfg.statusbarScale;
    }
}

void SBarChain_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_chain_t* chain = (guidata_chain_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    // Health marker chain animates up to the actual health value.
    int delta, curHealth = MAX_OF(plr->plr->mo->health, 0);
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
}

void SBarChain_Drawer(uiwidget_t* obj, int xOffset, int yOffset)
{
#define ORIGINX (-ST_WIDTH/2)
#define ORIGINY (0)

    assert(NULL != obj);
    {
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
    int fullscreen = fullscreenMode();
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    // Original player class (i.e. not pig).
    pClass = cfg.playerClass[obj->player];

    healthPos = MINMAX_OF(0, chain->healthMarker / 100.f, 100);

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

    gemglow = healthPos;

    // Draw the chain.
    x = ORIGINX+43;
    y = ORIGINY-7;
    w = ST_WIDTH - 43 - 43;
    h = 7;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(xOffset, yOffset, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, chainYOffset, 0);

    DGL_SetPatch(pChain[pClass].id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(1, 1, 1, iconAlpha);

    gemXOffset = 7 + ROUND((w - 14) * healthPos) - pLifeGem[pClass][pColor].width/2;

    if(gemXOffset > 0)
    {   // Left chain section.
        float cw = (float)(pChain[pClass].width - gemXOffset) / pChain[pClass].width;

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

    if(gemXOffset + pLifeGem[pClass][pColor].width < w)
    {   // Right chain section.
        float cw = (w - (float)gemXOffset - pLifeGem[pClass][pColor].width) / pChain[pClass].width;

        DGL_Begin(DGL_QUADS);
            DGL_TexCoord2f(0, 0, 0);
            DGL_Vertex2f(x + gemXOffset + pLifeGem[pClass][pColor].width, y);

            DGL_TexCoord2f(0, cw, 0);
            DGL_Vertex2f(x + w, y);

            DGL_TexCoord2f(0, cw, 1);
            DGL_Vertex2f(x + w, y + h);

            DGL_TexCoord2f(0, 0, 1);
            DGL_Vertex2f(x + gemXOffset + pLifeGem[pClass][pColor].width, y + h);
        DGL_End();
    }

    // Draw the life gem.
    {
    int vX = x + MAX_OF(0, gemXOffset);
    int vWidth;
    float s1 = 0, s2 = 1;

    vWidth = pLifeGem[pClass][pColor].width;
    if(gemXOffset + pLifeGem[pClass][pColor].width > w)
    {
        vWidth -= gemXOffset + pLifeGem[pClass][pColor].width - w;
        s2 = (float)vWidth / pLifeGem[pClass][pColor].width;
    }
    if(gemXOffset < 0)
    {
        vWidth -= -gemXOffset;
        s1 = (float)(-gemXOffset) / pLifeGem[pClass][pColor].width;
    }

    DGL_SetPatch(pLifeGem[pClass][pColor].id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
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
    DGL_DrawRect(x + gemXOffset + 23, y - 6, 41, 24, rgb[0], rgb[1], rgb[2], gemglow - (1 - iconAlpha));

    DGL_BlendMode(BM_NORMAL);
    DGL_Color4f(1, 1, 1, 1);

    DGL_Disable(DGL_TEXTURE_2D);
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

    if(NULL != width)  *width  = (ST_WIDTH - 21 - 28) * cfg.statusbarScale;
    if(NULL != height) *height = 8 * cfg.statusbarScale;
}

/**
 * Draws the whole statusbar backgound.
 * \todo There is a whole lot of constants in here. What if someone wants to
 * replace the statusbar with new patches?
 */
void SBarBackground_Drawer(uiwidget_t* obj, int xOffset, int yOffset)
{
#define WIDTH       (ST_WIDTH)
#define HEIGHT      (ST_HEIGHT)
#define ORIGINX     ((int)(-WIDTH/2))
#define ORIGINY     ((int)(-HEIGHT * hud->showBar))

    assert(NULL != obj);
    {
    const hudstate_t* hud = &hudStates[obj->player];
    int x, y, w, h, pClass = cfg.playerClass[obj->player]; // Original class (i.e. not pig).
    int fullscreen = fullscreenMode();
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarOpacity);
    float cw, ch;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(xOffset, yOffset, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);

    if(!(iconAlpha < 1))
    {
        DGL_Enable(DGL_TEXTURE_2D);

        DGL_Color4f(1, 1, 1, 1);
        GL_DrawPatch(pStatusBar.id, ORIGINX, ORIGINY-28);

        DGL_Disable(DGL_TEXTURE_2D);

        /**
         * \kludge The Hexen statusbar graphic has a chain already in the
         * image, which shows through the modified chain patches.
         * Mask out the chain on the statusbar by drawing a solid black
         * rectangle over it.
         */
        DGL_SetNoMaterial();
        DGL_DrawRect(ORIGINX+44, ORIGINY+31, 232, 7, .1f, .1f, .1f, 1);
        //// \kludge end

        DGL_Enable(DGL_TEXTURE_2D);

        DGL_Color4f(1, 1, 1, 1);
        GL_DrawPatch(pStatusBarTop.id, ORIGINX, ORIGINY-28);

        if(!Hu_InventoryIsOpen(obj->player))
        {
            // Main interface
            if(!AM_IsActive(AM_MapForPlayer(obj->player)))
            {
                GL_DrawPatch(pStatBar.id, ORIGINX+38, ORIGINY);

                if(deathmatch)
                {
                    GL_DrawPatch(pKills.id, ORIGINX+38, ORIGINY);
                }

                GL_DrawPatch(pWeaponSlot[pClass].id, ORIGINX+190, ORIGINY);
            }
            else
            {
                GL_DrawPatch(pKeyBar.id, ORIGINX+38, ORIGINY);
            }
        }
        else
        {
            GL_DrawPatch(pInventoryBar.id, ORIGINX+38, ORIGINY);
        }

        DGL_Disable(DGL_TEXTURE_2D);
    }
    else
    {
        DGL_Enable(DGL_TEXTURE_2D);

        DGL_Color4f(1, 1, 1, iconAlpha);
        DGL_SetPatch(pStatusBar.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);

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
        DGL_DrawCutRectTiled(ORIGINX+38, ORIGINY+31, 244, 8, 320, 65, 38, 192-134, ORIGINX+44, ORIGINY+31, 232, 7);
        DGL_Disable(DGL_TEXTURE_2D);
        DGL_SetNoMaterial();
        DGL_DrawRect(ORIGINX+44, ORIGINY+31, 232, 7, .1f, .1f, .1f, iconAlpha);
        DGL_Color4f(1, 1, 1, iconAlpha);
        //// \kludge end

        if(!Hu_InventoryIsOpen(obj->player))
        {
            DGL_Enable(DGL_TEXTURE_2D);

            // Main interface
            if(!AM_IsActive(AM_MapForPlayer(obj->player)))
            {
                x = ORIGINX + (deathmatch ? 68 : 38);
                y = ORIGINY;
                w = deathmatch?214:244;
                h = 31;
                DGL_SetPatch(pStatBar.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
                DGL_DrawCutRectTiled(x, y, w, h, pStatBar.width, pStatBar.height, deathmatch?30:0, 0, ORIGINX+190, ORIGINY, 57, 30);

                GL_DrawPatch(pWeaponSlot[pClass].id, ORIGINX+190, ORIGINY);
                if(deathmatch)
                    GL_DrawPatch(pKills.id, ORIGINX+38, ORIGINY);
            }
            else
            {
                GL_DrawPatch(pKeyBar.id, ORIGINX+38, ORIGINY);
            }

            DGL_Disable(DGL_TEXTURE_2D);
        }
        else
        {   // INVBAR
            
            DGL_SetPatch(pInventoryBar.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
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

#undef HEIGHT
#undef WIDTH
}

void ST_loadGraphics(void)
{
    char namebuf[9];
    int i;

    R_PrecachePatch("H2BAR", &pStatusBar);
    R_PrecachePatch("H2TOP", &pStatusBarTop);
    R_PrecachePatch("INVBAR", &pInventoryBar);
    R_PrecachePatch("STATBAR", &pStatBar);
    R_PrecachePatch("KEYBAR", &pKeyBar);

    R_PrecachePatch("MANAVL1D", &pManaAVials[0]);
    R_PrecachePatch("MANAVL2D", &pManaBVials[0]);
    R_PrecachePatch("MANAVL1", &pManaAVials[1]);
    R_PrecachePatch("MANAVL2", &pManaBVials[1]);

    R_PrecachePatch("MANADIM1", &pManaAIcons[0]);
    R_PrecachePatch("MANADIM2", &pManaBIcons[0]);
    R_PrecachePatch("MANABRT1", &pManaAIcons[1]);
    R_PrecachePatch("MANABRT2", &pManaBIcons[1]);

    R_PrecachePatch("KILLS", &pKills);

    for(i = 0; i < NUM_KEY_TYPES; ++i)
    {
        sprintf(namebuf, "KEYSLOT%X", i + 1);
        R_PrecachePatch(namebuf, &pKeySlot[i]);
    }

    for(i = 0; i < NUMARMOR; ++i)
    {
        sprintf(namebuf, "ARMSLOT%d", i + 1);
        R_PrecachePatch(namebuf, &pArmorSlot[i]);
    }

    for(i = 0; i < 16; ++i)
    {
        sprintf(namebuf, "SPFLY%d", i);
        R_PrecachePatch(namebuf, &pSpinFly[i]);

        sprintf(namebuf, "SPMINO%d", i);
        R_PrecachePatch(namebuf, &pSpinMinotaur[i]);

        sprintf(namebuf, "SPBOOT%d", i);
        R_PrecachePatch(namebuf, &pSpinSpeed[i]);

        sprintf(namebuf, "SPSHLD%d", i);
        R_PrecachePatch(namebuf, &pSpinDefense[i]);
    }

    // Fighter:
    R_PrecachePatch("WPIECEF1", &pWeaponPiece1[PCLASS_FIGHTER]);
    R_PrecachePatch("WPIECEF2", &pWeaponPiece2[PCLASS_FIGHTER]);
    R_PrecachePatch("WPIECEF3", &pWeaponPiece3[PCLASS_FIGHTER]);
    R_PrecachePatch("CHAIN", &pChain[PCLASS_FIGHTER]);
    R_PrecachePatch("WPSLOT0", &pWeaponSlot[PCLASS_FIGHTER]);
    R_PrecachePatch("WPFULL0", &pWeaponFull[PCLASS_FIGHTER]);
    R_PrecachePatch("LIFEGEM", &pLifeGem[PCLASS_FIGHTER][0]);
    for(i = 1; i < 8; ++i)
    {
        sprintf(namebuf, "LIFEGMF%d", i + 1);
        R_PrecachePatch(namebuf, &pLifeGem[PCLASS_FIGHTER][i]);
    }

    // Cleric:
    R_PrecachePatch("WPIECEC1", &pWeaponPiece1[PCLASS_CLERIC]);
    R_PrecachePatch("WPIECEC2", &pWeaponPiece2[PCLASS_CLERIC]);
    R_PrecachePatch("WPIECEC3", &pWeaponPiece3[PCLASS_CLERIC]);
    R_PrecachePatch("CHAIN2", &pChain[PCLASS_CLERIC]);
    R_PrecachePatch("WPSLOT1", &pWeaponSlot[PCLASS_CLERIC]);
    R_PrecachePatch("WPFULL1", &pWeaponFull[PCLASS_CLERIC]);
    for(i = 0; i < 8; ++i)
    {
        sprintf(namebuf, "LIFEGMC%d", i + 1);
        R_PrecachePatch(namebuf, &pLifeGem[PCLASS_CLERIC][i]);
    }

    // Mage:
    R_PrecachePatch("WPIECEM1", &pWeaponPiece1[PCLASS_MAGE]);
    R_PrecachePatch("WPIECEM2", &pWeaponPiece2[PCLASS_MAGE]);
    R_PrecachePatch("WPIECEM3", &pWeaponPiece3[PCLASS_MAGE]);
    R_PrecachePatch("CHAIN3", &pChain[PCLASS_MAGE]);
    R_PrecachePatch("WPSLOT2", &pWeaponSlot[PCLASS_MAGE]);
    R_PrecachePatch("WPFULL2", &pWeaponFull[PCLASS_MAGE]);
    for(i = 0; i < 8; ++i)
    {
        sprintf(namebuf, "LIFEGMM%d", i + 1);
        R_PrecachePatch(namebuf, &pLifeGem[PCLASS_MAGE][i]);
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
    hud->sbarReadyitem.flashCounter = 0;
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
    hud->readyitem.flashCounter = 0;
    hud->readyitem.patchId = 0;

    // Other:
    hud->flight.patchId = 0;
    hud->flight.hitCenterFrame = false;
    hud->boots.patchId = 0;
    hud->servant.patchId = 0;
    hud->defense.patchId = 0;
    hud->worldtimer.days = hud->worldtimer.hours = hud->worldtimer.minutes = hud->worldtimer.seconds = 0;

    ST_HUDUnHide(player, HUE_FORCE);
}

void ST_Start(int player)
{
    hudstate_t*         hud;

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
    hudstate_t*         hud;

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

void ST_FlashCurrentItem(int player)
{
    player_t*           plr;
    hudstate_t*         hud;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    plr = &players[player];
    if(!((plr->plr->flags & DDPF_LOCAL) && plr->plr->inGame))
        return;

    hud = &hudStates[player];

    hud->sbarReadyitem.flashCounter = 4;
    hud->readyitem.flashCounter = 4;
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

        // Either slide the statusbar in or fade out the fullscreen HUD.
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
 * Sets the new palette based upon the current values of
 * player_t->damageCount and player_t->bonusCount.
 */
void ST_doPaletteStuff(int player)
{
    int palette = 0;
    player_t* plr;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    plr = &players[player];

    if(G_GetGameState() == GS_MAP)
    {
        plr = &players[CONSOLEPLAYER];
        if(plr->poisonCount)
        {
            palette = 0;
            palette = (plr->poisonCount + 7) >> 3;
            if(palette >= NUMPOISONPALS)
            {
                palette = NUMPOISONPALS - 1;
            }
            palette += STARTPOISONPALS;
        }
        else if(plr->damageCount)
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
        else if(plr->plr->mo->flags2 & MF2_ICEDAMAGE)
        {   // Frozen player
            palette = STARTICEPAL;
        }
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

void Keys_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_keys_t* keys = (guidata_keys_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    int i;
    for(i = 0; i < NUM_KEY_TYPES; ++i)
    {
        keys->keyBoxes[i] = (plr->keys & (1 << i));
    }
    }
}

void SBarKeys_Drawer(uiwidget_t* obj, int x, int y)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT*hud->showBar)

    assert(NULL != obj);
    {
    guidata_keys_t* keys = (guidata_keys_t*)obj->typedata;
    hudstate_t* hud = &hudStates[obj->player];
    int i, numDrawn, pClass = cfg.playerClass[obj->player]; // Original player class (i.e. not pig).
    int fullscreen = fullscreenMode();
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(obj->player) || !AM_IsActive(AM_MapForPlayer(obj->player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);

    numDrawn = 0;
    for(i = 0; i < NUM_KEY_TYPES; ++i)
    {
        const patchinfo_t* patch;

        if(!keys->keyBoxes[i])
            continue;

        patch = &pKeySlot[i];

        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconAlpha);
        GL_DrawPatch(patch->id, ORIGINX + 46 + numDrawn * 20, ORIGINY + 1);

        DGL_Disable(DGL_TEXTURE_2D);

        ++numDrawn;
        if(numDrawn == 5)
            break;
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef ORIGINY
#undef ORIGINX
}

void SBarKeys_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_keys_t* keys = (guidata_keys_t*)obj->typedata;
    int i, numVisible;

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(Hu_InventoryIsOpen(obj->player) || !AM_IsActive(AM_MapForPlayer(obj->player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    numVisible = 0;
    for(i = 0; i < NUM_KEY_TYPES; ++i)
    {
        const patchinfo_t* patch;

        if(!keys->keyBoxes[i])
            continue;

        patch = &pKeySlot[i];

        if(NULL != width)
            *width += patch->width;
        if(NULL != height && patch->height > *height)
            *height = patch->height;

        ++numVisible;
        if(numVisible == 5)
            break;
    }

    if(NULL != width && 0 != numVisible)
        *width += (numVisible-1)*20;

    if(NULL != width)  *width  = *width  * cfg.statusbarScale;
    if(NULL != height) *height = *height * cfg.statusbarScale;
    }
}

void ArmorIcons_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_armoricons_t* icons = (guidata_armoricons_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    int i;
    for(i = 0; i < NUMARMOR; ++i)
    {
        icons->types[i].value = plr->armorPoints[i];
    }
    }
}

void SBarArmorIcons_Drawer(uiwidget_t* obj, int x, int y)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT*hud->showBar)

    assert(NULL != obj);
    {
    guidata_armoricons_t* icons = (guidata_armoricons_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int i, pClass = cfg.playerClass[obj->player]; // Original player class (i.e. not pig).
    int fullscreen = fullscreenMode();
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(obj->player) || !AM_IsActive(AM_MapForPlayer(obj->player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);

    for(i = 0; i < NUMARMOR; ++i)
    {
        const patchinfo_t* patch;
        float alpha;

        if(!icons->types[i].value)
            continue;

        patch = &pArmorSlot[i];
        if(icons->types[i].value <= (PCLASS_INFO(pClass)->armorIncrement[i] >> 2))
            alpha = .3f;
        else if(icons->types[i].value <= (PCLASS_INFO(pClass)->armorIncrement[i] >> 1))
            alpha = .6f;
        else
            alpha = 1;

        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconAlpha * alpha);
        GL_DrawPatch(patch->id, ORIGINX + 150 + 31 * i, ORIGINY + 2);
        DGL_Disable(DGL_TEXTURE_2D);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef ORIGINX
#undef ORIGINY
}

void SBarArmorIcons_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_armoricons_t* icons = (guidata_armoricons_t*)obj->typedata;
    int i, numVisible;

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(Hu_InventoryIsOpen(obj->player) || !AM_IsActive(AM_MapForPlayer(obj->player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    numVisible = 0;
    for(i = 0; i < NUMARMOR; ++i)
    {
        const patchinfo_t* patch;
        if(!icons->types[i].value)
            continue;

        patch = &pArmorSlot[i];
        if(NULL != width)
            *width += patch->width;
        if(NULL != height && patch->height > *height)
            *height = patch->height;

        ++numVisible;
    }

    if(NULL != width && 0 != numVisible)
        *width += (numVisible-1)*31;

    if(NULL != width)  *width  = *width  * cfg.statusbarScale;
    if(NULL != height) *height = *height * cfg.statusbarScale;
    }
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
    {
        if(!players[i].plr->inGame)
            continue;
        frags->value += plr->frags[i] * (i != obj->player ? 1 : -1);
    }
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
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = fullscreenMode();
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    char buf[20];

    if(!deathmatch || Hu_InventoryIsOpen(obj->player) || AM_IsActive(AM_MapForPlayer(obj->player)))
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

    FR_SetFont(FID(obj->fontId));
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

    if(!deathmatch || Hu_InventoryIsOpen(obj->player) || AM_IsActive(AM_MapForPlayer(obj->player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(frags->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", frags->value);
    FR_SetFont(FID(obj->fontId));
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
    char buf[20];
    int fullscreen = fullscreenMode();
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);

    if(deathmatch || Hu_InventoryIsOpen(obj->player) || AM_IsActive(AM_MapForPlayer(obj->player)))
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

    FR_SetFont(FID(obj->fontId));
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

    if(deathmatch || Hu_InventoryIsOpen(obj->player) || AM_IsActive(AM_MapForPlayer(obj->player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(hlth->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", hlth->value);
    FR_SetFont(FID(obj->fontId));
    if(NULL != width)  *width  = FR_TextFragmentWidth2(buf, TRACKING) * cfg.statusbarScale;
    if(NULL != height) *height = FR_TextFragmentHeight(buf) * cfg.statusbarScale;
    }
#undef TRACKING
}

void SBarArmor_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_armor_t* armor = (guidata_armor_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    int pClass = cfg.playerClass[obj->player]; // Original player class (i.e. not pig).

    armor->value = FixedDiv(
        PCLASS_INFO(pClass)->autoArmorSave + plr->armorPoints[ARMOR_ARMOR] +
        plr->armorPoints[ARMOR_SHIELD] +
        plr->armorPoints[ARMOR_HELMET] +
        plr->armorPoints[ARMOR_AMULET], 5 * FRACUNIT) >> FRACBITS;
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
    char buf[20];

    if(Hu_InventoryIsOpen(obj->player) || AM_IsActive(AM_MapForPlayer(obj->player)))
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

    FR_SetFont(FID(obj->fontId));
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

    if(Hu_InventoryIsOpen(obj->player) || AM_IsActive(AM_MapForPlayer(obj->player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(armor->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", armor->value);
    FR_SetFont(FID(obj->fontId));
    if(NULL != width)  *width  = FR_TextFragmentWidth2(buf, TRACKING) * cfg.statusbarScale;
    if(NULL != height) *height = FR_TextFragmentHeight(buf) * cfg.statusbarScale;
    }
#undef TRACKING
}

void BlueMana_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_bluemana_t* mana = (guidata_bluemana_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    mana->value = plr->ammo[AT_BLUEMANA].owned;
    }
}

void SBarBlueMana_Drawer(uiwidget_t* obj, int x, int y)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX+ST_MANAAX)
#define Y                   (ORIGINY+ST_MANAAY)
#define MAXDIGITS           (ST_MANAAWIDTH)

    assert(NULL != obj);
    {
    guidata_bluemana_t* mana = (guidata_bluemana_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = fullscreenMode();
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    char buf[20];

    if(mana->value <= 0 || Hu_InventoryIsOpen(obj->player) || AM_IsActive(AM_MapForPlayer(obj->player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(mana->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", mana->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(obj->fontId));
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

void SBarBlueMana_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_bluemana_t* mana = (guidata_bluemana_t*)obj->typedata;
    char buf[20];

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(mana->value <= 0 || Hu_InventoryIsOpen(obj->player) || AM_IsActive(AM_MapForPlayer(obj->player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(mana->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", mana->value);
    FR_SetFont(FID(obj->fontId));
    if(NULL != width)  *width  = FR_TextFragmentWidth(buf)  * cfg.statusbarScale;
    if(NULL != height) *height = FR_TextFragmentHeight(buf) * cfg.statusbarScale;
    }
}

void GreenMana_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_greenmana_t* mana = (guidata_greenmana_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    mana->value = plr->ammo[AT_GREENMANA].owned;
    }
}

void SBarGreenMana_Drawer(uiwidget_t* obj, int x, int y)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX+ST_MANABX)
#define Y                   (ORIGINY+ST_MANABY)
#define MAXDIGITS           (ST_MANABWIDTH)

    assert(NULL != obj);
    {
    guidata_greenmana_t* mana = (guidata_greenmana_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = fullscreenMode();
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    char buf[20];

    if(mana->value <= 0 || Hu_InventoryIsOpen(obj->player) || AM_IsActive(AM_MapForPlayer(obj->player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(mana->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", mana->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(obj->fontId));
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

void SBarGreenMana_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_greenmana_t* mana = (guidata_greenmana_t*)obj->typedata;
    char buf[20];

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(mana->value <= 0 || Hu_InventoryIsOpen(obj->player) || AM_IsActive(AM_MapForPlayer(obj->player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(mana->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", mana->value);
    FR_SetFont(FID(obj->fontId));
    if(NULL != width)  *width  = FR_TextFragmentWidth(buf)  * cfg.statusbarScale;
    if(NULL != height) *height = FR_TextFragmentHeight(buf) * cfg.statusbarScale;
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
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)

    assert(NULL != obj);
    {
    guidata_readyitem_t* item = (guidata_readyitem_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    inventoryitemtype_t readyItem;
    patchinfo_t boxInfo;
    int x, y;
    int fullscreen = fullscreenMode();
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(obj->player) || AM_IsActive(AM_MapForPlayer(obj->player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(item->patchId == 0)
        return;
    if(!R_GetPatchInfo(pInvItemBox, &boxInfo))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(xOffset, yOffset, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);

    if(item->flashCounter > 0)
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
    GL_DrawPatch(item->patchId, ORIGINX+x, ORIGINY+y);

    readyItem = P_InventoryReadyItem(obj->player);
    if(!(item->flashCounter > 0) && readyItem != IIT_NONE)
    {
        uint count = P_InventoryCount(obj->player, readyItem);
        if(count > 1)
        {
            char buf[20];
            FR_SetFont(FID(obj->fontId));
            DGL_Color4f(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
            dd_snprintf(buf, 20, "%i", count);
            FR_DrawTextFragment2(buf, ORIGINX+ST_INVITEMCX, ORIGINY+ST_INVITEMCY, DTF_ALIGN_TOPRIGHT|DTF_NO_EFFECTS);
        }
    }

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef ORIGINY
#undef ORIGINX
}

void SBarReadyItem_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_readyitem_t* item = (guidata_readyitem_t*)obj->typedata;
    patchinfo_t boxInfo;

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(Hu_InventoryIsOpen(obj->player) || AM_IsActive(AM_MapForPlayer(obj->player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(item->patchId != 0)
        return;
    if(!R_GetPatchInfo(pInvItemBox, &boxInfo))
        return;

    if(NULL != width)  *width  = boxInfo.width  * cfg.statusbarScale;
    if(NULL != height) *height = boxInfo.height * cfg.statusbarScale;
    }
}

void BlueManaIcon_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_bluemanaicon_t* icon = (guidata_bluemanaicon_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
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
}

void SBarBlueManaIcon_Drawer(uiwidget_t* obj, int x, int y)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX+ST_MANAAICONX)
#define Y                   (ORIGINY+ST_MANAAICONY)

    assert(NULL != obj);
    {
    guidata_bluemanaicon_t* icon = (guidata_bluemanaicon_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = fullscreenMode();
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(obj->player) || AM_IsActive(AM_MapForPlayer(obj->player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    if(icon->iconIdx >= 0)
    {
        patchid_t patchId = pManaAIcons[icon->iconIdx].id;

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        DGL_Translatef(x, y, 0);
        DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
        DGL_Translatef(0, yOffset, 0);
        DGL_Enable(DGL_TEXTURE_2D);

        WI_DrawPatch4(patchId, Hu_ChoosePatchReplacement(patchId), X, Y, DPF_ALIGN_TOPLEFT, FID(GF_FONTB), 1, 1, 1, iconAlpha);

        DGL_Disable(DGL_TEXTURE_2D);
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }
    }
#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarBlueManaIcon_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_bluemanaicon_t* icon = (guidata_bluemanaicon_t*)obj->typedata;

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(Hu_InventoryIsOpen(obj->player) || AM_IsActive(AM_MapForPlayer(obj->player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    if(NULL != width)  *width  = pManaAIcons[icon->iconIdx%2].width  * cfg.statusbarScale;
    if(NULL != height) *height = pManaAIcons[icon->iconIdx%2].height * cfg.statusbarScale;
    }
}

void GreenManaIcon_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_greenmanaicon_t* icon = (guidata_greenmanaicon_t*)obj->typedata;
    const player_t* plr = &players[obj->player];

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
}

void SBarGreenManaIcon_Drawer(uiwidget_t* obj, int x, int y)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX+ST_MANABICONX)
#define Y                   (ORIGINY+ST_MANABICONY)

    assert(NULL != obj);
    {
    guidata_greenmanaicon_t* icon = (guidata_greenmanaicon_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = fullscreenMode();
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(obj->player) || AM_IsActive(AM_MapForPlayer(obj->player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    if(icon->iconIdx >= 0)
    {
        patchid_t patchId = pManaBIcons[icon->iconIdx].id;

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        DGL_Translatef(x, y, 0);
        DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
        DGL_Translatef(0, yOffset, 0);
        DGL_Enable(DGL_TEXTURE_2D);

        WI_DrawPatch4(patchId, Hu_ChoosePatchReplacement(patchId), X, Y, DPF_ALIGN_TOPLEFT, FID(GF_FONTB), 1, 1, 1, iconAlpha);

        DGL_Disable(DGL_TEXTURE_2D);
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }
    }
#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarGreenManaIcon_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_greenmanaicon_t* icon = (guidata_greenmanaicon_t*)obj->typedata;

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(Hu_InventoryIsOpen(obj->player) || AM_IsActive(AM_MapForPlayer(obj->player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    if(NULL != width)  *width  = pManaBIcons[icon->iconIdx%2].width  * cfg.statusbarScale;
    if(NULL != height) *height = pManaBIcons[icon->iconIdx%2].height * cfg.statusbarScale;
    }
}

void BlueManaVial_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_bluemanavial_t* vial = (guidata_bluemanavial_t*)obj->typedata;
    const player_t* plr = &players[obj->player];

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
}

void SBarBlueManaVial_Drawer(uiwidget_t* obj, int x, int y)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (ST_HEIGHT*(1-hud->showBar))
#define X                   (ORIGINX+ST_MANAAVIALX)
#define Y                   (ORIGINY+ST_MANAAVIALY)
#define VIALHEIGHT          (22)

    assert(NULL != obj);
    {
    guidata_bluemanavial_t* vial = (guidata_bluemanavial_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int fullscreen = fullscreenMode();
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(obj->player) || AM_IsActive(AM_MapForPlayer(obj->player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, ORIGINY, 0);

    if(vial->iconIdx >= 0)
    {
        patchid_t patchId = pManaAVials[vial->iconIdx].id;
        DGL_Enable(DGL_TEXTURE_2D);
        WI_DrawPatch4(patchId, Hu_ChoosePatchReplacement(patchId), X, Y, DPF_ALIGN_TOPLEFT, FID(GF_FONTB), 1, 1, 1, iconAlpha);
        DGL_Disable(DGL_TEXTURE_2D);
    }

    DGL_SetNoMaterial();
    DGL_DrawRect(ORIGINX+95, -ST_HEIGHT+3, 3, (int) (VIALHEIGHT * (1-vial->filled) + .5f), 0, 0, 0, iconAlpha);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef VIALHEIGHT
#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarBlueManaVial_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_bluemanavial_t* vial = (guidata_bluemanavial_t*)obj->typedata;

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(Hu_InventoryIsOpen(obj->player) || AM_IsActive(AM_MapForPlayer(obj->player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    if(NULL != width)  *width  = pManaAVials[vial->iconIdx%2].width  * cfg.statusbarScale;
    if(NULL != height) *height = pManaAVials[vial->iconIdx%2].height * cfg.statusbarScale;
    }
}

void GreenManaVial_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_greenmanavial_t* vial = (guidata_greenmanavial_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
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
}

void SBarGreenManaVial_Drawer(uiwidget_t* obj, int x, int y)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (ST_HEIGHT*(1-hud->showBar))
#define X                   (ORIGINX+ST_MANABVIALX)
#define Y                   (ORIGINY+ST_MANABVIALY)
#define VIALHEIGHT          (22)

    assert(NULL != obj);
    {
    guidata_greenmanavial_t* vial = (guidata_greenmanavial_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int fullscreen = fullscreenMode();
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(obj->player) || AM_IsActive(AM_MapForPlayer(obj->player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, ORIGINY, 0);

    if(vial->iconIdx >= 0)
    {
        patchid_t patchId = pManaBVials[vial->iconIdx].id;
        DGL_Enable(DGL_TEXTURE_2D);
        WI_DrawPatch4(patchId, Hu_ChoosePatchReplacement(patchId), X, Y, DPF_ALIGN_TOPLEFT, FID(GF_FONTB), 1, 1, 1, iconAlpha);
        DGL_Disable(DGL_TEXTURE_2D);
    }

    DGL_SetNoMaterial();
    DGL_DrawRect(ORIGINX+103, -ST_HEIGHT+3, 3, (int) (VIALHEIGHT * (1-vial->filled) + .5f), 0, 0, 0, iconAlpha);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef VIALHEIGHT
#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarGreenManaVial_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_greenmanavial_t* vial = (guidata_greenmanavial_t*)obj->typedata;

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(Hu_InventoryIsOpen(obj->player) || AM_IsActive(AM_MapForPlayer(obj->player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    if(NULL != width)  *width  = pManaBVials[vial->iconIdx%2].width  * cfg.statusbarScale;
    if(NULL != height) *height = pManaBVials[vial->iconIdx%2].height * cfg.statusbarScale;
    }
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

void Health_Drawer(uiwidget_t* obj, int x, int y)
{
#define TRACKING                (1)

    assert(NULL != obj);
    {
    guidata_health_t* hlth = (guidata_health_t*)obj->typedata;
    int value = MAX_OF(hlth->value, 0);
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    char buf[20];

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(hlth->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(obj->fontId));
    DGL_Color4f(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    FR_DrawTextFragment3(buf, -1, -1, DTF_ALIGN_BOTTOMLEFT|DTF_NO_EFFECTS, TRACKING);

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
    int value = MAX_OF(hlth->value, 0);
    char buf[20];

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(hlth->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", value);
    FR_SetFont(FID(obj->fontId));
    if(NULL != width)  *width  = FR_TextFragmentWidth2(buf, TRACKING) * cfg.hudScale;
    if(NULL != height) *height = FR_TextFragmentHeight(buf) * cfg.hudScale;
    }

#undef TRACKING
}

void BlueManaIcon_Drawer(uiwidget_t* obj, int x, int y)
{
    assert(NULL != obj);
    {
    guidata_bluemanaicon_t* icon = (guidata_bluemanaicon_t*)obj->typedata;
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    if(icon->iconIdx >= 0)
    {
        patchid_t patchId = pManaAIcons[icon->iconIdx].id;
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        DGL_Translatef(x, y, 0);
        DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
        DGL_Enable(DGL_TEXTURE_2D);

        WI_DrawPatch4(patchId, Hu_ChoosePatchReplacement(patchId), 0, 0, DPF_ALIGN_TOPLEFT, FID(GF_FONTB), 1, 1, 1, iconAlpha);

        DGL_Disable(DGL_TEXTURE_2D);
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }
    }
}

void BlueManaIcon_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_bluemanaicon_t* icon = (guidata_bluemanaicon_t*)obj->typedata;

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    FR_SetFont(FID(GF_STATUS));
    if(NULL != width)  *width  = pManaAIcons[icon->iconIdx%2].width  * cfg.hudScale;
    if(NULL != height) *height = pManaAIcons[icon->iconIdx%2].height * cfg.hudScale;
    }
}

void BlueMana_Drawer(uiwidget_t* obj, int x, int y)
{
#define TRACKING                (1)

    assert(NULL != obj);
    {
    guidata_bluemana_t* mana = (guidata_bluemana_t*)obj->typedata;
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    char buf[20];

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(mana->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", mana->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(obj->fontId));
    DGL_Color4f(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    FR_DrawTextFragment3(buf, 0, 0, DTF_ALIGN_TOPLEFT|DTF_NO_EFFECTS, TRACKING);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef TRACKING
}

void BlueMana_Dimensions(uiwidget_t* obj, int* width, int* height)
{
#define TRACKING                (1)

    assert(NULL != obj);
    {
    guidata_bluemana_t* mana = (guidata_bluemana_t*)obj->typedata;
    char buf[20];

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(mana->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", mana->value);
    FR_SetFont(FID(obj->fontId));
    if(NULL != width)  *width  = FR_TextFragmentWidth2(buf, TRACKING) * cfg.hudScale;
    if(NULL != height) *height = FR_TextFragmentHeight(buf) * cfg.hudScale;
    }
#undef TRACKING
}

void GreenManaIcon_Drawer(uiwidget_t* obj, int x, int y)
{
    assert(NULL != obj);
    {
    guidata_greenmanaicon_t* icon = (guidata_greenmanaicon_t*)obj->typedata;
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    if(icon->iconIdx >= 0)
    {
        patchid_t patchId = pManaBIcons[icon->iconIdx].id;

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        DGL_Translatef(x, y, 0);
        DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
        DGL_Enable(DGL_TEXTURE_2D);

        WI_DrawPatch4(patchId, Hu_ChoosePatchReplacement(patchId), 0, 0, DPF_ALIGN_TOPLEFT, FID(GF_FONTB), 1, 1, 1, iconAlpha);

        DGL_Disable(DGL_TEXTURE_2D);
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }
    }
}

void GreenManaIcon_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_greenmanaicon_t* icon = (guidata_greenmanaicon_t*)obj->typedata;

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    if(NULL != width)  *width  = pManaBIcons[icon->iconIdx%2].width  * cfg.hudScale;
    if(NULL != height) *height = pManaBIcons[icon->iconIdx%2].height * cfg.hudScale;
    }
}

void GreenMana_Drawer(uiwidget_t* obj, int x, int y)
{
#define TRACKING                (1)

    assert(NULL != obj);
    {
    guidata_greenmana_t* mana = (guidata_greenmana_t*)obj->typedata;
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    char buf[20];

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(mana->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", mana->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(obj->fontId));
    DGL_Color4f(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    FR_DrawTextFragment3(buf, 0, 0, DTF_ALIGN_TOPLEFT|DTF_NO_EFFECTS, TRACKING);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef TRACKING
}

void GreenMana_Dimensions(uiwidget_t* obj, int* width, int* height)
{
#define TRACKING                (1)

    assert(NULL != obj);
    {
    guidata_greenmana_t* mana = (guidata_greenmana_t*)obj->typedata;
    char buf[20];

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(mana->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", mana->value);

    FR_SetFont(FID(obj->fontId));
    if(NULL != width)  *width  = FR_TextFragmentWidth2(buf, TRACKING) * cfg.hudScale;
    if(NULL != height) *height = FR_TextFragmentHeight(buf) * cfg.hudScale;
    }
#undef TRACKING
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

    FR_SetFont(FID(obj->fontId));
    DGL_Color4f(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    FR_DrawTextFragment3(buf, 0, -13, DTF_ALIGN_TOPLEFT|DTF_NO_EFFECTS, TRACKING);

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
    FR_SetFont(FID(obj->fontId));
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
    int xOffset, yOffset;
    patchinfo_t boxInfo;

    if(Hu_InventoryIsOpen(obj->player))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(item->patchId == 0)
        return;
    if(!R_GetPatchInfo(pInvItemBox, &boxInfo))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(1, 1, 1, iconAlpha/2);
    GL_DrawPatch(pInvItemBox, -30, -30);

    if(item->flashCounter > 0)
    {
        xOffset = -27;
        yOffset = -30;
    }
    else
    {
        xOffset = -32;
        yOffset = -31;
    }

    DGL_Color4f(1, 1, 1, iconAlpha);
    GL_DrawPatch(item->patchId, xOffset, yOffset);

    readyItem = P_InventoryReadyItem(obj->player);
    if(item->flashCounter == 0 && readyItem != IIT_NONE)
    {
        uint count = P_InventoryCount(obj->player, readyItem);
        if(count > 1)
        {
            char buf[20];
            FR_SetFont(FID(obj->fontId));
            DGL_Color4f(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
            dd_snprintf(buf, 20, "%i", count);
            FR_DrawTextFragment2(buf, -2, -7, DTF_ALIGN_TOPRIGHT|DTF_NO_EFFECTS);
        }
    }

    DGL_Disable(DGL_TEXTURE_2D);
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

    if(NULL != width)  *width  = (31 * 7 + 16 * 2) * EXTRA_SCALE * cfg.hudScale;
    if(NULL != height) *height = INVENTORY_HEIGHT  * EXTRA_SCALE * cfg.hudScale;

#undef EXTRA_SCALE
#undef INVENTORY_HEIGHT
}

void WorldTimer_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_worldtimer_t* time = (guidata_worldtimer_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    int worldTime = plr->worldTimer / TICRATE;
    time->days    = worldTime / 86400; worldTime -= time->days * 86400;
    time->hours   = worldTime / 3600;  worldTime -= time->hours * 3600;
    time->minutes = worldTime / 60;    worldTime -= time->minutes * 60;
    time->seconds = worldTime;
    }
}

void WorldTimer_Drawer(uiwidget_t* obj, int xOffset, int yOffset)
{
#define ORIGINX         (0)
#define ORIGINY         (0)
#define LEADING         (.5)
#define DRAWFLAGS       (DTF_ALIGN_TOP|DTF_NO_EFFECTS)

    assert(NULL != obj);
    {
    guidata_worldtimer_t* time = (guidata_worldtimer_t*)obj->typedata;
    int counterWidth, spacerWidth, lineHeight, x, y;
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    char buf[20];

    if(!AM_IsActive(AM_MapForPlayer(obj->player)))
        return;

    FR_SetFont(FID(obj->fontId));
    FR_TextFragmentDimensions(&counterWidth, &lineHeight, "00");
    spacerWidth = FR_TextFragmentWidth(" : ");

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(xOffset, yOffset, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);

    DGL_Color4f(1, 1, 1, textAlpha);
    DGL_Enable(DGL_TEXTURE_2D);

    x = ORIGINX;
    y = ORIGINY;
    dd_snprintf(buf, 20, "%.2d", time->seconds);
    FR_DrawTextFragment2(buf, x, y, DRAWFLAGS|DTF_ALIGN_RIGHT);
    x -= counterWidth + spacerWidth;

    FR_DrawChar2(':', x + spacerWidth/2, y, DRAWFLAGS);

    dd_snprintf(buf, 20, "%.2d", time->minutes);
    FR_DrawTextFragment2(buf, x, y, DRAWFLAGS|DTF_ALIGN_RIGHT);
    x -= counterWidth + spacerWidth;

    FR_DrawChar2(':', x + spacerWidth/2, y, DRAWFLAGS);

    dd_snprintf(buf, 20, "%.2d", time->hours);
    FR_DrawTextFragment2(buf, x, y, DRAWFLAGS|DTF_ALIGN_RIGHT);
    x -= counterWidth;
    y += lineHeight;

    if(time->days)
    {
        y += lineHeight * LEADING;
        dd_snprintf(buf, 20, "%.2d %s", time->days, time->days == 1? "day" : "days");
        FR_DrawTextFragment2(buf, ORIGINX, y, DRAWFLAGS|DTF_ALIGN_RIGHT);
        y += lineHeight;

        if(time->days >= 5)
        {
            y += lineHeight * LEADING;
            strncpy(buf, "You Freak!!!", 20);
            FR_DrawTextFragment2(buf, ORIGINX, y, DRAWFLAGS|DTF_ALIGN_RIGHT);
            x = -MAX_OF(abs(x), FR_TextFragmentWidth(buf));
            y += lineHeight;
        }
    }

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef DRAWFLAGS
#undef LEADING
#undef ORIGINY
#undef ORIGINX
}

void WorldTimer_Dimensions(uiwidget_t* obj, int* width, int* height)
{
#define ORIGINX         (0)
#define ORIGINY         (0)
#define LEADING         (.5)

    assert(NULL != obj);
    {
    guidata_worldtimer_t* time = (guidata_worldtimer_t*)obj->typedata;
    int counterWidth, spacerWidth, lineHeight, x, y;
    char buf[20];

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(!AM_IsActive(AM_MapForPlayer(obj->player)))
        return;

    FR_SetFont(FID(obj->fontId));
    FR_TextFragmentDimensions(&counterWidth, &lineHeight, "00");
    spacerWidth = FR_TextFragmentWidth(" : ");

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
            x = -MAX_OF(abs(x), FR_TextFragmentWidth(buf));
            y += lineHeight;
        }
    }

    if(NULL != width)  *width  = (ORIGINX - x) * cfg.hudScale;
    if(NULL != height) *height = (y - ORIGINY) * cfg.hudScale;
    }
#undef DRAWFLAGS
#undef LEADING
#undef ORIGINY
#undef ORIGINX
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
    if(NULL != width)  *width  = 0;
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
    gamefontid_t fontId;
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
            { UWG_BOTTOMLEFT,   UWGF_ALIGN_BOTTOM|UWGF_ALIGN_LEFT|UWGF_LEFTTORIGHT, PADDING },
            { UWG_BOTTOMRIGHT,  UWGF_ALIGN_BOTTOM|UWGF_ALIGN_RIGHT|UWGF_RIGHTTOLEFT, PADDING },
            { UWG_BOTTOM,       UWGF_ALIGN_BOTTOM|UWGF_VERTICAL|UWGF_RIGHTTOLEFT, PADDING },
            { UWG_TOP,          UWGF_ALIGN_TOP|UWGF_ALIGN_LEFT|UWGF_VERTICAL|UWGF_LEFTTORIGHT, PADDING },
            { UWG_TOPLEFT,      UWGF_ALIGN_TOP|UWGF_ALIGN_LEFT|UWGF_LEFTTORIGHT, PADDING },
            { UWG_TOPLEFT2,     UWGF_ALIGN_TOP|UWGF_ALIGN_LEFT|UWGF_LEFTTORIGHT, PADDING },
            { UWG_TOPLEFT3,     UWGF_ALIGN_TOP|UWGF_ALIGN_LEFT|UWGF_LEFTTORIGHT, PADDING },
            { UWG_TOPRIGHT,     UWGF_ALIGN_TOP|UWGF_ALIGN_RIGHT|UWGF_RIGHTTOLEFT, PADDING },
            { UWG_TOPRIGHT2,    UWGF_ALIGN_TOP|UWGF_ALIGN_RIGHT|UWGF_VERTICAL|UWGF_LEFTTORIGHT, PADDING }
        };
        const uiwidgetdef_t widgetDefs[] = {
            { GUI_BOX,          UWG_STATUSBAR,    -1,         0,            SBarBackground_Dimensions, SBarBackground_Drawer },
            { GUI_WEAPONPIECES, UWG_STATUSBAR,    -1,         0,            SBarWeaponPieces_Dimensions, SBarWeaponPieces_Drawer, WeaponPieces_Ticker, &hud->sbarWeaponpieces },
            { GUI_CHAIN,        UWG_STATUSBAR,    -1,         0,            SBarChain_Dimensions, SBarChain_Drawer, SBarChain_Ticker, &hud->sbarChain },
            { GUI_INVENTORY,    UWG_STATUSBAR,    -1,         GF_SMALLIN,   SBarInventory_Dimensions, SBarInventory_Drawer },
            { GUI_KEYS,         UWG_STATUSBAR,    -1,         0,            SBarKeys_Dimensions, SBarKeys_Drawer, Keys_Ticker, &hud->sbarKeys },
            { GUI_ARMORICONS,   UWG_STATUSBAR,    -1,         0,            SBarArmorIcons_Dimensions, SBarArmorIcons_Drawer, ArmorIcons_Ticker, &hud->sbarArmoricons },
            { GUI_FRAGS,        UWG_STATUSBAR,    -1,         GF_STATUS,    SBarFrags_Dimensions, SBarFrags_Drawer, Frags_Ticker, &hud->sbarFrags },
            { GUI_HEALTH,       UWG_STATUSBAR,    -1,         GF_STATUS,    SBarHealth_Dimensions, SBarHealth_Drawer, Health_Ticker, &hud->sbarHealth },
            { GUI_ARMOR,        UWG_STATUSBAR,    -1,         GF_STATUS,    SBarArmor_Dimensions, SBarArmor_Drawer, SBarArmor_Ticker, &hud->sbarArmor },
            { GUI_READYITEM,    UWG_STATUSBAR,    -1,         GF_SMALLIN,   SBarReadyItem_Dimensions, SBarReadyItem_Drawer, ReadyItem_Ticker, &hud->sbarReadyitem },
            { GUI_BLUEMANAICON, UWG_STATUSBAR,    -1,         0,            SBarBlueManaIcon_Dimensions, SBarBlueManaIcon_Drawer, BlueManaIcon_Ticker, &hud->sbarBluemanaicon },
            { GUI_BLUEMANA,     UWG_STATUSBAR,    -1,         GF_SMALLIN,   SBarBlueMana_Dimensions, SBarBlueMana_Drawer, BlueMana_Ticker, &hud->sbarBluemana },
            { GUI_BLUEMANAVIAL, UWG_STATUSBAR,    -1,         0,            SBarBlueManaVial_Dimensions, SBarBlueManaVial_Drawer, BlueManaVial_Ticker, &hud->sbarBluemanavial },
            { GUI_GREENMANAICON, UWG_STATUSBAR,   -1,         0,            SBarGreenManaIcon_Dimensions, SBarGreenManaIcon_Drawer, GreenManaIcon_Ticker, &hud->sbarGreenmanaicon },
            { GUI_GREENMANA,    UWG_STATUSBAR,    -1,         GF_SMALLIN,   SBarGreenMana_Dimensions, SBarGreenMana_Drawer, GreenMana_Ticker, &hud->sbarGreenmana },
            { GUI_GREENMANAVIAL, UWG_STATUSBAR,   -1,         0,            SBarGreenManaVial_Dimensions, SBarGreenManaVial_Drawer, GreenManaVial_Ticker, &hud->sbarGreenmanavial },
            { GUI_BLUEMANAICON, UWG_TOPLEFT,      HUD_MANA,   0,            BlueManaIcon_Dimensions, BlueManaIcon_Drawer, BlueManaIcon_Ticker, &hud->bluemanaicon },
            { GUI_BLUEMANA,     UWG_TOPLEFT,      HUD_MANA,   GF_STATUS,    BlueMana_Dimensions, BlueMana_Drawer, BlueMana_Ticker, &hud->bluemana },
            { GUI_GREENMANAICON, UWG_TOPLEFT2,    HUD_MANA,   0,            GreenManaIcon_Dimensions, GreenManaIcon_Drawer, GreenManaIcon_Ticker, &hud->greenmanaicon },
            { GUI_GREENMANA,    UWG_TOPLEFT2,     HUD_MANA,   GF_STATUS,    GreenMana_Dimensions, GreenMana_Drawer, GreenMana_Ticker, &hud->greenmana },
            { GUI_FLIGHT,       UWG_TOPLEFT3,     -1,         0,            Flight_Dimensions, Flight_Drawer, Flight_Ticker, &hud->flight },
            { GUI_BOOTS,        UWG_TOPLEFT3,     -1,         0,            Boots_Dimensions, Boots_Drawer, Boots_Ticker, &hud->boots },
            { GUI_SERVANT,      UWG_TOPRIGHT,     -1,         0,            Servant_Dimensions, Servant_Drawer, Servant_Ticker, &hud->servant },
            { GUI_DEFENSE,      UWG_TOPRIGHT,     -1,         0,            Defense_Dimensions, Defense_Drawer, Defense_Ticker, &hud->defense },
            { GUI_WORLDTIMER,   UWG_TOPRIGHT2,    -1,         GF_FONTA,     WorldTimer_Dimensions, WorldTimer_Drawer, WorldTimer_Ticker, &hud->worldtimer },
            { GUI_HEALTH,       UWG_BOTTOMLEFT,   HUD_HEALTH, GF_FONTB,     Health_Dimensions, Health_Drawer, Health_Ticker, &hud->health },
            { GUI_FRAGS,        UWG_BOTTOMLEFT,   -1,         GF_STATUS,    Frags_Dimensions, Frags_Drawer, Frags_Ticker, &hud->frags },
            { GUI_READYITEM,    UWG_BOTTOMRIGHT,  HUD_READYITEM, GF_SMALLIN,ReadyItem_Dimensions, ReadyItem_Drawer, ReadyItem_Ticker, &hud->readyitem },
            { GUI_INVENTORY,    UWG_BOTTOM,       -1,         GF_SMALLIN,   Inventory_Dimensions, Inventory_Drawer },
            { GUI_LOG,          UWG_TOP,          -1,         GF_FONTA,     Log_Dimensions2, Log_Drawer2 },
            { GUI_CHAT,         UWG_TOP,          -1,         GF_FONTA,     Chat_Dimensions2, Chat_Drawer2 },
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
            uiwidgetid_t id = GUI_CreateWidget(def->type, player, def->hideId, def->fontId, def->dimensions, def->drawer, def->ticker, def->typedata);
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

    hud->statusbarActive = (fullscreen < 2) || (AM_IsActive(AM_MapForPlayer(player)) && (cfg.automapHudDisplay == 0 || cfg.automapHudDisplay == 2));

    // Do palette shifts
    ST_doPaletteStuff(player);

    if(hud->statusbarActive || (fullscreen < 3 || hud->alpha > 0))
    {
        int viewW, viewH, x, y, width, height;
        float alpha;
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
            int w, h;
            GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPLEFT]), x, y, width, height, alpha, &drawnWidth, &drawnHeight);
            posY = y + (drawnHeight > 0 ? drawnHeight + PADDING : 0);
            GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPLEFT2]), x, posY, width, height, alpha, &w, &h);
            if(w > drawnWidth)
                drawnWidth = w;
        }
        else
        {
            drawnWidth = 0;
        }

        posX = x + (drawnWidth > 0 ? drawnWidth + PADDING : 0);
        availWidth = width - (drawnWidth > 0 ? drawnWidth + PADDING : 0);
        GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPLEFT3]), posX, y, availWidth, height, alpha, &drawnWidth, &drawnHeight);

        GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPRIGHT]), x, y, width, height, alpha, &drawnWidth, &drawnHeight);

        posY = y + (drawnHeight > 0 ? drawnHeight + PADDING : 0);
        availHeight = height - (drawnHeight > 0 ? drawnHeight + PADDING : 0);
        GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPRIGHT2]), x, posY, width, availHeight, alpha, &drawnWidth, &drawnHeight);

        if(!hud->statusbarActive)
        {
            GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOMLEFT]), x, y, width, height, alpha, &drawnWidth, &drawnHeight);
            GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOMRIGHT]), x, y, width, height, alpha, &drawnWidth, &drawnHeight);
            GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOM]), x, y, width, height, alpha, &drawnWidth, &drawnHeight);
        }
#undef PADDING
        }

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
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
