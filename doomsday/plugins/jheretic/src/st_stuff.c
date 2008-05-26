/**\file
 *\section License
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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
 *
 * In addition, as a special exception, we, the authors of deng
 * give permission to link the code of our release of deng with
 * the libjhexen and/or the libjheretic libraries (or with modified
 * versions of it that use the same license as the libjhexen or
 * libjheretic libraries), and distribute the linked executables.
 * You must obey the GNU General Public License in all respects for
 * all of the code used other than ?libjhexen or libjheretic?. If
 * you modify this file, you may extend this exception to your
 * version of the file, but you are not obligated to do so. If you
 * do not wish to do so, delete this exception statement from your version.
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

#define FMAKERGBA(r, g, b, a) ( \
    (byte)(0xff * (r)) + ((byte)(0xff * (g)) << 8) + \
    ((byte)(0xff * (b)) << 16) + ((byte)(0xff * (a)) << 24))

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

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void ST_drawWidgets(boolean refresh);

// Console commands for the HUD/Statusbar.
DEFCC(CCmdHUDShow);
DEFCC(CCmdStatusBarSize);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void shadeChain(void);
static void drawINumber(signed int val, int x, int y, float r, float g, float b, float a);
static void drawBNumber(signed int val, int x, int y, float Red, float Green, float Blue, float Alpha);
static void drawChain(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int playerKeys = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int inventoryTics;
static boolean inventory = false;

static int currentPalette;
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

static int artifactFlash;

static int hudHideTics;
static float hudHideAmount;

static boolean stopped = true;

// Slide statusbar amount 1.0 is fully open.
static float showBar = 0.0f;

// Fullscreen hud alpha value.
static float hudAlpha = 0.0f;

static float statusbarCounterAlpha = 0.0f;

// ST_Start() has just been called.
static boolean firstTime;

// Whether left-side main status bar is active.
static boolean statusbarActive;

// Current inventory slot indices. 0 = none.
static int invSlots[NUMVISINVSLOTS];

// Current inventory slot count indices. 0 = none.
static int invSlotsCount[NUMVISINVSLOTS];

// Current artifact index. 0 = none.
static int currentInvIdx = 0;

// Current ammo icon index.
static int currentAmmoIconIdx;

// Holds key-type for each key box on bar.
static boolean keyBoxes[3];

// Number of frags so far in deathmatch.
static int fragsCount;

// !deathmatch.
static boolean fragsOn;

// Whether to use alpha blending.
static boolean blended = false;

static int healthMarker;
static int chainWiggle;

static int oldCurrentArtifact = 0;
static int oldCurrentArtifactCount = 0;

static int oldAmmoIconIdx = -1;

static int oldReadyWeapon = -1;
static int oldHealth = -1;

static dpatch_t statusbar;
static dpatch_t statusbarTopLeft;
static dpatch_t statusbarTopRight;
static dpatch_t chain;
static dpatch_t statBar;
static dpatch_t invBar;
static dpatch_t lifeGem;
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

// Current artifact widget.
static st_multicon_t wCurrentArtifact;

// Current artifact count widget.
static st_number_t wCurrentArtifactCount;

// Inventory slot widgets.
static st_multicon_t wInvSlots[NUMVISINVSLOTS];

// Inventory slot count widgets.
static st_number_t wInvSlotsCount[NUMVISINVSLOTS];

// Current ammo icon widget.
static st_multicon_t wCurrentAmmoIcon;

// Ready-weapon widget.
static st_number_t wReadyWeapon;

// In deathmatch only, summary of frags stats.
static st_number_t wFrags;

// Health widget.
static st_number_t wHealth;

// Armor widget.
static st_number_t wArmor;

// Keycard widgets.
static st_binicon_t wKeyBoxes[3];

// CVARs for the HUD/Statusbar
cvar_t sthudCVars[] =
{
    // HUD scale
    {"hud-scale", 0, CVT_FLOAT, &cfg.hudScale, 0.1f, 10},

    {"hud-status-size", CVF_PROTECTED, CVT_INT, &cfg.statusbarScale, 1, 20},

    // HUD colour + alpha
    {"hud-color-r", 0, CVT_FLOAT, &cfg.hudColor[0], 0, 1},
    {"hud-color-g", 0, CVT_FLOAT, &cfg.hudColor[1], 0, 1},
    {"hud-color-b", 0, CVT_FLOAT, &cfg.hudColor[2], 0, 1},
    {"hud-color-a", 0, CVT_FLOAT, &cfg.hudColor[3], 0, 1},
    {"hud-icon-alpha", 0, CVT_FLOAT, &cfg.hudIconAlpha, 0, 1},

    {"hud-status-alpha", 0, CVT_FLOAT, &cfg.statusbarAlpha, 0, 1},
    {"hud-status-icon-a", 0, CVT_FLOAT, &cfg.statusbarCounterAlpha, 0, 1},

    // HUD icons
    {"hud-ammo", 0, CVT_BYTE, &cfg.hudShown[HUD_AMMO], 0, 1},
    {"hud-armor", 0, CVT_BYTE, &cfg.hudShown[HUD_ARMOR], 0, 1},
    {"hud-keys", 0, CVT_BYTE, &cfg.hudShown[HUD_KEYS], 0, 1},
    {"hud-health", 0, CVT_BYTE, &cfg.hudShown[HUD_HEALTH], 0, 1},
    {"hud-artifact", 0, CVT_BYTE, &cfg.hudShown[HUD_ARTI], 0, 1},

    // HUD displays
    {"hud-tome-timer", CVF_NO_MAX, CVT_INT, &cfg.tomeCounter, 0, 0},
    {"hud-tome-sound", CVF_NO_MAX, CVT_INT, &cfg.tomeSound, 0, 0},
    {"hud-inventory-timer", 0, CVT_FLOAT, &cfg.inventoryTimer, 0, 30},

    {"hud-frags-all", 0, CVT_BYTE, &huShowAllFrags, 0, 1},

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
    {"showhud",     "",     CCmdHUDShow},
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

void ST_loadGraphics(void)
{
    int                 i;
    char                nameBuf[9];

    R_CachePatch(&statusbar, "BARBACK");
    R_CachePatch(&invBar, "INVBAR");
    R_CachePatch(&chain, "CHAIN");

    if(deathmatch)
    {
        R_CachePatch(&statBar, "STATBAR");
    }
    else
    {
        R_CachePatch(&statBar, "LIFEBAR");
    }
    if(!IS_NETGAME)
    {                           // single player game uses red life gem
        R_CachePatch(&lifeGem, "LIFEGEM2");
    }
    else
    {
        sprintf(nameBuf, "LIFEGEM%d", CONSOLEPLAYER);
        R_CachePatch(&lifeGem, nameBuf);
    }

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
    for(i = 0; i < (NUMARTIFACTS + 5); ++i)
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

void SB_SetClassData(void)
{
    // Nothing to do
}

/**
 * Changes the class of the given player. Will not work if the player
 * is currently morphed.
 */
