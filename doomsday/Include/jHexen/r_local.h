
//**************************************************************************
//**
//** r_local.h : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile$
//** $Revision$
//** $Date$
//** $Author$
//**
//**************************************************************************

#ifndef __R_LOCAL__
#define __R_LOCAL__

//#pragma pack(1)

#define ANGLETOSKYSHIFT         22              // sky map is 256*128*4 maps

#define BASEYCENTER                     100

#define MAXWIDTH                        1120
#define MAXHEIGHT                       832

#define PI                                      3.141592657

#define CENTERY                         (SCREENHEIGHT/2)

#define MINZ                    (FRACUNIT*4)

#define FIELDOFVIEW             2048    // fineangles in the SCREENWIDTH wide window

//
// lighting constants
//
#define LIGHTLEVELS                     16
#define LIGHTSEGSHIFT           4
#define MAXLIGHTSCALE           48
#define LIGHTSCALESHIFT         12
#define MAXLIGHTZ                       128
#define LIGHTZSHIFT                     20
#define NUMCOLORMAPS            32              // number of diminishing
#define INVERSECOLORMAP         32

/*
==============================================================================

					INTERNAL MAP TYPES

==============================================================================
*/

//================ used by play and refresh

/*typedef struct
{
	fixed_t         x,y;

	// --- You can freely make changes after this.	

} vertex_t;*/

struct line_s;

typedef struct sector_s
{
	fixed_t	floorheight, ceilingheight;
	short	floorpic, ceilingpic;
	short   lightlevel;
	byte	rgb[3];
	int     validcount;             // if == validcount, already checked
	mobj_t  *thinglist;             // list of mobjs in sector
	int     linecount;
	struct line_s **Lines;			// [linecount] size
	float	flatoffx, flatoffy;		// Scrolling flats.
	float	ceiloffx, ceiloffy;		// Scrolling ceilings.
	int		skyfix;					// Offset to ceiling height rendering w/sky.
	float	reverb[NUM_REVERB_DATA];
	int 	blockbox[4];			// mapblock bounding box for height changes
	plane_t	planes[2];				// PLN_*
	degenmobj_t soundorg;           // for any sounds played by the sector

	// --- You can freely make changes after this.

	short  	special, tag;
	int     soundtraversed;         // 0 = untraversed, 1,2 = sndlines -1
	mobj_t  *soundtarget;           // thing that made a sound (or null)
	seqtype_t seqType;				// stone, metal, heavy, etc...
	void    *specialdata;           // thinker_t for reversable actions
} sector_t;

typedef struct side_s
{
	fixed_t         textureoffset;          // add this to the calculated texture col
	fixed_t         rowoffset;                      // add this to the calculated texture top
	short           toptexture, bottomtexture, midtexture;
	sector_t        *sector;

	// --- You can freely make changes after this.	

} side_t;

typedef struct line_s
{
	vertex_t *v1;
	vertex_t *v2;
	short flags;
	sector_t *frontsector;
	sector_t *backsector;
	fixed_t dx;
	fixed_t dy;
	slopetype_t slopetype;
	int validcount;
	short sidenum[2];
	fixed_t bbox[4];

	// --- You can freely make changes after this.	

	byte special;
	byte arg1;
	byte arg2;
	byte arg3;
	byte arg4;
	byte arg5;
	void *specialdata;
} line_t;

/*typedef struct
{
	vertex_t        *v1, *v2;
	float			length;			// Length of the segment (v1 -> v2).
	fixed_t         offset;
	side_t          *sidedef;
	line_t          *linedef;
	sector_t        *frontsector;
	sector_t        *backsector;            // NULL for one sided lines
	byte			flags;
	angle_t         angle;

	// --- You can freely make changes after this.	
	
} seg_t;*/

// ===== Polyobj data =====
typedef struct polyobj_s
{
	int numSegs;
	seg_t **Segs;
	int validcount;
	degenmobj_t startSpot;
	angle_t angle;
	vertex_t *originalPts; 	// used as the base for the rotations
	vertex_t *prevPts; 		// use to restore the old point values
	int tag;				// reference tag assigned in HereticEd
	int bbox[4];
	vertex_t dest;
	int speed;				// Destination XY and speed.
	angle_t destAngle, angleSpeed;	// Destination angle and rotation speed.

	// --- You can freely make changes after this.	
	
	boolean crush; 			// should the polyobj attempt to crush mobjs?
	int seqType;
	fixed_t size; // polyobj size (area of POLY_AREAUNIT == size of FRACUNIT)
	void *specialdata; // pointer a thinker, if the poly is moving
} polyobj_t;

