/* $Id$
 *
 * Copyright (C) 1993-1996 by id Software, Inc.
 *
 * This source is available for distribution and/or modification
 * only under the terms of the DOOM Source Code License as
 * published by id Software. All rights reserved.
 *
 * The source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
 * for more details.
 */

 /*
  * g_controls.c Game controls, ticcmd building/merging
  */

// HEADER FILES ------------------------------------------------------------

#include <math.h> // required for sqrt, fabs

#if   __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JHERETIC__
#  include "jheretic.h"
#  include "p_inventory.h"
#elif __JHEXEN__
#  include "jhexen.h"
#  include "p_inventory.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "g_controls.h"

// MACROS ------------------------------------------------------------------

#if __JDOOM__ || __JHERETIC__
#  define GOTWPN(x) (cplr->weaponowned[x])
#  define ISWPN(x)  (cplr->readyweapon == x)
#endif

#define SLOWTURNTICS        6

#define JOY(x)              (x) / (100)

// TYPES -------------------------------------------------------------------

// Joystick axes.
typedef enum joyaxis_e {
    JA_X, JA_Y, JA_Z, JA_RX, JA_RY, JA_RZ, JA_SLIDER0, JA_SLIDER1,
    NUM_JOYSTICK_AXES
} joyaxis_t;

#if __JHERETIC__
struct artifacthotkey_s {
    int     action;
    int     artifact;
}
ArtifactHotkeys[] =
{
    {A_INVULNERABILITY, arti_invulnerability},
    {A_INVISIBILITY, arti_invisibility},
    {A_HEALTH, arti_health},
    {A_SUPERHEALTH, arti_superhealth},
    {A_TORCH, arti_torch},
    {A_FIREBOMB, arti_firebomb},
    {A_EGG, arti_egg},
    {A_FLY, arti_fly},
    {A_TELEPORT, arti_teleport},
    {A_PANIC, NUMARTIFACTS},
    {0, arti_none}              // Terminator.
};
#endif

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