void SB_ChangePlayerClass(player_t *player, int newclass)
{
    // Don't change if morphed.
    if(player->morphTics)
        return;
}

void ST_loadData(void)
{
    currentPalette = W_GetNumForName("PLAYPAL");

    ST_loadGraphics();
}

void ST_initData(void)
{
    int                 i;

    firstTime = true;

    currentInvIdx = 0;
    currentAmmoIconIdx = 0;

    statusbarActive = true;

    for(i = 0; i < 3; ++i)
    {
        keyBoxes[i] = false;
    }

    for(i = 0; i < NUMVISINVSLOTS; ++i)
    {
        invSlots[i] = 0;
        invSlotsCount[i] = 0;
    }

    STlib_init();
    ST_HUDUnHide(HUE_FORCE);
}

void ST_updateWidgets(void)
{
    static int          largeammo = 1994; // Means "n/a".

    int                 i, x;
    ammotype_t          ammoType;
    boolean             found;
    player_t           *plr = &players[CONSOLEPLAYER];
    int                 lvl = (plr->powers[PT_WEAPONLEVEL2]? 1 : 0);

    if(blended)
    {
        statusbarCounterAlpha = cfg.statusbarCounterAlpha - hudHideAmount;
        CLAMP(statusbarCounterAlpha, 0.0f, 1.0f);
    }
    else
        statusbarCounterAlpha = 1.0f;

    // Must redirect the pointer if the ready weapon has changed.
    found = false;
    for(ammoType = 0; ammoType < NUM_AMMO_TYPES && !found; ++ammoType)
    {
        if(!weaponInfo[plr->readyWeapon][plr->class].mode[lvl].ammoType[ammoType])
            continue; // Weapon does not use this type of ammo.

        //// \todo Only supports one type of ammo per weapon.
        wReadyWeapon.num = &plr->ammo[ammoType];

        if(oldAmmoIconIdx != plr->ammo[ammoType] || oldReadyWeapon != plr->readyWeapon)
            currentAmmoIconIdx = plr->readyWeapon - 1;

        found = true;
    }

    if(!found) // Weapon takes no ammo at all.
    {
        wReadyWeapon.num = &largeammo;
        currentAmmoIconIdx = -1;
    }

    wReadyWeapon.data = plr->readyWeapon;

    // Update keycard multiple widgets.
    for(i = 0; i < 3; ++i)
    {
        keyBoxes[i] = plr->keys[i] ? true : false;
    }

    // Used by wFrags widget.
    fragsOn = deathmatch && statusbarActive;
    fragsCount = 0;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->inGame)
            continue;

        fragsCount += plr->frags[i] * (i != CONSOLEPLAYER ? 1 : -1);
    }

    // Current artifact.
    if(artifactFlash)
    {
        currentInvIdx = 5 - artifactFlash;
        artifactFlash--;
        oldCurrentArtifact = -1; // So that the correct artifact fills in after the flash.
    }
    else if(oldCurrentArtifact != plr->readyArtifact ||
            oldCurrentArtifactCount != plr->inventory[plr->invPtr].count)
    {

        if(plr->readyArtifact > 0)
        {
            currentInvIdx = plr->readyArtifact + 5;
        }

        oldCurrentArtifact = plr->readyArtifact;
        oldCurrentArtifactCount = plr->inventory[plr->invPtr].count;
    }

    // Update the inventory.
    x = plr->invPtr - plr->curPos;
    for(i = 0; i < NUMVISINVSLOTS; ++i)
    {
        invSlots[i] = plr->inventory[x + i].type +5; // Plus 5 for useartifact patches.
        invSlotsCount[i] = plr->inventory[x + i].count;
    }
}

