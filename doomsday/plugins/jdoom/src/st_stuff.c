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
#define RADIATIONPAL        13

// N/256*100% probability
//  that the normal face state will change
#define ST_FACEPROBABILITY  96

// Location of status bar
#define ST_X                0
#define ST_X2               104

#define ST_FX               144
#define ST_FY               169

// Number of status faces.
#define ST_NUMPAINFACES     5
#define ST_NUMSTRAIGHTFACES 3
#define ST_NUMTURNFACES     2
#define ST_NUMSPECIALFACES  3

#define ST_FACESTRIDE \
          (ST_NUMSTRAIGHTFACES+ST_NUMTURNFACES+ST_NUMSPECIALFACES)

#define ST_NUMEXTRAFACES    2

#define ST_NUMFACES \
          (ST_FACESTRIDE*ST_NUMPAINFACES+ST_NUMEXTRAFACES)

#define ST_TURNOFFSET       (ST_NUMSTRAIGHTFACES)
#define ST_OUCHOFFSET       (ST_TURNOFFSET + ST_NUMTURNFACES)
#define ST_EVILGRINOFFSET       (ST_OUCHOFFSET + 1)
#define ST_RAMPAGEOFFSET        (ST_EVILGRINOFFSET + 1)
#define ST_GODFACE          (ST_NUMPAINFACES*ST_FACESTRIDE)
#define ST_DEADFACE         (ST_GODFACE+1)

#define ST_FACESX           143
#define ST_FACESY           168

#define ST_EVILGRINCOUNT        (2*TICRATE)
#define ST_STRAIGHTFACECOUNT    (TICRATE/2)
#define ST_TURNCOUNT        (1*TICRATE)
#define ST_OUCHCOUNT        (1*TICRATE)
#define ST_RAMPAGEDELAY     (2*TICRATE)

#define ST_MUCHPAIN         20

// AMMO number pos.
#define ST_AMMOWIDTH        3
#define ST_AMMOX            44
#define ST_AMMOY            171

// HEALTH number pos.
#define ST_HEALTHWIDTH      3
#define ST_HEALTHX          90
#define ST_HEALTHY          171

// Weapon pos.
#define ST_ARMSX            111
#define ST_ARMSY            172
#define ST_ARMSBGX          104
#define ST_ARMSBGY          168
#define ST_ARMSXSPACE       12
#define ST_ARMSYSPACE       10

// Frags pos.
#define ST_FRAGSX           138
#define ST_FRAGSY           171
#define ST_FRAGSWIDTH       2

// ARMOR number pos.
#define ST_ARMORWIDTH       3
#define ST_ARMORX           221
#define ST_ARMORY           171

// Key icon positions.
#define ST_KEY0WIDTH        8
#define ST_KEY0HEIGHT       5
#define ST_KEY0X            239
#define ST_KEY0Y            171
#define ST_KEY1WIDTH        ST_KEY0WIDTH
#define ST_KEY1X            239
#define ST_KEY1Y            181
#define ST_KEY2WIDTH        ST_KEY0WIDTH
#define ST_KEY2X            239
#define ST_KEY2Y            191

// Ammunition counter.
#define ST_AMMO0WIDTH       3
#define ST_AMMO0HEIGHT      6
#define ST_AMMO0X           288
#define ST_AMMO0Y           173
#define ST_AMMO1WIDTH       ST_AMMO0WIDTH
#define ST_AMMO1X           288
#define ST_AMMO1Y           179
#define ST_AMMO2WIDTH       ST_AMMO0WIDTH
#define ST_AMMO2X           288
#define ST_AMMO2Y           191
#define ST_AMMO3WIDTH       ST_AMMO0WIDTH
#define ST_AMMO3X           288
#define ST_AMMO3Y           185

// Indicate maximum ammunition.
// Only needed because backpack exists.
#define ST_MAXAMMO0WIDTH    3
#define ST_MAXAMMO0HEIGHT   5
#define ST_MAXAMMO0X        314
#define ST_MAXAMMO0Y        173
#define ST_MAXAMMO1WIDTH        ST_MAXAMMO0WIDTH
#define ST_MAXAMMO1X        314
#define ST_MAXAMMO1Y        179
#define ST_MAXAMMO2WIDTH        ST_MAXAMMO0WIDTH
#define ST_MAXAMMO2X        314
#define ST_MAXAMMO2Y        191
#define ST_MAXAMMO3WIDTH        ST_MAXAMMO0WIDTH
#define ST_MAXAMMO3X        314
#define ST_MAXAMMO3Y        185

