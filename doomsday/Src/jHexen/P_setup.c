
//**************************************************************************
//**
//** p_setup.c : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile$
//** $Revision$
//** $Date$
//** $Author$
//**
//**************************************************************************

#ifdef WIN32
#pragma optimize("g", off)
#endif

// HEADER FILES ------------------------------------------------------------

#include <math.h>
#include <stdlib.h>
#include "h2def.h"
#include "p_local.h"
#include "soundst.h"
#include "r_common.h"

// MACROS ------------------------------------------------------------------

#define MAPINFO_SCRIPT_NAME "MAPINFO"
#define MCMD_SKY1 1
#define MCMD_SKY2 2
#define MCMD_LIGHTNING 3
#define MCMD_FADETABLE 4
#define MCMD_DOUBLESKY 5
#define MCMD_CLUSTER 6
#define MCMD_WARPTRANS 7
#define MCMD_NEXT 8
#define MCMD_CDTRACK 9
#define MCMD_CD_STARTTRACK 10
#define MCMD_CD_END1TRACK 11
#define MCMD_CD_END2TRACK 12
#define MCMD_CD_END3TRACK 13
#define MCMD_CD_INTERTRACK 14
#define MCMD_CD_TITLETRACK 15

#define UNKNOWN_MAP_NAME "DEVELOPMENT MAP"
#define DEFAULT_SKY_NAME "SKY1"
#define DEFAULT_SONG_LUMP "DEFSONG"
#define DEFAULT_FADE_TABLE "COLORMAP"

// TYPES -------------------------------------------------------------------

enum
{
	BLEFT, BTOP, BRIGHT, BBOTTOM
};

typedef struct mapInfo_s mapInfo_t;
struct mapInfo_s
{
	short cluster;
	short warpTrans;
	short nextMap;
	short cdTrack;
	char name[32];
	short sky1Texture;
	short sky2Texture;
	fixed_t sky1ScrollDelta;
	fixed_t sky2ScrollDelta;
	boolean doubleSky;
	boolean lightning;
	int fadetable;
	char songLump[10];
};

typedef struct
{
	unsigned short	v1, v2;
	short			linedef, side;
	unsigned short	partner;
} glseg_t;

typedef struct
{
	int				x, y;
} glvert2_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void P_SpawnMapThing(mapthing_t *mthing);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static int QualifyMap(int map);

float AccurateDistance(fixed_t dx,fixed_t dy);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int actual_leveltime;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int MapCount;
mapthing_t deathmatchstarts[MAXDEATHMATCHSTARTS], *deathmatch_p;
int firstGLvertex = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static mapInfo_t MapInfo[99];
static char *MapCmdNames[] =
{
	"SKY1",
	"SKY2",
	"DOUBLESKY",
	"LIGHTNING",
	"FADETABLE",
	"CLUSTER",
	"WARPTRANS",
	"NEXT",
	"CDTRACK",
	"CD_START_TRACK",
	"CD_END1_TRACK",
	"CD_END2_TRACK",
	"CD_END3_TRACK",
	"CD_INTERMISSION_TRACK",
	"CD_TITLE_TRACK",
	NULL
};
static int MapCmdIDs[] =
{
	MCMD_SKY1,
	MCMD_SKY2,
	MCMD_DOUBLESKY,
	MCMD_LIGHTNING,
	MCMD_FADETABLE,
	MCMD_CLUSTER,
	MCMD_WARPTRANS,
	MCMD_NEXT,
	MCMD_CDTRACK,
	MCMD_CD_STARTTRACK,
	MCMD_CD_END1TRACK,
	MCMD_CD_END2TRACK,
	MCMD_CD_END3TRACK,
	MCMD_CD_INTERTRACK,
	MCMD_CD_TITLETRACK
};

static int cd_NonLevelTracks[6]; // Non-level specific song cd track numbers 

static char *cd_SongDefIDs[] =	// Music defs that correspond the above.
{
	"startup",
	"hall",
	"orb",
	"chess",
	"hub",
	"hexen"
};

// CODE --------------------------------------------------------------------

/*
=================
=
= P_LoadVertexes
=
=================
*/

void P_LoadVertexes (int lump, int gllump)
{
	byte            *data, *glverts = 0;
	int				i, orignum, ver;
	mapvertex_t     *ml;
	glvert2_t		*glv;
	vertex_t        *li;

	orignum = numvertexes = W_LumpLength(lump) / sizeof(mapvertex_t);
	// glBSP lump given?
	if(gllump >= 0)
	{
		glverts = W_CacheLumpNum(gllump, PU_STATIC);
		// Which version?
		ver = 1;
		if(!strncmp(glverts, "gNd2", 4))
		{
			Con_Message("GL_VERT v2.0\n");
			ver = 2;
		}
		// There are additional vertices in gllump.
		numvertexes += (W_LumpLength(gllump) - (ver==2? 4 : 0)) / 
			(ver==1? sizeof(mapvertex_t) : sizeof(glvert2_t));
	}
	vertexes = Z_Malloc (numvertexes*sizeof(vertex_t),PU_LEVEL,0);
	data = W_CacheLumpNum (lump,PU_STATIC);
	ml = (mapvertex_t *)data;
	li = vertexes;
	for(i=0 ; i<orignum; i++, li++, ml++)
	{
		li->x = SHORT(ml->x)<<FRACBITS;
		li->y = SHORT(ml->y)<<FRACBITS;
	}
	Z_Free(data);

	firstGLvertex = orignum;
	// Also load the GL vertices.
	if(glverts)
	{
		ml = (mapvertex_t*) glverts;
		glv = (glvert2_t*) (glverts + 4);
		for(i=orignum; i<numvertexes; i++, li++, ml++, glv++)
		{
			if(ver == 1)
			{
				li->x = SHORT(ml->x)<<FRACBITS;
				li->y = SHORT(ml->y)<<FRACBITS;
			}
			else
			{
				li->x = glv->x;
				li->y = glv->y;
			}
		}
		Z_Free(glverts);
	}
}

/*
=================
=
= P_LoadSegs
=
=================
*/

