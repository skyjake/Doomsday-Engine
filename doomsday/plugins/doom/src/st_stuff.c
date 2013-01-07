/**\file st_stuff.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jdoom.h"

#include "dmu_lib.h"
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
    UWG_BOTTOM,
    UWG_BOTTOMLEFT,
    UWG_BOTTOMLEFT2,
    UWG_BOTTOMRIGHT,
    UWG_BOTTOMCENTER,
    UWG_TOPCENTER,
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

void updateViewWindow(void);
void unhideHUD(void);

int ST_ChatResponder(int player, event_t* ev);

static hudstate_t hudStates[MAXPLAYERS];

// Main bar left.
static patchid_t pStatusbar;

// 3 key-cards, 3 skulls.
static patchid_t pKeys[NUM_KEY_TYPES];

// Face status patches.
static patchid_t pFaces[ST_NUMFACES];

// Face background.
static patchid_t pFaceBackground[4];

 // Main bar right.
static patchid_t pArmsBackground;

// Weapon ownership patches.
static patchid_t pArms[6][2];

void ST_Register(void)
{
    C_VAR_FLOAT2( "hud-color-r", &cfg.hudColor[0], 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-color-g", &cfg.hudColor[1], 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-color-b", &cfg.hudColor[2], 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-color-a", &cfg.hudColor[3], 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-icon-alpha", &cfg.hudIconAlpha, 0, 0, 1, unhideHUD )
    C_VAR_INT(    "hud-patch-replacement", &cfg.hudPatchReplaceMode, 0, PRM_FIRST, PRM_LAST )
    C_VAR_FLOAT2( "hud-scale", &cfg.hudScale, 0, 0.1f, 1, unhideHUD )
    C_VAR_FLOAT(  "hud-timer", &cfg.hudTimer, 0, 0, 60 )

    // Displays
    C_VAR_BYTE2(  "hud-ammo", &cfg.hudShown[HUD_AMMO], 0, 0, 1, unhideHUD )
    C_VAR_BYTE2(  "hud-armor", &cfg.hudShown[HUD_ARMOR], 0, 0, 1, unhideHUD )
    C_VAR_BYTE2(  "hud-cheat-counter", &cfg.hudShownCheatCounters, 0, 0, 63, unhideHUD )
    C_VAR_FLOAT2( "hud-cheat-counter-scale", &cfg.hudCheatCounterScale, 0, .1f, 1, unhideHUD )
    C_VAR_BYTE2(  "hud-cheat-counter-show-mapopen", &cfg.hudCheatCounterShowWithAutomap, 0, 0, 1, unhideHUD )
    C_VAR_BYTE2(  "hud-face", &cfg.hudShown[HUD_FACE], 0, 0, 1, unhideHUD )
    C_VAR_BYTE(   "hud-face-ouchfix", &cfg.fixOuchFace, 0, 0, 1 )
    C_VAR_BYTE2(  "hud-frags", &cfg.hudShown[HUD_FRAGS], 0, 0, 1, unhideHUD )
    C_VAR_BYTE2(  "hud-health", &cfg.hudShown[HUD_HEALTH], 0, 0, 1, unhideHUD )
    C_VAR_BYTE2(  "hud-keys", &cfg.hudShown[HUD_KEYS], 0, 0, 1, unhideHUD )
    C_VAR_BYTE2(  "hud-keys-combine", &cfg.hudKeysCombine, 0, 0, 1, unhideHUD )

    C_VAR_FLOAT2( "hud-status-alpha", &cfg.statusbarOpacity, 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-status-icon-a", &cfg.statusbarCounterAlpha, 0, 0, 1, unhideHUD )
    C_VAR_FLOAT2( "hud-status-size", &cfg.statusbarScale, 0, 0.1f, 1, updateViewWindow )
    C_VAR_BYTE2(  "hud-status-weaponslots-ownedfix", &cfg.fixStatusbarOwnedWeapons, 0, 0, 1, unhideHUD )

    // Events.
    C_VAR_BYTE(   "hud-unhide-damage", &cfg.hudUnHide[HUE_ON_DAMAGE], 0, 0, 1 )
    C_VAR_BYTE(   "hud-unhide-pickup-ammo", &cfg.hudUnHide[HUE_ON_PICKUP_AMMO], 0, 0, 1 )
    C_VAR_BYTE(   "hud-unhide-pickup-armor", &cfg.hudUnHide[HUE_ON_PICKUP_ARMOR], 0, 0, 1 )
    C_VAR_BYTE(   "hud-unhide-pickup-health", &cfg.hudUnHide[HUE_ON_PICKUP_HEALTH], 0, 0, 1 )
    C_VAR_BYTE(   "hud-unhide-pickup-key", &cfg.hudUnHide[HUE_ON_PICKUP_KEY], 0, 0, 1 )
    C_VAR_BYTE(   "hud-unhide-pickup-powerup", &cfg.hudUnHide[HUE_ON_PICKUP_POWER], 0, 0, 1 )
    C_VAR_BYTE(   "hud-unhide-pickup-weapon", &cfg.hudUnHide[HUE_ON_PICKUP_WEAPON], 0, 0, 1 )

    C_CMD( "beginchat",       NULL,   ChatOpen )
    C_CMD( "chatcancel",      "",     ChatAction )
    C_CMD( "chatcomplete",    "",     ChatAction )
    C_CMD( "chatdelete",      "",     ChatAction )
    C_CMD( "chatsendmacro",   NULL,   ChatSendMacro )
}

static int headupDisplayMode(int player)
{
    return (cfg.screenBlocks < 10? 0 : cfg.screenBlocks - 10);
}

void SBarBackground_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
#define WIDTH       (ST_WIDTH)
#define HEIGHT      (ST_HEIGHT)
#define ORIGINX     ((int)(-WIDTH/2))
#define ORIGINY     ((int)(-HEIGHT * hud->showBar))

    const hudstate_t* hud = &hudStates[obj->player];
    float x = ORIGINX, y = ORIGINY, w = WIDTH, h = HEIGHT, armsBGX;
    int fullscreen = headupDisplayMode(obj->player);
    //const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarOpacity);
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarOpacity);
    patchinfo_t fbgInfo, armsInfo;
    boolean haveArms = false;
    float cw, cw2, ch;

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    if(!deathmatch)
    {
        haveArms = R_GetPatchInfo(pArmsBackground, &armsInfo);

        // Do not cut out the arms area if the graphic is "empty" (no color info).
        if(haveArms && armsInfo.flags.isEmpty)
            haveArms = false;

        if(haveArms)
        {
            armsBGX = ST_ARMSBGX + armsInfo.geometry.origin.x;
        }
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);

    DGL_SetPatch(pStatusbar, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
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
        // Up to faceback or ST_ARMS.
        w = haveArms? armsBGX : ST_FX;
        h = HEIGHT;
        cw = w / WIDTH;

        DGL_Begin(DGL_QUADS);
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
            if(haveArms && armsBGX + armsInfo.geometry.size.width < ST_FX)
            {
                int sectionWidth = armsBGX + armsInfo.geometry.size.width;
                x = ORIGINX + sectionWidth;
                y = ORIGINY;
                w = ST_FX - armsBGX - armsInfo.geometry.size.width;
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
            int sectionWidth = (haveArms? armsBGX + armsInfo.geometry.size.width : ST_FX);
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

    if(haveArms)
    {
        // Draw the ARMS background.
        DGL_SetPatch(armsInfo.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);

        x = ORIGINX + armsBGX;
        y = ORIGINY + armsInfo.geometry.origin.y;
        w = armsInfo.geometry.size.width;
        h = armsInfo.geometry.size.height;

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
    if(IS_NETGAME && R_GetPatchInfo(pFaceBackground[cfg.playerColor[obj->player%MAXPLAYERS]%4], &fbgInfo))
    {
        DGL_SetPatch(fbgInfo.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);

        x = ORIGINX + (ST_FX - ST_X);
        y = ORIGINY + (HEIGHT - 30);
        w = WIDTH - ST_FX - 141 - 2;
        h = HEIGHT - 3;
        cw = 1.f / fbgInfo.geometry.size.width;
        cw2 = ((float)fbgInfo.geometry.size.width - 1) / fbgInfo.geometry.size.width;
        ch = ((float)fbgInfo.geometry.size.height - 1) / fbgInfo.geometry.size.height;

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

#undef ORIGINY
#undef ORIGINX
#undef HEIGHT
#undef WIDTH
}

void SBarBackground_UpdateGeometry(uiwidget_t* obj)
{
    assert(obj);

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    Rect_SetWidthHeight(obj->geometry, ST_WIDTH  * cfg.statusbarScale,
                                       ST_HEIGHT * cfg.statusbarScale);
}

void ST_HUDUnHide(int player, hueevent_t ev)
{
    player_t* plr;

    if(player < 0 || player >= MAXPLAYERS)
    {
#if _DEBUG
        Con_Message("Warning:ST_HUDUnHide: Invalid player #%i, ignoring.", player);
#endif
        return;
    }
    if(ev < HUE_FORCE || ev > NUMHUDUNHIDEEVENTS)
    {
#if _DEBUG
        Con_Message("Warning:ST_HUDUnHide: Invalid event type %i, ignoring.", (int) ev);
#endif
        return;
    }

    plr = &players[player];
    if(!plr->plr->inGame) return;

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
    guidata_face_t* face = (guidata_face_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    angle_t badGuyAngle, diffAng;
    boolean doEvilGrin;
    int i;

    if(P_IsPaused() || !DD_IsSharpTick())
        return;

    if(face->priority < 10)
    {
        // Dead.
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
        {
            // Picking up a bonus.
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
            {
                // Evil grin if just picked up weapon.
                face->priority = 8;
                face->faceCount = ST_EVILGRINCOUNT;
                face->faceIndex = calcPainOffset(obj->player) + ST_EVILGRINOFFSET;
            }
        }
    }

    if(face->priority < 8)
    {
        if(plr->damageCount && plr->attacker && plr->attacker != plr->plr->mo)
        {
            // Being attacked.
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
                badGuyAngle = M_PointToAngle2(plr->plr->mo->origin, plr->attacker->origin);

                if(badGuyAngle > plr->plr->mo->angle)
                {
                    // Whether right or left.
                    diffAng = badGuyAngle - plr->plr->mo->angle;
                    i = diffAng > ANG180;
                }
                else
                {
                    // Whether left or right.
                    diffAng = plr->plr->mo->angle - badGuyAngle;
                    i = diffAng <= ANG180;
                }

                face->faceCount = ST_TURNCOUNT;
                face->faceIndex = calcPainOffset(obj->player);

                if(diffAng < ANG45)
                {
                    // Head-on.
                    face->faceIndex += ST_RAMPAGEOFFSET;
                }
                else if(i)
                {
                    // Turn face right.
                    face->faceIndex += ST_TURNOFFSET;
                }
                else
                {
                    // Turn face left.
                    face->faceIndex += ST_TURNOFFSET + 1;
                }
            }
        }
    }

    if(face->priority < 7)
    {
        // Getting hurt because of your own damn stupidity.
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
    {
        // Rapid firing.
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
    {
        // Invulnerability.
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

int ST_Responder(event_t* ev)
{
    int i, eaten = false;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        eaten = ST_ChatResponder(i, ev);
        if(eaten) break;
    }
    return eaten;
}

void ST_Ticker(timespan_t ticLength)
{
    const boolean isSharpTic = DD_IsSharpTick();
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
                }
                else if(hud->alpha < 1.0f)
                {
                    hud->alpha += 0.1f;
                }
            }
        }

        // The following is restricted to fixed 35 Hz ticks.
        if(isSharpTic && !P_IsPaused())
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
        else
        {
            if(hud->hideTics > 0)
                hud->hideTics--;
            if(hud->hideTics == 0 && cfg.hudTimer > 0 && hud->hideAmount < 1)
                hud->hideAmount += 0.1f;
        }
    }
}

void ReadyAmmo_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_readyammo_t* ammo = (guidata_readyammo_t*)obj->typedata;
    int player = obj->player;
    player_t* plr = &players[player];
    ammotype_t ammoType;
    boolean found;

    if(P_IsPaused() || !DD_IsSharpTick()) return;

    found = false;
    for(ammoType=0; ammoType < NUM_AMMO_TYPES && !found; ++ammoType)
    {
        if(!weaponInfo[plr->readyWeapon][plr->class_].mode[0].ammoType[ammoType])
            continue; // Weapon does not use this type of ammo.
        ammo->value = plr->ammo[ammoType].owned;
        found = true;
    }
    if(!found) // Weapon takes no ammo at all.
    {
        ammo->value = 1994; // Means "n/a".
    }
}

void SBarReadyAmmo_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX+ST_READYAMMOX)
#define Y                   (ORIGINY+ST_READYAMMOY)

    const guidata_readyammo_t* ammo = (guidata_readyammo_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = headupDisplayMode(obj->player);
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    //const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    char buf[20];

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(ammo->value == 1994) return;

    dd_snprintf(buf, 20, "%i", ammo->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->font);
    if(gameMode == doom_chex)
    {
        FR_SetColorAndAlpha(defFontRGB3[CR], defFontRGB3[CG], defFontRGB3[CB], textAlpha);
    }
    else
    {
        FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    }
    FR_DrawTextXY3(buf, X, Y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarReadyAmmo_UpdateGeometry(uiwidget_t* obj)
{
    const guidata_readyammo_t* ammo = (guidata_readyammo_t*)obj->typedata;
    Size2Raw textSize;
    char buf[20];

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(ammo->value == 1994) return;

    dd_snprintf(buf, 20, "%i", ammo->value);
    FR_SetFont(obj->font);
    FR_TextSize(&textSize, buf);
    textSize.width  *= cfg.statusbarScale;
    textSize.height *= cfg.statusbarScale;
    Rect_SetWidthHeight(obj->geometry, textSize.width, textSize.height);
}

void Ammo_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_ammo_t* ammo = (guidata_ammo_t*)obj->typedata;
    const player_t* plr = &players[obj->player];

    if(P_IsPaused() || !DD_IsSharpTick()) return;
    ammo->value = plr->ammo[ammo->ammotype].owned;
}

void Ammo_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define MAXDIGITS           (ST_AMMOWIDTH)

    static const Point2Raw ammoPos[NUM_AMMO_TYPES] = {
        { ST_AMMOX, ST_AMMOY },
        { ST_AMMOX, ST_AMMOY + (ST_AMMOHEIGHT * 1) },
        { ST_AMMOX, ST_AMMOY + (ST_AMMOHEIGHT * 3) },
        { ST_AMMOX, ST_AMMOY + (ST_AMMOHEIGHT * 2) }
    };
    const guidata_ammo_t* ammo = (guidata_ammo_t*)obj->typedata;
    const Point2Raw* loc = &ammoPos[ammo->ammotype];
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = headupDisplayMode(obj->player);
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    char buf[20];

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(ammo->value == 1994) return;

    dd_snprintf(buf, 20, "%i", ammo->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->font);
    FR_SetColorAndAlpha(defFontRGB3[CR], defFontRGB3[CG], defFontRGB3[CB], textAlpha);
    FR_DrawTextXY3(buf, ORIGINX+loc->x, ORIGINY+loc->y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef MAXDIGITS
#undef ORIGINY
#undef ORIGINX
}

void Ammo_UpdateGeometry(uiwidget_t* obj)
{
    assert(obj);

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    FR_SetFont(obj->font);
    Rect_SetWidthHeight(obj->geometry, (FR_CharWidth('0') * 3) * cfg.statusbarScale,
                                       FR_CharHeight('0') * cfg.statusbarScale);
}

void MaxAmmo_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_ammo_t* ammo = (guidata_ammo_t*)obj->typedata;
    const player_t* plr = &players[obj->player];

    if(P_IsPaused() || !DD_IsSharpTick()) return;
    ammo->value = plr->ammo[ammo->ammotype].max;
}

void MaxAmmo_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define MAXDIGITS           (ST_MAXAMMOWIDTH)

    static const Point2Raw ammoMaxPos[NUM_AMMO_TYPES] = {
        { ST_MAXAMMOX, ST_MAXAMMOY },
        { ST_MAXAMMOX, ST_MAXAMMOY + (ST_AMMOHEIGHT * 1) },
        { ST_MAXAMMOX, ST_MAXAMMOY + (ST_AMMOHEIGHT * 3) },
        { ST_MAXAMMOX, ST_MAXAMMOY + (ST_AMMOHEIGHT * 2) }
    };
    const guidata_ammo_t* ammo = (guidata_ammo_t*)obj->typedata;
    const Point2Raw* loc = &ammoMaxPos[ammo->ammotype];
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = headupDisplayMode(obj->player);
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    char buf[20];

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(ammo->value == 1994) return;

    dd_snprintf(buf, 20, "%i", ammo->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->font);
    FR_SetColorAndAlpha(defFontRGB3[CR], defFontRGB3[CG], defFontRGB3[CB], textAlpha);
    FR_DrawTextXY3(buf, ORIGINX+loc->x, ORIGINY+loc->y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef MAXDIGITS
#undef ORIGINY
#undef ORIGINX
}

void MaxAmmo_UpdateGeometry(uiwidget_t* obj)
{
    assert(obj);

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    FR_SetFont(obj->font);
    Rect_SetWidthHeight(obj->geometry, (FR_CharWidth('0') * 3) * cfg.statusbarScale,
                                       FR_CharHeight('0') * cfg.statusbarScale);
}

void Health_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_health_t* hlth = (guidata_health_t*) obj->typedata;
    const player_t* plr = &players[obj->player];

    if(P_IsPaused() || !DD_IsSharpTick()) return;
    hlth->value = plr->health;
}

void SBarHealth_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX+ST_HEALTHX)
#define Y                   (ORIGINY+ST_HEALTHY)

    const guidata_health_t* hlth = (guidata_health_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = headupDisplayMode(obj->player);
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    //const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    char buf[20];

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(hlth->value == 1994) return;

    dd_snprintf(buf, 20, "%i", hlth->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->font);
    if(gameMode == doom_chex)
    {
        FR_SetColorAndAlpha(defFontRGB3[CR], defFontRGB3[CG], defFontRGB3[CB], textAlpha);
    }
    else
    {
        FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    }
    FR_DrawTextXY3(buf, X, Y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);
    FR_DrawCharXY('%', X, Y);

    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarHealth_UpdateGeometry(uiwidget_t* obj)
{
    const guidata_health_t* hlth = (guidata_health_t*)obj->typedata;
    char buf[20];

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(hlth->value == 1994) return;

    dd_snprintf(buf, 20, "%i", hlth->value);
    FR_SetFont(obj->font);
    Rect_SetWidthHeight(obj->geometry, (FR_TextWidth(buf) + FR_CharWidth('%')) * cfg.statusbarScale,
                                       MAX_OF(FR_TextHeight(buf), FR_CharHeight('%')) * cfg.statusbarScale);
}

void Armor_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_armor_t* armor = (guidata_armor_t*)obj->typedata;
    const player_t* plr = &players[obj->player];

    if(P_IsPaused() || !DD_IsSharpTick()) return;
    armor->value = plr->armorPoints;
}

void SBarArmor_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX+ST_ARMORX)
#define Y                   (ORIGINY+ST_ARMORY)
#define MAXDIGITS           (ST_ARMORWIDTH)

    const guidata_armor_t* armor = (guidata_armor_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = headupDisplayMode(obj->player);
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    char buf[20];

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(armor->value == 1994) return;

    dd_snprintf(buf, 20, "%i", armor->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->font);
    if(gameMode == doom_chex)
    {
        FR_SetColorAndAlpha(defFontRGB3[CR], defFontRGB3[CG], defFontRGB3[CB], textAlpha);
    }
    else
    {
        FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    }
    FR_DrawTextXY3(buf, X, Y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);
    FR_DrawCharXY('%', X, Y);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef MAXDIGITS
#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarArmor_UpdateGeometry(uiwidget_t* obj)
{
    const guidata_armor_t* armor = (guidata_armor_t*)obj->typedata;
    char buf[20];

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(armor->value == 1994) return;

    dd_snprintf(buf, 20, "%i", armor->value);
    FR_SetFont(obj->font);
    Rect_SetWidthHeight(obj->geometry, (FR_TextWidth(buf) + FR_CharWidth('%')) * cfg.statusbarScale,
                                       MAX_OF(FR_TextHeight(buf), FR_CharHeight('%')) * cfg.statusbarScale);
}

void SBarFrags_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_frags_t* frags = (guidata_frags_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    int i;

    if(P_IsPaused() || !DD_IsSharpTick()) return;
    frags->value = 0;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(players[i].plr->inGame)
            frags->value += plr->frags[i] * (i != obj->player ? 1 : -1);
    }
}

void SBarFrags_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
#define ORIGINX             (-ST_WIDTH/2)
#define ORIGINY             (-ST_HEIGHT)
#define X                   (ORIGINX+ST_FRAGSX)
#define Y                   (ORIGINY+ST_FRAGSY)

    const guidata_frags_t* frags = (guidata_frags_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = headupDisplayMode(obj->player);
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    //const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    char buf[20];

    if(!deathmatch) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(frags->value == 1994) return;

    dd_snprintf(buf, 20, "%i", frags->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->font);
    if(gameMode == doom_chex)
    {
        FR_SetColorAndAlpha(defFontRGB3[CR], defFontRGB3[CG], defFontRGB3[CB], textAlpha);
    }
    else
    {
        FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    }
    FR_DrawTextXY3(buf, X, Y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef Y
#undef X
#undef ORIGINY
#undef ORIGINX
}

void SBarFrags_UpdateGeometry(uiwidget_t* obj)
{
    const guidata_frags_t* frags = (guidata_frags_t*)obj->typedata;
    Size2Raw textSize;
    char buf[20];

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(!deathmatch) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(frags->value == 1994) return;

    dd_snprintf(buf, 20, "%i", frags->value);
    FR_SetFont(obj->font);
    FR_TextSize(&textSize, buf);
    textSize.width  *= cfg.statusbarScale;
    textSize.height *= cfg.statusbarScale;
    Rect_SetWidthHeight(obj->geometry, textSize.width, textSize.height);
}

void SBarFace_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
#define ORIGINX (-ST_WIDTH/2)
#define ORIGINY (-ST_HEIGHT)

    const guidata_face_t* face = (guidata_face_t*)obj->typedata;
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = headupDisplayMode(obj->player);
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    if(face->faceIndex >= 0)
    {
        patchid_t patchId = pFaces[face->faceIndex];

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        if(offset) DGL_Translatef(offset->x, offset->y, 0);
        DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
        DGL_Translatef(0, yOffset, 0);

        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconAlpha);
        GL_DrawPatchXY2(patchId, ORIGINX+ST_FACESX, ORIGINY+ST_FACESY, ALIGN_TOPLEFT);

        DGL_Disable(DGL_TEXTURE_2D);
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }

#undef ORIGINY
#undef ORIGINX
}

void SBarFace_UpdateGeometry(uiwidget_t* obj)
{
    const guidata_face_t* face = (guidata_face_t*)obj->typedata;
    patchinfo_t info;

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(!R_GetPatchInfo(pFaces[face->faceIndex%ST_NUMFACES], &info)) return;

    Rect_SetWidthHeight(obj->geometry, info.geometry.size.width  * cfg.statusbarScale,
                                       info.geometry.size.height * cfg.statusbarScale);
}

void KeySlot_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_keyslot_t* kslt = (guidata_keyslot_t*)obj->typedata;
    const player_t* plr = &players[obj->player];

    if(P_IsPaused() || !DD_IsSharpTick()) return;

    if(plr->keys[kslt->keytypeA] || plr->keys[kslt->keytypeB])
    {
        kslt->patchId = pKeys[plr->keys[kslt->keytypeB]? kslt->keytypeB : kslt->keytypeA];
    }
    else
    {
        kslt->patchId = 0;
    }
    if(!cfg.hudKeysCombine && plr->keys[kslt->keytypeA] && plr->keys[kslt->keytypeB])
    {
        kslt->patchId2 = pKeys[kslt->keytypeA];
    }
    else
    {
        kslt->patchId2 = 0;
    }
}

void KeySlot_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
#define ORIGINX (-ST_WIDTH/2)
#define ORIGINY (-ST_HEIGHT)

    static const Point2Raw elements[] = {
        { ORIGINX+ST_KEY0X, ORIGINY+ST_KEY0Y },
        { ORIGINX+ST_KEY1X, ORIGINY+ST_KEY1Y },
        { ORIGINX+ST_KEY2X, ORIGINY+ST_KEY2Y }
    };
    guidata_keyslot_t* kslt = (guidata_keyslot_t*)obj->typedata;
    const Point2Raw* loc = &elements[kslt->slot];
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = headupDisplayMode(obj->player);
    const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    int comboOffset = (kslt->patchId2 != 0? -1 : 0);

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(kslt->patchId == 0 && kslt->patchId2 == 0) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, iconAlpha);

    GL_DrawPatchXY(kslt->patchId, loc->x + comboOffset, loc->y + comboOffset);
    if(kslt->patchId2 != 0)
    {
        GL_DrawPatchXY(kslt->patchId2, loc->x - comboOffset, loc->y - comboOffset);
    }

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef ORIGINY
#undef ORIGINX
}

void KeySlot_UpdateGeometry(uiwidget_t* obj)
{
    guidata_keyslot_t* kslt = (guidata_keyslot_t*)obj->typedata;
    patchinfo_t info;

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(kslt->patchId == 0 && kslt->patchId2 == 0) return;
    if(!R_GetPatchInfo(kslt->patchId, &info)) return;

    Rect_SetWidthHeight(obj->geometry, info.geometry.size.width,
                                       info.geometry.size.height);

    if(kslt->patchId2 != 0 && R_GetPatchInfo(kslt->patchId, &info))
    {
        info.geometry.origin.x = info.geometry.origin.y = 2; // Combine offset.
        Rect_UniteRaw(obj->geometry, &info.geometry);
    }

    Rect_SetWidthHeight(obj->geometry, Rect_Width(obj->geometry)  * cfg.statusbarScale,
                                       Rect_Height(obj->geometry) * cfg.statusbarScale);
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
    guidata_weaponslot_t* wpn = (guidata_weaponslot_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    boolean used = false;

    if(P_IsPaused() || !DD_IsSharpTick()) return;

    if(cfg.fixStatusbarOwnedWeapons)
    {
        // Does the player own any weapon bound to this slot?
        countownedweaponsinslot_params_t params;
        params.player = obj->player;
        params.numOwned = 0;
        P_IterateWeaponsBySlot(wpn->slot, false, countOwnedWeaponsInSlot, &params);
        used = params.numOwned > 0;
    }
    else
    {
        // Does the player own the originally hardwired weapon to this slot?
        used = plr->weapons[wpn->slot].owned;
    }
    wpn->patchId = pArms[wpn->slot-1][used?1:0];
}

void WeaponSlot_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
#define ORIGINX (-ST_WIDTH/2)
#define ORIGINY (-ST_HEIGHT)

    static const Point2Raw elements[] = {
        { ORIGINX+ST_ARMSX,                     ORIGINY+ST_ARMSY },
        { ORIGINX+ST_ARMSX + ST_ARMSXSPACE,     ORIGINY+ST_ARMSY },
        { ORIGINX+ST_ARMSX + ST_ARMSXSPACE*2,   ORIGINY+ST_ARMSY },
        { ORIGINX+ST_ARMSX,                     ORIGINY+ST_ARMSY + ST_ARMSYSPACE },
        { ORIGINX+ST_ARMSX + ST_ARMSXSPACE,     ORIGINY+ST_ARMSY + ST_ARMSYSPACE },
        { ORIGINX+ST_ARMSX + ST_ARMSXSPACE*2,   ORIGINY+ST_ARMSY + ST_ARMSYSPACE },
    };
    guidata_weaponslot_t* wpns = (guidata_weaponslot_t*)obj->typedata;
    const Point2Raw* element = &elements[wpns->slot-1];
    const hudstate_t* hud = &hudStates[obj->player];
    int yOffset = ST_HEIGHT*(1-hud->showBar);
    int fullscreen = headupDisplayMode(obj->player);
    const float textAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);
    //const float iconAlpha = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.statusbarCounterAlpha);

    if(deathmatch) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.statusbarScale, cfg.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, textAlpha);

    FR_SetFont(obj->font);
    if(gameMode == doom_chex)
    {
        FR_SetColorAndAlpha(defFontRGB3[CR], defFontRGB3[CG], defFontRGB3[CB], textAlpha);
    }
    else
    {
        FR_SetColorAndAlpha(defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB], textAlpha);
    }
    WI_DrawPatch3(wpns->patchId, Hu_ChoosePatchReplacement(cfg.hudPatchReplaceMode, wpns->patchId),
                  element, ALIGN_TOPLEFT, 0, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef ORIGINY
#undef ORIGINX
}

void WeaponSlot_UpdateGeometry(uiwidget_t* obj)
{
    guidata_weaponslot_t* wpns = (guidata_weaponslot_t*)obj->typedata;
    const char* text = Hu_ChoosePatchReplacement(cfg.hudPatchReplaceMode, wpns->patchId);
    patchinfo_t info;

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(deathmatch) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(!text && !R_GetPatchInfo(wpns->patchId, &info)) return;

    if(text)
    {
        Size2Raw textSize;
        FR_SetFont(obj->font);
        FR_TextSize(&textSize, text);
        Rect_SetWidthHeight(obj->geometry, textSize.width  * cfg.statusbarScale,
                                           textSize.height * cfg.statusbarScale);
        return;
    }

    R_GetPatchInfo(wpns->patchId, &info);
    Rect_SetWidthHeight(obj->geometry, info.geometry.size.width  * cfg.statusbarScale,
                                       info.geometry.size.height * cfg.statusbarScale);
}

void ST_drawHUDSprite(int sprite, float x, float y, hotloc_t hotspot,
    float scale, float alpha, boolean flip, int* drawnWidth, int* drawnHeight)
{
    spriteinfo_t info;

    if(!(alpha > 0)) return;

    alpha = MINMAX_OF(0.f, alpha, 1.f);
    R_GetSpriteInfo(sprite, 0, &info);

    switch(hotspot)
    {
    case HOT_BRIGHT:
        y -= info.geometry.size.height * scale;
        // Fall through.
    case HOT_TRIGHT:
        x -= info.geometry.size.width * scale;
        break;

    case HOT_BLEFT:
        y -= info.geometry.size.height * scale;
        break;
    default: break;
    }

    DGL_SetPSprite(info.material);
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(1, 1, 1, alpha);
    DGL_Begin(DGL_QUADS);
        DGL_TexCoord2f(0, flip * info.texCoord[0], 0);
        DGL_Vertex2f(x, y);

        DGL_TexCoord2f(0, !flip * info.texCoord[0], 0);
        DGL_Vertex2f(x + info.geometry.size.width * scale, y);

        DGL_TexCoord2f(0, !flip * info.texCoord[0], info.texCoord[1]);
        DGL_Vertex2f(x + info.geometry.size.width * scale, y + info.geometry.size.height * scale);

        DGL_TexCoord2f(0, flip * info.texCoord[0], info.texCoord[1]);
        DGL_Vertex2f(x, y + info.geometry.size.height * scale);
    DGL_End();

    DGL_Disable(DGL_TEXTURE_2D);

    if(drawnWidth)  *drawnWidth  = info.geometry.size.width  * scale;
    if(drawnHeight) *drawnHeight = info.geometry.size.height * scale;
}

void ST_HUDSpriteSize(int sprite, float scale, int* width, int* height)
{
    spriteinfo_t info;
    if(!width && !height) return;
    if(!R_GetSpriteInfo(sprite, 0, &info)) return;

    if(width)  *width  = info.geometry.size.width  * scale;
    if(height) *height = info.geometry.size.height * scale;
}

void Frags_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_frags_t* frags = (guidata_frags_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    int i;

    if(P_IsPaused() || !DD_IsSharpTick()) return;

    frags->value = 0;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(players[i].plr->inGame)
            frags->value += plr->frags[i] * (i != obj->player ? 1 : -1);
    }
}

void Frags_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
    const guidata_frags_t* frags = (guidata_frags_t*)obj->typedata;
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    char buf[20];

    if(!cfg.hudShown[HUD_FRAGS]) return;
    if(!deathmatch) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    sprintf(buf, "FRAGS:%i", frags->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->font);
    FR_SetColorAndAlpha(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    FR_DrawTextXY(buf, 0, 0);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void Frags_UpdateGeometry(uiwidget_t* obj)
{
    const guidata_frags_t* frags = (guidata_frags_t*)obj->typedata;
    Size2Raw textSize;
    char buf[20];

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(!cfg.hudShown[HUD_FRAGS]) return;
    if(!deathmatch) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    sprintf(buf, "FRAGS:%i", frags->value);
    FR_SetFont(obj->font);
    FR_TextSize(&textSize, buf);
    textSize.width  *= cfg.hudScale;
    textSize.height *= cfg.hudScale;
    Rect_SetWidthHeight(obj->geometry, textSize.width, textSize.height);
}

void Health_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
    guidata_health_t* hlth = (guidata_health_t*)obj->typedata;
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    char buf[20];

    if(!cfg.hudShown[HUD_HEALTH]) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    sprintf(buf, "%i%%", hlth->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->font);
    FR_SetColorAndAlpha(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    FR_DrawTextXY(buf, 0, 0);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void Health_UpdateGeometry(uiwidget_t* obj)
{
    guidata_health_t* hlth = (guidata_health_t*)obj->typedata;
    Size2Raw textSize;
    char buf[20];

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(!cfg.hudShown[HUD_HEALTH]) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    sprintf(buf, "%i%%", hlth->value);
    FR_SetFont(obj->font);
    FR_TextSize(&textSize, buf);
    textSize.width  *= cfg.hudScale;
    textSize.height *= cfg.hudScale;
    Rect_SetWidthHeight(obj->geometry, textSize.width, textSize.height);
}

void HealthIcon_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;

    if(!cfg.hudShown[HUD_HEALTH]) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);

    ST_drawHUDSprite(SPR_STIM, 0, 0, HOT_TLEFT, 1, iconAlpha, false, NULL, NULL);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void HealthIcon_UpdateGeometry(uiwidget_t* obj)
{
    Size2Raw iconSize;
    assert(obj);
    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(!cfg.hudShown[HUD_HEALTH]) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    ST_HUDSpriteSize(SPR_STIM, 1, &iconSize.width, &iconSize.height);
    iconSize.width  *= cfg.hudScale;
    iconSize.height *= cfg.hudScale;
    Rect_SetWidthHeight(obj->geometry, iconSize.width, iconSize.height);
}

void ReadyAmmo_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
    guidata_readyammo_t* ammo = (guidata_readyammo_t*)obj->typedata;
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    char buf[20];

    if(!cfg.hudShown[HUD_AMMO]) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(ammo->value == 1994) return;

    sprintf(buf, "%i", ammo->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->font);
    FR_SetColorAndAlpha(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    FR_DrawTextXY(buf, 0, 0);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void ReadyAmmo_UpdateGeometry(uiwidget_t* obj)
{
    guidata_readyammo_t* ammo = (guidata_readyammo_t*)obj->typedata;
    Size2Raw textSize;
    char buf[20];

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(!cfg.hudShown[HUD_AMMO]) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(ammo->value == 1994) return;

    sprintf(buf, "%i", ammo->value);
    FR_SetFont(obj->font);
    FR_TextSize(&textSize, buf);
    textSize.width  *= cfg.hudScale;
    textSize.height *= cfg.hudScale;
    Rect_SetWidthHeight(obj->geometry, textSize.width, textSize.height);
}

void ReadyAmmoIcon_Ticker(uiwidget_t* obj, timespan_t ticLength)
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

    if(P_IsPaused() || !DD_IsSharpTick()) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK)) return;

    icon->sprite = -1;
    for(ammoType = 0; ammoType < NUM_AMMO_TYPES; ++ammoType)
    {
        if(!weaponInfo[plr->readyWeapon][plr->class_].mode[0].ammoType[ammoType])
            continue;
        icon->sprite = ammoSprite[ammoType];
        break;
    }
}

void ReadyAmmoIcon_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
    guidata_readyammoicon_t* icon = (guidata_readyammoicon_t*)obj->typedata;
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;
    float scale;

    if(!cfg.hudShown[HUD_AMMO]) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    if(icon->sprite < 0) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);

    scale = (icon->sprite == SPR_ROCK? .72f : 1);
    ST_drawHUDSprite(icon->sprite, 0, 0, HOT_TLEFT, scale, iconAlpha, false, NULL, NULL);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void ReadyAmmoIcon_UpdateGeometry(uiwidget_t* obj)
{
    guidata_readyammoicon_t* icon = (guidata_readyammoicon_t*)obj->typedata;
    Size2Raw iconSize;
    float scale;

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(!cfg.hudShown[HUD_AMMO]) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    if(icon->sprite < 0) return;

    scale = (icon->sprite == SPR_ROCK? .72f : 1);
    ST_HUDSpriteSize(icon->sprite, scale, &iconSize.width, &iconSize.height);
    iconSize.width  *= cfg.hudScale;
    iconSize.height *= cfg.hudScale;
    Rect_SetWidthHeight(obj->geometry, iconSize.width, iconSize.height);
}

void Face_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
#define EXTRA_SCALE         .7f

    const guidata_face_t* face = (guidata_face_t*)obj->typedata;
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;
    patchinfo_t bgInfo;
    patchid_t pFace;
    int x, y;

    if(!cfg.hudShown[HUD_FACE]) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    pFace = pFaces[face->faceIndex%ST_NUMFACES];
    if(!pFace) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(EXTRA_SCALE * cfg.hudScale, EXTRA_SCALE * cfg.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(1, 1, 1, iconAlpha);
    x = -(SCREENWIDTH/2 - ST_FACESX);
    y = -1;
    if(R_GetPatchInfo(pFaceBackground[cfg.playerColor[obj->player]], &bgInfo))
    {
        if(IS_NETGAME)
        {
            GL_DrawPatchXY(bgInfo.id, 0, 0);
        }
        x += bgInfo.geometry.size.width/2;
    }
    GL_DrawPatchXY(pFace, x, y);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef EXTRA_SCALE
}

void Face_UpdateGeometry(uiwidget_t* obj)
{
#define EXTRA_SCALE         .7f

    const guidata_face_t* face = (guidata_face_t*)obj->typedata;
    patchinfo_t info;
    patchid_t pFace;

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(!cfg.hudShown[HUD_FACE]) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    pFace = pFaces[face->faceIndex%ST_NUMFACES];
    if(!pFace) return;

    if(R_GetPatchInfo(pFaceBackground[cfg.playerColor[obj->player]], &info) ||
       R_GetPatchInfo(pFace, &info))
    {
        Rect_SetWidthHeight(obj->geometry, info.geometry.size.width  * EXTRA_SCALE * cfg.hudScale,
                                           info.geometry.size.height * EXTRA_SCALE * cfg.hudScale);
    }
#undef EXTRA_SCALE
}

void Armor_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
    guidata_armor_t* armor = (guidata_armor_t*)obj->typedata;
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    char buf[20];

    if(!cfg.hudShown[HUD_ARMOR]) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(armor->value == 1994) return;

    dd_snprintf(buf, 20, "%i%%", armor->value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->font);
    FR_SetColorAndAlpha(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    FR_DrawTextXY(buf, 0, 0);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void Armor_UpdateGeometry(uiwidget_t* obj)
{
    guidata_armor_t* armor = (guidata_armor_t*)obj->typedata;
    Size2Raw textSize;
    char buf[20];

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(!cfg.hudShown[HUD_ARMOR]) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(armor->value == 1994) return;

    dd_snprintf(buf, 20, "%i%%", armor->value);

    FR_SetFont(obj->font);
    FR_TextSize(&textSize, buf);
    textSize.width  *= cfg.hudScale;
    textSize.height *= cfg.hudScale;
    Rect_SetWidthHeight(obj->geometry, textSize.width, textSize.height);
}

void ArmorIcon_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_armoricon_t* icon = (guidata_armoricon_t*)obj->typedata;
    const player_t* plr = &players[obj->player];

    if(P_IsPaused() || !DD_IsSharpTick()) return;
    icon->sprite = (plr->armorType == 2 ? SPR_ARM2 : SPR_ARM1);
}

void ArmorIcon_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
    guidata_armoricon_t* icon = (guidata_armoricon_t*)obj->typedata;
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;

    if(!cfg.hudShown[HUD_ARMOR]) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    if(icon->sprite < 0) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);

    ST_drawHUDSprite(icon->sprite, 0, 0, HOT_TLEFT, 1, iconAlpha, false, NULL, NULL);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void ArmorIcon_UpdateGeometry(uiwidget_t* obj)
{
    guidata_armoricon_t* icon = (guidata_armoricon_t*)obj->typedata;
    Size2Raw iconSize;

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(!cfg.hudShown[HUD_ARMOR]) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    if(icon->sprite < 0) return;

    ST_HUDSpriteSize(icon->sprite, 1, &iconSize.width, &iconSize.height);
    iconSize.width  *= cfg.hudScale;
    iconSize.height *= cfg.hudScale;
    Rect_SetWidthHeight(obj->geometry, iconSize.width, iconSize.height);
}

void Keys_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_keys_t* keys = (guidata_keys_t*)obj->typedata;
    const player_t* plr = &players[obj->player];
    int i;

    if(P_IsPaused() || !DD_IsSharpTick()) return;
    for(i = 0; i < NUM_KEY_TYPES; ++i)
    {
        keys->keyBoxes[i] = plr->keys[i];
    }
}

void Keys_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
#define EXTRA_SCALE     .75f

    static int keyPairs[3][2] = {
        { KT_REDCARD, KT_REDSKULL },
        { KT_YELLOWCARD, KT_YELLOWSKULL },
        { KT_BLUECARD, KT_BLUESKULL }
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
    //const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;
    int i, x, numDrawnKeys = 0;

    if(!cfg.hudShown[HUD_KEYS]) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(EXTRA_SCALE * cfg.hudScale, EXTRA_SCALE * cfg.hudScale, 1);

    x = 0;
    for(i = 0; i < NUM_KEY_TYPES; ++i)
    {
        boolean shown = true;

        if(!keys->keyBoxes[i]) continue;

        if(cfg.hudKeysCombine)
        {
            int j;
            for(j = 0; j < 3; ++j)
            {
                if(keyPairs[j][0] == i && keys->keyBoxes[keyPairs[j][0]] && keys->keyBoxes[keyPairs[j][1]])
                {
                    shown = false;
                    break;
                }
            }
        }

        if(shown)
        {
            int w, h, spr = keyIcons[i];
            ST_drawHUDSprite(spr, x, 0, HOT_TLEFT, 1, iconAlpha, false, &w, &h);
            x += w + 2;
            numDrawnKeys++;
        }
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef EXTRA_SCALE
}

void Keys_UpdateGeometry(uiwidget_t* obj)
{
#define EXTRA_SCALE     .75f

    static int keyPairs[3][2] = {
        { KT_REDCARD, KT_REDSKULL },
        { KT_YELLOWCARD, KT_YELLOWSKULL },
        { KT_BLUECARD, KT_BLUESKULL }
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
    RectRaw iconGeometry;
    int i;

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(!cfg.hudShown[HUD_KEYS]) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;

    iconGeometry.origin.x = iconGeometry.origin.y = 0;
    for(i = 0; i < NUM_KEY_TYPES; ++i)
    {
        boolean shown = true;

        if(!keys->keyBoxes[i]) continue;

        if(cfg.hudKeysCombine)
        {
            int j;
            for(j = 0; j < 3; ++j)
            {
                if(keyPairs[j][0] == i && keys->keyBoxes[keyPairs[j][0]] && keys->keyBoxes[keyPairs[j][1]])
                {
                    shown = false;
                    break;
                }
            }
        }

        if(shown)
        {
            int spr = keyIcons[i];
            ST_HUDSpriteSize(spr, 1, &iconGeometry.size.width, &iconGeometry.size.height);

            Rect_UniteRaw(obj->geometry, &iconGeometry);

            iconGeometry.origin.x += iconGeometry.size.width + 2;
        }
    }

    Rect_SetWidthHeight(obj->geometry, Rect_Width(obj->geometry)  * EXTRA_SCALE * cfg.hudScale,
                                       Rect_Height(obj->geometry) * EXTRA_SCALE * cfg.hudScale);

#undef EXTRA_SCALE
}

void Kills_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_kills_t* kills = (guidata_kills_t*)obj->typedata;
    const player_t* plr = &players[obj->player];

    if(P_IsPaused() || !DD_IsSharpTick()) return;
    kills->value = plr->killCount;
}

void Kills_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
    guidata_kills_t* kills = (guidata_kills_t*)obj->typedata;
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    char buf[40], tmp[20];

    if(!(cfg.hudShownCheatCounters & (CCH_KILLS | CCH_KILLS_PRCNT))) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(cfg.hudCheatCounterShowWithAutomap && !ST_AutomapIsActive(obj->player)) return;
    if(kills->value == 1994) return;

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
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.hudCheatCounterScale, cfg.hudCheatCounterScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->font);
    FR_SetColorAndAlpha(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    FR_DrawTextXY(buf, 0, 0);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void Kills_UpdateGeometry(uiwidget_t* obj)
{
    guidata_kills_t* kills = (guidata_kills_t*)obj->typedata;
    char buf[40], tmp[20];
    Size2Raw textSize;

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(!(cfg.hudShownCheatCounters & (CCH_KILLS | CCH_KILLS_PRCNT))) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(cfg.hudCheatCounterShowWithAutomap && !ST_AutomapIsActive(obj->player)) return;
    if(kills->value == 1994) return;

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

    FR_SetFont(obj->font);
    FR_TextSize(&textSize, buf);
    Rect_SetWidthHeight(obj->geometry, .5f + textSize.width  * cfg.hudCheatCounterScale,
                                       .5f + textSize.height * cfg.hudCheatCounterScale);
}

void Items_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_items_t* items = (guidata_items_t*)obj->typedata;
    const player_t* plr = &players[obj->player];

    if(P_IsPaused() || !DD_IsSharpTick()) return;
    items->value = plr->itemCount;
}

void Items_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
    guidata_items_t* items = (guidata_items_t*)obj->typedata;
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    char buf[40], tmp[20];

    if(!(cfg.hudShownCheatCounters & (CCH_ITEMS | CCH_ITEMS_PRCNT))) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(cfg.hudCheatCounterShowWithAutomap && !ST_AutomapIsActive(obj->player)) return;
    if(items->value == 1994) return;

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
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.hudCheatCounterScale, cfg.hudCheatCounterScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->font);
    FR_SetColorAndAlpha(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    FR_DrawTextXY(buf, 0, 0);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void Items_UpdateGeometry(uiwidget_t* obj)
{
    guidata_items_t* items = (guidata_items_t*)obj->typedata;
    Size2Raw textSize;
    char buf[40], tmp[20];

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(!(cfg.hudShownCheatCounters & (CCH_ITEMS | CCH_ITEMS_PRCNT))) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(cfg.hudCheatCounterShowWithAutomap && !ST_AutomapIsActive(obj->player)) return;
    if(items->value == 1994) return;

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

    FR_SetFont(obj->font);
    FR_TextSize(&textSize, buf);
    Rect_SetWidthHeight(obj->geometry, .5f + textSize.width  * cfg.hudCheatCounterScale,
                                       .5f + textSize.height * cfg.hudCheatCounterScale);
}

void Secrets_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
    guidata_secrets_t* scrt = (guidata_secrets_t*)obj->typedata;
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    char buf[40], tmp[20];

    if(!(cfg.hudShownCheatCounters & (CCH_SECRETS | CCH_SECRETS_PRCNT))) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(cfg.hudCheatCounterShowWithAutomap && !ST_AutomapIsActive(obj->player)) return;
    if(scrt->value == 1994) return;

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
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.hudCheatCounterScale, cfg.hudCheatCounterScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(obj->font);
    FR_SetColorAndAlpha(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    FR_DrawTextXY(buf, 0, 0);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void Secrets_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_secrets_t* scrt = (guidata_secrets_t*)obj->typedata;
    const player_t* plr = &players[obj->player];

    if(P_IsPaused() || !DD_IsSharpTick()) return;
    scrt->value = plr->secretCount;
}

void Secrets_UpdateGeometry(uiwidget_t* obj)
{
    guidata_secrets_t* scrt = (guidata_secrets_t*)obj->typedata;
    char buf[40], tmp[20];
    Size2Raw textSize;

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(!(cfg.hudShownCheatCounters & (CCH_SECRETS | CCH_SECRETS_PRCNT))) return;
    if(ST_AutomapIsActive(obj->player) && cfg.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[obj->player].plr->mo) && Get(DD_PLAYBACK)) return;
    if(cfg.hudCheatCounterShowWithAutomap && !ST_AutomapIsActive(obj->player)) return;
    if(scrt->value == 1994) return;

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

    FR_SetFont(obj->font);
    FR_TextSize(&textSize, buf);
    Rect_SetWidthHeight(obj->geometry, .5f + textSize.width  * cfg.hudCheatCounterScale,
                                       .5f + textSize.height * cfg.hudCheatCounterScale);
}

void MapName_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
    const float scale = .75f;
    const float textAlpha = uiRendState->pageAlpha;
    const patchid_t patch = P_FindMapTitlePatch(gameEpisode, gameMap);
    const char* text = Hu_ChoosePatchReplacement2(PRM_ALLOW_TEXT, patch, P_GetMapNiceName());
    assert(obj->type == GUI_MAPNAME);

    if(!text && 0 == patch) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(scale, scale, 1);

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, textAlpha);
    FR_SetFont(obj->font);
    FR_SetColorAndAlpha(cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);

    WI_DrawPatchXY3(patch, text, 0, 0, ALIGN_TOPLEFT, 0, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void MapName_UpdateGeometry(uiwidget_t* obj)
{
    const patchid_t patch = P_FindMapTitlePatch(gameEpisode, gameMap);
    const char* text = Hu_ChoosePatchReplacement2(PRM_ALLOW_TEXT, patch, P_GetMapNiceName());
    const float scale = .75f;
    patchinfo_t info;
    assert(obj && obj->type == GUI_MAPNAME);

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(!text && 0 == patch) return;

    if(text)
    {
        Size2Raw textSize;
        FR_SetFont(obj->font);
        FR_TextSize(&textSize, text);
        Rect_SetWidthHeight(obj->geometry, textSize.width * scale, textSize.height * scale);
        return;
    }

    R_GetPatchInfo(patch, &info);
    Rect_SetWidthHeight(obj->geometry, info.geometry.size.width  * scale,
                                       info.geometry.size.height * scale);
}

static void drawUIWidgetsForPlayer(player_t* plr)
{
#define DISPLAY_BORDER      (2) /// Units in fixed 320x200 screen space.

    const int playerNum = plr - players;
    const int displayMode = headupDisplayMode(playerNum);
    hudstate_t* hud = hudStates + playerNum;
    Size2Raw size, portSize;
    uiwidget_t* obj;
    float scale;
    assert(plr);

    R_ViewPortSize(playerNum, &portSize);

    // The automap is drawn in a viewport scaled coordinate space (of viewwindow dimensions).
    obj = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_AUTOMAP]);
    UIWidget_SetOpacity(obj, ST_AutomapOpacity(playerNum));
    UIWidget_SetMaximumSize(obj, &portSize);
    GUI_DrawWidgetXY(obj, 0, 0);

    // The rest of the UI is drawn in a fixed 320x200 coordinate space.
    // Determine scale factors.
    R_ChooseAlignModeAndScaleFactor(&scale, SCREENWIDTH, SCREENHEIGHT,
        portSize.width, portSize.height, SCALEMODE_SMART_STRETCH);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Scalef(scale, scale, 1);

    if(hud->statusbarActive || (displayMode < 3 || hud->alpha > 0))
    {
        float opacity = /**@todo Kludge: clamp*/MIN_OF(1.0f, hud->alpha)/**kludge end*/ * (1-hud->hideAmount);
        Size2Raw drawnSize = { 0, 0 };
        RectRaw displayRegion;
        int availHeight;

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Scalef(1, 1.2f/*aspect correct*/, 1);

        displayRegion.origin.x = displayRegion.origin.y = 0;
        displayRegion.size.width  = .5f + portSize.width  / scale;
        displayRegion.size.height = .5f + portSize.height / (scale * 1.2f /*aspect correct*/);

        if(hud->statusbarActive)
        {
            const float statusbarOpacity = (1 - hud->hideAmount) * hud->showBar;

            obj = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_STATUSBAR]);
            UIWidget_SetOpacity(obj, statusbarOpacity);
            UIWidget_SetMaximumSize(obj, &displayRegion.size);

            GUI_DrawWidget(obj, &displayRegion.origin);

            Size2_Raw(Rect_Size(UIWidget_Geometry(obj)), &drawnSize);
        }

        displayRegion.origin.x += DISPLAY_BORDER;
        displayRegion.origin.y += DISPLAY_BORDER;
        displayRegion.size.width  -= DISPLAY_BORDER*2;
        displayRegion.size.height -= DISPLAY_BORDER*2;

        if(!hud->statusbarActive)
        {
            obj = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOM]);
            UIWidget_SetOpacity(obj, opacity);
            UIWidget_SetMaximumSize(obj, &displayRegion.size);

            GUI_DrawWidget(obj, &displayRegion.origin);

            Size2_Raw(Rect_Size(UIWidget_Geometry(obj)), &drawnSize);
        }

        obj = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_MAPNAME]);
        UIWidget_SetOpacity(obj, ST_AutomapOpacity(playerNum));
        availHeight = displayRegion.size.height - (drawnSize.height > 0 ? drawnSize.height : 0);
        size.width = displayRegion.size.width; size.height = availHeight;
        UIWidget_SetMaximumSize(obj, &size);

        GUI_DrawWidget(obj, &displayRegion.origin);

        // The other displays are always visible except when using the "no-hud" mode.
        if(hud->statusbarActive || displayMode < 3)
            opacity = 1.0f;

        obj = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPCENTER]);
        UIWidget_SetOpacity(obj, opacity);
        UIWidget_SetMaximumSize(obj, &displayRegion.size);

        GUI_DrawWidget(obj, &displayRegion.origin);

        obj = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_COUNTERS]);
        UIWidget_SetOpacity(obj, opacity);
        UIWidget_SetMaximumSize(obj, &displayRegion.size);

        GUI_DrawWidget(obj, &displayRegion.origin);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef DISPLAY_BORDER
}

