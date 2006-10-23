/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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

/*
 * p_player.c: Common playsim routines relating to players.
 *
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>

#include "dd_share.h"

#if  __DOOM64TC__
#  include "doom64tc.h"
#elif __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "d_netsv.h"
#include "d_net.h"
#include "hu_msg.h"
#include "p_map.h"

// MACROS ------------------------------------------------------------------

#define MESSAGETICS (4*35)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * NOTE: Why not simply (player - players)?
 *
 * @param player        The player to work with.
 *
 * @return              Number of the given player.
 */
int P_GetPlayerNum(player_t *player)
{
    int     i;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(player == &players[i])
        {
            return i;
        }
    }
    return 0;
}

/**
 * Return a bit field for the current player's cheats.
 *
 * @param               The player to work with.
 *
 * @return              Cheats active for the given player in a bitfield.
 */
int P_GetPlayerCheats(player_t *player)
{
    if(!player)
    {
        return 0;
    }
    else
    {
        if(player->plr->flags & DDPF_CAMERA)
            return (player->cheats |
                    (CF_GODMODE | cfg.cameraNoClip? CF_NOCLIP : 0));
        else
            return player->cheats;
    }
}

/**
 * Subtract the appropriate amount of ammo from the player for firing
 * the current ready weapon.
 *
 * @param player        The player doing the firing.
 */
void P_ShotAmmo(player_t *player)
{
    ammotype_t  i;
    int         lvl;
    weaponinfo_t *win = &weaponinfo[player->readyweapon][player->class];

#if __JHERETIC__
    lvl = (player->powers[pw_weaponlevel2]? 1 : 0);
#else
    lvl = 0;
#endif

    for(i = 0; i < NUMAMMO; ++i)
    {
        if(!win->mode[lvl].ammotype[i])
            continue;   // Weapon does not take this ammo

#if __JHERETIC__
        if(deathmatch && lvl == 1)
            // In deathmatch always use level zero ammo requirements
            player->ammo[i] -= win->mode[0].pershot[i];
        else
#endif
            player->ammo[i] -= win->mode[lvl].pershot[i];

        // Don't let it fall below zero.
        if(player->ammo[i] < 0)
            player->ammo[i] = 0;
    }
}

/**
 * Decides if an automatic weapon change should occur and does it.
 *
 * Called when:
 * A) the player has ran out of ammo for the readied weapon.
 * B) the player has been given a NEW weapon.
 * C) the player is ABOUT TO be given some ammo.
 *
 * If "weapon" is non-zero then we'll always try to change weapon.
 * If "ammo" is non-zero then we'll consider the ammo level of weapons that
 * use this ammo type.
 * If both non-zero - no more ammo for the current weapon.
 *
 * TODO: (C) Should be called AFTER ammo is given but we need to
 *       remember the old count before the change.
 *
 * @param player            The player given the weapon.
 * @param weapon            The weapon given to the player (if any).
 * @param ammo              The ammo given to the player (if any).
 * @param force             (TRUE) if we should force a weapon change.
 *
 * @return weapontype_t     The weapon we changed to OR WP_NOCHANGE.
 */
