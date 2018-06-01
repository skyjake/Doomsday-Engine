/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <yagisan@dengine.net>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
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
#include <stdio.h>
#include <string.h>

#include "common.h"

#include "d_net.h"
#include "player.h"
#include "p_map.h"
#include "p_tick.h"
#include "p_inventory.h"

// MACROS ------------------------------------------------------------------

#define LOWERSPEED      6
#define RAISESPEED      6
#define WEAPONTOP       32

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

//static float swing[2];
static float bulletSlope;

// CODE --------------------------------------------------------------------

void R_GetWeaponBob(int player, float* x, float* y)
{
    if(x)
    {
        *x = 1 + (cfg.common.bobWeapon * players[player].bob) *
            FIX2FLT(finecosine[(128 * mapTime) & FINEMASK]);
    }

    if(y)
    {
        *y = 32 + (cfg.common.bobWeapon * players[player].bob) *
            FIX2FLT(finesine[(128 * mapTime) & FINEMASK & (FINEANGLES / 2 - 1)]);
    }
}

void P_SetPsprite(player_t *player, int position, statenum_t stnum)
{
    pspdef_t           *psp;
    state_t            *state;

    psp = &player->pSprites[position];

    do
    {
        if(!stnum)
        {
            // Object removed itself.
            psp->state = NULL;
            break;
        }

        state = &STATES[stnum];
        psp->state = state;
        psp->tics = state->tics; // Could be 0.

        Player_NotifyPSpriteChange(player, position);

        if(state->misc[0])
        {
            // Coordinate set.
            psp->pos[VX] = (float) state->misc[0];
            psp->pos[VY] = (float) state->misc[1];
        }

        // Call action routine.
        // Modified handling.
        if(state->action)
        {
            // Custom parameters in the action function are passed to libdoomsday this way.
            P_SetCurrentActionState((int) stnum);

            state->action(player, psp);
            if(!psp->state)
                break;
        }

        stnum = psp->state->nextState;

    } while(!psp->tics);
    // An initial state of 0 could cycle through.
}

/**
 * Starts bringing the pending weapon up from the bottom of the screen.
 */
void P_BringUpWeapon(player_t* player)
{
    weapontype_t const oldPendingWeapon = player->pendingWeapon;

    weaponmodeinfo_t* wminfo = NULL;
    weapontype_t raiseWeapon;

    if(!player) return;

    if(player->plr->flags & DDPF_UNDEFINED_WEAPON)
    {
        // We'll do this when the server informs us about the client's current weapon.
        return;
    }

    raiseWeapon = player->pendingWeapon;
    if(raiseWeapon == WT_NOCHANGE)
        raiseWeapon = player->readyWeapon;

    player->pendingWeapon = WT_NOCHANGE;
    player->pSprites[ps_weapon].pos[VY] = WEAPONBOTTOM;

    if(!VALID_WEAPONTYPE(raiseWeapon))
    {
        return;
    }

    wminfo = WEAPON_INFO(raiseWeapon, player->class_, 0);

    App_Log(DE2_MAP_XVERBOSE, "P_BringUpWeapon: Player %i, pending weapon was %i, weapon pspr to %i",
            (int)(player - players), oldPendingWeapon, wminfo->states[WSN_UP]);

    if(wminfo->raiseSound)
        S_StartSoundEx(wminfo->raiseSound, player->plr->mo);

    P_SetPsprite(player, ps_weapon, wminfo->states[WSN_UP]);
}

void P_FireWeapon(player_t *player)
{
    statenum_t          newstate;

    if(!P_CheckAmmo(player))
        return;

    // Psprite state.
    player->plr->pSprites[0].state = DDPSP_FIRE;

    P_MobjChangeState(player->plr->mo, PCLASS_INFO(player->class_)->attackState);
    newstate = weaponInfo[player->readyWeapon][player->class_].mode[0].states[WSN_ATTACK];
    P_SetPsprite(player, ps_weapon, newstate);
    P_NoiseAlert(player->plr->mo, player->plr->mo);
}

/**
 * Player died, so put the weapon away.
 */
void P_DropWeapon(player_t *player)
{
    P_SetPsprite(player, ps_weapon, weaponInfo[player->readyWeapon][player->class_].mode[0].states[WSN_DOWN]);
}

/**
 * The player can fire the weapon or change to another weapon at this time.
 * Follows after getting weapon up, or after previous attack/fire sequence.
 */
