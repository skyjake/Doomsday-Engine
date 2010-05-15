/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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
#include <assert.h>

#include "jdoom.h"

#include "d_net.h"
#include "st_lib.h"
#include "hu_stuff.h"
#include "hu_lib.h"
#include "p_tick.h" // for P_IsPaused
#include "p_player.h"
#include "am_map.h"
#include "p_user.h"
#include "hu_log.h"

// MACROS ------------------------------------------------------------------

// Radiation suit, green shift.
#define RADIATIONPAL        (13)

// N/256*100% probability
//  that the normal face state will change
#define ST_FACEPROBABILITY  (96)

// Location of status bar
#define ST_X                (0)

#define ST_FX               (144)

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
#define ST_FACESY           (0)

#define ST_EVILGRINCOUNT    (2*TICRATE)
#define ST_STRAIGHTFACECOUNT (TICRATE/2)
#define ST_TURNCOUNT        (1*TICRATE)
#define ST_OUCHCOUNT        (1*TICRATE)
#define ST_RAMPAGEDELAY     (2*TICRATE)

#define ST_MUCHPAIN         (20)

// HEALTH number pos.
#define ST_HEALTHWIDTH      (3)
#define ST_HEALTHX          (90)
#define ST_HEALTHY          (3)

// Weapon pos.
#define ST_ARMSX            (111)
#define ST_ARMSY            (4)
#define ST_ARMSBGX          (104)
#define ST_ARMSBGY          (1)
#define ST_ARMSXSPACE       (12)
#define ST_ARMSYSPACE       (10)

// Frags pos.
#define ST_FRAGSX           (138)
#define ST_FRAGSY           (3)
#define ST_FRAGSWIDTH       (2)

// ARMOR number pos.
#define ST_ARMORWIDTH       (3)
#define ST_ARMORX           (221)
#define ST_ARMORY           (3)

// Key icon positions.
#define ST_KEY0WIDTH        (8)
#define ST_KEY0HEIGHT       (5)
#define ST_KEY0X            (239)
#define ST_KEY0Y            (3)
#define ST_KEY1WIDTH        (ST_KEY0WIDTH)
#define ST_KEY1X            (239)
#define ST_KEY1Y            (13)
#define ST_KEY2WIDTH        (ST_KEY0WIDTH)
#define ST_KEY2X            (239)
#define ST_KEY2Y            (23)

// Ready Ammunition counter.
#define ST_READYAMMOWIDTH   (3)
#define ST_READYAMMOX       (44)
#define ST_READYAMMOY       (3)

// Ammo counters.
#define ST_AMMOWIDTH        (3)
#define ST_AMMOHEIGHT       (6)
#define ST_AMMOX            (288)
#define ST_AMMOY            (5)

#define ST_MAXAMMOWIDTH     (3)
#define ST_MAXAMMOHEIGHT    (6)
#define ST_MAXAMMOX         (314)
#define ST_MAXAMMOY         (5)

// Counter Cheat flags.
#define CCH_KILLS               0x1
#define CCH_ITEMS               0x2
#define CCH_SECRET              0x4
#define CCH_KILLS_PRCNT         0x8
#define CCH_ITEMS_PRCNT         0x10
#define CCH_SECRET_PRCNT        0x20

// TYPES -------------------------------------------------------------------

typedef enum {
    HOT_TLEFT,
    HOT_TRIGHT,
    HOT_BRIGHT,
    HOT_BLEFT,
    HOT_B,
    HOT_LEFT
} hotloc_t;

enum {
    UWG_STATUSBAR = 0,
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
    float           alpha; // Fullscreen hud alpha value.

    float           showBar; // Slide statusbar amount 1.0 is fully open.

    boolean         statusbarActive; // Whether the statusbar is active.
    int             currentFragsCount; // Number of frags so far in deathmatch.
    int             keyBoxes[3]; // Holds key-type for each key box on bar.

    // For status face:
    int             oldHealth; // Used to use appopriately pained face.
    boolean         oldWeaponsOwned[NUM_WEAPON_TYPES]; // Used for evil grin.
    int             faceCount; // Count until face changes.
    int             faceIndex; // Current face index, used by wFaces.
    int             lastAttackDown;
    int             priority;

