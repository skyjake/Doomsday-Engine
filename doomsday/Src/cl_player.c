
//**************************************************************************
//**
//** CL_PLAYER.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_refresh.h"
#include "de_play.h"

// MACROS ------------------------------------------------------------------

#define TOP_PSPY		32
#define BOTTOM_PSPY		128

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean	pred_forward;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

playerstate_t	playerstate[MAXPLAYERS];
int				psp_move_speed = 6 * FRACUNIT;
int				cplr_thrust_mul = FRACUNIT;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int		fixspeed = 15;
static int		xfix, yfix, fixtics;
static float	pspy;

// Console player demo momentum (used to smooth out abrupt momentum changes).
static int		cp_momx[LOCALCAM_WRITE_TICS], cp_momy[LOCALCAM_WRITE_TICS];

// CODE --------------------------------------------------------------------

//==========================================================================
// Cl_InitPlayers
//	Clears the player state table.
//==========================================================================
void Cl_InitPlayers(void)
{
	int i;

	memset(playerstate, 0, sizeof(playerstate));
	xfix = yfix = fixtics = 0;
	pspy = 0;
	memset(cp_momx, 0, sizeof(cp_momx));
	memset(cp_momy, 0, sizeof(cp_momy));

	// Clear psprites. The server will send them.
	for(i = 0; i < MAXPLAYERS; i++)
	{
		players[i].psprites[0].stateptr = NULL;
		players[i].psprites[1].stateptr = NULL;
	}
}

//==========================================================================
// Cl_LocalCommand
//	Updates the state of the local player by looking at lastcmd.
//==========================================================================
void Cl_LocalCommand(void)
{
	ddplayer_t *pl = players + consoleplayer;
	client_t *cl = clients + consoleplayer;
	playerstate_t *s = playerstate + consoleplayer;

	s->forwardmove = cl->lastcmd->forwardmove * 2048;
	s->sidemove = cl->lastcmd->sidemove * 2048;
	s->angle = pl->clAngle;
	s->turndelta = 0;
}

