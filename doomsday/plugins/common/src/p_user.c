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

#if __JDOOM__
# include "doomdef.h"
# include "d_config.h"
# include "d_event.h"
# include "p_local.h"
# include "p_inter.h"
# include "doomstat.h"
# include "g_common.h"
#elif __JHERETIC__
# include "jheretic.h"
# include "r_common.h"
#elif __JHEXEN__
# include <math.h>
# include "jhexen.h"
#endif

#include "p_player.h"
#include "p_view.h"
#include "d_net.h"
#include "p_player.h"

// MACROS ------------------------------------------------------------------

#if __JDOOM__ || __JHERETIC__
// Index of the special effects (INVUL inverse) map.
#define INVERSECOLORMAP     32

#define ANG5    (ANG90/18)

#elif __JHEXEN__

#define BLAST_RADIUS_DIST   255*FRACUNIT
#define BLAST_SPEED         20*FRACUNIT
#define BLAST_FULLSTRENGTH  255
#define HEAL_RADIUS_DIST    255*FRACUNIT

#endif

// 16 pixels of bob
#define MAXBOB  0x100000

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------
#if __JHERETIC__ || __JHEXEN__
void    P_PlayerNextArtifact(player_t *player);
#endif

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------
#if __JHERETIC__
extern int inv_ptr;
extern int curpos;
#endif

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
    }
};
#elif __JHERETIC__
int     newtorch;               // used in the torch flicker effect.
int     newtorchdelta;

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
    },
};
#elif __JHEXEN__
int     lookdirSpeed = 3, quakeFly = 0;
int     newtorch[MAXPLAYERS];   // used in the torch flicker effect.
int     newtorchdelta[MAXPLAYERS];

classinfo_t classInfo[NUMCLASSES] = {
    {   // Fighter
        S_FPLAY,
        S_FPLAY_RUN1,
        S_FPLAY_ATK1,
        S_FPLAY_ATK2,
        20,
        15 * FRACUNIT,
        0x3C,
        {0x1D, 0x3C},
        {0x1B, 0x3B},
        {25 * FRACUNIT, 20 * FRACUNIT, 15 * FRACUNIT, 5 * FRACUNIT},
        {190, 225, 234}
    },
    {   // Cleric
        S_CPLAY,
        S_CPLAY_RUN1,
        S_CPLAY_ATK1,
        S_CPLAY_ATK3,
        18,
        10 * FRACUNIT,
        0x32,
        {0x19, 0x32},
        {0x18, 0x28},
        {10 * FRACUNIT, 25 * FRACUNIT, 5 * FRACUNIT, 20 * FRACUNIT},
        {190, 212, 225}
    },
    {   // Mage
        S_MPLAY,
        S_MPLAY_RUN1,
        S_MPLAY_ATK1,
        S_MPLAY_ATK2,
        16,
        5 * FRACUNIT,
        0x2D,
        {0x16, 0x2E},
        {0x15, 0x25},
        {5 * FRACUNIT, 15 * FRACUNIT, 10 * FRACUNIT, 25 * FRACUNIT},
        {190, 205, 224}
    },
    {   // Pig
        S_PIGPLAY,
        S_PIGPLAY_RUN1,
        S_PIGPLAY_ATK1,
        S_PIGPLAY_ATK1,
        1,
        0,
        0x31,
        {0x18, 0x31},
        {0x17, 0x27},
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
#if __JDOOM__ || __JHERETIC__
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
#endif

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
#if __JHERETIC__
        if(player->morphTics)
        {                           // Chicken speed
            if(cmd->forwardMove && (onground || plrmo->flags2 & MF2_FLY))
                P_Thrust(player, plrmo->angle, cmd->forwardMove * 2500);
            if(cmd->sideMove && (onground || plrmo->flags2 & MF2_FLY))
                P_Thrust(player, plrmo->angle - ANG90, cmd->sideMove * 2500);
        }
        else
#endif
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
#if __JDOOM__ || __JHERETIC__
        P_CheckPlayerJump(player);
#endif
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
# if __JHERETIC__
            newtorch = 0;
            newtorchdelta = 0;
            H_SetFilter(0);
# else
            H2_SetFilter(0);
# endif
        }
# if __JHEXEN__
        newtorch[player - players] = 0;
        newtorchdelta[player - players] = 0;
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
# if __JHEXEN__
    int     oldBeast;

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
# endif
    P_SetMobjState(pmo, S_FREETARGMOBJ);

    playerNum = P_GetPlayerNum(player);
# if __JHEXEN__
    switch (cfg.PlayerClass[playerNum])
    {
    case PCLASS_FIGHTER:
        mo = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_PLAYER_FIGHTER);
        break;
    case PCLASS_CLERIC:
        mo = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_PLAYER_CLERIC);
        break;
    case PCLASS_MAGE:
        mo = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_PLAYER_MAGE);
        break;
    default:
        Con_Error("P_UndoPlayerMorph:  Unknown player class %d\n",
                  player->class);
    }
