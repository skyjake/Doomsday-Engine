/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * cl_world.c: Clientside World Management
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_play.h"

#include "r_util.h"

// MACROS ------------------------------------------------------------------

#define MAX_MOVERS			128		// Definitely enough!
#define MAX_TRANSLATIONS	16384

#define MVF_CEILING			0x1		// Move ceiling.
#define MVF_SET_FLOORPIC	0x2		// Set floor texture when move done.

typedef enum
{
	mvt_floor,
	mvt_ceiling
} movertype_t;

// TYPES -------------------------------------------------------------------

typedef struct
{
	thinker_t	thinker;
	sector_t	*sector;
	int			sectornum;
	movertype_t	type;
	fixed_t		*current;
	fixed_t		destination;
	fixed_t		speed;
} mover_t;

typedef struct
{
	thinker_t	thinker;
	int			number;
	polyobj_t	*poly;
	boolean		move;
	boolean		rotate;
} polymover_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static mover_t *activemovers[MAX_MOVERS];
static polymover_t *activepolys[MAX_MOVERS];
short *xlat_lump;

// CODE --------------------------------------------------------------------

//==========================================================================
// Cl_InitTranslations
//	Allocates and inits the lump translation array. Clients use this 
//	to make sure lump (e.g. flats) references are correct (in case the 
//	server and the client are using different WAD configurations and 
//	the lump index numbers happen to differ).
//
//	FIXME: A bit questionable? Why not allow the clients to download 
//	data from the server in ambiguous cases?
//==========================================================================
void Cl_InitTranslations(void)
{
	int i;
	
	xlat_lump = Z_Malloc(sizeof(short) * MAX_TRANSLATIONS, PU_REFRESHTEX, 0);
	memset(xlat_lump, 0, sizeof(short) * MAX_TRANSLATIONS);
	for(i = 0; i < numlumps; i++) xlat_lump[i] = i; // Identity translation.
}

//===========================================================================
// Cl_SetLumpTranslation
//===========================================================================
void Cl_SetLumpTranslation(short lumpnum, char *name)
{
	if(lumpnum < 0 || lumpnum >= MAX_TRANSLATIONS) 
		return; // Can't do it, sir! We just don't the power!!

	xlat_lump[lumpnum] = W_CheckNumForName(name);
	if(xlat_lump[lumpnum] < 0)
	{
		VERBOSE(Con_Message("Cl_SetLumpTranslation: %s not found.\n", name));
		xlat_lump[lumpnum] = 0;
	}
}

//==========================================================================
// Cl_TranslateLump
//	This is a fail-safe operation.
//==========================================================================
short Cl_TranslateLump(short lump)
{
	if(lump < 0 || lump >= MAX_TRANSLATIONS) return 0;
	return xlat_lump[lump];
}

//==========================================================================
// Cl_InitMovers
//	Clears the arrays that track active plane and polyobj mover thinkers.
//==========================================================================
void Cl_InitMovers()
{
	memset(activemovers, 0, sizeof(activemovers));
	memset(activepolys, 0, sizeof(activepolys));
}

void Cl_RemoveActiveMover(mover_t *mover)
{
	int		i;

	for(i = 0; i < MAX_MOVERS; i++)
		if(activemovers[i] == mover)
		{
			P_RemoveThinker(&mover->thinker);
			activemovers[i] = NULL;
			break;
		}
}

//==========================================================================
// Cl_RemoveActivePoly
//	Removes the given polymover from the active polys array.
//==========================================================================
void Cl_RemoveActivePoly(polymover_t *mover)
{
	int i;

	for(i = 0; i < MAX_MOVERS; i++)
		if(activepolys[i] == mover)
		{
			P_RemoveThinker(&mover->thinker);
			activepolys[i] = NULL;
			break;
		}
}

