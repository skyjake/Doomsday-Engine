/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
 *\author Copyright © 1993-1996 by id Software, Inc.
 *
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
 * Weapon sprite animation, weapon objects.
 * Action functions for weapons.
 */

#ifdef WIN32
// Sumtin' 'ere messes with poor ol' MSVC's head...
#  pragma optimize("g",off)
#endif

// HEADER FILES ------------------------------------------------------------

#include "doom64tc.h"

#include "d_net.h"
#include "p_player.h"
#include "p_map.h"

// MACROS ------------------------------------------------------------------

#define LOWERSPEED      FRACUNIT*6
#define RAISESPEED      FRACUNIT*6

#define WEAPONBOTTOM    128*FRACUNIT
#define WEAPONTOP       32*FRACUNIT

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

fixed_t swingx;
fixed_t swingy;

fixed_t bulletslope;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void P_SetPsprite(player_t *player, int position, statenum_t stnum)
{
    pspdef_t *psp;
    state_t *state;

    psp = &player->psprites[position];

    do
    {
        if(!stnum)
        {
            // object removed itself
            psp->state = NULL;
            break;
        }

        state = &states[stnum];
        psp->state = state;
        psp->tics = state->tics;    // could be 0

        if(state->misc[0])
        {
            // coordinate set
            psp->sx = state->misc[0] << FRACBITS;
            psp->sy = state->misc[1] << FRACBITS;
        }

        // Call action routine.
        // Modified handling.
        if(state->action)
        {
            state->action(player, psp);
            if(!psp->state)
                break;
        }

        stnum = psp->state->nextstate;

    } while(!psp->tics);
    // an initial state of 0 could cycle through
}

void P_CalcSwing(player_t *player)
{
    fixed_t swing;
    int     angle;

    // OPTIMIZE: tablify this.
    // A LUT would allow for different modes,
    //  and add flexibility.

    // FUNFACT: due to the way the swing and the sector damage are
    // calculated you are ALWAYS damaged at precisely the same time your
    // weapon is at the furthest point of its swing (left or right).

    swing = player->bob;

    angle = (FINEANGLES / 70 * leveltime) & FINEMASK;
    swingx = FixedMul(swing, finesine[angle]);

    angle = (FINEANGLES / 70 * leveltime + FINEANGLES / 2) & FINEMASK;
    swingy = -FixedMul(swingx, finesine[angle]);
}

/*
 * Starts bringing the pending weapon up
 * from the bottom of the screen.
 */
void P_BringUpWeapon(player_t *player)
{
    weaponmodeinfo_t *wminfo;

    wminfo = WEAPON_INFO(player->pendingweapon, player->class, 0);

    if(player->pendingweapon == WP_NOCHANGE)
        player->pendingweapon = player->readyweapon;

    if(wminfo->raisesound)
        S_StartSound(wminfo->raisesound, player->plr->mo);

    player->pendingweapon = WP_NOCHANGE;
    player->psprites[ps_weapon].sy = WEAPONBOTTOM;

    P_SetPsprite(player, ps_weapon, wminfo->upstate);
}

/*
 * Returns true if there is enough ammo to shoot.
 * If not, selects the next weapon to use.
 */
boolean P_CheckAmmo(player_t *player)
{
    ammotype_t i;
    int     count;
    boolean good;

    // Check we have enough of ALL ammo types used by this weapon.
    good = true;
    for(i=0; i < NUMAMMO && good; ++i)
    {
        if(!weaponinfo[player->readyweapon][player->class].mode[0].ammotype[i])
            continue; // Weapon does not take this type of ammo.

        // Minimal amount for one shot varies.
        count = weaponinfo[player->readyweapon][player->class].mode[0].pershot[i];

        // Return if current ammunition sufficient.
        if(player->ammo[i] < count)
        {
            good = false;
        }
    }
    if(good)
        return true;

    // Out of ammo, pick a weapon to change to.
    P_MaybeChangeWeapon(player, WP_NOCHANGE, AM_NOAMMO, false);

    // Now set appropriate weapon overlay.
    P_SetPsprite(player, ps_weapon, weaponinfo[player->readyweapon][player->class].mode[0].downstate);

    return false;
}

