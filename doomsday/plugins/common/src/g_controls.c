/**
 *\section License
 * License: GPL + jHeretic/jHexen Exception
 *
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999-2006 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © Raven Software, Corp.
 *\author Copyright © 1993-1996 by id Software, Inc.
 */

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

#if  __DOOM64TC__
#  include "doom64tc.h"
#elif __WOLFTC__
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
#include "p_tick.h" // for P_IsPaused()
#include "d_netsv.h"

// MACROS ------------------------------------------------------------------

#if __JDOOM__ || __JHERETIC__
#  define GOTWPN(x) (plr->weaponowned[x])
#  define ISWPN(x)  (plr->readyweapon == x)
#endif

#define SLOWTURNTICS        6

#define JOY(x)              (x) / (100)

// TYPES -------------------------------------------------------------------

typedef struct pcontrolstate_s {
    // Looking around.
    float   targetLookOffset;
    float   lookOffset;
    boolean mlookPressed;

    // for accelerative turning
    float   turnheld;
    float   lookheld;

    int     dclicktime;
    int     dclickstate;
    int     dclicks;
    int     dclicktime2;
    int     dclickstate2;
    int     dclicks2;
} pcontrolstate_t;

// Joystick axes.
typedef enum joyaxis_e {
    JA_X, JA_Y, JA_Z, JA_RX, JA_RY, JA_RZ, JA_SLIDER0, JA_SLIDER1,
    NUM_JOYSTICK_AXES
} joyaxis_t;

#if 0 // FIXME! __JHERETIC__
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

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void G_UpdateCmdControls(ticcmd_t *cmd, int pnum, float elapsedTime);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean sendpause;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// Binding classes (for the dynamic event responder chain)
bindclass_t BindClasses[] = {
    {"map",             GBC_CLASS1,     0, 0},
    {"mapfollowoff",    GBC_CLASS2,     0, 0},
    {"menu",            GBC_CLASS3,     0, BCF_ABSOLUTE},
    {"menuhotkey",      GBC_MENUHOTKEY, 1, 0},
    {"chat",            GBC_CHAT,       0, 0},
    {"message",         GBC_MESSAGE,    0, BCF_ABSOLUTE},
    {NULL}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// Input devices; state controls.
static int     joymove[NUM_JOYSTICK_AXES]; // joy axis state
static int     povangle = -1;          // -1 means centered (really 0 - 7).
static float   mousex;
static float   mousey;

// Player control state.
static pcontrolstate_t controlStates[MAXPLAYERS];

// CVars for control/input
cvar_t  controlCVars[] = {
// Control (options/preferences)
    {"ctl-aim-noauto", 0, CVT_INT, &cfg.noAutoAim, 0, 1},

    {"ctl-turn-speed", 0, CVT_FLOAT, &cfg.turnSpeed, 1, 5},
    {"ctl-run", 0, CVT_INT, &cfg.alwaysRun, 0, 1},

    {"ctl-use-dclick", 0, CVT_INT, &cfg.dclickuse, 0, 1},
#if !__JDOOM__
    {"ctl-use-immediate", 0, CVT_INT, &cfg.chooseAndUse, 0, 1},
    {"ctl-use-next", 0, CVT_INT, &cfg.inventoryNextOnUnuse, 0, 1},
#endif

    {"ctl-look-speed", 0, CVT_FLOAT, &cfg.lookSpeed, 1, 5},
    {"ctl-look-spring", 0, CVT_INT, &cfg.lookSpring, 0, 1},

    {"ctl-look-mouse", 0, CVT_INT, &cfg.usemlook, 0, 1},

    {"ctl-look-pov", 0, CVT_BYTE, &cfg.povLookAround, 0, 1},
    {"ctl-look-joy", 0, CVT_INT, &cfg.usejlook, 0, 1},
    {"ctl-look-joy-delta", 0, CVT_INT, &cfg.jlookDeltaMode, 0, 1},
    {NULL}
};

// CODE --------------------------------------------------------------------

/**
 * Register the CVars and CCmds for input/controls.
 */
void G_ControlRegister(void)
{
    uint        i;

    for(i = 0; controlCVars[i].name; ++i)
        Con_AddVariable(controlCVars + i);
}

/**
 * Registers the additional bind classes the game requires
 *
 * (Doomsday manages the bind class stack which forms the
 * dynamic event responder chain).
 */
void G_RegisterBindClasses(void)
{
    uint        i;

    Con_Message("G_PreInit: Registering Bind Classes...\n");

    for(i = 0; BindClasses[i].name; ++i)
        DD_AddBindClass(BindClasses + i);
}

/**
 * Retrieve the look offset for the given player.
 */
float G_GetLookOffset(int pnum)
{
    return controlStates[pnum].lookOffset;
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
 * Turn client angle.
 */
static void G_AdjustAngle(player_t *player, int turn, float elapsed)
{
    if(!player->plr->mo || player->playerstate == PST_DEAD ||
       player->viewlock)
        return; // Sorry, can't help you, pal.

    /* $unifiedangles */
    player->plr->mo->angle += FLT2FIX(cfg.turnSpeed * elapsed * 35.f * turn);
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
            ddplr->lookdir += cfg.lookSpeed * look * elapsed * 35; /* $unifiedangles */
        }
    }

    if(player->centering)
    {
        float step = 8 * elapsed * 35;

        /* $unifiedangles */
        if(ddplr->lookdir > step)
        {
            ddplr->lookdir -= step;
        }
        else if(ddplr->lookdir < -step)
        {
            ddplr->lookdir += step;
        }
        else
        {
            ddplr->lookdir = 0;
            player->centering = false;
        }
    }
}