# else
    mo = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_PLAYER);
# endif

    if(P_TestMobjLocation(mo) == false)
    {   // Didn't fit
        P_RemoveMobj(mo);
# if __JHERETIC__
        mo = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_CHICPLAYER);
# else
        mo = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], oldBeast);
# endif
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

#if __JHEXEN__
void P_PlayerJump(player_t *player)
{
    float   power;

    // Check if we are allowed to jump.
    if((!IS_CLIENT || netJumpPower > 0) &&
       P_IsPlayerOnGround(player) &&
       player->jumpTics <= 0)
    {
        power = (IS_CLIENT ? netJumpPower : cfg.jumpPower);

        if(player->morphTics) // Pigs don't jump that high.
            player->plr->mo->momz = (2 * power / 3) * FRACUNIT;
        else
            player->plr->mo->momz = power * FRACUNIT;

        player->plr->mo->flags2 &= ~MF2_ONMOBJ;
        player->jumpTics = 18;
    }
}
#endif

#if __JHERETIC__ || __JHEXEN__
void P_ArtiTele(player_t *player)
{
    int     i;
    int     selections;
    fixed_t destX;
    fixed_t destY;
    angle_t destAngle;

    if(deathmatch)
    {
        selections = deathmatch_p - deathmatchstarts;
        i = P_Random() % selections;
        destX = deathmatchstarts[i].x << FRACBITS;
        destY = deathmatchstarts[i].y << FRACBITS;
        destAngle = ANG45 * (deathmatchstarts[i].angle / 45);
    }
    else
    {
        // FIXME?: DJS - this doesn't seem right...
        destX = playerstarts[0].x << FRACBITS;
        destY = playerstarts[0].y << FRACBITS;
        destAngle = ANG45 * (playerstarts[0].angle / 45);
    }

# if __JHEXEN__
    P_Teleport(player->plr->mo, destX, destY, destAngle, true);
    if(player->morphTics)
    {   // Teleporting away will undo any morph effects (pig)
        P_UndoPlayerMorph(player);
    }
    //S_StartSound(NULL, sfx_wpnup); // Full volume laugh
# else
    P_Teleport(player->plr->mo, destX, destY, destAngle);
    /*S_StartSound(sfx_wpnup, NULL); // Full volume laugh
       NetSv_Sound(NULL, sfx_wpnup, player-players); */
    S_StartSound(sfx_wpnup, NULL);
# endif
}
#endif

#if __JHEXEN__
void P_ArtiTeleportOther(player_t *player)
{
    mobj_t *mo;

    mo = P_SpawnPlayerMissile(player->plr->mo, MT_TELOTHER_FX1);

    if(mo)
        mo->target = player->plr->mo;
}