void P_FireWeapon(player_t *player)
{
    statenum_t newstate;

    if(!P_CheckAmmo(player))
        return;

    // Psprite state.
    player->plr->psprites[0].state = DDPSP_FIRE;

    P_SetMobjState(player->plr->mo, PCLASS_INFO(player->class)->attackstate);
    newstate = weaponinfo[player->readyweapon][player->class].mode[0].atkstate;
    P_SetPsprite(player, ps_weapon, newstate);
    NetSv_PSpriteChange(player - players, newstate);
    P_NoiseAlert(player->plr->mo, player->plr->mo);
}

/*
 * Player died, so put the weapon away.
 */
void P_DropWeapon(player_t *player)
{
    P_SetPsprite(player, ps_weapon, weaponinfo[player->readyweapon][player->class].mode[0].downstate);
}

/*
 * The player can fire the weapon or change to another weapon at this time.
 * Follows after getting weapon up, or after previous attack/fire sequence.
 */
void C_DECL A_WeaponReady(player_t *player, pspdef_t * psp)
{
    weaponmodeinfo_t *wminfo;

    // Enable the pspr Y offset (might be disabled in A_Lower).
    DD_SetInteger(DD_WEAPON_OFFSET_SCALE_Y, 1000);

    // get out of attack state
    if(player->plr->mo->state == &states[PCLASS_INFO(player->class)->attackstate] ||
       player->plr->mo->state == &states[PCLASS_INFO(player->class)->attackendstate])
    {
        P_SetMobjState(player->plr->mo, PCLASS_INFO(player->class)->normalstate);
    }

    if(player->readyweapon != WP_NOCHANGE)
    {
        wminfo = WEAPON_INFO(player->readyweapon, player->class, 0);

        // A weaponready sound?
        if(psp->state == &states[wminfo->readystate] && wminfo->readysound)
            S_StartSound(wminfo->readysound, player->plr->mo);

        // d64tc >
        // DJS - FIXME!
        if(player->readyweapon == wp_unmaker)
        {
            if(actions[A_WEAPON10].on &&
               (player->artifacts[it_helltime] || player->artifacts[it_float]))
            {
                player->outcastcycle++;
                player->pendingweapon = wp_unmaker;
            }
        }
        // < d64tc

        // check for change
        //  if player is dead, put the weapon away
        if(player->pendingweapon != WP_NOCHANGE || !player->health)
        {   //  (pending weapon should allready be validated)
            P_SetPsprite(player, ps_weapon, wminfo->downstate);
            return;
        }
    }

    // check for autofire
    if(player->cmd.attack)
    {
        wminfo = WEAPON_INFO(player->readyweapon, player->class, 0);

        if(!player->attackdown || wminfo->autofire)
        {
            player->attackdown = true;
            P_FireWeapon(player);
            return;
        }
    }
    else
        player->attackdown = false;

    // Bob the weapon based on movement speed.
    psp->sx = G_GetInteger(DD_PSPRITE_BOB_X);
    psp->sy = G_GetInteger(DD_PSPRITE_BOB_Y);

    // Psprite state.
    player->plr->psprites[0].state = DDPSP_BOBBING;
}

/*
 * The player can re-fire the weapon without lowering it entirely.
 */
void C_DECL A_ReFire(player_t *player, pspdef_t * psp)
{
    // check for fire
    //  (if a weaponchange is pending, let it go through instead)
    if(player->cmd.attack &&
       player->pendingweapon == WP_NOCHANGE &&
       player->health)
    {
        player->refire++;
        P_FireWeapon(player);
    }
    else
    {
        player->refire = 0;
        P_CheckAmmo(player);
    }
}

