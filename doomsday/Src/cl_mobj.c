
//**************************************************************************
//**
//** CL_MOBJ.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_play.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int latest_frame_size, gotframe;
extern int predicted_tics;

extern playerstate_t playerstate[MAXPLAYERS];

// PUBLIC DATA DEFINITIONS -------------------------------------------------

clmobj_t cmRoot;

int	predict_tics = 70;

// Time skew tells whether client time is running before or after frame time.
int net_timeskew = 0;			
int net_skewdampen = 5;		// In frames.
boolean net_showskew = false;

boolean pred_forward = true;	// Predicting forward in time.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static byte previous_time = 0;
static int dampencount = 0;
static boolean buffers_ok = false;

// CODE --------------------------------------------------------------------

//==========================================================================
// Cl_LinkMobj
//	Links the clmobj before the root of the client mobj list. 
//	I.e. it becomes the last clmobj on the list.
//==========================================================================
void Cl_LinkMobj(clmobj_t *cmo)
{
	cmo->next = &cmRoot;
	cmo->prev = cmRoot.prev;
	cmRoot.prev = cmo;
	cmo->prev->next = cmo;
}

//==========================================================================
// Cl_UnlinkMobj
//	Unlinks the clmobj from the client mobj list.
//==========================================================================
void Cl_UnlinkMobj(clmobj_t *cmo)
{
	cmo->next->prev = cmo->prev;
	cmo->prev->next = cmo->next;
}

//==========================================================================
// Cl_FindMobj
//	Searches through the client mobj list and returns the clmobj with the
//	given ID, if that exists.
//==========================================================================
clmobj_t *Cl_FindMobj(thid_t id)
{
	clmobj_t *cmo;

	// Scan the existing client mobjs.
	for(cmo = cmRoot.next; cmo != &cmRoot; cmo = cmo->next)
		if(cmo->mo.thinker.id == id)
			return cmo;

	// Not found!
	return NULL;
}

//==========================================================================
// Cl_UnsetThingPosition
//	Unlinks the thing from sectorlinks and if the object is solid, the 
//	blockmap.
//==========================================================================
void Cl_UnsetThingPosition(mobj_t *thing)
{
	P_UnlinkThing(thing); /* , DDLINK_SECTOR | 
		(thing->ddflags & DDMF_SOLID? DDLINK_BLOCKMAP : 0)); */
}

//==========================================================================
// Cl_SetThingPosition
//	Links the thing into sectorlinks and if the object is solid, the 
//	blockmap. Linking to sectorlinks makes the thing visible and linking  
//	to the blockmap makes it possible to interact with it (collide).
//==========================================================================
void Cl_SetThingPosition(mobj_t *thing)
{
	P_LinkThing(thing, DDLINK_SECTOR |
		(thing->ddflags & DDMF_SOLID? DDLINK_BLOCKMAP : 0));
}

//===========================================================================
// Cl_SetThingState
//===========================================================================
void Cl_SetThingState(mobj_t *mo, int stnum)
{
	if(stnum < 0) return;
	do 
	{
		P_SetState(mo, stnum);
		stnum = states[stnum].nextstate;
	}
	while(!mo->tics && stnum > 0);

	// Update mobj's type (this is not perfectly reliable...)
	// from the stateowners table.
	if(stateowners[stnum])
		mo->type = stateowners[stnum] - mobjinfo;
	else
		mo->type = 0;
}

//==========================================================================
// Cl_CheckMobj
//	Updates floorz and ceilingz of the mobj.
//	Only fixes the Z coordinate of the mobj if the GOINGROUND flag
//	is not set; this is usually the case because we want things to stay
//	in sync with the floor when they are in moving sectors, etc.
//==========================================================================
void Cl_CheckMobj(mobj_t *mo)
{
#if _DEBUG
	int oldz = mo->z;
#endif

	// Find out floor and ceiling z.
	P_CheckPosition2(mo, mo->x, mo->y, mo->z);	
	mo->floorz = tmfloorz;
	mo->ceilingz = tmceilingz;

	// Fix the Z coordinate, if necessary.
	/*if(!(mo->ddflags & DDMF_GOINGROUND))
		if(mo->z < mo->floorz) mo->z = mo->floorz;*/

/*#if _DEBUG
	if(oldz != mo->z)
		CON_Printf("Cl_CheckMobj: fixed Z from %x to %x\n", oldz, mo->z);
#endif*/
}

