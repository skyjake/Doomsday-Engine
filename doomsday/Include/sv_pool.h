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
 * sv_pool.h: Delta Pools
 */

#ifndef __DOOMSDAY_SERVER_POOL_H__
#define __DOOMSDAY_SERVER_POOL_H__

#include "dd_share.h"
#include "p_mobj.h"

typedef enum
{
	DT_MOBJ,
	DT_PLAYER,
	DT_SECTOR,
	DT_SIDE,
	DT_POLY,
	DT_LUMP,
	DT_SOUND,			// No emitter
	DT_MOBJ_SOUND,		
	DT_SECTOR_SOUND,
	DT_POLY_SOUND,
	NUM_DELTA_TYPES,

	// Special types: (only in the psv_frame2 packet)
	DT_NULL_MOBJ,		// Mobj was removed (just type and ID).
	DT_CREATE_MOBJ		// Regular DT_MOBJ, but the mobj was just created.

} deltatype_t;

// OR'd with the type number when resending Unacked deltas.
#define DT_RESENT				0x80

// Mobj delta flags. These are used to determine what a delta contains.
// (Which parts of a delta mobj_t are used.)
#define MDF_POS_X				0x0001
#define MDF_POS_Y				0x0002
#define MDF_POS_Z				0x0004
#define MDF_POS					0x0007
#define MDF_MOM_X				0x0008
#define MDF_MOM_Y				0x0010
#define MDF_MOM_Z				0x0020
#define MDF_MOM					0x0038
#define MDF_ANGLE				0x0040
#define MDF_LONG_FLOORCLIP		0x0080
#define MDF_MORE_FLAGS			0x0100	// A byte of extra flags follows.
#define MDF_SELSPEC				0x0200	// Only during transfer.
#define MDF_SELECTOR			0x0400
#define MDF_STATE				0x0800
#define MDF_RADIUS				0x1000
#define MDF_HEIGHT				0x2000
#define MDF_FLAGS				0x4000
#define MDF_FLOORCLIP			0x8000

// Mobj Delta Control flags (not included directly in the frame).
#define MDFC_NULL				0x10000	// The delta is not defined.
#define MDFC_CREATE				0x20000	// Mobj didn't exist before.
#define MDFC_TRANSLUCENCY		0x40000	// Mobj has translucency.

// Extra flags for the Extra Flags byte.
#define MDFE_FAST_MOM			0x01	// Momentum has 10.6 bits (+/- 512)	
#define MDFE_TRANSLUCENCY		0x02
#define MDFE_Z_FLOOR			0x04	// Mobj z is on the floor.
#define MDFE_Z_CEILING			0x08	// Mobj z+hgt is in the ceiling.

// The flags that are not included when a mobj is the viewpoint.
#define MDF_CAMERA_EXCLUDE		0x0e00

// The flags that are not included for hidden mobjs.
#define MDF_DONTDRAW_EXCLUDE	0x0ec0

#define PDF_MOBJ				0x0001
#define PDF_FORWARDMOVE			0x0002
#define PDF_SIDEMOVE			0x0004
#define PDF_ANGLE				0x0008
#define PDF_TURNDELTA			0x0010
#define PDF_FRICTION			0x0020
#define PDF_EXTRALIGHT			0x0040	// Plus fixedcolormap (same byte).
#define PDF_FILTER				0x0080
#define PDF_CLYAW				0x1000	// Sent in the player num byte.
#define PDF_CLPITCH				0x2000	// Sent in the player num byte.
#define PDF_PSPRITES			0x4000	// Sent in the player num byte.

// Written separately, stored in playerdelta flags 2 highest bytes.
#define PSDF_STATEPTR			0x01
#define PSDF_OFFSET				0x08
#define PSDF_LIGHT				0x20
#define PSDF_ALPHA				0x40
#define PSDF_STATE				0x80

// The flags that are not included when a player is the viewpoint.
#define PDF_CAMERA_EXCLUDE		0x001e

