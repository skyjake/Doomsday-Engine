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
// Revision 1.7  2004/05/28 17:16:41  skyjake
// Resolved conflicts (branch-1-7 overrides)
//
// Revision 1.3.2.3  2004/05/23 14:11:09  skyjake
// Resolved conflict
//
// Revision 1.3.2.2  2004/05/16 10:01:37  skyjake
// Merged good stuff from branch-nix for the final 1.7.15
//
// Revision 1.3.2.1.2.2  2003/11/22 18:09:10  skyjake
// Cleanup
//
// Revision 1.3.2.1.2.1  2003/11/19 17:07:14  skyjake
// Modified to compile with gcc and -DUNIX
//
// Revision 1.3.2.1  2003/09/14 20:24:12  skyjake
// Clientside jumping rules come from the server
//
// Revision 1.3  2003/06/30 00:05:23  skyjake
// Use fixmom
//
// Revision 1.2  2003/02/28 19:24:44  skyjake
// Added player-air-movement (not netgame-friendly)
//
// Revision 1.1  2003/02/26 19:22:05  skyjake
// Initial checkin
//
// Revision 1.1  2002/09/29 01:11:47  Jaakko
// Added Doomsday sources
//
//
// DESCRIPTION:
//	Player related stuff.
//	Bobbing POV/weapon, movement.
//	Pending weapon.
//
//-----------------------------------------------------------------------------


static const char
rcsid[] = "$Id$";

#include <math.h>

#include "doomdef.h"
#include "d_config.h"
#include "d_event.h"
#include "p_local.h"
#include "p_view.h"
#include "doomstat.h"
#include "d_netJD.h"
#include "g_common.h"

int maxhealth;		// 100
int healthlimit;	// 200
int armorpoints[2];	// Green and blue points.

// Index of the special effects (INVUL inverse) map.
#define INVERSECOLORMAP		32

//
// Movement.
//

// 16 pixels of bob
#define MAXBOB	0x100000	

boolean		onground;


//
// P_Thrust
// Moves the given origin along a given angle.
//
void
P_Thrust
( player_t*	player,
  angle_t	angle,
  fixed_t	move ) 
{
	mobj_t *mo = player->plr->mo;
	int mul = XS_ThrustMul(mo->subsector->sector);

    angle >>= ANGLETOFINESHIFT;

	// On slippery surfaces one cannot accelerate so quickly.
	if(mul != FRACUNIT) move = FixedMul(move, mul);
    
    mo->momx += FixedMul(move, finecosine[angle]); 
    mo->momy += FixedMul(move, finesine[angle]);
}

//===========================================================================
// P_IsPlayerOnGround
//	Returns true if the player is currently standing on ground or on top
//	of another mobj.
//===========================================================================
boolean P_IsPlayerOnGround(player_t *player)
{
	boolean onground = (player->plr->mo->z <= player->plr->mo->floorz);
	if(player->plr->mo->onmobj && !onground)
	{
		mobj_t *on = player->plr->mo->onmobj;
		onground = (player->plr->mo->z <= on->z + on->height);
	}
	return onground;
}

//===========================================================================
// P_CheckPlayerJump
//	Will make the player jump if the latest command so instructs, 
//	providing that jumping is possible.
//===========================================================================
void P_CheckPlayerJump(player_t *player)
{
	ticcmd_t *cmd = &player->cmd;

	if(cfg.jumpEnabled 
		&& (!IS_CLIENT || netJumpPower > 0)
		&& P_IsPlayerOnGround(player)
		&& !(cmd->actions & BT_SPECIAL)
		&& !(cmd->actions & BT_CHANGE)
		&& cmd->actions & BT_JUMP
		&& player->jumptics <= 0)
	{
		// Jump, then!
		player->plr->mo->momz = FRACUNIT 
			* (IS_CLIENT? netJumpPower : cfg.jumpPower);
		player->jumptics = 24;
	}
}

