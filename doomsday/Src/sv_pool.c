
//**************************************************************************
//**
//** SV_POOL.C
//**
//**************************************************************************

/*

Delta Pools use PU_LEVEL, which means all the memory allocated for them
is deallocated when the level changes. Sv_InitPools is called in 
DD_SetupLevel to clear out all the old data.

* Real state vs. Register+Pool -> Changes in the world?
* Changes in the world -> Deltas
* Deltas -> Sent to client as a Set, placed in the Pool
* Client sends Ack -> Delta Set removed from Pool, applied to Register
* Client doesn't send Ack -> Delta Set resent

*/

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_refresh.h"
#include "de_play.h"

// MACROS ------------------------------------------------------------------

//#define FIXED8_8(x)			(((x) & 0xffff00) >> 8)
#define FIXED8_8(x)			(((x)*256) >> 16)
#define CLAMPED_CHAR(x)		((x)>127? 127 : (x)<-128? -128 : (x))

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void Sv_InitialWorld(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

pool_t pools[MAXPLAYERS];

// Number of tics to wait before resending an unacknowledged delta set.
int net_resendtime = 2*35;
int net_showsets = false;
int net_maxdif = 96;
int net_mindif = 12;
int net_minsecupd = 2;
int net_fullsecupd = 4;
int net_maxsecupd = 8;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean pools_inited = false;

// Initial state of the world (this is what new clients see).
static sectordelta_t *secInit;
static sidedelta_t *sideInit;
static polydelta_t *polyInit;

// CODE --------------------------------------------------------------------

//==========================================================================
// Sv_InitDeltaChain
//==========================================================================
void Sv_InitDeltaChain(delta_t *root)
{
	root->next = root->prev = root;
}

//==========================================================================
// Sv_InitPools
//	Called once for each level, from DD_SetupLevel.
//==========================================================================
void Sv_InitPools(void)
{
	int i;

	pools_inited = true;
	memset(&pools, 0, sizeof(pools));
	
	// Store the initial state of the world.
	Sv_InitialWorld();

	// Init the pools.
	for(i = 0; i < MAXPLAYERS; i++)
	{
		// The actual Pool contains all the Delta Sets.
		Sv_InitDeltaChain(&pools[i].setRoot);
	
		// The registers keep track what the client is seeing.
		Sv_InitDeltaChain(&pools[i].mobjReg);
		Sv_InitDeltaChain(&pools[i].playReg);

		// Init the world only for players that are in the game.
		if(!players[i].ingame) continue;

		// Initialize the registers with known world data.
		Sv_InitPoolForClient(i);
	}
}

//==========================================================================
// Sv_GetRegisteredMobj
//==========================================================================
int Sv_GetRegisteredMobj(pool_t *pool, thid_t id, mobjdelta_t *state)
{
	delta_t *dt;
	mobjdelta_t *mdt;

	// Clear the state. 
	memset(state, 0, sizeof(*state));

	// Scan the pool's mobj register.
	for(dt = pool->mobjReg.next; dt != &pool->mobjReg; dt = dt->next)
	{
		mdt = (mobjdelta_t*) dt;
		if(mdt->data.thinker.id == id)
		{
			// This is what we are looking for.
			memcpy(state, mdt, sizeof(*mdt));
			return true;
		}
	}
	return false;
}

//==========================================================================
// Sv_GetRegisteredPlayer
//==========================================================================
int Sv_GetRegisteredPlayer(pool_t *pool, int num, playerdelta_t *state)
{
	delta_t *dt;
	playerdelta_t *pdt;

	// Clear the state. 
	memset(state, 0, sizeof(*state));
	state->player = num;

	// Scan the pool's player register.
	for(dt = pool->playReg.next; dt != &pool->playReg; dt = dt->next)
	{
		pdt = (playerdelta_t*) dt;
		if(pdt->player == num)
		{
			// This is what we are looking for.
			memcpy(state, pdt, sizeof(*pdt));
			return true;
		}
	}
	return false;
}

//==========================================================================
// Sv_GetRegisteredSector
//==========================================================================
int Sv_GetRegisteredSector(pool_t *pool, int num, sectordelta_t *state)
{
	memcpy(state, pool->secReg + num, sizeof(*state));
	return true;
}

//==========================================================================
// Sv_GetRegisteredSide
//==========================================================================
int Sv_GetRegisteredSide(pool_t *pool, int num, sidedelta_t *state)
{
	memcpy(state, pool->sideReg + num, sizeof(*state));
	return true;
}

//==========================================================================
// Sv_GetRegisteredPoly
//==========================================================================
int Sv_GetRegisteredPoly(pool_t *pool, int num, polydelta_t *state)
{
	memcpy(state, pool->polyReg + num, sizeof(*state));
	return true;
}

//==========================================================================
// Sv_ApplyMobjDelta
//	Applies a delta on a state.
//==========================================================================
void Sv_ApplyMobjDelta(mobj_t *state, mobjdelta_t *delta)
{
	int df = delta->delta.flags;
	mobj_t *d = &delta->data;

	// *Always* set the player pointer.
	state->dplayer = d->dplayer;
	if(df & (MDF_POS_X | MDF_POS_Y)) state->subsector = d->subsector;

	if(df & MDF_POS_X) state->x = d->x;
	if(df & MDF_POS_Y) state->y = d->y;
	if(df & MDF_POS_Z) state->z = d->z;
	if(df & MDF_MOM_X) state->momx = d->momx;
	if(df & MDF_MOM_Y) state->momy = d->momy;
	if(df & MDF_MOM_Z) state->momz = d->momz;
	if(df & MDF_ANGLE) state->angle = d->angle;
	if(df & MDF_SELECTOR) state->selector = d->selector;
	if(df & MDF_STATE) 
	{
		state->state = d->state;
		state->tics = d->state->tics;
	}
	//if(df & MDF_TICS) state->tics = d->tics;
	//if(df & MDF_SPRITE) state->sprite = d->sprite;
	//if(df & MDF_FRAME) state->frame = d->frame;
	//if(df & MDF_NEXTFRAME) state->nextframe = d->nextframe;
	//if(df & MDF_NEXTTIME) state->nexttime = d->nexttime;
	if(df & MDF_RADIUS) state->radius = d->radius;
	if(df & MDF_HEIGHT) state->height = d->height;
	if(df & MDF_FLAGS) state->ddflags = d->ddflags;
	if(df & MDF_FLOORCLIP) state->floorclip = d->floorclip;
}

//==========================================================================
// Sv_ApplyPlayerDelta
//	Applies a delta on a player.
//==========================================================================
void Sv_ApplyPlayerDelta(playerdelta_t *state, playerdelta_t *d)
{
	int df = d->delta.flags;
	int i, off;

	if(df & PDF_MOBJ) state->mobjid = d->mobjid;
	if(df & PDF_FORWARDMOVE) state->forwardmove = d->forwardmove;
	if(df & PDF_SIDEMOVE) state->sidemove = d->sidemove;
	if(df & PDF_ANGLE) state->angle = d->angle;
	if(df & PDF_TURNDELTA) state->turndelta = d->turndelta;
	if(df & PDF_FRICTION) state->friction = d->friction;
	if(df & PDF_EXTRALIGHT) 
	{
		state->extralight = d->extralight;
		state->fixedcolormap = d->fixedcolormap;
	}
	if(df & PDF_FILTER) state->filter = d->filter;
	if(df & PDF_CLYAW) state->clyaw = d->clyaw;
	if(df & PDF_CLPITCH) state->clpitch = d->clpitch;
	if(df & PDF_PSPRITES)
	{
		for(i=0; i<2; i++)
		{
			off = 16 + i*8;
			/*if(df & (PSDF_SPRITE << off)) 
				state->psp[i].sprite = d->psp[i].sprite;
			if(df & (PSDF_FRAME << off)) 
				state->psp[i].frame = d->psp[i].frame;
			if(df & (PSDF_NEXT << off))
			{
				state->psp[i].nextframe = d->psp[i].nextframe;
			//if(df & (PSDF_NEXT << off))
				state->psp[i].nexttime = d->psp[i].nexttime;
			}
			if(df & (PSDF_TICS << off)) 
				state->psp[i].tics = d->psp[i].tics;*/
			if(df & (PSDF_STATEPTR << off))
			{
				state->psp[i].stateptr = d->psp[i].stateptr;
				state->psp[i].tics = d->psp[i].stateptr? d->psp[i].stateptr->tics : 0;
			}
			if(df & (PSDF_LIGHT << off)) 
				state->psp[i].light = d->psp[i].light;
			if(df & (PSDF_ALPHA << off)) 
				state->psp[i].alpha = d->psp[i].alpha;
			if(df & (PSDF_STATE << off)) 
				state->psp[i].state = d->psp[i].state;
			if(df & (PSDF_OFFSET << off))
			{
				state->psp[i].offx = d->psp[i].offx;
				state->psp[i].offy = d->psp[i].offy;
			}
		}
	}
}

//==========================================================================
// Sv_ApplySectorDelta
//	Applies a delta on a (registered) sector state.
//==========================================================================
void Sv_ApplySectorDelta(sectordelta_t *state, sectordelta_t *d)
{
	int df = d->delta.flags;

	state->number = d->number;

	if(df & SDF_FLOORPIC) state->floorpic = d->floorpic;
	if(df & SDF_CEILINGPIC) state->ceilingpic = d->ceilingpic;
	if(df & SDF_LIGHT) state->lightlevel = d->lightlevel;
	if(df & SDF_FLOOR_TARGET) 
		state->planes[PLN_FLOOR].target = d->planes[PLN_FLOOR].target;
	if(df & SDF_FLOOR_SPEED)
		state->planes[PLN_FLOOR].speed = d->planes[PLN_FLOOR].speed;
	if(df & SDF_FLOOR_TEXMOVE)
	{
		memcpy(state->planes[PLN_FLOOR].texmove,
			d->planes[PLN_FLOOR].texmove, sizeof(int)*2);
	}
	if(df & SDF_CEILING_TARGET) 
		state->planes[PLN_CEILING].target = d->planes[PLN_CEILING].target;
	if(df & SDF_CEILING_SPEED)
		state->planes[PLN_CEILING].speed = d->planes[PLN_CEILING].speed;
	if(df & SDF_CEILING_TEXMOVE)
	{
		memcpy(state->planes[PLN_CEILING].texmove,
			d->planes[PLN_CEILING].texmove, sizeof(int)*2);
	}
	if(df & SDF_COLOR_RED) state->rgb[0] = d->rgb[0];
	if(df & SDF_COLOR_GREEN) state->rgb[1] = d->rgb[1];
	if(df & SDF_COLOR_BLUE) state->rgb[2] = d->rgb[2];
}

//==========================================================================
// Sv_ApplySideDelta
//	Applies a delta on a side.
//==========================================================================
void Sv_ApplySideDelta(sidedelta_t *state, sidedelta_t *d)
{
	int df = d->delta.flags;

	state->number = d->number;

	if(df & SIDF_TOPTEX) state->toptexture = d->toptexture;
	if(df & SIDF_MIDTEX) state->midtexture = d->midtexture;
	if(df & SIDF_BOTTOMTEX) state->bottomtexture = d->bottomtexture;
}

//==========================================================================
// Sv_ApplyPolyDelta
//	Applies a delta on a poly.
//==========================================================================
void Sv_ApplyPolyDelta(polydelta_t *state, polydelta_t *d)
{
	int df = d->delta.flags;

	state->number = d->number;

	if(df & PODF_DEST_X) state->dest.x = d->dest.x;
	if(df & PODF_DEST_Y) state->dest.y = d->dest.y;
	if(df & PODF_SPEED) state->speed = d->speed;
	if(df & PODF_DEST_ANGLE) state->destAngle = d->destAngle;
	if(df & PODF_ANGSPEED) state->angleSpeed = d->angleSpeed;
}

//==========================================================================
// Sv_ApplyDeltas
//	Scan the sets of the pool and apply any deltas for the state.
//==========================================================================
void Sv_ApplyDeltas(pool_t *pool, int type, int id, delta_t *state)
{
	delta_t *dt;
	boolean done = false;

	for(dt = pool->setRoot.next; !done && dt != &pool->setRoot; dt = dt->next)
	{
		if(dt->type != type) continue;
		// If the IDs match, apply the delta on the given state.
		switch(type)
		{
		case DT_MOBJ:
			if( ((mobjdelta_t*)dt)->data.thinker.id == id )
			{
				Sv_ApplyMobjDelta(&((mobjdelta_t*)state)->data, 
					(mobjdelta_t*) dt);
				done = true;
			}
			break;

		case DT_PLAYER:
			if( ((playerdelta_t*)dt)->player == id )
			{
				Sv_ApplyPlayerDelta((playerdelta_t*) state, 
					(playerdelta_t*) dt);
				done = true;
			}
			break;

		case DT_SECTOR:
			if( ((sectordelta_t*)dt)->number == id )
			{
				Sv_ApplySectorDelta((sectordelta_t*) state, 
					(sectordelta_t*) dt);
				done = true;
			}
			break;

		case DT_SIDE:
			if( ((sidedelta_t*)dt)->number == id )
			{
				Sv_ApplySideDelta((sidedelta_t*) state, (sidedelta_t*) dt);
				done = true;
			}
			break;

		case DT_POLY:
			if( ((polydelta_t*)dt)->number == id )
			{
				Sv_ApplyPolyDelta((polydelta_t*) state, (polydelta_t*) dt);
				done = true;
			}
			break;
		}
	}
}

//==========================================================================
// Sv_CompareMobj
//==========================================================================
void Sv_CompareMobj(mobj_t *mo, mobjdelta_t *state, mobjdelta_t *delta)
{
	mobj_t *s = &state->data;
	int df = 0;

	// The data of the real state is what will be used.
	memcpy(&delta->data, mo, sizeof(mobj_t));

	// Determine which data is different.
	if(mo->x != s->x) df |= MDF_POS_X;
	if(mo->y != s->y) df |= MDF_POS_Y;
	if(mo->z != s->z) df |= MDF_POS_Z;
	if(mo->momx != s->momx) df |= MDF_MOM_X;
	if(mo->momy != s->momy) df |= MDF_MOM_Y;
	if(mo->momz != s->momz) df |= MDF_MOM_Z;
	if(mo->angle != s->angle) df |= MDF_ANGLE;
	//if(mo->tics != s->tics) df |= MDF_TICS;
	//if(mo->sprite != s->sprite) df |= MDF_SPRITE;
	//if(mo->frame != s->frame) df |= MDF_FRAME;
	//if(mo->nextframe != s->nextframe) df |= MDF_NEXTFRAME;
	//if(mo->nexttime != s->nexttime) df |= MDF_NEXTTIME;
	if(mo->selector != s->selector) df |= MDF_SELECTOR;
	if(!Def_SameStateSequence(mo->state, s->state)) df |= MDF_STATE;
	if(mo->radius != s->radius) df |= MDF_RADIUS;
	if(mo->height != s->height) df |= MDF_HEIGHT;
	if((mo->ddflags & DDMF_PACK_MASK) != (s->ddflags & DDMF_PACK_MASK)) 
		df |= MDF_FLAGS;
	if(mo->floorclip != s->floorclip) df |= MDF_FLOORCLIP;

	delta->delta.flags = df;
}

//==========================================================================
// Sv_ComparePlayer
//==========================================================================
void Sv_ComparePlayer(int num, playerdelta_t *s, playerdelta_t *d)
{
	ddplayer_t *p = players + num;
	client_t *c = clients + num;
	int df = 0, i, off;
	ddpsprite_t *dps, *sps;

	// Set the data of the delta.
	d->player = num;
	d->mobjid = p->mo->thinker.id;
	d->forwardmove = c->lastcmd->forwardmove;
	d->sidemove = c->lastcmd->sidemove;
	d->angle = p->mo->angle;
	d->turndelta = p->mo->angle - p->lastangle;	
	d->friction = gx.MobjFriction? gx.MobjFriction(p->mo) : DEFAULT_FRICTION;		
	d->extralight = p->extralight;
	d->fixedcolormap = p->fixedcolormap;
	d->filter = p->filter;
	d->clyaw = p->mo->angle;
	d->clpitch = p->lookdir;
	memcpy(d->psp, p->psprites, sizeof(ddpsprite_t) * 2);

	// Determine which data is different.
	if(d->mobjid != s->mobjid) df |= PDF_MOBJ;
	if(d->forwardmove != s->forwardmove) df |= PDF_FORWARDMOVE;
	if(d->sidemove != s->sidemove) df |= PDF_SIDEMOVE;
	if(d->angle != s->angle) df |= PDF_ANGLE;
	if(d->turndelta != s->turndelta) df |= PDF_TURNDELTA;
	if(d->friction != s->friction) df |= PDF_FRICTION;
	if(d->extralight != s->extralight
		|| d->fixedcolormap != s->fixedcolormap) df |= PDF_EXTRALIGHT;
	if(d->filter != s->filter) df |= PDF_FILTER;
	if(d->clyaw != s->clyaw) df |= PDF_CLYAW;
	if(d->clpitch != s->clpitch) df |= PDF_CLPITCH;

	// The player sprites are a bit more complicated to check.
	for(i = 0; i < 2; i++)
	{
		off = 16 + i*8;
		dps = d->psp + i;
		sps = s->psp + i;
		//if(dps->sprite != sps->sprite) df |= PSDF_SPRITE << off; 
		//if(dps->frame != sps->frame) df |= PSDF_FRAME << off;
		//if(dps->nextframe != sps->nextframe) df |= PSDF_NEXT/*FRAME*/ << off;
		//if(dps->nexttime != sps->nexttime) df |= PSDF_NEXT/*TIME*/ << off;
		//if(dps->tics != sps->tics) df |= PSDF_TICS << off;
		
		//if(!Sv_SameStateSequence(dps->stateptr, sps->stateptr)) 
		if(dps->stateptr != sps->stateptr) df |= PSDF_STATEPTR << off;

		if(dps->light != sps->light) df |= PSDF_LIGHT << off;
		if(dps->alpha != sps->alpha) df |= PSDF_ALPHA << off;
		if(dps->state != sps->state) df |= PSDF_STATE << off;
		if((dps->offx != sps->offx || dps->offy != sps->offy) 
			&& !i) df |= PSDF_OFFSET << off;
	}
	// Check for any psprite flags.
	if(df & 0xffff0000) df |= PDF_PSPRITES;

	d->delta.flags = df;
}

//==========================================================================
// Sv_CompareSector
//==========================================================================
void Sv_CompareSector(int num, sectordelta_t *s, sectordelta_t *d)
{
	sector_t *sec = SECTOR_PTR(num);
	int df = 0;

	// Set the data of the delta.
	d->number = num;
	d->floorpic = sec->floorpic;
	d->ceilingpic = sec->ceilingpic;
	d->lightlevel = sec->lightlevel;
	memcpy(d->rgb, sec->rgb, 3);
	memcpy(d->planes, sec->planes, sizeof(sec->planes));

	// Determine which data is different.
	if(d->floorpic != s->floorpic) df |= SDF_FLOORPIC;
	if(d->ceilingpic != s->ceilingpic) df |= SDF_CEILINGPIC;
	if(d->lightlevel != s->lightlevel) df |= SDF_LIGHT;
	if(d->rgb[0] != s->rgb[0]) df |= SDF_COLOR_RED;
	if(d->rgb[1] != s->rgb[1]) df |= SDF_COLOR_GREEN;
	if(d->rgb[2] != s->rgb[2]) df |= SDF_COLOR_BLUE;

	// Check planes, too.
	if(d->planes[PLN_FLOOR].target != s->planes[PLN_FLOOR].target)
		df |= SDF_FLOOR_TARGET;
	if(d->planes[PLN_FLOOR].speed != s->planes[PLN_FLOOR].speed)
		df |= SDF_FLOOR_SPEED;
	if(d->planes[PLN_FLOOR].texmove[0] != s->planes[PLN_FLOOR].texmove[0]
		|| d->planes[PLN_FLOOR].texmove[1] != s->planes[PLN_FLOOR].texmove[1])
		df |= SDF_FLOOR_TEXMOVE;
	if(d->planes[PLN_CEILING].target != s->planes[PLN_CEILING].target)
		df |= SDF_CEILING_TARGET;
	if(d->planes[PLN_CEILING].speed != s->planes[PLN_CEILING].speed)
		df |= SDF_CEILING_SPEED;
	if(d->planes[PLN_CEILING].texmove[0] != s->planes[PLN_CEILING].texmove[0]
		|| d->planes[PLN_CEILING].texmove[1] != s->planes[PLN_CEILING].texmove[1])
		df |= SDF_CEILING_TEXMOVE;

	d->delta.flags = df;
}

//==========================================================================
// Sv_CompareSide
//==========================================================================
void Sv_CompareSide(int num, sidedelta_t *s, sidedelta_t *d)
{
	side_t *sid = SIDE_PTR(num);
	int df = 0;

	// Set the data of the delta.
	d->number = num;
	d->toptexture = sid->toptexture;
	d->midtexture = sid->midtexture;
	d->bottomtexture = sid->bottomtexture;

	// Determine which data is different.
	if(d->toptexture != s->toptexture) df |= SIDF_TOPTEX;
	if(d->midtexture != s->midtexture) df |= SIDF_MIDTEX;
	if(d->bottomtexture != s->bottomtexture) df |= SIDF_BOTTOMTEX;

	d->delta.flags = df;
}

//==========================================================================
// Sv_ComparePoly
//==========================================================================
void Sv_ComparePoly(int num, polydelta_t *s, polydelta_t *d)
{
	polyobj_t *poly = PO_PTR(num);
	int df = 0;

	// Set the data of the delta.
	d->number = num;
	d->dest.x = poly->dest.x;
	d->dest.y = poly->dest.y;
	d->speed = poly->speed;
	d->destAngle = poly->destAngle;
	d->angleSpeed = poly->angleSpeed;

	// What is different?
	if(d->dest.x != s->dest.x) df |= PODF_DEST_X;
	if(d->dest.y != s->dest.y) df |= PODF_DEST_Y;
	if(d->speed != s->speed) df |= PODF_SPEED;
	if(d->destAngle != s->destAngle) df |= PODF_DEST_ANGLE;
	if(d->angleSpeed != s->angleSpeed) df |= PODF_ANGSPEED;

	d->delta.flags = df;
}

//==========================================================================
// Sv_SubtractDelta
//	Subtracts 'delta' from 'from'.
//	Subtracting means that if a given flag is defined for both 'from'
//	and 'delta', the flag for 'from' is cleared ('delta' overrides 'from').
//	The result is that the deltas can be applied in any order and the
//	result is still correct.
//==========================================================================
void Sv_SubtractDelta(delta_t *from, delta_t *delta)
{
	// Clear the common bits of 'from'.
	from->flags &= ~(from->flags & delta->flags);
}

//==========================================================================
// Sv_LinkDelta
//==========================================================================
void Sv_LinkDelta(delta_t *root, delta_t *delta)
{
	delta->next = root;
	delta->prev = root->prev;
	root->prev = delta;
	delta->prev->next = delta;
}

//==========================================================================
// Sv_UnlinkDelta
//==========================================================================
void Sv_UnlinkDelta(delta_t *delta)
{
	delta->next->prev = delta->prev;
	delta->prev->next = delta->next;
}

//==========================================================================
// Sv_AddMobjDelta
//	Makes a copy of the given delta and adds it to the Sets.
//==========================================================================
void Sv_AddMobjDelta(pool_t *pool, mobjdelta_t *delta)
{
	mobjdelta_t *md = Z_Malloc(sizeof(mobjdelta_t), PU_LEVEL, 0);

	memcpy(md, delta, sizeof(*md));
	md->delta.type = DT_MOBJ;
	md->delta.set = pool->setNumber;
	Sv_LinkDelta(&pool->setRoot, &md->delta);	// Link it in.
}

//==========================================================================
// Sv_AddPlayerDelta
//	Makes a copy of the given delta and adds it to the Sets.
//==========================================================================
void Sv_AddPlayerDelta(pool_t *pool, playerdelta_t *delta)
{
	playerdelta_t *pd = Z_Malloc(sizeof(playerdelta_t), PU_LEVEL, 0);

	memcpy(pd, delta, sizeof(*pd));
	pd->delta.type = DT_PLAYER;
	pd->delta.set = pool->setNumber;
	Sv_LinkDelta(&pool->setRoot, &pd->delta);	// Link it in.
}

//==========================================================================
// Sv_AddSectorDelta
//	Makes a copy of the given delta and adds it to the Sets.
//==========================================================================
void Sv_AddSectorDelta(pool_t *pool, sectordelta_t *delta)
{
	sectordelta_t *sd = Z_Malloc(sizeof(sectordelta_t), PU_LEVEL, 0);

	memcpy(sd, delta, sizeof(*sd));
	sd->delta.type = DT_SECTOR;
	sd->delta.set = pool->setNumber;
	Sv_LinkDelta(&pool->setRoot, &sd->delta);	// Link it in.
}

//==========================================================================
// Sv_AddSideDelta
//	Makes a copy of the given delta and adds it to the Sets.
//==========================================================================
void Sv_AddSideDelta(pool_t *pool, sidedelta_t *delta)
{
	sidedelta_t *sd = Z_Malloc(sizeof(sidedelta_t), PU_LEVEL, 0);

	memcpy(sd, delta, sizeof(*sd));
	sd->delta.type = DT_SIDE;
	sd->delta.set = pool->setNumber;
	Sv_LinkDelta(&pool->setRoot, &sd->delta);	// Link it in.
}

//==========================================================================
// Sv_AddPolyDelta
//	Makes a copy of the given delta and adds it to the Sets.
//==========================================================================
void Sv_AddPolyDelta(pool_t *pool, polydelta_t *delta)
{
	polydelta_t *sd = Z_Malloc(sizeof(polydelta_t), PU_LEVEL, 0);

	memcpy(sd, delta, sizeof(*sd));
	sd->delta.type = DT_POLY;
	sd->delta.set = pool->setNumber;
	Sv_LinkDelta(&pool->setRoot, &sd->delta);	// Link it in.
}

//===========================================================================
// Sv_AddLumpDelta
//===========================================================================
void Sv_AddLumpDelta(pool_t *pool, lumpdelta_t *delta)
{
	lumpdelta_t *ld = Z_Malloc(sizeof(lumpdelta_t), PU_LEVEL, 0);

	memcpy(ld, delta, sizeof(*ld));
	ld->delta.type = DT_LUMP;
	ld->delta.set = pool->setNumber;
	Sv_LinkDelta(&pool->setRoot, &ld->delta);	// Link it in.
}

//==========================================================================
// Sv_GenMobjDelta
//	Compares the given mobj's state (which is the real state) against the
//	state registered in the pool. The Excluded flags are never included,
//	Forced ones are always included.
//==========================================================================
void Sv_GenMobjDelta(pool_t *pool, mobj_t *mo, int exclude, int force)
{
	mobjdelta_t regstate;
	mobjdelta_t dt;

	// Retrieve the registered state. 
	Sv_GetRegisteredMobj(pool, mo->thinker.id, &regstate);

	// Apply the deltas already waiting in the pool.
	Sv_ApplyDeltas(pool, DT_MOBJ, mo->thinker.id, &regstate.delta);

	// Do the compare. 
	Sv_CompareMobj(mo, &regstate, &dt);

	// Remove the excluded data.
	dt.delta.flags &= ~exclude;
	dt.delta.flags |= force;	// And include forced data.
	
	// The resulting delta will be added to the pool.
	// It will be given the number of the current set.
	if(dt.delta.flags) // Something changed?
	{
		// Add it to the pool, in the current set.
		Sv_AddMobjDelta(pool, &dt);
	}
}

//==========================================================================
// Sv_IsVisible
//	Returns true if the mobj is visible from the viewpoint.
//	If the mobj is close enough it is always considered visible.
//	FIXME: Make the limits console variables.
//==========================================================================
boolean Sv_IsVisible(mobj_t *mo, mobj_t *view)
{
	int dist = P_ApproxDistance(mo->x - view->x, mo->y - view->y) >> 16;

	if(dist > FAR_MOBJ_DIST) return false;
	/*
	if(dist > CLOSE_MOBJ_DIST)
	{		
		// Check using line of sight.
		if(!P_CheckSight(view, mo)) return false;
	}
	// Visible!*/
	return true;
}

//==========================================================================
// Sv_GenMobjDeltas
//	Mobj traversing is done with the thinker list.
//	Also the register is checked for things that are visible on clientside.
//	This way all visible things are in the deltas.
//==========================================================================
void Sv_GenMobjDeltas(int playerNum)
{
	pool_t *pool = pools + playerNum;
	mobj_t *pmo = players[clients[playerNum].viewConsole].mo, *iter;
	thinker_t *th;
//	delta_t *dt;
	int ex, inc;
	
	for(th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		if(!P_IsMobjThinker(th->function)) continue;
		iter = (mobj_t*) th;
		// Some objects obviously should not be sent.
		if(iter->ddflags & DDMF_LOCAL /* | DDMF_DONTDRAW)*/) 
			continue;
		// Information for other players is only sent if 'send_all_players' 
		// is set to true (typically in co-op games).
		if(send_all_players && iter->dplayer 
			|| pmo == iter				// Hey, this is my mobj!
			|| Sv_IsVisible(iter, pmo))	// Everything close by passes.
		{
			ex = inc = 0;
			if(pmo == iter)
			{
				ex = MDF_CAMERA_EXCLUDE;
				// Should the coordinates be included?
				if(players[playerNum].flags & DDPF_FIXPOS)
				{
					// FixPos forced the inclusion of position and momentum.
					// They are always sent, whether they have changed or not.
					inc = MDF_POS | MDF_MOM;
				}
				else
				{
					// No; exclude pos and momentum.
					ex |= MDF_POS | MDF_MOM;
				}
				/*else
					Con_Printf("Fixpos for %i: moid=%i x=%x y=%x z=%x\n",
						playerNum, pmo->thinker.id, pmo->x, pmo->y, pmo->z);*/
			}
			// If the mobj won't be visible, let's leave out some data.
			if(iter->ddflags & DDMF_DONTDRAW) ex |= MDF_DONTDRAW_EXCLUDE;
			// Generate a Delta for this mobj.
			Sv_GenMobjDelta(pool, iter, ex, inc);
		}
		else 
		{
			mobjdelta_t mod;
			if(Sv_GetRegisteredMobj(pool, iter->thinker.id, &mod))
			{
				ex = 0;
				if(Sv_IsVisible(&mod.data, pmo))
				{
					if(iter->ddflags & DDMF_DONTDRAW) ex |= MDF_DONTDRAW_EXCLUDE;
					Sv_GenMobjDelta(pool, iter, ex, 0);
				}
			}
		}

		/*
		else
		{
			// Is this in the register?
			for(dt = pool->mobjReg.next; dt != &pool->mobjReg; 
				dt = dt->next)
			{
				mobj_t *mo = &( (mobjdelta_t*) dt)->data;
				if(mo->thinker.id != iter->thinker.id) continue;
				if(Sv_IsVisible(mo, pmo))
				{
					ex = 0;
					//if(pmo == iter) ex |= MDF_CAMERA_EXCLUDE;
					// If the mobj won't be visible, let's leave out some data.
					if(iter->ddflags & DDMF_DONTDRAW) 
						ex |= MDF_DONTDRAW_EXCLUDE;
					Sv_GenMobjDelta(pool, iter, ex, 0);
				}
				break;
			}
		}*/
	}

	// Is this in the register?
	/*for(dt = pool->mobjReg.next; dt != &pool->mobjReg; dt = dt->next)
	{
		mobj_t *mo = &( (mobjdelta_t*) dt)->data;
		//if(mo->thinker.id != iter->thinker.id) continue;
		if(mo == pmo) continue;
		if(Sv_IsVisible(mo, pmo))
		{
			ex = 0;
			//if(pmo == iter) ex |= MDF_CAMERA_EXCLUDE;
			
			// If the mobj won't be visible, let's leave out some data.
			if(mo->ddflags & DDMF_DONTDRAW) 
				ex |= MDF_DONTDRAW_EXCLUDE;
			Sv_GenMobjDelta(pool, iter, ex, 0);
		}
		break;
	}*/
}

//==========================================================================
// Sv_GenNullDeltas
//==========================================================================
void Sv_GenNullDeltas(pool_t *pool)
{
	delta_t *dt, *next = 0;
	mobjdelta_t delta;
	int id;
		
	// Scan through the mobj register and see if any of the mobjs in there
	// have been destroyed. If so, generate Null Deltas for them.
	memset(&delta, 0, sizeof(delta));

	for(dt = pool->mobjReg.next; dt != &pool->mobjReg; dt = next)
	{
		// dt->next will be the next delta to process.
		next = dt->next;

		if(!P_IsUsedMobjID( id = ((mobjdelta_t*)dt)->data.thinker.id ))
		{
			// This mobj is no more! Generate a Null Delta.
			delta.data.thinker.id = id;
			delta.delta.flags = MDF_NULL;
			Sv_AddMobjDelta(pool, &delta);

			// Delete this registered mobj.
			Sv_UnlinkDelta(dt);
			Z_Free(dt);
		}
	}
}

//==========================================================================
// Sv_GenPlayerDelta
//	Generates a delta of the player.
//==========================================================================
void Sv_GenPlayerDelta(pool_t *pool, int playerNum, int exclude)
{
	playerdelta_t regstate, dt;

	// Retrieve the registered state.
	Sv_GetRegisteredPlayer(pool, playerNum, &regstate);

	// Apply the deltas already waiting in the pool.
	Sv_ApplyDeltas(pool, DT_PLAYER, playerNum, &regstate.delta);

	// Do the compare.
	Sv_ComparePlayer(playerNum, &regstate, &dt);

	// Remove the excluded data.
	dt.delta.flags &= ~exclude;

	// Add the result to the pool.
	if(dt.delta.flags) Sv_AddPlayerDelta(pool, &dt);
}

//==========================================================================
// Sv_GenPlayerDeltas
//	Generates deltas for all necessary players.
//==========================================================================
void Sv_GenPlayerDeltas(int playerNum)
{
	pool_t *pool = pools + playerNum;
	int vc = clients[playerNum].viewConsole;
	mobj_t *viewmo = players[vc].mo;
	int i, ex;

	for(i = 0; i < MAXPLAYERS; i++)
	{
		if(!players[i].ingame || !players[i].mo) continue;
		// Check the visibility.
		if(send_all_players 
			|| i == playerNum
			|| Sv_IsVisible(players[i].mo, viewmo))
		{
			// Determine which flags to exclude.
			if(i == playerNum)
			{
				ex = PDF_CAMERA_EXCLUDE;
				// If we must fix angles, don't exclude yaw and pitch.
				if(!(players[i].flags & DDPF_FIXANGLES))
					ex |= PDF_CLYAW | PDF_CLPITCH;
				// Now the Fix flags can be cleared (both Angles and Pos).
				// Fixpos has already been sent when mobj deltas were created.
				players[i].flags &= ~(DDPF_FIXANGLES | DDPF_FIXPOS);
			}
			else
			{
				ex = PDF_NONCAMERA_EXCLUDE;
			}
			Sv_GenPlayerDelta(pool, i/*playerNum*/, ex);
		}
	}
}

//==========================================================================
// Sv_GenSideDelta
//	Generates a side delta for the given side.
//==========================================================================
void Sv_GenSideDelta(pool_t *pool, short sidenum)
{
	sidedelta_t regstate, dt;
	
	// Do we have a valid side number here?
	if(sidenum < 0) return;

	// Retrieve the registered state.
	Sv_GetRegisteredSide(pool, sidenum, &regstate);

	// Apply the deltas already waiting in the pool.
	Sv_ApplyDeltas(pool, DT_SIDE, sidenum, &regstate.delta);

	// Do the compare.
	Sv_CompareSide(sidenum, &regstate, &dt);

	// Add the result to the pool.
	if(dt.delta.flags) Sv_AddSideDelta(pool, &dt);
}

//==========================================================================
// Sv_GenSideDeltasFor
//	Generates side deltas for the line. Only called once per line.
//==========================================================================
boolean Sv_GenSideDeltasFor(line_t *line, pool_t *pool)
{
	Sv_GenSideDelta(pool, line->sidenum[0]);
	Sv_GenSideDelta(pool, line->sidenum[1]);
	return true;
}

//==========================================================================
// Sv_GenSideDeltas
//	Generates side deltas for all the lines near the player.
//==========================================================================
void Sv_GenSideDeltas(int playerNum)
{
	pool_t *pool = pools + playerNum;
	int vc = clients[playerNum].viewConsole;
	mobj_t *viewmo = players[vc].mo;
	int px = (viewmo->x - bmaporgx) >> MAPBLOCKSHIFT;
	int py = (viewmo->y - bmaporgy) >> MAPBLOCKSHIFT;
	int maxb = 4, maxx = px + maxb, maxy = py + maxb;
	int i, k;

	validcount++;
	for(i = px - maxb; i <= maxx; i++)
		for(k = py - maxb; k <= maxy; k++)
			P_BlockLinesIterator(i, k, Sv_GenSideDeltasFor, pool);
}

//===========================================================================
// Sv_IsLumpInPool
//	Is the given lump already waiting to be sent / acknowledged?
//===========================================================================
boolean Sv_IsLumpInPool(pool_t *pool, int lumpnum)
{
	delta_t *dt;

	for(dt = pool->setRoot.next; dt != &pool->setRoot; dt = dt->next)
	{
		if(dt->type != DT_LUMP) continue;
		if(( (lumpdelta_t*) dt)->number == lumpnum)
			return true;
	}
	return false;
}

//===========================================================================
// Sv_GenLumpDelta
//	Lump deltas are handled a bit differently than others.
//===========================================================================
void Sv_GenLumpDelta(pool_t *pool, int lumpnum)
{
	int pNum = pool - pools, bmask = 1 << pNum;
	lumpdelta_t dt;

	// Is there no need to generate a delta for this lump?
	if(!lumpnum 
		|| lumpinfo[lumpnum].sent & bmask
		|| Sv_IsLumpInPool(pool, lumpnum)) return; 

	dt.delta.flags = LDF_INFO;
	dt.number = lumpnum;
	Sv_AddLumpDelta(pool, &dt);
}

//==========================================================================
// Sv_GenSectorDelta
//	Generates a delta for the sector.
//==========================================================================
void Sv_GenSectorDelta(pool_t *pool, int num, int exclude)
{
	sectordelta_t regstate, dt;

	// Retrieve the registered state.
	Sv_GetRegisteredSector(pool, num, &regstate);

	// Apply the deltas already waiting in the pool.
	Sv_ApplyDeltas(pool, DT_SECTOR, num, &regstate.delta);

	// Do the compare.
	Sv_CompareSector(num, &regstate, &dt);

	// Remove the excluded data.
	dt.delta.flags &= ~exclude;

	// Add the result to the pool.
	if(dt.delta.flags) Sv_AddSectorDelta(pool, &dt);

	// How about the flats?
	if(dt.delta.flags & SDF_FLOORPIC) Sv_GenLumpDelta(pool, dt.floorpic);
	if(dt.delta.flags & SDF_CEILINGPIC) Sv_GenLumpDelta(pool, dt.ceilingpic);
}

//===========================================================================
// Sv_InBounds
//	Returns true if the given point (px,py) is inside the box (allowing
//	a range of 'range').
//===========================================================================
boolean Sv_InBounds(int px, int py, int range, int *box)
{
	return !(px < box[BOXLEFT] - range || px > box[BOXRIGHT] + range
			|| py < box[BOXBOTTOM] - range || py > box[BOXTOP] + range);
}

//==========================================================================
// Sv_GenSectorDeltas
//	Generates deltas for all necessary sectors.
//==========================================================================
void Sv_GenSectorDeltas(int playerNum)
{
	pool_t *pool = pools + playerNum;
	int vc = clients[playerNum].viewConsole;
	mobj_t *viewmo = players[vc].mo;
	int i, exclude;
	sector_t *sec;
	int px = (viewmo->x - bmaporgx) >> MAPBLOCKSHIFT;
	int py = (viewmo->y - bmaporgy) >> MAPBLOCKSHIFT;
	sector_t *plsec = viewmo->subsector->sector;
	//int maxb = 4; // FIXME: -> console variable.

	for(i = 0, sec = (sector_t*)sectors; i < numsectors; i++, 
		((byte*)sec) += SECTSIZE)
	{
		// Reject overrides everything but the player's own sector.
		//if(sec != plsec && !P_CheckReject(plsec, sec)) continue;
		// Check if this sector is too far.
		/*if(px < sec->blockbox[BOXLEFT] - maxb
			|| px > sec->blockbox[BOXRIGHT] + maxb
			|| py < sec->blockbox[BOXBOTTOM] - maxb
			|| py > sec->blockbox[BOXTOP] + maxb)*/
		//{
			// Yes; don't update.
			/*Con_Printf("Skipping %i (p%i,%i: %i,%i-%i,%i)\n", i,
				px, py, sec->blockbox[BOXLEFT], sec->blockbox[BOXBOTTOM],
				sec->blockbox[BOXRIGHT], sec->blockbox[BOXTOP]);*/
		//	continue; 
		//}

		exclude = 0;

		// No tests for the player's sector.
		if(sec == plsec) goto do_delta;

		// Check if this sector is too far.
		if(!Sv_InBounds(px, py, net_maxsecupd, sec->blockbox))
			continue; // Yes; don't generate a delta.
		
		// The sector is closer than the maximum; a delta might be generated.
		// Check if we're out of the forced range.
		if(!Sv_InBounds(px, py, net_minsecupd, sec->blockbox))
		{
			// Not forced; must check reject.
			if(!P_CheckReject(plsec, sec)) continue; // Not visible.
			
			// If we're out of the full range, exclude Light data.
			if(!Sv_InBounds(px, py, net_fullsecupd, sec->blockbox))
				exclude = SDF_LIGHT;
		}

do_delta: // Generate a delta for this sector.
		Sv_GenSectorDelta(pool, i, exclude);
	}
}

//==========================================================================
// Sv_GenPolyDeltaFor
//	Generates a delta the sector.
//==========================================================================
boolean Sv_GenPolyDeltaFor(polyobj_t *poly, pool_t *mypool)
{
	int num = GET_POLYOBJ_IDX(poly);
	polydelta_t regstate, dt;

	// Retrieve the registered state.
	Sv_GetRegisteredPoly(mypool, num, &regstate);

	// Apply the deltas already waiting in the pool.
	Sv_ApplyDeltas(mypool, DT_POLY, num, &regstate.delta);

	// Do the compare.
	Sv_ComparePoly(num, &regstate, &dt);

	/* // Remove the excluded data.
	dt.delta.flags &= ~exclude; */

	// Add the result to the pool.
	if(dt.delta.flags) Sv_AddPolyDelta(mypool, &dt);
	return true;
}

//==========================================================================
// Sv_GenPolyDeltas
//	Generates deltas for all necessary polyobjs.
//==========================================================================
void Sv_GenPolyDeltas(int playerNum)
{
	pool_t *pool = pools + playerNum;
	int vc = clients[playerNum].viewConsole;
	mobj_t *viewmo = players[vc].mo;
	int px = (viewmo->x - bmaporgx) >> MAPBLOCKSHIFT;
	int py = (viewmo->y - bmaporgy) >> MAPBLOCKSHIFT;
	int maxb = 5, maxx = px + maxb, maxy = py + maxb;
	int i, k;

	validcount++;
	for(i = px - maxb; i <= maxx; i++)
		for(k = py - maxb; k <= maxy; k++)
			P_BlockPolyobjsIterator(i, k, Sv_GenPolyDeltaFor, pool);
}

//==========================================================================
// Sv_DoFrameDelta
//	Updates the pool with new deltas.
//	The set number of the pool is incremented.
//	This is the "main interface" to the Delta Pools.
//==========================================================================
void Sv_DoFrameDelta(int playerNum)
{
	pool_t *pool = pools + playerNum;

	// A new set will be generated. All new deltas that are generated
	// will receive this set number.
	pool->setNumber++;

	// Generate Null Deltas for destroyed mobjs. This is done first so all
	// the destroyed mobjs will be removed from the client's register.
	Sv_GenNullDeltas(pool);

	// Generate Deltas for mobjs near the client.
	Sv_GenMobjDeltas(playerNum);

	// Generate Deltas for all necessary players.
	Sv_GenPlayerDeltas(playerNum);

	// Generate world deltas.
	Sv_GenSectorDeltas(playerNum);
	Sv_GenSideDeltas(playerNum);
	Sv_GenPolyDeltas(playerNum);
}

//==========================================================================
// Sv_WriteMobjDelta
//	Writes the mobj delta to the message buffer, using as few bytes as
//	possible. 
//==========================================================================
void Sv_WriteMobjDelta(mobjdelta_t *delta)
{
	mobj_t *d = &delta->data;
	int df = delta->delta.flags;
	int startmsgpos = Msg_Offset();

#if _DEBUG
	if(!df) Con_Error("Sv_WriteMobjDelta: Delta is empty.\n");
#endif

	// First the mobj ID number.
	Msg_WriteShort(d->thinker.id);

	//
	// FIXME: Optimize to write as few bytes as possible!
	//

	// Flags. What elements are included in the delta?
	if(d->selector & ~DDMOBJ_SELECTOR_MASK) df |= MDF_SELSPEC;
	Msg_WriteShort(df & 0xffff);

	// Coordinates with three bytes.
	if(df & MDF_POS_X) 
	{
		Msg_WriteShort(d->x >> FRACBITS);
		Msg_WriteByte(d->x >> 8);
	}
	if(df & MDF_POS_Y) 
	{
		Msg_WriteShort(d->y >> FRACBITS);
		Msg_WriteByte(d->y >> 8);
	}
	if(df & MDF_POS_Z) 
	{
		Msg_WriteShort(d->z >> FRACBITS);
		Msg_WriteByte(d->z >> 8);
	}

	// Momentum using 8.8 fixed point.
	if(df & MDF_MOM_X) Msg_WriteShort(FIXED8_8(d->momx));
	if(df & MDF_MOM_Y) Msg_WriteShort(FIXED8_8(d->momy));
	if(df & MDF_MOM_Z) Msg_WriteShort(FIXED8_8(d->momz));
	
	// Angles with 16-bit accuracy.
	if(df & MDF_ANGLE) Msg_WriteShort(d->angle >> 16);

	if(df & MDF_SELECTOR) Msg_WritePackedShort(d->selector);
	if(df & MDF_SELSPEC) Msg_WriteByte(d->selector >> 24);

	if(df & MDF_STATE) Msg_WritePackedShort(d->state - states);

#if 0
	// Frame in a byte.
	if(df & MDF_SPRITE) Msg_WritePackedShort(d->sprite);
	if(df & MDF_FRAME) 
		Msg_WriteByte(d->frame | (d->frame & 0x8000? 0x80 : 0));

	// We'll pack the nextframe, tics and nexttime into a word 
	// (if all present).
	if((df & MDF_NEXTFRAME) && (df & MDF_NEXTTIME) && (df & MDF_TICS))
	{
		// This is usually the case.
		// We'll pack the nextframe, tics and nexttime into a word.
		int //nextframe = d->nextframe, 
			tics = d->tics; /*, 
			nexttime = d->nexttime;*/
		// We have 5 bits for each value, so let's check that they
		// are in range.
		//if(nextframe > 31) nextframe = 31;	
		if(tics > 31) tics = 31;
		//if(nexttime > 31) nexttime = 31;
		Msg_WriteShort(/*nextframe | */(tics << 5)/* | (nexttime << 10)*/);
	}
	else
	{
		// Write separately.
		//if(df & MDF_NEXTFRAME) Msg_WriteByte(d->nextframe);
		//if(df & MDF_NEXTTIME) Msg_WriteByte(d->nexttime);
		if(df & MDF_TICS) Msg_WriteByte(d->tics);
	}
#endif
	
	// Pack flags into a word (3 bytes?).
	// FIXME: Do the packing!
	if(df & MDF_FLAGS) Msg_WriteLong(d->ddflags & DDMF_PACK_MASK);
		
	// Radius, height and floorclip are all bytes.
	if(df & MDF_RADIUS) Msg_WriteByte(d->radius >> FRACBITS);
	if(df & MDF_HEIGHT) Msg_WriteByte(d->height >> FRACBITS);
	if(df & MDF_FLOORCLIP) Msg_WriteByte(d->floorclip >> 14);

	if(net_showsets) Con_Printf("- mo %i (%x) [%i b]\n", d->thinker.id, df,
		Msg_Offset() - startmsgpos);
}

//==========================================================================
// Sv_WritePlayerDelta
//	Writes the player delta to the message buffer, using as few bytes as
//	possible. 
//==========================================================================
void Sv_WritePlayerDelta(playerdelta_t *d)
{
	int df = d->delta.flags, psdf, i, k;
	ddpsprite_t *psp;

#if _DEBUG
	if(!df) Con_Error("Sv_WritePlayerDelta: Delta is empty.\n");
#endif

	if(net_showsets) Con_Printf("- pl %i (%x)\n", d->player, df);

	// First the player number. Upper three bits contain flags.
	Msg_WriteByte(d->player | (df >> 8));

	// Flags. What elements are included in the delta?
	Msg_WriteByte(df & 0xff);
	
	if(df & PDF_MOBJ) Msg_WriteShort(d->mobjid);
	if(df & PDF_FORWARDMOVE) Msg_WriteByte(d->forwardmove);
	if(df & PDF_SIDEMOVE) Msg_WriteByte(d->sidemove);
	if(df & PDF_ANGLE) Msg_WriteByte(d->angle >> 24);
	if(df & PDF_TURNDELTA) Msg_WriteByte((d->turndelta*16) >> 24);
	if(df & PDF_FRICTION) Msg_WriteByte(d->friction >> 8);
	if(df & PDF_EXTRALIGHT) 
	{	
		// Three bits is enough for fixedcolormap.
		i = d->fixedcolormap;
		if(i < 0) i = 0;
		if(i > 7) i = 7;
		// Write the five upper bytes of extralight.
		Msg_WriteByte(i | (d->extralight & 0xf8)); 
	}
	if(df & PDF_FILTER) Msg_WriteLong(d->filter);
	if(df & PDF_CLYAW) Msg_WriteShort(d->clyaw >> 16);
	if(df & PDF_CLPITCH) Msg_WriteShort(d->clpitch/110 * DDMAXSHORT);
	if(df & PDF_PSPRITES) // Only set if there's something to write.
	{
		for(i = 0; i < 2; i++)
		{
			psdf = df >> (16 + i*8);
			psp = d->psp + i;
			// First the flags.
			Msg_WriteByte(psdf);
			if(psdf & PSDF_STATEPTR)
			{
				if(psp->stateptr)
					Msg_WritePackedShort(psp->stateptr - states + 1);
				else
					Msg_WritePackedShort(0);
			}
			//if(psdf & PSDF_SPRITE) Msg_WritePackedShort(psp->sprite + 1);
			//if(psdf & PSDF_FRAME) Msg_WriteByte(psp->frame);
			//if(psdf & PSDF_NEXT/*FRAME*/) Msg_WriteByte(psp->nextframe);
			//if(psdf & PSDF_NEXT/*TIME*/) Msg_WriteByte(psp->nexttime);
			//if(psdf & PSDF_TICS) Msg_WriteByte(psp->tics);
			if(psdf & PSDF_LIGHT) 
			{
				k = psp->light * 255;
				if(k < 0) k = 0;
				if(k > 255) k = 255;
				Msg_WriteByte(k);
			}
			if(psdf & PSDF_ALPHA)
			{
				k = psp->alpha * 255;
				if(k < 0) k = 0;
				if(k > 255) k = 255;
				Msg_WriteByte(k);
			}
			if(psdf & PSDF_STATE) 
			{
				Msg_WriteByte(psp->state);
			}
			if(psdf & PSDF_OFFSET)
			{
				Msg_WriteByte(CLAMPED_CHAR(psp->offx/2));
				Msg_WriteByte(CLAMPED_CHAR(psp->offy/2));
			}
		}
	}
}

//==========================================================================
// Sv_WriteSectorDelta
//	Writes the sector delta to the message buffer, using as few bytes as
//	possible. 
//==========================================================================
void Sv_WriteSectorDelta(sectordelta_t *d)
{
	int df = d->delta.flags, spd;
	byte floorspd, ceilspd;

#if _DEBUG
	if(!df) Con_Error("Sv_WriteSectorDelta: Delta is empty.\n");
#endif

	if(net_showsets) Con_Printf("- sec %i (%x)\n", d->number, df);

	// Sector number first (0 terminates).
	Msg_WritePackedShort(d->number + 1);

	// Is there need to use 4.4 fixed-point speeds?
	// (7.1 is too inaccurate for very slow movement)
	if(df & SDF_FLOOR_SPEED)
	{
		spd = abs(d->planes[PLN_FLOOR].speed);
		floorspd = spd >> 15;
		if(!floorspd) 
		{
			df |= SDF_FLOOR_SPEED_44;
			floorspd = spd >> 12;
		}
	}
	if(df & SDF_CEILING_SPEED)
	{
		spd = abs(d->planes[PLN_CEILING].speed);
		ceilspd = spd >> 15;
		if(!ceilspd) 
		{
			df |= SDF_CEILING_SPEED_44;
			ceilspd = spd >> 12;
		}
	}

	// Flags as a packed short. (Usually only one byte, though.)
	Msg_WritePackedShort(df & 0xffff);

	if(df & SDF_FLOORPIC) Msg_WritePackedShort(d->floorpic);
	if(df & SDF_CEILINGPIC) Msg_WritePackedShort(d->ceilingpic);
	if(df & SDF_LIGHT) Msg_WriteByte(d->lightlevel);
	if(df & SDF_FLOOR_TARGET) 
		Msg_WriteShort(d->planes[PLN_FLOOR].target >> 16);
	if(df & SDF_FLOOR_SPEED) // 7.1/4.4 fixed-point
		Msg_WriteByte(floorspd);
	if(df & SDF_FLOOR_TEXMOVE)
	{
		Msg_WriteShort(d->planes[PLN_FLOOR].texmove[0] >> 8);
		Msg_WriteShort(d->planes[PLN_FLOOR].texmove[1] >> 8);
	}
	if(df & SDF_CEILING_TARGET) 
		Msg_WriteShort(d->planes[PLN_CEILING].target >> 16);
	if(df & SDF_CEILING_SPEED) // 7.1/4.4 fixed-point
		Msg_WriteByte(ceilspd);
	if(df & SDF_CEILING_TEXMOVE)
	{
		Msg_WriteShort(d->planes[PLN_CEILING].texmove[0] >> 8);
		Msg_WriteShort(d->planes[PLN_CEILING].texmove[1] >> 8);
	}
	if(df & SDF_COLOR_RED) Msg_WriteByte(d->rgb[0]);
	if(df & SDF_COLOR_GREEN) Msg_WriteByte(d->rgb[1]);
	if(df & SDF_COLOR_BLUE) Msg_WriteByte(d->rgb[2]);
}

//==========================================================================
// Sv_WriteSideDelta
//	Writes the side delta to the message buffer, using as few bytes as
//	possible. 
//==========================================================================
void Sv_WriteSideDelta(sidedelta_t *d)
{
	int df = d->delta.flags;

#if _DEBUG
	if(!df) Con_Error("Sv_WriteSideDelta: Delta is empty.\n");
#endif

	if(net_showsets) Con_Printf("- sid %i (%x)\n", d->number, df);

	// Side number first (0 terminates).
	Msg_WritePackedShort(d->number + 1);

	// Flags.
	Msg_WriteByte(df & 0xff);

	if(df & SIDF_TOPTEX) Msg_WritePackedShort(d->toptexture);
	if(df & SIDF_MIDTEX) Msg_WritePackedShort(d->midtexture);
	if(df & SIDF_BOTTOMTEX) Msg_WritePackedShort(d->bottomtexture);
}

//==========================================================================
// Sv_WritePolyDelta
//	Writes the polyobj delta to the message buffer, using as few bytes as
//	possible. 
//==========================================================================
void Sv_WritePolyDelta(polydelta_t *d)
{
	int df = d->delta.flags;

#if _DEBUG
	if(!df) Con_Error("Sv_WritePolyDelta: Delta is empty.\n");
#endif

	if(net_showsets) Con_Printf("- po %i (%x)\n", d->number, df);

	// Poly number first (0 terminates).
	Msg_WritePackedShort(d->number + 1);

	if(d->destAngle == (unsigned) -1) 
	{
		// Send Perpetual Rotate instead of Dest Angle flag.
		df |= PODF_PERPETUAL_ROTATE;
		df &= ~PODF_DEST_ANGLE;
	}

	// Flags.
	Msg_WriteByte(df & 0xff);

	if(df & PODF_DEST_X)
	{
		Msg_WriteShort(d->dest.x >> 16);
		Msg_WriteByte(d->dest.x >> 8);
	}
	if(df & PODF_DEST_Y)
	{
		Msg_WriteShort(d->dest.y >> 16);
		Msg_WriteByte(d->dest.y >> 8);
	}
	if(df & PODF_SPEED) Msg_WriteShort(d->speed >> 8);
	if(df & PODF_DEST_ANGLE) Msg_WriteShort(d->destAngle >> 16);
	if(df & PODF_ANGSPEED) Msg_WriteShort(d->angleSpeed >> 16);
}

//===========================================================================
// Sv_WriteLumpDelta
//===========================================================================
void Sv_WriteLumpDelta(lumpdelta_t *d)
{
	// We'll only write the number of the lump and its name.
	// Lump zero is never sent (0 terminates).
	Msg_WritePackedShort(d->number);
	Msg_Write(lumpinfo[d->number].name, 8);
}

//==========================================================================
// Sv_WriteDeltas
//	Returns true only if something was written.
//==========================================================================
int Sv_WriteDeltas(pool_t *pool, int set, int type)
{
	int written = false;
	delta_t *dt;

	for(dt = pool->setRoot.next; dt != &pool->setRoot;
		dt = dt->next)
	{
		if(dt->type != type || dt->set != set) continue;
		written = true;
		dt->senttime = gametic;
		switch(type)
		{
		case DT_MOBJ:
			Sv_WriteMobjDelta( (mobjdelta_t*) dt);
			break;

		case DT_PLAYER:
			Sv_WritePlayerDelta( (playerdelta_t*) dt);
			break;

		case DT_SECTOR:
			Sv_WriteSectorDelta( (sectordelta_t*) dt);
			break;

		case DT_SIDE:
			Sv_WriteSideDelta( (sidedelta_t*) dt);
			break;

		case DT_POLY:
			Sv_WritePolyDelta( (polydelta_t*) dt);
			break;

		case DT_LUMP:
			Sv_WriteLumpDelta( (lumpdelta_t*) dt);
			break;
		}
	}
	return written;
}

//==========================================================================
// Sv_WriteDeltaSet
//	Writes the given set to the message buffer.
//==========================================================================
void Sv_WriteDeltaSet(pool_t *pool, int set)
{
	byte *headerptr;
	int present = 0;

	if(net_showsets) Con_Printf("----WDS %i\n", set);

	// The header of the set consists of the presence flags and set #.
	headerptr = netbuffer.cursor;
	Msg_WriteShort(0);

	// First any mobjs.
	if(Sv_WriteDeltas(pool, set, DT_MOBJ))
	{
		// Mobjs are present in the packet.
		present |= BIT( DT_MOBJ );
		// Write a zero ID as the end marker.
		Msg_WriteShort(0);
	}
	
	// Players.
	if(Sv_WriteDeltas(pool, set, DT_PLAYER))
	{
		// Players are present in the packet.
		present |= BIT( DT_PLAYER );
		// Write 0xff as the end marker.
		Msg_WriteByte(0xff);
	}

	// Lump names. Must be written before sectors.
	if(Sv_WriteDeltas(pool, set, DT_LUMP))
	{
		// Lump names are present in the packet.
		present |= BIT( DT_LUMP );
		// Write 0 as the end marker.
		Msg_WriteByte(0);
	}

	// Sectors.
	if(Sv_WriteDeltas(pool, set, DT_SECTOR))
	{
		// Sectors are present in the packet.
		present |= BIT( DT_SECTOR );
		// Write 0 as the end marker.
		Msg_WriteByte(0);
	}

	// Sides.
	if(Sv_WriteDeltas(pool, set, DT_SIDE))
	{
		// Sides are present in the packet.
		present |= BIT( DT_SIDE );
		// Write 0 as the end marker.
		Msg_WriteByte(0);
	}

	// Polyobjs.
	if(Sv_WriteDeltas(pool, set, DT_POLY))
	{
		// Polys are present in the packet.
		present |= BIT( DT_POLY );
		// Write 0 as the end marker.
		Msg_WriteByte(0);
	}

	// Update the header.
	headerptr[0] = present;	
	headerptr[1] = set;
}

//==========================================================================
// Sv_WriteFrameDelta
//	Writes all the deltas that have the current set number to the message
//	buffer. Also checks for unacked sets and resends them, if necessary.
//==========================================================================
void Sv_WriteFrameDelta(int playerNum)
{
	pool_t *pool = pools + playerNum;
	delta_t *dt;

	// Scan the Pool for sets that need to be resent.
	for(dt = pool->setRoot.next; dt != &pool->setRoot; dt = dt->next)
	{
		if(dt->set < pool->setNumber 
			&& Net_TimeDelta(gametic, dt->senttime) > net_resendtime)
		{
			// Older than 2 sec; resend this set.
			Sv_WriteDeltaSet(pool, dt->set);
		}
	}

	// Write the current set.
	Sv_WriteDeltaSet(pool, pool->setNumber);
}

//==========================================================================
// Sv_ApplyMobjDeltaToRegister
//==========================================================================
void Sv_ApplyMobjDeltaToRegister(pool_t *pool, mobjdelta_t *mod)
{
	delta_t *dt;
	mobjdelta_t *state;

	if(mod->delta.flags & MDF_NULL) return;

	// Try to find the delta in the mobj register.
	for(dt = pool->mobjReg.next; dt != &pool->mobjReg; dt = dt->next)
	{
		mobjdelta_t *reg = (mobjdelta_t*) dt;
		// IDs are used for matching.
		if(reg->data.thinker.id == mod->data.thinker.id)
		{
			// Apply the delta to the registered state.
			Sv_ApplyMobjDelta(&reg->data, mod);
			return;	// Done.
		}
	}
	
	// The mobj wasn't in the register, so add it.
	state = Z_Malloc(sizeof(*state), PU_LEVEL, 0);
	memset(state, 0, sizeof(*state)); // Start with nothing but zeros.
	state->data.thinker.id = mod->data.thinker.id;
	Sv_ApplyMobjDelta(&state->data, mod);
	Sv_LinkDelta(&pool->mobjReg, &state->delta);
}

//==========================================================================
// Sv_ApplyPlayerDeltaToRegister
//==========================================================================
void Sv_ApplyPlayerDeltaToRegister(pool_t *pool, playerdelta_t *mod)
{
	delta_t *dt;
	playerdelta_t *state;

	// Try to find the delta in the player register.
	for(dt = pool->playReg.next; dt != &pool->playReg; dt = dt->next)
	{
		playerdelta_t *reg = (playerdelta_t*) dt;
		if(reg->player == mod->player)
		{
			// Apply the delta to the registered state.
			Sv_ApplyPlayerDelta(reg, mod);
			return;	
		}
	}
	
	// The player wasn't in the register, so add it.
	state = Z_Malloc(sizeof(*state), PU_LEVEL, 0);
	memset(state, 0, sizeof(*state)); // Start with nothing but zeros.
	state->player = mod->player;
	Sv_ApplyPlayerDelta(state, mod);
	Sv_LinkDelta(&pool->playReg, &state->delta);
}

//==========================================================================
// Sv_ApplySectorDeltaToRegister
//==========================================================================
void Sv_ApplySectorDeltaToRegister(pool_t *pool, sectordelta_t *mod)
{
	Sv_ApplySectorDelta(pool->secReg + mod->number, mod);
}

//==========================================================================
// Sv_ApplySideDeltaToRegister
//==========================================================================
void Sv_ApplySideDeltaToRegister(pool_t *pool, sidedelta_t *mod)
{
	Sv_ApplySideDelta(pool->sideReg + mod->number, mod);
}

//==========================================================================
// Sv_ApplyPolyDeltaToRegister
//==========================================================================
void Sv_ApplyPolyDeltaToRegister(pool_t *pool, polydelta_t *mod)
{
	Sv_ApplyPolyDelta(pool->polyReg + mod->number, mod);
}

//==========================================================================
// Sv_ApplyToRegister
//==========================================================================
void Sv_ApplyToRegister(pool_t *pool, delta_t *delta)
{
	switch(delta->type)
	{
	case DT_MOBJ:
		Sv_ApplyMobjDeltaToRegister(pool, (mobjdelta_t*) delta);
		break;

	case DT_PLAYER:
		Sv_ApplyPlayerDeltaToRegister(pool, (playerdelta_t*) delta);
		break;

	case DT_SECTOR:
		Sv_ApplySectorDeltaToRegister(pool, (sectordelta_t*) delta);
		break;

	case DT_SIDE:
		Sv_ApplySideDeltaToRegister(pool, (sidedelta_t*) delta);
		break;

	case DT_POLY:
		Sv_ApplyPolyDeltaToRegister(pool, (polydelta_t*) delta);
		break;

	case DT_LUMP: // Trivial.
		lumpinfo[((lumpdelta_t*)delta)->number].sent |= 1 << (pool - pools);
		break;
	}
}

//==========================================================================
// Sv_PoolSubObsolete
//==========================================================================
void Sv_PoolSubObsolete(pool_t *pool, delta_t *delta)
{
	delta_t *dt, *next = 0;

	for(dt = pool->setRoot.next; dt != &pool->setRoot; dt = next)
	{
		next = dt->next;
		// Only process older sets.
		if(dt->type == delta->type 
			&& Net_TimeDelta(delta->set, dt->set) > 0)
		{
			Sv_SubtractDelta(dt, delta);
			// Was something left of the delta?
			if(!dt->flags)
			{
				// No; unlink and destroy it. It has become totally obsolete.
				Sv_UnlinkDelta(dt);
				Z_Free(dt);
			}
		}
	}
}

//==========================================================================
// Sv_AckDeltaSet
//	This is called after receiving an acknowledgement from the client.
//	All the deltas of the set will be
//	1) applied to the register
//	2) subtracted from older deltas in the pool (if they're resent, their
//	   data must not overwrite what has already been sent)
//	3) unlinked and destroyed
//==========================================================================
void Sv_AckDeltaSet(int playerNum, byte set)
{
	pool_t *pool = pools + playerNum;
	delta_t *dt, *next = 0;

	// Scan the pool.
	for(dt = pool->setRoot.next; dt != &pool->setRoot; dt = next)
	{
		// Remember the next one in case dt is removed.
		next = dt->next;
		// Does this belong to the acked set?
		if(set == dt->set)
		{
			// Yes; apply this to the register.
			Sv_ApplyToRegister(pool, dt);
			// Subtract from older deltas in the pool.
			Sv_PoolSubObsolete(pool, dt);
			// Unlink and destroy.
			Sv_UnlinkDelta(dt);
			Z_Free(dt);
		}
	}
}

//==========================================================================
// Sv_AckDeltaSetLocal
//	Server calls this after it sends a delta set to itself (when writing
//	a demo). Local sets naturally arrive immediately after they're sent.
//==========================================================================
void Sv_AckDeltaSetLocal(int plrNum)
{
	Sv_AckDeltaSet(plrNum, pools[plrNum].setNumber);
}

//==========================================================================
// Sv_InitialWorld
//	Stores the initial state of the world.
//==========================================================================
void Sv_InitialWorld(void)
{
	int i;

	// Allocate memory for the sectors and sides.
	secInit = Z_Malloc(sizeof(sectordelta_t) * numsectors, PU_LEVEL, 0);
	sideInit = Z_Malloc(sizeof(sidedelta_t) * numsides, PU_LEVEL, 0);
	polyInit = Z_Malloc(sizeof(polydelta_t) * po_NumPolyobjs, PU_LEVEL, 0);

	// Init sectors.
	for(i = 0; i < numsectors; i++)
		Sv_CompareSector(i, secInit + i, secInit + i);

	// Init sides.
	for(i = 0; i < numsides; i++)
		Sv_CompareSide(i, sideInit + i, sideInit + i);

	// Init polyobjs.
	for(i = 0; i < po_NumPolyobjs; i++)
		Sv_ComparePoly(i, polyInit + i, polyInit + i);
}

//==========================================================================
// Sv_DestroyDeltaChain
//	Unlink everything and Z_Free them.
//==========================================================================
void Sv_DestroyDeltaChain(delta_t *root)
{
	delta_t *iter, *next;

	for(iter = root->next; iter != root; iter = next)
	{
		next = iter->next;
		Sv_UnlinkDelta(iter);
		Z_Free(iter);
	}
}

//==========================================================================
// Sv_DrainPool
//	Delete everything in the pool and reset the register.
//==========================================================================
void Sv_DrainPool(int playerNum)
{
	pool_t *pool = pools + playerNum;
	int secSize = sizeof(sectordelta_t) * numsectors;
	int sideSize = sizeof(sidedelta_t) * numsides;
	int polySize = sizeof(polyobj_t) * po_NumPolyobjs;

	pool->setNumber = 0;
	Sv_DestroyDeltaChain(&pool->setRoot);
	Sv_DestroyDeltaChain(&pool->mobjReg);
	Sv_DestroyDeltaChain(&pool->playReg);

	// Allocate the registers if they aren't yet created.
	if(!pool->secReg) pool->secReg = Z_Malloc(secSize, PU_LEVEL, 0);
	if(!pool->sideReg) pool->sideReg = Z_Malloc(sideSize, PU_LEVEL, 0);
	if(!pool->polyReg) pool->polyReg = Z_Malloc(polySize, PU_LEVEL, 0);
	
	memcpy(pool->secReg, secInit, secSize);
	memcpy(pool->sideReg, sideInit, sideSize);
	memcpy(pool->polyReg, polyInit, polySize);
}

//==========================================================================
// Sv_ClearCoordError
//==========================================================================
void Sv_ClearCoordError(int pnum)
{
	client_t *cl = clients + pnum;

	cl->errorPos = 0;
	memset(cl->error, 0, sizeof(cl->error));
}

//===========================================================================
// Sv_ClearLumpSendFlags
//===========================================================================
void Sv_ClearLumpSendFlags(int clnum)
{
	int i, bmask = 1 << clnum;
	for(i = 0; i < numlumps; i++) lumpinfo[i].sent &= ~bmask;
}

//==========================================================================
// Sv_InitPoolForClient
//	Called when a client joins the game.
//==========================================================================
void Sv_InitPoolForClient(int clnum)
{
	Sv_ClearCoordError(clnum);
	Sv_DrainPool(clnum);
	Sv_ClearLumpSendFlags(clnum);
}

//==========================================================================
// Sv_ClientCoords
//	Reads a pkt_coords packet from the message buffer and checks if the
//	coordinates are OK. If they're not, the fixpos flag is set. If the
//	difference is small enough, we assume the client is right and correct
//	our coordinates accordingly (otherwise the clients would have a hell
//	of a time with all the warping around... :-)).
//==========================================================================
void Sv_ClientCoords(int playerNum)
{
	client_t *cl = clients + playerNum;
	mobj_t *mo = players[playerNum].mo;
	int maxdiff = net_maxdif << FRACBITS;
	int mindiff = net_mindif << FRACBITS;
	int clx, cly, clz;
	int i, dz, dx, dy;
	int avgx, avgy;
	float unit = FRACUNIT;

	// Under certain circumstances the message is discarded.
	if(!mo || !players[playerNum].ingame
		|| players[playerNum].flags & DDPF_DEAD) return;

	dx = mo->x - (clx = Msg_ReadShort() << 16);
	dy = mo->y - (cly = Msg_ReadShort() << 16);
	dz = mo->z - (clz = Msg_ReadShort() << 16);

	// Register a new difference.
	cl->error[cl->errorPos].x = dx;
	cl->error[cl->errorPos].y = dy;

	// Advance the error marker.
	cl->errorPos = (cl->errorPos + 1) % NUM_CERR;

	// Calculate the average.
	for(i = 0, avgx = avgy = 0; i < NUM_CERR; i++)
	{
		avgx += cl->error[i].x;
		avgy += cl->error[i].y;
	}
	avgx /= NUM_CERR;
	avgy /= NUM_CERR;
	
	// Check the difference. If it's greater than the maximum,
	// send a hard fixpos.
	if(abs(dz) > maxdiff || P_ApproxDistance(dx, dy) > maxdiff)
	{
		// The client is in the wrong place, fix it.
		players[playerNum].flags |= DDPF_FIXPOS;
		// Clear the coords list.
		memset(cl->error, 0, sizeof(cl->error));
	}

	// Keep fixing the client's position. If the error is small enough
	// we like to think the client is right.
	else if(!(players[playerNum].flags & DDPF_FIXPOS)
		&& P_CheckPosition2(mo, clx, cly, clz)) // But it must be a valid pos.
	{
		// We mustn't accept too big changes in the Z coord.
		if(tmfloorz - clz < 24*FRACUNIT)
		{
			P_UnlinkThing(mo);//, DDLINK_SECTOR | DDLINK_BLOCKMAP);
			mo->x = clx;
			mo->y = cly;
			mo->z = clz;
			P_LinkThing(mo, DDLINK_SECTOR | DDLINK_BLOCKMAP);
			mo->floorz = tmfloorz;
			mo->ceilingz = tmceilingz;
			if(mo->z < mo->subsector->sector->floorheight) 
				mo->z = mo->subsector->sector->floorheight;
		}
	}
}

//===========================================================================
// Sv_AnimatePoolMobj
//===========================================================================
void Sv_AnimatePoolMobj(mobj_t *mo)
{
	if(mo->tics < 0) return; // In statis.
	mo->tics--;
	if(mo->state && mo->tics <= 0)
	{
		// Go to next state, if possible.
		if(mo->state->nextstate > 0)
		{
			mo->state = &states[mo->state->nextstate];
			mo->tics = mo->state->tics;
		}
		else
		{
			// Freeze it; the server will probably to remove it soon.
			mo->tics = -1;
		}
	}
}

//==========================================================================
// Sv_PoolTicker
//	Do stuff on the registers that the clients will do, too.
//==========================================================================
void Sv_PoolTicker(void)
{
	int i;
	delta_t *dt;
	pool_t *pool;
	
	if(!pools_inited) return;

	for(i = 0; i < MAXPLAYERS; i++)
	{
		// Do this only for players who are in the game.
		if(!players[i].ingame) continue;
		pool = pools + i;
	
		// Traverse the mobj register.
		for(dt = pool->mobjReg.next; dt != &pool->mobjReg; dt = dt->next)
		{
			mobjdelta_t *m = (mobjdelta_t*) dt;
			//if(m->data.tics > 0) m->data.tics--;
			Sv_AnimatePoolMobj(&m->data);			
			if(m->data.dplayer) continue;
			// Simplified movement (applies to no-gravity/no-friction cases;
			// i.e. most missiles).
			m->data.x += m->data.momx;
			m->data.y += m->data.momy;
			m->data.z += m->data.momz;
			// Players must also simulate friction.
			/*if(m->data.dplayer)// && m->data.z <= m->data.floorz)
			{
				int fric = gx.MobjFriction? gx.MobjFriction(&m->data) 
					: DEFAULT_FRICTION;
				m->data.momx = FixedMul(m->data.momx, fric);
				m->data.momy = FixedMul(m->data.momy, fric);
			}*/
		}

		// Traverse the player register.
		for(dt = pool->playReg.next; dt != &pool->playReg; dt = dt->next)
		{
			playerdelta_t *p = (playerdelta_t*) dt;
			if(p->psp[0].stateptr && p->psp[0].tics > 1) 
				p->psp[0].tics--;
			if(p->psp[1].stateptr && p->psp[1].tics > 1) 
				p->psp[1].tics--;
		}
	}
}