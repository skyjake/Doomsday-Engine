//===========================================================================
// R_THINGS.H
//===========================================================================
#ifndef __DOOMSDAY_REFRESH_THINGS_H__
#define __DOOMSDAY_REFRESH_THINGS_H__

#include "p_data.h"
#include "r_data.h"

#define MAXVISSPRITES   8192 

// A vissprite_t is a thing or masked wall that will be drawn during 
// a refresh.
typedef struct vissprite_s
{
	struct vissprite_s *prev, *next;
	boolean			issprite;			// True if this is a sprite (otherwise a wall segment).
	float			distance;			// Vissprites are sorted by distance.

	// An anonymous union for the data.
	union vissprite_data_u {
		struct vissprite_mobj_s {
			int		patch;
			fixed_t	gx, gy;             // for line side calculation
			fixed_t	gz, gzt;            // global bottom / top for silhouette clipping
			boolean	flip;				// Flip texture?
			float	v1[2], v2[2];		// The vertices (v1 is the left one).
			int		flags;				// for color translation and shadow draw
			uint	id;
			int		selector;
			int		class;				// player class (used in translation)
			fixed_t floorclip;               
			boolean	viewaligned;		// Align to view plane.
			float	secfloor, secceil;
			boolean	hasglow;			
			byte	floorglow[3];		// Floor glow color.
			byte	ceilglow[3];		// Ceiling glow color.
			byte	rgb[3];				// Sector light color.
			int		lightlevel;			
			float	alpha;
			float	visoff[3];			// Last-minute offset to coords.
			struct modeldef_s *mf, *nextmf;
			float	yaw, pitch;			// For models.
			float	inter;				// Frame interpolation, 0..1
			struct lumobj_s *light;		// For the halo (NULL if no halo).
		} mo;
		struct vissprite_wall_s {
			int		texture;
			boolean	masked;
			float	top;				// Top and bottom height.
			float	bottom;
			struct vissprite_wall_vertex_s {
				float pos[2];			// x and y coordinates.
				int color;
			} vertices[2];
			float texc[2][2];			// u and v coordinates.
		} wall;
	};
} vissprite_t;


// Sprites are patches with a special naming convention so they can be
// recognized by R_InitSprites.  The sprite and frame specified by a
// thing_t is range checked at run time.

// a sprite is a patch_t that is assumed to represent a three dimensional
// object and may have multiple rotations pre drawn.  Horizontal flipping
// is used to save space. Some sprites will only have one picture used
// for all views.

typedef struct
{
	boolean         rotate;         // if false use 0 for any position
	int				lump[8];        // sprite lump to use for view angles 0-7
	byte            flip[8];        // flip (1 = flip) to use for view angles 0-7
} spriteframe_t;

typedef struct
{
	int             numframes;
	spriteframe_t   *spriteframes;
} spritedef_t;

typedef struct
{
	int				lump;			// Real lump number.
	short			width;
	short			height;
	short			offset;
	short			topoffset;
	float			flarex;			// Offset to flare.
	float			flarey;
	float			lumsize;
	float			tc[2];			// Texture coordinates.
	DGLuint			tex;			// Name of the associated DGL texture.
	DGLuint			hudtex;
	short			hudwidth;
	short			hudheight;
	rgbcol_t		color;			// Average color, for lighting.
} spritelump_t;

extern  spritedef_t		*sprites;
extern  int             numsprites;
extern	spritelump_t	*spritelumps;
extern	int				numspritelumps;
extern	int				pspOffX, pspOffY;
extern	int				alwaysAlign;
extern	float			weaponOffsetScale, weaponFOVShift;
extern	int				r_maxmodelz, r_nospritez;
extern	int				r_use_srvo, r_use_srvo_angle;
extern  vissprite_t     vissprites[MAXVISSPRITES], *vissprite_p;
extern	vissprite_t		vispsprites[DDMAXPSPRITES];
extern  vissprite_t     vsprsortedhead;

void	R_GetSpriteInfo(int sprite, int frame, spriteinfo_t *sprinfo);
int		R_VisualRadius(mobj_t *mo);
void	R_ProjectSprite(mobj_t *thing);
void	R_ProjectPlayerSprites(void);
void	R_ProjectDecoration(mobj_t *source);
void    R_SortVisSprites(void);
void    R_AddSprites(sector_t *sec);
void    R_AddPSprites(void);
void    R_DrawSprites(void);
void    R_InitSprites(void);
void    R_ClearSprites(void);

void    R_ClipVisSprite(vissprite_t *vis, int xl, int xh);

#endif 