//
// P_MovePlayer
//
void P_MovePlayer (player_t* player)
{
	mobj_t*			plrmo = player->plr->mo;
    ticcmd_t*		cmd;
	
    cmd = &player->cmd;
	
	// Change the angle if possible.
	if(!(player->plr->flags & DDPF_FIXANGLES))
	{
		plrmo->angle = cmd->angle << 16;
		player->plr->lookdir = cmd->pitch/(float)DDMAXSHORT * 110;
	}

    // Do not let the player control movement if not onground.
    onground = (player->plr->mo->z <= player->plr->mo->floorz);
	if(player->plr->mo->onmobj && !onground)
	{
		mobj_t *on = player->plr->mo->onmobj;
		onground = (player->plr->mo->z <= on->z + on->height);
	}

	if(player->plr->flags & DDPF_CAMERA) // $democam
	{
		// Cameramen have a 3D thrusters!
		P_Thrust3D(player, player->plr->mo->angle,
			player->plr->lookdir, cmd->forwardMove*2048,
			cmd->sideMove*2048);
	}
	else
	{
		// 'Move while in air' hack (server doesn't know about this!!).
		int movemul = onground? 2048 
			: cfg.airborneMovement? cfg.airborneMovement*64 
			: 0; // Movement while in air traditionally disabled.

		if (cmd->forwardMove && movemul)
		{
			P_Thrust(player, player->plr->mo->angle, 
				cmd->forwardMove * movemul);
		}
		
		if (cmd->sideMove && movemul)
		{
			P_Thrust(player, player->plr->mo->angle-ANG90, 
				cmd->sideMove * movemul);
		}
		
		if ( (cmd->forwardMove || cmd->sideMove) 
			&& player->plr->mo->state == &states[S_PLAY] )
		{
			P_SetMobjState (player->plr->mo, S_PLAY_RUN1);
		}
		P_CheckPlayerJump(player);
	}
}	



//
// P_DeathThink
// Fall on your face when dying.
// Decrease POV height to floor height.
//
#define ANG5   	(ANG90/18)

void P_DeathThink (player_t* player)
{
    angle_t		angle;
    angle_t		delta;

    P_MovePsprites (player);
	
    // fall to the ground
    if (player->plr->viewheight > 6*FRACUNIT)
		player->plr->viewheight -= FRACUNIT;

    if (player->plr->viewheight < 6*FRACUNIT)
		player->plr->viewheight = 6*FRACUNIT;

    player->plr->deltaviewheight = 0;
	player->plr->flags |= DDPF_FIXANGLES | DDPF_FIXPOS | DDPF_FIXMOM;
    onground = (player->plr->mo->z <= player->plr->mo->floorz);
    P_CalcHeight (player);

	// In netgames we won't keep tracking the killer.
    if(!IS_NETGAME && player->attacker && player->attacker != player->plr->mo)
    {
		angle = R_PointToAngle2(player->plr->mo->x,
			player->plr->mo->y,
			player->attacker->x,
			player->attacker->y);
		
		delta = angle - player->plr->mo->angle;
		
		if (delta < ANG5 || delta > (unsigned)-ANG5)
		{
			// Looking at killer,
			//  so fade damage flash down.
			player->plr->mo->angle = angle;
			if(player->damagecount) player->damagecount--;
		}
		else if (delta < ANG180)
			player->plr->mo->angle += ANG5;
		else
			player->plr->mo->angle -= ANG5;
    }
    else if (player->damagecount)
		player->damagecount--;
	
    if (player->cmd.actions & BT_USE)
		player->playerstate = PST_REBORN;
}

#define LIST_BEGIN		-1
#define LIST_END		-2

