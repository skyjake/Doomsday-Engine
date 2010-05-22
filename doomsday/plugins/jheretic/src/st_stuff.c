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
#include "hu_stuff.h"
#include "p_tick.h" // for P_IsPaused
#include "p_player.h"
#include "am_map.h"
#include "hu_lib.h"
#include "hu_inventory.h"
#include "hu_log.h"
#include "p_inventory.h"

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

// Counter Cheat flags.
#define CCH_KILLS           0x01
#define CCH_ITEMS           0x02
#define CCH_SECRET          0x04
#define CCH_KILLS_PRCNT     0x08
#define CCH_ITEMS_PRCNT     0x10
#define CCH_SECRET_PRCNT    0x20

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
    boolean         inited;
    boolean         stopped;
    int             hideTics;
    float           hideAmount;

    float           showBar; // Slide statusbar amount 1.0 is fully open.
    float           alpha; // Fullscreen hud alpha value.

    boolean         statusbarActive; // Whether main statusbar is active.

    boolean         hitCenterFrame;
    int             currentInvItemFlash;
    int             currentAmmoIconIdx; // Current ammo icon index.
    boolean         keyBoxes[3]; // Holds key-type for each key box on bar.
    int             fragsCount; // Number of frags so far in deathmatch.

    int             tomePlay;
    int             healthMarker;
    int             chainWiggle;

    int             oldAmmoIconIdx;
    int             oldReadyWeapon;
    int             readyAmmo;
    int             health;
    int             armor;

    int             widgetGroupNames[NUM_UIWIDGET_GROUPS];
} hudstate_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void updateViewWindow(cvar_t* cvar);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static hudstate_t hudStates[MAXPLAYERS];

static patchinfo_t statusbar;
static patchinfo_t statusbarTopLeft;
static patchinfo_t statusbarTopRight;
static patchinfo_t chain;
static patchinfo_t statBar;
static patchinfo_t lifeBar;
static patchinfo_t invBar;
static patchinfo_t lifeGems[4];
static patchinfo_t ammoIcons[11];
static patchinfo_t dpInvItemFlash[5];
static patchinfo_t spinBook[16];
static patchinfo_t spinFly[16];
static patchinfo_t keys[NUM_KEY_TYPES];
static patchinfo_t godLeft;
static patchinfo_t godRight;

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

    {"hud-cheat-counter", 0, CVT_BYTE, &cfg.counterCheat, 0, 63},
    {"hud-cheat-counter-scale", 0, CVT_FLOAT, &cfg.counterCheatScale, .1f, 1},
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

static void shadeChain(int x, int y, float alpha)
{
    DGL_Disable(DGL_TEXTURING);

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

    DGL_Enable(DGL_TEXTURING);
}

void drawChainWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define ORIGINX (-ST_WIDTH/2)
#define ORIGINY (0)

    static int theirColors[] = {
        144, // Green.
        197, // Yellow.
        150, // Red.
        220  // Blue.
    };
    int chainY;
    float healthPos, gemXOffset, gemglow;
    int x, y, w, h, gemNum;
    float cw, rgb[3];
    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    float yOffset = ST_HEIGHT*(1-hud->showBar);

    if(!hud->statusbarActive)
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;

    chainY = -9;
    if(hud->healthMarker != plr->plr->mo->health)
        chainY += hud->chainWiggle;

    healthPos = MINMAX_OF(0, hud->healthMarker / 100.f, 1);

    if(!IS_NETGAME)
        gemNum = 2; // Always use the red gem in single player.
    else
        gemNum = cfg.playerColor[player];
    gemglow = healthPos;

    // Draw the chain.
    x = ORIGINX+21;
    y = ORIGINY+chainY;
    w = ST_WIDTH - 21 - 28;
    h = 8;
    cw = (float) w / chain.width;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, yOffset, 0);

    DGL_SetPatch(chain.id, DGL_REPEAT, DGL_CLAMP);

    DGL_Color4f(1, 1, 1, iconAlpha);

    gemXOffset = (w - lifeGems[gemNum].width) * healthPos;

    if(gemXOffset > 0)
    {   // Left chain section.
        float cw = gemXOffset / chain.width;

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
        float cw = (w - gemXOffset - lifeGems[gemNum].width) / chain.width;

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
    DGL_Color4f(1, 1, 1, iconAlpha);
    M_DrawPatch(lifeGems[gemNum].id, x + gemXOffset, chainY);

    shadeChain(ORIGINX, ORIGINY-ST_HEIGHT, iconAlpha/2);

    // How about a glowing gem?
    DGL_BlendMode(BM_ADD);
    DGL_Bind(Get(DD_DYNLIGHT_TEXTURE));

    R_GetColorPaletteRGBf(0, rgb, theirColors[gemNum], false);
    DGL_DrawRect(x + gemXOffset - 11, chainY - 6, 41, 24, rgb[0], rgb[1], rgb[2], gemglow - (1 - iconAlpha));

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

    if(!hud->statusbarActive)
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;

    if(!(iconAlpha < 1))
    {   // We can just render the full thing as normal.
        // Top bits.
        DGL_Color4f(1, 1, 1, 1);
        M_DrawPatch(statusbarTopLeft.id, ORIGINX, ORIGINY-10);
        M_DrawPatch(statusbarTopRight.id, ORIGINX+290, ORIGINY-10);

        // Faces.
        M_DrawPatch(statusbar.id, ORIGINX, ORIGINY);

        if(P_GetPlayerCheats(plr) & CF_GODMODE)
        {
            M_DrawPatch(godLeft.id, ORIGINX+16, ORIGINY+9);
            M_DrawPatch(godRight.id, ORIGINX+287, ORIGINY+9);
        }

        if(!Hu_InventoryIsOpen(player))
        {
            if(deathmatch)
                M_DrawPatch(statBar.id, ORIGINX+34, ORIGINY+2);
            else
                M_DrawPatch(lifeBar.id, ORIGINX+34, ORIGINY+2);
        }
        else
        {
            M_DrawPatch(invBar.id, ORIGINX+34, ORIGINY+2);
        }
    }
    else
    {
        DGL_Color4f(1, 1, 1, iconAlpha);

        // Top bits.
        M_DrawPatch(statusbarTopLeft.id, ORIGINX, ORIGINY-10);
        M_DrawPatch(statusbarTopRight.id, ORIGINX+290, ORIGINY-10);

        DGL_SetPatch(statusbar.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);

        // Top border.
        DGL_DrawCutRectTiled(ORIGINX+34, ORIGINY, 248, 2, 320, 42, 34, 0, ORIGINX, ORIGINY, 0, 0);

        // Chain background.
        DGL_DrawCutRectTiled(ORIGINX+34, ORIGINY+33, 248, 9, 320, 42, 34, 33, ORIGINX, ORIGINY+191, 16, 8);

        // Faces.
        if(P_GetPlayerCheats(plr) & CF_GODMODE)
        {
            // If GOD mode we need to cut windows
            DGL_DrawCutRectTiled(ORIGINX, ORIGINY, 34, 42, 320, 42, 0, 0, ORIGINX+16, ORIGINY+9, 16, 8);
            DGL_DrawCutRectTiled(ORIGINX+282, ORIGINY, 38, 42, 320, 42, 282, 0, ORIGINX+287, ORIGINY+9, 16, 8);

            M_DrawPatch(godLeft.id, ORIGINX+16, ORIGINY+9);
            M_DrawPatch(godRight.id, ORIGINX+287, ORIGINY+9);
        }
        else
        {
            DGL_DrawCutRectTiled(ORIGINX, ORIGINY, 34, 42, 320, 42, 0, 0, ORIGINX, ORIGINY, 0, 0);
            DGL_DrawCutRectTiled(ORIGINX+282, ORIGINY, 38, 42, 320, 42, 282, 0, ORIGINX, ORIGINY, 0, 0);
        }

        if(!Hu_InventoryIsOpen(player))
        {
            if(deathmatch)
                M_DrawPatch(statBar.id, ORIGINX+34, ORIGINY+2);
            else
                M_DrawPatch(lifeBar.id, ORIGINX+34, ORIGINY+2);
        }
        else
        {
            M_DrawPatch(invBar.id, ORIGINX+34, ORIGINY+2);
        }
    }

    *drawnWidth = WIDTH;
    *drawnHeight = HEIGHT;

#undef WIDTH
#undef HEIGHT
#undef ORIGINX
#undef ORIGINY
}