void P_TeleportToPlayerStarts(mobj_t *victim)
{
    int     i, selections = 0;
    fixed_t destX, destY;
    angle_t destAngle;
    thing_t *start;

    for(i = 0; i < MAXPLAYERS; i++)
    {
        if(!players[i].plr->ingame)
            continue;

        selections++;
    }

    i = P_Random() % selections;
    start = P_GetPlayerStart(0, i);

    destX = start->x << FRACBITS;
    destY = start->y << FRACBITS;
    destAngle = ANG45 * (playerstarts[i].angle / 45);

    P_Teleport(victim, destX, destY, destAngle, true);
    //S_StartSound(NULL, sfx_wpnup); // Full volume laugh
}

void P_TeleportToDeathmatchStarts(mobj_t *victim)
{
    int     i, selections;
    fixed_t destX, destY;
    angle_t destAngle;

    selections = deathmatch_p - deathmatchstarts;
    if(selections)
    {
        i = P_Random() % selections;
        destX = deathmatchstarts[i].x << FRACBITS;
        destY = deathmatchstarts[i].y << FRACBITS;
        destAngle = ANG45 * (deathmatchstarts[i].angle / 45);

        P_Teleport(victim, destX, destY, destAngle, true);
        //S_StartSound(NULL, sfx_wpnup); // Full volume laugh
    }
    else
    {
        P_TeleportToPlayerStarts(victim);
    }
}

void P_TeleportOther(mobj_t *victim)
{
    if(victim->player)
    {
        if(deathmatch)
            P_TeleportToDeathmatchStarts(victim);
        else
            P_TeleportToPlayerStarts(victim);
    }
    else
    {
        // If death action, run it upon teleport
        if(victim->flags & MF_COUNTKILL && victim->special)
        {
            P_RemoveMobjFromTIDList(victim);
            P_ExecuteLineSpecial(victim->special, victim->args, NULL, 0,
                                 victim);
            victim->special = 0;
        }

        // Send all monsters to deathmatch spots
        P_TeleportToDeathmatchStarts(victim);
    }
}

void ResetBlasted(mobj_t *mo)
{
    mo->flags2 &= ~MF2_BLASTED;
    if(!(mo->flags & MF_ICECORPSE))
    {
        mo->flags2 &= ~MF2_SLIDE;
    }
}

void P_BlastMobj(mobj_t *source, mobj_t *victim, fixed_t strength)
{
    angle_t angle, ang;
    mobj_t *mo;
    fixed_t pos[3];

    angle = R_PointToAngle2(source->pos[VX], source->pos[VY],
                            victim->pos[VX], victim->pos[VY]);
    angle >>= ANGLETOFINESHIFT;
    if(strength < BLAST_FULLSTRENGTH)
    {
        victim->momx = FixedMul(strength, finecosine[angle]);
        victim->momy = FixedMul(strength, finesine[angle]);
        if(victim->player)
        {
            // Players handled automatically
        }
        else
        {
            victim->flags2 |= MF2_SLIDE;
            victim->flags2 |= MF2_BLASTED;
        }
    }
    else // full strength blast from artifact
    {
        if(victim->flags & MF_MISSILE)
        {
            switch (victim->type)
            {
            case MT_SORCBALL1: // don't blast sorcerer balls
            case MT_SORCBALL2:
            case MT_SORCBALL3:
                return;
                break;
            case MT_MSTAFF_FX2: // Reflect to originator
                victim->special1 = (int) victim->target;
                victim->target = source;
                break;
            default:
                break;
            }
        }
        if(victim->type == MT_HOLY_FX)
        {
            if((mobj_t *) (victim->special1) == source)
            {
                victim->special1 = (int) victim->target;
                victim->target = source;
            }
        }
        victim->momx = FixedMul(BLAST_SPEED, finecosine[angle]);
        victim->momy = FixedMul(BLAST_SPEED, finesine[angle]);

        // Spawn blast puff
        ang = R_PointToAngle2(victim->pos[VX], victim->pos[VY],
                              source->pos[VX], source->pos[VY]);
        ang >>= ANGLETOFINESHIFT;

        memcpy(pos, victim->pos, sizeof(pos));
        pos[VX] += FixedMul(victim->radius + FRACUNIT, finecosine[ang]);
        pos[VY] += FixedMul(victim->radius + FRACUNIT, finesine[ang]);
        pos[VZ] -= victim->floorclip + (victim->height >> 1);

        mo = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_BLASTEFFECT);
        if(mo)
        {
            mo->momx = victim->momx;
            mo->momy = victim->momy;
        }

        if(victim->flags & MF_MISSILE)
        {
            victim->momz = 8 * FRACUNIT;
            mo->momz = victim->momz;
        }
        else
        {
            victim->momz = (1000 / victim->info->mass) << FRACBITS;
        }

        if(victim->player)
        {
            // Players handled automatically
        }
        else
        {
            victim->flags2 |= MF2_SLIDE;
            victim->flags2 |= MF2_BLASTED;
        }
    }
}

