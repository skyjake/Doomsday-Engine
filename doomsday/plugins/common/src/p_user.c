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
 * p_user.c : Player related stuff.
 *
 * Bobbing POV/weapon, movement, pending weapon, artifact usage...
 * Compiles for jDoom, jHeretic and jHexen.
 */

/*
 * TODO: DJS - This file contains a fair amount of Raven licensed code
 *       however none is included when the build target is jDoom.
 *
 *       In the process of cleaning this up I will attempt to segregate
 *       offending code ready for rewrite or split.
 */

// HEADER FILES ------------------------------------------------------------

#if __JHEXEN__
#  include <math.h>
#endif

#if   __WOLFTC__
#  include "wolftc.h"
#  include "g_common.h"
#elif __JDOOM__
#  include "jdoom.h"
#  include "g_common.h"
#elif __JHERETIC__
#  include "jheretic.h"
#  include "g_common.h"
#  include "r_common.h"
#  include "p_inventory.h"
#elif __JHEXEN__
#  include <math.h>
#  include "jhexen.h"
#  include "p_inventory.h"
#endif

#include "p_player.h"
#include "p_view.h"
#include "d_net.h"
#include "p_player.h"

// MACROS ------------------------------------------------------------------

#define ANG5    (ANG90/18)

// 16 pixels of bob
#define MAXBOB  0x100000

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------
boolean onground;

#if __JDOOM__
int     maxhealth;              // 100
int     healthlimit;            // 200
int     godmodehealth;          // 100
int     soulspherelimit;        // 200
int     megaspherehealth;       // 200
int     soulspherehealth;       // 100
int     armorpoints[4];         // Green, blue, IDFA and IDKFA points.
int     armorclass[4];          // Green, blue, IDFA and IDKFA armor classes.

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
        2048,
        24
    }
};
#elif __JHERETIC__
int     newtorch[MAXPLAYERS];   // used in the torch flicker effect.
int     newtorchdelta[MAXPLAYERS];

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
        2048,
        24
    },
    {   // Chicken
        S_CHICPLAY,
        S_CHICPLAY_RUN1,
        S_CHICPLAY_ATK1,
        S_CHICPLAY_ATK1,
        20,
        0x3C,
        {0x19, 0x32},
        {0x18, 0x28},
        2500,
        24
    },
};
#elif __JHEXEN__
int     newtorch[MAXPLAYERS];   // used in the torch flicker effect.
int     newtorchdelta[MAXPLAYERS];

classinfo_t classInfo[NUMCLASSES] = {
    {   // Fighter
        MT_PLAYER_FIGHTER,
        S_FPLAY,
        S_FPLAY_RUN1,
        S_FPLAY_ATK1,
        S_FPLAY_ATK2,
        20,
        15 * FRACUNIT,
        0x3C,
        {0x1D, 0x3C},
        {0x1B, 0x3B},
        2048,
        18,
        {25 * FRACUNIT, 20 * FRACUNIT, 15 * FRACUNIT, 5 * FRACUNIT},
        {190, 225, 234}
    },
    {   // Cleric
        MT_PLAYER_CLERIC,
        S_CPLAY,
        S_CPLAY_RUN1,
        S_CPLAY_ATK1,
        S_CPLAY_ATK3,
        18,
        10 * FRACUNIT,
        0x32,
        {0x19, 0x32},
        {0x18, 0x28},
        2048,
        18,
        {10 * FRACUNIT, 25 * FRACUNIT, 5 * FRACUNIT, 20 * FRACUNIT},
        {190, 212, 225}
    },
    {   // Mage
        MT_PLAYER_MAGE,
        S_MPLAY,
        S_MPLAY_RUN1,
        S_MPLAY_ATK1,
        S_MPLAY_ATK2,
        16,
        5 * FRACUNIT,
        0x2D,
        {0x16, 0x2E},
        {0x15, 0x25},
        2048,
        18,
        {5 * FRACUNIT, 15 * FRACUNIT, 10 * FRACUNIT, 25 * FRACUNIT},
        {190, 205, 224}
    },
    {   // Pig
        MT_PIGPLAYER,
        S_PIGPLAY,
        S_PIGPLAY_RUN1,
        S_PIGPLAY_ATK1,
        S_PIGPLAY_ATK1,
        1,
        0,
        0x31,
        {0x18, 0x31},
        {0x17, 0x27},
        2048,
        18,
        {0, 0, 0, 0},
        {0, 0, 0}
    },
};
#endif

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Moves the given origin along a given angle.
 */