void C_DECL A_WeaponReady(player_t* player, pspdef_t* psp)
{
    weaponmodeinfo_t*   wminfo;

    // Enable the pspr Y offset (might be disabled in A_Lower).
    DD_SetInteger(DD_WEAPON_OFFSET_SCALE_Y, 1000);

    // Get out of attack state.
    if(player->plr->mo->state == &STATES[PCLASS_INFO(player->class_)->attackState] ||
       player->plr->mo->state == &STATES[PCLASS_INFO(player->class_)->attackEndState])
    {
        P_MobjChangeState(player->plr->mo, PCLASS_INFO(player->class_)->normalState);
    }

    if(player->readyWeapon != WT_NOCHANGE)
    {
        wminfo = WEAPON_INFO(player->readyWeapon, player->class_, 0);

        // A weaponready sound?
        if(psp->state == &STATES[wminfo->states[WSN_READY]] && wminfo->readySound)
            S_StartSound(wminfo->readySound, player->plr->mo);

        // Check for change. If player is dead, put the weapon away.
        if(player->pendingWeapon != WT_NOCHANGE || !player->health)
        {   //  (pending weapon should allready be validated)
            P_SetPsprite(player, ps_weapon, wminfo->states[WSN_DOWN]);
            return;
        }
    }

    // Check for autofire.
    if(player->brain.attack)
    {
        wminfo = WEAPON_INFO(player->readyWeapon, player->class_, 0);

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
    R_GetWeaponBob(player - players, &psp->pos[0], &psp->pos[1]);

    // Psprite state.
    player->plr->pSprites[0].state = DDPSP_BOBBING;
}

/**
 * The player can re-fire the weapon without lowering it entirely.
 */
void C_DECL A_ReFire(player_t *player, pspdef_t *psp)
{
    // Check for fire (if a weaponchange is pending, let it go through
    // instead).
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
}

/**
 * Lowers current weapon, and changes weapon at bottom.
 */
void C_DECL A_Lower(player_t *player, pspdef_t *psp)
{
    psp->pos[VY] += LOWERSPEED;

    // Psprite state.
    player->plr->pSprites[0].state = DDPSP_DOWN;

    // Should we disable the lowering?
    if(!cfg.bobWeaponLower ||
       weaponInfo[player->readyWeapon][player->class_].mode[0].staticSwitch)
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

        // Don't bring weapon back up.
        return;
    }

    if(player->readyWeapon == WT_SIXTH) // jd64
        P_SetPsprite(player, ps_flash, S_NULL);

    // The old weapon has been lowered off the screen, so change the weapon
    // and start raising it.
    if(!player->health)
    {
        // Player is dead, so keep the weapon off screen.
        P_SetPsprite(player, ps_weapon, S_NULL);
        return;
    }

    player->readyWeapon = player->pendingWeapon;
    player->update |= PSF_READY_WEAPON;

    // Should we suddenly lower the weapon?
    if(cfg.bobWeaponLower && !weaponInfo[player->readyWeapon][player->class_].mode[0].staticSwitch)
    {
        DD_SetInteger(DD_WEAPON_OFFSET_SCALE_Y, 1000);
    }

    P_BringUpWeapon(player);
}

void C_DECL A_Raise(player_t *player, pspdef_t *psp)
{
    statenum_t          newstate;

    // Psprite state.
    player->plr->pSprites[0].state = DDPSP_UP;

    // Should we disable the lowering?
    if(!cfg.bobWeaponLower || weaponInfo[player->readyWeapon][player->class_].mode[0].staticSwitch)
    {
        DD_SetInteger(DD_WEAPON_OFFSET_SCALE_Y, 0);
    }

    P_SetPsprite(player, ps_flash, S_NULL);
    psp->pos[VY] -= RAISESPEED;

    if(psp->pos[VY] > WEAPONTOP)
        return;

    // Enable the pspr Y offset once again.
    DD_SetInteger(DD_WEAPON_OFFSET_SCALE_Y, 1000);

    psp->pos[VY] = WEAPONTOP;

    // The weapon has been raised all the way, so change to the ready state.
    newstate = weaponInfo[player->readyWeapon][player->class_].mode[0].states[WSN_READY];

    P_SetPsprite(player, ps_weapon, newstate);
}

void C_DECL A_PlasmaShock(player_t* pl, pspdef_t* psp)
{
    S_StartSound(SFX_PSIDL, pl->plr->mo);
    P_SetPsprite(pl, ps_flash, S_PLASMASHOCK1);
}

void C_DECL A_GunFlash(player_t *player, pspdef_t *psp)
{
    P_MobjChangeState(player->plr->mo, PCLASS_INFO(player->class_)->attackEndState);
    P_SetPsprite(player, ps_flash, weaponInfo[player->readyWeapon][player->class_].mode[0].states[WSN_FLASH]);
}

