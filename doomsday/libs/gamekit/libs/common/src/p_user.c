/**\file p_user.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2000-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
 * Player related stuff.
 *
 * Bobbing POV/weapon, movement, pending weapon...
 */

#include "common.h"
#include "p_user.h"

#include <math.h>
#include <string.h>
#include "fi_lib.h"
#include "g_common.h"
#include "d_net.h"
#include "d_netcl.h"
#include "player.h"
#include "p_tick.h" // for Pause_IsPaused()
#include "p_view.h"
#include "player.h"
#include "p_map.h"
#include "g_common.h"
#include "hu_stuff.h"
#include "r_common.h"
#include "g_controls.h"

#if __JHERETIC__ || __JHEXEN__
#  include "p_inventory.h"
#  include "hu_inventory.h"
#endif

#define ANG5                (ANG90/18)

dd_bool onground;
float   turboMul = 1.0f; // Multiplier for running speed.

int maxHealth; // 100
#if __JDOOM__ || __JDOOM64__
int healthLimit; // 200
int godModeHealth; // 100
int soulSphereLimit; // 200
int megaSphereHealth; // 200
int soulSphereHealth; // 100
int armorPoints[4]; // Green, blue, IDFA and IDKFA points.
int armorClass[4]; // Green, blue, IDFA and IDKFA armor classes.
#endif

#if __JDOOM__ || __JDOOM64__
classinfo_t classInfo[NUM_PLAYER_CLASSES] = {
    {   // Player
        PCLASS_PLAYER, NULL, true,
        MT_PLAYER,
        S_PLAY,
        S_PLAY_RUN1,
        S_PLAY_ATK1,
        S_PLAY_ATK2,
        20,
        0x32,
        {0x19, 0x32},
        {0x18, 0x28},
        2048,
        {640, 1280},
        24,
        SFX_NOWAY
    }
};
#elif __JHERETIC__
classinfo_t classInfo[NUM_PLAYER_CLASSES] = {
    {   // Player
        PCLASS_PLAYER, NULL, true,
        MT_PLAYER,
        S_PLAY,
        S_PLAY_RUN1,
        S_PLAY_ATK1,
        S_PLAY_ATK2,
        20,
        0x32,
        {0x19, 0x32},
        {0x18, 0x28},
        2048,
        {640, 1280},
        24,
        SFX_NONE
    },
    {
        PCLASS_CHICKEN, NULL, false,
        MT_CHICPLAYER,
        S_CHICPLAY,
        S_CHICPLAY_RUN1,
        S_CHICPLAY_ATK1,
        S_CHICPLAY_ATK1,
        20,
        0x32,
        {0x19, 0x32},
        {0x18, 0x28},
        2500,
        {640, 1280},
        24,
        SFX_NONE
    },
};
#elif __JHEXEN__
classinfo_t classInfo[NUM_PLAYER_CLASSES] = {
    {
        PCLASS_FIGHTER, NULL, true,
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
        {640, 1280},
        18,
        SFX_PLAYER_FIGHTER_FAILED_USE,
        {25 * FRACUNIT, 20 * FRACUNIT, 15 * FRACUNIT, 5 * FRACUNIT},
        { TXT_SKILLF1, TXT_SKILLF2, TXT_SKILLF3, TXT_SKILLF4, TXT_SKILLF5 },
        { { { 190, 0 }, "WPIECEF1" }, { { 225, 0 }, "WPIECEF2" }, { { 234, 0 }, "WPIECEF3" } },
        "WPFULL0"
    },
    {   // Cleric
        PCLASS_CLERIC, NULL, true,
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
        {640, 1280},
        18,
        SFX_PLAYER_CLERIC_FAILED_USE,
        {10 * FRACUNIT, 25 * FRACUNIT, 5 * FRACUNIT, 20 * FRACUNIT},
        { TXT_SKILLC1, TXT_SKILLC2, TXT_SKILLC3, TXT_SKILLC4, TXT_SKILLC5 },
        { { { 190, 0 }, "WPIECEC1" }, { { 212, 0 }, "WPIECEC2" }, { { 225, 0 }, "WPIECEC3" } },
        "WPFULL1"
    },
    {   // Mage
        PCLASS_MAGE, NULL, true,
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
        {640, 1280},
        18,
        SFX_PLAYER_MAGE_FAILED_USE,
        {5 * FRACUNIT, 15 * FRACUNIT, 10 * FRACUNIT, 25 * FRACUNIT},
        { TXT_SKILLM1, TXT_SKILLM2, TXT_SKILLM3, TXT_SKILLM4, TXT_SKILLM5 },
        { { { 190, 0 }, "WPIECEM1" }, { { 205, 0 }, "WPIECEM2" }, { { 224, 0 }, "WPIECEM3" } },
        "WPFULL2"
    },
    {   // Pig
        PCLASS_PIG, NULL, false,
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
        {640, 1280},
        18
    },
};
#endif

#if __JHERETIC__ || __JHEXEN__
static int newTorch[MAXPLAYERS]; // Used in the torch flicker effect.
static int newTorchDelta[MAXPLAYERS];
#endif

/**
 * Moves the given origin along a given angle.
 */
void P_Thrust(player_t *player, angle_t angle, coord_t move)
{
    mobj_t *mo = player->plr->mo;
    uint an = angle >> ANGLETOFINESHIFT;

    /*coord_t xmul=1, ymul=1;
    // How about Quake-flying? -- jk
    if(quakeFly)
    {
        float ang = LOOKDIR2RAD(player->plr->lookDir);
        xmul = ymul = cos(ang);
        mo->mom[MZ] += sin(ang) * move;
    }*/

    if(!(player->powers[PT_FLIGHT] && !(mo->origin[VZ] <= mo->floorZ)))
    {
        move *= Mobj_ThrustMul(mo);
    }

    mo->mom[MX] += move * FIX2FLT(finecosine[an]);
    mo->mom[MY] += move * FIX2FLT(finesine[an]);
}

/**
 * Returns true if the player is currently standing on ground
 * or on top of another mobj.
 */
dd_bool P_IsPlayerOnGround(player_t* player)
{
    dd_bool onground = (player->plr->mo->origin[VZ] <= player->plr->mo->floorZ);

#if __JHEXEN__
    if((player->plr->mo->onMobj) && !onground)
    {
        onground = true; //(player->plr->mo->origin[VZ] <= on->pos[VZ] + on->height);
    }
#else
    if(player->plr->mo->onMobj && !onground && !(player->plr->mo->flags2 & MF2_FLY))
    {
        mobj_t *on = player->plr->mo->onMobj;

        onground = (player->plr->mo->origin[VZ] <= on->origin[VZ] + on->height);
    }
#endif

    return onground;
}

/**
 * Will make the player jump if the latest command so instructs,
 * providing that jumping is possible.
 */
void P_CheckPlayerJump(player_t* player)
{
    float power = (IS_CLIENT ? netJumpPower : cfg.common.jumpPower);

    if(player->plr->flags & DDPF_CAMERA)
        return; // Cameras don't jump.

    // Check if we are allowed to jump.
    if(cfg.common.jumpEnabled && power > 0 && P_IsPlayerOnGround(player) &&
       player->brain.jump && player->jumpTics <= 0)
    {
        // Jump, then!
#if __JHEXEN__
        if(player->morphTics) // Pigs don't jump that high.
            player->plr->mo->mom[MZ] = (2 * power / 3);
        else
#endif
            player->plr->mo->mom[MZ] = power;

        player->jumpTics = PCLASS_INFO(player->class_)->jumpTics;

#if __JHEXEN__
        player->plr->mo->onMobj = NULL;
#endif
    }
}