void P_Thrust(player_t *player, angle_t angle, fixed_t move)
{
    mobj_t *plrmo = player->plr->mo;
#if __JDOOM__ || __JHERETIC__
    sector_t* sector = P_GetPtrp(plrmo->subsector, DMU_SECTOR);
#endif

    angle >>= ANGLETOFINESHIFT;
    if(player->powers[pw_flight] && !(plrmo->pos[VZ] <= plrmo->floorz))
    {
        /*float xmul=1, ymul=1;

           // How about Quake-flying? -- jk
           if(quakeFly)
           {
           float ang = LOOKDIR2RAD(player->plr->lookdir);
           xmul = ymul = cos(ang);
           mo->momz += sin(ang) * move;
           } */

        plrmo->momx += FixedMul(move, finecosine[angle]);
        plrmo->momy += FixedMul(move, finesine[angle]);
    }
#if __JHERETIC__
    else if(P_XSector(sector)->special == 15)    // Friction_Low
    {
        plrmo->momx += FixedMul(move >> 2, finecosine[angle]);
        plrmo->momy += FixedMul(move >> 2, finesine[angle]);
    }
#elif __JHEXEN__
    else if(P_GetThingFloorType(plrmo) == FLOOR_ICE)   // Friction_Low
    {
        plrmo->momx += FixedMul(move >> 1, finecosine[angle]);
        plrmo->momy += FixedMul(move >> 1, finesine[angle]);
    }
#endif
    else
    {
#if __JDOOM__ || __JHERETIC__
        int     mul = XS_ThrustMul(sector);

        if(mul != FRACUNIT)
            move = FixedMul(move, mul);
#endif
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

#if __JHEXEN__
    if((player->plr->mo->flags2 & MF2_ONMOBJ) && !onground)
    {
        //mobj_t *on = onmobj;

        onground = true; //(player->plr->mo->pos[VZ] <= on->pos[VZ] + on->height);
    }
#else
    if(player->plr->mo->onmobj && !onground && !(player->plr->mo->flags2 & MF2_FLY))
    {
        mobj_t *on = player->plr->mo->onmobj;

        onground = (player->plr->mo->pos[VZ] <= on->pos[VZ] + on->height);
    }
#endif
    return onground;
}

/*
 * Will make the player jump if the latest command so instructs,
 * providing that jumping is possible.
 */
void P_CheckPlayerJump(player_t *player)
{
    float   power;
    ticcmd_t *cmd = &player->cmd;

    // Check if we are allowed to jump.
    if(cfg.jumpEnabled && (!IS_CLIENT || netJumpPower > 0) &&
       P_IsPlayerOnGround(player) && cmd->jump && player->jumptics <= 0)
    {
        // Jump, then!
        power = (IS_CLIENT ? netJumpPower : cfg.jumpPower);

#if __JHEXEN__
        if(player->morphTics) // Pigs don't jump that high.
            player->plr->mo->momz = FRACUNIT * (2 * power / 3);
        else
#endif
            player->plr->mo->momz = FRACUNIT * power;

        player->jumptics = PCLASS_INFO(player->class)->jumptics;

#if __JHEXEN__
        player->plr->mo->flags2 &= ~MF2_ONMOBJ;
#endif
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
        int movemul = (onground || plrmo->flags2 & MF2_FLY) ? PCLASS_INFO(player->class)->movemul :
                       (cfg.airborneMovement) ? cfg.airborneMovement * 64 : 0;

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
#if __JHEXEN__
                    if(plrmo->momz <= -39 * FRACUNIT)
                    {           // stop falling scream
                        S_StopSound(0, plrmo);
                    }
#endif
                }
            }
            else
            {
                plrmo->flags2 &= ~MF2_FLY;
                plrmo->flags &= ~MF_NOGRAVITY;
            }
        }
