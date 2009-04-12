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

#include "jheretic.h"

#include "d_net.h"
#include "st_lib.h"
#include "hu_stuff.h"
#include "p_tick.h" // for P_IsPaused
#include "p_player.h"
#include "am_map.h"
#include "p_inventory.h"

// MACROS ------------------------------------------------------------------

// Current ammo icon(sbbar).
#define ST_AMMOIMGWIDTH     (24)
#define ST_AMMOICONX        (111)
#define ST_AMMOICONY        (172)

// Inventory.
#define ST_INVENTORYX       (50)
#define ST_INVENTORYY       (160)

// How many inventory slots are visible.
#define NUMVISINVSLOTS      (7)

// Invslot artifact count (relative to each slot).
#define ST_INVCOUNTOFFX     (27)
#define ST_INVCOUNTOFFY     (22)

// Current artifact (sbbar).
#define ST_ARTIFACTWIDTH    (24)
#define ST_ARTIFACTX        (179)
#define ST_ARTIFACTY        (160)

// Current artifact count (sbar).
#define ST_ARTIFACTCWIDTH   (2)
#define ST_ARTIFACTCX       (209)
#define ST_ARTIFACTCY       (182)

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
    int             inventoryTics;
    boolean         inventory;
    int             artifactFlash;

    int             hideTics;
    float           hideAmount;

    boolean         stopped;
    float           showBar; // Slide statusbar amount 1.0 is fully open.
    float           alpha; // Fullscreen hud alpha value.

    float           statusbarCounterAlpha;
    boolean         firstTime; // ST_Start() has just been called.
    boolean         statusbarActive; // Whether left-side main status bar is active.
    int             invSlots[NUMVISINVSLOTS]; // Current inventory slot indices. 0 = none.
    int             invSlotsCount[NUMVISINVSLOTS]; // Current inventory slot count indices. 0 = none.
    int             currentInvIdx; // Current artifact index. 0 = none.
    int             currentAmmoIconIdx; // Current ammo icon index.
    boolean         keyBoxes[3]; // Holds key-type for each key box on bar.
    int             fragsCount; // Number of frags so far in deathmatch.
    boolean         fragsOn; // !deathmatch.
    boolean         blended; // Whether to use alpha blending.

    boolean         hitCenterFrame;
    int             tomePlay;
    int             healthMarker;
    int             chainWiggle;

    int             oldCurrentArtifact;
    int             oldCurrentArtifactCount;
    int             oldAmmoIconIdx;
    int             oldReadyWeapon;
    int             oldHealth;

    // Widgets:
    st_multicon_t   wCurrentArtifact; // Current artifact.
    st_number_t     wCurrentArtifactCount; // Current artifact count.
    st_multicon_t   wInvSlots[NUMVISINVSLOTS]; // Inventory slots.
    st_number_t     wInvSlotsCount[NUMVISINVSLOTS]; // Inventory slot counts.
    st_multicon_t   wCurrentAmmoIcon; // Current ammo icon.
    st_number_t     wReadyWeapon; // Ready-weapon.
    st_number_t     wFrags; // In deathmatch only, summary of frags stats.
    st_number_t     wHealth; // Health.
    st_number_t     wArmor; // Armor.
    st_binicon_t    wKeyBoxes[3]; // Owned keys.
} hudstate_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// Console commands for the HUD/Statusbar.
DEFCC(CCmdStatusBarSize);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void drawINumber(signed int val, int x, int y, float r, float g, float b, float a);
static void drawBNumber(signed int val, int x, int y, float Red, float Green, float Blue, float Alpha);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static hudstate_t hudStates[MAXPLAYERS];

static int fontBNumBase;

// Ammo patch names:
static char ammoPic[][10] = {
    {"INAMGLD"},
    {"INAMBOW"},
    {"INAMBST"},
    {"INAMRAM"},
    {"INAMPNX"},
    {"INAMLOB"}
};

