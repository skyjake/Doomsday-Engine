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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jdoom.h"

#include "d_net.h"
#include "hu_stuff.h"
#include "hu_lib.h"
#include "hu_chat.h"
#include "hu_log.h"
#include "hu_automap.h"
#include "p_mapsetup.h"
#include "p_tick.h" // for P_IsPaused
#include "p_player.h"
#include "p_user.h"
#include "r_common.h"
#include "am_map.h"

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
    UWG_MAPNAME,
    UWG_BOTTOMLEFT,
    UWG_BOTTOMLEFT2,
    UWG_BOTTOMRIGHT,
    UWG_BOTTOM,
    UWG_TOP,
    UWG_COUNTERS,
    UWG_AUTOMAP,
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
    int automapCheatLevel; /// \todo Belongs in player state?

    int widgetGroupIds[NUM_UIWIDGET_GROUPS];
    int automapWidgetId;
    int chatWidgetId;
    int logWidgetId;

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
    guidata_automap_t automap;
    guidata_chat_t chat;
    guidata_log_t log;
    guidata_secrets_t secrets;
    guidata_items_t items;
    guidata_kills_t kills;
} hudstate_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void updateViewWindow(void);
void unhideHUD(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

int ST_ChatResponder(int player, event_t* ev);

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

// CODE --------------------------------------------------------------------

/**
 * Register CVARs and CCmds for the HUD/Status bar
 */
void ST_Register(void)
{
    cvartemplate_t cvars[] = {
        { "hud-scale", 0, CVT_FLOAT, &cfg.hudScale, 0.1f, 1, unhideHUD },
        { "hud-wideoffset", 0, CVT_FLOAT, &cfg.hudWideOffset, 0, 1, unhideHUD },

        { "hud-status-size", 0, CVT_FLOAT, &cfg.statusbarScale, 0.1f, 1, updateViewWindow },

        // HUD color + alpha
        { "hud-color-r", 0, CVT_FLOAT, &cfg.hudColor[0], 0, 1, unhideHUD },
        { "hud-color-g", 0, CVT_FLOAT, &cfg.hudColor[1], 0, 1, unhideHUD },
        { "hud-color-b", 0, CVT_FLOAT, &cfg.hudColor[2], 0, 1, unhideHUD },
        { "hud-color-a", 0, CVT_FLOAT, &cfg.hudColor[3], 0, 1, unhideHUD },
        { "hud-icon-alpha", 0, CVT_FLOAT, &cfg.hudIconAlpha, 0, 1, unhideHUD },

        { "hud-face-ouchfix", 0, CVT_BYTE, &cfg.fixOuchFace, 0, 1 },

        { "hud-status-alpha", 0, CVT_FLOAT, &cfg.statusbarOpacity, 0, 1, unhideHUD },
        { "hud-status-icon-a", 0, CVT_FLOAT, &cfg.statusbarCounterAlpha, 0, 1, unhideHUD },
        { "hud-status-weaponslots-ownedfix", 0, CVT_BYTE, &cfg.fixStatusbarOwnedWeapons, 0, 1, unhideHUD },

        // HUD icons
        { "hud-face", 0, CVT_BYTE, &cfg.hudShown[HUD_FACE], 0, 1, unhideHUD },
        { "hud-health", 0, CVT_BYTE, &cfg.hudShown[HUD_HEALTH], 0, 1, unhideHUD },
        { "hud-armor", 0, CVT_BYTE, &cfg.hudShown[HUD_ARMOR], 0, 1, unhideHUD },
        { "hud-ammo", 0, CVT_BYTE, &cfg.hudShown[HUD_AMMO], 0, 1, unhideHUD },
        { "hud-keys", 0, CVT_BYTE, &cfg.hudShown[HUD_KEYS], 0, 1, unhideHUD },
        { "hud-keys-combine", 0, CVT_BYTE, &cfg.hudKeysCombine, 0, 1, unhideHUD },

        // HUD displays
        { "hud-frags", 0, CVT_BYTE, &cfg.hudShown[HUD_FRAGS], 0, 1, unhideHUD },

        { "hud-timer", 0, CVT_FLOAT, &cfg.hudTimer, 0, 60 },

        { "hud-unhide-damage", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_DAMAGE], 0, 1 },
        { "hud-unhide-pickup-health", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_HEALTH], 0, 1 },
        { "hud-unhide-pickup-armor", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_ARMOR], 0, 1 },
        { "hud-unhide-pickup-powerup", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_POWER], 0, 1 },
        { "hud-unhide-pickup-weapon", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_WEAPON], 0, 1 },
        { "hud-unhide-pickup-ammo", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_AMMO], 0, 1 },
        { "hud-unhide-pickup-key", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_KEY], 0, 1 },

        { "hud-cheat-counter", 0, CVT_BYTE, &cfg.hudShownCheatCounters, 0, 63, unhideHUD },
        { "hud-cheat-counter-scale", 0, CVT_FLOAT, &cfg.hudCheatCounterScale, .1f, 1, unhideHUD },
        { NULL }
    };
    ccmdtemplate_t ccmds[] = {
        { "beginchat",       NULL,   CCmdChatOpen },
        { "chatcancel",      "",     CCmdChatAction },
        { "chatcomplete",    "",     CCmdChatAction },
        { "chatdelete",      "",     CCmdChatAction },
        { "chatsendmacro",   NULL,   CCmdChatSendMacro },
        { NULL }
    };
    int i;
    for(i = 0; cvars[i].name; ++i)
        Con_AddVariable(cvars + i);
    for(i = 0; ccmds[i].name; ++i)
        Con_AddCommand(ccmds + i);
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

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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
#undef ORIGINY
#undef ORIGINX
#undef HEIGHT
#undef WIDTH
}