void P_LoadSegs (int lump)
{
	byte            *data;
	int             i;
	mapseg_t        *ml;
	seg_t           *li;
	line_t			*ldef;
	int             linedef, side;

	numsegs = W_LumpLength (lump) / sizeof(mapseg_t);
	segs = Z_Malloc (numsegs*sizeof(seg_t),PU_LEVEL,0);
	memset (segs, 0, numsegs*sizeof(seg_t));
	data = W_CacheLumpNum (lump,PU_STATIC);

	ml = (mapseg_t *)data;
	li = segs;
	for (i=0 ; i<numsegs ; i++, li++, ml++)
	{
		li->v1 = &vertexes[SHORT(ml->v1)];
		li->v2 = &vertexes[SHORT(ml->v2)];

		li->angle = (SHORT(ml->angle))<<16;
		li->offset = (SHORT(ml->offset))<<16;
		linedef = SHORT(ml->linedef);
		ldef = &lines[linedef];
		li->linedef = ldef;
		side = SHORT(ml->side);
		li->sidedef = &sides[ldef->sidenum[side]];
		li->frontsector = sides[ldef->sidenum[side]].sector;
		if (ldef-> flags & ML_TWOSIDED)
			li->backsector = sides[ldef->sidenum[side^1]].sector;
		else
			li->backsector = 0;
		
		// Calculate the length of the segment. We need this for
		// the texture coordinates. -jk
		li->length = AccurateDistance(li->v2->x - li->v1->x, li->v2->y - li->v1->y);								
	}

	Z_Free (data);
}

void P_LoadSegsGL(int lump)
{
	byte		*data;
	int			i;
	glseg_t		*gls;
	seg_t		*li;
	line_t		*ldef;

	numsegs = W_LumpLength(lump) / sizeof(glseg_t);
	segs = Z_Malloc(numsegs * sizeof(seg_t), PU_LEVEL, 0);
	memset(segs, 0, numsegs * sizeof(seg_t));	
	data = W_CacheLumpNum(lump, PU_STATIC);

	gls = (glseg_t*) data;
	li = segs;
	for(i=0; i<numsegs; i++, li++, gls++)
	{
		li->v1 = &vertexes[gls->v1 & 0x8000? firstGLvertex + (gls->v1 & ~0x8000) : gls->v1];
		li->v2 = &vertexes[gls->v2 & 0x8000? firstGLvertex + (gls->v2 & ~0x8000) : gls->v2];
		//Con_Message( "seg %i: linedef %i\n", i, gls->linedef);
		if(gls->linedef != -1)
		{
			ldef = &lines[gls->linedef];
			li->linedef = ldef;
			li->sidedef = &sides[ldef->sidenum[gls->side]];
			li->frontsector = sides[ldef->sidenum[gls->side]].sector;
			if (ldef->flags & ML_TWOSIDED)
				li->backsector = sides[ldef->sidenum[gls->side^1]].sector;
			else
				li->backsector = 0;
			if(gls->side == 0)
				li->offset = FRACUNIT * AccurateDistance(li->v1->x - ldef->v1->x, li->v1->y - ldef->v1->y); 
			else 
				li->offset = FRACUNIT * AccurateDistance(li->v1->x - ldef->v2->x, li->v1->y - ldef->v2->y); 
			li->angle = bamsAtan2((li->v2->y - li->v1->y)>>FRACBITS, 
				(li->v2->x - li->v1->x)>>FRACBITS) << 16;
		}
		else
		{
			li->linedef = NULL;
			li->sidedef = NULL;
			li->frontsector = NULL;
			li->backsector = NULL;
		}
		
		// Calculate the length of the segment. We need this for
		// the texture coordinates. -jk
		li->length = AccurateDistance(li->v2->x - li->v1->x, li->v2->y - li->v1->y);								
	}
	
	Z_Free(data);
}

//==========================================================================
//
// AccurateDistance
//
//==========================================================================

float AccurateDistance(fixed_t dx, fixed_t dy)
{
	float fx = FIX2FLT(dx), fy = FIX2FLT(dy);
	return (float)sqrt(fx*fx + fy*fy);
}



/*
=================
=
= P_LoadSubsectors
=
=================
*/

void P_LoadSubsectors (int lump)
{
	byte                    *data;
	int                     i;
	mapsubsector_t			*ms;
	subsector_t             *ss;

	numsubsectors = W_LumpLength (lump) / sizeof(mapsubsector_t);
	subsectors = Z_Malloc (numsubsectors*sizeof(subsector_t),PU_LEVEL,0);
	memset(subsectors, 0, numsubsectors*sizeof(subsector_t));
	data = W_CacheLumpNum (lump,PU_STATIC);

	ms = (mapsubsector_t *)data;
	memset (subsectors,0, numsubsectors*sizeof(subsector_t));
	ss = subsectors;
	for (i=0 ; i<numsubsectors ; i++, ss++, ms++)
	{
		ss->linecount = ms->numSegs;
		ss->firstline = ms->firstseg;
	}

	Z_Free (data);
}


/*
=================
=
= P_LoadSectors
=
=================
*/

void P_LoadSectors (int lump)
{
	byte                    *data;
	int                             i;
	mapsector_t             *ms;
	sector_t                *ss;

	numsectors = W_LumpLength (lump) / sizeof(mapsector_t);
	sectors = Z_Malloc (numsectors*sizeof(sector_t),PU_LEVEL,0);
	memset (sectors, 0, numsectors*sizeof(sector_t));
	data = W_CacheLumpNum (lump,PU_STATIC);

	ms = (mapsector_t *)data;
	ss = sectors;

	// Make sure primary lumps are used for flat searching
//	W_UsePrimary();

	for(i = 0; i < numsectors; i++, ss++, ms++)
	{
		ss->floorheight = SHORT(ms->floorheight)<<FRACBITS;
		ss->ceilingheight = SHORT(ms->ceilingheight)<<FRACBITS;
		ss->floorpic = R_FlatNumForName(ms->floorpic);
		ss->ceilingpic = R_FlatNumForName(ms->ceilingpic);
		ss->lightlevel = SHORT(ms->lightlevel);
		ss->special = SHORT(ms->special);
		ss->tag = SHORT(ms->tag);
		ss->thinglist = NULL;
		ss->seqType = SEQTYPE_STONE; // default seqType

		memset(ss->rgb, 0xff, 3);

//		ss->flatoffx = ss->flatoffy = 0;	// Flat scrolling. -jk
//		ss->skyfix = 0;		// Set if needed.
//		ss->reverbVolume = ss->reverbSpace = ss->reverbDecay = ss->reverbDamping = 0;
	}
/*	if(DevMaps)
	{
		W_UseAuxiliary();
	}*/
	Z_Free(data);
}


