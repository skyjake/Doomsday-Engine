/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * st_stuff.c: Statusbar code.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>
#include <assert.h>

#include "jheretic.h"

#include "d_net.h"
#include "st_lib.h"
#include "hu_stuff.h"
#include "p_tick.h" // for P_IsPaused
#include "p_player.h"
#include "am_map.h"
#include "p_inventory.h"

// MACROS ------------------------------------------------------------------

// How many inventory slots are visible in the fixed-size inventory.
#define NUMVISINVSLOTS      (7)

// Current ammo icon(sbbar).
#define ST_AMMOIMGWIDTH     (24)
#define ST_AMMOICONX        (111)
#define ST_AMMOICONY        (172)

// Inventory.
#define ST_INVENTORYX       (50)
#define ST_INVENTORYY       (160)
#define ST_INVENTORYHEIGHT  (30)

#define ST_INVSLOTWIDTH     (31)

// Inventory item count (relative to each slot).
#define ST_INVCOUNTOFFX     (27)
#define ST_INVCOUNTOFFY     (22)

// Current inventory item.
#define ST_INVITEMX         (179)
#define ST_INVITEMY         (160)

// Current inventory item count.
#define ST_INVITEMCWIDTH    (2) // Num digits
#define ST_INVITEMCX        (208)
#define ST_INVITEMCY        (182)

// AMMO number pos.
#define ST_AMMOWIDTH        (3)
#define ST_AMMOX            (135)
#define ST_AMMOY            (162)

// ARMOR number pos.
#define ST_ARMORWIDTH       (3)
#define ST_ARMORX           (254)
#define ST_ARMORY           (170)

// HEALTH number pos.
#define ST_HEALTHWIDTH      (3)
#define ST_HEALTHX          (85)
#define ST_HEALTHY          (170)

// Key icon positions.
#define ST_KEY0WIDTH        (10)
#define ST_KEY0HEIGHT       (6)
#define ST_KEY0X            (153)
#define ST_KEY0Y            (164)
#define ST_KEY1WIDTH        (ST_KEY0WIDTH)
#define ST_KEY1X            (153)
#define ST_KEY1Y            (172)
#define ST_KEY2WIDTH        (ST_KEY0WIDTH)
#define ST_KEY2X            (153)
#define ST_KEY2Y            (180)

// Frags pos.
#define ST_FRAGSX           (85)
#define ST_FRAGSY           (171)
#define ST_FRAGSWIDTH       (2)

// HUD Inventory Flags:
#define HIF_VISIBLE         0x1
#define HIF_IS_DIRTY        0x8

// TYPES -------------------------------------------------------------------

typedef struct {
    boolean         stopped;
    int             hideTics;
    float           hideAmount;

    float           showBar; // Slide statusbar amount 1.0 is fully open.
    float           alpha; // Fullscreen hud alpha value.

    float           statusbarCounterAlpha;
    boolean         firstTime; // ST_Start() has just been called.
    boolean         statusbarActive; // Whether left-side main status bar is active.

    boolean         hitCenterFrame;
    int             currentInvItemFlash;
    int             currentAmmoIconIdx; // Current ammo icon index.
    boolean         keyBoxes[3]; // Holds key-type for each key box on bar.
    int             fragsCount; // Number of frags so far in deathmatch.
    boolean         fragsOn; // !deathmatch.
    boolean         blended; // Whether to use alpha blending.

    int             tomePlay;
    int             healthMarker;
    int             chainWiggle;

    int             oldAmmoIconIdx;
    int             oldReadyWeapon;
    int             oldHealth;

    struct {
        byte            flags; // HIF_* flags
        int             hideTics;
        uint            numOwnedItemTypes;

        uint            slots[NUM_INVENTORYITEM_TYPES - 1];
        uint            numUsedSlots;
        uint            selected;

        uint            varCursorPos; // Variable-range, fullscreen inv cursor.
        uint            fixedCursorPos; // Fixed-range, statusbar inv cursor.
    } inventory;

    // Widgets:
    st_multicon_t   wCurrentAmmoIcon; // Current ammo icon.
    st_number_t     wReadyWeapon; // Ready-weapon.
    st_number_t     wFrags; // In deathmatch only, summary of frags stats.
    st_number_t     wHealth; // Health.
    st_number_t     wArmor; // Armor.
    st_binicon_t    wKeyBoxes[3]; // Owned keys.
} hudstate_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void        ST_ResizeInventory(struct cvar_s* cvar);

// Console commands for the HUD/Statusbar.
DEFCC(CCmdStatusBarSize);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void inventoryIndexes(const player_t* plr, const hudstate_t* hud,
                             uint maxVisSlots, int origCursor,
                             uint* firstVisible, uint* cursorPos,
                             uint* startSlot, uint* endSlot);

static void drawINumber(signed int val, int x, int y, float r, float g, float b, float a);
static void drawBNumber(signed int val, int x, int y, float Red, float Green, float Blue, float Alpha);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static hudstate_t hudStates[MAXPLAYERS];

static dpatch_t statusbar;
static dpatch_t statusbarTopLeft;
static dpatch_t statusbarTopRight;
static dpatch_t chain;
static dpatch_t statBar;
static dpatch_t lifeBar;
static dpatch_t invBar;
static dpatch_t lifeGems[4];
static dpatch_t invSelectBox;
static dpatch_t invPageLeft[2];
static dpatch_t invPageRight[2];
static dpatch_t iNumbers[10];
static dpatch_t sNumbers[10];
static dpatch_t negative;
static dpatch_t ammoIcons[11];
static dpatch_t dpInvItemFlash[5];
static dpatch_t dpInvItemBox;
static dpatch_t spinBook[16];
static dpatch_t spinFly[16];
static dpatch_t keys[NUM_KEY_TYPES];
static dpatch_t godLeft;
static dpatch_t godRight;
static dpatch_t lame;

