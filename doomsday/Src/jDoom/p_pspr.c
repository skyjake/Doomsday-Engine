// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log$
// Revision 1.4  2003/07/03 21:23:24  skyjake
// D_Get/H_GetString/H2_GetString => G_Get
//
// Revision 1.3  2003/07/01 23:58:56  skyjake
// Static weapon switching (value, weaponinfo)
//
// Revision 1.2  2003/06/27 20:23:45  skyjake
// Added cvar to disable weapon Y offset in A_Lower/A_Raise (view-bob-weapon-switch-lower)
//
// Revision 1.1  2003/02/26 19:21:57  skyjake
// Initial checkin
//
// Revision 1.1  2002/09/29 01:11:47  Jaakko
// Added Doomsday sources
//
//
// DESCRIPTION:
//	Weapon sprite animation, weapon objects.
//	Action functions for weapons.
//
//-----------------------------------------------------------------------------

// Sumtin 'ere screws up poor ol' MSVC's head...
#pragma optimize("g",off)

static const char
rcsid[] = "$Id$";

#include "doomdef.h"
#include "d_config.h"
#include "d_event.h"
#include "d_netjd.h"

#include "m_random.h"
#include "p_local.h"
#include "s_sound.h"

// State.
#include "doomstat.h"

#include "p_pspr.h"

#define LOWERSPEED		FRACUNIT*6
#define RAISESPEED		FRACUNIT*6

#define WEAPONBOTTOM	128*FRACUNIT
#define WEAPONTOP		32*FRACUNIT


// plasma cells for a bfg attack
#define BFGCELLS		40		


void P_ShotAmmo(player_t *player)
{
	weaponinfo_t *win = weaponinfo + player->readyweapon;

	// If the weapon takes no ammo, do nothing.
	if(win->ammo == am_noammo) return;
	player->ammo[win->ammo] -= win->pershot;
	// Don't let it fall below zero.
	if(player->ammo[win->ammo] < 0) player->ammo[win->ammo] = 0;
}

//
// P_SetPsprite
//
void
P_SetPsprite
( player_t*	player,
 int		position,
 statenum_t	stnum ) 
{
    pspdef_t*	psp;
    state_t*	state;
	
    psp = &player->psprites[position];
	
    do
    {
		if (!stnum)
		{
			// object removed itself
			psp->state = NULL;
			break;	
		}
		
		state = &states[stnum];
		psp->state = state;
		psp->tics = state->tics;	// could be 0
		
		if (state->misc[0])
		{
			// coordinate set
			psp->sx = state->misc[0] << FRACBITS;
			psp->sy = state->misc[1] << FRACBITS;
		}
		
		// Call action routine.
		// Modified handling.
		if (state->action)
		{
			state->action(player, psp);
			if (!psp->state)
				break;
		}
		
		stnum = psp->state->nextstate;
		
    } while (!psp->tics);
    // an initial state of 0 could cycle through
}



//
// P_CalcSwing
//	
fixed_t		swingx;
fixed_t		swingy;

void P_CalcSwing (player_t*	player)
{
    fixed_t	swing;
    int		angle;
	
    // OPTIMIZE: tablify this.
    // A LUT would allow for different modes,
    //  and add flexibility.

    swing = player->bob;

    angle = (FINEANGLES/70*leveltime)&FINEMASK;
    swingx = FixedMul ( swing, finesine[angle]);

    angle = (FINEANGLES/70*leveltime+FINEANGLES/2)&FINEMASK;
    swingy = -FixedMul ( swingx, finesine[angle]);
}



//
// P_BringUpWeapon
// Starts bringing the pending weapon up
// from the bottom of the screen.
// Uses player
//
void P_BringUpWeapon (player_t* player)
{
    statenum_t	newstate;
	
    if (player->pendingweapon == wp_nochange)
		player->pendingweapon = player->readyweapon;
	
    if (player->pendingweapon == wp_chainsaw)
		S_StartSound(sfx_sawup, player->plr->mo);
	
    newstate = weaponinfo[player->pendingweapon].upstate;
	
    player->pendingweapon = wp_nochange;
    player->psprites[ps_weapon].sy = WEAPONBOTTOM;
	
    P_SetPsprite (player, ps_weapon, newstate);
}