weapontype_t P_MaybeChangeWeapon(player_t *player, weapontype_t weapon,
                                 ammotype_t ammo, boolean force)
{
    int i, lvl, pclass;
    ammotype_t ammotype;
    weapontype_t candidate;
    weapontype_t returnval = WP_NOCHANGE;
    weaponinfo_t *winf;
    boolean     found;

    // Assume weapon power level zero.
    lvl = 0;

    pclass = player->class;

#if __JHERETIC__
    if(player->powers[pw_weaponlevel2])
        lvl = 1;
#endif

    if(weapon == WP_NOCHANGE && ammo == AM_NOAMMO) // Out of ammo.
    {
        boolean good;

        // Note we have no auto-logical choice for a forced change.
        // Preferences are set by the user.
        found = false;
        for(i=0; i < NUMWEAPONS && !found; ++i)
        {
            candidate = cfg.weaponOrder[i];
            winf = &weaponinfo[candidate][pclass];

            // Is candidate available in this game mode?
            if(!(winf->mode[lvl].gamemodebits & gamemodebits))
                continue;

            // Does the player actually own this candidate?
            if(!player->weaponowned[candidate])
                continue;

            // Is there sufficent ammo for the candidate weapon?
            // Check amount for each used ammo type.
            good = true;
            for(ammotype = 0; ammotype < NUMAMMO && good; ++ammotype)
            {
                if(!winf->mode[lvl].ammotype[ammotype])
                    continue;   // Weapon does not take this type of ammo.
#if __JHERETIC__
                // Heretic always uses lvl 0 ammo requirements in deathmatch
                if(deathmatch &&
                   player->ammo[ammotype] < winf->mode[0].pershot[ammotype])
                {   // Not enough ammo of this type. Candidate is NOT good.
                    good = false;
                }
                else
#endif
                if(player->ammo[ammotype] < winf->mode[lvl].pershot[ammotype])
                {   // Not enough ammo of this type. Candidate is NOT good.
                    good = false;
                }
            }
            if(good)
            {   // Candidate weapon meets the criteria.
                returnval = candidate;
                found = true;
            }
        }
    }
    else if(weapon != WP_NOCHANGE) // Player was given a NEW weapon.
    {
        // Should we change weapon automatically?
        if(cfg.weaponAutoSwitch == 2 || force)
        {   // Always change weapon mode
            returnval = weapon;
        }
        else if(cfg.weaponAutoSwitch == 1)
        {   // Change if better mode

            // Iterate the weapon order array and see if a weapon change
            // should be made. Preferences are user selectable.
            for(i=0; i < NUMWEAPONS; ++i)
            {
                candidate = cfg.weaponOrder[i];
                winf = &weaponinfo[candidate][pclass];

                // Is candidate available in this game mode?
                if(!(winf->mode[lvl].gamemodebits & gamemodebits))
                    continue;

                if(weapon == candidate)
                {   // weapon has a higher priority than the readyweapon.
                    returnval = weapon;
                }
                else if(player->readyweapon == candidate)
                {   // readyweapon has a higher priority so don't change.
                    break;
                }
            }
        }
    }
    else if(ammo != AM_NOAMMO) // Player is about to be given some ammo.
    {
        if((!player->ammo[ammo] && cfg.ammoAutoSwitch != 0) || force)
        {   // We were down to zero, so select a new weapon.

            // Iterate the weapon order array and see if the player owns a
            // weapon that can be used now they have this ammo.
            // Preferences are user selectable.
            for(i=0; i < NUMWEAPONS; ++i)
            {
                candidate = cfg.weaponOrder[i];
                winf = &weaponinfo[candidate][pclass];

                // Is candidate available in this game mode?
                if(!(winf->mode[lvl].gamemodebits & gamemodebits))
                    continue;

                // Does the player actually own this candidate?
                if(!player->weaponowned[candidate])
                    continue;

                // Does the weapon use this type of ammo?
                if(!(winf->mode[lvl].ammotype[ammo]))
                    continue;

                // FIXME: Have we got enough of ALL used ammo types?
                // Problem, since the ammo has not been given yet (could
                // be an object that gives several ammo types eg backpack)
                // we can't test for this with what we know!

                // This routine should be called AFTER the new ammo has
                // been given. Somewhat complex logic to decipher first...

                if(cfg.ammoAutoSwitch == 2)
                {   // Always change weapon mode
                    returnval = candidate;
                    break;
                }
                else if(cfg.ammoAutoSwitch == 1 &&
                        player->readyweapon == candidate)
                {   // readyweapon has a higher priority so don't change.
                    break;
                }
            }
        }
    }

    // Don't change to the exisitng weapon.
    if(returnval == player->readyweapon)
        returnval = WP_NOCHANGE;

    // Choosen a weapon to change to?
    if(returnval != WP_NOCHANGE)
    {
        player->pendingweapon = returnval;
        player->update |= PSF_PENDING_WEAPON | PSF_READY_WEAPON;
    }

    return returnval;
}

/**
 * Return the next weapon for the given player. Can return the existing
 * weapon if no other valid choices. Preferences are NOT user selectable.
 *
 * @param player        The player to work with.
 * @param next          Search direction (TRUE) = next, (FALSE) = previous.
 */