void SBarBackground_UpdateDimensions(uiwidget_t* obj)
{
#define WIDTH       (ST_WIDTH)
#define HEIGHT      (ST_HEIGHT)

    assert(NULL != obj);

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    obj->dimensions.width  = WIDTH  * cfg.statusbarScale;
    obj->dimensions.height = HEIGHT * cfg.statusbarScale;

#undef HEIGHT
#undef WIDTH
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
void Face_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    assert(NULL != obj);
    {
    guidata_face_t* face = (guidata_face_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    angle_t badGuyAngle, diffAng;
    boolean doEvilGrin;
    int i;

    if(P_IsPaused() || !GUI_GameTicTriggerIsSharp())
        return;

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

int ST_Responder(event_t* ev)
{
    int i, eaten;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        eaten = ST_ChatResponder(i, ev);
        if(0 != eaten)
            return eaten;
    }
    return false;
}

void ST_Ticker(timespan_t ticLength)
{
    /// \kludge 
    boolean runGameTic = GUI_RunGameTicTrigger(ticLength);
    /// kludge end.
    int i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t* plr = &players[i];
        hudstate_t* hud = &hudStates[i];

        if(!plr->plr->inGame)
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
        if(runGameTic && !P_IsPaused())
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
        }

        if(hud->inited)
        {
            int j;
            for(j = 0; j < NUM_UIWIDGET_GROUPS; ++j)
            {
                UIWidget_RunTic(GUI_MustFindObjectById(hud->widgetGroupIds[j]), ticLength);
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

void ReadyAmmo_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    assert(NULL != obj);
    {
    guidata_readyammo_t* ammo = (guidata_readyammo_t*)obj->typedata;
    int player = obj->player;
    player_t* plr = &players[player];
    ammotype_t ammoType;
    boolean found;

    if(P_IsPaused() || !GUI_GameTicTriggerIsSharp())
        return;

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

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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

    FR_SetFont(obj->fontId);

    DGL_Color4f(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);

    FR_DrawTextFragment2(buf, X, Y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarReadyAmmo_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    const guidata_readyammo_t* ammo = (guidata_readyammo_t*)obj->typedata;
    char buf[20];

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(ammo->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", ammo->value);
    FR_SetFont(obj->fontId);
    FR_TextFragmentDimensions(&obj->dimensions.width, &obj->dimensions.height, buf);
    obj->dimensions.width  *= cfg.statusbarScale;
    obj->dimensions.height *= cfg.statusbarScale;
    }
}

void Ammo_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    assert(NULL != obj);
    {
    guidata_ammo_t* ammo = (guidata_ammo_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    if(P_IsPaused() || !GUI_GameTicTriggerIsSharp())
        return;
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

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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

    FR_SetFont(obj->fontId);
    DGL_Color4f(defFontRGB3[CR], defFontRGB3[CG], defFontRGB3[CB], textAlpha);
    FR_DrawTextFragment2(buf, ORIGINX+loc->x, ORIGINY+loc->y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef MAXDIGITS
#undef ORIGINY
#undef ORIGINX
}

void Ammo_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj);

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    FR_SetFont(obj->fontId);
    obj->dimensions.width  = (FR_CharWidth('0') * 3) * cfg.statusbarScale;
    obj->dimensions.height = FR_CharHeight('0') * cfg.statusbarScale;
}

void MaxAmmo_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    assert(NULL != obj);
    {
    guidata_ammo_t* ammo = (guidata_ammo_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    if(P_IsPaused() || !GUI_GameTicTriggerIsSharp())
        return;
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

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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

    FR_SetFont(obj->fontId);
    DGL_Color4f(defFontRGB3[CR], defFontRGB3[CG], defFontRGB3[CB], textAlpha);
    FR_DrawTextFragment2(buf, ORIGINX+loc->x, ORIGINY+loc->y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef MAXDIGITS
#undef ORIGINY
#undef ORIGINX
}

void MaxAmmo_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj);

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    FR_SetFont(obj->fontId);
    obj->dimensions.width  = (FR_CharWidth('0') * 3) * cfg.statusbarScale;
    obj->dimensions.height = FR_CharHeight('0') * cfg.statusbarScale;
}

void Health_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    assert(NULL != obj);
    {
    guidata_health_t* hlth = (guidata_health_t*) obj->typedata;
    const player_t* plr = &players[obj->player];
    if(P_IsPaused() || !GUI_GameTicTriggerIsSharp())
        return;
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

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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

    FR_SetFont(obj->fontId);
    DGL_Color4f(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);

    FR_DrawTextFragment2(buf, X, Y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);
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

void SBarHealth_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    const guidata_health_t* hlth = (guidata_health_t*)obj->typedata;
    char buf[20];

    obj->dimensions.width  = 0;    
    obj->dimensions.height = 0;

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(hlth->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", hlth->value);
    FR_SetFont(obj->fontId);
    obj->dimensions.width  = (FR_TextFragmentWidth(buf) + FR_CharWidth('%')) * cfg.statusbarScale;
    obj->dimensions.height = MAX_OF(FR_TextFragmentHeight(buf), FR_CharHeight('%')) * cfg.statusbarScale;
    }
}

void Armor_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    assert(NULL != obj);
    {
    guidata_armor_t* armor = (guidata_armor_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    if(P_IsPaused() || !GUI_GameTicTriggerIsSharp())
        return;
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

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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

    FR_SetFont(obj->fontId);
    DGL_Color4f(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);

    FR_DrawTextFragment2(buf, X, Y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);
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

void SBarArmor_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    const guidata_armor_t* armor = (guidata_armor_t*)obj->typedata;
    char buf[20];

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(armor->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", armor->value);
    FR_SetFont(obj->fontId);
    obj->dimensions.width  = (FR_TextFragmentWidth(buf) + FR_CharWidth('%')) * cfg.statusbarScale;
    obj->dimensions.height = MAX_OF(FR_TextFragmentHeight(buf), FR_CharHeight('%')) * cfg.statusbarScale;
    }
}

void SBarFrags_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    assert(NULL != obj);
    {
    guidata_frags_t* frags = (guidata_frags_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    int i;
    if(P_IsPaused() || !GUI_GameTicTriggerIsSharp())
        return;
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
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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

    FR_SetFont(obj->fontId);
    DGL_Color4f(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);

    FR_DrawTextFragment2(buf, X, Y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarFrags_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    const guidata_frags_t* frags = (guidata_frags_t*)obj->typedata;
    char buf[20];

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(!deathmatch)
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(frags->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i", frags->value);
    FR_SetFont(obj->fontId);
    FR_TextFragmentDimensions(&obj->dimensions.width, &obj->dimensions.height, buf);
    obj->dimensions.width  *= cfg.statusbarScale;
    obj->dimensions.height *= cfg.statusbarScale;
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

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    if(face->faceIndex >= 0)
    {
        patchid_t patchId = pFaces[face->faceIndex].id;
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        DGL_Translatef(x, y, 0);
        DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
        DGL_Translatef(0, yOffset, 0);
        DGL_Enable(DGL_TEXTURE_2D);

        WI_DrawPatch4(patchId, Hu_ChoosePatchReplacement(patchId), ORIGINX+ST_FACESX, ORIGINY+ST_FACESY, ALIGN_TOPLEFT, 0, FID(GF_FONTB), 1, 1, 1, iconAlpha);

        DGL_Disable(DGL_TEXTURE_2D);
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }
    }
#undef ORIGINY
#undef ORIGINX
}

void SBarFace_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    const guidata_face_t* face = (guidata_face_t*)obj->typedata;
    const patchinfo_t* facePatch;

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    facePatch = &pFaces[face->faceIndex%ST_NUMFACES];
    obj->dimensions.width  = facePatch->width  * cfg.statusbarScale;
    obj->dimensions.height = facePatch->height * cfg.statusbarScale;
    }
}

void KeySlot_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    assert(NULL != obj);
    {
    guidata_keyslot_t* kslt = (guidata_keyslot_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    if(P_IsPaused() || !GUI_GameTicTriggerIsSharp())
        return;
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

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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

    WI_DrawPatch4(kslt->patchId, Hu_ChoosePatchReplacement(kslt->patchId), loc->x + offset, loc->y + offset, ALIGN_TOPLEFT, 0, FID(GF_FONTB), 1, 1, 1, iconAlpha);
    if(kslt->patchId2 != 0)
    {
        WI_DrawPatch4(kslt->patchId2, Hu_ChoosePatchReplacement(kslt->patchId2), loc->x - offset, loc->y - offset, ALIGN_TOPLEFT, 0, FID(GF_FONTB), 1, 1, 1, iconAlpha);
    }

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef ORIGINY
#undef ORIGINX
}

void KeySlot_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_keyslot_t* kslt = (guidata_keyslot_t*)obj->typedata;
    patchinfo_t info;

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(kslt->patchId == 0 && kslt->patchId2 == 0)
        return;
    if(!R_GetPatchInfo(kslt->patchId, &info))
        return;

    obj->dimensions.width  = info.width;
    obj->dimensions.height = info.height;

    if(kslt->patchId2 != 0)
    {
        if(R_GetPatchInfo(kslt->patchId, &info))
        {
            if(info.width > obj->dimensions.width)
                obj->dimensions.width = info.width;
            if(info.height > obj->dimensions.height)
                obj->dimensions.height = info.height;
        }
        obj->dimensions.width  += 2;
        obj->dimensions.height += 2;
    }

    obj->dimensions.width  *= cfg.statusbarScale;
    obj->dimensions.height *= cfg.statusbarScale;
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

void WeaponSlot_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    assert(NULL != obj);
    {
    guidata_weaponslot_t* wpn = (guidata_weaponslot_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    boolean used = false;

    if(P_IsPaused() || !GUI_GameTicTriggerIsSharp())
        return;

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
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    WI_DrawPatch4(wpns->patchId, Hu_ChoosePatchReplacement(wpns->patchId), element->x, element->y, ALIGN_TOPLEFT, 0, 0, 1, 1, 1, textAlpha);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
#undef ORIGINY
#undef ORIGINX
}

void WeaponSlot_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_weaponslot_t* wpns = (guidata_weaponslot_t*)obj->typedata;
    patchinfo_t info;

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(deathmatch)
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(!R_GetPatchInfo(wpns->patchId, &info))
        return;

    obj->dimensions.width  = info.width  * cfg.statusbarScale;
    obj->dimensions.height = info.height * cfg.statusbarScale;
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

void Frags_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    assert(NULL != obj);
    {
    guidata_frags_t* frags = (guidata_frags_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    int i;
    if(P_IsPaused() || !GUI_GameTicTriggerIsSharp())
        return;
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

    if(!cfg.hudShown[HUD_FRAGS])
        return;
    if(!deathmatch)
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    sprintf(buf, "FRAGS:%i", frags->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->fontId);
    DGL_Color4f(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    FR_DrawTextFragment2(buf, 0, 0, ALIGN_TOPLEFT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void Frags_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    const guidata_frags_t* frags = (guidata_frags_t*)obj->typedata;
    char buf[20];

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(!cfg.hudShown[HUD_FRAGS])
        return;
    if(!deathmatch)
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    sprintf(buf, "FRAGS:%i", frags->value);
    FR_SetFont(obj->fontId);
    FR_TextFragmentDimensions(&obj->dimensions.width, &obj->dimensions.height, buf);
    obj->dimensions.width  *= cfg.hudScale;
    obj->dimensions.height *= cfg.hudScale;
    }
}

void Health_Drawer(uiwidget_t* obj, int x, int y)
{
    assert(NULL != obj);
    {
    guidata_health_t* hlth = (guidata_health_t*)obj->typedata;
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    char buf[20];

    if(!cfg.hudShown[HUD_HEALTH])
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    sprintf(buf, "%i%%", hlth->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->fontId);
    DGL_Color4f(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    FR_DrawTextFragment2(buf, 0, 0, ALIGN_BOTTOMLEFT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void Health_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_health_t* hlth = (guidata_health_t*)obj->typedata;
    char buf[20];

    obj->dimensions.width = 0;
    obj->dimensions.height = 0;

    if(!cfg.hudShown[HUD_HEALTH])
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    sprintf(buf, "%i%%", hlth->value);
    FR_SetFont(obj->fontId);
    FR_TextFragmentDimensions(&obj->dimensions.width, &obj->dimensions.height, buf);
    obj->dimensions.width  *= cfg.hudScale;
    obj->dimensions.height *= cfg.hudScale;
    }
}

void HealthIcon_Drawer(uiwidget_t* obj, int x, int y)
{
    assert(NULL != obj);
    {
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;

    if(!cfg.hudShown[HUD_HEALTH])
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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

void HealthIcon_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(!cfg.hudShown[HUD_HEALTH])
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    ST_HUDSpriteDimensions(SPR_STIM, 1, &obj->dimensions.width, &obj->dimensions.height);
    obj->dimensions.width  *= cfg.hudScale;
    obj->dimensions.height *= cfg.hudScale;
    }
}

void ReadyAmmo_Drawer(uiwidget_t* obj, int x, int y)
{
    assert(NULL != obj);
    {
    guidata_readyammo_t* ammo = (guidata_readyammo_t*)obj->typedata;
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    char buf[20];

    if(!cfg.hudShown[HUD_AMMO])
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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

    FR_SetFont(obj->fontId);
    DGL_Color4f(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    FR_DrawTextFragment2(buf, 0, 0, ALIGN_BOTTOMLEFT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void ReadyAmmo_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_readyammo_t* ammo = (guidata_readyammo_t*)obj->typedata;
    char buf[20];

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(!cfg.hudShown[HUD_AMMO])
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(ammo->value == 1994)
        return;

    sprintf(buf, "%i", ammo->value);
    FR_SetFont(obj->fontId);
    FR_TextFragmentDimensions(&obj->dimensions.width, &obj->dimensions.height, buf);
    obj->dimensions.width  *= cfg.hudScale;
    obj->dimensions.height *= cfg.hudScale;
    }
}

void ReadyAmmoIcon_Ticker(uiwidget_t* obj, timespan_t ticLength)
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
    if(P_IsPaused() || !GUI_GameTicTriggerIsSharp())
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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

    if(!cfg.hudShown[HUD_AMMO])
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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

void ReadyAmmoIcon_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_readyammoicon_t* icon = (guidata_readyammoicon_t*)obj->typedata;
    float scale;

    obj->dimensions.width = 0;
    obj->dimensions.height = 0;

    if(!cfg.hudShown[HUD_AMMO])
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(icon->sprite < 0)
        return;

    scale = (icon->sprite == SPR_ROCK? .72f : 1);
    ST_HUDSpriteDimensions(icon->sprite, scale, &obj->dimensions.width, &obj->dimensions.height);
    obj->dimensions.width  *= cfg.hudScale;
    obj->dimensions.height *= cfg.hudScale;
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

    if(!cfg.hudShown[HUD_FACE])
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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

void Face_UpdateDimensions(uiwidget_t* obj)
{
#define EXTRA_SCALE         .7f
    assert(NULL != obj);
    {
    const patchinfo_t* bgPatch = &pFaceBackground[cfg.playerColor[obj->player]];

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(!cfg.hudShown[HUD_FACE])
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;

    obj->dimensions.width  = bgPatch->width  * EXTRA_SCALE * cfg.hudScale;
    obj->dimensions.height = bgPatch->height * EXTRA_SCALE * cfg.hudScale;
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

    if(!cfg.hudShown[HUD_ARMOR])
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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

    FR_SetFont(obj->fontId);
    DGL_Color4f(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    FR_DrawTextFragment2(buf, 0, 0, ALIGN_BOTTOMRIGHT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void Armor_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_armor_t* armor = (guidata_armor_t*)obj->typedata;
    char buf[20];

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(!cfg.hudShown[HUD_ARMOR])
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(armor->value == 1994)
        return;

    dd_snprintf(buf, 20, "%i%%", armor->value);

    FR_SetFont(obj->fontId);
    FR_TextFragmentDimensions(&obj->dimensions.width, &obj->dimensions.height, buf);
    obj->dimensions.width  *= cfg.hudScale;
    obj->dimensions.height *= cfg.hudScale;
    }
}

void ArmorIcon_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    assert(NULL != obj);
    {
    guidata_armoricon_t* icon = (guidata_armoricon_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    if(P_IsPaused() || !GUI_GameTicTriggerIsSharp())
        return;
    icon->sprite = (plr->armorType == 2 ? SPR_ARM2 : SPR_ARM1);
    }
}

void ArmorIcon_Drawer(uiwidget_t* obj, int x, int y)
{
    assert(NULL != obj);
    {
    guidata_armoricon_t* icon = (guidata_armoricon_t*)obj->typedata;
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;

    if(!cfg.hudShown[HUD_ARMOR])
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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

void ArmorIcon_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_armoricon_t* icon = (guidata_armoricon_t*)obj->typedata;

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(!cfg.hudShown[HUD_ARMOR])
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
        return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK))
        return;
    if(icon->sprite < 0)
        return;

    ST_HUDSpriteDimensions(icon->sprite, 1, &obj->dimensions.width, &obj->dimensions.height);
    obj->dimensions.width  *= cfg.hudScale;
    obj->dimensions.height *= cfg.hudScale;
    }
}

void Keys_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    assert(NULL != obj);
    {
    guidata_keys_t* keys = (guidata_keys_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    int i;
    if(P_IsPaused() || !GUI_GameTicTriggerIsSharp())
        return;
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

    if(!cfg.hudShown[HUD_KEYS])
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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

void Keys_UpdateDimensions(uiwidget_t* obj)
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

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(!cfg.hudShown[HUD_KEYS])
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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
            obj->dimensions.width += w;
            if(h > obj->dimensions.height)
                obj->dimensions.height = h;
            numDrawnKeys++;
        }
    }
    obj->dimensions.width += (numDrawnKeys-1) * 2;

    obj->dimensions.width  = obj->dimensions.width  * EXTRA_SCALE * cfg.hudScale;
    obj->dimensions.height = obj->dimensions.height * EXTRA_SCALE * cfg.hudScale;
    }
#undef EXTRA_SCALE
}

void Kills_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    assert(NULL != obj);
    {
    guidata_kills_t* kills = (guidata_kills_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    if(P_IsPaused() || !GUI_GameTicTriggerIsSharp())
        return;
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
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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

    FR_SetFont(obj->fontId);
    DGL_Color4f(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    FR_DrawTextFragment2(buf, 0, 0, ALIGN_BOTTOMLEFT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void Kills_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_kills_t* kills = (guidata_kills_t*)obj->typedata;
    char buf[40], tmp[20];

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(!(cfg.hudShownCheatCounters & (CCH_KILLS | CCH_KILLS_PRCNT)))
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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

    FR_SetFont(obj->fontId);
    FR_TextFragmentDimensions(&obj->dimensions.width, &obj->dimensions.height, buf);
    obj->dimensions.width  *= cfg.hudCheatCounterScale;
    obj->dimensions.height *= cfg.hudCheatCounterScale;
    }
}

void Items_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    assert(NULL != obj);
    {
    guidata_items_t* items = (guidata_items_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    if(P_IsPaused() || !GUI_GameTicTriggerIsSharp())
        return;
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
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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

    FR_SetFont(obj->fontId);
    DGL_Color4f(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    FR_DrawTextFragment2(buf, 0, 0, ALIGN_BOTTOMLEFT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void Items_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_items_t* items = (guidata_items_t*)obj->typedata;
    char buf[40], tmp[20];

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(!(cfg.hudShownCheatCounters & (CCH_ITEMS | CCH_ITEMS_PRCNT)))
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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

    FR_SetFont(obj->fontId);
    FR_TextFragmentDimensions(&obj->dimensions.width, &obj->dimensions.height, buf);
    obj->dimensions.width  *= cfg.hudCheatCounterScale;
    obj->dimensions.height *= cfg.hudCheatCounterScale;
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
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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

    FR_SetFont(obj->fontId);
    DGL_Color4f(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    FR_DrawTextFragment2(buf, 0, 0, ALIGN_BOTTOMLEFT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void Secrets_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    assert(NULL != obj);
    {
    guidata_secrets_t* scrt = (guidata_secrets_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    if(P_IsPaused() || !GUI_GameTicTriggerIsSharp())
        return;
    scrt->value = plr->secretCount;
    }
}

void Secrets_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    guidata_secrets_t* scrt = (guidata_secrets_t*)obj->typedata;
    char buf[40], tmp[20];

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(!(cfg.hudShownCheatCounters & (CCH_SECRETS | CCH_SECRETS_PRCNT)))
        return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0)
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

    FR_SetFont(obj->fontId);
    FR_TextFragmentDimensions(&obj->dimensions.width, &obj->dimensions.height, buf);
    obj->dimensions.width  *= cfg.hudCheatCounterScale;
    obj->dimensions.height *= cfg.hudCheatCounterScale;
    }
}

void MapName_Drawer(uiwidget_t* obj, int x, int y)
{
    assert(NULL != obj && obj->type == GUI_MAPNAME);
    {
    const float scale = .75f;
    const float textAlpha = uiRendState->pageAlpha;
    const patchid_t patch = P_FindMapTitlePatch(gameEpisode, gameMap);
    const char* text = Hu_ChoosePatchReplacement2(patch, P_GetMapNiceName(), false);

    if(NULL == text && 0 == patch)
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(scale, scale, 1);

    DGL_Enable(DGL_TEXTURE_2D);
    WI_DrawPatch4(patch, text, 0, 0, ALIGN_BOTTOMLEFT, 0, obj->fontId, cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
    }
}

void MapName_UpdateDimensions(uiwidget_t* obj)
{
    assert(NULL != obj && obj->type == GUI_MAPNAME);
    {
    const patchid_t patch = P_FindMapTitlePatch(gameEpisode, gameMap);
    const char* text = Hu_ChoosePatchReplacement2(patch, P_GetMapNiceName(), false);
    const float scale = .75f;
    patchinfo_t info;

    obj->dimensions.width  = 0;
    obj->dimensions.height = 0;

    if(NULL == text && 0 == patch)
        return;

    if(NULL != text)
    {
        FR_TextDimensions(&obj->dimensions.width, &obj->dimensions.height, text, obj->fontId);
        obj->dimensions.width  *= scale;
        obj->dimensions.height *= scale;
        return;    
    }

    R_GetPatchInfo(patch, &info);
    obj->dimensions.width  = info.width  * scale;
    obj->dimensions.height = info.height * scale;
    }
}

typedef struct {
    guiwidgettype_t type;
    int group;
    gamefontid_t fontIdx;
    void (*updateDimensions) (uiwidget_t* obj);
    void (*drawer) (uiwidget_t* obj, int x, int y);
    void (*ticker) (uiwidget_t* obj, timespan_t ticLength);
    void* typedata;
} uiwidgetdef_t;

typedef struct {
    int group;
    int alignFlags;
    int groupFlags;
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
    hud->statusbarActive = (fullscreen < 2) || (ST_AutomapIsActive(player) && (cfg.automapHudDisplay == 0 || cfg.automapHudDisplay == 2));

    // Do palette shifts.
    ST_doPaletteStuff(player);

    GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_AUTOMAP]), 0, 0, SCREENWIDTH, SCREENHEIGHT, ST_AutomapOpacity(player), NULL, NULL);

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

        int availHeight, drawnWidth = 0, drawnHeight = 0;

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

        if(!hud->statusbarActive)
        {
            int h = 0;
            GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOMLEFT]), x, y, width, height, alpha, &drawnWidth, &drawnHeight);
            if(drawnHeight > h) h = drawnHeight;
            GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOM]), x, y, width, height, alpha, &drawnWidth, &drawnHeight);
            if(drawnHeight > h) h = drawnHeight;
            GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOMRIGHT]), x, y, width, height, alpha, &drawnWidth, &drawnHeight);
            if(drawnHeight > h) h = drawnHeight;
            drawnHeight = h;
        }

        if(!hud->statusbarActive)
        {
            int h = drawnHeight;
            availHeight = height - (drawnHeight > 0 ? drawnHeight : 0);
            GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOMLEFT2]), x, y, width, availHeight, alpha, &drawnWidth, &drawnHeight);
            drawnHeight += h;
        }

        availHeight = height - (drawnHeight > 0 ? drawnHeight : 0);
        GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_MAPNAME]), x, y, width, availHeight, ST_AutomapOpacity(player), NULL, NULL);

        GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOP]), x, y, width, height, alpha, NULL, NULL);
        GUI_DrawWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_COUNTERS]), x, y, width, height, alpha, NULL, NULL);

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

    hud->log._msgCount = 0;
    hud->log._nextUsedMsg = 0;
    hud->log._pvisMsgCount = 0;
    memset(hud->log._msgs, 0, sizeof(hud->log._msgs));

    ST_HUDUnHide(player, HUE_FORCE);
}

