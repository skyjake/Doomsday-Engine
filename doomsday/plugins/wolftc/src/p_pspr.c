/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <yagisan@dengine.net>
 *\author Copyright © 2006 Martin Eyre <martineyre@btinternet.com>
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
 * p_pspr.c: Weapon sprite animation, weapon objects.
 *
 * Action functions for weapons.
 */

#ifdef MSVC
// Sumtin' 'ere messes with poor ol' MSVC's head...
#  pragma optimize("g",off)
#endif

// HEADER FILES ------------------------------------------------------------

#include <math.h>
#include "wolftc.h"

#include "d_net.h"
#include "p_player.h"
#include "p_map.h"

// MACROS ------------------------------------------------------------------

#define LOWERSPEED          (6)
#define RAISESPEED          (6)
#define WEAPONBOTTOM        (128)
#define WEAPONTOP           (32)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

static float swing[2];
static float bulletSlope;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void P_SetPsprite(player_t *player, int position, statenum_t stnum)
{
    pspdef_t *psp;
    state_t *state;

    psp = &player->pSprites[position];

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
            psp->pos[VX] = (float) state->misc[0];
            psp->pos[VY] = (float) state->misc[1];
        }

        // Call action routine.
        // Modified handling.
        if(state->action)
        {
            state->action(player, psp);
            if(!psp->state)
                break;
        }

        stnum = psp->state->nextState;

    } while(!psp->tics);
    // an initial state of 0 could cycle through
}

void P_CalcSwing(player_t *player)
{
    int                 angle;
    float               mul;

    /**
     * Fun Fact:
     * Due to the way the swing and the sector damage are calculated you
     * are ALWAYS damaged at precisely the same time your weapon is at the
     * furthest point of its swing (left or right).
     */
    mul = player->bob;

    angle = (FINEANGLES / 70 * levelTime) & FINEMASK;
    swing[VX] = mul * FIX2FLT(finesine[angle]);

    angle = (FINEANGLES / 70 * levelTime + FINEANGLES / 2) & FINEMASK;
    swing[VY] = -(mul * FIX2FLT(finesine[angle]));
}

/*
 * Starts bringing the pending weapon up
 * from the bottom of the screen.
 */
void P_BringUpWeapon(player_t *player)
{
    weaponmodeinfo_t *wminfo;

    wminfo = WEAPON_INFO(player->pendingWeapon, player->class, 0);

    if(player->pendingWeapon == WT_NOCHANGE)
        player->pendingWeapon = player->readyWeapon;

    if(wminfo->raiseSound)
        S_StartSound(wminfo->raiseSound, player->plr->mo);

    player->pendingWeapon = WT_NOCHANGE;
    player->pSprites[ps_weapon].pos[VY] = WEAPONBOTTOM;

    P_SetPsprite(player, ps_weapon, wminfo->upState);
}

void P_FireWeapon(player_t *player)
{
    statenum_t newstate;

    if(!P_CheckAmmo(player))
        return;

    // Psprite state.
    player->plr->pSprites[0].state = DDPSP_FIRE;

    P_MobjChangeState(player->plr->mo, PCLASS_INFO(player->class)->attackState);
    newstate = weaponInfo[player->readyWeapon][player->class].mode[0].attackState;
    P_SetPsprite(player, ps_weapon, newstate);
    NetSv_PSpriteChange(player - players, newstate);
    P_NoiseAlert(player->plr->mo, player->plr->mo);
}

/*
 * Player died, so put the weapon away.
 */
void P_DropWeapon(player_t *player)
{
    P_SetPsprite(player, ps_weapon, weaponInfo[player->readyWeapon][player->class].mode[0].downState);
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
    if(player->plr->mo->state == &states[PCLASS_INFO(player->class)->attackState] ||
       player->plr->mo->state == &states[PCLASS_INFO(player->class)->attackEndState])
    {
        P_MobjChangeState(player->plr->mo, PCLASS_INFO(player->class)->normalState);
    }

    if(player->readyWeapon != WT_NOCHANGE)
    {
        wminfo = WEAPON_INFO(player->readyWeapon, player->class, 0);

        // A weaponready sound?
        if(psp->state == &states[wminfo->readyState] && wminfo->readySound)
            S_StartSound(wminfo->readySound, player->plr->mo);

        // check for change
        //  if player is dead, put the weapon away
        if(player->pendingWeapon != WT_NOCHANGE || !player->health)
        {   //  (pending weapon should allready be validated)
            P_SetPsprite(player, ps_weapon, wminfo->downState);
            return;
        }
    }

    // check for autofire
    if(player->brain.attack)
    {
        wminfo = WEAPON_INFO(player->readyWeapon, player->class, 0);

        if(!player->attackDown || wminfo->autoFire)
        {
            player->attackDown = true;
            P_FireWeapon(player);
            return;
        }
    }
    else
        player->attackDown = false;

    // Bob the weapon based on movement speed.
    psp->pos[VX] = *((float *)G_GetVariable(DD_PSPRITE_BOB_X));
    psp->pos[VY] = *((float *)G_GetVariable(DD_PSPRITE_BOB_Y));

    // Psprite state.
    player->plr->pSprites[0].state = DDPSP_BOBBING;
}