/*
=================
=
= P_LoadNodes
=
=================
*/

void P_LoadNodes (int lump)
{
	byte            *data;
	int                     i,j,k;
	mapnode_t       *mn;
	node_t          *no;

	numnodes = W_LumpLength (lump) / sizeof(mapnode_t);
	nodes = Z_Malloc (numnodes*sizeof(node_t),PU_LEVEL,0);
	data = W_CacheLumpNum (lump,PU_STATIC);

	mn = (mapnode_t *)data;
	no = nodes;
	for (i=0 ; i<numnodes ; i++, no++, mn++)
	{
		no->x = SHORT(mn->x)<<FRACBITS;
		no->y = SHORT(mn->y)<<FRACBITS;
		no->dx = SHORT(mn->dx)<<FRACBITS;
		no->dy = SHORT(mn->dy)<<FRACBITS;
		for (j=0 ; j<2 ; j++)
		{
			no->children[j] = SHORT(mn->children[j]);
			for (k=0 ; k<4 ; k++)
				no->bbox[j][k] = SHORT(mn->bbox[j][k])<<FRACBITS;
		}
	}
	Z_Free (data);
}

//==========================================================================
//
// P_LoadThings
//
//==========================================================================

void P_LoadThings(int lump)
{
	byte *data;
	int i;
	mapthing_t *mt;
	int numthings;
	int playerCount;
	int deathSpotsCount;

	data = W_CacheLumpNum(lump, PU_STATIC);
	numthings = W_LumpLength(lump)/sizeof(mapthing_t);

	mt = (mapthing_t *)data;
	for(i = 0; i < numthings; i++, mt++)
	{
		mt->tid = SHORT(mt->tid);
		mt->x = SHORT(mt->x);
		mt->y = SHORT(mt->y);
		mt->height = SHORT(mt->height);
		mt->angle = SHORT(mt->angle);
		mt->type = SHORT(mt->type);
		mt->options = SHORT(mt->options);
		P_SpawnMapThing(mt);
	}
	P_CreateTIDList();
	P_InitCreatureCorpseQueue(false); // false = do NOT scan for corpses
	Z_Free(data);

	if(!deathmatch)
	{ // Don't need to check deathmatch spots
		return;
	}
	playerCount = 0;
	for(i = 0; i < MAXPLAYERS; i++)
	{
		playerCount += players[i].plr->ingame;
	}
	deathSpotsCount = deathmatch_p-deathmatchstarts;
	if(deathSpotsCount < playerCount)
	{
		Con_Error("P_LoadThings: Player count (%d) exceeds deathmatch "
			"spots (%d)", playerCount, deathSpotsCount);
	}
}

/*
=================
=
= P_LoadLineDefs
=
=================
*/

void P_LoadLineDefs(int lump)
{
	byte *data;
	int i;
	maplinedef_t *mld;
	line_t *ld;
	vertex_t *v1, *v2;

	numlines = W_LumpLength(lump)/sizeof(maplinedef_t);
	lines = Z_Malloc(numlines*sizeof(line_t), PU_LEVEL, 0);
	memset(lines, 0, numlines*sizeof(line_t));
	data = W_CacheLumpNum(lump, PU_STATIC);

	mld = (maplinedef_t *)data;
	ld = lines;
	for(i = 0; i < numlines; i++, mld++, ld++)
	{
		ld->flags = SHORT(mld->flags);

		// Old line special info ...
		//ld->special = SHORT(mld->special);
		//ld->tag = SHORT(mld->tag);

		// New line special info ...
		ld->special = mld->special;
		ld->arg1 = mld->arg1;
		ld->arg2 = mld->arg2;
		ld->arg3 = mld->arg3;
		ld->arg4 = mld->arg4;
		ld->arg5 = mld->arg5;

		v1 = ld->v1 = &vertexes[SHORT(mld->v1)];
		v2 = ld->v2 = &vertexes[SHORT(mld->v2)];
		ld->dx = v2->x - v1->x;
		ld->dy = v2->y - v1->y;
		if (!ld->dx)
			ld->slopetype = ST_VERTICAL;
		else if (!ld->dy)
			ld->slopetype = ST_HORIZONTAL;
		else
		{
			if (FixedDiv (ld->dy , ld->dx) > 0)
				ld->slopetype = ST_POSITIVE;
			else
				ld->slopetype = ST_NEGATIVE;
		}

		if (v1->x < v2->x)
		{
			ld->bbox[BOXLEFT] = v1->x;
			ld->bbox[BOXRIGHT] = v2->x;
		}
		else
		{
			ld->bbox[BOXLEFT] = v2->x;
			ld->bbox[BOXRIGHT] = v1->x;
		}
		if (v1->y < v2->y)
		{
			ld->bbox[BOXBOTTOM] = v1->y;
			ld->bbox[BOXTOP] = v2->y;
		}
		else
		{
			ld->bbox[BOXBOTTOM] = v2->y;
			ld->bbox[BOXTOP] = v1->y;
		}
		ld->sidenum[0] = SHORT(mld->sidenum[0]);
		ld->sidenum[1] = SHORT(mld->sidenum[1]);
		if (ld->sidenum[0] != -1)
			ld->frontsector = sides[ld->sidenum[0]].sector;
		else
			ld->frontsector = 0;
		if (ld->sidenum[1] != -1)
			ld->backsector = sides[ld->sidenum[1]].sector;
		else
			ld->backsector = 0;
	}

	Z_Free (data);
}


/*
=================
=
= P_LoadSideDefs
=
=================
*/