    int             widgetGroupNames[NUM_UIWIDGET_GROUPS];

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

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void updateViewWindow(cvar_t* cvar);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static hudstate_t hudStates[MAXPLAYERS];

// Main bar left.
static patchinfo_t statusbar;

// 0-9, tall numbers.
static patchinfo_t tallNum[10];

// Tall % sign.
static patchinfo_t tallPercent;

// 0-9, short, yellow (,different!) numbers.
static patchinfo_t shortNum[10];

// 3 key-cards, 3 skulls.
static patchinfo_t keys[NUM_KEY_TYPES];

// Face status patches.
static patchinfo_t faces[ST_NUMFACES];

// Face background.
static patchinfo_t faceBackground[4];

 // Main bar right.
static patchinfo_t armsBackground;

// Weapon ownership patches.
static patchinfo_t arms[6][2];

// CVARs for the HUD/Statusbar:
cvar_t sthudCVars[] =
{
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
}

void drawStatusBarBackground(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define WIDTH (ST_WIDTH)
#define HEIGHT (ST_HEIGHT)
#define ORIGINX (-WIDTH/2)
#define ORIGINY (-HEIGHT * hud->showBar)

    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    float x = ORIGINX, y = ORIGINY, w = WIDTH, h = HEIGHT;
    float armsBGX = ST_ARMSBGX - armsBackground.offset;
    float cw, cw2, ch;

    if(!hud->statusbarActive)
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;

    DGL_SetPatch(statusbar.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
    DGL_Color4f(1, 1, 1, iconAlpha);

    if(!(iconAlpha < 1))
    {
        // We can draw the full graphic in one go.
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
    else
    {
        // Alpha blended status bar, we'll need to cut it up into smaller bits...
        DGL_Begin(DGL_QUADS);

        // Up to faceback if deathmatch, else ST_ARMS.
        w = !deathmatch ? armsBGX : ST_FX;
        h = HEIGHT;
        cw = w / WIDTH;

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
                int sectionWidth = armsBGX + armsBackground.width;
                x = ORIGINX + sectionWidth;
                y = ORIGINY;
                w = ST_FX - armsBGX - armsBackground.width;
                h = HEIGHT;
                cw = (float)sectionWidth / WIDTH;
                cw2 = (sectionWidth + w) / WIDTH;

                DGL_TexCoord2f(0, cw, 0);
                DGL_Vertex2f(x, y);
                DGL_TexCoord2f(0, cw2, 0);
                DGL_Vertex2f(x + w, y);
                DGL_TexCoord2f(0, cw2, 1);
                DGL_Vertex2f(x + w, y + h);
                DGL_TexCoord2f(0, cw, 1);
                DGL_Vertex2f(x, y + h);
            }

            // Awkward, 2 pixel tall strip above faceback.
            x = ORIGINX + ST_FX;
            y = ORIGINY;
            w = WIDTH - ST_FX - 141 - 2;
            h = HEIGHT - 30;
            cw = (float)ST_FX / WIDTH;
            cw2 = (float)(ST_FX + w) / WIDTH;
            ch = h / HEIGHT;

            DGL_TexCoord2f(0, cw, 0);
            DGL_Vertex2f(x, y);
            DGL_TexCoord2f(0, cw2, 0);
            DGL_Vertex2f(x + w, y);
            DGL_TexCoord2f(0, cw2, ch);
            DGL_Vertex2f(x + w, y + h);
            DGL_TexCoord2f(0, cw, ch);
            DGL_Vertex2f(x, y + h);

            // Awkward, 1 pixel tall strip bellow faceback.
            x = ORIGINX + ST_FX;
            y = ORIGINY + (HEIGHT - 1);
            w = WIDTH - ST_FX - 141 - 2;
            h = HEIGHT - 31;
            cw = (float)ST_FX / WIDTH;
            cw2 = (float)(ST_FX + w) / WIDTH;
            ch = (float)(HEIGHT - 1) / HEIGHT;

            DGL_TexCoord2f(0, cw, ch);
            DGL_Vertex2f(x, y);
            DGL_TexCoord2f(0, cw2, ch);
            DGL_Vertex2f(x + w, y);
            DGL_TexCoord2f(0, cw2, 1);
            DGL_Vertex2f(x + w, y + h);
            DGL_TexCoord2f(0, cw, 1);
            DGL_Vertex2f(x, y + h);

            // After faceback.
            {
            int sectionWidth = ST_FX + (WIDTH - ST_FX - 141 - 2);
            x = ORIGINX + sectionWidth;
            y = ORIGINY;
            w = WIDTH - sectionWidth;
            h = HEIGHT;
            cw = (float)sectionWidth / WIDTH;
            }
        }
        else
        {
            // Including area behind the face status indicator.
            int sectionWidth = armsBGX + armsBackground.width;
            x = ORIGINX + sectionWidth;
            y = ORIGINY;
            w = WIDTH - sectionWidth;
            h = HEIGHT;
            cw = (float)sectionWidth / WIDTH;
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
        DGL_SetPatch(armsBackground.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);

        x = ORIGINX + armsBGX;
        y = ORIGINY + armsBackground.topOffset;
        w = armsBackground.width;
        h = armsBackground.height;

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

    // Faceback?
    if(IS_NETGAME)
    {
        const patchinfo_t* patch = &faceBackground[cfg.playerColor[player%MAXPLAYERS]%4];

        DGL_SetPatch(patch->id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);

        x = ORIGINX + (ST_FX - ST_X);
        y = ORIGINY + (HEIGHT - 30);
        w = WIDTH - ST_FX - 141 - 2;
        h = HEIGHT - 3;
        cw = 1.f / patch->width;
        cw2 = ((float)patch->width - 1) / patch->width;
        ch = ((float)patch->height - 1) / patch->height;

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

    *drawnWidth = WIDTH;
    *drawnHeight = HEIGHT;

#undef WIDTH
#undef HEIGHT
#undef ORIGINX
#undef ORIGINY
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

    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    ammotype_t ammoType;
    boolean found;
    int i;

    // Must redirect the pointer if the ready weapon has changed.
    found = false;
    for(ammoType=0; ammoType < NUM_AMMO_TYPES && !found; ++ammoType)
    {
        if(!weaponInfo[plr->readyWeapon][plr->class].mode[0].ammoType[ammoType])
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

void ST_Ticker(timespan_t ticLength)
{
    static trigger_t fixed = {1.0 / TICSPERSEC};

    boolean runFixedTic = M_RunTrigger(&fixed, ticLength);
    int i;

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
                hud->oldHealth = plr->health;
            }
        }
    }
}

void ST_doPaletteStuff(int player)
{
    int palette = 0, cnt, bzc;
    player_t* plr = &players[player];

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

void drawReadyAmmoWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    float yOffset = ST_HEIGHT*(1-hud->showBar);
    if(!hud->statusbarActive)
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, yOffset, 0);
    STlib_DrawNum(&hud->wReadyWeapon, textAlpha);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, -yOffset, 0);
    *drawnWidth = tallNum[0].width * 3;
    *drawnHeight = tallNum[0].height;
}

void drawOwnedAmmoWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    float yOffset = ST_HEIGHT*(1-hud->showBar);
    int i;
    if(!hud->statusbarActive)
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, yOffset, 0);
    for(i = 0; i < 4; ++i)
    {
        STlib_DrawNum(&hud->wAmmo[i], textAlpha);
    }
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, -yOffset, 0);
    *drawnWidth = shortNum[0].width;
    *drawnHeight = (shortNum[0].height + 10) * 4;
}

void drawMaxAmmoWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    float yOffset = ST_HEIGHT*(1-hud->showBar);
    int i;
    if(!hud->statusbarActive)
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, yOffset, 0);
    for(i = 0; i < 4; ++i)
    {
        STlib_DrawNum(&hud->wMaxAmmo[i], textAlpha);
    }
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, -yOffset, 0);
    *drawnWidth = shortNum[0].width;
    *drawnHeight = (shortNum[0].height + 10) * 4;
}

void drawSBarHealthWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    float yOffset = ST_HEIGHT*(1-hud->showBar);
    if(!hud->statusbarActive)
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, yOffset, 0);
    STlib_DrawPercent(&hud->wHealth, textAlpha);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, -yOffset, 0);
    *drawnWidth = tallNum[0].width * 3;
    *drawnHeight = tallNum[0].height;
}

void drawSBarArmorWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    float yOffset = ST_HEIGHT*(1-hud->showBar);
    if(!hud->statusbarActive)
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, yOffset, 0);
    STlib_DrawPercent(&hud->wArmor, textAlpha);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, -yOffset, 0);
    *drawnWidth = tallNum[0].width * 3;
    *drawnHeight = tallNum[0].height;
}

void drawSBarFragsWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    float yOffset = ST_HEIGHT*(1-hud->showBar);
    if(!hud->statusbarActive || !deathmatch)
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, yOffset, 0);
    STlib_DrawPercent(&hud->wArmor, textAlpha);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, -yOffset, 0);
    *drawnWidth = tallNum[0].width * 3;
    *drawnHeight = tallNum[0].height;
}

void drawSBarFaceWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    float yOffset = ST_HEIGHT*(1-hud->showBar);
    if(!hud->statusbarActive)
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, yOffset, 0);
    STlib_DrawMultiIcon(&hud->wFaces, hud->faceIndex, iconAlpha);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, -yOffset, 0);
    {
    const patchinfo_t* facePatch = &faces[hud->faceIndex%ST_NUMFACES];
    *drawnWidth = facePatch->width;
    *drawnHeight = facePatch->height;
    }
}

void drawSBarKeysWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    int i, numDrawnKeys = 0;
    float yOffset = ST_HEIGHT*(1-hud->showBar);
    if(!hud->statusbarActive)
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;
    *drawnWidth = 0;
    *drawnHeight = 0;
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, yOffset, 0);
    for(i = 0; i < 3; ++i)
    {
        const patchinfo_t* patch;
        if(hud->keyBoxes[i] == -1)
            continue;
        patch = &keys[hud->keyBoxes[i]%3];
        STlib_DrawMultiIcon(&hud->wKeyBoxes[i], hud->keyBoxes[i], iconAlpha);
        if(patch->width > *drawnWidth)
            *drawnWidth = patch->width;
        *drawnHeight += patch->height;
    }
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, -yOffset, 0);
    *drawnHeight += (numDrawnKeys-1) * 10;
}

typedef struct {
    hudstate_t*     hud;
    int             slot;
    float           alpha;
} drawownedweapondisply_params_t;

int drawOwnedWeaponWidget2(weapontype_t type, void* context)
{
    drawownedweapondisply_params_t* params = (drawownedweapondisply_params_t*) context;
    const player_t* plr = &players[params->hud - hudStates];

    if(cfg.fixStatusbarOwnedWeapons)
    {
        if(!plr->weapons[type].owned)
            return 1; // Continue iteration.
    }

    STlib_DrawMultiIcon(&params->hud->wArms[params->slot], plr->weapons[type].owned ? 1 : 0, params->alpha);
    return 0; // Stop iteration.
}

void drawOwnedWeaponWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    float yOffset = ST_HEIGHT*(1-hud->showBar);
    int i;
    if(!hud->statusbarActive || deathmatch)
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, yOffset, 0);
    for(i = 0; i < 6; ++i)
    {
        drawownedweapondisply_params_t params;
        int result;

        params.hud = hud;
        params.slot = i;
        params.alpha = textAlpha;

        result = P_IterateWeaponsInSlot(i+1, true, drawOwnedWeaponWidget2, &params);

        if(cfg.fixStatusbarOwnedWeapons && result)
        {   // No weapon bound to slot is owned by player.
            STlib_DrawMultiIcon(&hud->wArms[i], 0, textAlpha);
        }
    }
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, -yOffset, 0);
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
    spriteinfo_t info;

    if(!(alpha > 0))
        return;

    alpha = MINMAX_OF(0.f, alpha, 1.f);
    R_GetSpriteInfo(sprite, 0, &info);

    switch(hotspot)
    {
    case HOT_BRIGHT:
        y -= info.height * scale;

    case HOT_TRIGHT:
        x -= info.width * scale;
        break;

    case HOT_BLEFT:
        y -= info.height * scale;
        break;
    default:
        break;
    }

    DGL_SetPSprite(info.material);

    DGL_Color4f(1, 1, 1, alpha);
    DGL_Begin(DGL_QUADS);
        DGL_TexCoord2f(0, flip * info.texCoord[0], 0);
        DGL_Vertex2f(x, y);

        DGL_TexCoord2f(0, !flip * info.texCoord[0], 0);
        DGL_Vertex2f(x + info.width * scale, y);

        DGL_TexCoord2f(0, !flip * info.texCoord[0], info.texCoord[1]);
        DGL_Vertex2f(x + info.width * scale, y + info.height * scale);

        DGL_TexCoord2f(0, flip * info.texCoord[0], info.texCoord[1]);
        DGL_Vertex2f(x, y + info.height * scale);
    DGL_End();
}

void drawFragsWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    char buf[20];
    if(hud->statusbarActive || !deathmatch)
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;
    sprintf(buf, "FRAGS:%i", hud->currentFragsCount);
    M_DrawText4(buf, 0, 0, GF_FONTA, false, false, cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    *drawnWidth = M_TextWidth(buf, GF_FONTA);
    *drawnHeight = M_TextHeight(buf, GF_FONTA);
}

void drawHealthWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    char buf[20];
    int w, h;
    if(hud->statusbarActive)
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;
    ST_drawHUDSprite(SPR_STIM, 0, 0, HOT_BLEFT, 1, iconAlpha, false);
    ST_HUDSpriteSize(SPR_STIM, &w, &h);
    sprintf(buf, "%i%%", plr->health);
    M_DrawText4(buf, w + 2, -12, GF_FONTB, false, false, cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    *drawnWidth = w + 2 + M_TextWidth(buf, GF_FONTB);
    *drawnHeight = MAX_OF(h, M_TextHeight(buf, GF_FONTB));
}

void drawAmmoWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    static const int ammoSprite[NUM_AMMO_TYPES] = {
        SPR_AMMO,
        SPR_SBOX,
        SPR_CELL,
        SPR_ROCK
    };
    player_t* plr = &players[player];
    hudstate_t* hud = &hudStates[player];
    ammotype_t ammoType;
    char buf[20];
    float scale;

    if(hud->statusbarActive)
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;
    *drawnWidth = 0;
    *drawnHeight = 0;

    /// \todo Only supports one type of ammo per weapon.
    /// for each type of ammo this weapon takes.
    for(ammoType = 0; ammoType < NUM_AMMO_TYPES; ++ammoType)
    {
        int spr, w, h;

        if(!weaponInfo[plr->readyWeapon][plr->class].mode[0].ammoType[ammoType])
            continue;

        spr = ammoSprite[ammoType];
        scale = (spr == SPR_ROCK? .72f : 1);

        ST_drawHUDSprite(spr, 0, 0, HOT_BLEFT, scale, iconAlpha, false);
        ST_HUDSpriteSize(spr, &w, &h);
        sprintf(buf, "%i", plr->ammo[ammoType].owned);
        M_DrawText4(buf, w+2, -12, GF_FONTB, false, false, cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
        *drawnWidth += w+2+M_TextWidth(buf, GF_FONTB);
        *drawnHeight += MAX_OF(h, M_TextHeight(buf, GF_FONTB));
        break;
    }
}

void drawFaceWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    const patchinfo_t* facePatch = &faces[hud->faceIndex];
    const patchinfo_t* bgPatch = &faceBackground[cfg.playerColor[player]];
    int x = -(bgPatch->width/2);
    if(hud->statusbarActive)
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;
    DGL_Color4f(1, 1, 1, iconAlpha);
    if(IS_NETGAME)
        M_DrawPatch(bgPatch->id, x, -bgPatch->height + 1);
    M_DrawPatch(facePatch->id, x, -bgPatch->height);
    *drawnWidth = bgPatch->width;
    *drawnHeight = bgPatch->height;
}

void drawArmorWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    player_t* plr = &players[player];
    hudstate_t* hud = &hudStates[player];
    int maxArmor, armorOffset, spr, w, h;
    char buf[20];

    if(hud->statusbarActive)
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;
    maxArmor = MAX_OF(armorPoints[0], armorPoints[1]);
    maxArmor = MAX_OF(maxArmor, armorPoints[2]);
    maxArmor = MAX_OF(maxArmor, armorPoints[2]);
    dd_snprintf(buf, 20, "%i%%", maxArmor);
    armorOffset = M_TextWidth(buf, GF_FONTB);

    dd_snprintf(buf, 20, "%i%%", plr->armorPoints);

    M_DrawText4(buf, -M_TextWidth(buf, GF_FONTB), -12, GF_FONTB, false, false, cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    spr = (plr->armorType == 2 ? SPR_ARM2 : SPR_ARM1);
    ST_drawHUDSprite(spr, -(armorOffset+2), 0, HOT_BRIGHT, 1, iconAlpha, false);
    ST_HUDSpriteSize(spr, &w, &h);
    *drawnWidth = armorOffset + w + 2;
    *drawnHeight = MAX_OF(h, M_TextHeight(buf, GF_FONTB));
}

void drawKeysWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    static int keyPairs[3][2] = {
        {KT_REDCARD, KT_REDSKULL},
        {KT_YELLOWCARD, KT_YELLOWSKULL},
        {KT_BLUECARD, KT_BLUESKULL}
    };
    static int keyIcons[NUM_KEY_TYPES] = {
        SPR_BKEY,
        SPR_YKEY,
        SPR_RKEY,
        SPR_BSKU,
        SPR_YSKU,
        SPR_RSKU
    };
    player_t* plr = &players[player];
    hudstate_t* hud = &hudStates[player];
    int i, numDrawnKeys = 0, x = 0;

    if(hud->statusbarActive)
        return;
    if(AM_IsActive(AM_MapForPlayer(player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;

    *drawnWidth = 0;
    *drawnHeight = 0;

    for(i = 0; i < NUM_KEY_TYPES; ++i)
    {
        boolean shown = true;

        if(!plr->keys[i])
            continue;

        if(cfg.hudKeysCombine)
        {
            int j;

            for(j = 0; j < 3; ++j)
                if(keyPairs[j][0] == i && plr->keys[keyPairs[j][0]] && plr->keys[keyPairs[j][1]])
                {
                    shown = false;
                    break;
                }
        }

        if(shown)
        {
            int w, h, spr = keyIcons[i];
            ST_drawHUDSprite(spr, x, 0, HOT_BRIGHT, 1, iconAlpha, false);
            ST_HUDSpriteSize(spr, &w, &h);
            *drawnWidth += w;
            if(h > *drawnHeight)
                *drawnHeight = h;
            x -= w + 2;
            numDrawnKeys++;
        }
    }

    *drawnWidth += (numDrawnKeys-1) * 2;
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
        sprintf(tmp, "%s%i%%%s", (cfg.counterCheat & CCH_KILLS ? "(" : ""),
                totalKills ? plr->killCount * 100 / totalKills : 100,
                (cfg.counterCheat & CCH_KILLS ? ")" : ""));
        strcat(buf, tmp);
    }

    *drawnHeight = M_TextHeight(buf, GF_FONTA);
    *drawnWidth = M_TextWidth(buf, GF_FONTA);
    M_DrawText4(buf, 0, -(*drawnHeight), GF_FONTA, false, false, cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
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
        sprintf(tmp, "%s%i%%%s", (cfg.counterCheat & CCH_ITEMS ? "(" : ""),
                totalItems ? plr->itemCount * 100 / totalItems : 100,
                (cfg.counterCheat & CCH_ITEMS ? ")" : ""));
        strcat(buf, tmp);
    }

    *drawnHeight = M_TextHeight(buf, GF_FONTA);
    *drawnWidth = M_TextWidth(buf, GF_FONTA);
    M_DrawText4(buf, 0, -(*drawnHeight), GF_FONTA, false, false, cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
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
        sprintf(tmp, "%s%i%%%s", (cfg.counterCheat & CCH_SECRET ? "(" : ""),
                totalSecret ? plr->secretCount * 100 / totalSecret : 100,
                (cfg.counterCheat & CCH_SECRET ? ")" : ""));
        strcat(buf, tmp);
    }

    *drawnHeight = M_TextHeight(buf, GF_FONTA);
    *drawnWidth = M_TextWidth(buf, GF_FONTA);
    M_DrawText4(buf, 0, -(*drawnHeight), GF_FONTA, false, false, cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
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
            { UWG_BOTTOMLEFT, UWGF_ALIGN_BOTTOM|UWGF_ALIGN_LEFT|UWGF_LEFT2RIGHT, PADDING },
            { UWG_BOTTOMLEFT2, UWGF_ALIGN_BOTTOM|UWGF_ALIGN_LEFT|UWGF_LEFT2RIGHT, PADDING },
            { UWG_BOTTOMRIGHT, UWGF_ALIGN_BOTTOM|UWGF_ALIGN_RIGHT|UWGF_RIGHT2LEFT, PADDING },
            { UWG_BOTTOM, UWGF_ALIGN_BOTTOM|UWGF_BOTTOM2TOP, PADDING },
            { UWG_TOP, UWGF_ALIGN_TOP|UWGF_ALIGN_LEFT|UWGF_TOP2BOTTOM, PADDING },
            { UWG_COUNTERS, UWGF_ALIGN_LEFT|UWGF_BOTTOM2TOP, PADDING }
        };
        const uiwidgetdef_t widgetDefs[] = {
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawStatusBarBackground, &cfg.statusbarOpacity, &cfg.statusbarOpacity },
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawReadyAmmoWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawSBarHealthWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawOwnedWeaponWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawSBarFragsWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawSBarFaceWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawSBarArmorWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawSBarKeysWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawOwnedAmmoWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
            { UWG_STATUSBAR, -1, &cfg.statusbarScale, 1, drawMaxAmmoWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
            { UWG_BOTTOMLEFT, HUD_HEALTH, &cfg.hudScale, 1, drawHealthWidget, &cfg.hudColor[3], &cfg.hudIconAlpha },
            { UWG_BOTTOMLEFT, HUD_AMMO, &cfg.hudScale, 1, drawAmmoWidget, &cfg.hudColor[3], &cfg.hudIconAlpha },
            { UWG_BOTTOMLEFT2, HUD_FRAGS, &cfg.hudScale, 1, drawFragsWidget, &cfg.hudColor[3], &cfg.hudIconAlpha },
            { UWG_BOTTOMRIGHT, HUD_ARMOR, &cfg.hudScale, 1, drawArmorWidget, &cfg.hudColor[3], &cfg.hudIconAlpha },
            { UWG_BOTTOMRIGHT, HUD_KEYS, &cfg.hudScale, .75f, drawKeysWidget, &cfg.hudColor[3], &cfg.hudIconAlpha },
            { UWG_BOTTOM, HUD_FACE, &cfg.hudScale, .7f, drawFaceWidget, &cfg.hudColor[3], &cfg.hudIconAlpha },
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

    hud->statusbarActive = (fullscreenMode < 2) || (AM_IsActive(AM_MapForPlayer(player)) && (cfg.automapHudDisplay == 0 || cfg.automapHudDisplay == 2));

    // Do palette shifts.
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

        int availHeight, drawnWidth, drawnHeight;

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
        GUI_DrawWidgets(hud->widgetGroupNames[UWG_BOTTOMLEFT], 0, x, y, width, height, alpha, &drawnWidth, &drawnHeight);

        availHeight = height - (drawnHeight > 0 ? drawnHeight + PADDING : 0);
        GUI_DrawWidgets(hud->widgetGroupNames[UWG_BOTTOMLEFT2], 0, x, y, width, availHeight, alpha, &drawnWidth, &drawnHeight);

        GUI_DrawWidgets(hud->widgetGroupNames[UWG_BOTTOM], 0, x, y, width, height, alpha, &drawnWidth, &drawnHeight);
        GUI_DrawWidgets(hud->widgetGroupNames[UWG_BOTTOMRIGHT], 0, x, y, width, height, alpha, &drawnWidth, &drawnHeight);
        GUI_DrawWidgets(hud->widgetGroupNames[UWG_COUNTERS], 0, x, y, width, height, alpha, &drawnWidth, &drawnHeight);

#undef PADDING
        }

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }
}