void ST_updateWidgets(int player)
{
    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    int lvl = (plr->powers[PT_WEAPONLEVEL2]? 1 : 0);
    ammotype_t ammoType;
    boolean found;
    int i;

    // Must redirect the pointer if the ready weapon has changed.
    found = false;
    for(ammoType = 0; ammoType < NUM_AMMO_TYPES && !found; ++ammoType)
    {
        if(!weaponInfo[plr->readyWeapon][plr->class].mode[lvl].ammoType[ammoType])
            continue; // Weapon does not use this type of ammo.

        //// \todo Only supports one type of ammo per weapon.
        hud->readyAmmo = plr->ammo[ammoType].owned;

        if(hud->oldReadyWeapon != plr->readyWeapon)
            hud->currentAmmoIconIdx = (int) ammoType;

        found = true;
    }

    if(!found) // Weapon takes no ammo at all.
    {
        hud->readyAmmo = 1994; // Means n/a.
        hud->currentAmmoIconIdx = -1;
    }

    // Update keycard multiple widgets.
    for(i = 0; i < 3; ++i)
    {
        hud->keyBoxes[i] = plr->keys[i] ? true : false;
    }

    hud->fragsCount = 0;
    for(i = 0; i < MAXPLAYERS; ++i)
        if(players[i].plr->inGame)
            hud->fragsCount += plr->frags[i] * (i != player ? 1 : -1);

    hud->health = plr->health;
    hud->armor = plr->armorPoints;
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
                    int timeleft = plr->powers[PT_WEAPONLEVEL2] / 35;

                    if(hud->tomePlay != timeleft)
                    {
                        hud->tomePlay = timeleft;
                        S_LocalSound(SFX_KEYUP, NULL);
                    }
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
    if(!hud->statusbarActive || !deathmatch || Hu_InventoryIsOpen(player))
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
    if(!hud->statusbarActive || deathmatch || Hu_InventoryIsOpen(player))
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
    if(!hud->statusbarActive || Hu_InventoryIsOpen(player))
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;
    if(hud->armor == 1994)
        return;

    dd_snprintf(buf, 20, "%i", hud->armor);

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

void drawSBarKeysWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define ORIGINX (-ST_WIDTH/2)
#define ORIGINY (-ST_HEIGHT)

    typedef struct {
        int x, y;
    } loc_t;
    static const loc_t elements[] = {
        { ORIGINX+ST_KEY0X, ORIGINY+ST_KEY0Y },
        { ORIGINX+ST_KEY1X, ORIGINY+ST_KEY1Y },
        { ORIGINX+ST_KEY2X, ORIGINY+ST_KEY2Y }
    };

    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    float yOffset = ST_HEIGHT*(1-hud->showBar);
    int i;
    if(!hud->statusbarActive || Hu_InventoryIsOpen(player))
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, yOffset, 0);

    // Draw keys.
    for(i = 0; i < 3; ++i)
    {
        const loc_t* loc = &elements[i];
        if(!hud->keyBoxes[i])
            continue;
        WI_DrawPatch4(keys[i].id, loc->x, loc->y, NULL, GF_FONTB, false, DPF_ALIGN_TOPLEFT, 1, 1, 1, iconAlpha);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, -yOffset, 0);

    // \fixme calculate dimensions properly!
    *drawnWidth = 0;
    *drawnHeight = 0;

#undef ORIGINY
#undef ORIGINX
}

void drawSBarReadyWeaponWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX+ST_AMMOX)
#define Y                   (ORIGINY+ST_AMMOY)
#define MAXDIGITS           (ST_AMMOWIDTH)
#define TRACKING            (1)

    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    float yOffset = ST_HEIGHT*(1-hud->showBar);
    char buf[20];
    if(!hud->statusbarActive || Hu_InventoryIsOpen(player))
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;
    if(hud->readyAmmo == 1994)
        return;

    dd_snprintf(buf, 20, "%i", hud->readyAmmo);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, yOffset, 0);

    DGL_Color4f(defFontRGB3[CR], defFontRGB3[CG], defFontRGB3[CB], iconAlpha);
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

void drawSBarCurrentAmmoWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX+ST_AMMOICONX)
#define Y                   (ORIGINY+ST_AMMOICONY)

    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    float yOffset = ST_HEIGHT*(1-hud->showBar);
    if(!hud->statusbarActive || Hu_InventoryIsOpen(player))
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;
    if(hud->currentAmmoIconIdx < 0)
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, yOffset, 0);
   
    WI_DrawPatch4(ammoIcons[hud->currentAmmoIconIdx].id, X, Y, NULL, GF_FONTB, false, DPF_ALIGN_TOPLEFT, 1, 1, 1, iconAlpha);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, -yOffset, 0);

    *drawnWidth = ammoIcons[hud->currentAmmoIconIdx].width;
    *drawnHeight = ammoIcons[hud->currentAmmoIconIdx].height;