// CVARs for the HUD/Statusbar
cvar_t sthudCVars[] = {
    // HUD scale
    {"hud-scale", 0, CVT_FLOAT, &cfg.hudScale, 0.1f, 10},

    {"hud-status-size", CVF_PROTECTED, CVT_INT, &cfg.statusbarScale, 1, 20},

    // HUD colour + alpha
    {"hud-color-r", 0, CVT_FLOAT, &cfg.hudColor[0], 0, 1},
    {"hud-color-g", 0, CVT_FLOAT, &cfg.hudColor[1], 0, 1},
    {"hud-color-b", 0, CVT_FLOAT, &cfg.hudColor[2], 0, 1},
    {"hud-color-a", 0, CVT_FLOAT, &cfg.hudColor[3], 0, 1},
    {"hud-icon-alpha", 0, CVT_FLOAT, &cfg.hudIconAlpha, 0, 1},

    {"hud-status-alpha", 0, CVT_FLOAT, &cfg.statusbarOpacity, 0, 1},
    {"hud-status-icon-a", 0, CVT_FLOAT, &cfg.statusbarCounterAlpha, 0, 1},

    // HUD icons
    {"hud-ammo", 0, CVT_BYTE, &cfg.hudShown[HUD_AMMO], 0, 1},
    {"hud-armor", 0, CVT_BYTE, &cfg.hudShown[HUD_ARMOR], 0, 1},
    {"hud-keys", 0, CVT_BYTE, &cfg.hudShown[HUD_KEYS], 0, 1},
    {"hud-health", 0, CVT_BYTE, &cfg.hudShown[HUD_HEALTH], 0, 1},
    {"hud-currentitem", 0, CVT_BYTE, &cfg.hudShown[HUD_CURRENTITEM], 0, 1},

    // HUD displays
    {"hud-tome-timer", CVF_NO_MAX, CVT_INT, &cfg.tomeCounter, 0, 0},
    {"hud-tome-sound", CVF_NO_MAX, CVT_INT, &cfg.tomeSound, 0, 0},
    {"hud-inventory-timer", 0, CVT_FLOAT, &cfg.inventoryTimer, 0, 30},
    {"hud-inventory-slot-showempty", 0, CVT_BYTE, &cfg.inventorySlotShowEmpty, 0, 1},
    {"hud-inventory-slot-max", CVF_NO_MAX, CVT_INT, &cfg.inventorySlotMaxVis, 0, 0, ST_ResizeInventory},

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

// Console commands for the HUD/Status bar
ccmd_t  sthudCCmds[] = {
    {"sbsize",      "s",    CCmdStatusBarSize},
    {NULL}
};

// CODE --------------------------------------------------------------------

/**
 * Register CVARs and CCmds for the HUD/Status bar
 */
void ST_Register(void)
{
    int                 i;

    for(i = 0; sthudCVars[i].name; ++i)
        Con_AddVariable(sthudCVars + i);
    for(i = 0; sthudCCmds[i].name; ++i)
        Con_AddCommand(sthudCCmds + i);
}

static void shadeChain(float alpha)
{
    DGL_Disable(DGL_TEXTURING);

    DGL_Begin(DGL_QUADS);
        // Left shadow.
        DGL_Color4f(0, 0, 0, alpha);
        DGL_Vertex2f(20, 200);
        DGL_Vertex2f(20, 190);
        DGL_Color4f(0, 0, 0, 0);
        DGL_Vertex2f(35, 190);
        DGL_Vertex2f(35, 200);

        // Right shadow.
        DGL_Vertex2f(277, 200);
        DGL_Vertex2f(277, 190);
        DGL_Color4f(0, 0, 0, alpha);
        DGL_Vertex2f(293, 190);
        DGL_Vertex2f(293, 200);
    DGL_End();

    DGL_Enable(DGL_TEXTURING);
}

static void drawChain(hudstate_t* hud)
{
    static int theirColors[] = {
        144, // Green.
        197, // Yellow.
        150, // Red.
        220  // Blue.
    };
    int                 chainY;
    float               healthPos, gemXOffset, gemglow;
    int                 x, y, w, h, gemNum;
    float               cw, rgb[3];
    int                 player = hud - hudStates;
    player_t*           plr = &players[player];

    hud->oldHealth = hud->healthMarker;

    chainY = 191;
    if(hud->healthMarker != plr->plr->mo->health)
        chainY += hud->chainWiggle;

    healthPos = MINMAX_OF(0, hud->healthMarker / 100.f, 1);

    if(!IS_NETGAME)
        gemNum = 2; // Always use the red gem in single player.
    else
        gemNum = cfg.playerColor[player];
    gemglow = healthPos;

    // Draw the chain.
    x = 21;
    y = chainY;
    w = ST_WIDTH - 21 - 28;
    h = 8;
    cw = (float) w / chain.width;

    DGL_SetPatch(chain.lump, DGL_REPEAT, DGL_CLAMP);

    DGL_Color4f(1, 1, 1, hud->statusbarCounterAlpha);

    gemXOffset = (w - lifeGems[gemNum].width) * healthPos;

    if(gemXOffset > 0)
    {   // Left chain section.
        float               cw = gemXOffset / chain.width;

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

    if(gemXOffset + lifeGems[gemNum].width < w)
    {   // Right chain section.
        float               cw =
            (w - gemXOffset - lifeGems[gemNum].width) / chain.width;

        DGL_Begin(DGL_QUADS);
            DGL_TexCoord2f(0, 0, 0);
            DGL_Vertex2f(x + gemXOffset + lifeGems[gemNum].width, y);

            DGL_TexCoord2f(0, cw, 0);
            DGL_Vertex2f(x + w, y);

            DGL_TexCoord2f(0, cw, 1);
            DGL_Vertex2f(x + w, y + h);

            DGL_TexCoord2f(0, 0, 1);
            DGL_Vertex2f(x + gemXOffset + lifeGems[gemNum].width, y + h);
        DGL_End();
    }

    // Draw the life gem.
    GL_DrawPatchLitAlpha(x + gemXOffset, chainY,
                         1, hud->statusbarCounterAlpha,
                         lifeGems[gemNum].lump);

    shadeChain((hud->statusbarCounterAlpha + cfg.statusbarOpacity) /3);

    // How about a glowing gem?
    DGL_BlendMode(BM_ADD);
    DGL_Bind(Get(DD_DYNLIGHT_TEXTURE));

    R_PalIdxToRGB(rgb, theirColors[gemNum], false);
    DGL_DrawRect(x + gemXOffset - 11, chainY - 6, 41, 24, rgb[0], rgb[1],
                 rgb[2], gemglow - (1 - hud->statusbarCounterAlpha));

    DGL_BlendMode(BM_NORMAL);
    DGL_Color4f(1, 1, 1, 1);
}

/**
 * Draws the whole statusbar backgound.
 * \todo There is a whole lot of constants in here. What if someone wants to
 * replace the statusbar with new patches?
 */
static void drawStatusBarBackground(int player)
{
    hudstate_t*         hud = &hudStates[player];
    player_t*           plr = &players[player];
    float               alpha;

    if(hud->blended)
    {
        alpha = cfg.statusbarOpacity - hud->hideAmount;
        if(!(alpha > 0))
            return;
        alpha = MINMAX_OF(0.f, alpha, 1.f);
    }
    else
        alpha = 1.0f;

    if(!(alpha < 1))
    {   // We can just render the full thing as normal.
        // Top bits.
        GL_DrawPatch(0, 148, statusbarTopLeft.lump);
        GL_DrawPatch(290, 148, statusbarTopRight.lump);

        // Faces.
        GL_DrawPatch(0, 158, statusbar.lump);

        if(P_GetPlayerCheats(plr) & CF_GODMODE)
        {
            GL_DrawPatch(16, 167, godLeft.lump);
            GL_DrawPatch(287, 167, godRight.lump);
        }

        if(!(hud->inventory.flags & HIF_VISIBLE))
        {
            if(deathmatch)
                GL_DrawPatch(34, 160, statBar.lump);
            else
                GL_DrawPatch(34, 160, lifeBar.lump);
        }
        else
        {
            GL_DrawPatch(34, 160, invBar.lump);
        }
    }
    else
    {
        DGL_Color4f(1, 1, 1, alpha);

        // Top bits.
        GL_DrawPatch_CS(0, 148, statusbarTopLeft.lump);
        GL_DrawPatch_CS(290, 148, statusbarTopRight.lump);

        DGL_SetPatch(statusbar.lump, DGL_REPEAT, DGL_REPEAT);

        // Top border.
        DGL_DrawCutRectTiled(34, 158, 248, 2, 320, 42, 34, 0, 0, 158, 0, 0);

        // Chain background.
        DGL_DrawCutRectTiled(34, 191, 248, 9, 320, 42, 34, 33, 0, 191, 16, 8);

        // Faces.
        if(P_GetPlayerCheats(plr) & CF_GODMODE)
        {
            // If GOD mode we need to cut windows
            DGL_DrawCutRectTiled(0, 158, 34, 42, 320, 42, 0, 0, 16, 167, 16, 8);
            DGL_DrawCutRectTiled(282, 158, 38, 42, 320, 42, 282, 0, 287, 167, 16, 8);

            GL_DrawPatch_CS(16, 167, godLeft.lump);
            GL_DrawPatch_CS(287, 167, godRight.lump);
        }
        else
        {
            DGL_DrawCutRectTiled(0, 158, 34, 42, 320, 42, 0, 0, 0, 158, 0, 0);
            DGL_DrawCutRectTiled(282, 158, 38, 42, 320, 42, 282, 0, 0, 158, 0, 0);
        }

        if(!(hud->inventory.flags & HIF_VISIBLE))
        {
            if(deathmatch)
                GL_DrawPatch_CS(34, 160, statBar.lump);
            else
                GL_DrawPatch_CS(34, 160, lifeBar.lump);
        }
        else
        {
            GL_DrawPatch_CS(34, 160, invBar.lump);
        }
    }
}

void ST_updateWidgets(int player)
{
    static int          largeammo = 1994; // Means "n/a".

    int                 i;
    ammotype_t          ammoType;
    boolean             found;
    hudstate_t*         hud = &hudStates[player];
    player_t*           plr = &players[player];
    int                 lvl = (plr->powers[PT_WEAPONLEVEL2]? 1 : 0);

    if(hud->blended)
    {
        hud->statusbarCounterAlpha =
            MINMAX_OF(0.f, cfg.statusbarCounterAlpha - hud->hideAmount, 1.f);
    }
    else
        hud->statusbarCounterAlpha = 1.0f;

    // Must redirect the pointer if the ready weapon has changed.
    found = false;
    for(ammoType = 0; ammoType < NUM_AMMO_TYPES && !found; ++ammoType)
    {
        if(!weaponInfo[plr->readyWeapon][plr->class].mode[lvl].ammoType[ammoType])
            continue; // Weapon does not use this type of ammo.

        //// \todo Only supports one type of ammo per weapon.
        hud->wReadyWeapon.num = &plr->ammo[ammoType].owned;

        if(hud->oldReadyWeapon != plr->readyWeapon)
            hud->currentAmmoIconIdx = (int) ammoType;

        found = true;
    }

    if(!found) // Weapon takes no ammo at all.
    {
        hud->wReadyWeapon.num = &largeammo;
        hud->currentAmmoIconIdx = -1;
    }

    hud->wReadyWeapon.data = plr->readyWeapon;

    // Update keycard multiple widgets.
    for(i = 0; i < 3; ++i)
    {
        hud->keyBoxes[i] = plr->keys[i] ? true : false;
    }

    // Used by wFrags widget.
    hud->fragsOn = deathmatch && hud->statusbarActive;
    hud->fragsCount = 0;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->inGame)
            continue;

        hud->fragsCount += plr->frags[i] * (i != player ? 1 : -1);
    }
}

