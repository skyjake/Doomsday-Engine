/**\file st_stuff.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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
 * Status bar code - jDoom specific.
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
#include "hu_stuff.h"
#include "hu_lib.h"
#include "p_tick.h" // for P_IsPaused
#include "p_player.h"
#include "am_map.h"
#include "p_user.h"
#include "hu_chat.h"
#include "hu_log.h"
#include "r_common.h"

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

#define ST_FACESTRIDE       (ST_NUMSTRAIGHTFACES+ST_NUMTURNFACES+ST_NUMSPECIALFACES)

#define ST_NUMEXTRAFACES    (2)

#define ST_NUMFACES         (ST_FACESTRIDE*ST_NUMPAINFACES+ST_NUMEXTRAFACES)

#define ST_TURNOFFSET       (ST_NUMSTRAIGHTFACES)
#define ST_OUCHOFFSET       (ST_TURNOFFSET + ST_NUMTURNFACES)
#define ST_EVILGRINOFFSET   (ST_OUCHOFFSET + 1)
#define ST_RAMPAGEOFFSET    (ST_EVILGRINOFFSET + 1)
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
    boolean inited;
    boolean stopped;
    int hideTics;
    float hideAmount;
    float alpha; // Fullscreen hud alpha value.
    float showBar; // Slide statusbar amount 1.0 is fully open.
    boolean statusbarActive; // Whether the statusbar is active.

    int widgetGroupIds[NUM_UIWIDGET_GROUPS];

    // Statusbar:
    guidata_health_t sbarHealth;
    guidata_readyammo_t sbarReadyammo;
    guidata_ammo_t sbarAmmos[NUM_AMMO_TYPES];
    guidata_ammo_t sbarMaxammos[NUM_AMMO_TYPES];
    guidata_weaponslot_t sbarWeaponslots[6];
    guidata_armor_t sbarArmor;
    guidata_frags_t sbarFrags;
    guidata_keyslot_t sbarKeyslots[3];
    guidata_face_t sbarFace;

    // Fullscreen:
    guidata_health_t health;
    guidata_armoricon_t armoricon;
    guidata_keys_t keys;
    guidata_armor_t armor;
    guidata_readyammoicon_t readyammoicon;
    guidata_readyammo_t readyammo;
    guidata_face_t face;
    guidata_frags_t frags;

    // Other:
    guidata_secrets_t secrets;
    guidata_items_t items;
    guidata_kills_t kills;
} hudstate_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void updateViewWindow(void);
void unhideHUD(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static hudstate_t hudStates[MAXPLAYERS];

// Main bar left.
static patchinfo_t pStatusbar;

// 3 key-cards, 3 skulls.
static patchinfo_t pKeys[NUM_KEY_TYPES];

// Face status patches.
static patchinfo_t pFaces[ST_NUMFACES];

// Face background.
static patchinfo_t pFaceBackground[4];

 // Main bar right.
static patchinfo_t pArmsBackground;

// Weapon ownership patches.
static patchinfo_t pArms[6][2];

// CVARs for the HUD/Statusbar:
cvartemplate_t sthudCVars[] =
{
    // HUD scale
    {"hud-scale", 0, CVT_FLOAT, &cfg.hudScale, 0.1f, 1, unhideHUD},
    {"hud-wideoffset", 0, CVT_FLOAT, &cfg.hudWideOffset, 0, 1, unhideHUD},

    {"hud-status-size", 0, CVT_FLOAT, &cfg.statusbarScale, 0.1f, 1, updateViewWindow},

    // HUD colour + alpha
    {"hud-color-r", 0, CVT_FLOAT, &cfg.hudColor[0], 0, 1, unhideHUD},
    {"hud-color-g", 0, CVT_FLOAT, &cfg.hudColor[1], 0, 1, unhideHUD},
    {"hud-color-b", 0, CVT_FLOAT, &cfg.hudColor[2], 0, 1, unhideHUD},
    {"hud-color-a", 0, CVT_FLOAT, &cfg.hudColor[3], 0, 1, unhideHUD},
    {"hud-icon-alpha", 0, CVT_FLOAT, &cfg.hudIconAlpha, 0, 1, unhideHUD},

    {"hud-face-ouchfix", 0, CVT_BYTE, &cfg.fixOuchFace, 0, 1},

    {"hud-status-alpha", 0, CVT_FLOAT, &cfg.statusbarOpacity, 0, 1, unhideHUD},
    {"hud-status-icon-a", 0, CVT_FLOAT, &cfg.statusbarCounterAlpha, 0, 1, unhideHUD},
    {"hud-status-weaponslots-ownedfix", 0, CVT_BYTE, &cfg.fixStatusbarOwnedWeapons, 0, 1, unhideHUD},

    // HUD icons
    {"hud-face", 0, CVT_BYTE, &cfg.hudShown[HUD_FACE], 0, 1, unhideHUD},
    {"hud-health", 0, CVT_BYTE, &cfg.hudShown[HUD_HEALTH], 0, 1, unhideHUD},
    {"hud-armor", 0, CVT_BYTE, &cfg.hudShown[HUD_ARMOR], 0, 1, unhideHUD},
    {"hud-ammo", 0, CVT_BYTE, &cfg.hudShown[HUD_AMMO], 0, 1, unhideHUD},
    {"hud-keys", 0, CVT_BYTE, &cfg.hudShown[HUD_KEYS], 0, 1, unhideHUD},
    {"hud-keys-combine", 0, CVT_BYTE, &cfg.hudKeysCombine, 0, 1, unhideHUD},

    // HUD displays
    {"hud-frags", 0, CVT_BYTE, &cfg.hudShown[HUD_FRAGS], 0, 1, unhideHUD},

    {"hud-timer", 0, CVT_FLOAT, &cfg.hudTimer, 0, 60},

    {"hud-unhide-damage", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_DAMAGE], 0, 1},
    {"hud-unhide-pickup-health", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_HEALTH], 0, 1},
    {"hud-unhide-pickup-armor", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_ARMOR], 0, 1},
    {"hud-unhide-pickup-powerup", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_POWER], 0, 1},
    {"hud-unhide-pickup-weapon", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_WEAPON], 0, 1},
    {"hud-unhide-pickup-ammo", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_AMMO], 0, 1},
    {"hud-unhide-pickup-key", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_KEY], 0, 1},

    {"hud-cheat-counter", 0, CVT_BYTE, &cfg.hudShownCheatCounters, 0, 63, unhideHUD},
    {"hud-cheat-counter-scale", 0, CVT_FLOAT, &cfg.hudCheatCounterScale, .1f, 1, unhideHUD},
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

static int fullscreenMode(void)
{
    return (cfg.screenBlocks < 10? 0 : cfg.screenBlocks - 10);
}

void SBarBackground_Drawer(uiwidget_t* obj, int xOffset, int yOffset)
{
#define WIDTH       (ST_WIDTH)
#define HEIGHT      (ST_HEIGHT)
#define ORIGINX     ((int)(-WIDTH/2))
#define ORIGINY     ((int)(-HEIGHT * hud->showBar))

    assert(NULL != obj);
    {
    const hudstate_t* hud = &hudStates[obj->player];
    float x = ORIGINX, y = ORIGINY, w = WIDTH, h = HEIGHT;
    float armsBGX = ST_ARMSBGX + pArmsBackground.offset;
    int fullscreen = fullscreenMode();
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarOpacity);
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarOpacity);
    float cw, cw2, ch;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(xOffset, yOffset, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);

    DGL_SetPatch(pStatusbar.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
    DGL_Enable(DGL_TEXTURE_2D);
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
            if(armsBGX + pArmsBackground.width < ST_FX)
            {
                int sectionWidth = armsBGX + pArmsBackground.width;
                x = ORIGINX + sectionWidth;
                y = ORIGINY;
                w = ST_FX - armsBGX - pArmsBackground.width;
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
            int sectionWidth = armsBGX + pArmsBackground.width;
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
        DGL_SetPatch(pArmsBackground.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);

        x = ORIGINX + armsBGX;
        y = ORIGINY + pArmsBackground.topOffset;
        w = pArmsBackground.width;
        h = pArmsBackground.height;

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
        const patchinfo_t* patch = &pFaceBackground[cfg.playerColor[obj->player%MAXPLAYERS]%4];

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

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef WIDTH
#undef HEIGHT
#undef ORIGINX
#undef ORIGINY
}

void SBarBackground_Dimensions(uiwidget_t* obj, int* drawnWidth, int* drawnHeight)
{
#define WIDTH       (ST_WIDTH)
#define HEIGHT      (ST_HEIGHT)

    assert(NULL != obj);

    if(NULL != drawnWidth)  *drawnWidth  = 0;
    if(NULL != drawnHeight) *drawnHeight = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    if(NULL != drawnWidth)  *drawnWidth  = WIDTH  * cfg.statusbarScale;
    if(NULL != drawnHeight) *drawnHeight = HEIGHT * cfg.statusbarScale;

#undef WIDTH
#undef HEIGHT
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

static int calcPainOffset(int player)
{
    player_t* plr = &players[player];
    int health = (plr->health > 100 ? 100 : plr->health);
    return ST_FACESTRIDE * (((100 - health) * ST_NUMPAINFACES) / 101);
}

/**
 * This is a not-very-pretty routine which handles the face states
 * and their timing. the precedence of expressions is:
 *
 * dead > evil grin > turned head > straight ahead
 */