// Artifact patch names:
static char artifactList[][10] = {
    {"USEARTIA"}, // Use artifact flash.
    {"USEARTIB"},
    {"USEARTIC"},
    {"USEARTID"},
    {"USEARTIE"},
    {"ARTIBOX"}, // None.
    {"ARTIINVU"}, // Invulnerability.
    {"ARTIINVS"}, // Invisibility.
    {"ARTIPTN2"}, // Health.
    {"ARTISPHL"}, // Super health.
    {"ARTIPWBK"}, // Tome of Power.
    {"ARTITRCH"}, // Torch.
    {"ARTIFBMB"}, // Fire bomb.
    {"ARTIEGGC"}, // Egg.
    {"ARTISOAR"}, // Fly.
    {"ARTIATLP"} // Teleport.
};

static dpatch_t statusbar;
static dpatch_t statusbarTopLeft;
static dpatch_t statusbarTopRight;
static dpatch_t chain;
static dpatch_t statBar;
static dpatch_t lifeBar;
static dpatch_t invBar;
static dpatch_t lifeGems[4];
static dpatch_t artifactSelectBox;
static dpatch_t invPageLeft;
static dpatch_t invPageLeft2;
static dpatch_t invPageRight;
static dpatch_t invPageRight2;
static dpatch_t iNumbers[10];
static dpatch_t sNumbers[10];
static dpatch_t negative;
static dpatch_t ammoIcons[11];
static dpatch_t artifacts[16];
static dpatch_t spinBook;
static dpatch_t spinFly;
static dpatch_t keys[NUM_KEY_TYPES];
static dpatch_t godLeft;
static dpatch_t godRight;

// CVARs for the HUD/Statusbar
cvar_t sthudCVars[] = {
    // HUD scale
    {"hud-scale", 0, CVT_FLOAT, &PLRPROFILE.hud.scale, 0.1f, 10},

    {"hud-status-size", CVF_PROTECTED, CVT_INT, &PLRPROFILE.statusbar.scale, 1, 20},

    // HUD colour + alpha
    {"hud-color-r", 0, CVT_FLOAT, &PLRPROFILE.hud.color[0], 0, 1},
    {"hud-color-g", 0, CVT_FLOAT, &PLRPROFILE.hud.color[1], 0, 1},
    {"hud-color-b", 0, CVT_FLOAT, &PLRPROFILE.hud.color[2], 0, 1},
    {"hud-color-a", 0, CVT_FLOAT, &PLRPROFILE.hud.color[3], 0, 1},
    {"hud-icon-alpha", 0, CVT_FLOAT, &PLRPROFILE.hud.iconAlpha, 0, 1},

    {"hud-status-alpha", 0, CVT_FLOAT, &PLRPROFILE.statusbar.opacity, 0, 1},
    {"hud-status-icon-a", 0, CVT_FLOAT, &PLRPROFILE.statusbar.counterAlpha, 0, 1},

    // HUD icons
    {"hud-ammo", 0, CVT_BYTE, &PLRPROFILE.hud.shown[HUD_AMMO], 0, 1},
    {"hud-armor", 0, CVT_BYTE, &PLRPROFILE.hud.shown[HUD_ARMOR], 0, 1},
    {"hud-keys", 0, CVT_BYTE, &PLRPROFILE.hud.shown[HUD_KEYS], 0, 1},
    {"hud-health", 0, CVT_BYTE, &PLRPROFILE.hud.shown[HUD_HEALTH], 0, 1},
    {"hud-artifact", 0, CVT_BYTE, &PLRPROFILE.hud.shown[HUD_ARTI], 0, 1},

    // HUD displays
    {"hud-tome-timer", CVF_NO_MAX, CVT_INT, &PLRPROFILE.hud.tomeCounter, 0, 0},
    {"hud-tome-sound", CVF_NO_MAX, CVT_INT, &PLRPROFILE.hud.tomeSound, 0, 0},
    {"hud-inventory-timer", 0, CVT_FLOAT, &PLRPROFILE.inventory.timer, 0, 30},

    {"hud-timer", 0, CVT_FLOAT, &PLRPROFILE.hud.timer, 0, 60},

    {"hud-unhide-damage", 0, CVT_BYTE, &PLRPROFILE.hud.unHide[HUE_ON_DAMAGE], 0, 1},
    {"hud-unhide-pickup-health", 0, CVT_BYTE, &PLRPROFILE.hud.unHide[HUE_ON_PICKUP_HEALTH], 0, 1},
    {"hud-unhide-pickup-armor", 0, CVT_BYTE, &PLRPROFILE.hud.unHide[HUE_ON_PICKUP_ARMOR], 0, 1},
    {"hud-unhide-pickup-powerup", 0, CVT_BYTE, &PLRPROFILE.hud.unHide[HUE_ON_PICKUP_POWER], 0, 1},
    {"hud-unhide-pickup-weapon", 0, CVT_BYTE, &PLRPROFILE.hud.unHide[HUE_ON_PICKUP_WEAPON], 0, 1},
    {"hud-unhide-pickup-ammo", 0, CVT_BYTE, &PLRPROFILE.hud.unHide[HUE_ON_PICKUP_AMMO], 0, 1},
    {"hud-unhide-pickup-key", 0, CVT_BYTE, &PLRPROFILE.hud.unHide[HUE_ON_PICKUP_KEY], 0, 1},
    {"hud-unhide-pickup-invitem", 0, CVT_BYTE, &PLRPROFILE.hud.unHide[HUE_ON_PICKUP_INVITEM], 0, 1},
    {NULL}
};