/**
 * Rebuild the inventory item type table.
 * These indices can be used to associate slots in an inventory browser
 * to the items held within.
 */
static void rebuildInventory(hudstate_t* hud)
{
    int                 i, plrNum = hud - hudStates;
    uint                numOwnedItemTypes;
    inventoryitemtype_t selectedType = P_GetInvItem(
        hud->inventory.slots[hud->inventory.selected])->type;

    hud->inventory.selected = 0;

    numOwnedItemTypes = 0;
    for(i = 0; i < NUM_INVENTORYITEM_TYPES - 1; ++i)
        if(P_InventoryCount(plrNum, IIT_FIRST + i) > 0)
            numOwnedItemTypes++;

    // Always 1:1
    hud->inventory.numOwnedItemTypes = hud->inventory.numUsedSlots =
        numOwnedItemTypes;

    memset(hud->inventory.slots, 0,
           sizeof(hud->inventory.slots[0]) * (NUM_INVENTORYITEM_TYPES - 1));
    if(hud->inventory.numUsedSlots)
    {
        uint                idx = 0;

        for(i = 0; i < NUM_INVENTORYITEM_TYPES - 1; ++i)
        {
            if(P_InventoryCount(plrNum, IIT_FIRST + i))
            {
                hud->inventory.slots[idx] = i;
                if(P_GetInvItem(i)->type == selectedType)
                    hud->inventory.selected = idx;

                if(++idx >= hud->inventory.numUsedSlots)
                    break;
            }
        }
    }

    if(hud->inventory.flags & HIF_IS_DIRTY)
        hud->inventory.flags &= ~HIF_IS_DIRTY;
}