void Face_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_face_t* face = (guidata_face_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    angle_t badGuyAngle, diffAng;
    boolean doEvilGrin;
    int i;

    if(face->priority < 10)
    {   // Player is dead.
        if(!plr->health)
        {
            face->priority = 9;
            face->faceIndex = ST_DEADFACE;
            face->faceCount = 1;
        }
    }

    if(face->priority < 9)
    {
        if(plr->bonusCount)
        {   // Picking up a bonus.
            doEvilGrin = false;

            for(i = 0; i < NUM_WEAPON_TYPES; ++i)
            {
                if(face->oldWeaponsOwned[i] != plr->weapons[i].owned)
                {
                    doEvilGrin = true;
                    face->oldWeaponsOwned[i] = plr->weapons[i].owned;
                }
            }

            if(doEvilGrin)
            {   // Evil grin if just picked up weapon.
                face->priority = 8;
                face->faceCount = ST_EVILGRINCOUNT;
                face->faceIndex = calcPainOffset(obj->player) + ST_EVILGRINOFFSET;
            }
        }
    }

    if(face->priority < 8)
    {
        if(plr->damageCount && plr->attacker &&
           plr->attacker != plr->plr->mo)
        {   // Being attacked.
            face->priority = 7;

            // DOOM BUG
            // This test was inversed, thereby the OUCH face was NEVER used
            // in normal gameplay as it requires the player recieving damage
            // to end up with MORE health than he started with.
            // Also, priority was not changed which would have resulted in a
            // frame duration of only 1 tic.
            // if(plr->health - oldHealth > ST_MUCHPAIN)

            if((cfg.fixOuchFace?
               (face->oldHealth - plr->health) :
               (plr->health - face->oldHealth)) > ST_MUCHPAIN)
            {
                face->faceCount = ST_TURNCOUNT;
                face->faceIndex = calcPainOffset(obj->player) + ST_OUCHOFFSET;
                if(cfg.fixOuchFace)
                    face->priority = 8; // Added to fix 1 tic issue.
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

                face->faceCount = ST_TURNCOUNT;
                face->faceIndex = calcPainOffset(obj->player);

                if(diffAng < ANG45)
                {   // Head-on.
                    face->faceIndex += ST_RAMPAGEOFFSET;
                }
                else if(i)
                {   // Turn face right.
                    face->faceIndex += ST_TURNOFFSET;
                }
                else
                {   // Turn face left.
                    face->faceIndex += ST_TURNOFFSET + 1;
                }
            }
        }
    }

    if(face->priority < 7)
    {   // Getting hurt because of your own damn stupidity.
        if(plr->damageCount)
        {
            // DOOM BUG
            // This test was inversed, thereby the OUCH face was NEVER used
            // in normal gameplay as it requires the player recieving damage
            // to end up with MORE health than he started with.
            // if(plr->health - oldHealth > ST_MUCHPAIN)

            if((cfg.fixOuchFace?
               (face->oldHealth - plr->health) :
               (plr->health - face->oldHealth)) > ST_MUCHPAIN)
            {
                face->priority = 7;
                face->faceCount = ST_TURNCOUNT;
                face->faceIndex = calcPainOffset(obj->player) + ST_OUCHOFFSET;
            }
            else
            {
                face->priority = 6;
                face->faceCount = ST_TURNCOUNT;
                face->faceIndex = calcPainOffset(obj->player) + ST_RAMPAGEOFFSET;
            }
        }
    }

    if(face->priority < 6)
    {   // Rapid firing.
        if(plr->attackDown)
        {
            if(face->lastAttackDown == -1)
            {
                face->lastAttackDown = ST_RAMPAGEDELAY;
            }
            else if(!--face->lastAttackDown)
            {
                face->priority = 5;
                face->faceIndex = calcPainOffset(obj->player) + ST_RAMPAGEOFFSET;
                face->faceCount = 1;
                face->lastAttackDown = 1;
            }
        }
        else
        {
            face->lastAttackDown = -1;
        }
    }

    if(face->priority < 5)
    {   // Invulnerability.
        if((P_GetPlayerCheats(plr) & CF_GODMODE) || plr->powers[PT_INVULNERABILITY])
        {
            face->priority = 4;

            face->faceIndex = ST_GODFACE;
            face->faceCount = 1;
        }
    }

    // Look left or look right if the facecount has timed out.
    if(!face->faceCount)
    {
        face->faceIndex = calcPainOffset(obj->player) + (M_Random() % 3);
        face->faceCount = ST_STRAIGHTFACECOUNT;
        face->priority = 0;
    }
    face->oldHealth = plr->health;

    --face->faceCount;
    }
}

void ST_updateWidgets(int player)
{
    hudstate_t* hud = &hudStates[player];
    if(hud->inited)
    {
        int i;
        for(i = 0; i < NUM_UIWIDGET_GROUPS; ++i)
        {
            GUI_TickWidget(GUI_MustFindObjectById(hud->widgetGroupIds[i]));
        }
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

void ReadyAmmo_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_readyammo_t* ammo = (guidata_readyammo_t*)obj->typedata;
    int player = obj->player;
    player_t* plr = &players[player];
    ammotype_t ammoType;
    boolean found;

    // Must redirect the pointer if the ready weapon has changed.
    found = false;
    for(ammoType=0; ammoType < NUM_AMMO_TYPES && !found; ++ammoType)
    {
        if(!weaponInfo[plr->readyWeapon][plr->class].mode[0].ammoType[ammoType])
            continue; // Weapon does not use this type of ammo.
        ammo->value = plr->ammo[ammoType].owned;
        found = true;
    }
    if(!found) // Weapon takes no ammo at all.
    {
        ammo->value = 1994; // Means "n/a".
    }
    }
}