boolean P_IsPaused(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void G_UpdateCmdControls(ticcmd_t *cmd, float elapsedTime);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean sendpause;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// Binding classes (for the dynamic event responder chain)
bindclass_t BindClasses[] = {
    {"map", GBC_CLASS1, 0, 0},
    {"mapfollowoff", GBC_CLASS2, 0, 0},
    {"menu", GBC_CLASS3, 0, 1},
    {"menuhotkey", GBC_MENUHOTKEY, 1, 0},
    {"chat", GBC_CHAT, 0, 0},
    {"message", GBC_MESSAGE, 0, 1},
    {NULL}
};

// Looking around.
int     povangle = -1;          // -1 means centered (really 0 - 7).
float   targetLookOffset = 0;
float   lookOffset = 0;

// Player movement.
fixed_t angleturn[3] = { 640, 1280, 320 };  // + slow turn

// for accelerative turning
float   turnheld;
float   lookheld;

// mouse values are used once
int     mousex;
int     mousey;

int     dclicktime;
int     dclickstate;
int     dclicks;
int     dclicktime2;
int     dclickstate2;
int     dclicks2;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int     joymove[NUM_JOYSTICK_AXES];

// CVars for control/input
cvar_t  controlCVars[] = {
// Input (settings)
    // Mouse
    {"input-mouse-x-sensi", CVF_NO_MAX, CVT_INT, &cfg.mouseSensiX, 0, 25,
        "Mouse X axis sensitivity."},
    {"input-mouse-y-sensi", CVF_NO_MAX, CVT_INT, &cfg.mouseSensiY, 0, 25,
        "Mouse Y axis sensitivity."},

    // Joystick/Gamepad
    {"input-joy-x", 0, CVT_INT, &cfg.joyaxis[0], 0, 4,
        "X axis control: 0=None, 1=Move, 2=Turn, 3=Strafe, 4=Look."},
    {"input-joy-y", 0, CVT_INT, &cfg.joyaxis[1], 0, 4,
        "Y axis control."},
    {"input-joy-z", 0, CVT_INT, &cfg.joyaxis[2], 0, 4,
        "Z axis control."},
    {"input-joy-rx", 0, CVT_INT, &cfg.joyaxis[3], 0, 4,
        "X rotational axis control."},
    {"input-joy-ry", 0, CVT_INT, &cfg.joyaxis[4], 0, 4,
        "Y rotational axis control."},
    {"input-joy-rz", 0, CVT_INT, &cfg.joyaxis[5], 0, 4,
        "Z rotational axis control."},

    {"input-joy-slider1", 0, CVT_INT, &cfg.joyaxis[6], 0, 4,
        "First slider control."},
    {"input-joy-slider2", 0, CVT_INT, &cfg.joyaxis[7], 0, 4,
        "Second slider control."},

// Control (options/preferences)
    {"ctl-aim-noauto", 0, CVT_INT, &cfg.noAutoAim, 0, 1,
        "1=Autoaiming disabled."},

    {"ctl-turn-speed", 0, CVT_FLOAT, &cfg.turnSpeed, 1, 5,
        "The speed of turning left/right."},
    {"ctl-run", 0, CVT_INT, &cfg.alwaysRun, 0, 1,
        "1=Always run."},

    {"ctl-use-dclick", 0, CVT_INT, &cfg.dclickuse, 0, 1,
        "1=Doubleclick forward/strafe equals use key."},
#if !__JDOOM__
    {"ctl-use-immediate", 0, CVT_INT, &cfg.chooseAndUse, 0, 1,
        "1=Use items immediately from the inventory."},
    {"ctl-use-next", 0, CVT_INT, &cfg.inventoryNextOnUnuse, 0, 1,
        "1=Automatically select the next inventory item when unusable."},
#endif

    {"ctl-look-speed", 0, CVT_FLOAT, &cfg.lookSpeed, 1, 5,
        "The speed of looking up/down."},
    {"ctl-look-spring", 0, CVT_INT, &cfg.lookSpring, 0, 1,
        "1=Lookspring active."},

    {"ctl-look-mouse", 0, CVT_INT, &cfg.usemlook, 0, 1,
        "1=Mouse look active."},
    {"ctl-look-mouse-inverse", 0, CVT_INT, &cfg.mlookInverseY, 0, 1,
        "1=Inverse mouse look Y axis."},

    {"ctl-look-pov", 0, CVT_BYTE, &cfg.povLookAround, 0, 1,
        "1=Look around using the POV hat."},
    {"ctl-look-joy", 0, CVT_INT, &cfg.usejlook, 0, 1,
        "1=Joystick look active."},
    {"ctl-look-joy-inverse", 0, CVT_INT, &cfg.jlookInverseY, 0, 1,
        "1=Inverse joystick look Y axis."},
    {"ctl-look-joy-delta", 0, CVT_INT, &cfg.jlookDeltaMode, 0, 1,
        "1=Joystick values => look angle delta."},

    {NULL}
};

// CODE --------------------------------------------------------------------

/*
 * Register the CVars and CCmds for input/controls.
 */
void G_ControlRegister(void)
{
    int     i;

    for(i = 0; controlCVars[i].name; i++)
        Con_AddVariable(controlCVars + i);
}

/*
 * Registers the additional bind classes the game requires
 *
 * (Doomsday manages the bind class stack which forms the
 * dynamic event responder chain).
 */
void G_BindClassRegistration(void)
{
    int i;

    Con_Message("G_PreInit: Registering Bind Classes...\n");

    for(i = 0; BindClasses[i].name; i++)
        DD_AddBindClass(BindClasses + i);
}

/*
 * Set default bindings for unbound Controls.
 */
void G_DefaultBindings(void)
{
    int     i;
    const Control_t *ctr;
    char    evname[80], cmd[256], buff[256];
    event_t event;

    // Check all Controls.
    for(i = 0; controls[i].command[0]; i++)
    {
        ctr = controls + i;
        // If this command is bound to something, skip it.
        sprintf(cmd, "%s%s", ctr->flags & CLF_ACTION ? "+" : "", ctr->command);
        memset(buff, 0, sizeof(buff));
        if(B_BindingsForCommand(cmd, buff, -1))
            continue;

        // This Control has no bindings, set it to the default.
        sprintf(buff, "\"%s\"", ctr->command);
        if(ctr->defKey)
        {
            event.type = ev_keydown;
            event.data1 = ctr->defKey;
            B_EventBuilder(evname, &event, false);
            sprintf(cmd, "%s bdc%d %s %s",
                    ctr->flags & CLF_REPEAT ? "safebindr" : "safebind",
                    controls[i].bindClass, evname + 1, buff);
            DD_Execute(cmd, true);
        }

        if(ctr->defMouse)
        {
            event.type = ev_mousebdown;
            event.data1 = 1 << (ctr->defMouse - 1);
            B_EventBuilder(evname, &event, false);
            sprintf(cmd, "%s bdc%d %s %s",
                    ctr->flags & CLF_REPEAT ? "safebindr" : "safebind",
                    controls[i].bindClass, evname + 1, buff);
            DD_Execute(cmd, true);
        }

        if(ctr->defJoy)
        {
            event.type = ev_joybdown;
            event.data1 = 1 << (ctr->defJoy - 1);
            B_EventBuilder(evname, &event, false);
            sprintf(cmd, "%s bdc%d %s %s",
                    ctr->flags & CLF_REPEAT ? "safebindr" : "safebind",
                    controls[i].bindClass, evname + 1, buff);
            DD_Execute(cmd, true);
        }
    }
}

/*
 * Offset is in 'angles', where 110 corresponds 85 degrees.
 * The delta has higher precision with small offsets.
 */
#if 0
char G_MakeLookDelta(float offset)
{
    boolean minus = offset < 0;

    offset = sqrt(fabs(offset)) * DELTAMUL;
    if(minus)
        offset = -offset;
    // It's only a char...
    if(offset > 127)
        offset = 127;
    if(offset < -128)
        offset = -128;
    return (signed char) offset;
}
#endif

/*
 * Turn client angle.  If 'elapsed' is negative, the turn delta is
 * considered an immediate change.
 */
static void G_AdjustAngle(player_t *player, int turn, float elapsed)
{
    fixed_t delta = 0;

    if(!player->plr->mo ||
        player->playerstate == PST_DEAD)
        return; // Sorry, can't help you, pal.

    delta = (fixed_t) (turn << FRACBITS);

    if(elapsed > 0)
        delta = FixedMul(delta, FLT2FIX(cfg.turnSpeed * elapsed * 35.f));

    player->plr->clAngle += delta;
}

static void G_AdjustLookDir(player_t *player, int look, float elapsed)
{
    ddplayer_t *ddplr = player->plr;

    if(look)
    {
        if(look == TOCENTER)
        {
            player->centering = true;
        }
        else
        {
            ddplr->clLookDir += cfg.lookSpeed * look * elapsed * 35;
        }
    }

    if(player->centering)
    {
        float step = 8 * elapsed * 35;

        if(ddplr->clLookDir > step)
        {
            ddplr->clLookDir -= step;
        }
        else if(ddplr->clLookDir < -step)
        {
            ddplr->clLookDir += step;
        }
        else
        {
            ddplr->clLookDir = 0;
            player->centering = false;
        }
    }
}

/*
 * Updates the viewers' look angle.
 * Called every tic by G_Ticker.
 */
void G_LookAround(void)
{
    if(povangle != -1)
    {
        targetLookOffset = povangle / 8.0f;
        if(targetLookOffset == .5f)
        {
            if(lookOffset < 0)
                targetLookOffset = -.5f;
        }
        else if(targetLookOffset > .5)
            targetLookOffset -= 1;
    }
    else
        targetLookOffset = 0;

    if(targetLookOffset != lookOffset && cfg.povLookAround)
    {
        float   diff = (targetLookOffset - lookOffset) / 2;

        // Clamp it.
        if(diff > .075f)
            diff = .075f;
        if(diff < -.075f)
            diff = -.075f;

        lookOffset += diff;
    }
}

static void G_SetCmdViewAngles(ticcmd_t *cmd, player_t *pl)
{
    // These will be sent to the server (or P_MovePlayer).
    cmd->angle = pl->plr->clAngle >> 16;

    // Clamp it. 110 corresponds 85 degrees.
    if(pl->plr->clLookDir > 110)
        pl->plr->clLookDir = 110;
    if(pl->plr->clLookDir < -110)
        pl->plr->clLookDir = -110;

    cmd->pitch = pl->plr->clLookDir / 110 * DDMAXSHORT;
}

/*
 * Builds a ticcmd from all of the available inputs.
 */
void G_BuildTiccmd(ticcmd_t *cmd, float elapsedTime)
{
    player_t *cplr = &players[consoleplayer];

    memset(cmd, 0, sizeof(*cmd));

    // During demo playback, all cmds will be blank.
    if(Get(DD_PLAYBACK))
        return;

    G_UpdateCmdControls(cmd, elapsedTime);

    G_SetCmdViewAngles(cmd, cplr);

    // special buttons
    if(sendpause)
    {
        sendpause = false;
        // Clients can't pause anything.
        if(!IS_CLIENT)
            cmd->pause = true;
    }

    if(IS_CLIENT)
    {
        // Clients mirror their local commands.
        memcpy(&players[consoleplayer].cmd, cmd, sizeof(*cmd));
    }
}

/*
 * Combine the source ticcmd with the destination ticcmd.  This is
 * done when there are multiple ticcmds to execute on a single game
 * tick.
 */
void G_MergeTiccmd(ticcmd_t *dest, ticcmd_t *src)
{
    dest->forwardMove = src->forwardMove;
    dest->sideMove = src->sideMove;

    dest->angle = src->angle;
    dest->pitch = src->pitch;

    dest->fly = src->fly;

    if(src->arti)
        dest->arti = src->arti;

    if(src->changeWeapon)
        dest->changeWeapon = src->changeWeapon;

    dest->attack |= src->attack;
    dest->use |= src->use;
    dest->jump |= src->jump;
    dest->pause |= src->pause;
}

/*
 * Response to in-game control actions (movement, inventory etc).
 * Updates the ticcmd with the current control states.
 */
static void G_UpdateCmdControls(ticcmd_t *cmd, float elapsedTime)
{
    static boolean mlook_pressed = false;
    float elapsedTics = elapsedTime * 35;

    boolean pausestate = P_IsPaused();
    int     i;
    boolean strafe = 0;
    boolean bstrafe = 0;
    int     speed = 0;
    int     tspeed = 0;
    int     forward = 0;
    int     side = 0;
    int     turn = 0;
    player_t *cplr = &players[consoleplayer];
    int     joyturn = 0, joystrafe = 0, joyfwd = 0, joylook = 0;
    int    *axes[5] = { 0, &joyfwd, &joyturn, &joystrafe, &joylook };
    int     look = 0, lspeed = 0;
    int     flyheight = 0;
    int     pClass = players[consoleplayer].class;

    // Check the joystick axes.
    for(i = 0; i < 8; i++)
        if(axes[cfg.joyaxis[i]])
            *axes[cfg.joyaxis[i]] += joymove[i];

    strafe = actions[A_STRAFE].on;
    speed = actions[A_SPEED].on;

    // Walk -> run, run -> walk.
    if(cfg.alwaysRun)
        speed = !speed;

    // Use two stage accelerative turning on the keyboard and joystick.
    if(joyturn < -0
       || joyturn > 0
       || actions[A_TURNRIGHT].on || actions[A_TURNLEFT].on)
        turnheld += elapsedTics;
    else
        turnheld = 0;

    if(turnheld < SLOWTURNTICS)
        tspeed = 2;             // slow turn
    else
        tspeed = speed;

    // Determine the appropriate look speed based on how long the key
    // has been held down.
    if(actions[A_LOOKDOWN].on || actions[A_LOOKUP].on)
        lookheld += elapsedTics;
    else
        lookheld = 0;

    if(lookheld < SLOWTURNTICS)
        lspeed = 1;
    else
        lspeed = 2;

    // let movement keys cancel each other out
    if(strafe)
    {
        if(actions[A_TURNRIGHT].on)
            side += PCLASS_INFO(pClass)->sidemove[speed];
        if(actions[A_TURNLEFT].on)
            side -= PCLASS_INFO(pClass)->sidemove[speed];

        // Swap strafing and turning.
        i = joystrafe;
        joystrafe = joyturn;
        joyturn = i;
    }
    else
    {
        if(actions[A_TURNRIGHT].on)
            turn -= angleturn[tspeed];
        if(actions[A_TURNLEFT].on)
            turn += angleturn[tspeed];
    }

    // Joystick turn.
    if(joyturn > 0)
        turn -= angleturn[tspeed] * JOY(joyturn);
    if(joyturn < -0)
        turn += angleturn[tspeed] * JOY(-joyturn);

    // Joystick strafe.
    if(joystrafe < -0)
        side -= PCLASS_INFO(pClass)->sidemove[speed] * JOY(-joystrafe);
    if(joystrafe > 0)
        side += PCLASS_INFO(pClass)->sidemove[speed] * JOY(joystrafe);

    if(actions[A_FORWARD].on)
        forward += PCLASS_INFO(pClass)->forwardmove[speed];

    if(actions[A_BACKWARD].on)
        forward -= PCLASS_INFO(pClass)->forwardmove[speed];

    if(joyfwd < -0)
        forward += PCLASS_INFO(pClass)->forwardmove[speed] * JOY(-joyfwd);
    if(joyfwd > 0)
        forward -= PCLASS_INFO(pClass)->forwardmove[speed] * JOY(joyfwd);

    if(actions[A_STRAFERIGHT].on)
        side += PCLASS_INFO(pClass)->sidemove[speed];
    if(actions[A_STRAFELEFT].on)
        side -= PCLASS_INFO(pClass)->sidemove[speed];

    if(joystrafe < -0)
        side -= PCLASS_INFO(pClass)->sidemove[speed] * JOY(-joystrafe);
    if(joystrafe > 0)
        side += PCLASS_INFO(pClass)->sidemove[speed] * JOY(joystrafe);

    // Look up/down/center keys
    if(!cfg.lookSpring || (cfg.lookSpring && !forward))
    {
        if(actions[A_LOOKUP].on)
        {
            look = lspeed;
        }
        if(actions[A_LOOKDOWN].on)
        {
            look = -lspeed;
        }
        if(actions[A_LOOKCENTER].on)
        {
            look = TOCENTER;
        }
    }

    // Fly up/down/drop keys
    if(actions[A_FLYUP].on)
        // note that the actual flyheight will be twice this
        flyheight = 5;

    if(actions[A_FLYDOWN].on)
        flyheight = -5;

    if(actions[A_FLYCENTER].on)
    {
        flyheight = TOCENTER;

#if __JHERETIC__
        if(!cfg.usemlook) // only in jHeretic
            look = TOCENTER;
#else
        look = TOCENTER;
#endif
    }

#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    // Use artifact key
    if(actions[A_USEARTIFACT].on)
    {
        if(actions[A_SPEED].on && artiskip)
        {
            if(players[consoleplayer].inventory[inv_ptr].type != arti_none)
            {
                actions[A_USEARTIFACT].on = false;

                cmd->arti = 0xff;
            }
        }
        else
        {
            if(ST_IsInventoryVisible())
            {
                players[consoleplayer].readyArtifact =
                    players[consoleplayer].inventory[inv_ptr].type;

                ST_Inventory(false); // close the inventory

                if(cfg.chooseAndUse)
                    cmd->arti =
                        players[consoleplayer].inventory[inv_ptr].type;
                else
                    cmd->arti = 0;

                usearti = false;
            }
            else if(usearti)
            {
                cmd->arti = players[consoleplayer].inventory[inv_ptr].type;
                usearti = false;
            }
        }
    }
#endif

    //
    // Artifact hot keys
    //
#if __JHERETIC__
    // Check Tome of Power and other artifact hotkeys.
    if(actions[A_TOMEOFPOWER].on && !cmd->arti &&
       !players[consoleplayer].powers[pw_weaponlevel2])
    {
        actions[A_TOMEOFPOWER].on = false;
        cmd->arti = arti_tomeofpower;
    }
    for(i = 0; ArtifactHotkeys[i].artifact != arti_none && !cmd->arti; i++)
    {
        if(actions[ArtifactHotkeys[i].action].on)
        {
            actions[ArtifactHotkeys[i].action].on = false;
            cmd->arti = ArtifactHotkeys[i].artifact;
            break;
        }
    }
#endif

#if __JHEXEN__ || __JSTRIFE__
    if(actions[A_PANIC].on && !cmd->arti)
    {
        actions[A_PANIC].on = false;    // Use one of each artifact
        cmd->arti = NUMARTIFACTS;
    }
    else if(players[consoleplayer].plr->mo && actions[A_HEALTH].on &&
            !cmd->arti && (players[consoleplayer].plr->mo->health < MAXHEALTH))
    {
        actions[A_HEALTH].on = false;
        cmd->arti = arti_health;
    }
    else if(actions[A_POISONBAG].on && !cmd->arti)
    {
        actions[A_POISONBAG].on = false;
        cmd->arti = arti_poisonbag;
    }
    else if(actions[A_BLASTRADIUS].on && !cmd->arti)
    {
        actions[A_BLASTRADIUS].on = false;
        cmd->arti = arti_blastradius;
    }
    else if(actions[A_TELEPORT].on && !cmd->arti)
    {
        actions[A_TELEPORT].on = false;
        cmd->arti = arti_teleport;
    }
    else if(actions[A_TELEPORTOTHER].on && !cmd->arti)
    {
        actions[A_TELEPORTOTHER].on = false;
        cmd->arti = arti_teleportother;
    }
    else if(actions[A_EGG].on && !cmd->arti)
    {
        actions[A_EGG].on = false;
        cmd->arti = arti_egg;
    }
    else if(actions[A_INVULNERABILITY].on && !cmd->arti &&
            !players[consoleplayer].powers[pw_invulnerability])
    {
        actions[A_INVULNERABILITY].on = false;
        cmd->arti = arti_invulnerability;
    }
    else if(actions[A_MYSTICURN].on && !cmd->arti)
    {
        actions[A_MYSTICURN].on = false;
        cmd->arti = arti_superhealth;
    }
    else if(actions[A_TORCH].on && !cmd->arti)
    {
        actions[A_TORCH].on = false;
        cmd->arti = arti_torch;
    }
    else if(actions[A_KRATER].on && !cmd->arti)
    {
        actions[A_KRATER].on = false;
        cmd->arti = arti_boostmana;
    }
    else if(actions[A_SPEEDBOOTS].on & !cmd->arti)
    {
        actions[A_SPEEDBOOTS].on = false;
        cmd->arti = arti_speed;
    }
    else if(actions[A_DARKSERVANT].on && !cmd->arti)
    {
        actions[A_DARKSERVANT].on = false;
        cmd->arti = arti_summon;
    }
#endif

    // Buttons

    if(actions[A_FIRE].on)
        cmd->attack = true;

    if(actions[A_USE].on)
    {
        cmd->use = true;
        // clear double clicks if hit use button
        dclicks = 0;
    }

    if(actions[A_JUMP].on)
        cmd->jump = true;

#if __JDOOM__
    // Determine whether a weapon change should be done.
    if(actions[A_WEAPONCYCLE1].on)  // Fist/chainsaw.
    {
        if(ISWPN(wp_fist) && GOTWPN(wp_chainsaw))
            i = wp_chainsaw;
        else if(ISWPN(wp_chainsaw))
            i = wp_fist;
        else if(GOTWPN(wp_chainsaw))
            i = wp_chainsaw;
        else
            i = wp_fist;

        cmd->changeWeapon = i + 1;
    }
    else if(actions[A_WEAPONCYCLE2].on) // Shotgun/super sg.
    {
        if(ISWPN(wp_shotgun) && GOTWPN(wp_supershotgun) &&
           gamemode == commercial)
            i = wp_supershotgun;
        else if(ISWPN(wp_supershotgun))
            i = wp_shotgun;
        else if(GOTWPN(wp_supershotgun) && gamemode == commercial)
            i = wp_supershotgun;
        else
            i = wp_shotgun;

        cmd->changeWeapon = i + 1;
    }
    else
#elif __JHERETIC__
    // Determine whether a weapon change should be done.
    if(actions[A_WEAPONCYCLE1].on)  // Staff/Gauntlets.
    {
        if(ISWPN(WP_FIRST) && GOTWPN(WP_EIGHTH))
            i = WP_EIGHTH;
        else if(ISWPN(WP_EIGHTH))
            i = WP_FIRST;
        else if(GOTWPN(WP_EIGHTH))
            i = WP_EIGHTH;
        else
            i = WP_FIRST;

        cmd->changeWeapon = i + 1;
    }
    else
#endif
    {
        // Take the first weapon action.
        for(i = 0; i < NUMWEAPONS; i++)
            if(actions[A_WEAPON1 + i].on)
            {
                cmd->changeWeapon = i + 1;
                break;
            }
    }

    if(actions[A_NEXTWEAPON].on || actions[A_PREVIOUSWEAPON].on)
    {
        cmd->changeWeapon =
            (actions[A_NEXTWEAPON].on ? TICCMD_NEXT_WEAPON :
             TICCMD_PREV_WEAPON);
    }

    // forward double click
    if(actions[A_FORWARD].on != dclickstate && dclicktime > 1 && cfg.dclickuse)
    {
        dclickstate = actions[A_FORWARD].on;

        if(dclickstate)
            dclicks++;
        if(dclicks == 2)
        {
            cmd->use = true;
            dclicks = 0;
        }
        else
            dclicktime = 0;
    }
    else
    {
        dclicktime++;
        if(dclicktime > 20)
        {
            dclicks = 0;
            dclickstate = 0;
        }
    }

    // strafe double click
    bstrafe = strafe;
    if(bstrafe != dclickstate2 && dclicktime2 > 1 && cfg.dclickuse)
    {
        dclickstate2 = bstrafe;
        if(dclickstate2)
            dclicks2++;
        if(dclicks2 == 2)
        {
            cmd->use = true;
            dclicks2 = 0;
        }
        else
            dclicktime2 = 0;
    }
    else
    {
        dclicktime2++;
        if(dclicktime2 > 20)
        {
            dclicks2 = 0;
            dclickstate2 = 0;
        }
    }

    // Mouse strafe and turn (X axis).
    if(strafe)
        side += mousex * 2;
    else
    {
        //turn -= mousex * 8;
        // Mouse angle changes are immediate.
        if(!pausestate)
            G_AdjustAngle(cplr, mousex * -8, -1);
    }

    if(!pausestate)
    {
        // Speed based turning.
        G_AdjustAngle(cplr, turn, elapsedTime);

        if(strafe || (!cfg.usemlook && !actions[A_MLOOK].on) ||
           players[consoleplayer].playerstate == PST_DEAD)
        {
            forward += 8 * mousey * elapsedTics;
        }
        else
        {
            float adj =
                (((mousey * 8) << 16) / (float) ANGLE_180) * 180 *
                110.0 / 85.0;

            if(cfg.mlookInverseY)
                adj = -adj;
            cplr->plr->clLookDir += adj;
        }
        if(cfg.usejlook)
        {
            if(cfg.jlookDeltaMode)
                cplr->plr->clLookDir +=
                    joylook / 20.0f * cfg.lookSpeed *
                    (cfg.jlookInverseY ? -1 : 1) * elapsedTics;
            else
                cplr->plr->clLookDir =
                    joylook * 1.1f * (cfg.jlookInverseY ? -1 : 1);
        }
    }

    G_ResetMousePos();

#define MAXPLMOVE PCLASS_INFO(pClass)->maxmove

    if(forward > MAXPLMOVE)
        forward = MAXPLMOVE;
    else if(forward < -MAXPLMOVE)
        forward = -MAXPLMOVE;
    if(side > MAXPLMOVE)
        side = MAXPLMOVE;
    else if(side < -MAXPLMOVE)
        side = -MAXPLMOVE;

#if __JHEXEN__
    if(cplr->powers[pw_speed] && !cplr->morphTics)
    {
        // Adjust for a player with a speed artifact
        forward = (3 * forward) >> 1;
        side = (3 * side) >> 1;
    }
#endif

    if(cfg.playerMoveSpeed > 1)
        cfg.playerMoveSpeed = 1;

    forward *= cfg.playerMoveSpeed;
    side *= cfg.playerMoveSpeed;

    cmd->forwardMove += forward;
    cmd->sideMove += side;

    if(cfg.lookSpring && !actions[A_MLOOK].on &&
       (cmd->forwardMove > MAXPLMOVE / 3 || cmd->forwardMove < -MAXPLMOVE / 3
        || cmd->sideMove > MAXPLMOVE / 3 || cmd->sideMove < -MAXPLMOVE / 3 ||
        mlook_pressed))
    {
        // Center view when mlook released w/lookspring, or when moving.
        look = TOCENTER;
    }

    if(players[consoleplayer].playerstate == PST_LIVE && !pausestate)
        G_AdjustLookDir(cplr, look, elapsedTime);

    cmd->fly = flyheight;

    // Store the current mlook key state.
    mlook_pressed = actions[A_MLOOK].on;
}

/*
 * Called by G_Responder.
 * Depending on the type of the event we may wish to eat it before
 * it is sent to the engine to check for bindings.
 *
 * TODO: all controls should be handled by the engine.
 * Merge in engine-side axis controls from 1.8 alpha.
 *
 * @return boolean  (True) If the event should be checked for bindings
 */
boolean G_AdjustControlState(event_t* ev)
{
    switch (ev->type)
    {
    case ev_keydown:
        return false;

    case ev_keyup:
        return false;           // always let key up events filter down

    case ev_keyrepeat:
        return false;

    case ev_mouse:
        mousex += ev->data1 * (1 + cfg.mouseSensiX/5.0f);
        mousey += ev->data2 * (1 + cfg.mouseSensiY/5.0f);
        return true;            // eat events

    case ev_mousebdown:
        return false;

    case ev_mousebup:
        return false;

    case ev_joystick:           // Joystick movement
        joymove[JA_X] = ev->data1;
        joymove[JA_Y] = ev->data2;
        joymove[JA_Z] = ev->data3;
        joymove[JA_RX] = ev->data4;
        joymove[JA_RY] = ev->data5;
        joymove[JA_RZ] = ev->data6;
        return true;            // eat events

    case ev_joyslider:          // Joystick slider movement
        joymove[JA_SLIDER0] = ev->data1;
        joymove[JA_SLIDER1] = ev->data2;
        return true;

    case ev_joybdown:
        return false;           // eat events

    case ev_joybup:
        return false;           // eat events

    case ev_povup:
        if(!automapactive && !menuactive)
        {
            povangle = -1;
            // If looking around with PoV, don't allow bindings.
            if(cfg.povLookAround)
                return true;
        }
        break;

    case ev_povdown:
        if(!automapactive && !menuactive)
        {
            povangle = ev->data1;
            if(cfg.povLookAround)
                return true;
        }
        break;

    default:
        break;
    }

    return false;
}

/*
 * Resets the mouse position to 0,0
 * Called eg when loading a new level.
 */
void G_ResetMousePos(void)
{
    mousex = mousey = 0;
}