/*
 * Blast all mobj things away
 */
void P_BlastRadius(player_t *player)
{
    mobj_t *mo;
    mobj_t *pmo = player->plr->mo;
    thinker_t *think;
    fixed_t dist;

    S_StartSound(SFX_ARTIFACT_BLAST, pmo);
    P_NoiseAlert(player->plr->mo, player->plr->mo);

    for(think = gi.thinkercap->next; think != gi.thinkercap;
        think = think->next)
    {
        if(think->function != P_MobjThinker)
            continue; // Not a mobj thinker

        mo = (mobj_t *) think;
        if((mo == pmo) || (mo->flags2 & MF2_BOSS))
            continue; // Not a valid monster

        if((mo->type == MT_POISONCLOUD) ||  // poison cloud
           (mo->type == MT_HOLY_FX) ||  // holy fx
           (mo->flags & MF_ICECORPSE))  // frozen corpse
        {
            // Let these special cases go
        }
        else if((mo->flags & MF_COUNTKILL) && (mo->health <= 0))
        {
            continue;
        }
        else if(!(mo->flags & MF_COUNTKILL) && !(mo->player) &&
                !(mo->flags & MF_MISSILE))
        {                       // Must be monster, player, or missile
            continue;
        }

        if(mo->flags2 & MF2_DORMANT)
            continue;           // no dormant creatures

        if((mo->type == MT_WRAITHB) && (mo->flags2 & MF2_DONTDRAW))
            continue;           // no underground wraiths

        if((mo->type == MT_SPLASHBASE) || (mo->type == MT_SPLASH))
            continue;

        if(mo->type == MT_SERPENT || mo->type == MT_SERPENTLEADER)
            continue;

        dist = P_ApproxDistance(pmo->pos[VX] - mo->pos[VX],
                                pmo->pos[VY] - mo->pos[VY]);
        if(dist > BLAST_RADIUS_DIST)
            continue; // Out of range

        P_BlastMobj(pmo, mo, BLAST_FULLSTRENGTH);
    }
}

/*
 * Do class specific effect for everyone in radius
 */