#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void drawSBarCurrentItemWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define ORIGINX (-ST_WIDTH/2)
#define ORIGINY (-ST_HEIGHT*hud->showBar)

    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    inventoryitemtype_t readyItem;
    if(!hud->statusbarActive || Hu_InventoryIsOpen(player))
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;

    // Current inventory item.
    if((readyItem = P_InventoryReadyItem(player)) != IIT_NONE)
    {
        lumpnum_t patch;
        int x, y;
        
        if(hud->currentInvItemFlash > 0)
        {
            patch = dpInvItemFlash[hud->currentInvItemFlash % 5].id;
            x = ORIGINX+ST_INVITEMX + 2;
            y = ORIGINY+ST_INVITEMY + 1;
        }
        else
        {
            patch = P_GetInvItem(readyItem-1)->patchId;
            x = ORIGINX+ST_INVITEMX;
            y = ORIGINY+ST_INVITEMY;
        }

        DGL_Color4f(1, 1, 1, iconAlpha);
        M_DrawPatch(patch, x, y);

        if(!(hud->currentInvItemFlash > 0))
        {
            uint count = P_InventoryCount(player, readyItem);
            if(count > 1)
            {
                char buf[20];
                dd_snprintf(buf, 20, "%i", count);

                DGL_Color4f(defFontRGB3[CR], defFontRGB3[CG], defFontRGB3[CB], textAlpha);
                GL_DrawTextFragment4(buf, ORIGINX+ST_INVITEMCX, ORIGINY+ST_INVITEMCY, GF_SMALLIN, DTF_ALIGN_TOPRIGHT|DTF_NO_EFFECTS, 2);
            }
        }
    }

    // \fixme calculate dimensions properly!
    *drawnWidth = 0;
    *drawnHeight = 0;

#undef ORIGINY
#undef ORIGINX
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

    hud->currentInvItemFlash = 4;
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
        M_DrawPatch(spinFly[frame].id, 16, 14);
    }
    /// \kludge calculate dimensions properly!
    *drawnWidth = 32;
    *drawnHeight = 32;
}

void drawTombOfPowerWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    player_t* plr = &players[player];

    if(!plr->powers[PT_WEAPONLEVEL2] || plr->morphTics)
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;

    *drawnWidth = 0;
    *drawnHeight = 0;

    if(cfg.tomeCounter || plr->powers[PT_WEAPONLEVEL2] > BLINKTHRESHOLD || !(plr->powers[PT_WEAPONLEVEL2] & 16))
    {
        int frame = (mapTime / 3) & 15;
        float alpha = iconAlpha;
        if(cfg.tomeCounter && plr->powers[PT_WEAPONLEVEL2] < 35)
            alpha *= plr->powers[PT_WEAPONLEVEL2] / 35.0f;
        DGL_Color4f(1, 1, 1, alpha);
        M_DrawPatch(spinBook[frame].id, -13, 13);
        // \fixme Determine the actual center point of the animation at widget creation time.
        *drawnWidth += 25;//spinBook[frame].width;
        *drawnHeight += 25;//spinBook[frame].height;
    }

    if(plr->powers[PT_WEAPONLEVEL2] < cfg.tomeCounter * 35)
    {
#define COUNT_X             (303)
#define COUNT_Y             (30)
#define TRACKING            (2)

        char buf[20];
        int w;

        dd_snprintf(buf, 20, "%i", 1 + plr->powers[PT_WEAPONLEVEL2] / 35);

        DGL_Color4f(defFontRGB3[CR], defFontRGB3[CG], defFontRGB3[CB], textAlpha);
        GL_DrawTextFragment4(buf, 0, *drawnHeight+2, GF_SMALLIN, DTF_ALIGN_TOPRIGHT|DTF_NO_TYPEIN, TRACKING);

        w = GL_TextFragmentWidth2(buf, GF_SMALLIN, TRACKING);
        if(w > *drawnWidth)
            *drawnWidth += w;
        *drawnHeight += GL_TextFragmentHeight(buf, GF_SMALLIN) + 2;

#undef TRACKING
#undef COUNT_Y
#undef COUNT_X
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

/**
 * All drawing for the statusbar starts and ends here.
 */
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

void drawAmmoWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define TRACKING                (1)

    player_t* plr = &players[player];
    hudstate_t* hud = &hudStates[player];
    int lvl = (plr->powers[PT_WEAPONLEVEL2]? 1 : 0);
    ammotype_t ammoType;

    if(hud->statusbarActive || (!(plr->readyWeapon > 0 && plr->readyWeapon < 7)))
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;

    *drawnWidth = 0;
    *drawnHeight = 0;

    /// \todo Only supports one type of ammo per weapon.
    /// For each type of ammo this weapon takes.
    for(ammoType = 0; ammoType < NUM_AMMO_TYPES; ++ammoType)
    {
        const patchinfo_t* dp;
        char buf[20];
        int h;

        if(!weaponInfo[plr->readyWeapon][plr->class].mode[lvl].ammoType[ammoType])
            continue;

        dp = &ammoIcons[plr->readyWeapon - 1];
        dd_snprintf(buf, 20, "%i", plr->ammo[ammoType].owned);

        DGL_Color4f(1, 1, 1, iconAlpha);
        M_DrawPatch2(dp->id, 0, 0, DPF_ALIGN_TOPLEFT|DPF_NO_OFFSET);

        DGL_Color4f(defFontRGB3[CR], defFontRGB3[CG], defFontRGB3[CB], textAlpha);
        GL_DrawTextFragment4(buf, dp->width+2, -2, GF_STATUS, DTF_ALIGN_TOPLEFT|DTF_NO_EFFECTS, TRACKING);

        *drawnWidth += dp->width + 2 + GL_TextFragmentWidth2(buf, GF_STATUS, TRACKING);
        h = GL_TextFragmentHeight(buf, GF_STATUS);
        if(MAX_OF(dp->height, h) > *drawnHeight)
            *drawnHeight = MAX_OF(dp->height, h);
        break;
    }

#undef TRACKING
}

void drawHealthWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define TRACKING                (1)

    player_t* plr = &players[player];
    hudstate_t* hud = &hudStates[player];
    int health = MAX_OF(plr->plr->mo->health, 0);
    char buf[20];
    if(hud->statusbarActive)
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;

    dd_snprintf(buf, 5, "%i", health);

    DGL_Color4f(0, 0, 0, textAlpha * .4f);
    GL_DrawTextFragment4(buf, 2, 1, GF_FONTB, DTF_ALIGN_BOTTOMLEFT|DTF_NO_EFFECTS, TRACKING);

    DGL_Color4f(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    GL_DrawTextFragment4(buf, 0, -1, GF_FONTB, DTF_ALIGN_BOTTOMLEFT|DTF_NO_EFFECTS, TRACKING);

    *drawnWidth = GL_TextFragmentWidth2(buf, GF_FONTB, TRACKING);
    *drawnHeight = GL_TextFragmentHeight(buf, GF_FONTB);

#undef TRACKING
}

void drawArmorWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define TRACKING                (1)

    player_t* plr = &players[player];
    hudstate_t* hud = &hudStates[player];
    char buf[20];

    if(hud->statusbarActive)
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;

    dd_snprintf(buf, 20, "%i", plr->armorPoints);

    DGL_Color4f(defFontRGB3[CR], defFontRGB3[CG], defFontRGB3[CB], textAlpha);
    GL_DrawTextFragment4(buf, -1, -11, GF_STATUS, DTF_ALIGN_TOPLEFT|DTF_NO_EFFECTS, TRACKING);

    *drawnWidth = GL_TextFragmentWidth2(buf, GF_STATUS, TRACKING);
    *drawnHeight = GL_TextFragmentHeight(buf, GF_STATUS);

#undef TRACKING
}

void drawKeysWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    player_t* plr = &players[player];
    hudstate_t* hud = &hudStates[player];
    int x = 0, numDrawnKeys = 0; 

    if(hud->statusbarActive)
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;
    *drawnWidth = 0;
    *drawnHeight = 0;

    if(plr->keys[KT_YELLOW])
    {
        DGL_Color4f(1, 1, 1, iconAlpha);
        M_DrawPatch2(keys[0].id, x, -keys[0].height, DPF_ALIGN_TOPLEFT|DPF_NO_OFFSET);
        x += keys[0].width + 1;
        *drawnWidth += keys[0].width;
        if(keys[0].height > *drawnHeight)
            *drawnHeight = keys[0].height;
        numDrawnKeys++;
    }

    if(plr->keys[KT_GREEN])
    {
        DGL_Color4f(1, 1, 1, iconAlpha);
        M_DrawPatch2(keys[1].id, x, -keys[1].height, DPF_ALIGN_TOPLEFT|DPF_NO_OFFSET);
        x += keys[1].width + 1;
        *drawnWidth += keys[1].width;
        if(keys[1].height > *drawnHeight)
            *drawnHeight = keys[1].height;
        numDrawnKeys++;
    }

    if(plr->keys[KT_BLUE])
    {
        DGL_Color4f(1, 1, 1, iconAlpha);
        M_DrawPatch2(keys[2].id, x, -keys[2].height, DPF_ALIGN_TOPLEFT|DPF_NO_OFFSET);
        x += keys[2].width;
        *drawnWidth += keys[1].width;
        if(keys[2].height > *drawnHeight)
            *drawnHeight = keys[2].height;
        numDrawnKeys++;
    }

    *drawnWidth += (numDrawnKeys-1) * 1;
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
        if(players[i].plr->inGame)
            numFrags += plr->frags[i];
    dd_snprintf(buf, 20, "%i", numFrags);

    DGL_Color4f(defFontRGB3[CR], defFontRGB3[CG], defFontRGB3[CB], textAlpha);
    GL_DrawTextFragment4(buf, 0, 0, GF_STATUS, DTF_ALIGN_BOTTOMLEFT|DTF_NO_EFFECTS, TRACKING);

    *drawnWidth = GL_TextFragmentWidth2(buf, GF_STATUS, TRACKING);
    *drawnHeight = GL_TextFragmentHeight(buf, GF_STATUS);