//===========================================================================
// Cl_MoverThinker
//	Plane mover.
//===========================================================================
void Cl_MoverThinker(mover_t *mover)
{
	fixed_t	*current = mover->current, original = *current;
	boolean remove = false;
	boolean freeMove;
	
	if(!Cl_GameReady()) return; // Can we think yet?

	// The move is cancelled if the consoleplayer becomes obstructed.
	freeMove = Cl_IsFreeToMove(consoleplayer);

	// How's the gap?
	if(abs(mover->destination - *current) > abs(mover->speed))
	{
		// Do the move.
		*current += mover->speed;			
	}
	else
	{
		// We have reached the destination.
		*current = mover->destination;

		// This thinker can now be removed.
		remove = true;
	}

	P_SectorPlanesChanged(mover->sector);

	// Make sure the client didn't get stuck as a result of this move.
	if(freeMove != Cl_IsFreeToMove(consoleplayer))
	{
		// Something was blocking the way!
		*current = original;
		P_SectorPlanesChanged(mover->sector);
	}
	else if(remove)	// Can we remove this thinker?
	{
		Cl_RemoveActiveMover(mover);
	}
}

void Cl_AddMover(int sectornum, movertype_t type, fixed_t dest, fixed_t speed)
{
	sector_t	*sector;
	int			i;
	mover_t		*mov;

	if(speed == 0) return;

	if(sectornum >= numsectors) return;
	sector = SECTOR_PTR(sectornum);

	// Remove any existing movers for the same plane.
	for(i = 0; i < MAX_MOVERS; i++)
		if(activemovers[i] 
			&& activemovers[i]->sector == sector 
			&& activemovers[i]->type == type)
		{
			Cl_RemoveActiveMover(activemovers[i]);
		}

	/*if(!speed) // Lightspeed?
	{
		// Change immediately.
		if(type == mvt_ceiling)
			sector->ceilingheight = dest;
		else
			sector->floorheight = dest;
		return;
	}*/

	// Add a new mover.
	for(i = 0; i < MAX_MOVERS; i++)
		if(activemovers[i] == NULL)
		{
			// Allocate a new mover_t thinker.
			mov = activemovers[i] = Z_Malloc(sizeof(mover_t), PU_LEVEL, 0);
			memset(mov, 0, sizeof(mover_t));
			mov->thinker.function = Cl_MoverThinker;			
			mov->type = type;
			mov->sectornum = sectornum;
			mov->sector = SECTOR_PTR(sectornum);
			mov->destination = dest;
			mov->speed = abs(speed); 
			mov->current = (type == mvt_floor? 
				&mov->sector->floorheight : &mov->sector->ceilingheight);
			// Set the right sign for speed.
			if(mov->destination < *mov->current) mov->speed = -mov->speed;
			
			P_AddThinker(&mov->thinker);
			break;
		}
}

//==========================================================================
// Cl_PolyMoverThinker
//==========================================================================
void Cl_PolyMoverThinker(polymover_t *mover)
{
	polyobj_t *poly = mover->poly;
	int dx, dy, dist;

	if(mover->move)
	{
		// How much to go?
		dx = poly->dest.x - poly->startSpot.x;
		dy = poly->dest.y - poly->startSpot.y;
		dist = P_ApproxDistance(dx, dy);
		if(dist <= poly->speed || !poly->speed)
		{
			// We'll arrive at the destination.
			mover->move = false;
		}
		else
		{
			// Adjust deltas to fit speed.
			dx = FixedMul(poly->speed, FixedDiv(dx, dist));
			dy = FixedMul(poly->speed, FixedDiv(dy, dist));
		}
		// Do the move.
		PO_MovePolyobj(mover->number | 0x80000000, dx, dy);
	}

	if(mover->rotate)
	{
		// How much to go?
		dist = poly->destAngle - poly->angle;
		if((abs(dist >> 4) <= abs( ((signed) poly->angleSpeed) >> 4)
			&& poly->destAngle != -1)
			|| !poly->angleSpeed)
		{
			// We'll arrive at the destination.
			mover->rotate = false;
		}
		else
		{
			// Adjust to speed.
			dist = poly->angleSpeed;
		}
		PO_RotatePolyobj(mover->number | 0x80000000, dist);
	}

	// Can we get rid of this mover?
	if(!mover->move && !mover->rotate) Cl_RemoveActivePoly(mover);
}

