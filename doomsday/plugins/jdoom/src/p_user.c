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
 * Player related stuff.
 * Bobbing POV/weapon, movement.
 * Pending weapon.
 */

// HEADER FILES ------------------------------------------------------------

#include "doomdef.h"
#include "d_config.h"
#include "d_event.h"
#include "p_local.h"
#include "p_inter.h"
#include "p_view.h"
#include "doomstat.h"
#include "g_common.h"

#include "Common/d_net.h"
#include "Common/p_player.h"

// MACROS ------------------------------------------------------------------

// Index of the special effects (INVUL inverse) map.
#define INVERSECOLORMAP     32

// 16 pixels of bob
#define MAXBOB  0x100000

#define ANG5    (ANG90/18)

#define LIST_BEGIN      -1
#define LIST_END        -2

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     maxhealth;              // 100
int     healthlimit;            // 200
int     godmodehealth;          // 100
int     soulspherelimit;        // 200
int     megaspherehealth;       // 200
int     soulspherehealth;       // 100
int     armorpoints[4];         // Green, blue, IDFA and IDKFA points.
int     armorclass[4];          // Green, blue, IDFA and IDKFA armor classes.

boolean onground;

classinfo_t classInfo[NUMCLASSES] = {
    {   // Player
        S_PLAY,
        S_PLAY_RUN1,
        S_PLAY_ATK1,
        S_PLAY_ATK2,
        20,
        0x3C,
        {0x19, 0x32},
        {0x18, 0x28},
    }
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Moves the given origin along a given angle.
 */
void P_Thrust(player_t *player, angle_t angle, fixed_t move)
{
    mobj_t *plrmo = player->plr->mo;
    sector_t* sector = P_GetPtrp(plrmo->subsector, DMU_SECTOR);

    angle >>= ANGLETOFINESHIFT;
    if(player->powers[pw_flight] && !(plrmo->pos[VZ] <= plrmo->floorz))
    {
        plrmo->momx += FixedMul(move, finecosine[angle]);
        plrmo->momy += FixedMul(move, finesine[angle]);
    }
#if __JHERETIC__
    else if(P_XSector(sector)->special == 15)    // Friction_Low
    {
        plrmo->momx += FixedMul(move >> 2, finecosine[angle]);
        plrmo->momy += FixedMul(move >> 2, finesine[angle]);
    }
#endif
    else
    {
        int     mul = XS_ThrustMul(sector);

        if(mul != FRACUNIT)
            move = FixedMul(move, mul);
        plrmo->momx += FixedMul(move, finecosine[angle]);
        plrmo->momy += FixedMul(move, finesine[angle]);
    }
}

/*
 * Returns true if the player is currently standing on ground
 * or on top of another mobj.
 */
boolean P_IsPlayerOnGround(player_t *player)
{
    boolean onground = (player->plr->mo->pos[VZ] <= player->plr->mo->floorz);

    if(player->plr->mo->onmobj && !onground && !(player->plr->mo->flags2 & MF2_FLY))
    {
        mobj_t *on = player->plr->mo->onmobj;

        onground = (player->plr->mo->pos[VZ] <= on->pos[VZ] + on->height);
    }
    return onground;
}

/*
 * Will make the player jump if the latest command so instructs,
 * providing that jumping is possible.
 */
void P_CheckPlayerJump(player_t *player)
{
    ticcmd_t *cmd = &player->cmd;

    if(cfg.jumpEnabled && (!IS_CLIENT || netJumpPower > 0) &&
       P_IsPlayerOnGround(player) && cmd->jump &&
       player->jumptics <= 0)
    {
        // Jump, then!
        player->plr->mo->momz =
            FRACUNIT * (IS_CLIENT ? netJumpPower : cfg.jumpPower);
        player->jumptics = 24;
    }
}

void P_MovePlayer(player_t *player)
{
    int     fly;
    mobj_t *plrmo = player->plr->mo;
    ticcmd_t *cmd;

    cmd = &player->cmd;

    //  player->plr->mo->angle += (cmd->angleturn<<16);

    // Change the angle if possible.
    if(!(player->plr->flags & DDPF_FIXANGLES))
    {
        plrmo->angle = cmd->angle << 16;
        player->plr->lookdir = cmd->pitch / (float) DDMAXSHORT *110;
    }

    // Do not let the player control movement if not onground.
    onground =  P_IsPlayerOnGround(player);
    if(player->plr->flags & DDPF_CAMERA)    // $democam
    {
        // Cameramen have a 3D thrusters!
        P_Thrust3D(player, player->plr->mo->angle, player->plr->lookdir,
                   cmd->forwardMove * 2048, cmd->sideMove * 2048);
    }
    else
    {
        // 'Move while in air' hack (server doesn't know about this!!).
        // Movement while in air traditionally disabled.
        int     movemul = (onground || plrmo->flags2 & MF2_FLY) ? 2048 :
                    cfg.airborneMovement ? cfg.airborneMovement * 64 : 0;

        if(cmd->forwardMove && movemul)
        {
            P_Thrust(player, player->plr->mo->angle,
                     cmd->forwardMove * movemul);
        }

        if(cmd->sideMove && movemul)
        {
            P_Thrust(player, player->plr->mo->angle - ANG90,
                     cmd->sideMove * movemul);
        }

        if((cmd->forwardMove || cmd->sideMove) &&
           player->plr->mo->state == &states[PCLASS_INFO(player->class)->normalstate])
        {
            P_SetMobjState(player->plr->mo, PCLASS_INFO(player->class)->runstate);
        }

        fly = cmd->fly; //lookfly >> 4;
    /*  if(fly > 7)
            fly -= 16;
    */
        if(fly && player->powers[pw_flight])
        {
            if(fly != TOCENTER)
            {
                player->flyheight = fly * 2;
                if(!(plrmo->flags2 & MF2_FLY))
                {
                    plrmo->flags2 |= MF2_FLY;
                    plrmo->flags |= MF_NOGRAVITY;
                }
            }
            else
            {
                plrmo->flags2 &= ~MF2_FLY;
                plrmo->flags &= ~MF_NOGRAVITY;
            }
        }
#if __JHERETIC__
        else if(fly > 0)
        {
            P_PlayerUseArtifact(player, arti_fly);
        }
#endif
        if(plrmo->flags2 & MF2_FLY)
        {
            plrmo->momz = player->flyheight * FRACUNIT;
            if(player->flyheight)
            {
                player->flyheight /= 2;
            }
        }

        P_CheckPlayerJump(player);
    }
}

/*
 * Fall on your ass when dying.
 * Decrease POV height to floor height.
 */
void P_DeathThink(player_t *player)
{
    angle_t angle;
    angle_t delta;
    int     lookDelta;

    P_MovePsprites(player);

    onground = (player->plr->mo->pos[VZ] <= player->plr->mo->floorz);
    if(cfg.deathLookUp) // Flying bloody skull
    {
        player->plr->viewheight = 6 * FRACUNIT;
        player->plr->deltaviewheight = 0;

        if(onground)
        {
            if(player->plr->lookdir < 60)
            {
                lookDelta = (60 - player->plr->lookdir) / 8;
                if(lookDelta < 1 && (leveltime & 1))
                {
                    lookDelta = 1;
                }
                else if(lookDelta > 6)
                {
                    lookDelta = 6;
                }
                player->plr->lookdir += lookDelta;
            }
        }
    }
    else // fall to the ground
    {
        if(player->plr->viewheight > 6 * FRACUNIT)
            player->plr->viewheight -= FRACUNIT;

        if(player->plr->viewheight < 6 * FRACUNIT)
            player->plr->viewheight = 6 * FRACUNIT;

        player->plr->deltaviewheight = 0;
    }

    player->plr->flags |= DDPF_FIXANGLES | DDPF_FIXPOS | DDPF_FIXMOM;
    P_CalcHeight(player);

    // In netgames we won't keep tracking the killer.
    if(!IS_NETGAME && player->attacker && player->attacker != player->plr->mo)
    {
        angle =
            R_PointToAngle2(player->plr->mo->pos[VX], player->plr->mo->pos[VY],
                            player->attacker->pos[VX], player->attacker->pos[VY]);

        delta = angle - player->plr->mo->angle;

        if(delta < ANG5 || delta > (unsigned) -ANG5)
        {
            // Looking at killer,
            //  so fade damage flash down.
            player->plr->mo->angle = angle;
            if(player->damagecount)
                player->damagecount--;
        }
        else if(delta < ANG180)
            player->plr->mo->angle += ANG5;
        else
            player->plr->mo->angle -= ANG5;
    }
    else if(player->damagecount)
        player->damagecount--;

    if(player->cmd.use)
        player->playerstate = PST_REBORN;
}

/*
 * Called once per tick by P_Ticker.
 * This routine does all the thinking for the console player during
 * netgames.
 */
void P_ClientSideThink()
{
    int     i;
    player_t *pl;
    ddplayer_t *dpl;
    mobj_t *mo;
    ticcmd_t *cmd;
    int     fly;

    if(!IS_CLIENT || !Get(DD_GAME_READY))
        return;

    pl = &players[consoleplayer];
    dpl = pl->plr;
    mo = dpl->mo;
    cmd = &pl->cmd; // The latest local command.
    P_CalcHeight(pl);

    // Message timer.
    pl->messageTics--;          // Can go negative
    if(!pl->messageTics)
    {                           // Refresh the screen when a message goes away
        //BorderTopRefresh = true;
        GL_Update(DDUF_TOP);
    }

    // Powers tic away.
    for(i = 0; i < NUMPOWERS; i++)
    {
        switch (i)
        {
        case pw_invulnerability:
        case pw_invisibility:
        case pw_ironfeet:
        case pw_infrared:
            if(pl->powers[i] > 0)
                pl->powers[i]--;
            else
                pl->powers[i] = 0;
            break;
        }
    }

    // Are we dead?
    if(pl->playerstate == PST_DEAD)
    {
        if(dpl->viewheight > 6 * FRACUNIT)
            dpl->viewheight -= FRACUNIT;
        if(dpl->viewheight < 6 * FRACUNIT)
            dpl->viewheight = 6 * FRACUNIT;
    }

    // Jumping.
    if(pl->jumptics)
        pl->jumptics--;
    P_CheckPlayerJump(pl);

    // Flying.
    fly = cmd->fly; //lookfly >> 4;
/*  if(fly > 7)
    {
        fly -= 16;
    }
*/
    if(fly && pl->powers[pw_flight])
    {
        if(fly != TOCENTER)
        {
            pl->flyheight = fly * 2;
        }
    }
    // We are flying when the Fly flag is set.
    if(mo->ddflags & DDMF_FLY)
    {
        // If we were on a mobj, we are NOT now.
        if(mo->onmobj)
            mo->onmobj = NULL;

        // Keep the fly flag in sync.
        mo->flags2 |= MF2_FLY;

        mo->momz = pl->flyheight * FRACUNIT;
        if(pl->flyheight)
            pl->flyheight /= 2;
        // Do some fly-bobbing.
        if(mo->pos[VZ] > mo->floorz && (mo->flags2 & MF2_FLY) &&
           !mo->onmobj && (leveltime & 2))
            mo->pos[VZ] += finesine[(FINEANGLES / 20 * leveltime >> 2) & FINEMASK];
    }

    // Set the proper thrust multiplier. XG gives this quite easily.
    // (The thrust multiplier is used by Cl_MovePlayer, the movement
    // "predictor"; almost all clientside movement is handled by that
    // routine, though.)
    Set(DD_CPLAYER_THRUST_MUL,
        XS_ThrustMul(P_GetPtrp(mo->subsector, DMU_SECTOR)));

    // Update view angles. The server fixes them if necessary.
    mo->angle = dpl->clAngle;
    dpl->lookdir = dpl->clLookDir;
}

void P_PlayerThink(player_t *player)
{
    mobj_t *plrmo = player->plr->mo;
    ticcmd_t *cmd;
    weapontype_t newweapon, oldweapon;

    // Selector 0 = Generic (used by default)
    // Selector 1 = Fist
    // Selector 2 = Pistol
    // Selector 3 = Shotgun
    // Selector 4 = Fist
    // Selector 5 = Chaingun
    // Selector 6 = Missile
    // Selector 7 = Plasma
    // Selector 8 = BFG
    // Selector 9 = Chainsaw
    // Selector 10 = Super shotgun
    plrmo->selector =
        (plrmo->selector & ~DDMOBJ_SELECTOR_MASK) | (player->readyweapon + 1);

    P_CameraThink(player);      // $democam

    // fixme: do this in the cheat code
    if(player->cheats & CF_NOCLIP)
        player->plr->mo->flags |= MF_NOCLIP;
    else
        player->plr->mo->flags &= ~MF_NOCLIP;

    // chain saw run forward
    cmd = &player->cmd;
    if(player->plr->mo->flags & MF_JUSTATTACKED)
    {
        cmd->angle = plrmo->angle >> 16;    // Don't turn.
        // The client must know of this.
        player->plr->flags |= DDPF_FIXANGLES;
        cmd->forwardMove = 0xc800 / 512;
        cmd->sideMove = 0;
        player->plr->mo->flags &= ~MF_JUSTATTACKED;
    }

    // messageTics is above the rest of the counters so that messages will
    //              go away, even in death.
    player->messageTics--;      // Can go negative
    if(!player->messageTics)
    {                           // Refresh the screen when a message goes away
        //BorderTopRefresh = true;
        GL_Update(DDUF_TOP);
    }
    if(player->playerstate == PST_DEAD)
    {
        P_DeathThink(player);
        return;
    }

    // Move around.
    // Reactiontime is used to prevent movement
    //  for a bit after a teleport.
    if(player->plr->mo->reactiontime)
        player->plr->mo->reactiontime--;
    else
        P_MovePlayer(player);

    P_CalcHeight(player);

    if(P_XSector(P_GetPtrp(player->plr->mo->subsector, DMU_SECTOR))->special)
        P_PlayerInSpecialSector(player);

    if(player->jumptics)
        player->jumptics--;

    oldweapon = player->pendingweapon;

    // There might be a special weapon change.
    if(cmd->changeWeapon == TICCMD_NEXT_WEAPON ||
       cmd->changeWeapon == TICCMD_PREV_WEAPON)
    {
        player->pendingweapon =
            P_PlayerFindWeapon(player,
                               cmd->changeWeapon == TICCMD_NEXT_WEAPON);
        cmd->changeWeapon = 0;
    }

    // Check for weapon change.
    if(cmd->changeWeapon)
    {
        // The actual changing of the weapon is done
        //  when the weapon psprite can do it
        //  (read: not in the middle of an attack).
        newweapon = cmd->changeWeapon - 1;

        if(gamemode != commercial && newweapon == wp_supershotgun)
        {
            // In non-Doom II, supershotgun is the same as normal shotgun.
            newweapon = wp_shotgun;
        }

        if(player->weaponowned[newweapon] && newweapon != player->readyweapon)
        {
            if(weaponinfo[newweapon][player->class].mode[0].gamemodebits
                & gamemodebits)
            {
                player->pendingweapon = newweapon;
            }
        }
    }

    if(player->pendingweapon != oldweapon)
        player->update |= PSF_PENDING_WEAPON | PSF_READY_WEAPON;

    // check for use
    if(cmd->use)
    {
        if(!player->usedown)
        {
            P_UseLines(player);
            player->usedown = true;
        }
    }
    else
        player->usedown = false;

    // cycle psprites
    P_MovePsprites(player);

    // Counters, time dependend power ups.

    // Strength counts up to diminish fade.
    if(player->powers[pw_strength])
        player->powers[pw_strength]++;

    if(player->powers[pw_invulnerability])
        player->powers[pw_invulnerability]--;

    if(player->powers[pw_invisibility])
    {
        if(!--player->powers[pw_invisibility])
            player->plr->mo->flags &= ~MF_SHADOW;
    }

    if(player->powers[pw_infrared])
        player->powers[pw_infrared]--;

    if(player->powers[pw_ironfeet])
        player->powers[pw_ironfeet]--;

    if(player->damagecount)
        player->damagecount--;

    if(player->bonuscount)
        player->bonuscount--;
}