void SBarReadyAmmo_Drawer(uiwidget_t* obj, int x, int y)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX+ST_READYAMMOX)
#define Y                   (ORIGINY+ST_READYAMMOY)

    assert(NULL != obj);
    {
    const guidata_readyammo_t* ammo = (guidata_readyammo_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = fullscreenMode();
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    char buf[20];

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(ammo->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", ammo->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(obj->fontId));
    DGL_Color4f(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);

    FR_DrawTextFragment2(buf, X, Y, DTF_ALIGN_TOPRIGHT|DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarReadyAmmo_Dimensions(uiwidget_t* obj, int* drawnWidth, int* drawnHeight)
{
    assert(NULL != obj && NULL != drawnWidth && NULL != drawnHeight);
    {
    const guidata_readyammo_t* ammo = (guidata_readyammo_t*)obj->typedata;
    char buf[20];

    if(NULL != drawnWidth)  *drawnWidth  = 0;
    if(NULL != drawnHeight) *drawnHeight = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(ammo->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", ammo->value);
    FR_SetFont(FID(obj->fontId));
    if(NULL != drawnWidth)  *drawnWidth  = FR_TextFragmentWidth(buf)  * cfg.statusbarScale;
    if(NULL != drawnHeight) *drawnHeight = FR_TextFragmentHeight(buf) * cfg.statusbarScale;
    }
}

void Ammo_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_ammo_t* ammo = (guidata_ammo_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    ammo->value = plr->ammo[ammo->ammotype].owned;
    }
}

void Ammo_Drawer(uiwidget_t* obj, int x, int y)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define MAXDIGITS           (ST_AMMOWIDTH)

    typedef struct {
        float x, y;
    } loc_t;

    assert(NULL != obj);
    {
    static const loc_t ammoPos[NUM_AMMO_TYPES] = {
        {ST_AMMOX, ST_AMMOY},
        {ST_AMMOX, ST_AMMOY + (ST_AMMOHEIGHT * 1)},
        {ST_AMMOX, ST_AMMOY + (ST_AMMOHEIGHT * 3)},
        {ST_AMMOX, ST_AMMOY + (ST_AMMOHEIGHT * 2)}
    };
    const guidata_ammo_t* ammo = (guidata_ammo_t*)obj->typedata;
    const loc_t* loc = &ammoPos[ammo->ammotype];
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = fullscreenMode();
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    char buf[20];

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(ammo->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", ammo->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(obj->fontId));
    DGL_Color4f(defFontRGB3[CR], defFontRGB3[CG], defFontRGB3[CB], textAlpha);
    FR_DrawTextFragment2(buf, ORIGINX+loc->x, ORIGINY+loc->y, DTF_ALIGN_TOPRIGHT|DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef MAXDIGITS
#undef ORIGINY
#undef ORIGINX
}

void Ammo_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    FR_SetFont(FID(obj->fontId));
    if(NULL != width)  *width  = (FR_CharWidth('0') * 3) * cfg.statusbarScale;
    if(NULL != height) *height = FR_CharHeight('0') * cfg.statusbarScale;
}

void MaxAmmo_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_ammo_t* ammo = (guidata_ammo_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    ammo->value = plr->ammo[ammo->ammotype].max;
    }
}

void MaxAmmo_Drawer(uiwidget_t* obj, int x, int y)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define MAXDIGITS           (ST_MAXAMMOWIDTH)

    typedef struct {
        float x, y;
    } loc_t;

    assert(NULL != obj);
    {
    static const loc_t ammoMaxPos[NUM_AMMO_TYPES] = {
        {ST_MAXAMMOX, ST_MAXAMMOY},
        {ST_MAXAMMOX, ST_MAXAMMOY + (ST_AMMOHEIGHT * 1)},
        {ST_MAXAMMOX, ST_MAXAMMOY + (ST_AMMOHEIGHT * 3)},
        {ST_MAXAMMOX, ST_MAXAMMOY + (ST_AMMOHEIGHT * 2)}
    };
    const guidata_ammo_t* ammo = (guidata_ammo_t*)obj->typedata;
    const loc_t* loc = &ammoMaxPos[ammo->ammotype];
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = fullscreenMode();
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    char buf[20];

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(ammo->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", ammo->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(obj->fontId));
    DGL_Color4f(defFontRGB3[CR], defFontRGB3[CG], defFontRGB3[CB], textAlpha);
    FR_DrawTextFragment2(buf, ORIGINX+loc->x, ORIGINY+loc->y, DTF_ALIGN_TOPRIGHT|DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef MAXDIGITS
#undef ORIGINY
#undef ORIGINX
}

void MaxAmmo_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    FR_SetFont(FID(obj->fontId));
    if(NULL != width)  *width  = (FR_CharWidth('0') * 3) * cfg.statusbarScale;
    if(NULL != height) *height = FR_CharHeight('0') * cfg.statusbarScale;
}

void SBarHealth_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_health_t* hlth = (guidata_health_t*) obj->typedata;
    const player_t* plr = &players[obj->player];
    hlth->value = plr->health;
    }
}

void SBarHealth_Drawer(uiwidget_t* obj, int x, int y)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX+ST_HEALTHX)
#define Y                   (ORIGINY+ST_HEALTHY)

    assert(NULL != obj);
    {
    const guidata_health_t* hlth = (guidata_health_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = fullscreenMode();
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    char buf[20];

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(hlth->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", hlth->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(obj->fontId));
    DGL_Color4f(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);

    FR_DrawTextFragment2(buf, X, Y, DTF_ALIGN_TOPRIGHT|DTF_NO_EFFECTS);
    FR_DrawChar('%', X, Y);

    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarHealth_Dimensions(uiwidget_t* obj, int* drawnWidth, int* drawnHeight)
{
    assert(NULL != obj);
    {
    const guidata_health_t* hlth = (guidata_health_t*)obj->typedata;
    char buf[20];

    if(NULL != drawnWidth)  *drawnWidth  = 0;    
    if(NULL != drawnHeight) *drawnHeight = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(hlth->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", hlth->value);
    FR_SetFont(FID(obj->fontId));
    if(NULL != drawnWidth)  *drawnWidth  = (FR_TextFragmentWidth(buf) + FR_CharWidth('%')) * cfg.statusbarScale;
    if(NULL != drawnHeight) *drawnHeight = MAX_OF(FR_TextFragmentHeight(buf), FR_CharHeight('%')) * cfg.statusbarScale;
    }
}

void Armor_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_armor_t* armor = (guidata_armor_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    armor->value = plr->armorPoints;
    }
}

void SBarArmor_Drawer(uiwidget_t* obj, int x, int y)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX+ST_ARMORX)
#define Y                   (ORIGINY+ST_ARMORY)
#define MAXDIGITS           (ST_ARMORWIDTH)

    assert(NULL != obj);
    {
    const guidata_armor_t* armor = (guidata_armor_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = fullscreenMode();
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    char buf[20];

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(armor->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", armor->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(obj->fontId));
    DGL_Color4f(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);

    FR_DrawTextFragment2(buf, X, Y, DTF_ALIGN_TOPRIGHT|DTF_NO_EFFECTS);
    FR_DrawChar('%', X, Y);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef MAXDIGITS
#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarArmor_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    const guidata_armor_t* armor = (guidata_armor_t*)obj->typedata;
    char buf[20];

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(armor->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", armor->value);
    FR_SetFont(FID(obj->fontId));
    if(NULL != width)  *width  = (FR_TextFragmentWidth(buf) + FR_CharWidth('%')) * cfg.statusbarScale;
    if(NULL != height) *height = MAX_OF(FR_TextFragmentHeight(buf), FR_CharHeight('%')) * cfg.statusbarScale;
    }
}

void SBarFrags_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_frags_t* frags = (guidata_frags_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    int i;
    frags->value = 0;
    for(i = 0; i < MAXPLAYERS; ++i)
        if(players[i].plr->inGame)
            frags->value += plr->frags[i] * (i != obj->player ? 1 : -1);
    }
}

void SBarFrags_Drawer(uiwidget_t* obj, int x, int y)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX+ST_FRAGSX)
#define Y                   (ORIGINY+ST_FRAGSY)

    assert(NULL != obj);
    {
    const guidata_frags_t* frags = (guidata_frags_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = fullscreenMode();
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    char buf[20];

    if(!deathmatch)
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(frags->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", frags->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(obj->fontId));
    DGL_Color4f(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);

    FR_DrawTextFragment2(buf, X, Y, DTF_ALIGN_TOPRIGHT|DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarFrags_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    const guidata_frags_t* frags = (guidata_frags_t*)obj->typedata;
    char buf[20];

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(!deathmatch)
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(frags->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", frags->value);
    FR_SetFont(FID(obj->fontId));
    if(NULL != width)  *width  = FR_TextFragmentWidth(buf)  * cfg.statusbarScale;
    if(NULL != height) *height = FR_TextFragmentHeight(buf) * cfg.statusbarScale;
    }
}

void SBarFace_Drawer(uiwidget_t* obj, int x, int y)
{
#define ORIGINX (-ST_WIDTH/2)
#define ORIGINY (-ST_HEIGHT)

    assert(NULL != obj);
    {
    const guidata_face_t* face = (guidata_face_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = fullscreenMode();
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    if(face->faceIndex >= 0)
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        DGL_Translatef(x, y, 0);
        DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
        DGL_Translatef(0, yOffset, 0);
        DGL_Enable(DGL_TEXTURE_2D);

        WI_DrawPatch4(pFaces[face->faceIndex].id, ORIGINX+ST_FACESX, ORIGINY+ST_FACESY, NULL, GF_FONTB, false, DPF_ALIGN_TOPLEFT, 1, 1, 1, iconAlpha);

        DGL_Disable(DGL_TEXTURE_2D);
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }
    }
#undef ORIGINY
#undef ORIGINX
}

void SBarFace_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    const guidata_face_t* face = (guidata_face_t*)obj->typedata;
    const patchinfo_t* facePatch;

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    facePatch = &pFaces[face->faceIndex%ST_NUMFACES];
    if(NULL != width)  *width  = facePatch->width  * cfg.statusbarScale;
    if(NULL != height) *height = facePatch->height * cfg.statusbarScale;
    }
}

void KeySlot_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_keyslot_t* kslt = (guidata_keyslot_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    if(plr->keys[kslt->keytypeA] || plr->keys[kslt->keytypeB])
    {
        kslt->patchId = pKeys[plr->keys[kslt->keytypeB]? kslt->keytypeB : kslt->keytypeA].id;
    }
    else
    {
        kslt->patchId = 0;
    }
    if(!cfg.hudKeysCombine && plr->keys[kslt->keytypeA] && plr->keys[kslt->keytypeB])
    {
        kslt->patchId2 = pKeys[kslt->keytypeA].id;
    }
    else
    {
        kslt->patchId2 = 0;
    }
    }
}

void KeySlot_Drawer(uiwidget_t* obj, int x, int y)
{
#define ORIGINX (-ST_WIDTH/2)
#define ORIGINY (-ST_HEIGHT)

    typedef struct {
        int x, y;
    } loc_t;

    assert(NULL != obj);
    {
    static const loc_t elements[] = {
        { ORIGINX+ST_KEY0X, ORIGINY+ST_KEY0Y },
        { ORIGINX+ST_KEY1X, ORIGINY+ST_KEY1Y },
        { ORIGINX+ST_KEY2X, ORIGINY+ST_KEY2Y }
    };
    guidata_keyslot_t* kslt = (guidata_keyslot_t*)obj->typedata;
    const loc_t* loc = &elements[kslt->slot];
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = fullscreenMode();
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    int offset = (kslt->patchId2 != 0? -1 : 0);

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(kslt->patchId == 0 && kslt->patchId2 == 0)
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    WI_DrawPatch4(kslt->patchId, loc->x + offset, loc->y + offset, NULL, GF_FONTB, false, DPF_ALIGN_TOPLEFT, 1, 1, 1, iconAlpha);
    if(kslt->patchId2 != 0)
    {
        WI_DrawPatch4(kslt->patchId2, loc->x - offset, loc->y - offset, NULL, GF_FONTB, false, DPF_ALIGN_TOPLEFT, 1, 1, 1, iconAlpha);
    }

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef ORIGINY
#undef ORIGINX
}

void KeySlot_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_keyslot_t* kslt = (guidata_keyslot_t*)obj->typedata;
    patchinfo_t info;

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(kslt->patchId == 0 && kslt->patchId2 == 0)
        return;
    if(!R_GetPatchInfo(kslt->patchId, &info))
        return;

    if(NULL != width)  *width  = info.width;
    if(NULL != height) *height = info.height;

    if(kslt->patchId2 != 0)
    {
        if(R_GetPatchInfo(kslt->patchId, &info))
        {
            if(NULL != width && info.width > *width)
                *width = info.width;
            if(NULL != height && info.height > *height)
                *height = info.height;
        }
        *width  += 2;
        *height += 2;
    }

    if(NULL != width)  *width  = *width  * cfg.statusbarScale;
    if(NULL != height) *height = *height * cfg.statusbarScale;
    }
}

typedef struct {
    int player;
    int numOwned;
} countownedweaponsinslot_params_t;

int countOwnedWeaponsInSlot(weapontype_t type, void* context)
{
    countownedweaponsinslot_params_t* params = (countownedweaponsinslot_params_t*) context;
    const player_t* plr = &players[params->player];
    if(plr->weapons[type].owned)
        ++params->numOwned;
    return 1; // Continue iteration.
}

void WeaponSlot_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_weaponslot_t* wpn = (guidata_weaponslot_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    boolean used = false;

    if(cfg.fixStatusbarOwnedWeapons)
    {   // Does the player own any weapon bound to this slot?
        countownedweaponsinslot_params_t params;
        params.player = obj->player;
        params.numOwned = 0;
        P_IterateWeaponsInSlot(wpn->slot, false, countOwnedWeaponsInSlot, &params);
        used = params.numOwned > 0;
    }
    else
    {   // Does the player own the originally hardwired weapon to this slot?
        used = plr->weapons[wpn->slot].owned;
    }
    wpn->patchId = pArms[wpn->slot-1][used?1:0].id;
    }
}

void WeaponSlot_Drawer(uiwidget_t* obj, int x, int y)
{
#define ORIGINX (-ST_WIDTH/2)
#define ORIGINY (-ST_HEIGHT)

    typedef struct {
        int x, y;
    } loc_t;

    assert(NULL != obj);
    {
    static const loc_t elements[] = {
        { ORIGINX+ST_ARMSX,                     ORIGINY+ST_ARMSY },
        { ORIGINX+ST_ARMSX + ST_ARMSXSPACE,     ORIGINY+ST_ARMSY },
        { ORIGINX+ST_ARMSX + ST_ARMSXSPACE*2,   ORIGINY+ST_ARMSY },
        { ORIGINX+ST_ARMSX,                     ORIGINY+ST_ARMSY + ST_ARMSYSPACE },
        { ORIGINX+ST_ARMSX + ST_ARMSXSPACE,     ORIGINY+ST_ARMSY + ST_ARMSYSPACE },
        { ORIGINX+ST_ARMSX + ST_ARMSXSPACE*2,   ORIGINY+ST_ARMSY + ST_ARMSYSPACE },
    };
    guidata_weaponslot_t* wpns = (guidata_weaponslot_t*)obj->typedata;
    const loc_t* element = &elements[wpns->slot-1];
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = fullscreenMode();
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);

    if(deathmatch)
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    WI_DrawPatch4(wpns->patchId, element->x, element->y, NULL, 0, false, DPF_ALIGN_TOPLEFT, 1, 1, 1, textAlpha);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef ORIGINY
#undef ORIGINX
}

void WeaponSlot_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_weaponslot_t* wpns = (guidata_weaponslot_t*)obj->typedata;
    patchinfo_t info;

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(deathmatch)
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(!R_GetPatchInfo(wpns->patchId, &info))
        return;

    if(NULL != width)  *width  = info.width  * cfg.statusbarScale;
    if(NULL != height) *height = info.height * cfg.statusbarScale;
    }
}