//==========================================================================
// Cl_FindActivePoly
//==========================================================================
polymover_t *Cl_FindActivePoly(int number)
{
	int i;

	for(i = 0; i < MAX_MOVERS; i++)
		if(activepolys[i] && activepolys[i]->number == number)
			return activepolys[i];
	return NULL;
}

//==========================================================================
// Cl_NewPolyMover
//==========================================================================
polymover_t *Cl_NewPolyMover(int number)
{
	polymover_t *mover;
	polyobj_t *poly = PO_PTR(number);

	mover = Z_Malloc(sizeof(polymover_t), PU_LEVEL, 0);
	memset(mover, 0, sizeof(*mover));
	mover->thinker.function = Cl_PolyMoverThinker;
	mover->poly = poly;
	mover->number = number;
	P_AddThinker(&mover->thinker);
	return mover;
}

//==========================================================================
// Cl_SetPolyMover
//==========================================================================
void Cl_SetPolyMover(int number, int move, int rotate)
{
	polymover_t *mover;

	// Try to find an existing mover.
	mover = Cl_FindActivePoly(number);
	if(!mover) mover = Cl_NewPolyMover(number);
	// Flag for moving.
	if(move) mover->move = true;
	if(rotate) mover->rotate = true;
}

//==========================================================================
// Cl_RemoveMovers
//	Removes all the active movers.
//==========================================================================
void Cl_RemoveMovers()
{
	int	i;

	for(i = 0; i < MAX_MOVERS; i++)
	{
		if(activemovers[i])
		{
			P_RemoveThinker(&activemovers[i]->thinker);
			activemovers[i] = NULL;
		}
		if(activepolys[i])
		{
			P_RemoveThinker(&activepolys[i]->thinker);
			activepolys[i] = NULL;
		}
	}
}

mover_t *Cl_GetActiveMover(int sectornum, movertype_t type)
{
	int i;

	for(i = 0; i < MAX_MOVERS; i++)
		if(activemovers[i] 
			&& activemovers[i]->sectornum == sectornum
			&& activemovers[i]->type == type)
		{
			return activemovers[i];
		}
	return NULL;
}

//===========================================================================
// Cl_ReadLumpDelta
//	Returns false iff the end marker is found (lump index zero).
//===========================================================================
int Cl_ReadLumpDelta(void)
{
	int num = Msg_ReadPackedShort();
	char name[9];

	if(!num) return false; // No more.

	// Read the name of the lump.
	memset(name, 0, sizeof(name));
	Msg_Read(name, 8);

	VERBOSE( Con_Printf("LumpTranslate: %i => %s\n", num, name) );

	// Set up translation.
	Cl_SetLumpTranslation(num, name);
	return true; 
}