//
// P_CheckAmmo
// Returns true if there is enough ammo to shoot.
// If not, selects the next weapon to use.
//
boolean P_CheckAmmo (player_t* player)
{
    ammotype_t		ammo;
    int			count;
	
    ammo = weaponinfo[player->readyweapon].ammo;
	
    // Minimal amount for one shot varies.
    /*if (player->readyweapon == wp_bfg)
		count = BFGCELLS;
    else if (player->readyweapon == wp_supershotgun)
		count = 2;	// Double barrel.
    else
		count = 1;	// Regular.*/
	count = weaponinfo[player->readyweapon].pershot;
	
    // Some do not need ammunition anyway.
    // Return if current ammunition sufficient.
    if (ammo == am_noammo || player->ammo[ammo] >= count)
		return true;
	
    // Out of ammo, pick a weapon to change to.
    // Preferences are set here.
    do
    {
		if (player->weaponowned[wp_plasma]
			&& player->ammo[am_cell] >= weaponinfo[wp_plasma].pershot
			&& (gamemode != shareware) )
		{
			player->pendingweapon = wp_plasma;
		}
		else if (player->weaponowned[wp_supershotgun] 
			&& player->ammo[am_shell] > weaponinfo[wp_supershotgun].pershot
			&& (gamemode == commercial) )
		{
			player->pendingweapon = wp_supershotgun;
		}
		else if (player->weaponowned[wp_chaingun]
			&& player->ammo[am_clip] >= weaponinfo[wp_chaingun].pershot)
		{
			player->pendingweapon = wp_chaingun;
		}
		else if (player->weaponowned[wp_shotgun]
			&& player->ammo[am_shell] >= weaponinfo[wp_shotgun].pershot)
		{
			player->pendingweapon = wp_shotgun;
		}
		else if (player->ammo[am_clip] >= weaponinfo[wp_pistol].pershot)
		{
			player->pendingweapon = wp_pistol;
		}
		else if (player->weaponowned[wp_chainsaw])
		{
			player->pendingweapon = wp_chainsaw;
		}
		else if (player->weaponowned[wp_missile]
			&& player->ammo[am_misl] >= weaponinfo[wp_missile].pershot)
		{
			player->pendingweapon = wp_missile;
		}
		else if (player->weaponowned[wp_bfg]
			&& player->ammo[am_cell] > weaponinfo[wp_bfg].pershot
			&& (gamemode != shareware) )
		{
			player->pendingweapon = wp_bfg;
		}
		else
		{
			// If everything fails.
			player->pendingweapon = wp_fist;
		}
		player->update |= PSF_PENDING_WEAPON | PSF_READY_WEAPON;		
    } while (player->pendingweapon == wp_nochange);
	
    // Now set appropriate weapon overlay.
    P_SetPsprite (player,
		ps_weapon,
		weaponinfo[player->readyweapon].downstate);
	
    return false;	
}


//
// P_FireWeapon.
//
void P_FireWeapon (player_t* player)
{
    statenum_t	newstate;

	if (!P_CheckAmmo (player))
		return;
	
	// Psprite state.
	player->plr->psprites[0].state = DDPSP_FIRE;

    P_SetMobjState (player->plr->mo, S_PLAY_ATK1);
    newstate = weaponinfo[player->readyweapon].atkstate;
    P_SetPsprite (player, ps_weapon, newstate);
	NetSv_PSpriteChange(player-players, newstate);	
    P_NoiseAlert (player->plr->mo, player->plr->mo);
}



//
// P_DropWeapon
// Player died, so put the weapon away.
//
void P_DropWeapon (player_t* player)
{
    P_SetPsprite (player,
		  ps_weapon,
		  weaponinfo[player->readyweapon].downstate);
}