//==========================================================================
// Cl_UpdateRealPlayerMobj
//	Makes the real player mobj identical with the client mobj.
//	The client mobj is always unlinked. Only the real mobj is visible.
//==========================================================================
void Cl_UpdateRealPlayerMobj(mobj_t *mo, mobj_t *clmo, int flags)
{
#if _DEBUG
	if(!mo || !clmo)
	{
		Con_Message("Cl_UpdateRealPlayerMobj: mo=%p clmo=%p\n", mo, clmo);
		return;
	}
#endif

	if(flags & (MDF_POS_X | MDF_POS_Y))
	{
		// We have to unlink the real mobj before we move it.
		P_UnlinkThing(mo); //, DDLINK_SECTOR | DDLINK_BLOCKMAP);
		mo->x = clmo->x;
		mo->y = clmo->y;
		P_LinkThing(mo, DDLINK_SECTOR | DDLINK_BLOCKMAP);
	}
	mo->z = clmo->z;
	mo->momx = clmo->momx;
	mo->momy = clmo->momy;
	mo->momz = clmo->momz;
	mo->angle = clmo->angle;
	mo->sprite = clmo->sprite;
	mo->frame = clmo->frame;
	//mo->nextframe = clmo->nextframe;
	mo->tics = clmo->tics;
	mo->state = clmo->state;
	//mo->nexttime = clmo->nexttime;
	mo->ddflags = clmo->ddflags;
	mo->radius = clmo->radius;
	mo->height = clmo->height;
	mo->floorclip = clmo->floorclip;
	mo->floorz = clmo->floorz;
	mo->ceilingz = clmo->ceilingz;
	mo->selector &= ~DDMOBJ_SELECTOR_MASK;
	mo->selector |= clmo->selector & DDMOBJ_SELECTOR_MASK;
	mo->visangle = clmo->angle >> 16;
	
	//if(flags & MDF_FLAGS) CON_Printf("Cl_RMD: ddf=%x\n", mo->ddflags);
}