//==========================================================================
// Cl_ReadSectorDelta
//	Reads a sector delta from the message buffer and applies it to 
//	the world. Returns false only if the end marker is found.
//
//	THIS FUNCTION IS NOW OBSOLETE (only used with psv_frame packets)
//==========================================================================
int Cl_ReadSectorDelta(void)
{
	short num = Msg_ReadPackedShort();
	sector_t *sec;
	int df;

	// Sector number first (0 terminates).
	if(!num) return false;

	sec = SECTOR_PTR(--num);

	// Flags.
	df = Msg_ReadPackedShort();

	if(df & SDF_FLOORPIC) 
		sec->floorpic = Cl_TranslateLump( Msg_ReadPackedShort() );
	if(df & SDF_CEILINGPIC) 
		sec->ceilingpic = Cl_TranslateLump( Msg_ReadPackedShort() );
	if(df & SDF_LIGHT) sec->lightlevel = Msg_ReadByte();
	if(df & SDF_FLOOR_TARGET)
		sec->planes[PLN_FLOOR].target = Msg_ReadShort() << 16;
	if(df & SDF_FLOOR_SPEED)
	{
		sec->planes[PLN_FLOOR].speed 
			= Msg_ReadByte() << (df & SDF_FLOOR_SPEED_44? 12 : 15);
	}
	if(df & SDF_FLOOR_TEXMOVE)
	{
		sec->planes[PLN_FLOOR].texmove[0] = Msg_ReadShort() << 8;
		sec->planes[PLN_FLOOR].texmove[1] = Msg_ReadShort() << 8;
	}
	if(df & SDF_CEILING_TARGET)
		sec->planes[PLN_CEILING].target = Msg_ReadShort() << 16;
	if(df & SDF_CEILING_SPEED)
	{
		sec->planes[PLN_CEILING].speed 
			= Msg_ReadByte() << (df & SDF_CEILING_SPEED_44? 12 : 15);
	}
	if(df & SDF_CEILING_TEXMOVE)
	{
		sec->planes[PLN_CEILING].texmove[0] = Msg_ReadShort() << 8;
		sec->planes[PLN_CEILING].texmove[1] = Msg_ReadShort() << 8;
	}
	if(df & SDF_COLOR_RED) sec->rgb[0] = Msg_ReadByte();
	if(df & SDF_COLOR_GREEN) sec->rgb[1] = Msg_ReadByte();
	if(df & SDF_COLOR_BLUE) sec->rgb[2] = Msg_ReadByte();

	// Do we need to start any moving planes?
	if(df & (SDF_FLOOR_TARGET | SDF_FLOOR_SPEED))
	{
		Cl_AddMover(num, mvt_floor, sec->planes[PLN_FLOOR].target, 
			sec->planes[PLN_FLOOR].speed);
	}
	if(df & (SDF_CEILING_TARGET | SDF_CEILING_SPEED))
	{
		Cl_AddMover(num, mvt_ceiling, sec->planes[PLN_CEILING].target,
			sec->planes[PLN_CEILING].speed);
	}

	// Continue reading.
	return true;
}

//==========================================================================
// Cl_ReadSideDelta
//	Reads a side delta from the message buffer and applies it to 
//	the world. Returns false only if the end marker is found.
//
//	THIS FUNCTION IS NOW OBSOLETE (only used with psv_frame packets)
//==========================================================================
int Cl_ReadSideDelta(void)
{
	short num = Msg_ReadPackedShort();
	side_t *sid;
	int df;

	// Side number first (0 terminates).
	if(!num) return false;

	sid = SIDE_PTR(--num);

	// Flags.
	df = Msg_ReadByte();

	if(df & SIDF_TOPTEX) 
		sid->toptexture = Msg_ReadPackedShort();
	if(df & SIDF_MIDTEX) 
		sid->midtexture = Msg_ReadPackedShort();
	if(df & SIDF_BOTTOMTEX) 
		sid->bottomtexture = Msg_ReadPackedShort();
	if(df & SIDF_LINE_FLAGS)
	{
		byte updatedFlags = Msg_ReadByte();
		line_t *line = R_GetLineForSide(num);

		if(line)
		{
			// The delta includes the lowest byte.
			line->flags &= ~0xff;
			line->flags |= updatedFlags;
#if _DEBUG
			Con_Printf("lineflag %i: %02x\n", GET_LINE_IDX(line), 
				updatedFlags);
#endif
		}
	}

	// Continue reading.
	return true;
}

//==========================================================================
// Cl_ReadPolyDelta
//	Reads a poly delta from the message buffer and applies it to 
//	the world. Returns false only if the end marker is found.
//
//	THIS FUNCTION IS NOW OBSOLETE (only used with psv_frame packets)
//==========================================================================
int Cl_ReadPolyDelta(void)
{
	int df;
	short num = Msg_ReadPackedShort();
	polyobj_t *po;

	// Check the number. A zero terminates.
	if(!num) return false;

	po = PO_PTR(--num);

	// Flags.
	df = Msg_ReadPackedShort();

	if(df & PODF_DEST_X) 
		po->dest.x = (Msg_ReadShort() << 16) + ((char)Msg_ReadByte() << 8);
	if(df & PODF_DEST_Y)
		po->dest.y = (Msg_ReadShort() << 16) + ((char)Msg_ReadByte() << 8);
	if(df & PODF_SPEED) po->speed = Msg_ReadShort() << 8;
	if(df & PODF_DEST_ANGLE) po->destAngle = Msg_ReadShort() << 16;
	if(df & PODF_ANGSPEED) po->angleSpeed = Msg_ReadShort() << 16;

	/*CON_Printf("num=%i:\n", num);
	CON_Printf("  destang=%x angsp=%x\n", po->destAngle, po->angleSpeed);*/

	if(df & PODF_PERPETUAL_ROTATE) po->destAngle = -1;

	// Update the polyobj's mover thinkers.
	Cl_SetPolyMover(num, df & (PODF_DEST_X | PODF_DEST_Y | PODF_SPEED),
		df & (PODF_DEST_ANGLE | PODF_ANGSPEED | PODF_PERPETUAL_ROTATE));

	// Continue reading.
	return true;
}

