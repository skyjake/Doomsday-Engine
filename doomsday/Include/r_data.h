//===========================================================================
// R_DATA.H
//	Data structures for refresh.
//===========================================================================
#ifndef __DOOMSDAY_REFRESH_DATA_H__
#define __DOOMSDAY_REFRESH_DATA_H__

#include "p_data.h"
#include "p_think.h"
#include "m_nodepile.h"
#include "dedfile.h"

#define SIF_VISIBLE			0x1			// Sector is visible on this frame.
#define SIF_FRAME_CLEAR		0x1			// Flags to clear before each frame.

// Sector flags.
#define SECF_INVIS_FLOOR	0x1
#define SECF_INVIS_CEILING	0x2

#define SECT_FLOOR(x)	(secinfo[GET_SECTOR_IDX(x)].visfloor)
#define SECT_CEIL(x)	(secinfo[GET_SECTOR_IDX(x)].visceiling)

// Flags for decorations.
#define DCRF_NO_IWAD	0x1		// Don't use if from IWAD.
#define DCRF_PWAD		0x2		// Can use if from PWAD.
#define DCRF_EXTERNAL	0x4		// Can use if from external resource.

// Texture flags.
#define TXF_MASKED		0x1
#define TXF_GLOW		0x2		// For lava etc, textures that glow.

enum
{ // bbox coordinates
	BOXTOP,
	BOXBOTTOM,
	BOXLEFT,
	BOXRIGHT
};

// This is the dummy mobj_t used for blockring roots.
// It has some excess information since it has to be compatible with 
// regular mobjs (otherwise the rings don't really work).
// Note: the thinker and x,y,z data could be used for something else...
typedef struct linkmobj_s
{
	thinker_t		thinker;
	fixed_t			x,y,z;	
	struct mobj_s	*next, *prev;
} linkmobj_t;

typedef struct 
{
	float	visfloor, visceiling;	// Visible floor and ceiling, floats.
	sector_t *linkedfloor;			// Floor attached to another sector.
	sector_t *linkedceiling;		// Ceiling attached to another sector.
	boolean permanentlink;
	float	bounds[4];				// Bounding box for the sector.
	int		flags;
	fixed_t oldfloor[2], oldceiling[2];
	float	visflooroffset, visceilingoffset;
} sectorinfo_t;

typedef struct
{
	float	length;					// Accurate length.
} lineinfo_t;

typedef struct polyblock_s
{
	polyobj_t *polyobj;
	struct polyblock_s *prev;
	struct polyblock_s *next;
} polyblock_t;

typedef struct vertexowner_s
{
	unsigned short	num;			// Number of owners.
	unsigned short	*list;			// Sector indices.
} vertexowner_t;

// The sector divisions list is similar to vertexowners.
typedef struct vertexowner_s sector_divisions_t;

// Detail texture information.
typedef struct detailinfo_s {
	DGLuint	tex;
	int		width, height;
	float	strength;
	float	scale;
	float	maxdist;
} detailinfo_t;

typedef struct
{
	int		originx;	// block origin (allways UL), which has allready
	int		originy;	// accounted  for the patch's internal origin
	int		patch;
} texpatch_t;

// a maptexturedef_t describes a rectangular texture, which is composed of one
// or more mappatch_t structures that arrange graphic patches
typedef struct
{
	char		name[9];		// for switch changing, etc; ends in \0
	short		width;
	short		height;
	int			flags;			// TXF_* flags.
	short		patchcount;
	DGLuint		tex;			// Name of the associated DGL texture.
	byte		masked;			// Is the (DGL) texture masked?
	detailinfo_t detail;		// Detail texture information.
	int			group;			// Animation group (or zero).
	struct ded_decor_s *decoration;	// Pointer to the surface decoration, if any.
	texpatch_t	patches[1];		// [patchcount] drawn back to front
								//  into the cached texture
} texture_t;

typedef struct
{
	byte		rgb[3];
} rgbcol_t;

typedef struct flat_s
{
	int			lump;
	int			translation;
	short		flags;
	rgbcol_t	color;
	detailinfo_t detail;		// Detail texture information.
	int			group;			// Animation group (or zero).
	struct ded_decor_s *decoration;	// Pointer to the surface decoration, if any.
} flat_t;

typedef struct lumptexinfo_s
{
	DGLuint		tex[2];		// Names of the textures (two parts for big ones)
	unsigned short width[2], height;
	short		offx, offy;
} lumptexinfo_t;

extern vertexowner_t *vertexowners;
extern sectorinfo_t *secinfo;
extern lineinfo_t *lineinfo;
extern nodeindex_t *linelinks;
extern short *blockmaplump;			// offsets in blockmap are from here
extern short *blockmap;
extern int bmapwidth, bmapheight;	// in mapblocks
extern fixed_t bmaporgx, bmaporgy;	// origin of block map
extern linkmobj_t *blockrings;		
extern polyblock_t **polyblockmap;
extern byte *rejectmatrix;			// for fast sight rejection
extern nodepile_t thingnodes, linenodes;

extern	lumptexinfo_t	*lumptexinfo;
extern	int				numlumptexinfo;
extern  int             viewwidth, viewheight;
extern	int				numtextures;		
extern	texture_t		**textures;
extern  int             *flattranslation;		// for global animation
extern  int             *texturetranslation;	// for global animation
extern	int				LevelFullBright;
extern  int             numflats;
extern flat_t			*flats;
extern	int				r_texglow;
extern	int				r_precache_sprites, r_precache_skins;

void    R_InitData (void);
void	R_UpdateData (void);
void	R_PrecacheLevel (void);
int		R_TextureFlags(int texture);
int		R_FlatFlags(int flat);
flat_t*	R_FindFlat(int lumpnum);	// May return NULL.
flat_t*	R_GetFlat(int lumpnum);		// Creates new entries.
int		R_FlatNumForName (char *name);
int		R_CheckTextureNumForName (char *name);
int		R_TextureNumForName (char *name);
char *	R_TextureNameForNum(int num);
int		R_SetFlatTranslation(int flat, int translateTo);
int		R_SetTextureTranslation(int tex, int translateTo);
void	R_SetAnimGroup(int type, int number, int group);
boolean	R_IsCustomTexture(int texture);
boolean	R_IsAllowedDecoration(ded_decor_t *def, int index, boolean hasExternal);

#endif