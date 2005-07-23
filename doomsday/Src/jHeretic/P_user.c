
// P_user.c

#include "jHeretic/Doomdef.h"
#include "jHeretic/P_local.h"
#include "p_view.h"
#include "jHeretic/Soundst.h"
#include "jHeretic/h_config.h"

void    P_PlayerNextArtifact(player_t *player);

// Macros

#define MAXBOB 0x100000			// 16 pixels of bob

// Data

//int lookdirSpeed = 3;
boolean onground;
int     newtorch;				// used in the torch flicker effect.
int     newtorchdelta;

boolean WeaponInShareware[] = {
	true,						// Staff
	true,						// Gold wand
	true,						// Crossbow
	true,						// Blaster
	false,						// Skull rod
	false,						// Phoenix rod
	false,						// Mace
	true,						// Gauntlets
	true						// Beak
};

/*
   ==================
   =
   = P_Thrust
   =
   = moves the given origin along a given angle
   =
   ==================
 */

void P_Thrust(player_t *player, angle_t angle, fixed_t move)
{
	mobj_t *plrmo = player->plr->mo;

	angle >>= ANGLETOFINESHIFT;
	if(player->powers[pw_flight] && !(plrmo->z <= plrmo->floorz))
	{
		plrmo->momx += FixedMul(move, finecosine[angle]);
		plrmo->momy += FixedMul(move, finesine[angle]);
	}
	else if(plrmo->subsector->sector->special == 15)	// Friction_Low
	{
		plrmo->momx += FixedMul(move >> 2, finecosine[angle]);
		plrmo->momy += FixedMul(move >> 2, finesine[angle]);
	}
	else
	{
		int     mul = XS_ThrustMul(plrmo->subsector->sector);

		if(mul != FRACUNIT)
			move = FixedMul(move, mul);
		plrmo->momx += FixedMul(move, finecosine[angle]);
		plrmo->momy += FixedMul(move, finesine[angle]);
	}
}

/*
   ==================
   =
   = P_CalcHeight
   =
   =Calculate the walking / running height adjustment
   =
   ==================
 */

#if 0
void P_CalcHeight(player_t *player)
{
	int     angle;
	fixed_t bob;
	ddplayer_t *dp = player->plr;
	mobj_t *plrmo = dp->mo;

	//
	// regular movement bobbing (needs to be calculated for gun swing even
	// if not on ground)
	// OPTIMIZE: tablify angle

	// $democam: simplified height calculation for cameramen
	if(P_IsCamera(plrmo))
	{
		dp->viewz = plrmo->z + dp->viewheight;
		return;
	}

	player->bob =
		FixedMul(plrmo->momx, plrmo->momx) + FixedMul(plrmo->momy,
													  plrmo->momy);
	player->bob >>= 2;
	if(player->bob > MAXBOB)
		player->bob = MAXBOB;
	if(plrmo->flags2 & MF2_FLY && plrmo->z > plrmo->floorz)
	{
		player->bob = FRACUNIT / 2;
	}
	// Clients do view bobbing by themselves.
	/*if(IS_CLIENT)
	   {
	   angle = (FINEANGLES/20*leveltime)&FINEMASK;
	   bob = FixedMul (player->bob/2, finesine[angle]);
	   player->plr->viewz = plrmo->z + player->plr->viewheight +
	   (player->chickenTics? -20*FRACUNIT : bob);
	   return;
	   } */

	if((player->cheats & CF_NOMOMENTUM))
	{
		player->plr->viewz = plrmo->z + VIEWHEIGHT;
		if(player->plr->viewz > plrmo->ceilingz - 4 * FRACUNIT)
			player->plr->viewz = plrmo->ceilingz - 4 * FRACUNIT;
		player->plr->viewz = plrmo->z + player->plr->viewheight;
		return;
	}

	angle = (FINEANGLES / 20 * leveltime) & FINEMASK;
	bob = FixedMul(player->bob / 2, finesine[angle]);

	//
	// move viewheight
	//
	if(player->playerstate == PST_LIVE)
	{
		player->plr->viewheight += player->deltaviewheight;
		if(player->plr->viewheight > VIEWHEIGHT)
		{
			player->plr->viewheight = VIEWHEIGHT;
			player->deltaviewheight = 0;
		}
		if(player->plr->viewheight < VIEWHEIGHT / 2)
		{
			player->plr->viewheight = VIEWHEIGHT / 2;
			if(player->deltaviewheight <= 0)
				player->deltaviewheight = 1;
		}
		if(player->deltaviewheight)
		{
			player->deltaviewheight += FRACUNIT / 4;
			if(!player->deltaviewheight)
				player->deltaviewheight = 1;
		}
	}

	if(player->chickenTics)
	{
		player->plr->viewz =
			plrmo->z + player->plr->viewheight - (20 * FRACUNIT);
	}
	else
	{
		player->plr->viewz = plrmo->z + player->plr->viewheight;
		if(plrmo->z <= plrmo->floorz)
			player->plr->viewz += bob;
	}
	if(plrmo->flags2 & MF2_FEETARECLIPPED && player->playerstate != PST_DEAD &&
	   plrmo->z <= plrmo->floorz)
	{
		player->plr->viewz -= FOOTCLIPSIZE;
	}
	if(player->plr->viewz > plrmo->ceilingz - 4 * FRACUNIT)
	{
		player->plr->viewz = plrmo->ceilingz - 4 * FRACUNIT;
	}
	if(player->plr->viewz < plrmo->floorz + 4 * FRACUNIT)
	{
		player->plr->viewz = plrmo->floorz + 4 * FRACUNIT;
	}
}
#endif