// pistol
#define ST_WEAPON0X         110
#define ST_WEAPON0Y         172

// shotgun
#define ST_WEAPON1X         122
#define ST_WEAPON1Y         172

// chain gun
#define ST_WEAPON2X         134
#define ST_WEAPON2Y         172

// missile launcher
#define ST_WEAPON3X         110
#define ST_WEAPON3Y         181

// plasma gun
#define ST_WEAPON4X         122
#define ST_WEAPON4Y         181

 // bfg
#define ST_WEAPON5X         134
#define ST_WEAPON5Y         181

// WPNS title
#define ST_WPNSX            109
#define ST_WPNSY            191

 // DETH title
#define ST_DETHX            109
#define ST_DETHY            191

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

extern boolean hu_showallfrags; // in hu_stuff.c currently

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int hudHideTics;
static float hudHideAmount;

// slide statusbar amount 1.0 is fully open
static float showbar = 0.0f;

// fullscreen hud alpha value
static float hudalpha = 0.0f;

static float statusbarCounterAlpha = 0.0f;

// ST_Start() has just been called
static boolean st_firsttime;

// whether to use alpha blending
static boolean st_blended = false;

// used to execute ST_Init() only once
static int veryfirsttime = 1;

// lump number for PLAYPAL
static int lu_palette;

// used for timing
static unsigned int st_clock;

// used for making messages go away
static int st_msgcounter = 0;

// used when in chat
static st_chatstateenum_t st_chatstate;

// whether in automap or first-person
static st_stateenum_t st_gamestate;

// whether left-side main status bar is active
static boolean st_statusbaron;

// whether status bar chat is active
static boolean st_chat;

// value of st_chat before message popped up
static boolean st_oldchat;

// whether chat window has the cursor on
static boolean st_cursoron;

// !deathmatch
static boolean st_notdeathmatch;

// !deathmatch && st_statusbaron
static boolean st_armson;

// !deathmatch
static boolean st_fragson;

// main bar left
static dpatch_t sbar;

// 0-9, tall numbers
static dpatch_t tallnum[10];

// tall % sign
static dpatch_t tallpercent;

// 0-9, short, yellow (,different!) numbers
static dpatch_t shortnum[10];

// 3 key-cards, 3 skulls
static dpatch_t keys[NUM_KEY_TYPES];

// face status patches
static dpatch_t faces[ST_NUMFACES];

// face background
static dpatch_t faceback;

 // main bar right
static dpatch_t armsbg;

// weapon ownership patches
static dpatch_t arms[6][2];

// ready-weapon widget
static st_number_t w_ready;

 // in deathmatch only, summary of frags stats
static st_number_t w_frags;

// health widget
static st_percent_t w_health;

// weapon ownership widgets
static st_multicon_t w_arms[6];

// face status widget
static st_multicon_t w_faces;

// keycard widgets
static st_multicon_t w_keyboxes[3];

// armor widget
static st_percent_t w_armor;

// ammo widgets
static st_number_t w_ammo[4];

// max ammo widgets
static st_number_t w_maxammo[4];

 // number of frags so far in deathmatch
static int st_fragscount;

// used to use appopriately pained face
static int st_oldhealth = -1;

// used for evil grin
static boolean oldweaponsowned[NUM_WEAPON_TYPES];

 // count until face changes
static int st_facecount = 0;

// current face index, used by w_faces
static int st_faceindex = 0;

// holds key-type for each key box on bar
static int keyboxes[3];

// a random number per tick
static int st_randomnumber;

static boolean st_stopped = true;
static int st_palette = 0;

// CVARs for the HUD/Statusbar
cvar_t sthudCVars[] =
{
    // HUD scale
    {"hud-scale", 0, CVT_FLOAT, &cfg.hudScale, 0.1f, 10},

    {"hud-status-size", CVF_PROTECTED, CVT_INT, &cfg.sbarscale, 1, 20},

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

    // HUD displays
    {"hud-frags", 0, CVT_BYTE, &cfg.hudShown[HUD_FRAGS], 0, 1},

    {"hud-frags-all", 0, CVT_BYTE, &hu_showallfrags, 0, 1},

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
    int     i;

    for(i = 0; sthudCVars[i].name; i++)
        Con_AddVariable(sthudCVars + i);
    for(i = 0; sthudCCmds[i].name; i++)
        Con_AddCommand(sthudCCmds + i);
}