weapontype_t P_PlayerFindWeapon(player_t *player, boolean next)
{
    weapontype_t *list;
    int lvl, i;
#if __DOOM64TC__
    static weapontype_t  wp_list[] = {
        wp_fist, wp_pistol, wp_shotgun, wp_supershotgun, wp_chaingun,
        wp_missile, wp_plasma, wp_bfg, wp_chainsaw, wp_unmaker
    };

#elif __JDOOM__
    static weapontype_t  wp_list[] = {
        wp_fist, wp_pistol, wp_shotgun, wp_supershotgun, wp_chaingun,
        wp_missile, wp_plasma, wp_bfg, wp_chainsaw
    };

#elif __JHERETIC__
    static weapontype_t  wp_list[] = {
        WP_FIRST, WP_SECOND, WP_THIRD, WP_FOURTH, WP_FIFTH,
        WP_SIXTH, WP_SEVENTH, WP_EIGHTH
    };

#elif __JHEXEN__ || __JSTRIFE__
    static weapontype_t  wp_list[] = {
        WP_FIRST, WP_SECOND, WP_THIRD, WP_FOURTH
    };
#endif

#if __JHERETIC__
    lvl = (player->powers[pw_weaponlevel2]? 1 : 0);
#else
    lvl = 0;
#endif

    // Are we using weapon order preferences for next/previous?
    if(cfg.weaponNextMode)
    {
        list = (weapontype_t*) cfg.weaponOrder;
        next = !next; // Invert order.
    }
    else
        list = wp_list;

    // Find the current position in the weapon list.
    for(i = 0; i < NUMWEAPONS; ++i)
        if(list[i] == player->readyweapon)
            break;
    // Locate the next or previous weapon owned by the player.
    for(;;)
    {
        // Move the iterator.
        if(next)
            i++;
        else
            i--;
        if(i < 0)
            i = NUMWEAPONS - 1;
        else if(i > NUMWEAPONS - 1)
            i = 0;

        // Have we circled around?
        if(list[i] == player->readyweapon)
            break;

        // Available in this game mode? And a valid weapon?
        if((weaponinfo[list[i]][player->class].mode[lvl].
            gamemodebits & gamemodebits) &&
           player->weaponowned[list[i]])
            break;
    }
    return list[i];
}

/**
 * Send a message to the given player and maybe echos it to the console.
 *
 * @param player        The player to send the message to.
 * @param msg           The message to be sent.
 * @param noHide        <code>true</code> = show message even if messages
 *                      have been disabled by the player.
 */
void P_SetMessage(player_t *pl, char *msg, boolean noHide)
{
    HUMsg_PlayerMessage(pl, msg, MESSAGETICS, noHide, false);

    if(pl == &players[consoleplayer] && cfg.echoMsg)
        Con_FPrintf(CBLF_CYAN, "%s\n", msg);

    // Servers are responsible for sending these messages to the clients.
    NetSv_SendMessage(pl - players, msg);
}

#if __JHEXEN__ || __JSTRIFE__
/**
 * Send a yellow message to the given player and maybe echos it to the console.
 *
 * @param player        The player to send the message to.
 * @param msg           The message to be sent.
 * @param noHide        <code>true</code> = show message even if messages
 *                      have been disabled by the player.
 */
void P_SetYellowMessage(player_t *pl, char *msg, boolean noHide)
{
    HUMsg_PlayerMessage(pl, msg, 5 * MESSAGETICS, noHide, true);

    if(pl == &players[consoleplayer] && cfg.echoMsg)
        Con_FPrintf(CBLF_CYAN, "%s\n", msg);

    // Servers are responsible for sending these messages to the clients.
    NetSv_SendMessage(pl - players, msg);
}
#endif

void P_Thrust3D(player_t *player, angle_t angle, float lookdir,
                int forwardmove, int sidemove)
{
    angle_t pitch = LOOKDIR2DEG(lookdir) / 360 * ANGLE_MAX;
    angle_t sideangle = angle - ANG90;
    mobj_t *mo = player->plr->mo;
    int     zmul;
    fixed_t mom[3];

    angle >>= ANGLETOFINESHIFT;
    sideangle >>= ANGLETOFINESHIFT;
    pitch >>= ANGLETOFINESHIFT;

    mom[VX] = FixedMul(forwardmove, finecosine[angle]);
    mom[VY] = FixedMul(forwardmove, finesine[angle]);
    mom[VZ] = FixedMul(forwardmove, finesine[pitch]);

    zmul = finecosine[pitch];
    mom[VX] = FixedMul(mom[VX], zmul) + FixedMul(sidemove, finecosine[sideangle]);
    mom[VY] = FixedMul(mom[VY], zmul) + FixedMul(sidemove, finesine[sideangle]);

    mo->momx += mom[VX];
    mo->momy += mom[VY];
    mo->momz += mom[VZ];
}

boolean P_IsCamera(mobj_t *mo)
{
    // Client mobjs do not have thinkers and thus cannot be cameras.
    return (mo->thinker.function && mo->player &&
            mo->player->plr->flags & DDPF_CAMERA);
}