void C_DECL A_CheckReload(player_t *player, pspdef_t * psp)
{
    P_CheckAmmo(player);
#if 0
    if(player->ammo[am_shell] < 2)
        P_SetPsprite(player, ps_weapon, S_DSNR1);
#endif
}

/*
 * Lowers current weapon, and changes weapon at bottom.
 */
void C_DECL A_Lower(player_t *player, pspdef_t * psp)
{
    psp->sy += LOWERSPEED;

    // Psprite state.
    player->plr->psprites[0].state = DDPSP_DOWN;

    // Should we disable the lowering?
    if(!cfg.bobWeaponLower || weaponinfo[player->readyweapon][player->class].mode[0].static_switch)
    {
        DD_SetInteger(DD_WEAPON_OFFSET_SCALE_Y, 0);
    }

    // Is already down.
    if(psp->sy < WEAPONBOTTOM)
        return;

    // Player is dead.
    if(player->playerstate == PST_DEAD)
    {
        psp->sy = WEAPONBOTTOM;

        // don't bring weapon back up
        return;
    }

    if(player->readyweapon == wp_plasma) // d64tc
        P_SetPsprite(player, ps_flash, S_NULL);

    // The old weapon has been lowered off the screen,
    // so change the weapon and start raising it
    if(!player->health)
    {
        // Player is dead, so keep the weapon off screen.
        P_SetPsprite(player, ps_weapon, S_NULL);
        return;
    }

    player->readyweapon = player->pendingweapon;
    player->update |= PSF_READY_WEAPON;

    // Should we suddenly lower the weapon?
    if(cfg.bobWeaponLower && !weaponinfo[player->readyweapon][player->class].mode[0].static_switch)
    {
        DD_SetInteger(DD_WEAPON_OFFSET_SCALE_Y, 1000);
    }

    P_BringUpWeapon(player);
}

void C_DECL A_Raise(player_t *player, pspdef_t * psp)
{
    statenum_t newstate;

    // Psprite state.
    player->plr->psprites[0].state = DDPSP_UP;

    // Should we disable the lowering?
    if(!cfg.bobWeaponLower || weaponinfo[player->readyweapon][player->class].mode[0].static_switch)
    {
        DD_SetInteger(DD_WEAPON_OFFSET_SCALE_Y, 0);
    }

    // d64tc >
    if(player->readyweapon == wp_plasma)
    {
        P_SetPsprite(player, ps_flash, S_PLASMASHOCK1);
    }
    else if(player->readyweapon == wp_unmaker)
    {
        if(player->artifacts[it_helltime] ||
            (player->artifacts[it_helltime] && player->artifacts[it_float]))
        {
            if(player->outcastcycle == 1 && player->artifacts[it_helltime])
            {
                P_SetPsprite(player, ps_flash, S_HTIMEBLINK1);
            }
            else if(player->outcastcycle == 2 && player->artifacts[it_float])
            {
                P_SetPsprite(player, ps_flash, S_LDBLINK1);
            }
            else
            {
                player->outcastcycle = 0;
                P_SetPsprite(player, ps_flash, S_NULL);
            }
        }

        if(player->artifacts[it_float] && !player->artifacts[it_helltime])
        {
            if(player->outcastcycle == 1 && player->artifacts[it_float])
            {
                P_SetPsprite(player, ps_flash, S_LDBLINK1);
            }
            else
            {
                player->outcastcycle = 0;
                P_SetPsprite(player, ps_flash, S_NULL);
            }
        }
    }
    else
    {
        P_SetPsprite(player, ps_flash, S_NULL);
    }
    // < d64tc

    psp->sy -= RAISESPEED;

    if(psp->sy > WEAPONTOP)
        return;

    // Enable the pspr Y offset once again.
    DD_SetInteger(DD_WEAPON_OFFSET_SCALE_Y, 1000);

    psp->sy = WEAPONTOP;

    // The weapon has been raised all the way,
    //  so change to the ready state.
    newstate = weaponinfo[player->readyweapon][player->class].mode[0].readystate;

    P_SetPsprite(player, ps_weapon, newstate);
}