// The flags that are not included when a player is not the viewpoint.
#define PDF_NONCAMERA_EXCLUDE	0x70de

#define SDF_FLOORPIC			0x0001
#define SDF_CEILINGPIC			0x0002
#define SDF_LIGHT				0x0004
#define SDF_FLOOR_TARGET		0x0008
#define SDF_FLOOR_SPEED			0x0010
#define SDF_CEILING_TARGET		0x0020
#define SDF_CEILING_SPEED		0x0040
#define SDF_FLOOR_TEXMOVE		0x0080
#define SDF_CEILING_TEXMOVE		0x0100
#define SDF_COLOR_RED			0x0200
#define SDF_COLOR_GREEN			0x0400
#define SDF_COLOR_BLUE			0x0800
#define SDF_FLOOR_SPEED_44		0x1000	// Used for sent deltas.
#define SDF_CEILING_SPEED_44	0x2000	// Used for sent deltas.
#define SDF_FLOOR_HEIGHT		0x4000
#define SDF_CEILING_HEIGHT		0x8000

#define SIDF_TOPTEX				0x01
#define SIDF_MIDTEX				0x02
#define SIDF_BOTTOMTEX			0x04
#define SIDF_LINE_FLAGS			0x08

#define PODF_DEST_X				0x01
#define PODF_DEST_Y				0x02
#define PODF_SPEED				0x04
#define PODF_DEST_ANGLE			0x08
#define PODF_ANGSPEED			0x10
#define PODF_PERPETUAL_ROTATE	0x20	// Special flag.

#define LDF_INFO				0x01

#define SNDDF_VOLUME			0x01	// 0=stop, 1=full, >1=no att.
#define SNDDF_REPEAT			0x02	// Start repeating sound.

typedef enum deltastate_e {
	DELTA_NEW,
	DELTA_UNACKED
} deltastate_t;

/*
 * All delta structures begin the same way (with a delta_t).
 * That way they can all be linked into the same hash table.
 */
typedef struct delta_s 
{
	// Links to the next and previous delta in the hash.
	struct delta_s *next, *prev;

	// The ID number and type determine the entity this delta applies to.
	deltatype_t type;
	uint id;

	// The priority score tells how badly the delta needs to be sent to
	// the client.
	float score;

	// Deltas can be either New or Unacked. New deltas haven't yet been sent.
	deltastate_t state;

	// ID of the delta set. Assigned when the delta is sent to a client.
	// All deltas in the same frame update have the same set ID.
	// Clients acknowledge complete sets (and then the whole set is removed).
	byte set;		

	// Resend ID of this delta. Assigned when the delta is first resent.
	// Zero means there is no resend ID.
	byte resend;
					
	// System time when the delta was sent.
	uint timeStamp;

	int flags;		
} 
delta_t;

typedef mobj_t dt_mobj_t;

typedef struct 
{
	delta_t	delta;				// The header.
	dt_mobj_t mo;				// The data of the delta.
} 
mobjdelta_t;

typedef struct
{
	thid_t		mobj;
	char		forwardMove;
	char		sideMove;
	int			angle;
	int			turnDelta;
	int			friction;
	int			extraLight;
	int			fixedColorMap;
	int			filter;
	int			clYaw;
	float		clPitch;
	ddpsprite_t	psp[2];			// Player sprites.
}
dt_player_t;

typedef struct
{
	delta_t	delta;				// The header.
	dt_player_t player;
} 
playerdelta_t;

typedef struct
{
	short		floorPic;
	short		ceilingPic;
	short		lightlevel;
	byte		rgb[3];
	plane_t		planes[2];
	int			floorHeight;
	int			ceilingHeight;
}
dt_sector_t;

typedef struct
{
	delta_t		delta;
	dt_sector_t	sector;
} 
sectordelta_t;

typedef struct
{
	delta_t		delta;
} 
lumpdelta_t;

