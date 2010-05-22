/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
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
 * st_stuff.c:
 */

// HEADER FILES ------------------------------------------------------------

#include <assert.h>

#include "jhexen.h"
#include "d_net.h"

#include "p_tick.h" // for P_IsPaused
#include "am_map.h"
#include "g_common.h"
#include "p_inventory.h"
#include "hu_lib.h"
#include "hu_inventory.h"
#include "hu_log.h"

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
    UWG_TOPRIGHT,
    UWG_TOPRIGHT2,
    NUM_UIWIDGET_GROUPS
};

typedef struct {
    boolean         inited;
    boolean         stopped;
    int             hideTics;
    float           hideAmount;

    float           showBar; // Slide statusbar amount 1.0 is fully open.
    float           alpha; // Fullscreen hud alpha value.

    boolean         statusbarActive; // Whether the statusbar is active.

    boolean         hitCenterFrame;
    int             currentInvItemFlash;
    int             armorLevel; // Current armor level.
    int             manaAIcon; // Current mana A icon index. 0 = none.
    int             manaBIcon; // Current mana B icon index. 0 = none.
    int             manaAVial; // Current mana A vial index. 0 = none.
    int             manaBVial; // Current mana B vial index. 0 = none.
    int             manaACount;
    int             manaBCount;
    int             fragsCount; // Number of frags so far in deathmatch.
    int             health;
    int             healthMarker;

    int             widgetGroupNames[NUM_UIWIDGET_GROUPS];
} hudstate_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

void ST_updateWidgets(int player);
static void updateViewWindow(cvar_t* cvar);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DECLARATIONS ------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static hudstate_t hudStates[MAXPLAYERS];

static patchinfo_t dpStatusBar;
static patchinfo_t dpStatusBarTop;
static patchinfo_t dpKills;
static patchinfo_t dpStatBar;
static patchinfo_t dpKeyBar;
static patchinfo_t dpKeySlot[NUM_KEY_TYPES];
static patchinfo_t dpArmorSlot[NUMARMOR];
static patchinfo_t dpManaAVials[2];
static patchinfo_t dpManaBVials[2];
static patchinfo_t dpManaAIcons[2];
static patchinfo_t dpManaBIcons[2];
static patchinfo_t dpInventoryBar;
static patchinfo_t dpWeaponSlot[3]; // [Fighter, Cleric, Mage]
static patchinfo_t dpWeaponFull[3]; // [Fighter, Cleric, Mage]
static patchinfo_t dpLifeGem[3][8]; // [Fighter, Cleric, Mage][color]
static patchinfo_t dpWeaponPiece1[3]; // [Fighter, Cleric, Mage]
static patchinfo_t dpWeaponPiece2[3]; // [Fighter, Cleric, Mage]
static patchinfo_t dpWeaponPiece3[3]; // [Fighter, Cleric, Mage]
static patchinfo_t dpChain[3]; // [Fighter, Cleric, Mage]
static patchinfo_t dpInvItemFlash[5];
static patchinfo_t dpSpinFly[16];
static patchinfo_t dpSpinMinotaur[16];
static patchinfo_t dpSpinSpeed[16];
static patchinfo_t dpSpinDefense[16];

// CVARs for the HUD/Statusbar
cvar_t sthudCVars[] = {
    // HUD scale
    {"hud-scale", 0, CVT_FLOAT, &cfg.hudScale, 0.1f, 1},
    {"hud-wideoffset", 0, CVT_FLOAT, &cfg.hudWideOffset, 0, 1},

    {"hud-status-size", 0, CVT_FLOAT, &cfg.statusbarScale, 0.1f, 1, updateViewWindow},

    // HUD colour + alpha
    {"hud-color-r", 0, CVT_FLOAT, &cfg.hudColor[0], 0, 1},
    {"hud-color-g", 0, CVT_FLOAT, &cfg.hudColor[1], 0, 1},
    {"hud-color-b", 0, CVT_FLOAT, &cfg.hudColor[2], 0, 1},
    {"hud-color-a", 0, CVT_FLOAT, &cfg.hudColor[3], 0, 1},
    {"hud-icon-alpha", 0, CVT_FLOAT, &cfg.hudIconAlpha, 0, 1},

    {"hud-status-alpha", 0, CVT_FLOAT, &cfg.statusbarOpacity, 0, 1},
    {"hud-status-icon-a", 0, CVT_FLOAT, &cfg.statusbarCounterAlpha, 0, 1},

    // HUD icons
    {"hud-mana", 0, CVT_BYTE, &cfg.hudShown[HUD_MANA], 0, 2},
    {"hud-health", 0, CVT_BYTE, &cfg.hudShown[HUD_HEALTH], 0, 1},
    {"hud-currentitem", 0, CVT_BYTE, &cfg.hudShown[HUD_CURRENTITEM], 0, 1},

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

void drawFlightWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];

    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;
    if(!plr->powers[PT_FLIGHT])
        return;

    if(plr->powers[PT_FLIGHT] > BLINKTHRESHOLD || !(plr->powers[PT_FLIGHT] & 16))
    {
        int frame = (mapTime / 3) & 15;
        if(plr->plr->mo->flags2 & MF2_FLY)
        {
            if(hud->hitCenterFrame && (frame != 15 && frame != 0))
                frame = 15;
            else
                hud->hitCenterFrame = false;
        }
        else
        {
            if(!hud->hitCenterFrame && (frame != 15 && frame != 0))
            {
                hud->hitCenterFrame = false;
            }
            else
            {
                frame = 15;
                hud->hitCenterFrame = true;
            }
        }
        DGL_Color4f(1, 1, 1, iconAlpha);
        M_DrawPatch(dpSpinFly[frame].id, 16, 14);
    }
    *drawnWidth = 32;
    *drawnHeight = 28;
}

void drawBootsWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    player_t* plr = &players[player];
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;
    if(!plr->powers[PT_SPEED])
        return;
    if(plr->powers[PT_SPEED] > BLINKTHRESHOLD || !(plr->powers[PT_SPEED] & 16))
    {
        DGL_Color4f(1, 1, 1, iconAlpha);
        M_DrawPatch(dpSpinSpeed[(mapTime / 3) & 15].id, 12, 14);
    }
    *drawnWidth = 24;
    *drawnHeight = 28;
}

void drawDefenseWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    player_t* plr = &players[player];
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;
    if(!plr->powers[PT_INVULNERABILITY])
        return;
    if(plr->powers[PT_INVULNERABILITY] > BLINKTHRESHOLD || !(plr->powers[PT_INVULNERABILITY] & 16))
    {
        DGL_Color4f(1, 1, 1, iconAlpha);
        M_DrawPatch(dpSpinDefense[(mapTime / 3) & 15].id, -13, 14);
    }
    *drawnWidth = 26;
    *drawnHeight = 28;
}

void drawServantWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    player_t* plr = &players[player];
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;
    if(!plr->powers[PT_MINOTAUR])
        return;
    if(plr->powers[PT_MINOTAUR] > BLINKTHRESHOLD || !(plr->powers[PT_MINOTAUR] & 16))
    {
        DGL_Color4f(1, 1, 1, iconAlpha);
        M_DrawPatch(dpSpinMinotaur[(mapTime / 3) & 15].id, -13, 17);
    }
    *drawnWidth = 26;
    *drawnHeight = 29;
}

void drawWeaponPiecesWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define ORIGINX (-ST_WIDTH/2)
#define ORIGINY (-ST_HEIGHT*hud->showBar)

    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    int pClass = cfg.playerClass[player]; // Original player class (i.e. not pig).

    if(!hud->statusbarActive || Hu_InventoryIsOpen(player) || AM_IsActive(AM_MapForPlayer(player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;

    if(plr->pieces == 7)
    {
        DGL_Color4f(1, 1, 1, iconAlpha);
        M_DrawPatch(dpWeaponFull[pClass].id, ORIGINX+190, ORIGINY);
    }
    else
    {
        if(plr->pieces & WPIECE1)
        {
            DGL_Color4f(1, 1, 1, iconAlpha);
            M_DrawPatch(dpWeaponPiece1[pClass].id, ORIGINX+PCLASS_INFO(pClass)->pieceX[0], ORIGINY);
        }

        if(plr->pieces & WPIECE2)
        {
            DGL_Color4f(1, 1, 1, iconAlpha);
            M_DrawPatch(dpWeaponPiece2[pClass].id, ORIGINX+PCLASS_INFO(pClass)->pieceX[1], ORIGINY);
        }

        if(plr->pieces & WPIECE3)
        {
            DGL_Color4f(1, 1, 1, iconAlpha);
            M_DrawPatch(dpWeaponPiece3[pClass].id, ORIGINX+PCLASS_INFO(pClass)->pieceX[2], ORIGINY);
        }
    }

    *drawnWidth = 57;
    *drawnHeight = 30;

#undef ORIGINX
#undef ORIGINY
}

void drawHealthChainWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
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

    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    int x, y, w, h;
    int pClass, pColor;
    float healthPos;
    int gemXOffset;
    float gemglow, rgb[3];
    float yOffset = ST_HEIGHT*(1-hud->showBar);

    if(!hud->statusbarActive)
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;

    // Original player class (i.e. not pig).
    pClass = cfg.playerClass[player];

    healthPos = MINMAX_OF(0, hud->healthMarker / 100.f, 100);

    if(!IS_NETGAME)
    {
        pColor = 1; // Always use the red life gem (the second gem).
    }
    else
    {
        pColor = cfg.playerColor[player];

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
    DGL_Translatef(0, yOffset, 0);

    DGL_SetPatch(dpChain[pClass].id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);

    DGL_Color4f(1, 1, 1, iconAlpha);

    gemXOffset = 7 + ROUND((w - 14) * healthPos) - dpLifeGem[pClass][pColor].width/2;

    if(gemXOffset > 0)
    {   // Left chain section.
        float cw = (float)(dpChain[pClass].width - gemXOffset) / dpChain[pClass].width;

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

    if(gemXOffset + dpLifeGem[pClass][pColor].width < w)
    {   // Right chain section.
        float cw = (w - (float)gemXOffset - dpLifeGem[pClass][pColor].width) / dpChain[pClass].width;

        DGL_Begin(DGL_QUADS);
            DGL_TexCoord2f(0, 0, 0);
            DGL_Vertex2f(x + gemXOffset + dpLifeGem[pClass][pColor].width, y);

            DGL_TexCoord2f(0, cw, 0);
            DGL_Vertex2f(x + w, y);

            DGL_TexCoord2f(0, cw, 1);
            DGL_Vertex2f(x + w, y + h);

            DGL_TexCoord2f(0, 0, 1);
            DGL_Vertex2f(x + gemXOffset + dpLifeGem[pClass][pColor].width, y + h);
        DGL_End();
    }

    // Draw the life gem.
    {
    int vX = x + MAX_OF(0, gemXOffset);
    int vWidth;
    float s1 = 0, s2 = 1;

    vWidth = dpLifeGem[pClass][pColor].width;
    if(gemXOffset + dpLifeGem[pClass][pColor].width > w)
    {
        vWidth -= gemXOffset + dpLifeGem[pClass][pColor].width - w;
        s2 = (float)vWidth / dpLifeGem[pClass][pColor].width;
    }
    if(gemXOffset < 0)
    {
        vWidth -= -gemXOffset;
        s1 = (float)(-gemXOffset) / dpLifeGem[pClass][pColor].width;
    }

    DGL_SetPatch(dpLifeGem[pClass][pColor].id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
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

    R_GetColorPaletteRGBf(0, rgb, theirColors[pColor], false);
    DGL_DrawRect(x + gemXOffset + 23, y - 6, 41, 24, rgb[0], rgb[1], rgb[2], gemglow - (1 - iconAlpha));

    DGL_BlendMode(BM_NORMAL);
    DGL_Color4f(1, 1, 1, 1);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, -yOffset, 0);

    *drawnWidth = ST_WIDTH - 21 - 28;
    *drawnHeight = 8;

#undef ORIGINX
#undef ORIGINY
}

/**
 * Draws the whole statusbar backgound.
 * \todo There is a whole lot of constants in here. What if someone wants to
 * replace the statusbar with new patches?
 */
void drawStatusBarBackground(int player, float textAlpha, float iconAlpha, int* drawnWidth, int* drawnHeight)
{
#define WIDTH (ST_WIDTH)
#define HEIGHT (ST_HEIGHT)
#define ORIGINX (-WIDTH/2)
#define ORIGINY (-HEIGHT * hud->showBar)

    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    int x, y, w, h, pClass = cfg.playerClass[player]; // Original class (i.e. not pig).
    float cw, ch;

    if(!hud->statusbarActive)
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;

    if(!(iconAlpha < 1))
    {
        DGL_Color4f(1, 1, 1, 1);
        M_DrawPatch(dpStatusBar.id, ORIGINX, ORIGINY-28);
        /**
         * \kludge The Hexen statusbar graphic has a chain already in the
         * image, which shows through the modified chain patches.
         * Mask out the chain on the statusbar by drawing a solid black
         * rectangle over it.
         */
        DGL_SetNoMaterial();
        DGL_DrawRect(ORIGINX+44, ORIGINY+31, 232, 7, .1f, .1f, .1f, 1);
        //// \kludge end
        DGL_Color4f(1, 1, 1, 1);
        M_DrawPatch(dpStatusBarTop.id, ORIGINX, ORIGINY-28);

        if(!Hu_InventoryIsOpen(player))
        {
            // Main interface
            if(!AM_IsActive(AM_MapForPlayer(player)))
            {
                M_DrawPatch(dpStatBar.id, ORIGINX+38, ORIGINY);

                if(deathmatch)
                {
                    M_DrawPatch(dpKills.id, ORIGINX+38, ORIGINY);
                }

                M_DrawPatch(dpWeaponSlot[pClass].id, ORIGINX+190, ORIGINY);
            }
            else
            {
                M_DrawPatch(dpKeyBar.id, ORIGINX+38, ORIGINY);
            }
        }
        else
        {
            M_DrawPatch(dpInventoryBar.id, ORIGINX+38, ORIGINY);
        }
    }
    else
    {
        DGL_Color4f(1, 1, 1, iconAlpha);
        DGL_SetPatch(dpStatusBar.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);

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
        DGL_SetNoMaterial();
        DGL_DrawRect(ORIGINX+44, ORIGINY+31, 232, 7, .1f, .1f, .1f, iconAlpha);
        DGL_Color4f(1, 1, 1, iconAlpha);
        //// \kludge end

        if(!Hu_InventoryIsOpen(player))
        {
            // Main interface
            if(!AM_IsActive(AM_MapForPlayer(player)))
            {
                x = ORIGINX + (deathmatch ? 68 : 38);
                y = ORIGINY;
                w = deathmatch?214:244;
                h = 31;
                DGL_SetPatch(dpStatBar.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
                DGL_DrawCutRectTiled(x, y, w, h, dpStatBar.width, dpStatBar.height, deathmatch?30:0, 0, ORIGINX+190, ORIGINY, 57, 30);

                M_DrawPatch(dpWeaponSlot[pClass].id, ORIGINX+190, ORIGINY);
                if(deathmatch)
                    M_DrawPatch(dpKills.id, ORIGINX+38, ORIGINY);
            }
            else
            {
                M_DrawPatch(dpKeyBar.id, ORIGINX+38, ORIGINY);
            }
        }
        else
        {
            // INVBAR
            DGL_SetPatch(dpInventoryBar.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
            DGL_Begin(DGL_QUADS);

            x = ORIGINX+38;
            y = ORIGINY;
            w = 244;
            h = 30;
            ch = 0.96774193548387096774193548387097f;

            DGL_TexCoord2f(0, 0, 0);
            DGL_Vertex2f(x, y);
            DGL_TexCoord2f(0, 1, 0);
            DGL_Vertex2f(x + w, y);
            DGL_TexCoord2f(0, 1, ch);
            DGL_Vertex2f(x + w, y + h);
            DGL_TexCoord2f(0, 0, ch);
            DGL_Vertex2f(x, y + h);

            DGL_End();
        }
    }

    *drawnWidth = WIDTH;
    *drawnHeight = HEIGHT;

#undef WIDTH
#undef HEIGHT
#undef ORIGINX
#undef ORIGINY
}

void ST_loadGraphics(void)
{
    char namebuf[9];
    int i;

    R_PrecachePatch("H2BAR", &dpStatusBar);
    R_PrecachePatch("H2TOP", &dpStatusBarTop);
    R_PrecachePatch("INVBAR", &dpInventoryBar);
    R_PrecachePatch("STATBAR", &dpStatBar);
    R_PrecachePatch("KEYBAR", &dpKeyBar);

    R_PrecachePatch("MANAVL1D", &dpManaAVials[0]);
    R_PrecachePatch("MANAVL2D", &dpManaBVials[0]);
    R_PrecachePatch("MANAVL1", &dpManaAVials[1]);
    R_PrecachePatch("MANAVL2", &dpManaBVials[1]);

    R_PrecachePatch("MANADIM1", &dpManaAIcons[0]);
    R_PrecachePatch("MANADIM2", &dpManaBIcons[0]);
    R_PrecachePatch("MANABRT1", &dpManaAIcons[1]);
    R_PrecachePatch("MANABRT2", &dpManaBIcons[1]);

    R_PrecachePatch("KILLS", &dpKills);

    for(i = 0; i < NUM_KEY_TYPES; ++i)
    {
        sprintf(namebuf, "KEYSLOT%X", i + 1);
        R_PrecachePatch(namebuf, &dpKeySlot[i]);
    }

    for(i = 0; i < NUMARMOR; ++i)
    {
        sprintf(namebuf, "ARMSLOT%d", i + 1);
        R_PrecachePatch(namebuf, &dpArmorSlot[i]);
    }

    for(i = 0; i < 16; ++i)
    {
        sprintf(namebuf, "SPFLY%d", i);
        R_PrecachePatch(namebuf, &dpSpinFly[i]);

        sprintf(namebuf, "SPMINO%d", i);
        R_PrecachePatch(namebuf, &dpSpinMinotaur[i]);

        sprintf(namebuf, "SPBOOT%d", i);
        R_PrecachePatch(namebuf, &dpSpinSpeed[i]);

        sprintf(namebuf, "SPSHLD%d", i);
        R_PrecachePatch(namebuf, &dpSpinDefense[i]);
    }

    // Fighter:
    R_PrecachePatch("WPIECEF1", &dpWeaponPiece1[PCLASS_FIGHTER]);
    R_PrecachePatch("WPIECEF2", &dpWeaponPiece2[PCLASS_FIGHTER]);
    R_PrecachePatch("WPIECEF3", &dpWeaponPiece3[PCLASS_FIGHTER]);
    R_PrecachePatch("CHAIN", &dpChain[PCLASS_FIGHTER]);
    R_PrecachePatch("WPSLOT0", &dpWeaponSlot[PCLASS_FIGHTER]);
    R_PrecachePatch("WPFULL0", &dpWeaponFull[PCLASS_FIGHTER]);
    R_PrecachePatch("LIFEGEM", &dpLifeGem[PCLASS_FIGHTER][0]);
    for(i = 1; i < 8; ++i)
    {
        sprintf(namebuf, "LIFEGMF%d", i + 1);
        R_PrecachePatch(namebuf, &dpLifeGem[PCLASS_FIGHTER][i]);
    }

    // Cleric:
    R_PrecachePatch("WPIECEC1", &dpWeaponPiece1[PCLASS_CLERIC]);
    R_PrecachePatch("WPIECEC2", &dpWeaponPiece2[PCLASS_CLERIC]);
    R_PrecachePatch("WPIECEC3", &dpWeaponPiece3[PCLASS_CLERIC]);
    R_PrecachePatch("CHAIN2", &dpChain[PCLASS_CLERIC]);
    R_PrecachePatch("WPSLOT1", &dpWeaponSlot[PCLASS_CLERIC]);
    R_PrecachePatch("WPFULL1", &dpWeaponFull[PCLASS_CLERIC]);
    for(i = 0; i < 8; ++i)
    {
        sprintf(namebuf, "LIFEGMC%d", i + 1);
        R_PrecachePatch(namebuf, &dpLifeGem[PCLASS_CLERIC][i]);
    }

    // Mage:
    R_PrecachePatch("WPIECEM1", &dpWeaponPiece1[PCLASS_MAGE]);
    R_PrecachePatch("WPIECEM2", &dpWeaponPiece2[PCLASS_MAGE]);
    R_PrecachePatch("WPIECEM3", &dpWeaponPiece3[PCLASS_MAGE]);
    R_PrecachePatch("CHAIN3", &dpChain[PCLASS_MAGE]);
    R_PrecachePatch("WPSLOT2", &dpWeaponSlot[PCLASS_MAGE]);
    R_PrecachePatch("WPFULL2", &dpWeaponFull[PCLASS_MAGE]);
    for(i = 0; i < 8; ++i)
    {
        sprintf(namebuf, "LIFEGMM%d", i + 1);
        R_PrecachePatch(namebuf, &dpLifeGem[PCLASS_MAGE][i]);
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
        R_PrecachePatch(invItemFlashAnim[i], &dpInvItemFlash[i]);
    }
    }
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
    // Health marker chain animates up to the actual health value.
    hud->healthMarker = 0;
    hud->showBar = 1;

    hud->health = 1994;
    hud->manaACount = 1994;
    hud->manaBCount = 1994;
    hud->fragsCount = 1994;

    ST_updateWidgets(player);

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

    hud->currentInvItemFlash = 4;
}

void ST_updateWidgets(int player)
{
    int                 i, pClass;
    hudstate_t*         hud = &hudStates[player];
    player_t*           plr = &players[player];

    // Original player class (i.e. not pig).
    pClass = cfg.playerClass[player];

    // Used by w_frags widget.
    hud->fragsCount = 0;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->inGame)
            continue;

        hud->fragsCount += plr->frags[i] * (i != player ? 1 : -1);
    }

    // Armor
    hud->armorLevel = FixedDiv(
        PCLASS_INFO(pClass)->autoArmorSave + plr->armorPoints[ARMOR_ARMOR] +
        plr->armorPoints[ARMOR_SHIELD] +
        plr->armorPoints[ARMOR_HELMET] +
        plr->armorPoints[ARMOR_AMULET], 5 * FRACUNIT) >> FRACBITS;

    // mana A
    hud->manaACount = plr->ammo[AT_BLUEMANA].owned;

    // mana B
    hud->manaBCount = plr->ammo[AT_GREENMANA].owned;

    hud->manaAIcon = hud->manaBIcon = hud->manaAVial = hud->manaBVial = -1;

    // Mana
    if(!(plr->ammo[AT_BLUEMANA].owned > 0))
        hud->manaAIcon = 0; // Draw dim Mana icon.

    if(!(plr->ammo[AT_GREENMANA].owned > 0))
        hud->manaBIcon = 0; // Draw dim Mana icon.

    // Update mana graphics based upon mana count weapon type
    if(plr->readyWeapon == WT_FIRST)
    {
        hud->manaAIcon = 0;
        hud->manaBIcon = 0;

        hud->manaAVial = 0;
        hud->manaBVial = 0;
    }
    else if(plr->readyWeapon == WT_SECOND)
    {
        // If there is mana for this weapon, make it bright!
        if(hud->manaAIcon == -1)
        {
            hud->manaAIcon = 1;
        }

        hud->manaAVial = 1;

        hud->manaBIcon = 0;
        hud->manaBVial = 0;
    }
    else if(plr->readyWeapon == WT_THIRD)
    {
        hud->manaAIcon = 0;
        hud->manaAVial = 0;

        // If there is mana for this weapon, make it bright!
        if(hud->manaBIcon == -1)
        {
            hud->manaBIcon = 1;
        }

        hud->manaBVial = 1;
    }
    else
    {
        hud->manaAVial = 1;
        hud->manaBVial = 1;

        // If there is mana for this weapon, make it bright!
        if(hud->manaAIcon == -1)
        {
            hud->manaAIcon = 1;
        }
        if(hud->manaBIcon == -1)
        {
            hud->manaBIcon = 1;
        }
    }

    hud->health = plr->health;
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
            ST_updateWidgets(i);

            if(!P_IsPaused())
            {
                int delta, curHealth;

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

                if(hud->currentInvItemFlash > 0)
                    hud->currentInvItemFlash--;

                curHealth = MAX_OF(plr->plr->mo->health, 0);
                if(curHealth < hud->healthMarker)
                {
                    delta = MINMAX_OF(1, (hud->healthMarker - curHealth) >> 2, 6);
                    hud->healthMarker -= delta;
                }
                else if(curHealth > hud->healthMarker)
                {
                    delta = MINMAX_OF(1, (curHealth - hud->healthMarker) >> 2, 6);
                    hud->healthMarker += delta;
                }
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

void drawSBarInventoryWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    float yOffset = ST_HEIGHT*(1-hud->showBar);
    if(!hud->statusbarActive || !Hu_InventoryIsOpen(player))
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;
    Hu_InventoryDraw2(player, -ST_WIDTH/2 + ST_INVENTORYX, -ST_HEIGHT + yOffset + ST_INVENTORYY, iconAlpha);

    // \fixme calculate dimensions properly!
    *drawnWidth = ST_WIDTH-(43*2);
    *drawnHeight = 41;
}

void drawKeysWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT*hud->showBar)

    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    int i, numDrawn, pClass = cfg.playerClass[player]; // Original player class (i.e. not pig).

    if(!hud->statusbarActive || Hu_InventoryIsOpen(player) || !AM_IsActive(AM_MapForPlayer(player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;

    *drawnWidth = 0;
    *drawnHeight = 0;

    numDrawn = 0;
    for(i = 0; i < NUM_KEY_TYPES; ++i)
    {
        const patchinfo_t* patch;

        if(!(plr->keys & (1 << i)))
            continue;

        patch = &dpKeySlot[i];
        DGL_Color4f(1, 1, 1, iconAlpha);
        M_DrawPatch(patch->id, ORIGINX + 46 + numDrawn * 20, ORIGINY + 1);

        *drawnWidth += patch->width;
        if(patch->height > *drawnHeight)
            *drawnHeight = patch->height;

        numDrawn++;
        if(numDrawn == 5)
            break;
    }
    if(numDrawn)
        *drawnWidth += (numDrawn-1)*20;

#undef ORIGINY
#undef ORIGINX
}

void drawSBarArmorIconsWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT*hud->showBar)

    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    int i, numDrawn, pClass = cfg.playerClass[player]; // Original player class (i.e. not pig).

    if(!hud->statusbarActive || Hu_InventoryIsOpen(player) || !AM_IsActive(AM_MapForPlayer(player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;

    *drawnWidth = 0;
    *drawnHeight = 0;

    numDrawn = 0;
    for(i = 0; i < NUMARMOR; ++i)
    {
        const patchinfo_t* patch;
        float alpha;

        if(!plr->armorPoints[i])
            continue;

        patch = &dpArmorSlot[i];
        if(plr->armorPoints[i] <= (PCLASS_INFO(pClass)->armorIncrement[i] >> 2))
            alpha = .3f;
        else if(plr->armorPoints[i] <= (PCLASS_INFO(pClass)->armorIncrement[i] >> 1))
            alpha = .6f;
        else
            alpha = 1;

        DGL_Color4f(1, 1, 1, iconAlpha * alpha);
        M_DrawPatch(patch->id, ORIGINX + 150 + 31 * i, ORIGINY + 2);

        *drawnWidth += patch->width;
        if(patch->height > *drawnHeight)
            *drawnHeight = patch->height;

        numDrawn++;
    }
    if(numDrawn)
        *drawnWidth += (numDrawn-1)*31;

#undef ORIGINX
#undef ORIGINY
}

void drawSBarFragsWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX+ST_FRAGSX)
#define Y                   (ORIGINY+ST_FRAGSY)
#define MAXDIGITS           (ST_FRAGSWIDTH)

    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    float yOffset = ST_HEIGHT*(1-hud->showBar);
    char buf[20];
    if(!hud->statusbarActive || !deathmatch || Hu_InventoryIsOpen(player) || AM_IsActive(AM_MapForPlayer(player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;
    if(hud->fragsCount == 1994)
        return;

    dd_snprintf(buf, 20, "%i", hud->fragsCount);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, yOffset, 0);

    DGL_Color4f(defFontRGB3[CR], defFontRGB3[CG], defFontRGB3[CB], textAlpha);
    GL_DrawTextFragment3(buf, X, Y, GF_STATUS, DTF_ALIGN_TOPRIGHT|DTF_NO_EFFECTS);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, -yOffset, 0);

    *drawnWidth = GL_TextFragmentWidth(buf, GF_STATUS);
    *drawnHeight = GL_TextFragmentHeight(buf, GF_STATUS);

#undef MAXDIGITS
#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void drawSBarHealthWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX+ST_HEALTHX)
#define Y                   (ORIGINY+ST_HEALTHY)
#define MAXDIGITS           (ST_HEALTHWIDTH)
#define TRACKING            (1)

    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    float yOffset = ST_HEIGHT*(1-hud->showBar);
    char buf[20];
    if(!hud->statusbarActive || deathmatch || Hu_InventoryIsOpen(player) || AM_IsActive(AM_MapForPlayer(player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;
    if(hud->health == 1994)
        return;

    dd_snprintf(buf, 20, "%i", hud->health);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, yOffset, 0);

    DGL_Color4f(defFontRGB3[CR], defFontRGB3[CG], defFontRGB3[CB], textAlpha);
    GL_DrawTextFragment4(buf, X, Y, GF_STATUS, DTF_ALIGN_TOPRIGHT|DTF_NO_EFFECTS, TRACKING);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, -yOffset, 0);

    *drawnWidth = GL_TextFragmentWidth2(buf, GF_STATUS, TRACKING);
    *drawnHeight = GL_TextFragmentHeight(buf, GF_STATUS);

#undef TRACKING
#undef MAXDIGITS
#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX

}

void drawSBarArmorWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX+ST_ARMORX)
#define Y                   (ORIGINY+ST_ARMORY)
#define MAXDIGITS           (ST_ARMORWIDTH)
#define TRACKING            (1)

    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    float yOffset = ST_HEIGHT*(1-hud->showBar);
    char buf[20];
    if(!hud->statusbarActive || Hu_InventoryIsOpen(player) || AM_IsActive(AM_MapForPlayer(player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;
    if(hud->armorLevel == 1994)
        return;

    dd_snprintf(buf, 20, "%i", hud->armorLevel);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, yOffset, 0);

    DGL_Color4f(defFontRGB3[CR], defFontRGB3[CG], defFontRGB3[CB], textAlpha);
    GL_DrawTextFragment4(buf, X, Y, GF_STATUS, DTF_ALIGN_TOPRIGHT|DTF_NO_EFFECTS, TRACKING);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, -yOffset, 0);

    *drawnWidth = GL_TextFragmentWidth2(buf, GF_STATUS, TRACKING);
    *drawnHeight = GL_TextFragmentHeight(buf, GF_STATUS);

#undef TRACKING
#undef MAXDIGITS
#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void drawSBarBlueManaWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX+ST_MANAAX)
#define Y                   (ORIGINY+ST_MANAAY)
#define MAXDIGITS           (ST_MANAAWIDTH)

    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    float yOffset = ST_HEIGHT*(1-hud->showBar);
    char buf[20];
    if(!hud->statusbarActive || hud->manaACount <= 0 || Hu_InventoryIsOpen(player) || AM_IsActive(AM_MapForPlayer(player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;
    if(hud->manaACount == 1994)
        return;

    dd_snprintf(buf, 20, "%i", hud->manaACount);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, yOffset, 0);

    DGL_Color4f(defFontRGB3[CR], defFontRGB3[CG], defFontRGB3[CB], textAlpha);
    GL_DrawTextFragment3(buf, X, Y, GF_SMALLIN, DTF_ALIGN_TOPRIGHT|DTF_NO_EFFECTS);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, -yOffset, 0);

    *drawnWidth = GL_TextFragmentWidth(buf, GF_SMALLIN);
    *drawnHeight = GL_TextFragmentHeight(buf, GF_SMALLIN);

#undef MAXDIGITS
#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void drawSBarGreenManaWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX+ST_MANABX)
#define Y                   (ORIGINY+ST_MANABY)
#define MAXDIGITS           (ST_MANABWIDTH)

    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    float yOffset = ST_HEIGHT*(1-hud->showBar);
    char buf[20];
    if(!hud->statusbarActive || hud->manaBCount <= 0 || Hu_InventoryIsOpen(player) || AM_IsActive(AM_MapForPlayer(player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;
    if(hud->manaBCount == 1994)
        return;

    dd_snprintf(buf, 20, "%i", hud->manaBCount);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, yOffset, 0);

    DGL_Color4f(defFontRGB3[CR], defFontRGB3[CG], defFontRGB3[CB], textAlpha);
    GL_DrawTextFragment3(buf, X, Y, GF_SMALLIN, DTF_ALIGN_TOPRIGHT|DTF_NO_EFFECTS);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, -yOffset, 0);

    *drawnWidth = GL_TextFragmentWidth(buf, GF_SMALLIN);
    *drawnHeight = GL_TextFragmentHeight(buf, GF_SMALLIN);

#undef MAXDIGITS
#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void drawSBarCurrentItemWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)

    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    float yOffset = ST_HEIGHT*(1-hud->showBar);
    inventoryitemtype_t readyItem;
    patchinfo_t boxInfo;
    patchid_t patch;
    int x, y;

    if(!hud->statusbarActive || Hu_InventoryIsOpen(player) || AM_IsActive(AM_MapForPlayer(player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;
    if((readyItem = P_InventoryReadyItem(player)) == IIT_NONE)
        return;
    if(!R_GetPatchInfo(dpInvItemBox, &boxInfo))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, yOffset, 0);

    if(hud->currentInvItemFlash > 0)
    {
        patch = dpInvItemFlash[hud->currentInvItemFlash % 5].id;
        x = ST_INVITEMX + 4;
        y = ST_INVITEMY;
    }
    else
    {
        patch = P_GetInvItem(readyItem-1)->patchId;
        x = ST_INVITEMX;
        y = ST_INVITEMY;
    }

    DGL_Color4f(1, 1, 1, iconAlpha);
    M_DrawPatch(patch, ORIGINX+x, ORIGINY+y);

    if(!(hud->currentInvItemFlash > 0))
    {
        uint count = P_InventoryCount(player, readyItem);
        if(count > 1)
        {
            char buf[20];
            DGL_Color4f(defFontRGB3[CR], defFontRGB3[CG], defFontRGB3[CB], textAlpha);
            dd_snprintf(buf, 20, "%i", count);
            GL_DrawTextFragment3(buf, ORIGINX+ST_INVITEMCX, ORIGINY+ST_INVITEMCY, GF_SMALLIN, DTF_ALIGN_TOPRIGHT|DTF_NO_EFFECTS);
        }
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, -yOffset, 0);

    *drawnWidth = boxInfo.width;
    *drawnHeight = boxInfo.height;

#undef ORIGINY
#undef ORIGINX
}

void drawBlueManaIconWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX+ST_MANAAICONX)
#define Y                   (ORIGINY+ST_MANAAICONY)

    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    float yOffset = ST_HEIGHT*(1-hud->showBar);

    if(!hud->statusbarActive || Hu_InventoryIsOpen(player) || AM_IsActive(AM_MapForPlayer(player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, yOffset, 0);

    if(hud->manaAIcon >= 0)
        WI_DrawPatch4(dpManaAIcons[hud->manaAIcon].id, X, Y, NULL, GF_FONTB, false, DPF_ALIGN_TOPLEFT, 1, 1, 1, iconAlpha);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, -yOffset, 0);

    *drawnWidth = dpManaAIcons[hud->manaAIcon%2].width;
    *drawnHeight = dpManaAIcons[hud->manaAIcon%2].height;

#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void drawGreenManaIconWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX+ST_MANABICONX)
#define Y                   (ORIGINY+ST_MANABICONY)

    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    float yOffset = ST_HEIGHT*(1-hud->showBar);

    if(!hud->statusbarActive || Hu_InventoryIsOpen(player) || AM_IsActive(AM_MapForPlayer(player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, yOffset, 0);

    if(hud->manaBIcon >= 0)
        WI_DrawPatch4(dpManaBIcons[hud->manaBIcon].id, X, Y, NULL, GF_FONTB, false, DPF_ALIGN_TOPLEFT, 1, 1, 1, iconAlpha);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, -yOffset, 0);

    *drawnWidth = dpManaBIcons[hud->manaBIcon%2].width;
    *drawnHeight = dpManaBIcons[hud->manaBIcon%2].height;

#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void drawBlueManaVialWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (ST_HEIGHT*(1-hud->showBar))
#define X                   (ORIGINX+ST_MANAAVIALX)
#define Y                   (ORIGINY+ST_MANAAVIALY)

    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];

    if(!hud->statusbarActive || Hu_InventoryIsOpen(player) || AM_IsActive(AM_MapForPlayer(player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, ORIGINY, 0);

    if(hud->manaAVial >= 0)
        WI_DrawPatch4(dpManaAVials[hud->manaAVial].id, X, Y, NULL, GF_FONTB, false, DPF_ALIGN_TOPLEFT, 1, 1, 1, iconAlpha);

    DGL_SetNoMaterial();
    DGL_DrawRect(ORIGINX+95, -ST_HEIGHT+3, 3, 22 - (22 * plr->ammo[AT_BLUEMANA].owned) / MAX_MANA, 0, 0, 0, iconAlpha);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, -ORIGINY, 0);

    *drawnWidth = dpManaAVials[hud->manaAVial%2].width;
    *drawnHeight = dpManaAVials[hud->manaAVial%2].height;

#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void drawGreenManaVialWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (ST_HEIGHT*(1-hud->showBar))
#define X                   (ORIGINX+ST_MANABVIALX)
#define Y                   (ORIGINY+ST_MANABVIALY)

    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];

    if(!hud->statusbarActive || Hu_InventoryIsOpen(player) || AM_IsActive(AM_MapForPlayer(player)))
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, ORIGINY, 0);

    if(hud->manaBVial >= 0)
        WI_DrawPatch4(dpManaBVials[hud->manaBVial].id, X, Y, NULL, GF_FONTB, false, DPF_ALIGN_TOPLEFT, 1, 1, 1, iconAlpha);

    DGL_SetNoMaterial();
    DGL_DrawRect(ORIGINX+103, -ST_HEIGHT+3, 3, 22 - (22 * plr->ammo[AT_GREENMANA].owned) / MAX_MANA, 0, 0, 0, iconAlpha);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, -ORIGINY, 0);

    *drawnWidth = dpManaBVials[hud->manaBVial%2].width;
    *drawnHeight = dpManaBVials[hud->manaBVial%2].height;

#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
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

static boolean pickStatusbarScalingStrategy(int viewportWidth, int viewportHeight)
{
    float a = (float)viewportWidth/viewportHeight;
    float b = (float)SCREENWIDTH/SCREENHEIGHT;

    if(INRANGE_OF(a, b, .001f))
        return true; // The same, so stretch.
    if(Con_GetByte("rend-hud-nostretch") || !INRANGE_OF(a, b, .38f))
        return false; // No stretch; translate and scale to fit.
    // Otherwise stretch.
    return true;
}

void drawHealthWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define TRACKING                (1)

    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    int health = MAX_OF(plr->plr->mo->health, 0);
    char buf[20];
    if(hud->statusbarActive)
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;

    dd_snprintf(buf, 20, "%i", health);

    DGL_Color4f(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    GL_DrawTextFragment4(buf, -1, -1, GF_FONTB, DTF_ALIGN_BOTTOMLEFT|DTF_NO_EFFECTS, TRACKING);

    *drawnWidth = GL_TextFragmentWidth2(buf, GF_FONTB, TRACKING);
    *drawnHeight = GL_TextFragmentHeight(buf, GF_FONTB);

#undef TRACKING
}

void drawBlueManaWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define TRACKING                (1)

    player_t* plr = &players[player];
    hudstate_t* hud = &hudStates[player];
    const patchinfo_t* dim =  &dpManaAIcons[0];
    const patchinfo_t* bright = &dpManaAIcons[1];
    const patchinfo_t* patch = NULL;
    char buf[20];

    if(hud->statusbarActive)
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;

    if(!(plr->ammo[AT_BLUEMANA].owned > 0))
        patch = dim;

    switch(plr->readyWeapon)
    {
    case WT_FIRST:
        patch = dim;
        break;
    case WT_SECOND:
        if(!patch)
            patch = bright;
        break;
    case WT_THIRD:
        patch = dim;
        break;
    case WT_FOURTH:
        if(!patch)
            patch = bright;
        break;
    default:
        break;
    }

    dd_snprintf(buf, 20, "%i", plr->ammo[AT_BLUEMANA].owned);

    DGL_Color4f(1, 1, 1, iconAlpha);
    M_DrawPatch(patch->id, 0, 0);

    DGL_Color4f(defFontRGB3[CR], defFontRGB3[CG], defFontRGB3[CB], textAlpha);
    GL_DrawTextFragment4(buf, patch->width+2, 0, GF_STATUS, DTF_ALIGN_TOPLEFT|DTF_NO_EFFECTS, TRACKING);

    *drawnWidth = patch->width+2+GL_TextFragmentWidth2(buf, GF_STATUS, TRACKING);
    *drawnHeight = MAX_OF(patch->height, GL_TextFragmentHeight(buf, GF_STATUS));

#undef TRACKING
}

void drawGreenManaWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define TRACKING                (1)

    player_t* plr = &players[player];
    hudstate_t* hud = &hudStates[player];
    const patchinfo_t* dim = &dpManaBIcons[0];
    const patchinfo_t* bright = &dpManaBIcons[1];
    const patchinfo_t* patch = NULL;
    char buf[20];

    if(hud->statusbarActive)
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;

    if(!(plr->ammo[AT_GREENMANA].owned > 0))
        patch = dim;

    switch(plr->readyWeapon)
    {
    case WT_FIRST:
        patch = dim;
        break;
    case WT_SECOND:
        patch = dim;
        break;
    case WT_THIRD:
        if(!patch)
            patch = bright;
        break;
    case WT_FOURTH:
        if(!patch)
            patch = bright;
        break;
    default:
        break;
    }

    dd_snprintf(buf, 20, "%i", plr->ammo[AT_GREENMANA].owned);

    DGL_Color4f(1, 1, 1, iconAlpha);
    M_DrawPatch(patch->id, 0, 0);

    DGL_Color4f(defFontRGB3[CR], defFontRGB3[CG], defFontRGB3[CB], textAlpha);
    GL_DrawTextFragment4(buf, patch->width+2, 0, GF_STATUS, DTF_ALIGN_TOPLEFT|DTF_NO_EFFECTS, TRACKING);

    *drawnWidth = patch->width+2+GL_TextFragmentWidth2(buf, GF_STATUS, TRACKING);
    *drawnHeight = MAX_OF(patch->height, GL_TextFragmentHeight(buf, GF_STATUS));

#undef TRACKING
}

void drawFragsWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define TRACKING                (1)

    player_t* plr = &players[player];
    hudstate_t* hud = &hudStates[player];
    int i, numFrags = 0;
    char buf[20];

    if(hud->statusbarActive || !deathmatch)
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;

    for(i = 0; i < MAXPLAYERS; ++i)
        if(plr->plr->inGame)
            numFrags += plr->frags[i];
    dd_snprintf(buf, 20, "%i", numFrags);

    DGL_Color4f(defFontRGB3[CR], defFontRGB3[CG], defFontRGB3[CB], textAlpha);
    GL_DrawTextFragment4(buf, 0, -13, GF_STATUS, DTF_ALIGN_TOPLEFT|DTF_NO_EFFECTS, TRACKING);

    *drawnWidth = GL_TextFragmentWidth2(buf, GF_STATUS, TRACKING);
    *drawnHeight = GL_TextFragmentHeight(buf, GF_STATUS);

#undef TRACKING
}

void drawCurrentItemWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    patchinfo_t boxInfo;

    if(hud->statusbarActive || Hu_InventoryIsOpen(player))
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;
    if(!R_GetPatchInfo(dpInvItemBox, &boxInfo))
        return;

    if(hud->currentInvItemFlash > 0)
    {
        DGL_Color4f(1, 1, 1, iconAlpha/2);
        M_DrawPatch(dpInvItemBox, -30, -30);

        DGL_Color4f(1, 1, 1, iconAlpha);
        M_DrawPatch(dpInvItemFlash[hud->currentInvItemFlash % 5].id, -27, -30);
    }
    else
    {
        inventoryitemtype_t readyItem = P_InventoryReadyItem(player);

        if(readyItem != IIT_NONE)
        {
            patchid_t patch = P_GetInvItem(readyItem-1)->patchId;
            uint count;

            DGL_Color4f(1, 1, 1, iconAlpha/2);
            M_DrawPatch(dpInvItemBox, -30, -30);

            DGL_Color4f(1, 1, 1, iconAlpha);
            M_DrawPatch(patch, -32, -31);
            if((count = P_InventoryCount(player, readyItem)) > 1)
            {
                char buf[20];
                DGL_Color4f(defFontRGB3[CR], defFontRGB3[CG], defFontRGB3[CB], textAlpha);
                dd_snprintf(buf, 20, "%i", count);
                GL_DrawTextFragment3(buf, -2, -7, GF_SMALLIN, DTF_ALIGN_TOPRIGHT|DTF_NO_EFFECTS);
            }
        }
    }
    *drawnWidth = boxInfo.width;
    *drawnHeight = boxInfo.height;
}

void drawInventoryWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define INVENTORY_HEIGHT    29

    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    if(hud->statusbarActive || !Hu_InventoryIsOpen(player))
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;
    Hu_InventoryDraw(player, 0, -INVENTORY_HEIGHT, textAlpha, iconAlpha);
    *drawnWidth = 31 * 7 + 16 * 2;
    *drawnHeight = INVENTORY_HEIGHT;

#undef INVENTORY_HEIGHT
}

void drawWorldTimerWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    int days, hours, minutes, seconds;
    int worldTimer;
    char buf[60];

    if(!AM_IsActive(AM_MapForPlayer(player)))
        return;

    worldTimer = players[player].worldTimer / TICRATE;
    days = worldTimer / 86400;
    worldTimer -= days * 86400;
    hours = worldTimer / 3600;
    worldTimer -= hours * 3600;
    minutes = worldTimer / 60;
    worldTimer -= minutes * 60;
    seconds = worldTimer;

    dd_snprintf(buf, 60, "%.2d : %.2d : %.2d", hours, minutes, seconds);

    if(days)
    {
        char buf2[30];
        dd_snprintf(buf2, 20, "\n%.2d %s", days, days == 1? "day" : "days");
        if(days >= 5)
            strncat(buf2, "\nYOU FREAK!!!", 20);
        strncat(buf, buf2, 60);
    }
    GL_DrawText(buf, 0, 0, GF_FONTA, DTF_ALIGN_TOPRIGHT|DTF_NO_EFFECTS, 0, 1, 1, 1, textAlpha, false);

    *drawnWidth = GL_TextWidth(buf, GF_FONTA);
    *drawnHeight = GL_TextHeight(buf, GF_FONTA);
}

typedef struct {
    int group;
    int id;
    float* scale;
    float extraScale;
    void (*draw) (int player, float textAlpha, float iconAlpha, int* drawnWidth, int* drawnHeight);
    float* textAlpha;
    float *iconAlpha;
} uiwidgetdef_t;

typedef struct {
    int group;
    short flags;
    int padding; // In fixed 320x200 pixels.
} uiwidgetgroupdef_t;

static int __inline toGroupName(int player, int group)
{
    return player * NUM_UIWIDGET_GROUPS + group;
}

static void drawStatusbar(int player, int x, int y, int viewW, int viewH)
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

void ST_Drawer(int player)
{
    hudstate_t* hud;
    player_t* plr;
    int fullscreenMode = (cfg.screenBlocks < 10? 0 : cfg.screenBlocks - 10);
    boolean blended = (fullscreenMode!=0);

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
            { UWG_STATUSBAR, UWGF_ALIGN_BOTTOM, 0 },
            { UWG_BOTTOMLEFT, UWGF_ALIGN_BOTTOM|UWGF_ALIGN_LEFT|UWGF_LEFT2RIGHT, PADDING },
            { UWG_BOTTOMRIGHT, UWGF_ALIGN_BOTTOM|UWGF_ALIGN_RIGHT|UWGF_RIGHT2LEFT, PADDING },
            { UWG_BOTTOM, UWGF_ALIGN_BOTTOM|UWGF_BOTTOM2TOP, PADDING },
            { UWG_TOP, UWGF_ALIGN_TOP|UWGF_ALIGN_LEFT|UWGF_TOP2BOTTOM, PADDING },
            { UWG_TOPLEFT, UWGF_ALIGN_TOP|UWGF_ALIGN_LEFT|UWGF_TOP2BOTTOM, PADDING },
            { UWG_TOPLEFT2, UWGF_ALIGN_TOP|UWGF_ALIGN_LEFT|UWGF_LEFT2RIGHT, PADDING },
            { UWG_TOPRIGHT, UWGF_ALIGN_TOP|UWGF_ALIGN_RIGHT|UWGF_RIGHT2LEFT, PADDING },
            { UWG_TOPRIGHT2, UWGF_ALIGN_TOP|UWGF_ALIGN_RIGHT|UWGF_TOP2BOTTOM, PADDING }
        };
        const uiwidgetdef_t widgetDefs[] = {
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawStatusBarBackground, &cfg.statusbarOpacity, &cfg.statusbarOpacity },
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawWeaponPiecesWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawHealthChainWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawSBarInventoryWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawKeysWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawSBarArmorIconsWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawSBarFragsWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawSBarHealthWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawSBarArmorWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawSBarCurrentItemWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawBlueManaIconWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawSBarBlueManaWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawBlueManaVialWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawGreenManaIconWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawSBarGreenManaWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawGreenManaVialWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
            { UWG_TOPLEFT, HUD_MANA, &cfg.hudScale, 1, drawBlueManaWidget },
            { UWG_TOPLEFT, HUD_MANA, &cfg.hudScale, 1, drawGreenManaWidget },
            { UWG_TOPLEFT2, -1, &cfg.hudScale, 1, drawFlightWidget, &cfg.hudColor[3], &cfg.hudIconAlpha },
            { UWG_TOPLEFT2, -1, &cfg.hudScale, 1, drawBootsWidget, &cfg.hudColor[3], &cfg.hudIconAlpha },
            { UWG_TOPRIGHT, -1, &cfg.hudScale, 1, drawServantWidget, &cfg.hudColor[3], &cfg.hudIconAlpha },
            { UWG_TOPRIGHT, -1, &cfg.hudScale, 1, drawDefenseWidget, &cfg.hudColor[3], &cfg.hudIconAlpha },
            { UWG_TOPRIGHT2, -1, &cfg.hudScale, 1, drawWorldTimerWidget, &cfg.hudColor[3], &cfg.hudIconAlpha },
            { UWG_BOTTOMLEFT, HUD_HEALTH, &cfg.hudScale, 1, drawHealthWidget, &cfg.hudColor[3], &cfg.hudIconAlpha },
            { UWG_BOTTOMLEFT, -1, &cfg.hudScale, 1, drawFragsWidget, &cfg.hudColor[3], &cfg.hudIconAlpha },
            { UWG_BOTTOMRIGHT, HUD_CURRENTITEM, &cfg.hudScale, 1, drawCurrentItemWidget, &cfg.hudColor[3], &cfg.hudIconAlpha },
            { UWG_BOTTOM, -1, &cfg.hudScale, .75f, drawInventoryWidget, &cfg.hudColor[3], &cfg.hudIconAlpha },
            { UWG_TOP, HUD_LOG, &cfg.msgScale, 1, Hu_LogDrawer, &cfg.hudColor[3], &cfg.hudIconAlpha },
            { UWG_TOP, -1, &cfg.msgScale, 1, Chat_Drawer, &cfg.hudColor[3], &cfg.hudIconAlpha }
        };
        size_t i;

        for(i = 0; i < sizeof(widgetGroupDefs)/sizeof(widgetGroupDefs[0]); ++i)
        {
            const uiwidgetgroupdef_t* def = &widgetGroupDefs[i];
            hud->widgetGroupNames[i] = GUI_CreateWidgetGroup(toGroupName(player, def->group), def->flags, def->padding);
        }

        for(i = 0; i < sizeof(widgetDefs)/sizeof(widgetDefs[0]); ++i)
        {
            const uiwidgetdef_t* def = &widgetDefs[i];
            uiwidgetid_t id = GUI_CreateWidget(player, def->id, def->scale, def->extraScale, def->draw, def->textAlpha, def->iconAlpha);
            GUI_GroupAddWidget(toGroupName(player, def->group), id);
        }

        // Initialize widgets according to player preferences.
        {
        short flags = GUI_GroupFlags(hud->widgetGroupNames[UWG_TOP]);
        flags &= ~(UWGF_ALIGN_LEFT|UWGF_ALIGN_RIGHT);
        if(cfg.msgAlign == 0)
            flags |= UWGF_ALIGN_LEFT;
        else if(cfg.msgAlign == 2)
            flags |= UWGF_ALIGN_RIGHT;
        GUI_GroupSetFlags(hud->widgetGroupNames[UWG_TOP], flags);
        }

        hud->inited = true;