// Console commands for the HUD/Status bar
ccmd_t sthudCCmds[] = {
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
        gemNum = gs.players[player].color;
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

    shadeChain((hud->statusbarCounterAlpha + PLRPROFILE.statusbar.opacity) /3);

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
        alpha = PLRPROFILE.statusbar.opacity - hud->hideAmount;
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

        if(!hud->inventory)
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

            GL_DrawPatch_CS(16, 167, W_GetNumForName("GOD1"));
            GL_DrawPatch_CS(287, 167, W_GetNumForName("GOD2"));
        }
        else
        {
            DGL_DrawCutRectTiled(0, 158, 34, 42, 320, 42, 0, 0, 0, 158, 0, 0);
            DGL_DrawCutRectTiled(282, 158, 38, 42, 320, 42, 282, 0, 0, 158, 0, 0);
        }

        if(!hud->inventory)
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

    int                 i, x;
    ammotype_t          ammoType;
    boolean             found;
    hudstate_t*         hud = &hudStates[player];
    player_t*           plr = &players[player];
    int                 lvl = (plr->powers[PT_WEAPONLEVEL2]? 1 : 0);

    if(hud->blended)
    {
        hud->statusbarCounterAlpha =
            MINMAX_OF(0.f, PLRPROFILE.statusbar.counterAlpha - hud->hideAmount, 1.f);
    }
    else
        hud->statusbarCounterAlpha = 1.0f;

    // Must redirect the pointer if the ready weapon has changed.
    found = false;
    for(ammoType = 0; ammoType < NUM_AMMO_TYPES && !found; ++ammoType)
    {
        if(!weaponInfo[plr->readyWeapon][plr->pClass].mode[lvl].ammoType[ammoType])
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

    // Current artifact.
    if(hud->artifactFlash)
    {
        hud->currentInvIdx = 5 - hud->artifactFlash;
        hud->artifactFlash--;
        hud->oldCurrentArtifact = -1; // So that the correct artifact fills in after the flash.
    }
    else if(hud->oldCurrentArtifact != plr->readyArtifact ||
            hud->oldCurrentArtifactCount != plr->inventory[plr->invPtr].count)
    {

        if(plr->readyArtifact > 0)
        {
            hud->currentInvIdx = plr->readyArtifact + 5;
        }

        hud->oldCurrentArtifact = plr->readyArtifact;
        hud->oldCurrentArtifactCount = plr->inventory[plr->invPtr].count;
    }

    // Update the inventory.
    x = plr->invPtr - plr->curPos;
    for(i = 0; i < NUMVISINVSLOTS; ++i)
    {
        hud->invSlots[i] = plr->inventory[x + i].type +5; // Plus 5 for useartifact patches.
        hud->invSlotsCount[i] = plr->inventory[x + i].count;
    }
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

        if(!P_IsPaused())
        {
            int                 delta, curHealth;

            if(PLRPROFILE.hud.timer == 0)
            {
                hud->hideTics = hud->hideAmount = 0;
            }
            else
            {
                if(hud->hideTics > 0)
                    hud->hideTics--;
                if(hud->hideTics == 0 && PLRPROFILE.hud.timer > 0 && hud->hideAmount < 1)
                    hud->hideAmount += 0.1f;
            }

            ST_updateWidgets(i);

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
               plr->powers[PT_WEAPONLEVEL2] < PLRPROFILE.hud.tomeSound * 35)
            {
                int                 timeleft =
                    plr->powers[PT_WEAPONLEVEL2] / 35;

                if(hud->tomePlay != timeleft)
                {
                    hud->tomePlay = timeleft;
                    S_LocalSound(SFX_KEYUP, NULL);
                }
            }

            // Turn inventory off after a certain amount of time.
            if(hud->inventory && !(--hud->inventoryTics))
            {
                plr->readyArtifact = plr->inventory[plr->invPtr].type;
                hud->inventory = false;
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
    int                 x, i;
    int                 player = hud - hudStates;
    player_t*           plr = &players[player];
    boolean             refresh = true;

    hud->oldHealth = -1;
    if(!hud->inventory)
    {
        hud->oldCurrentArtifact = 0;
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

        // Current artifact.
        if(plr->readyArtifact > 0)
        {
            STlib_updateMultIcon(&hud->wCurrentArtifact, refresh);
            if(!hud->artifactFlash && plr->inventory[plr->invPtr].count > 1)
                STlib_updateNum(&hud->wCurrentArtifactCount, refresh);
        }
    }
    else
    {   // Draw Inventory.
        x = plr->invPtr - plr->curPos;

        for(i = 0; i < NUMVISINVSLOTS; ++i)
        {
            if(plr->inventory[x + i].type != AFT_NONE)
            {
                STlib_updateMultIcon(&hud->wInvSlots[i], refresh);

                if(plr->inventory[x + i].count > 1)
                    STlib_updateNum(&hud->wInvSlotsCount[i], refresh);
            }
        }

        // Draw selector box.
        GL_DrawPatchLitAlpha(ST_INVENTORYX + plr->curPos * 31, 189, 1,
                             hud->statusbarCounterAlpha, artifactSelectBox.lump);

        // Draw more left indicator.
        if(x != 0)
            GL_DrawPatchLitAlpha(38, 159, 1, hud->statusbarCounterAlpha,
                                 !(mapTime & 4) ? invPageLeft.lump : invPageLeft2.lump);

        // Draw more right indicator.
        if(plr->inventorySlotNum - x > 7)
            GL_DrawPatchLitAlpha(269, 159, 1, hud->statusbarCounterAlpha,
                                 !(mapTime & 4) ? invPageRight.lump : invPageRight2.lump);
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
        hud->inventory = true;

        hud->inventoryTics = (int) (PLRPROFILE.inventory.timer * TICSPERSEC);
        if(hud->inventoryTics < 1)
            hud->inventoryTics = 1;

        ST_HUDUnHide(player, HUE_FORCE);
    }
    else
    {
        hud->inventory = false;
    }
}

boolean ST_IsInventoryVisible(int player)
{
    player_t*           plr;
    hudstate_t*         hud;

    if(player < 0 || player >= MAXPLAYERS)
        return false;

    plr = &players[player];
    if(!((plr->plr->flags & DDPF_LOCAL) && plr->plr->inGame))
        return false;

    hud = &hudStates[player];

    return hud->inventory;
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

    hud->artifactFlash = 4;
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
            GL_DrawPatch_CS(x + 1, y + 1, W_GetNumForName("LAME"));
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
    lumppatch_t        *patch;
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
        patch = W_CacheLumpNum(fontBNumBase + val / 100, PU_CACHE);

        GL_DrawPatchLitAlpha(xpos + 8 - SHORT(patch->width) / 2, y +2, 0, alpha * .4f,
                             fontBNumBase + val / 100);
        DGL_Color4f(red, green, blue, alpha);
        GL_DrawPatch_CS(xpos + 6 - SHORT(patch->width) / 2, y,
                             fontBNumBase + val / 100);
        DGL_Color4f(1, 1, 1, 1);
    }

    val = val % 100;
    xpos += 12;
    if(val > 9 || oldval > 99)
    {
        patch = W_CacheLumpNum(fontBNumBase + val / 10, PU_CACHE);

        GL_DrawPatchLitAlpha(xpos + 8 - SHORT(patch->width) / 2, y +2, 0, alpha * .4f,
                             fontBNumBase + val / 10);
        DGL_Color4f(red, green, blue, alpha);
        GL_DrawPatch_CS(xpos + 6 - SHORT(patch->width) / 2, y,
                             fontBNumBase + val / 10);
        DGL_Color4f(1, 1, 1, 1);
    }

    val = val % 10;
    xpos += 12;
    patch = W_CacheLumpNum(fontBNumBase + val, PU_CACHE);

    GL_DrawPatchLitAlpha(xpos + 8 - SHORT(patch->width) / 2, y +2, 0, alpha * .4f,
                             fontBNumBase + val);
    DGL_Color4f(red, green, blue, alpha);
    GL_DrawPatch_CS(xpos + 6 - SHORT(patch->width) / 2, y,
                             fontBNumBase + val);
    DGL_Color4f(1, 1, 1, 1);
}