/**
 * The player can re-fire the weapon without lowering it entirely.
 */
void C_DECL A_ReFire(player_t *player, pspdef_t *psp)
{
    // check for fire
    //  (if a weaponchange is pending, let it go through instead)
    if((player->brain.attack) &&
       player->pendingWeapon == WT_NOCHANGE && player->health)
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

void C_DECL A_CheckReload(player_t *player, pspdef_t *psp)
{
    P_CheckAmmo(player);
#if 0
    if(player->ammo[AT_SHELL] < 2)
        P_SetPsprite(player, ps_weapon, S_DSNR1);
#endif
}

/*
 * Lowers current weapon, and changes weapon at bottom.
 */
void C_DECL A_Lower(player_t *player, pspdef_t * psp)
{
    psp->pos[VY] += LOWERSPEED;

    // Psprite state.
    player->plr->pSprites[0].state = DDPSP_DOWN;

    // Should we disable the lowering?
    if(!cfg.bobWeaponLower || weaponInfo[player->readyWeapon][player->class].mode[0].staticSwitch)
    {
        DD_SetInteger(DD_WEAPON_OFFSET_SCALE_Y, 0);
    }

    // Is already down.
    if(psp->pos[VY] < WEAPONBOTTOM)
        return;

    // Player is dead.
    if(player->playerState == PST_DEAD)
    {
        psp->pos[VY] = WEAPONBOTTOM;

        // don't bring weapon back up
        return;
    }

    // The old weapon has been lowered off the screen,
    // so change the weapon and start raising it
    if(!player->health)
    {
        // Player is dead, so keep the weapon off screen.
        P_SetPsprite(player, ps_weapon, S_NULL);
        return;
    }

    player->readyWeapon = player->pendingWeapon;
    player->update |= PSF_READY_WEAPON;

    // Should we suddenly lower the weapon?
    if(cfg.bobWeaponLower && !weaponInfo[player->readyWeapon][player->class].mode[0].staticSwitch)
    {
        DD_SetInteger(DD_WEAPON_OFFSET_SCALE_Y, 1000);
    }

    P_BringUpWeapon(player);
}

void C_DECL A_Raise(player_t *player, pspdef_t * psp)
{
    statenum_t newstate;

    // Psprite state.
    player->plr->pSprites[0].state = DDPSP_UP;

    // Should we disable the lowering?
    if(!cfg.bobWeaponLower || weaponInfo[player->readyWeapon][player->class].mode[0].staticSwitch)
    {
        DD_SetInteger(DD_WEAPON_OFFSET_SCALE_Y, 0);
    }

    psp->pos[VY] -= RAISESPEED;

    if(psp->pos[VY] > WEAPONTOP)
        return;

    // Enable the pspr Y offset once again.
    DD_SetInteger(DD_WEAPON_OFFSET_SCALE_Y, 1000);

    psp->pos[VY] = WEAPONTOP;

    // The weapon has been raised all the way,
    //  so change to the ready state.
    newstate = weaponInfo[player->readyWeapon][player->class].mode[0].readyState;

    P_SetPsprite(player, ps_weapon, newstate);
}

void C_DECL A_GunFlash(player_t *player, pspdef_t * psp)
{
    P_MobjChangeState(player->plr->mo, PCLASS_INFO(player->class)->attackEndState);
    P_SetPsprite(player, ps_flash, weaponInfo[player->readyWeapon][player->class].mode[0].flashState);
}

void C_DECL A_Punch(player_t *player, pspdef_t * psp)
{
    angle_t     angle;
    int         damage;
    float       slope;

    P_ShotAmmo(player);
    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    damage = (P_Random() % 10 + 1) << 1;

    if(player->powers[PT_STRENGTH])
        damage *= 10;

    angle = player->plr->mo->angle;
    angle += (P_Random() - P_Random()) << 18;
    slope = P_AimLineAttack(player->plr->mo, angle, MELEERANGE);
    P_LineAttack(player->plr->mo, angle, MELEERANGE, slope, damage);

    // Turn to face target.
    if(lineTarget)
    {
        S_StartSound(SFX_PUNCH, player->plr->mo);

        player->plr->mo->angle =
            R_PointToAngle2(player->plr->mo->pos[VX], player->plr->mo->pos[VY],
                            lineTarget->pos[VX], lineTarget->pos[VY]);
        player->plr->flags |= DDPF_FIXANGLES;
    }
}

void C_DECL A_Saw(player_t *player, pspdef_t *psp)
{
    angle_t     angle;
    int         damage;
    float       slope;

    P_ShotAmmo(player);
    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    damage = (float) (P_Random() % 10 + 1) * 2;
    angle = player->plr->mo->angle;
    angle += (P_Random() - P_Random()) << 18;

    // Use meleerange + 1 so the puff doesn't skip the flash.
    slope = P_AimLineAttack(player->plr->mo, angle, MELEERANGE + 1);
    P_LineAttack(player->plr->mo, angle, MELEERANGE + 1, slope, damage);

    if(!lineTarget)
    {
        S_StartSound(SFX_SAWFUL, player->plr->mo);
        return;
    }

    S_StartSound(SFX_SAWHIT, player->plr->mo);

    // Turn to face target.
    angle =
        R_PointToAngle2(player->plr->mo->pos[VX], player->plr->mo->pos[VY],
                        lineTarget->pos[VX], lineTarget->pos[VY]);
    if(angle - player->plr->mo->angle > ANG180)
    {
        if(angle - player->plr->mo->angle < -ANG90 / 20)
            player->plr->mo->angle = angle + ANG90 / 21;
        else
            player->plr->mo->angle -= ANG90 / 20;
    }
    else
    {
        if(angle - player->plr->mo->angle > ANG90 / 20)
            player->plr->mo->angle = angle - ANG90 / 21;
        else
            player->plr->mo->angle += ANG90 / 20;
    }
    player->plr->mo->flags |= MF_JUSTATTACKED;
}

void C_DECL A_FireMissile(player_t *player, pspdef_t *psp)
{
    P_ShotAmmo(player);
    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_SpawnMissile(MT_ROCKET, player->plr->mo, NULL);
}

void C_DECL A_FireBFG(player_t *player, pspdef_t *psp)
{
    P_ShotAmmo(player);
    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_SpawnMissile(MT_BFG, player->plr->mo, NULL);
}

void C_DECL A_FirePlasma(player_t *player, pspdef_t *psp)
{
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash,
                 weaponInfo[player->readyWeapon][player->class].mode[0].flashState +
                 (P_Random() & 1));

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_SpawnMissile(MT_PLASMA, player->plr->mo, NULL);
}