void ST_Drawer(int player)
{
    hudstate_t* hud;

    if(player < 0 || player >= MAXPLAYERS)
    {
#if _DEBUG
        Con_Message("Warning:ST_Drawer: Invalid player #%i, ignoring.\n", player);
#endif
        return;
    }
    if(!players[player].plr->inGame) return;

    R_UpdateViewFilter(player);

    hud = &hudStates[player];
    hud->statusbarActive = (headupDisplayMode(player) < 2) || (ST_AutomapIsActive(player) && (cfg.automapHudDisplay == 0 || cfg.automapHudDisplay == 2));

    drawUIWidgetsForPlayer(players + player);
}

void ST_loadGraphics(void)
{
    int i, j, faceNum;
    char nameBuf[9];

    // Key cards:
    for(i = 0; i < NUM_KEY_TYPES; ++i)
    {
        sprintf(nameBuf, "STKEYS%d", i);
        pKeys[i] = R_DeclarePatch(nameBuf);
    }

    // Arms background.
    pArmsBackground = R_DeclarePatch("STARMS");

    // Arms ownership icons:
    for(i = 0; i < 6; ++i)
    {
        // gray
        sprintf(nameBuf, "STGNUM%d", i + 2);
        pArms[i][0] = R_DeclarePatch(nameBuf);

        // yellow
        sprintf(nameBuf, "STYSNUM%d", i + 2);
        pArms[i][1] = R_DeclarePatch(nameBuf);
    }

    // Face backgrounds for different color players.
    for(i = 0; i < 4; ++i)
    {
        sprintf(nameBuf, "STFB%d", i);
        pFaceBackground[i] = R_DeclarePatch(nameBuf);
    }

    // Status bar background bits.
    pStatusbar = R_DeclarePatch("STBAR");

    // Face states:
    faceNum = 0;
    for(i = 0; i < ST_NUMPAINFACES; ++i)
    {
        for(j = 0; j < ST_NUMSTRAIGHTFACES; ++j)
        {
            sprintf(nameBuf, "STFST%d%d", i, j);
            pFaces[faceNum++] = R_DeclarePatch(nameBuf);
        }
        sprintf(nameBuf, "STFTR%d0", i); // Turn right.
        pFaces[faceNum++] = R_DeclarePatch(nameBuf);
        sprintf(nameBuf, "STFTL%d0", i); // Turn left.
        pFaces[faceNum++] = R_DeclarePatch(nameBuf);
        sprintf(nameBuf, "STFOUCH%d", i); // Ouch.
        pFaces[faceNum++] = R_DeclarePatch(nameBuf);
        sprintf(nameBuf, "STFEVL%d", i); // Evil grin.
        pFaces[faceNum++] = R_DeclarePatch(nameBuf);
        sprintf(nameBuf, "STFKILL%d", i); // Pissed off.
        pFaces[faceNum++] = R_DeclarePatch(nameBuf);
    }
    pFaces[faceNum++] = R_DeclarePatch("STFGOD0");
    pFaces[faceNum++] = R_DeclarePatch("STFDEAD0");
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

static void setAutomapCheatLevel(uiwidget_t* obj, int level)
{
    const int playerNum = UIWidget_Player(obj);
    hudstate_t* hud = &hudStates[playerNum];
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

static void initAutomapForCurrentMap(uiwidget_t* obj)
{
    hudstate_t* hud = &hudStates[UIWidget_Player(obj)];
    automapcfg_t* mcfg;
    mobj_t* followMobj;
    uint i;

    UIAutomap_Reset(obj);

    UIAutomap_SetMinScale(obj, 2 * PLAYERRADIUS);
    UIAutomap_SetWorldBounds(obj, *((coord_t*) DD_GetVariable(DD_MAP_MIN_X)),
                                  *((coord_t*) DD_GetVariable(DD_MAP_MAX_X)),
                                  *((coord_t*) DD_GetVariable(DD_MAP_MIN_Y)),
                                  *((coord_t*) DD_GetVariable(DD_MAP_MAX_Y)));

    mcfg = UIAutomap_Config(obj);

    // Determine the obj view scale factors.
    if(UIAutomap_ZoomMax(obj))
        UIAutomap_SetScale(obj, 0);

    UIAutomap_ClearPoints(obj);

#if !__JHEXEN__
    if(gameSkill == SM_BABY && cfg.automapBabyKeys)
    {
        int flags = UIAutomap_Flags(obj);
        UIAutomap_SetFlags(obj, flags|AMF_REND_KEYS);
    }
#endif

#if __JDOOM__
    if(!IS_NETGAME && hud->automapCheatLevel)
        AM_SetVectorGraphic(mcfg, AMO_THINGPLAYER, VG_CHEATARROW);
#endif

    // Are we re-centering on a followed mobj?
    followMobj = UIAutomap_FollowMobj(obj);
    if(followMobj)
    {
        UIAutomap_SetCameraOrigin(obj, followMobj->origin[VX], followMobj->origin[VY]);
    }

    if(IS_NETGAME)
    {
        setAutomapCheatLevel(obj, 0);
    }

    UIAutomap_SetReveal(obj, false);

    // Add all immediately visible lines.
    for(i = 0; i < numlines; ++i)
    {
        xline_t* xline = &xlines[i];
        if(!(xline->flags & ML_MAPPED)) continue;

        P_SetLinedefAutomapVisibility(UIWidget_Player(obj), i, true);
    }
}

void ST_Start(int player)
{
    uiwidget_t* obj;
    hudstate_t* hud;
    int flags;

    if(player < 0 || player >= MAXPLAYERS)
    {
        Con_Error("ST_Start: Invalid player #%i.", player);
        exit(1); // Unreachable.
    }
    hud = &hudStates[player];

    if(!hud->stopped)
    {
        ST_Stop(player);
    }

    initData(hud);

    /**
     * Initialize widgets according to player preferences.
     */

    obj = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPCENTER]);
    flags = UIWidget_Alignment(obj);
    flags &= ~(ALIGN_LEFT|ALIGN_RIGHT);
    if(cfg.msgAlign == 0)
        flags |= ALIGN_LEFT;
    else if(cfg.msgAlign == 2)
        flags |= ALIGN_RIGHT;
    UIWidget_SetAlignment(obj, flags);

    obj = GUI_MustFindObjectById(hud->automapWidgetId);
    // If the automap was left open; close it.
    UIAutomap_Open(obj, false, true);
    initAutomapForCurrentMap(obj);
    UIAutomap_SetCameraRotation(obj, cfg.automapRotate);

    hud->stopped = false;
}