typedef struct
{
	short		topTexture;
	short		midTexture;
	short		bottomTexture;
	byte		lineFlags;	// note: only a byte!
}
dt_side_t;

typedef struct
{
	delta_t		delta;
	dt_side_t	side;
} 
sidedelta_t;

typedef struct
{
	vertex_t	dest;
	int			speed;				
	angle_t		destAngle;
	angle_t		angleSpeed;	
}
dt_poly_t;

typedef struct
{
	delta_t		delta;
	dt_poly_t	po;
} 
polydelta_t;

typedef struct
{
	delta_t		delta;		// id = Emitter identifier (mobjid/sectoridx)
	int			sound;		// Sound ID
	mobj_t		*mobj;
	float		volume;
}
sounddelta_t;

/*
 * One hash table holds all the deltas in a pool.
 * (delta ID number) & (mask) is the key.
 */
#define POOL_HASH_SIZE				1024
#define POOL_HASH_FUNCTION_MASK		0x3ff

/*
 * The missile record contains an entry for each missile mobj that 
 * the client has acknowledged. Since missiles move predictably, 
 * we do not need to sent the coordinates in every delta. The record 
 * is checked every time a missile delta is added to a pool.
 */
#define POOL_MISSILE_HASH_SIZE		256

typedef struct misrecord_s
{
	struct misrecord_s *next, *prev;
	thid_t id;
	//fixed_t momx, momy, momz;
}
misrecord_t;

typedef struct mislink_s 
{
	misrecord_t *first, *last;
}
mislink_t;

typedef struct deltalink_s
{
	// Links to the first and last delta in the hash key.
	struct delta_s *first, *last;
}
deltalink_t;

/*
 * When calculating priority scores, this struct is used to store
 * information about the owner of the pool.
 */
typedef struct ownerinfo_s
{
	struct pool_s *pool;
	fixed_t	x, y, z;		// Distance is the most important factor
	angle_t angle;			// Angle can change rapidly => not very important
	fixed_t speed;
	uint ackThreshold;		// Expected ack time in milliseconds
}
ownerinfo_t;

/*
 * Each client has a delta pool. 
 */
typedef struct pool_s
{
	// True if the first frame has not yet been sent.
	boolean isFirst;

	// The number of the console this pool belongs to. (i.e. player number)
	uint owner;
	ownerinfo_t ownerInfo;

	// The set ID numbers are generated using this value. It's 
	// incremented after each transmitted set.
	byte setDealer;

	// The resend ID numbers are generated using this value. It's incremented
	// for each resent delta. Zero is not used.
	byte resendDealer;

	// The delta hash table holds all kinds of deltas.
	deltalink_t hash[POOL_HASH_SIZE];
	
	// The missile record is used to detect when the mobj coordinates need
	// not be sent.
	mislink_t misHash[POOL_MISSILE_HASH_SIZE];

	// The priority queue (a heap). Built when the pool contents are rated.
	// Contains pointers to deltas in the hash. Becomes invalid when deltas
	// are removed from the hash!
	int queueSize;
	int allocatedSize;
	delta_t **queue;
} 
pool_t;

void		Sv_InitPools(void);
void		Sv_ShutdownPools(void);
void		Sv_DrainPool(int clientNumber);
void		Sv_InitPoolForClient(int clientNumber);
void		Sv_MobjRemoved(thid_t id);
void		Sv_PlayerRemoved(int playerNumber);
void		Sv_GenerateFrameDeltas(void);
boolean		Sv_IsFrameTarget(int number);
uint		Sv_GetTimeStamp(void);
pool_t*		Sv_GetPool(int consoleNumber);
void		Sv_RatePool(pool_t *pool);
delta_t*	Sv_PoolQueueExtract(pool_t *pool);
void		Sv_AckDeltaSet(int consoleNumber, int set, byte resent);

void		Sv_NewSoundDelta(int soundId, mobj_t *emitter, int sourceSector, 
				int sourcePoly, float volume, boolean isRepeating, 
				int justForClient);

#endif