static void findMapBounds(float* lowX, float* hiX, float* lowY, float* hiY)
{
    assert(NULL != lowX && NULL != hiX && NULL != lowY && NULL != hiY);
    {
    float pos[2];
    uint i;

    *lowX = *lowY =  DDMAXFLOAT;
    *hiX  = *hiY  = -DDMAXFLOAT;

    for(i = 0; i < numvertexes; ++i)
    {
        P_GetFloatv(DMU_VERTEX, i, DMU_XY, pos);

        if(pos[VX] < *lowX)
            *lowX = pos[VX];
        if(pos[VX] > *hiX)
            *hiX  = pos[VX];

        if(pos[VY] < *lowY)
            *lowY = pos[VY];
        if(pos[VY] > *hiY)
            *hiY  = pos[VY];
    }
    }
}

static void setAutomapCheatLevel(uiwidget_t* obj, int level)
{
    assert(NULL != obj);
    {
    hudstate_t* hud = &hudStates[UIWidget_Player(obj)];
    int flags;

    hud->automapCheatLevel = level;

    flags = UIAutomap_Flags(obj) & ~(AMF_REND_ALLLINES|AMF_REND_THINGS|AMF_REND_SPECIALLINES|AMF_REND_VERTEXES|AMF_REND_LINE_NORMALS);
    if(hud->automapCheatLevel >= 1)
        flags |= AMF_REND_ALLLINES;
    if(hud->automapCheatLevel == 2)
        flags |= AMF_REND_THINGS | AMF_REND_SPECIALLINES;
    if(hud->automapCheatLevel > 2)
        flags |= (AMF_REND_VERTEXES | AMF_REND_LINE_NORMALS);
    UIAutomap_SetFlags(obj, flags);
    }
}