//
// A_WeaponReady
// The player can fire the weapon
// or change to another weapon at this time.
// Follows after getting weapon up,
// or after previous attack/fire sequence.
//
void C_DECL
A_WeaponReady
( player_t*	player,
 pspdef_t*	psp )
{	
    statenum_t	newstate;
    
	// Enable the pspr Y offset (might be disabled in A_Lower).
	DD_SetInteger(DD_WEAPON_OFFSET_SCALE_Y, 1000);

    // get out of attack state
    if (player->plr->mo->state == &states[S_PLAY_ATK1]
		|| player->plr->mo->state == &states[S_PLAY_ATK2] )
    {
		P_SetMobjState (player->plr->mo, S_PLAY);
    }
    
    if (player->readyweapon == wp_chainsaw
		&& psp->state == &states[S_SAW])
    {
		S_StartSound(sfx_sawidl, player->plr->mo);
    }
    
    // check for change
    //  if player is dead, put the weapon away
    if (player->pendingweapon != wp_nochange || !player->health)
    {
		// change weapon
		//  (pending weapon should allready be validated)
		newstate = weaponinfo[player->readyweapon].downstate;
		P_SetPsprite (player, ps_weapon, newstate);
		return;	
    }
    
    // check for fire
    //  the missile launcher and bfg do not auto fire
    if (player->cmd.buttons & BT_ATTACK)
    {
		if ( !player->attackdown
			|| (player->readyweapon != wp_missile
			&& player->readyweapon != wp_bfg) )
		{
			player->attackdown = true;
			P_FireWeapon (player);		
			return;
		}
    }
    else
		player->attackdown = false;
    
    // Bob the weapon based on movement speed.
	psp->sx = (int) G_Get(DD_PSPRITE_BOB_X);
	psp->sy = (int) G_Get(DD_PSPRITE_BOB_Y);

	// Psprite state.
	player->plr->psprites[0].state = DDPSP_BOBBING;
}



//
// A_ReFire
// The player can re-fire the weapon
// without lowering it entirely.
//
void C_DECL A_ReFire( player_t*	player, pspdef_t*	psp)
{
    // check for fire
    //  (if a weaponchange is pending, let it go through instead)
    if ((player->cmd.buttons & BT_ATTACK) 
		&& player->pendingweapon == wp_nochange
		&& player->health)
    {
		//if(IS_CLIENT) gi.conprintf( "client refire: atk=%i\n", player->cmd.buttons & BT_ATTACK);
		player->refire++;
		P_FireWeapon (player);
    }
    else
    {
		player->refire = 0;
		P_CheckAmmo (player);
    }
}


void C_DECL
A_CheckReload
( player_t*	player,
  pspdef_t*	psp )
{
    P_CheckAmmo (player);
#if 0
    if (player->ammo[am_shell]<2)
	P_SetPsprite (player, ps_weapon, S_DSNR1);
#endif
}



//
// A_Lower
// Lowers current weapon,
//  and changes weapon at bottom.
//
void C_DECL
A_Lower
( player_t*	player,
 pspdef_t*	psp )
{	
	psp->sy += LOWERSPEED;
	
	// Psprite state.
	player->plr->psprites[0].state = DDPSP_DOWN;

	// Should we disable the lowering?
	if(!cfg.bobWeaponLower 
		|| weaponinfo[player->readyweapon].static_switch)
	{
		DD_SetInteger(DD_WEAPON_OFFSET_SCALE_Y, 0);
	}

    // Is already down.
    if (psp->sy < WEAPONBOTTOM )
		return;
	
    // Player is dead.
    if (player->playerstate == PST_DEAD)
    {
		psp->sy = WEAPONBOTTOM;
		
		// don't bring weapon back up
		return;		
    }
    
    // The old weapon has been lowered off the screen,
    // so change the weapon and start raising it
    if (!player->health)
    {
		// Player is dead, so keep the weapon off screen.
		P_SetPsprite (player,  ps_weapon, S_NULL);
		return;	
    }
    
	player->readyweapon = player->pendingweapon; 
	player->update |= PSF_READY_WEAPON;
	
	// Should we suddenly lower the weapon?
	if(cfg.bobWeaponLower 
		&& !weaponinfo[player->readyweapon].static_switch)
	{
		DD_SetInteger(DD_WEAPON_OFFSET_SCALE_Y, 1000);
	}

    P_BringUpWeapon (player);
}