void ST_createWidgets(void)
{
    static int          largeammo = 1994; // Means "n/a".

    int                 i;
    ammotype_t          ammoType;
    boolean             found;
    int                 width, temp;
    player_t           *plyr = &players[CONSOLEPLAYER];
    int                 lvl = (plyr->powers[PT_WEAPONLEVEL2]? 1 : 0);

    // Ready weapon ammo.
    //// \todo Only supports one type of ammo per weapon.
    found = false;
    for(ammoType = 0; ammoType < NUM_AMMO_TYPES && !found; ++ammoType)
    {
        if(!weaponInfo[plyr->readyWeapon][plyr->class].mode[lvl].ammoType[ammoType])
            continue; // Weapon does not take this ammo.

        STlib_initNum(&wReadyWeapon, ST_AMMOX, ST_AMMOY, iNumbers, &plyr->ammo[ammoType],
                      &statusbarActive, ST_AMMOWIDTH, &statusbarCounterAlpha);

        found = true;
    }
    if(!found) // Weapon requires no ammo at all.
    {
        // HERETIC.EXE returns an address beyond plyr->ammo[NUM_AMMO_TYPES]
        // if weaponInfo[plyr->readyWeapon].ammo == am_noammo
        // ...obviously a bug.

        //STlib_initNum(&wReadyWeapon, ST_AMMOX, ST_AMMOY, iNumbers,
        //              &plyr->ammo[weaponinfo[plyr->readyWeapon].ammo],
        //              &statusbarActive, ST_AMMOWIDTH, &statusbarCounterAlpha);

        // Ready weapon ammo.
        STlib_initNum(&wReadyWeapon, ST_AMMOX, ST_AMMOY, iNumbers, &largeammo,
                      &statusbarActive, ST_AMMOWIDTH, &statusbarCounterAlpha);
    }

    // Ready weapon icon
    STlib_initMultIcon(&wCurrentAmmoIcon, ST_AMMOICONX, ST_AMMOICONY, ammoIcons, &currentAmmoIconIdx,
                       &statusbarActive, &statusbarCounterAlpha);

    // The last weapon type.
    wReadyWeapon.data = plyr->readyWeapon;

    // Health num.
    STlib_initNum(&wHealth, ST_HEALTHX, ST_HEALTHY, iNumbers,
                      &plyr->health, &statusbarActive, ST_HEALTHWIDTH, &statusbarCounterAlpha);

    // Armor percentage - should be colored later.
    STlib_initNum(&wArmor, ST_ARMORX, ST_ARMORY, iNumbers,
                      &plyr->armorPoints, &statusbarActive, ST_ARMORWIDTH, &statusbarCounterAlpha );

    // Frags sum.
    STlib_initNum(&wFrags, ST_FRAGSX, ST_FRAGSY, iNumbers, &fragsCount,
                  &fragsOn, ST_FRAGSWIDTH, &statusbarCounterAlpha);

    // KeyBoxes 0-2.
    STlib_initBinIcon(&wKeyBoxes[0], ST_KEY0X, ST_KEY0Y, &keys[0], &keyBoxes[0],
                       &keyBoxes[0], 0, &statusbarCounterAlpha);

    STlib_initBinIcon(&wKeyBoxes[1], ST_KEY1X, ST_KEY1Y, &keys[1], &keyBoxes[1],
                       &keyBoxes[1], 0, &statusbarCounterAlpha);

    STlib_initBinIcon(&wKeyBoxes[2], ST_KEY2X, ST_KEY2Y, &keys[2], &keyBoxes[2],
                       &keyBoxes[2], 0, &statusbarCounterAlpha);

    // Current artifact (stbar not inventory).
    STlib_initMultIcon(&wCurrentArtifact, ST_ARTIFACTX, ST_ARTIFACTY, artifacts, &currentInvIdx,
                       &statusbarActive, &statusbarCounterAlpha);

    // Current artifact count.
    STlib_initNum(&wCurrentArtifactCount, ST_ARTIFACTCX, ST_ARTIFACTCY, sNumbers,
                      &oldCurrentArtifactCount, &statusbarActive, ST_ARTIFACTCWIDTH, &statusbarCounterAlpha);

    // Inventory slots.
    width = artifacts[5].width + 1;
    temp = 0;

    for(i = 0; i < NUMVISINVSLOTS; ++i)
    {
        // Inventory slot icon.
        STlib_initMultIcon(&wInvSlots[i], ST_INVENTORYX + temp , ST_INVENTORYY, artifacts, &invSlots[i],
                       &statusbarActive, &statusbarCounterAlpha);

        // Inventory slot count.
        STlib_initNum(&wInvSlotsCount[i], ST_INVENTORYX + temp + ST_INVCOUNTOFFX, ST_INVENTORYY + ST_INVCOUNTOFFY, sNumbers,
                      &invSlotsCount[i], &statusbarActive, ST_ARTIFACTCWIDTH, &statusbarCounterAlpha);

        temp += width;
    }
}