void C_DECL A_Punch(player_t *player, pspdef_t *psp)
{
    angle_t angle;
    int damage;
    float slope;

    P_ShotAmmo(player);
    player->update |= PSF_AMMO;

    if(IS_CLIENT) return;

    damage = (P_Random() % 10 + 1) * 2;
    if(player->powers[PT_STRENGTH])
        damage *= 10;

    angle = player->plr->mo->angle;
    angle += (P_Random() - P_Random()) << 18;
    slope = P_AimLineAttack(player->plr->mo, angle, PLRMELEERANGE);
    P_LineAttack(player->plr->mo, angle, PLRMELEERANGE, slope, damage, MT_PUFF);

    // Turn to face target.
    if(lineTarget)
    {
        S_StartSound(SFX_PUNCH, player->plr->mo);

        player->plr->mo->angle = M_PointToAngle2(player->plr->mo->origin, lineTarget->origin);
        player->plr->flags |= DDPF_FIXANGLES;
    }
}

void C_DECL A_Saw(player_t *player, pspdef_t *psp)
{
    angle_t angle;
    int damage;
    float slope;

    DE_UNUSED(psp);

    P_ShotAmmo(player);
    player->update |= PSF_AMMO;

    if(IS_CLIENT) return;

    damage = (float) (P_Random() % 10 + 1) * 2;
    angle = player->plr->mo->angle;
    angle += (P_Random() - P_Random()) << 18;

    // Use meleerange + 1 so the puff doesn't skip the flash.
    slope = P_AimLineAttack(player->plr->mo, angle, PLRMELEERANGE + 1);
    P_LineAttack(player->plr->mo, angle, PLRMELEERANGE + 1, slope, damage, MT_PUFF);

    if(!lineTarget)
    {
        S_StartSound(SFX_SAWFUL, player->plr->mo);
        return;
    }

    S_StartSound(SFX_SAWHIT, player->plr->mo);

    // Turn to face target.
    angle = M_PointToAngle2(player->plr->mo->origin, lineTarget->origin);
    if(angle - player->plr->mo->angle > ANG180)
    {
        if(angle - player->plr->mo->angle < -ANG90 / 32) // jd64 was "/ 20"
            player->plr->mo->angle = angle + ANG90 / 32; // jd64 was "/ 21"
        else
            player->plr->mo->angle -= ANG90 / 20;
    }
    else
    {
        if(angle - player->plr->mo->angle > ANG90 / 32) // jd64 was "/ 20"
            player->plr->mo->angle = angle - ANG90 / 32; // jd64 was "/ 21"
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

    // jd64 >
    if(cfg.weaponRecoil)
    {
        angle_t         angle = player->plr->mo->angle + ANG180;
        uint            an = angle >> ANGLETOFINESHIFT;

        player->plr->mo->mom[MX] += 4 * FIX2FLT(finecosine[an]);
        player->plr->mo->mom[MY] += 4 * FIX2FLT(finesine[an]);
    }
    // < d64tc
}

void C_DECL A_FireBFG(player_t* player, pspdef_t* psp)
{
    P_ShotAmmo(player);
    player->update |= PSF_AMMO;

    if(IS_CLIENT)
        return;

    P_SpawnMissile(MT_BFG, player->plr->mo, NULL);
}

void C_DECL A_FirePlasma(player_t* player, pspdef_t* psp)
{
    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash,
                 weaponInfo[player->readyWeapon][player->class_].mode[0].states[WSN_FLASH] +
                 (P_Random() & 1));

    //P_SetPsprite(player, ps_flash, S_NULL); // jd64 wha?

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_SpawnMissile(MT_PLASMA, player->plr->mo, NULL);
}

/**
 * d64tc
 */
void C_DECL A_FireSingleLaser(player_t *player, pspdef_t *psp)
{
    int                 plrNum = player - players;
    mobj_t*             pmo;
    short               laserPower;

    P_ShotAmmo(player);
    P_SetPsprite(player, ps_flash,
                 weaponInfo[player->readyWeapon][player->class_].mode[0].states[WSN_FLASH]);
    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    pmo = player->plr->mo;

    laserPower = 0;
    if(P_InventoryCount(plrNum, IIT_DEMONKEY1))
        laserPower++;
    if(P_InventoryCount(plrNum, IIT_DEMONKEY2))
        laserPower++;
    if(P_InventoryCount(plrNum, IIT_DEMONKEY3))
        laserPower++;

    switch(laserPower)
    {
    case 0:
        P_SpawnMissile(MT_LASERSHOTWEAK, player->plr->mo, NULL);
        break;

    case 1:
        P_SpawnMissile(MT_LASERSHOT, player->plr->mo, NULL);
        break;

    case 2:
        P_SPMAngle(MT_LASERSHOT, pmo, pmo->angle - (ANG45 / 8));
        P_SPMAngle(MT_LASERSHOT, pmo, pmo->angle + (ANG45 / 8));
        break;

    case 3:
        P_SpawnMissile(MT_LASERSHOT, pmo, NULL);
        P_SPMAngle(MT_LASERSHOT, pmo, pmo->angle - (ANG45 / 6));
        P_SPMAngle(MT_LASERSHOT, pmo, pmo->angle + (ANG45 / 6));
        break;
    }
}