void C_DECL A_GunFlash(player_t *player, pspdef_t * psp)
{
    P_SetMobjState(player->plr->mo, PCLASS_INFO(player->class)->attackendstate);
    P_SetPsprite(player, ps_flash, weaponinfo[player->readyweapon][player->class].mode[0].flashstate);
}

void C_DECL A_Punch(player_t *player, pspdef_t * psp)
{
    angle_t angle;
    int     damage;
    int     slope;

    if(IS_CLIENT)
        return;

    damage = (P_Random() % 10 + 1) << 1;

    if(player->powers[pw_strength])
        damage *= 10;

    angle = player->plr->mo->angle;
    angle += (P_Random() - P_Random()) << 18;
    //slope = P_AimLineAttack(player->plr->mo, angle, MELEERANGE);
    //P_LineAttack(player->plr->mo, angle, MELEERANGE, slope, damage);
    slope = P_AimLineAttack(player->plr->mo, angle, PLRMELEERANGE); // d64tc
    P_LineAttack(player->plr->mo, angle, PLRMELEERANGE, slope, damage); // d64tc

    // turn to face target
    if(linetarget)
    {
        //S_StartSound (player->plr->mo, sfx_punch, player);
        S_StartSound(sfx_punch, player->plr->mo);
        player->plr->mo->angle =
            R_PointToAngle2(player->plr->mo->pos[VX], player->plr->mo->pos[VY],
                            linetarget->pos[VX], linetarget->pos[VY]);
        player->plr->flags |= DDPF_FIXANGLES;
    }
}

void C_DECL A_Saw(player_t *player, pspdef_t * psp)
{
    angle_t angle;
    int     damage;
    int     slope;

    if(IS_CLIENT)
        return;

    damage = 2 * (P_Random() % 10 + 1);
    angle = player->plr->mo->angle;
    angle += (P_Random() - P_Random()) << 18;

    // use meleerange + 1 se the puff doesn't skip the flash
    //slope = P_AimLineAttack(player->plr->mo, angle, MELEERANGE + 1); // d64tc
    slope = P_AimLineAttack(player->plr->mo, angle, PLRMELEERANGE + 1); // d64tc
    //P_LineAttack(player->plr->mo, angle, MELEERANGE + 1, slope, damage);
    P_LineAttack(player->plr->mo, angle, PLRMELEERANGE + 1, slope, damage); // d64tc

    if(!linetarget)
    {
        //S_StartSound (player->plr->mo, sfx_sawful, player);
        S_StartSound(sfx_sawful, player->plr->mo);
        return;
    }
    //S_StartSound (player->plr->mo, sfx_sawhit, player);
    S_StartSound(sfx_sawhit, player->plr->mo);

    // turn to face target
    angle =
        R_PointToAngle2(player->plr->mo->pos[VX], player->plr->mo->pos[VY],
                        linetarget->pos[VX], linetarget->pos[VY]);
    if(angle - player->plr->mo->angle > ANG180)
    {
        if(angle - player->plr->mo->angle < -ANG90 / 32) // d64tc was "/ 20"
            player->plr->mo->angle = angle + ANG90 / 32; // d64tc was "/ 21"
        else
            player->plr->mo->angle -= ANG90 / 20;
    }
    else
    {
        if(angle - player->plr->mo->angle > ANG90 / 32) // d64tc was "/ 20"
            player->plr->mo->angle = angle - ANG90 / 32; // d64tc was "/ 21"
        else
            player->plr->mo->angle += ANG90 / 20;
    }
    player->plr->mo->flags |= MF_JUSTATTACKED;
}

