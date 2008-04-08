/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * st_stuff.c: Status bar code - jDoom specific.
 *
 * Does the face/direction indicator animation and the palette indicators as
 * well (red pain/berserk, bright pickup)
 */

 // HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>

#include "jdoom.h"

#include "d_net.h"
#include "st_lib.h"
#include "hu_stuff.h"
#include "p_tick.h" // for P_IsPaused
#include "p_player.h"
#include "am_map.h"

// MACROS ------------------------------------------------------------------

#define FMAKERGBA(r,g,b,a) ( (byte)(0xff*r) + ((byte)(0xff*g)<<8)           \
                            + ((byte)(0xff*b)<<16) + ((byte)(0xff*a)<<24) )

// Radiation suit, green shift.
#define RADIATIONPAL        (13)

// N/256*100% probability
//  that the normal face state will change
#define ST_FACEPROBABILITY  (96)

// Location of status bar
#define ST_X                (0)
#define ST_X2               (104)

#define ST_FX               (144)
#define ST_FY               (169)

// Number of status faces.
#define ST_NUMPAINFACES     (5)
#define ST_NUMSTRAIGHTFACES (3)
#define ST_NUMTURNFACES     (2)
#define ST_NUMSPECIALFACES  (3)

#define ST_FACESTRIDE \
          (ST_NUMSTRAIGHTFACES+ST_NUMTURNFACES+ST_NUMSPECIALFACES)

#define ST_NUMEXTRAFACES    (2)

#define ST_NUMFACES \
          (ST_FACESTRIDE*ST_NUMPAINFACES+ST_NUMEXTRAFACES)

#define ST_TURNOFFSET       (ST_NUMSTRAIGHTFACES)
#define ST_OUCHOFFSET       (ST_TURNOFFSET + ST_NUMTURNFACES)
#define ST_EVILGRINOFFSET       (ST_OUCHOFFSET + 1)
#define ST_RAMPAGEOFFSET        (ST_EVILGRINOFFSET + 1)
#define ST_GODFACE          (ST_NUMPAINFACES*ST_FACESTRIDE)
#define ST_DEADFACE         (ST_GODFACE+1)

#define ST_FACESX           (143)
#define ST_FACESY           (168)

#define ST_EVILGRINCOUNT    (2*TICRATE)
#define ST_STRAIGHTFACECOUNT (TICRATE/2)
#define ST_TURNCOUNT        (1*TICRATE)
#define ST_OUCHCOUNT        (1*TICRATE)
#define ST_RAMPAGEDELAY     (2*TICRATE)

#define ST_MUCHPAIN         (20)

// AMMO number pos.
#define ST_AMMOWIDTH        (3)
#define ST_AMMOX            (44)
#define ST_AMMOY            (171)

// HEALTH number pos.
#define ST_HEALTHWIDTH      (3)
#define ST_HEALTHX          (90)
#define ST_HEALTHY          (171)

// Weapon pos.
#define ST_ARMSX            (111)
#define ST_ARMSY            (172)
#define ST_ARMSBGX          (104)
#define ST_ARMSBGY          (168)
#define ST_ARMSXSPACE       (12)
#define ST_ARMSYSPACE       (10)

// Frags pos.
#define ST_FRAGSX           (138)
#define ST_FRAGSY           (171)
#define ST_FRAGSWIDTH       (2)

// ARMOR number pos.
#define ST_ARMORWIDTH       (3)
#define ST_ARMORX           (221)
#define ST_ARMORY           (171)

// Key icon positions.
#define ST_KEY0WIDTH        (8)
#define ST_KEY0HEIGHT       (5)
#define ST_KEY0X            (239)
#define ST_KEY0Y            (171)
#define ST_KEY1WIDTH        (ST_KEY0WIDTH)
#define ST_KEY1X            (239)
#define ST_KEY1Y            (181)
#define ST_KEY2WIDTH        (ST_KEY0WIDTH)
#define ST_KEY2X            (239)
#define ST_KEY2Y            (191)

// Ammunition counter.
#define ST_AMMO0WIDTH       (3)
#define ST_AMMO0HEIGHT      (6)
#define ST_AMMO0X           (288)
#define ST_AMMO0Y           (173)
#define ST_AMMO1WIDTH       (ST_AMMO0WIDTH)
#define ST_AMMO1X           (288)
#define ST_AMMO1Y           (179)
#define ST_AMMO2WIDTH       (ST_AMMO0WIDTH)
#define ST_AMMO2X           (288)
#define ST_AMMO2Y           (191)
#define ST_AMMO3WIDTH       (ST_AMMO0WIDTH)
#define ST_AMMO3X           (288)
#define ST_AMMO3Y           (185)

// Indicate maximum ammunition.
// Only needed because backpack exists.
#define ST_MAXAMMO0WIDTH    (3)
#define ST_MAXAMMO0HEIGHT   (5)
#define ST_MAXAMMO0X        (314)
#define ST_MAXAMMO0Y        (173)
#define ST_MAXAMMO1WIDTH    (ST_MAXAMMO0WIDTH)
#define ST_MAXAMMO1X        (314)
#define ST_MAXAMMO1Y        (179)
#define ST_MAXAMMO2WIDTH    (ST_MAXAMMO0WIDTH)
#define ST_MAXAMMO2X        (314)
#define ST_MAXAMMO2Y        (191)
#define ST_MAXAMMO3WIDTH    (ST_MAXAMMO0WIDTH)
#define ST_MAXAMMO3X        (314)
#define ST_MAXAMMO3Y        (185)