weapontype_t P_PlayerFindWeapon(player_t *player, boolean next)
{
/*		if (newweapon == wp_fist
			&& player->weaponowned[wp_chainsaw]
			&& !(player->readyweapon == wp_chainsaw
			&& player->powers[pw_strength]))
		{
			newweapon = wp_chainsaw;
		}
		
		if ( (gamemode == commercial)
			&& newweapon == wp_shotgun 
			&& player->weaponowned[wp_supershotgun]
			&& player->readyweapon != wp_supershotgun)
		{
			newweapon = wp_supershotgun;
		}

		if (player->weaponowned[newweapon]
			&& newweapon != player->readyweapon)
		{
			// Do not go to plasma or BFG in shareware,
			//  even if cheated.
			if ((newweapon != wp_plasma
				&& newweapon != wp_bfg)
				|| (gamemode != shareware) )
			{
				player->pendingweapon = newweapon;
			}
		}
*/	
	int sw_list[] = 
	{ 
		wp_fist, wp_pistol, wp_shotgun, wp_chaingun, wp_missile, wp_chainsaw
	};
	int reg_list[] = 
	{ 
		wp_fist, wp_pistol, wp_shotgun, wp_chaingun, wp_missile, wp_plasma, 
		wp_bfg, wp_chainsaw
	};
	int dm2_list[] =
	{
		wp_fist, wp_pistol, wp_shotgun, wp_supershotgun, wp_chaingun, 
		wp_missile,	wp_plasma, wp_bfg, wp_chainsaw
	};
	int *list, num, i;

	if(gamemode == shareware)
	{
		list = sw_list;
		num = 6;
	}
	else if(gamemode == commercial)
	{
		list = dm2_list;
		num = 9;
	}
	else
	{
		list = reg_list;
		num = 8;
	}
	// Find the current position in the weapon list.
	for(i = 0; i < num; i++) if(list[i] == player->readyweapon) break;
	// Locate the next or previous weapon owned by the player.
	for(;;)
	{
		// Move the iterator.
		if(next) i++; else i--;
		if(i < 0) i = num-1;
		else if(i > num-1) i = 0;
		// Have we circled around?
		if(list[i] == player->readyweapon) break;
		// A valid weapon?
		if(player->weaponowned[list[i]]) break;
	}
	return list[i];
}

//----------------------------------------------------------------------------
//
// PROC P_ClientSideThink
//
// Called once per tick by P_Ticker.
//
//----------------------------------------------------------------------------
void P_ClientSideThink()
{
	int	i;
	player_t *pl;
	ddplayer_t *dpl;
	mobj_t *mo;
	ticcmd_t *cmd;

	if(!IS_CLIENT || !Get(DD_GAME_READY)) return;

	pl = &players[consoleplayer];
	dpl = pl->plr;
	mo = dpl->mo;
	cmd = &pl->cmd; // The latest local command.
	P_CalcHeight(pl);

	// Powers tic away.
	for(i = 0; i < NUMPOWERS; i++)
	{
		switch(i)
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
	    if(dpl->viewheight > 6*FRACUNIT) dpl->viewheight -= FRACUNIT;
	    if(dpl->viewheight < 6*FRACUNIT) dpl->viewheight = 6*FRACUNIT;
	}

	// Jumping.
	if(pl->jumptics) pl->jumptics--;
	P_CheckPlayerJump(pl);
 
	// Set the proper thrust multiplier. XG gives this quite easily.
	// (The thrust multiplier is used by Cl_MovePlayer, the movement
	// "predictor"; almost all clientside movement is handled by that
	// routine, though.)
	Set(DD_CPLAYER_THRUST_MUL, XS_ThrustMul(mo->subsector->sector));

	// Update view angles. The server fixes them if necessary.
	mo->angle = dpl->clAngle;
	dpl->lookdir = dpl->clLookDir;
}