/**
 * Updates the viewers' look angle.
 * Called every tic by G_Ticker.
 */
void G_LookAround(int pnum)
{
    pcontrolstate_t *cstate = &controlStates[pnum];

    if(povangle != -1)
    {
        cstate->targetLookOffset = povangle / 8.0f;
        if(cstate->targetLookOffset == .5f)
        {
            if(cstate->lookOffset < 0)
                cstate->targetLookOffset = -.5f;
        }
        else if(cstate->targetLookOffset > .5)
            cstate->targetLookOffset -= 1;
    }
    else
        cstate->targetLookOffset = 0;

    if(cstate->targetLookOffset != cstate->lookOffset && cfg.povLookAround)
    {
        float   diff = (cstate->targetLookOffset - cstate->lookOffset) / 2;

        // Clamp it.
        if(diff > .075f)
            diff = .075f;
        if(diff < -.075f)
            diff = -.075f;

        cstate->lookOffset += diff;
    }
}

#if 0
/**
 * Builds a ticcmd from all of the available inputs.
 */
void G_BuildTiccmd(ticcmd_t *cmd, float elapsedTime)
{
    player_t *cplr = &players[consoleplayer];

    memset(cmd, 0, sizeof(*cmd));

    // During demo playback, all cmds will be blank.
    if(Get(DD_PLAYBACK))
        return;

    G_UpdateCmdControls(cmd, consoleplayer, elapsedTime);

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
        // Clients mirror their local commands.#endif
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

/**
 * Response to in-game control actions (movement, inventory etc).
 * Updates the ticcmd with the current control states.
 */
static void G_UpdateCmdControls(ticcmd_t *cmd, int pnum,
                                float elapsedTime)
{
    float elapsedTics = elapsedTime * 35;

    boolean pausestate = P_IsPaused();
    int     i;
    boolean strafe = 0;
    boolean bstrafe = 0;
    int     speed = 0;
    int     turnSpeed = 0, fwdMoveSpeed = 0, sideMoveSpeed = 0;
    int     forward = 0;
    int     side = 0;
    int     turn = 0;
    int     joyturn = 0, joystrafe = 0, joyfwd = 0, joylook = 0;
    int    *axes[5] = { 0, &joyfwd, &joyturn, &joystrafe, &joylook };
    int     look = 0, lspeed = 0;
    int     flyheight = 0;
    pcontrolstate_t *cstate = &controlStates[pnum];
    player_t *plr = &players[pnum];
    classinfo_t *pClassInfo = PCLASS_INFO(plr->class);

    // Check the joystick axes.
    for(i = 0; i < 8; i++)
        if(axes[cfg.joyaxis[i]])
            *axes[cfg.joyaxis[i]] += joymove[i];

    strafe = PLAYER_ACTION(pnum, A_STRAFE);
    speed = PLAYER_ACTION(pnum, A_SPEED);

    // Walk -> run, run -> walk.
    if(cfg.alwaysRun)
        speed = !speed;

    // Use two stage accelerative turning on the keyboard and joystick.
    if(joyturn < -0 || joyturn > 0 ||
       PLAYER_ACTION(pnum, A_TURNRIGHT) ||
       PLAYER_ACTION(pnum, A_TURNLEFT))
        cstate->turnheld += elapsedTics;
    else
        cstate->turnheld = 0;

    // Determine the appropriate look speed based on how long the key
    // has been held down.
    if(PLAYER_ACTION(pnum, A_LOOKDOWN) || PLAYER_ACTION(pnum, A_LOOKUP))
        cstate->lookheld += elapsedTics;
    else
        cstate->lookheld = 0;

    if(cstate->lookheld < SLOWTURNTICS)
        lspeed = 1;
    else
        lspeed = 2;

    // Return the max speed for the player's class.
    // FIXME: the Turbo movement multiplier should happen server-side!
    sideMoveSpeed = pClassInfo->sidemove[speed] * turbomul;
    fwdMoveSpeed = pClassInfo->forwardmove[speed] * turbomul;
    turnSpeed = pClassInfo->turnSpeed[(cstate->turnheld < SLOWTURNTICS ? 2 : speed)];

    // let movement keys cancel each other out
    if(strafe)
    {
        if(PLAYER_ACTION(pnum, A_TURNRIGHT))
            side += sideMoveSpeed;
        if(PLAYER_ACTION(pnum, A_TURNLEFT))
            side -= sideMoveSpeed;

        // Swap strafing and turning.
        i = joystrafe;
        joystrafe = joyturn;
        joyturn = i;
    }
    else
    {
        if(PLAYER_ACTION(pnum, A_TURNRIGHT))
            turn -= turnSpeed;
        if(PLAYER_ACTION(pnum, A_TURNLEFT))
            turn += turnSpeed;
    }

    // Joystick turn.
    if(joyturn > 0)
        turn -= turnSpeed * JOY(joyturn);
    if(joyturn < -0)
        turn += turnSpeed * JOY(-joyturn);

    // Joystick strafe.
    if(joystrafe < -0)
        side -= sideMoveSpeed * JOY(-joystrafe);
    if(joystrafe > 0)
        side += sideMoveSpeed * JOY(joystrafe);

    if(joyfwd < -0)
        forward += fwdMoveSpeed * JOY(-joyfwd);
    if(joyfwd > 0)
        forward -= fwdMoveSpeed * JOY(joyfwd);


    if(PLAYER_ACTION(pnum, A_FORWARD))
        forward += fwdMoveSpeed;

    if(PLAYER_ACTION(pnum, A_BACKWARD))
        forward -= fwdMoveSpeed;

    if(PLAYER_ACTION(pnum, A_STRAFERIGHT))
        side += sideMoveSpeed;
    if(PLAYER_ACTION(pnum, A_STRAFELEFT))
        side -= sideMoveSpeed;

    // Look up/down/center keys
    if(!cfg.lookSpring || (cfg.lookSpring && !forward))
    {
        if(PLAYER_ACTION(pnum, A_LOOKUP))
        {
            look = lspeed;
        }
        if(PLAYER_ACTION(pnum, A_LOOKDOWN))
        {
            look = -lspeed;
        }
        if(PLAYER_ACTION(pnum, A_LOOKCENTER))
        {
            look = TOCENTER;
        }
    }

    // Fly up/down/drop keys
    if(PLAYER_ACTION(pnum, A_FLYUP))
        // note that the actual flyheight will be twice this
        flyheight = 5;

    if(PLAYER_ACTION(pnum, A_FLYDOWN))
        flyheight = -5;

    if(PLAYER_ACTION(pnum, A_FLYCENTER))
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
    if(PLAYER_ACTION(pnum, A_USEARTIFACT))
    {
        if(PLAYER_ACTION(pnum, A_SPEED) && artiskip)
        {
            if(plr->inventory[plr->inv_ptr].type != arti_none)
            {
                PLAYER_ACTION(pnum, A_USEARTIFACT) = false;

                cmd->arti = 0xff;
            }
        }
        else
        {
            if(ST_IsInventoryVisible())
            {
                plr->readyArtifact = plr->inventory[plr->inv_ptr].type;

                ST_Inventory(false); // close the inventory

                if(cfg.chooseAndUse)
                    cmd->arti = plr->inventory[plr->inv_ptr].type;
                else
                    cmd->arti = 0;

                usearti = false;
            }
            else if(usearti)
            {
                cmd->arti = plr->inventory[plr->inv_ptr].type;
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
    if(PLAYER_ACTION(pnum, A_TOMEOFPOWER) && !cmd->arti &&
       !plr->powers[PT_WEAPONLEVEL2])
    {
        PLAYER_ACTION(pnum, A_TOMEOFPOWER) = false;
        cmd->arti = arti_tomeofpower;
    }
    for(i = 0; ArtifactHotkeys[i].artifact != arti_none && !cmd->arti; i++)
    {
        if(PLAYER_ACTION(pnum, ArtifactHotkeys[i].action))
        {
            PLAYER_ACTION(pnum, ArtifactHotkeys[i].action) = false;
            cmd->arti = ArtifactHotkeys[i].artifact;
            break;
        }
    }
#endif

#if __JHEXEN__ || __JSTRIFE__
    if(PLAYER_ACTION(pnum, A_PANIC) && !cmd->arti)
    {
        PLAYER_ACTION(pnum, A_PANIC) = false;    // Use one of each artifact
        cmd->arti = NUMARTIFACTS;
    }
    else if(plr->plr->mo && PLAYER_ACTION(pnum, A_HEALTH) &&
            !cmd->arti && (plr->plr->mo->health < MAXHEALTH))
    {
        PLAYER_ACTION(pnum, A_HEALTH) = false;
        cmd->arti = arti_health;
    }
    else if(PLAYER_ACTION(pnum, A_POISONBAG) && !cmd->arti)
    {
        PLAYER_ACTION(pnum, A_POISONBAG) = false;
        cmd->arti = arti_poisonbag;
    }
    else if(PLAYER_ACTION(pnum, A_BLASTRADIUS) && !cmd->arti)
    {
        PLAYER_ACTION(pnum, A_BLASTRADIUS) = false;
        cmd->arti = arti_blastradius;
    }
    else if(PLAYER_ACTION(pnum, A_TELEPORT) && !cmd->arti)
    {
        PLAYER_ACTION(pnum, A_TELEPORT) = false;
        cmd->arti = arti_teleport;
    }
    else if(PLAYER_ACTION(pnum, A_TELEPORTOTHER) && !cmd->arti)
    {
        PLAYER_ACTION(pnum, A_TELEPORTOTHER) = false;
        cmd->arti = arti_teleportother;
    }
    else if(PLAYER_ACTION(pnum, A_EGG) && !cmd->arti)
    {
        PLAYER_ACTION(pnum, A_EGG) = false;
        cmd->arti = arti_egg;
    }
    else if(PLAYER_ACTION(pnum, A_INVULNERABILITY) && !cmd->arti &&
            !plr->powers[PT_INVULNERABILITY])
    {
        PLAYER_ACTION(pnum, A_INVULNERABILITY) = false;
        cmd->arti = arti_invulnerability;
    }
    else if(PLAYER_ACTION(pnum, A_MYSTICURN) && !cmd->arti)
    {
        PLAYER_ACTION(pnum, A_MYSTICURN) = false;
        cmd->arti = arti_superhealth;
    }
    else if(PLAYER_ACTION(pnum, A_TORCH) && !cmd->arti)
    {
        PLAYER_ACTION(pnum, A_TORCH) = false;
        cmd->arti = arti_torch;
    }
    else if(PLAYER_ACTION(pnum, A_KRATER) && !cmd->arti)
    {
        PLAYER_ACTION(pnum, A_KRATER) = false;
        cmd->arti = arti_boostmana;
    }
    else if(PLAYER_ACTION(pnum, A_SPEEDBOOTS) & !cmd->arti)
    {
        PLAYER_ACTION(pnum, A_SPEEDBOOTS) = false;
        cmd->arti = arti_speed;
    }
    else if(PLAYER_ACTION(pnum, A_DARKSERVANT) && !cmd->arti)
    {
        PLAYER_ACTION(pnum, A_DARKSERVANT) = false;
        cmd->arti = arti_summon;
    }
#endif

    // Buttons

    if(PLAYER_ACTION(pnum, A_FIRE))
        cmd->attack = true;

    if(PLAYER_ACTION(pnum, A_USE))
    {
        cmd->use = true;
        // clear double clicks if hit use button
        cstate->dclicks = 0;
    }

    if(PLAYER_ACTION(pnum, A_JUMP))
        cmd->jump = true;

#if __JDOOM__
    // Determine whether a weapon change should be done.
    if(PLAYER_ACTION(pnum, A_WEAPONCYCLE1))  // Fist/chainsaw.
    {
        if(ISWPN(WT_FIRST) && GOTWPN(WT_EIGHTH))
            i = WT_EIGHTH;
        else if(ISWPN(WT_EIGHTH))
            i = WT_FIRST;
        else if(GOTWPN(WT_EIGHTH))
            i = WT_EIGHTH;
        else
            i = WT_FIRST;

        cmd->changeWeapon = i + 1;
    }
    else if(PLAYER_ACTION(pnum, A_WEAPONCYCLE2)) // Shotgun/super sg.
    {
        if(ISWPN(WT_THIRD) && GOTWPN(WT_NINETH) &&
           gamemode == commercial)
            i = WT_NINETH;
        else if(ISWPN(WT_NINETH))
            i = WT_THIRD;
        else if(GOTWPN(WT_NINETH) && gamemode == commercial)
            i = WT_NINETH;
        else
            i = WT_THIRD;

        cmd->changeWeapon = i + 1;
    }
    else
#elif __JHERETIC__
    // Determine whether a weapon change should be done.
    if(PLAYER_ACTION(pnum, A_WEAPONCYCLE1))  // Staff/Gauntlets.
    {
        if(ISWPN(WT_FIRST) && GOTWPN(WT_EIGHTH))
            i = WT_EIGHTH;
        else if(ISWPN(WT_EIGHTH))
            i = WT_FIRST;
        else if(GOTWPN(WT_EIGHTH))
            i = WT_EIGHTH;
        else
            i = WT_FIRST;

        cmd->changeWeapon = i + 1;
    }
    else
#endif
    {
        // Take the first weapon action.
        for(i = 0; i < NUM_WEAPON_TYPES; i++)
            if(PLAYER_ACTION(pnum, A_WEAPON1 + i))
            {
                cmd->changeWeapon = i + 1;
                break;
            }
    }

    if(PLAYER_ACTION(pnum, A_NEXTWEAPON) ||
       PLAYER_ACTION(pnum, A_PREVIOUSWEAPON))
    {
        cmd->changeWeapon =
            (PLAYER_ACTION(pnum, A_NEXTWEAPON) ? TICCMD_NEXT_WEAPON :
             TICCMD_PREV_WEAPON);
    }

    // forward double click
    if(PLAYER_ACTION(pnum, A_FORWARD) != cstate->dclickstate &&
        cstate->dclicktime > 1 && cfg.dclickuse)
    {
        cstate->dclickstate = PLAYER_ACTION(pnum, A_FORWARD);

        if(cstate->dclickstate)
            cstate->dclicks++;
        if(cstate->dclicks == 2)
        {
            cmd->use = true;
            cstate->dclicks = 0;
        }
        else
            cstate->dclicktime = 0;
    }
    else
    {
        cstate->dclicktime++;
        if(cstate->dclicktime > 20)
        {
            cstate->dclicks = 0;
            cstate->dclickstate = 0;
        }
    }

    // strafe double click
    bstrafe = strafe;
    if(bstrafe != cstate->dclickstate2 && 
       cstate->dclicktime2 > 1 && cfg.dclickuse)
    {
        cstate->dclickstate2 = bstrafe;
        if(cstate->dclickstate2)
            cstate->dclicks2++;
        if(cstate->dclicks2 == 2)
        {
            cmd->use = true;
            cstate->dclicks2 = 0;
        }
        else
            cstate->dclicktime2 = 0;
    }
    else
    {
        cstate->dclicktime2++;
        if(cstate->dclicktime2 > 20)
        {
            cstate->dclicks2 = 0;
            cstate->dclickstate2 = 0;
        }
    }

    // Mouse strafe and turn (X axis).
    if(strafe)
        side += mousex * 2;
    else if(mousex)
    {
        // Mouse angle changes are immediate.
        if(!pausestate && plr->plr->mo &&
           plr->playerstate != PST_DEAD)
        {
            plr->plr->mo->angle += FLT2FIX(mousex * -8); //G_AdjustAngle(plr, mousex * -8, 1);
        }
    }

    if(!pausestate)
    {
        // Speed based turning.
        G_AdjustAngle(plr, turn, elapsedTime);

        if(strafe || (!cfg.usemlook && !PLAYER_ACTION(pnum, A_MLOOK)) ||
           plr->playerstate == PST_DEAD)
        {
            forward += 8 * mousey * elapsedTics;
        }
        else
        {
            float adj =
                (FLT2FIX(mousey * 8) / (float) ANGLE_180) * 180 *
                110.0 / 85.0;

            if(cfg.mlookInverseY)
                adj = -adj;
            plr->plr->lookdir += adj; /* $unifiedangles */
        }
        if(cfg.usejlook)
        {
            if(cfg.jlookDeltaMode) /* $unifiedangles */
                plr->plr->lookdir +=
                    joylook / 20.0f * cfg.lookSpeed *
                    (cfg.jlookInverseY ? -1 : 1) * elapsedTics;
            else
                plr->plr->lookdir =
                    joylook * 1.1f * (cfg.jlookInverseY ? -1 : 1);
        }
    }

    G_ResetMousePos();

#define MAXPLMOVE pClassInfo->maxmove

    if(forward > MAXPLMOVE)
        forward = MAXPLMOVE;
    else if(forward < -MAXPLMOVE)
        forward = -MAXPLMOVE;
    if(side > MAXPLMOVE)
        side = MAXPLMOVE;
    else if(side < -MAXPLMOVE)
        side = -MAXPLMOVE;

#if __JHEXEN__
    if(plr->powers[PT_SPEED] && !plr->morphTics)
    {
        // Adjust for a player with a speed artifact
        forward = (3 * forward) >> 1;
        side = (3 * side) >> 1;
    }
#endif

    if(cfg.playerMoveSpeed > 1)
        cfg.playerMoveSpeed = 1;

    cmd->forwardMove += forward * cfg.playerMoveSpeed;
    cmd->sideMove += side * cfg.playerMoveSpeed;;

    if(cfg.lookSpring && !PLAYER_ACTION(pnum, A_MLOOK) &&
       (cmd->forwardMove > MAXPLMOVE / 3 || cmd->forwardMove < -MAXPLMOVE / 3 ||
           cmd->sideMove > MAXPLMOVE / 3 || cmd->sideMove < -MAXPLMOVE / 3 ||
           cstate->mlookPressed))
    {
        // Center view when mlook released w/lookspring, or when moving.
        look = TOCENTER;
    }

    if(plr->playerstate == PST_LIVE && !pausestate)
        G_AdjustLookDir(plr, look, elapsedTime);

    cmd->fly = flyheight;

    // Store the current mlook key state.
    cstate->mlookPressed = PLAYER_ACTION(pnum, A_MLOOK);
}
#endif

/**
 * Clear all controls for the given player.
 *
 * @param player        Player number of whose controls to reset. If
 *                      negative; clear ALL player's controls.
 */
void G_ControlReset(int player)
{
    P_ControlReset(player);
}

/**
 * Handles special controls, such as pause.
 */
void G_SpecialButton(int pnum)
{
    player_t *pl = &players[pnum];
    if(pl->plr->ingame)
    {
        if(pl->plr->cmd.actions & BT_SPECIAL)
        {
			switch(pl->plr->cmd.actions & BT_SPECIALMASK) 
			{ 
			case BTS_PAUSE:
                paused ^= 1;
                if(paused)
                {
                    // This will stop all sounds from all origins.
                    S_StopSound(0, 0);
                }

                // Servers are responsible for informing clients about
                // pauses in the game.
                NetSv_Paused(paused);

                pl->plr->cmd.actions = 0;
                break;

            default:
                break;
            }
        }
    }
}

/**
 * Called by G_Responder.
 * Depending on the type of the event we may wish to eat it before
 * it is sent to the engine to check for bindings.
 *
 * \todo all controls should be handled by the engine.
 * Merge in engine-side axis controls from 1.8 alpha.
 *
 * @return boolean  (True) If the event should be checked for bindings
 */
boolean G_AdjustControlState(event_t* ev)
{
/*
    switch (ev->type)
    {
    case EV_KEY:
        return false;           // always let key events filter down

    case EV_MOUSE_AXIS:
        mousex += (float)(ev->data1 * (1 + cfg.mouseSensiX/5.0f)) / DD_MICKEY_ACCURACY;
        mousey += (float)(ev->data2 * (1 + cfg.mouseSensiY/5.0f)) / DD_MICKEY_ACCURACY;
        return true;            // eat events

    case EV_MOUSE_BUTTON:
        return false;           // always let mouse button events filter down

    case EV_JOY_AXIS:           // Joystick movement
        joymove[JA_X] = ev->data1;
        joymove[JA_Y] = ev->data2;
        joymove[JA_Z] = ev->data3;
        joymove[JA_RX] = ev->data4;
        joymove[JA_RY] = ev->data5;
        joymove[JA_RZ] = ev->data6;
        return true;            // eat events

    case EV_JOY_SLIDER:         // Joystick slider movement
        joymove[JA_SLIDER0] = ev->data1;
        joymove[JA_SLIDER1] = ev->data2;
        return true;

    case EV_JOY_BUTTON:
        return false;           // always let joy button events filter down

    case EV_POV:
        if(!automapactive && !menuactive)
        {
            if(ev->state == EVS_UP)
                povangle = -1;
            else
                povangle = ev->data1;

            // If looking around with PoV, don't allow bindings.
            if(cfg.povLookAround)
                return true;
        }
        break;

    default:
        break;
    }
*/
    return false;
}

/**
 * Resets the mouse position to 0,0
 * Called e.g. when starting a new level.
 */
void G_ResetMousePos(void)
{
    mousex = mousey = 0.f;
}

/**
 * Resets the look offsets.
 * Called e.g. when starting a new level.
 */
void G_ResetLookOffset(int pnum)
{
    pcontrolstate_t *cstate = &controlStates[pnum];

    cstate->lookOffset = 0;
    cstate->targetLookOffset = 0;
    cstate->lookheld = 0;
}

int G_PrivilegedResponder(event_t *event)
{
    // Process the screen shot key right away.
    if(devparm && event->type == EV_KEY && event->data1 == DDKEY_F1)
    {
        if(event->state == EVS_DOWN)
            G_ScreenShot();
        // All F1 events are eaten.
        return true;
    }
    return false;
}