void ST_Stop(int player)
{
    hudstate_t* hud;

    if(player < 0 || player >= MAXPLAYERS) return;

    hud = &hudStates[player];
    if(hud->stopped) return;

    hud->stopped = true;
}

void ST_BuildWidgets(int player)
{
#define PADDING             (2) /// Units in fixed 320x200 screen space.

typedef struct {
    int group;
    int alignFlags;
    order_t order;
    int groupFlags;
    int padding;
} uiwidgetgroupdef_t;

typedef struct {
    guiwidgettype_t type;
    int alignFlags;
    int group;
    gamefontid_t fontIdx;
    void (*updateGeometry) (uiwidget_t* obj);
    void (*drawer) (uiwidget_t* obj, const Point2Raw* origin);
    void (*ticker) (uiwidget_t* obj, timespan_t ticLength);
    void* typedata;
} uiwidgetdef_t;

    hudstate_t* hud = hudStates + player;
    const uiwidgetgroupdef_t widgetGroupDefs[] = {
        { UWG_STATUSBAR,    ALIGN_BOTTOM },
        { UWG_MAPNAME,      ALIGN_BOTTOMLEFT },
        { UWG_BOTTOMLEFT,   ALIGN_BOTTOMLEFT,  ORDER_RIGHTTOLEFT, UWGF_VERTICAL, PADDING },
        { UWG_BOTTOMLEFT2,  ALIGN_BOTTOMLEFT,  ORDER_LEFTTORIGHT, 0, PADDING },
        { UWG_BOTTOMRIGHT,  ALIGN_BOTTOMRIGHT, ORDER_RIGHTTOLEFT, 0, PADDING },
        { UWG_BOTTOMCENTER, ALIGN_BOTTOM,      ORDER_RIGHTTOLEFT, UWGF_VERTICAL, PADDING },
        { UWG_BOTTOM,       ALIGN_BOTTOMLEFT,  ORDER_LEFTTORIGHT },
        { UWG_TOPCENTER,    ALIGN_TOPLEFT,     ORDER_LEFTTORIGHT, UWGF_VERTICAL, PADDING },
        { UWG_COUNTERS,     ALIGN_LEFT,        ORDER_RIGHTTOLEFT, UWGF_VERTICAL, PADDING },
        { UWG_AUTOMAP,      ALIGN_TOPLEFT }
    };
    const uiwidgetdef_t widgetDefs[] = {
        { GUI_BOX,      ALIGN_TOPLEFT,      UWG_STATUSBAR,      0,          SBarBackground_UpdateGeometry, SBarBackground_Drawer },
        { GUI_READYAMMO, ALIGN_TOPLEFT,     UWG_STATUSBAR,      GF_STATUS,  SBarReadyAmmo_UpdateGeometry, SBarReadyAmmo_Drawer, ReadyAmmo_Ticker, &hud->sbarReadyammo },
        { GUI_HEALTH,   ALIGN_TOPLEFT,      UWG_STATUSBAR,      GF_STATUS,  SBarHealth_UpdateGeometry, SBarHealth_Drawer, Health_Ticker, &hud->sbarHealth },
        { GUI_WEAPONSLOT, ALIGN_TOPLEFT,    UWG_STATUSBAR,      0,          WeaponSlot_UpdateGeometry, WeaponSlot_Drawer, WeaponSlot_Ticker, &hud->sbarWeaponslots[0] },
        { GUI_WEAPONSLOT, ALIGN_TOPLEFT,    UWG_STATUSBAR,      0,          WeaponSlot_UpdateGeometry, WeaponSlot_Drawer, WeaponSlot_Ticker, &hud->sbarWeaponslots[1] },
        { GUI_WEAPONSLOT, ALIGN_TOPLEFT,    UWG_STATUSBAR,      0,          WeaponSlot_UpdateGeometry, WeaponSlot_Drawer, WeaponSlot_Ticker, &hud->sbarWeaponslots[2] },
        { GUI_WEAPONSLOT, ALIGN_TOPLEFT,    UWG_STATUSBAR,      0,          WeaponSlot_UpdateGeometry, WeaponSlot_Drawer, WeaponSlot_Ticker, &hud->sbarWeaponslots[3] },
        { GUI_WEAPONSLOT, ALIGN_TOPLEFT,    UWG_STATUSBAR,      0,          WeaponSlot_UpdateGeometry, WeaponSlot_Drawer, WeaponSlot_Ticker, &hud->sbarWeaponslots[4] },
        { GUI_WEAPONSLOT, ALIGN_TOPLEFT,    UWG_STATUSBAR,      0,          WeaponSlot_UpdateGeometry, WeaponSlot_Drawer, WeaponSlot_Ticker, &hud->sbarWeaponslots[5] },
        { GUI_FRAGS,    ALIGN_TOPLEFT,      UWG_STATUSBAR,      GF_STATUS,  SBarFrags_UpdateGeometry, SBarFrags_Drawer, SBarFrags_Ticker, &hud->sbarFrags },
        { GUI_FACE,     ALIGN_TOPLEFT,      UWG_STATUSBAR,      0,          SBarFace_UpdateGeometry, SBarFace_Drawer, Face_Ticker, &hud->sbarFace },
        { GUI_ARMOR,    ALIGN_TOPLEFT,      UWG_STATUSBAR,      GF_STATUS,  SBarArmor_UpdateGeometry, SBarArmor_Drawer, Armor_Ticker, &hud->sbarArmor },
        { GUI_KEYSLOT,  ALIGN_TOPLEFT,      UWG_STATUSBAR,      0,          KeySlot_UpdateGeometry, KeySlot_Drawer, KeySlot_Ticker, &hud->sbarKeyslots[0] },
        { GUI_KEYSLOT,  ALIGN_TOPLEFT,      UWG_STATUSBAR,      0,          KeySlot_UpdateGeometry, KeySlot_Drawer, KeySlot_Ticker, &hud->sbarKeyslots[1] },
        { GUI_KEYSLOT,  ALIGN_TOPLEFT,      UWG_STATUSBAR,      0,          KeySlot_UpdateGeometry, KeySlot_Drawer, KeySlot_Ticker, &hud->sbarKeyslots[2] },
        { GUI_AMMO,     ALIGN_TOPLEFT,      UWG_STATUSBAR,      GF_INDEX,   Ammo_UpdateGeometry, Ammo_Drawer, Ammo_Ticker, &hud->sbarAmmos[AT_CLIP] },
        { GUI_AMMO,     ALIGN_TOPLEFT,      UWG_STATUSBAR,      GF_INDEX,   Ammo_UpdateGeometry, Ammo_Drawer, Ammo_Ticker, &hud->sbarAmmos[AT_SHELL] },
        { GUI_AMMO,     ALIGN_TOPLEFT,      UWG_STATUSBAR,      GF_INDEX,   Ammo_UpdateGeometry, Ammo_Drawer, Ammo_Ticker, &hud->sbarAmmos[AT_CELL] },
        { GUI_AMMO,     ALIGN_TOPLEFT,      UWG_STATUSBAR,      GF_INDEX,   Ammo_UpdateGeometry, Ammo_Drawer, Ammo_Ticker, &hud->sbarAmmos[AT_MISSILE] },
        { GUI_AMMO,     ALIGN_TOPLEFT,      UWG_STATUSBAR,      GF_INDEX,   MaxAmmo_UpdateGeometry, MaxAmmo_Drawer, MaxAmmo_Ticker, &hud->sbarMaxammos[AT_CLIP] },
        { GUI_AMMO,     ALIGN_TOPLEFT,      UWG_STATUSBAR,      GF_INDEX,   MaxAmmo_UpdateGeometry, MaxAmmo_Drawer, MaxAmmo_Ticker, &hud->sbarMaxammos[AT_SHELL] },
        { GUI_AMMO,     ALIGN_TOPLEFT,      UWG_STATUSBAR,      GF_INDEX,   MaxAmmo_UpdateGeometry, MaxAmmo_Drawer, MaxAmmo_Ticker, &hud->sbarMaxammos[AT_CELL] },
        { GUI_AMMO,     ALIGN_TOPLEFT,      UWG_STATUSBAR,      GF_INDEX,   MaxAmmo_UpdateGeometry, MaxAmmo_Drawer, MaxAmmo_Ticker, &hud->sbarMaxammos[AT_MISSILE] },
        { GUI_MAPNAME,  ALIGN_BOTTOMLEFT,   UWG_MAPNAME,        GF_FONTB,   MapName_UpdateGeometry, MapName_Drawer },
        { GUI_BOX,      ALIGN_BOTTOMLEFT,   UWG_BOTTOMLEFT2,    0,          HealthIcon_UpdateGeometry, HealthIcon_Drawer },
        { GUI_HEALTH,   ALIGN_BOTTOMLEFT,   UWG_BOTTOMLEFT2,    GF_FONTB,   Health_UpdateGeometry, Health_Drawer, Health_Ticker, &hud->health },
        { GUI_READYAMMOICON, ALIGN_BOTTOMLEFT, UWG_BOTTOMLEFT2, 0,          ReadyAmmoIcon_UpdateGeometry, ReadyAmmoIcon_Drawer, ReadyAmmoIcon_Ticker, &hud->readyammoicon },
        { GUI_READYAMMO, ALIGN_BOTTOMLEFT,  UWG_BOTTOMLEFT2,    GF_FONTB,   ReadyAmmo_UpdateGeometry, ReadyAmmo_Drawer, ReadyAmmo_Ticker, &hud->readyammo },
        { GUI_FRAGS,    ALIGN_BOTTOMLEFT,   UWG_BOTTOMLEFT,     GF_FONTA,   Frags_UpdateGeometry, Frags_Drawer, Frags_Ticker, &hud->frags },
        { GUI_ARMOR,    ALIGN_BOTTOMRIGHT,  UWG_BOTTOMRIGHT,    GF_FONTB,   Armor_UpdateGeometry, Armor_Drawer, Armor_Ticker, &hud->armor },
        { GUI_ARMORICON, ALIGN_BOTTOMRIGHT, UWG_BOTTOMRIGHT,    0,          ArmorIcon_UpdateGeometry, ArmorIcon_Drawer, ArmorIcon_Ticker, &hud->armoricon },
        { GUI_KEYS,     ALIGN_BOTTOMRIGHT,  UWG_BOTTOMRIGHT,    0,          Keys_UpdateGeometry, Keys_Drawer, Keys_Ticker, &hud->keys },
        { GUI_FACE,     ALIGN_BOTTOM,       UWG_BOTTOMCENTER,   0,          Face_UpdateGeometry, Face_Drawer, Face_Ticker, &hud->face },
        { GUI_SECRETS,  ALIGN_TOPLEFT,      UWG_COUNTERS,       GF_FONTA,   Secrets_UpdateGeometry, Secrets_Drawer, Secrets_Ticker, &hud->secrets },
        { GUI_ITEMS,    ALIGN_TOPLEFT,      UWG_COUNTERS,       GF_FONTA,   Items_UpdateGeometry, Items_Drawer, Items_Ticker, &hud->items },
        { GUI_KILLS,    ALIGN_TOPLEFT,      UWG_COUNTERS,       GF_FONTA,   Kills_UpdateGeometry, Kills_Drawer, Kills_Ticker, &hud->kills },
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
        hud->widgetGroupIds[def->group] = GUI_CreateGroup(def->groupFlags, player, def->alignFlags, def->order, def->padding);
    }

    UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOMLEFT]),
                      GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOMLEFT2]));

    for(i = 0; widgetDefs[i].type != GUI_NONE; ++i)
    {
        const uiwidgetdef_t* def = &widgetDefs[i];
        uiwidgetid_t id = GUI_CreateWidget(def->type, player, def->alignFlags, FID(def->fontIdx), 1, def->updateGeometry, def->drawer, def->ticker, def->typedata);
        UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[def->group]), GUI_FindObjectById(id));
    }

    UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOM]),
                      GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOMLEFT]));
    UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOM]),
                      GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOMCENTER]));
    UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOM]),
                      GUI_MustFindObjectById(hud->widgetGroupIds[UWG_BOTTOMRIGHT]));

    hud->logWidgetId = GUI_CreateWidget(GUI_LOG, player, ALIGN_TOPLEFT, FID(GF_FONTA), 1, UILog_UpdateGeometry, UILog_Drawer, UILog_Ticker, &hud->log);
    UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPCENTER]), GUI_FindObjectById(hud->logWidgetId));

    hud->chatWidgetId = GUI_CreateWidget(GUI_CHAT, player, ALIGN_TOPLEFT, FID(GF_FONTA), 1, UIChat_UpdateGeometry, UIChat_Drawer, NULL, &hud->chat);
    UIGroup_AddWidget(GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPCENTER]), GUI_FindObjectById(hud->chatWidgetId));

    hud->automapWidgetId = GUI_CreateWidget(GUI_AUTOMAP, player, ALIGN_TOPLEFT, FID(GF_FONTA), 1, UIAutomap_UpdateGeometry, UIAutomap_Drawer, UIAutomap_Ticker, &hud->automap);
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
    if(!obj) return false;

    return UIChat_Responder(obj, ev);
}