/**
 * Sets a slope so a near miss is at aproximately the height of the
 * intended target.
 */
void P_BulletSlope(mobj_t *mo)
{
    angle_t     an;

    // See which target is to be aimed at.
    an = mo->angle;
    bulletSlope = P_AimLineAttack(mo, an, 16 * 64);
    if(!cfg.noAutoAim)
    {
        if(!lineTarget)
        {
            an += 1 << 26;
            bulletSlope = P_AimLineAttack(mo, an, 16 * 64);

            if(!lineTarget)
            {
                an -= 2 << 26;
                bulletSlope = P_AimLineAttack(mo, an, 16 * 64);
            }

            if(!lineTarget)
            {
                an += 2 << 26;
                bulletSlope =
                    tan(LOOKDIR2RAD(mo->dPlayer->lookDir)) / 1.2;
            }
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

    P_LineAttack(mo, angle, MISSILERANGE, bulletSlope, damage);
}

void C_DECL A_FirePistol(player_t *player, pspdef_t * psp)
{
    S_StartSound(SFX_PISTOL, player->plr->mo);

    P_MobjChangeState(player->plr->mo, PCLASS_INFO(player->class)->attackEndState);

    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash, weaponInfo[player->readyWeapon][player->class].mode[0].flashState);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_BulletSlope(player->plr->mo);
    P_GunShot(player->plr->mo, !player->refire);
}

void C_DECL A_FireShotgun(player_t *player, pspdef_t * psp)
{
    int     i;

    S_StartSound(SFX_SHOTGN, player->plr->mo);
    P_MobjChangeState(player->plr->mo, PCLASS_INFO(player->class)->attackEndState);

    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash, weaponInfo[player->readyWeapon][player->class].mode[0].flashState);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_BulletSlope(player->plr->mo);

    for(i = 0; i < 7; i++)
        P_GunShot(player->plr->mo, false);
}

void C_DECL A_FireShotgun2(player_t *player, pspdef_t * psp)
{
    int         i;
    angle_t     angle;
    int         damage;

    S_StartSound(SFX_DSHTGN, player->plr->mo);
    P_MobjChangeState(player->plr->mo, PCLASS_INFO(player->class)->attackEndState);

    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash, weaponInfo[player->readyWeapon][player->class].mode[0].flashState);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_BulletSlope(player->plr->mo);

    for(i = 0; i < 20; ++i)
    {
        damage = 5 * (P_Random() % 3 + 1);
        angle = player->plr->mo->angle;
        angle += (P_Random() - P_Random()) << 19;

        P_LineAttack(player->plr->mo, angle, MISSILERANGE,
                     bulletSlope + FIX2FLT((P_Random() - P_Random()) << 5), damage);
    }
}

void C_DECL A_FireCGun(player_t *player, pspdef_t * psp)
{
    S_StartSound(SFX_PISTOL, player->plr->mo);

    P_MobjChangeState(player->plr->mo, PCLASS_INFO(player->class)->attackEndState);

    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash,
                 weaponInfo[player->readyWeapon][player->class].mode[0].flashState + psp->state -
                 &states[S_CHAIN1]);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_BulletSlope(player->plr->mo);

    P_GunShot(player->plr->mo, !player->refire);
}

