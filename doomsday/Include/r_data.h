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
 *
 * Based on Hexen by Raven Software.
 */

/*
 * r_data.h: Data Structures and Constants for Refresh
 */

#ifndef __DOOMSDAY_REFRESH_DATA_H__
#define __DOOMSDAY_REFRESH_DATA_H__

#include "dd_def.h"
#include "p_data.h"
#include "p_think.h"
#include "m_nodepile.h"
#include "de_defs.h"

#define SIF_VISIBLE			0x1			// Sector is visible on this frame.
#define SIF_FRAME_CLEAR		0x1			// Flags to clear before each frame.

// Sector flags.
#define SECF_INVIS_FLOOR	0x1
#define SECF_INVIS_CEILING	0x2

#define LINE_INFO(x)	(lineinfo + GET_LINE_IDX(x))
#define SUBSECT_INFO(x)	(subsecinfo + GET_SUBSECTOR_IDX(x))
#define SECT_INFO(x)	(secinfo + GET_SECTOR_IDX(x))
#define SECT_FLOOR(x)	(secinfo[GET_SECTOR_IDX(x)].visfloor)
#define SECT_CEIL(x)	(secinfo[GET_SECTOR_IDX(x)].visceil)

// Flags for decorations.
#define DCRF_NO_IWAD	0x1		// Don't use if from IWAD.
#define DCRF_PWAD		0x2		// Can use if from PWAD.
#define DCRF_EXTERNAL	0x4		// Can use if from external resource.

// Texture flags.
#define TXF_MASKED		0x1
#define TXF_GLOW		0x2		// For lava etc, textures that glow.

// Animation group flags.
#define AGF_SMOOTH		0x1
#define AGF_FIRST_ONLY	0x2
#define AGF_TEXTURE		0x1000
#define AGF_FLAT		0x2000
#define AGF_PRECACHE	0x4000	// Group is just for precaching.

// Detail texture information.
typedef struct detailinfo_s {
	DGLuint	tex;
	int		width, height;
	float	strength;
	float	scale;
	float	maxdist;
} detailinfo_t;

typedef struct gltexture_s {
	DGLuint	id;
	float width, height;
	detailinfo_t *detail;
} gltexture_t;

typedef struct glcommand_vertex_s {
	float s, t;
	int index;
} glcommand_vertex_t;

#define RL_MAX_POLY_SIDES	64
#define RL_MAX_DIVS			64

// Rendpoly flags.
#define RPF_MASKED		0x0001	// Use the special list for masked textures.
#define RPF_SKY_MASK	0x0004	// A sky mask polygon.
#define RPF_LIGHT		0x0008	// A dynamic light.
#define RPF_DYNLIT		0x0010	// Normal list: poly is dynamically lit.
#define RPF_GLOW		0x0020	// Multiply original vtx colors.
#define RPF_DETAIL		0x0040	// Render with detail (incl. vtx distances)
#define RPF_SHADOW		0x0100
#define RPF_HORIZONTAL	0x0200
#define RPF_DONE		0x8000	// This poly has already been drawn.

typedef enum {
	RP_NONE,
	RP_QUAD,					// Wall segment.
	RP_DIVQUAD,					// Divided wall segment.
	RP_FLAT						// Floor or ceiling.
} rendpolytype_t;

typedef struct {
	float pos[2];				// X and Y coordinates.
	gl_rgba_t color;			// Color of the vertex.
	float dist;					// Distance to the vertex.
} rendpoly_vertex_t;

// rendpoly_t is only for convenience; the data written in the rendering
// list data buffer is taken from this struct.
typedef struct {
	rendpolytype_t type;		
	short flags;				// RPF_*.
	float texoffx, texoffy;		// Texture coordinates for left/top (in real texcoords).
	gltexture_t tex;
	gltexture_t intertex;
	float interpos;				// Blending strength (0..1).
	struct dynlight_s *lights;	// List of lights that affect this poly.
	DGLuint decorlightmap;		// Pregen RGB lightmap for decor lights.
	sector_t *sector;			// The sector this poly belongs to (if any).

	// The geometry:
	float top, bottom;			
	float length;
	byte numvertices;			// Number of vertices for the poly.
	rendpoly_vertex_t vertices[RL_MAX_POLY_SIDES];
	struct div_t {
		byte num;
		float pos[RL_MAX_DIVS];
	} divs[2];					// For wall segments (two vertices).
} rendpoly_t;

// This is the dummy mobj_t used for blockring roots.
// It has some excess information since it has to be compatible with 
// regular mobjs (otherwise the rings don't really work).
// Note: the thinker and x,y,z data could be used for something else...
typedef struct linkmobj_s {
	thinker_t		thinker;
	fixed_t			x,y,z;	
	struct mobj_s	*next, *prev;
} linkmobj_t;

typedef struct {
	float		visfloor, visceil;		// Visible floor and ceiling, floats.
	sector_t	*linkedfloor;			// Floor attached to another sector.
	sector_t	*linkedceil;			// Ceiling attached to another sector.
	boolean		permanentlink;
	float		bounds[4];				// Bounding box for the sector.
	int			flags;
	int			addspritecount;			// frame number of last R_AddSprites
	sector_t	*lightsource;			// Main sky light source
} sectorinfo_t;

typedef struct planeinfo_s {
	short		flags;
	ushort		numvertices;
	fvertex_t	*vertices;
	int			pic;
	boolean		isfloor;
} planeinfo_t;