void P_LoadSideDefs (int lump)
{
	byte                    *data;
	int                             i;
	mapsidedef_t    *msd;
	side_t                  *sd;

	numsides = W_LumpLength (lump) / sizeof(mapsidedef_t);
	sides = Z_Malloc (numsides*sizeof(side_t),PU_LEVEL,0);
	memset (sides, 0, numsides*sizeof(side_t));
	data = W_CacheLumpNum (lump,PU_STATIC);

	msd = (mapsidedef_t *)data;
	sd = sides;

	// Make sure primary lumps are used for texture searching
//	W_UsePrimary();

	for(i = 0; i < numsides; i++, msd++, sd++)
	{
		sd->textureoffset = SHORT(msd->textureoffset)<<FRACBITS;
		sd->rowoffset = SHORT(msd->rowoffset)<<FRACBITS;
		sd->toptexture = R_TextureNumForName(msd->toptexture);
		sd->bottomtexture = R_TextureNumForName(msd->bottomtexture);
		sd->midtexture = R_TextureNumForName(msd->midtexture);
		sd->sector = &sectors[SHORT(msd->sector)];
	}
/*	if(DevMaps)
	{
		W_UseAuxiliary();
	}*/
	Z_Free(data);
}

/*
=================
=
= P_LoadBlockMap
=
=================
*/

/*void P_LoadBlockMap (int lump)
{
	int             i, count;

	blockmaplump = W_CacheLumpNum (lump,PU_LEVEL);
	blockmap = blockmaplump+4;
	count = W_LumpLength (lump)/2;
	for (i=0 ; i<count ; i++)
		blockmaplump[i] = SHORT(blockmaplump[i]);

	bmaporgx = blockmaplump[0]<<FRACBITS;
	bmaporgy = blockmaplump[1]<<FRACBITS;
	bmapwidth = blockmaplump[2];
	bmapheight = blockmaplump[3];

// clear out mobj chains
	count = sizeof(*blocklinks)* bmapwidth*bmapheight;
	blocklinks = Z_Malloc (count,PU_LEVEL, 0);
	memset (blocklinks, 0, count);
}*/




/*
=================
=
= P_GroupLines
=
= Builds sector line lists and subsector sector numbers
= Finds block bounding boxes for sectors
=================
*/

void P_GroupLines (void)
{
	line_t          **linebuffer;
	int             i, j, total;
	line_t          *li;
	sector_t        *sector;
	subsector_t     *ss;
	seg_t           *seg;
	fixed_t         bbox[4];
	int             block;

// look up sector number for each subsector
	ss = subsectors;
	for(i=0; i<numsubsectors; i++, ss++)
	{
		seg = &segs[ss->firstline];
		ss->sector = NULL;
		for(j=0; j<ss->linecount; j++, seg++)
			if(seg->sidedef)
			{
				ss->sector = seg->sidedef->sector;
				break;
			}
		if(ss->sector == NULL)
			Con_Error("P_GroupLines: Subsector a part of no sector.\n");
	}

// count number of lines in each sector
	li = lines;
	total = 0;
	for (i=0 ; i<numlines ; i++, li++)
	{
		total++;
		li->frontsector->linecount++;
		if (li->backsector && li->backsector != li->frontsector)
		{
			li->backsector->linecount++;
			total++;
		}
	}

// build line tables for each sector
	linebuffer = Z_Malloc (total*4, PU_LEVEL, 0);
	sector = sectors;
	for (i=0 ; i<numsectors ; i++, sector++)
	{
		M_ClearBox (bbox);
		sector->Lines = linebuffer;
		li = lines;
		for (j=0 ; j<numlines ; j++, li++)
		{
			if (li->frontsector == sector || li->backsector == sector)
			{
				*linebuffer++ = li;
				M_AddToBox (bbox, li->v1->x, li->v1->y);
				M_AddToBox (bbox, li->v2->x, li->v2->y);
			}
		}
		if (linebuffer - sector->Lines != sector->linecount)
			Con_Error ("P_GroupLines: miscounted");

		// set the degenmobj_t to the middle of the bounding box
		sector->soundorg.x = (bbox[BOXRIGHT]+bbox[BOXLEFT])/2;
		sector->soundorg.y = (bbox[BOXTOP]+bbox[BOXBOTTOM])/2;

		// adjust bounding box to map blocks
		block = (bbox[BOXTOP]-bmaporgy+MAXRADIUS)>>MAPBLOCKSHIFT;
		block = block >= bmapheight ? bmapheight-1 : block;
		sector->blockbox[BOXTOP]=block;

		block = (bbox[BOXBOTTOM]-bmaporgy-MAXRADIUS)>>MAPBLOCKSHIFT;
		block = block < 0 ? 0 : block;
		sector->blockbox[BOXBOTTOM]=block;

		block = (bbox[BOXRIGHT]-bmaporgx+MAXRADIUS)>>MAPBLOCKSHIFT;
		block = block >= bmapwidth ? bmapwidth-1 : block;
		sector->blockbox[BOXRIGHT]=block;

		block = (bbox[BOXLEFT]-bmaporgx-MAXRADIUS)>>MAPBLOCKSHIFT;
		block = block < 0 ? 0 : block;
		sector->blockbox[BOXLEFT]=block;
	}

}

//=============================================================================

#if 0

// (0,1) = top left; (2,3) = bottom right
// Assumes sectors are always closed.
void P_SectorBoundingBox(sector_t *sec, float *bbox)
{
	float	x, y;
	int		i;
	line_t	*li;
	
	bbox[BLEFT] = bbox[BRIGHT] = sec->Lines[0]->v1->x >> FRACBITS;
	bbox[BTOP] = bbox[BBOTTOM] = sec->Lines[0]->v1->y >> FRACBITS;
	for(i=1; i<sec->linecount; i++)
	{
		li = sec->Lines[i];
		x = li->v1->x >> FRACBITS;
		y = li->v1->y >> FRACBITS;
		if(x < bbox[BLEFT]) bbox[BLEFT] = x;
		if(x > bbox[BRIGHT]) bbox[BRIGHT] = x;
		if(y < bbox[BTOP]) bbox[BTOP] = y;
		if(y > bbox[BBOTTOM]) bbox[BBOTTOM] = y;
	}
}