// Pistol.
#define ST_WEAPON0X         (110)
#define ST_WEAPON0Y         (172)

// Shotgun.
#define ST_WEAPON1X         (122)
#define ST_WEAPON1Y         (172)

// Chain gun.
#define ST_WEAPON2X         (134)
#define ST_WEAPON2Y         (172)

// Missile launcher.
#define ST_WEAPON3X         (110)
#define ST_WEAPON3Y         (181)

// Plasma gun.
#define ST_WEAPON4X         (122)
#define ST_WEAPON4Y         (181)

 // BFG.
#define ST_WEAPON5X         (134)
#define ST_WEAPON5Y         (181)

// WPNS title.
#define ST_WPNSX            (109)
#define ST_WPNSY            (191)

 // DETH title.
#define ST_DETHX            (109)
#define ST_DETHY            (191)

// TYPES -------------------------------------------------------------------

typedef enum hotloc_e {
    HOT_TLEFT,
    HOT_TRIGHT,
    HOT_BRIGHT,
    HOT_BLEFT
} hotloc_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void    ST_Stop(void);

DEFCC(CCmdHUDShow);
DEFCC(CCmdStatusBarSize);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int hudHideTics;
static float hudHideAmount;

// Side statusbar amount 1.0 is fully open.
static float showBar = 0.0f;

// Fullscreen hud alpha value.
static float hudAlpha = 0.0f;

static float statusbarCounterAlpha = 0.0f;

// ST_Start() has just been called.
static boolean firstTime;

// Whether to use alpha blending.
static boolean blended = false;

// Whether the main status bar is active.
static boolean statusbarActive;

// !deathmatch && statusbarActive.
static boolean statusbarArmsOn;

// !deathmatch.
static boolean statusbarFragsOn;

// Number of frags so far in deathmatch.
static int currentFragsCount;

// Used to use appopriately pained face.
static int oldHealth = -1;

// Used for evil grin.
static boolean oldWeaponsOwned[NUM_WEAPON_TYPES];

// Count until face changes.
static int faceCount = 0;

// Current face index, used by wFaces.
static int faceIndex = 0;

// Holds key-type for each key box on bar.
static int keyBoxes[3];

// A random number per tick.
static int randomNumber;

static boolean stopped = true;
static int currentPalette = 0;

// Main bar left.
static dpatch_t statusbar;

// 0-9, tall numbers.
static dpatch_t tallNum[10];

// Tall % sign.
static dpatch_t tallPercent;

// 0-9, short, yellow (,different!) numbers.
static dpatch_t shortNum[10];

// 3 key-cards, 3 skulls.
static dpatch_t keys[NUM_KEY_TYPES];

// Face status patches.
static dpatch_t faces[ST_NUMFACES];

// Face background.
static dpatch_t faceBackground;

 // Main bar right.
static dpatch_t armsBackground;

// Weapon ownership patches.
static dpatch_t arms[6][2];

// Ready-weapon widget.
static st_number_t wReadyWeapon;

 // In deathmatch only, summary of frags stats.
static st_number_t wFrags;

// Health widget.
static st_percent_t wHealth;

// Weapon ownership widgets.
static st_multicon_t wArms[6];

// Face status widget.
static st_multicon_t wFaces;

// Keycard widgets.
static st_multicon_t wKeyBoxes[3];

// Armor widget.
static st_percent_t wArmor;

// Ammo widgets.
static st_number_t wAmmo[4];

// Max ammo widgets.
static st_number_t wMaxAmmo[4];

// CVARs for the HUD/Statusbar:
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
    {"hud-face", 0, CVT_BYTE, &cfg.hudShown[HUD_FACE], 0, 1},
    {"hud-health", 0, CVT_BYTE, &cfg.hudShown[HUD_HEALTH], 0, 1},
    {"hud-armor", 0, CVT_BYTE, &cfg.hudShown[HUD_ARMOR], 0, 1},
    {"hud-ammo", 0, CVT_BYTE, &cfg.hudShown[HUD_AMMO], 0, 1},
    {"hud-keys", 0, CVT_BYTE, &cfg.hudShown[HUD_KEYS], 0, 1},
    {"hud-keys-combine", 0, CVT_BYTE, &cfg.hudKeysCombine, 0, 1},

    // HUD displays
    {"hud-frags", 0, CVT_BYTE, &cfg.hudShown[HUD_FRAGS], 0, 1},
    {"hud-frags-all", 0, CVT_BYTE, &huShowAllFrags, 0, 1},

    {"hud-timer", 0, CVT_FLOAT, &cfg.hudTimer, 0, 60},

    {"hud-unhide-damage", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_DAMAGE], 0, 1},
    {"hud-unhide-pickup-health", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_HEALTH], 0, 1},
    {"hud-unhide-pickup-armor", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_ARMOR], 0, 1},
    {"hud-unhide-pickup-powerup", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_POWER], 0, 1},
    {"hud-unhide-pickup-weapon", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_WEAPON], 0, 1},
    {"hud-unhide-pickup-ammo", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_AMMO], 0, 1},
    {"hud-unhide-pickup-key", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_KEY], 0, 1},
    {NULL}
};

// Console commands for the HUD/Status bar:
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

