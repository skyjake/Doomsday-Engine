// R_local.h

#ifndef __R_LOCAL__
#define __R_LOCAL__

#include "p_xg.h"

#define	ANGLETOSKYSHIFT		22		// sky map is 256*128*4 maps

#define	BASEYCENTER			100

#define MAXWIDTH			1120
#define	MAXHEIGHT			832

#define	PI					3.141592657

#define	CENTERY				(SCREENHEIGHT/2)

#define	MINZ			(FRACUNIT*4)

#define	FIELDOFVIEW		2048	// fineangles in the SCREENWIDTH wide window

//
// lighting constants
//
#define	LIGHTLEVELS			16
#define	LIGHTSEGSHIFT		4
#define	MAXLIGHTSCALE		48
#define	LIGHTSCALESHIFT		12
#define	MAXLIGHTZ			128
#define	LIGHTZSHIFT			20
#define	NUMCOLORMAPS		32		// number of diminishing
#define	INVERSECOLORMAP		32

/*
==============================================================================

					INTERNAL MAP TYPES

==============================================================================
*/

//================ used by play and refresh

struct line_s;

typedef	struct sector_s
{
	fixed_t		floorheight, ceilingheight;
	short		floorpic, ceilingpic;
	short		lightlevel;
	byte		rgb[3];
	int			validcount;			// if == validcount, already checked
	mobj_t		*thinglist;			// list of mobjs in sector
	int			linecount;
	struct line_s **Lines;			// [linecount] size
//	int			flatoffx, flatoffy;		// Scrolling flats.
	float 		flooroffx, flooroffy; 	// floor texture offset
	float		ceiloffx, ceiloffy;		// ceiling texture offset
	int			skyfix;					// Offset to ceiling height rendering w/sky.
	float		reverb[NUM_REVERB_DATA];
	int			blockbox[4];		// mapblock bounding box for height changes
	plane_t		planes[2];				// PLN_*
	degenmobj_t	soundorg;			// for any sounds played by the sector	

	// --- You can freely make changes after this.

	short		special, tag;
	int			soundtraversed;		// 0 = untraversed, 1,2 = sndlines -1
	mobj_t		*soundtarget;		// thing that made a sound (or null)
	void		*specialdata;		// thinker_t for reversable actions
	int			origfloor, origceiling, origlight;
	byte		origrgb[3];
	xgsector_t	*xg;
} sector_t;

typedef struct side_s
{
	fixed_t		textureoffset;		// add this to the calculated texture col
	fixed_t		rowoffset;			// add this to the calculated texture top
	short		toptexture, bottomtexture, midtexture;
	sector_t	*sector;

	// --- You can freely make changes after this.	

} side_t;

typedef struct line_s
{
	vertex_t	*v1, *v2;
	short		flags;
	sector_t	*frontsector, *backsector;
	fixed_t		dx,dy;				// v2 - v1 for side checking
	slopetype_t	slopetype;			// to aid move clipping
	int			validcount;			// if == validcount, already checked
	short		sidenum[2];
	fixed_t		bbox[4];

	// --- You can freely make changes after this.	

	short		special, tag;
	void		*specialdata;		// thinker_t for reversable actions
	xgline_t	*xg;				// Extended generalized lines.
} line_t;

enum // Subsector reverb data indices.
{
	SSRD_VOLUME,
	SSRD_SPACE,
	SSRD_DECAY,
	SSRD_DAMPING
};

typedef struct
{
	fixed_t		x,y,dx,dy;			// partition line
	fixed_t		bbox[2][4];			// bounding box for each child
	unsigned short	children[2];		// if NF_SUBSECTOR its a subsector

	// --- You can freely make changes after this.	

} node_t;


/*
==============================================================================

						OTHER TYPES

==============================================================================
*/

//=============================================================================

#define numvertexes		(*gi.numvertexes)
#define numsegs			(*gi.numsegs)
#define	numsectors		(*gi.numsectors)
#define numsubsectors	(*gi.numsubsectors)
#define numnodes		(*gi.numnodes)
#define numlines		(*gi.numlines)
#define numsides		(*gi.numsides)

#define vertexes		((vertex_t*)(*gi.vertexes))
#define segs			((seg_t*)(*gi.segs))
#define	sectors			((sector_t*)(*gi.sectors))
#define subsectors		((subsector_t*)(*gi.subsectors))
#define nodes			((node_t*)(*gi.nodes))
#define lines			((line_t*)(*gi.lines))
#define sides			((side_t*)(*gi.sides))


//extern	fixed_t		viewx, viewy, viewz;
//extern	angle_t		viewangle;
#define viewangle	Get(DD_VIEWANGLE)
extern	player_t	*viewplayer;


extern	angle_t		clipangle;

extern	int			viewangletox[FINEANGLES/2];
extern	angle_t		xtoviewangle[SCREENWIDTH+1];
extern	fixed_t		finetangent[FINEANGLES/2];

extern	fixed_t		rw_distance;
extern	angle_t		rw_normalangle;

//
// R_main.c
//
//extern	int				viewwidth, viewheight, viewwindowx, viewwindowy;
extern	int				centerx, centery;
extern	int				flyheight;
extern	fixed_t			centerxfrac;
extern	fixed_t			centeryfrac;
extern	fixed_t			projection;

//extern	int				validcount;
#define Validcount		(*gi.validcount)

extern	int				sscount, linecount, loopcount;

extern	int				extralight;

extern	fixed_t			viewcos, viewsin;

extern	int				detailshift;		// 0 = high, 1 = low


/*int		R_PointOnSide (fixed_t x, fixed_t y, node_t *node);
int		R_PointOnSegSide (fixed_t x, fixed_t y, seg_t *line);*/
angle_t R_PointToAngle (fixed_t x, fixed_t y);
//#define R_PointToAngle2			gi.R_PointToAngle2
//fixed_t	R_PointToDist (fixed_t x, fixed_t y);
fixed_t R_ScaleFromGlobalAngle (angle_t visangle);
//#define R_PointInSubsector		gi.R_PointInSubsector
void R_AddPointToBox (int x, int y, fixed_t *box);

#define skyflatnum		Get(DD_SKYFLATNUM)

#endif		// __R_LOCAL__