// Calculate the reverb settings for each sector.
void P_CalcSectorReverbs()
{
	int			i, c, type, k;
	subsector_t	*sub;
	sector_t	*sec;
	seg_t		*seg;
//	float		volume;
	float		total, metal, rock, wood, cloth;

	//Con_Message( "P_CalcSectorReverbs: begin at %i\n", gi.GetTime());

	// First determine each subsectors' individual characteristics.
	for(c=0, sub=subsectors; c<numsubsectors; c++, sub++)
	{
		// Space is the rough volume of the subsector (bounding box).
		sub->reverb[SSRD_SPACE] = ((sub->sector->ceilingheight - sub->sector->floorheight) >> FRACBITS)
			* (sub->bbox[1].x - sub->bbox[0].x) * (sub->bbox[1].y - sub->bbox[0].y);
		//Con_Message( "sub %i: volume %f Mu\n", c, volume/1e6);
/*	i = (int) (volume/1e4);
		if(i < 5) i = 5;
		if(i > 255) i = 255;
		sub->reverb[SSRD_SPACE] = i;*/
		
		// The other reverb properties can be found out by taking a look at the
		// walls surrounding the subsector.
		total = metal = rock = wood = cloth = 0;
		for(i=0, seg=segs+sub->firstline; i<sub->numLines; i++, seg++)
		{
			if(!seg->linedef || !seg->sidedef->midtexture) continue;
			total += seg->length;
			// The texture of the seg determines its type.
			type = R_TextureTypeForName(R_TextureNameForNum(seg->sidedef->midtexture));
			switch(type)
			{
			case TEXTYPE_METAL:
				metal += seg->length;
				break;

			case TEXTYPE_ROCK:
				rock += seg->length;
				break;

			case TEXTYPE_WOOD:
				wood += seg->length;
				break;

			case TEXTYPE_CLOTH:
				cloth += seg->length;
				break;

			default:
				// The type of the texture is unknown. Assume it's wood.
				wood += seg->length;
			}
		}
		if(!total) continue; // Huh?
		metal /= total;
		rock /= total;
		wood /= total;
		cloth /= total;

		// Volume.
		i = (int) ( metal*255 + rock*200 + wood*80 + cloth*5 );
		if(i < 0) i = 0;
		if(i > 255) i = 255;
		sub->reverb[SSRD_VOLUME] = i;

		// Decay time.
		i = (int) ( metal*255 + rock*160 + wood*50 + cloth*5 );
		if(i < 0) i = 0;
		if(i > 255) i = 255;
		sub->reverb[SSRD_DECAY] = i;

		// High frequency damping.
		i = (int) ( metal*25 + rock*100 + wood*200 + cloth*255 );
		if(i < 0) i = 0;
		if(i > 255) i = 255;
		sub->reverb[SSRD_DAMPING] = i;

		// The floor and sky also have an effect, especially is there is 
		// sky involved.
/*		if(sub->sector->ceilingpic == skyflatnum)
		{
			sub->reverb[SSRD_VOLUME] /= 1.5;
			sub->reverb[SSRD_DECAY] /= 2;
		}*/

/*		Con_Message( "sub %04i: vol:%3i sp:%3i dec:%3i dam:%3i\n", c, sub->reverb[SSRD_VOLUME],
			sub->reverb[SSRD_SPACE], sub->reverb[SSRD_DECAY], sub->reverb[SSRD_DAMPING]);*/
	}
	
	for(c=0, sec=sectors; c<numsectors; c++, sec++)
	{
		float bbox[4], spaceScatter;
		unsigned int sectorSpace;
		P_SectorBoundingBox(sec, bbox);
		//Con_Message( "sector %i: (%f,%f) - (%f,%f)\n", c, bbox[BLEFT], bbox[BTOP], bbox[BRIGHT], bbox[BBOTTOM]);

		sectorSpace = ((sec->ceilingheight - sec->floorheight) >> FRACBITS)
			* (bbox[BRIGHT] - bbox[BLEFT]) * (bbox[BBOTTOM] - bbox[BTOP]);

		bbox[BLEFT] -= 128;
		bbox[BRIGHT] += 128;
		bbox[BTOP] -= 128;
		bbox[BBOTTOM] += 128;

		for(i=0, k=0, sub=subsectors; i<numsubsectors; i++, sub++)
		{
			// Is this subsector close enough?
			if(sub->midpoint.x > bbox[BLEFT] && sub->midpoint.x < bbox[BRIGHT] 
				&& sub->midpoint.y > bbox[BTOP] && sub->midpoint.y < bbox[BBOTTOM])
			{
				//Con_Message( "- sub %i within, own:%i\n", i, sub->sector == sec);
				k++; 
				total = sub->reverb[SSRD_SPACE]/* / 255.0f*/;
				sec->reverbSpace += total;
				sec->reverbVolume += sub->reverb[SSRD_VOLUME] / 255.0f * total;
				sec->reverbDecay += sub->reverb[SSRD_DECAY] / 255.0f * total;
				sec->reverbDamping += sub->reverb[SSRD_DAMPING] / 255.0f * total;
			}
		}
		if(sec->reverbSpace)
		{
			spaceScatter = sectorSpace / (float) sec->reverbSpace;
			// These three are weighted by the space.
			sec->reverbVolume /= sec->reverbSpace;
			sec->reverbDecay /= sec->reverbSpace;
			sec->reverbDamping /= sec->reverbSpace;
		}
		else
		{
			spaceScatter = 0;
			sec->reverbVolume = .2f;
			sec->reverbDecay = .4f;
			sec->reverbDamping = 1;
		}
		//Con_Message( "sector %i: secSp:%fM revSp:%fM scatter: %f\n", c, sectorSpace/1e6f, sec->reverbSpace/1e6f, spaceScatter);

		// If the space is scattered, the reverb effect lessens.
		sec->reverbSpace /= spaceScatter > .8? 10 : spaceScatter > .6? 4 : 1;

		// Scale the reverb space to a reasonable range, so that 0 is very small and
		// .99 is very large. 1.0 is only for open areas.
		sec->reverbSpace /= 120e6;
		if(sec->reverbSpace > .99) sec->reverbSpace = .99f;
		
		if(sec->ceilingpic == skyflatnum) // An open sector?
		{
			// An open sector can still be small. In that case the reverb
			// is diminished a bit.
			if(sec->reverbSpace > .5) 
				sec->reverbVolume = 1; // Full volume.
			else 
				sec->reverbVolume = .5f; // Small sector, but still open.
			sec->reverbSpace = 1;
		}
		else // A closed sector.
		{
			// Large spaces have automatically a bit more audible reverb.
			sec->reverbVolume += sec->reverbSpace / 4;
		}
		if(sec->reverbVolume > 1) sec->reverbVolume = 1;
/*		sec->reverbDecay /= k/2.0f;
		sec->reverbDamping /= k;*/
	}

	//Con_Message( "P_CalcSectorReverbs: end at %i\n", gi.GetTime());
}