/*
   =================
   =
   = P_MovePlayer
   =
   =================
 */

void P_MovePlayer(player_t *player)
{
	int     fly;
	ticcmd_t *cmd;
	mobj_t *plrmo = player->plr->mo;

	cmd = &player->cmd;

	// Change the angle if possible.
	if(!(player->plr->flags & DDPF_FIXANGLES))
	{
		plrmo->angle = cmd->angle << 16;
		player->plr->lookdir = cmd->pitch / (float) DDMAXSHORT *110;
	}

	if(player->plr->flags & DDPF_CAMERA)	// $democam
	{
		// Cameramen have a 3D thrusters!
		P_Thrust3D(player, player->plr->mo->angle, player->plr->lookdir,
				   cmd->forwardMove * 2048, cmd->sideMove * 2048);
		return;
	}

	onground = (plrmo->z <= plrmo->floorz || (plrmo->flags2 & MF2_ONMOBJ));

	if(player->chickenTics)
	{							// Chicken speed
		if(cmd->forwardMove && (onground || plrmo->flags2 & MF2_FLY))
			P_Thrust(player, plrmo->angle, cmd->forwardMove * 2500);
		if(cmd->sideMove && (onground || plrmo->flags2 & MF2_FLY))
			P_Thrust(player, plrmo->angle - ANG90, cmd->sideMove * 2500);
	}
	else
	{							// Normal speed
		if(cmd->forwardMove && (onground || plrmo->flags2 & MF2_FLY))
			P_Thrust(player, plrmo->angle, cmd->forwardMove * 2048);
		if(cmd->sideMove && (onground || plrmo->flags2 & MF2_FLY))
			P_Thrust(player, plrmo->angle - ANG90, cmd->sideMove * 2048);
	}

	if(cmd->forwardMove || cmd->sideMove)
	{
		if(player->chickenTics)
		{
			if(plrmo->state == &states[S_CHICPLAY])
			{
				P_SetMobjState(plrmo, S_CHICPLAY_RUN1);
			}
		}
		else
		{
			if(plrmo->state == &states[S_PLAY])
			{
				P_SetMobjState(plrmo, S_PLAY_RUN1);
			}
		}
	}

	fly = cmd->fly; //lookfly >> 4;
/*	if(fly > 7)
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
	else if(fly > 0)
	{
		P_PlayerUseArtifact(player, arti_fly);
	}
	if(plrmo->flags2 & MF2_FLY)
	{
		plrmo->momz = player->flyheight * FRACUNIT;
		if(player->flyheight)
		{
			player->flyheight /= 2;
		}
	}
}

/*
   =================
   =
   = P_DeathThink
   =
   =================
 */

#define         ANG5    (ANG90/18)

void P_DeathThink(player_t *player)
{
	angle_t angle, delta;
	extern int inv_ptr;
	extern int curpos;
	int     lookDelta;
	mobj_t *plrmo = player->plr->mo;

	P_MovePsprites(player);

	onground = (plrmo->z <= plrmo->floorz);
	if(plrmo->type == MT_BLOODYSKULL)
	{							// Flying bloody skull
		player->plr->viewheight = 6 * FRACUNIT;
		player->plr->deltaviewheight = 0;
		//player->damagecount = 20;
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
	else
	{							// Fall to ground
		player->plr->deltaviewheight = 0;
		if(player->plr->viewheight > 6 * FRACUNIT)
			player->plr->viewheight -= FRACUNIT;
		if(player->plr->viewheight < 6 * FRACUNIT)
			player->plr->viewheight = 6 * FRACUNIT;
		if(player->plr->lookdir > 0)
		{
			player->plr->lookdir -= 6;
		}
		else if(player->plr->lookdir < 0)
		{
			player->plr->lookdir += 6;
		}
		if(abs(player->plr->lookdir) < 6)
		{
			player->plr->lookdir = 0;
		}
	}

	player->plr->flags |= DDPF_FIXANGLES | DDPF_FIXPOS | DDPF_FIXMOM;
	P_CalcHeight(player);

	if(!IS_NETGAME && player->attacker && player->attacker != plrmo)
	{
		angle =
			R_PointToAngle2(plrmo->x, plrmo->y, player->attacker->x,
							player->attacker->y);
		delta = angle - plrmo->angle;
		if(delta < ANG5 || delta > (unsigned) -ANG5)
		{						// Looking at killer, so fade damage flash down
			plrmo->angle = angle;
			if(player->damagecount)
			{
				player->damagecount--;
			}
		}
		else if(delta < ANG180)
			plrmo->angle += ANG5;
		else
			plrmo->angle -= ANG5;
	}
	else if(player->damagecount)
	{
		player->damagecount--;
	}

	if(player->cmd.use)
	{
		if(player == &players[consoleplayer])
		{
			//          I_SetPalette((byte *)W_CacheLumpName("PLAYPAL", PU_CACHE));
			H_SetFilter(0);
			inv_ptr = 0;
			curpos = 0;
			newtorch = 0;
			newtorchdelta = 0;
		}
		player->playerstate = PST_REBORN;
		// Let the mobj know the player has entered the reborn state.  Some
		// mobjs need to know when it's ok to remove themselves.
		plrmo->special2 = 666;
	}
}

//----------------------------------------------------------------------------
//
// PROC P_ChickenPlayerThink
//
//----------------------------------------------------------------------------

void P_ChickenPlayerThink(player_t *player)
{
	mobj_t *pmo;

	if(player->health > 0)
	{							// Handle beak movement
		P_UpdateBeak(player, &player->psprites[ps_weapon]);
	}
	if(IS_CLIENT || player->chickenTics & 15)
	{
		return;
	}
	pmo = player->plr->mo;
	if(!(pmo->momx + pmo->momy) && P_Random() < 160)
	{							// Twitch view angle
		pmo->angle += (P_Random() - P_Random()) << 19;
	}
	if((pmo->z <= pmo->floorz) && (P_Random() < 32))
	{							// Jump and noise
		pmo->momz += FRACUNIT;
		P_SetMobjState(pmo, S_CHICPLAY_PAIN);
		return;
	}
	if(P_Random() < 48)
	{							// Just noise
		S_StartSound(sfx_chicact, pmo);
	}
}

//----------------------------------------------------------------------------
//
// FUNC P_GetPlayerNum
//
//----------------------------------------------------------------------------

int P_GetPlayerNum(player_t *player)
{
	int     i;

	for(i = 0; i < MAXPLAYERS; i++)
	{
		if(player == &players[i])
		{
			return (i);
		}
	}
	return (0);
}

//----------------------------------------------------------------------------
//
// FUNC P_UndoPlayerChicken
//
//----------------------------------------------------------------------------

boolean P_UndoPlayerChicken(player_t *player)
{
	mobj_t *fog;
	mobj_t *mo;
	mobj_t *pmo;
	fixed_t x;
	fixed_t y;
	fixed_t z;
	angle_t angle;
	int     playerNum;
	weapontype_t weapon;
	int     oldFlags;
	int     oldFlags2;

	player->update |= PSF_CHICKEN_TIME | PSF_POWERS | PSF_HEALTH;

	pmo = player->plr->mo;
	x = pmo->x;
	y = pmo->y;
	z = pmo->z;
	angle = pmo->angle;
	weapon = pmo->special1;
	oldFlags = pmo->flags;
	oldFlags2 = pmo->flags2;
	P_SetMobjState(pmo, S_FREETARGMOBJ);
	mo = P_SpawnMobj(x, y, z, MT_PLAYER);
	if(P_TestMobjLocation(mo) == false)
	{							// Didn't fit
		P_RemoveMobj(mo);
		mo = P_SpawnMobj(x, y, z, MT_CHICPLAYER);
		mo->angle = angle;
		mo->health = player->health;
		mo->special1 = weapon;
		mo->player = player;
		mo->dplayer = player->plr;
		mo->flags = oldFlags;
		mo->flags2 = oldFlags2;
		player->plr->mo = mo;
		player->chickenTics = 2 * 35;
		return (false);
	}
	playerNum = P_GetPlayerNum(player);
	if(playerNum != 0)
	{							// Set color translation
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
	player->chickenTics = 0;
	player->powers[pw_weaponlevel2] = 0;
	player->health = mo->health = MAXHEALTH;
	player->plr->mo = mo;
	player->plr->flags |= DDPF_FIXPOS | DDPF_FIXMOM;
	player->update |= PSF_CHICKEN_TIME | PSF_HEALTH;
	angle >>= ANGLETOFINESHIFT;
	fog =
		P_SpawnMobj(x + 20 * finecosine[angle], y + 20 * finesine[angle],
					z + TELEFOGHEIGHT, MT_TFOG);
	S_StartSound(sfx_telept, fog);
	P_PostChickenWeapon(player, weapon);
	return (true);
}

//===========================================================================
// P_IsPlayerOnGround
//  Returns true if the player is currently standing on ground or on top
//  of another mobj.
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
//  Will make the player jump if the latest command so instructs,
//  providing that jumping is possible.
//===========================================================================
void P_CheckPlayerJump(player_t *player)
{
	ticcmd_t *cmd = &player->cmd;

	if(cfg.jumpEnabled && (!IS_CLIENT || netJumpPower > 0) &&
	   (P_IsPlayerOnGround(player) || player->plr->mo->flags2 & MF2_ONMOBJ) &&
	   cmd->jump && player->jumpTics <= 0)
	{
		// Jump, then!
		player->plr->mo->momz =
			FRACUNIT * (IS_CLIENT ? netJumpPower : cfg.jumpPower);
		player->plr->mo->flags2 &= ~MF2_ONMOBJ;
		player->jumpTics = 24;
	}
}

//===========================================================================
// P_ClientSideThink
//  This routine does all the thinking for the console player during
//  netgames.
//===========================================================================
void P_ClientSideThink()
{
	int     i, fly;
	player_t *pl;
	ddplayer_t *dpl;
	mobj_t *mo;
	ticcmd_t *cmd;

	if(!IS_CLIENT || !Get(DD_GAME_READY))
		return;

	pl = players + consoleplayer;
	dpl = pl->plr;
	mo = dpl->mo;
	cmd = &pl->cmd;

	P_CalcHeight(pl);

	if(pl->chickenTics)
	{
		P_ChickenPlayerThink(pl);
	}

	// Message timer.
	pl->messageTics--;			// Can go negative
	if(!pl->messageTics)
	{							// Refresh the screen when a message goes away
		//BorderTopRefresh = true;
		GL_Update(DDUF_TOP);
	}
	// Powers tic away.
	for(i = 0; i < NUMPOWERS; i++)
	{
		switch (i)
		{
		case pw_invulnerability:
		case pw_weaponlevel2:
		case pw_invisibility:
		case pw_flight:
		case pw_infrared:
			if(pl->powers[i] > 0)
				pl->powers[i]--;
			else
				pl->powers[i] = 0;
			break;
		}
	}
	if(pl->chickenTics > 0)
	{
		pl->chickenTics--;
		if(!pl->chickenTics)	// Chic mode ends?
			pl->psprites[ps_weapon].sy = WEAPONBOTTOM;
	}
	if(pl->chickenPeck > 0)
		pl->chickenPeck--;

	// Fall down if we're dead.
	if(pl->playerstate == PST_DEAD)
	{
		if(dpl->viewheight > 6 * FRACUNIT)
			dpl->viewheight -= FRACUNIT;
		if(dpl->viewheight < 6 * FRACUNIT)
			dpl->viewheight = 6 * FRACUNIT;
	}

	// Jump?
	pl->jumpTics--;
	P_CheckPlayerJump(pl);

	// Sector wind thrusts the player around.
	if(mo->subsector->sector->special)
		P_PlayerInWindSector(pl);

	// Flying.
	fly = cmd->fly; //lookfly >> 4;
/*	if(fly > 7)
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
		// Keep the Heretic fly flag in sync.
		mo->flags2 |= MF2_FLY;

		mo->momz = pl->flyheight * FRACUNIT;
		if(pl->flyheight)
			pl->flyheight /= 2;
		// Do some fly-bobbing.
		if(mo->z > mo->floorz && leveltime & 2)
			mo->z += finesine[(FINEANGLES / 20 * leveltime >> 2) & FINEMASK];
	}

	// Set consoleplayer thrust multiplier (used by client move code).
	if(mo->z > mo->floorz)		// Airborne?
	{
		Set(DD_CPLAYER_THRUST_MUL, (mo->ddflags & DDMF_FLY) ? FRACUNIT : 0);
	}
	else
	{
		Set(DD_CPLAYER_THRUST_MUL,
			// FIXME: Client can't know for sure about sector specials.
			(mo->subsector->sector->special == 15) ? FRACUNIT >> 1	// Friction_Low
			: XS_ThrustMul(mo->subsector->sector));
	}

	// Update view angles. The server fixes them if necessary.
	mo->angle = dpl->clAngle;
	dpl->lookdir = dpl->clLookDir;

	/*  for(i=0; i<MAXPLAYERS; i++)
	   if(players[i].plr->ingame)
	   P_MovePsprites(&players[i]); */
}

//----------------------------------------------------------------------------
//
// PROC P_PlayerThink
//
//----------------------------------------------------------------------------

void P_PlayerThink(player_t *player)
{
	ticcmd_t *cmd;
	weapontype_t newweapon;
	mobj_t *plrmo = player->plr->mo;

	// No-clip cheat
	if(player->cheats & CF_NOCLIP)
	{
		plrmo->flags |= MF_NOCLIP;
	}
	else
	{
		plrmo->flags &= ~MF_NOCLIP;
	}

	// Selector 0 = Generic (used by default)
	// Selector 1 = Staff
	// Selector 2 = Goldwand
	// Selector 3 = Crossbow
	// Selector 4 = Blaster
	// Selector 5 = Skullrod
	// Selector 6 = Phoenixrod
	// Selector 7 = Mace
	// Selector 8 = Gauntlets
	// Selector 9 = Beak
	plrmo->selector =
		(plrmo->selector & ~DDMOBJ_SELECTOR_MASK) | (player->readyweapon + 1);

	P_CameraThink(player);		// $democam

	cmd = &player->cmd;
	if(plrmo->flags & MF_JUSTATTACKED)
	{							// Gauntlets attack auto forward motion
		//cmd->angleturn = 0;
		cmd->angle = plrmo->angle >> 16;	// Don't turn.
		// The client must know of this.
		player->plr->flags |= DDPF_FIXANGLES;
		cmd->forwardMove = 0xc800 / 512;
		cmd->sideMove = 0;
		plrmo->flags &= ~MF_JUSTATTACKED;
	}
	// messageTics is above the rest of the counters so that messages will
	//              go away, even in death.
	player->messageTics--;		// Can go negative
	if(!player->messageTics)
	{							// Refresh the screen when a message goes away
		//BorderTopRefresh = true;
		GL_Update(DDUF_TOP);
	}
	if(player->playerstate == PST_DEAD)
	{
		P_DeathThink(player);
		return;
	}
	if(player->chickenTics)
	{
		P_ChickenPlayerThink(player);
	}
	// Handle movement
	if(plrmo->reactiontime)
	{							// Player is frozen
		plrmo->reactiontime--;
	}
	else
	{
		P_MovePlayer(player);
	}
	P_CalcHeight(player);
	if(plrmo->subsector->sector->special)
	{
		P_PlayerInSpecialSector(player);
	}
	if(player->jumpTics)
		player->jumpTics--;
	P_CheckPlayerJump(player);
	if(cmd->arti)
	{							// Use an artifact
		if(cmd->arti == 0xff)
			P_PlayerNextArtifact(player);
		else				   
			P_PlayerUseArtifact(player, cmd->arti);
	}

	if(cmd->suicide)
	{
		P_DamageMobj(player->plr->mo, NULL, NULL, 10000);
	}

	// Check for weapon change
	if(cmd->changeWeapon)
	{
		int     oldweapon = player->pendingweapon;

		// The actual changing of the weapon is done when the weapon
		// psprite can do it (A_WeaponReady), so it doesn't happen in
		// the middle of an attack.
		newweapon = cmd->changeWeapon - 1;

		if(player->weaponowned[newweapon] && newweapon != player->readyweapon)
		{
			if(WeaponInShareware[newweapon] || !shareware)
			{
				player->pendingweapon = newweapon;
			}
		}
		if(oldweapon != player->pendingweapon)
			player->update |= PSF_PENDING_WEAPON;
	}
	// Check for use
	if(cmd->use)
	{
		if(!player->usedown)
		{
			P_UseLines(player);
			player->usedown = true;
		}
	}
	else
	{
		player->usedown = false;
	}
	// Chicken counter
	if(player->chickenTics)
	{
		if(player->chickenPeck)
		{						// Chicken attack counter
			player->chickenPeck -= 3;
		}
		if(!--player->chickenTics)
		{						// Attempt to undo the chicken
			P_UndoPlayerChicken(player);
		}
	}
	// Cycle psprites
	P_MovePsprites(player);
	// Other Counters
	if(player->powers[pw_invulnerability])
	{
		player->powers[pw_invulnerability]--;
	}
	if(player->powers[pw_invisibility])
	{
		if(!--player->powers[pw_invisibility])
		{
			plrmo->flags &= ~MF_SHADOW;
		}
	}
	if(player->powers[pw_infrared])
	{
		player->powers[pw_infrared]--;
	}
	if(player->powers[pw_flight])
	{
		if(!--player->powers[pw_flight])
		{
#ifdef __WATCOMC__
			if(plrmo->z != plrmo->floorz && !useexterndriver)
			{
				player->centering = true;
			}
#else
			if(plrmo->z != plrmo->floorz)
			{
				player->centering = true;
			}
#endif

			plrmo->flags2 &= ~MF2_FLY;
			plrmo->flags &= ~MF_NOGRAVITY;
			//BorderTopRefresh = true; //make sure the sprite's cleared out
			GL_Update(DDUF_TOP);
		}
	}
	if(player->powers[pw_weaponlevel2])
	{
		if(!--player->powers[pw_weaponlevel2])
		{
			if((player->readyweapon == wp_phoenixrod) &&
			   (player->psprites[ps_weapon].state != &states[S_PHOENIXREADY])
			   && (player->psprites[ps_weapon].state != &states[S_PHOENIXUP]))
			{
				P_SetPsprite(player, ps_weapon, S_PHOENIXREADY);
				player->ammo[am_phoenixrod] -= USE_PHRD_AMMO_2;
				player->refire = 0;
				player->update |= PSF_AMMO;
			}
			else if((player->readyweapon == wp_gauntlets) ||
					(player->readyweapon == wp_staff))
			{
				player->pendingweapon = player->readyweapon;
				player->update |= PSF_PENDING_WEAPON;
			}
			//BorderTopRefresh = true;
			GL_Update(DDUF_TOP);
		}
	}
	if(player->damagecount)
	{
		player->damagecount--;
	}
	if(player->bonuscount)
	{
		player->bonuscount--;
	}
	// Colormaps
	if(player->powers[pw_invulnerability])
	{
		/*if(player->powers[pw_invulnerability] > BLINKTHRESHOLD
		   || (player->powers[pw_invulnerability]&8))
		   {
		   player->plr->fixedcolormap = INVERSECOLORMAP;
		   }
		   else
		   {
		   player->plr->fixedcolormap = 0;
		   } */
	}
	else if(player->powers[pw_infrared])
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
		else if(!(leveltime & 16) && player == &players[consoleplayer])
		{
			if(newtorch)
			{
				if(player->plr->fixedcolormap + newtorchdelta > 7 ||
				   player->plr->fixedcolormap + newtorchdelta < 1 ||
				   newtorch == player->plr->fixedcolormap)
				{
					newtorch = 0;
				}
				else
				{
					player->plr->fixedcolormap += newtorchdelta;
				}
			}
			else
			{
				newtorch = (M_Random() & 7) + 1;
				newtorchdelta =
					(newtorch ==
					 player->plr->fixedcolormap) ? 0 : ((newtorch >
														 player->plr->
														 fixedcolormap) ? 1 :
														-1);
			}
		}
	}
	else
	{
		player->plr->fixedcolormap = 0;
	}
}

//----------------------------------------------------------------------------
//
// PROC P_ArtiTele
//
//----------------------------------------------------------------------------

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
		destX = playerstarts[0].x << FRACBITS;
		destY = playerstarts[0].y << FRACBITS;
		destAngle = ANG45 * (playerstarts[0].angle / 45);
	}
	P_Teleport(player->plr->mo, destX, destY, destAngle);
	/*S_StartSound(sfx_wpnup, NULL); // Full volume laugh
	   NetSv_Sound(NULL, sfx_wpnup, player-players); */
	S_StartSound(sfx_wpnup, NULL);
}