void ST_refreshBackground(void)
{
    int                 x, y, w, h;
    float               cw, cw2, ch;
    float               alpha;

    GL_SetPatch(statusbar.lump, DGL_CLAMP, DGL_CLAMP);

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
    {
        // we can just render the full thing as normal
        GL_DrawPatch(ST_X, ST_Y, statusbar.lump);

        if(statusbarArmsOn)  // arms baground
            GL_DrawPatch(ST_ARMSBGX, ST_ARMSBGY, armsBackground.lump);

        if(IS_NETGAME) // faceback
            GL_DrawPatch(ST_FX, ST_Y+1, faceBackground.lump);
    }
    else
    {
        // Alpha blended status bar, we'll need to cut it up into smaller bits...
        DGL_Color4f(1, 1, 1, alpha);

        DGL_Begin(DGL_QUADS);

        // (up to faceback if deathmatch, else ST_ARMS)
        x = ST_X;
        y = ST_Y;
        w = statusbarArmsOn ? 104 : 143;
        h = 32;
        cw = statusbarArmsOn ? 0.325f : 0.446875f;

        DGL_TexCoord2f(0, 0);
        DGL_Vertex2f(x, y);
        DGL_TexCoord2f(cw, 0);
        DGL_Vertex2f(x + w, y);
        DGL_TexCoord2f(cw, 1);
        DGL_Vertex2f(x + w, y + h);
        DGL_TexCoord2f(0, 1);
        DGL_Vertex2f(x, y + h);

        if(IS_NETGAME)
        {
            // (fiddly little bit above faceback)
            x = ST_X + 144;
            y = ST_Y;
            w = 35;
            h = 1;
            cw = 0.446875f;
            cw2 = 0.55625f;
            ch = 0.03125f;

            DGL_TexCoord2f(cw, 0);
            DGL_Vertex2f(x, y);
            DGL_TexCoord2f(cw2, 0);
            DGL_Vertex2f(x + w, y);
            DGL_TexCoord2f(cw2, ch);
            DGL_Vertex2f(x + w, y + h);
            DGL_TexCoord2f(cw, ch);
            DGL_Vertex2f(x, y + h);

            // (after faceback)
            x = ST_X + 178;
            y = ST_Y;
            w = 142;
            h = 32;
            cw = 0.55625f;

        }
        else
        {
            // (including area behind the face)
            x = ST_X + 144;
            y = ST_Y;
            w = 176;
            h = 32;
            cw = 0.45f;
        }

        DGL_TexCoord2f(cw, 0);
        DGL_Vertex2f(x, y);
        DGL_TexCoord2f(1, 0);
        DGL_Vertex2f(x + w, y);
        DGL_TexCoord2f(1, 1);
        DGL_Vertex2f(x + w, y + h);
        DGL_TexCoord2f(cw, 1);
        DGL_Vertex2f(x, y + h);

        DGL_End();

        if(statusbarArmsOn)  // arms baground
            GL_DrawPatch_CS(ST_ARMSBGX, ST_ARMSBGY, armsBackground.lump);

        if(IS_NETGAME) // faceback
            GL_DrawPatch_CS(ST_FX, ST_Y+1, faceBackground.lump);
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

int ST_calcPainOffset(void)
{
    int                 health;
    static int          lastCalc;
    static int          oldHealth = -1;
    player_t           *plyr = &players[CONSOLEPLAYER];

    health = (plyr->health > 100 ? 100 : plyr->health);

    if(health != oldHealth)
    {
        lastCalc = ST_FACESTRIDE * (((100 - health) * ST_NUMPAINFACES) / 101);
        oldHealth = health;
    }

    return lastCalc;
}

/**
 * This is a not-very-pretty routine which handles the face states
 * and their timing. the precedence of expressions is:
 *
 * dead > evil grin > turned head > straight ahead
 */
void ST_updateFaceWidget(void)
{
    int                 i;
    angle_t             badGuyAngle;
    angle_t             diffAng;
    static int          lastAttackDown = -1;
    static int          priority = 0;
    boolean             doEvilGrin;
    player_t           *plyr = &players[CONSOLEPLAYER];

    if(priority < 10)
    {   // Player is dead.
        if(!plyr->health)
        {
            priority = 9;
            faceIndex = ST_DEADFACE;
            faceCount = 1;
        }
    }

    if(priority < 9)
    {
        if(plyr->bonusCount)
        {   // Picking up a bonus.
            doEvilGrin = false;

            for(i = 0; i < NUM_WEAPON_TYPES; ++i)
            {
                if(oldWeaponsOwned[i] != plyr->weaponOwned[i])
                {
                    doEvilGrin = true;
                    oldWeaponsOwned[i] = plyr->weaponOwned[i];
                }
            }

            if(doEvilGrin)
            {   // Evil grin if just picked up weapon.
                priority = 8;
                faceCount = ST_EVILGRINCOUNT;
                faceIndex = ST_calcPainOffset() + ST_EVILGRINOFFSET;
            }
        }
    }

    if(priority < 8)
    {
        if(plyr->damageCount && plyr->attacker &&
           plyr->attacker != plyr->plr->mo)
        {   // Being attacked.
            priority = 7;

            // DOOM BUG
            // This test was inversed, thereby the OUCH face was NEVER used
            // in normal gameplay as it requires the player recieving damage
            // to end up with MORE health than he started with.
            // Also, priority was not changed which would have resulted in a
            // frame duration of only 1 tic.
            // if(plyr->health - oldHealth > ST_MUCHPAIN)

            if((cfg.fixOuchFace?
               (oldHealth - plyr->health) :
               (plyr->health - oldHealth)) > ST_MUCHPAIN)
            {
                faceCount = ST_TURNCOUNT;
                faceIndex = ST_calcPainOffset() + ST_OUCHOFFSET;
                if(cfg.fixOuchFace)
                    priority = 8; // Added to fix 1 tic issue.
            }
            else
            {
                badGuyAngle =
                    R_PointToAngle2(FLT2FIX(plyr->plr->mo->pos[VX]),
                                    FLT2FIX(plyr->plr->mo->pos[VY]),
                                    FLT2FIX(plyr->attacker->pos[VX]),
                                    FLT2FIX(plyr->attacker->pos[VY]));

                if(badGuyAngle > plyr->plr->mo->angle)
                {   // Whether right or left.
                    diffAng = badGuyAngle - plyr->plr->mo->angle;
                    i = diffAng > ANG180;
                }
                else
                {   // Whether left or right.
                    diffAng = plyr->plr->mo->angle - badGuyAngle;
                    i = diffAng <= ANG180;
                }

                faceCount = ST_TURNCOUNT;
                faceIndex = ST_calcPainOffset();

                if(diffAng < ANG45)
                {   // Head-on.
                    faceIndex += ST_RAMPAGEOFFSET;
                }
                else if(i)
                {   // Turn face right.
                    faceIndex += ST_TURNOFFSET;
                }
                else
                {   // Turn face left.
                    faceIndex += ST_TURNOFFSET + 1;
                }
            }
        }
    }

    if(priority < 7)
    {   // Getting hurt because of your own damn stupidity.
        if(plyr->damageCount)
        {
            // DOOM BUG
            // This test was inversed, thereby the OUCH face was NEVER used
            // in normal gameplay as it requires the player recieving damage
            // to end up with MORE health than he started with.
            // if(plyr->health - oldHealth > ST_MUCHPAIN)

            if((cfg.fixOuchFace?
               (oldHealth - plyr->health) :
               (plyr->health - oldHealth)) > ST_MUCHPAIN)
            {
                priority = 7;
                faceCount = ST_TURNCOUNT;
                faceIndex = ST_calcPainOffset() + ST_OUCHOFFSET;
            }
            else
            {
                priority = 6;
                faceCount = ST_TURNCOUNT;
                faceIndex = ST_calcPainOffset() + ST_RAMPAGEOFFSET;
            }
        }
    }

    if(priority < 6)
    {   // Rapid firing.
        if(plyr->attackDown)
        {
            if(lastAttackDown == -1)
            {
                lastAttackDown = ST_RAMPAGEDELAY;
            }
            else if(!--lastAttackDown)
            {
                priority = 5;
                faceIndex = ST_calcPainOffset() + ST_RAMPAGEOFFSET;
                faceCount = 1;
                lastAttackDown = 1;
            }
        }
        else
        {
            lastAttackDown = -1;
        }
    }

    if(priority < 5)
    {   // Invulnerability.
        if((P_GetPlayerCheats(plyr) & CF_GODMODE) ||
           plyr->powers[PT_INVULNERABILITY])
        {
            priority = 4;

            faceIndex = ST_GODFACE;
            faceCount = 1;
        }
    }

    // Look left or look right if the facecount has timed out.
    if(!faceCount)
    {
        faceIndex = ST_calcPainOffset() + (randomNumber % 3);
        faceCount = ST_STRAIGHTFACECOUNT;
        priority = 0;
    }

    faceCount--;
}

void ST_updateWidgets(void)
{
    static int          largeAmmo = 1994; // Means "n/a".

    int                 i;
    ammotype_t          ammoType;
    boolean             found;
    player_t           *plr = &players[CONSOLEPLAYER];

    if(blended)
    {
        statusbarCounterAlpha = cfg.statusbarCounterAlpha - hudHideAmount;
        CLAMP(statusbarCounterAlpha, 0.0f, 1.0f);
    }
    else
        statusbarCounterAlpha = 1.0f;

    // Must redirect the pointer if the ready weapon has changed.
    found = false;
    for(ammoType=0; ammoType < NUM_AMMO_TYPES && !found; ++ammoType)
    {
        if(!weaponInfo[plr->readyWeapon][plr->class].mode[0].ammoType[ammoType])
            continue; // Weapon does not use this type of ammo.

        //// \todo Only supports one type of ammo per weapon
        wReadyWeapon.num = &plr->ammo[ammoType];
        found = true;
    }
    if(!found) // Weapon takes no ammo at all.
    {
        wReadyWeapon.num = &largeAmmo;
    }

    wReadyWeapon.data = plr->readyWeapon;

    // Update keycard multiple widgets.
    for(i = 0; i < 3; ++i)
    {
        keyBoxes[i] = plr->keys[i] ? i : -1;

        if(plr->keys[i + 3])
            keyBoxes[i] = i + 3;
    }

    // Refresh everything if this is him coming back to life.
    ST_updateFaceWidget();

    // Used by wArms[] widgets.
    statusbarArmsOn = statusbarActive && !deathmatch;

    // Used by wFrags widget.
    statusbarFragsOn = deathmatch && statusbarActive;
    currentFragsCount = 0;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->inGame)
            continue;

        currentFragsCount += plr->frags[i] * (i != CONSOLEPLAYER ? 1 : -1);
    }
}