boolean ST_ChatIsActive(int player)
{
    uiwidget_t* obj = ST_UIChatForPlayer(player);
    if(!obj) return false;

    return UIChat_IsActive(obj);
}

void ST_LogPost(int player, byte flags, const char* msg)
{
    uiwidget_t* obj = ST_UILogForPlayer(player);
    if(!obj) return;

    UILog_Post(obj, flags, msg);
}

void ST_LogPostVisibilityChangeNotification(void)
{
    int i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        ST_LogPost(i, LMF_NO_HIDE, !cfg.hudShown[HUD_LOG] ? MSGOFF : MSGON);
    }
}

void ST_LogRefresh(int player)
{
    uiwidget_t* obj = ST_UILogForPlayer(player);
    if(!obj) return;

    UILog_Refresh(obj);
}

void ST_LogEmpty(int player)
{
    uiwidget_t* obj = ST_UILogForPlayer(player);
    if(!obj) return;

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

        obj = GUI_MustFindObjectById(hud->widgetGroupIds[UWG_TOPCENTER]);
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
    if(!obj) return;
    UIAutomap_Open(obj, yes, fast);
}

boolean ST_AutomapIsActive(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return false;
    return UIAutomap_Active(obj);
}

boolean ST_AutomapObscures2(int player, const RectRaw* region)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return false;
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
                const Rect* rect = UIWidget_Geometry(obj);
                float fx = FIXXTOSCREENX(region->origin.x);
                float fy = FIXYTOSCREENY(region->origin.y);
                float fw = FIXXTOSCREENX(region->size.width);
                float fh = FIXYTOSCREENY(region->size.height);

                if(dims->origin.x >= fx && dims->origin.y >= fy && dims->size.width >= fw && dims->size.height >= fh)
                    return true;
            }*/
        }
    }
    return false;
}