/**
 * d64tc
 */
static void fireDoubleLaser(player_t* player, pspdef_t* psp,
                            angle_t angleDelta)
{
    mobj_t*             pmo;

    P_ShotAmmo(player);

    pmo = player->plr->mo;
    player->update |= PSF_AMMO;

    if(IS_CLIENT)
        return;

    P_SpawnMissile(MT_LASERSHOT, pmo, NULL);
    P_SPMAngle(MT_LASERSHOT, pmo, pmo->angle - angleDelta);
    P_SPMAngle(MT_LASERSHOT, pmo, pmo->angle + angleDelta);
}

/**
 * d64tc
 */
void C_DECL A_FireDoubleLaser(player_t *player, pspdef_t *psp)
{
    fireDoubleLaser(player, psp, ANG45 / 8);
}

/**
 * d64tc
 */
void C_DECL A_FireDoubleLaser1(player_t *player, pspdef_t *psp)
{
    fireDoubleLaser(player, psp, ANG45 / 4);
}

/**
 * d64tc
 */
void C_DECL A_FireDoubleLaser2(player_t *player, pspdef_t *psp)
{
    fireDoubleLaser(player, psp, ANG45 / 3);
}

/**
 * Sets a slope so a near miss is at aproximately the height of the
 * intended target.
 */
void P_BulletSlope(mobj_t *mo)
{
    angle_t             angle;

    // See which target is to be aimed at.
    angle = mo->angle;
    bulletSlope = P_AimLineAttack(mo, angle, 16 * 64);
    if(!cfg.common.noAutoAim)
    {
        if(!lineTarget)
        {
            angle += 1 << 26;
            bulletSlope = P_AimLineAttack(mo, angle, 16 * 64);

            if(!lineTarget)
            {
                angle -= 2 << 26;
                bulletSlope = P_AimLineAttack(mo, angle, 16 * 64);
            }

            if(!lineTarget)
            {
                angle += 2 << 26;
                bulletSlope =
                    tan(LOOKDIR2RAD(mo->dPlayer->lookDir)) / 1.2;
            }
        }
    }
}

void P_GunShot(mobj_t *mo, dd_bool accurate)
{
    angle_t angle;
    int damage;

    damage = 5 * (P_Random() % 3 + 1);

    angle  = mo->angle;
    if(!accurate)
    {
        angle += (P_Random() - P_Random()) << 18;
    }

    P_LineAttack(mo, angle, MISSILERANGE, bulletSlope, damage, MT_PUFF);
}

void C_DECL A_FirePistol(player_t *player, pspdef_t *psp)
{
    S_StartSound(SFX_PISTOL, player->plr->mo);

    P_MobjChangeState(player->plr->mo, PCLASS_INFO(player->class_)->attackEndState);

    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash, weaponInfo[player->readyWeapon][player->class_].mode[0].states[WSN_FLASH]);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_BulletSlope(player->plr->mo);
    P_GunShot(player->plr->mo, !player->refire);
}

void C_DECL A_FireShotgun(player_t *player, pspdef_t *psp)
{
    int                 i;

    S_StartSound(SFX_SHOTGN, player->plr->mo);
    P_MobjChangeState(player->plr->mo, PCLASS_INFO(player->class_)->attackEndState);

    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash, weaponInfo[player->readyWeapon][player->class_].mode[0].states[WSN_FLASH]);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_BulletSlope(player->plr->mo);

    for(i = 0; i < 7; ++i)
        P_GunShot(player->plr->mo, false);
}

