/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
#include <math.h>

#include "jdoom.h"

#include "d_net.h"
#include "st_lib.h"
#include "hu_stuff.h"
#include "p_tick.h" // for P_IsPaused
#include "p_player.h"
#include "am_map.h"
#include "p_user.h"

// MACROS ------------------------------------------------------------------

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
#define ST_CURRENTAMMOWIDTH (3)
#define ST_CURRENTAMMOX     (44)
#define ST_CURRENTAMMOY     (171)

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

// Ready Ammunition counter.
#define ST_READYAMMOWIDTH   (3)
#define ST_READYAMMOX       (44)
#define ST_READYAMMOY       (171)

// Ammo counters.
#define ST_AMMOWIDTH        (3)
#define ST_AMMOHEIGHT       (6)
#define ST_AMMOX            (288)
#define ST_AMMOY            (173)

#define ST_MAXAMMOWIDTH     (3)
#define ST_MAXAMMOHEIGHT    (6)
#define ST_MAXAMMOX         (314)
#define ST_MAXAMMOY         (173)

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

typedef struct {
    boolean         stopped;
    int             hideTics;
    float           hideAmount;
    float           alpha; // Fullscreen hud alpha value.

    float           showBar; // Slide statusbar amount 1.0 is fully open.
    float           statusbarCounterAlpha;

    boolean         firstTime; // ST_Start() has just been called.
    boolean         blended; // Whether to use alpha blending.
    boolean         statusbarActive; // Whether the main status bar is active.
    int             currentFragsCount; // Number of frags so far in deathmatch.
    int             keyBoxes[3]; // Holds key-type for each key box on bar.

    // For status face:
    int             oldHealth; // Used to use appopriately pained face.
    boolean         oldWeaponsOwned[NUM_WEAPON_TYPES]; // Used for evil grin.
    int             faceCount; // Count until face changes.
    int             faceIndex; // Current face index, used by wFaces.
    int             lastAttackDown;
    int             priority;

    // Widgets:
    st_number_t     wReadyWeapon; // Ready-weapon widget.
    st_number_t     wFrags; // In deathmatch only, summary of frags stats.
    st_percent_t    wHealth; // Health widget.
    st_multiicon_t  wArms[6]; // Weapon ownership widgets.
    st_multiicon_t  wFaces; // Face status widget.
    st_multiicon_t  wKeyBoxes[3]; // Keycard widgets.
    st_percent_t    wArmor; // Armor widget.
    st_number_t     wAmmo[NUM_AMMO_TYPES]; // Ammo widgets.
    st_number_t     wMaxAmmo[NUM_AMMO_TYPES]; // Max ammo widgets.
} hudstate_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

DEFCC(CCmdStatusBarSize);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static hudstate_t hudStates[MAXPLAYERS];

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
static dpatch_t faceBackground[4];

 // Main bar right.
static dpatch_t armsBackground;

// Weapon ownership patches.
static dpatch_t arms[6][2];

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

    {"hud-face-ouchfix", 0, CVT_BYTE, &cfg.fixOuchFace, 0, 1},

    {"hud-status-alpha", 0, CVT_FLOAT, &cfg.statusbarOpacity, 0, 1},
    {"hud-status-icon-a", 0, CVT_FLOAT, &cfg.statusbarCounterAlpha, 0, 1},
    {"hud-status-weaponslots-ownedfix", 0, CVT_BYTE, &cfg.fixStatusbarOwnedWeapons, 0, 1},

    // HUD icons
    {"hud-face", 0, CVT_BYTE, &cfg.hudShown[HUD_FACE], 0, 1},
    {"hud-health", 0, CVT_BYTE, &cfg.hudShown[HUD_HEALTH], 0, 1},
    {"hud-armor", 0, CVT_BYTE, &cfg.hudShown[HUD_ARMOR], 0, 1},
    {"hud-ammo", 0, CVT_BYTE, &cfg.hudShown[HUD_AMMO], 0, 1},
    {"hud-keys", 0, CVT_BYTE, &cfg.hudShown[HUD_KEYS], 0, 1},
    {"hud-keys-combine", 0, CVT_BYTE, &cfg.hudKeysCombine, 0, 1},

    // HUD displays
    {"hud-frags", 0, CVT_BYTE, &cfg.hudShown[HUD_FRAGS], 0, 1},

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