void ST_Ticker(void)
{
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

    randomNumber = M_Random();
    ST_updateWidgets();
    oldHealth = plyr->health;
}

int R_GetFilterColor(int filter)
{
    int                 rgba = 0;

    // We have to choose the right color and alpha.
    if(filter >= STARTREDPALS && filter < STARTREDPALS + NUMREDPALS)
        // Red.
        rgba = FMAKERGBA(1, 0, 0, filter / 9.0);
    else if(filter >= STARTBONUSPALS && filter < STARTBONUSPALS + NUMBONUSPALS)
        // Gold.
        rgba = FMAKERGBA(1, .8, .5, (filter - STARTBONUSPALS + 1) / 16.0);
    else if(filter == RADIATIONPAL)
        // Green.
        rgba = FMAKERGBA(0, .7, 0, .15f);
    else if(filter)
        Con_Error("R_GetFilterColor: Real strange filter number: %d.\n", filter);
    return rgba;
}

void ST_doPaletteStuff(void)
{
    int                 palette;
    int                 cnt;
    int                 bzc;
    player_t           *plyr = &players[CONSOLEPLAYER];

    cnt = plyr->damageCount;

    if(plyr->powers[PT_STRENGTH])
    {
        // slowly fade the berzerk out
        bzc = 12 - (plyr->powers[PT_STRENGTH] >> 6);

        if(bzc > cnt)
            cnt = bzc;
    }

    if(cnt)
    {
        palette = (cnt + 7) >> 3;

        if(palette >= NUMREDPALS)
            palette = NUMREDPALS - 1;

        palette += STARTREDPALS;
    }

    else if(plyr->bonusCount)
    {
        palette = (plyr->bonusCount + 7) >> 3;

        if(palette >= NUMBONUSPALS)
            palette = NUMBONUSPALS - 1;

        palette += STARTBONUSPALS;
    }

    else if(plyr->powers[PT_IRONFEET] > 4 * 32 ||
            plyr->powers[PT_IRONFEET] & 8)
        palette = RADIATIONPAL;
    else
        palette = 0;

    if(palette != currentPalette)
    {
        currentPalette = palette;
        plyr->plr->filter = R_GetFilterColor(palette); // $democam
    }
}