#endif

//===========================================================================
// P_PointLineDistance
//===========================================================================
fixed_t P_PointLineDistance(line_t *line, fixed_t x, fixed_t y, fixed_t *offset)
{
	float	a[2], b[2], c[2], d[2], len;

	a[VX] = FIX2FLT(line->v1->x);
	a[VY] = FIX2FLT(line->v1->y);

	b[VX] = FIX2FLT(line->v2->x);
	b[VY] = FIX2FLT(line->v2->y);

	c[VX] = FIX2FLT(x);
	c[VY] = FIX2FLT(y);

	d[VX] = b[VX] - a[VX];
	d[VY] = b[VY] - a[VY];
	len = sqrt(d[VX]*d[VX] + d[VY]*d[VY]); // Accurate.

	if(offset) *offset = FRACUNIT * ((a[VY]-c[VY])*(a[VY]-b[VY]) - (a[VX]-c[VX])*(b[VX]-a[VX])) / len;
	return FRACUNIT * ((a[VY]-c[VY])*(b[VX]-a[VX]) - (a[VX]-c[VX])*(b[VY]-a[VY])) / len;
}

//==========================================================================
// P_TurnTorchesToFaceWalls
//==========================================================================
void P_TurnTorchesToFaceWalls()
{
#define MAXLIST	200
	sector_t	*sec;
	mobj_t		*iter;
	int			i, k, t;
	line_t		*closestline = NULL, *li;
	fixed_t		closestdist, dist, off, linelen, minrad;
	mobj_t		*tlist[MAXLIST];

	for(sec = sectors, i = 0; i < numsectors; i++, sec++)
	{
		memset(tlist, 0, sizeof(tlist));
		
		// First all the things to process.
		for(k = 0, iter = sec->thinglist; k < MAXLIST-1 && iter; 
			iter = iter->snext)
		{
			if(iter->type == MT_ZWALLTORCH 
				|| iter->type == MT_ZWALLTORCH_UNLIT) 
				tlist[k++] = iter;
		}
		
		// Turn to face away from the nearest wall.
		for(t = 0; (iter = tlist[t]) != NULL; t++)
		{
			minrad = iter->radius;
			closestline = NULL;
			for(k = 0; k < sec->linecount; k++)
			{
				li = sec->Lines[k];
				if(li->backsector) continue;
				linelen = P_ApproxDistance(li->v2->x - li->v1->x, li->v2->y - li->v1->y);
				dist = P_PointLineDistance(li, iter->x, iter->y, &off);
				if(off > -minrad && off < linelen+minrad &&
					(!closestline || dist < closestdist) && dist >= 0)
				{
					closestdist = dist;
					closestline = li;
				}
			}
			if(closestline && closestdist < minrad)
			{
				iter->angle = R_PointToAngle2
					(closestline->v1->x, closestline->v1->y, 
					closestline->v2->x, closestline->v2->y) - ANG90;
			}
		}
	}
}


/*
=================
=
= P_SetupLevel
=
=================
*/

//#ifdef __WATCOMC__
//extern boolean i_CDMusic;
//#endif

void P_SetupLevel(int episode, int map, int playermask, skill_t skill)
{
	int i, setupflags = DDSLF_POLYGONIZE | DDSLF_FIX_SKY | DDSLF_REVERB;
	int parm;
	char lumpname[9], levelid[9];
	int lumpnum, gllumpnum;
	
	for(i = 0; i < MAXPLAYERS; i++)
	{
		players[i].killcount = players[i].secretcount
			= players[i].itemcount = 0;
	}
	players[consoleplayer].plr->viewz = 1; // will be set by player think

#if _DEBUG
	Z_CheckHeap();
#endif

	S_LevelChange();

#if _DEBUG
	Z_CheckHeap();
#endif

	S_StartMusic("chess", true); // Waiting-for-level-load song

#if _DEBUG
	Z_CheckHeap();
#endif

	Z_FreeTags(PU_LEVEL, PU_PURGELEVEL-1);

	P_InitThinkers();
	actual_leveltime = leveltime = 0;

	sprintf(lumpname, "MAP%02d", map);
	strcpy(levelid, lumpname);
	lumpnum = W_GetNumForName(lumpname);

	P_LoadBlockMap(lumpnum + ML_BLOCKMAP);

	// Check for GL lumps.
	sprintf(lumpname, "GL_MAP%02d", map);
	gllumpnum = W_CheckNumForName(lumpname);
	if(gllumpnum > lumpnum)
	{
		// We have GL nodes! Let's load them in.
		P_LoadVertexes(lumpnum+ML_VERTEXES, gllumpnum+1);				
		P_LoadSectors(lumpnum+ML_SECTORS);
		P_LoadSideDefs(lumpnum+ML_SIDEDEFS);
		P_LoadLineDefs(lumpnum+ML_LINEDEFS);
		P_LoadSubsectors(gllumpnum+3);
		P_LoadNodes(gllumpnum+4);
		P_LoadSegsGL(gllumpnum+2);
		setupflags |= DDSLF_DONT_CLIP;
	}
	else
	{
		//
		// Begin processing map lumps
		// Note: most of this ordering is important
		//
		P_LoadVertexes(lumpnum+ML_VERTEXES, -1);
		P_LoadSectors(lumpnum+ML_SECTORS);
		P_LoadSideDefs(lumpnum+ML_SIDEDEFS);
		P_LoadLineDefs(lumpnum+ML_LINEDEFS);
		P_LoadSubsectors(lumpnum+ML_SSECTORS);
		P_LoadNodes(lumpnum+ML_NODES);
		P_LoadSegs(lumpnum+ML_SEGS);
	}

	// Must be called before any mobjs are spawned.
	R_SetupLevel(levelid, DDSLF_INIT_LINKS);

	P_LoadReject(lumpnum + ML_REJECT);

	P_GroupLines();
	bodyqueslot = 0;
	po_NumPolyobjs = 0;
	deathmatch_p = deathmatchstarts;
	playerstart_p = playerstarts;
	P_LoadThings(lumpnum+ML_THINGS);

	// Server can't be initialized before PO_Init is done, but PO_Init
	// can't be done until SetupLevel is called...
	R_SetupLevel(levelid, setupflags | DDSLF_NO_SERVER);
	
	// Initialize polyobjs.
	PO_Init(lumpnum+ML_THINGS); // Initialize the polyobjs

	// Now we can init the server.
	R_SetupLevel(levelid, DDSLF_SERVER_ONLY);
	
	P_LoadACScripts(lumpnum+ML_BEHAVIOR); // ACS object code

	P_DealPlayerStarts();
	P_SpawnPlayers();

	// If deathmatch, randomly spawn the active players
	TimerGame = 0;
	if(deathmatch)
	{
		parm = ArgCheck("-timer");
		if(parm && parm < Argc()-1)
		{
			TimerGame = atoi(Argv(parm+1))*35*60;
		}
	}

	// Set up world state.
	P_SpawnSpecials ();

	//gi.SetupLevel(levelid, setupflags);

	// Preload graphics.
	if(precache) 
	{
		R_PrecacheLevel ();
		R_PrecachePSprites();
	}

	// Check if the level is a lightning level.
	P_InitLightning();

/*	S_StopAllSound(); */
	SN_StopAllSequences();
	S_LevelMusic();

	// Load colormap and set the fullbright flag
	i = P_GetMapFadeTable(gamemap);
	if(i == W_GetNumForName("COLORMAP"))
	{
		// We don't want fog in this case.
		GL_UseFog(false);
	}
	else
	{ 
		// Probably fog ... don't use fullbright sprites
		if(i == W_GetNumForName("FOGMAP"))
		{
			// Tell the renderer to turn on the fog.
			GL_UseFog(true);
		}
	}

	P_TurnTorchesToFaceWalls();

	// Print a message in the console about this level.
	Con_Message("Map %d (%d): %s\n", P_GetMapWarpTrans(map), map, 
		P_GetMapName(map));

	R_SetupLevel(levelid, DDSLF_FINALIZE);
}