int P_CameraXYMovement(mobj_t *mo)
{
    if(!P_IsCamera(mo))
        return false;
#if __JDOOM__
    if(mo->flags & MF_NOCLIP
       // This is a very rough check!
       // Sometimes you get stuck in things.
       || P_CheckPosition2(mo, mo->pos[VX] + mo->momx, mo->pos[VY] + mo->momy, mo->pos[VZ]))
    {
#endif

        P_UnsetThingPosition(mo);
        mo->pos[VX] += mo->momx;
        mo->pos[VY] += mo->momy;
        P_SetThingPosition(mo);
        P_CheckPosition(mo, mo->pos[VX], mo->pos[VY]);
        mo->floorz = tmfloorz;
        mo->ceilingz = tmceilingz;

#if __JDOOM__
    }
#endif
    // Friction.
    mo->momx = FixedMul(mo->momx, 0xe800);
    mo->momy = FixedMul(mo->momy, 0xe800);
    return true;
}

int P_CameraZMovement(mobj_t *mo)
{
    if(!P_IsCamera(mo))
        return false;
    mo->pos[VZ] += mo->momz;
    mo->momz = FixedMul(mo->momz, 0xe800);

    /*if(mo->pos[VZ] < mo->floorz + 6 * FRACUNIT)
        mo->pos[VZ] = mo->floorz + 6 * FRACUNIT;
    if(mo->pos[VZ] > mo->ceilingz - 6 * FRACUNIT)
        mo->pos[VZ] = mo->ceilingz - 6 * FRACUNIT;*/
    return true;
}

/**
 * Set appropriate parameters for a camera.
 */
void P_PlayerThinkCamera(player_t *player)
{
    angle_t angle;
    int     full, dist;
    mobj_t *mo;

    // If this player is not a camera, get out of here.
    if(!(player->plr->flags & DDPF_CAMERA))
    {
        player->plr->mo->flags |= (MF_SOLID | MF_SHOOTABLE | MF_PICKUP);
        return;
    }

    mo = player->plr->mo;

    player->plr->viewheight = 0;
    mo->flags &= ~(MF_SOLID | MF_SHOOTABLE | MF_PICKUP);

    // How about viewlock?
    if(player->viewlock)
    {
        mobj_t *target = players->viewlock;

        if(!target->player || !target->player->plr->ingame)
        {
            player->viewlock = NULL;
            return;
        }

        full = player->lockFull;

        angle = R_PointToAngle2(mo->pos[VX], mo->pos[VY],
                                target->pos[VX], target->pos[VY]);
        //player->plr->flags |= DDPF_FIXANGLES;
        /* $unifiedangles */
        mo->angle = angle;
        //player->plr->clAngle = angle;
        player->plr->flags |= DDPF_INTERYAW;

        if(full)
        {
            dist = P_ApproxDistance(mo->pos[VX] - target->pos[VX],
                                    mo->pos[VY] - target->pos[VY]);
            angle =
                R_PointToAngle2(0, 0,
                                target->pos[VZ] + target->height / 2 - mo->pos[VZ],
                                dist);
            //player->plr->clLookDir =
            player->plr->lookdir =
                -(angle / (float) ANGLE_MAX * 360.0f - 90);
            if(player->plr->lookdir > 180)
                player->plr->lookdir -= 360;
            player->plr->lookdir *= 110.0f / 85.0f;
            if(player->plr->lookdir > 110)
                player->plr->lookdir = 110;
            if(player->plr->lookdir < -110)
                player->plr->lookdir = -110;

            player->plr->flags |= DDPF_INTERPITCH;
        }
    }
}

DEFCC(CCmdSetCamera)
{
    int     p;
    player_t* player;

    p = atoi(argv[1]);
    if(p < 0 || p >= MAXPLAYERS)
    {
        Con_Printf("Invalid console number %i.\n", p);
        return false;
    }

    player = &players[p];

    player->plr->flags ^= DDPF_CAMERA;

    if(player->plr->ingame && player->plr->flags & DDPF_CAMERA)
    {
        // Is now a camera.
        if(player->plr->mo)
            player->plr->mo->pos[VZ] += player->plr->viewheight;
    }

    return true;
}

DEFCC(CCmdSetViewMode)
{
    int     pl = consoleplayer;

    if(argc > 2)
        return false;

    if(argc == 2)
        pl = atoi(argv[1]);

    if(pl < 0 || pl >= MAXPLAYERS)
        return false;

    if(!(players[pl].plr->flags & DDPF_CHASECAM))
        players[pl].plr->flags |= DDPF_CHASECAM;
    else
        players[pl].plr->flags &= ~DDPF_CHASECAM;
    return true;
}

DEFCC(CCmdSetViewLock)
{
    int     pl = consoleplayer, lock;

    if(!stricmp(argv[0], "lockmode"))
    {
        lock = atoi(argv[1]);
        if(lock)
            players[pl].lockFull = true;
        else
            players[pl].lockFull = false;
        return true;
    }
    if(argc < 2)
        return false;
    if(argc >= 3)
        pl = atoi(argv[2]);     // Console number.
    lock = atoi(argv[1]);

    if(!(lock == pl || lock < 0 || lock >= MAXPLAYERS))
    {
        if(players[lock].plr->ingame && players[lock].plr->mo)
        {
            players[pl].viewlock = players[lock].plr->mo;
            return true;
        }
    }

    players[pl].viewlock = NULL;
    return false;
}