static void initAutomapForCurrentMap(uiwidget_t* obj)
{
    assert(NULL != obj);
    {
    hudstate_t* hud = &hudStates[UIWidget_Player(obj)];
    float lowX, hiX, lowY, hiY;
    automapcfg_t* mcfg;

    UIAutomap_Reset(obj);

    findMapBounds(&lowX, &hiX, &lowY, &hiY);
    UIAutomap_SetMinScale(obj, 2 * PLAYERRADIUS);
    UIAutomap_SetWorldBounds(obj, lowX, hiX, lowY, hiY);

    mcfg = UIAutomap_Config(obj);

    // Determine the obj view scale factors.
    UIAutomap_SetScale(obj, UIAutomap_ZoomMax(obj)? 0 : .45f);
    UIAutomap_ClearPoints(obj);

#if !__JHEXEN__
    if(gameSkill == SM_BABY && cfg.automapBabyKeys)
    {
        int flags = UIAutomap_Flags(obj);
        UIAutomap_SetFlags(obj, flags|AMF_REND_KEYS);
    }

    if(!IS_NETGAME && hud->automapCheatLevel)
        AM_SetVectorGraphic(mcfg, AMO_THINGPLAYER, VG_CHEATARROW);
#endif

    // Are we re-centering on a followed mobj?
    { mobj_t* mo = UIAutomap_FollowMobj(obj);
    if(NULL != mo)
    {
        UIAutomap_SetCameraOrigin(obj, mo->pos[VX], mo->pos[VY]);
    }}

    if(IS_NETGAME)
    {
        setAutomapCheatLevel(obj, 0);
    }

    UIAutomap_SetReveal(obj, false);

    // Add all immediately visible lines.
    { uint i;
    for(i = 0; i < numlines; ++i)
    {
        xline_t* xline = &xlines[i];
        if(!(xline->flags & ML_MAPPED)) continue;
        P_SetLinedefAutomapVisibility(UIWidget_Player(obj), i, true);
    }}
    }
}