//===========================================================================
// P_SetSongCDTrack
//===========================================================================
void P_SetSongCDTrack(int index, int track)
{
	// Set the internal array.
	cd_NonLevelTracks[index] = sc_Number;

	// Update the corresponding Doomsday definition.
	Def_Set(DD_DEF_MUSIC, Def_Get(DD_DEF_MUSIC, cd_SongDefIDs[index], 0), 
		DD_CD_TRACK, (void*) track);
}

//==========================================================================
// InitMapInfo
//==========================================================================
static void InitMapInfo(void)
{
	int map;
	int mapMax;
	int mcmdValue;
	mapInfo_t *info;
	char songMulch[10];

	mapMax = 1;

	// Put defaults into MapInfo[0]
	info = MapInfo;
	info->cluster = 0;
	info->warpTrans = 0;
	info->nextMap = 1; // Always go to map 1 if not specified
	info->cdTrack = 1;
	info->sky1Texture = R_TextureNumForName(shareware? "SKY2" : DEFAULT_SKY_NAME);
	info->sky2Texture = info->sky1Texture;
	info->sky1ScrollDelta = 0;
	info->sky2ScrollDelta = 0;
	info->doubleSky = false;
	info->lightning = false;
	info->fadetable = W_GetNumForName(DEFAULT_FADE_TABLE);
	strcpy(info->name, UNKNOWN_MAP_NAME);

	for(map=0; map<99; map++) MapInfo[map].warpTrans = 0;

//	strcpy(info->songLump, DEFAULT_SONG_LUMP);
	SC_Open(MAPINFO_SCRIPT_NAME);
	while(SC_GetString())
	{
		if(SC_Compare("MAP") == false)
		{
			SC_ScriptError(NULL);
		}
		SC_MustGetNumber();
		if(sc_Number < 1 || sc_Number > 99)
		{ // 
			SC_ScriptError(NULL);
		}
		map = sc_Number;

		info = &MapInfo[map];

		// Save song lump name
		strcpy(songMulch, info->songLump);

		// Copy defaults to current map definition
		memcpy(info, &MapInfo[0], sizeof(*info));

		// Restore song lump name
		strcpy(info->songLump, songMulch);

		// The warp translation defaults to the map number
		info->warpTrans = map;

		// Map name must follow the number
		SC_MustGetString();
		strcpy(info->name, sc_String);

		// Process optional tokens
		while(SC_GetString())
		{
			if(SC_Compare("MAP"))
			{ // Start next map definition
				SC_UnGet();
				break;
			}
			mcmdValue = MapCmdIDs[SC_MustMatchString(MapCmdNames)];
			switch(mcmdValue)
			{
				case MCMD_CLUSTER:
					SC_MustGetNumber();
					info->cluster = sc_Number;
					break;
				case MCMD_WARPTRANS:
					SC_MustGetNumber();
					info->warpTrans = sc_Number;
					break;
				case MCMD_NEXT:
					SC_MustGetNumber();
					info->nextMap = sc_Number;
					break;
				case MCMD_CDTRACK:
					SC_MustGetNumber();
					info->cdTrack = sc_Number;
					break;
				case MCMD_SKY1:
					SC_MustGetString();
					info->sky1Texture = R_TextureNumForName(sc_String);
					SC_MustGetNumber();
					info->sky1ScrollDelta = sc_Number<<8;
					break;
				case MCMD_SKY2:
					SC_MustGetString();
					info->sky2Texture = R_TextureNumForName(sc_String);
					SC_MustGetNumber();
					info->sky2ScrollDelta = sc_Number<<8;
					break;
				case MCMD_DOUBLESKY:
					info->doubleSky = true;
					break;
				case MCMD_LIGHTNING:
					info->lightning = true;
					break;
				case MCMD_FADETABLE:
					SC_MustGetString();
					info->fadetable = W_GetNumForName(sc_String);
					break;
				case MCMD_CD_STARTTRACK:
				case MCMD_CD_END1TRACK:
				case MCMD_CD_END2TRACK:
				case MCMD_CD_END3TRACK:
				case MCMD_CD_INTERTRACK:
				case MCMD_CD_TITLETRACK:
					SC_MustGetNumber();
					P_SetSongCDTrack(mcmdValue - MCMD_CD_STARTTRACK, 
						sc_Number);
					break;
			}
		}
		mapMax = map > mapMax ? map : mapMax;
	}
	SC_Close();
	MapCount = mapMax;
}