boolean ST_AutomapObscures(int player, int x, int y, int width, int height)
{
    RectRaw rect;
    rect.origin.x = x;
    rect.origin.y = y;
    rect.size.width  = width;
    rect.size.height = height;
    return ST_AutomapObscures2(player, &rect);
}

void ST_AutomapClearPoints(int player)
{
    uiwidget_t* ob = ST_UIAutomapForPlayer(player);
    if(!ob) return;

    UIAutomap_ClearPoints(ob);
    P_SetMessage(&players[player], LMF_NO_HIDE, AMSTR_MARKSCLEARED);
}

/**
 * Adds a marker at the specified X/Y location.
 */
int ST_AutomapAddPoint(int player, coord_t x, coord_t y, coord_t z)
{
    static char buffer[20];
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    int newPoint;
    if(!obj) return - 1;

    if(UIAutomap_PointCount(obj) == MAX_MAP_POINTS)
    {
        return -1;
    }

    newPoint = UIAutomap_AddPoint(obj, x, y, z);
    sprintf(buffer, "%s %d", AMSTR_MARKEDSPOT, newPoint);
    P_SetMessage(&players[player], LMF_NO_HIDE, buffer);

    return newPoint;
}

boolean ST_AutomapPointOrigin(int player, int point, coord_t* x, coord_t* y, coord_t* z)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return false;
    return UIAutomap_PointOrigin(obj, point, x, y, z);
}

