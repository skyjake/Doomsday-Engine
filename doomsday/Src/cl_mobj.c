
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
		/*mo->state = states + stnum;
		mo->tics = mo->state->tics;
		mo->sprite = mo->state->sprite;
		mo->frame = mo->state->frame;*/
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

#if 0
	// Frame in a byte.
	if(df & MDF_SPRITE) 
	{
		d->sprite = Msg_ReadPackedShort();
#if _DEBUG
		if(d->sprite < 0 || d->sprite >= numsprites)
			I_Error("Cl_ReadMobjDelta: Invalid sprite number %i.\n", d->sprite);
#endif
	}
	if(df & MDF_FRAME) 
	{
		byte b = Msg_ReadByte();
		d->frame = b & 0x7f;
		if(b & 0x80) d->frame |= 0x8000; // Fullbright flag.
	}

	// We'll pack the nextframe, tics and nexttime into a word 
	// (if all present).
	if((df & MDF_NEXTFRAME) && (df & MDF_NEXTTIME) && (df & MDF_TICS))
	{
		unsigned short s = Msg_ReadShort();
		//d->nextframe = s & 0x1f;
		d->tics = (s >> 5) & 0x1f;
		//d->nexttime = s >> 10;
	}
	else
	{
		// Write separately.
		//if(df & MDF_NEXTFRAME) d->nextframe = (char) Msg_ReadByte();
		//if(df & MDF_NEXTTIME) d->nexttime = (char) Msg_ReadByte(); 
		if(df & MDF_TICS) d->tics = (char) Msg_ReadByte();
	}
#endif

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
	
	// Only need to initialize once.
/*	if(buffers_ok) return;
	buffers_ok = true;
	Cl_InitBuffer(&cmbInput);
	Cl_InitBuffer(&cmbVisible);*/

	// List of client mobjs.
	cmRoot.next = cmRoot.prev = &cmRoot;

	Cl_InitPlayers();
}

/*void Cl_UnlinkVisibleBuffer()
{
	int i;
	mobj_t *mo;

	// Unlink all the mobjs in the visible buffer.
	for(i=0, mo=cmbVisible.list; i<cmbVisible.num; i++, mo++)
		Cl_UnsetThingPosition(mo);
}*/

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

/*void Cl_ZeroBuffer(clmobj_buffer_t *buf)
{
	buf->num = 0;
	buf->time = 0;
	buf->age = 0;
}*/

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

#if 0
void Cl_UnpackMobj(mobj_t *mobj)
{
	Cl_UnpackMobj2(0, mobj);
}