boolean P_HealRadius(player_t *player)
{
    mobj_t *mo;
    mobj_t *pmo = player->plr->mo;
    thinker_t *think;
    fixed_t dist;
    int     effective = false;
    int     amount;

    for(think = gi.thinkercap->next; think != gi.thinkercap;
        think = think->next)
    {
        if(think->function != P_MobjThinker)
            continue; // Not a mobj thinker

        mo = (mobj_t *) think;
        if(!mo->player || mo->health <= 0)
            continue;

        dist = P_ApproxDistance(pmo->pos[VX] - mo->pos[VX],
                                pmo->pos[VY] - mo->pos[VY]);
        if(dist > HEAL_RADIUS_DIST)
            continue; // Out of range

        switch (player->class)
        {
        case PCLASS_FIGHTER: // Radius armor boost
            if((P_GiveArmor(mo->player, ARMOR_ARMOR, 1)) ||
               (P_GiveArmor(mo->player, ARMOR_SHIELD, 1)) ||
               (P_GiveArmor(mo->player, ARMOR_HELMET, 1)) ||
               (P_GiveArmor(mo->player, ARMOR_AMULET, 1)))
            {
                effective = true;
                S_StartSound(SFX_MYSTICINCANT, mo);
            }
            break;
        case PCLASS_CLERIC: // Radius heal
            amount = 50 + (P_Random() % 50);
            if(P_GiveBody(mo->player, amount))
            {
                effective = true;
                S_StartSound(SFX_MYSTICINCANT, mo);
            }
            break;
        case PCLASS_MAGE: // Radius mana boost
            amount = 50 + (P_Random() % 50);
            if((P_GiveMana(mo->player, MANA_1, amount)) ||
               (P_GiveMana(mo->player, MANA_2, amount)))
            {
                effective = true;
                S_StartSound(SFX_MYSTICINCANT, mo);
            }
            break;
        case PCLASS_PIG:
        default:
            break;
        }
    }
    return (effective);
}
#endif

#if __JHERETIC__
void P_CheckReadyArtifact()
{
    player_t *player = &players[consoleplayer];

    if(!player->inventory[inv_ptr].count)
    {
        // Set position markers and get next readyArtifact
        inv_ptr--;
        if(inv_ptr < 6)
        {
            curpos--;
            if(curpos < 0)
            {
                curpos = 0;
            }
        }
        if(inv_ptr >= player->inventorySlotNum)
        {
            inv_ptr = player->inventorySlotNum - 1;
        }
        if(inv_ptr < 0)
        {
            inv_ptr = 0;
        }
        player->readyArtifact = player->inventory[inv_ptr].type;

        if(!player->inventorySlotNum)
            player->readyArtifact = arti_none;
    }
}
#endif

#if __JHERETIC__ || __JHEXEN__
void P_PlayerNextArtifact(player_t *player)
{
    if(player == &players[consoleplayer])
    {
        inv_ptr--;
        if(inv_ptr < 6)
        {
            curpos--;
            if(curpos < 0)
            {
                curpos = 0;
            }
        }
        if(inv_ptr < 0)
        {
            inv_ptr = player->inventorySlotNum - 1;
            if(inv_ptr < 6)
            {
                curpos = inv_ptr;
            }
            else
            {
                curpos = 6;
            }
        }
        player->readyArtifact = player->inventory[inv_ptr].type;
    }
}

void P_PlayerRemoveArtifact(player_t *player, int slot)
{
    int     i;
    extern int inv_ptr;
    extern int curpos;

    player->update |= PSF_INVENTORY;
    player->artifactCount--;
    if(!(--player->inventory[slot].count))
    {   // Used last of a type - compact the artifact list
        player->readyArtifact = arti_none;
        player->inventory[slot].type = arti_none;
        for(i = slot + 1; i < player->inventorySlotNum; i++)
        {
            player->inventory[i - 1] = player->inventory[i];
        }
        player->inventorySlotNum--;
        if(player == &players[consoleplayer])
        {   // Set position markers and get next readyArtifact
            inv_ptr--;
            if(inv_ptr < 6)
            {
                curpos--;
                if(curpos < 0)
                {
                    curpos = 0;
                }
            }
            if(inv_ptr >= player->inventorySlotNum)
            {
                inv_ptr = player->inventorySlotNum - 1;
            }
            if(inv_ptr < 0)
            {
                inv_ptr = 0;
            }
            player->readyArtifact = player->inventory[inv_ptr].type;
        }
    }
}