static void drawStatusBarBackground(int player, float width, float height)
{
    float               x, y, w, h;
    float               cw, cw2, ch;
    float               alpha;
    hudstate_t*         hud = &hudStates[player];
    float               armsBGX = ST_ARMSBGX - armsBackground.leftOffset;

    DGL_SetPatch(statusbar.lump, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);

    if(hud->blended)
    {
        alpha = cfg.statusbarOpacity - hud->hideAmount;
        if(!(alpha > 0))
            return;

        alpha = MINMAX_OF(0.f, alpha, 1.f);
    }
    else
        alpha = 1.0f;

    DGL_Color4f(1, 1, 1, alpha);

    if(!(alpha < 1))
    {
        // We can draw the full graphic in one go.
        DGL_Begin(DGL_QUADS);
            DGL_TexCoord2f(0, 0, 0);
            DGL_Vertex2f(0, 0);
            DGL_TexCoord2f(0, 1, 0);
            DGL_Vertex2f(width, 0);
            DGL_TexCoord2f(0, 1, 1);
            DGL_Vertex2f(width, height);
            DGL_TexCoord2f(0, 0, 1);
            DGL_Vertex2f(0, height);
        DGL_End();
    }
    else
    {
        // Alpha blended status bar, we'll need to cut it up into smaller bits...
        DGL_Begin(DGL_QUADS);

        // Up to faceback if deathmatch, else ST_ARMS.
        x = 0;
        y = 0;
        w = width * (float) ((hud->statusbarActive && !deathmatch) ? armsBGX : ST_FX) / ST_WIDTH;
        h = height * (float) ST_HEIGHT / ST_HEIGHT;
        cw = w / width;

        DGL_TexCoord2f(0, 0, 0);
        DGL_Vertex2f(x, y);
        DGL_TexCoord2f(0, cw, 0);
        DGL_Vertex2f(x + w, y);
        DGL_TexCoord2f(0, cw, 1);
        DGL_Vertex2f(x + w, y + h);
        DGL_TexCoord2f(0, 0, 1);
        DGL_Vertex2f(x, y + h);

        if(IS_NETGAME)
        {
            // Fill in any gap left before the faceback due to small ARMS.
            if(armsBGX + armsBackground.width < ST_FX)
            {
                x = width * (float) (armsBGX + armsBackground.width) / ST_WIDTH;
                y = 0;
                w = width * (float) (ST_FX - armsBGX - armsBackground.width) / ST_WIDTH;
                h = height * (float) (ST_HEIGHT) / ST_HEIGHT;
                cw = x / width;
                cw2 = (x + w) / width;
                ch = h / height;

                DGL_TexCoord2f(0, cw, 0);
                DGL_Vertex2f(x, y);
                DGL_TexCoord2f(0, cw2, 0);
                DGL_Vertex2f(x + w, y);
                DGL_TexCoord2f(0, cw2, ch);
                DGL_Vertex2f(x + w, y + h);
                DGL_TexCoord2f(0, cw, ch);
                DGL_Vertex2f(x, y + h);
            }

            // Awkward, 2 pixel tall strip above faceback.
            x = width * (float) (ST_FX) / ST_WIDTH;
            y = 0;
            w = width * (float) (ST_WIDTH - ST_FX - 141 - 2) / ST_WIDTH;
            h = height * (float) (ST_HEIGHT - 30) / ST_HEIGHT;
            cw = x / width;
            cw2 = (x + w) / width;
            ch = h / height;

            DGL_TexCoord2f(0, cw, 0);
            DGL_Vertex2f(x, y);
            DGL_TexCoord2f(0, cw2, 0);
            DGL_Vertex2f(x + w, y);
            DGL_TexCoord2f(0, cw2, ch);
            DGL_Vertex2f(x + w, y + h);
            DGL_TexCoord2f(0, cw, ch);
            DGL_Vertex2f(x, y + h);

            // Awkward, 1 pixel tall strip bellow faceback.
            x = width * (float) (ST_FX) / ST_WIDTH;
            y = height * (float) (ST_HEIGHT - 1) / ST_HEIGHT;
            w = width * (float) (ST_WIDTH - ST_FX - 141 - 2) / ST_WIDTH;
            h = height * (float) (ST_HEIGHT - 31) / ST_HEIGHT;
            cw = x / width;
            cw2 = (x + w) / width;
            ch = (ST_HEIGHT - 1) / height;

            DGL_TexCoord2f(0, cw, ch);
            DGL_Vertex2f(x, y);
            DGL_TexCoord2f(0, cw2, ch);
            DGL_Vertex2f(x + w, y);
            DGL_TexCoord2f(0, cw2, 1);
            DGL_Vertex2f(x + w, y + h);
            DGL_TexCoord2f(0, cw, 1);
            DGL_Vertex2f(x, y + h);

            // After faceback.
            x = width * (float) (ST_FX + (ST_WIDTH - ST_FX - 141 - 2)) / ST_WIDTH;
            y = 0;
            w = width * (float) (ST_WIDTH - (ST_FX + (ST_WIDTH - ST_FX - 141 - 2))) / ST_WIDTH;
            h = height * (float) ST_HEIGHT / ST_HEIGHT;
            cw = x / width;
        }
        else
        {
            // Including area behind the face status indicator.
            x = width * (float) (armsBGX + armsBackground.width) / ST_WIDTH;
            y = 0;
            w = width * (float) (ST_WIDTH - armsBGX - armsBackground.width) / ST_WIDTH;
            h = height * (float) ST_HEIGHT / ST_HEIGHT;
            cw = x / width;
        }

        DGL_TexCoord2f(0, cw, 0);
        DGL_Vertex2f(x, y);
        DGL_TexCoord2f(0, 1, 0);
        DGL_Vertex2f(x + w, y);
        DGL_TexCoord2f(0, 1, 1);
        DGL_Vertex2f(x + w, y + h);
        DGL_TexCoord2f(0, cw, 1);
        DGL_Vertex2f(x, y + h);

        DGL_End();
    }

    if(!deathmatch)
    {   // Draw the ARMS background.
        DGL_SetPatch(armsBackground.lump, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);

        x = width * ((float) armsBGX - ST_X) / ST_WIDTH;
        y = height * ((float) armsBackground.topOffset) / ST_HEIGHT;
        w = width * ((float) armsBackground.width) / ST_WIDTH;
        h = height * ((float) armsBackground.height) / ST_HEIGHT;

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

    if(IS_NETGAME) // Faceback.
    {
        int             plrColor = cfg.playerColor[player];
        dpatch_t*       patch = &faceBackground[plrColor];

        DGL_SetPatch(patch->lump, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);

        x = width * (float) (ST_FX - ST_X) / ST_WIDTH;
        y = height * (float) (ST_HEIGHT - 30) / ST_HEIGHT;
        w = width * (float) (ST_WIDTH - ST_FX - 141 - 2) / ST_WIDTH;
        h = height * (float) (ST_HEIGHT - 3) / ST_HEIGHT;
        cw = (float) (1) / patch->width;
        cw2 = (float) (patch->width - 1) / patch->width;
        ch = (float) (patch->height - 1) / patch->height;

        DGL_Begin(DGL_QUADS);
            DGL_TexCoord2f(0, cw, 0);
            DGL_Vertex2f(x, y);
            DGL_TexCoord2f(0, cw2, 0);
            DGL_Vertex2f(x + w, y);
            DGL_TexCoord2f(0, cw2, ch);
            DGL_Vertex2f(x + w, y + h);
            DGL_TexCoord2f(0, cw, ch);
            DGL_Vertex2f(x, y + h);
        DGL_End();
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

static int calcPainOffset(hudstate_t* hud)
{
    player_t*           plr = &players[hud - hudStates];
    int                 health = (plr->health > 100 ? 100 : plr->health);

    return  ST_FACESTRIDE * (((100 - health) * ST_NUMPAINFACES) / 101);
}

/**
 * This is a not-very-pretty routine which handles the face states
 * and their timing. the precedence of expressions is:
 *
 * dead > evil grin > turned head > straight ahead
 */
void ST_updateFaceWidget(int player)
{
    int                 i;
    angle_t             badGuyAngle;
    angle_t             diffAng;
    boolean             doEvilGrin;
    player_t*           plr = &players[player];
    hudstate_t*         hud = &hudStates[player];

    if(hud->priority < 10)
    {   // Player is dead.
        if(!plr->health)
        {
            hud->priority = 9;
            hud->faceIndex = ST_DEADFACE;
            hud->faceCount = 1;
        }
    }

    if(hud->priority < 9)
    {
        if(plr->bonusCount)
        {   // Picking up a bonus.
            doEvilGrin = false;

            for(i = 0; i < NUM_WEAPON_TYPES; ++i)
            {
                if(hud->oldWeaponsOwned[i] != plr->weapons[i].owned)
                {
                    doEvilGrin = true;
                    hud->oldWeaponsOwned[i] = plr->weapons[i].owned;
                }
            }

            if(doEvilGrin)
            {   // Evil grin if just picked up weapon.
                hud->priority = 8;
                hud->faceCount = ST_EVILGRINCOUNT;
                hud->faceIndex = calcPainOffset(hud) + ST_EVILGRINOFFSET;
            }
        }
    }

    if(hud->priority < 8)
    {
        if(plr->damageCount && plr->attacker &&
           plr->attacker != plr->plr->mo)
        {   // Being attacked.
            hud->priority = 7;

            // DOOM BUG
            // This test was inversed, thereby the OUCH face was NEVER used
            // in normal gameplay as it requires the player recieving damage
            // to end up with MORE health than he started with.
            // Also, priority was not changed which would have resulted in a
            // frame duration of only 1 tic.
            // if(plr->health - oldHealth > ST_MUCHPAIN)

            if((cfg.fixOuchFace?
               (hud->oldHealth - plr->health) :
               (plr->health - hud->oldHealth)) > ST_MUCHPAIN)
            {
                hud->faceCount = ST_TURNCOUNT;
                hud->faceIndex = calcPainOffset(hud) + ST_OUCHOFFSET;
                if(cfg.fixOuchFace)
                    hud->priority = 8; // Added to fix 1 tic issue.
            }
            else
            {
                badGuyAngle =
                    R_PointToAngle2(FLT2FIX(plr->plr->mo->pos[VX]),
                                    FLT2FIX(plr->plr->mo->pos[VY]),
                                    FLT2FIX(plr->attacker->pos[VX]),
                                    FLT2FIX(plr->attacker->pos[VY]));

                if(badGuyAngle > plr->plr->mo->angle)
                {   // Whether right or left.
                    diffAng = badGuyAngle - plr->plr->mo->angle;
                    i = diffAng > ANG180;
                }
                else
                {   // Whether left or right.
                    diffAng = plr->plr->mo->angle - badGuyAngle;
                    i = diffAng <= ANG180;
                }

                hud->faceCount = ST_TURNCOUNT;
                hud->faceIndex = calcPainOffset(hud);

                if(diffAng < ANG45)
                {   // Head-on.
                    hud->faceIndex += ST_RAMPAGEOFFSET;
                }
                else if(i)
                {   // Turn face right.
                    hud->faceIndex += ST_TURNOFFSET;
                }
                else
                {   // Turn face left.
                    hud->faceIndex += ST_TURNOFFSET + 1;
                }
            }
        }
    }

    if(hud->priority < 7)
    {   // Getting hurt because of your own damn stupidity.
        if(plr->damageCount)
        {
            // DOOM BUG
            // This test was inversed, thereby the OUCH face was NEVER used
            // in normal gameplay as it requires the player recieving damage
            // to end up with MORE health than he started with.
            // if(plr->health - oldHealth > ST_MUCHPAIN)

            if((cfg.fixOuchFace?
               (hud->oldHealth - plr->health) :
               (plr->health - hud->oldHealth)) > ST_MUCHPAIN)
            {
                hud->priority = 7;
                hud->faceCount = ST_TURNCOUNT;
                hud->faceIndex = calcPainOffset(hud) + ST_OUCHOFFSET;
            }
            else
            {
                hud->priority = 6;
                hud->faceCount = ST_TURNCOUNT;
                hud->faceIndex = calcPainOffset(hud) + ST_RAMPAGEOFFSET;
            }
        }
    }

    if(hud->priority < 6)
    {   // Rapid firing.
        if(plr->attackDown)
        {
            if(hud->lastAttackDown == -1)
            {
                hud->lastAttackDown = ST_RAMPAGEDELAY;
            }
            else if(!--hud->lastAttackDown)
            {
                hud->priority = 5;
                hud->faceIndex = calcPainOffset(hud) + ST_RAMPAGEOFFSET;
                hud->faceCount = 1;
                hud->lastAttackDown = 1;
            }
        }
        else
        {
            hud->lastAttackDown = -1;
        }
    }

    if(hud->priority < 5)
    {   // Invulnerability.
        if((P_GetPlayerCheats(plr) & CF_GODMODE) ||
           plr->powers[PT_INVULNERABILITY])
        {
            hud->priority = 4;

            hud->faceIndex = ST_GODFACE;
            hud->faceCount = 1;
        }
    }

    // Look left or look right if the facecount has timed out.
    if(!hud->faceCount)
    {
        hud->faceIndex = calcPainOffset(hud) + (M_Random() % 3);
        hud->faceCount = ST_STRAIGHTFACECOUNT;
        hud->priority = 0;
    }

    hud->faceCount--;
}

void ST_updateWidgets(int player)
{
    static int          largeAmmo = 1994; // Means "n/a".

    int                 i;
    ammotype_t          ammoType;
    boolean             found;
    hudstate_t*         hud = &hudStates[player];
    player_t*           plr = &players[player];

    if(hud->blended)
    {
        hud->statusbarCounterAlpha =
            MINMAX_OF(0.f, cfg.statusbarCounterAlpha - hud->hideAmount, 1.f);
    }
    else
        hud->statusbarCounterAlpha = 1.0f;

    // Must redirect the pointer if the ready weapon has changed.
    found = false;
    for(ammoType=0; ammoType < NUM_AMMO_TYPES && !found; ++ammoType)
    {
        if(!weaponInfo[plr->readyWeapon][plr->class_].mode[0].ammoType[ammoType])
            continue; // Weapon does not use this type of ammo.

        //// \todo Only supports one type of ammo per weapon
        hud->wReadyWeapon.num = &plr->ammo[ammoType].owned;
        found = true;
    }
    if(!found) // Weapon takes no ammo at all.
    {
        hud->wReadyWeapon.num = &largeAmmo;
    }

    // Update keycard multiple widgets.
    for(i = 0; i < 3; ++i)
    {
        hud->keyBoxes[i] = plr->keys[i] ? i : -1;

        if(plr->keys[i + 3])
            hud->keyBoxes[i] = i + 3;
    }

    // Refresh everything if this is him coming back to life.
    ST_updateFaceWidget(player);

    // Used by wFrags widget.
    hud->currentFragsCount = 0;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->inGame)
            continue;

        hud->currentFragsCount += plr->frags[i] * (i != player ? 1 : -1);
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
            hud->oldHealth = plr->health;
        }
    }
}