#undef PADDING
    }

    hud->statusbarActive = (fullscreenMode < 2) || (AM_IsActive(AM_MapForPlayer(player)) && (cfg.automapHudDisplay == 0 || cfg.automapHudDisplay == 2));

    // Do palette shifts
    ST_doPaletteStuff(player);

    if(hud->statusbarActive || (fullscreenMode < 3 || hud->alpha > 0))
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

        GUI_DrawWidgets(hud->widgetGroupNames[UWG_STATUSBAR], (!blended? UWF_OVERRIDE_ALPHA : 0), x, y, width, height, alpha, &drawnWidth, &drawnHeight);

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
                x = (viewW/2/scale - SCREENWIDTH/2) * (1-cfg.hudWideOffset);
                width -= x*2;
            }
            else
            {
                y = (viewH/2/scale - SCREENHEIGHT/2) * (1-cfg.hudWideOffset);
                height -= y*2;
            }
        }

        x += PADDING;
        y += PADDING;
        width -= PADDING*2;
        height -= PADDING*2;

        GUI_DrawWidgets(hud->widgetGroupNames[UWG_TOP], 0, x, y, width, height, alpha, &drawnWidth, &drawnHeight);
        GUI_DrawWidgets(hud->widgetGroupNames[UWG_TOPLEFT], 0, x, y, width, height, alpha, &drawnWidth, &drawnHeight);

        posX = x + (drawnWidth > 0 ? drawnWidth + PADDING : 0);
        availWidth = width - (drawnWidth > 0 ? drawnWidth + PADDING : 0);
        GUI_DrawWidgets(hud->widgetGroupNames[UWG_TOPLEFT2],0, posX, y, availWidth, height, alpha, &drawnWidth, &drawnHeight);

        GUI_DrawWidgets(hud->widgetGroupNames[UWG_TOPRIGHT], 0, x, y, width, height, alpha, &drawnWidth, &drawnHeight);

        posY = y + (drawnHeight > 0 ? drawnHeight + PADDING : 0);
        availHeight = height - (drawnHeight > 0 ? drawnHeight + PADDING : 0);
        GUI_DrawWidgets(hud->widgetGroupNames[UWG_TOPRIGHT2], 0, x, posY, width, availHeight, alpha, &drawnWidth, &drawnHeight);

        GUI_DrawWidgets(hud->widgetGroupNames[UWG_BOTTOMLEFT], 0, x, y, width, height, alpha, &drawnWidth, &drawnHeight);
        GUI_DrawWidgets(hud->widgetGroupNames[UWG_BOTTOMRIGHT], 0, x, y, width, height, alpha, &drawnWidth, &drawnHeight);
        GUI_DrawWidgets(hud->widgetGroupNames[UWG_BOTTOM], 0, x, y, width, height, alpha, &drawnWidth, &drawnHeight);
#undef PADDING
        }

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }
}

/**
 * Called when the statusbar scale cvar changes.
 */
static void updateViewWindow(cvar_t* cvar)
{
    R_UpdateViewWindow(true);
    ST_HUDUnHide(CONSOLEPLAYER, HUE_FORCE); // So the user can see the change.
}