#if __JHERETIC__ || __JHEXEN__
        else if(fly > 0)
        {
            P_InventoryUseArtifact(player, arti_fly);
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
#if __JHEXEN__
    // Look up/down using the delta.
    /*  if(cmd->lookdirdelta)
       {
       float fd = cmd->lookdirdelta / DELTAMUL;
       float delta = fd * fd;
       if(cmd->lookdirdelta < 0) delta = -delta;
       player->plr->lookdir += delta;
       } */

    // 110 corresponds 85 degrees.
    if(player->plr->lookdir > 110)
        player->plr->lookdir = 110;
    if(player->plr->lookdir < -110)
        player->plr->lookdir = -110;
#endif
}

/*
 * Fall on your ass when dying.
 * Decrease POV height to floor height.
 */
void P_DeathThink(player_t *player)
{
#if __JDOOM__ || __JHERETIC__
    angle_t angle;
#endif
    angle_t delta;
    int     lookDelta;

    P_MovePsprites(player);

    onground = (player->plr->mo->pos[VZ] <= player->plr->mo->floorz);
#if __JDOOM__
    if(cfg.deathLookUp)
#elif __JHERETIC__
    if(player->plr->mo->type == MT_BLOODYSKULL)
#elif __JHEXEN__
    if(player->plr->mo->type == MT_BLOODYSKULL ||
       player->plr->mo->type == MT_ICECHUNK)
#endif
    {   // Flying bloody skull
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
#if __JHEXEN__
    else if(!(player->plr->mo->flags2 & MF2_ICEDAMAGE)) // (if not frozen)
#else
    else // fall to the ground
#endif
    {
        if(player->plr->viewheight > 6 * FRACUNIT)
            player->plr->viewheight -= FRACUNIT;

        if(player->plr->viewheight < 6 * FRACUNIT)
            player->plr->viewheight = 6 * FRACUNIT;

        player->plr->deltaviewheight = 0;

#if  __JHERETIC__ || __JHEXEN__
        if(player->plr->lookdir > 0)
            player->plr->lookdir -= 6;
        else if(player->plr->lookdir < 0)
            player->plr->lookdir += 6;

        if(abs((int) player->plr->lookdir) < 6)
            player->plr->lookdir = 0;
#endif
    }

#if __JHEXEN__
    player->update |= PSF_VIEW_HEIGHT;
#endif
    player->plr->flags |= DDPF_FIXANGLES | DDPF_FIXPOS | DDPF_FIXMOM;
    P_CalcHeight(player);

    // In netgames we won't keep tracking the killer.
    if(
#if !__JHEXEN__
        !IS_NETGAME &&
#endif
        player->attacker && player->attacker != player->plr->mo)
    {
#if __JHEXEN__
        int dir = P_FaceMobj(player->plr->mo, player->attacker, &delta);
        if(delta < ANGLE_1 * 10)
        {                       // Looking at killer, so fade damage and poison counters
            if(player->damagecount)
            {
                player->damagecount--;
            }
            if(player->poisoncount)
            {
                player->poisoncount--;
            }
        }
        delta = delta / 8;
        if(delta > ANGLE_1 * 5)
        {
            delta = ANGLE_1 * 5;
        }
        if(dir)
            player->plr->mo->angle += delta; // Turn clockwise
        else
            player->plr->mo->angle -= delta; // Turn counter clockwise
#else
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
            player->plr->mo->angle += ANG5; // Turn clockwise
        else
            player->plr->mo->angle -= ANG5; // Turn counter clockwise
#endif
    }
    else
    {
        if(player->damagecount)
            player->damagecount--;

#if __JHEXEN__
        if(player->poisoncount)
            player->poisoncount--;
#endif
    }

    if(player->cmd.use)
    {
        player->playerstate = PST_REBORN;
#if __JHERETIC__ || __JHEXEN__
        if(player == &players[consoleplayer])
        {
            inv_ptr = 0;
            curpos = 0;
            R_SetFilter(0);
        }

        newtorch[player - players] = 0;
        newtorchdelta[player - players] = 0;
# if __JHEXEN__
        player->plr->mo->special1 = player->class;
        if(player->plr->mo->special1 > 2)
        {
            player->plr->mo->special1 = 0;
        }
# endif
        // Let the mobj know the player has entered the reborn state.  Some
        // mobjs need to know when it's ok to remove themselves.
        player->plr->mo->special2 = 666;
#endif
    }
}

#if __JHERETIC__ || __JHEXEN__
void P_MorphPlayerThink(player_t *player)
{
    mobj_t *pmo;

#if __JHEXEN__
    if(player->morphTics & 15)
        return;

    pmo = player->plr->mo;
    if(!(pmo->momx + pmo->momy) && P_Random() < 64)
    {   // Snout sniff
        P_SetPspriteNF(player, ps_weapon, S_SNOUTATK2);
        S_StartSound(SFX_PIG_ACTIVE1, pmo); // snort
        return;
    }

    if(P_Random() < 48)
    {
        if(P_Random() < 128)
            S_StartSound(SFX_PIG_ACTIVE1, pmo);
        else
            S_StartSound(SFX_PIG_ACTIVE2, pmo);
    }
# else
    if(player->health > 0)
        P_UpdateBeak(player, &player->psprites[ps_weapon]); // Handle beak movement

    if(IS_CLIENT || player->morphTics & 15)
        return;

    pmo = player->plr->mo;
    if(!(pmo->momx + pmo->momy) && P_Random() < 160)
    {                           // Twitch view angle
        pmo->angle += (P_Random() - P_Random()) << 19;
    }
    if((pmo->pos[VZ] <= pmo->floorz) && (P_Random() < 32))
    {                           // Jump and noise
        pmo->momz += FRACUNIT;
        P_SetMobjState(pmo, S_CHICPLAY_PAIN);
        return;
    }
    if(P_Random() < 48)
    {                           // Just noise
        S_StartSound(sfx_chicact, pmo);
    }
# endif
}

boolean P_UndoPlayerMorph(player_t *player)
{
    mobj_t *fog = 0;
    mobj_t *mo = 0;
    mobj_t *pmo = 0;
    fixed_t pos[3];
    angle_t angle;
    int     playerNum;
    weapontype_t weapon;
    int     oldFlags;
    int     oldFlags2;
    int     oldBeast;

# if __JHEXEN__
    player->update |= PSF_MORPH_TIME | PSF_POWERS | PSF_HEALTH;
# endif

    pmo = player->plr->mo;
    memcpy(pos, pmo->pos, sizeof(pos));

    angle = pmo->angle;
    weapon = pmo->special1;
    oldFlags = pmo->flags;
    oldFlags2 = pmo->flags2;
# if __JHEXEN__
    oldBeast = pmo->type;
# else
    oldBeast = MT_CHICPLAYER;
# endif
    P_SetMobjState(pmo, S_FREETARGMOBJ);

    playerNum = P_GetPlayerNum(player);
# if __JHEXEN__
    mo = P_SpawnMobj(pos[VX], pos[VY], pos[VZ],
                     PCLASS_INFO(cfg.PlayerClass[playerNum])->mobjtype);
# else
    mo = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_PLAYER);
# endif

    if(P_TestMobjLocation(mo) == false)
    {   // Didn't fit
        P_RemoveMobj(mo);
        mo = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], oldBeast);

        mo->angle = angle;
        mo->health = player->health;
        mo->special1 = weapon;
        mo->player = player;
        mo->dplayer = player->plr;
        mo->flags = oldFlags;
        mo->flags2 = oldFlags2;
        player->plr->mo = mo;
        player->morphTics = 2 * 35;
        return (false);
    }