/**
 * Moves a player according to its smoother.
 */
void P_PlayerRemoteMove(player_t *player)
{
    int plrNum = player - players;
    ddplayer_t *ddpl = player->plr;
    Smoother *smoother = Net_PlayerSmoother(plrNum);
    mobj_t *mo = player->plr->mo;
    coord_t xyz[3];

    if(!IS_NETGAME || !mo || !smoother)
        return;

    // On client, the console player is not remote.
    if(IS_CLIENT && plrNum == CONSOLEPLAYER)
        return;

#if !defined (DE_MOBILE)
    // On server, there must be valid coordinates.
    if(IS_SERVER && !Sv_CanTrustClientPos(plrNum))
        return;
#endif

    // Unless there is a pending momentum fix, clear the mobj's momentum.
    if(ddpl->fixCounter.mom == ddpl->fixAcked.mom && !(ddpl->flags & DDPF_FIXMOM))
    {
        // As the mobj is being moved by the smoother, it has no momentum in the regular
        // physics sense.
        mo->mom[VX] = 0;
        mo->mom[VY] = 0;
        mo->mom[VZ] = 0;
    }

    if(!Smoother_Evaluate(smoother, xyz))
    {
        // The smoother has no coordinates for us, so we won't touch the mobj.
        return;
    }

    if(IS_SERVER)
    {
        // On the server, the move must trigger all the usual player movement side-effects
        // (e.g., teleporting).

        if(P_TryMoveXYZ(mo, xyz[VX], xyz[VY], xyz[VZ]))
        {
            if(INRANGE_OF(mo->origin[VX], xyz[VX], .001f) &&
               INRANGE_OF(mo->origin[VY], xyz[VY], .001f))
            {
                if(Smoother_IsOnFloor(smoother))
                {
                    // It successfully moved to the right XY coords.
                    mo->origin[VZ] = mo->floorZ;

                    App_Log(DE2_DEV_MAP_XVERBOSE,
                            "Player %i: Smooth move to %f, %f, %f (floorz)",
                            plrNum, mo->origin[VX], mo->origin[VY], mo->origin[VZ]);
                }
                else
                {
                    App_Log(DE2_DEV_MAP_XVERBOSE,
                            "Player %i: Smooth move to %f, %f, %f",
                            plrNum, mo->origin[VX], mo->origin[VY], mo->origin[VZ]);
                }
            }

            if(players[plrNum].plr->flags & DDPF_FIXORIGIN)
            {
                // The player must have teleported.
                App_Log(DE2_DEV_MAP_MSG, "Player %i: Clearing smoother because of FIXPOS", plrNum);

                Smoother_Clear(smoother);
            }
        }
        else
        {
            App_Log(DE2_DEV_MAP_NOTE,
                    "P_PlayerRemoteMove: Player %i: Smooth move to %f, %f, %f FAILED!",
                    plrNum, mo->origin[VX], mo->origin[VY], mo->origin[VZ]);
        }
    }
}

void P_MovePlayer(player_t *player)
{
    ddplayer_t* dp = player->plr;
    mobj_t* plrmo = player->plr->mo;
    playerbrain_t *brain = &player->brain;
    classinfo_t* pClassInfo = PCLASS_INFO(player->class_);
    int speed;
    coord_t forwardMove;
    coord_t sideMove;

    if(!plrmo) return;

    if(IS_NETWORK_SERVER)
    {
        // Server starts the walking animation for remote players.
        if((NON_ZERO(dp->forwardMove) || NON_ZERO(dp->sideMove)) &&
           plrmo->state == &STATES[pClassInfo->normalState])
        {
            P_MobjChangeState(plrmo, pClassInfo->runState);
        }
        else if(P_PlayerInWalkState(player) && IS_ZERO(dp->forwardMove) && IS_ZERO(dp->sideMove))
        {
            // If in a walking frame, stop moving.
            P_MobjChangeState(plrmo, pClassInfo->normalState);
        }
        return;
    }

    // Slow > fast. Fast > slow.
    speed = brain->speed;
    if(cfg.common.alwaysRun)
        speed = !speed;

    // Do not let the player control movement if not onground.
    onground = P_IsPlayerOnGround(player);
    if(dp->flags & DDPF_CAMERA)    // $democam
    {
        static const coord_t cameraSpeed[2] = { FIX2FLT(0x19), FIX2FLT(0x54) };
        int moveMul = 2048;

        // Cameramen have 3D thrusters!
        P_Thrust3D(player, plrmo->angle, dp->lookDir,
                   brain->forwardMove * cameraSpeed[speed] * moveMul,
                   brain->sideMove    * cameraSpeed[speed] * moveMul);
    }
    else
    {
        // 'Move while in air' hack (server doesn't know about this!!).
        // Movement while in air traditionally disabled.
        int const movemul = (onground || (plrmo->flags2 & MF2_FLY))?
                    pClassInfo->moveMul : (cfg.common.airborneMovement? cfg.common.airborneMovement * 64 : 0);

        if(!brain->lunge)
        {
            coord_t const maxMove = FIX2FLT(pClassInfo->maxMove) * turboMul;

            forwardMove = FIX2FLT(pClassInfo->forwardMove[speed]) * turboMul * MINMAX_OF(-1.f, brain->forwardMove, 1.f);
            sideMove    = FIX2FLT(pClassInfo->sideMove[speed])    * turboMul * MINMAX_OF(-1.f, brain->sideMove,    1.f);

            // Players can opt to reduce their maximum possible movement speed.
            if((int) cfg.common.playerMoveSpeed != 1)
            {
                // A divsor has been specified, apply it.
                coord_t m = MINMAX_OF(0.f, cfg.common.playerMoveSpeed, 1.f);
                forwardMove *= m;
                sideMove    *= m;
            }

            // Make sure it's within valid bounds.
            forwardMove = MINMAX_OF(-maxMove, forwardMove, maxMove);
            sideMove    = MINMAX_OF(-maxMove,    sideMove, maxMove);

#if __JHEXEN__
            // The speed power gives an extra boost .
            if(player->powers[PT_SPEED] && !player->morphTics)
            {
                forwardMove = (3 * forwardMove) / 2;
                sideMove    = (3 * sideMove) / 2;
            }
#endif
        }
        else
        {
            /**
             * Do the "lunge" move.
             *
             * @note Normal valid range clamp not used with lunge as with
             * it; the amount of forward velocity is not sufficent to
             * prevent the player from easily backing out while lunging.
             */
            forwardMove = FIX2FLT(0xc800 / 512);
            sideMove = 0;
        }

        if(NON_ZERO(forwardMove) && movemul)
        {
            P_Thrust(player, plrmo->angle, forwardMove * movemul);
        }

        if(NON_ZERO(sideMove) && movemul)
        {
            P_Thrust(player, plrmo->angle - ANG90, sideMove * movemul);
        }

        if((NON_ZERO(forwardMove) || NON_ZERO(sideMove)) &&
           plrmo->state == &STATES[pClassInfo->normalState])
        {
            P_MobjChangeState(plrmo, pClassInfo->runState);
        }

        //P_CheckPlayerJump(player); // done in a different place
    }
#if __JHEXEN__
    // Look up/down using the delta.
    /*  if(cmd->lookdirdelta)
       {
       float fd = cmd->lookdirdelta / DELTAMUL;
       float delta = fd * fd;
       if(cmd->lookdirdelta < 0) delta = -delta;
       player->plr->lookDir += delta;
       } */

    // 110 corresponds 85 degrees.
    dp->lookDir = MINMAX_OF(-LOOKDIRMAX, dp->lookDir, LOOKDIRMAX);
#endif
}