void C_DECL A_Light0(player_t *player, pspdef_t * psp)
{
    player->plr->extraLight = 0;
}

void C_DECL A_Light1(player_t *player, pspdef_t * psp)
{
    player->plr->extraLight = 1;
}

void C_DECL A_Light2(player_t *player, pspdef_t * psp)
{
    player->plr->extraLight = 2;
}

/**
 * Spawn a BFG explosion on every monster in view.
 */
void C_DECL A_BFGSpray(mobj_t *mo)
{
    int         i, j, damage;
    angle_t     an;

    // Offset angles from its attack angle.
    for(i = 0; i < 40; ++i)
    {
        an = mo->angle - ANG90 / 2 + ANG90 / 40 * i;

        // mo->target is the originator (player) of the missile
        P_AimLineAttack(mo->target, an, 16 * 64);

        if(!lineTarget)
            continue;

        P_SpawnMobj3f(MT_EXTRABFG,
                      lineTarget->pos[VX], lineTarget->pos[VY],
                      lineTarget->pos[VZ] + (lineTarget->height / 4));

        damage = 0;
        for(j = 0; j < 15; ++j)
            damage += (P_Random() & 7) + 1;

        P_DamageMobj(lineTarget, mo->target, mo->target, damage);
    }
}

void C_DECL A_BFGsound(player_t *player, pspdef_t *psp)
{
    S_StartSound(SFX_BFG, player->plr->mo);
}

/**
 * Called at start of level for each player.
 */
void P_SetupPsprites(player_t *player)
{
    int     i;

    // remove all psprites
    for(i = 0; i < NUMPSPRITES; i++)
        player->pSprites[i].state = NULL;

    // spawn the gun
    player->pendingWeapon = player->readyWeapon;
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

    psp = &player->pSprites[0];
    for(i = 0; i < NUMPSPRITES; i++, psp++)
    {
        // a null state means not active
        if((state = psp->state) != NULL)
        {
            // drop tic count and possibly change state

            // a -1 tic count never changes
            if(psp->tics != -1)
            {
                psp->tics--;
                if(!psp->tics)
                    P_SetPsprite(player, i, psp->state->nextState);
            }
        }
    }

    player->pSprites[ps_flash].pos[VX] = player->pSprites[ps_weapon].pos[VX];
    player->pSprites[ps_flash].pos[VY] = player->pSprites[ps_weapon].pos[VY];
}


//
// WOLFTC PLAYER WEAPONS
//

//
// P_WPistolShot Def
//
void P_WPistolShot(mobj_t *mo, boolean accurate)
{
    angle_t     angle;
    int         damage;

    damage = 9 * (P_Random() % 3 + 1);
    angle = mo->angle;

    if(!accurate)
        angle += (P_Random() - P_Random()) << 16;

    P_LineAttack(mo, angle, MISSILERANGE, bulletSlope, damage);
}

//
// P_WMachineGunShot
//
void P_WMachineGunShot(mobj_t *mo, boolean accurate)
{
    angle_t     angle;
    int         damage;

    damage = 9 * (P_Random() % 3 + 1);
    angle = mo->angle;

    if(!accurate)
        angle += (P_Random() - P_Random()) << 17;

    P_LineAttack(mo, angle, MISSILERANGE, bulletSlope, damage);
}

//
// P_WChainGunShot Def
//
void P_WChainGunShot(mobj_t *mo, boolean accurate)
{
    angle_t     angle;
    int         damage;

    damage = 12 * (P_Random() % 3 + 1);
    angle = mo->angle;

    if(!accurate)
        angle += (P_Random() - P_Random()) << 18;

    P_LineAttack(mo, angle, MISSILERANGE, bulletSlope, damage);
}

//
// P_RifleShot Def
//

void P_RifleShot(mobj_t *mo, boolean accurate)
{
    angle_t     angle;
    int         damage;

    damage = 16 * (P_Random() % 3 + 1);
    angle = mo->angle;

    if(!accurate)
        angle += (P_Random() - P_Random()) << 17;

    P_LineAttack(mo, angle, MISSILERANGE, bulletSlope, damage);
}

//
// P_RevolverShot Def
//

void P_RevolverShot(mobj_t *mo, boolean accurate)
{
    angle_t     angle;
    int         damage;

    damage = 50 * (P_Random() % 3 + 1);
    angle = mo->angle;

    if(!accurate)
        angle += (P_Random() - P_Random()) << 17;

    P_LineAttack(mo, angle, MISSILERANGE, bulletSlope, damage);
}

//
// P_MPistolShot Def
//

void P_MPistolShot(mobj_t *mo, boolean accurate)
{
    angle_t     angle;
    int         damage;

    damage = 7 * (P_Random() % 3 + 1);
    angle = mo->angle;

    if(!accurate)
        angle += (P_Random() - P_Random()) << 16;

    P_LineAttack(mo, angle, MISSILERANGE, bulletSlope, damage);
}