# if __JHEXEN__
    if(player->class == PCLASS_FIGHTER)
    {
        // The first type should be blue, and the third should be the
        // Fighter's original gold color
        if(playerNum == 0)
            mo->flags |= 2 << MF_TRANSSHIFT;
        else if(playerNum != 2)
            mo->flags |= playerNum << MF_TRANSSHIFT;
    }
    else
# endif
    if(playerNum != 0)
    {   // Set color translation bits for player sprites
        mo->flags |= playerNum << MF_TRANSSHIFT;
    }

    mo->angle = angle;
    mo->player = player;
    mo->dplayer = player->plr;
    mo->reactiontime = 18;

    if(oldFlags2 & MF2_FLY)
    {
        mo->flags2 |= MF2_FLY;
        mo->flags |= MF_NOGRAVITY;
    }

    player->morphTics = 0;
# if __JHERETIC__
    player->powers[pw_weaponlevel2] = 0;
# endif
    player->health = mo->health = MAXHEALTH;
    player->plr->mo = mo;
# if __JHERETIC__
    player->class = PCLASS_PLAYER;
# else
    player->class = cfg.PlayerClass[playerNum];
# endif
    angle >>= ANGLETOFINESHIFT;

    fog =
        P_SpawnMobj(pos[VX] + 20 * finecosine[angle], pos[VY] + 20 * finesine[angle],
                    pos[VZ] + TELEFOGHEIGHT, MT_TFOG);