//==========================================================================
//
// P_GetMapCluster
//
//==========================================================================

int P_GetMapCluster(int map)
{
	return MapInfo[QualifyMap(map)].cluster;
}

//==========================================================================
//
// P_GetMapCDTrack
//
//==========================================================================

int P_GetMapCDTrack(int map)
{
	return MapInfo[QualifyMap(map)].cdTrack;
}

//==========================================================================
//
// P_GetMapWarpTrans
//
//==========================================================================

int P_GetMapWarpTrans(int map)
{
	return MapInfo[QualifyMap(map)].warpTrans;
}

//==========================================================================
//
// P_GetMapNextMap
//
//==========================================================================

int P_GetMapNextMap(int map)
{
	return MapInfo[QualifyMap(map)].nextMap;
}

//==========================================================================
//
// P_TranslateMap
//
// Returns the actual map number given a warp map number.
//
//==========================================================================

int P_TranslateMap(int map)
{
	int i;

/*	ST_Message("P_TranslateMap(%d):\n", map);
	for(i = 1; i < 99; i++) // Make this a macro
		ST_Message("- %d: warp to %d\n", i, MapInfo[i].warpTrans);*/

	for(i = 1; i < 99; i++) // Make this a macro (?)
	{
		if(MapInfo[i].warpTrans == map)
		{
			return i;
		}
	}
	// Not found
	return -1;
}

//==========================================================================
//
// P_GetMapSky1Texture
//
//==========================================================================

int P_GetMapSky1Texture(int map)
{
	return MapInfo[QualifyMap(map)].sky1Texture;
}

//==========================================================================
//
// P_GetMapSky2Texture
//
//==========================================================================

int P_GetMapSky2Texture(int map)
{
	return MapInfo[QualifyMap(map)].sky2Texture;
}

//==========================================================================
//
// P_GetMapName
//
//==========================================================================

char *P_GetMapName(int map)
{
	return MapInfo[QualifyMap(map)].name;
}

//==========================================================================
//
// P_GetMapSky1ScrollDelta
//
//==========================================================================

fixed_t P_GetMapSky1ScrollDelta(int map)
{
	return MapInfo[QualifyMap(map)].sky1ScrollDelta;
}

//==========================================================================
//
// P_GetMapSky2ScrollDelta
//
//==========================================================================

fixed_t P_GetMapSky2ScrollDelta(int map)
{
	return MapInfo[QualifyMap(map)].sky2ScrollDelta;
}

//==========================================================================
//
// P_GetMapDoubleSky
//
//==========================================================================

boolean P_GetMapDoubleSky(int map)
{
	return MapInfo[QualifyMap(map)].doubleSky;
}

//==========================================================================
//
// P_GetMapLightning
//
//==========================================================================

boolean P_GetMapLightning(int map)
{
	return MapInfo[QualifyMap(map)].lightning;
}

//==========================================================================
//
// P_GetMapFadeTable
//
//==========================================================================

boolean P_GetMapFadeTable(int map)
{
	return MapInfo[QualifyMap(map)].fadetable;
}

//==========================================================================
//
// P_GetMapSongLump
//
//==========================================================================

char *P_GetMapSongLump(int map)
{
	if(!strcasecmp(MapInfo[QualifyMap(map)].songLump, DEFAULT_SONG_LUMP))
	{
		return NULL;
	}
	else
	{
		return MapInfo[QualifyMap(map)].songLump;
	}
}

//==========================================================================
//
// P_PutMapSongLump
//
//==========================================================================

void P_PutMapSongLump(int map, char *lumpName)
{
	if(map < 1 || map > MapCount)
	{
		return;
	}
	strcpy(MapInfo[map].songLump, lumpName);
}

//==========================================================================
//
// P_GetCDStartTrack
//
//==========================================================================

int P_GetCDStartTrack(void)
{
	return cd_NonLevelTracks[MCMD_CD_STARTTRACK-MCMD_CD_STARTTRACK];
}

//==========================================================================
//
// P_GetCDEnd1Track
//
//==========================================================================

int P_GetCDEnd1Track(void)
{
	return cd_NonLevelTracks[MCMD_CD_END1TRACK-MCMD_CD_STARTTRACK];
}

//==========================================================================
//
// P_GetCDEnd2Track
//
//==========================================================================

int P_GetCDEnd2Track(void)
{
	return cd_NonLevelTracks[MCMD_CD_END2TRACK-MCMD_CD_STARTTRACK];
}

//==========================================================================
//
// P_GetCDEnd3Track
//
//==========================================================================

int P_GetCDEnd3Track(void)
{
	return cd_NonLevelTracks[MCMD_CD_END3TRACK-MCMD_CD_STARTTRACK];
}

//==========================================================================
//
// P_GetCDIntermissionTrack
//
//==========================================================================

int P_GetCDIntermissionTrack(void)
{
	return cd_NonLevelTracks[MCMD_CD_INTERTRACK-MCMD_CD_STARTTRACK];
}

//==========================================================================
//
// P_GetCDTitleTrack
//
//==========================================================================

int P_GetCDTitleTrack(void)
{
	return cd_NonLevelTracks[MCMD_CD_TITLETRACK-MCMD_CD_STARTTRACK];
}

//==========================================================================
//
// QualifyMap
//
//==========================================================================

static int QualifyMap(int map)
{
	return (map < 1 || map > MapCount) ? 0 : map;
}

//==========================================================================
//
// P_Init
//
//==========================================================================

void P_Init(void)
{
	InitMapInfo();
	P_InitSwitchList();
	P_InitFTAnims(); // Init flat and texture animations
	P_InitTerrainTypes();
	P_InitLava();
}


// Special early initializer needed to start sound before R_Init()
void InitMapMusicInfo(void)
{
	int i;

	for (i=0; i<99; i++)
	{
		strcpy(MapInfo[i].songLump, DEFAULT_SONG_LUMP);
	}
	MapCount = 98;
}