void C_DECL A_FireMissile(player_t *player, pspdef_t * psp)
{
    P_ShotAmmo(player);
    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;
    P_SpawnPlayerMissile(player->plr->mo, MT_ROCKET);

    // d64tc >
    if (cfg.weaponRecoil)
    {
        angle_t angle = player->plr->mo->angle + ANG180;

        angle >>= ANGLETOFINESHIFT;
        player->plr->mo->momx += FixedMul(4 * FRACUNIT, finecosine[angle]);
        player->plr->mo->momy += FixedMul(4 * FRACUNIT, finesine[angle]);
    }
    // < d64tc
}

/**
 * d64tc: Spawns a smoke sprite during the missle attack
 */
void C_DECL A_Rocketpuff(mobj_t *smoke)
{
    if(!smoke)
        return;

    P_SpawnMobj(smoke->pos[VX], smoke->pos[VY], smoke->pos[VZ], MT_ROCKETPUFF);

    if(smoke->type == MT_GRENADE)
    {
        smoke->reactiontime -= 8;
        if(smoke->reactiontime <= 0)
        {
            smoke->momx = smoke->momy = smoke->momz = 0;

            P_SetMobjState(smoke, smoke->info->deathstate);
            S_StartSound(smoke->info->deathsound, smoke);
        }
    }
}

void C_DECL A_FireBFG(player_t *player, pspdef_t * psp)
{
    P_ShotAmmo(player);
    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;
    P_SpawnPlayerMissile(player->plr->mo, MT_BFG);
}

void C_DECL A_FirePlasma(player_t *player, pspdef_t * psp)
{
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash,
                 weaponinfo[player->readyweapon][player->class].mode[0].flashstate +
                 (P_Random() & 1));

    P_SetPsprite(player, ps_flash, S_NULL); // d64tc wha?

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_SpawnPlayerMissile(player->plr->mo, MT_PLASMA);
}

/**
 * d64tc
 */
void C_DECL A_DrawPlasmaTube(player_t *player, pspdef_t *psp)
{
    P_SetPsprite(player, ps_flash, S_PLASMASHOCK1);
}

/**
 * d64tc
 */
void C_DECL A_PlasmaBuzz(player_t *player)
{
    S_StartSound(sfx_psidl, player->plr->mo);
}

/**
 * d64tc
 */
void C_DECL A_Lasersmoke(mobj_t *spark)
{
    P_SpawnMobj(spark->pos[VX], spark->pos[VY], spark->pos[VZ], MT_LASERDUST);
}

/**
 * d64tc
 */
void C_DECL A_RevealFloater(mobj_t *mo)
{
    mobj_t *mo2;
    fixed_t pos[3];

    memcpy(pos, mo->pos, sizeof(pos));
    pos[VX] += ((P_Random() % 63) - 32) * FRACUNIT;
    pos[VY] += ((P_Random() % 63) - 32) * FRACUNIT;

    mo2 = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_LASERDUST);

    mo2->flags &= ~MF_NOGRAVITY;
    mo->reactiontime--;
    // DJS - Is this ever true? We've just spawned it so it will be -1...
    // FIXME?
    if(mo->reactiontime == 0)
    {
        P_SetMobjState(mo, S_TBALLX1);
    }
}

/**
 * d64tc
 */
void A_SpawnFloater(player_t *player)
{
    angle_t ang;
    mobj_t *mo, *floater;
    fixed_t pos[3];

    if(!player || !player->plr->mo)
        return;

    mo = player->plr->mo;
    ang = mo->angle >> ANGLETOFINESHIFT;

    memcpy(pos, mo->pos, sizeof(pos));
    pos[VX] += FixedMul(mo->radius * 4, finecosine[ang]);
    pos[VY] += FixedMul(mo->radius * 4, finesine[ang]);

    floater = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_FLOATER);

    if(floater)
    {
        floater->angle = mo->angle;
        floater->momz += 1 * FRACUNIT;
    }
}