static void _DrSmallNumber(int val, int x, int y, boolean skipone, float r,
                           float g, float b, float a)
{
    DGL_Color4f(r, g, b, a);

    if(skipone && val == 1)
    {
        return;
    }

    if(val > 9)
    {
        GL_DrawPatch_CS(x, y, sNumbers[val / 10].lump);
    }

    val = val % 10;
    GL_DrawPatch_CS(x + 4, y, sNumbers[val].lump);
}

static void DrSmallNumber(int val, int x, int y, float r, float g, float b,
                          float a)
{
    _DrSmallNumber(val, x, y, true, r,g,b,a);
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

    if(ev == HUE_FORCE || PLRPROFILE.hud.unHide[ev])
    {
        hudStates[player].hideTics = (PLRPROFILE.hud.timer * TICSPERSEC);
        hudStates[player].hideAmount = 0;
    }
}

static void drawIcons(int player)
{
    int                 frame;
    float               iconAlpha = PLRPROFILE.hud.iconAlpha;
    float               textAlpha = PLRPROFILE.hud.color[3];
    hudstate_t*         hud = &hudStates[player];
    player_t*           plr = &players[player];

    Draw_BeginZoom(PLRPROFILE.hud.scale, 2, 2);

    // Flight icons
    if(plr->powers[PT_FLIGHT])
    {
        int     offset = (PLRPROFILE.hud.shown[HUD_AMMO] && PLRPROFILE.screen.blocks > 10 &&
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
                                         spinFly.lump + 15);
                }
                else
                {
                    GL_DrawPatchLitAlpha(20 + offset, 17, 1, iconAlpha,
                                         spinFly.lump + frame);
                    hud->hitCenterFrame = false;
                }
            }
            else
            {
                if(!hud->hitCenterFrame && (frame != 15 && frame != 0))
                {
                    GL_DrawPatchLitAlpha(20 + offset, 17, 1, iconAlpha,
                                         spinFly.lump + frame);
                    hud->hitCenterFrame = false;
                }
                else
                {
                    GL_DrawPatchLitAlpha(20 + offset, 17, 1, iconAlpha,
                                         spinFly.lump + 15);
                    hud->hitCenterFrame = true;
                }
            }
        }
    }

    Draw_EndZoom();

    Draw_BeginZoom(PLRPROFILE.hud.scale, 318, 2);

    if(plr->powers[PT_WEAPONLEVEL2] && !plr->morphTics)
    {
        if(PLRPROFILE.hud.tomeCounter ||
           plr->powers[PT_WEAPONLEVEL2] > BLINKTHRESHOLD ||
           !(plr->powers[PT_WEAPONLEVEL2] & 16))
        {
            frame = (mapTime / 3) & 15;
            if(PLRPROFILE.hud.tomeCounter &&
               plr->powers[PT_WEAPONLEVEL2] < TICSPERSEC)
            {
                DGL_Color4f(1, 1, 1, plr->powers[PT_WEAPONLEVEL2] / (float) TICSPERSEC);
            }

            GL_DrawPatchLitAlpha(300, 17, 1, iconAlpha,
                                 spinBook.lump + frame);
        }

        if(plr->powers[PT_WEAPONLEVEL2] < PLRPROFILE.hud.tomeCounter * TICSPERSEC)
        {
            _DrSmallNumber(1 + plr->powers[PT_WEAPONLEVEL2] / TICSPERSEC,
                           303, 30, false, 1, 1, 1, textAlpha);
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

    statusbarVisible = (PLRPROFILE.statusbar.scale < 20 ||
        (PLRPROFILE.statusbar.scale == 20 && hud->showBar < 1.0f));

    hud->firstTime = false;

    if(statusbarVisible)
    {
        float               fscale = PLRPROFILE.statusbar.scale / 20.0f;
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
        MINMAX_OF(0.f, hud->alpha - hud->hideAmount - ( 1 - PLRPROFILE.hud.color[3]), 1.f);
    float               iconAlpha =
        MINMAX_OF(0.f, hud->alpha - hud->hideAmount - ( 1 - PLRPROFILE.hud.iconAlpha), 1.f);

    if(PLRPROFILE.hud.shown[HUD_AMMO])
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
                if(!weaponInfo[plr->readyWeapon][plr->pClass].mode[lvl].ammoType[ammoType])
                    continue;

                Draw_BeginZoom(PLRPROFILE.hud.scale, 2, 2);
                GL_DrawPatchLitAlpha(-1, 0, 1, iconAlpha,
                                     W_GetNumForName(ammoPic[plr->readyWeapon - 1]));
                drawINumber(plr->ammo[ammoType].owned, 18, 2,
                            1, 1, 1, textAlpha);

                Draw_EndZoom();
                break;
            }
        }
    }