//
// A_Raise
//
void C_DECL
A_Raise
( player_t*	player,
 pspdef_t*	psp )
{
    statenum_t	newstate;
	
	// Psprite state.
	player->plr->psprites[0].state = DDPSP_UP;

	// Should we disable the lowering?
	if(!cfg.bobWeaponLower
		|| weaponinfo[player->readyweapon].static_switch)
	{
		DD_SetInteger(DD_WEAPON_OFFSET_SCALE_Y, 0);
	}

	psp->sy -= RAISESPEED;
	
    if (psp->sy > WEAPONTOP )
		return;
    
	// Enable the pspr Y offset once again.
	DD_SetInteger(DD_WEAPON_OFFSET_SCALE_Y, 1000);

    psp->sy = WEAPONTOP;
    
    // The weapon has been raised all the way,
    //  so change to the ready state.
    newstate = weaponinfo[player->readyweapon].readystate;
	
    P_SetPsprite (player, ps_weapon, newstate);
}



//
// A_GunFlash
//
void C_DECL
A_GunFlash
( player_t*	player,
  pspdef_t*	psp ) 
{
    P_SetMobjState (player->plr->mo, S_PLAY_ATK2);
    P_SetPsprite (player,ps_flash,weaponinfo[player->readyweapon].flashstate);
}



//
// WEAPON ATTACKS
//


//
// A_Punch
//
void C_DECL
A_Punch
( player_t*	player,
 pspdef_t*	psp ) 
{
    angle_t	angle;
    int		damage;
    int		slope;
	
	if(IS_CLIENT) return;

    damage = (P_Random ()%10+1)<<1;
	
    if (player->powers[pw_strength])	
		damage *= 10;
	
    angle = player->plr->mo->angle;
    angle += (P_Random()-P_Random())<<18;
    slope = P_AimLineAttack (player->plr->mo, angle, MELEERANGE);
    P_LineAttack (player->plr->mo, angle, MELEERANGE, slope, damage);
	
    // turn to face target
    if (linetarget)
    {
		//S_StartSound (player->plr->mo, sfx_punch, player);
		S_StartSound(sfx_punch, player->plr->mo);
		player->plr->mo->angle = R_PointToAngle2 (player->plr->mo->x,
			player->plr->mo->y,
			linetarget->x,
			linetarget->y);
		player->plr->flags |= DDPF_FIXANGLES;
    }
}


//
// A_Saw
//
void C_DECL
A_Saw
( player_t*	player,
 pspdef_t*	psp ) 
{
    angle_t	angle;
    int		damage;
    int		slope;
	
	if(IS_CLIENT) return;

    damage = 2*(P_Random ()%10+1);
    angle = player->plr->mo->angle;
    angle += (P_Random()-P_Random())<<18;
    
    // use meleerange + 1 se the puff doesn't skip the flash
    slope = P_AimLineAttack (player->plr->mo, angle, MELEERANGE+1);
    P_LineAttack (player->plr->mo, angle, MELEERANGE+1, slope, damage);
	
    if (!linetarget)
    {
		//S_StartSound (player->plr->mo, sfx_sawful, player);
		S_StartSound(sfx_sawful, player->plr->mo);
		return;
    }
    //S_StartSound (player->plr->mo, sfx_sawhit, player);
	S_StartSound(sfx_sawhit, player->plr->mo);
	
    // turn to face target
    angle = R_PointToAngle2 (player->plr->mo->x, player->plr->mo->y,
			     linetarget->x, linetarget->y);
    if (angle - player->plr->mo->angle > ANG180)
    {
		if (angle - player->plr->mo->angle < -ANG90/20)
			player->plr->mo->angle = angle + ANG90/21;
		else
			player->plr->mo->angle -= ANG90/20;
    }
    else
    {
		if (angle - player->plr->mo->angle > ANG90/20)
			player->plr->mo->angle = angle - ANG90/21;
		else
			player->plr->mo->angle += ANG90/20;
    }
    player->plr->mo->flags |= MF_JUSTATTACKED;
}



//
// A_FireMissile
//
void C_DECL
A_FireMissile
( player_t*	player,
  pspdef_t*	psp ) 
{
    //player->ammo[weaponinfo[player->readyweapon].ammo]--;
	P_ShotAmmo(player);
	player->update |= PSF_AMMO;
	if(IS_CLIENT) return;
    P_SpawnPlayerMissile (player->plr->mo, MT_ROCKET);
}