void ST_drawWidgets(boolean refresh)
{
    int                 i;

    // Used by wArms[] widgets.
    statusbarArmsOn = statusbarActive && !deathmatch;

    // Used by wFrags widget.
    statusbarFragsOn = deathmatch && statusbarActive;

    STlib_updateNum(&wReadyWeapon, refresh);

    for(i = 0; i < 4; ++i)
    {
        STlib_updateNum(&wAmmo[i], refresh);
        STlib_updateNum(&wMaxAmmo[i], refresh);
    }

    STlib_updatePercent(&wHealth, refresh);
    STlib_updatePercent(&wArmor, refresh);

    for(i = 0; i < 6; ++i)
        STlib_updateMultIcon(&wArms[i], refresh);

    STlib_updateMultIcon(&wFaces, refresh);

    for(i = 0; i < 3; ++i)
        STlib_updateMultIcon(&wKeyBoxes[i], refresh);

    STlib_updateNum(&wFrags, refresh);
}

void ST_doRefresh(void)
{
    boolean             statusbarVisible = (cfg.statusbarScale < 20 ||
        (cfg.statusbarScale == 20 && showBar < 1.0f));

    firstTime = false;

    if(statusbarVisible)
    {
        float               fscale = cfg.statusbarScale / 20.0f;
        float               h = 200 * (1 - fscale);

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        DGL_Translatef(160 - 320 * fscale / 2, h / showBar, 0);
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

void ST_HUDSpriteSize(int sprite, int *w, int *h)
{
    spriteinfo_t        sprInfo;

    R_GetSpriteInfo(sprite, 0, &sprInfo);
    *w = sprInfo.width;
    *h = sprInfo.height;

    if(sprite == SPR_ROCK)
    {
        // Must scale it a bit.
        *w /= 1.5;
        *h /= 1.5;
    }
}

void ST_drawHUDSprite(int sprite, int x, int y, int hotSpot, float alpha)
{
    int                 w, h;
    spriteinfo_t        sprInfo;

    CLAMP(alpha, 0.0f, 1.0f);
    if(alpha == 0.0f)
        return;

    R_GetSpriteInfo(sprite, 0, &sprInfo);
    w = sprInfo.width;
    h = sprInfo.height;
    if(sprite == SPR_ROCK)
    {
        w /= 1.5;
        h /= 1.5;
    }

    switch(hotSpot)
    {
    case HOT_BRIGHT:
        y -= h;

    case HOT_TRIGHT:
        x -= w;
        break;

    case HOT_BLEFT:
        y -= h;
        break;
    }

    DGL_Color4f(1, 1, 1, alpha );
    GL_DrawPSprite(x, y, sprite == SPR_ROCK ? 1 / 1.5 : 1, false,
                   sprInfo.idx);
}

void ST_doFullscreenStuff(void)
{
    static const int    ammoSprite[NUM_AMMO_TYPES] = {
        SPR_AMMO,
        SPR_SBOX,
        SPR_CELL,
        SPR_ROCK
    };

    player_t           *plr = &players[DISPLAYPLAYER];
    char                buf[20];
    int                 w, h, pos = 0, spr, i;
    int                 width = 320 / cfg.hudScale;
    int                 height = 200 / cfg.hudScale;
    float               textAlpha = hudAlpha - hudHideAmount - ( 1 - cfg.hudColor[3]);
    float               iconAlpha = hudAlpha - hudHideAmount - ( 1 - cfg.hudIconAlpha);

    CLAMP(textAlpha, 0.0f, 1.0f);
    CLAMP(iconAlpha, 0.0f, 1.0f);

    if(IS_NETGAME && deathmatch && cfg.hudShown[HUD_FRAGS])
    {
        // Display the frag counter.
        i = 199 - 8;
        if(cfg.hudShown[HUD_HEALTH] || cfg.hudShown[HUD_AMMO])
        {
            i -= 18 * cfg.hudScale;
        }
        sprintf(buf, "FRAGS:%i", currentFragsCount);
        M_WriteText2(2, i, buf, huFontA, cfg.hudColor[0], cfg.hudColor[1],
                     cfg.hudColor[2], textAlpha);
    }

    // Setup the scaling matrix.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);

    // Draw the visible HUD data, first health.
    if(cfg.hudShown[HUD_HEALTH])
    {
        ST_drawHUDSprite(SPR_STIM, 2, height - 2, HOT_BLEFT, iconAlpha);
        ST_HUDSpriteSize(SPR_STIM, &w, &h);
        pos = w + 2;
        sprintf(buf, "%i%%", plr->health);
        M_WriteText2(pos, height - 14, buf, huFontB, cfg.hudColor[0],
                     cfg.hudColor[1], cfg.hudColor[2], textAlpha);
        pos += M_StringWidth(buf, huFontB) + 2;
    }

    if(cfg.hudShown[HUD_AMMO])
    {
        ammotype_t          ammoType;

        //// \todo Only supports one type of ammo per weapon.
        //// for each type of ammo this weapon takes.
        for(ammoType=0; ammoType < NUM_AMMO_TYPES; ++ammoType)
        {
            if(!weaponInfo[plr->readyWeapon][plr->class].mode[0].ammoType[ammoType])
                continue;

            spr = ammoSprite[ammoType];
            ST_drawHUDSprite(spr, pos, height - 2, HOT_BLEFT, iconAlpha);
            ST_HUDSpriteSize(spr, &w, &h);
            pos += w + 2;
            sprintf(buf, "%i", plr->ammo[ammoType]);
            M_WriteText2(pos, height - 14, buf, huFontB,
                         cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2],
                         textAlpha);
            break;
        }
    }

    // Doomguy's face | use a bit of extra scale.
    if(cfg.hudShown[HUD_FACE])
    {
        pos = (width/2) -(faceBackground.width/2) + 6;

        if(iconAlpha != 0.0f)
        {
Draw_BeginZoom(0.7f, pos , height - 1);
            DGL_Color4f(1, 1, 1, iconAlpha);
            if(IS_NETGAME)
                GL_DrawPatch_CS(pos, height - faceBackground.height + 1,
                                faceBackground.lump);

            GL_DrawPatch_CS(pos, height - faceBackground.height,
                            faces[faceIndex].lump);
Draw_EndZoom();
        }
    }

    pos = width - 2;
    if(cfg.hudShown[HUD_ARMOR])
    {
        int                 maxArmor, armorOffset;

        maxArmor = MAX_OF(armorPoints[0], armorPoints[1]);
        maxArmor = MAX_OF(maxArmor, armorPoints[2]);
        maxArmor = MAX_OF(maxArmor, armorPoints[2]);
        sprintf(buf, "%i%%", maxArmor);
        armorOffset = M_StringWidth(buf, huFontB);

        sprintf(buf, "%i%%", plr->armorPoints);
        pos -= armorOffset;
        M_WriteText2(pos + armorOffset - M_StringWidth(buf, huFontB), height - 14, buf, huFontB,
                     cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
        pos -= 2;

        spr = (plr->armorType == 2 ? SPR_ARM2 : SPR_ARM1);
        ST_drawHUDSprite(spr, pos, height - 2, HOT_BRIGHT, iconAlpha);
        ST_HUDSpriteSize(spr, &w, &h);
        pos -= w + 2;
    }

    // Keys | use a bit of extra scale.
    if(cfg.hudShown[HUD_KEYS])
    {
        static int          keyPairs[3][2] = {
            {KT_REDCARD, KT_REDSKULL},
            {KT_YELLOWCARD, KT_YELLOWSKULL},
            {KT_BLUECARD, KT_BLUESKULL}
        };
        static int          keyIcons[NUM_KEY_TYPES] = {
            SPR_BKEY,
            SPR_YKEY,
            SPR_RKEY,
            SPR_BSKU,
            SPR_YSKU,
            SPR_RSKU
        };

Draw_BeginZoom(0.75f, pos , height - 2);
        for(i = 0; i < NUM_KEY_TYPES; ++i)
        {
            boolean             shown = true;

            if(!plr->keys[i])
                continue;

            if(cfg.hudKeysCombine)
            {
                int                 j;

                for(j = 0; j < 3; ++j)
                    if(keyPairs[j][0] == i &&
                       plr->keys[keyPairs[j][0]] && plr->keys[keyPairs[j][1]])
                    {
                        shown = false;
                        break;
                    }
            }

            if(shown)
            {
                spr = keyIcons[i];
                ST_drawHUDSprite(spr, pos, height - 2, HOT_BRIGHT, iconAlpha);
                ST_HUDSpriteSize(spr, &w, &h);
                pos -= w + 2;
            }
        }
Draw_EndZoom();
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void ST_Drawer(int fullscreenMode, boolean refresh)
{
    firstTime = firstTime || refresh;
    statusbarActive = (fullscreenMode < 2) || (AM_IsMapActive(CONSOLEPLAYER) &&
                     (cfg.automapHudDisplay == 0 || cfg.automapHudDisplay == 2));

    // Do palette shifts.
    ST_doPaletteStuff();

    // Either slide the statusbar in or fade out the fullscreen HUD.
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
        if(fullscreenMode == 3)
        {
            if(hudAlpha > 0.0f)
            {
                hudAlpha -= 0.1f;
                fullscreenMode = 2;
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

    // Always try to render statusbar with alpha in fullscreen modes.
    if(fullscreenMode)
        blended = 1;
    else
        blended = 0;

    if(statusbarActive)
        ST_doRefresh();
    else if(fullscreenMode != 3)
        ST_doFullscreenStuff();
}

void ST_loadGraphics(void)
{
    int                 i, j, faceNum;
    char                nameBuf[9];

    // Load the numbers, tall and short.
    for(i = 0; i < 10; ++i)
    {
        sprintf(nameBuf, "STTNUM%d", i);
        R_CachePatch(&tallNum[i], nameBuf);

        sprintf(nameBuf, "STYSNUM%d", i);
        R_CachePatch(&shortNum[i], nameBuf);
    }

    // Load percent key.
    // Note: why not load STMINUS here, too?
    R_CachePatch(&tallPercent, "STTPRCNT");

    // Key cards:
    for(i = 0; i < NUM_KEY_TYPES; ++i)
    {
        sprintf(nameBuf, "STKEYS%d", i);
        R_CachePatch(&keys[i], nameBuf);
    }

    // Arms background.
    R_CachePatch(&armsBackground, "STARMS");

    // Arms ownership widgets:
    for(i = 0; i < 6; ++i)
    {
        sprintf(nameBuf, "STGNUM%d", i + 2);

        // gray #
        R_CachePatch(&arms[i][0], nameBuf);

        // yellow #
        memcpy(&arms[i][1], &shortNum[i + 2], sizeof(dpatch_t));
    }

    // Face backgrounds for different color players.
    sprintf(nameBuf, "STFB%d", CONSOLEPLAYER);
    R_CachePatch(&faceBackground, nameBuf);

    // Status bar background bits.
    R_CachePatch(&statusbar, "STBAR");

    // Face states:
    faceNum = 0;
    for(i = 0; i < ST_NUMPAINFACES; ++i)
    {
        for(j = 0; j < ST_NUMSTRAIGHTFACES; ++j)
        {
            sprintf(nameBuf, "STFST%d%d", i, j);
            R_CachePatch(&faces[faceNum++], nameBuf);
        }
        sprintf(nameBuf, "STFTR%d0", i); // Turn right.
        R_CachePatch(&faces[faceNum++], nameBuf);
        sprintf(nameBuf, "STFTL%d0", i); // Turn left.
        R_CachePatch(&faces[faceNum++], nameBuf);
        sprintf(nameBuf, "STFOUCH%d", i); // Ouch.
        R_CachePatch(&faces[faceNum++], nameBuf);
        sprintf(nameBuf, "STFEVL%d", i); // Evil grin.
        R_CachePatch(&faces[faceNum++], nameBuf);
        sprintf(nameBuf, "STFKILL%d", i); // Pissed off.
        R_CachePatch(&faces[faceNum++], nameBuf);
    }
    R_CachePatch(&faces[faceNum++], "STFGOD0");
    R_CachePatch(&faces[faceNum++], "STFDEAD0");
}

void ST_updateGraphics(void)
{
    char                nameBuf[9];

    // Face backgrounds for different color players.
    sprintf(nameBuf, "STFB%d", cfg.playerColor[CONSOLEPLAYER]);
    R_CachePatch(&faceBackground, nameBuf);
}

void ST_loadData(void)
{
    ST_loadGraphics();
}

void ST_initData(void)
{
    int                 i;
    player_t           *plyr = &players[CONSOLEPLAYER];

    firstTime = true;
    statusbarActive = true;
    faceIndex = 0;
    currentPalette = -1;
    oldHealth = -1;

    for(i = 0; i < NUM_WEAPON_TYPES; ++i)
    {
        oldWeaponsOwned[i] = plyr->weaponOwned[i];
    }

    for(i = 0; i < 3; ++i)
    {
        keyBoxes[i] = -1;
    }

    STlib_init();

    ST_HUDUnHide(HUE_FORCE);
}

void ST_createWidgets(void)
{
    static int          largeAmmo = 1994;    // means "n/a"
    static ammotype_t   ammoType;

    int                 i;
    boolean             found;
    player_t           *plyr = &players[CONSOLEPLAYER];

    // Ready weapon ammo:
    //// \todo Only supports one type of ammo per weapon.
    found = false;
    for(ammoType = 0; ammoType < NUM_AMMO_TYPES && !found; ++ammoType)
    {
        if(!weaponInfo[plyr->readyWeapon][plyr->class].mode[0].ammoType[ammoType])
            continue; // Weapon does not take this ammo.

        STlib_initNum(&wReadyWeapon, ST_AMMOX, ST_AMMOY, tallNum, &plyr->ammo[ammoType],
                      &statusbarActive, ST_AMMOWIDTH, &statusbarCounterAlpha);
        found = true;
    }
    if(!found) // Weapon requires no ammo at all.
    {
        // DOOM.EXE returns an address beyond plyr->ammo[NUM_AMMO_TYPES]
        // if weaponInfo[plyr->readyWeapon].ammo == am_noammo
        // ...obviously a bug.

        //STlib_initNum(&wReadyWeapon, ST_AMMOX, ST_AMMOY, tallNum,
        //              &plyr->ammo[weaponInfo[plyr->readyWeapon].ammo],
        //              &statusbarActive, ST_AMMOWIDTH, &statusbarCounterAlpha);


        STlib_initNum(&wReadyWeapon, ST_AMMOX, ST_AMMOY, tallNum, &largeAmmo,
                      &statusbarActive, ST_AMMOWIDTH, &statusbarCounterAlpha);
    }

    // Last weapon type.
    wReadyWeapon.data = plyr->readyWeapon;

    // Health percentage.
    STlib_initPercent(&wHealth, ST_HEALTHX, ST_HEALTHY, tallNum,
                      &plyr->health, &statusbarActive, &tallPercent,
                      &statusbarCounterAlpha);

    // Weapons owned.
    for(i = 0; i < 6; ++i)
    {
        STlib_initMultIcon(&wArms[i], ST_ARMSX + (i % 3) * ST_ARMSXSPACE,
                           ST_ARMSY + (i / 3) * ST_ARMSYSPACE, arms[i],
                           (int *) &plyr->weaponOwned[i + 1], &statusbarArmsOn,
                           &statusbarCounterAlpha);
    }

    // Frags sum.
    STlib_initNum(&wFrags, ST_FRAGSX, ST_FRAGSY, tallNum, &currentFragsCount,
                  &statusbarFragsOn, ST_FRAGSWIDTH, &statusbarCounterAlpha);

    // Faces.
    STlib_initMultIcon(&wFaces, ST_FACESX, ST_FACESY, faces, &faceIndex,
                       &statusbarActive, &statusbarCounterAlpha);

    // Armor percentage - should be colored later.
    STlib_initPercent(&wArmor, ST_ARMORX, ST_ARMORY, tallNum,
                      &plyr->armorPoints, &statusbarActive, &tallPercent,
                      &statusbarCounterAlpha);

    // Keyboxes 0-2.
    STlib_initMultIcon(&wKeyBoxes[0], ST_KEY0X, ST_KEY0Y, keys, &keyBoxes[0],
                       &statusbarActive, &statusbarCounterAlpha);

    STlib_initMultIcon(&wKeyBoxes[1], ST_KEY1X, ST_KEY1Y, keys, &keyBoxes[1],
                       &statusbarActive, &statusbarCounterAlpha);

    STlib_initMultIcon(&wKeyBoxes[2], ST_KEY2X, ST_KEY2Y, keys, &keyBoxes[2],
                       &statusbarActive, &statusbarCounterAlpha);

    // Ammo count (all four kinds).
    STlib_initNum(&wAmmo[0], ST_AMMO0X, ST_AMMO0Y, shortNum, &plyr->ammo[0],
                  &statusbarActive, ST_AMMO0WIDTH, &statusbarCounterAlpha);

    STlib_initNum(&wAmmo[1], ST_AMMO1X, ST_AMMO1Y, shortNum, &plyr->ammo[1],
                  &statusbarActive, ST_AMMO1WIDTH, &statusbarCounterAlpha);

    STlib_initNum(&wAmmo[2], ST_AMMO2X, ST_AMMO2Y, shortNum, &plyr->ammo[2],
                  &statusbarActive, ST_AMMO2WIDTH, &statusbarCounterAlpha);

    STlib_initNum(&wAmmo[3], ST_AMMO3X, ST_AMMO3Y, shortNum, &plyr->ammo[3],
                  &statusbarActive, ST_AMMO3WIDTH, &statusbarCounterAlpha);

    // Max ammo count (all four kinds).
    STlib_initNum(&wMaxAmmo[0], ST_MAXAMMO0X, ST_MAXAMMO0Y, shortNum,
                  &plyr->maxAmmo[0], &statusbarActive, ST_MAXAMMO0WIDTH,
                  &statusbarCounterAlpha);

    STlib_initNum(&wMaxAmmo[1], ST_MAXAMMO1X, ST_MAXAMMO1Y, shortNum,
                  &plyr->maxAmmo[1], &statusbarActive, ST_MAXAMMO1WIDTH,
                  &statusbarCounterAlpha);

    STlib_initNum(&wMaxAmmo[2], ST_MAXAMMO2X, ST_MAXAMMO2Y, shortNum,
                  &plyr->maxAmmo[2], &statusbarActive, ST_MAXAMMO2WIDTH,
                  &statusbarCounterAlpha);

    STlib_initNum(&wMaxAmmo[3], ST_MAXAMMO3X, ST_MAXAMMO3Y, shortNum,
                  &plyr->maxAmmo[3], &statusbarActive, ST_MAXAMMO3WIDTH,
                  &statusbarCounterAlpha);
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