//==========================================================================
// Cl_ReadMobjDelta
//	Reads a single mobj delta from the message buffer and applies
//	it to the client mobj in question.
//	For client mobjs that belong to players, updates the real player mobj.
//	Returns false only if the list of deltas ends.
//==========================================================================
int Cl_ReadMobjDelta(void)
{
	thid_t id = Msg_ReadShort();	// Read the ID.
	clmobj_t *cmo;
	boolean linked = true;
	int df = 0;
	mobj_t *d;

	// Stop if end marker found.
	if(!id) return false;

	// Get a mobj for this.
	cmo = Cl_FindMobj(id);
	if(!cmo)
	{
		// This is a new ID, allocate a new mobj.
		cmo = Z_Malloc(sizeof(*cmo), PU_LEVEL, 0);
		memset(cmo, 0, sizeof(*cmo));
		cmo->mo.ddflags |= DDMF_REMOTE;
		Cl_LinkMobj(cmo);
		cmo->mo.thinker.id = id;
		P_SetMobjID(id, true); // Mark this ID as used.
		linked = false;
	}

	// Flags.
	df = Msg_ReadShort();
	if(!df)
	{
		// Clear the thinker ID.
		P_SetMobjID(cmo->mo.thinker.id, false);
		// A Null Delta. We should delete this mobj.
		if(!cmo->mo.dplayer) 
			Cl_UnsetThingPosition(&cmo->mo);
		else
			playerstate[cmo->mo.dplayer - players].cmo = NULL;
		Cl_UnlinkMobj(cmo);
		Z_Free(cmo);
		return true; // Continue.
	}

	d = &cmo->mo;

	// Need to unlink? (Flags because DDMF_SOLID determines block-linking.)
	if(df & (MDF_POS_X | MDF_POS_Y | MDF_POS_Z | MDF_FLAGS) 
		&& linked
		&& !d->dplayer)
	{
		linked = false;
		Cl_UnsetThingPosition(d);
	}

	// Coordinates with three bytes.
	if(df & MDF_POS_X) 
		d->x = (Msg_ReadShort() << FRACBITS) | (Msg_ReadByte() << 8);
	if(df & MDF_POS_Y) 
		d->y = (Msg_ReadShort() << FRACBITS) | (Msg_ReadByte() << 8);
	if(df & MDF_POS_Z)
		d->z = (Msg_ReadShort() << FRACBITS) | (Msg_ReadByte() << 8);

	// Momentum using 8.8 fixed point.
	if(df & MDF_MOM_X) d->momx = (Msg_ReadShort() << 16)/256; //>> 8; 
	if(df & MDF_MOM_Y) d->momy = (Msg_ReadShort() << 16)/256; //>> 8; 
	if(df & MDF_MOM_Z) d->momz = (Msg_ReadShort() << 16)/256; //>> 8; 

	// Angles with 16-bit accuracy.
	if(df & MDF_ANGLE) d->angle = Msg_ReadShort() << 16;

	// MDF_SELSPEC is never used without MDF_SELECTOR.
	if(df & MDF_SELECTOR) d->selector = Msg_ReadPackedShort();
	if(df & MDF_SELSPEC) d->selector |= Msg_ReadByte() << 24;

	if(df & MDF_STATE) 
		Cl_SetThingState(d, (unsigned short) Msg_ReadPackedShort());

	// Pack flags into a word (3 bytes?).
	// FIXME: Do the packing!
	if(df & MDF_FLAGS) 
	{
		// Only the flags in the pack mask are affected.
		d->ddflags &= ~DDMF_PACK_MASK;
		d->ddflags |= DDMF_REMOTE | (Msg_ReadLong() & DDMF_PACK_MASK);
	}

	// Radius, height and floorclip are all bytes.
	if(df & MDF_RADIUS) d->radius = Msg_ReadByte() << FRACBITS;
	if(df & MDF_HEIGHT) d->height = Msg_ReadByte() << FRACBITS;
	if(df & MDF_FLOORCLIP) d->floorclip = Msg_ReadByte() << 14;

	// Link again.
	if(!linked && !d->dplayer)	
		Cl_SetThingPosition(d);
	
	if(df & (MDF_POS_X | MDF_POS_Y | MDF_POS_Z))
		Cl_CheckMobj(d);
	
	// Update players.
	if(d->dplayer) 
		Cl_UpdateRealPlayerMobj(d->dplayer->mo, d, df);

	// Continue reading.
	return true;
}

//==========================================================================
// Cl_InitClientMobjs
//	Initialize clientside data.
//==========================================================================
void Cl_InitClientMobjs()
{
	previous_time = gametic;
	latest_frame_size = 0;
//	memset(playMov, 0, sizeof(playMov));
	
	// List of client mobjs.
	cmRoot.next = cmRoot.prev = &cmRoot;

	Cl_InitPlayers();
}

//==========================================================================
// Cl_DestroyClientMobjs
//	Called when the client is shut down. Unlinks everything from the 
//	sectors and the blockmap and clears the clmobj list.
//==========================================================================
void Cl_DestroyClientMobjs()
{
	clmobj_t *cmo;

	for(cmo = cmRoot.next; cmo != &cmRoot; cmo = cmo->next)
	{ 
		// Players' clmobjs are not linked anywhere.
		if(!cmo->mo.dplayer)
			Cl_UnsetThingPosition(&cmo->mo);
	}

	//Cl_UnlinkVisibleBuffer();
	Cl_CleanClientMobjs();
}