void ST_Start(int player)
{
    const int winWidth  = Get(DD_WINDOW_WIDTH);
    const int winHeight = Get(DD_WINDOW_HEIGHT);
    uiwidget_t* obj;
    hudstate_t* hud;
    int flags;
    if(player < 0 && player >= MAXPLAYERS)
    {
        Con_Error("ST_Start: Invalid player #%i.", player);
        exit(1); // Unreachable.
    }
    hud = &hudStates[player];

    if(!hud->stopped)
        ST_Stop(player);

    initData(hud);
    // If the automap has been left open; close it.
    ST_AutomapOpen(player, false, true);

    /**
     * Initialize widgets according to player preferences.
     */

    obj = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOP]);
    flags = UIWidget_Alignment(obj);
    flags &= ~(ALIGN_LEFT|ALIGN_RIGHT);
    if(cfg.msgAlign == 0)
        flags |= ALIGN_LEFT;
    else if(cfg.msgAlign == 2)
        flags |= ALIGN_RIGHT;
    UIWidget_SetAlignment(obj, flags);

    obj = GUI_MustFindObjectById(hud->automapWidgetId);
    initAutomapForCurrentMap(obj);
    UIAutomap_SetScale(obj, 1);
    UIAutomap_SetCameraRotation(obj, cfg.automapRotate);
    UIAutomap_SetOrigin(obj, 0, 0);
    UIAutomap_SetDimensions(obj, winWidth, winHeight);

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