# if __JHERETIC__
    S_StartSound(sfx_telept, fog);
# else
    S_StartSound(SFX_TELEPORT, fog);
# endif
    P_PostMorphWeapon(player, weapon);

    player->update |= PSF_MORPH_TIME | PSF_HEALTH;
    player->plr->flags |= DDPF_FIXPOS | DDPF_FIXMOM;
    return (true);
}
#endif

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

    if(!mo)
        return;

    cmd = &pl->cmd; // The latest local command.
    P_CalcHeight(pl);

    // Message timer.
    pl->messageTics--;          // Can go negative
    if(!pl->messageTics)
    {   // Refresh the screen when a message goes away
#if __JHEXEN__
        pl->ultimateMessage = false;    // clear out any chat messages.
        pl->yellowMessage = false;
#endif
        GL_Update(DDUF_TOP);
    }

#if __JHEXEN__
    if(pl->morphTics > 0)
        pl->morphTics--;
#endif

    // Powers tic away.
    for(i = 0; i < NUMPOWERS; i++)
    {
        switch (i)
        {
#if __JDOOM__
        case pw_invulnerability:
        case pw_invisibility:
        case pw_ironfeet:
        case pw_infrared:
        case pw_strength:
#elif __JHEXEN__
        case pw_invulnerability:
        case pw_infrared:
        case pw_flight:
        case pw_speed:
        case pw_minotaur:
#endif
            if(pl->powers[i] > 0)
                pl->powers[i]--;
            else
                pl->powers[i] = 0;
            break;
        }
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
            if(!(mo->ddflags & DDMF_FLY))
            {
                // Start flying.
                //      mo->ddflags |= DDMF_FLY | DDMF_NOGRAVITY;
            }
        }
        else
        {
            //  mo->ddflags &= ~(DDMF_FLY | DDMF_NOGRAVITY);
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
#if __JHEXEN__
    else
    {
        // Clear the Fly flag.
        mo->flags2 &= ~MF2_FLY;
    }
#endif

#if __JHEXEN__
    if(xsectors[P_GetIntp(mo->subsector, DMU_SECTOR)].special)
        P_PlayerInSpecialSector(pl);

    // Set consoleplayer thrust multiplier.
    if(mo->pos[VZ] > mo->floorz)      // Airborne?
    {
        Set(DD_CPLAYER_THRUST_MUL, (mo->ddflags & DDMF_FLY) ? FRACUNIT : 0);
    }
    else
    {
        Set(DD_CPLAYER_THRUST_MUL,
            (P_GetThingFloorType(mo) == FLOOR_ICE) ? FRACUNIT >> 1 : FRACUNIT);
    }
#else
    // Set the proper thrust multiplier. XG gives this quite easily.
    // (The thrust multiplier is used by Cl_MovePlayer, the movement
    // "predictor"; almost all clientside movement is handled by that
    // routine, though.)
    Set(DD_CPLAYER_THRUST_MUL,
        XS_ThrustMul(P_GetPtrp(mo->subsector, DMU_SECTOR)));
#endif

    // Update view angles. The server fixes them if necessary.
    mo->angle = dpl->clAngle;
    dpl->lookdir = dpl->clLookDir;
}