void P_PlayerUseArtifact(player_t *player, artitype_t arti)
{
    int     i;
# if __JHERETIC__
    boolean play_sound = false;
# endif
    for(i = 0; i < player->inventorySlotNum; i++)
    {
# if __JHERETIC__
        if(arti == NUMARTIFACTS)    // Use everything in panic?
        {
            if(P_UseArtifact(player, player->inventory[i].type))
            {                   // Artifact was used - remove it from inventory
                P_PlayerRemoveArtifact(player, i);
                play_sound = true;
                if(player == &players[consoleplayer])
                    ArtifactFlash = 4;
            }
        }
        else
# endif
        if(player->inventory[i].type == arti)
        {   // Found match - try to use
            if(P_UseArtifact(player, arti))
            {   // Artifact was used - remove it from inventory
                P_PlayerRemoveArtifact(player, i);
# if __JHERETIC__
                play_sound = true;
# else
                if(arti < arti_firstpuzzitem)
                {
                    S_ConsoleSound(SFX_ARTIFACT_USE, NULL, player - players);
                }
                else
                {
                    S_ConsoleSound(SFX_PUZZLE_SUCCESS, NULL, player - players);
                }
# endif
                if(player == &players[consoleplayer])
                    ArtifactFlash = 4;
            }
# if __JHERETIC__
            else
# else
            else if(arti < arti_firstpuzzitem)
# endif
            {   // Unable to use artifact, advance pointer
                P_PlayerNextArtifact(player);
            }
            break;
        }
    }

# if __JHERETIC__
    if(play_sound)
        S_ConsoleSound(sfx_artiuse, NULL, player - players);
# endif
}

/*
 * Returns true if the artifact was used.
 */