void ST_Start(void)
{
    if(!stopped)
        ST_Stop();

    ST_initData();
    ST_createWidgets();
    stopped = false;
}

void ST_Stop(void)
{
    if(stopped)
        return;
    stopped = true;
}

void ST_Init(void)
{
    ST_loadData();
}

void ST_Inventory(boolean show)
{
    if(show)
    {
        inventory = true;

        inventoryTics = (int) (cfg.inventoryTimer * TICSPERSEC);
        if(inventoryTics < 1)
            inventoryTics = 1;

        ST_HUDUnHide(HUE_FORCE);
    }
    else
        inventory = false;
}

boolean ST_IsInventoryVisible(void)
{
    return inventory;
}

void ST_InventoryFlashCurrent(player_t *player)
{
    if(player == &players[CONSOLEPLAYER])
        artifactFlash = 4;
}

void ST_Ticker(void)
{
    static int          tomePlay = 0;

    int                 delta;
    int                 curHealth;
    player_t           *plyr = &players[CONSOLEPLAYER];

    if(!P_IsPaused())
    {
        if(cfg.hudTimer == 0)
        {
            hudHideTics = hudHideAmount = 0;
        }
        else
        {
            if(hudHideTics > 0)
                hudHideTics--;
            if(hudHideTics == 0 && cfg.hudTimer > 0 && hudHideAmount < 1)
                hudHideAmount += 0.1f;
        }
    }

    ST_updateWidgets();

    if(levelTime & 1)
    {
        chainWiggle = P_Random() & 1;
    }
    curHealth = plyr->plr->mo->health;
    if(curHealth < 0)
    {
        curHealth = 0;
    }
    if(curHealth < healthMarker)
    {
        delta = (healthMarker - curHealth) >> 2;
        if(delta < 1)
        {
            delta = 1;
        }
        else if(delta > 8)
        {
            delta = 8;
        }
        healthMarker -= delta;
    }
    else if(curHealth > healthMarker)
    {
        delta = (curHealth - healthMarker) >> 2;
        if(delta < 1)
        {
            delta = 1;
        }
        else if(delta > 8)
        {
            delta = 8;
        }
        healthMarker += delta;
    }

    // Tome of Power countdown sound.
    if(plyr->powers[PT_WEAPONLEVEL2] &&
       plyr->powers[PT_WEAPONLEVEL2] < cfg.tomeSound * 35)
    {
        int     timeleft = plyr->powers[PT_WEAPONLEVEL2] / 35;

        if(tomePlay != timeleft)
        {
            tomePlay = timeleft;
            S_LocalSound(sfx_keyup, NULL);
        }
    }

    // Turn inventory off after a certain amount of time.
    if(inventory && !(--inventoryTics))
    {
        plyr->readyArtifact = plyr->inventory[plyr->invPtr].type;
        inventory = false;
    }
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

static void shadeChain(void)
{
    float               shadea =
        (statusbarCounterAlpha + cfg.statusbarAlpha) /3;

    DGL_Disable(DGL_TEXTURING);
    DGL_Begin(DGL_QUADS);

    // The left shader.
    DGL_Color4f(0, 0, 0, shadea);
    DGL_Vertex2f(20, 200);
    DGL_Vertex2f(20, 190);
    DGL_Color4f(0, 0, 0, 0);
    DGL_Vertex2f(35, 190);
    DGL_Vertex2f(35, 200);

    // The right shader.
    DGL_Vertex2f(277, 200);
    DGL_Vertex2f(277, 190);
    DGL_Color4f(0, 0, 0, shadea);
    DGL_Vertex2f(293, 190);
    DGL_Vertex2f(293, 200);

    DGL_End();
    DGL_Enable(DGL_TEXTURING);
}

/**
 * Draws the whole statusbar backgound.
 * \todo There is a whole lot of constants in here. What if someone wants to
 * replace the statusbar with new patches?
 */
void ST_refreshBackground(void)
{
    player_t           *pl = &players[CONSOLEPLAYER];
    float               alpha;

    if(blended)
    {
        alpha = cfg.statusbarAlpha - hudHideAmount;
        // Clamp
        CLAMP(alpha, 0.0f, 1.0f);
        if(!(alpha > 0))
            return;
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

        if(P_GetPlayerCheats(pl) & CF_GODMODE)
        {
            GL_DrawPatch(16, 167, W_GetNumForName("GOD1"));
            GL_DrawPatch(287, 167, W_GetNumForName("GOD2"));
        }

        if(!inventory)
            GL_DrawPatch(34, 160, statBar.lump);
        else
            GL_DrawPatch(34, 160, invBar.lump);

        drawChain();
    }
    else
    {
        DGL_Color4f(1, 1, 1, alpha);

        // Top bits.
        GL_DrawPatch_CS(0, 148, statusbarTopLeft.lump);
        GL_DrawPatch_CS(290, 148, statusbarTopRight.lump);

        GL_SetPatch(statusbar.lump, DGL_REPEAT, DGL_REPEAT);

        // Top border.
        GL_DrawCutRectTiled(34, 158, 248, 2, 320, 42, 34, 0, 0, 158, 0, 0);

        // Chain background.
        GL_DrawCutRectTiled(34, 191, 248, 9, 320, 42, 34, 33, 0, 191, 16, 8);

        // Faces.
        if(P_GetPlayerCheats(pl) & CF_GODMODE)
        {
            // If GOD mode we need to cut windows
            GL_DrawCutRectTiled(0, 158, 34, 42, 320, 42, 0, 0, 16, 167, 16, 8);
            GL_DrawCutRectTiled(282, 158, 38, 42, 320, 42, 282, 0, 287, 167, 16, 8);

            GL_DrawPatch_CS(16, 167, W_GetNumForName("GOD1"));
            GL_DrawPatch_CS(287, 167, W_GetNumForName("GOD2"));
        }
        else
        {
            GL_DrawCutRectTiled(0, 158, 34, 42, 320, 42, 0, 0, 0, 158, 0, 0);
            GL_DrawCutRectTiled(282, 158, 38, 42, 320, 42, 282, 0, 0, 158, 0, 0);
        }

        if(!inventory)
            GL_DrawPatch_CS(34, 160, statBar.lump);
        else
            GL_DrawPatch_CS(34, 160, invBar.lump);

        drawChain();
    }
}

/**
 * Unhides the current HUD display if hidden.
 *
 * @param event         The HUD Update Event type to check for triggering.
 */
void ST_HUDUnHide(hueevent_t event)
{
    if(event < HUE_FORCE || event > NUMHUDUNHIDEEVENTS)
        return;

    if(event == HUE_FORCE || cfg.hudUnHide[event])
    {
        hudHideTics = (cfg.hudTimer * TICSPERSEC);
        hudHideAmount = 0;
    }
}

void ST_drawIcons(void)
{
    static boolean      hitCenterFrame;

    int                 frame;
    float               iconAlpha = cfg.hudIconAlpha;
    float               textAlpha = cfg.hudColor[3];
    player_t           *plyr = &players[CONSOLEPLAYER];

    Draw_BeginZoom(cfg.hudScale, 2, 2);

    // Flight icons
    if(plyr->powers[PT_FLIGHT])
    {
        int     offset = (cfg.hudShown[HUD_AMMO] && cfg.screenBlocks > 10 &&
                          plyr->readyWeapon > 0 &&
                          plyr->readyWeapon < 7) ? 43 : 0;
        if(plyr->powers[PT_FLIGHT] > BLINKTHRESHOLD ||
           !(plyr->powers[PT_FLIGHT] & 16))
        {
            frame = (levelTime / 3) & 15;
            if(plyr->plr->mo->flags2 & MF2_FLY)
            {
                if(hitCenterFrame && (frame != 15 && frame != 0))
                {
                    GL_DrawPatchLitAlpha(20 + offset, 17, 1, iconAlpha, spinFly.lump + 15);
                }
                else
                {
                    GL_DrawPatchLitAlpha(20 + offset, 17, 1, iconAlpha, spinFly.lump + frame);
                    hitCenterFrame = false;
                }
            }
            else
            {
                if(!hitCenterFrame && (frame != 15 && frame != 0))
                {
                    GL_DrawPatchLitAlpha(20 + offset, 17, 1, iconAlpha, spinFly.lump + frame);
                    hitCenterFrame = false;
                }
                else
                {
                    GL_DrawPatchLitAlpha(20 + offset, 17, 1, iconAlpha, spinFly.lump + 15);
                    hitCenterFrame = true;
                }
            }
        }
    }

    Draw_EndZoom();

    Draw_BeginZoom(cfg.hudScale, 318, 2);

    if(plyr->powers[PT_WEAPONLEVEL2] && !plyr->morphTics)
    {
        if(cfg.tomeCounter || plyr->powers[PT_WEAPONLEVEL2] > BLINKTHRESHOLD
           || !(plyr->powers[PT_WEAPONLEVEL2] & 16))
        {
            frame = (levelTime / 3) & 15;
            if(cfg.tomeCounter && plyr->powers[PT_WEAPONLEVEL2] < 35)
            {
                DGL_Color4f(1, 1, 1, plyr->powers[PT_WEAPONLEVEL2] / 35.0f);
            }
            GL_DrawPatchLitAlpha(300, 17, 1, iconAlpha, spinBook.lump + frame);
        }

        if(plyr->powers[PT_WEAPONLEVEL2] < cfg.tomeCounter * 35)
        {
            _DrSmallNumber(1 + plyr->powers[PT_WEAPONLEVEL2] / 35, 303, 30,
                           false,1,1,1,textAlpha);
        }
    }

    Draw_EndZoom();
}

/**
 * All drawing for the statusbar starts and ends here.
 */
void ST_doRefresh(void)
{
    boolean             statusbarVisible =
        (cfg.statusbarScale < 20 || (cfg.statusbarScale == 20 && showBar < 1.0f));

    firstTime = false;

    if(statusbarVisible)
    {
        float               fscale = cfg.statusbarScale / 20.0f;
        float               h = 200 * (1 - fscale);

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        DGL_Translatef(160 - 320 * fscale / 2, h /showBar, 0);
        DGL_Scalef(fscale, fscale, 1);
    }

    // Draw status bar background.
    ST_refreshBackground();

    // And refresh all widgets.
    ST_drawWidgets(true);

    if(statusbarVisible)
    {
        // Restore the normal modelview matrix.
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }
}

void ST_doFullscreenStuff(void)
{
    int                 i, x, temp = 0;
    float               textAlpha = hudAlpha - hudHideAmount - ( 1 - cfg.hudColor[3]);
    float               iconAlpha = hudAlpha - hudHideAmount - ( 1 - cfg.hudIconAlpha);
    player_t           *plyr = &players[CONSOLEPLAYER];

    CLAMP(textAlpha, 0.0f, 1.0f);
    CLAMP(iconAlpha, 0.0f, 1.0f);

    if(cfg.hudShown[HUD_AMMO])
    {
        if(plyr->readyWeapon > 0 && plyr->readyWeapon < 7)
        {
            ammotype_t          ammoType;
            int                 lvl =
                (plyr->powers[PT_WEAPONLEVEL2]? 1 : 0);

            //// \todo Only supports one type of ammo per weapon.
            // For each type of ammo this weapon takes.
            for(ammoType = 0; ammoType < NUM_AMMO_TYPES; ++ammoType)
            {
                if(!weaponInfo[plyr->readyWeapon][plyr->class].mode[lvl].ammoType[ammoType])
                    continue;

                Draw_BeginZoom(cfg.hudScale, 2, 2);
                GL_DrawPatchLitAlpha(-1, 0, 1, iconAlpha,
                                     W_GetNumForName(ammoPic[plyr->readyWeapon - 1]));
                drawINumber(plyr->ammo[ammoType], 18, 2, 1, 1, 1, textAlpha);

                Draw_EndZoom();
                break;
            }
        }
    }

Draw_BeginZoom(cfg.hudScale, 2, 198);
    if(cfg.hudShown[HUD_HEALTH])
    {
        if(plyr->plr->mo->health > 0)
        {
            drawBNumber(plyr->plr->mo->health, 2, 180,
                      cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
        }
        else
        {
            drawBNumber(0, 2, 180, cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
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

        drawINumber(plyr->armorPoints, 6, temp, 1, 1, 1, textAlpha);
    }

    if(cfg.hudShown[HUD_KEYS])
    {
        x = 6;

        // Draw keys above health?
        if(plyr->keys[KT_YELLOW])
        {
            GL_DrawPatchLitAlpha(x, cfg.hudShown[HUD_HEALTH]? 172 : 190, 1,
                                 iconAlpha, W_GetNumForName("ykeyicon"));
            x += 11;
        }

        if(plyr->keys[KT_GREEN])
        {
            GL_DrawPatchLitAlpha(x, cfg.hudShown[HUD_HEALTH]? 172 : 190, 1,
                                 iconAlpha, W_GetNumForName("gkeyicon"));
            x += 11;
        }

        if(plyr->keys[KT_BLUE])
        {
            GL_DrawPatchLitAlpha(x, cfg.hudShown[HUD_HEALTH]? 172 : 190, 1,
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
                temp += plyr->frags[i];
            }
        }

Draw_BeginZoom(cfg.hudScale, 2, 198);
        drawINumber(temp, 45, 185, 1, 1, 1, textAlpha);
Draw_EndZoom();
    }

    if(!inventory)
    {
        if(cfg.hudShown[HUD_ARTI] && plyr->readyArtifact > 0)
        {
Draw_BeginZoom(cfg.hudScale, 318, 198);

            GL_DrawPatchLitAlpha(286, 166, 1, iconAlpha / 2,
                                 W_GetNumForName("ARTIBOX"));
            GL_DrawPatchLitAlpha(286, 166, 1, iconAlpha,
                                 W_GetNumForName(artifactList[plyr->readyArtifact + 5]));  //plus 5 for useartifact flashes
            DrSmallNumber(plyr->inventory[plyr->invPtr].count, 307, 188, 1,
                          1, 1, textAlpha);
Draw_EndZoom();
        }
    }
    else
    {
        float               invScale;

        invScale = MINMAX_OF(0.25f, cfg.hudScale - 0.25f, 0.8f);

Draw_BeginZoom(invScale, 160, 198);

        x = plyr->invPtr - plyr->curPos;
        for(i = 0; i < 7; ++i)
        {
            GL_DrawPatchLitAlpha(50 + i * 31, 168, 1, iconAlpha/2, W_GetNumForName("ARTIBOX"));
            if(plyr->inventorySlotNum > x + i &&
               plyr->inventory[x + i].type != arti_none)
            {
                GL_DrawPatchLitAlpha(50 + i * 31, 168, 1, i==plyr->curPos? hudAlpha : iconAlpha,
                             W_GetNumForName(artifactList[plyr->inventory[x + i].
                                              type +5])); //plus 5 for useartifact flashes
                DrSmallNumber(plyr->inventory[x + i].count, 69 + i * 31, 190, 1, 1, 1, i==plyr->curPos? hudAlpha : textAlpha/2);
            }
        }

        GL_DrawPatchLitAlpha(50 + plyr->curPos * 31, 197, 1, hudAlpha, artifactSelectBox.lump);
        if(x != 0)
        {
            GL_DrawPatchLitAlpha(38, 167, 1, iconAlpha,
                                 !(levelTime & 4)? invPageLeft.lump : invPageLeft2.lump);
        }

        if(plyr->inventorySlotNum - x > 7)
        {
            GL_DrawPatchLitAlpha(269, 167, 1, iconAlpha,
                                 !(levelTime & 4)? invPageRight.lump : invPageRight2.lump);
        }

Draw_EndZoom();
    }
}

void ST_Drawer(int fullscreenmode, boolean refresh)
{
    firstTime = firstTime || refresh;
    statusbarActive = (fullscreenmode < 2) || ( AM_IsMapActive(CONSOLEPLAYER) && (cfg.automapHudDisplay == 0 || cfg.automapHudDisplay == 2) );

    // Do palette shifts
    ST_doPaletteStuff();

    // Either slide the status bar in or fade out the fullscreen hud
    if(statusbarActive)
    {
        if(hudAlpha > 0.0f)
        {
            statusbarActive = 0;
            hudAlpha -= 0.1f;
        }
        else if(showBar < 1.0f)
        {
            showBar += 0.1f;
        }
    }
    else
    {
        if(fullscreenmode == 3)
        {
            if(hudAlpha > 0.0f)
            {
                hudAlpha -= 0.1f;
                fullscreenmode = 2;
            }
        }
        else
        {
            if(showBar > 0.0f)
            {
                showBar -= 0.1f;
                statusbarActive = 1;
            }
            else if(hudAlpha < 1.0f)
            {
                hudAlpha += 0.1f;
            }
        }
    }

    // Always try to render statusbar with alpha in fullscreen modes
    if(fullscreenmode)
        blended = 1;
    else
        blended = 0;

    if(statusbarActive)
    {
        ST_doRefresh();
    }
    else if(fullscreenmode != 3)
    {
        ST_doFullscreenStuff();
    }

    DGL_Color4f(1, 1, 1, 1);
    ST_drawIcons();
}

int R_GetFilterColor(int filter)
{
    int                 rgba = 0;

    // We have to choose the right color and alpha.
    if(filter >= STARTREDPALS && filter < STARTREDPALS + NUMREDPALS)
        // Red?
        rgba = FMAKERGBA(1, 0, 0, filter / 8.0);    // Full red with filter 8.
    else if(filter >= STARTBONUSPALS && filter < STARTBONUSPALS + NUMBONUSPALS)
        // Light Yellow?
        rgba = FMAKERGBA(1, 1, .5, (filter - STARTBONUSPALS + 1) / 16.0);
    return rgba;
}

void R_SetFilter(int filter)
{
    GL_SetFilter(R_GetFilterColor(filter));
}

/**
 * Sets the new palette based upon current values of player->damageCount
 * and player->bonusCount
 */
void ST_doPaletteStuff(void)
{
    static int          sb_palette = 0;

    int                 palette;
    player_t           *plyr = &players[CONSOLEPLAYER];

    if(plyr->damageCount)
    {
        palette = (plyr->damageCount + 7) >> 3;
        if(palette >= NUMREDPALS)
        {
            palette = NUMREDPALS - 1;
        }
        palette += STARTREDPALS;
    }
    else if(plyr->bonusCount)
    {
        palette = (plyr->bonusCount + 7) >> 3;
        if(palette >= NUMBONUSPALS)
        {
            palette = NUMBONUSPALS - 1;
        }
        palette += STARTBONUSPALS;
    }
    else
    {
        palette = 0;
    }

    if(palette != sb_palette)
    {
        sb_palette = palette;

        plyr->plr->filter = R_GetFilterColor(palette);   // $democam
    }
}

void drawChain(void)
{
    int                 chainY;
    float               healthPos;
    float               gemglow;
    int                 x, y, w, h;
    float               cw;
    player_t           *plyr = &players[DISPLAYPLAYER];

    if(oldHealth != healthMarker)
    {
        oldHealth = healthMarker;
        healthPos = healthMarker;
        if(healthPos < 0)
        {
            healthPos = 0;
        }
        if(healthPos > 100)
        {
            healthPos = 100;
        }

        gemglow = healthPos / 100;
        chainY =
            (healthMarker ==
             plyr->plr->mo->health) ? 191 : 191 + chainWiggle;

        // draw the chain

        x = 21;
        y = chainY;
        w = 271;
        h = 8;
        cw = (healthPos / 118) + 0.018f;

        GL_SetPatch(chain.lump, DGL_REPEAT, DGL_CLAMP);

        DGL_Color4f(1, 1, 1, statusbarCounterAlpha);

        DGL_Begin(DGL_QUADS);

        DGL_TexCoord2f( 0 - cw, 0);
        DGL_Vertex2f(x, y);

        DGL_TexCoord2f( 0.916f - cw, 0);
        DGL_Vertex2f(x + w, y);

        DGL_TexCoord2f( 0.916f - cw, 1);
        DGL_Vertex2f(x + w, y + h);

        DGL_TexCoord2f( 0 - cw, 1);
        DGL_Vertex2f(x, y + h);

        DGL_End();

        // Draw the life gem.
        healthPos = (healthPos * 256) / 102;

        GL_DrawPatchLitAlpha( x + healthPos, chainY, 1, statusbarCounterAlpha, lifeGem.lump);

        shadeChain();

        // How about a glowing gem?
        GL_BlendMode(BM_ADD);
        DGL_Bind(Get(DD_DYNLIGHT_TEXTURE));

        GL_DrawRect(x + healthPos - 11, chainY - 6, 41, 24, 1, 0, 0, gemglow - (1 - statusbarCounterAlpha));

        GL_BlendMode(BM_NORMAL);
        DGL_Color4f(1, 1, 1, 1);
    }
}

void ST_drawWidgets(boolean refresh)
{
    int                 x, i;
    player_t           *plyr = &players[CONSOLEPLAYER];

    oldHealth = -1;
    if(!inventory)
    {
        oldCurrentArtifact = 0;
        // Draw all the counters.

        // Frags.
        if(deathmatch)
                STlib_updateNum(&wFrags, refresh);
        else
                STlib_updateNum(&wHealth, refresh);

        // Draw armor.
        STlib_updateNum(&wArmor, refresh);

        // Draw keys.
        for(i = 0; i < 3; ++i)
            STlib_updateBinIcon(&wKeyBoxes[i], refresh);

        STlib_updateNum(&wReadyWeapon, refresh);
        STlib_updateMultIcon(&wCurrentAmmoIcon, refresh);

        // Current artifact.
        if(plyr->readyArtifact > 0)
        {
            STlib_updateMultIcon(&wCurrentArtifact, refresh);
            if(!artifactFlash && plyr->inventory[plyr->invPtr].count > 1)
                STlib_updateNum(&wCurrentArtifactCount, refresh);
        }
    }
    else
    {   // Draw Inventory.
        x = plyr->invPtr - plyr->curPos;

        for(i = 0; i < NUMVISINVSLOTS; ++i)
        {
            if( plyr->inventory[x + i].type != arti_none)
            {
                STlib_updateMultIcon(&wInvSlots[i], refresh);

                if( plyr->inventory[x + i].count > 1)
                    STlib_updateNum(&wInvSlotsCount[i], refresh);
            }
        }

        // Draw selector box.
        GL_DrawPatchLitAlpha(ST_INVENTORYX + plyr->curPos * 31, 189, 1,
                             statusbarCounterAlpha, artifactSelectBox.lump);

        // Draw more left indicator.
        if(x != 0)
            GL_DrawPatchLitAlpha(38, 159, 1, statusbarCounterAlpha,
                                 !(levelTime & 4) ? invPageLeft.lump : invPageLeft2.lump);

        // Draw more right indicator.
        if(plyr->inventorySlotNum - x > 7)
            GL_DrawPatchLitAlpha(269, 159, 1, statusbarCounterAlpha,
                                 !(levelTime & 4) ? invPageRight.lump : invPageRight2.lump);
    }
}

/**
 * Console command to show the hud if hidden.
 */
DEFCC(CCmdHUDShow)
{
    ST_HUDUnHide(HUE_FORCE);
    return true;
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

    if(*val < min)
        *val = min;
    if(*val > max)
        *val = max;

    // Update the view size if necessary.
    R_SetViewSize(cfg.screenBlocks, 0);
    ST_HUDUnHide(HUE_FORCE); // so the user can see the change.
    return true;
}