/*
static boolean pickStatusbarScalingStrategy(int viewportWidth, int viewportHeight)
{
    float a = (float)viewportWidth/viewportHeight;
    float b = (float)SCREENWIDTH/SCREENHEIGHT;

    if(INRANGE_OF(a, b, .001f))
        return true; // The same, so stretch.
    if(Con_GetByte("rend-hud-stretch") || !INRANGE_OF(a, b, .38f))
        return false; // No stretch; translate and scale to fit.
    // Otherwise stretch.
    return true;
}

static void old_drawStatusbar(int player, int x, int y, int viewW, int viewH)
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
*/

void ST_drawHUDSprite(int sprite, float x, float y, hotloc_t hotspot,
    float scale, float alpha, boolean flip, int* drawnWidth, int* drawnHeight)
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
    DGL_Enable(DGL_TEXTURE_2D);

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

    DGL_Disable(DGL_TEXTURE_2D);

    if(drawnWidth)  *drawnWidth  = info.width  * scale;
    if(drawnHeight) *drawnHeight = info.height * scale;
}

void ST_HUDSpriteDimensions(int sprite, float scale, int* width, int* height)
{
    spriteinfo_t info;
    if(NULL == width && NULL == height) return;
    if(!R_GetSpriteInfo(sprite, 0, &info)) return;

    if(NULL != width)  *width  = info.width  * scale;
    if(NULL != height) *height = info.height * scale;
}

void Frags_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_frags_t* frags = (guidata_frags_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    int i;
    frags->value = 0;
    for(i = 0; i < MAXPLAYERS; ++i)
        if(players[i].plr->inGame)
            frags->value += plr->frags[i] * (i != obj->player ? 1 : -1);
    }
}