/**
 * d64tc
 * kaiser - thanks to jow for giving me a jumpstart :)
 * DJS - TODO: Split into multiple firing modes.
 */
void C_DECL A_FireSingleLaser(player_t *player, pspdef_t *psp)
{
    mobj_t *pmo;

    if(player->outcastcycle == 1 && player->artifacts[it_helltime])
    {
        P_SetPsprite(player, ps_flash, S_HTIMEBLINK3);
    }
    else if((player->outcastcycle == 2 && player->artifacts[it_float]) ||
            (player->outcastcycle == 1 &&
             (player->artifacts[it_float] && !(player->artifacts[it_helltime]))
            ))
    {
        P_SetPsprite(player, ps_flash, S_LDBLINK3);
    }
    else
    {
        P_SetPsprite(player, ps_flash,
                     weaponinfo[player->readyweapon][player->class].mode[0].flashstate);
    }

    if(IS_CLIENT)
        return;

    if(player->outcastcycle == 1 && player->artifacts[it_helltime] &&
       !player->helltime)
    {
        if(player->health >= 20)
        {
            if(!(player->cheats & CF_GODMODE))
            {
                player->health -= 20;
            }

            player->helltime = 450;

            P_SetMessage(player, HELLTIMEON, false);
            S_StartSound(sfx_htime, player->plr->mo);
        }
        else
        {
            P_SetMessage(player, HELLTIMEWEAK, false);
        }

    }
    else if((player->outcastcycle == 2 && player->artifacts[it_float]) ||
            (player->outcastcycle == 1 &&
             (player->artifacts[it_float] && !(player->artifacts[it_helltime]))
            ))
    {
        if(player->devicetime < 80)
        {
            P_SetMessage(player, UNMAKERCHARGE, false);
            return;
        }

        if(player->devicetime >= 81)
            player->devicetime -= 80;

        if(player->devicetime)
        {
            A_SpawnFloater(player);
            S_StartSound(sfx_itmbk, player->plr->mo);
        }
    }
    else
    {
        P_ShotAmmo(player);

        pmo = player->plr->mo;
        player->update |= PSF_AMMO;

        if(player->laserpw == 0)
        {
            P_SpawnPlayerMissile(player->plr->mo, MT_LASERSHOTWEAK);
        }
        else if(player->laserpw == 1)
        {
            P_SpawnPlayerMissile(player->plr->mo, MT_LASERSHOT);
        }
        else if(player->laserpw == 2)
        {
            P_ShotAmmo(player); // adds an extra ammo subtractor

            P_SPMAngle(pmo, MT_LASERSHOT, pmo->angle - (ANG45 / 8));
            P_SPMAngle(pmo, MT_LASERSHOT, pmo->angle + (ANG45 / 8));
        }
        else if(player->laserpw == 3)
        {
            P_ShotAmmo(player); //adds another subtractor, now consumes 3 cells!
            P_ShotAmmo(player);

            P_SpawnPlayerMissile(pmo, MT_LASERSHOT);
            P_SPMAngle(pmo, MT_LASERSHOT, pmo->angle - (ANG45 / 6));
            P_SPMAngle(pmo, MT_LASERSHOT, pmo->angle + (ANG45 / 6));
        }
    }
}

/**
 * d64tc
 */
void C_DECL A_FireDoubleLaser(player_t *player, pspdef_t *psp)
{
    mobj_t *pmo;

    P_ShotAmmo(player);

    pmo = player->plr->mo;
    player->update |= PSF_AMMO;

    if(IS_CLIENT)
        return;

    P_SpawnPlayerMissile(pmo, MT_LASERSHOT);
    P_SPMAngle(pmo, MT_LASERSHOT, pmo->angle - (ANG45 / 8));
    P_SPMAngle(pmo, MT_LASERSHOT, pmo->angle + (ANG45 / 8));
}