//==========================================================================
// Cl_CleanClientMobjs
//	Clear the clmobj list. Called when the level changes.
//==========================================================================
void Cl_CleanClientMobjs()
{
	// Make sure the currently waiting frame is discarded.
	latest_frame_size = 0;
	gotframe = false;

	cmRoot.next = cmRoot.prev = &cmRoot;

	// Clear player data, too, since we just lost all clmobjs.
	Cl_InitPlayers();
}

//===========================================================================
// Cl_MoveThing
//===========================================================================
void Cl_MoveThing(mobj_t *mobj)
{
	boolean needlink = false;

	// Is the thing moving at all?
	if(!mobj->momx && !mobj->momy && !mobj->momz) return;

	// Does it need to be un/linked?
	if(mobj->momx || mobj->momy) needlink = true;

	// Link out of lists if necessary.
	if(needlink) Cl_UnsetThingPosition(mobj);

	// Do the predicted move.
	if(pred_forward)
	{
		mobj->x += mobj->momx;
		mobj->y += mobj->momy;
		mobj->z += mobj->momz;
	}
	else // Backwards in time.
	{
		mobj->x -= mobj->momx;
		mobj->y -= mobj->momy;
		mobj->z -= mobj->momz;
	}
	
	// Link back to lists if necessary.
	if(needlink) Cl_SetThingPosition(mobj);
}

//===========================================================================
// Cl_AnimateThing
//	Decrements tics counter and changes the state of the thing if necessary.
//===========================================================================
void Cl_AnimateThing(mobj_t *mo)
{
	if(!mo->state || mo->tics < 0) return; // In stasis.

	mo->tics--;
	if(mo->tics <= 0)
	{
		// Go to next state, if possible.
		if(mo->state->nextstate >= 0)
		{
			Cl_SetThingState(mo, mo->state->nextstate);

			// Players have both client mobjs and regular mobjs. This
			// routine modifies the *client* mobj, so for players we need
			// to update the real, visible mobj as well.
			if(mo->dplayer) 
				Cl_UpdateRealPlayerMobj(mo->dplayer->mo, mo, 0);
		}
		else
		{
			// Freeze it; the server will probably to remove it soon.
			mo->tics = -1;
		}
	}
}

//===========================================================================
// Cl_PredictMovement
//	'forward' is true if we're predicting forward in time. At the moment
//	backwards predicting is not done.
//===========================================================================
void Cl_PredictMovement(boolean forward, boolean age_buffer)
{
	clmobj_t *cmo, *next;

	pred_forward = forward;

	if(age_buffer)
	{
		// Only predict up to a certain number of tics.
		if(predicted_tics == predict_tics) return;
		// Predict one tic more.
		if(forward) predicted_tics++; else predicted_tics--;
	}

	// Move all client mobjs.
	for(cmo = cmRoot.next; cmo != &cmRoot; cmo = cmo->next)
	{
		// The local player gets a bit more complex prediction.
		if(cmo->mo.dplayer) 
			Cl_MovePlayer(cmo->mo.dplayer);			
		else // Simple prediction, then.
			Cl_MoveThing(&cmo->mo);

		// Tic away.
		Cl_AnimateThing(&cmo->mo);

		// Update the visual angle of the mobj (no SRVO action).
		cmo->mo.visangle = cmo->mo.angle >> 16;
	}

	// Remove mobjs that are at state NULL.
	for(cmo = cmRoot.next; cmo != &cmRoot; cmo = next)
	{
		next = cmo->next;
		// Don't touch players here.
		if(cmo->mo.dplayer) continue; 
		if(cmo->mo.state == states)
		{
			if(cmo->mo.thinker.id)
				P_SetMobjID(cmo->mo.thinker.id, false);
			Cl_UnsetThingPosition(&cmo->mo);
			Cl_UnlinkMobj(cmo);
			Z_Free(cmo);
		}
	}
}