void Cl_UnpackMobj2(int unpack_flags, mobj_t *mobj)
{
	unsigned short i = Msg_ReadShort();
	byte flags = (i & MDP_FLAG_MASK) >> 8;
	boolean withdims = false;

	mobj->dplayer = 0;
	mobj->sprite = i & MDP_SPRITE_MASK;

#ifdef _DEBUG
	if(mobj->sprite < 0 || mobj->sprite >= defs.num_sprites)
	{
		I_Error("Cl_UnpackMobj2: Invalid sprite number (%i).\n", mobj->sprite);
	}
#endif

	if(flags & MDP_ACCURATE || unpack_flags & MDPMODE_ACCURATE_POS)
	{
		mobj->x = (Msg_ReadShort() << 16) + (Msg_ReadByte() << 8); 
		mobj->y = (Msg_ReadShort() << 16) + (Msg_ReadByte() << 8); 
		if(flags & MDP_POSITION_Z)
		{
			mobj->z = (Msg_ReadShort() << 16) + (Msg_ReadByte() << 8); 
		}
		else
		{
			mobj->z = DDMAXINT;	// Will be corrected after linking in.
		}
	}
	else
	{
		mobj->x = Msg_ReadShort() << 16; 
		mobj->y = Msg_ReadShort() << 16;
		if(flags & MDP_POSITION_Z)
		{
			mobj->z = Msg_ReadShort() << 16;
		}
		else
		{
			mobj->z = DDMAXINT; // Will be corrected after linking in.
		}
	}
	if(flags & MDP_MOVEMENT_XY)
	{
		mobj->momx = Msg_ReadShort() << 8; 
		mobj->momy = Msg_ReadShort() << 8;
	}
	else
	{
		mobj->momx = mobj->momy = 0;
	}
	if(flags & MDP_MOVEMENT_Z)
	{
		mobj->momz = Msg_ReadShort() << 8;
	}
	else
	{
		mobj->momz = 0;
	}
	if(flags & MDP_DIRECTION)
	{
		if(flags & MDP_ACCURATE)
		{
			mobj->angle = Msg_ReadShort() << 16;
		}
		else
		{
			mobj->angle = Msg_ReadByte() << 24;
		}
	}
	else
	{
		mobj->angle = 0;
	}
	mobj->ddflags = DDMF_REMOTE;	// Client mobjs always have this.
	mobj->floorclip = 0;
	if(flags & MDP_EXTRA_FLAGS)
	{
		i = Msg_ReadShort();
		if((i & MDPE_BRIGHTSHADOW) == MDPE_BRIGHTSHADOW) 
		{
			mobj->ddflags |= DDMF_BRIGHTSHADOW;
		}
		else
		{
			if(i & MDPE_SHADOW) mobj->ddflags |= DDMF_SHADOW;
			if(i & MDPE_ALTSHADOW) mobj->ddflags |= DDMF_ALTSHADOW;
		}
		if(i & MDPE_NOFITBOTTOM) mobj->ddflags |= DDMF_NOFITBOTTOM;
		if(i & MDPE_VIEWALIGN) mobj->ddflags |= DDMF_VIEWALIGN;
		if(i & MDPE_SOLID) 
		{
			mobj->ddflags |= DDMF_SOLID;
			withdims = true;
		}
		mobj->ddflags |= (i & MDPE_LIGHTOFFSET) >> MDPE_LIGHTOFFSETSHIFT
			<< DDMF_LIGHTOFFSETSHIFT;
		mobj->ddflags |= (i & MDPE_TRANSCLASS) >> MDPE_CLASSSHIFT
			<< DDMF_CLASSTRSHIFT;
		mobj->ddflags |= (i & MDPE_TRANSLATION) >> MDPE_TRANSSHIFT
			<< DDMF_TRANSSHIFT;
		// Extended flags?
		if(i & MDPE_EXTENDED) 
		{
			byte ex = Msg_ReadByte();
			mobj->ddflags |= (ex & MDPE2_LIGHTSCALE) >> MDPE_LIGHTSCALESHIFT 
				<< DDMF_LIGHTSCALESHIFT;
			if(ex & MDPE2_NOGRAVITY) mobj->ddflags |= DDMF_NOGRAVITY;
		}
		if(i & MDPE_FLOORCLIP)
		{
			mobj->floorclip = ((char) Msg_ReadByte()) << 14;
		}
	}
	if(flags & MDP_FRAME)
	{
		i = Msg_ReadByte();
		mobj->frame = i & MDP_FRAME_MASK;
		if(i & MDPF_FULLBRIGHT) 
		{
			mobj->frame |= FF_FULLBRIGHT;
			withdims = true;
		}
		if(i & MDPF_NEXTFRAME) 
		{
			unsigned short tmp = Msg_ReadShort();
			mobj->nextframe = tmp & 0x1f;
			mobj->tics = (tmp >> 5) & 0x1f;
			mobj->nexttime = tmp >> 10;
		}
		else
		{
			// No next frame in this case.
			mobj->nextframe = -1;
		}
	}
	else
	{
		mobj->frame = 0;
		mobj->nextframe = -1;
	}
	// The last two bytes are the mobj's height and radius.
	mobj->height = Msg_ReadByte() << FRACBITS;
	mobj->radius = Msg_ReadByte() << FRACBITS;
}
#endif

// Moves everything from the input buffer to the visible buffer.
// Leaves the input buffer empty.
/*void Cl_SwapBuffers()
{
	int		i;
	mobj_t	*mo;

	// First clear the visible buffer.
	Cl_UnlinkVisibleBuffer();

	// Update the visible buffer.
	cmbVisible.num = cmbInput.num;
	cmbVisible.age = 0;
	cmbVisible.time = cmbInput.time;

	// Copy all the mobjs from the input buffer.
	memcpy(cmbVisible.list, cmbInput.list, sizeof(mobj_t) * cmbInput.num);
	
	// Clear the input buffer.
	cmbInput.num = 0;

	// Link in all the visible mobjs.
	for(i=0, mo=cmbVisible.list; i<cmbVisible.num; i++, mo++)
	{
		Cl_SetThingPosition(mo);
		Cl_CheckMobj(mo);
	}

	// Also check the player mobjs, which were created by the Game DLL.
	for(i=0; i<MAXPLAYERS; i++)
		if(players[i].ingame && players[i].mo)
		{
			Cl_CheckMobj(players[i].mo);
		}
}*/