/**
 * d64tc
 */
void C_DECL A_FireDoubleLaser1(player_t *player, pspdef_t *psp)
{
    mobj_t *pmo;

    P_ShotAmmo(player);

    pmo = player->plr->mo;
    player->update |= PSF_AMMO;

    if(IS_CLIENT)
        return;

    P_SpawnPlayerMissile(pmo, MT_LASERSHOT);
    P_SPMAngle(pmo, MT_LASERSHOT, pmo->angle - (ANG45 / 4));
    P_SPMAngle(pmo, MT_LASERSHOT, pmo->angle + (ANG45 / 4));
}

/**
 * d64tc
 */
void C_DECL A_FireDoubleLaser2(player_t *player, pspdef_t *psp)
{
    mobj_t *pmo;

    P_ShotAmmo(player);

    pmo = player->plr->mo;
    player->update |= PSF_AMMO;

    if(IS_CLIENT)
        return;

    P_SpawnPlayerMissile(pmo, MT_LASERSHOT);
    P_SPMAngle(pmo, MT_LASERSHOT, pmo->angle - (ANG45 / 3));
    P_SPMAngle(pmo, MT_LASERSHOT, pmo->angle + (ANG45 / 3));
}

/*
 * Sets a slope so a near miss is at aproximately
 * the height of the intended target
 */
void P_BulletSlope(mobj_t *mo)
{
    angle_t an;

    // see which target is to be aimed at
    an = mo->angle;
    bulletslope = P_AimLineAttack(mo, an, 16 * 64 * FRACUNIT);

    if(!linetarget)
    {
        an += 1 << 26;
        bulletslope = P_AimLineAttack(mo, an, 16 * 64 * FRACUNIT);
        if(!linetarget)
        {
            an -= 2 << 26;
            bulletslope = P_AimLineAttack(mo, an, 16 * 64 * FRACUNIT);
        }
    }
}

void P_GunShot(mobj_t *mo, boolean accurate)
{
    angle_t angle;
    int     damage;

    damage = 5 * (P_Random() % 3 + 1);
    angle = mo->angle;

    if(!accurate)
        angle += (P_Random() - P_Random()) << 18;

    P_LineAttack(mo, angle, MISSILERANGE, bulletslope, damage);
}

void C_DECL A_FirePistol(player_t *player, pspdef_t * psp)
{
    S_StartSound(sfx_pistol, player->plr->mo);

    P_SetMobjState(player->plr->mo, PCLASS_INFO(player->class)->attackendstate);

    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash, weaponinfo[player->readyweapon][player->class].mode[0].flashstate);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_BulletSlope(player->plr->mo);
    P_GunShot(player->plr->mo, !player->refire);
}

void C_DECL A_FireShotgun(player_t *player, pspdef_t * psp)
{
    int     i;

    S_StartSound(sfx_shotgn, player->plr->mo);
    P_SetMobjState(player->plr->mo, PCLASS_INFO(player->class)->attackendstate);

    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash, weaponinfo[player->readyweapon][player->class].mode[0].flashstate);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_BulletSlope(player->plr->mo);

    for(i = 0; i < 7; i++)
        P_GunShot(player->plr->mo, false);
}

void C_DECL A_FireShotgun2(player_t *player, pspdef_t * psp)
{
    int     i;
    angle_t angle;
    int     damage;

    S_StartSound(sfx_dshtgn, player->plr->mo);
    P_SetMobjState(player->plr->mo, PCLASS_INFO(player->class)->attackendstate);

    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash, weaponinfo[player->readyweapon][player->class].mode[0].flashstate);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_BulletSlope(player->plr->mo);

    // d64tc >
    if(cfg.weaponRecoil)
    {
        player->plr->mo->angle += ANG90/90;
        angle = player->plr->mo->angle + ANG180;
        angle >>= ANGLETOFINESHIFT;

        player->plr->mo->momx += FixedMul(4 * FRACUNIT, finecosine[angle]);
        player->plr->mo->momy += FixedMul(4 * FRACUNIT, finesine[angle]);
    }
    // < d64tc

    for(i = 0; i < 20; i++)
    {
        damage = 5 * (P_Random() % 3 + 1);
        angle = player->plr->mo->angle;
        angle += (P_Random() - P_Random()) << 19;
        P_LineAttack(player->plr->mo, angle, MISSILERANGE,
                     bulletslope + ((P_Random() - P_Random()) << 5), damage);
    }
}