Draw_BeginZoom(PLRPROFILE.hud.scale, 2, 198);
    if(PLRPROFILE.hud.shown[HUD_HEALTH])
    {
        if(plr->plr->mo->health > 0)
        {
            drawBNumber(plr->plr->mo->health, 2, 180,
                      PLRPROFILE.hud.color[0], PLRPROFILE.hud.color[1], PLRPROFILE.hud.color[2], textAlpha);
        }
        else
        {
            drawBNumber(0, 2, 180, PLRPROFILE.hud.color[0], PLRPROFILE.hud.color[1], PLRPROFILE.hud.color[2], textAlpha);
        }
    }

    if(PLRPROFILE.hud.shown[HUD_ARMOR])
    {
        if(PLRPROFILE.hud.shown[HUD_HEALTH] && PLRPROFILE.hud.shown[HUD_KEYS])
            temp = 158;
        else if(!PLRPROFILE.hud.shown[HUD_HEALTH] && PLRPROFILE.hud.shown[HUD_KEYS])
            temp = 176;
        else if(PLRPROFILE.hud.shown[HUD_HEALTH] && !PLRPROFILE.hud.shown[HUD_KEYS])
            temp = 168;
        else if(!PLRPROFILE.hud.shown[HUD_HEALTH] && !PLRPROFILE.hud.shown[HUD_KEYS])
            temp = 186;

        drawINumber(plr->armorPoints, 6, temp, 1, 1, 1, textAlpha);
    }

    if(PLRPROFILE.hud.shown[HUD_KEYS])
    {
        x = 6;

        // Draw keys above health?
        if(plr->keys[KT_YELLOW])
        {
            GL_DrawPatchLitAlpha(x, PLRPROFILE.hud.shown[HUD_HEALTH]? 172 : 190, 1,
                                 iconAlpha, W_GetNumForName("ykeyicon"));
            x += 11;
        }

        if(plr->keys[KT_GREEN])
        {
            GL_DrawPatchLitAlpha(x, PLRPROFILE.hud.shown[HUD_HEALTH]? 172 : 190, 1,
                                 iconAlpha, W_GetNumForName("gkeyicon"));
            x += 11;
        }

        if(plr->keys[KT_BLUE])
        {
            GL_DrawPatchLitAlpha(x, PLRPROFILE.hud.shown[HUD_HEALTH]? 172 : 190, 1,
                                 iconAlpha, W_GetNumForName("bkeyicon"));
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

Draw_BeginZoom(PLRPROFILE.hud.scale, 2, 198);
        drawINumber(temp, 45, 185, 1, 1, 1, textAlpha);
Draw_EndZoom();
    }

    if(!hud->inventory)
    {
        if(PLRPROFILE.hud.shown[HUD_ARTI] && plr->readyArtifact > 0)
        {
Draw_BeginZoom(PLRPROFILE.hud.scale, 318, 198);

            GL_DrawPatchLitAlpha(286, 166, 1, iconAlpha / 2,
                                 W_GetNumForName("ARTIBOX"));
            GL_DrawPatchLitAlpha(286, 166, 1, iconAlpha,
                                 W_GetNumForName(artifactList[plr->readyArtifact + 5]));  //plus 5 for useartifact flashes
            DrSmallNumber(plr->inventory[plr->invPtr].count, 307, 188, 1,
                          1, 1, textAlpha);
Draw_EndZoom();
        }
    }
    else
    {
        float               invScale;

        invScale = MINMAX_OF(0.25f, PLRPROFILE.hud.scale - 0.25f, 0.8f);

Draw_BeginZoom(invScale, 160, 198);

        x = plr->invPtr - plr->curPos;
        for(i = 0; i < 7; ++i)
        {
            GL_DrawPatchLitAlpha(50 + i * 31, 168, 1, iconAlpha/2, W_GetNumForName("ARTIBOX"));
            if(plr->inventorySlotNum > x + i &&
               plr->inventory[x + i].type != AFT_NONE)
            {
                int                 lump;

                // Plus 5 for useartifact flashes.
                lump = W_GetNumForName(
                    artifactList[plr->inventory[x + i].type + 5]);

                GL_DrawPatchLitAlpha(50 + i * 31, 168, 1,
                                     i == plr->curPos? hud->alpha : iconAlpha,
                                     lump);
                DrSmallNumber(plr->inventory[x + i].count, 69 + i * 31,
                              190, 1, 1, 1,
                              i == plr->curPos? hud->alpha : textAlpha / 2);
            }
        }

        GL_DrawPatchLitAlpha(50 + plr->curPos * 31, 197, 1, hud->alpha,
                             artifactSelectBox.lump);
        if(x != 0)
        {
            GL_DrawPatchLitAlpha(38, 167, 1, iconAlpha,
                                 !(mapTime & 4)? invPageLeft.lump : invPageLeft2.lump);
        }

        if(plr->inventorySlotNum - x > 7)
        {
            GL_DrawPatchLitAlpha(269, 167, 1, iconAlpha,
                                 !(mapTime & 4)? invPageRight.lump : invPageRight2.lump);
        }

Draw_EndZoom();
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
         (PLRPROFILE.automap.hudDisplay == 0 || PLRPROFILE.automap.hudDisplay == 2) );

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
    R_CachePatch(&artifactSelectBox, "SELECTBOX");
    R_CachePatch(&invPageLeft, "INVGEML1");
    R_CachePatch(&invPageLeft2, "INVGEML2");
    R_CachePatch(&invPageRight, "INVGEMR1");
    R_CachePatch(&invPageRight2, "INVGEMR2");
    R_CachePatch(&negative, "NEGNUM");
    R_CachePatch(&spinBook, "SPINBK0");
    R_CachePatch(&spinFly, "SPFLY0");

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

    // Artifact icons (+5 for the use artifact flash patches).
    for(i = 0; i < (NUM_ARTIFACT_TYPES + 5); ++i)
    {
        sprintf(nameBuf, "%s", artifactList[i]);
        R_CachePatch(&artifacts[i], nameBuf);
    }

    // Ammo icons.
    for(i = 0; i < 10; ++i)
    {
        sprintf(nameBuf, "%s", ammoPic[i]);
        R_CachePatch(&ammoIcons[i], nameBuf);
    }

    // Key cards.
    R_CachePatch(&keys[0], "ykeyicon");
    R_CachePatch(&keys[1], "gkeyicon");
    R_CachePatch(&keys[2], "bkeyicon");

    fontBNumBase = W_GetNumForName("FONTB16");
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

    // Ensure the HUD widget lib has been inited.
    STlib_init();

    hud->firstTime = true;
    hud->inventory = false;
    hud->stopped = true;
    hud->showBar = 0.0f;
    hud->alpha = 0.0f;

    hud->tomePlay = 0;
    hud->statusbarCounterAlpha = 0.0f;
    hud->currentInvIdx = 0;
    hud->blended = false;
    hud->oldCurrentArtifact = 0;
    hud->oldCurrentArtifactCount = 0;
    hud->oldAmmoIconIdx = -1;
    hud->oldReadyWeapon = -1;
    hud->oldHealth = -1;
    hud->currentInvIdx = 0;
    hud->currentAmmoIconIdx = 0;

    hud->statusbarActive = true;

    for(i = 0; i < 3; ++i)
    {
        hud->keyBoxes[i] = false;
    }

    for(i = 0; i < NUMVISINVSLOTS; ++i)
    {
        hud->invSlots[i] = 0;
        hud->invSlotsCount[i] = 0;
    }

    ST_HUDUnHide(player, HUE_FORCE);
}