//
// A_FireBFG
//
void C_DECL
A_FireBFG
( player_t*	player,
  pspdef_t*	psp ) 
{
    //player->ammo[weaponinfo[player->readyweapon].ammo] -= BFGCELLS;
	P_ShotAmmo(player);
	player->update |= PSF_AMMO;
	if(IS_CLIENT) return;
    P_SpawnPlayerMissile (player->plr->mo, MT_BFG);
}



//
// A_FirePlasma
//
void C_DECL
A_FirePlasma
( player_t*	player,
  pspdef_t*	psp ) 
{
    //player->ammo[weaponinfo[player->readyweapon].ammo]--;
	P_ShotAmmo(player);

    P_SetPsprite (player,
		  ps_flash,
		  weaponinfo[player->readyweapon].flashstate+(P_Random ()&1) );

	player->update |= PSF_AMMO;
	if(IS_CLIENT) return;

    P_SpawnPlayerMissile (player->plr->mo, MT_PLASMA);
}



//
// P_BulletSlope
// Sets a slope so a near miss is at aproximately
// the height of the intended target
//
fixed_t		bulletslope;


void P_BulletSlope (mobj_t*	mo)
{
    angle_t	an;
    
    // see which target is to be aimed at
    an = mo->angle;
    bulletslope = P_AimLineAttack (mo, an, 16*64*FRACUNIT);
	
    if (!linetarget)
    {
		an += 1<<26;
		bulletslope = P_AimLineAttack (mo, an, 16*64*FRACUNIT);
		if (!linetarget)
		{
			an -= 2<<26;
			bulletslope = P_AimLineAttack (mo, an, 16*64*FRACUNIT);
		}
    }
}


//
// P_GunShot
//
void
P_GunShot
( mobj_t*	mo,
 boolean	accurate )
{
    angle_t	angle;
    int		damage;
	
    damage = 5*(P_Random ()%3+1);
    angle = mo->angle;
	
    if (!accurate)
		angle += (P_Random()-P_Random())<<18;
	
    P_LineAttack (mo, angle, MISSILERANGE, bulletslope, damage);
}


//
// A_FirePistol
//
void C_DECL
A_FirePistol
( player_t*	player,
  pspdef_t*	psp ) 
{
    S_StartSound(sfx_pistol, player->plr->mo);

    P_SetMobjState (player->plr->mo, S_PLAY_ATK2);
    //player->ammo[weaponinfo[player->readyweapon].ammo]--;
	P_ShotAmmo(player);

    P_SetPsprite (player,
		  ps_flash,
		  weaponinfo[player->readyweapon].flashstate);

	player->update |= PSF_AMMO;
	if(IS_CLIENT) return;

    P_BulletSlope (player->plr->mo);
    P_GunShot (player->plr->mo, !player->refire);
}


//
// A_FireShotgun
//
void C_DECL
A_FireShotgun
( player_t*	player,
 pspdef_t*	psp ) 
{
    int		i;
	
    S_StartSound(sfx_shotgn, player->plr->mo);
    P_SetMobjState (player->plr->mo, S_PLAY_ATK2);
	
    //player->ammo[weaponinfo[player->readyweapon].ammo]--;
	P_ShotAmmo(player);
	
    P_SetPsprite (player,
		ps_flash,
		weaponinfo[player->readyweapon].flashstate);

	player->update |= PSF_AMMO;
	if(IS_CLIENT) return;

    P_BulletSlope (player->plr->mo);
	
    for (i=0 ; i<7 ; i++)
		P_GunShot (player->plr->mo, false);
}