#undef TRACKING
}

void drawCurrentItemWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    inventoryitemtype_t readyItem;
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
        M_DrawPatch2(dpInvItemBox, 0, 0, DPF_ALIGN_BOTTOMRIGHT|DPF_NO_OFFSET);
        DGL_Color4f(1, 1, 1, iconAlpha);
        M_DrawPatch(dpInvItemFlash[hud->currentInvItemFlash % 5].id, -boxInfo.width, -boxInfo.height + 1);
    }
    else
    {
        readyItem = P_InventoryReadyItem(player);

        if(readyItem != IIT_NONE)
        {
            patchid_t patch = P_GetInvItem(readyItem-1)->patchId;
            uint count;

            DGL_Color4f(1, 1, 1, iconAlpha/2);
            M_DrawPatch2(dpInvItemBox, 0, 0, DPF_ALIGN_BOTTOMRIGHT|DPF_NO_OFFSET);
            DGL_Color4f(1, 1, 1, iconAlpha);
            M_DrawPatch(patch, -boxInfo.width, -boxInfo.height);
            if((count = P_InventoryCount(player, readyItem)) > 1)
            {
                char buf[20];
                DGL_Color4f(defFontRGB3[CR], defFontRGB3[CG], defFontRGB3[CB], textAlpha);
                dd_snprintf(buf, 20, "%i", count);
                GL_DrawTextFragment4(buf, -1, -7, GF_SMALLIN, DTF_ALIGN_TOPRIGHT|DTF_NO_EFFECTS, 2);
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
    /// \kludge calculate the visual dimensions properly!
    *drawnWidth = 31*7+16*2;
    *drawnHeight = INVENTORY_HEIGHT;

#undef INVENTORY_HEIGHT
}

void drawKillsWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    player_t* plr = &players[player];
    char buf[40], tmp[20];

    if(!(cfg.counterCheat & (CCH_KILLS | CCH_KILLS_PRCNT)))
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;

    strcpy(buf, "Kills: ");
    if(cfg.counterCheat & CCH_KILLS)
    {
        sprintf(tmp, "%i/%i ", plr->killCount, totalKills);
        strcat(buf, tmp);
    }
    if(cfg.counterCheat & CCH_KILLS_PRCNT)
    {
        sprintf(tmp, "%s%i%%%s", (cfg.counterCheat & CCH_KILLS ? "(" : ""), totalKills ? plr->killCount * 100 / totalKills : 100, (cfg.counterCheat & CCH_KILLS ? ")" : ""));
        strcat(buf, tmp);
    }

    *drawnHeight = GL_TextHeight(buf, GF_FONTA);
    *drawnWidth = GL_TextWidth(buf, GF_FONTA);
    DGL_Color4f(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    GL_DrawTextFragment3(buf, 0, 0, GF_FONTA, DTF_ALIGN_BOTTOMLEFT|DTF_NO_EFFECTS);
}

void drawItemsWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    player_t* plr = &players[player];
    char buf[40], tmp[20];

    if(!(cfg.counterCheat & (CCH_ITEMS | CCH_ITEMS_PRCNT)))
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;

    strcpy(buf, "Items: ");
    if(cfg.counterCheat & CCH_ITEMS)
    {
        sprintf(tmp, "%i/%i ", plr->itemCount, totalItems);
        strcat(buf, tmp);
    }
    if(cfg.counterCheat & CCH_ITEMS_PRCNT)
    {
        sprintf(tmp, "%s%i%%%s", (cfg.counterCheat & CCH_ITEMS ? "(" : ""), totalItems ? plr->itemCount * 100 / totalItems : 100, (cfg.counterCheat & CCH_ITEMS ? ")" : ""));
        strcat(buf, tmp);
    }

    *drawnHeight = GL_TextHeight(buf, GF_FONTA);
    *drawnWidth = GL_TextWidth(buf, GF_FONTA);
    DGL_Color4f(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    GL_DrawTextFragment3(buf, 0, 0, GF_FONTA, DTF_ALIGN_BOTTOMLEFT|DTF_NO_EFFECTS);
}

void drawSecretsWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    player_t* plr = &players[player];
    char buf[40], tmp[20];

    if(!(cfg.counterCheat & (CCH_SECRET | CCH_SECRET_PRCNT)))
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;

    strcpy(buf, "Secret: ");
    if(cfg.counterCheat & CCH_SECRET)
    {
        sprintf(tmp, "%i/%i ", plr->secretCount, totalSecret);
        strcat(buf, tmp);
    }
    if(cfg.counterCheat & CCH_SECRET_PRCNT)
    {
        sprintf(tmp, "%s%i%%%s", (cfg.counterCheat & CCH_SECRET ? "(" : ""), totalSecret ? plr->secretCount * 100 / totalSecret : 100, (cfg.counterCheat & CCH_SECRET ? ")" : ""));
        strcat(buf, tmp);
    }

    *drawnHeight = GL_TextHeight(buf, GF_FONTA);
    *drawnWidth = GL_TextWidth(buf, GF_FONTA);
    DGL_Color4f(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    GL_DrawTextFragment3(buf, 0, 0, GF_FONTA, DTF_ALIGN_BOTTOMLEFT|DTF_NO_EFFECTS);
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
            { UWG_TOPLEFT, UWGF_ALIGN_TOP|UWGF_ALIGN_LEFT|UWGF_LEFT2RIGHT, PADDING },
            { UWG_TOPLEFT2, UWGF_ALIGN_TOP|UWGF_ALIGN_LEFT|UWGF_LEFT2RIGHT, PADDING },
            { UWG_TOPRIGHT, UWGF_ALIGN_TOP|UWGF_ALIGN_RIGHT|UWGF_RIGHT2LEFT, PADDING },
            { UWG_BOTTOMLEFT, UWGF_ALIGN_BOTTOM|UWGF_ALIGN_LEFT|UWGF_BOTTOM2TOP, PADDING },
            { UWG_BOTTOMLEFT2, UWGF_ALIGN_BOTTOM|UWGF_ALIGN_LEFT|UWGF_LEFT2RIGHT, PADDING },
            { UWG_BOTTOMRIGHT, UWGF_ALIGN_BOTTOM|UWGF_ALIGN_RIGHT|UWGF_RIGHT2LEFT, PADDING },
            { UWG_BOTTOM, UWGF_ALIGN_BOTTOM|UWGF_BOTTOM2TOP, PADDING },
            { UWG_TOP, UWGF_ALIGN_TOP|UWGF_ALIGN_LEFT|UWGF_TOP2BOTTOM, PADDING },
            { UWG_COUNTERS, UWGF_ALIGN_LEFT|UWGF_BOTTOM2TOP, PADDING }
        };
        const uiwidgetdef_t widgetDefs[] = {
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawStatusBarBackground, &cfg.statusbarOpacity, &cfg.statusbarOpacity },
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawSBarInventoryWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawSBarFragsWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawSBarHealthWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawSBarArmorWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawSBarKeysWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawSBarReadyWeaponWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawSBarCurrentAmmoWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawSBarCurrentItemWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawChainWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
            { UWG_TOPLEFT, HUD_AMMO, &cfg.hudScale, 1, drawAmmoWidget, &cfg.hudColor[3], &cfg.hudIconAlpha },
            { UWG_TOPLEFT2, -1, &cfg.hudScale, 1, drawFlightWidget, &cfg.hudColor[3], &cfg.hudIconAlpha },
            { UWG_TOPRIGHT, -1, &cfg.hudScale, 1, drawTombOfPowerWidget, &cfg.hudColor[3], &cfg.hudIconAlpha },
            { UWG_BOTTOMLEFT, HUD_HEALTH, &cfg.hudScale, 1, drawHealthWidget, &cfg.hudColor[3], &cfg.hudIconAlpha },
            { UWG_BOTTOMLEFT, HUD_KEYS, &cfg.hudScale, 1, drawKeysWidget, &cfg.hudColor[3], &cfg.hudIconAlpha },
            { UWG_BOTTOMLEFT, HUD_ARMOR, &cfg.hudScale, 1, drawArmorWidget, &cfg.hudColor[3], &cfg.hudIconAlpha },
            { UWG_BOTTOMLEFT2, -1, &cfg.hudScale, 1, drawFragsWidget, &cfg.hudColor[3], &cfg.hudIconAlpha },
            { UWG_BOTTOMRIGHT, HUD_CURRENTITEM, &cfg.hudScale, 1, drawCurrentItemWidget, &cfg.hudColor[3], &cfg.hudIconAlpha },
            { UWG_BOTTOM, -1, &cfg.hudScale, .75f, drawInventoryWidget, &cfg.hudColor[3], &cfg.hudIconAlpha },
            { UWG_TOP, HUD_LOG, &cfg.msgScale, 1, Hu_LogDrawer, &cfg.hudColor[3], &cfg.hudIconAlpha },
            { UWG_TOP, -1, &cfg.msgScale, 1, Chat_Drawer, &cfg.hudColor[3], &cfg.hudIconAlpha },
            { UWG_COUNTERS, -1, &cfg.counterCheatScale, 1, drawSecretsWidget, &cfg.hudColor[3], &cfg.hudIconAlpha },
            { UWG_COUNTERS, -1, &cfg.counterCheatScale, 1, drawItemsWidget, &cfg.hudColor[3], &cfg.hudIconAlpha },
            { UWG_COUNTERS, -1, &cfg.counterCheatScale, 1, drawKillsWidget, &cfg.hudColor[3], &cfg.hudIconAlpha },
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

    hud->statusbarActive = (fullscreenMode < 2) || (AM_IsActive(AM_MapForPlayer(player)) && (cfg.automapHudDisplay == 0 || cfg.automapHudDisplay == 2) );

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
        posY = y;
        availWidth = width - (drawnWidth > 0 ? drawnWidth + PADDING : 0);
        availHeight = height;
        GUI_DrawWidgets(hud->widgetGroupNames[UWG_TOPLEFT2], 0, posX, posY, availWidth, availHeight, alpha, &drawnWidth, &drawnHeight);

        GUI_DrawWidgets(hud->widgetGroupNames[UWG_TOPRIGHT], 0, x, y, width, height, alpha, &drawnWidth, &drawnHeight);
        GUI_DrawWidgets(hud->widgetGroupNames[UWG_BOTTOMLEFT], 0, x, y, width, height, alpha, &drawnWidth, &drawnHeight);

        posX = x + (drawnWidth > 0 ? drawnWidth + PADDING : 0);
        posY = y;
        availWidth = width - (drawnWidth > 0 ? drawnWidth + PADDING : 0);
        availHeight = height;
        GUI_DrawWidgets(hud->widgetGroupNames[UWG_BOTTOMLEFT2], 0, posX, posY, availWidth, availHeight, alpha, &drawnWidth, &drawnHeight);

        GUI_DrawWidgets(hud->widgetGroupNames[UWG_BOTTOMRIGHT], 0, x, y, width, height, alpha, &drawnWidth, &drawnHeight);
        GUI_DrawWidgets(hud->widgetGroupNames[UWG_BOTTOM], 0, x, y, width, height, alpha, &drawnWidth, &drawnHeight);
        GUI_DrawWidgets(hud->widgetGroupNames[UWG_COUNTERS], 0, x, y, width, height, alpha, &drawnWidth, &drawnHeight);
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

    R_PrecachePatch("BARBACK", &statusbar);
    R_PrecachePatch("INVBAR", &invBar);
    R_PrecachePatch("CHAIN", &chain);

    R_PrecachePatch("STATBAR", &statBar);
    R_PrecachePatch("LIFEBAR", &lifeBar);

    // Order of lifeGems changed to match player color index.
    R_PrecachePatch("LIFEGEM1", &lifeGems[0]);
    R_PrecachePatch("LIFEGEM3", &lifeGems[1]);
    R_PrecachePatch("LIFEGEM2", &lifeGems[2]);
    R_PrecachePatch("LIFEGEM0", &lifeGems[3]);

    R_PrecachePatch("GOD1", &godLeft);
    R_PrecachePatch("GOD2", &godRight);
    R_PrecachePatch("LTFCTOP", &statusbarTopLeft);
    R_PrecachePatch("RTFCTOP", &statusbarTopRight);
    for(i = 0; i < 16; ++i)
    {
        sprintf(nameBuf, "SPINBK%d", i);
        R_PrecachePatch(nameBuf, &spinBook[i]);

        sprintf(nameBuf, "SPFLY%d", i);
        R_PrecachePatch(nameBuf, &spinFly[i]);
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
        R_PrecachePatch(ammoPic[i], &ammoIcons[i]);
    }
    }

    // Key cards.
    R_PrecachePatch("YKEYICON", &keys[0]);
    R_PrecachePatch("GKEYICON", &keys[1]);
    R_PrecachePatch("BKEYICON", &keys[2]);
}

void ST_loadData(void)
{
    ST_loadGraphics();
}

static void initData(hudstate_t* hud)
{
    int i, player = hud - hudStates;
    player_t* plr = &players[player];

    hud->statusbarActive = true;
    hud->stopped = true;
    // Health marker chain animates up to the actual health value.
    hud->healthMarker = 0;
    hud->showBar = 1;

    for(i = 0; i < 3; ++i)
    {
        hud->keyBoxes[i] = false;
    }

    hud->tomePlay = 0;
    hud->oldAmmoIconIdx = -1;
    hud->oldReadyWeapon = -1;
    hud->currentAmmoIconIdx = 0;

    hud->health = 1994;
    hud->readyAmmo = 1994;
    hud->armor = 1994;
    hud->fragsCount = 1994;

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

/**
 * Called when the statusbar scale cvar changes.
 */
static void updateViewWindow(cvar_t* cvar)
{
    R_UpdateViewWindow(true);
    ST_HUDUnHide(CONSOLEPLAYER, HUE_FORCE); // So the user can see the change.
}
