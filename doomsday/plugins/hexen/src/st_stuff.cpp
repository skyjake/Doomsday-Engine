/** @file st_stuff.cpp  Hexen specific HUD and statusbar widgets.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Activision
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

#if defined(WIN32) && defined(_MSC_VER)
// Something in here is incompatible with MSVC 2010 optimization.
// Symptom: automap not visible.
#  pragma optimize("", off)
#  pragma warning(disable : 4748)
#endif

#include "jhexen.h"
#include "st_stuff.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "d_net.h"
#include "dmu_lib.h"
#include "g_common.h"
#include "p_inventory.h"
#include "p_mapsetup.h"
#include "p_tick.h" // for Pause_IsPaused
#include "player.h"
#include "gl_drawpatch.h"
#include "hu_lib.h"
#include "am_map.h"
#include "hu_automap.h"
#include "hu_chat.h"
#include "hu_inventory.h"
#include "hu_log.h"
#include "hu_stuff.h"
#include "r_common.h"

using namespace de;

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
    UWG_STATUSBAR,
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

struct hudstate_t
{
    dd_bool inited;
    dd_bool stopped;
    int hideTics;
    float hideAmount;
    float alpha; // Fullscreen hud alpha value.
    float showBar; // Slide statusbar amount 1.0 is fully open.
    dd_bool statusbarActive; // Whether the statusbar is active.
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
};

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

static int headupDisplayMode(int /*player*/)
{
    return (cfg.common.screenBlocks < 10? 0 : cfg.common.screenBlocks - 10);
}

void Flight_Ticker(uiwidget_t *wi, timespan_t /*ticLength*/)
{
    DENG2_ASSERT(wi);
    guidata_flight_t *flht = (guidata_flight_t *)wi->typedata;
    player_t const *plr    = &players[wi->player];

    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

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

void Flight_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
    DENG2_ASSERT(wi);
    guidata_flight_t *flht = (guidata_flight_t *)wi->typedata;
    float const iconAlpha  = uiRendState->pageAlpha * cfg.common.hudIconAlpha;

    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    if(flht->patchId != 0)
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        if(offset) DGL_Translatef(offset->x, offset->y, 0);
        DGL_Scalef(cfg.common.hudScale, cfg.common.hudScale, 1);
        DGL_Enable(DGL_TEXTURE_2D);

        DGL_Color4f(1, 1, 1, iconAlpha);
        GL_DrawPatch(flht->patchId, Vector2i(16, 14));

        DGL_Disable(DGL_TEXTURE_2D);
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }
}

void Flight_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);
    //guidata_flight_t *flht = (guidata_flight_t *)wi->typedata;
    player_t const *plr = &players[wi->player];

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(!plr->powers[PT_FLIGHT]) return;

    Rect_SetWidthHeight(wi->geometry, 32 * cfg.common.hudScale, 28 * cfg.common.hudScale);
}

void Boots_Ticker(uiwidget_t *wi, timespan_t /*ticLength*/)
{
    DENG2_ASSERT(wi);
    guidata_boots_t *boots = (guidata_boots_t *)wi->typedata;

    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    player_t const *plr = &players[wi->player];

    boots->patchId = 0;
    if(plr->powers[PT_SPEED] &&
       (plr->powers[PT_SPEED] > BLINKTHRESHOLD || !(plr->powers[PT_SPEED] & 16)))
    {
        boots->patchId = pSpinSpeed[(mapTime / 3) & 15];
    }
}

void Boots_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
    DENG2_ASSERT(wi);
    guidata_boots_t *boots = (guidata_boots_t *)wi->typedata;
    float const iconAlpha  = uiRendState->pageAlpha * cfg.common.hudIconAlpha;

    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(boots->patchId == 0) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.hudScale, cfg.common.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(1, 1, 1, iconAlpha);
    GL_DrawPatch(boots->patchId, Vector2i(12, 14));

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void Boots_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);
    //guidata_boots_t *boots = (guidata_boots_t *)wi->typedata;
    player_t const *plr = &players[wi->player];

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(!plr->powers[PT_SPEED]) return;

    Rect_SetWidthHeight(wi->geometry, 24 * cfg.common.hudScale, 28 * cfg.common.hudScale);
}

void Defense_Ticker(uiwidget_t *wi, timespan_t ticLength)
{
    DENG2_ASSERT(wi);
    guidata_defense_t *dfns = (guidata_defense_t *)wi->typedata;
    player_t const *plr = &players[wi->player];

    DENG_UNUSED(ticLength);

    if(Pause_IsPaused() || !DD_IsSharpTick()) return;
    dfns->patchId = 0;
    if(!plr->powers[PT_INVULNERABILITY]) return;

    if(plr->powers[PT_INVULNERABILITY] > BLINKTHRESHOLD || !(plr->powers[PT_INVULNERABILITY] & 16))
    {
        dfns->patchId = pSpinDefense[(mapTime / 3) & 15];
    }
}

void Defense_Drawer(uiwidget_t *wi, const Point2Raw* offset)
{
    DENG2_ASSERT(wi);
    guidata_defense_t *dfns = (guidata_defense_t *)wi->typedata;
    const float iconAlpha = uiRendState->pageAlpha * cfg.common.hudIconAlpha;

    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(dfns->patchId == 0) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.hudScale, cfg.common.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(1, 1, 1, iconAlpha);
    GL_DrawPatch(dfns->patchId, Vector2i(13, 14));

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void Defense_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);
    //guidata_defense_t *dfns = (guidata_defense_t *)wi->typedata;
    player_t const *plr = &players[wi->player];

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(!plr->powers[PT_INVULNERABILITY]) return;

    Rect_SetWidthHeight(wi->geometry, 26 * cfg.common.hudScale, 28 * cfg.common.hudScale);
}

void Servant_Ticker(uiwidget_t *wi, timespan_t /*ticLength*/)
{
    DENG2_ASSERT(wi);
    guidata_servant_t *svnt = (guidata_servant_t *)wi->typedata;
    player_t const *plr = &players[wi->player];

    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    svnt->patchId = 0;
    if(!plr->powers[PT_MINOTAUR]) return;

    if(plr->powers[PT_MINOTAUR] > BLINKTHRESHOLD || !(plr->powers[PT_MINOTAUR] & 16))
    {
        svnt->patchId = pSpinMinotaur[(mapTime / 3) & 15];
    }
}

void Servant_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
    DENG2_ASSERT(wi);
    guidata_servant_t *svnt = (guidata_servant_t *)wi->typedata;
    float const iconAlpha   = uiRendState->pageAlpha * cfg.common.hudIconAlpha;

    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(svnt->patchId == 0) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.hudScale, cfg.common.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(1, 1, 1, iconAlpha);
    GL_DrawPatch(svnt->patchId, Vector2i(13, 17));

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void Servant_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);
    //guidata_servant_t *svnt = (guidata_servant_t *)wi->typedata;
    player_t const *plr = &players[wi->player];

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(!plr->powers[PT_MINOTAUR]) return;

    Rect_SetWidthHeight(wi->geometry, 26 * cfg.common.hudScale, 29 * cfg.common.hudScale);
}

void WeaponPieces_Ticker(uiwidget_t *wi, timespan_t /*ticLength*/)
{
    DENG2_ASSERT(wi);
    guidata_weaponpieces_t *wpn = (guidata_weaponpieces_t *)wi->typedata;

    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    player_t const *plr = &players[wi->player];
    wpn->pieces = plr->pieces;
}

void SBarWeaponPieces_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
#define ORIGINX (-ST_WIDTH/2)
#define ORIGINY (-ST_HEIGHT*hud->showBar)

    DENG2_ASSERT(wi);
    guidata_weaponpieces_t *wpn = (guidata_weaponpieces_t *)wi->typedata;
    hudstate_t const *hud = &hudStates[wi->player];
    int const pClass      = cfg.playerClass[wi->player]; // Original player class (i.e. not pig).
    int const fullscreen  = headupDisplayMode(wi->player);
    float const iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(wi->player) || ST_AutomapIsActive(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.statusbarScale, cfg.common.statusbarScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    if(wpn->pieces == 7)
    {
        DGL_Color4f(1, 1, 1, iconAlpha);
        GL_DrawPatch(pWeaponFull[pClass], Vector2i(ORIGINX + 190, ORIGINY));
    }
    else
    {
        if(wpn->pieces & WPIECE1)
        {
            DGL_Color4f(1, 1, 1, iconAlpha);
            GL_DrawPatch(pWeaponPiece1[pClass], Vector2i(ORIGINX + PCLASS_INFO(pClass)->pieceX[0], ORIGINY));
        }

        if(wpn->pieces & WPIECE2)
        {
            DGL_Color4f(1, 1, 1, iconAlpha);
            GL_DrawPatch(pWeaponPiece2[pClass], Vector2i(ORIGINX + PCLASS_INFO(pClass)->pieceX[1], ORIGINY));
        }

        if(wpn->pieces & WPIECE3)
        {
            DGL_Color4f(1, 1, 1, iconAlpha);
            GL_DrawPatch(pWeaponPiece3[pClass], Vector2i(ORIGINX + PCLASS_INFO(pClass)->pieceX[2], ORIGINY));
        }
    }

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef ORIGINX
#undef ORIGINY
}