void Cl_HandleFrameMobjs(int time)
{
#if 0
	mobj_t	*mo, temp;
	int		count;

	// Place the new mobjs into the input buffer.
	if(time != cmbInput.time && cmbInput.num)
	{
		CON_Printf( "time:%i - mobjs left in input buffer!\n", cmbInput.time);
		// The input buffer contains mobjs and the frame has evidently
		// changed. Move everything to visible buffer before adding the
		// new mobjs. Notice that this only happens if the server's End 
		// Frame event was not received.
		Cl_SwapBuffers();
	}
	// Update the input buffer's time.
	cmbInput.time = time;

	// Read all the mobjs from the packet.
	count = Msg_ReadByte();

	//CON_Printf("HFMos: cnt=%i\n", count);	

	while(count-- > 0)
	{
		// If there is no room in the input buffer, we'll have to 
		// discard the rest of the mobjs.
		if(cmbInput.num == MAX_CLMOBJS) 
		{
			// Oh well.
			mo = &temp;
		}
		else
		{
			// Insert into the input buffer.
			mo = cmbInput.list + cmbInput.num++;
		}
		// Unpack the mobj data.
		Cl_UnpackMobj(mo);
	}
#endif
}

void Cl_HandleFramePlayer()
{
/*
	boolean ingame;
	int		flags, plrNum;
	mobj_t	*mo, dummy;
	player_moveinfo_t *pInfo;

	flags = Msg_ReadByte();
	plrNum = flags & PDP_NUMBER_MASK;
	mo = players[plrNum].mo;
	ingame = players[plrNum].ingame;
	if(!mo || !ingame) 
	{
		if(ingame)
			I_Error("Cl_HandleFramePlayer: got P#%i, ingame but no mobj!\n", plrNum);
		// Player has evidently left. We must use a bit of trickery.
		mo = &dummy;		
	}
	// Update received, reset age.
	clients[plrNum].age = 0;
	if(ingame) Cl_UnsetThingPosition(mo);
	Cl_UnpackMobj2(MDPMODE_ACCURATE_POS, mo);
	if(ingame) Cl_SetThingPosition(mo);
	mo->dplayer = &players[plrNum];
	// Command.
	pInfo = playMov + plrNum;
	if(flags & PDP_CMD)
	{
		pInfo->forwardmove = ((char) Msg_ReadByte()) * 2048;
		pInfo->sidemove = ((char) Msg_ReadByte()) * 2048;
		pInfo->angle = mo->angle;
		pInfo->turndelta = (char) Msg_ReadByte() << 20;
	}
	else
	{
		memset(pInfo, 0, sizeof(*pInfo));
	}
	// Friction.
	if(flags & PDP_FRICTION)
	{
		pInfo->friction = Msg_ReadByte() << 8;
		if(!pInfo->friction) pInfo->friction = 1;
	}
	else
	{
		// Use default friction.
		pInfo->friction = DEFAULT_FRICTION;
	}
*/
}

// A camera packet tells us the position, direction and movement 
// of the viewpoint.
void Cl_HandleFrameCamera()
{
/*	ddplayer_t *plr = &players[consoleplayer];
	byte flags = Msg_ReadByte();
	boolean fixangles = flags & CDP_FIX_ANGLES;
	player_moveinfo_t *pInfo = &playMov[consoleplayer];

	Cl_UnsetThingPosition(plr->mo);
	Cl_UnpackMobj(plr->mo);
	// Set the implied flags.
	plr->mo->ddflags |= DDMF_DONTDRAW | DDMF_SOLID; 
	if(flags & CDP_NOGRAVITY) plr->mo->ddflags |= DDMF_NOGRAVITY;
	Cl_SetThingPosition(plr->mo);
	plr->mo->dplayer = plr;	
	if(fixangles) 
	{
		plr->clAngle = plr->mo->angle;
		plr->clLookDir = Msg_ReadShort() / (float) DDMAXSHORT * 110;	
	}
	// Also unpack viewheight.
	plr->viewheight = Msg_ReadShort() << 8;
	// Color filter. 
	//if(flags & CDP_FILTER) plr->filter = Msg_ReadLong();
	// Extra light.
	plr->extralight = flags & CDP_EXTRA_LIGHT? Msg_ReadByte() : 0;
	// Command.
	if(flags & CDP_CMD)
	{
		pInfo->forwardmove = ((char) Msg_ReadByte()) * 2048;
		pInfo->sidemove = ((char) Msg_ReadByte()) * 2048;
		pInfo->angle = Msg_ReadByte() << 24;
		pInfo->turndelta = (char) Msg_ReadByte() << 20;
	}
	else
	{
		// Clear the player's cmd.
		memset(pInfo, 0, sizeof(*pInfo));
	}
	// Friction.
	if(flags & CDP_FRICTION)
	{
		pInfo->friction = Msg_ReadByte() << 8;
		if(!pInfo->friction) pInfo->friction = 1;
	}
	else
	{
		// Use default friction.
		pInfo->friction = DEFAULT_FRICTION;
	}
	clients[consoleplayer].age = 0;*/
}