//
// P_MChaingunShot Def
//

void P_MChaingunShot(mobj_t *mo, boolean accurate)
{
    angle_t angle;
    int     damage;

    damage = 10 * (P_Random() % 3 + 1);
    angle = mo->angle;

    if(!accurate)
        angle += (P_Random() - P_Random()) << 20;

    P_LineAttack(mo, angle, MISSILERANGE, bulletSlope, damage);
}

//
// P_MRifleShot Def
//

void P_MRifleShot(mobj_t *mo, boolean accurate)
{
    angle_t angle;
    int     damage;

    damage = 14 * (P_Random() % 3 + 1);
    angle = mo->angle;

    if(!accurate)
        angle += (P_Random() - P_Random()) << 18;

    P_LineAttack(mo, angle, MISSILERANGE, bulletSlope, damage);
}

//
// P_RShotGunShot Def
//

void P_RShotgunShot(mobj_t *mo, boolean accurate)
{
    angle_t angle;
    int     damage;

    damage = 10 * (P_Random() % 3 + 1);
    angle = mo->angle;

    if(!accurate)
        angle += (P_Random() - P_Random()) << 18;

    P_LineAttack(mo, angle, MISSILERANGE, bulletSlope, damage);
}

//
// Knife (All)
//

void C_DECL A_Knife(player_t *player, pspdef_t *psp)
{
    angle_t     angle;
    int         damage;
    float       slope;

    P_ShotAmmo(player);
    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    damage = (P_Random() % 12 + 1) * 2;

    if(player->powers[PT_STRENGTH])
        damage *= 10;

    angle = player->plr->mo->angle;
    angle += (P_Random() - P_Random()) << 17;
    slope = P_AimLineAttack(player->plr->mo, angle, MELEERANGE);
    P_LineAttack(player->plr->mo, angle, MELEERANGE, slope, damage);

    // Turn to face target.
    if(lineTarget)
    {
        S_StartSound(SFX_PUNCH, player->plr->mo);
        player->plr->mo->angle =
            R_PointToAngle2(player->plr->mo->pos[VX], player->plr->mo->pos[VY],
                            lineTarget->pos[VX], lineTarget->pos[VY]);
        player->plr->flags |= DDPF_FIXANGLES;
    }
}

//
// Wolf3d Pistol
//

void C_DECL A_FireWPistol(player_t *player, pspdef_t *psp)
{
    S_StartSound(SFX_WPISTO, player->plr->mo);

    P_MobjChangeState(player->plr->mo, S_PLAY_ATK2);
    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash, weaponInfo[player->readyWeapon][player->class].mode[0].flashState);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_BulletSlope(player->plr->mo);
    P_WPistolShot(player->plr->mo, !player->refire);
}

//
// SODMP Pistol
//

void C_DECL A_FireSPistol(player_t *player, pspdef_t * psp)
{
    S_StartSound(SFX_LPISTO, player->plr->mo);

    P_MobjChangeState(player->plr->mo, S_PLAY_ATK2);
    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash, weaponInfo[player->readyWeapon][player->class].mode[0].flashState);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_BulletSlope(player->plr->mo);
    P_WPistolShot(player->plr->mo, !player->refire);
}

//
// Console Pistol
//

void C_DECL A_FireCPistol(player_t *player, pspdef_t * psp)
{
    S_StartSound(SFX_CPISTO, player->plr->mo);

    P_MobjChangeState(player->plr->mo, S_PLAY_ATK2);
    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash, weaponInfo[player->readyWeapon][player->class].mode[0].flashState);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_BulletSlope(player->plr->mo);
    P_WPistolShot(player->plr->mo, !player->refire);
}

//
// 3D0 Pistol
//

void C_DECL A_Fire3Pistol(player_t *player, pspdef_t * psp)
{
    S_StartSound(SFX_3PISTO, player->plr->mo);

    P_MobjChangeState(player->plr->mo, S_PLAY_ATK2);
    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash, weaponInfo[player->readyWeapon][player->class].mode[0].flashState);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_BulletSlope(player->plr->mo);
    P_WPistolShot(player->plr->mo, !player->refire);
}

//
// Uranus Pistol
//

void C_DECL A_FireUPistol(player_t *player, pspdef_t * psp)
{
    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash,
                 weaponInfo[player->readyWeapon][player->class].mode[0].flashState +
                 (P_Random() & 1));

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_SpawnMissile(MT_UPISTOLMISSILE, player->plr->mo, NULL);
}

//
// Uranus Pistol Trail
//

void C_DECL A_UranusPlayerPistolBlur(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_UPISTOLMISSILEBLUR);
    mo->angle = actor->angle;
    mo->target = actor->target;
}

//
// Multiplayer Pistol
//

void C_DECL A_FireMPistol(player_t *player, pspdef_t * psp)
{
    S_StartSound(SFX_WPISTO, player->plr->mo);

    P_MobjChangeState(player->plr->mo, S_PLAY_ATK2);
    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash, weaponInfo[player->readyWeapon][player->class].mode[0].flashState);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_BulletSlope(player->plr->mo);
    P_MPistolShot(player->plr->mo, !player->refire);
}