//
// A_FireShotgun2
//
void C_DECL
A_FireShotgun2
( player_t*	player,
 pspdef_t*	psp ) 
{
    int		i;
    angle_t	angle;
    int		damage;
	
	
    S_StartSound(sfx_dshtgn, player->plr->mo);
    P_SetMobjState (player->plr->mo, S_PLAY_ATK2);
	
    //player->ammo[weaponinfo[player->readyweapon].ammo]-=2;
	P_ShotAmmo(player);
	
    P_SetPsprite (player,
		ps_flash,
		weaponinfo[player->readyweapon].flashstate);
	
	player->update |= PSF_AMMO;
	if(IS_CLIENT) return;
	
    P_BulletSlope (player->plr->mo);
	
    for (i=0 ; i<20 ; i++)
    {
		damage = 5*(P_Random ()%3+1);
		angle = player->plr->mo->angle;
		angle += (P_Random()-P_Random())<<19;
		P_LineAttack (player->plr->mo,
			angle,
			MISSILERANGE,
			bulletslope + ((P_Random()-P_Random())<<5), damage);
    }
}


//
// A_FireCGun
//
void C_DECL
A_FireCGun
( player_t*	player,
 pspdef_t*	psp ) 
{
    S_StartSound(sfx_pistol, player->plr->mo);
	
    /*if(!player->ammo[weaponinfo[player->readyweapon].ammo])
		return;*/
	
    P_SetMobjState (player->plr->mo, S_PLAY_ATK2);
    //player->ammo[weaponinfo[player->readyweapon].ammo]--;
	P_ShotAmmo(player);
	
    P_SetPsprite (player,
		ps_flash,
		weaponinfo[player->readyweapon].flashstate
		+ psp->state - &states[S_CHAIN1] );

	player->update |= PSF_AMMO;
	if(IS_CLIENT) return;

    P_BulletSlope (player->plr->mo);
	
    P_GunShot (player->plr->mo, !player->refire);
}



//
// ?
//
void C_DECL A_Light0 (player_t *player, pspdef_t *psp)
{
    player->plr->extralight = 0;
}

void C_DECL A_Light1 (player_t *player, pspdef_t *psp)
{
    player->plr->extralight = 1;
}

void C_DECL A_Light2 (player_t *player, pspdef_t *psp)
{
    player->plr->extralight = 2;
}


//
// A_BFGSpray
// Spawn a BFG explosion on every monster in view
//
void C_DECL A_BFGSpray (mobj_t* mo) 
{
    int			i;
    int			j;
    int			damage;
    angle_t		an;
	
    // offset angles from its attack angle
    for (i=0 ; i<40 ; i++)
    {
		an = mo->angle - ANG90/2 + ANG90/40*i;
		
		// mo->target is the originator (player)
		//  of the missile
		P_AimLineAttack (mo->target, an, 16*64*FRACUNIT);
		
		if (!linetarget)
			continue;
		
		P_SpawnMobj (linetarget->x,
			linetarget->y,
			linetarget->z + (linetarget->height>>2),
			MT_EXTRABFG);
		
		damage = 0;
		for (j=0;j<15;j++)
			damage += (P_Random()&7) + 1;
		
		P_DamageMobj (linetarget, mo->target,mo->target, damage);
    }
}


//
// A_BFGsound
//
void C_DECL
A_BFGsound
( player_t*	player,
  pspdef_t*	psp )
{
    S_StartSound(sfx_bfg, player->plr->mo);
}



//
// P_SetupPsprites
// Called at start of level for each player.
//
void P_SetupPsprites (player_t* player) 
{
    int	i;
	
    // remove all psprites
    for (i=0 ; i<NUMPSPRITES ; i++)
		player->psprites[i].state = NULL;
		
    // spawn the gun
    player->pendingweapon = player->readyweapon;
    P_BringUpWeapon (player);
}




//
// P_MovePsprites
// Called every tic by player thinking routine.
//
void P_MovePsprites (player_t* player) 
{
    int		i;
    pspdef_t*	psp;
    state_t*	state;
	
    psp = &player->psprites[0];
    for (i=0 ; i<NUMPSPRITES ; i++, psp++)
    {
		// a null state means not active
		if ( (state = psp->state) )	
		{
			// drop tic count and possibly change state
			
			// a -1 tic count never changes
			if (psp->tics != -1)	
			{
				psp->tics--;
				if (!psp->tics)
					P_SetPsprite (player, i, psp->state->nextstate);
			}				
		}
    }
    
    player->psprites[ps_flash].sx = player->psprites[ps_weapon].sx;
    player->psprites[ps_flash].sy = player->psprites[ps_weapon].sy;
}