/*
 * Reads a sector delta from the psv_frame2 message buffer and applies it 
 * to the world.
 */
void Cl_ReadSectorDelta2(boolean skip)
{
	unsigned short num;
	sector_t *sec;
	int df;
	boolean wasChanged = false;
	static sector_t dummy; // Used when skipping.

	// Sector index number.
	num = Msg_ReadShort();

	// Flags.
	df = Msg_ReadShort();

	if(!skip)
	{
#ifdef _DEBUG
		if(num >= numsectors)
		{
			// This is worrisome.
			Con_Error("Cl_ReadSectorDelta2: Sector %i out of range.\n", num);
		}
#endif
		sec = SECTOR_PTR(num);
	}
	else
	{
		// Read the data into the dummy if we're skipping.
		sec = &dummy;
	}

	if(df & SDF_FLOORPIC) 
		sec->floorpic = Cl_TranslateLump( Msg_ReadPackedShort() );
	if(df & SDF_CEILINGPIC) 
		sec->ceilingpic = Cl_TranslateLump( Msg_ReadPackedShort() );
	if(df & SDF_LIGHT) sec->lightlevel = Msg_ReadByte();
	if(df & SDF_FLOOR_HEIGHT) 
	{
		sec->floorheight = Msg_ReadShort() << 16;
		wasChanged = true;
	}
	if(df & SDF_CEILING_HEIGHT)
	{
		sec->ceilingheight = Msg_ReadShort() << 16;
		wasChanged = true;
	}
	if(df & SDF_FLOOR_TARGET)
		sec->planes[PLN_FLOOR].target = Msg_ReadShort() << 16;
	if(df & SDF_FLOOR_SPEED)
	{
		sec->planes[PLN_FLOOR].speed 
			= Msg_ReadByte() << (df & SDF_FLOOR_SPEED_44? 12 : 15);
	}
	if(df & SDF_FLOOR_TEXMOVE)
	{
		sec->planes[PLN_FLOOR].texmove[0] = Msg_ReadShort() << 8;
		sec->planes[PLN_FLOOR].texmove[1] = Msg_ReadShort() << 8;
	}
	if(df & SDF_CEILING_TARGET)
		sec->planes[PLN_CEILING].target = Msg_ReadShort() << 16;
	if(df & SDF_CEILING_SPEED)
	{
		sec->planes[PLN_CEILING].speed 
			= Msg_ReadByte() << (df & SDF_CEILING_SPEED_44? 12 : 15);
	}
	if(df & SDF_CEILING_TEXMOVE)
	{
		sec->planes[PLN_CEILING].texmove[0] = Msg_ReadShort() << 8;
		sec->planes[PLN_CEILING].texmove[1] = Msg_ReadShort() << 8;
	}
	if(df & SDF_COLOR_RED) sec->rgb[0] = Msg_ReadByte();
	if(df & SDF_COLOR_GREEN) sec->rgb[1] = Msg_ReadByte();
	if(df & SDF_COLOR_BLUE) sec->rgb[2] = Msg_ReadByte();

	// The whole delta has been read. If we're about to skip, let's do so.
	if(skip) return;

	// If the plane heights were changed, we need to update the mobjs in
	// the sector.
	if(wasChanged)
	{
		P_SectorPlanesChanged(sec);
	}

	// Do we need to start any moving planes?
	if(df & (SDF_FLOOR_TARGET | SDF_FLOOR_SPEED))
	{
		Cl_AddMover(num, mvt_floor, sec->planes[PLN_FLOOR].target, 
			sec->planes[PLN_FLOOR].speed);
	}
	if(df & (SDF_CEILING_TARGET | SDF_CEILING_SPEED))
	{
		Cl_AddMover(num, mvt_ceiling, sec->planes[PLN_CEILING].target,
			sec->planes[PLN_CEILING].speed);
	}
}