void ST_BuildWidgets(int player)
{
#define PADDING 2 // In fixed 320x200 units.

    hudstate_t* hud = hudStates + player;
    const uiwidgetgroupdef_t widgetGroupDefs[] = {
        { UWG_STATUSBAR,    ALIGN_BOTTOM },
        { UWG_MAPNAME,      ALIGN_BOTTOMLEFT },
        { UWG_BOTTOMLEFT,   ALIGN_BOTTOMLEFT,  UWGF_LEFTTORIGHT, PADDING },
        { UWG_BOTTOMLEFT2,  ALIGN_BOTTOMLEFT,  UWGF_LEFTTORIGHT, PADDING },
        { UWG_BOTTOMRIGHT,  ALIGN_BOTTOMRIGHT, UWGF_RIGHTTOLEFT, PADDING },
        { UWG_BOTTOM,       ALIGN_BOTTOM,      UWGF_VERTICAL|UWGF_RIGHTTOLEFT, PADDING },
        { UWG_TOP,          ALIGN_TOPLEFT,     UWGF_VERTICAL|UWGF_LEFTTORIGHT, PADDING },
        { UWG_COUNTERS,     ALIGN_LEFT,        UWGF_VERTICAL|UWGF_RIGHTTOLEFT, PADDING },
        { UWG_AUTOMAP,      ALIGN_TOPLEFT }
    };
    const uiwidgetdef_t widgetDefs[] = {
        { GUI_BOX,      UWG_STATUSBAR,      0,          SBarBackground_UpdateDimensions, SBarBackground_Drawer },
        { GUI_READYAMMO, UWG_STATUSBAR,     GF_STATUS,  SBarReadyAmmo_UpdateDimensions, SBarReadyAmmo_Drawer, ReadyAmmo_Ticker, &hud->sbarReadyammo },
        { GUI_HEALTH,   UWG_STATUSBAR,      GF_STATUS,  SBarHealth_UpdateDimensions, SBarHealth_Drawer, Health_Ticker, &hud->sbarHealth },
        { GUI_WEAPONSLOT, UWG_STATUSBAR,    0,          WeaponSlot_UpdateDimensions, WeaponSlot_Drawer, WeaponSlot_Ticker, &hud->sbarWeaponslots[0] },
        { GUI_WEAPONSLOT, UWG_STATUSBAR,    0,          WeaponSlot_UpdateDimensions, WeaponSlot_Drawer, WeaponSlot_Ticker, &hud->sbarWeaponslots[1] },
        { GUI_WEAPONSLOT, UWG_STATUSBAR,    0,          WeaponSlot_UpdateDimensions, WeaponSlot_Drawer, WeaponSlot_Ticker, &hud->sbarWeaponslots[2] },
        { GUI_WEAPONSLOT, UWG_STATUSBAR,    0,          WeaponSlot_UpdateDimensions, WeaponSlot_Drawer, WeaponSlot_Ticker, &hud->sbarWeaponslots[3] },
        { GUI_WEAPONSLOT, UWG_STATUSBAR,    0,          WeaponSlot_UpdateDimensions, WeaponSlot_Drawer, WeaponSlot_Ticker, &hud->sbarWeaponslots[4] },
        { GUI_WEAPONSLOT, UWG_STATUSBAR,    0,          WeaponSlot_UpdateDimensions, WeaponSlot_Drawer, WeaponSlot_Ticker, &hud->sbarWeaponslots[5] },
        { GUI_FRAGS,    UWG_STATUSBAR,      GF_STATUS,  SBarFrags_UpdateDimensions, SBarFrags_Drawer, SBarFrags_Ticker, &hud->sbarFrags },
        { GUI_FACE,     UWG_STATUSBAR,      0,          SBarFace_UpdateDimensions, SBarFace_Drawer, Face_Ticker, &hud->sbarFace },
        { GUI_ARMOR,    UWG_STATUSBAR,      GF_STATUS,  SBarArmor_UpdateDimensions, SBarArmor_Drawer, Armor_Ticker, &hud->sbarArmor },
        { GUI_KEYSLOT,  UWG_STATUSBAR,      0,          KeySlot_UpdateDimensions, KeySlot_Drawer, KeySlot_Ticker, &hud->sbarKeyslots[0] },
        { GUI_KEYSLOT,  UWG_STATUSBAR,      0,          KeySlot_UpdateDimensions, KeySlot_Drawer, KeySlot_Ticker, &hud->sbarKeyslots[1] },
        { GUI_KEYSLOT,  UWG_STATUSBAR,      0,          KeySlot_UpdateDimensions, KeySlot_Drawer, KeySlot_Ticker, &hud->sbarKeyslots[2] },
        { GUI_AMMO,     UWG_STATUSBAR,      GF_INDEX,   Ammo_UpdateDimensions, Ammo_Drawer, Ammo_Ticker, &hud->sbarAmmos[AT_CLIP] },
        { GUI_AMMO,     UWG_STATUSBAR,      GF_INDEX,   Ammo_UpdateDimensions, Ammo_Drawer, Ammo_Ticker, &hud->sbarAmmos[AT_SHELL] },
        { GUI_AMMO,     UWG_STATUSBAR,      GF_INDEX,   Ammo_UpdateDimensions, Ammo_Drawer, Ammo_Ticker, &hud->sbarAmmos[AT_CELL] },
        { GUI_AMMO,     UWG_STATUSBAR,      GF_INDEX,   Ammo_UpdateDimensions, Ammo_Drawer, Ammo_Ticker, &hud->sbarAmmos[AT_MISSILE] },
        { GUI_AMMO,     UWG_STATUSBAR,      GF_INDEX,   MaxAmmo_UpdateDimensions, MaxAmmo_Drawer, MaxAmmo_Ticker, &hud->sbarMaxammos[AT_CLIP] },
        { GUI_AMMO,     UWG_STATUSBAR,      GF_INDEX,   MaxAmmo_UpdateDimensions, MaxAmmo_Drawer, MaxAmmo_Ticker, &hud->sbarMaxammos[AT_SHELL] },
        { GUI_AMMO,     UWG_STATUSBAR,      GF_INDEX,   MaxAmmo_UpdateDimensions, MaxAmmo_Drawer, MaxAmmo_Ticker, &hud->sbarMaxammos[AT_CELL] },
        { GUI_AMMO,     UWG_STATUSBAR,      GF_INDEX,   MaxAmmo_UpdateDimensions, MaxAmmo_Drawer, MaxAmmo_Ticker, &hud->sbarMaxammos[AT_MISSILE] },
        { GUI_MAPNAME,  UWG_MAPNAME,        GF_FONTB,   MapName_UpdateDimensions, MapName_Drawer },
        { GUI_BOX,      UWG_BOTTOMLEFT,     0,          HealthIcon_UpdateDimensions, HealthIcon_Drawer },
        { GUI_HEALTH,   UWG_BOTTOMLEFT,     GF_FONTB,   Health_UpdateDimensions, Health_Drawer, Health_Ticker, &hud->health },
        { GUI_READYAMMOICON, UWG_BOTTOMLEFT,0,          ReadyAmmoIcon_UpdateDimensions, ReadyAmmoIcon_Drawer, ReadyAmmoIcon_Ticker, &hud->readyammoicon },
        { GUI_READYAMMO, UWG_BOTTOMLEFT,    GF_FONTB,   ReadyAmmo_UpdateDimensions, ReadyAmmo_Drawer, ReadyAmmo_Ticker, &hud->readyammo },
        { GUI_FRAGS,    UWG_BOTTOMLEFT2,    GF_FONTA,   Frags_UpdateDimensions, Frags_Drawer, Frags_Ticker, &hud->frags },
        { GUI_ARMOR,    UWG_BOTTOMRIGHT,    GF_FONTB,   Armor_UpdateDimensions, Armor_Drawer, Armor_Ticker, &hud->armor },
        { GUI_ARMORICON, UWG_BOTTOMRIGHT,   0,          ArmorIcon_UpdateDimensions, ArmorIcon_Drawer, ArmorIcon_Ticker, &hud->armoricon },
        { GUI_KEYS,     UWG_BOTTOMRIGHT,    0,          Keys_UpdateDimensions, Keys_Drawer, Keys_Ticker, &hud->keys },
        { GUI_FACE,     UWG_BOTTOM,         0,          Face_UpdateDimensions, Face_Drawer, Face_Ticker, &hud->face },
        { GUI_SECRETS,  UWG_COUNTERS,       GF_FONTA,   Secrets_UpdateDimensions, Secrets_Drawer, Secrets_Ticker, &hud->secrets },
        { GUI_ITEMS,    UWG_COUNTERS,       GF_FONTA,   Items_UpdateDimensions, Items_Drawer, Items_Ticker, &hud->items },
        { GUI_KILLS,    UWG_COUNTERS,       GF_FONTA,   Kills_UpdateDimensions, Kills_Drawer, Kills_Ticker, &hud->kills },
        { GUI_NONE }
    };
    size_t i;

    if(player < 0 || player >= MAXPLAYERS)
    {
        Con_Error("ST_BuildWidgets: Invalid player #%i.", player);
        exit(1); // Unreachable.
    }

    for(i = 0; i < sizeof(widgetGroupDefs)/sizeof(widgetGroupDefs[0]); ++i)
    {
        const uiwidgetgroupdef_t* def = &widgetGroupDefs[i];
        hud->widgetGroupIds[def->group] = GUI_CreateGroup(player, def->groupFlags, def->alignFlags, def->padding);
    }

    for(i = 0; widgetDefs[i].type != GUI_NONE; ++i)
    {
        const uiwidgetdef_t* def = &widgetDefs[i];
        uiwidgetid_t id = GUI_CreateWidget(def->type, player, FID(def->fontIdx), def->updateDimensions, def->drawer, def->ticker, def->typedata);
        UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[def->group]), GUI_FindObjectById(id));
    }

    hud->logWidgetId = GUI_CreateWidget(GUI_LOG, player, FID(GF_FONTA), UILog_UpdateDimensions, UILog_Drawer, UILog_Ticker, &hud->log);
    UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOP]), GUI_FindObjectById(hud->logWidgetId));

    hud->chatWidgetId = GUI_CreateWidget(GUI_CHAT, player, FID(GF_FONTA), UIChat_UpdateDimensions, UIChat_Drawer, NULL, &hud->chat);
    UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOP]), GUI_FindObjectById(hud->chatWidgetId));

    hud->automapWidgetId = GUI_CreateWidget(GUI_AUTOMAP, player, FID(GF_FONTA), UIAutomap_UpdateDimensions, UIAutomap_Drawer, UIAutomap_Ticker, &hud->automap);
    UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_AUTOMAP]), GUI_FindObjectById(hud->automapWidgetId));