boolean P_UseArtifact(player_t *player, artitype_t arti)
{
    mobj_t *mo;
    angle_t angle;
# if __JHEXEN__
    int     i;
    int     count;
# endif

    switch (arti)
    {
    case arti_invulnerability:
        if(!P_GivePower(player, pw_invulnerability))
        {
            return (false);
        }
        break;
# if __JHERETIC__
    case arti_invisibility:
        if(!P_GivePower(player, pw_invisibility))
        {
            return (false);
        }
        break;
# endif
    case arti_health:
        if(!P_GiveBody(player, 25))
        {
            return (false);
        }
        break;
    case arti_superhealth:
        if(!P_GiveBody(player, 100))
        {
            return (false);
        }
        break;
# if __JHEXEN__
    case arti_healingradius:
        if(!P_HealRadius(player))
        {
            return (false);
        }
        break;
# endif
# if __JHERETIC__
    case arti_tomeofpower:
        if(player->morphTics)
        {                       // Attempt to undo chicken
            if(P_UndoPlayerMorph(player) == false)
            {                   // Failed
                P_DamageMobj(player->plr->mo, NULL, NULL, 10000);
            }
            else
            {                   // Succeeded
                player->morphTics = 0;
                S_StartSound(sfx_wpnup, player->plr->mo);
            }
        }
        else
        {
            if(!P_GivePower(player, pw_weaponlevel2))
            {
                return (false);
            }
            if(player->readyweapon == WP_FIRST)
            {
                P_SetPsprite(player, ps_weapon, S_STAFFREADY2_1);
            }
            else if(player->readyweapon == WP_EIGHTH)
            {
                P_SetPsprite(player, ps_weapon, S_GAUNTLETREADY2_1);
            }
        }
        break;
# endif
    case arti_torch:
        if(!P_GivePower(player, pw_infrared))
        {
            return (false);
        }
        break;
# if __JHERETIC__
    case arti_firebomb:
        angle = player->plr->mo->angle >> ANGLETOFINESHIFT;
        mo = P_SpawnMobj(player->plr->mo->pos[VX] + 24 * finecosine[angle],
                         player->plr->mo->pos[VY] + 24 * finesine[angle],
                         player->plr->mo->pos[VZ] - player->plr->mo->floorclip +
                         15 * FRACUNIT, MT_FIREBOMB);
        mo->target = player->plr->mo;
        break;
# endif
    case arti_egg:
        mo = player->plr->mo;
        P_SpawnPlayerMissile(mo, MT_EGGFX);
        P_SPMAngle(mo, MT_EGGFX, mo->angle - (ANG45 / 6));
        P_SPMAngle(mo, MT_EGGFX, mo->angle + (ANG45 / 6));
        P_SPMAngle(mo, MT_EGGFX, mo->angle - (ANG45 / 3));
        P_SPMAngle(mo, MT_EGGFX, mo->angle + (ANG45 / 3));
        break;
    case arti_teleport:
        P_ArtiTele(player);
        break;
    case arti_fly:
        if(!P_GivePower(player, pw_flight))
        {
            return (false);
        }
# if __JHEXEN__
        if(player->plr->mo->momz <= -35 * FRACUNIT)
        {   // stop falling scream
            S_StopSound(0, player->plr->mo);
        }
#endif
        break;
# if __JHEXEN__
    case arti_summon:
        mo = P_SpawnPlayerMissile(player->plr->mo, MT_SUMMON_FX);
        if(mo)
        {
            mo->target = player->plr->mo;
            mo->special1 = (int) (player->plr->mo);
            mo->momz = 5 * FRACUNIT;
        }
        break;
    case arti_teleportother:
        P_ArtiTeleportOther(player);
        break;
    case arti_poisonbag:
        angle = player->plr->mo->angle >> ANGLETOFINESHIFT;
        if(player->class == PCLASS_CLERIC)
        {
            mo = P_SpawnMobj(player->plr->mo->pos[VX] + 16 * finecosine[angle],
                             player->plr->mo->pos[VY] + 24 * finesine[angle],
                             player->plr->mo->pos[VZ] - player->plr->mo->floorclip +
                             8 * FRACUNIT, MT_POISONBAG);
            if(mo)
            {
                mo->target = player->plr->mo;
            }
        }
        else if(player->class == PCLASS_MAGE)
        {
            mo = P_SpawnMobj(player->plr->mo->pos[VX] + 16 * finecosine[angle],
                             player->plr->mo->pos[VY] + 24 * finesine[angle],
                             player->plr->mo->pos[VZ] - player->plr->mo->floorclip +
                             8 * FRACUNIT, MT_FIREBOMB);
            if(mo)
            {
                mo->target = player->plr->mo;
            }
        }
        else // PCLASS_FIGHTER, obviously (also pig, not so obviously)
        {
            mo = P_SpawnMobj(player->plr->mo->pos[VX],
                             player->plr->mo->pos[VY],
                             player->plr->mo->pos[VZ] - player->plr->mo->floorclip +
                             35 * FRACUNIT, MT_THROWINGBOMB);
            if(mo)
            {
                mo->angle =
                    player->plr->mo->angle + (((P_Random() & 7) - 4) << 24);
                mo->momz =
                    4 * FRACUNIT +
                    (((int) player->plr->lookdir) << (FRACBITS - 4));
                mo->pos[VZ] += ((int) player->plr->lookdir) << (FRACBITS - 4);
                P_ThrustMobj(mo, mo->angle, mo->info->speed);
                mo->momx += player->plr->mo->momx >> 1;
                mo->momy += player->plr->mo->momy >> 1;
                mo->target = player->plr->mo;
                mo->tics -= P_Random() & 3;
                P_CheckMissileSpawn(mo);
            }
        }
        break;
    case arti_speed:
        if(!P_GivePower(player, pw_speed))
        {
            return (false);
        }
        break;
    case arti_boostmana:
        if(!P_GiveMana(player, MANA_1, MAX_MANA))
        {
            if(!P_GiveMana(player, MANA_2, MAX_MANA))
            {
                return false;
            }

        }
        else
        {
            P_GiveMana(player, MANA_2, MAX_MANA);
        }
        break;
    case arti_boostarmor:
        count = 0;

        for(i = 0; i < NUMARMOR; i++)
        {
            count += P_GiveArmor(player, i, 1); // 1 point per armor type
        }
        if(!count)
        {
            return false;
        }
        break;
    case arti_blastradius:
        P_BlastRadius(player);
        break;

    case arti_puzzskull:
    case arti_puzzgembig:
    case arti_puzzgemred:
    case arti_puzzgemgreen1:
    case arti_puzzgemgreen2:
    case arti_puzzgemblue1:
    case arti_puzzgemblue2:
    case arti_puzzbook1:
    case arti_puzzbook2:
    case arti_puzzskull2:
    case arti_puzzfweapon:
    case arti_puzzcweapon:
    case arti_puzzmweapon:
    case arti_puzzgear1:
    case arti_puzzgear2:
    case arti_puzzgear3:
    case arti_puzzgear4:
        if(P_UsePuzzleItem(player, arti - arti_firstpuzzitem))
        {
            return true;
        }
        else
        {
            P_SetYellowMessage(player, TXT_USEPUZZLEFAILED);
            return false;
        }
        break;
# endif
    default:
        return false;
    }
    return true;
}
#endif