void ST_doPaletteStuff(int player)
{
    int                 palette = 0, cnt, bzc;
    player_t*           plr = &players[player];

    cnt = plr->damageCount;

    if(plr->powers[PT_STRENGTH])
    {   // Slowly fade the berzerk out.
        bzc = 12 - (plr->powers[PT_STRENGTH] >> 6);

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
    else if(plr->bonusCount)
    {
        palette = (plr->bonusCount + 7) >> 3;
        if(palette >= NUMBONUSPALS)
            palette = NUMBONUSPALS - 1;
        palette += STARTBONUSPALS;
    }
    else if(plr->powers[PT_IRONFEET] > 4 * 32 ||
            plr->powers[PT_IRONFEET] & 8)
    {
        palette = RADIATIONPAL;
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

typedef struct {
    hudstate_t*     hud;
    int             slot;
    float           alpha;
} drawownedweapondisply_params_t;

int drawOwnedWeaponDisplay(weapontype_t type, void* context)
{
    drawownedweapondisply_params_t* params =
        (drawownedweapondisply_params_t*) context;
    const player_t*     plr = &players[params->hud - hudStates];

    if(cfg.fixStatusbarOwnedWeapons)
    {
        if(!plr->weapons[type].owned)
            return 1; // Continue iteration.
    }

    STlib_DrawMultiIcon(&params->hud->wArms[params->slot],
                        plr->weapons[type].owned ? 1 : 0, params->alpha);

    return 0; // Stop iteration.
}

static void drawWidgets(hudstate_t* hud)
{
    int                 i;
    float               alpha = hud->statusbarCounterAlpha;

    STlib_DrawNum(&hud->wReadyWeapon, alpha);

    for(i = 0; i < 4; ++i)
    {
        STlib_DrawNum(&hud->wAmmo[i], alpha);
        STlib_DrawNum(&hud->wMaxAmmo[i], alpha);
    }

    STlib_DrawPercent(&hud->wHealth, alpha);
    STlib_DrawPercent(&hud->wArmor, alpha);

    if(!deathmatch)
    {
        for(i = 0; i < 6; ++i)
        {
            int                 result;
            drawownedweapondisply_params_t params;

            params.hud = hud;
            params.slot = i;
            params.alpha = alpha;

            result =
                P_IterateWeaponsInSlot(i+1, true, drawOwnedWeaponDisplay,
                                       &params);

            if(cfg.fixStatusbarOwnedWeapons && result)
            {   // No weapon bound to slot is owned by player.
                STlib_DrawMultiIcon(&hud->wArms[i], 0, alpha);
            }
        }
    }
    else
        STlib_DrawNum(&hud->wFrags, alpha);

    STlib_DrawMultiIcon(&hud->wFaces, hud->faceIndex, alpha);

    for(i = 0; i < 3; ++i)
    {
        if(hud->keyBoxes[i] != -1)
        {
            STlib_DrawMultiIcon(&hud->wKeyBoxes[i], hud->keyBoxes[i], alpha);
        }
    }
}

void ST_doRefresh(int player)
{
    hudstate_t*         hud;
    boolean             statusbarVisible;
    float               fscale, h;

    if(player < 0 || player > MAXPLAYERS)
        return;

    hud = &hudStates[player];

    statusbarVisible = (cfg.statusbarScale < 20 ||
        (cfg.statusbarScale == 20 && hud->showBar < 1.0f));

    hud->firstTime = false;

    fscale = cfg.statusbarScale / 20.0f;
    h = SCREENHEIGHT * (1 - fscale);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef((SCREENWIDTH/2) - SCREENWIDTH * fscale / 2,
                   h / hud->showBar, 0);
    DGL_Scalef(fscale, fscale, 1);
    DGL_Translatef(ST_X, ST_Y, 0);

    // Draw background.
    drawStatusBarBackground(player, ST_WIDTH, ST_HEIGHT);

    DGL_Translatef(-ST_X, -ST_Y, 0);

    drawWidgets(hud);

    // Restore the normal modelview matrix.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
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

void ST_drawHUDSprite(int sprite, float x, float y, hotloc_t hotspot,
                      float scale, float alpha, boolean flip)
{
    int                 w, h, w2, h2;
    float               s, t;
    spriteinfo_t        sprInfo;

    if(!(alpha > 0))
        return;

    alpha = MINMAX_OF(0.f, alpha, 1.f);
    R_GetSpriteInfo(sprite, 0, &sprInfo);
    w = sprInfo.width;
    h = sprInfo.height;
    w2 = M_CeilPow2(w);
    h2 = M_CeilPow2(h);

    switch(hotspot)
    {
    case HOT_BRIGHT:
        y -= h * scale;

    case HOT_TRIGHT:
        x -= w * scale;
        break;

    case HOT_BLEFT:
        y -= h * scale;
        break;
    }

    DGL_SetPSprite(sprInfo.material);

    // Let's calculate texture coordinates.
    // To remove a possible edge artifact, move the corner a bit up/left.
    s = (w - 0.4f) / w2;
    t = (h - 0.4f) / h2;

    DGL_Color4f(1, 1, 1, alpha);
    DGL_Begin(DGL_QUADS);
        DGL_TexCoord2f(0, flip * s, 0);
        DGL_Vertex2f(x, y);

        DGL_TexCoord2f(0, !flip * s, 0);
        DGL_Vertex2f(x + w * scale, y);

        DGL_TexCoord2f(0, !flip * s, t);
        DGL_Vertex2f(x + w * scale, y + h * scale);

        DGL_TexCoord2f(0, flip * s, t);
        DGL_Vertex2f(x, y + h * scale);
    DGL_End();
}

void ST_doFullscreenStuff(int player)
{
    static const int    ammoSprite[NUM_AMMO_TYPES] = {
        SPR_AMMO,
        SPR_SBOX,
        SPR_CELL,
        SPR_ROCK
    };

    hudstate_t*         hud = &hudStates[player];
    player_t*           plr = &players[player];
    char                buf[20];
    int                 w, h, pos = 0, spr, i;
    int                 width = 320 / cfg.hudScale;
    int                 height = 200 / cfg.hudScale;
    float               textAlpha =
        MINMAX_OF(0.f, hud->alpha - hud->hideAmount - ( 1 - cfg.hudColor[3]), 1.f);
    float               iconAlpha =
        MINMAX_OF(0.f, hud->alpha - hud->hideAmount - ( 1 - cfg.hudIconAlpha), 1.f);

    if(IS_NETGAME && deathmatch && cfg.hudShown[HUD_FRAGS])
    {
        // Display the frag counter.
        i = 199 - 8;
        if(cfg.hudShown[HUD_HEALTH] || cfg.hudShown[HUD_AMMO])
        {
            i -= 18 * cfg.hudScale;
        }
        sprintf(buf, "FRAGS:%i", hud->currentFragsCount);
        M_WriteText2(2, i, buf, GF_FONTA, cfg.hudColor[0], cfg.hudColor[1],
                     cfg.hudColor[2], textAlpha);
    }

    // Setup the scaling matrix.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);

    // Draw the visible HUD data, first health.
    if(cfg.hudShown[HUD_HEALTH])
    {
        ST_drawHUDSprite(SPR_STIM, 2, height - 2, HOT_BLEFT, 1, iconAlpha,
                         false);
        ST_HUDSpriteSize(SPR_STIM, &w, &h);
        pos = w + 2;
        sprintf(buf, "%i%%", plr->health);
        M_WriteText2(pos, height - 14, buf, GF_FONTB, cfg.hudColor[0],
                     cfg.hudColor[1], cfg.hudColor[2], textAlpha);
        pos += M_StringWidth(buf, GF_FONTB) + 2;
    }

    if(cfg.hudShown[HUD_AMMO])
    {
        ammotype_t          ammoType;
        float               scale;

        //// \todo Only supports one type of ammo per weapon.
        //// for each type of ammo this weapon takes.
        for(ammoType = 0; ammoType < NUM_AMMO_TYPES; ++ammoType)
        {
            if(!weaponInfo[plr->readyWeapon][plr->class_].mode[0].ammoType[ammoType])
                continue;

            spr = ammoSprite[ammoType];
            scale = (spr == SPR_ROCK? .72f : 1);

            ST_drawHUDSprite(spr, pos, height - 2, HOT_BLEFT, scale,
                             iconAlpha, false);
            ST_HUDSpriteSize(spr, &w, &h);
            pos += w + 2;
            sprintf(buf, "%i", plr->ammo[ammoType].owned);
            M_WriteText2(pos, height - 14, buf, GF_FONTB,
                         cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2],
                         textAlpha);
            break;
        }
    }

    // Doomguy's face | use a bit of extra scale.
    if(cfg.hudShown[HUD_FACE])
    {
        int             plrColor = cfg.playerColor[player];

        pos = (width/2) -(faceBackground[plrColor].width/2) + 6;

        if(iconAlpha != 0.0f)
        {
Draw_BeginZoom(0.7f, pos , height - 1);
            DGL_Color4f(1, 1, 1, iconAlpha);
            if(IS_NETGAME)
                GL_DrawPatch_CS(pos, height - faceBackground[plrColor].height + 1,
                                faceBackground[plrColor].lump);

            GL_DrawPatch_CS(pos, height - faceBackground[plrColor].height,
                            faces[hud->faceIndex].lump);
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
        armorOffset = M_StringWidth(buf, GF_FONTB);

        sprintf(buf, "%i%%", plr->armorPoints);
        pos -= armorOffset;
        M_WriteText2(pos + armorOffset - M_StringWidth(buf, GF_FONTB), height - 14, buf, GF_FONTB,
                     cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
        pos -= 2;

        spr = (plr->armorType == 2 ? SPR_ARM2 : SPR_ARM1);
        ST_drawHUDSprite(spr, pos, height - 2, HOT_BRIGHT, 1, iconAlpha,
                         false);
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
                ST_drawHUDSprite(spr, pos, height - 2, HOT_BRIGHT, 1,
                                 iconAlpha, false);
                ST_HUDSpriteSize(spr, &w, &h);
                pos -= w + 2;
            }
        }
Draw_EndZoom();
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void ST_Drawer(int player, int fullscreenMode, boolean refresh)
{
    hudstate_t*         hud;
    player_t*           plr;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    plr = &players[player];
    if(/*!((plr->plr->flags & DDPF_LOCAL) &&*/ !plr->plr->inGame)
        return;

    hud = &hudStates[player];

    hud->firstTime = hud->firstTime || refresh;
    hud->statusbarActive = (fullscreenMode < 2) ||
        (AM_IsActive(AM_MapForPlayer(player)) &&
         (cfg.automapHudDisplay == 0 || cfg.automapHudDisplay == 2));

    // Do palette shifts.
    ST_doPaletteStuff(player);

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
        if(fullscreenMode == 3)
        {
            if(hud->alpha > 0.0f)
            {
                hud->alpha -= 0.1f;
                fullscreenMode = 2;
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

    // Always try to render statusbar with alpha in fullscreen modes.
    if(fullscreenMode)
        hud->blended = 1;
    else
        hud->blended = 0;

    if(hud->statusbarActive)
        ST_doRefresh(player);
    else if(fullscreenMode != 3)
        ST_doFullscreenStuff(player);
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
    for(i = 0; i < 4; ++i)
    {
        sprintf(nameBuf, "STFB%d", i);
        R_CachePatch(&faceBackground[i], nameBuf);
    }

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
    hud->faceIndex = 0;
    hud->oldHealth = -1;
    hud->priority = 0;
    hud->lastAttackDown = -1;
    hud->blended = false;
    hud->showBar = 0.f;
    hud->statusbarCounterAlpha = 1.f;

    for(i = 0; i < 3; ++i)
    {
        hud->keyBoxes[i] = -1;
    }

    for(i = 0; i < NUM_WEAPON_TYPES; ++i)
    {
        hud->oldWeaponsOwned[i] = plr->weapons[i].owned;
    }

    ST_HUDUnHide(player, HUE_FORCE);
}

void ST_createWidgets(int player)
{
    typedef struct {
        float               x, y;
    } hudelement_t;

    static int          largeAmmo = 1994; // means "n/a"
    static const hudelement_t ammoPos[NUM_AMMO_TYPES] =
    {
        {ST_AMMOX, ST_AMMOY},
        {ST_AMMOX, ST_AMMOY + (ST_AMMOHEIGHT * 1)},
        {ST_AMMOX, ST_AMMOY + (ST_AMMOHEIGHT * 3)},
        {ST_AMMOX, ST_AMMOY + (ST_AMMOHEIGHT * 2)}
    };
    static const hudelement_t ammoMaxPos[NUM_AMMO_TYPES] =
    {
        {ST_MAXAMMOX, ST_MAXAMMOY},
        {ST_MAXAMMOX, ST_MAXAMMOY + (ST_AMMOHEIGHT * 1)},
        {ST_MAXAMMOX, ST_MAXAMMOY + (ST_AMMOHEIGHT * 3)},
        {ST_MAXAMMOX, ST_MAXAMMOY + (ST_AMMOHEIGHT * 2)}
    };

    ammotype_t          ammoType;
    int*                ptr = &largeAmmo;
    int                 i;
    boolean             found;
    player_t*           plr = &players[player];
    hudstate_t*         hud = &hudStates[player];

    // Ready weapon ammo:
    //// \todo Only supports one type of ammo per weapon.
    found = false;
    for(ammoType = 0; ammoType < NUM_AMMO_TYPES && !found; ++ammoType)
    {
        if(!weaponInfo[plr->readyWeapon][plr->class_].mode[0].ammoType[ammoType])
            continue; // Weapon does not take this ammo.

        ptr = &plr->ammo[ammoType].owned;
        found = true;
    }

    STlib_InitNum(&hud->wReadyWeapon, ST_READYAMMOX, ST_READYAMMOY, tallNum,
                  ptr, ST_READYAMMOWIDTH, 1);

    // Health percentage.
    STlib_InitPercent(&hud->wHealth, ST_HEALTHX, ST_HEALTHY, tallNum,
                      &plr->health, &tallPercent, 1);

    // Weapons owned.
    for(i = 0; i < 6; ++i)
    {
        STlib_InitMultiIcon(&hud->wArms[i], ST_ARMSX + (i % 3) * ST_ARMSXSPACE,
                            ST_ARMSY + (i / 3) * ST_ARMSYSPACE, arms[i], 1);
    }

    // Frags sum.
    STlib_InitNum(&hud->wFrags, ST_FRAGSX, ST_FRAGSY, tallNum,
                  &hud->currentFragsCount, ST_FRAGSWIDTH, 1);

    // Faces.
    STlib_InitMultiIcon(&hud->wFaces, ST_FACESX, ST_FACESY, faces, 1);

    // Armor percentage - should be colored later.
    STlib_InitPercent(&hud->wArmor, ST_ARMORX, ST_ARMORY, tallNum,
                      &plr->armorPoints, &tallPercent, 1);

    // Keyboxes 0-2.
    STlib_InitMultiIcon(&hud->wKeyBoxes[0], ST_KEY0X, ST_KEY0Y, keys, 1);

    STlib_InitMultiIcon(&hud->wKeyBoxes[1], ST_KEY1X, ST_KEY1Y, keys, 1);

    STlib_InitMultiIcon(&hud->wKeyBoxes[2], ST_KEY2X, ST_KEY2Y, keys, 1);

    // Ammo count and max (all four kinds).
    for(i = 0; i < NUM_AMMO_TYPES; ++i)
    {
        STlib_InitNum(&hud->wAmmo[i], ammoPos[i].x, ammoPos[i].y, shortNum,
                      &plr->ammo[i].owned, ST_AMMOWIDTH, 1);

        STlib_InitNum(&hud->wMaxAmmo[i], ammoMaxPos[i].x, ammoMaxPos[i].y,
                      shortNum, &plr->ammo[i].max,
                      ST_MAXAMMOWIDTH, 1);
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
    R_SetViewSize(cfg.screenBlocks);
    ST_HUDUnHide(CONSOLEPLAYER, HUE_FORCE); // So the user can see the change.
    return true;
}