typedef struct polyblock_s
{
	polyobj_t *polyobj;
	struct polyblock_s *prev;
	struct polyblock_s *next;
	// Don't change this; engine uses a similar struct.
} polyblock_t;

/*typedef struct subsector_s
{
	sector_t        *sector;
	unsigned short	numLines;
	unsigned short	firstline;
	polyobj_t		*poly;
	// Sorted edge vertices for rendering floors and ceilings.
	char			numverts;
	fvertex_t		*verts;			// A list of edge vertices.
	fvertex_t		bbox[2];		// Min and max points.
	fvertex_t		midpoint;		// Center of vertices.
	byte			flags;

	// --- You can freely make changes after this.	

} subsector_t;*/

typedef struct
{
	fixed_t         x,y,dx,dy;                      // partition line
	fixed_t         bbox[2][4];                     // bounding box for each child
	unsigned short  children[2];            // if NF_SUBSECTOR its a subsector

	// --- You can freely make changes after this.	

} node_t;

/*
==============================================================================

						OTHER TYPES

==============================================================================
*/

typedef byte    lighttable_t;           // this could be wider for >8 bit display

/*#define MAXVISPLANES    160
#define MAXOPENINGS             SCREENWIDTH*64

typedef struct
{
	fixed_t         height;
	int                     picnum;
	int                     lightlevel;
	int                     special;
	int                     minx, maxx;
	byte            pad1;                                           // leave pads for [minx-1]/[maxx+1]
	byte            top[SCREENWIDTH];
	byte            pad2;
	byte            pad3;
	byte            bottom[SCREENWIDTH];
	byte            pad4;
} visplane_t;
*/
/*typedef struct drawseg_s
{
	seg_t           *curline;
	int                     x1, x2;
	fixed_t         scale1, scale2, scalestep;
	int                     silhouette;                     // 0=none, 1=bottom, 2=top, 3=both
	fixed_t         bsilheight;                     // don't clip sprites above this
	fixed_t         tsilheight;                     // don't clip sprites below this
// pointers to lists for sprite clipping
	short           *sprtopclip;            // adjusted so [x1] is first value
	short           *sprbottomclip;         // adjusted so [x1] is first value
	short           *maskedtexturecol;      // adjusted so [x1] is first value
} drawseg_t;
*/
#define SIL_NONE        0
#define SIL_BOTTOM      1
#define SIL_TOP         2
#define SIL_BOTH        3

//#define MAXDRAWSEGS             256
/*
// A vissprite_t is a thing that will be drawn during a refresh
typedef struct vissprite_s
{
	struct vissprite_s *prev, *next;
	int             x1, x2;
	fixed_t         gx, gy;                 // for line side calculation
	fixed_t         gz, gzt;                // global bottom / top for silhouette clipping
	fixed_t         startfrac;              // horizontal position of x1
	fixed_t         scale;
	fixed_t         xiscale;                // negative if flipped
	fixed_t         texturemid;
	int             patch;
//	lighttable_t    *colormap;
	int				lightlevel;
	float			v1[2], v2[2];		// The vertices (v1 is the left one).
	float			secfloor, secceil;
	int             mobjflags;        // for color translation and shadow draw
	boolean         psprite;                // true if psprite
	int				class;			// player class (used in translation)
	fixed_t         floorclip;               
	
	boolean			viewAligned;		// Align to view plane.
} vissprite_t;
*/


//=============================================================================

// Map data is in the main engine, so these are helpers...
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

extern  angle_t         clipangle;

extern  int				viewangletox[FINEANGLES/2];
extern  angle_t         xtoviewangle[SCREENWIDTH+1];
extern  fixed_t         finetangent[FINEANGLES/2];

extern  fixed_t         rw_distance;
extern  angle_t         rw_normalangle;

//
// R_main.c
//
extern  int             centerx, centery;
extern  int             flyheight;

#define Validcount		(*gi.validcount)

extern  int             sscount, linecount, loopcount;
extern  lighttable_t    *scalelight[LIGHTLEVELS][MAXLIGHTSCALE];
extern  lighttable_t    *scalelightfixed[MAXLIGHTSCALE];
extern  lighttable_t    *zlight[LIGHTLEVELS][MAXLIGHTZ];

extern  int				extralight;

extern  fixed_t			viewcos, viewsin;

extern  int             detailshift;            // 0 = high, 1 = low

extern  void            (*colfunc) (void);
extern  void            (*basecolfunc) (void);
extern  void            (*fuzzcolfunc) (void);
extern  void            (*spanfunc) (void);

//
// R_bsp.c
//
extern  seg_t           *curline;
extern  side_t  *sidedef;
extern  line_t  *linedef;
extern  sector_t        *frontsector, *backsector;

extern  int     rw_x;
extern  int     rw_stopx;