void ST_refreshBackground(void)
{
    int         x, y, w, h;
    float       cw, cw2, ch;
    float       alpha;

    GL_SetPatch(sbar.lump, DGL_CLAMP, DGL_CLAMP);

    if(st_blended)
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
        GL_DrawPatch(ST_X, ST_Y, sbar.lump);

        if(st_armson)  // arms baground
            GL_DrawPatch(ST_ARMSBGX, ST_ARMSBGY, armsbg.lump);

        if(IS_NETGAME) // faceback
            GL_DrawPatch(ST_FX, ST_Y+1, faceback.lump);
    }
    else
    {
        // Alpha blended status bar, we'll need to cut it up into smaller bits...
        DGL_Color4f(1, 1, 1, alpha);

        DGL_Begin(DGL_QUADS);

        // (up to faceback if deathmatch, else ST_ARMS)
        x = ST_X;
        y = ST_Y;
        w = st_armson ? 104 : 143;
        h = 32;
        cw = st_armson ? 0.325f : 0.446875f;

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

        if(st_armson)  // arms baground
            GL_DrawPatch_CS(ST_ARMSBGX, ST_ARMSBGY, armsbg.lump);

        if(IS_NETGAME) // faceback
            GL_DrawPatch_CS(ST_FX, ST_Y+1, faceback.lump);
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
    int     health;
    static int lastcalc;
    static int oldhealth = -1;
    player_t *plyr = &players[consoleplayer];

    health = (plyr->health > 100 ? 100 : plyr->health);

    if(health != oldhealth)
    {
        lastcalc = ST_FACESTRIDE * (((100 - health) * ST_NUMPAINFACES) / 101);
        oldhealth = health;
    }
    return lastcalc;
}

/**
 * This is a not-very-pretty routine which handles the face states
 * and their timing. the precedence of expressions is:
 *
 * dead > evil grin > turned head > straight ahead
 */