//==========================================================================
// Cl_ReadPlayerDelta
//	Reads a single player delta from the message buffer and applies
//	it to the player in question. Returns false only if the list of 
//	deltas ends.
//==========================================================================
int Cl_ReadPlayerDelta(void)
{
	int df, psdf, i, idx;
	int num = Msg_ReadByte();
	playerstate_t *s;
	ddplayer_t *pl;
	ddpsprite_t *psp;

	if(num == 0xff) return false;	// End of list.

	// The first byte consists of a player number and some flags.
	df = (num & 0xf0) << 8;
	df |= Msg_ReadByte();	// Second byte is just flags.
	num &= 0xf;				// Clear the upper bits of the number.

	s = playerstate + num;
	pl = players + num;

	if(df & PDF_MOBJ) 
	{
		clmobj_t *old = s->cmo;
		int newid = Msg_ReadShort();
		// Make sure the 'new' mobj is different than the old one; 
		// there will be linking problems otherwise.
		// FIXME: What causes the duplicate sending of mobj ids?
		if(newid != s->mobjid) 
		{
			s->mobjid = newid;
			
			// Find the new mobj.
			s->cmo = Cl_FindMobj(s->mobjid);
#if _DEBUG
			Con_Message("Pl%i: mobj=%i old=%x\n", num, s->mobjid, old); 
			Con_Message("  x=%x y=%x z=%x\n", s->cmo->mo.x,
				s->cmo->mo.y, s->cmo->mo.z);
#endif
			s->cmo->mo.dplayer = pl;

#if _DEBUG
			Con_Message("Cl_RPlD: pl=%i => moid=%i\n",
				s->cmo->mo.dplayer - players, s->mobjid);
#endif

			// Unlink this cmo (not interactive or visible).
			Cl_UnsetThingPosition(&s->cmo->mo);
			// Make the old clmobj a non-player one.
			if(old) 
			{	
				old->mo.dplayer = NULL;
				Cl_SetThingPosition(&old->mo);
				Cl_UpdateRealPlayerMobj(pl->mo, &s->cmo->mo, ~0);
			}
			else
			{
				//Cl_UpdatePlayerPos(pl);

				// Replace the hidden client mobj with the real player mobj.
				Cl_UpdateRealPlayerMobj(pl->mo, &s->cmo->mo, ~0);
			}
			// Update the real player mobj.
			//Cl_UpdateRealPlayerMobj(pl->mo, &s->cmo->mo, ~0);
		}
	}
	if(df & PDF_FORWARDMOVE) s->forwardmove = (char) Msg_ReadByte() * 2048;
	if(df & PDF_SIDEMOVE) s->sidemove = (char) Msg_ReadByte() * 2048;
	if(df & PDF_ANGLE) s->angle = Msg_ReadByte() << 24;
	if(df & PDF_TURNDELTA) 
	{
		s->turndelta = ((char) Msg_ReadByte() << 24) / 16;
	}
	if(df & PDF_FRICTION) s->friction = Msg_ReadByte() << 8;
	if(df & PDF_EXTRALIGHT) 
	{
		i = Msg_ReadByte();
		pl->fixedcolormap = i & 7;	
		pl->extralight = i & 0xf8;	
	}
	if(df & PDF_FILTER) 
		pl->filter = Msg_ReadLong();
	if(df & PDF_CLYAW) // Only sent when Fixangles is used.
		pl->clAngle = Msg_ReadShort() << 16;
	if(df & PDF_CLPITCH) // Only sent when Fixangles is used.
		pl->clLookDir = Msg_ReadShort()*110.0 / DDMAXSHORT;
	if(df & PDF_PSPRITES)
	{
		for(i = 0; i < 2; i++)
		{
			// First the flags.
			psdf = Msg_ReadByte();
			psp = pl->psprites + i;
			if(psdf & PSDF_STATEPTR)
			{
				idx = Msg_ReadPackedShort();
				if(!idx) 
					psp->stateptr = 0;
				else if(idx < count_states.num)
				{
					psp->stateptr = states + (idx - 1);
					psp->tics = psp->stateptr->tics;
				}
			}
			//if(psdf & PSDF_SPRITE) psp->sprite = Msg_ReadPackedShort() - 1;
			//if(psdf & PSDF_FRAME) psp->frame = Msg_ReadByte();
			//if(psdf & PSDF_NEXT/*FRAME*/) psp->nextframe = (char) Msg_ReadByte();
			//if(psdf & PSDF_NEXT/*TIME*/) psp->nexttime = (char) Msg_ReadByte();
			//if(psdf & PSDF_TICS) psp->tics = (char) Msg_ReadByte();
			if(psdf & PSDF_LIGHT) psp->light = Msg_ReadByte() / 255.0f;
			if(psdf & PSDF_ALPHA) psp->alpha = Msg_ReadByte() / 255.0f;
			if(psdf & PSDF_STATE) psp->state = Msg_ReadByte();
			if(psdf & PSDF_OFFSET)
			{
				psp->offx = (char) Msg_ReadByte() * 2;
				psp->offy = (char) Msg_ReadByte() * 2;
			}
		}
	}
	
	// Continue reading.
	return true;
}

//==========================================================================
// Cl_ThrustMul
//	Thrust (with a multiplier).
//==========================================================================
void Cl_ThrustMul(mobj_t *mo, angle_t angle, fixed_t move, fixed_t thmul)
{
	// Make a fine angle.
    angle >>= ANGLETOFINESHIFT;
	move = FixedMul(move, thmul);
    mo->momx += FixedMul(move, finecosine[angle]); 
    mo->momy += FixedMul(move, finesine[angle]);
}

//==========================================================================
// Cl_Thrust
//==========================================================================
void Cl_Thrust(mobj_t *mo, angle_t angle, fixed_t move)
{
	Cl_ThrustMul(mo, angle, move, FRACUNIT);
}

