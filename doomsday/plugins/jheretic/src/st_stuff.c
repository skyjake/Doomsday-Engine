/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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

#include <assert.h>

#include "jheretic.h"

#include "d_net.h"
#include "st_lib.h"
#include "hu_stuff.h"
#include "p_tick.h" // for P_IsPaused
#include "p_player.h"
#include "am_map.h"
#include "hu_lib.h"
#include "hu_inventory.h"
#include "p_inventory.h"

// MACROS ------------------------------------------------------------------

// Current ammo icon(sbbar).
#define ST_AMMOIMGWIDTH     (24)
#define ST_AMMOICONX        (111)
#define ST_AMMOICONY        (172)

// Inventory.
#define ST_INVENTORYX       (50)
#define ST_INVENTORYY       (160)

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
    boolean         blended; // Whether to use alpha blending.

    int             tomePlay;
    int             healthMarker;
    int             chainWiggle;

    int             oldAmmoIconIdx;
    int             oldReadyWeapon;
    int             oldHealth;

    // Widgets:
    st_multiicon_t  wCurrentAmmoIcon; // Current ammo icon.
    st_number_t     wReadyWeapon; // Ready-weapon.
    st_number_t     wFrags; // In deathmatch only, summary of frags stats.
    st_number_t     wHealth; // Health.
    st_number_t     wArmor; // Armor.
    st_icon_t       wKeyBoxes[3]; // Owned keys.
} hudstate_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void drawINumber(signed int val, int x, int y, float r, float g, float b, float a);
static void updateViewWindow(cvar_t* cvar);

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
static dpatch_t iNumbers[10];
static dpatch_t negative;
static dpatch_t ammoIcons[11];
static dpatch_t dpInvItemFlash[5];
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

    {"hud-status-size", 0, CVT_INT, &cfg.statusbarScale, 1, 20, updateViewWindow},

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
 * Register CVARs and CCmds for the HUD/Status bar
 */
void ST_Register(void)
{
    int                 i;

    for(i = 0; sthudCVars[i].name; ++i)
        Con_AddVariable(sthudCVars + i);

    Hu_InventoryRegister();
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

    R_GetColorPaletteRGBf(0, rgb, theirColors[gemNum], false);
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
    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    float alpha;

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
        GL_DrawPatch(0, -10, statusbarTopLeft.lump);
        GL_DrawPatch(290, -10, statusbarTopRight.lump);

        // Faces.
        GL_DrawPatch(0, 0, statusbar.lump);

        if(P_GetPlayerCheats(plr) & CF_GODMODE)
        {
            GL_DrawPatch(16, 9, godLeft.lump);
            GL_DrawPatch(287, 9, godRight.lump);
        }

        if(!Hu_InventoryIsOpen(player))
        {
            if(deathmatch)
                GL_DrawPatch(34, 2, statBar.lump);
            else
                GL_DrawPatch(34, 2, lifeBar.lump);
        }
        else
        {
            GL_DrawPatch(34, 2, invBar.lump);
        }
    }
    else
    {
        DGL_Color4f(1, 1, 1, alpha);

        // Top bits.
        GL_DrawPatch_CS(0, -10, statusbarTopLeft.lump);
        GL_DrawPatch_CS(290, -10, statusbarTopRight.lump);

        DGL_SetPatch(statusbar.lump, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);

        // Top border.
        DGL_DrawCutRectTiled(34, 0, 248, 2, 320, 42, 34, 0, 0, 0, 0, 0);

        // Chain background.
        DGL_DrawCutRectTiled(34, 33, 248, 9, 320, 42, 34, 33, 0, 191, 16, 8);

        // Faces.
        if(P_GetPlayerCheats(plr) & CF_GODMODE)
        {
            // If GOD mode we need to cut windows
            DGL_DrawCutRectTiled(0, 0, 34, 42, 320, 42, 0, 0, 16, 9, 16, 8);
            DGL_DrawCutRectTiled(282, 0, 38, 42, 320, 42, 282, 0, 287, 9, 16, 8);

            GL_DrawPatch_CS(16, 9, godLeft.lump);
            GL_DrawPatch_CS(287, 9, godRight.lump);
        }
        else
        {
            DGL_DrawCutRectTiled(0, 0, 34, 42, 320, 42, 0, 0, 0, 0, 0, 0);
            DGL_DrawCutRectTiled(282, 0, 38, 42, 320, 42, 282, 0, 0, 0, 0, 0);
        }

        if(!Hu_InventoryIsOpen(player))
        {
            if(deathmatch)
                GL_DrawPatch_CS(34, 2, statBar.lump);
            else
                GL_DrawPatch_CS(34, 2, lifeBar.lump);
        }
        else
        {
            GL_DrawPatch_CS(34, 2, invBar.lump);
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

    // Update keycard multiple widgets.
    for(i = 0; i < 3; ++i)
    {
        hud->keyBoxes[i] = plr->keys[i] ? true : false;
    }

    // Used by wFrags widget.
    hud->fragsCount = 0;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->inGame)
            continue;

        hud->fragsCount += plr->frags[i] * (i != player ? 1 : -1);
    }
}