//
// Wolf3d Machine Gun
//

void C_DECL A_FireWMachineGun(player_t *player, pspdef_t * psp)
{
    S_StartSound(SFX_WMACHI, player->plr->mo);

    P_MobjChangeState(player->plr->mo, S_PLAY_ATK2);
    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash, weaponInfo[player->readyWeapon][player->class].mode[0].flashState);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_BulletSlope(player->plr->mo);
    P_WMachineGunShot(player->plr->mo, !player->refire);
}

//
// SODMP Machine Gun
//

void C_DECL A_FireSMachineGun(player_t *player, pspdef_t * psp)
{
    S_StartSound(SFX_LMACHI, player->plr->mo);

    P_MobjChangeState(player->plr->mo, S_PLAY_ATK2);
    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash, weaponInfo[player->readyWeapon][player->class].mode[0].flashState);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_BulletSlope(player->plr->mo);
    P_WMachineGunShot(player->plr->mo, !player->refire);
}

//
// OMS Machine Gun
//

void C_DECL A_FireOMachineGun(player_t *player, pspdef_t * psp)
{
    S_StartSound(SFX_OMACHI, player->plr->mo);

    P_MobjChangeState(player->plr->mo, S_PLAY_ATK2);
    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash, weaponInfo[player->readyWeapon][player->class].mode[0].flashState);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_BulletSlope(player->plr->mo);
    P_WMachineGunShot(player->plr->mo, !player->refire);
}

//
// Console Machine Gun
//

void C_DECL A_FireCMachineGun(player_t *player, pspdef_t * psp)
{
    S_StartSound(SFX_CPISTO, player->plr->mo);

    P_MobjChangeState(player->plr->mo, S_PLAY_ATK2);
    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash, weaponInfo[player->readyWeapon][player->class].mode[0].flashState);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_BulletSlope(player->plr->mo);
    P_WMachineGunShot(player->plr->mo, !player->refire);
}

//
// 3D0 Machine Gun
//

void C_DECL A_Fire3MachineGun(player_t *player, pspdef_t * psp)
{
    S_StartSound(SFX_3MACHI, player->plr->mo);

    P_MobjChangeState(player->plr->mo, S_PLAY_ATK2);
    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash, weaponInfo[player->readyWeapon][player->class].mode[0].flashState);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_BulletSlope(player->plr->mo);
    P_WMachineGunShot(player->plr->mo, !player->refire);
}

//
// Uranus Machine Gun
//

void C_DECL A_FireUMachineGun(player_t *player, pspdef_t * psp)
{
    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash,
                 weaponInfo[player->readyWeapon][player->class].mode[0].flashState +
                 (P_Random() & 1));

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_SpawnMissile(MT_UMACHINEGUNMISSILE, player->plr->mo, NULL);
}

//
// Uranus Machine Gun Trail
//

void C_DECL A_UranusPlayerMachinegunBlur(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_UMACHINEGUNMISSILEBLUR);
    mo->angle = actor->angle;
    mo->target = actor->target;
}

//
// Wolf3d Gattling Gun
//

void C_DECL A_FireWGattlingGun(player_t *player, pspdef_t * psp)
{
    S_StartSound(SFX_WCHGUN, player->plr->mo);

    /*if(!player->ammo[weaponInfo[player->readyWeapon].ammo])
       return; */

    P_MobjChangeState(player->plr->mo, S_PLAY_ATK2);
    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash, weaponInfo[player->readyWeapon][player->class].mode[0].flashState);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_BulletSlope(player->plr->mo);

    P_WChainGunShot(player->plr->mo, !player->refire);
}

//
// SODMP Gattling Gun
//

void C_DECL A_FireSGattlingGun(player_t *player, pspdef_t * psp)
{
    S_StartSound(SFX_LCHGUN, player->plr->mo);

    /*if(!player->ammo[weaponInfo[player->readyWeapon].ammo])
       return; */

    P_MobjChangeState(player->plr->mo, S_PLAY_ATK2);
    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash, weaponInfo[player->readyWeapon][player->class].mode[0].flashState);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_BulletSlope(player->plr->mo);

    P_WChainGunShot(player->plr->mo, !player->refire);
}

//
// Wolf3dAlpha Gattling Gun
//

void C_DECL A_FireAGattlingGun(player_t *player, pspdef_t * psp)
{
    S_StartSound(SFX_WMACHI, player->plr->mo);

    /*if(!player->ammo[weaponInfo[player->readyWeapon].ammo])
       return; */

    P_MobjChangeState(player->plr->mo, S_PLAY_ATK2);
    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash, weaponInfo[player->readyWeapon][player->class].mode[0].flashState);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_BulletSlope(player->plr->mo);

    P_WChainGunShot(player->plr->mo, !player->refire);
}

//
// OMS Gattling Gun
//