extern  boolean         segtextured;
extern  boolean         markfloor;              // false if the back side is the same plane
extern  boolean         markceiling;
extern  boolean         skymap;

//extern  drawseg_t       drawsegs[MAXDRAWSEGS], *ds_p;

extern  lighttable_t    **hscalelight, **vscalelight, **dscalelight;

typedef void (*drawfunc_t) (int start, int stop);
void R_ClearClipSegs (void);

void R_ClearDrawSegs (void);
//void R_InitSkyMap (void);
void R_RenderBSPNode (int bspnum);

//
// R_segs.c
//
extern  int                     rw_angle1;              // angle to line origin
extern int TransTextureStart;
extern int TransTextureEnd;

//void R_RenderMaskedSegRange (drawseg_t *ds, int x1, int x2);

//
// R_plane.c
//
//typedef void (*planefunction_t) (int top, int bottom);
//extern  planefunction_t         floorfunc, ceilingfunc;

//extern  int                     skyflatnum;
//#define skyflatnum (*gi.skyflatnum)
#define skyflatnum		Get(DD_SKYFLATNUM)

//extern  short                   openings[MAXOPENINGS], *lastopening;

extern  short           floorclip[SCREENWIDTH];
extern  short           ceilingclip[SCREENWIDTH];

extern  fixed_t         yslope[SCREENHEIGHT];
extern  fixed_t         distscale[SCREENWIDTH];

void R_InitPlanes (void);
void R_ClearPlanes (void);
void R_MapPlane (int y, int x1, int x2);
void R_MakeSpans (int x, int t1, int b1, int t2, int b2);
void R_DrawPlanes (void);

/*visplane_t *R_FindPlane (fixed_t height, int picnum, int lightlevel,
	int special);
visplane_t *R_CheckPlane (visplane_t *pl, int start, int stop);*/


//
// R_debug.m
//
extern  int     drawbsp;

void RD_OpenMapWindow (void);
void RD_ClearMapWindow (void);
void RD_DisplayLine (int x1, int y1, int x2, int y2, float gray);
void RD_DrawNodeLine (node_t *node);
void RD_DrawLineCheck (seg_t *line);
void RD_DrawLine (seg_t *line);
void RD_DrawBBox (fixed_t *bbox);


//
// R_data.c
//
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
	char		name[8];		// for switch changing, etc
	short		width;
	short		height;
	short		patchcount;
	texpatch_t	patches[1];		// [patchcount] drawn back to front
								//  into the cached texture
	// Extra stuff. -jk
	boolean		masked;			// from maptexture_t
} texture_t;

extern  fixed_t         *textureheight;         // needed for texture pegging
extern  fixed_t         *spritewidth;           // needed for pre rendering (fracs)
extern  fixed_t         *spriteoffset;
extern  fixed_t         *spritetopoffset;
extern  int             viewwidth, scaledviewwidth, viewheight;
#define firstflat		gi.Get(DD_FIRSTFLAT)
#define numflats		gi.Get(DD_NUMFLATS)

extern  int             firstspritelump, lastspritelump, numspritelumps;
//extern boolean LevelUseFullBright;

byte    *R_GetColumn (int tex, int col);
void    R_InitData (void);
void	R_UpdateData (void);
//void R_PrecacheLevel (void);


//
// R_things.c
//
#define MAXVISSPRITES   1024 //192

//extern  vissprite_t     vissprites[MAXVISSPRITES], *vissprite_p;
//extern  vissprite_t     vsprsortedhead;

// constant arrays used for psprite clipping and initializing clipping
extern  short   negonearray[SCREENWIDTH];
extern  short   screenheightarray[SCREENWIDTH];

// vars for R_DrawMaskedColumn
extern  short           *mfloorclip;
extern  short           *mceilingclip;
extern  fixed_t         spryscale;
extern  fixed_t         sprtopscreen;
extern  fixed_t         sprbotscreen;

extern  fixed_t         pspritescale, pspriteiscale;


void R_DrawMaskedColumn (column_t *column, signed int baseclip);


void    R_SortVisSprites (void);

void    R_AddSprites (sector_t *sec);
void    R_AddPSprites (void);
void    R_DrawSprites (void);
//void    R_InitSprites (char **namelist);
void    R_ClearSprites (void);
void    R_DrawMasked (void);
//void    R_ClipVisSprite (vissprite_t *vis, int xl, int xh);

//=============================================================================
//
// R_draw.c
//
//=============================================================================

extern  byte    *translationtables;
extern  byte    *dc_translation;

void    R_InitBuffer (int width, int height);
void    R_InitTranslationTables (void);
void	R_UpdateTranslationTables (void);


//#pragma pack()

#endif          // __R_LOCAL__