void C_DECL A_FireShotgun2(player_t *player, pspdef_t *psp)
{
    int i;
    angle_t angle;
    int damage;

    S_StartSound(SFX_DSHTGN, player->plr->mo);
    P_MobjChangeState(player->plr->mo, PCLASS_INFO(player->class_)->attackEndState);

    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash, weaponInfo[player->readyWeapon][player->class_].mode[0].states[WSN_FLASH]);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    P_BulletSlope(player->plr->mo);

    // jd64 >
    if(cfg.weaponRecoil)
    {
        uint an;

        player->plr->mo->angle += ANG90/90;
        an = (player->plr->mo->angle + ANG180) >> ANGLETOFINESHIFT;

        player->plr->mo->mom[MX] += 4 * FIX2FLT(finecosine[an]);
        player->plr->mo->mom[MY] += 4 * FIX2FLT(finesine[an]);
    }
    // < d64tc

    for(i = 0; i < 20; ++i)
    {
        damage = 5 * (P_Random() % 3 + 1);
        angle = player->plr->mo->angle;
        angle += (P_Random() - P_Random()) << 19;

        P_LineAttack(player->plr->mo, angle, MISSILERANGE,
                     bulletSlope + FIX2FLT((P_Random() - P_Random()) << 5),
                     damage, MT_PUFF);
    }
}

void C_DECL A_OpenShotgun2(player_t *player, pspdef_t *psp)
{
    S_StartSound(SFX_DBOPN, player->plr->mo);
}

void C_DECL A_LoadShotgun2(player_t *player, pspdef_t *psp)
{
    S_StartSound(SFX_DBLOAD, player->plr->mo);
}

void C_DECL A_FireCGun(player_t *player, pspdef_t *psp)
{
    S_StartSound(SFX_PISTOL, player->plr->mo);

    P_MobjChangeState(player->plr->mo, PCLASS_INFO(player->class_)->attackEndState);

    P_ShotAmmo(player);

    P_SetPsprite(player, ps_flash,
                 weaponInfo[player->readyWeapon][player->class_].mode[0].states[WSN_FLASH] + psp->state -
                 &STATES[S_CHAIN1]);

    player->update |= PSF_AMMO;
    if(IS_CLIENT)
        return;

    psp->pos[VY] = WEAPONTOP + FIX2FLT((P_Random() & 8) - 2); // jd64

    P_BulletSlope(player->plr->mo);

    // jd64 >
    if(cfg.weaponRecoil)
    {   // Nice little recoil effect.
        player->plr->mo->angle += ANG90/256;
    }
    // < d64tc

    P_GunShot(player->plr->mo, !player->refire);
}

void C_DECL A_Light0(player_t *player, pspdef_t *psp)
{
    player->plr->extraLight = 0;
}

void C_DECL A_Light1(player_t *player, pspdef_t *psp)
{
    player->plr->extraLight = 1;
}

void C_DECL A_Light2(player_t *player, pspdef_t *psp)
{
    player->plr->extraLight = 2;
}

/**
 * Spawn a BFG explosion on every monster in view.
 */
void C_DECL A_BFGSpray(mobj_t* mo)
{
    int i, j, damage;
    angle_t angle;

    // Offset angles from its attack angle.
    for(i = 0; i < 40; ++i)
    {
        angle = mo->angle - ANG90 / 2 + ANG90 / 40 * i;

        // mo->target is the originator (player) of the missile.
        P_AimLineAttack(mo->target, angle, 16 * 64);

        if(!lineTarget) continue;

        P_SpawnMobjXYZ(MT_EXTRABFG, lineTarget->origin[VX], lineTarget->origin[VY],
                                    lineTarget->origin[VZ] + lineTarget->height / 4,
                                    angle + ANG180, 0);

        damage = 0;
        for(j = 0; j < 15; ++j)
        {
            damage += (P_Random() & 7) + 1;
        }

        P_DamageMobj(lineTarget, mo->target, mo->target, damage, false);
    }
}

void C_DECL A_BFGsound(player_t* player, pspdef_t* psp)
{
    S_StartSound(SFX_BFG, player->plr->mo);
}

/**
 * Called at start of level for each player.
 */
void P_SetupPsprites(player_t* player)
{
    int i;

    // Remove all psprites.
    for(i = 0; i < NUMPSPRITES; ++i)
        player->pSprites[i].state = NULL;

    // Spawn the gun.
    if(player->pendingWeapon == WT_NOCHANGE)
        player->pendingWeapon = player->readyWeapon;
    P_BringUpWeapon(player);
}

/**
 * Called every tic by player thinking routine.
 */
void P_MovePsprites(player_t *player)
{
    int                 i;
    pspdef_t           *psp;
    state_t            *state;

    psp = &player->pSprites[0];
    for(i = 0; i < NUMPSPRITES; ++i, psp++)
    {
        // A null state means not active.
        state = psp->state;
        if(state)
        {
            // Decrease tic count and possibly change state.
            // A -1 tic count never changes.
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