void C_DECL A_FireOGattlingGun(player_t *player, pspdef_t * psp)
{
    S_StartSound(SFX_ORIFLE, player->plr->mo);

    /*if(!player->ammo[weaponInfo[player->readyWeapon].ammo])
       return; */

    P_MobjChangeState(player->plr->mo, S_PLAY_ATK2);
    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash, weaponInfo[player->readyWeapon][player->class].mode[0].flashState);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_BulletSlope(player->plr->mo);

    P_WChainGunShot(player->plr->mo, !player->refire);
}

//
// SAEL Gattling Gun
//

void C_DECL A_FireEGattlingGun(player_t *player, pspdef_t * psp)
{
    S_StartSound(SFX_WCHGUN, player->plr->mo);

    /*if(!player->ammo[weaponInfo[player->readyWeapon].ammo])
       return; */

    P_MobjChangeState(player->plr->mo, S_PLAY_ATK2);
    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash,
                 weaponInfo[player->readyWeapon][player->class].mode[0].flashState + psp->state -
                 &states[S_SAELCHAIN1]);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_BulletSlope(player->plr->mo);

    P_WChainGunShot(player->plr->mo, !player->refire);
}

//
// Console Gattling Gun
//

void C_DECL A_FireCGattlingGun(player_t *player, pspdef_t * psp)
{
    S_StartSound(SFX_CPISTO, player->plr->mo);

    /*if(!player->ammo[weaponInfo[player->readyWeapon].ammo])
       return; */

    P_MobjChangeState(player->plr->mo, S_PLAY_ATK2);
    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash, weaponInfo[player->readyWeapon][player->class].mode[0].flashState);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_BulletSlope(player->plr->mo);

    P_WChainGunShot(player->plr->mo, !player->refire);
}

//
// 3D0 Gattling Gun
//

void C_DECL A_Fire3GattlingGun(player_t *player, pspdef_t * psp)
{
    S_StartSound(SFX_3CHGUN, player->plr->mo);

    /*if(!player->ammo[weaponInfo[player->readyWeapon].ammo])
       return; */

    P_MobjChangeState(player->plr->mo, S_PLAY_ATK2);
    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash, weaponInfo[player->readyWeapon][player->class].mode[0].flashState);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_BulletSlope(player->plr->mo);

    P_WChainGunShot(player->plr->mo, !player->refire);
}

//
// Uranus Gattling Gun
//

void C_DECL A_FireUGattlingGun(player_t *player, pspdef_t * psp)
{
    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash,
                 weaponInfo[player->readyWeapon][player->class].mode[0].flashState +
                 (P_Random() & 1));

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_SpawnMissile(MT_UCHAINGUNMISSILE, player->plr->mo, NULL);
}

//
// Uranus ChainGun Trail
//

void C_DECL A_UranusPlayerChaingunBlur(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_UCHAINGUNMISSILEBLUR);
    mo->angle = actor->angle;
    mo->target = actor->target;
}

//
// Multiplayer Gattling Gun
//

void C_DECL A_FireMGattlingGun(player_t *player, pspdef_t * psp)
{
    S_StartSound(SFX_WCHGUN, player->plr->mo);

    /*if(!player->ammo[weaponInfo[player->readyWeapon].ammo])
       return; */

    P_MobjChangeState(player->plr->mo, S_PLAY_ATK2);
    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash, weaponInfo[player->readyWeapon][player->class].mode[0].flashState);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_BulletSlope(player->plr->mo);

    P_MChaingunShot(player->plr->mo, !player->refire);
}

//
// OMS Rifle
//

void C_DECL A_FireORifle(player_t *player, pspdef_t * psp)
{
    S_StartSound(SFX_ORIFLE, player->plr->mo);

    P_MobjChangeState(player->plr->mo, S_PLAY_ATK2);
    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash, weaponInfo[player->readyWeapon][player->class].mode[0].flashState);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_BulletSlope(player->plr->mo);
    P_RifleShot(player->plr->mo, !player->refire);
}

//
// Multiplayer Rifle
//
void C_DECL A_FireMRifle(player_t *player, pspdef_t * psp)
{
    int     i;

    S_StartSound(SFX_ORIFLE, player->plr->mo);
    P_MobjChangeState(player->plr->mo, S_PLAY_ATK2);

    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash, weaponInfo[player->readyWeapon][player->class].mode[0].flashState);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_BulletSlope(player->plr->mo);

    for(i = 0; i < 3; i++)
        P_MRifleShot(player->plr->mo, false);
}

//
// OMS Revolver
//

void C_DECL A_FireORevolver(player_t *player, pspdef_t * psp)
{
    S_StartSound(SFX_OREVOL, player->plr->mo);

    P_MobjChangeState(player->plr->mo, S_PLAY_ATK2);
    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash, weaponInfo[player->readyWeapon][player->class].mode[0].flashState);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_BulletSlope(player->plr->mo);
    P_RevolverShot(player->plr->mo, !player->refire);
}

//
// ROM Shotgun
//