//----------------------------------------------------------------------------
//
// PROC P_CheckReadyArtifact
//
//----------------------------------------------------------------------------

void P_CheckReadyArtifact()
{
	player_t *player = &players[consoleplayer];
	extern int inv_ptr;
	extern int curpos;

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

//----------------------------------------------------------------------------
//
// PROC P_PlayerNextArtifact
//
//----------------------------------------------------------------------------

void P_PlayerNextArtifact(player_t *player)
{
	extern int inv_ptr;
	extern int curpos;

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

//----------------------------------------------------------------------------
//
// PROC P_PlayerRemoveArtifact
//
//----------------------------------------------------------------------------

void P_PlayerRemoveArtifact(player_t *player, int slot)
{
	int     i;
	extern int inv_ptr;
	extern int curpos;

	player->update |= PSF_INVENTORY;
	player->artifactCount--;
	if(!(--player->inventory[slot].count))
	{							// Used last of a type - compact the artifact list
		player->readyArtifact = arti_none;
		player->inventory[slot].type = arti_none;
		for(i = slot + 1; i < player->inventorySlotNum; i++)
		{
			player->inventory[i - 1] = player->inventory[i];
		}
		player->inventorySlotNum--;
		if(player == &players[consoleplayer])
		{						// Set position markers and get next readyArtifact
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

//----------------------------------------------------------------------------
//
// PROC P_PlayerUseArtifact
//
//----------------------------------------------------------------------------

void P_PlayerUseArtifact(player_t *player, artitype_t arti)
{
	int     i;
	boolean play_sound = false;

	for(i = 0; i < player->inventorySlotNum; i++)
	{
		if(arti == NUMARTIFACTS)	// Use everything in panic?
		{
			if(P_UseArtifact(player, player->inventory[i].type))
			{					// Artifact was used - remove it from inventory
				P_PlayerRemoveArtifact(player, i);
				play_sound = true;
				if(player == &players[consoleplayer])
					ArtifactFlash = 4;
			}
		}
		else if(player->inventory[i].type == arti)
		{						// Found match - try to use
			if(P_UseArtifact(player, arti))
			{					// Artifact was used - remove it from inventory
				P_PlayerRemoveArtifact(player, i);
				play_sound = true;
				if(player == &players[consoleplayer])
					ArtifactFlash = 4;
			}
			else
			{					// Unable to use artifact, advance pointer
				P_PlayerNextArtifact(player);
			}
			break;
		}
	}
	if(play_sound)
	{
		S_ConsoleSound(sfx_artiuse, NULL, player - players);
	}
}

//----------------------------------------------------------------------------
//
// FUNC P_UseArtifact
//
// Returns true if artifact was used.
//
//----------------------------------------------------------------------------

boolean P_UseArtifact(player_t *player, artitype_t arti)
{
	mobj_t *mo;
	angle_t angle;

	switch (arti)
	{
	case arti_invulnerability:
		if(!P_GivePower(player, pw_invulnerability))
		{
			return (false);
		}
		break;
	case arti_invisibility:
		if(!P_GivePower(player, pw_invisibility))
		{
			return (false);
		}
		break;
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
	case arti_tomeofpower:
		if(player->chickenTics)
		{						// Attempt to undo chicken
			if(P_UndoPlayerChicken(player) == false)
			{					// Failed
				P_DamageMobj(player->plr->mo, NULL, NULL, 10000);
			}
			else
			{					// Succeeded
				player->chickenTics = 0;
				S_StartSound(sfx_wpnup, player->plr->mo);
			}
		}
		else
		{
			if(!P_GivePower(player, pw_weaponlevel2))
			{
				return (false);
			}
			if(player->readyweapon == wp_staff)
			{
				P_SetPsprite(player, ps_weapon, S_STAFFREADY2_1);
			}
			else if(player->readyweapon == wp_gauntlets)
			{
				P_SetPsprite(player, ps_weapon, S_GAUNTLETREADY2_1);
			}
		}
		break;
	case arti_torch:
		if(!P_GivePower(player, pw_infrared))
		{
			return (false);
		}
		break;
	case arti_firebomb:
		angle = player->plr->mo->angle >> ANGLETOFINESHIFT;
		mo = P_SpawnMobj(player->plr->mo->x + 24 * finecosine[angle],
						 player->plr->mo->y + 24 * finesine[angle],
						 player->plr->mo->z -
						 15 * FRACUNIT *
						 ((player->plr->mo->flags2 & MF2_FEETARECLIPPED) != 0),
						 MT_FIREBOMB);
		mo->target = player->plr->mo;
		break;
	case arti_egg:
		mo = player->plr->mo;
		P_SpawnPlayerMissile(mo, MT_EGGFX);
		P_SPMAngle(mo, MT_EGGFX, mo->angle - (ANG45 / 6));
		P_SPMAngle(mo, MT_EGGFX, mo->angle + (ANG45 / 6));
		P_SPMAngle(mo, MT_EGGFX, mo->angle - (ANG45 / 3));
		P_SPMAngle(mo, MT_EGGFX, mo->angle + (ANG45 / 3));
		break;
	case arti_fly:
		if(!P_GivePower(player, pw_flight))
		{
			return (false);
		}
		break;
	case arti_teleport:
		P_ArtiTele(player);
		break;
	default:
		return (false);
	}
	return (true);
}