void ST_ToggleAutomapMaxZoom(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return;
    if(UIAutomap_SetZoomMax(obj, !UIAutomap_ZoomMax(obj)))
    {
        Con_Printf("Maximum zoom %s in automap.\n", UIAutomap_ZoomMax(obj)? "ON":"OFF");
    }
}

float ST_AutomapOpacity(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return 0;
    return UIAutomap_Opacity(obj);
}

void ST_SetAutomapCameraRotation(int player, boolean on)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return;
    UIAutomap_SetCameraRotation(obj, on);
}

void ST_ToggleAutomapPanMode(int player)
{
    uiwidget_t* ob = ST_UIAutomapForPlayer(player);
    if(!ob) return;
    if(UIAutomap_SetPanMode(ob, !UIAutomap_PanMode(ob)))
    {
        P_SetMessage(&players[player], LMF_NO_HIDE, (UIAutomap_PanMode(ob)? AMSTR_FOLLOWOFF : AMSTR_FOLLOWON));
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
    if(!obj) return;
    setAutomapCheatLevel(obj, level);
}

void ST_RevealAutomap(int player, boolean on)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return;
    UIAutomap_SetReveal(obj, on);
}

boolean ST_AutomapHasReveal(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return false;
    return UIAutomap_Reveal(obj);
}