DEFCC(CCmdMakeLocal)
{
    int     p;
    char    buf[20];

    if(gamestate != GS_LEVEL)
    {
        Con_Printf("You must be in a game to create a local player.\n");
        return false;
    }

    p = atoi(argv[1]);
    if(p < 0 || p >= MAXPLAYERS)
    {
        Con_Printf("Invalid console number %i.\n", p);
        return false;
    }
    if(players[p].plr->ingame)
    {
        Con_Printf("Player %i is already in the game.\n", p);
        return false;
    }
    players[p].playerstate = PST_REBORN;
    players[p].plr->ingame = true;
    sprintf(buf, "conlocp %i", p);
    DD_Execute(buf, false);
    P_DealPlayerStarts(0);
    return true;
}

/**
 * Print the console player's coordinates.
 */
DEFCC(CCmdPrintPlayerCoords)
{
    mobj_t *mo = players[consoleplayer].plr->mo;

    if(!mo || gamestate != GS_LEVEL)
        return false;
    Con_Printf("Console %i: X=%g Y=%g Z=%g\n", consoleplayer,
               FIX2FLT(mo->pos[VX]), FIX2FLT(mo->pos[VY]), FIX2FLT(mo->pos[VZ]));
    return true;
}

DEFCC(CCmdCycleSpy)
{
    // FIXME: The engine should do this.
    Con_Printf("Spying not allowed.\n");
#if 0
    if(gamestate == GS_LEVEL && !deathmatch)
    {                           // Cycle the display player
        do
        {
            Set(DD_DISPLAYPLAYER, displayplayer + 1);
            if(displayplayer == MAXPLAYERS)
            {
                Set(DD_DISPLAYPLAYER, 0);
            }
        } while(!players[displayplayer].plr->ingame &&
                displayplayer != consoleplayer);
    }
#endif
    return true;
}

DEFCC(CCmdSpawnMobj)
{
    int     type;
    fixed_t pos[3];
    mobj_t *mo;

    if(argc != 5 && argc != 6)
    {
        Con_Printf("Usage: %s (type) (x) (y) (z) (angle)\n", argv[0]);
        Con_Printf("Type must be a defined Thing ID or Name.\n");
        Con_Printf("Z is an offset from the floor, 'floor' or 'ceil'.\n");
        Con_Printf("Angle (0..360) is optional.\n");
        return true;
    }

    if(IS_CLIENT)
    {
        Con_Printf("%s can't be used by clients.\n", argv[0]);
        return false;
    }

    // First try to find the thing by ID.
    if((type = Def_Get(DD_DEF_MOBJ, argv[1], 0)) < 0)
    {
        // Try to find it by name instead.
        if((type = Def_Get(DD_DEF_MOBJ_BY_NAME, argv[1], 0)) < 0)
        {
            Con_Printf("Undefined thing type %s.\n", argv[1]);
            return false;
        }
    }

    // The coordinates.
    pos[VX] = strtod(argv[2], 0) * FRACUNIT;
    pos[VY] = strtod(argv[3], 0) * FRACUNIT;
    if(!stricmp(argv[4], "floor"))
        pos[VZ] = ONFLOORZ;
    else if(!stricmp(argv[4], "ceil"))
        pos[VZ] = ONCEILINGZ;
    else
    {
        pos[VZ] = strtod(argv[4], 0) * FRACUNIT +
            P_GetFixedp(R_PointInSubsector(pos[VX], pos[VY]), DMU_SECTOR_OF_SUBSECTOR | DMU_FLOOR_HEIGHT);
    }

    mo = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], type);
    if(mo && argc == 6)
    {
        mo->angle = ((int) (strtod(argv[5], 0) / 360 * FRACUNIT)) << 16;
    }

#if __DOOM64TC__
    // d64tc > kaiser - another cheesy hack!!!
    if(mo->type == MT_DART || mo->type == MT_RDART)
    {
        S_StartSound(sfx_skeswg, mo); // we got darts! spawn skeswg sound!
    }
    else
    {
        mo->translucency = 255;

        S_StartSound(sfx_itmbk, mo); // if not dart, then spawn itmbk sound
        mo->intflags = MIF_FADE;
        mo->translucency = 255;
    }
    // << d64tc
#endif
    return true;
}