void C_DECL A_FireCGun(player_t *player, pspdef_t *psp)
{
    S_StartSound(sfx_pistol, player->plr->mo);

    P_SetMobjState(player->plr->mo, PCLASS_INFO(player->class)->attackendstate);

    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash,
                 weaponinfo[player->readyweapon][player->class].mode[0].flashstate + psp->state -
                 &states[S_CHAIN1]);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    psp->sx = ((P_Random() & 8) - 2) * FRACUNIT; // d64tc

    P_BulletSlope(player->plr->mo);

    // d64tc >
    if(cfg.weaponRecoil)
    {
        player->plr->mo->angle += ANG90/256; //nice little recoil effect there
    }
    // < d64tc

    P_GunShot(player->plr->mo, !player->refire);
}

void C_DECL A_Light0(player_t *player, pspdef_t * psp)
{
    player->plr->extralight = 0;
}

void C_DECL A_Light1(player_t *player, pspdef_t * psp)
{
    player->plr->extralight = 1;
}

void C_DECL A_Light2(player_t *player, pspdef_t * psp)
{
    player->plr->extralight = 2;
}

/*
 * Spawn a BFG explosion on every monster in view
 */
void C_DECL A_BFGSpray(mobj_t *mo)
{
    int     i;
    int     j;
    int     damage;
    angle_t an;

    // offset angles from its attack angle
    for(i = 0; i < 40; i++)
    {
        an = mo->angle - ANG90 / 2 + ANG90 / 40 * i;

        // mo->target is the originator (player)
        //  of the missile
        P_AimLineAttack(mo->target, an, 16 * 64 * FRACUNIT);

        if(!linetarget)
            continue;

        P_SpawnMobj(linetarget->pos[VX], linetarget->pos[VY],
                    linetarget->pos[VZ] + (linetarget->height >> 2), MT_EXTRABFG);

        damage = 0;
        for(j = 0; j < 15; j++)
            damage += (P_Random() & 7) + 1;

        P_DamageMobj(linetarget, mo->target, mo->target, damage);
    }
}

void C_DECL A_BFGsound(player_t *player, pspdef_t * psp)
{
    S_StartSound(sfx_bfg, player->plr->mo);
}

/*
 * Called at start of level for each player.
 */
void P_SetupPsprites(player_t *player)
{
    int     i;

    // remove all psprites
    for(i = 0; i < NUMPSPRITES; i++)
        player->psprites[i].state = NULL;

    // spawn the gun
    player->pendingweapon = player->readyweapon;
    P_BringUpWeapon(player);
}

/*
 * Called every tic by player thinking routine.
 */
void P_MovePsprites(player_t *player)
{
    int     i;
    pspdef_t *psp;
    state_t *state;

    psp = &player->psprites[0];
    for(i = 0; i < NUMPSPRITES; i++, psp++)
    {
        // a null state means not active
        state = psp->state;
        if(state)
        {
            // drop tic count and possibly change state

            // a -1 tic count never changes
            if(psp->tics != -1)
            {
                psp->tics--;
                if(!psp->tics)
                    P_SetPsprite(player, i, psp->state->nextstate);
            }
        }
    }

    player->psprites[ps_flash].sx = player->psprites[ps_weapon].sx;
    player->psprites[ps_flash].sy = player->psprites[ps_weapon].sy;
}