//===========================================================================
// P_PlayerThink
//===========================================================================
void P_PlayerThink (player_t* player)
{
	mobj_t*			plrmo = player->plr->mo;
    ticcmd_t*		cmd;
    weapontype_t	newweapon, oldweapon;

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
	plrmo->selector = (plrmo->selector & ~DDMOBJ_SELECTOR_MASK)
		| (player->readyweapon + 1);

	P_CameraThink(player); // $democam

    // fixme: do this in the cheat code
    if (player->cheats & CF_NOCLIP)
		player->plr->mo->flags |= MF_NOCLIP;
    else
		player->plr->mo->flags &= ~MF_NOCLIP;
   
    // chain saw run forward
    cmd = &player->cmd;
    if (player->plr->mo->flags & MF_JUSTATTACKED)
    {
		cmd->angle = plrmo->angle >> 16;	// Don't turn.
		// The client must know of this.
		player->plr->flags |= DDPF_FIXANGLES;
		cmd->forwardMove = 0xc800/512;
		cmd->sideMove = 0;
		player->plr->mo->flags &= ~MF_JUSTATTACKED;
    }
	
    if (player->playerstate == PST_DEAD)
    {
		P_DeathThink (player);
		return;
    }
    
    // Move around.
    // Reactiontime is used to prevent movement
    //  for a bit after a teleport.
    if (player->plr->mo->reactiontime)
		player->plr->mo->reactiontime--;
    else
		P_MovePlayer (player);
    
    P_CalcHeight (player);
	
    if (player->plr->mo->subsector->sector->special)
		P_PlayerInSpecialSector (player);
    
	if(player->jumptics) player->jumptics--;

	oldweapon = player->pendingweapon;

	if(cmd->actions & BT_SPECIAL)
	{
		// There might be a special weapon change.
		if(cmd->actions & (BTS_NEXTWEAPON | BTS_PREVWEAPON) &&
			!(cmd->actions & BTS_PAUSE))
		{
			player->pendingweapon = P_PlayerFindWeapon(player, 
				(cmd->actions & BTS_NEXTWEAPON) != 0);
		}
		cmd->actions = 0;
	}

    // Check for weapon change.
    if (cmd->actions & BT_CHANGE)
    {
		// The actual changing of the weapon is done
		//  when the weapon psprite can do it
		//  (read: not in the middle of an attack).
		newweapon = (cmd->actions & BT_WEAPONMASK)>>BT_WEAPONSHIFT;

/*		if (newweapon == wp_fist
			&& player->weaponowned[wp_chainsaw]
			&& !(player->readyweapon == wp_chainsaw
			&& player->powers[pw_strength]))
		{
			newweapon = wp_chainsaw;
		}*/
		
		/*if ( (gamemode == commercial)
			&& newweapon == wp_shotgun 
			&& player->weaponowned[wp_supershotgun]
			&& player->readyweapon != wp_supershotgun)
		{
			newweapon = wp_supershotgun;
		}*/

		if(gamemode != commercial && newweapon == wp_supershotgun)
		{
			// In non-Doom II, supershotgun is the same as normal shotgun.
			newweapon = wp_shotgun;
		}

		if (player->weaponowned[newweapon]
			&& newweapon != player->readyweapon)
		{
			// Do not go to plasma or BFG in shareware,
			//  even if cheated.
			if ((newweapon != wp_plasma
				&& newweapon != wp_bfg)
				|| (gamemode != shareware) )
			{
				player->pendingweapon = newweapon;
			}
		}
    }

	if(player->pendingweapon != oldweapon)
		player->update |= PSF_PENDING_WEAPON | PSF_READY_WEAPON;

    // check for use
    if (cmd->actions & BT_USE)
    {
		if (!player->usedown)
		{
			P_UseLines (player);
			player->usedown = true;
		}
    }
    else
		player->usedown = false;
    
    // cycle psprites
    P_MovePsprites (player);
    
    // Counters, time dependend power ups.
	
    // Strength counts up to diminish fade.
    if (player->powers[pw_strength])
		player->powers[pw_strength]++;	
	
    if (player->powers[pw_invulnerability])
		player->powers[pw_invulnerability]--;
	
    if (player->powers[pw_invisibility])
	{
		if (! --player->powers[pw_invisibility] )
			player->plr->mo->flags &= ~MF_SHADOW;
	}
	
	if (player->powers[pw_infrared])
		player->powers[pw_infrared]--;
	
	if (player->powers[pw_ironfeet])
		player->powers[pw_ironfeet]--;
	
	if (player->damagecount)
		player->damagecount--;
	
	if (player->bonuscount)
		player->bonuscount--;
}

//===========================================================================
// P_SetMessage
//===========================================================================
void P_SetMessage(player_t *pl, char *msg)
{
	pl->message = msg;

	if(pl == &players[consoleplayer] && cfg.echoMsg)
		Con_FPrintf(CBLF_CYAN, "%s\n", msg);

	// Servers are responsible for sending these messages to the clients.
	NetSv_SendMessage(pl - players, msg);
}