/**
 * Fall on your ass when dying. Decrease viewheight to floor height.
 */
void P_DeathThink(player_t* player)
{
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    angle_t angle;
#endif
    angle_t delta;
    int     lookDelta;

    if(player->rebornWait > 0)
        player->rebornWait--;

    P_MovePsprites(player);

    onground = (player->plr->mo->origin[VZ] <= player->plr->mo->floorZ);
#if __JDOOM__ || __JDOOM64__
    if(cfg.deathLookUp)
#elif __JHERETIC__
    if(player->plr->mo->type == MT_BLOODYSKULL)
#elif __JHEXEN__
    if(player->plr->mo->type == MT_BLOODYSKULL ||
       player->plr->mo->type == MT_ICECHUNK)
#endif
    {   // Flying bloody skull
        player->viewHeight = 6;
        player->viewHeightDelta = 0;

        if(onground)
        {
            if(player->plr->lookDir < 60)
            {
                lookDelta = (60 - player->plr->lookDir) / 8;
                if(lookDelta < 1 && (mapTime & 1))
                {
                    lookDelta = 1;
                }
                else if(lookDelta > 6)
                {
                    lookDelta = 6;
                }
                player->plr->lookDir += lookDelta;
                player->plr->flags |= DDPF_INTERPITCH;
                player->plr->flags |= DDPF_FIXANGLES;
            }
        }
    }
#if __JHEXEN__
    else if(!(player->plr->mo->flags2 & MF2_ICEDAMAGE)) // (if not frozen)
#else
    else // fall to the ground
#endif
    {
        if(player->viewHeight > 6)
            player->viewHeight -= 1;

        if(player->viewHeight < 6)
            player->viewHeight = 6;

        player->viewHeightDelta = 0;

#if __JHERETIC__ || __JHEXEN__
        if(player->plr->lookDir > 0)
            player->plr->lookDir -= 6;
        else if(player->plr->lookDir < 0)
            player->plr->lookDir += 6;

        if(abs((int) player->plr->lookDir) < 6)
            player->plr->lookDir = 0;
#endif
        player->plr->flags |= DDPF_INTERPITCH;
        player->plr->flags |= DDPF_FIXANGLES;
    }

#if __JHEXEN__
    player->update |= PSF_VIEW_HEIGHT;
#endif

    P_CalcHeight(player);

    // Keep track of the killer.
    if(player->attacker && player->attacker != player->plr->mo)
    {
#if __JHEXEN__
        int dir = P_FaceMobj(player->plr->mo, player->attacker, &delta);
        if(delta < ANGLE_1 * 10)
        {   // Looking at killer, so fade damage and poison counters
            if(player->damageCount)
            {
                player->damageCount--;
            }
            if(player->poisonCount)
            {
                player->poisonCount--;
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
        angle = M_PointToAngle2(player->plr->mo->origin, player->attacker->origin);
        delta = angle - player->plr->mo->angle;

        if(delta < ANG5 || delta > (unsigned) -ANG5)
        {
            // Looking at killer, so fade damage flash down.
            player->plr->mo->angle = angle;
            if(player->damageCount)
                player->damageCount--;
        }
        else if(delta < ANG180)
            player->plr->mo->angle += ANG5; // Turn clockwise
        else
            player->plr->mo->angle -= ANG5; // Turn counter clockwise

        player->plr->flags |= DDPF_INTERYAW;
#endif

        // Update client.
        player->plr->flags |= DDPF_FIXANGLES;
    }
    else
    {
        if(player->damageCount)
            player->damageCount--;

#if __JHEXEN__
        if(player->poisonCount)
            player->poisonCount--;
#endif
    }

    if(!(player->rebornWait > 0) && player->brain.doReborn)
    {
        if(IS_CLIENT)
        {
            NetCl_PlayerActionRequest(player, GPA_USE, 0);
        }
        else
        {
            P_PlayerReborn(player);
        }
    }
}

/**
 * Called when a dead player wishes to be reborn.
 *
 * @param player        Player that wishes to be reborn.
 */
void P_PlayerReborn(player_t *player)
{
    int const plrNum = player - players;

    if(plrNum == CONSOLEPLAYER)
    {
        App_Log(DE2_DEV_SCR_MSG, "Reseting Infine due to console player being reborn");

        // Clear the currently playing script, if any.
        FI_StackClear();
    }

    player->playerState = PST_REBORN;
#if __JHERETIC__ || __JHEXEN__
    player->plr->flags &= ~DDPF_VIEW_FILTER;
    newTorch[player - players] = 0;
    newTorchDelta[player - players] = 0;
# if __JHEXEN__
    player->plr->mo->special1 = player->class_;
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

#if __JHERETIC__ || __JHEXEN__
void P_MorphThink(player_t *player)
{
    mobj_t *pmo;

#if __JHEXEN__
    if(player->morphTics & 15)
        return;

    pmo = player->plr->mo;
    if(IS_ZERO(pmo->mom[MX]) && IS_ZERO(pmo->mom[MY]) && P_Random() < 64)
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
        P_UpdateBeak(player, &player->pSprites[ps_weapon]); // Handle beak movement

    if(player->chickenPeck)
    {   // Chicken attack counter.
        player->chickenPeck -= 3;
    }

    if(/*IS_CLIENT || */ player->morphTics & 15)
        return;

    pmo = player->plr->mo;
    if(INRANGE_OF(pmo->mom[MX], 0, NOMOM_THRESHOLD) &&
       INRANGE_OF(pmo->mom[MY], 0, NOMOM_THRESHOLD) && P_Random() < 160)
    {   // Twitch view angle
        pmo->angle += (P_Random() - P_Random()) << 19;
    }

    if(!IS_NETGAME || IS_CLIENT)
    {
        if(IS_ZERO(pmo->mom[MX]) && IS_ZERO(pmo->mom[MY]) && P_Random() < 160)
        {   // Twitch view angle
            pmo->angle += (P_Random() - P_Random()) << 19;
        }
/*    }
    if(!IS_NETGAME || !IS_CLIENT)
    {*/
        if(pmo->origin[VZ] <= pmo->floorZ && (P_Random() < 32))
        {   // Jump and noise
            pmo->mom[MZ] += 1;
            P_MobjChangeState(pmo, S_CHICPLAY_PAIN);
            return;
        }
    }

    if(P_Random() < 48)
    {   // Just noise.
        S_StartSound(SFX_CHICACT, pmo);
    }
# endif
}

dd_bool P_UndoPlayerMorph(player_t* player)
{
    mobj_t* fog = 0, *mo = 0, *pmo = 0;
    coord_t pos[3];
    unsigned int an;
    angle_t angle;
    int playerNum;
    weapontype_t weapon;
    int oldFlags, oldFlags2, oldBeast;

    if(IS_CLIENT) return false;

# if __JHEXEN__
    player->update |= PSF_MORPH_TIME | PSF_POWERS | PSF_HEALTH;
# endif

    pmo = player->plr->mo;
    memcpy(pos, pmo->origin, sizeof(pos));

    angle = pmo->angle;
    weapon = pmo->special1;
    oldFlags = pmo->flags;
    oldFlags2 = pmo->flags2;
# if __JHEXEN__
    oldBeast = pmo->type;
# else
    oldBeast = MT_CHICPLAYER;
# endif
    P_MobjChangeState(pmo, S_FREETARGMOBJ);

    playerNum = P_GetPlayerNum(player);
# if __JHEXEN__
    mo = P_SpawnMobj(PCLASS_INFO(cfg.playerClass[playerNum])->mobjType,
                        pos, angle, 0);
# else
    mo = P_SpawnMobj(MT_PLAYER, pos, angle, 0);
# endif

    if(!mo) return false;

    if(P_TestMobjLocation(mo) == false)
    {
        // Didn't fit
        P_MobjRemove(mo, false);
        if((mo = P_SpawnMobj(oldBeast, pos, angle, 0)))
        {
            mo->health = player->health;
            mo->special1 = weapon;
            mo->player = player;
            mo->dPlayer = player->plr;
            mo->flags = oldFlags;
            mo->flags2 = oldFlags2;
            player->plr->mo = mo;
            player->morphTics = 2 * 35;
        }

        return false;
    }

    if(playerNum != 0)
    {
        // Set color translation bits for player sprites
        mo->flags |= playerNum << MF_TRANSSHIFT; /// @todo Use configured color? -jk
    }

    mo->player = player;
    mo->dPlayer = player->plr;
    mo->reactionTime = 18;

    if(oldFlags2 & MF2_FLY)
    {
        mo->flags2 |= MF2_FLY;
        mo->flags |= MF_NOGRAVITY;
    }

    player->morphTics = 0;
# if __JHERETIC__
    player->powers[PT_WEAPONLEVEL2] = 0;
# endif
    player->health = mo->health = maxHealth;
    player->plr->mo = mo;
# if __JHERETIC__
    player->class_ = PCLASS_PLAYER;
# else
    player->class_ = cfg.playerClass[playerNum];
# endif
    an = angle >> ANGLETOFINESHIFT;

    if((fog = P_SpawnMobjXYZ(MT_TFOG,
                            pos[VX] + 20 * FIX2FLT(finecosine[an]),
                            pos[VY] + 20 * FIX2FLT(finesine[an]),
                            pos[VZ] + TELEFOGHEIGHT, angle + ANG180, 0)))
    {
# if __JHERETIC__
        S_StartSound(SFX_TELEPT, fog);
# else
        S_StartSound(SFX_TELEPORT, fog);
# endif
    }
    P_PostMorphWeapon(player, weapon);

    player->update |= PSF_MORPH_TIME | PSF_HEALTH;
    player->plr->flags |= DDPF_FIXORIGIN | DDPF_FIXMOM;

    return true;
}
#endif

void P_PlayerThinkState(player_t* player)
{
    if(player->plr->mo)
    {
        mobj_t* plrmo = player->plr->mo;

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
            (plrmo->selector & ~DDMOBJ_SELECTOR_MASK) | (player->readyWeapon + 1);

        // Reactiontime is used to prevent movement for a bit after a teleport.
        if(plrmo->reactionTime > 0)
        {
            plrmo->reactionTime--;
        }
        else
        {
            plrmo->reactionTime = 0;
        }
    }

    if(player->playerState != PST_DEAD)
    {
        // Clear the view angle interpolation flags by default.
        player->plr->flags &= ~(DDPF_INTERYAW | DDPF_INTERPITCH);
    }
}

void P_PlayerThinkCheat(player_t *player)
{
    if(player->plr->mo)
    {
        mobj_t             *plrmo = player->plr->mo;

        // fixme: do this in the cheat code
        if(P_GetPlayerCheats(player) & CF_NOCLIP)
            plrmo->flags |= MF_NOCLIP;
        else
            plrmo->flags &= ~MF_NOCLIP;
    }
}

void P_PlayerThinkAttackLunge(player_t *player)
{
    mobj_t* plrmo = player->plr->mo;

    // Normally we don't lunge.
    player->brain.lunge = false;

    if(plrmo && (plrmo->flags & MF_JUSTATTACKED))
    {
        player->brain.lunge = true;
        plrmo->flags &= ~MF_JUSTATTACKED;
        player->plr->flags |= DDPF_FIXANGLES;
    }
}

/**
 * @return              @c true, if thinking should be stopped. Otherwise,
 *                      @c false.
 */
dd_bool P_PlayerThinkDeath(player_t *player)
{
    if(player->playerState == PST_DEAD)
    {
        P_DeathThink(player);
        return true; // stop!
    }
    return false; // don't stop
}

void P_PlayerThinkMorph(player_t *player)
{
#if __JHERETIC__ || __JHEXEN__
    if(player->morphTics)
    {
        P_MorphThink(player);
        if(!--player->morphTics)
        {   // Attempt to undo the pig/chicken.
            P_UndoPlayerMorph(player);
        }
    }
#endif
}

void P_PlayerThinkMove(player_t *player)
{
    mobj_t* plrmo = player->plr->mo;

    // Move around.
    // Reactiontime is used to prevent movement for a bit after a teleport.
    if(plrmo && !plrmo->reactionTime)
    {
        P_MovePlayer(player);

#if __JHEXEN__
        plrmo = player->plr->mo;
        if(player->powers[PT_SPEED] && !(mapTime & 1) &&
           M_ApproxDistance(plrmo->mom[MX], plrmo->mom[MY]) > 12)
        {
            mobj_t*             speedMo;
            int                 playerNum;

            if((speedMo = P_SpawnMobj(MT_PLAYER_SPEED, plrmo->origin,
                                         plrmo->angle, 0)))
            {
                playerNum = P_GetPlayerNum(player);

                if(playerNum)
                {
                    // Set color translation bits for player sprites.
                    speedMo->flags |= playerNum << MF_TRANSSHIFT; /// @todo Use configured color? -jk
                }

                speedMo->target = plrmo;
                speedMo->special1 = player->class_;
                if(speedMo->special1 > 2)
                {
                    speedMo->special1 = 0;
                }

                speedMo->sprite = plrmo->sprite;
                speedMo->floorClip = plrmo->floorClip;
                if(player == &players[CONSOLEPLAYER])
                {
                    speedMo->flags2 |= MF2_DONTDRAW;
                }
            }
        }
#endif
    }
}

void P_PlayerThinkFly(player_t *player)
{
    mobj_t             *plrmo = player->plr->mo;

    // Reactiontime is used to prevent movement for a bit after a teleport.
    if(!plrmo || plrmo->reactionTime)
        return;

    // Is flying allowed?
    if(player->plr->flags & DDPF_CAMERA)
        return;

    if(player->brain.fallDown)
    {
        plrmo->flags2 &= ~MF2_FLY;
        plrmo->flags &= ~MF_NOGRAVITY;
    }
    else if(NON_ZERO(player->brain.upMove) && player->powers[PT_FLIGHT])
    {
        player->flyHeight = player->brain.upMove * 10;
        if(!(plrmo->flags2 & MF2_FLY))
        {
            plrmo->flags2 |= MF2_FLY;
            plrmo->flags |= MF_NOGRAVITY;
#if __JHEXEN__
            if(plrmo->mom[MZ] <= -39)
            {   // Stop falling scream.
                S_StopSound(0, plrmo);
            }
#endif
        }
    }

    // Apply Z momentum based on flight speed.
    if(plrmo->flags2 & MF2_FLY)
    {
        plrmo->mom[MZ] = (coord_t) player->flyHeight;
        if(player->flyHeight)
        {
            player->flyHeight /= 2;
        }
    }
}

void P_PlayerThinkJump(player_t *player)
{
    if(!player->plr->mo || player->plr->mo->reactionTime)
        return; // Not yet.

    // Jumping.
    if(player->jumpTics)
        player->jumpTics--;

    P_CheckPlayerJump(player);
}

void P_PlayerThinkView(player_t *player)
{
    if(player->plr->mo)
    {
        P_CalcHeight(player);
    }
}


void P_PlayerThinkSpecial(player_t *player)
{
    if(!player->plr->mo) return;

    if(P_ToXSector(Mobj_Sector(player->plr->mo))->special)
        P_PlayerInSpecialSector(player);

#if __JHEXEN__
    P_PlayerOnSpecialFloor(player);
#endif
}

#if __JHERETIC__ || __JHEXEN__
/**
 * For inventory management, could be done client-side.
 */
void P_PlayerThinkInventory(player_t *player)
{
    int pnum = player - players;

    if(player->brain.cycleInvItem)
    {
        if(!Hu_InventoryIsOpen(pnum))
        {
            Hu_InventoryOpen(pnum, true);
            return;
        }

        Hu_InventoryMove(pnum, player->brain.cycleInvItem,
                         cfg.inventoryWrap, false);
    }
}
#endif

void P_PlayerThinkSounds(player_t* player)
{
#if __JHEXEN__
    mobj_t* plrmo = player->plr->mo;

    if(!plrmo) return;

    switch(player->class_)
    {
        case PCLASS_FIGHTER:
            if(plrmo->mom[MZ] <= -35 &&
               plrmo->mom[MZ] >= -40 && !player->morphTics &&
               !S_IsPlaying(SFX_PLAYER_FIGHTER_FALLING_SCREAM, plrmo))
            {
                S_StartSound(SFX_PLAYER_FIGHTER_FALLING_SCREAM, plrmo);
            }
            break;

        case PCLASS_CLERIC:
            if(plrmo->mom[MZ] <= -35 &&
               plrmo->mom[MZ] >= -40 && !player->morphTics &&
               !S_IsPlaying(SFX_PLAYER_CLERIC_FALLING_SCREAM, plrmo))
            {
                S_StartSound(SFX_PLAYER_CLERIC_FALLING_SCREAM, plrmo);
            }
            break;

        case PCLASS_MAGE:
            if(plrmo->mom[MZ] <= -35 &&
               plrmo->mom[MZ] >= -40 && !player->morphTics &&
               !S_IsPlaying(SFX_PLAYER_MAGE_FALLING_SCREAM, plrmo))
            {
                S_StartSound(SFX_PLAYER_MAGE_FALLING_SCREAM, plrmo);
            }
            break;

        default:
            break;
    }
#endif
}

void P_PlayerThinkItems(player_t *player)
{
#if __JHERETIC__ || __JHEXEN__
    inventoryitemtype_t i, type = IIT_NONE; // What to use?
    int pnum = player - players;

    if(player->brain.useInvItem)
    {
        type = P_InventoryReadyItem(pnum);
    }

    // Inventory item hot keys.
    for(i = IIT_FIRST; i < NUM_INVENTORYITEM_TYPES; ++i)
    {
        def_invitem_t const *def = P_GetInvItemDef(i);

        if(def->hotKeyCtrlIdent != -1 &&
           P_GetImpulseControlState(pnum, def->hotKeyCtrlIdent))
        {
            type = i;
            break;
        }
    }

    // Panic?
    if(type == IIT_NONE && P_GetImpulseControlState(pnum, CTL_PANIC))
        type = NUM_INVENTORYITEM_TYPES;

    if(type != IIT_NONE)
    {   // Use one (or more) inventory items.
        P_InventoryUse(pnum, type, false);
    }
#endif

#if __JHERETIC__ || __JHEXEN__
    if(player->brain.upMove > 0 && !player->powers[PT_FLIGHT])
    {
        // Start flying automatically, if Wings are available.
        if(P_InventoryCount(pnum, IIT_FLY))
        {
            P_InventoryUse(pnum, IIT_FLY, false);
        }
    }
#endif
}

void P_PlayerThinkWeapons(player_t *player)
{
    playerbrain_t *brain = &player->brain;
    //weapontype_t oldweapon = player->pendingWeapon;
    weapontype_t newweapon = WT_NOCHANGE;

    if(IS_NETWORK_SERVER)
    {
        if(brain->changeWeapon != WT_NOCHANGE)
        {
            // Weapon change logic has already been done by the client.
            newweapon = brain->changeWeapon;

            if(!player->weapons[newweapon].owned)
            {
                App_Log(DE2_MAP_WARNING, "Player %i tried to change to unowned weapon %i!",
                        (int)(player - players), newweapon);
                newweapon = WT_NOCHANGE;
            }
        }
    }
    else
    // Check for weapon change.
#if defined(__JHERETIC__) || defined(__JHEXEN__)
    if (brain->changeWeapon != WT_NOCHANGE && !player->morphTics)
#else
    if (brain->changeWeapon != WT_NOCHANGE)
#endif
    {
        // Direct slot selection.
        weapontype_t cand, first;

        // Is this a same-slot weapon cycle?
        if(P_GetWeaponSlot(brain->changeWeapon) ==
           P_GetWeaponSlot(player->readyWeapon))
        {   // Yes.
            cand = player->readyWeapon;
        }
        else
        {   // No.
            cand = brain->changeWeapon;
        }

        first = cand = P_WeaponSlotCycle(cand, brain->cycleWeapon < 0);

        do
        {
            if(player->weapons[cand].owned)
                newweapon = cand;
        } while(newweapon == WT_NOCHANGE &&
               (cand = P_WeaponSlotCycle(cand, brain->cycleWeapon < 0)) !=
                first);
    }
#if defined(__JHERETIC__) || defined(__JHEXEN__)
    else if (brain->cycleWeapon && player->morphTics == 0)
#else
    else if (brain->cycleWeapon)
#endif
    {
        // Linear cycle.
        newweapon = P_PlayerFindWeapon(player, brain->cycleWeapon < 0);
    }

    if(newweapon != WT_NOCHANGE && newweapon != player->readyWeapon)
    {
        if(weaponInfo[newweapon][player->class_].mode[0].gameModeBits & gameModeBits)
        {
            if(IS_CLIENT)
            {
                // Send a notification to the server.
                NetCl_PlayerActionRequest(player, GPA_CHANGE_WEAPON, newweapon);
            }
            App_Log(DE2_DEV_MAP_VERBOSE, "Player %i changing weapon to %i (brain thinks %i)",
                    (int)(player - players), newweapon, brain->changeWeapon);

            player->pendingWeapon = newweapon;
            brain->changeWeapon = WT_NOCHANGE;
        }
    }

    /*
    if(player->pendingWeapon != oldweapon)
    {
#if __JDOOM__ || __JDOOM64__
        player->update |= PSF_PENDING_WEAPON | PSF_READY_WEAPON;
#elif __JHEXEN__
        player->update |= PSF_PENDING_WEAPON;
#endif
    }
    */
}

void P_PlayerThinkUse(player_t *player)
{
    if(IS_NETWORK_SERVER && player != &players[CONSOLEPLAYER])
    {
        // Clients send use requests instead.
        return;
    }

    // Check for use.
    if(player->brain.use)
    {
        if(!player->useDown)
        {
            P_UseLines(player);
            player->useDown = true;
        }
    }
    else
    {
        player->useDown = false;
    }
}

void P_PlayerThinkPsprites(player_t *player)
{
    // Cycle psprites.
    P_MovePsprites(player);
}

void P_PlayerThinkHUD(player_t* player)
{
    uint playerIdx = player - players;
    playerbrain_t* brain = &player->brain;

    if(brain->hudShow)
        ST_HUDUnHide(playerIdx, HUE_FORCE);

    if(brain->scoreShow)
        HU_ScoreBoardUnHide(playerIdx);

    if(brain->logRefresh)
        ST_LogRefresh(playerIdx);
}

void P_PlayerThinkMap(player_t *player)
{
    uint           playerIdx = player - players;
    playerbrain_t *brain     = &player->brain;

    if (brain->mapToggle)
        ST_AutomapOpen(playerIdx, !ST_AutomapIsOpen(playerIdx), false);

    if (brain->mapFollow)
        ST_AutomapFollowMode(playerIdx);

    if (brain->mapRotate)
        G_SetAutomapRotateMode(!cfg.common.automapRotate); // changes cvar as well

    if (brain->mapZoomMax)
        ST_AutomapZoomMode(playerIdx);

    if (brain->mapMarkAdd)
    {
        mobj_t *pmo = player->plr->mo;
        ST_AutomapAddPoint(playerIdx, pmo->origin[VX], pmo->origin[VY], pmo->origin[VZ]);
    }

    if(brain->mapMarkClearAll)
        ST_AutomapClearPoints(playerIdx);
}

void P_PlayerThinkPowers(player_t* player)
{
    // Counters, time dependend power ups.

#if __JDOOM__ || __JDOOM64__
    // Strength counts up to diminish fade.
    if(player->powers[PT_STRENGTH])
        player->powers[PT_STRENGTH]++;

    if(player->powers[PT_IRONFEET])
        player->powers[PT_IRONFEET]--;
#endif

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    if(player->powers[PT_INVULNERABILITY])
        player->powers[PT_INVULNERABILITY]--;

    if(player->powers[PT_INVISIBILITY])
    {
        if(!--player->powers[PT_INVISIBILITY])
            player->plr->mo->flags &= ~MF_SHADOW;
    }
#endif

    // Infrared/Torch times out eventually.
    if(player->powers[PT_INFRARED])
        player->powers[PT_INFRARED]--;

    if(player->damageCount)
        player->damageCount--;

    if(player->bonusCount)
        player->bonusCount--;

#if __JHERETIC__ || __JHEXEN__
# if __JHERETIC__
    if(player->powers[PT_FLIGHT])
# elif __JHEXEN__
    if(player->powers[PT_FLIGHT] && IS_NETGAME)
# endif
    {
        if(!--player->powers[PT_FLIGHT])
        {
            if(player->plr->mo->origin[VZ] != player->plr->mo->floorZ && cfg.common.lookSpring)
            {
                player->centering = true;
            }

            player->plr->mo->flags2 &= ~MF2_FLY;
            player->plr->mo->flags &= ~MF_NOGRAVITY;
        }
    }
#endif

#if __JHERETIC__
    if(player->powers[PT_WEAPONLEVEL2])
    {
        if(!--player->powers[PT_WEAPONLEVEL2])
        {
            if((player->readyWeapon == WT_SIXTH) &&
               (player->pSprites[ps_weapon].state != &STATES[S_PHOENIXREADY])
               && (player->pSprites[ps_weapon].state != &STATES[S_PHOENIXUP]))
            {
                P_SetPsprite(player, ps_weapon, S_PHOENIXREADY);
                player->ammo[AT_FIREORB].owned = MAX_OF(0,
                    player->ammo[AT_FIREORB].owned - USE_PHRD_AMMO_2);
                player->refire = 0;
                player->update |= PSF_AMMO;
            }
            else if((player->readyWeapon == WT_EIGHTH) ||
                    (player->readyWeapon == WT_FIRST))
            {
                player->pendingWeapon = player->readyWeapon;
                player->update |= PSF_PENDING_WEAPON;
            }
        }
    }
#endif

    // Colormaps
#if __JHERETIC__ || __JHEXEN__
    if(!IS_CLIENT)
    {
        if(player->powers[PT_INFRARED])
        {
            if(player->powers[PT_INFRARED] <= BLINKTHRESHOLD)
            {
                if(player->powers[PT_INFRARED] & 8)
                {
                    player->plr->fixedColorMap = 0;
                }
                else
                {
                    player->plr->fixedColorMap = 1;
                }
            }
            else if(!(mapTime & 16))  /* && player == &players[CONSOLEPLAYER]) */
            {
                ddplayer_t *dp = player->plr;
                int     playerNumber = player - players;

                if(newTorch[playerNumber])
                {
                    if(dp->fixedColorMap + newTorchDelta[playerNumber] > 7 ||
                       dp->fixedColorMap + newTorchDelta[playerNumber] < 1 ||
                       newTorch[playerNumber] == dp->fixedColorMap)
                    {
                        newTorch[playerNumber] = 0;
                    }
                    else
                    {
                        dp->fixedColorMap += newTorchDelta[playerNumber];
                    }
                }
                else
                {
                    newTorch[playerNumber] = (M_Random() & 7) + 1;
                    newTorchDelta[playerNumber] =
                        (newTorch[playerNumber] ==
                         dp->fixedColorMap) ? 0 : ((newTorch[playerNumber] >
                                                    dp->fixedColorMap) ? 1 : -1);
                }
            }
        }
        else
        {
            player->plr->fixedColorMap = 0;
        }
    }
#endif

#if __JHEXEN__
    if(player->powers[PT_INVULNERABILITY])
    {
        if(player->class_ == PCLASS_CLERIC)
        {
            if(!(mapTime & 7) && (player->plr->mo->flags & MF_SHADOW) &&
               !(player->plr->mo->flags2 & MF2_DONTDRAW))
            {
                player->plr->mo->flags &= ~MF_SHADOW;
                if(!(player->plr->mo->flags & MF_ALTSHADOW))
                {
                    player->plr->mo->flags2 |= MF2_DONTDRAW | MF2_NONSHOOTABLE;
                }
            }
            if(!(mapTime & 31))
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
        if(!(--player->powers[PT_INVULNERABILITY]))
        {
            player->plr->mo->flags2 &= ~(MF2_INVULNERABLE | MF2_REFLECTIVE);
            if(player->class_ == PCLASS_CLERIC)
            {
                player->plr->mo->flags2 &= ~(MF2_DONTDRAW | MF2_NONSHOOTABLE);
                player->plr->mo->flags &= ~(MF_SHADOW | MF_ALTSHADOW);
            }
        }
    }
    if(player->powers[PT_MINOTAUR])
    {
        player->powers[PT_MINOTAUR]--;
    }

    if(player->powers[PT_SPEED])
    {
        player->powers[PT_SPEED]--;
    }

    if(player->poisonCount && !(mapTime & 15))
    {
        player->poisonCount -= 5;
        if(player->poisonCount < 0)
        {
            player->poisonCount = 0;
        }
        P_PoisonDamage(player, player->poisoner, 1, true);
    }
#endif // __JHEXEN__
}

/**
 * Handles the updating of the player's yaw view angle depending on the game
 * input controllers. Control states are queried from the engine. Note
 * that this is done once per sharp tic so that behavior conforms to the
 * original engine.
 *
 * @param player        Player doing the thinking.
 * @param ticLength     Time to think, in seconds. Use as a multiplier.
 *                      Note that original game logic was always using a
 *                      tick duration of 1/35 seconds.
 */
void P_PlayerThinkLookYaw(player_t* player, timespan_t ticLength)
{
    int playerNum = player - players;
    ddplayer_t* plr = player->plr;
    classinfo_t* pClassInfo = PCLASS_INFO(player->class_);
    float offsetSensitivity = 100; /// @todo Should be done engine-side, mouse sensitivity!
    float vel, off, turnSpeedPerTic, yawDelta;

    static float yaws[DDMAXPLAYERS]; /// @todo Move to a more appropriate, resetable place.

    if(IS_DEDICATED) return;

    if(!plr->mo || player->playerState == PST_DEAD || player->viewLock)
        return;

    if(IS_CLIENT && playerNum != CONSOLEPLAYER)
    {
        // This is only for the local player.
        return;
    }

    // Turn the head?
    P_PlayerThinkHeadTurning(playerNum, ticLength);

    turnSpeedPerTic = pClassInfo->turnSpeed[0];

    // Check for extra speed.
    P_GetControlState(playerNum, CTL_SPEED, &vel, NULL);
    if(NON_ZERO(vel) ^ (cfg.common.alwaysRun != 0))
    {
        // Hurry, good man!
        turnSpeedPerTic = pClassInfo->turnSpeed[1];
    }

    // Check the yaw offset control (absolute value).
    /// @todo This could be done better?
    P_GetControlState(playerNum, CTL_BODY_YAW, &off, 0);
    yawDelta = off - yaws[playerNum];
    yaws[playerNum] = off;
    plr->appliedBodyYaw = (fixed_t)(off * ANGLE_180);
    plr->mo->angle += (fixed_t)(yawDelta * ANGLE_180);

    // Yaw.
    if(!((plr->mo->flags & MF_JUSTATTACKED) || player->brain.lunge))
    {
        P_GetControlState(playerNum, CTL_TURN, &vel, &off);
        plr->mo->angle -= FLT2FIX(turnSpeedPerTic * vel * ticLength * TICRATE) +
            (fixed_t)(offsetSensitivity * off / 180 * ANGLE_180);
    }
}

/**
 * Handles the updating of the player's view pitch angle depending on the game
 * input controllers. Control states are queried from the engine. Note
 * that this is done as often as possible (i.e., on every frame) so that
 * changes will be smooth and lag-free.
 *
 * @param player        Player doing the thinking.
 * @param ticLength     Time to think, in seconds. Use as a multiplier.
 *                      Note that original game logic was always using a
 *                      tick duration of 1/35 seconds.
 */
void P_PlayerThinkLookPitch(player_t* player, timespan_t ticLength)
{
    int playerNum = player - players;
    ddplayer_t* plr = player->plr;
    float vel, off;
    float offsetSensitivity = 100; /// @todo Should be done engine-side, mouse sensitivity!

    if(IS_DEDICATED) return;

    if(!plr->mo || player->playerState == PST_DEAD || player->viewLock)
        return; // Nothing to control.

    if(IS_CLIENT && playerNum != CONSOLEPLAYER)
    {
        // This is only for the local player.
        return;
    }

    // The absolute look pitch overrides CTL_LOOK.
    if(P_IsControlBound(playerNum, CTL_LOOK_PITCH))
    {
        P_GetControlState(playerNum, CTL_LOOK_PITCH, &off, NULL);
        plr->lookDir = off * LOOKDIRMAX;
    }
    else
    {
        // Look center requested?
        if(P_GetImpulseControlState(playerNum, CTL_LOOK_CENTER))
            player->centering = true;

        // Use a delta-based control for looking.
        P_GetControlState(playerNum, CTL_LOOK, &vel, &off);
        if(player->centering)
        {
            // Automatic vertical look centering.
            float step = 8 * ticLength * TICRATE;

            if(plr->lookDir > step)
            {
                plr->lookDir -= step;
            }
            else if(plr->lookDir < -step)
            {
                plr->lookDir += step;
            }
            else
            {
                plr->lookDir = 0;
                player->centering = false;
            }
        }
        else
        {
            // Pitch as controlled by CTL_LOOK.
            plr->lookDir += LOOKDIRMAX/85.f * ((640 * TICRATE)/65535.f*360 * vel * ticLength +
                                               offsetSensitivity * off);
        }
    }

    plr->lookDir = MINMAX_OF(-LOOKDIRMAX, plr->lookDir, LOOKDIRMAX);
}

void P_PlayerThinkUpdateControls(player_t *player)
{
    int playerNum = player - players;
    ddplayer_t *dp = player->plr;
    float vel, off, offsetSensitivity = 100;
    int i;
    //dd_bool strafe = false;
    playerbrain_t *brain = &player->brain;
    dd_bool oldAttack = brain->attack;

    if(IS_DEDICATED) return;

    // Check for speed.
    P_GetControlState(playerNum, CTL_SPEED, &vel, 0);
    brain->speed = (NON_ZERO(vel));

    // Check for strafe.
    //P_GetControlState(playerNum, CTL_MODIFIER_1, &vel, 0);
    //strafe = (NON_ZERO(vel));

    // Move status.
    P_GetControlState(playerNum, CTL_WALK, &vel, &off);
    brain->forwardMove = off * offsetSensitivity + vel;
    P_GetControlState(playerNum, CTL_SIDESTEP, &vel, &off);
    brain->sideMove = off * offsetSensitivity + vel;

    // Clamp.
    brain->forwardMove = MINMAX_OF(-1.f, brain->forwardMove, 1.f);
    brain->sideMove    = MINMAX_OF(-1.f, brain->sideMove,    1.f);

    // Let the engine know these.
    dp->forwardMove = brain->forwardMove;
    dp->sideMove = brain->sideMove;

    // Flight.
    P_GetControlState(playerNum, CTL_ZFLY, &vel, &off);
    brain->upMove = off + vel;
    if(P_GetImpulseControlState(playerNum, CTL_FALL_DOWN))
    {
        brain->fallDown = true;
    }
    else
    {
        brain->fallDown = false;
    }

    // Check for look centering based on lookSpring.
    if(cfg.common.lookSpring &&
       (fabs(brain->forwardMove) > .333f || fabs(brain->sideMove) > .333f))
    {
        // Center view when mlook released w/lookspring, or when moving.
        player->centering = true;
    }

    // Jump.
    brain->jump = (P_GetImpulseControlState(playerNum, CTL_JUMP) != 0);

    // Use.
    brain->use = (P_GetImpulseControlState(playerNum, CTL_USE) != 0);

    // Fire.
    P_GetControlState(playerNum, CTL_ATTACK, &vel, &off);
    brain->attack = (vel + off != 0);

    // Once dead, the intended action for a given control state change,
    // changes. Here we interpret Use and Fire as "I wish to be Reborn".
    brain->doReborn = false;
    if(player->playerState == PST_DEAD)
    {
        if(brain->use || (brain->attack && !oldAttack))
            brain->doReborn = true;
    }

    // Weapon cycling.
    if(P_GetImpulseControlState(playerNum, CTL_NEXT_WEAPON))
    {
        brain->cycleWeapon = +1;
    }
    else if(P_GetImpulseControlState(playerNum, CTL_PREV_WEAPON))
    {
        brain->cycleWeapon = -1;
    }
    else
    {
        brain->cycleWeapon = 0;
    }

    // Weapons.
    brain->changeWeapon = WT_NOCHANGE;
    for(i = 0; i < NUM_WEAPON_TYPES && (CTL_WEAPON1 + i <= CTL_WEAPON0); i++)
        if(P_GetImpulseControlState(playerNum, CTL_WEAPON1 + i))
        {
            brain->changeWeapon = i;
            brain->cycleWeapon = +1; // Direction for same-slot cycle.
#if __JDOOM__ || __JDOOM64__
            if(i == WT_EIGHTH || i == WT_NINETH)
                brain->cycleWeapon = -1;
#elif __JHERETIC__
            if(i == WT_EIGHTH)
                brain->cycleWeapon = -1;
#endif
        }

#if __JHERETIC__ || __JHEXEN__
    // Inventory items.
    brain->useInvItem = false;
    if(P_GetImpulseControlState(playerNum, CTL_USE_ITEM))
    {
        // If the inventory is visible, close it (depending on cfg.chooseAndUse).
        if(Hu_InventoryIsOpen(player - players))
        {
            Hu_InventoryOpen(player - players, false); // close the inventory

            if(cfg.inventoryUseImmediate)
                brain->useInvItem = true;
        }
        else
        {
            brain->useInvItem = true;
        }
    }

    if(P_GetImpulseControlState(playerNum, CTL_NEXT_ITEM))
    {
        brain->cycleInvItem = +1;
    }
    else if(P_GetImpulseControlState(playerNum, CTL_PREV_ITEM))
    {
        brain->cycleInvItem = -1;
    }
    else
    {
        brain->cycleInvItem = 0;
    }
#endif

    // HUD.
    brain->hudShow = (P_GetImpulseControlState(playerNum, CTL_HUD_SHOW) != 0);
#if __JHERETIC__ || __JHEXEN__
    // Also unhide the HUD when cycling inventory items.
    if(brain->cycleInvItem != 0)
        brain->hudShow = true;
#endif
    brain->scoreShow = (P_GetImpulseControlState(playerNum, CTL_SCORE_SHOW) != 0);
    brain->logRefresh = (P_GetImpulseControlState(playerNum, CTL_LOG_REFRESH) != 0);

    // Automap.
    brain->mapToggle = (P_GetImpulseControlState(playerNum, CTL_MAP) != 0);
    brain->mapZoomMax = (P_GetImpulseControlState(playerNum, CTL_MAP_ZOOM_MAX) != 0);
    brain->mapFollow = (P_GetImpulseControlState(playerNum, CTL_MAP_FOLLOW) != 0);
    brain->mapRotate = (P_GetImpulseControlState(playerNum, CTL_MAP_ROTATE) != 0);
    brain->mapMarkAdd = (P_GetImpulseControlState(playerNum, CTL_MAP_MARK_ADD) != 0);
    brain->mapMarkClearAll = (P_GetImpulseControlState(playerNum, CTL_MAP_MARK_CLEAR_ALL) != 0);
}

/**
 * Verify that the player state is valid. This is a debugging utility and
 * only gets called when _DEBUG is defined.
 */
void P_PlayerThinkAssertions(player_t* player)
{
    int plrNum = player - players;
    mobj_t* mo = player->plr->mo;
    if(!mo) return;

    if(IS_CLIENT)
    {
        // Let's do some checks about the state of a client player.
        if(player->playerState == PST_LIVE)
        {
            if(!(mo->ddFlags & DDMF_SOLID))
            {
                App_Log(DE2_DEV_MAP_NOTE, "P_PlayerThinkAssertions: player %i, mobj should be solid when alive!", plrNum);
            }
        }
        else if(player->playerState == PST_DEAD)
        {
            if(mo->ddFlags & DDMF_SOLID)
            {
                App_Log(DE2_DEV_MAP_NOTE, "P_PlayerThinkAssertions: player %i, mobj should not be solid when dead!", plrNum);
            }
        }
    }
}

/**
 * Main thinker function for players. Handles both single player and
 * multiplayer games, as well as all the different types of players
 * (normal/camera).
 *
 * Functionality is divided to various other functions whose name begins
 * with "P_PlayerThink".
 *
 * @param player        Player that is doing the thinking.
 * @param ticLength     How much time has passed in the game world, in
 *                      seconds. For instance, to be used as a multiplier
 *                      on turning.
 */
void P_PlayerThink(player_t *player, timespan_t ticLength)
{
    int useSharpInput = G_UsingSharpInput();

    if(Pause_IsPaused())
        return;

    if(G_GameState() != GS_MAP)
    {
        if(DD_IsSharpTick())
        {
            // Just check the controls in case some UI stuff is relying on them
            // (like intermission).
            P_PlayerThinkUpdateControls(player);
        }
        return;
    }

#ifdef _DEBUG
    P_PlayerThinkAssertions(player);
#endif

    P_PlayerThinkState(player);

    P_PlayerRemoteMove(player);

    if(!useSharpInput)
    {
        // Adjust turn angles and look direction. This is done in fractional time.
        P_PlayerThinkLookPitch(player, ticLength);
        P_PlayerThinkLookYaw(player, ticLength);
    }

    if(!DD_IsSharpTick())
    {
        // The rest of this function occurs only during sharp ticks.
        return;
    }

#if __JHEXEN__
    player->worldTimer++;
#endif

    if(useSharpInput)
    {
        // Adjust turn angles and look direction. This is done in sharp time.
        P_PlayerThinkLookPitch(player, 1.0/35.0);
        P_PlayerThinkLookYaw(player, 1.0/35.0);
    }

    P_PlayerThinkUpdateControls(player);
    P_PlayerThinkCamera(player); // $democam

    if(!IS_CLIENT) // Only singleplayer.
    {
        P_PlayerThinkCheat(player);
    }

    P_PlayerThinkHUD(player);

    if(P_PlayerThinkDeath(player))
        return; // I'm dead!

    P_PlayerThinkMorph(player);
    P_PlayerThinkAttackLunge(player);
    P_PlayerThinkMove(player);
    P_PlayerThinkFly(player);
    P_PlayerThinkJump(player);
    P_PlayerThinkView(player);
    P_PlayerThinkSpecial(player);

    if(!IS_NETWORK_SERVER) // Locally only (client or singleplayer).
    {
        P_PlayerThinkSounds(player);
    }

#if __JHERETIC__ || __JHEXEN__
    P_PlayerThinkInventory(player);
    P_PlayerThinkItems(player);
#endif

    P_PlayerThinkUse(player);
    P_PlayerThinkWeapons(player);
    P_PlayerThinkPsprites(player);
    P_PlayerThinkPowers(player);
    P_PlayerThinkMap(player);
}