void P_PlayerThink(player_t *player)
{
    mobj_t *plrmo = player->plr->mo;
    ticcmd_t *cmd;
    weapontype_t newweapon, oldweapon;
#if __JHEXEN__
    int     floorType;
#endif

    // jDoom
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
    // jHexen
    // Selector 0 = Generic (used by default)
    // Selector 1..4 = Weapon 1..4
    plrmo->selector =
        (plrmo->selector & ~DDMOBJ_SELECTOR_MASK) | (player->readyweapon + 1);

    P_CameraThink(player);      // $democam

    // fixme: do this in the cheat code
    if(player->cheats & CF_NOCLIP)
        player->plr->mo->flags |= MF_NOCLIP;
    else
        player->plr->mo->flags &= ~MF_NOCLIP;

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
    {   // Refresh the screen when a message goes away
#if __JHEXEN__
        player->ultimateMessage = false;    // clear out any chat messages.
        player->yellowMessage = false;
#endif
        //BorderTopRefresh = true;
        GL_Update(DDUF_TOP);
    }
#if __JHEXEN__
    player->worldTimer++;
#endif
    if(player->playerstate == PST_DEAD)
    {
        P_DeathThink(player);
        return;
    }

#if __JHEXEN__
    if(player->morphTics)
    {
        P_MorphPlayerThink(player);
    }
#endif

    // Move around.
    // Reactiontime is used to prevent movement
    //  for a bit after a teleport.
    if(player->plr->mo->reactiontime)
        player->plr->mo->reactiontime--;
    else
    {
        P_MovePlayer(player);
#if __JHEXEN__
        plrmo = player->plr->mo;
        if(player->powers[pw_speed] && !(leveltime & 1) &&
           P_ApproxDistance(plrmo->momx, plrmo->momy) > 12 * FRACUNIT)
        {
            mobj_t *speedMo;
            int     playerNum;

            speedMo = P_SpawnMobj(plrmo->pos[VX], plrmo->pos[VY], plrmo->pos[VZ],
                                  MT_PLAYER_SPEED);
            if(speedMo)
            {
                speedMo->angle = plrmo->angle;
                playerNum = P_GetPlayerNum(player);
                if(player->class == PCLASS_FIGHTER)
                {
                    // The first type should be blue, and the
                    // third should be the Fighter's original gold color
                    if(playerNum == 0)
                    {
                        speedMo->flags |= 2 << MF_TRANSSHIFT;
                    }
                    else if(playerNum != 2)
                    {
                        speedMo->flags |= playerNum << MF_TRANSSHIFT;
                    }
                }
                else if(playerNum)
                {               // Set color translation bits for player sprites
                    speedMo->flags |= playerNum << MF_TRANSSHIFT;
                }
                speedMo->target = plrmo;
                speedMo->special1 = player->class;
                if(speedMo->special1 > 2)
                {
                    speedMo->special1 = 0;
                }
                speedMo->sprite = plrmo->sprite;
                speedMo->floorclip = plrmo->floorclip;
                if(player == &players[consoleplayer])
                {
                    speedMo->flags2 |= MF2_DONTDRAW;
                }
            }
        }
#endif
    }

    P_CalcHeight(player);

    if(P_XSector(P_GetPtrp(player->plr->mo->subsector, DMU_SECTOR))->special)
        P_PlayerInSpecialSector(player);

#if __JHEXEN__
    if((floorType = P_GetThingFloorType(player->plr->mo)) != FLOOR_SOLID)
    {
        P_PlayerOnSpecialFlat(player, floorType);
    }
    switch (player->class)
    {
    case PCLASS_FIGHTER:
        if(player->plr->mo->momz <= -35 * FRACUNIT &&
           player->plr->mo->momz >= -40 * FRACUNIT && !player->morphTics &&
           !S_IsPlaying(SFX_PLAYER_FIGHTER_FALLING_SCREAM, player->plr->mo))
        {
            S_StartSound(SFX_PLAYER_FIGHTER_FALLING_SCREAM, player->plr->mo);
        }
        break;
    case PCLASS_CLERIC:
        if(player->plr->mo->momz <= -35 * FRACUNIT &&
           player->plr->mo->momz >= -40 * FRACUNIT && !player->morphTics &&
           !S_IsPlaying(SFX_PLAYER_CLERIC_FALLING_SCREAM, player->plr->mo))
        {
            S_StartSound(SFX_PLAYER_CLERIC_FALLING_SCREAM, player->plr->mo);
        }
        break;
    case PCLASS_MAGE:
        if(player->plr->mo->momz <= -35 * FRACUNIT &&
           player->plr->mo->momz >= -40 * FRACUNIT && !player->morphTics &&
           !S_IsPlaying(SFX_PLAYER_MAGE_FALLING_SCREAM, player->plr->mo))
        {
            S_StartSound(SFX_PLAYER_MAGE_FALLING_SCREAM, player->plr->mo);
        }
        break;
    default:
        break;
    }
#endif

    if(player->jumptics)
        player->jumptics--;

#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    if(cmd->arti)
    {                           // Use an artifact
        if(cmd->arti == NUMARTIFACTS)
        {                       // use one of each artifact (except puzzle artifacts)
            int     i;
# if __JHEXEN__ || __JSTRIFE__
            for(i = arti_none + 1; i < arti_firstpuzzitem; i++)
# else
            for(i = arti_none + 1; i < NUMARTIFACTS; i++)
# endif
            {
                P_InventoryUseArtifact(player, i);
            }
        }
        else if(cmd->arti == 0xff)
        {
            P_InventoryNextArtifact(player);
        }
        else
        {
            P_InventoryUseArtifact(player, cmd->arti);
        }
    }
#endif

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
#if __JHEXEN__
    if(cmd->changeWeapon && !player->morphTics)
#else
    if(cmd->changeWeapon)
#endif
    {
        // The actual changing of the weapon is done
        //  when the weapon psprite can do it
        //  (read: not in the middle of an attack).
        newweapon = cmd->changeWeapon - 1;
#if __JDOOM__
        if(gamemode != commercial && newweapon == wp_supershotgun)
        {
            // In non-Doom II, supershotgun is the same as normal shotgun.
            newweapon = wp_shotgun;
        }
#endif
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
    {
#if __JDOOM__
        player->update |= PSF_PENDING_WEAPON | PSF_READY_WEAPON;
#elif __JHEXEN__
        player->update |= PSF_PENDING_WEAPON;
#endif
    }

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

#if __JHEXEN__
    // Morph counter
    if(player->morphTics)
    {
        if(!--player->morphTics)
        {                       // Attempt to undo the pig
            P_UndoPlayerMorph(player);
        }
    }
#endif

    // cycle psprites
    P_MovePsprites(player);

    // Counters, time dependend power ups.

#if __JDOOM__
    // Strength counts up to diminish fade.
    if(player->powers[pw_strength])
        player->powers[pw_strength]++;

    if(player->powers[pw_ironfeet])
        player->powers[pw_ironfeet]--;
#endif

#if __JDOOM__ || __JHERETIC__
    if(player->powers[pw_invulnerability])
        player->powers[pw_invulnerability]--;

    if(player->powers[pw_invisibility])
    {
        if(!--player->powers[pw_invisibility])
            player->plr->mo->flags &= ~MF_SHADOW;
    }
#endif

#if __JDOOM__ || __JHEXEN__
    if(player->powers[pw_infrared])
        player->powers[pw_infrared]--;
#endif

    if(player->damagecount)
        player->damagecount--;

    if(player->bonuscount)
        player->bonuscount--;

#if __JHERETIC__ || __JHEXEN__
# if __JHERETIC__
    if(player->powers[pw_flight])
# elif __JHEXEN__
    if(player->powers[pw_flight] && IS_NETGAME)
# endif
    {
        if(!--player->powers[pw_flight])
        {
            if(player->plr->mo->pos[VZ] != player->plr->mo->floorz)
            {
                player->centering = true;
            }
            player->plr->mo->flags2 &= ~MF2_FLY;
            player->plr->mo->flags &= ~MF_NOGRAVITY;
            //BorderTopRefresh = true; //make sure the sprite's cleared out
            GL_Update(DDUF_TOP);
        }
    }
#endif

#if __JHERETIC__
    if(player->powers[pw_weaponlevel2])
    {
        if(!--player->powers[pw_weaponlevel2])
        {
            if((player->readyweapon == WP_SIXTH) &&
               (player->psprites[ps_weapon].state != &states[S_PHOENIXREADY])
               && (player->psprites[ps_weapon].state != &states[S_PHOENIXUP]))
            {
                P_SetPsprite(player, ps_weapon, S_PHOENIXREADY);
                player->ammo[am_phoenixrod] -= USE_PHRD_AMMO_2;
                player->refire = 0;
                player->update |= PSF_AMMO;
            }
            else if((player->readyweapon == WP_EIGHTH) ||
                    (player->readyweapon == WP_FIRST))
            {
                player->pendingweapon = player->readyweapon;
                player->update |= PSF_PENDING_WEAPON;
            }
            //BorderTopRefresh = true;
            GL_Update(DDUF_TOP);
        }
    }
#endif

    // Colormaps
#if __JHERETIC__ || __JHEXEN__
    if(player->powers[pw_infrared])
    {
        if(player->powers[pw_infrared] <= BLINKTHRESHOLD)
        {
            if(player->powers[pw_infrared] & 8)
            {
                player->plr->fixedcolormap = 0;
            }
            else
            {
                player->plr->fixedcolormap = 1;
            }
        }
        else if(!(leveltime & 16))  /* && player == &players[consoleplayer]) */
        {
            ddplayer_t *dp = player->plr;
            int     playerNumber = player - players;

            if(newtorch[playerNumber])
            {
                if(dp->fixedcolormap + newtorchdelta[playerNumber] > 7 ||
                   dp->fixedcolormap + newtorchdelta[playerNumber] < 1 ||
                   newtorch[playerNumber] == dp->fixedcolormap)
                {
                    newtorch[playerNumber] = 0;
                }
                else
                {
                    dp->fixedcolormap += newtorchdelta[playerNumber];
                }
            }
            else
            {
                newtorch[playerNumber] = (M_Random() & 7) + 1;
                newtorchdelta[playerNumber] =
                    (newtorch[playerNumber] ==
                     dp->fixedcolormap) ? 0 : ((newtorch[playerNumber] >
                                                dp->fixedcolormap) ? 1 : -1);
            }
        }
    }
    else
    {
        player->plr->fixedcolormap = 0;
    }

# if __JHEXEN__
    if(player->powers[pw_invulnerability])
    {
        if(player->class == PCLASS_CLERIC)
        {
            if(!(leveltime & 7) && player->plr->mo->flags & MF_SHADOW &&
               !(player->plr->mo->flags2 & MF2_DONTDRAW))
            {
                player->plr->mo->flags &= ~MF_SHADOW;
                if(!(player->plr->mo->flags & MF_ALTSHADOW))
                {
                    player->plr->mo->flags2 |= MF2_DONTDRAW | MF2_NONSHOOTABLE;
                }
            }
            if(!(leveltime & 31))
            {
                if(player->plr->mo->flags2 & MF2_DONTDRAW)
                {
                    if(!(player->plr->mo->flags & MF_SHADOW))
                    {
                        player->plr->mo->flags |= MF_SHADOW | MF_ALTSHADOW;
                    }
                    else
                    {
                        player->plr->mo->flags2 &=
                            ~(MF2_DONTDRAW | MF2_NONSHOOTABLE);
                    }
                }
                else
                {
                    player->plr->mo->flags |= MF_SHADOW;
                    player->plr->mo->flags &= ~MF_ALTSHADOW;
                }
            }
        }
        if(!(--player->powers[pw_invulnerability]))
        {
            player->plr->mo->flags2 &= ~(MF2_INVULNERABLE | MF2_REFLECTIVE);
            if(player->class == PCLASS_CLERIC)
            {
                player->plr->mo->flags2 &= ~(MF2_DONTDRAW | MF2_NONSHOOTABLE);
                player->plr->mo->flags &= ~(MF_SHADOW | MF_ALTSHADOW);
            }
        }
    }
    if(player->powers[pw_minotaur])
    {
        player->powers[pw_minotaur]--;
    }

    if(player->powers[pw_speed])
    {
        player->powers[pw_speed]--;
    }

    if(player->poisoncount && !(leveltime & 15))
    {
        player->poisoncount -= 5;
        if(player->poisoncount < 0)
        {
            player->poisoncount = 0;
        }
        P_PoisonDamage(player, player->poisoner, 1, true);
    }
# endif
#endif
}