void SBarWeaponPieces_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);
    //guidata_weaponpieces_t *wpn = (guidata_weaponpieces_t *)wi->typedata;

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(Hu_InventoryIsOpen(wi->player) || ST_AutomapIsActive(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    Rect_SetWidthHeight(wi->geometry, 57 * cfg.common.statusbarScale, 30 * cfg.common.statusbarScale);
}

void SBarChain_Ticker(uiwidget_t *wi, timespan_t /*ticLength*/)
{
    DENG2_ASSERT(wi);
    guidata_chain_t *chain = (guidata_chain_t *)wi->typedata;
    const player_t *plr = &players[wi->player];
    // Health marker chain animates up to the actual health value.
    int delta, curHealth = MAX_OF(plr->plr->mo->health, 0);
    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

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

void SBarChain_Drawer(uiwidget_t *wi, const Point2Raw* offset)
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

    DENG2_ASSERT(wi);
    guidata_chain_t *chain = (guidata_chain_t *)wi->typedata;
    const hudstate_t *hud = &hudStates[wi->player];
    int x, y, w, h;
    int pClass, pColor;
    float healthPos;
    int gemXOffset;
    float gemglow, rgb[3];
    int chainYOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = headupDisplayMode(wi->player);
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);
    patchinfo_t pChainInfo, pGemInfo;

    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    // Original player class (i.e. not pig).
    pClass = cfg.playerClass[wi->player];

    if(!IS_NETGAME)
    {
        pColor = 1; // Always use the red life gem (the second gem).
    }
    else
    {
        pColor = players[wi->player].colorMap; // cfg.playerColor[wi->player];
        // Flip Red/Blue.
        pColor = (pColor == 1? 0 : pColor == 0? 1 : pColor);
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
    DGL_Scalef(cfg.common.statusbarScale, cfg.common.statusbarScale, 1);
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

void SBarChain_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    Rect_SetWidthHeight(wi->geometry, (ST_WIDTH - 21 - 28) * cfg.common.statusbarScale,
                                       8 * cfg.common.statusbarScale);
}

/**
 * Draws the whole statusbar backgound.
 * \todo There is a whole lot of constants in here. What if someone wants to
 * replace the statusbar with new patches?
 */
void SBarBackground_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
#define WIDTH       (ST_WIDTH)
#define HEIGHT      (ST_HEIGHT)
#define ORIGINX     int(-WIDTH / 2)
#define ORIGINY     int(-HEIGHT * hud->showBar)

    DENG2_ASSERT(wi);
    hudstate_t const *hud = &hudStates[wi->player];
    int x, y, w, h, pClass = cfg.playerClass[wi->player]; // Original class (i.e. not pig).
    int const fullscreen  = headupDisplayMode(wi->player);
    float const iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarOpacity);
    float cw, ch;

    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.statusbarScale, cfg.common.statusbarScale, 1);

    if(!(iconAlpha < 1))
    {
        DGL_Enable(DGL_TEXTURE_2D);

        DGL_Color4f(1, 1, 1, 1);
        GL_DrawPatch(pStatusBar, Vector2i(ORIGINX, ORIGINY - 28));

        DGL_Disable(DGL_TEXTURE_2D);

        /**
         * @todo Kludge: The Hexen statusbar graphic has a chain already in the
         * image, which shows through the modified chain patches.
         * Mask out the chain on the statusbar by drawing a solid black
         * rectangle over it.
         */
        DGL_SetNoMaterial();
        DGL_DrawRectf2Color(ORIGINX+44, ORIGINY+31, 232, 7, .1f, .1f, .1f, 1);
        //// @todo Kludge: end

        DGL_Enable(DGL_TEXTURE_2D);

        DGL_Color4f(1, 1, 1, 1);
        GL_DrawPatch(pStatusBarTop, Vector2i(ORIGINX, ORIGINY - 28));

        if(!Hu_InventoryIsOpen(wi->player))
        {
            // Main interface
            if(!ST_AutomapIsActive(wi->player))
            {
                GL_DrawPatch(pStatBar, Vector2i(ORIGINX + 38, ORIGINY));

                if(G_Ruleset_Deathmatch())
                {
                    GL_DrawPatch(pKills, Vector2i(ORIGINX + 38, ORIGINY));
                }

                GL_DrawPatch(pWeaponSlot[pClass], Vector2i(ORIGINX + 190, ORIGINY));
            }
            else
            {
                GL_DrawPatch(pKeyBar, Vector2i(ORIGINX + 38, ORIGINY));
            }
        }
        else
        {
            GL_DrawPatch(pInventoryBar, Vector2i(ORIGINX + 38, ORIGINY));
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
         * @todo Kludge: The Hexen statusbar graphic has a chain already in the
         * image, which shows through the modified chain patches.
         * Mask out the chain on the statusbar by cutting a window out and
         * drawing a solid near-black rectangle to fill the hole.
         */
        DGL_DrawCutRectf2Tiled(ORIGINX+38, ORIGINY+31, 244, 8, 320, 65, 38, 192-134, ORIGINX+44, ORIGINY+31, 232, 7);
        DGL_Disable(DGL_TEXTURE_2D);
        DGL_SetNoMaterial();
        DGL_DrawRectf2Color(ORIGINX+44, ORIGINY+31, 232, 7, .1f, .1f, .1f, iconAlpha);
        DGL_Color4f(1, 1, 1, iconAlpha);
        //// @todo Kludge: end

        if(!Hu_InventoryIsOpen(wi->player))
        {
            DGL_Enable(DGL_TEXTURE_2D);

            // Main interface
            if(!ST_AutomapIsActive(wi->player))
            {
                patchinfo_t pStatBarInfo;
                if(R_GetPatchInfo(pStatBar, &pStatBarInfo))
                {
                    x = ORIGINX + (G_Ruleset_Deathmatch() ? 68 : 38);
                    y = ORIGINY;
                    w = G_Ruleset_Deathmatch()?214:244;
                    h = 31;
                    DGL_SetPatch(pStatBar, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
                    DGL_DrawCutRectf2Tiled(x, y, w, h, pStatBarInfo.geometry.size.width, pStatBarInfo.geometry.size.height, G_Ruleset_Deathmatch()?30:0, 0, ORIGINX+190, ORIGINY, 57, 30);
                }

                GL_DrawPatch(pWeaponSlot[pClass], Vector2i(ORIGINX + 190, ORIGINY));
                if(G_Ruleset_Deathmatch())
                    GL_DrawPatch(pKills, Vector2i(ORIGINX + 38, ORIGINY));
            }
            else
            {
                GL_DrawPatch(pKeyBar, Vector2i(ORIGINX + 38, ORIGINY));
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

void SBarBackground_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    Rect_SetWidthHeight(wi->geometry, ST_WIDTH  * cfg.common.statusbarScale,
                                       ST_HEIGHT * cfg.common.statusbarScale);
}

void SBarInventory_Drawer(uiwidget_t *wi, const Point2Raw* offset)
{
    DENG2_ASSERT(wi);
    const hudstate_t *hud = &hudStates[wi->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = headupDisplayMode(wi->player);
    //const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);

    if(!Hu_InventoryIsOpen(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.statusbarScale, cfg.common.statusbarScale, 1);

    Hu_InventoryDraw2(wi->player, -ST_WIDTH/2 + ST_INVENTORYX, -ST_HEIGHT + yOffset + ST_INVENTORYY, iconAlpha);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void SBarInventory_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(!Hu_InventoryIsOpen(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    // @todo calculate dimensions properly!
    Rect_SetWidthHeight(wi->geometry, (ST_WIDTH-(43*2)) * cfg.common.statusbarScale,
                                       41 * cfg.common.statusbarScale);
}

void Keys_Ticker(uiwidget_t *wi, timespan_t /*ticLength*/)
{
    DENG2_ASSERT(wi);
    guidata_keys_t *keys = (guidata_keys_t *)wi->typedata;
    const player_t *plr = &players[wi->player];
    int i;
    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    for(i = 0; i < NUM_KEY_TYPES; ++i)
    {
        keys->keyBoxes[i] = (plr->keys & (1 << i));
    }
}

void SBarKeys_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT*hud->showBar)

    DENG2_ASSERT(wi);
    guidata_keys_t *keys = (guidata_keys_t *)wi->typedata;
    hudstate_t *hud = &hudStates[wi->player];
    int i, numDrawn;
    int const fullscreen  = headupDisplayMode(wi->player);
    float const iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(wi->player) || !ST_AutomapIsActive(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.statusbarScale, cfg.common.statusbarScale, 1);

    numDrawn = 0;
    for(i = 0; i < NUM_KEY_TYPES; ++i)
    {
        patchid_t patch;

        if(!keys->keyBoxes[i]) continue;

        patch = pKeySlot[i];
        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconAlpha);
        GL_DrawPatch(patch, Vector2i(ORIGINX + 46 + numDrawn * 20, ORIGINY + 1));

        DGL_Disable(DGL_TEXTURE_2D);

        ++numDrawn;
        if(numDrawn == 5) break;
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef ORIGINY
#undef ORIGINX
}

void SBarKeys_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);
    guidata_keys_t *keys = (guidata_keys_t *)wi->typedata;
    int i, numVisible, x;
    patchinfo_t pInfo;
    patchid_t patch;

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(Hu_InventoryIsOpen(wi->player) || !ST_AutomapIsActive(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    x = 0;
    numVisible = 0;
    for(i = 0; i < NUM_KEY_TYPES; ++i)
    {
        if(!keys->keyBoxes[i]) continue;
        patch = pKeySlot[i];
        if(!R_GetPatchInfo(patch, &pInfo)) continue;

        pInfo.geometry.origin.x = x;
        pInfo.geometry.origin.y = 0;
        Rect_UniteRaw(wi->geometry, &pInfo.geometry);

        ++numVisible;
        if(numVisible == 5) break;

        x += 20;
    }

    Rect_SetWidthHeight(wi->geometry, Rect_Width(wi->geometry)  * cfg.common.statusbarScale,
                                       Rect_Height(wi->geometry) * cfg.common.statusbarScale);
}

void ArmorIcons_Ticker(uiwidget_t *wi, timespan_t /*ticLength*/)
{
    DENG2_ASSERT(wi);
    guidata_armoricons_t *icons = (guidata_armoricons_t *)wi->typedata;

    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    player_t const *plr = &players[wi->player];
    for(int i = 0; i < NUMARMOR; ++i)
    {
        icons->types[i].value = plr->armorPoints[i];
    }
}

void SBarArmorIcons_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
#define ORIGINX             (-ST_WIDTH / 2)
#define ORIGINY             (-ST_HEIGHT * hud->showBar)

    DENG2_ASSERT(wi);
    guidata_armoricons_t *icons = (guidata_armoricons_t *)wi->typedata;
    hudstate_t const *hud = &hudStates[wi->player];
    int const pClass      = cfg.playerClass[wi->player]; // Original player class (i.e. not pig).
    int const fullscreen  = headupDisplayMode(wi->player);
    float const iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(wi->player) || !ST_AutomapIsActive(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.statusbarScale, cfg.common.statusbarScale, 1);

    for(int i = 0; i < NUMARMOR; ++i)
    {
        if(!icons->types[i].value) continue;

        patchid_t patch = pArmorSlot[i];

        float alpha = 1;
        if(icons->types[i].value <= (PCLASS_INFO(pClass)->armorIncrement[i] >> 2))
            alpha = .3f;
        else if(icons->types[i].value <= (PCLASS_INFO(pClass)->armorIncrement[i] >> 1))
            alpha = .6f;

        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconAlpha * alpha);
        GL_DrawPatch(patch, Vector2i(ORIGINX + 150 + 31 * i, ORIGINY + 2));
        DGL_Disable(DGL_TEXTURE_2D);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef ORIGINX
#undef ORIGINY
}

void SBarArmorIcons_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);
    guidata_armoricons_t *icons = (guidata_armoricons_t *)wi->typedata;

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(Hu_InventoryIsOpen(wi->player) || !ST_AutomapIsActive(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    int x = 0;
    patchinfo_t pInfo;
    for(int i = 0; i < NUMARMOR; ++i, x += 31)
    {
        if(!icons->types[i].value) continue;
        if(!R_GetPatchInfo(pArmorSlot[i], &pInfo)) continue;

        pInfo.geometry.origin.x = x;
        pInfo.geometry.origin.y = 0;
        Rect_UniteRaw(wi->geometry, &pInfo.geometry);
    }

    Rect_SetWidthHeight(wi->geometry, Rect_Width(wi->geometry)  * cfg.common.statusbarScale,
                                      Rect_Height(wi->geometry) * cfg.common.statusbarScale);
}

void Frags_Ticker(uiwidget_t *wi, timespan_t /*ticLength*/)
{
    DENG2_ASSERT(wi);
    guidata_frags_t *frags = (guidata_frags_t *)wi->typedata;

    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    frags->value = 0;

    player_t const *plr = &players[wi->player];
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->inGame) continue;

        frags->value += plr->frags[i] * (i != wi->player ? 1 : -1);
    }
}

void SBarFrags_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
#define ORIGINX             (-ST_WIDTH / 2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX + ST_FRAGSX)
#define Y                   (ORIGINY + ST_FRAGSY)
#define MAXDIGITS           (ST_FRAGSWIDTH)

    DENG2_ASSERT(wi);
    guidata_frags_t *frags = (guidata_frags_t *)wi->typedata;
    hudstate_t const *hud = &hudStates[wi->player];
    int const yOffset     = ST_HEIGHT*(1-hud->showBar);
    int const fullscreen  = headupDisplayMode(wi->player);
    float const textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);
    //float const iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);

    if(!G_Ruleset_Deathmatch() || Hu_InventoryIsOpen(wi->player) || ST_AutomapIsActive(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(frags->value == 1994) return;

    char buf[20]; dd_snprintf(buf, 20, "%i", frags->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.statusbarScale, cfg.common.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(wi->font);
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

void SBarFrags_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);
    guidata_frags_t *frags = (guidata_frags_t *)wi->typedata;

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(!G_Ruleset_Deathmatch() || Hu_InventoryIsOpen(wi->player) || ST_AutomapIsActive(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(frags->value == 1994) return;

    char buf[20]; dd_snprintf(buf, 20, "%i", frags->value);

    FR_SetFont(wi->font);
    FR_SetTracking(0);
    Size2Raw textSize;
    FR_TextSize(&textSize, buf);
    Rect_SetWidthHeight(wi->geometry, textSize.width  * cfg.common.statusbarScale,
                                      textSize.height * cfg.common.statusbarScale);
}

void Health_Ticker(uiwidget_t *wi, timespan_t /*ticLength*/)
{
    DENG2_ASSERT(wi);
    guidata_health_t *hlth = (guidata_health_t *)wi->typedata;

    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    player_t const *plr = &players[wi->player];
    hlth->value = plr->health;
}

void SBarHealth_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
#define ORIGINX             (-ST_WIDTH / 2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX + ST_HEALTHX)
#define Y                   (ORIGINY + ST_HEALTHY)
#define MAXDIGITS           (ST_HEALTHWIDTH)

    DENG2_ASSERT(wi);
    guidata_health_t *hlth = (guidata_health_t *)wi->typedata;
    hudstate_t const *hud = &hudStates[wi->player];
    int const yOffset     = ST_HEIGHT * (1 - hud->showBar);
    int const fullscreen  = headupDisplayMode(wi->player);
    float const textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);
    //const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);

    if(G_Ruleset_Deathmatch() || Hu_InventoryIsOpen(wi->player) || ST_AutomapIsActive(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(hlth->value == 1994) return;

    char buf[20]; dd_snprintf(buf, 20, "%i", hlth->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.statusbarScale, cfg.common.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(wi->font);
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

void SBarHealth_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);
    guidata_health_t *hlth = (guidata_health_t *)wi->typedata;

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(G_Ruleset_Deathmatch() || Hu_InventoryIsOpen(wi->player) || ST_AutomapIsActive(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(hlth->value == 1994) return;

    char buf[20]; dd_snprintf(buf, 20, "%i", hlth->value);

    FR_SetFont(wi->font);
    FR_SetTracking(0);
    Size2Raw textSize;
    FR_TextSize(&textSize, buf);
    Rect_SetWidthHeight(wi->geometry, textSize.width  * cfg.common.statusbarScale,
                                      textSize.height * cfg.common.statusbarScale);
}

void SBarArmor_Ticker(uiwidget_t *wi, timespan_t /*ticLength*/)
{
    DENG2_ASSERT(wi);
    guidata_armor_t *armor = (guidata_armor_t *)wi->typedata;
    int const pClass = cfg.playerClass[wi->player]; // Original player class (i.e. not pig).

    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    player_t const *plr = &players[wi->player];
    armor->value = FixedDiv(
        PCLASS_INFO(pClass)->autoArmorSave + plr->armorPoints[ARMOR_ARMOR] +
        plr->armorPoints[ARMOR_SHIELD] +
        plr->armorPoints[ARMOR_HELMET] +
        plr->armorPoints[ARMOR_AMULET], 5 * FRACUNIT) >> FRACBITS;
}

void SBarArmor_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
#define ORIGINX             (-ST_WIDTH / 2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX + ST_ARMORX)
#define Y                   (ORIGINY + ST_ARMORY)
#define MAXDIGITS           (ST_ARMORWIDTH)

    DENG2_ASSERT(wi);
    guidata_armor_t *armor = (guidata_armor_t *)wi->typedata;
    hudstate_t const *hud = &hudStates[wi->player];
    int const yOffset     = ST_HEIGHT * (1 - hud->showBar);
    int const fullscreen  = headupDisplayMode(wi->player);
    float const textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(wi->player) || ST_AutomapIsActive(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(armor->value == 1994) return;

    char buf[20]; dd_snprintf(buf, 20, "%i", armor->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.statusbarScale, cfg.common.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(wi->font);
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

void SBarArmor_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);
    guidata_armor_t *armor = (guidata_armor_t *)wi->typedata;

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(Hu_InventoryIsOpen(wi->player) || ST_AutomapIsActive(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(armor->value == 1994) return;

    char buf[20]; dd_snprintf(buf, 20, "%i", armor->value);

    FR_SetFont(wi->font);
    FR_SetTracking(0);
    Size2Raw textSize;
    FR_TextSize(&textSize, buf);
    Rect_SetWidthHeight(wi->geometry, textSize.width  * cfg.common.statusbarScale,
                                       textSize.height * cfg.common.statusbarScale);
}

void BlueMana_Ticker(uiwidget_t *wi, timespan_t /*ticLength*/)
{
    DENG2_ASSERT(wi);
    guidata_bluemana_t *mana = (guidata_bluemana_t *)wi->typedata;

    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    player_t const *plr = &players[wi->player];
    mana->value = plr->ammo[AT_BLUEMANA].owned;
}

void SBarBlueMana_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
#define ORIGINX             (-ST_WIDTH / 2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX + ST_MANAAX)
#define Y                   (ORIGINY + ST_MANAAY)
#define MAXDIGITS           (ST_MANAAWIDTH)

    DENG2_ASSERT(wi);
    guidata_bluemana_t *mana = (guidata_bluemana_t *)wi->typedata;
    hudstate_t const *hud = &hudStates[wi->player];
    int const yOffset     = ST_HEIGHT * (1 - hud->showBar);
    int const fullscreen  = headupDisplayMode(wi->player);
    float const textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);

    if(mana->value <= 0 || Hu_InventoryIsOpen(wi->player) || ST_AutomapIsActive(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(mana->value == 1994) return;

    char buf[20]; dd_snprintf(buf, 20, "%i", mana->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.statusbarScale, cfg.common.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(wi->font);
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

void SBarBlueMana_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);
    guidata_bluemana_t *mana = (guidata_bluemana_t *)wi->typedata;

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(mana->value <= 0 || Hu_InventoryIsOpen(wi->player) || ST_AutomapIsActive(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(mana->value == 1994) return;

    char buf[20]; dd_snprintf(buf, 20, "%i", mana->value);

    FR_SetFont(wi->font);
    FR_SetTracking(0);
    Size2Raw textSize;
    FR_TextSize(&textSize, buf);
    Rect_SetWidthHeight(wi->geometry, textSize.width  * cfg.common.statusbarScale,
                                      textSize.height * cfg.common.statusbarScale);
}

void GreenMana_Ticker(uiwidget_t *wi, timespan_t /*ticLength*/)
{
    DENG2_ASSERT(wi);
    guidata_greenmana_t *mana = (guidata_greenmana_t *)wi->typedata;

    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    player_t const *plr = &players[wi->player];
    mana->value = plr->ammo[AT_GREENMANA].owned;
}

void SBarGreenMana_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
#define ORIGINX             (-ST_WIDTH / 2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX + ST_MANABX)
#define Y                   (ORIGINY + ST_MANABY)
#define MAXDIGITS           (ST_MANABWIDTH)

    DENG2_ASSERT(wi);
    guidata_greenmana_t *mana = (guidata_greenmana_t *)wi->typedata;
    hudstate_t const *hud = &hudStates[wi->player];
    int const yOffset     = ST_HEIGHT * (1 - hud->showBar);
    int const fullscreen  = headupDisplayMode(wi->player);
    float const textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);

    if(mana->value <= 0 || Hu_InventoryIsOpen(wi->player) || ST_AutomapIsActive(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(mana->value == 1994) return;

    char buf[20]; dd_snprintf(buf, 20, "%i", mana->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.statusbarScale, cfg.common.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(wi->font);
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

void SBarGreenMana_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);
    guidata_greenmana_t *mana = (guidata_greenmana_t *)wi->typedata;

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(mana->value <= 0 || Hu_InventoryIsOpen(wi->player) || ST_AutomapIsActive(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(mana->value == 1994) return;

    char buf[20]; dd_snprintf(buf, 20, "%i", mana->value);

    FR_SetFont(wi->font);
    FR_SetTracking(0);
    Size2Raw textSize;
    FR_TextSize(&textSize, buf);
    Rect_SetWidthHeight(wi->geometry, textSize.width  * cfg.common.statusbarScale,
                                      textSize.height * cfg.common.statusbarScale);
}

void ReadyItem_Ticker(uiwidget_t *wi, timespan_t /*ticLength*/)
{
    DENG2_ASSERT(wi);
    guidata_readyitem_t *item = (guidata_readyitem_t *)wi->typedata;
    int const flashCounter = hudStates[wi->player].readyItemFlashCounter;

    if(flashCounter > 0)
    {
        item->patchId = pInvItemFlash[flashCounter % 5];
    }
    else
    {
        inventoryitemtype_t readyItem = P_InventoryReadyItem(wi->player);
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

void SBarReadyItem_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
#define ORIGINX             (-ST_WIDTH / 2)
#define ORIGINY             (-ST_HEIGHT)

    DENG2_ASSERT(wi);
    guidata_readyitem_t *item = (guidata_readyitem_t *)wi->typedata;
    hudstate_t const *hud = &hudStates[wi->player];
    int yOffset = ST_HEIGHT * (1 - hud->showBar);
    inventoryitemtype_t readyItem;
    patchinfo_t boxInfo;
    int x, y;
    int fullscreen = headupDisplayMode(wi->player);
    float const textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);
    float const iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(wi->player) || ST_AutomapIsActive(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(item->patchId == 0) return;
    if(!R_GetPatchInfo(pInvItemBox, &boxInfo)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.statusbarScale, cfg.common.statusbarScale, 1);
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
    GL_DrawPatch(item->patchId, Vector2i(ORIGINX + x, ORIGINY + y));

    readyItem = P_InventoryReadyItem(wi->player);
    if(!(hud->readyItemFlashCounter > 0) && readyItem != IIT_NONE)
    {
        uint count = P_InventoryCount(wi->player, readyItem);
        if(count > 1)
        {
            char buf[20];
            FR_SetFont(wi->font);
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

void SBarReadyItem_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);
    guidata_readyitem_t *item = (guidata_readyitem_t *)wi->typedata;
    patchinfo_t boxInfo;

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(Hu_InventoryIsOpen(wi->player) || ST_AutomapIsActive(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(item->patchId != 0) return;
    if(!R_GetPatchInfo(pInvItemBox, &boxInfo)) return;

    Rect_SetWidthHeight(wi->geometry, boxInfo.geometry.size.width  * cfg.common.statusbarScale,
                                       boxInfo.geometry.size.height * cfg.common.statusbarScale);
}

void BlueManaIcon_Ticker(uiwidget_t *wi, timespan_t /*ticLength*/)
{
    DENG2_ASSERT(wi);
    guidata_bluemanaicon_t *icon = (guidata_bluemanaicon_t *)wi->typedata;
    player_t const *plr = &players[wi->player];

    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

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

void SBarBlueManaIcon_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
#define ORIGINX             (-ST_WIDTH / 2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX + ST_MANAAICONX)
#define Y                   (ORIGINY + ST_MANAAICONY)

    DENG2_ASSERT(wi);
    guidata_bluemanaicon_t *icon = (guidata_bluemanaicon_t *)wi->typedata;
    hudstate_t const *hud = &hudStates[wi->player];
    int const yOffset     = ST_HEIGHT*(1-hud->showBar);
    int const fullscreen  = headupDisplayMode(wi->player);
    //float const textAlpha  = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);
    float const iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(wi->player) || ST_AutomapIsActive(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    if(icon->iconIdx >= 0)
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        if(offset) DGL_Translatef(offset->x, offset->y, 0);
        DGL_Scalef(cfg.common.statusbarScale, cfg.common.statusbarScale, 1);
        DGL_Translatef(0, yOffset, 0);
        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconAlpha);

        GL_DrawPatch(pManaAIcons[icon->iconIdx], Vector2i(X, Y));

        DGL_Disable(DGL_TEXTURE_2D);
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }

#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarBlueManaIcon_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);
    guidata_bluemanaicon_t *icon = (guidata_bluemanaicon_t *)wi->typedata;
    patchinfo_t pInfo;

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(Hu_InventoryIsOpen(wi->player) || ST_AutomapIsActive(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(!R_GetPatchInfo(pManaAIcons[icon->iconIdx%2], &pInfo)) return;

    Rect_SetWidthHeight(wi->geometry, pInfo.geometry.size.width  * cfg.common.statusbarScale,
                                       pInfo.geometry.size.height * cfg.common.statusbarScale);
}

void GreenManaIcon_Ticker(uiwidget_t *wi, timespan_t /*ticLength*/)
{
    DENG2_ASSERT(wi);
    guidata_greenmanaicon_t *icon = (guidata_greenmanaicon_t *)wi->typedata;
    player_t const *plr = &players[wi->player];

    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

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

void SBarGreenManaIcon_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
#define ORIGINX             (-ST_WIDTH / 2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX + ST_MANABICONX)
#define Y                   (ORIGINY + ST_MANABICONY)

    DENG2_ASSERT(wi);
    guidata_greenmanaicon_t *icon = (guidata_greenmanaicon_t *)wi->typedata;
    hudstate_t const *hud = &hudStates[wi->player];
    int const yOffset     = ST_HEIGHT * (1 - hud->showBar);
    int const fullscreen  = headupDisplayMode(wi->player);
    float const iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(wi->player) || ST_AutomapIsActive(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    if(icon->iconIdx >= 0)
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        if(offset) DGL_Translatef(offset->x, offset->y, 0);
        DGL_Scalef(cfg.common.statusbarScale, cfg.common.statusbarScale, 1);
        DGL_Translatef(0, yOffset, 0);
        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconAlpha);

        GL_DrawPatch(pManaBIcons[icon->iconIdx], Vector2i(X, Y));

        DGL_Disable(DGL_TEXTURE_2D);
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }

#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarGreenManaIcon_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);
    guidata_greenmanaicon_t *icon = (guidata_greenmanaicon_t *)wi->typedata;
    patchinfo_t pInfo;

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(Hu_InventoryIsOpen(wi->player) || ST_AutomapIsActive(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(!R_GetPatchInfo(pManaBIcons[icon->iconIdx%2], &pInfo)) return;

    Rect_SetWidthHeight(wi->geometry, pInfo.geometry.size.width  * cfg.common.statusbarScale,
                                       pInfo.geometry.size.height * cfg.common.statusbarScale);
}

void BlueManaVial_Ticker(uiwidget_t *wi, timespan_t /*ticLength*/)
{
    DENG2_ASSERT(wi);
    guidata_bluemanavial_t *vial = (guidata_bluemanavial_t *)wi->typedata;
    player_t const *plr = &players[wi->player];

    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

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

void SBarBlueManaVial_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
#define ORIGINX             (-ST_WIDTH / 2)
#define ORIGINY             (ST_HEIGHT * (1 - hud->showBar))
#define X                   (ORIGINX + ST_MANAAVIALX)
#define Y                   (ORIGINY + ST_MANAAVIALY)
#define VIALHEIGHT          (22)

    DENG2_ASSERT(wi);
    guidata_bluemanavial_t *vial = (guidata_bluemanavial_t *)wi->typedata;
    hudstate_t const *hud = &hudStates[wi->player];
    int const fullscreen  = headupDisplayMode(wi->player);
    float const iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(wi->player) || ST_AutomapIsActive(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.statusbarScale, cfg.common.statusbarScale, 1);
    DGL_Translatef(0, ORIGINY, 0);

    if(vial->iconIdx >= 0)
    {
        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconAlpha);
        GL_DrawPatch(pManaAVials[vial->iconIdx], Vector2i(X, Y));
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

void SBarBlueManaVial_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);
    guidata_bluemanavial_t *vial = (guidata_bluemanavial_t *)wi->typedata;
    patchinfo_t pInfo;

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(Hu_InventoryIsOpen(wi->player) || ST_AutomapIsActive(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(!R_GetPatchInfo(pManaAVials[vial->iconIdx%2], &pInfo)) return;

    Rect_SetWidthHeight(wi->geometry, pInfo.geometry.size.width  * cfg.common.statusbarScale,
                                       pInfo.geometry.size.height * cfg.common.statusbarScale);
}

void GreenManaVial_Ticker(uiwidget_t *wi, timespan_t /*ticLength*/)
{
    DENG2_ASSERT(wi);
    guidata_greenmanavial_t *vial = (guidata_greenmanavial_t *)wi->typedata;
    player_t const *plr = &players[wi->player];

    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

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

void SBarGreenManaVial_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
#define ORIGINX             (-ST_WIDTH / 2)
#define ORIGINY             (ST_HEIGHT * (1 - hud->showBar))
#define X                   (ORIGINX + ST_MANABVIALX)
#define Y                   (ORIGINY + ST_MANABVIALY)
#define VIALHEIGHT          (22)

    DENG2_ASSERT(wi);
    guidata_greenmanavial_t *vial = (guidata_greenmanavial_t *)wi->typedata;
    hudstate_t const *hud = &hudStates[wi->player];
    int const fullscreen  = headupDisplayMode(wi->player);
    float const iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(wi->player) || ST_AutomapIsActive(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.statusbarScale, cfg.common.statusbarScale, 1);
    DGL_Translatef(0, ORIGINY, 0);

    if(vial->iconIdx >= 0)
    {
        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconAlpha);
        GL_DrawPatch(pManaBVials[vial->iconIdx], Vector2i(X, Y));
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

void SBarGreenManaVial_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);
    guidata_greenmanavial_t *vial = (guidata_greenmanavial_t *)wi->typedata;
    patchinfo_t pInfo;

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(Hu_InventoryIsOpen(wi->player) || ST_AutomapIsActive(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(!R_GetPatchInfo(pManaBVials[vial->iconIdx%2], &pInfo)) return;

    Rect_SetWidthHeight(wi->geometry, pInfo.geometry.size.width  * cfg.common.statusbarScale,
                                       pInfo.geometry.size.height * cfg.common.statusbarScale);
}

void ST_HUDUnHide(int player, hueevent_t ev)
{
    player_t *plr;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    if(ev < HUE_FORCE || ev > NUMHUDUNHIDEEVENTS)
    {
        DENG_ASSERT(!"ST_HUDUnHide: Invalid event type");
        return;
    }

    plr = &players[player];
    if(!plr->plr->inGame) return;

    if(ev == HUE_FORCE || cfg.hudUnHide[ev])
    {
        hudStates[player].hideTics = (cfg.common.hudTimer * TICSPERSEC);
        hudStates[player].hideAmount = 0;
    }
}

void Health_Drawer(uiwidget_t *wi, const Point2Raw* offset)
{
#define TRACKING                (1)

    DENG2_ASSERT(wi);
    guidata_health_t *hlth = (guidata_health_t *)wi->typedata;
    int value = MAX_OF(hlth->value, 0);
    const float textAlpha = uiRendState->pageAlpha * cfg.common.hudColor[3];
    char buf[20];

    if(!cfg.hudShown[HUD_HEALTH]) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(hlth->value == 1994) return;

    dd_snprintf(buf, 20, "%i", value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.hudScale, cfg.common.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(wi->font);
    FR_SetTracking(TRACKING);
    FR_SetColorAndAlpha(cfg.common.hudColor[0], cfg.common.hudColor[1], cfg.common.hudColor[2], textAlpha);
    FR_DrawTextXY(buf, -1, -1);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef TRACKING
}

void Health_UpdateGeometry(uiwidget_t *wi)
{
#define TRACKING                (1)

    DENG2_ASSERT(wi);
    guidata_health_t *hlth = (guidata_health_t *)wi->typedata;
    int value = MAX_OF(hlth->value, 0);
    Size2Raw textSize;
    char buf[20];

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(!cfg.hudShown[HUD_HEALTH]) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(hlth->value == 1994) return;

    dd_snprintf(buf, 20, "%i", value);
    FR_SetFont(wi->font);
    FR_SetTracking(TRACKING);
    FR_TextSize(&textSize, buf);
    Rect_SetWidthHeight(wi->geometry, textSize.width  * cfg.common.hudScale,
                                       textSize.height * cfg.common.hudScale);

#undef TRACKING
}

void BlueManaIcon_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
    DENG2_ASSERT(wi);
    guidata_bluemanaicon_t *icon = (guidata_bluemanaicon_t *)wi->typedata;
    float const iconAlpha = uiRendState->pageAlpha * cfg.common.hudIconAlpha;

    if(!cfg.hudShown[HUD_MANA]) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    if(icon->iconIdx >= 0)
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        if(offset) DGL_Translatef(offset->x, offset->y, 0);
        DGL_Scalef(cfg.common.hudScale, cfg.common.hudScale, 1);
        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconAlpha);

        GL_DrawPatch(pManaAIcons[icon->iconIdx], Vector2i(0, 0));

        DGL_Disable(DGL_TEXTURE_2D);
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }
}

void BlueManaIcon_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);
    guidata_bluemanaicon_t *icon = (guidata_bluemanaicon_t *)wi->typedata;
    patchinfo_t pInfo;

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(!cfg.hudShown[HUD_MANA]) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(!R_GetPatchInfo(pManaAIcons[icon->iconIdx%2], &pInfo)) return;

    Rect_SetWidthHeight(wi->geometry, pInfo.geometry.size.width  * cfg.common.hudScale,
                                       pInfo.geometry.size.height * cfg.common.hudScale);
}

void BlueMana_Drawer(uiwidget_t *wi, const Point2Raw* offset)
{
#define TRACKING                (1)

    DENG2_ASSERT(wi);
    guidata_bluemana_t *mana = (guidata_bluemana_t *)wi->typedata;
    const float textAlpha = uiRendState->pageAlpha * cfg.common.hudColor[3];
    char buf[20];

    if(!cfg.hudShown[HUD_MANA]) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(mana->value == 1994) return;

    dd_snprintf(buf, 20, "%i", mana->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.hudScale, cfg.common.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(wi->font);
    FR_SetTracking(TRACKING);
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    FR_DrawTextXY3(buf, 0, 0, ALIGN_TOPLEFT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef TRACKING
}

void BlueMana_UpdateGeometry(uiwidget_t *wi)
{
#define TRACKING                (1)

    DENG2_ASSERT(wi);
    guidata_bluemana_t *mana = (guidata_bluemana_t *)wi->typedata;
    Size2Raw textSize;
    char buf[20];

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(!cfg.hudShown[HUD_MANA]) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(mana->value == 1994) return;

    dd_snprintf(buf, 20, "%i", mana->value);
    FR_SetFont(wi->font);
    FR_SetTracking(TRACKING);
    FR_TextSize(&textSize, buf);
    Rect_SetWidthHeight(wi->geometry, textSize.width  * cfg.common.hudScale,
                                       textSize.height * cfg.common.hudScale);

#undef TRACKING
}

void GreenManaIcon_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
    DENG2_ASSERT(wi);
    guidata_greenmanaicon_t *icon = (guidata_greenmanaicon_t *)wi->typedata;
    float const iconAlpha = uiRendState->pageAlpha * cfg.common.hudIconAlpha;

    if(!cfg.hudShown[HUD_MANA]) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    if(icon->iconIdx >= 0)
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        if(offset) DGL_Translatef(offset->x, offset->y, 0);
        DGL_Scalef(cfg.common.hudScale, cfg.common.hudScale, 1);
        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconAlpha);

        GL_DrawPatch(pManaBIcons[icon->iconIdx], Vector2i(0, 0));

        DGL_Disable(DGL_TEXTURE_2D);
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }
}

void GreenManaIcon_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);
    guidata_greenmanaicon_t *icon = (guidata_greenmanaicon_t *)wi->typedata;
    patchinfo_t pInfo;

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(!cfg.hudShown[HUD_MANA]) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(!R_GetPatchInfo(pManaBIcons[icon->iconIdx%2], &pInfo)) return;

    Rect_SetWidthHeight(wi->geometry, pInfo.geometry.size.width  * cfg.common.hudScale,
                                      pInfo.geometry.size.height * cfg.common.hudScale);
}

void GreenMana_Drawer(uiwidget_t *wi, const Point2Raw* offset)
{
#define TRACKING                (1)

    DENG2_ASSERT(wi);
    guidata_greenmana_t *mana = (guidata_greenmana_t *)wi->typedata;
    float const textAlpha = uiRendState->pageAlpha * cfg.common.hudColor[3];

    if(!cfg.hudShown[HUD_MANA]) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(mana->value == 1994) return;

    char buf[20]; dd_snprintf(buf, 20, "%i", mana->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.hudScale, cfg.common.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(wi->font);
    FR_SetTracking(TRACKING);
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    FR_DrawTextXY3(buf, 0, 0, ALIGN_TOPLEFT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef TRACKING
}

void GreenMana_UpdateGeometry(uiwidget_t *wi)
{
#define TRACKING                (1)

    DENG2_ASSERT(wi);
    guidata_greenmana_t *mana = (guidata_greenmana_t *)wi->typedata;
    Size2Raw textSize;
    char buf[20];

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(!cfg.hudShown[HUD_MANA]) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(mana->value == 1994) return;

    dd_snprintf(buf, 20, "%i", mana->value);

    FR_SetFont(wi->font);
    FR_SetTracking(TRACKING);
    FR_TextSize(&textSize, buf);
    Rect_SetWidthHeight(wi->geometry, textSize.width  * cfg.common.hudScale,
                                       textSize.height * cfg.common.hudScale);

#undef TRACKING
}

void Frags_Drawer(uiwidget_t *wi, const Point2Raw* offset)
{
#define TRACKING                (1)

    DENG2_ASSERT(wi);
    guidata_frags_t *frags = (guidata_frags_t *)wi->typedata;
    const float textAlpha = uiRendState->pageAlpha * cfg.common.hudColor[3];
    char buf[20];

    if(!G_Ruleset_Deathmatch()) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(frags->value == 1994) return;

    dd_snprintf(buf, 20, "%i", frags->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.hudScale, cfg.common.hudScale, 1);

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(wi->font);
    FR_SetTracking(TRACKING);
    FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    FR_DrawTextXY3(buf, 0, -13, ALIGN_TOPLEFT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef TRACKING
}

void Frags_UpdateGeometry(uiwidget_t *wi)
{
#define TRACKING                (1)

    DENG2_ASSERT(wi);
    guidata_frags_t *frags = (guidata_frags_t *)wi->typedata;
    Size2Raw textSize;
    char buf[20];

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(!G_Ruleset_Deathmatch()) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(frags->value == 1994) return;

    dd_snprintf(buf, 20, "%i", frags->value);
    FR_SetFont(wi->font);
    FR_SetTracking(TRACKING);
    FR_TextSize(&textSize, buf);
    Rect_SetWidthHeight(wi->geometry, textSize.width  * cfg.common.hudScale,
                                       textSize.height * cfg.common.hudScale);

#undef TRACKING
}

void ReadyItem_Drawer(uiwidget_t *wi, const Point2Raw* offset)
{
    DENG2_ASSERT(wi);
    guidata_readyitem_t *item = (guidata_readyitem_t *)wi->typedata;
    const hudstate_t *hud = &hudStates[wi->player];
    const float textAlpha = uiRendState->pageAlpha * cfg.common.hudColor[3];
    const float iconAlpha = uiRendState->pageAlpha * cfg.common.hudIconAlpha;
    inventoryitemtype_t readyItem;
    int xOffset, yOffset;
    patchinfo_t boxInfo;

    if(!cfg.hudShown[HUD_READYITEM]) return;
    if(Hu_InventoryIsOpen(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(item->patchId == 0) return;
    if(!R_GetPatchInfo(pInvItemBox, &boxInfo)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.hudScale, cfg.common.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(1, 1, 1, iconAlpha/2);
    GL_DrawPatch(pInvItemBox, Vector2i(0, 0));

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
    GL_DrawPatch(item->patchId, Vector2i(xOffset, yOffset));

    readyItem = P_InventoryReadyItem(wi->player);
    if(hud->readyItemFlashCounter == 0 && readyItem != IIT_NONE)
    {
        uint count = P_InventoryCount(wi->player, readyItem);
        if(count > 1)
        {
            char buf[20];
            FR_SetFont(wi->font);
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

void ReadyItem_UpdateGeometry(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);
    patchinfo_t boxInfo;

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(!cfg.hudShown[HUD_READYITEM]) return;
    if(Hu_InventoryIsOpen(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(!R_GetPatchInfo(pInvItemBox, &boxInfo)) return;

    Rect_SetWidthHeight(wi->geometry, boxInfo.geometry.size.width  * cfg.common.hudScale,
                                       boxInfo.geometry.size.height * cfg.common.hudScale);
}

void Inventory_Drawer(uiwidget_t *wi, const Point2Raw* offset)
{
#define INVENTORY_HEIGHT    29
#define EXTRA_SCALE         .75f

    DENG2_ASSERT(wi);
    const float textAlpha = uiRendState->pageAlpha * cfg.common.hudColor[3];
    const float iconAlpha = uiRendState->pageAlpha * cfg.common.hudIconAlpha;

    if(!Hu_InventoryIsOpen(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(EXTRA_SCALE * cfg.common.hudScale, EXTRA_SCALE * cfg.common.hudScale, 1);

    Hu_InventoryDraw(wi->player, 0, -INVENTORY_HEIGHT, textAlpha, iconAlpha);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef EXTRA_SCALE
#undef INVENTORY_HEIGHT
}

void Inventory_UpdateGeometry(uiwidget_t *wi)
{
#define INVENTORY_HEIGHT    29
#define EXTRA_SCALE         .75f

    DENG2_ASSERT(wi);

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(!Hu_InventoryIsOpen(wi->player)) return;
    if(ST_AutomapIsActive(wi->player) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[wi->player].plr->mo) && Get(DD_PLAYBACK)) return;

    Rect_SetWidthHeight(wi->geometry, (31 * 7 + 16 * 2) * EXTRA_SCALE * cfg.common.hudScale,
                                       INVENTORY_HEIGHT  * EXTRA_SCALE * cfg.common.hudScale);

#undef EXTRA_SCALE
#undef INVENTORY_HEIGHT
}

void WorldTimer_Ticker(uiwidget_t *wi, timespan_t /*ticLength*/)
{
    DENG2_ASSERT(wi);
    guidata_worldtimer_t *time = (guidata_worldtimer_t *)wi->typedata;
    player_t const *plr = &players[wi->player];

    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    int wt = plr->worldTimer / TICRATE;
    time->days    = wt / 86400; wt -= time->days * 86400;
    time->hours   = wt / 3600;  wt -= time->hours * 3600;
    time->minutes = wt / 60;    wt -= time->minutes * 60;
    time->seconds = wt;
}

void WorldTimer_Drawer(uiwidget_t *wi, Point2Raw const *offset)
{
#define ORIGINX         (0)
#define ORIGINY         (0)
#define LEADING         (.5)

    DENG2_ASSERT(wi);
    guidata_worldtimer_t *time = (guidata_worldtimer_t *)wi->typedata;
    float const textAlpha = uiRendState->pageAlpha * cfg.common.hudColor[3];

    if(!ST_AutomapIsActive(wi->player)) return;

    FR_SetFont(wi->font);
    FR_SetTracking(0);
    FR_SetColorAndAlpha(1, 1, 1, textAlpha);

    int const counterWidth = FR_TextWidth("00");
    int const lineHeight   = FR_TextHeight("00");
    int const spacerWidth  = FR_TextWidth(" : ");

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.hudScale, cfg.common.hudScale, 1);

    DGL_Enable(DGL_TEXTURE_2D);

    int x = ORIGINX - counterWidth;
    int y = ORIGINY;
    char buf[20]; dd_snprintf(buf, 20, "%.2d", time->seconds);
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

void WorldTimer_UpdateGeometry(uiwidget_t *wi)
{
#define ORIGINX         (0)
#define ORIGINY         (0)
#define LEADING         (.5)

    DENG2_ASSERT(wi);
    guidata_worldtimer_t *time = (guidata_worldtimer_t *)wi->typedata;
    int counterWidth, spacerWidth, lineHeight, x, y;
    char buf[20];

    Rect_SetWidthHeight(wi->geometry, 0, 0);

    if(!ST_AutomapIsActive(wi->player)) return;

    FR_SetFont(wi->font);
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

    Rect_SetWidthHeight(wi->geometry, (x - ORIGINX) * cfg.common.hudScale,
                                       (y - ORIGINY) * cfg.common.hudScale);

#undef DRAWFLAGS
#undef LEADING
#undef ORIGINY
#undef ORIGINX
}

void ST_loadGraphics()
{
    pStatusBar    = R_DeclarePatch("H2BAR");
    pStatusBarTop = R_DeclarePatch("H2TOP");
    pInventoryBar = R_DeclarePatch("INVBAR");
    pStatBar      = R_DeclarePatch("STATBAR");
    pKeyBar       = R_DeclarePatch("KEYBAR");

    pManaAVials[0] = R_DeclarePatch("MANAVL1D");
    pManaBVials[0] = R_DeclarePatch("MANAVL2D");
    pManaAVials[1] = R_DeclarePatch("MANAVL1");
    pManaBVials[1] = R_DeclarePatch("MANAVL2");

    pManaAIcons[0] = R_DeclarePatch("MANADIM1");
    pManaBIcons[0] = R_DeclarePatch("MANADIM2");
    pManaAIcons[1] = R_DeclarePatch("MANABRT1");
    pManaBIcons[1] = R_DeclarePatch("MANABRT2");

    pKills = R_DeclarePatch("KILLS");

    char namebuf[9];
    for(int i = 0; i < NUM_KEY_TYPES; ++i)
    {
        sprintf(namebuf, "KEYSLOT%X", i + 1);
        pKeySlot[i] = R_DeclarePatch(namebuf);
    }

    for(int i = 0; i < NUMARMOR; ++i)
    {
        sprintf(namebuf, "ARMSLOT%d", i + 1);
        pArmorSlot[i] = R_DeclarePatch(namebuf);
    }

    for(int i = 0; i < 16; ++i)
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
    for(int i = 1; i < 8; ++i)
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
    for(int i = 0; i < 8; ++i)
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
    for(int i = 0; i < 8; ++i)
    {
        sprintf(namebuf, "LIFEGMM%d", i + 1);
        pLifeGem[PCLASS_MAGE][i] = R_DeclarePatch(namebuf);
    }

    // Inventory item flash anim.
    char const invItemFlashAnim[5][9] = {
        {"USEARTIA"},
        {"USEARTIB"},
        {"USEARTIC"},
        {"USEARTID"},
        {"USEARTIE"}
    };
    for(int i = 0; i < 5; ++i)
    {
        pInvItemFlash[i] = R_DeclarePatch(invItemFlashAnim[i]);
    }
}

void ST_loadData()
{
    ST_loadGraphics();
}

static void initData(hudstate_t *hud)
{
    DENG2_ASSERT(hud);
    int const player = hud - hudStates;

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
    for(int i = 0; i < NUM_KEY_TYPES; ++i)
    {
        hud->sbarKeys.keyBoxes[i] = false;
    }
    for(int i = (int)ARMOR_FIRST; i < (int)NUMARMOR; ++i)
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

    hud->log._msgCount     = 0;
    hud->log._nextUsedMsg  = 0;
    hud->log._pvisMsgCount = 0;
    std::memset(hud->log._msgs, 0, sizeof(hud->log._msgs));

    ST_HUDUnHide(player, HUE_FORCE);
}

static void setAutomapCheatLevel(uiwidget_t *wi, int level)
{
    DENG2_ASSERT(wi);
    hudstate_t *hud = &hudStates[UIWidget_Player(wi)];
    hud->automapCheatLevel = level;

    int flags = UIAutomap_Flags(wi) & ~(AMF_REND_ALLLINES|AMF_REND_THINGS|AMF_REND_SPECIALLINES|AMF_REND_VERTEXES|AMF_REND_LINE_NORMALS);
    if(hud->automapCheatLevel >= 1)
        flags |= AMF_REND_ALLLINES;
    if(hud->automapCheatLevel == 2)
        flags |= AMF_REND_THINGS | AMF_REND_SPECIALLINES;
    if(hud->automapCheatLevel > 2)
        flags |= (AMF_REND_VERTEXES | AMF_REND_LINE_NORMALS);
    UIAutomap_SetFlags(wi, flags);
}

static void initAutomapForCurrentMap(uiwidget_t *wi)
{
    DENG2_ASSERT(wi);
#ifdef __JDOOM__
    hudstate_t *hud = &hudStates[UIWidget_Player(wi)];
    automapcfg_t *mcfg;
#endif

    UIAutomap_Reset(wi);

    UIAutomap_SetMinScale(wi, 2 * PLAYERRADIUS);
    UIAutomap_SetWorldBounds(wi, *((coord_t *) DD_GetVariable(DD_MAP_MIN_X)),
                                  *((coord_t *) DD_GetVariable(DD_MAP_MAX_X)),
                                  *((coord_t *) DD_GetVariable(DD_MAP_MIN_Y)),
                                  *((coord_t *) DD_GetVariable(DD_MAP_MAX_Y)));

#if __JDOOM__
    mcfg = UIAutomap_Config(wi);
#endif

    // Determine the wi view scale factors.
    if(UIAutomap_ZoomMax(wi))
        UIAutomap_SetScale(wi, 0);

    UIAutomap_ClearPoints(wi);

#if !__JHEXEN__
    if(gameRules.skill == SM_BABY && cfg.common.automapBabyKeys)
    {
        int flags = UIAutomap_Flags(wi);
        UIAutomap_SetFlags(wi, flags|AMF_REND_KEYS);
    }
#endif

#if __JDOOM__
    if(!IS_NETGAME && hud->automapCheatLevel)
        AM_SetVectorGraphic(mcfg, AMO_THINGPLAYER, VG_CHEATARROW);
#endif

    // Are we re-centering on a followed mobj?
    mobj_t *followMobj = UIAutomap_FollowMobj(wi);
    if(followMobj)
    {
        UIAutomap_SetCameraOrigin(wi, followMobj->origin[VX], followMobj->origin[VY]);
    }

    if(IS_NETGAME)
    {
        setAutomapCheatLevel(wi, 0);
    }

    UIAutomap_SetReveal(wi, false);

    // Add all immediately visible lines.
    for(int i = 0; i < numlines; ++i)
    {
        xline_t *xline = &xlines[i];
        if(!(xline->flags & ML_MAPPED)) continue;

        P_SetLineAutomapVisibility(UIWidget_Player(wi), i, true);
    }
}

void ST_Start(int player)
{
    if(player < 0 || player >= MAXPLAYERS)
    {
        Con_Error("ST_Start: Invalid player #%i.", player);
        exit(1); // Unreachable.
    }
    hudstate_t *hud = &hudStates[player];

    if(!hud->stopped)
    {
        ST_Stop(player);
    }

    initData(hud);

    // Initialize widgets according to player preferences.

    uiwidget_t *wi = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPCENTER]);
    int flags = UIWidget_Alignment(wi);
    flags &= ~(ALIGN_LEFT|ALIGN_RIGHT);
    if(cfg.common.msgAlign == 0)
        flags |= ALIGN_LEFT;
    else if(cfg.common.msgAlign == 2)
        flags |= ALIGN_RIGHT;
    UIWidget_SetAlignment(wi, flags);

    wi = GUI_MustFindObjectById(hud->automapWidgetId);
    // If the automap was left open; close it.
    UIAutomap_Open(wi, false, true);
    initAutomapForCurrentMap(wi);
    UIAutomap_SetCameraRotation(wi, cfg.common.automapRotate);

    hud->stopped = false;
}

void ST_Stop(int player)
{
    if(player < 0 || player >= MAXPLAYERS)
        return;

    hudstate_t *hud = &hudStates[player];
    hud->stopped = true;
}

void ST_BuildWidgets(int player)
{
#define PADDING             (2) /// Units in fixed 320x200 screen space.

struct uiwidgetgroupdef_t
{
    int group;
    int alignFlags;
    order_t order;
    int groupFlags;
    int padding; // In fixed 320x200 pixels.
};

struct uiwidgetdef_t
{
    guiwidgettype_t type;
    int alignFlags;
    int group;
    gamefontid_t fontIdx;
    void (*updateGeometry) (uiwidget_t *wi);
    void (*drawer) (uiwidget_t *wi, Point2Raw const *offset);
    void (*ticker) (uiwidget_t *wi, timespan_t ticLength);
    void *typedata;
};

    hudstate_t *hud = hudStates + player;
    uiwidgetgroupdef_t const widgetGroupDefs[] = {
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
    int const widgetGroupDefCount = int(sizeof(widgetGroupDefs) / sizeof(widgetGroupDefs[0]));

    uiwidgetdef_t const widgetDefs[] = {
        { GUI_BOX,          ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_NONE,      SBarBackground_UpdateGeometry, SBarBackground_Drawer },
        { GUI_WEAPONPIECES, ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_NONE,      SBarWeaponPieces_UpdateGeometry, SBarWeaponPieces_Drawer, WeaponPieces_Ticker, &hud->sbarWeaponpieces },
        { GUI_CHAIN,        ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_NONE,      SBarChain_UpdateGeometry, SBarChain_Drawer, SBarChain_Ticker, &hud->sbarChain },
        { GUI_INVENTORY,    ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_SMALLIN,   SBarInventory_UpdateGeometry, SBarInventory_Drawer },
        { GUI_KEYS,         ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_NONE,      SBarKeys_UpdateGeometry, SBarKeys_Drawer, Keys_Ticker, &hud->sbarKeys },
        { GUI_ARMORICONS,   ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_NONE,      SBarArmorIcons_UpdateGeometry, SBarArmorIcons_Drawer, ArmorIcons_Ticker, &hud->sbarArmoricons },
        { GUI_FRAGS,        ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_STATUS,    SBarFrags_UpdateGeometry, SBarFrags_Drawer, Frags_Ticker, &hud->sbarFrags },
        { GUI_HEALTH,       ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_STATUS,    SBarHealth_UpdateGeometry, SBarHealth_Drawer, Health_Ticker, &hud->sbarHealth },
        { GUI_ARMOR,        ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_STATUS,    SBarArmor_UpdateGeometry, SBarArmor_Drawer, SBarArmor_Ticker, &hud->sbarArmor },
        { GUI_READYITEM,    ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_SMALLIN,   SBarReadyItem_UpdateGeometry, SBarReadyItem_Drawer, ReadyItem_Ticker, &hud->sbarReadyitem },
        { GUI_BLUEMANAICON, ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_NONE,      SBarBlueManaIcon_UpdateGeometry, SBarBlueManaIcon_Drawer, BlueManaIcon_Ticker, &hud->sbarBluemanaicon },
        { GUI_BLUEMANA,     ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_SMALLIN,   SBarBlueMana_UpdateGeometry, SBarBlueMana_Drawer, BlueMana_Ticker, &hud->sbarBluemana },
        { GUI_BLUEMANAVIAL, ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_NONE,      SBarBlueManaVial_UpdateGeometry, SBarBlueManaVial_Drawer, BlueManaVial_Ticker, &hud->sbarBluemanavial },
        { GUI_GREENMANAICON, ALIGN_TOPLEFT,   UWG_STATUSBAR,    GF_NONE,      SBarGreenManaIcon_UpdateGeometry, SBarGreenManaIcon_Drawer, GreenManaIcon_Ticker, &hud->sbarGreenmanaicon },
        { GUI_GREENMANA,    ALIGN_TOPLEFT,    UWG_STATUSBAR,    GF_SMALLIN,   SBarGreenMana_UpdateGeometry, SBarGreenMana_Drawer, GreenMana_Ticker, &hud->sbarGreenmana },
        { GUI_GREENMANAVIAL, ALIGN_TOPLEFT,   UWG_STATUSBAR,    GF_NONE,      SBarGreenManaVial_UpdateGeometry, SBarGreenManaVial_Drawer, GreenManaVial_Ticker, &hud->sbarGreenmanavial },
        { GUI_BLUEMANAICON, ALIGN_TOPLEFT,    UWG_TOPLEFT,      GF_NONE,      BlueManaIcon_UpdateGeometry, BlueManaIcon_Drawer, BlueManaIcon_Ticker, &hud->bluemanaicon },
        { GUI_BLUEMANA,     ALIGN_TOPLEFT,    UWG_TOPLEFT,      GF_STATUS,    BlueMana_UpdateGeometry, BlueMana_Drawer, BlueMana_Ticker, &hud->bluemana },
        { GUI_GREENMANAICON, ALIGN_TOPLEFT,   UWG_TOPLEFT2,     GF_NONE,      GreenManaIcon_UpdateGeometry, GreenManaIcon_Drawer, GreenManaIcon_Ticker, &hud->greenmanaicon },
        { GUI_GREENMANA,    ALIGN_TOPLEFT,    UWG_TOPLEFT2,     GF_STATUS,    GreenMana_UpdateGeometry, GreenMana_Drawer, GreenMana_Ticker, &hud->greenmana },
        { GUI_FLIGHT,       ALIGN_TOPLEFT,    UWG_TOPLEFT3,     GF_NONE,      Flight_UpdateGeometry, Flight_Drawer, Flight_Ticker, &hud->flight },
        { GUI_BOOTS,        ALIGN_TOPLEFT,    UWG_TOPLEFT3,     GF_NONE,      Boots_UpdateGeometry, Boots_Drawer, Boots_Ticker, &hud->boots },
        { GUI_SERVANT,      ALIGN_TOPRIGHT,   UWG_TOPRIGHT,     GF_NONE,      Servant_UpdateGeometry, Servant_Drawer, Servant_Ticker, &hud->servant },
        { GUI_DEFENSE,      ALIGN_TOPRIGHT,   UWG_TOPRIGHT,     GF_NONE,      Defense_UpdateGeometry, Defense_Drawer, Defense_Ticker, &hud->defense },
        { GUI_WORLDTIMER,   ALIGN_TOPRIGHT,   UWG_TOPRIGHT,     GF_FONTA,     WorldTimer_UpdateGeometry, WorldTimer_Drawer, WorldTimer_Ticker, &hud->worldtimer },
        { GUI_HEALTH,       ALIGN_BOTTOMLEFT, UWG_BOTTOMLEFT,   GF_FONTB,     Health_UpdateGeometry, Health_Drawer, Health_Ticker, &hud->health },
        { GUI_FRAGS,        ALIGN_BOTTOMLEFT, UWG_BOTTOMLEFT,   GF_STATUS,    Frags_UpdateGeometry, Frags_Drawer, Frags_Ticker, &hud->frags },
        { GUI_READYITEM,    ALIGN_BOTTOMRIGHT, UWG_BOTTOMRIGHT, GF_SMALLIN,   ReadyItem_UpdateGeometry, ReadyItem_Drawer, ReadyItem_Ticker, &hud->readyitem },
        { GUI_INVENTORY,    ALIGN_TOPLEFT,    UWG_BOTTOMCENTER, GF_SMALLIN,   Inventory_UpdateGeometry, Inventory_Drawer },
        { GUI_NONE }
    };

    if(player < 0 || player >= MAXPLAYERS)
    {
        Con_Error("ST_BuildWidgets: Invalid player #%i.", player);
        exit(1); // Unreachable.
    }

    for(int i = 0; i < widgetGroupDefCount; ++i)
    {
        uiwidgetgroupdef_t const *def = &widgetGroupDefs[i];
        hud->widgetGroupIds[def->group] = GUI_CreateGroup(def->groupFlags, player, def->alignFlags, def->order, def->padding);
    }

    for(int i = 0; widgetDefs[i].type != GUI_NONE; ++i)
    {
        uiwidgetdef_t const *def = &widgetDefs[i];
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

    hud->chatWidgetId = GUI_CreateWidget(GUI_CHAT, player, ALIGN_TOPLEFT, FID(GF_FONTA), 1, UIChat_UpdateGeometry, UIChat_Drawer, nullptr, &hud->chat);
    UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPCENTER]), GUI_FindObjectById(hud->chatWidgetId));

    hud->automapWidgetId = GUI_CreateWidget(GUI_AUTOMAP, player, ALIGN_TOPLEFT, FID(GF_FONTA), 1, UIAutomap_UpdateGeometry, UIAutomap_Drawer, UIAutomap_Ticker, &hud->automap);
    UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_AUTOMAP]), GUI_FindObjectById(hud->automapWidgetId));

#undef PADDING
}

void ST_Init()
{
    int i;
    ST_InitAutomapConfig();
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        hudstate_t *hud = &hudStates[i];
        ST_BuildWidgets(i);
        hud->inited = true;
    }
    ST_loadData();
}

void ST_Shutdown()
{
    int i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        hudstate_t *hud = &hudStates[i];
        hud->inited = false;
    }
}

void ST_CloseAll(int player, dd_bool fast)
{
    ST_AutomapOpen(player, false, fast);
#if __JHERETIC__ || __JHEXEN__
    Hu_InventoryOpen(player, false);
#endif
}

uiwidget_t *ST_UIChatForPlayer(int player)
{
    if(player >= 0 && player < MAXPLAYERS)
    {
        hudstate_t *hud = &hudStates[player];
        return GUI_FindObjectById(hud->chatWidgetId);
    }
    Con_Error("ST_UIChatForPlayer: Invalid player #%i.", player);
    exit(1); // Unreachable.
}

uiwidget_t *ST_UILogForPlayer(int player)
{
    if(player >= 0 && player < MAXPLAYERS)
    {
        hudstate_t *hud = &hudStates[player];
        return GUI_FindObjectById(hud->logWidgetId);
    }
    Con_Error("ST_UILogForPlayer: Invalid player #%i.", player);
    exit(1); // Unreachable.
}

uiwidget_t *ST_UIAutomapForPlayer(int player)
{
    if(player >= 0 && player < MAXPLAYERS)
    {
        hudstate_t *hud = &hudStates[player];
        return GUI_FindObjectById(hud->automapWidgetId);
    }
    Con_Error("ST_UIAutomapForPlayer: Invalid player #%i.", player);
    exit(1); // Unreachable.
}

int ST_ChatResponder(int player, event_t *ev)
{
    uiwidget_t *wi = ST_UIChatForPlayer(player);
    if(!wi) return false;
    return UIChat_Responder(wi, ev);
}

dd_bool ST_ChatIsActive(int player)
{
    uiwidget_t *wi = ST_UIChatForPlayer(player);
    if(!wi) return false;
    return UIChat_IsActive(wi);
}

void ST_LogPost(int player, byte flags, const char* msg)
{
    uiwidget_t *wi = ST_UILogForPlayer(player);
    if(!wi) return;
    UILog_Post(wi, flags, msg);
}

void ST_LogRefresh(int player)
{
    uiwidget_t *wi = ST_UILogForPlayer(player);
    if(!wi) return;
    UILog_Refresh(wi);
}

void ST_LogEmpty(int player)
{
    uiwidget_t *wi = ST_UILogForPlayer(player);
    if(!wi) return;
    UILog_Empty(wi);
}

void ST_LogPostVisibilityChangeNotification()
{
    int i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        ST_LogPost(i, LMF_NO_HIDE, !cfg.hudShown[HUD_LOG] ? MSGOFF : MSGON);
    }
}

void ST_LogUpdateAlignment()
{
    int i, flags;
    uiwidget_t *wi;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        hudstate_t *hud = &hudStates[i];
        if(!hud->inited) continue;

        wi = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPCENTER]);
        flags = UIWidget_Alignment(wi);
        flags &= ~(ALIGN_LEFT|ALIGN_RIGHT);
        if(cfg.common.msgAlign == 0)
            flags |= ALIGN_LEFT;
        else if(cfg.common.msgAlign == 2)
            flags |= ALIGN_RIGHT;
        UIWidget_SetAlignment(wi, flags);
    }
}

void ST_AutomapOpen(int player, dd_bool yes, dd_bool fast)
{
    uiwidget_t *wi = ST_UIAutomapForPlayer(player);
    if(!wi) return;
    UIAutomap_Open(wi, yes, fast);
}

dd_bool ST_AutomapIsActive(int player)
{
    uiwidget_t *wi = ST_UIAutomapForPlayer(player);
    if(!wi) return false;
    return UIAutomap_Active(wi);
}

dd_bool ST_AutomapObscures2(int player, RectRaw const * /*region*/)
{
    uiwidget_t *wi = ST_UIAutomapForPlayer(player);
    if(!wi) return false;
    if(UIAutomap_Active(wi))
    {
        if(cfg.common.automapOpacity * ST_AutomapOpacity(player) >= ST_AUTOMAP_OBSCURE_TOLERANCE)
        {
            /*if(UIAutomap_Fullscreen(wi))
            {*/
                return true;
            /*}
            else
            {
                // We'll have to compare the dimensions.
                const int scrwidth  = Get(DD_WINDOW_WIDTH);
                const int scrheight = Get(DD_WINDOW_HEIGHT);
                const Rect* rect = UIWidget_Geometry(wi);
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

dd_bool ST_AutomapObscures(int player, int x, int y, int width, int height)
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
    uiwidget_t *wi = ST_UIAutomapForPlayer(player);
    if(!wi) return;

    UIAutomap_ClearPoints(wi);
    P_SetMessage(&players[player], LMF_NO_HIDE, AMSTR_MARKSCLEARED);
}

/**
 * Adds a marker at the specified X/Y location.
 */
int ST_AutomapAddPoint(int player, coord_t x, coord_t y, coord_t z)
{
    static char buffer[20];
    uiwidget_t *wi = ST_UIAutomapForPlayer(player);
    int newPoint;
    if(!wi) return - 1;

    if(UIAutomap_PointCount(wi) == MAX_MAP_POINTS)
        return -1;

    newPoint = UIAutomap_AddPoint(wi, x, y, z);
    sprintf(buffer, "%s %d", AMSTR_MARKEDSPOT, newPoint);
    P_SetMessage(&players[player], LMF_NO_HIDE, buffer);

    return newPoint;
}

dd_bool ST_AutomapPointOrigin(int player, int point, coord_t *x, coord_t *y, coord_t *z)
{
    uiwidget_t *wi = ST_UIAutomapForPlayer(player);
    if(!wi) return false;
    return UIAutomap_PointOrigin(wi, point, x, y, z);
}

void ST_ToggleAutomapMaxZoom(int player)
{
    uiwidget_t *wi = ST_UIAutomapForPlayer(player);
    if(!wi) return;
    if(UIAutomap_SetZoomMax(wi, !UIAutomap_ZoomMax(wi)))
    {
        App_Log(0, "Maximum zoom %s in automap", UIAutomap_ZoomMax(wi)? "ON":"OFF");
    }
}

float ST_AutomapOpacity(int player)
{
    uiwidget_t *wi = ST_UIAutomapForPlayer(player);
    if(!wi) return 0;
    return UIAutomap_Opacity(wi);
}

void ST_SetAutomapCameraRotation(int player, dd_bool on)
{
    uiwidget_t *wi = ST_UIAutomapForPlayer(player);
    if(!wi) return;
    UIAutomap_SetCameraRotation(wi, on);
}

void ST_ToggleAutomapPanMode(int player)
{
    uiwidget_t *wi = ST_UIAutomapForPlayer(player);
    if(!wi) return;
    if(UIAutomap_SetPanMode(wi, !UIAutomap_PanMode(wi)))
    {
        P_SetMessage(&players[player], LMF_NO_HIDE, (UIAutomap_PanMode(wi)? AMSTR_FOLLOWOFF : AMSTR_FOLLOWON));
    }
}

void ST_CycleAutomapCheatLevel(int player)
{
    if(player >= 0 && player < MAXPLAYERS)
    {
        hudstate_t *hud = &hudStates[player];
        ST_SetAutomapCheatLevel(player, (hud->automapCheatLevel + 1) % 3);
    }
}

void ST_SetAutomapCheatLevel(int player, int level)
{
    uiwidget_t *wi = ST_UIAutomapForPlayer(player);
    if(!wi) return;
    setAutomapCheatLevel(wi, level);
}

void ST_RevealAutomap(int player, dd_bool on)
{
    uiwidget_t *wi = ST_UIAutomapForPlayer(player);
    if(!wi) return;
    UIAutomap_SetReveal(wi, on);
}

dd_bool ST_AutomapHasReveal(int player)
{
    uiwidget_t *wi = ST_UIAutomapForPlayer(player);
    if(!wi) return false;
    return UIAutomap_Reveal(wi);
}

void ST_RebuildAutomap(int player)
{
    uiwidget_t *wi = ST_UIAutomapForPlayer(player);
    if(!wi) return;
    UIAutomap_Rebuild(wi);
}

int ST_AutomapCheatLevel(int player)
{
    if(player >=0 && player < MAXPLAYERS)
        return hudStates[player].automapCheatLevel;
    return 0;
}

void ST_FlashCurrentItem(int player)
{
    player_t *plr;
    hudstate_t *hud;

    if(player < 0 || player >= MAXPLAYERS) return;

    plr = &players[player];
    if(!(/*(plr->plr->flags & DDPF_LOCAL) &&*/ plr->plr->inGame)) return;

    hud = &hudStates[player];
    hud->readyItemFlashCounter = 4;
}

int ST_Responder(event_t *ev)
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
    const dd_bool isSharpTic = DD_IsSharpTick();
    int i;

    if(isSharpTic)
        Hu_InventoryTicker();

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t *plr = &players[i];
        hudstate_t *hud = &hudStates[i];

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
            if(cfg.common.screenBlocks == 13)
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
        if(isSharpTic && !Pause_IsPaused())
        {
            if(cfg.common.hudTimer == 0)
            {
                hud->hideTics = hud->hideAmount = 0;
            }
            else
            {
                if(hud->hideTics > 0)
                    hud->hideTics--;
                if(hud->hideTics == 0 && cfg.common.hudTimer > 0 && hud->hideAmount < 1)
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

static void drawUIWidgetsForPlayer(player_t *plr)
{
/// Units in fixed 320x200 screen space.
#define DISPLAY_BORDER      (2)
#define PADDING             (2)

    const int playerNum = plr - players;
    const int displayMode = headupDisplayMode(playerNum);
    hudstate_t *hud = hudStates + playerNum;
    Size2Raw size, portSize;
    uiwidget_t *wi;
    float scale;
    assert(plr);

    R_ViewPortSize(playerNum, &portSize);

    // The automap is drawn in a viewport scaled coordinate space (of viewwindow dimensions).
    wi = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_AUTOMAP]);
    UIWidget_SetOpacity(wi, ST_AutomapOpacity(playerNum));
    UIWidget_SetMaximumSize(wi, &portSize);
    GUI_DrawWidgetXY(wi, 0, 0);

    // The rest of the UI is drawn in a fixed 320x200 coordinate space.
    // Determine scale factors.
    R_ChooseAlignModeAndScaleFactor(&scale, SCREENWIDTH, SCREENHEIGHT,
        portSize.width, portSize.height, SCALEMODE_SMART_STRETCH);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Scalef(scale, scale, 1);

    if(hud->statusbarActive || (displayMode < 3 || hud->alpha > 0))
    {
        float opacity = /**@todo Kludge: clamp*/MIN_OF(1.0f, hud->alpha)/**kludge end*/ * (1-hud->hideAmount);
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

            wi = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_STATUSBAR]);
            UIWidget_SetOpacity(wi, statusbarOpacity);
            UIWidget_SetMaximumSize(wi, &displayRegion.size);

            GUI_DrawWidget(wi, &displayRegion.origin);

            Size2_Raw(Rect_Size(UIWidget_Geometry(wi)), &drawnSize);
        }

        displayRegion.origin.x += DISPLAY_BORDER;
        displayRegion.origin.y += DISPLAY_BORDER;
        displayRegion.size.width  -= DISPLAY_BORDER*2;
        displayRegion.size.height -= DISPLAY_BORDER*2;

        if(!hud->statusbarActive)
        {
            wi = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOM]);
            UIWidget_SetOpacity(wi, opacity);
            UIWidget_SetMaximumSize(wi, &displayRegion.size);

            GUI_DrawWidget(wi, &displayRegion.origin);

            Size2_Raw(Rect_Size(UIWidget_Geometry(wi)), &drawnSize);
        }

        availHeight = displayRegion.size.height - (drawnSize.height > 0 ? drawnSize.height : 0);
        wi = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_MAPNAME]);
        UIWidget_SetOpacity(wi, ST_AutomapOpacity(playerNum));
        size.width = displayRegion.size.width; size.height = availHeight;
        UIWidget_SetMaximumSize(wi, &size);

        GUI_DrawWidget(wi, &displayRegion.origin);

        // The other displays are always visible except when using the "no-hud" mode.
        if(hud->statusbarActive || displayMode < 3)
            opacity = 1.0f;

        wi = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOP]);
        UIWidget_SetOpacity(wi, opacity);
        UIWidget_SetMaximumSize(wi, &displayRegion.size);

        GUI_DrawWidget(wi, &displayRegion.origin);

        Size2_Raw(Rect_Size(UIWidget_Geometry(wi)), &drawnSize);

        if(!hud->statusbarActive)
        {
            Size2Raw tlDrawnSize;

            wi = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPLEFT]);
            UIWidget_SetOpacity(wi, opacity);
            UIWidget_SetMaximumSize(wi, &displayRegion.size);

            GUI_DrawWidget(wi, &displayRegion.origin);

            Size2_Raw(Rect_Size(UIWidget_Geometry(wi)), &drawnSize);
            posY = displayRegion.origin.y + (drawnSize.height > 0 ? drawnSize.height + PADDING : 0);

            wi = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPLEFT2]);
            UIWidget_SetOpacity(wi, opacity);
            UIWidget_SetMaximumSize(wi, &displayRegion.size);

            GUI_DrawWidgetXY(wi, displayRegion.origin.x, posY);

            Size2_Raw(Rect_Size(UIWidget_Geometry(wi)), &tlDrawnSize);
            if(tlDrawnSize.width > drawnSize.width)
                drawnSize.width = tlDrawnSize.width;
        }
        else
        {
            drawnSize.width = 0;
        }

        posX = displayRegion.origin.x + (drawnSize.width > 0 ? drawnSize.width + PADDING : 0);
        availWidth = displayRegion.size.width - (drawnSize.width > 0 ? drawnSize.width + PADDING : 0);
        wi = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPLEFT3]);
        UIWidget_SetOpacity(wi, opacity);
        size.width = availWidth; size.height = displayRegion.size.height;
        UIWidget_SetMaximumSize(wi, &size);

        GUI_DrawWidgetXY(wi, posX, displayRegion.origin.y);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef PADDING
#undef DISPLAY_BORDER
}

void ST_Drawer(int player)
{
    hudstate_t *hud;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    if(!players[player].plr->inGame) return;

    R_UpdateViewFilter(player);

    hud = &hudStates[player];
    hud->statusbarActive = (headupDisplayMode(player) < 2) || (ST_AutomapIsActive(player) && (cfg.common.automapHudDisplay == 0 || cfg.common.automapHudDisplay == 2));

    drawUIWidgetsForPlayer(players + player);
}

dd_bool ST_StatusBarIsActive(int player)
{
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(!players[player].plr->inGame) return false;

    return hudStates[player].statusbarActive;
}

/**
 * Called when the statusbar scale cvar changes.
 */
void updateViewWindow()
{
    int i;
    R_ResizeViewWindow(RWF_FORCE);
    for(i = 0; i < MAXPLAYERS; ++i)
        ST_HUDUnHide(i, HUE_FORCE); // So the user can see the change.
}

/**
 * Called when a cvar changes that affects the look/behavior of the HUD in order to unhide it.
 */
void unhideHUD()
{
    int i;
    for(i = 0; i < MAXPLAYERS; ++i)
        ST_HUDUnHide(i, HUE_FORCE);
}

D_CMD(ChatOpen)
{
    DENG2_UNUSED(src);
    int player = CONSOLEPLAYER, destination = 0;
    uiwidget_t *wi;

    if(G_QuitInProgress())
    {
        return false;
    }

    wi = ST_UIChatForPlayer(player);
    if(!wi)
    {
        return false;
    }

    if(argc == 2)
    {
        destination = UIChat_ParseDestination(argv[1]);
        if(destination < 0)
        {
            App_Log(DE2_SCR_ERROR, "Invalid team number #%i (valid range: 0...%i)", destination, NUMTEAMS);
            return false;
        }
    }
    UIChat_SetDestination(wi, destination);
    UIChat_Activate(wi, true);
    return true;
}

D_CMD(ChatAction)
{
    DENG2_UNUSED2(argc, src);
    int player = CONSOLEPLAYER;
    char const *cmd = argv[0] + 4;
    uiwidget_t *wi;

    if(G_QuitInProgress())
    {
        return false;
    }

    wi = ST_UIChatForPlayer(player);
    if(nullptr == wi || !UIChat_IsActive(wi))
    {
        return false;
    }
    if(!stricmp(cmd, "complete")) // Send the message.
    {
        return UIChat_CommandResponder(wi, MCMD_SELECT);
    }
    else if(!stricmp(cmd, "cancel")) // Close chat.
    {
        return UIChat_CommandResponder(wi, MCMD_CLOSE);
    }
    else if(!stricmp(cmd, "delete"))
    {
        return UIChat_CommandResponder(wi, MCMD_DELETE);
    }
    return true;
}

D_CMD(ChatSendMacro)
{
    DENG2_UNUSED(src);
    int player = CONSOLEPLAYER, macroId, destination = 0;
    uiwidget_t *wi;

    if(G_QuitInProgress())
        return false;

    if(argc < 2 || argc > 3)
    {
        App_Log(DE2_SCR_NOTE, "Usage: %s (team) (macro number)", argv[0]);
        App_Log(DE2_SCR_MSG, "Send a chat macro to other player(s). "
                "If (team) is omitted, the message will be sent to all players.");
        return true;
    }

    wi = ST_UIChatForPlayer(player);
    if(!wi)
    {
        return false;
    }

    if(argc == 3)
    {
        destination = UIChat_ParseDestination(argv[1]);
        if(destination < 0)
        {
            App_Log(DE2_SCR_ERROR, "Invalid team number #%i (valid range: 0...%i)", destination, NUMTEAMS);
            return false;
        }
    }

    macroId = UIChat_ParseMacroId(argc == 3? argv[2] : argv[1]);
    if(-1 == macroId)
    {
        App_Log(DE2_SCR_ERROR, "Invalid macro id");
        return false;
    }

    UIChat_Activate(wi, true);
    UIChat_SetDestination(wi, destination);
    UIChat_LoadMacro(wi, macroId);
    UIChat_CommandResponder(wi, MCMD_SELECT);
    UIChat_Activate(wi, false);
    return true;
}

void ST_Register()
{
    C_VAR_FLOAT2( "hud-color-r",                &cfg.common.hudColor[0], 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-color-g",                &cfg.common.hudColor[1], 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-color-b",                &cfg.common.hudColor[2], 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-color-a",                &cfg.common.hudColor[3], 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-icon-alpha",             &cfg.common.hudIconAlpha, 0, 0, 1, unhideHUD )
    C_VAR_INT   ( "hud-patch-replacement",      &cfg.common.hudPatchReplaceMode, 0, 0, 1 )
    C_VAR_FLOAT2( "hud-scale",                  &cfg.common.hudScale, 0, 0.1f, 1, unhideHUD )
    C_VAR_FLOAT ( "hud-timer",                  &cfg.common.hudTimer, 0, 0, 60 )

    // Displays:
    C_VAR_BYTE2 ( "hud-currentitem",            &cfg.hudShown[HUD_READYITEM], 0, 0, 1, unhideHUD )
    C_VAR_BYTE2 ( "hud-health",                 &cfg.hudShown[HUD_HEALTH], 0, 0, 1, unhideHUD )
    C_VAR_BYTE2 ( "hud-mana",                   &cfg.hudShown[HUD_MANA], 0, 0, 1, unhideHUD )

    C_VAR_FLOAT2( "hud-status-alpha",           &cfg.common.statusbarOpacity, 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-status-icon-a",          &cfg.common.statusbarCounterAlpha, 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-status-size",            &cfg.common.statusbarScale, 0, 0.1f, 1, updateViewWindow )

    // Events:
    C_VAR_BYTE  ( "hud-unhide-damage",          &cfg.hudUnHide[HUE_ON_DAMAGE], 0, 0, 1 )
    C_VAR_BYTE  ( "hud-unhide-pickup-ammo",     &cfg.hudUnHide[HUE_ON_PICKUP_AMMO], 0, 0, 1 )
    C_VAR_BYTE  ( "hud-unhide-pickup-armor",    &cfg.hudUnHide[HUE_ON_PICKUP_ARMOR], 0, 0, 1 )
    C_VAR_BYTE  ( "hud-unhide-pickup-health",   &cfg.hudUnHide[HUE_ON_PICKUP_HEALTH], 0, 0, 1 )
    C_VAR_BYTE  ( "hud-unhide-pickup-invitem",  &cfg.hudUnHide[HUE_ON_PICKUP_INVITEM], 0, 0, 1 )
    C_VAR_BYTE  ( "hud-unhide-pickup-key",      &cfg.hudUnHide[HUE_ON_PICKUP_KEY], 0, 0, 1 )
    C_VAR_BYTE  ( "hud-unhide-pickup-powerup",  &cfg.hudUnHide[HUE_ON_PICKUP_POWER], 0, 0, 1 )
    C_VAR_BYTE  ( "hud-unhide-pickup-weapon",   &cfg.hudUnHide[HUE_ON_PICKUP_WEAPON], 0, 0, 1 )

    C_CMD("beginchat",       nullptr,   ChatOpen )
    C_CMD("chatcancel",      "",        ChatAction )
    C_CMD("chatcomplete",    "",        ChatAction )
    C_CMD("chatdelete",      "",        ChatAction )
    C_CMD("chatsendmacro",   nullptr,   ChatSendMacro )

    Hu_InventoryRegister();
}