//==========================================================================
// Cl_MovePlayer
//	Predict the movement of the given player.
//==========================================================================
void Cl_MovePlayer(ddplayer_t *pl)
{
	int num = pl - players;
	playerstate_t *st = playerstate + num;
	int time_mul = 1, old_friction;
	mobj_t *clmo = &st->cmo->mo, *mo = pl->mo;

	if(!mo) return;

	if(playback && num == consoleplayer)
	{
		// This kind of player movement can't be used with demos.
		// The local player movement is recorded into the demo file as
		// coordinates.
		return;
	}

	// If we're predicting backwards in time, reverse momentum.
	// Everything else stays the same.
	if(!pred_forward) 
	{
		time_mul = -1;
		mo->momx = -mo->momx;
		mo->momy = -mo->momy;
		mo->momz = -mo->momz;
		old_friction = st->friction;
		st->friction = FixedDiv(FRACUNIT, st->friction);
	}

	// Move.
	P_XYMovement2(mo, st);
	P_ZMovement(mo);

	// Predict change in movement (thrust). 
	// The console player is always affected by the thrust multiplier.
	// (Other players are never handled because clients only receive mobj 
	// information about non-local player movement.)
	if(num == consoleplayer)
	{
		if((mo->z <= mo->floorz || mo->ddflags & DDMF_FLY)
			&& !(pl->flags & DDPF_DEAD)) // Dead players do not move willfully.
		{
			int mul = /*(num == consoleplayer)?*/ cplr_thrust_mul /*: FRACUNIT*/;
			if(st->forwardmove)
				Cl_ThrustMul(mo, st->angle, st->forwardmove, mul);
			if(st->sidemove)
				Cl_ThrustMul(mo, st->angle - ANG90, st->sidemove, mul);
		}
		// Turn delta on move prediction angle.
		st->angle += st->turndelta * time_mul;
		mo->angle += st->turndelta * time_mul;
	}

	// Undo the reversing.
	if(!pred_forward)
	{
		mo->momx = -mo->momx;
		mo->momy = -mo->momy;
		mo->momz = -mo->momz;
		st->friction = old_friction;
	}

	// Mirror changes in the (hidden) client mobj. 
	Cl_UpdatePlayerPos(pl);
}

//==========================================================================
// Cl_UpdatePlayerPos
//	Move the (hidden, unlinked) client player mobj to the same coordinates
//	where the real mobj of the player is.
//==========================================================================
void Cl_UpdatePlayerPos(ddplayer_t *pl)
{
	int num = pl - players;
	mobj_t *clmo, *mo;

	if(!playerstate[num].cmo || !pl->mo) return; // Must have a mobj!
	clmo = &playerstate[num].cmo->mo;
	mo = pl->mo;
	clmo->angle = mo->angle;
	// The player's client mobj is not linked to any lists, so position 
	// can be updated without any hassles.
	clmo->x = mo->x;
	clmo->y = mo->y;
	clmo->z = mo->z;
	P_LinkThing(clmo, 0);	// Update subsector pointer.
	clmo->floorz = mo->floorz;
	clmo->ceilingz = mo->ceilingz;
	clmo->momx = mo->momx;
	clmo->momy = mo->momy;
	clmo->momz = mo->momz;
}

//==========================================================================
// Cl_CoordsReceived
//==========================================================================
void Cl_CoordsReceived(void)
{
	if(playback) return;
	xfix = Msg_ReadShort() << 16;
	yfix = Msg_ReadShort() << 16;
	fixtics = fixspeed;
	xfix /= fixspeed;
	yfix /= fixspeed;
}