void C_DECL A_FireRShotgun(player_t *player, pspdef_t * psp)
{
    int     i;

    S_StartSound(SFX_ORIFLE, player->plr->mo);
    P_MobjChangeState(player->plr->mo, S_PLAY_ATK2);

    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash, weaponInfo[player->readyWeapon][player->class].mode[0].flashState);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_BulletSlope(player->plr->mo);

    for(i = 0; i < 3; i++)
        P_RShotgunShot(player->plr->mo, false);
}

//
// Multiplayer Syringe
//
void C_DECL A_FireMSyringe(player_t *player, pspdef_t * psp)
{
    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash,
                 weaponInfo[player->readyWeapon][player->class].mode[0].flashState +
                 (P_Random() & 1));

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_SpawnMissile(MT_MULTIPLAYERSYRINGE, player->plr->mo, NULL);
}

//
// Wolf3d Rocket Launcher
//
void C_DECL A_FireWMissile(player_t *player, pspdef_t * psp)
{
    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);
    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;
    P_SpawnMissile(MT_WROCKET, player->plr->mo, NULL);
}

//
// SODMP Rocket Launcher
//
void C_DECL A_FireLMissile(player_t *player, pspdef_t * psp)
{
    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);
    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;
    P_SpawnMissile(MT_LROCKET, player->plr->mo, NULL);
}

//
// Console Rocket Launcher
//
void C_DECL A_FireCMissile(player_t *player, pspdef_t * psp)
{
    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);
    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;
    P_SpawnMissile(MT_CROCKET, player->plr->mo, NULL);
}

//
// Wolf3d Flamethrower
//
void C_DECL A_FireFlame(player_t *player, pspdef_t * psp)
{
    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash,
                 weaponInfo[player->readyWeapon][player->class].mode[0].flashState +
                 (P_Random() & 1));

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_SpawnMissile(MT_FLAMETHROWERMISSILE, player->plr->mo, NULL);
}

//
// Console Flamethrower
//
void C_DECL A_FireCFlame(player_t *player, pspdef_t * psp)
{
    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash,
                 weaponInfo[player->readyWeapon][player->class].mode[0].flashState +
                 (P_Random() & 1));

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_SpawnMissile(MT_CONSOLEFLAMETHROWERMISSILE, player->plr->mo, NULL);
}

//
// 3D0 Flamethrower
//
void C_DECL A_Fire3Flame(player_t *player, pspdef_t * psp)
{
    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash,
                 weaponInfo[player->readyWeapon][player->class].mode[0].flashState +
                 (P_Random() & 1));

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_SpawnMissile(MT_3D0FLAMETHROWERMISSILE, player->plr->mo, NULL);
}

//
// Catacomb Player Missile 1 (Weak One)
//
void C_DECL A_FireCMissile1(player_t *player, pspdef_t * psp)
{
    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash,
                 weaponInfo[player->readyWeapon][player->class].mode[0].flashState +
                 (P_Random() & 1));

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_SpawnMissile(MT_CATAPMISSILE1, player->plr->mo, NULL);
}

//
// Catacomb Player Missile 2 (Strong One)
//
void C_DECL A_FireCMissile2(player_t *player, pspdef_t * psp)
{
    //player->ammo[weaponInfo[player->readyWeapon].ammo]--;
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash,
                 weaponInfo[player->readyWeapon][player->class].mode[0].flashState +
                 (P_Random() & 1));

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_SpawnMissile(MT_CATAPMISSILE2, player->plr->mo, NULL);
}

//
// Catacomb Player Missile 2 (Strong One No Ammo Used)
//
void C_DECL A_FireCMissile2NA(player_t *player, pspdef_t * psp)
{
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash,
                 weaponInfo[player->readyWeapon][player->class].mode[0].flashState +
                 (P_Random() & 1));

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_SpawnMissile(MT_CATAPMISSILE2, player->plr->mo, NULL);
}


//
// Catacomb Player Missile 3 (Strong One No Ammo Used)
//
void C_DECL A_FireCMissile3(player_t *player, pspdef_t * psp)
{
    P_ShotAmmo(player);
    P_SetPsprite(player, ps_flash,
                 weaponInfo[player->readyWeapon][player->class].mode[0].flashState +
                 (P_Random() & 1));
    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_SpawnMissile(MT_CATAPMISSILE2, player->plr->mo, NULL);
}

//
// Knife Thrust (All Except Console)
//
void C_DECL A_KnifeThrust(player_t *player, pspdef_t * psp)
{
    S_StartSound(SFX_KNFATK, player->plr->mo);
}

//
// Console Knife Thrust
//
void C_DECL A_CKnifeThrust(player_t *player, pspdef_t * psp)
{
    S_StartSound(SFX_CKNFAT, player->plr->mo);
}

//
// Machine Gun Reloading (All)
//
void C_DECL A_LoadMachineGun(player_t *player, pspdef_t * psp)
{
    S_StartSound(SFX_WMLOAD, player->plr->mo);
}