void ST_Ticker(void)
{
    int                 i;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t*           plr = &players[i];
        hudstate_t*         hud = &hudStates[i];

        if(!(plr->plr->inGame && (plr->plr->flags & DDPF_LOCAL)))
            continue;

        if(hud->inventory.flags & HIF_IS_DIRTY)
            rebuildInventory(hud);

        ST_updateWidgets(i);

        if(!P_IsPaused())
        {
            int                 delta, curHealth;

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

            if(mapTime & 1)
            {
                hud->chainWiggle = P_Random() & 1;
            }

            curHealth = MAX_OF(plr->plr->mo->health, 0);
            if(curHealth < hud->healthMarker)
            {
                delta = MINMAX_OF(1, (hud->healthMarker - curHealth) >> 2, 4);
                hud->healthMarker -= delta;
            }
            else if(curHealth > hud->healthMarker)
            {
                delta = MINMAX_OF(1, (curHealth - hud->healthMarker) >> 2, 4);
                hud->healthMarker += delta;
            }

            // Tome of Power countdown sound.
            if(plr->powers[PT_WEAPONLEVEL2] &&
               plr->powers[PT_WEAPONLEVEL2] < cfg.tomeSound * 35)
            {
                int                 timeleft =
                    plr->powers[PT_WEAPONLEVEL2] / 35;

                if(hud->tomePlay != timeleft)
                {
                    hud->tomePlay = timeleft;
                    S_LocalSound(SFX_KEYUP, NULL);
                }
            }

            if(hud->inventory.flags & HIF_VISIBLE)
            {
                // Turn inventory off after a certain amount of time?
                if(cfg.inventoryTimer == 0)
                {
                    hud->inventory.hideTics = 0;
                }
                else
                {
                    if(hud->inventory.hideTics > 0)
                        hud->inventory.hideTics--;
                    if(hud->inventory.hideTics == 0 && cfg.inventoryTimer > 0)
                        ST_Inventory(i, false); // Close the inventory.
                }
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
    int                 palette = 0;
    player_t*           plr = &players[player];

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

static void drawSmallNum(int val, int numDigits, int x, int y, float alpha)
{
    int                 w = sNumbers[0].width;
    boolean             drawMinus = false;

    if(val < 0)
    {
        if(numDigits == 2 && val < -9)
            val = -9;
        else if(numDigits == 3 && val < -99)
            val = -99;
        val = -val;
        drawMinus = true;
    }

    // In the special case of 0, you draw 0.
    if(val == 0)
        WI_DrawPatch(x - w, y, 1, 1, 1, alpha, &sNumbers[0], NULL, false,
                     ALIGN_LEFT);

    // Draw the number.
    while(val && numDigits--)
    {
        x -= w;
        WI_DrawPatch(x, y, 1, 1, 1, alpha, &sNumbers[val % 10],
                     NULL, false, ALIGN_LEFT);
        val /= 10;
    }

    // Draw a minus sign if necessary.
    if(drawMinus)
        WI_DrawPatch(x - 8, y, 1, 1, 1, alpha, &huMinus, NULL, false,
                     ALIGN_LEFT);
}

static void inventoryIndexes(const player_t* plr, const hudstate_t* hud,
                             uint maxVisSlots, int origCursor,
                             uint* firstVisible, uint* cursorPos,
                             uint* fromSlot, uint* toSlot)
{
    int                 cursor, first, from, to;

    if(!firstVisible && !cursorPos && !fromSlot && !toSlot)
        return;

    if(cfg.inventorySelectMode)
    {   // Scroll.
        int                 last;

        cursor = maxVisSlots / 2;

        if(cfg.inventoryWrap)
        {
            first = hud->inventory.selected - cursor;
            if(first < 0)
                first += hud->inventory.numUsedSlots;

            from = 0;
            to = maxVisSlots;
        }
        else
        {
            first = hud->inventory.selected - cursor;
            if(first < 0)
                first = 0;

            from = cursor - hud->inventory.selected;
            if(from < 0)
                from = 0;

            last = hud->inventory.selected + cursor + (maxVisSlots % 2? 1 : 0);
            if(last - 1 < (signed) hud->inventory.numUsedSlots)
                to = maxVisSlots;
            else
                to = maxVisSlots - (last - hud->inventory.numUsedSlots);
        }
    }
    else
    {   // Cursor.
        cursor = origCursor;

        if(hud->inventory.numUsedSlots < maxVisSlots)
        {
            from = (maxVisSlots - hud->inventory.numUsedSlots) / 2;
            to = from + hud->inventory.numUsedSlots;
            cursor += from;
        }
        else
        {
            if(cfg.inventoryWrap)
            {
                from = 0;
                to = maxVisSlots;
            }
            else
            {
                int                 last;

                from = cursor - hud->inventory.selected;
                if(from < 0)
                    from = 0;

                last = hud->inventory.numUsedSlots - 1 -
                    (hud->inventory.selected + cursor);
                to = maxVisSlots;
            }
        }

        first = hud->inventory.selected - origCursor;
        if(cfg.inventoryWrap)
        {
            if(first < 0)
                first += hud->inventory.numUsedSlots;
        }
        else
        {
            if(hud->inventory.numUsedSlots < maxVisSlots ||
               first + maxVisSlots > hud->inventory.numUsedSlots)
            {
                int             shift = hud->inventory.numUsedSlots -
                    (first + maxVisSlots);

                first += shift;
                if(first < 0)
                {
                    first = 0;
                    cursor = from + hud->inventory.selected;
                }
                else
                    cursor -= shift;
            }
        }
    }

    if(firstVisible)
        *firstVisible = (unsigned) first;
    if(cursorPos)
        *cursorPos = (unsigned) cursor;
    if(fromSlot)
        *fromSlot = (unsigned) from;
    if(toSlot)
        *toSlot = (unsigned) to;
}

static void drawInventory(const hudstate_t* hud, int x, int y,
                          float alpha)
{
#define BORDER              1

    int                 player = hud - hudStates;
    player_t*           plr = &players[player];
    uint                i, from, to, idx, slot, first, selected,
                        numVisSlots, maxVisSlots, startSlot, endSlot;
    float               textAlpha =
        MINMAX_OF(0.f, alpha - hud->hideAmount - ( 1 - cfg.hudColor[3]), 1.f);
    float               iconAlpha =
        MINMAX_OF(0.f, alpha - hud->hideAmount - ( 1 - cfg.hudIconAlpha), 1.f);
    float               invScale, lightDelta;

    if(cfg.inventorySlotMaxVis)
        maxVisSlots = cfg.inventorySlotMaxVis;
    else
        maxVisSlots = NUM_INVENTORYITEM_TYPES - 1;

    inventoryIndexes(plr, hud, maxVisSlots, hud->inventory.varCursorPos,
                     &first, &selected, &startSlot, &endSlot);

    numVisSlots = maxVisSlots;

    {
#define EXTRA_SCALE         .75f

    float               availWidth = SCREENWIDTH - 50 * 2,
                        width = (numVisSlots * ST_INVSLOTWIDTH) * EXTRA_SCALE;

    if(width > availWidth)
        invScale = availWidth / width;
    else
        invScale = 1;
    invScale *= cfg.hudScale * EXTRA_SCALE;

#undef EXTRA_SCALE
    }

    if(maxVisSlots % 2)
        lightDelta = 1.f / maxVisSlots;
    else
        lightDelta = 1.f / (maxVisSlots - 1);
    lightDelta *= 2;

    idx = first;
    slot = (cfg.inventorySlotShowEmpty? 0 : startSlot);
    from = (cfg.inventorySlotShowEmpty? 0 : startSlot);

    if(cfg.inventorySlotShowEmpty)
    {
        to = maxVisSlots;
    }
    else
    {
        if(startSlot > 0)
            to = maxVisSlots;
        else
            to = endSlot - startSlot;
        if(hud->inventory.numUsedSlots - 1 < endSlot - startSlot)
            to = from + hud->inventory.numUsedSlots;
    }

Draw_BeginZoom(invScale, x, y + ST_INVENTORYHEIGHT);

    x -= (numVisSlots * ST_INVSLOTWIDTH) / 2;

    for(i = from; i < to; ++i)
    {
        float               light, a;

        if(i < maxVisSlots / 2)
            light = (i + 1) * lightDelta;
        else
            light = (maxVisSlots - i) * lightDelta;
        a = i == selected? .5f : light / 2;

        GL_DrawPatchLitAlpha(x + slot * ST_INVSLOTWIDTH, y,
                             light, a * alpha,
                             dpInvItemBox.lump);

        if(i >= startSlot && i < endSlot)
        {
            uint                count;
            const invitem_t*    item =
                P_GetInvItem(hud->inventory.slots[idx]);

            if((count = P_InventoryCount(player, item->type)))
            {
                GL_DrawPatchLitAlpha(x + slot * ST_INVSLOTWIDTH, y, 1,
                                     slot == selected? alpha : iconAlpha / 3,
                                     item->patchLump);

                if(count > 1)
                    drawSmallNum(P_InventoryCount(player, item->type),
                                 ST_INVITEMCWIDTH,
                                 x + slot * ST_INVSLOTWIDTH + ST_INVCOUNTOFFX,
                                 y + ST_INVCOUNTOFFY,
                                 slot == selected? alpha : textAlpha / 2);
            }

            if(++idx > hud->inventory.numOwnedItemTypes - 1)
                idx = 0;
        }
        slot++;
    }

    GL_DrawPatchLitAlpha(x + selected * ST_INVSLOTWIDTH,
                         y + ST_INVENTORYHEIGHT - BORDER,
                         1, alpha, invSelectBox.lump);

    if(hud->inventory.numUsedSlots > maxVisSlots)
    {
#define ARROW_RELXOFF          2
#define ARROW_YOFFSET          9

        if(cfg.inventoryWrap || first != 0)
        {
            GL_DrawPatchLitAlpha(x - invPageLeft[0].width - ARROW_RELXOFF,
                                 y + ARROW_YOFFSET, 1, iconAlpha,
                                 invPageLeft[!(mapTime & 4)? 1 : 0].lump);
        }

        if(cfg.inventoryWrap || hud->inventory.numUsedSlots - first > numVisSlots)
        {
            GL_DrawPatchLitAlpha(x + numVisSlots * ST_INVSLOTWIDTH + ARROW_RELXOFF,
                                 y + ARROW_YOFFSET, 1, iconAlpha,
                                 invPageRight[!(mapTime & 4)? 1 : 0].lump);
        }

#undef ARROW_XOFFSET
#undef ARROW_YOFFSET
    }

Draw_EndZoom();

#undef BORDER
}

static void drawInventory2(const hudstate_t* hud, int x, int y, float alpha)
{
    int                 player = hud - hudStates;
    player_t*           plr = &players[player];
    uint                i, idx, slot, from, to, first, cursor;

    inventoryIndexes(plr, hud, NUMVISINVSLOTS,
                     hud->inventory.fixedCursorPos, &first, &cursor,
                     NULL, NULL);

    from = 0;
    to = MIN_OF(hud->inventory.numUsedSlots, NUMVISINVSLOTS);

    idx = first;
    slot = 0;
    for(i = from; i < to; ++i)
    {
        uint                count;
        const invitem_t*    item = P_GetInvItem(hud->inventory.slots[idx]);

        if((count = P_InventoryCount(player, item->type)))
        {
            GL_DrawPatchLitAlpha(x + slot * 31, y, 1,
                                 hud->statusbarCounterAlpha,
                                 item->patchLump);

            if(count > 1)
                drawSmallNum(P_InventoryCount(player, item->type),
                             ST_INVITEMCWIDTH,
                             x + slot * 31 + ST_INVCOUNTOFFX,
                             y + ST_INVCOUNTOFFY,
                             hud->statusbarCounterAlpha);
        }

        if(i == cursor)
            GL_DrawPatchLitAlpha(x + slot * 31,
                                 y + ST_INVENTORYHEIGHT - 1,
                                 1, hud->statusbarCounterAlpha,
                                 invSelectBox.lump);

        if(++idx > hud->inventory.numOwnedItemTypes - 1)
            idx = 0;
        slot++;
    }

    if(hud->inventory.numUsedSlots > NUMVISINVSLOTS)
    {
        // Draw more left indicator.
        if(cfg.inventoryWrap || first != 0)
            GL_DrawPatchLitAlpha(x - 12, y - 1, 1,
                                 hud->statusbarCounterAlpha,
                                 invPageLeft[!(mapTime & 4)? 1 : 0].lump);

        // Draw more right indicator.
        if(cfg.inventoryWrap || hud->inventory.numUsedSlots - first > NUMVISINVSLOTS)
            GL_DrawPatchLitAlpha(269, y - 1, 1,
                                 hud->statusbarCounterAlpha,
                                 invPageRight[!(mapTime & 4)? 1 : 0].lump);
    }
}

static void drawWidgets(hudstate_t* hud)
{
    int                 i;
    int                 player = hud - hudStates;
    player_t*           plr = &players[player];
    boolean             refresh = true;

    hud->oldHealth = -1;
    if(!(hud->inventory.flags & HIF_VISIBLE))
    {
        inventoryitemtype_t readyItem;

        // Draw all the counters.

        // Frags.
        if(deathmatch)
            STlib_updateNum(&hud->wFrags, refresh);
        else
            STlib_updateNum(&hud->wHealth, refresh);

        // Draw armor.
        STlib_updateNum(&hud->wArmor, refresh);

        // Draw keys.
        for(i = 0; i < 3; ++i)
            STlib_updateBinIcon(&hud->wKeyBoxes[i], refresh);

        STlib_updateNum(&hud->wReadyWeapon, refresh);
        STlib_updateMultIcon(&hud->wCurrentAmmoIcon, refresh);

        // Current inventory item.
        if((readyItem = P_InventoryReadyItem(player)) != IIT_NONE)
        {
            lumpnum_t           patch;

            if(hud->currentInvItemFlash > 0)
            {
                patch = dpInvItemFlash[hud->currentInvItemFlash % 5].lump;
            }
            else
            {
                patch = P_GetInvItem(readyItem-1)->patchLump;
            }

            DGL_Color4f(1, 1, 1, hud->statusbarCounterAlpha);
            GL_DrawPatch_CS(ST_INVITEMX, ST_INVITEMY, patch);

            if(!(hud->currentInvItemFlash > 0))
            {
                uint                count =
                    P_InventoryCount(player, readyItem);

                if(count > 1)
                    drawSmallNum(count, ST_INVITEMCWIDTH, ST_INVITEMCX,
                                 ST_INVITEMCY, hud->statusbarCounterAlpha);
            }
        }
    }
    else
    {   // Draw Inventory.
        drawInventory2(hud, ST_INVENTORYX, ST_INVENTORYY, 1);
    }
}

void ST_Inventory(int player, boolean show)
{
    player_t*           plr;
    hudstate_t*         hud;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    plr = &players[player];
    if(!((plr->plr->flags & DDPF_LOCAL) && plr->plr->inGame))
        return;

    hud = &hudStates[player];

    if(show)
    {
        hud->inventory.flags |= HIF_VISIBLE;
        hud->inventory.hideTics = (int) (cfg.inventoryTimer * TICSPERSEC);

        ST_HUDUnHide(player, HUE_FORCE);
    }
    else
    {
        hud->inventory.flags &= ~HIF_VISIBLE;
        P_InventorySetReadyItem(player,
            P_GetInvItem(hud->inventory.slots[hud->inventory.selected])->type);
    }
}

boolean ST_InventoryIsVisible(int player)
{
    player_t*           plr;
    hudstate_t*         hud;

    if(player < 0 || player >= MAXPLAYERS)
        return false;

    plr = &players[player];
    if(!((plr->plr->flags & DDPF_LOCAL) && plr->plr->inGame))
        return false;

    hud = &hudStates[player];

    return ((hud->inventory.flags & HIF_VISIBLE)? true : false);
}

/**
 * Mark the HUD inventory as dirty (i.e., the player inventory state has
 * changed in such a way that would require the HUD inventory display(s)
 * to be updated e.g., the player gains a new item).
 *
 * @param player        Player whoose in HUD inventory is dirty.
 */
void ST_InventoryMarkDirty(int player)
{
    if(player < 0 || player >= MAXPLAYERS)
        return;

    hudStates[player].inventory.flags |= HIF_IS_DIRTY;
}

boolean ST_InventorySelect(int player, inventoryitemtype_t type)
{
    assert(type == IIT_NONE ||
           (type >= IIT_FIRST && type < NUM_INVENTORYITEM_TYPES));

    if(player >= 0 && player < MAXPLAYERS)
    {
        hudstate_t*         hud = &hudStates[player];

        if(P_InventoryCount(player, type))
        {
            uint                i;

            for(i = 0; i < hud->inventory.numUsedSlots; ++i)
                if(P_GetInvItem(hud->inventory.slots[i])->type == type)
                {
                    hud->inventory.selected = i;
                    hud->inventory.varCursorPos = hud->inventory.fixedCursorPos = 0;

                    return true;
                }
        }
    }

    return false;
}

boolean ST_InventoryMove(int player, int dir, boolean silent)
{
    player_t*           plr;
    hudstate_t*         hud;

    if(player < 0 || player >= MAXPLAYERS)
        return false;

    plr = &players[player];
    if(!((plr->plr->flags & DDPF_LOCAL) && plr->plr->inGame))
        return false;
    hud = &hudStates[player];

    if(hud->inventory.flags & HIF_IS_DIRTY)
        rebuildInventory(hud);

    if(hud->inventory.numOwnedItemTypes > 1)
    {
        hudstate_t*         hud = &hudStates[player];

        if(dir == -1)
        {
            if(hud->inventory.selected == 0)
            {
                if(cfg.inventoryWrap)
                    hud->inventory.selected = hud->inventory.numUsedSlots - 1;
            }
            else
                hud->inventory.selected--;

            // First the fixed range statusbar cursor.
            if(hud->inventory.fixedCursorPos > 0)
                hud->inventory.fixedCursorPos--;

            // Now the variable range full-screen cursor.
            if(hud->inventory.varCursorPos > 0)
                hud->inventory.varCursorPos--;
        }
        else if(dir == 1)
        {
            uint                maxVisSlots;

            if(hud->inventory.selected == hud->inventory.numUsedSlots - 1)
            {
                if(cfg.inventoryWrap)
                    hud->inventory.selected = 0;
            }
            else
                hud->inventory.selected++;

            // First the fixed range statusbar cursor.
            if(hud->inventory.fixedCursorPos < NUMVISINVSLOTS - 1 &&
               !(hud->inventory.fixedCursorPos + 1 > hud->inventory.numUsedSlots - 1))
                hud->inventory.fixedCursorPos++;

            // Now the variable range full-screen cursor.
            if(cfg.inventorySlotMaxVis)
                maxVisSlots = cfg.inventorySlotMaxVis;
            else
                maxVisSlots = NUM_INVENTORYITEM_TYPES - 1;

            if(hud->inventory.varCursorPos < maxVisSlots - 1 &&
               !(hud->inventory.varCursorPos + 1 > hud->inventory.numUsedSlots - 1))
                hud->inventory.varCursorPos++;
        }
        hud->inventory.flags |= HIF_IS_DIRTY;

        P_InventorySetReadyItem(player,
            P_GetInvItem(hud->inventory.selected)->type);
    }

    if(!silent)
    {
        hud->inventory.hideTics = (int) (cfg.inventoryTimer * TICSPERSEC);
    }

    return true;
}

void ST_InventoryFlashCurrent(int player)
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

static void drawINumber(signed int val, int x, int y, float r, float g,
                        float b, float a)
{
    int                 oldval;

    DGL_Color4f(r, g, b, a);

    // Limit to 999.
    if(val > 999)
        val = 999;

    oldval = val;
    if(val < 0)
    {
        if(val < -9)
        {
            GL_DrawPatch_CS(x + 1, y + 1, lame.lump);
        }
        else
        {
            val = -val;
            GL_DrawPatch_CS(x + 18, y, iNumbers[val].lump);
            GL_DrawPatch_CS(x + 9, y, negative.lump);
        }
        return;
    }

    if(val > 99)
    {
        GL_DrawPatch_CS(x, y, iNumbers[val / 100].lump);
    }

    val = val % 100;
    if(val > 9 || oldval > 99)
    {
        GL_DrawPatch_CS(x + 9, y, iNumbers[val / 10].lump);
    }

    val = val % 10;
    GL_DrawPatch_CS(x + 18, y, iNumbers[val].lump);
}

static void drawBNumber(signed int val, int x, int y, float red,
                        float green, float blue, float alpha)
{
    const dpatch_t*     patch;
    int                 xpos;
    int                 oldval;

    oldval = val;
    xpos = x;
    if(val < 0)
    {
        val = 0;
    }

    if(val > 99)
    {
        patch = &huFontB[15 + val / 100];

        GL_DrawPatchLitAlpha(xpos + 8 - patch->width / 2, y +2, 0, alpha * .4f,
                             patch->lump);
        DGL_Color4f(red, green, blue, alpha);
        GL_DrawPatch_CS(xpos + 6 - patch->width / 2, y, patch->lump);
        DGL_Color4f(1, 1, 1, 1);
    }

    val = val % 100;
    xpos += 12;
    if(val > 9 || oldval > 99)
    {
        patch = &huFontB[15 + val / 10];

        GL_DrawPatchLitAlpha(xpos + 8 - patch->width / 2, y +2, 0, alpha * .4f,
                             patch->lump);
        DGL_Color4f(red, green, blue, alpha);
        GL_DrawPatch_CS(xpos + 6 - patch->width / 2, y, patch->lump);
        DGL_Color4f(1, 1, 1, 1);
    }

    val = val % 10;
    xpos += 12;
    patch = &huFontB[15 + val];

    GL_DrawPatchLitAlpha(xpos + 8 - patch->width / 2, y +2, 0, alpha * .4f,
                         patch->lump);
    DGL_Color4f(red, green, blue, alpha);
    GL_DrawPatch_CS(xpos + 6 - patch->width / 2, y, patch->lump);
    DGL_Color4f(1, 1, 1, 1);
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

static void drawIcons(int player)
{
    int                 frame;
    float               iconAlpha = cfg.hudIconAlpha;
    float               textAlpha = cfg.hudColor[3];
    hudstate_t*         hud = &hudStates[player];
    player_t*           plr = &players[player];

    Draw_BeginZoom(cfg.hudScale, 2, 2);

    // Flight icons
    if(plr->powers[PT_FLIGHT])
    {
        int     offset = (cfg.hudShown[HUD_AMMO] && cfg.screenBlocks > 10 &&
                          plr->readyWeapon > 0 &&
                          plr->readyWeapon < 7) ? 43 : 0;

        if(plr->powers[PT_FLIGHT] > BLINKTHRESHOLD ||
           !(plr->powers[PT_FLIGHT] & 16))
        {
            frame = (mapTime / 3) & 15;
            if(plr->plr->mo->flags2 & MF2_FLY)
            {
                if(hud->hitCenterFrame && (frame != 15 && frame != 0))
                {
                    GL_DrawPatchLitAlpha(20 + offset, 17, 1, iconAlpha,
                                         spinFly[15].lump);
                }
                else
                {
                    GL_DrawPatchLitAlpha(20 + offset, 17, 1, iconAlpha,
                                         spinFly[frame].lump);
                    hud->hitCenterFrame = false;
                }
            }
            else
            {
                if(!hud->hitCenterFrame && (frame != 15 && frame != 0))
                {
                    GL_DrawPatchLitAlpha(20 + offset, 17, 1, iconAlpha,
                                         spinFly[frame].lump);
                    hud->hitCenterFrame = false;
                }
                else
                {
                    GL_DrawPatchLitAlpha(20 + offset, 17, 1, iconAlpha,
                                         spinFly[15].lump);
                    hud->hitCenterFrame = true;
                }
            }
        }
    }

    Draw_EndZoom();

    Draw_BeginZoom(cfg.hudScale, 318, 2);

    if(plr->powers[PT_WEAPONLEVEL2] && !plr->morphTics)
    {
        if(cfg.tomeCounter || plr->powers[PT_WEAPONLEVEL2] > BLINKTHRESHOLD
           || !(plr->powers[PT_WEAPONLEVEL2] & 16))
        {
            frame = (mapTime / 3) & 15;
            if(cfg.tomeCounter && plr->powers[PT_WEAPONLEVEL2] < 35)
            {
                DGL_Color4f(1, 1, 1, plr->powers[PT_WEAPONLEVEL2] / 35.0f);
            }

            GL_DrawPatchLitAlpha(300, 17, 1, iconAlpha,
                                 spinBook[frame].lump);
        }

        if(plr->powers[PT_WEAPONLEVEL2] < cfg.tomeCounter * 35)
        {
#define COUNT_X             (303)
#define COUNT_Y             (30)

            int                 val = 1 + plr->powers[PT_WEAPONLEVEL2] / 35;

            DGL_Color4f(1, 1, 1, textAlpha);

            if(val > 9)
            {
                GL_DrawPatch_CS(COUNT_X, COUNT_Y, sNumbers[(val / 10) % 10].lump);
            }

            val = val % 10;
            GL_DrawPatch_CS(COUNT_X + 4, COUNT_Y, sNumbers[val].lump);

#undef COUNT_X
#undef COUNT_Y
        }
    }

    Draw_EndZoom();
}

/**
 * All drawing for the statusbar starts and ends here.
 */
void ST_doRefresh(int player)
{
    hudstate_t*         hud;
    boolean             statusbarVisible;

    if(player < 0 || player > MAXPLAYERS)
        return;

    hud = &hudStates[player];

    statusbarVisible = (cfg.statusbarScale < 20 ||
        (cfg.statusbarScale == 20 && hud->showBar < 1.0f));

    hud->firstTime = false;

    if(statusbarVisible)
    {
        float               fscale = cfg.statusbarScale / 20.0f;
        float               h = 200 * (1 - fscale);

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        DGL_Translatef(160 - 320 * fscale / 2, h / hud->showBar, 0);
        DGL_Scalef(fscale, fscale, 1);
    }

    drawStatusBarBackground(player);
    drawChain(hud);
    drawWidgets(hud);

    if(statusbarVisible)
    {
        // Restore the normal modelview matrix.
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }
}

void ST_doFullscreenStuff(int player)
{
    int                 i, x, temp = 0;
    hudstate_t*         hud = &hudStates[player];
    player_t*           plr = &players[player];
    float               textAlpha =
        MINMAX_OF(0.f, hud->alpha - hud->hideAmount - ( 1 - cfg.hudColor[3]), 1.f);
    float               iconAlpha =
        MINMAX_OF(0.f, hud->alpha - hud->hideAmount - ( 1 - cfg.hudIconAlpha), 1.f);

    if(cfg.hudShown[HUD_AMMO])
    {
        if(plr->readyWeapon > 0 && plr->readyWeapon < 7)
        {
            ammotype_t          ammoType;
            int                 lvl =
                (plr->powers[PT_WEAPONLEVEL2]? 1 : 0);

            //// \todo Only supports one type of ammo per weapon.
            // For each type of ammo this weapon takes.
            for(ammoType = 0; ammoType < NUM_AMMO_TYPES; ++ammoType)
            {
                if(!weaponInfo[plr->readyWeapon][plr->class].mode[lvl].ammoType[ammoType])
                    continue;

                Draw_BeginZoom(cfg.hudScale, 2, 2);
                GL_DrawPatchLitAlpha(-1, 0, 1, iconAlpha,
                                     ammoIcons[plr->readyWeapon - 1].lump);
                drawINumber(plr->ammo[ammoType].owned, 18, 2,
                            1, 1, 1, textAlpha);

                Draw_EndZoom();
                break;
            }
        }
    }

Draw_BeginZoom(cfg.hudScale, 2, 198);
    if(cfg.hudShown[HUD_HEALTH])
    {
        if(plr->plr->mo->health > 0)
        {
            drawBNumber(plr->plr->mo->health, 2, 180,
                        cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2],
                        textAlpha);
        }
        else
        {
            drawBNumber(0, 2, 180, cfg.hudColor[0], cfg.hudColor[1],
                        cfg.hudColor[2], textAlpha);
        }
    }

    if(cfg.hudShown[HUD_ARMOR])
    {
        if(cfg.hudShown[HUD_HEALTH] && cfg.hudShown[HUD_KEYS])
            temp = 158;
        else if(!cfg.hudShown[HUD_HEALTH] && cfg.hudShown[HUD_KEYS])
            temp = 176;
        else if(cfg.hudShown[HUD_HEALTH] && !cfg.hudShown[HUD_KEYS])
            temp = 168;
        else if(!cfg.hudShown[HUD_HEALTH] && !cfg.hudShown[HUD_KEYS])
            temp = 186;

        drawINumber(plr->armorPoints, 6, temp, 1, 1, 1, textAlpha);
    }

    if(cfg.hudShown[HUD_KEYS])
    {
        x = 6;

        // Draw keys above health?
        if(plr->keys[KT_YELLOW])
        {
            GL_DrawPatchLitAlpha(x, cfg.hudShown[HUD_HEALTH]? 172 : 190, 1,
                                 iconAlpha, keys[0].lump);
            x += 11;
        }

        if(plr->keys[KT_GREEN])
        {
            GL_DrawPatchLitAlpha(x, cfg.hudShown[HUD_HEALTH]? 172 : 190, 1,
                                 iconAlpha, keys[1].lump);
            x += 11;
        }

        if(plr->keys[KT_BLUE])
        {
            GL_DrawPatchLitAlpha(x, cfg.hudShown[HUD_HEALTH]? 172 : 190, 1,
                                 iconAlpha, keys[2].lump);
        }
    }
Draw_EndZoom();

    if(deathmatch)
    {
        temp = 0;
        for(i = 0; i < MAXPLAYERS; ++i)
        {
            if(players[i].plr->inGame)
            {
                temp += plr->frags[i];
            }
        }

Draw_BeginZoom(cfg.hudScale, 2, 198);
        drawINumber(temp, 45, 185, 1, 1, 1, textAlpha);
Draw_EndZoom();
    }

    if(!(hud->inventory.flags & HIF_VISIBLE))
    {
        if(cfg.hudShown[HUD_CURRENTITEM])
        {
            if(hud->currentInvItemFlash > 0)
            {
Draw_BeginZoom(cfg.hudScale, 318, 198);
                GL_DrawPatchLitAlpha(286, 166, 1, iconAlpha / 2,
                                     dpInvItemBox.lump);
                GL_DrawPatchLitAlpha(286, 166, 1, iconAlpha,
                                     dpInvItemFlash[hud->currentInvItemFlash % 5].lump);
Draw_EndZoom();
            }
            else
            {
                inventoryitemtype_t readyItem = P_InventoryReadyItem(player);

                if(readyItem != IIT_NONE)
                {
                    uint                count;
                    lumpnum_t           patch =
                        P_GetInvItem(readyItem-1)->patchLump;

Draw_BeginZoom(cfg.hudScale, 318, 198);

                    GL_DrawPatchLitAlpha(286, 166, 1, iconAlpha / 2,
                                         dpInvItemBox.lump);
                    GL_DrawPatchLitAlpha(286, 166, 1, iconAlpha, patch);
                    if((count = P_InventoryCount(player, readyItem)) > 1)
                        drawSmallNum(count, ST_INVITEMCWIDTH, 313, 188,
                                     textAlpha);
Draw_EndZoom();
                }
            }
        }
    }
    else
    {
#define BORDER              2
#define INVENTORY_X         SCREENWIDTH / 2
#define INVENTORY_Y         SCREENHEIGHT - ST_INVENTORYHEIGHT - BORDER

        drawInventory(hud, INVENTORY_X, INVENTORY_Y, hud->alpha);

#undef BORDER
#undef INVENTORY_X
#undef INVENTORY_Y
    }
}

void ST_Drawer(int player, int fullscreenmode, boolean refresh)
{
    hudstate_t*         hud;
    player_t*           plr;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    plr = &players[player];
    if(!((plr->plr->flags & DDPF_LOCAL) && plr->plr->inGame))
        return;

    hud = &hudStates[player];

    hud->firstTime = hud->firstTime || refresh;
    hud->statusbarActive = (fullscreenmode < 2) ||
        (AM_IsActive(AM_MapForPlayer(player)) &&
         (cfg.automapHudDisplay == 0 || cfg.automapHudDisplay == 2) );

    // Do palette shifts
    ST_doPaletteStuff(player);

    // Either slide the status bar in or fade out the fullscreen hud
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
        if(fullscreenmode == 3)
        {
            if(hud->alpha > 0.0f)
            {
                hud->alpha -= 0.1f;
                fullscreenmode = 2;
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

    // Always try to render statusbar with alpha in fullscreen modes
    if(fullscreenmode)
        hud->blended = 1;
    else
        hud->blended = 0;

    if(hud->statusbarActive)
    {
        ST_doRefresh(player);
    }
    else if(fullscreenmode != 3)
    {
        ST_doFullscreenStuff(player);
    }

    DGL_Color4f(1, 1, 1, 1);
    drawIcons(player);
}

void ST_loadGraphics(void)
{
    int                 i;
    char                nameBuf[9];

    R_CachePatch(&statusbar, "BARBACK");
    R_CachePatch(&invBar, "INVBAR");
    R_CachePatch(&chain, "CHAIN");

    R_CachePatch(&statBar, "STATBAR");
    R_CachePatch(&lifeBar, "LIFEBAR");

    // Order of lifeGems changed to match player color index.
    R_CachePatch(&lifeGems[0], "LIFEGEM1");
    R_CachePatch(&lifeGems[1], "LIFEGEM3");
    R_CachePatch(&lifeGems[2], "LIFEGEM2");
    R_CachePatch(&lifeGems[3], "LIFEGEM0");

    R_CachePatch(&godLeft, "GOD1");
    R_CachePatch(&godRight, "GOD2");
    R_CachePatch(&statusbarTopLeft, "LTFCTOP");
    R_CachePatch(&statusbarTopRight, "RTFCTOP");
    R_CachePatch(&invSelectBox, "SELECTBO");
    R_CachePatch(&invPageLeft[0], "INVGEML1");
    R_CachePatch(&invPageLeft[1], "INVGEML2");
    R_CachePatch(&invPageRight[0], "INVGEMR1");
    R_CachePatch(&invPageRight[1], "INVGEMR2");
    R_CachePatch(&negative, "NEGNUM");
    for(i = 0; i < 16; ++i)
    {
        sprintf(nameBuf, "SPINBK%d", i);
        R_CachePatch(&spinBook[i], nameBuf);

        sprintf(nameBuf, "SPFLY%d", i);
        R_CachePatch(&spinFly[i], nameBuf);
    }
    R_CachePatch(&lame, "LAME");

    for(i = 0; i < 10; ++i)
    {
        sprintf(nameBuf, "IN%d", i);
        R_CachePatch(&iNumbers[i], nameBuf);
    }

    for(i = 0; i < 10; ++i)
    {
        sprintf(nameBuf, "SMALLIN%d", i);
        R_CachePatch(&sNumbers[i], nameBuf);
    }

    R_CachePatch(&dpInvItemBox, "ARTIBOX");

    // Inventory item flash anim.
    {
    const char          invItemFlashAnim[5][9] = {
        {"USEARTIA"},
        {"USEARTIB"},
        {"USEARTIC"},
        {"USEARTID"},
        {"USEARTIE"}
    };

    for(i = 0; i < 5; ++i)
    {
        R_CachePatch(&dpInvItemFlash[i], invItemFlashAnim[i]);
    }
    }

    // Ammo icons.
    {
    const char          ammoPic[NUM_AMMO_TYPES][9] = {
        {"INAMGLD"},
        {"INAMBOW"},
        {"INAMBST"},
        {"INAMRAM"},
        {"INAMPNX"},
        {"INAMLOB"}
    };

    for(i = 0; i < 10; ++i)
    {
        R_CachePatch(&ammoIcons[i], ammoPic[i]);
    }
    }

    // Key cards.
    R_CachePatch(&keys[0], "ykeyicon");
    R_CachePatch(&keys[1], "gkeyicon");
    R_CachePatch(&keys[2], "bkeyicon");
}

void ST_loadData(void)
{
    ST_loadGraphics();
}

static void initData(hudstate_t* hud)
{
    int                 i;
    int                 player = hud - hudStates;
    player_t*           plr = &players[player];

    hud->firstTime = true;
    hud->stopped = true;
    hud->showBar = 0.0f;
    hud->alpha = 0.0f;

    hud->tomePlay = 0;
    hud->statusbarCounterAlpha = 0.0f;
    hud->blended = false;
    hud->oldAmmoIconIdx = -1;
    hud->oldReadyWeapon = -1;
    hud->oldHealth = -1;
    hud->currentAmmoIconIdx = 0;

    hud->statusbarActive = true;

    for(i = 0; i < 3; ++i)
    {
        hud->keyBoxes[i] = false;
    }

    hud->inventory.flags = HIF_IS_DIRTY;
    hud->inventory.hideTics = 0;

    ST_HUDUnHide(player, HUE_FORCE);
}

void ST_createWidgets(int player)
{
    static int          largeammo = 1994; // Means "n/a".

    ammotype_t          ammoType;
    boolean             found;
    hudstate_t*         hud = &hudStates[player];
    player_t*           plr = &players[player];
    int                 lvl = (plr->powers[PT_WEAPONLEVEL2]? 1 : 0);

    // Ready weapon ammo.
    //// \todo Only supports one type of ammo per weapon.
    found = false;
    for(ammoType = 0; ammoType < NUM_AMMO_TYPES && !found; ++ammoType)
    {
        if(!weaponInfo[plr->readyWeapon][plr->class].mode[lvl].ammoType[ammoType])
            continue; // Weapon does not take this ammo.

        STlib_initNum(&hud->wReadyWeapon, ST_AMMOX, ST_AMMOY, iNumbers,
                      &plr->ammo[ammoType].owned, &hud->statusbarActive,
                      ST_AMMOWIDTH, &hud->statusbarCounterAlpha);

        found = true;
    }
    if(!found) // Weapon requires no ammo at all.
    {
        // HERETIC.EXE returns an address beyond plr->ammo[NUM_AMMO_TYPES]
        // if weaponInfo[plr->readyWeapon].ammo == am_noammo
        // ...obviously a bug.

        //STlib_initNum(&wReadyWeapon, ST_AMMOX, ST_AMMOY, iNumbers,
        //              &plr->ammo[weaponinfo[plr->readyWeapon].ammo],
        //              &statusbarActive, ST_AMMOWIDTH, &statusbarCounterAlpha);

        // Ready weapon ammo.
        STlib_initNum(&hud->wReadyWeapon, ST_AMMOX, ST_AMMOY, iNumbers, &largeammo,
                      &hud->statusbarActive, ST_AMMOWIDTH, &hud->statusbarCounterAlpha);
    }

    // Ready weapon icon
    STlib_initMultIcon(&hud->wCurrentAmmoIcon, ST_AMMOICONX, ST_AMMOICONY,
                       ammoIcons, &hud->currentAmmoIconIdx,
                       &hud->statusbarActive, &hud->statusbarCounterAlpha);

    // The last weapon type.
    hud->wReadyWeapon.data = plr->readyWeapon;

    // Health num.
    STlib_initNum(&hud->wHealth, ST_HEALTHX, ST_HEALTHY, iNumbers,
                  &plr->health, &hud->statusbarActive, ST_HEALTHWIDTH,
                  &hud->statusbarCounterAlpha);

    // Armor percentage - should be colored later.
    STlib_initNum(&hud->wArmor, ST_ARMORX, ST_ARMORY, iNumbers,
                  &plr->armorPoints, &hud->statusbarActive, ST_ARMORWIDTH,
                  &hud->statusbarCounterAlpha);

    // Frags sum.
    STlib_initNum(&hud->wFrags, ST_FRAGSX, ST_FRAGSY, iNumbers,
                  &hud->fragsCount, &hud->fragsOn, ST_FRAGSWIDTH,
                  &hud->statusbarCounterAlpha);

    // KeyBoxes 0-2.
    STlib_initBinIcon(&hud->wKeyBoxes[0], ST_KEY0X, ST_KEY0Y, &keys[0],
                      &hud->keyBoxes[0], &hud->keyBoxes[0], 0,
                      &hud->statusbarCounterAlpha);

    STlib_initBinIcon(&hud->wKeyBoxes[1], ST_KEY1X, ST_KEY1Y, &keys[1],
                      &hud->keyBoxes[1], &hud->keyBoxes[1], 0,
                      &hud->statusbarCounterAlpha);

    STlib_initBinIcon(&hud->wKeyBoxes[2], ST_KEY2X, ST_KEY2Y, &keys[2],
                      &hud->keyBoxes[2], &hud->keyBoxes[2], 0,
                      &hud->statusbarCounterAlpha);
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
    ST_createWidgets(player);

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

void ST_ResizeInventory(cvar_t* unused)
{
    int                 i;
    uint                maxVisSlots;

    if(cfg.inventorySlotMaxVis)
        maxVisSlots = cfg.inventorySlotMaxVis;
    else
        maxVisSlots = NUM_INVENTORYITEM_TYPES - 1;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        hudstate_t*         hud = &hudStates[i];

        if(hud->inventory.varCursorPos >= maxVisSlots - 1)
        {
            hud->inventory.varCursorPos = maxVisSlots - 1;
        }
        hud->inventory.flags |= HIF_IS_DIRTY;
    }
}

/**
 * Console command to change the size of the status bar.
 */
DEFCC(CCmdStatusBarSize)
{
    int                 min = 1, max = 20, *val = &cfg.statusbarScale;

    if(!stricmp(argv[1], "+"))
        (*val)++;
    else if(!stricmp(argv[1], "-"))
        (*val)--;
    else
        *val = strtol(argv[1], NULL, 0);

    *val = MINMAX_OF(min, *val, max);

    // Update the view size if necessary.
    R_SetViewSize(cfg.screenBlocks);
    ST_HUDUnHide(CONSOLEPLAYER, HUE_FORCE); // so the user can see the change.

    return true;
}