void ST_RebuildAutomap(int player)
{
    uiwidget_t* obj = ST_UIAutomapForPlayer(player);
    if(!obj) return;
    UIAutomap_Rebuild(obj);
}

int ST_AutomapCheatLevel(int player)
{
    if(player >=0 && player < MAXPLAYERS)
    {
        return hudStates[player].automapCheatLevel;
    }
    return 0;
}

/// @note Called when the statusbar scale cvar changes.
void updateViewWindow(void)
{
    int i;
    R_ResizeViewWindow(RWF_FORCE);
    // Reveal the HUD so the user can see the change.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        ST_HUDUnHide(i, HUE_FORCE);
    }
}

/**
 * Called when a cvar changes that affects the look/behavior of the HUD in order to unhide it.
 */
void unhideHUD(void)
{
    int i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        ST_HUDUnHide(i, HUE_FORCE);
    }
}

D_CMD(ChatOpen)
{
    int player = CONSOLEPLAYER, destination = 0;
    uiwidget_t* obj;

    if(G_GameAction() == GA_QUIT) return false;

    obj = ST_UIChatForPlayer(player);
    if(!obj) return false;

    if(argc == 2)
    {
        destination = UIChat_ParseDestination(argv[1]);
        if(destination < 0)
        {
            Con_Message("Invalid team number #%i (valid range: 0..%i).\n", destination, NUMTEAMS);
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

    if(G_GameAction() == GA_QUIT) return false;

    obj = ST_UIChatForPlayer(player);
    if(!obj || !UIChat_IsActive(obj)) return false;

    if(!stricmp(cmd, "complete")) // Send the message.
    {
        return UIChat_CommandResponder(obj, MCMD_SELECT);
    }
    if(!stricmp(cmd, "cancel")) // Close chat.
    {
        return UIChat_CommandResponder(obj, MCMD_CLOSE);
    }
    if(!stricmp(cmd, "delete"))
    {
        return UIChat_CommandResponder(obj, MCMD_DELETE);
    }
    return true;
}

D_CMD(ChatSendMacro)
{
    int player = CONSOLEPLAYER, macroId, destination = 0;
    uiwidget_t* obj;

    if(G_GameAction() == GA_QUIT) return false;

    if(argc < 2 || argc > 3)
    {
        Con_Message("Usage: %s (team) (macro number)\n", argv[0]);
        Con_Message("Send a chat macro to other player(s).\n"
                    "If (team) is omitted, the message will be sent to all players.\n");
        return true;
    }

    obj = ST_UIChatForPlayer(player);
    if(!obj) return false;

    if(argc == 3)
    {
        destination = UIChat_ParseDestination(argv[1]);
        if(destination < 0)
        {
            Con_Message("Invalid team number #%i (valid range: 0..%i).\n", destination, NUMTEAMS);
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