//==========================================================================
// Cl_MoveLocalPlayer
//	Used in DEMOS. (Not in regular netgames.)
//	Applies the given dx and dy to the local player's coordinates.
//	The Z coordinate is given as the absolute viewpoint height. 
//	If onground is true, the mobj's Z will be set to floorz, and the
//	player's viewheight is set so that the viewpoint height is 'z'.
//	If onground is false, the mobj's Z will be 'z' and viewheight is zero.
//==========================================================================
void Cl_MoveLocalPlayer(int dx, int dy, int z, boolean onground)
{
//	clmobj_t *cmo = playerstate[consoleplayer].cmo;
	ddplayer_t *pl = players + consoleplayer;
	mobj_t *mo;
	int i;

	mo = pl->mo;
	if(!mo) return;

	//CON_Printf("Cl_MLP: mo->ddf=%x\n", mo->ddflags);

	// Place the new momentum in the appropriate place.
	cp_momx[gametic % LOCALCAM_WRITE_TICS] = dx;
	cp_momy[gametic % LOCALCAM_WRITE_TICS] = dy;

	// Calculate an average.
	for(mo->momx = mo->momy = i = 0; i < LOCALCAM_WRITE_TICS; i++)
	{
		mo->momx += cp_momx[i];
		mo->momy += cp_momy[i];
	}
	mo->momx /= LOCALCAM_WRITE_TICS;
	mo->momy /= LOCALCAM_WRITE_TICS;

	if(dx || dy)
	{
		//Cl_UnsetThingPosition(mo);
		P_UnlinkThing(mo); //, DDLINK_SECTOR | DDLINK_BLOCKMAP);
		mo->x += dx;
		mo->y += dy;
		P_LinkThing(mo, DDLINK_SECTOR | DDLINK_BLOCKMAP);
		//Cl_SetThingPosition(mo);
	}
	//Cl_CheckMobj(mo);

	//P_CheckPosition2(mo, mo->x, mo->y, mo->z);	
	mo->subsector = R_PointInSubsector(mo->x, mo->y);
	mo->floorz = mo->subsector->sector->floorheight; //tmfloorz;
	mo->ceilingz = mo->subsector->sector->ceilingheight; //tmceilingz;

	if(onground)
	{
		//mo->z = mo->floorz;
		//pl->viewheight = z - mo->z;
		mo->z = z - 1;
		pl->viewheight = 1;
	}
	else
	{
		mo->z = z;
		pl->viewheight = 0;
	}

	Cl_UpdatePlayerPos(players + consoleplayer);
}

//==========================================================================
// Cl_MovePsprites
//	Animates the player sprites based on their states (up, down, etc.)
//==========================================================================
void Cl_MovePsprites(void)
{
	ddplayer_t *pl = players + consoleplayer;
	ddpsprite_t *psp = pl->psprites;
	int i;

	for(i = 0; i < 2; i++) 
		if(psp[i].tics > 0) psp[i].tics--;

	switch(psp->state)
	{
	case DDPSP_UP:
		pspy -= FIX2FLT(psp_move_speed);
		if(pspy <= TOP_PSPY) 
		{
			pspy = TOP_PSPY;
			psp->state = DDPSP_BOBBING;
		}
		psp->y = pspy;
		break;
		
	case DDPSP_DOWN:
		pspy += FIX2FLT(psp_move_speed);
		if(pspy > BOTTOM_PSPY) pspy = BOTTOM_PSPY;
		psp->y = pspy;
		break;
		
	case DDPSP_FIRE:
		pspy = TOP_PSPY;
		//psp->x = 0;
		psp->y = pspy;
		break;
		
	case DDPSP_BOBBING:
		pspy = TOP_PSPY;		
		// Get bobbing from the Game DLL.
		psp->x = FIX2FLT( (int) gx.Get(DD_PSPRITE_BOB_X) );
		psp->y = FIX2FLT( (int) gx.Get(DD_PSPRITE_BOB_Y) );
		break;
	}
	if(psp->state != DDPSP_BOBBING)
	{
		if(psp->offx) psp->x = psp->offx;
		if(psp->offy) psp->y = psp->offy;
	}

	// The other psprite gets the same coords.
	psp[1].x = psp->x;
	psp[1].y = psp->y;
}