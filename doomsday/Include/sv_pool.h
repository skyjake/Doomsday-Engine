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
	DT_LUMP
} deltatype_t;

// Mobj delta flags. These are used to determine what a delta contains.
// (Which parts of a delta mobj_t are used.)
#define MDF_POS_X		0x0001
#define MDF_POS_Y		0x0002
#define MDF_POS_Z		0x0004
#define MDF_POS			0x0007
#define MDF_MOM_X		0x0008
#define MDF_MOM_Y		0x0010
#define MDF_MOM_Z		0x0020
#define MDF_MOM			0x0038
#define MDF_ANGLE		0x0040
//#define MDF_TICS		0x0080
//#define MDF_SPRITE	0x0100
#define MDF_SELSPEC		0x0200	// Only during transfer.
#define MDF_SELECTOR	0x0400
#define MDF_STATE		0x0800
#define MDF_RADIUS		0x1000
#define MDF_HEIGHT		0x2000
#define MDF_FLAGS		0x4000
#define MDF_FLOORCLIP	0x8000
#define MDF_NULL		0x10000	// The delta is not defined.

// The flags that are not included when a mobj is the viewpoint.
#define MDF_CAMERA_EXCLUDE 0x0f80

// The flags that are not included for hidden mobjs.
#define MDF_DONTDRAW_EXCLUDE 0x0fc0

#define PDF_MOBJ		0x0001
#define PDF_FORWARDMOVE	0x0002
#define PDF_SIDEMOVE	0x0004
#define PDF_ANGLE		0x0008
#define PDF_TURNDELTA	0x0010
#define PDF_FRICTION	0x0020
#define PDF_EXTRALIGHT	0x0040	// Plus fixedcolormap (same byte).
#define PDF_FILTER		0x0080
#define PDF_CLYAW		0x1000	// Sent in the player num byte.
#define PDF_CLPITCH		0x2000	// Sent in the player num byte.
#define PDF_PSPRITES	0x4000	// Sent in the player num byte.

// Written separately, stored in playerdelta flags 2 highest bytes.
//#define PSDF_SPRITE		0x01
//#define PSDF_FRAME		0x02
//#define PSDF_NEXTFRAME	0x04
//#define PSDF_NEXTTIME		0x08
//#define PSDF_NEXT		0x04
#define PSDF_STATEPTR	0x01
#define PSDF_OFFSET		0x08
//#define PSDF_TICS		0x10
#define PSDF_LIGHT		0x20
#define PSDF_ALPHA		0x40
#define PSDF_STATE		0x80

// The flags that are not included when a player is the viewpoint.
#define PDF_CAMERA_EXCLUDE 0x001e

// The flags that are not included when a player is not the viewpoint.
#define PDF_NONCAMERA_EXCLUDE 0x70de

#define SDF_FLOORPIC		0x0001
#define SDF_CEILINGPIC		0x0002
#define SDF_LIGHT			0x0004
#define SDF_FLOOR_TARGET	0x0008
#define SDF_FLOOR_SPEED		0x0010
#define SDF_CEILING_TARGET	0x0020
#define SDF_CEILING_SPEED	0x0040
#define SDF_FLOOR_TEXMOVE	0x0080
#define SDF_CEILING_TEXMOVE	0x0100
#define SDF_COLOR_RED		0x0200
#define SDF_COLOR_GREEN		0x0400
#define SDF_COLOR_BLUE		0x0800
#define SDF_FLOOR_SPEED_44	0x1000		// Used for sent deltas.
#define SDF_CEILING_SPEED_44 0x2000		// Used for sent deltas.

#define SIDF_TOPTEX			0x01
#define SIDF_MIDTEX			0x02
#define SIDF_BOTTOMTEX		0x04

#define PODF_DEST_X				0x01
#define PODF_DEST_Y				0x02
#define PODF_SPEED				0x04
#define PODF_DEST_ANGLE			0x08
#define PODF_ANGSPEED			0x10
#define PODF_PERPETUAL_ROTATE	0x20	// Special flag.

#define LDF_INFO			0x01

// All delta structures begin the same way (with a delta_t).
typedef struct delta_s
{
	deltatype_t		type;
	struct delta_s	*next, *prev;
	byte			set;		// Number of the Set.
	byte			senttime;	// LB of gametic when the delta was sent.
	int				flags;		// Depends on the type.
} delta_t;

typedef struct 
{
	delta_t		delta;			// The header.
	mobj_t		data;			// The data of the delta.
} mobjdelta_t;

typedef struct
{
	delta_t		delta;			// The header.
	int			player;			// Player number.
	thid_t		mobjid;
	char		forwardmove;
	char		sidemove;
	int			angle;
	int			turndelta;
	int			friction;
	int			extralight;
	int			fixedcolormap;
	int			filter;
	int			clyaw;
	float		clpitch;
	ddpsprite_t	psp[2];			// Player sprites.
} playerdelta_t;

typedef struct
{
	delta_t		delta;
	short		number;
	short		floorpic;
	short		ceilingpic;
	short		lightlevel;
	byte		rgb[3];
	plane_t		planes[2];
} sectordelta_t;

typedef struct
{
	delta_t		delta;
	int			number;
} lumpdelta_t;

typedef struct
{
	delta_t		delta;
	short		number;
	short		toptexture;
	short		midtexture;
	short		bottomtexture;
} sidedelta_t;

typedef struct
{
	delta_t		delta;
	short		number;
	vertex_t	dest;
	int			speed;				
	angle_t		destAngle;
	angle_t		angleSpeed;	
} polydelta_t;

// Each client has its own Delta Pool.
// A bunch of linked lists.
typedef struct
{
	byte		setNumber;		// Number of the current Set.
	delta_t		setRoot;		// Root of the Delta Sets list.
	// Registers.
	delta_t		mobjReg;		// Image of the client's mobjs.
	delta_t		playReg;		// Image of playerstate on the client.
	sectordelta_t *secReg;		// Image of sectors on the client.
	sidedelta_t *sideReg;		// Image of sides on the client.
	polydelta_t	*polyReg;		// Image of polyobjs on the client.
} pool_t;

extern int net_resendtime;
extern int net_showsets;
extern int net_maxdif;
extern int net_minsecupd;
extern int net_fullsecupd;
extern int net_maxsecupd;

extern pool_t pools[];

#endif