#undef PADDING
}

void ST_Init(void)
{
    int i;
    ST_InitAutomapConfig();
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        hudstate_t* hud = &hudStates[i];
        ST_BuildWidgets(i);
        hud->inited = true;
    }
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

uiwidget_t* ST_UIChatForPlayer(int player)
{
    if(player >= 0 && player < MAXPLAYERS)
    {
        hudstate_t* hud = &hudStates[player];
        return GUI_FindObjectById(hud->chatWidgetId);
    }
    Con_Error("ST_UIChatForPlayer: Invalid player #%i.", player);
    exit(1); // Unreachable.
}

uiwidget_t* ST_UILogForPlayer(int player)
{
    if(player >= 0 && player < MAXPLAYERS)
    {
        hudstate_t* hud = &hudStates[player];
        return GUI_FindObjectById(hud->logWidgetId);
    }
    Con_Error("ST_UILogForPlayer: Invalid player #%i.", player);
    exit(1); // Unreachable.
}

uiwidget_t* ST_UIAutomapForPlayer(int player)
{
    if(player >= 0 && player < MAXPLAYERS)
    {
        hudstate_t* hud = &hudStates[player];
        return GUI_FindObjectById(hud->automapWidgetId);
    }
    Con_Error("ST_UIAutomapForPlayer: Invalid player #%i.", player);
    exit(1); // Unreachable.
}

int ST_ChatResponder(int player, event_t* ev)
{
    uiwidget_t* obj = ST_UIChatForPlayer(player);
    if(NULL != obj)
    {
        return UIChat_Responder(obj, ev);
    }
    return false;
}

boolean ST_ChatIsActive(int player)
{    
    uiwidget_t* obj = ST_UIChatForPlayer(player);
    if(NULL != obj)
    {
        return UIChat_IsActive(obj);
    }
    return false;
}

void ST_LogPost(int player, byte flags, const char* msg)
{
    uiwidget_t* obj = ST_UILogForPlayer(player);
    if(NULL == obj) return;

    UILog_Post(obj, flags, msg);
}

void ST_LogPostVisibilityChangeNotification(void)
{
    int i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        ST_LogPost(i, LMF_NOHIDE, !cfg.hudShown[HUD_LOG] ? MSGOFF : MSGON);
    }
}

void ST_LogRefresh(int player)
{
    uiwidget_t* obj = ST_UILogForPlayer(player);
    if(NULL == obj) return;

    UILog_Refresh(obj);
}

void ST_LogEmpty(int player)
{
    uiwidget_t* obj = ST_UILogForPlayer(player);
    if(NULL == obj) return;

    UILog_Empty(obj);
}

void ST_LogUpdateAlignment(void)
{
    int i, flags;
    uiwidget_t* obj;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        hudstate_t* hud = &hudStates[i];
        if(!hud->inited) continue;

        obj = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOP]);
        flags = UIWidget_Alignment(obj);
        flags &= ~(ALIGN_LEFT|ALIGN_RIGHT);
        if(cfg.msgAlign == 0)
            flags |= ALIGN_LEFT;
        else if(cfg.msgAlign == 2)
            flags |= ALIGN_RIGHT;
        UIWidget_SetAlignment(obj, flags);
    }
}