void Frags_Drawer(uiwidget_t* obj, int x, int y)
{
    assert(NULL != obj);
    {
    const guidata_frags_t* frags = (guidata_frags_t*)obj->typedata;
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    char buf[20];

    if(!deathmatch)
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    sprintf(buf, "FRAGS:%i", frags->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(obj->fontId));
    DGL_Color4f(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    FR_DrawTextFragment2(buf, 0, 0, DTF_ALIGN_TOPLEFT|DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void Frags_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    const guidata_frags_t* frags = (guidata_frags_t*)obj->typedata;
    char buf[20];

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(!deathmatch)
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    sprintf(buf, "FRAGS:%i", frags->value);
    FR_SetFont(FID(obj->fontId));
    if(NULL != width)  *width  = FR_TextFragmentWidth(buf)  * cfg.hudScale;
    if(NULL != height) *height = FR_TextFragmentHeight(buf) * cfg.hudScale;
    }
}

void Health_Drawer(uiwidget_t* obj, int x, int y)
{
    assert(NULL != obj);
    {
    guidata_health_t* hlth = (guidata_health_t*)obj->typedata;
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    char buf[20];

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    sprintf(buf, "%i%%", hlth->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(obj->fontId));
    DGL_Color4f(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    FR_DrawTextFragment2(buf, 0, 0, DTF_ALIGN_BOTTOMLEFT|DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void Health_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_health_t* hlth = (guidata_health_t*)obj->typedata;
    char buf[20];

    if(NULL != width)  *width = 0;
    if(NULL != height) *height = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    sprintf(buf, "%i%%", hlth->value);
    FR_SetFont(FID(obj->fontId));
    if(NULL != width)  *width  = FR_TextFragmentWidth(buf)  * cfg.hudScale;
    if(NULL != height) *height = FR_TextFragmentHeight(buf) * cfg.hudScale;
    }
}

void HealthIcon_Drawer(uiwidget_t* obj, int x, int y)
{
    assert(NULL != obj);
    {
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);

    ST_drawHUDSprite(SPR_STIM, 0, 0, HOT_BLEFT, 1, iconAlpha, false, NULL, NULL);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void HealthIcon_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    int w, h;

    if(NULL != width)  *width = 0;
    if(NULL != height) *height = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    ST_HUDSpriteDimensions(SPR_STIM, 1, &w, &h);
    if(NULL != width)  *width  = w * cfg.hudScale;
    if(NULL != height) *height = h * cfg.hudScale;
    }
}

void ReadyAmmo_Drawer(uiwidget_t* obj, int x, int y)
{
    assert(NULL != obj);
    {
    guidata_readyammo_t* ammo = (guidata_readyammo_t*)obj->typedata;
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    char buf[20];

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(ammo->value == 1994)
        return;

    sprintf(buf, "%i", ammo->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(obj->fontId));
    DGL_Color4f(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    FR_DrawTextFragment2(buf, 0, 0, DTF_ALIGN_BOTTOMLEFT|DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void ReadyAmmo_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_readyammo_t* ammo = (guidata_readyammo_t*)obj->typedata;
    char buf[20];

    if(NULL != width)  *width = 0;
    if(NULL != height) *height = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(ammo->value == 1994)
        return;

    sprintf(buf, "%i", ammo->value);
    FR_SetFont(FID(obj->fontId));
    if(NULL != width)  *width  = FR_TextFragmentWidth(buf)  * cfg.hudScale;
    if(NULL != height) *height = FR_TextFragmentHeight(buf) * cfg.hudScale;
    }
}

void ReadyAmmoIcon_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    static const int ammoSprite[NUM_AMMO_TYPES] = {
        SPR_AMMO,
        SPR_SBOX,
        SPR_CELL,
        SPR_ROCK
    };
    guidata_readyammoicon_t* icon = (guidata_readyammoicon_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    ammotype_t ammoType;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))
        return;

    icon->sprite = -1;
    for(ammoType = 0; ammoType < NUM_AMMO_TYPES; ++ammoType)
    {
        if(!weaponInfo[plr->readyWeapon][plr->class].mode[0].ammoType[ammoType])
            continue;
        icon->sprite = ammoSprite[ammoType];
        break;
    }
    }
}

void ReadyAmmoIcon_Drawer(uiwidget_t* obj, int x, int y)
{
    assert(NULL != obj);
    {
    guidata_readyammoicon_t* icon = (guidata_readyammoicon_t*)obj->typedata;
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;
    float scale;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(icon->sprite < 0)
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);

    scale = (icon->sprite == SPR_ROCK? .72f : 1);
    ST_drawHUDSprite(icon->sprite, 0, 0, HOT_BLEFT, scale, iconAlpha, false, NULL, NULL);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void ReadyAmmoIcon_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_readyammoicon_t* icon = (guidata_readyammoicon_t*)obj->typedata;
    float scale;
    int w, h;

    if(NULL != width)  *width = 0;
    if(NULL != height) *height = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(icon->sprite < 0)
        return;

    scale = (icon->sprite == SPR_ROCK? .72f : 1);
    ST_HUDSpriteDimensions(icon->sprite, scale, &w, &h);
    if(NULL != width)  *width  = w * cfg.hudScale;
    if(NULL != height) *height = h * cfg.hudScale;
    }
}

void Face_Drawer(uiwidget_t* obj, int x, int y)
{
#define EXTRA_SCALE         .7f
    assert(NULL != obj);
    {
    const guidata_face_t* face = (guidata_face_t*)obj->typedata;
    const patchinfo_t* facePatch = &pFaces[face->faceIndex];
    const patchinfo_t* bgPatch = &pFaceBackground[cfg.playerColor[obj->player]];
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(EXTRA_SCALE * cfg.hudScale, EXTRA_SCALE * cfg.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    x = -(bgPatch->width/2);
    DGL_Color4f(1, 1, 1, iconAlpha);
    if(IS_NETGAME)
        GL_DrawPatch(bgPatch->id, x, -bgPatch->height + 1);
    GL_DrawPatch(facePatch->id, x, -bgPatch->height);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef EXTRA_SCALE
}

void Face_Dimensions(uiwidget_t* obj, int* width, int* height)
{
#define EXTRA_SCALE         .7f
    assert(NULL != obj);
    {
    const patchinfo_t* bgPatch = &pFaceBackground[cfg.playerColor[obj->player]];

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    if(NULL != width)  *width  = bgPatch->width  * EXTRA_SCALE * cfg.hudScale;
    if(NULL != height) *height = bgPatch->height * EXTRA_SCALE * cfg.hudScale;
    }
#undef EXTRA_SCALE
}

void Armor_Drawer(uiwidget_t* obj, int x, int y)
{
    assert(NULL != obj);
    {
    guidata_armor_t* armor = (guidata_armor_t*)obj->typedata;
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    char buf[20];

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(armor->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i%%", armor->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(obj->fontId));
    DGL_Color4f(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    FR_DrawTextFragment2(buf, 0, 0, DTF_ALIGN_BOTTOMRIGHT|DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void Armor_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_armor_t* armor = (guidata_armor_t*)obj->typedata;
    char buf[20];

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(armor->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i%%", armor->value);

    FR_SetFont(FID(obj->fontId));
    if(NULL != width)  *width  = FR_TextFragmentWidth(buf)  * cfg.hudScale;
    if(NULL != height) *height = FR_TextFragmentHeight(buf) * cfg.hudScale;
    }
}

void ArmorIcon_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_armoricon_t* icon = (guidata_armoricon_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    icon->sprite = (plr->armorType == 2 ? SPR_ARM2 : SPR_ARM1);
    }
}

void ArmorIcon_Drawer(uiwidget_t* obj, int x, int y)
{
    assert(NULL != obj);
    {
    guidata_armoricon_t* icon = (guidata_armoricon_t*)obj->typedata;
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(icon->sprite < 0)
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);

    ST_drawHUDSprite(icon->sprite, 0, 0, HOT_BRIGHT, 1, iconAlpha, false, NULL, NULL);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void ArmorIcon_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_armoricon_t* icon = (guidata_armoricon_t*)obj->typedata;
    int w, h;

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(icon->sprite < 0)
        return;

    ST_HUDSpriteDimensions(icon->sprite, 1, &w, &h);
    if(NULL != width)  *width  = w * cfg.hudScale;
    if(NULL != height) *height = h * cfg.hudScale;
    }
}

void Keys_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_keys_t* keys = (guidata_keys_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    int i;
    for(i = 0; i < NUM_KEY_TYPES; ++i)
        keys->keyBoxes[i] = plr->keys[i];
    }
}

void Keys_Drawer(uiwidget_t* obj, int x, int y)
{
#define EXTRA_SCALE     .75f
    assert(NULL != obj);
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
    guidata_keys_t* keys = (guidata_keys_t*)obj->typedata;
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;
    int i, numDrawnKeys = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(EXTRA_SCALE * cfg.hudScale, EXTRA_SCALE * cfg.hudScale, 1);

    x = 0;
    for(i = 0; i < NUM_KEY_TYPES; ++i)
    {
        boolean shown = true;

        if(!keys->keyBoxes[i])
            continue;

        if(cfg.hudKeysCombine)
        {
            int j;

            for(j = 0; j < 3; ++j)
                if(keyPairs[j][0] == i && keys->keyBoxes[keyPairs[j][0]] && keys->keyBoxes[keyPairs[j][1]])
                {
                    shown = false;
                    break;
                }
        }

        if(shown)
        {
            int w, h, spr = keyIcons[i];
            ST_drawHUDSprite(spr, x, 0, HOT_BRIGHT, 1, iconAlpha, false, &w, &h);
            x -= w + 2;
            numDrawnKeys++;
        }
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef EXTRA_SCALE
}

void Keys_Dimensions(uiwidget_t* obj, int* width, int* height)
{
#define EXTRA_SCALE     .75f
    assert(NULL != obj);
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
    guidata_keys_t* keys = (guidata_keys_t*)obj->typedata;
    int i, numDrawnKeys = 0;

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    for(i = 0; i < NUM_KEY_TYPES; ++i)
    {
        boolean shown = true;

        if(!keys->keyBoxes[i])
            continue;

        if(cfg.hudKeysCombine)
        {
            int j;

            for(j = 0; j < 3; ++j)
                if(keyPairs[j][0] == i && keys->keyBoxes[keyPairs[j][0]] && keys->keyBoxes[keyPairs[j][1]])
                {
                    shown = false;
                    break;
                }
        }

        if(shown)
        {
            int w, h, spr = keyIcons[i];
            ST_HUDSpriteDimensions(spr, 1, &w, &h);
            if(NULL != width) *width += w;
            if(NULL != height && h > *height)
                *height = h;
            numDrawnKeys++;
        }
    }
    if(NULL != width) *width += (numDrawnKeys-1) * 2;

    if(NULL != width)  *width  = *width  * EXTRA_SCALE * cfg.hudScale;
    if(NULL != height) *height = *height * EXTRA_SCALE * cfg.hudScale;
    }
#undef EXTRA_SCALE
}

void Kills_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_kills_t* kills = (guidata_kills_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    kills->value = plr->killCount;
    }
}

void Kills_Drawer(uiwidget_t* obj, int x, int y)
{
    assert(NULL != obj);
    {
    guidata_kills_t* kills = (guidata_kills_t*)obj->typedata;
    char buf[40], tmp[20];
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];

    if(!(cfg.hudShownCheatCounters & (CCH_KILLS | CCH_KILLS_PRCNT)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(kills->value == 1994)
        return;

    strcpy(buf, "Kills: ");
    if(cfg.hudShownCheatCounters & CCH_KILLS)
    {
        sprintf(tmp, "%i/%i ", kills->value, totalKills);
        strcat(buf, tmp);
    }
    if(cfg.hudShownCheatCounters & CCH_KILLS_PRCNT)
    {
        sprintf(tmp, "%s%i%%%s", (cfg.hudShownCheatCounters & CCH_KILLS ? "(" : ""),
                totalKills ? kills->value * 100 / totalKills : 100,
                (cfg.hudShownCheatCounters & CCH_KILLS ? ")" : ""));
        strcat(buf, tmp);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.hudCheatCounterScale, cfg.hudCheatCounterScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(obj->fontId));
    DGL_Color4f(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    FR_DrawTextFragment2(buf, 0, 0, DTF_ALIGN_BOTTOMLEFT|DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void Kills_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_kills_t* kills = (guidata_kills_t*)obj->typedata;
    char buf[40], tmp[20];

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(!(cfg.hudShownCheatCounters & (CCH_KILLS | CCH_KILLS_PRCNT)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(kills->value == 1994)
        return;

    strcpy(buf, "Kills: ");
    if(cfg.hudShownCheatCounters & CCH_KILLS)
    {
        sprintf(tmp, "%i/%i ", kills->value, totalKills);
        strcat(buf, tmp);
    }
    if(cfg.hudShownCheatCounters & CCH_KILLS_PRCNT)
    {
        sprintf(tmp, "%s%i%%%s", (cfg.hudShownCheatCounters & CCH_KILLS ? "(" : ""),
                totalKills ? kills->value * 100 / totalKills : 100,
                (cfg.hudShownCheatCounters & CCH_KILLS ? ")" : ""));
        strcat(buf, tmp);
    }

    FR_SetFont(FID(obj->fontId));
    if(NULL != width)  *width  = FR_TextFragmentWidth(buf)  * cfg.hudCheatCounterScale;
    if(NULL != height) *height = FR_TextFragmentHeight(buf) * cfg.hudCheatCounterScale;
    }
}

void Items_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_items_t* items = (guidata_items_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    items->value = plr->itemCount;
    }
}

void Items_Drawer(uiwidget_t* obj, int x, int y)
{
    assert(NULL != obj);
    {
    guidata_items_t* items = (guidata_items_t*)obj->typedata;
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    char buf[40], tmp[20];

    if(!(cfg.hudShownCheatCounters & (CCH_ITEMS | CCH_ITEMS_PRCNT)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(items->value == 1994)
        return;

    strcpy(buf, "Items: ");
    if(cfg.hudShownCheatCounters & CCH_ITEMS)
    {
        sprintf(tmp, "%i/%i ", items->value, totalItems);
        strcat(buf, tmp);
    }
    if(cfg.hudShownCheatCounters & CCH_ITEMS_PRCNT)
    {
        sprintf(tmp, "%s%i%%%s", (cfg.hudShownCheatCounters & CCH_ITEMS ? "(" : ""),
                totalItems ? items->value * 100 / totalItems : 100,
                (cfg.hudShownCheatCounters & CCH_ITEMS ? ")" : ""));
        strcat(buf, tmp);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.hudCheatCounterScale, cfg.hudCheatCounterScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(obj->fontId));
    DGL_Color4f(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    FR_DrawTextFragment2(buf, 0, 0, DTF_ALIGN_BOTTOMLEFT|DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void Items_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_items_t* items = (guidata_items_t*)obj->typedata;
    char buf[40], tmp[20];

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(!(cfg.hudShownCheatCounters & (CCH_ITEMS | CCH_ITEMS_PRCNT)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(items->value == 1994)
        return;

    strcpy(buf, "Items: ");
    if(cfg.hudShownCheatCounters & CCH_ITEMS)
    {
        sprintf(tmp, "%i/%i ", items->value, totalItems);
        strcat(buf, tmp);
    }
    if(cfg.hudShownCheatCounters & CCH_ITEMS_PRCNT)
    {
        sprintf(tmp, "%s%i%%%s", (cfg.hudShownCheatCounters & CCH_ITEMS ? "(" : ""),
                totalItems ? items->value * 100 / totalItems : 100,
                (cfg.hudShownCheatCounters & CCH_ITEMS ? ")" : ""));
        strcat(buf, tmp);
    }

    FR_SetFont(FID(obj->fontId));
    if(NULL != width)  *width  = FR_TextFragmentWidth(buf)  * cfg.hudCheatCounterScale;
    if(NULL != height) *height = FR_TextFragmentHeight(buf) * cfg.hudCheatCounterScale;
    }
}

void Secrets_Drawer(uiwidget_t* obj, int x, int y)
{
    assert(NULL != obj);
    {
    guidata_secrets_t* scrt = (guidata_secrets_t*)obj->typedata;
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    char buf[40], tmp[20];

    if(!(cfg.hudShownCheatCounters & (CCH_SECRETS | CCH_SECRETS_PRCNT)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(scrt->value == 1994)
        return;

    strcpy(buf, "Secret: ");
    if(cfg.hudShownCheatCounters & CCH_SECRETS)
    {
        sprintf(tmp, "%i/%i ", scrt->value, totalSecret);
        strcat(buf, tmp);
    }
    if(cfg.hudShownCheatCounters & CCH_SECRETS_PRCNT)
    {
        sprintf(tmp, "%s%i%%%s", (cfg.hudShownCheatCounters & CCH_SECRETS ? "(" : ""),
                totalSecret ? scrt->value * 100 / totalSecret : 100,
                (cfg.hudShownCheatCounters & CCH_SECRETS ? ")" : ""));
        strcat(buf, tmp);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.hudCheatCounterScale, cfg.hudCheatCounterScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(obj->fontId));
    DGL_Color4f(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    FR_DrawTextFragment2(buf, 0, 0, DTF_ALIGN_BOTTOMLEFT|DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void Secrets_Ticker(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_secrets_t* scrt = (guidata_secrets_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    scrt->value = plr->secretCount;
    }
}

void Secrets_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    guidata_secrets_t* scrt = (guidata_secrets_t*)obj->typedata;
    char buf[40], tmp[20];

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(!(cfg.hudShownCheatCounters & (CCH_SECRETS | CCH_SECRETS_PRCNT)))
        return;
    if(AM_IsActive(AM_MapForPlayer(obj->player)) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(scrt->value == 1994)
        return;

    strcpy(buf, "Secret: ");
    if(cfg.hudShownCheatCounters & CCH_SECRETS)
    {
        sprintf(tmp, "%i/%i ", scrt->value, totalSecret);
        strcat(buf, tmp);
    }
    if(cfg.hudShownCheatCounters & CCH_SECRETS_PRCNT)
    {
        sprintf(tmp, "%s%i%%%s", (cfg.hudShownCheatCounters & CCH_SECRETS ? "(" : ""),
                totalSecret ? scrt->value * 100 / totalSecret : 100,
                (cfg.hudShownCheatCounters & CCH_SECRETS ? ")" : ""));
        strcat(buf, tmp);
    }

    FR_SetFont(FID(obj->fontId));
    if(NULL != width)  *width  = FR_TextFragmentWidth(buf)  * cfg.hudCheatCounterScale;
    if(NULL != height) *height = FR_TextFragmentHeight(buf) * cfg.hudCheatCounterScale;
    }
}

void Log_Drawer(uiwidget_t* obj, int x, int y)
{
    assert(NULL != obj);
    {
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;

    /// \kludge Do not draw message logs while the map title is being displayed.
    if(cfg.mapTitle && actualMapTime < 6 * 35)
        return;
    /// kludge end.

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.msgScale, cfg.msgScale, 1);

    Hu_LogDrawer(obj->player, textAlpha);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void Log_Dimensions(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    /// \kludge Do not draw message logs while the map title is being displayed.
    if(cfg.mapTitle && actualMapTime < 6 * 35)
        return;
    /// kludge end.

    Hu_LogDimensions(obj->player, width, height);

    if(NULL != width)  *width  = *width  * cfg.msgScale;
    if(NULL != height) *height = *height * cfg.msgScale;
}

void Chat_Drawer2(uiwidget_t* obj, int x, int y)
{
    assert(NULL != obj);
    {
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.msgScale, cfg.msgScale, 1);

    Chat_Drawer(obj->player, textAlpha, iconAlpha);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void Chat_Dimensions2(uiwidget_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    Chat_Dimensions(obj->player, width, height);

    if(NULL != width)  *width  = *width  * cfg.msgScale;
    if(NULL != height) *height = *height * cfg.msgScale;
}

typedef struct {
    guiwidgettype_t type;
    int group;
    int hideId;
    gamefontid_t fontId;
    void (*dimensions) (uiwidget_t* obj, int* width, int* height);
    void (*drawer) (uiwidget_t* obj, int x, int y);
    void (*ticker) (uiwidget_t* obj);
    void* typedata;
} uiwidgetdef_t;

typedef struct {
    int group;
    short flags;
    int padding; // In fixed 320x200 pixels.
} uiwidgetgroupdef_t;

void ST_Drawer(int player)
{
    hudstate_t* hud;
    player_t* plr;
    int fullscreen = fullscreenMode();
    boolean blended = (fullscreen!=0);

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
            { UWG_STATUSBAR,    UWGF_ALIGN_BOTTOM },
            { UWG_BOTTOMLEFT,   UWGF_ALIGN_BOTTOM|UWGF_ALIGN_LEFT|UWGF_LEFTTORIGHT, PADDING },
            { UWG_BOTTOMLEFT2,  UWGF_ALIGN_BOTTOM|UWGF_ALIGN_LEFT|UWGF_LEFTTORIGHT, PADDING },
            { UWG_BOTTOMRIGHT,  UWGF_ALIGN_BOTTOM|UWGF_ALIGN_RIGHT|UWGF_RIGHTTOLEFT, PADDING },
            { UWG_BOTTOM,       UWGF_ALIGN_BOTTOM|UWGF_VERTICAL|UWGF_RIGHTTOLEFT, PADDING },
            { UWG_TOP,          UWGF_ALIGN_TOP|UWGF_ALIGN_LEFT|UWGF_VERTICAL|UWGF_LEFTTORIGHT, PADDING },
            { UWG_COUNTERS,     UWGF_ALIGN_LEFT|UWGF_VERTICAL|UWGF_RIGHTTOLEFT, PADDING }
        };
        const uiwidgetdef_t widgetDefs[] = {
            { GUI_BOX,      UWG_STATUSBAR,      -1,         0,          SBarBackground_Dimensions, SBarBackground_Drawer },
            { GUI_READYAMMO, UWG_STATUSBAR,     -1,         GF_STATUS,  SBarReadyAmmo_Dimensions, SBarReadyAmmo_Drawer, ReadyAmmo_Ticker, &hud->sbarReadyammo },
            { GUI_HEALTH,   UWG_STATUSBAR,      -1,         GF_STATUS,  SBarHealth_Dimensions, SBarHealth_Drawer, SBarHealth_Ticker, &hud->sbarHealth },
            { GUI_WEAPONSLOT, UWG_STATUSBAR,    -1,         0,          WeaponSlot_Dimensions, WeaponSlot_Drawer, WeaponSlot_Ticker, &hud->sbarWeaponslots[0] },
            { GUI_WEAPONSLOT, UWG_STATUSBAR,    -1,         0,          WeaponSlot_Dimensions, WeaponSlot_Drawer, WeaponSlot_Ticker, &hud->sbarWeaponslots[1] },
            { GUI_WEAPONSLOT, UWG_STATUSBAR,    -1,         0,          WeaponSlot_Dimensions, WeaponSlot_Drawer, WeaponSlot_Ticker, &hud->sbarWeaponslots[2] },
            { GUI_WEAPONSLOT, UWG_STATUSBAR,    -1,         0,          WeaponSlot_Dimensions, WeaponSlot_Drawer, WeaponSlot_Ticker, &hud->sbarWeaponslots[3] },
            { GUI_WEAPONSLOT, UWG_STATUSBAR,    -1,         0,          WeaponSlot_Dimensions, WeaponSlot_Drawer, WeaponSlot_Ticker, &hud->sbarWeaponslots[4] },
            { GUI_WEAPONSLOT, UWG_STATUSBAR,    -1,         0,          WeaponSlot_Dimensions, WeaponSlot_Drawer, WeaponSlot_Ticker, &hud->sbarWeaponslots[5] },
            { GUI_FRAGS,    UWG_STATUSBAR,      -1,         GF_STATUS,  SBarFrags_Dimensions, SBarFrags_Drawer, SBarFrags_Ticker, &hud->sbarFrags },
            { GUI_FACE,     UWG_STATUSBAR,      -1,         0,          SBarFace_Dimensions, SBarFace_Drawer, Face_Ticker, &hud->sbarFace },
            { GUI_ARMOR,    UWG_STATUSBAR,      -1,         GF_STATUS,  SBarArmor_Dimensions, SBarArmor_Drawer, Armor_Ticker, &hud->sbarArmor },
            { GUI_KEYSLOT,  UWG_STATUSBAR,      -1,         0,          KeySlot_Dimensions, KeySlot_Drawer, KeySlot_Ticker, &hud->sbarKeyslots[0] },
            { GUI_KEYSLOT,  UWG_STATUSBAR,      -1,         0,          KeySlot_Dimensions, KeySlot_Drawer, KeySlot_Ticker, &hud->sbarKeyslots[1] },
            { GUI_KEYSLOT,  UWG_STATUSBAR,      -1,         0,          KeySlot_Dimensions, KeySlot_Drawer, KeySlot_Ticker, &hud->sbarKeyslots[2] },
            { GUI_AMMO,     UWG_STATUSBAR,      -1,         GF_INDEX,   Ammo_Dimensions, Ammo_Drawer, Ammo_Ticker, &hud->sbarAmmos[AT_CLIP] },
            { GUI_AMMO,     UWG_STATUSBAR,      -1,         GF_INDEX,   Ammo_Dimensions, Ammo_Drawer, Ammo_Ticker, &hud->sbarAmmos[AT_SHELL] },
            { GUI_AMMO,     UWG_STATUSBAR,      -1,         GF_INDEX,   Ammo_Dimensions, Ammo_Drawer, Ammo_Ticker, &hud->sbarAmmos[AT_CELL] },
            { GUI_AMMO,     UWG_STATUSBAR,      -1,         GF_INDEX,   Ammo_Dimensions, Ammo_Drawer, Ammo_Ticker, &hud->sbarAmmos[AT_MISSILE] },
            { GUI_AMMO,     UWG_STATUSBAR,      -1,         GF_INDEX,   MaxAmmo_Dimensions, MaxAmmo_Drawer, MaxAmmo_Ticker, &hud->sbarMaxammos[AT_CLIP] },
            { GUI_AMMO,     UWG_STATUSBAR,      -1,         GF_INDEX,   MaxAmmo_Dimensions, MaxAmmo_Drawer, MaxAmmo_Ticker, &hud->sbarMaxammos[AT_SHELL] },
            { GUI_AMMO,     UWG_STATUSBAR,      -1,         GF_INDEX,   MaxAmmo_Dimensions, MaxAmmo_Drawer, MaxAmmo_Ticker, &hud->sbarMaxammos[AT_CELL] },
            { GUI_AMMO,     UWG_STATUSBAR,      -1,         GF_INDEX,   MaxAmmo_Dimensions, MaxAmmo_Drawer, MaxAmmo_Ticker, &hud->sbarMaxammos[AT_MISSILE] },
            { GUI_BOX,      UWG_BOTTOMLEFT,     HUD_HEALTH, 0,          HealthIcon_Dimensions, HealthIcon_Drawer },
            { GUI_HEALTH,   UWG_BOTTOMLEFT,     HUD_HEALTH, GF_FONTB,   Health_Dimensions, Health_Drawer, SBarHealth_Ticker, &hud->health },
            { GUI_READYAMMOICON, UWG_BOTTOMLEFT, HUD_AMMO,  0,          ReadyAmmoIcon_Dimensions, ReadyAmmoIcon_Drawer, ReadyAmmoIcon_Ticker, &hud->readyammoicon },
            { GUI_READYAMMO, UWG_BOTTOMLEFT,    HUD_AMMO,   GF_FONTB,   ReadyAmmo_Dimensions, ReadyAmmo_Drawer, ReadyAmmo_Ticker, &hud->readyammo },
            { GUI_FRAGS,    UWG_BOTTOMLEFT2,    HUD_FRAGS,  GF_FONTA,   Frags_Dimensions, Frags_Drawer, Frags_Ticker, &hud->frags },
            { GUI_ARMOR,    UWG_BOTTOMRIGHT,    HUD_ARMOR,  GF_FONTB,   Armor_Dimensions, Armor_Drawer, Armor_Ticker, &hud->armor },
            { GUI_ARMORICON, UWG_BOTTOMRIGHT,   HUD_ARMOR,  0,          ArmorIcon_Dimensions, ArmorIcon_Drawer, ArmorIcon_Ticker, &hud->armoricon },
            { GUI_KEYS,     UWG_BOTTOMRIGHT,    HUD_KEYS,   0,          Keys_Dimensions, Keys_Drawer, Keys_Ticker, &hud->keys },
            { GUI_FACE,     UWG_BOTTOM,         HUD_FACE,   0,          Face_Dimensions, Face_Drawer, Face_Ticker, &hud->face },
            { GUI_LOG,      UWG_TOP,            -1,         GF_FONTA,   Log_Dimensions, Log_Drawer },
            { GUI_CHAT,     UWG_TOP,            -1,         GF_FONTA,   Chat_Dimensions2, Chat_Drawer2 },
            { GUI_SECRETS,  UWG_COUNTERS,       -1,         GF_FONTA,   Secrets_Dimensions, Secrets_Drawer, Secrets_Ticker, &hud->secrets },
            { GUI_ITEMS,    UWG_COUNTERS,       -1,         GF_FONTA,   Items_Dimensions, Items_Drawer, Items_Ticker, &hud->items },
            { GUI_KILLS,    UWG_COUNTERS,       -1,         GF_FONTA,   Kills_Dimensions, Kills_Drawer, Kills_Ticker, &hud->kills },
            { GUI_NONE }
        };
        size_t i;

        for(i = 0; i < sizeof(widgetGroupDefs)/sizeof(widgetGroupDefs[0]); ++i)
        {
            const uiwidgetgroupdef_t* def = &widgetGroupDefs[i];
            hud->widgetGroupIds[def->group] = GUI_CreateGroup(player, def->flags, def->padding);
        }

        for(i = 0; widgetDefs[i].type != GUI_NONE; ++i)
        {
            const uiwidgetdef_t* def = &widgetDefs[i];
            uiwidgetid_t id = GUI_CreateWidget(def->type, player, def->hideId, def->fontId, def->dimensions, def->drawer, def->ticker, def->typedata);
            UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[def->group]), GUI_FindObjectById(id));
        }

        // Initialize widgets according to player preferences.
        {
        short flags = UIGroup_Flags(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOP]));
        flags &= ~(UWGF_ALIGN_LEFT|UWGF_ALIGN_RIGHT);
        if(cfg.msgAlign == 0)
            flags |= UWGF_ALIGN_LEFT;
        else if(cfg.msgAlign == 2)
            flags |= UWGF_ALIGN_RIGHT;
        UIGroup_SetFlags(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOP]), flags);
        }

        hud->inited = true;

#undef PADDING
    }

    hud->statusbarActive = (fullscreen < 2) || (AM_IsActive(AM_MapForPlayer(player)) && (cfg.automapHudDisplay == 0 || cfg.automapHudDisplay == 2));

    // Do palette shifts.
    ST_doPaletteStuff(player);

    if(hud->statusbarActive || (fullscreen < 3 || hud->alpha > 0))
    {
        int viewW, viewH, x, y, width, height;
        float alpha, scale;

        R_GetViewPort(player, NULL, NULL, &viewW, &viewH);

        if(viewW >= viewH)
            scale = (float)viewH/SCREENHEIGHT;
        else
            scale = (float)viewW/SCREENWIDTH;

        x = y = 0;
        width  = viewW / scale;
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

        if(hud->statusbarActive)
        {
            alpha = (1 - hud->hideAmount) * hud->showBar;
            GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_STATUSBAR]), x, y, width, height, alpha, &drawnWidth, &drawnHeight);
        }

        /**
         * Wide offset scaling.
         * Used with ultra-wide/tall resolutions to move the widgets into
         * the viewer's primary field of vision (without this, widgets
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
                x = (viewW/2/scale -  SCREENWIDTH/2) * (1-cfg.hudWideOffset);
                width -= x*2;
            }
            else
            {
                y = (viewH/2/scale - SCREENHEIGHT/2) * (1-cfg.hudWideOffset);
                height -= y*2;
            }
        }

        alpha = hud->alpha * (1-hud->hideAmount);
        x += PADDING;
        y += PADDING;
        width  -= PADDING*2;
        height -= PADDING*2;

        GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOP]), x, y, width, height, alpha, &drawnWidth, &drawnHeight);
        if(!hud->statusbarActive)
        {
            GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOMLEFT]), x, y, width, height, alpha, &drawnWidth, &drawnHeight);

            availHeight = height - (drawnHeight > 0 ? drawnHeight + PADDING : 0);
            GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOMLEFT2]), x, y, width, availHeight, alpha, &drawnWidth, &drawnHeight);

            GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOM]), x, y, width, height, alpha, &drawnWidth, &drawnHeight);
            GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOMRIGHT]), x, y, width, height, alpha, &drawnWidth, &drawnHeight);
        }
        GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_COUNTERS]), x, y, width, height, alpha, &drawnWidth, &drawnHeight);

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

    // Key cards:
    for(i = 0; i < NUM_KEY_TYPES; ++i)
    {
        sprintf(nameBuf, "STKEYS%d", i);
        R_PrecachePatch(nameBuf, &pKeys[i]);
    }

    // Arms background.
    R_PrecachePatch("STARMS", &pArmsBackground);

    // Arms ownership icons:
    for(i = 0; i < 6; ++i)
    {
        // gray
        sprintf(nameBuf, "STGNUM%d", i + 2);
        R_PrecachePatch(nameBuf, &pArms[i][0]);

        // yellow
        sprintf(nameBuf, "STYSNUM%d", i + 2);
        R_PrecachePatch(nameBuf, &pArms[i][1]);
    }

    // Face backgrounds for different color players.
    for(i = 0; i < 4; ++i)
    {
        sprintf(nameBuf, "STFB%d", i);
        R_PrecachePatch(nameBuf, &pFaceBackground[i]);
    }

    // Status bar background bits.
    R_PrecachePatch("STBAR", &pStatusbar);

    // Face states:
    faceNum = 0;
    for(i = 0; i < ST_NUMPAINFACES; ++i)
    {
        for(j = 0; j < ST_NUMSTRAIGHTFACES; ++j)
        {
            sprintf(nameBuf, "STFST%d%d", i, j);
            R_PrecachePatch(nameBuf, &pFaces[faceNum++]);
        }
        sprintf(nameBuf, "STFTR%d0", i); // Turn right.
        R_PrecachePatch(nameBuf, &pFaces[faceNum++]);
        sprintf(nameBuf, "STFTL%d0", i); // Turn left.
        R_PrecachePatch(nameBuf, &pFaces[faceNum++]);
        sprintf(nameBuf, "STFOUCH%d", i); // Ouch.
        R_PrecachePatch(nameBuf, &pFaces[faceNum++]);
        sprintf(nameBuf, "STFEVL%d", i); // Evil grin.
        R_PrecachePatch(nameBuf, &pFaces[faceNum++]);
        sprintf(nameBuf, "STFKILL%d", i); // Pissed off.
        R_PrecachePatch(nameBuf, &pFaces[faceNum++]);
    }
    R_PrecachePatch("STFGOD0", &pFaces[faceNum++]);
    R_PrecachePatch("STFDEAD0", &pFaces[faceNum++]);
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
    hud->showBar = 1;

    // Statusbar:
    hud->sbarHealth.value = 1994;
    hud->sbarReadyammo.value = 1994;
    hud->sbarArmor.value = 1994;
    hud->sbarFrags.value = 1994;
    for(i = 0; i < NUM_AMMO_TYPES; ++i)
    {
        hud->sbarAmmos[i].ammotype = (ammotype_t)i;
        hud->sbarAmmos[i].value = 1994;

        hud->sbarMaxammos[i].ammotype = (ammotype_t)i;
        hud->sbarMaxammos[i].value = 1994;
    }
    for(i = 0; i < 6/*num weapon slots*/; ++i)
    {
        hud->sbarWeaponslots[i].slot = i+1;
        hud->sbarWeaponslots[i].patchId = 0;
    }
    for(i = 0; i < 3/*num key slots*/; ++i)
    {
        hud->sbarKeyslots[i].slot = i;
        hud->sbarKeyslots[i].keytypeA = (keytype_t)i;
        hud->sbarKeyslots[i].keytypeB = (keytype_t)(i+3);
        hud->sbarKeyslots[i].patchId = hud->sbarKeyslots[i].patchId2 = 0;
    }
    hud->sbarFace.faceCount = 0;
    hud->sbarFace.faceIndex = 0;
    hud->sbarFace.lastAttackDown = -1;
    hud->sbarFace.oldHealth = -1;
    hud->sbarFace.priority = 0;
    for(i = 0; i < NUM_WEAPON_TYPES; ++i)
    {
        hud->sbarFace.oldWeaponsOwned[i] = plr->weapons[i].owned;
    }

    // Fullscreen:
    hud->health.value = 1994;
    hud->armoricon.sprite = -1;
    hud->armor.value = 1994;
    hud->readyammoicon.sprite = -1;
    hud->readyammo.value = 1994;
    for(i = 0; i < NUM_KEY_TYPES; ++i)
    {
        hud->keys.keyBoxes[i] = 0;
    }
    hud->frags.value = 1994;
    hud->face.faceCount = 0;
    hud->face.faceIndex = 0;
    hud->face.lastAttackDown = -1;
    hud->face.oldHealth = -1;
    hud->face.priority = 0;
    for(i = 0; i < NUM_WEAPON_TYPES; ++i)
    {
        hud->face.oldWeaponsOwned[i] = plr->weapons[i].owned;
    }

    // Other:
    hud->secrets.value = 1994;
    hud->items.value = 1994;
    hud->kills.value = 1994;

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

void ST_Shutdown(void)
{
    int i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        hudstate_t* hud = &hudStates[i];
        hud->inited = false;
    }
}

void ST_UpdateLogAlignment(void)
{
    short flags;
    int i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        hudstate_t* hud = &hudStates[i];
        if(!hud->inited) continue;

        flags = UIGroup_Flags(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOP]));
        flags &= ~(UWGF_ALIGN_LEFT|UWGF_ALIGN_RIGHT);
        if(cfg.msgAlign == 0)
            flags |= UWGF_ALIGN_LEFT;
        else if(cfg.msgAlign == 2)
            flags |= UWGF_ALIGN_RIGHT;
        UIGroup_SetFlags(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOP]), flags);
    }
}

/**
 * Called when the statusbar scale cvar changes.
 */
void updateViewWindow(void)
{
    int i;
    R_UpdateViewWindow(true);
    for(i = 0; i < MAXPLAYERS; ++i)
        ST_HUDUnHide(i, HUE_FORCE); // So the user can see the change.
}

/**
 * Called when a cvar changes that affects the look/behavior of the HUD in order to unhide it.
 */
void unhideHUD(void)
{
    int i;
    for(i = 0; i < MAXPLAYERS; ++i)
        ST_HUDUnHide(i, HUE_FORCE);
}