/*
 * Reads a side delta from the message buffer and applies it to the world.
 */
void Cl_ReadSideDelta2(boolean skip)
{
	unsigned short num;
	int df, toptexture, midtexture, bottomtexture;
	byte lineFlags;
	side_t *sid;

	// First read all the data.
	num = Msg_ReadShort();

	// Flags.
	df = Msg_ReadByte();

	if(df & SIDF_TOPTEX) toptexture = Msg_ReadPackedShort();
	if(df & SIDF_MIDTEX) midtexture = Msg_ReadPackedShort();
	if(df & SIDF_BOTTOMTEX) bottomtexture = Msg_ReadPackedShort();
	if(df & SIDF_LINE_FLAGS) lineFlags = Msg_ReadByte();

	// Must we skip this?
	if(skip) return;
	
#ifdef _DEBUG
	if(num >= numsides)
	{
		// This is worrisome.
		Con_Error("Cl_ReadSideDelta2: Side %i out of range.\n", num);
	}
#endif

	sid = SIDE_PTR(num);

	if(df & SIDF_TOPTEX) 
		sid->toptexture = toptexture;
	if(df & SIDF_MIDTEX) 
		sid->midtexture = midtexture;
	if(df & SIDF_BOTTOMTEX) 
		sid->bottomtexture = bottomtexture;
	if(df & SIDF_LINE_FLAGS)
	{
		line_t *line = R_GetLineForSide(num);
		if(line)
		{
			// The delta includes the entire lowest byte.
			line->flags &= ~0xff;
			line->flags |= lineFlags;
#if _DEBUG
			Con_Printf("Cl_ReadSideDelta2: Lineflag %i: %02x\n", 
				GET_LINE_IDX(line), lineFlags);
#endif
		}
	}
}

/*
 * Reads a poly delta from the message buffer and applies it to 
 * the world.
 */
void Cl_ReadPolyDelta2(boolean skip)
{
	int df;
	unsigned short num;
	polyobj_t *po;
	int destX, destY, speed, destAngle, angleSpeed;

	num = Msg_ReadPackedShort();

	// Flags.
	df = Msg_ReadByte();

	if(df & PODF_DEST_X) 
		destX = (Msg_ReadShort() << 16) + ((char)Msg_ReadByte() << 8);
	if(df & PODF_DEST_Y)
		destY = (Msg_ReadShort() << 16) + ((char)Msg_ReadByte() << 8);
	if(df & PODF_SPEED) speed = Msg_ReadShort() << 8;
	if(df & PODF_DEST_ANGLE) destAngle = Msg_ReadShort() << 16;
	if(df & PODF_ANGSPEED) angleSpeed = Msg_ReadShort() << 16;

	if(skip) return;

#ifdef _DEBUG
	if(num >= po_NumPolyobjs)
	{
		// This is worrisome.
		Con_Error("Cl_ReadPolyDelta2: PO %i out of range.\n", num);
	}
#endif

	po = PO_PTR(num);

	if(df & PODF_DEST_X) po->dest.x = destX;
	if(df & PODF_DEST_Y) po->dest.y = destY;
	if(df & PODF_SPEED) po->speed = speed;
	if(df & PODF_DEST_ANGLE) po->destAngle = destAngle;
	if(df & PODF_ANGSPEED) po->angleSpeed = angleSpeed;
	if(df & PODF_PERPETUAL_ROTATE) po->destAngle = -1;

	// Update the polyobj's mover thinkers.
	Cl_SetPolyMover(num, df & (PODF_DEST_X | PODF_DEST_Y | PODF_SPEED),
		df & (PODF_DEST_ANGLE | PODF_ANGSPEED | PODF_PERPETUAL_ROTATE));
}