void ST_AutomapOpen(int player, boolean yes, boolean fast)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(NULL == obj) return;
    UIAutomap_Open(obj, yes, fast);
}

boolean ST_AutomapIsActive(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(NULL == obj) return false;
    return UIAutomap_Active(obj);
}

boolean ST_AutomapWindowObscures2(int player, const rectanglei_t* region)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(NULL == obj) return false;
    if(UIAutomap_Active(obj))
    {
        if(cfg.automapOpacity * ST_AutomapOpacity(player) >= ST_AUTOMAP_OBSCURE_TOLERANCE)
        {
            /*if(UIAutomap_Fullscreen(obj))
            {*/
                return true;
            /*}
            else
            {
                // We'll have to compare the dimensions.
                const int scrwidth  = Get(DD_WINDOW_WIDTH);
                const int scrheight = Get(DD_WINDOW_HEIGHT);
                const rectanglei_t* dims = UIWidget_Dimensions(obj);
                float fx = FIXXTOSCREENX(region->x);
                float fy = FIXYTOSCREENY(region->y);
                float fw = FIXXTOSCREENX(region->width);
                float fh = FIXYTOSCREENY(region->height);

                if(dims->x >= fx && dims->y >= fy && dims->width >= fw && dims->height >= fh)
                    return true;
            }*/
        }
    }
    return false;
}

boolean ST_AutomapWindowObscures(int player, int x, int y, int width, int height)
{
    rectanglei_t rect;
    rect.x = x;
    rect.y = y;
    rect.width  = width;
    rect.height = height;
    return ST_AutomapWindowObscures2(player, &rect);
}

void ST_AutomapClearPoints(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(NULL == obj) return;
    UIAutomap_ClearPoints(obj);
}

/**
 * Adds a marker at the specified X/Y location.
 */
int ST_AutomapAddPoint(int player, float x, float y, float z)
{
    static char buffer[20];
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    int newPoint;
    if(NULL == obj) return - 1;

    if(UIAutomap_PointCount(obj) == MAX_MAP_POINTS)
        return -1;

    newPoint = UIAutomap_AddPoint(obj, x, y, z);
    sprintf(buffer, "%s %d", AMSTR_MARKEDSPOT, newPoint);
    P_SetMessage(&players[player], buffer, false);

    return newPoint;
}

boolean ST_AutomapPointOrigin(int player, int point, float* x, float* y, float* z)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(NULL == obj) return false;
    return UIAutomap_PointOrigin(obj, point, x, y, z);
}

void ST_ToggleAutomapMaxZoom(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(NULL == obj) return;
    if(UIAutomap_SetZoomMax(obj, !UIAutomap_ZoomMax(obj)))
    {
        Con_Printf("Maximum zoom %s in automap.\n", UIAutomap_ZoomMax(obj)? "ON":"OFF");
    }
}

float ST_AutomapOpacity(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(NULL == obj) return 0;
    return UIAutomap_Opacity(obj);
}

void ST_SetAutomapCameraRotation(int player, boolean on)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(NULL == obj) return;
    UIAutomap_SetCameraRotation(obj, on);
}

void ST_ToggleAutomapPanMode(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(NULL == obj) return;
    if(UIAutomap_SetPanMode(obj, !UIAutomap_PanMode(obj)))
    {
        P_SetMessage(&players[player], (UIAutomap_PanMode(obj)? AMSTR_FOLLOWON : AMSTR_FOLLOWOFF), true);
    }
}

void ST_CycleAutomapCheatLevel(int player)
{
    if(player >= 0 && player < MAXPLAYERS)
    {
        hudstate_t* hud = &hudStates[player];
        ST_SetAutomapCheatLevel(player, (hud->automapCheatLevel + 1) % 3);
    }
}

void ST_SetAutomapCheatLevel(int player, int level)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(NULL == obj) return;
    setAutomapCheatLevel(obj, level);
}

void ST_RevealAutomap(int player, boolean on)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(NULL == obj) return;
    UIAutomap_SetReveal(obj, on);
}

boolean ST_AutomapHasReveal(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(NULL == obj) return false;
    return UIAutomap_Reveal(obj);
}

void ST_RebuildAutomap(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(NULL == obj) return;
    UIAutomap_Rebuild(obj);
}

int ST_AutomapCheatLevel(int player)
{
    if(player >=0 && player < MAXPLAYERS)
        return hudStates[player].automapCheatLevel;
    return 0;
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

D_CMD(ChatOpen)
{
    int player = CONSOLEPLAYER, destination = 0;
    uiwidget_t* obj;

    if(G_GetGameAction() == GA_QUIT)
    {
        return false;
    }

    obj = ST_UIChatForPlayer(player);
    if(NULL == obj)
    {
        return false;
    }

    if(argc == 2)
    {
        destination = UIChat_ParseDestination(argv[1]);
        if(destination < 0)
        {
            Con_Message("Invalid team number #%i (valid range: 0...%i).\n", destination, NUMTEAMS);
            return false;
        }
    }
    UIChat_SetDestination(obj, destination);
    UIChat_Activate(obj, true);
    return true;
}

D_CMD(ChatAction)
{
    int player = CONSOLEPLAYER;
    const char* cmd = argv[0] + 4;
    uiwidget_t* obj;

    if(G_GetGameAction() == GA_QUIT)
    {
        return false;
    }

    obj = ST_UIChatForPlayer(player);
    if(NULL == obj || !UIChat_IsActive(obj))
    {
        return false;
    }
    if(!stricmp(cmd, "complete")) // Send the message.
    {
        return UIChat_CommandResponder(obj, MCMD_SELECT);
    }
    else if(!stricmp(cmd, "cancel")) // Close chat.
    {
        return UIChat_CommandResponder(obj, MCMD_CLOSE);
    }
    else if(!stricmp(cmd, "delete"))
    {
        return UIChat_CommandResponder(obj, MCMD_DELETE);
    }
    return true;
}

D_CMD(ChatSendMacro)
{
    int player = CONSOLEPLAYER, macroId, destination = 0;
    uiwidget_t* obj;

    if(G_GetGameAction() == GA_QUIT)
        return false;

    if(argc < 2 || argc > 3)
    {
        Con_Message("Usage: %s (team) (macro number)\n", argv[0]);
        Con_Message("Send a chat macro to other player(s).\n"
                    "If (team) is omitted, the message will be sent to all players.\n");
        return true;
    }

    obj = ST_UIChatForPlayer(player);
    if(NULL == obj)
    {
        return false;
    }

    if(argc == 3)
    {
        destination = UIChat_ParseDestination(argv[1]);
        if(destination < 0)
        {
            Con_Message("Invalid team number #%i (valid range: 0...%i).\n", destination, NUMTEAMS);
            return false;
        }
    }

    macroId = UIChat_ParseMacroId(argc == 3? argv[2] : argv[1]);
    if(-1 == macroId)
    {
        Con_Message("Invalid macro id.\n");
        return false;
    }

    UIChat_Activate(obj, true);
    UIChat_SetDestination(obj, destination);
    UIChat_LoadMacro(obj, macroId);
    UIChat_CommandResponder(obj, MCMD_SELECT);
    UIChat_Activate(obj, false);
    return true;
}