void ST_loadGraphics(void)
{
    int i, j, faceNum;
    char nameBuf[9];

    // Load the numbers, tall and short.
    for(i = 0; i < 10; ++i)
    {
        sprintf(nameBuf, "STTNUM%d", i);
        R_PrecachePatch(nameBuf, &tallNum[i]);

        sprintf(nameBuf, "STYSNUM%d", i);
        R_PrecachePatch(nameBuf, &shortNum[i]);
    }

    // Key cards:
    for(i = 0; i < NUM_KEY_TYPES; ++i)
    {
        sprintf(nameBuf, "STKEYS%d", i);
        R_PrecachePatch(nameBuf, &keys[i]);
    }

    // Percent.
    R_PrecachePatch("STTPRCNT", &tallPercent);
    // Arms background.
    R_PrecachePatch("STARMS", &armsBackground);

    // Arms ownership widgets:
    for(i = 0; i < 6; ++i)
    {
        sprintf(nameBuf, "STGNUM%d", i + 2);

        // gray #
        R_PrecachePatch(nameBuf, &arms[i][0]);

        // yellow #
        memcpy(&arms[i][1], &shortNum[i + 2], sizeof(patchinfo_t));
    }

    // Face backgrounds for different color players.
    for(i = 0; i < 4; ++i)
    {
        sprintf(nameBuf, "STFB%d", i);
        R_PrecachePatch(nameBuf, &faceBackground[i]);
    }

    // Status bar background bits.
    R_PrecachePatch("STBAR", &statusbar);

    // Face states:
    faceNum = 0;
    for(i = 0; i < ST_NUMPAINFACES; ++i)
    {
        for(j = 0; j < ST_NUMSTRAIGHTFACES; ++j)
        {
            sprintf(nameBuf, "STFST%d%d", i, j);
            R_PrecachePatch(nameBuf, &faces[faceNum++]);
        }
        sprintf(nameBuf, "STFTR%d0", i); // Turn right.
        R_PrecachePatch(nameBuf, &faces[faceNum++]);
        sprintf(nameBuf, "STFTL%d0", i); // Turn left.
        R_PrecachePatch(nameBuf, &faces[faceNum++]);
        sprintf(nameBuf, "STFOUCH%d", i); // Ouch.
        R_PrecachePatch(nameBuf, &faces[faceNum++]);
        sprintf(nameBuf, "STFEVL%d", i); // Evil grin.
        R_PrecachePatch(nameBuf, &faces[faceNum++]);
        sprintf(nameBuf, "STFKILL%d", i); // Pissed off.
        R_PrecachePatch(nameBuf, &faces[faceNum++]);
    }
    R_PrecachePatch("STFGOD0", &faces[faceNum++]);
    R_PrecachePatch("STFDEAD0", &faces[faceNum++]);
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
    hud->faceIndex = 0;
    hud->oldHealth = -1;
    hud->priority = 0;
    hud->lastAttackDown = -1;
    hud->showBar = 1;

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
#define ORIGINX (-ST_WIDTH/2)
#define ORIGINY (-ST_HEIGHT)

    typedef struct {
        float x, y;
    } hudelement_t;

    static int largeAmmo = 1994; // means "n/a"
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

    player_t* plr = &players[player];
    hudstate_t* hud = &hudStates[player];
    int* ptr = &largeAmmo;
    ammotype_t ammoType;
    boolean found;
    int i;

    // Ready weapon ammo:
    //// \todo Only supports one type of ammo per weapon.
    found = false;
    for(ammoType = 0; ammoType < NUM_AMMO_TYPES && !found; ++ammoType)
    {
        if(!weaponInfo[plr->readyWeapon][plr->class].mode[0].ammoType[ammoType])
            continue; // Weapon does not take this ammo.
        ptr = &plr->ammo[ammoType].owned;
        found = true;
    }

    STlib_InitNum(&hud->wReadyWeapon, ORIGINX+ST_READYAMMOX, ORIGINY+ST_READYAMMOY, tallNum, ptr, ST_READYAMMOWIDTH, 1);

    // Health percentage.
    STlib_InitPercent(&hud->wHealth, ORIGINX+ST_HEALTHX, ORIGINY+ST_HEALTHY, tallNum, &plr->health, &tallPercent, 1);

    // Weapons owned.
    for(i = 0; i < 6; ++i)
    {
        STlib_InitMultiIcon(&hud->wArms[i], ORIGINX+ST_ARMSX + (i % 3) * ST_ARMSXSPACE, ORIGINY+ST_ARMSY + (i / 3) * ST_ARMSYSPACE, arms[i], 1);
    }

    // Frags sum.
    STlib_InitNum(&hud->wFrags, ORIGINX+ST_FRAGSX, ORIGINY+ST_FRAGSY, tallNum, &hud->currentFragsCount, ST_FRAGSWIDTH, 1);

    // Faces.
    STlib_InitMultiIcon(&hud->wFaces, ORIGINX+ST_FACESX, ORIGINY+ST_FACESY, faces, 1);

    // Armor percentage - should be colored later.
    STlib_InitPercent(&hud->wArmor, ORIGINX+ST_ARMORX, ORIGINY+ST_ARMORY, tallNum, &plr->armorPoints, &tallPercent, 1);

    // Keyboxes 0-2.
    STlib_InitMultiIcon(&hud->wKeyBoxes[0], ORIGINX+ST_KEY0X, ORIGINY+ST_KEY0Y, keys, 1);

    STlib_InitMultiIcon(&hud->wKeyBoxes[1], ORIGINX+ST_KEY1X, ORIGINY+ST_KEY1Y, keys, 1);

    STlib_InitMultiIcon(&hud->wKeyBoxes[2], ORIGINX+ST_KEY2X, ORIGINY+ST_KEY2Y, keys, 1);

    // Ammo count and max (all four kinds).
    for(i = 0; i < NUM_AMMO_TYPES; ++i)
    {
        STlib_InitNum(&hud->wAmmo[i], ORIGINX+ammoPos[i].x, ORIGINY+ammoPos[i].y, shortNum, &plr->ammo[i].owned, ST_AMMOWIDTH, 1);
        STlib_InitNum(&hud->wMaxAmmo[i], ORIGINX+ammoMaxPos[i].x, ORIGINY+ammoMaxPos[i].y, shortNum, &plr->ammo[i].max, ST_MAXAMMOWIDTH, 1);
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
 * Called when the statusbar scale cvar changes.
 */
static void updateViewWindow(cvar_t* cvar)
{
    R_UpdateViewWindow(true);
    ST_HUDUnHide(CONSOLEPLAYER, HUE_FORCE); // So the user can see the change.
}