void ST_Ticker(void)
{
    int                 i;

    Hu_InventoryTicker();

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t*           plr = &players[i];
        hudstate_t*         hud = &hudStates[i];

        if(!(plr->plr->inGame && (plr->plr->flags & DDPF_LOCAL)))
            continue;

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

static void drawWidgets(hudstate_t* hud)
{
    int                 i, player = hud - hudStates;
    player_t*           plr = &players[player];
    float               alpha = hud->statusbarCounterAlpha;

    hud->oldHealth = -1;
    if(!Hu_InventoryIsOpen(player))
    {
        inventoryitemtype_t readyItem;

        // Draw all the counters.

        // Frags.
        if(deathmatch)
            STlib_DrawNum(&hud->wFrags, alpha);
        else
            STlib_DrawNum(&hud->wHealth, alpha);

        // Draw armor.
        STlib_DrawNum(&hud->wArmor, alpha);

        // Draw keys.
        for(i = 0; i < 3; ++i)
        {
            if(hud->keyBoxes[i])
                STlib_DrawIcon(&hud->wKeyBoxes[i], alpha);
        }

        STlib_DrawNum(&hud->wReadyWeapon, alpha);
        STlib_DrawMultiIcon(&hud->wCurrentAmmoIcon, hud->currentAmmoIconIdx, alpha);

        // Current inventory item.
        if((readyItem = P_InventoryReadyItem(player)) != IIT_NONE)
        {
            int                 x, y;
            lumpnum_t           patch;

            if(hud->currentInvItemFlash > 0)
            {
                patch = dpInvItemFlash[hud->currentInvItemFlash % 5].lump;
                x = ST_INVITEMX + 2;
                y = ST_INVITEMY + 1;
            }
            else
            {
                patch = P_GetInvItem(readyItem-1)->patchLump;
                x = ST_INVITEMX;
                y = ST_INVITEMY;
            }

            DGL_Color4f(1, 1, 1, alpha);
            GL_DrawPatch_CS(x, y, patch);

            if(!(hud->currentInvItemFlash > 0))
            {
                uint                count =
                    P_InventoryCount(player, readyItem);

                if(count > 1)
                    Hu_DrawSmallNum(count, ST_INVITEMCWIDTH, ST_INVITEMCX,
                                    ST_INVITEMCY, alpha);
            }
        }
    }
    else
    {   // Draw Inventory.
        Hu_InventoryDraw2(player, ST_INVENTORYX, ST_INVENTORYY, alpha);
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

static void drawPatch(lumpnum_t lump, int x, int y, float alpha, boolean usePatchOffset)
{
    patchinfo_t info;
    int w, h;

    if(!R_GetPatchInfo(lump, &info))
        return;

    w = (int) info.width;
    h = (int) info.height;

    if(usePatchOffset)
    {
        x += (int) info.offset;
        y += (int) info.topOffset;
    }

    DGL_SetPatch(lump, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
    DGL_Color4f(1, 1, 1, alpha);
    DGL_Begin(DGL_QUADS);
        DGL_TexCoord2f(0, 0, 0);
        DGL_Vertex2f(x, y);
        DGL_TexCoord2f(0, 1, 0);
        DGL_Vertex2f(x + w, y);
        DGL_TexCoord2f(0, 1, 1);
        DGL_Vertex2f(x + w, y + h);
        DGL_TexCoord2f(0, 0, 1);
        DGL_Vertex2f(x, y + h);
    DGL_End();
}

int drawFlightWidget(int player, float textAlpha, float iconAlpha)
{
    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];

    if(!plr->powers[PT_FLIGHT])
        return 0;

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
        drawPatch(spinFly[frame].lump, 16, 14, iconAlpha, true);
    }
    return 32;
}

int drawTombOfPowerWidget(int player, float textAlpha, float iconAlpha)
{
    player_t* plr = &players[player];

    if(!plr->powers[PT_WEAPONLEVEL2] || plr->morphTics)
        return 0;

    if(cfg.tomeCounter || plr->powers[PT_WEAPONLEVEL2] > BLINKTHRESHOLD || !(plr->powers[PT_WEAPONLEVEL2] & 16))
    {
        int frame = (mapTime / 3) & 15;
        float alpha = iconAlpha;
        if(cfg.tomeCounter && plr->powers[PT_WEAPONLEVEL2] < 35)
            alpha *= plr->powers[PT_WEAPONLEVEL2] / 35.0f;
        drawPatch(spinBook[frame].lump, -12, 13, alpha, true);
    }

    if(plr->powers[PT_WEAPONLEVEL2] < cfg.tomeCounter * 35)
    {
#define COUNT_X             (303)
#define COUNT_Y             (30)

        int val = 1 + plr->powers[PT_WEAPONLEVEL2] / 35;
        if(val > 9)
            drawPatch(dpSmallNumbers[(val / 10) % 10].lump, -12+3, 13+13, textAlpha, true);
        val = val % 10;
        drawPatch(dpSmallNumbers[val].lump, -12+7, 13+13, textAlpha, true);

#undef COUNT_X
#undef COUNT_Y
    }

    return 24;
}

static boolean pickStatusbarScalingStrategy(int viewportWidth, int viewportHeight)
{
    float a = (float)viewportWidth/viewportHeight;
    float b = (float)SCREENWIDTH/SCREENHEIGHT;

    if(INRANGE_OF(a, b, .001f))
        return true; // The same, so stretch.
    if(Con_GetByte("rend-hud-nostretch") || !INRANGE_OF(a, b, .18f))
        return false; // No stretch; translate and scale to fit.
    // Otherwise stretch.
    return true;
}

/**
 * All drawing for the statusbar starts and ends here.
 */
static void drawStatusbar(int player, int x, int y, int viewW, int viewH)
{
    hudstate_t* hud = &hudStates[player];
    int needWidth = ((viewW >= viewH)? (float)viewH/SCREENHEIGHT : (float)viewW/SCREENWIDTH) * ST_WIDTH;

    if(!hud->statusbarActive)
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);

    /// \fixme Should be done higher up rather than counteract (hence the fudge).
    if(pickStatusbarScalingStrategy(viewW, viewH))
    {
        DGL_Scalef((float)viewW/needWidth+0.01f/*fudge*/, 1, 1);
    }
    else
    {
        if(needWidth > viewW)
            DGL_Scalef((float)viewW/needWidth, (float)viewW/needWidth, 1);
    }

    DGL_Translatef(-ST_WIDTH/2, -ST_HEIGHT * hud->showBar, 0);

    drawStatusBarBackground(player);

    DGL_Translatef(0, -(SCREENHEIGHT-ST_HEIGHT), 0);

    drawChain(hud);
    drawWidgets(hud);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

int drawAmmoWidget(int player, float textAlpha, float iconAlpha)
{
    player_t* plr = &players[player];
    hudstate_t* hud = &hudStates[player];
    int lvl = (plr->powers[PT_WEAPONLEVEL2]? 1 : 0);
    ammotype_t ammoType;
    int drawnWidth = 0;

    if(hud->statusbarActive || (!(plr->readyWeapon > 0 && plr->readyWeapon < 7)))
        return 0;

    /// \todo Only supports one type of ammo per weapon.
    /// For each type of ammo this weapon takes.
    for(ammoType = 0; ammoType < NUM_AMMO_TYPES; ++ammoType)
    {
        const dpatch_t* dp;
        if(!weaponInfo[plr->readyWeapon][plr->class].mode[lvl].ammoType[ammoType])
            continue;
        dp = &ammoIcons[plr->readyWeapon - 1];
        drawPatch(dp->lump, 0, 0, iconAlpha, false);
        drawINumber(plr->ammo[ammoType].owned, dp->width+2, -2, 1, 1, 1, textAlpha);
        /// \kludge calculate the visual width properly!
        drawnWidth = dp->width + 2 + (iNumbers[0].width+1) * 3;
        break;
    }
    return drawnWidth;
}

int drawHealthWidget(int player, float textAlpha, float iconAlpha)
{
    player_t* plr = &players[player];
    hudstate_t* hud = &hudStates[player];
    int health = MAX_OF(plr->plr->mo->health, 0);
    if(hud->statusbarActive)
        return 0;
    HU_DrawBNumber(health, -1, -17, cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    return 17;
}

int drawArmorWidget(int player, float textAlpha, float iconAlpha)
{
    player_t* plr = &players[player];
    hudstate_t* hud = &hudStates[player];
    if(hud->statusbarActive)
        return 0;
    drawINumber(plr->armorPoints, 0, -11, 1, 1, 1, textAlpha);
    /// \kludge calculate the visual width properly!
    return (iNumbers[0].width+1) * 3;
}

int drawKeysWidget(int player, float textAlpha, float iconAlpha)
{
    player_t* plr = &players[player];
    hudstate_t* hud = &hudStates[player];
    int x = 0, drawnHeight = 0;

    if(hud->statusbarActive)
        return 0;

    if(plr->keys[KT_YELLOW])
    {
        drawPatch(keys[0].lump, x, -keys[0].height, iconAlpha, false);
        x += keys[0].width + 2;
        drawnHeight = keys[0].height;
    }

    if(plr->keys[KT_GREEN])
    {
        drawPatch(keys[1].lump, x, -keys[1].height, iconAlpha, false);
        x += keys[1].width + 2;
        if(keys[1].height > drawnHeight)
            drawnHeight = keys[1].height;
    }

    if(plr->keys[KT_BLUE])
    {
        drawPatch(keys[2].lump, x, -keys[2].height, iconAlpha, false);
        x += keys[2].width;
        if(keys[2].height > drawnHeight)
            drawnHeight = keys[2].height;
    }
    return drawnHeight;
}

int drawFragsWidget(int player, float textAlpha, float iconAlpha)
{
    player_t* plr = &players[player];
    hudstate_t* hud = &hudStates[player];
    int i, numFrags = 0;
    if(hud->statusbarActive || !deathmatch)
        return 0;
    for(i = 0; i < MAXPLAYERS; ++i)
        if(players[i].plr->inGame)
            numFrags += plr->frags[i];
    drawINumber(numFrags, 0, -13, 1, 1, 1, textAlpha);
    /// \kludge calculate the visual width properly!
    return (iNumbers[0].width+1) * 3;
}

int drawCurrentItemWidget(int player, float textAlpha, float iconAlpha)
{
    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    inventoryitemtype_t readyItem;

    if(hud->statusbarActive || Hu_InventoryIsOpen(player))
        return 0;

    if(hud->currentInvItemFlash > 0)
    {
        drawPatch(dpInvItemBox.lump, -29, -29, iconAlpha / 2, false);
        drawPatch(dpInvItemFlash[hud->currentInvItemFlash % 5].lump, -29, -28, iconAlpha, true);
        return dpInvItemBox.width;
    }

    readyItem = P_InventoryReadyItem(player);

    if(readyItem != IIT_NONE)
    {
        lumpnum_t patch = P_GetInvItem(readyItem-1)->patchLump;
        uint count;

        drawPatch(dpInvItemBox.lump, -29, -29, iconAlpha / 2, false);
        drawPatch(patch, -29, -29, iconAlpha, true);
        if((count = P_InventoryCount(player, readyItem)) > 1)
            Hu_DrawSmallNum(count, ST_INVITEMCWIDTH, -1, -7, textAlpha);
    }
    return dpInvItemBox.width;
}

int drawInventoryWidget(int player, float textAlpha, float iconAlpha)
{
#define INVENTORY_HEIGHT    29

    hudstate_t* hud = &hudStates[player];
    if(hud->statusbarActive || !Hu_InventoryIsOpen(player))
        return 0;
    Hu_InventoryDraw(player, 0, -INVENTORY_HEIGHT, textAlpha, iconAlpha);
    /// \kludge calculate the visual width properly!
    return 31*7+16*2;

#undef INVENTORY_HEIGHT
}

uiwidget_t widgetsTopLeft[] = {
    { HUD_AMMO, 1, drawAmmoWidget }
};

uiwidget_t widgetsTopLeft2[] = {
    { -1, 1, drawFlightWidget, &cfg.hudColor[3], &cfg.hudIconAlpha }
};

uiwidget_t widgetsTopRight[] = {
    { -1, 1, drawTombOfPowerWidget, &cfg.hudColor[3], &cfg.hudIconAlpha }
};

uiwidget_t widgetsBottomLeft[] = {
    { HUD_HEALTH, 1, drawHealthWidget },
    { HUD_KEYS, 1, drawKeysWidget },
    { HUD_ARMOR, 1, drawArmorWidget }
};

uiwidget_t widgetsBottomLeft2[] = {
    { -1, 1, drawFragsWidget }
};

uiwidget_t widgetsBottomRight[] = {
    { HUD_CURRENTITEM, 1, drawCurrentItemWidget }
};

uiwidget_t widgetsBottom[] = {
    { -1, .75f, drawInventoryWidget }
};

void ST_Drawer(int player, int fullscreenmode, boolean refresh)
{
    hudstate_t* hud;
    player_t* plr;

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

    if(hud->statusbarActive || fullscreenmode != 3)
    {
        int viewW, viewH, x, y, width, height;
        float textAlpha = MINMAX_OF(0.f, /*hud->alpha -*/ 1 - hud->hideAmount - ( 1 - cfg.hudColor[3]), 1.f);
        float iconAlpha = MINMAX_OF(0.f, /*hud->alpha -*/ 1 - hud->hideAmount - ( 1 - cfg.hudIconAlpha), 1.f);
        float scale;

        R_GetViewPort(player, NULL, NULL, &viewW, &viewH);

        if(viewW >= viewH)
            scale = (float)viewH/SCREENHEIGHT;
        else
            scale = (float)viewW/SCREENWIDTH;

        if(hud->statusbarActive)
            scale *= cfg.statusbarScale / 20.0f;
        else
            scale *= cfg.hudScale;

        x = y = 0;
        width = viewW / scale;
        height = viewH / scale;

        if(!hud->statusbarActive)
        {
#define INSET_BORDER 2 // In fixed 320x200 pixels

            x += INSET_BORDER;
            y += INSET_BORDER;
            height -= INSET_BORDER*2;
            width -= INSET_BORDER*2;

#undef INSET_BORDER
        }

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();

        DGL_Scalef(scale, scale, 1);

        /**
         * Draw widgets.
         */
        {
        int posX, posY;

        posX = x + width/2;
        posY = y + height;
        drawStatusbar(player, posX, posY, viewW, viewH);

        posX = x;
        posY = y;
        UI_DrawWidgets(widgetsTopLeft, sizeof(widgetsTopLeft)/sizeof(widgetsTopLeft[0]),
            posX, posY, player, textAlpha, iconAlpha, HOT_TLEFT);

        posX = x;
        posY = y;
        UI_DrawWidgets(widgetsTopLeft2, sizeof(widgetsTopLeft2)/sizeof(widgetsTopLeft2[0]),
            posX, posY, player, textAlpha, iconAlpha, HOT_TLEFT);

        posX = x + width;
        posY = y;
        UI_DrawWidgets(widgetsTopRight, sizeof(widgetsTopRight)/sizeof(widgetsTopRight[0]),
            posX, posY, player, textAlpha, iconAlpha, HOT_TRIGHT);

        posX = x;
        posY = y + height;
        UI_DrawWidgets(widgetsBottomLeft, sizeof(widgetsBottomLeft)/sizeof(widgetsBottomLeft[0]),
            posX, posY, player, textAlpha, iconAlpha, HOT_BLEFT);

        posX = x + 43;
        posY = y + height;
        UI_DrawWidgets(widgetsBottomLeft2, sizeof(widgetsBottomLeft2)/sizeof(widgetsBottomLeft2[0]),
            posX, posY, player, textAlpha, iconAlpha, HOT_LEFT);

        posX = x + width;
        posY = y + height;
        UI_DrawWidgets(widgetsBottomRight, sizeof(widgetsBottomRight)/sizeof(widgetsBottomRight[0]),
            posX, posY, player, textAlpha, iconAlpha, HOT_BRIGHT);

        posX = x + width/2;
        posY = y + height;
        UI_DrawWidgets(widgetsBottom, sizeof(widgetsBottom)/sizeof(widgetsBottom[0]),
            posX, posY, player, textAlpha, iconAlpha, HOT_B);
        }

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }
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

    for(i = 0; i < NUM_AMMO_TYPES; ++i)
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
    hud->statusbarActive = true;
    hud->stopped = true;
    hud->oldHealth = -1;
    // Health marker chain animates up to the actual health value.
    hud->healthMarker = 0;
    hud->blended = false;
    hud->showBar = 0.0f;
    hud->statusbarCounterAlpha = 1.f;

    for(i = 0; i < 3; ++i)
    {
        hud->keyBoxes[i] = false;
    }

    hud->tomePlay = 0;
    hud->oldAmmoIconIdx = -1;
    hud->oldReadyWeapon = -1;
    hud->currentAmmoIconIdx = 0;

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

        STlib_InitNum(&hud->wReadyWeapon, ST_AMMOX, ST_AMMOY, iNumbers,
                      &plr->ammo[ammoType].owned, ST_AMMOWIDTH, 1);

        found = true;
    }
    if(!found) // Weapon requires no ammo at all.
    {
        // HERETIC.EXE returns an address beyond plr->ammo[NUM_AMMO_TYPES]
        // if weaponInfo[plr->readyWeapon].ammo == am_noammo
        // ...obviously a bug.

        //STlib_InitNum(&wReadyWeapon, ST_AMMOX, ST_AMMOY, iNumbers,
        //              &plr->ammo[weaponinfo[plr->readyWeapon].ammo],
        //              &statusbarActive, ST_AMMOWIDTH, &statusbarCounterAlpha);

        // Ready weapon ammo.
        STlib_InitNum(&hud->wReadyWeapon, ST_AMMOX, ST_AMMOY, iNumbers, &largeammo,
                      ST_AMMOWIDTH, 1);
    }

    // Ready weapon icon
    STlib_InitMultiIcon(&hud->wCurrentAmmoIcon, ST_AMMOICONX, ST_AMMOICONY,
                        ammoIcons, 1);

    // Health num.
    STlib_InitNum(&hud->wHealth, ST_HEALTHX, ST_HEALTHY, iNumbers,
                  &plr->health, ST_HEALTHWIDTH, 1);

    // Armor percentage - should be colored later.
    STlib_InitNum(&hud->wArmor, ST_ARMORX, ST_ARMORY, iNumbers,
                  &plr->armorPoints, ST_ARMORWIDTH, 1);

    // Frags sum.
    STlib_InitNum(&hud->wFrags, ST_FRAGSX, ST_FRAGSY, iNumbers,
                  &hud->fragsCount, ST_FRAGSWIDTH, 1);

    // KeyBoxes 0-2.
    STlib_InitIcon(&hud->wKeyBoxes[0], ST_KEY0X, ST_KEY0Y, &keys[0], 1);
    STlib_InitIcon(&hud->wKeyBoxes[1], ST_KEY1X, ST_KEY1Y, &keys[1], 1);
    STlib_InitIcon(&hud->wKeyBoxes[2], ST_KEY2X, ST_KEY2Y, &keys[2], 1);
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

/**
 * Called when the statusbar scale cvar changes.
 */
static void updateViewWindow(cvar_t* cvar)
{
    R_UpdateViewWindow(true);
    ST_HUDUnHide(CONSOLEPLAYER, HUE_FORCE); // So the user can see the change.
}