#if __JHEXEN__
void C_DECL A_SpeedFade(mobj_t *actor)
{
    actor->flags |= MF_SHADOW;
    actor->flags &= ~MF_ALTSHADOW;
    actor->sprite = actor->target->sprite;
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
    if(!pl->messageTics
#if __JHEXEN__
       || pl->messageTics == -1
#endif
       )
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
    if(pl->jumpTics)
        pl->jumpTics--;
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

#if __JDOOM__
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
#elif __JHEXEN__
    if(cmd->jump)
        P_PlayerJump(pl);
#endif

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
    int     playerNumber = player - players;
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
    if(!player->messageTics
#if __JHEXEN__
       || player->messageTics == -1
#endif
      )
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
    if(player->jumpTics)
    {
        player->jumpTics--;
    }
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

    if(cmd->jump && onground && !player->jumpTics)
    {
        P_PlayerJump(player);
    }

    if(cmd->arti)
    {                           // Use an artifact
        if(cmd->arti == NUMARTIFACTS)
        {                       // use one of each artifact (except puzzle artifacts)
            int     i;

            for(i = 1; i < arti_firstpuzzitem; i++)
            {
                P_PlayerUseArtifact(player, i);
            }
        }
        else
        {
            P_PlayerUseArtifact(player, cmd->arti);
        }
    }
#elif __JDOOM__
    if(player->jumptics)
        player->jumptics--;
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
    if(cmd->changeWeapon
#if __JHEXEN__
       && !player->morphTics
#endif
      )
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
#if __JDOOM__
        player->update |= PSF_PENDING_WEAPON | PSF_READY_WEAPON;
#elif __JHEXEN__
        player->update |= PSF_PENDING_WEAPON;
#endif

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
#elif __JHEXEN__
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
    if(player->powers[pw_infrared])
    {
        player->powers[pw_infrared]--;
    }
    if(player->powers[pw_flight] && IS_NETGAME)
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
    if(player->powers[pw_speed])
    {
        player->powers[pw_speed]--;
    }
    if(player->damagecount)
    {
        player->damagecount--;
    }
    if(player->bonuscount)
    {
        player->bonuscount--;
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
    // Colormaps
    //  if(player->powers[pw_invulnerability])
    //  {
    //      if(player->powers[pw_invulnerability] > BLINKTHRESHOLD
    //          || (player->powers[pw_invulnerability]&8))
    //      {
    //          player->plr->fixedcolormap = INVERSECOLORMAP;
    //      }
    //      else
    //      {
    //          player->plr->fixedcolormap = 0;
    //      }
    //  }
    //  else
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
#endif
}