void Cl_HandleFrame()
{
#if 0
	static int time_skew = 0;
	int	delta, time = Msg_ReadByte();	// Frame time (ID of sorts).
	int i, count;

	//CON_Printf("gt=%i\n", gametic);

	// The delta tells us if there will be a time warp.
	delta = Net_TimeDelta(time, previous_time + cmbVisible.age);
	previous_time = time;

	//CON_Printf("CL_HF: len=%i time = %i\n", netbuffer.length, time);

	// Get all the frame data...
	Cl_HandleFrameCamera();
	// The number of players.
	count = Msg_ReadByte();	
	for(i=0; i<count; i++) Cl_HandleFramePlayer();
	Cl_HandleFrameMobjs(time);
	// The number of missile spawn requests.
/*	count = Msg_ReadByte();
	//CON_Printf("  Missiles=%i\n", count);
	gx.NetWorldEvent(DDWE_PROJECTILE, count, netbuffer.cursor);*/
	Cl_SwapBuffers();

	if(!time_skew && delta) 
		dampencount = 0;

	// The delta skews time.
	time_skew += delta;

	if(time_skew)
	{
		dampencount++;
		// We don't want the skew to affect too many frames.
		// new_skewdampen is the limit.
		if(dampencount > net_skewdampen) time_skew = 0;
	}
	if(net_showskew)
	{
		CON_Printf("Cl_HandleFrame: D=%+04i (S=%+04i, c=%i)\n", delta,
			time_skew, dampencount);
	}
	if(time_skew) 
	{
		count = time_skew;
		// Get the absolute value.
		if(count < 0) count = -count;
		for(i=0; i<count; i++)
			Cl_PredictMovement(time_skew < 0, false);
	}
#endif
}

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
/*
void Cl_MovePlayer(int plnum)
{
	ddplayer_t *plr = players + plnum;
	player_moveinfo_t *pInfo = playMov + plnum;
	int time_factor = 1;
	fixed_t old_friction;

	// If we're predicting backwards in time, reverse momentum.
	// Everything else stays the same.
	if(!pred_forward) 
	{
		time_factor = -1;
		plr->mo->momx = -plr->mo->momx;
		plr->mo->momy = -plr->mo->momy;
		plr->mo->momz = -plr->mo->momz;
		old_friction = pInfo->friction;
		pInfo->friction = FixedDiv(FRACUNIT, pInfo->friction);
	}
	// Move first. 
	P_XYMovement2(plr->mo, pInfo);
	P_ZMovement(plr->mo);
	// Predict change in movement, if touching ground.
	if(plr->mo->z <= plr->mo->floorz)
	{
		if(pInfo->forwardmove)
			Cl_Thrust(plr->mo, pInfo->angle, pInfo->forwardmove);
		if(pInfo->sidemove)
			Cl_Thrust(plr->mo, pInfo->angle - ANG90, pInfo->sidemove);
	}
	// Turn delta on move prediction angle.
	pInfo->angle += pInfo->turndelta * time_factor;
	plr->mo->angle += pInfo->turndelta * time_factor;
	// Undo the reversing.
	if(!pred_forward)
	{
		plr->mo->momx = -plr->mo->momx;
		plr->mo->momy = -plr->mo->momy;
		plr->mo->momz = -plr->mo->momz;
		pInfo->friction = old_friction;
	}
}
*/

//===========================================================================
// Cl_AnimateThing
//	Decrements tics counter and changes the state of the thing if necessary.
//===========================================================================
void Cl_AnimateThing(mobj_t *mo)
{
	if(mo->tics < 0) return; // In statis.

	mo->tics--;
	if(mo->state && mo->tics <= 0)
	{
		// Go to next state, if possible.
		if(mo->state->nextstate >= 0)
		{
			Cl_SetThingState(mo, mo->state->nextstate);
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

/*	int			i;
	mobj_t		*mo;
	ddplayer_t	*plr;
	client_t	*cl;

	// Which direction are we predicting?
	pred_forward = forward;

	if(age_buffer)
	{
		// Make the visible buffer a tic older.
		if(forward) cmbVisible.age++; else cmbVisible.age--;
		// We can't predict too far ahead or back.
		if(cmbVisible.age > predict_tics ||
			cmbVisible.age < -predict_tics) return;
	}

	// Predict the movement of all the players.
	for(i=0, cl=clients, plr=players; i<MAXPLAYERS; i++, cl++, plr++)
		if(plr->ingame && cl->age <= predict_tics)
		{
			Cl_MovePlayer(i);
			cl->age++;
		}

	// Predict the movement of mobjs in the visible buffer.
	for(i=0, mo=cmbVisible.list; i<cmbVisible.num; i++, mo++)
		Cl_MoveThing(mo);*/
}