// Shadowpoly flags.
#define SHPF_FRONTSIDE	0x1

typedef struct shadowpoly_s {
	struct line_s *line;
	short flags;
	ushort visframe;				// Last visible frame (for rendering).
	vertex_t *outer[2];				// Left and right.
	float inoffset[2][2];			// Offset from 'outer.'
	float extoffset[2][2];			// Extended: offset from 'outer.'
	float bextoffset[2][2];			// Back-extended: offset frmo 'outer.'
} shadowpoly_t;

typedef struct shadowlink_s {
	struct shadowlink_s *next;
	shadowpoly_t *poly;
} shadowlink_t;

typedef struct subsectorinfo_s {
	planeinfo_t	floor, ceil;
	int			validcount;
	shadowlink_t *shadows;
} subsectorinfo_t;

typedef struct lineinfo_side_s {
	struct line_s *neighbor[2];		// Left and right neighbour.
	struct sector_s *proxsector[2];	// Sectors behind the neighbors.
	struct line_s *backneighbor[2];	// Neighbour in the backsector (if any).
	struct line_s *alignneighbor[2];// Aligned left and right neighbours.
} lineinfo_side_t;

typedef struct {
	float length;					// Accurate length.
	binangle_t angle;				// Calculated from front side's normal.
	lineinfo_side_t side[2];		// 0 = front, 1 = back
} lineinfo_t;

typedef struct polyblock_s {
	polyobj_t *polyobj;
	struct polyblock_s *prev;
	struct polyblock_s *next;
} polyblock_t;

typedef struct vertexowner_s {
	unsigned short	num;			// Number of owners.
	unsigned short	*list;			// Sector indices.
} vertexowner_t;

// The sector divisions list is similar to vertexowners.
typedef struct vertexowner_s sector_divisions_t;

typedef struct {
	int		originx;	// block origin (allways UL), which has allready
	int		originy;	// accounted  for the patch's internal origin
	int		patch;
} texpatch_t;

// a maptexturedef_t describes a rectangular texture, which is composed of one
// or more mappatch_t structures that arrange graphic patches
typedef struct {
	char		name[9];		// for switch changing, etc; ends in \0
	short		width;
	short		height;
	int			flags;			// TXF_* flags.
	short		patchcount;
	DGLuint		tex;			// Name of the associated DGL texture.
	byte		masked;			// Is the (DGL) texture masked?
	detailinfo_t detail;		// Detail texture information.
	byte		ingroup;		// True if texture belongs to some animgroup.
	struct ded_decor_s *decoration;	// Pointer to the surface decoration, if any.
	texpatch_t	patches[1];		// [patchcount] drawn back to front
} texture_t;					//   into the cached texture

typedef struct {
	byte		rgb[3];
} rgbcol_t;

typedef struct translation_s {
	int current;
	int next;
	float inter;
} translation_t;

typedef struct flat_s {
	struct flat_s *next;
	int			lump;
	translation_t translation;
	short		flags;
	rgbcol_t	color;
	detailinfo_t detail;		// Detail texture information.
	byte		ingroup;		// True if belongs to some animgroup.
	struct ded_decor_s *decoration;	// Pointer to the surface decoration, if any.
} flat_t;

typedef struct lumptexinfo_s {
	DGLuint		tex[2];		// Names of the textures (two parts for big ones)
	unsigned short width[2], height;
	short		offx, offy;
} lumptexinfo_t;

typedef struct animframe_s {
	int number;
	ushort tics;
	ushort random;
} animframe_t;

typedef struct animgroup_s {
	int id;
	int flags;
	int index;
	int maxtimer;
	int timer;
	int count;
	animframe_t *frames;	
} animgroup_t;

extern vertexowner_t *vertexowners;
extern sectorinfo_t *secinfo;
extern subsectorinfo_t *subsecinfo;
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
extern  translation_t	*texturetranslation;	// for global animation
extern	int				numgroups;
extern	animgroup_t		*groups;
extern	int				LevelFullBright;
extern	int				r_texglow;
extern	int				r_precache_sprites, r_precache_skins;

void    R_InitData(void);
void	R_UpdateData(void);
void	R_ShutdownData(void);
void	R_PrecacheLevel(void);
void	R_InitAnimGroup(ded_group_t *def);
void	R_ResetAnimGroups(void);
boolean	R_IsInAnimGroup(int groupNum, int type, int number);
void	R_AnimateAnimGroups(void);
int		R_TextureFlags(int texture);
int		R_FlatFlags(int flat);
flat_t* R_FindFlat(int lumpnum);	// May return NULL.
flat_t* R_GetFlat(int lumpnum);		// Creates new entries.
flat_t** R_CollectFlats(int *count);
int		R_FlatNumForName (char *name);
int		R_CheckTextureNumForName (char *name);
int		R_TextureNumForName (char *name);
char*	R_TextureNameForNum(int num);
int		R_SetFlatTranslation(int flat, int translateTo);
int		R_SetTextureTranslation(int tex, int translateTo);
void	R_SetAnimGroup(int type, int number, int group);
boolean	R_IsCustomTexture(int texture);
boolean	R_IsAllowedDecoration(ded_decor_t *def, int index, boolean hasExternal);
boolean	R_IsValidLightDecoration(ded_decorlight_t *lightDef);
void	R_GenerateDecorMap(ded_decor_t *def);

#endif