void ST_updateFaceWidget(void)
{
    int         i;
    angle_t     badguyangle;
    angle_t     diffang;
    static int  lastattackdown = -1;
    static int  priority = 0;
    boolean     doevilgrin;
    player_t   *plyr = &players[consoleplayer];

    if(priority < 10)
    {   // Player is dead.
        if(!plyr->health)
        {
            priority = 9;
            st_faceindex = ST_DEADFACE;
            st_facecount = 1;
        }
    }

    if(priority < 9)
    {
        if(plyr->bonusCount)
        {   // Picking up a bonus.
            doevilgrin = false;

            for(i = 0; i < NUM_WEAPON_TYPES; ++i)
            {
                if(oldweaponsowned[i] != plyr->weaponOwned[i])
                {
                    doevilgrin = true;
                    oldweaponsowned[i] = plyr->weaponOwned[i];
                }
            }

            if(doevilgrin)
            {   // Evil grin if just picked up weapon.
                priority = 8;
                st_facecount = ST_EVILGRINCOUNT;
                st_faceindex = ST_calcPainOffset() + ST_EVILGRINOFFSET;
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
            // if(plyr->health - st_oldhealth > ST_MUCHPAIN)

            if((cfg.fixOuchFace?
               (st_oldhealth - plyr->health) :
               (plyr->health - st_oldhealth)) > ST_MUCHPAIN)
            {
                st_facecount = ST_TURNCOUNT;
                st_faceindex = ST_calcPainOffset() + ST_OUCHOFFSET;
                if(cfg.fixOuchFace)
                    priority = 8; // Added to fix 1 tic issue.
            }
            else
            {
                badguyangle =
                    R_PointToAngle2(FLT2FIX(plyr->plr->mo->pos[VX]),
                                    FLT2FIX(plyr->plr->mo->pos[VY]),
                                    FLT2FIX(plyr->attacker->pos[VX]),
                                    FLT2FIX(plyr->attacker->pos[VY]));

                if(badguyangle > plyr->plr->mo->angle)
                {   // Whether right or left.
                    diffang = badguyangle - plyr->plr->mo->angle;
                    i = diffang > ANG180;
                }
                else
                {   // Whether left or right.
                    diffang = plyr->plr->mo->angle - badguyangle;
                    i = diffang <= ANG180;
                }

                st_facecount = ST_TURNCOUNT;
                st_faceindex = ST_calcPainOffset();

                if(diffang < ANG45)
                {   // Head-on.
                    st_faceindex += ST_RAMPAGEOFFSET;
                }
                else if(i)
                {   // Turn face right.
                    st_faceindex += ST_TURNOFFSET;
                }
                else
                {   // Turn face left.
                    st_faceindex += ST_TURNOFFSET + 1;
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
            // if(plyr->health - st_oldhealth > ST_MUCHPAIN)

            if((cfg.fixOuchFace?
               (st_oldhealth - plyr->health) :
               (plyr->health - st_oldhealth)) > ST_MUCHPAIN)
            {
                priority = 7;
                st_facecount = ST_TURNCOUNT;
                st_faceindex = ST_calcPainOffset() + ST_OUCHOFFSET;
            }
            else
            {
                priority = 6;
                st_facecount = ST_TURNCOUNT;
                st_faceindex = ST_calcPainOffset() + ST_RAMPAGEOFFSET;
            }
        }
    }

    if(priority < 6)
    {   // Rapid firing.
        if(plyr->attackDown)
        {
            if(lastattackdown == -1)
            {
                lastattackdown = ST_RAMPAGEDELAY;
            }
            else if(!--lastattackdown)
            {
                priority = 5;
                st_faceindex = ST_calcPainOffset() + ST_RAMPAGEOFFSET;
                st_facecount = 1;
                lastattackdown = 1;
            }
        }
        else
        {
            lastattackdown = -1;
        }
    }

    if(priority < 5)
    {   // Invulnerability.
        if((P_GetPlayerCheats(plyr) & CF_GODMODE) ||
           plyr->powers[PT_INVULNERABILITY])
        {
            priority = 4;

            st_faceindex = ST_GODFACE;
            st_facecount = 1;
        }
    }

    // Look left or look right if the facecount has timed out.
    if(!st_facecount)
    {
        st_faceindex = ST_calcPainOffset() + (st_randomnumber % 3);
        st_facecount = ST_STRAIGHTFACECOUNT;
        priority = 0;
    }

    st_facecount--;
}

void ST_updateWidgets(void)
{
    static int largeammo = 1994;    // means "n/a"
    int     i;
    ammotype_t ammotype;
    boolean found;
    player_t *plr = &players[consoleplayer];

    if(st_blended)
    {
        statusbarCounterAlpha = cfg.statusbarCounterAlpha - hudHideAmount;
        CLAMP(statusbarCounterAlpha, 0.0f, 1.0f);
    }
    else
        statusbarCounterAlpha = 1.0f;

    // must redirect the pointer if the ready weapon has changed.
    found = false;
    for(ammotype=0; ammotype < NUM_AMMO_TYPES && !found; ++ammotype)
    {
        if(!weaponinfo[plr->readyWeapon][plr->class].mode[0].ammotype[ammotype])
            continue; // Weapon does not use this type of ammo.

        //// \todo Only supports one type of ammo per weapon
        w_ready.num = &plr->ammo[ammotype];
        found = true;
    }
    if(!found) // Weapon takes no ammo at all.
    {
        w_ready.num = &largeammo;
    }

    w_ready.data = plr->readyWeapon;

    // update keycard multiple widgets
    for(i = 0; i < 3; i++)
    {
        keyboxes[i] = plr->keys[i] ? i : -1;

        if(plr->keys[i + 3])
            keyboxes[i] = i + 3;
    }

    // refresh everything if this is him coming back to life
    ST_updateFaceWidget();

    // used by the w_armsbg widget
    st_notdeathmatch = !deathmatch;

    // used by w_arms[] widgets
    st_armson = st_statusbaron && !deathmatch;

    // used by w_frags widget
    st_fragson = deathmatch && st_statusbaron;
    st_fragscount = 0;

    for(i = 0; i < MAXPLAYERS; i++)
    {
        if(!players[i].plr->inGame)
            continue;

        st_fragscount += plr->frags[i] * (i != consoleplayer ? 1 : -1);
    }

    // get rid of chat window if up because of message
    if(!--st_msgcounter)
        st_chat = st_oldchat;

}

void ST_Ticker(void)
{
    player_t *plyr = &players[consoleplayer];

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

    st_clock++;
    st_randomnumber = M_Random();
    ST_updateWidgets();
    st_oldhealth = plyr->health;
}

int R_GetFilterColor(int filter)
{
    int     rgba = 0;

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
    int     palette;
    int     cnt;
    int     bzc;
    player_t *plyr = &players[consoleplayer];

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

    if(palette != st_palette)
    {
        st_palette = palette;
        plyr->plr->filter = R_GetFilterColor(palette);  // $democam
    }
}

void ST_drawWidgets(boolean refresh)
{
    int     i;

    // used by w_arms[] widgets
    st_armson = st_statusbaron && !deathmatch;

    // used by w_frags widget
    st_fragson = deathmatch && st_statusbaron;

    STlib_updateNum(&w_ready, refresh);

    for(i = 0; i < 4; i++)
    {
        STlib_updateNum(&w_ammo[i], refresh);
        STlib_updateNum(&w_maxammo[i], refresh);
    }

    STlib_updatePercent(&w_health, refresh);
    STlib_updatePercent(&w_armor, refresh);

    for(i = 0; i < 6; i++)
        STlib_updateMultIcon(&w_arms[i], refresh);

    STlib_updateMultIcon(&w_faces, refresh);

    for(i = 0; i < 3; i++)
        STlib_updateMultIcon(&w_keyboxes[i], refresh);

    STlib_updateNum(&w_frags, refresh);

}

void ST_doRefresh(void)
{
    st_firsttime = false;

    if(cfg.sbarscale < 20 || (cfg.sbarscale == 20 && showbar < 1.0f))
    {
        float fscale = cfg.sbarscale / 20.0f;
        float h = 200 * (1 - fscale);

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        DGL_Translatef(160 - 320 * fscale / 2, h /showbar, 0);
        DGL_Scalef(fscale, fscale, 1);
    }

    // draw status bar background
    ST_refreshBackground();

    // and refresh all widgets
    ST_drawWidgets(true);

    if(cfg.sbarscale < 20 || (cfg.sbarscale == 20 && showbar < 1.0f))
    {
        // Restore the normal modelview matrix.
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }
}

void ST_HUDSpriteSize(int sprite, int *w, int *h)
{
    spriteinfo_t sprInfo;

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

void ST_drawHUDSprite(int sprite, int x, int y, int hotspot, float alpha)
{
    spriteinfo_t sprInfo;
    int     w, h;

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
    switch (hotspot)
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
                   sprInfo.lump);
}

void ST_doFullscreenStuff(void)
{
    player_t *plr = &players[displayplayer];
    char    buf[20];
    int     w, h, pos = 0, spr, i;
    int     h_width = 320 / cfg.hudScale, h_height = 200 / cfg.hudScale;
    float textalpha = hudalpha - hudHideAmount - ( 1 - cfg.hudColor[3]);
    float iconalpha = hudalpha - hudHideAmount - ( 1 - cfg.hudIconAlpha);
    int     ammo_sprite[NUM_AMMO_TYPES] = {
        SPR_AMMO,
        SPR_SBOX,
        SPR_CELL,
        SPR_ROCK
    };

    CLAMP(textalpha, 0.0f, 1.0f);
    CLAMP(iconalpha, 0.0f, 1.0f);

    if(IS_NETGAME && deathmatch && cfg.hudShown[HUD_FRAGS])
    {
        // Display the frag counter.
        i = 199 - 8;
        if(cfg.hudShown[HUD_HEALTH] || cfg.hudShown[HUD_AMMO])
        {
            i -= 18 * cfg.hudScale;
        }
        sprintf(buf, "FRAGS:%i", st_fragscount);
        M_WriteText2(2, i, buf, hu_font_a, cfg.hudColor[0], cfg.hudColor[1],
                     cfg.hudColor[2], textalpha);
    }

    // Setup the scaling matrix.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);

    // draw the visible HUD data, first health
    if(cfg.hudShown[HUD_HEALTH])
    {
        ST_drawHUDSprite(SPR_STIM, 2, h_height - 2, HOT_BLEFT, iconalpha);
        ST_HUDSpriteSize(SPR_STIM, &w, &h);
        sprintf(buf, "%i%%", plr->health);
        M_WriteText2(w + 4, h_height - 14, buf, hu_font_b, cfg.hudColor[0],
                     cfg.hudColor[1], cfg.hudColor[2], textalpha);
        pos = 60;
    }

    if(cfg.hudShown[HUD_AMMO])
    {
        ammotype_t ammotype;

        //// \todo Only supports one type of ammo per weapon.
        //// for each type of ammo this weapon takes.
        for(ammotype=0; ammotype < NUM_AMMO_TYPES; ++ammotype)
        {
            if(!weaponinfo[plr->readyWeapon][plr->class].mode[0].ammotype[ammotype])
                continue;

            spr = ammo_sprite[ammotype];
            ST_drawHUDSprite(spr, pos + 2, h_height - 2, HOT_BLEFT, iconalpha);
            ST_HUDSpriteSize(spr, &w, &h);
            sprintf(buf, "%i", plr->ammo[ammotype]);
            M_WriteText2(pos + w + 4, h_height - 14, buf, hu_font_b,
                         cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textalpha);
            break;
        }
    }

    // doom guy's face | use a bit of extra scale
    if(cfg.hudShown[HUD_FACE])
    {
        pos = (h_width/2) -(faceback.width/2) + 6;

        if(iconalpha != 0.0f)
        {
Draw_BeginZoom(0.7f, pos , h_height - 1);
            DGL_Color4f(1, 1, 1, iconalpha);
            if(IS_NETGAME)
                GL_DrawPatch_CS( pos, h_height - faceback.height + 1, faceback.lump);

            GL_DrawPatch_CS( pos, h_height - faceback.height, faces[st_faceindex].lump);
Draw_EndZoom();
        }
    }

    pos = h_width - 1;
    if(cfg.hudShown[HUD_ARMOR])
    {
        sprintf(buf, "%i%%", plr->armorPoints);
        spr = plr->armorType == 2 ? SPR_ARM2 : SPR_ARM1;
        ST_drawHUDSprite(spr, h_width - 49, h_height - 2, HOT_BRIGHT, iconalpha);
        ST_HUDSpriteSize(spr, &w, &h);

        M_WriteText2(h_width - M_StringWidth(buf, hu_font_b) - 2, h_height - 14, buf, hu_font_b,
                     cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textalpha);
        pos = h_width - w - 52;
    }

    // Keys  | use a bit of extra scale
    if(cfg.hudShown[HUD_KEYS])
    {
Draw_BeginZoom(0.75f, pos , h_height - 2);
        for(i = 0; i < 3; i++)
        {
            spr = 0;
            if(plr->
               keys[i == 0 ? KT_REDCARD : i ==
                     1 ? KT_YELLOWCARD : KT_BLUECARD])
                spr = i == 0 ? SPR_RKEY : i == 1 ? SPR_YKEY : SPR_BKEY;
            if(plr->
               keys[i == 0 ? KT_REDSKULL : i ==
                     1 ? KT_YELLOWSKULL : KT_BLUESKULL])
                spr = i == 0 ? SPR_RSKU : i == 1 ? SPR_YSKU : SPR_BSKU;
            if(spr)
            {
                ST_drawHUDSprite(spr, pos, h_height - 2, HOT_BRIGHT,iconalpha);
                ST_HUDSpriteSize(spr, &w, &h);
                pos -= w + 2;
            }
        }
Draw_EndZoom();
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void ST_Drawer(int fullscreenmode, boolean refresh)
{
    st_firsttime = st_firsttime || refresh;
    st_statusbaron = (fullscreenmode < 2) || (AM_IsMapActive(consoleplayer) &&
                     (cfg.automapHudDisplay == 0 || cfg.automapHudDisplay == 2));

    // Do palette shifts
    ST_doPaletteStuff();

    // Either slide the status bar in or fade out the fullscreen hud
    if(st_statusbaron)
    {
        if(hudalpha > 0.0f)
        {
            st_statusbaron = 0;
            hudalpha-=0.1f;
        }
        else if(showbar < 1.0f)
            showbar+=0.1f;
    }
    else
    {
        if(fullscreenmode == 3)
        {
            if(hudalpha > 0.0f)
            {
                hudalpha-=0.1f;
                fullscreenmode = 2;
            }
        }
        else
        {
            if(showbar > 0.0f)
            {
                showbar-=0.1f;
                st_statusbaron = 1;
            }
            else if(hudalpha < 1.0f)
                hudalpha+=0.1f;
        }
    }

    // Always try to render statusbar with alpha in fullscreen modes
    if(fullscreenmode)
        st_blended = 1;
    else
        st_blended = 0;

    if(st_statusbaron)
        ST_doRefresh();
    else if(fullscreenmode != 3)
        ST_doFullscreenStuff();
}

void ST_loadGraphics(void)
{

    int     i;
    int     j;
    int     facenum;

    char    namebuf[9];

    // Load the numbers, tall and short
    for(i = 0; i < 10; i++)
    {
        sprintf(namebuf, "STTNUM%d", i);
        R_CachePatch(&tallnum[i], namebuf);

        sprintf(namebuf, "STYSNUM%d", i);
        R_CachePatch(&shortnum[i], namebuf);
    }

    // Load percent key.
    // Note: why not load STMINUS here, too?
    R_CachePatch(&tallpercent, "STTPRCNT");

    // key cards
    for(i = 0; i < NUM_KEY_TYPES; i++)
    {
        sprintf(namebuf, "STKEYS%d", i);
        R_CachePatch(&keys[i], namebuf);
    }

    // arms background
    R_CachePatch(&armsbg, "STARMS");

    // arms ownership widgets
    for(i = 0; i < 6; i++)
    {
        sprintf(namebuf, "STGNUM%d", i + 2);

        // gray #
        R_CachePatch(&arms[i][0], namebuf);

        // yellow #
        memcpy(&arms[i][1], &shortnum[i + 2], sizeof(dpatch_t));
    }

    // face backgrounds for different color players
    sprintf(namebuf, "STFB%d", consoleplayer);
    R_CachePatch(&faceback, namebuf);

    // status bar background bits
    R_CachePatch(&sbar, "STBAR");

    // face states
    facenum = 0;
    for(i = 0; i < ST_NUMPAINFACES; i++)
    {
        for(j = 0; j < ST_NUMSTRAIGHTFACES; j++)
        {
            sprintf(namebuf, "STFST%d%d", i, j);
            R_CachePatch(&faces[facenum++], namebuf);
        }
        sprintf(namebuf, "STFTR%d0", i);    // turn right
        R_CachePatch(&faces[facenum++], namebuf);
        sprintf(namebuf, "STFTL%d0", i);    // turn left
        R_CachePatch(&faces[facenum++], namebuf);
        sprintf(namebuf, "STFOUCH%d", i);   // ouch!
        R_CachePatch(&faces[facenum++], namebuf);
        sprintf(namebuf, "STFEVL%d", i);    // evil grin ;)
        R_CachePatch(&faces[facenum++], namebuf);
        sprintf(namebuf, "STFKILL%d", i);   // pissed off
        R_CachePatch(&faces[facenum++], namebuf);
    }
    R_CachePatch(&faces[facenum++], "STFGOD0");
    R_CachePatch(&faces[facenum++], "STFDEAD0");
}

void ST_updateGraphics(void)
{
    char    namebuf[9];

    // face backgrounds for different color players
    sprintf(namebuf, "STFB%d", cfg.playerColor[consoleplayer]);
    R_CachePatch(&faceback, namebuf);
}

void ST_loadData(void)
{
    lu_palette = W_GetNumForName("PLAYPAL");
    ST_loadGraphics();
}

void ST_initData(void)
{
    int     i;
    player_t *plyr = &players[consoleplayer];

    st_firsttime = true;

    st_clock = 0;
    st_chatstate = StartChatState;
    st_gamestate = FirstPersonState;

    st_statusbaron = true;
    st_oldchat = st_chat = false;
    st_cursoron = false;

    st_faceindex = 0;
    st_palette = -1;

    st_oldhealth = -1;

    for(i = 0; i < NUM_WEAPON_TYPES; i++)
    {
        oldweaponsowned[i] = plyr->weaponOwned[i];
    }

    for(i = 0; i < 3; i++)
    {
        keyboxes[i] = -1;
    }

    STlib_init();

    ST_HUDUnHide(HUE_FORCE);
}

void ST_createWidgets(void)
{
    int     i;
    static int largeammo = 1994;    // means "n/a"
    static
    ammotype_t ammotype;
    boolean    found;
    player_t *plyr = &players[consoleplayer];

    //// ready weapon ammo
    //// \todo Only supports one type of ammo per weapon.
    found = false;
    for(ammotype=0; ammotype < NUM_AMMO_TYPES && !found; ++ammotype)
    {
        if(!weaponinfo[plyr->readyWeapon][plyr->class].mode[0].ammotype[ammotype])
            continue; // Weapon does not take this ammo.

        STlib_initNum(&w_ready, ST_AMMOX, ST_AMMOY, tallnum, &plyr->ammo[ammotype],
                      &st_statusbaron, ST_AMMOWIDTH, &statusbarCounterAlpha);
        found = true;
    }
    if(!found) // Weapon requires no ammo at all.
    {
        // DOOM.EXE returns an address beyond plyr->ammo[NUM_AMMO_TYPES]
        // if weaponinfo[plyr->readyWeapon].ammo == am_noammo
        // ...obviously a bug.

        //STlib_initNum(&w_ready, ST_AMMOX, ST_AMMOY, tallnum,
        //              &plyr->ammo[weaponinfo[plyr->readyWeapon].ammo],
        //              &st_statusbaron, ST_AMMOWIDTH, &statusbarCounterAlpha);


        STlib_initNum(&w_ready, ST_AMMOX, ST_AMMOY, tallnum, &largeammo,
                      &st_statusbaron, ST_AMMOWIDTH, &statusbarCounterAlpha);
    }

    // the last weapon type
    w_ready.data = plyr->readyWeapon;

    // health percentage
    STlib_initPercent(&w_health, ST_HEALTHX, ST_HEALTHY, tallnum,
                      &plyr->health, &st_statusbaron, &tallpercent,
                      &statusbarCounterAlpha);

    // weapons owned
    for(i = 0; i < 6; i++)
    {
        STlib_initMultIcon(&w_arms[i], ST_ARMSX + (i % 3) * ST_ARMSXSPACE,
                           ST_ARMSY + (i / 3) * ST_ARMSYSPACE, arms[i],
                           (int *) &plyr->weaponOwned[i + 1], &st_armson,
                           &statusbarCounterAlpha);
    }

    // frags sum
    STlib_initNum(&w_frags, ST_FRAGSX, ST_FRAGSY, tallnum, &st_fragscount,
                  &st_fragson, ST_FRAGSWIDTH, &statusbarCounterAlpha);

    // faces
    STlib_initMultIcon(&w_faces, ST_FACESX, ST_FACESY, faces, &st_faceindex,
                       &st_statusbaron, &statusbarCounterAlpha);

    // armor percentage - should be colored later
    STlib_initPercent(&w_armor, ST_ARMORX, ST_ARMORY, tallnum,
                      &plyr->armorPoints, &st_statusbaron, &tallpercent,
                      &statusbarCounterAlpha);

    // keyboxes 0-2
    STlib_initMultIcon(&w_keyboxes[0], ST_KEY0X, ST_KEY0Y, keys, &keyboxes[0],
                       &st_statusbaron, &statusbarCounterAlpha);

    STlib_initMultIcon(&w_keyboxes[1], ST_KEY1X, ST_KEY1Y, keys, &keyboxes[1],
                       &st_statusbaron, &statusbarCounterAlpha);

    STlib_initMultIcon(&w_keyboxes[2], ST_KEY2X, ST_KEY2Y, keys, &keyboxes[2],
                       &st_statusbaron, &statusbarCounterAlpha);

    // ammo count (all four kinds)
    STlib_initNum(&w_ammo[0], ST_AMMO0X, ST_AMMO0Y, shortnum, &plyr->ammo[0],
                  &st_statusbaron, ST_AMMO0WIDTH, &statusbarCounterAlpha);

    STlib_initNum(&w_ammo[1], ST_AMMO1X, ST_AMMO1Y, shortnum, &plyr->ammo[1],
                  &st_statusbaron, ST_AMMO1WIDTH, &statusbarCounterAlpha);

    STlib_initNum(&w_ammo[2], ST_AMMO2X, ST_AMMO2Y, shortnum, &plyr->ammo[2],
                  &st_statusbaron, ST_AMMO2WIDTH, &statusbarCounterAlpha);

    STlib_initNum(&w_ammo[3], ST_AMMO3X, ST_AMMO3Y, shortnum, &plyr->ammo[3],
                  &st_statusbaron, ST_AMMO3WIDTH, &statusbarCounterAlpha);

    // max ammo count (all four kinds)
    STlib_initNum(&w_maxammo[0], ST_MAXAMMO0X, ST_MAXAMMO0Y, shortnum,
                  &plyr->maxAmmo[0], &st_statusbaron, ST_MAXAMMO0WIDTH,
                  &statusbarCounterAlpha);

    STlib_initNum(&w_maxammo[1], ST_MAXAMMO1X, ST_MAXAMMO1Y, shortnum,
                  &plyr->maxAmmo[1], &st_statusbaron, ST_MAXAMMO1WIDTH,
                  &statusbarCounterAlpha);

    STlib_initNum(&w_maxammo[2], ST_MAXAMMO2X, ST_MAXAMMO2Y, shortnum,
                  &plyr->maxAmmo[2], &st_statusbaron, ST_MAXAMMO2WIDTH,
                  &statusbarCounterAlpha);

    STlib_initNum(&w_maxammo[3], ST_MAXAMMO3X, ST_MAXAMMO3Y, shortnum,
                  &plyr->maxAmmo[3], &st_statusbaron, ST_MAXAMMO3WIDTH,
                  &statusbarCounterAlpha);
}

void ST_Start(void)
{
    if(!st_stopped)
        ST_Stop();

    ST_initData();
    ST_createWidgets();
    st_stopped = false;

}

void ST_Stop(void)
{
    if(st_stopped)
        return;
    st_stopped = true;
}

void ST_Init(void)
{
    veryfirsttime = 0;
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
    int     min = 1, max = 20, *val = &cfg.sbarscale;

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
    R_SetViewSize(cfg.screenblocks, 0);
    ST_HUDUnHide(HUE_FORCE); // so the user can see the change.
    return true;
}