void ST_createWidgets(int player)
{
    static int          largeammo = 1994; // Means "n/a".

    int                 i;
    ammotype_t          ammoType;
    boolean             found;
    int                 width, temp;
    hudstate_t*         hud = &hudStates[player];
    player_t*           plr = &players[player];
    int                 lvl = (plr->powers[PT_WEAPONLEVEL2]? 1 : 0);

    // Ready weapon ammo.
    //// \todo Only supports one type of ammo per weapon.
    found = false;
    for(ammoType = 0; ammoType < NUM_AMMO_TYPES && !found; ++ammoType)
    {
        if(!weaponInfo[plr->readyWeapon][plr->pClass].mode[lvl].ammoType[ammoType])
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

    // Current artifact (stbar not inventory).
    STlib_initMultIcon(&hud->wCurrentArtifact, ST_ARTIFACTX, ST_ARTIFACTY,
                       artifacts, &hud->currentInvIdx,
                       &hud->statusbarActive, &hud->statusbarCounterAlpha);

    // Current artifact count.
    STlib_initNum(&hud->wCurrentArtifactCount, ST_ARTIFACTCX, ST_ARTIFACTCY,
                  sNumbers, &hud->oldCurrentArtifactCount,
                  &hud->statusbarActive, ST_ARTIFACTCWIDTH,
                  &hud->statusbarCounterAlpha);

    // Inventory slots.
    width = artifacts[5].width + 1;
    temp = 0;

    for(i = 0; i < NUMVISINVSLOTS; ++i)
    {
        // Inventory slot icon.
        STlib_initMultIcon(&hud->wInvSlots[i], ST_INVENTORYX + temp,
                           ST_INVENTORYY, artifacts, &hud->invSlots[i],
                           &hud->statusbarActive, &hud->statusbarCounterAlpha);

        // Inventory slot count.
        STlib_initNum(&hud->wInvSlotsCount[i], ST_INVENTORYX + temp + ST_INVCOUNTOFFX,
                      ST_INVENTORYY + ST_INVCOUNTOFFY, sNumbers,
                      &hud->invSlotsCount[i], &hud->statusbarActive,
                      ST_ARTIFACTCWIDTH, &hud->statusbarCounterAlpha);

        temp += width;
    }
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
 * Console command to change the size of the status bar.
 */
DEFCC(CCmdStatusBarSize)
{
    int                 min = 1, max = 20, *val = &PLRPROFILE.statusbar.scale;

    if(!stricmp(argv[1], "+"))
        (*val)++;
    else if(!stricmp(argv[1], "-"))
        (*val)--;
    else
        *val = strtol(argv[1], NULL, 0);

    if(*val < min)
        *val = min;
    if(*val > max)
        *val = max;

    // Update the view size if necessary.
    R_SetViewSize(CONSOLEPLAYER, PLRPROFILE.screen.blocks);
    ST_HUDUnHide(CONSOLEPLAYER, HUE_FORCE); // so the user can see the change.

    return true;
}
