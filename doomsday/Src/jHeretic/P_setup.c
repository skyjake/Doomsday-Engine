
// P_main.c

#include "Doomdef.h"
#include "P_local.h"
#include "Soundst.h"
#include "S_sound.h"
#include "r_common.h"
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

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


void	P_SpawnMapThing (mapthing_t *mthing);

int firstGLvertex = 0;

/*
short		*blockmaplump;			// offsets in blockmap are from here
short		*blockmap;
int			bmapwidth, bmapheight;	// in mapblocks
fixed_t		bmaporgx, bmaporgy;		// origin of block map
mobj_t		**blocklinks;			// for thing chains

byte		*rejectmatrix;			// for fast sight rejection
*/

mapthing_t	deathmatchstarts[16], *deathmatch_p;
//mapthing_t	playerstarts[MAXPLAYERS], *playerstart_p;

void P_CalcSectorReverbs();

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
	byte		*data;
	int			i;
	mapseg_t	*ml;
	seg_t		*li;
	line_t	*ldef;
	int			linedef, side;
	
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
		li->v1 = &vertexes[gls->v1 & 0x8000? firstGLvertex 
			+ (gls->v1 & 0x7fff) : gls->v1];

		li->v2 = &vertexes[gls->v2 & 0x8000? firstGLvertex 
			+ (gls->v2 & 0x7fff) : gls->v2];

		if(gls->linedef != -1)
		{
			ldef = &lines[gls->linedef];
			li->linedef = ldef;
			li->sidedef = &sides[ldef->sidenum[gls->side]];
			li->frontsector = sides[ldef->sidenum[gls->side]].sector;
			if(ldef->flags & ML_TWOSIDED)
			{
				li->backsector = sides[ldef->sidenum[gls->side^1]].sector;
			}
			else
			{
				li->backsector = 0;
			}
			if(gls->side == 0)
			{
				li->offset = FRACUNIT * AccurateDistance(li->v1->x 
					- ldef->v1->x, li->v1->y - ldef->v1->y); 
			}
			else 
			{
				li->offset = FRACUNIT * AccurateDistance(li->v1->x 
					- ldef->v2->x, li->v1->y - ldef->v2->y); 
			}
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
		li->length = AccurateDistance(li->v2->x - li->v1->x, 
			li->v2->y - li->v1->y);								
	}
	
	Z_Free(data);
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
	byte			*data;
	int				i;
	mapsubsector_t	*ms;
	subsector_t		*ss;
	
	numsubsectors = W_LumpLength (lump) / sizeof(mapsubsector_t);
	subsectors = Z_Malloc (numsubsectors*sizeof(subsector_t),PU_LEVEL,0);	
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
	byte			*data;
	int				i;
	mapsector_t		*ms;
	sector_t		*ss;
	
	numsectors = W_LumpLength (lump) / sizeof(mapsector_t);
	sectors = Z_Malloc (numsectors*sizeof(sector_t),PU_LEVEL,0);	
	memset (sectors, 0, numsectors*sizeof(sector_t));
	data = W_CacheLumpNum (lump,PU_STATIC);
	
	ms = (mapsector_t *)data;
	ss = sectors;
	for (i=0 ; i<numsectors ; i++, ss++, ms++)
	{
		ss->floorheight = SHORT(ms->floorheight)<<FRACBITS;
		ss->ceilingheight = SHORT(ms->ceilingheight)<<FRACBITS;
		ss->floorpic = R_FlatNumForName(ms->floorpic);
		ss->ceilingpic = R_FlatNumForName(ms->ceilingpic);
		ss->lightlevel = SHORT(ms->lightlevel);
		ss->special = SHORT(ms->special);
		ss->tag = SHORT(ms->tag);
		ss->thinglist = NULL;
		memset(ss->rgb, 0xff, 3);
	}
	
	Z_Free (data);
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
	byte		*data;
	int			i,j,k;
	mapnode_t	*mn;
	node_t		*no;
	
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



/*
=================
=
= P_LoadThings
=
=================
*/

void P_LoadThings (int lump)
{
	byte			*data;
	int				i;
	mapthing_t		*mt;
	int				numthings;

	data = W_CacheLumpNum (lump,PU_STATIC);
	numthings = W_LumpLength (lump) / sizeof(mapthing_t);
	
	mt = (mapthing_t *)data;
	for (i=0 ; i<numthings ; i++, mt++)
	{
		mt->x = SHORT(mt->x);
		mt->y = SHORT(mt->y);
		mt->angle = SHORT(mt->angle);
		mt->type = SHORT(mt->type);
		mt->options = SHORT(mt->options);
		P_SpawnMapThing (mt);
	}
	
	Z_Free (data);
}



/*
=================
=
= P_LoadLineDefs
=
= Also counts secret lines for intermissions
=================
*/

void P_LoadLineDefs (int lump)
{
	byte			*data;
	int				i;
	maplinedef_t	*mld;
	line_t			*ld;
	vertex_t		*v1, *v2;
	
	numlines = W_LumpLength (lump) / sizeof(maplinedef_t);
	lines = Z_Malloc (numlines*sizeof(line_t),PU_LEVEL,0);	
	memset (lines, 0, numlines*sizeof(line_t));
	data = W_CacheLumpNum (lump,PU_STATIC);
	
	mld = (maplinedef_t *)data;
	ld = lines;
	for (i=0 ; i<numlines ; i++, mld++, ld++)
	{
		ld->flags = SHORT(mld->flags);
		ld->special = SHORT(mld->special);
		ld->tag = SHORT(mld->tag);
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
	byte			*data;
	int				i;
	mapsidedef_t	*msd;
	side_t			*sd;
	
	numsides = W_LumpLength (lump) / sizeof(mapsidedef_t);
	sides = Z_Malloc (numsides*sizeof(side_t),PU_LEVEL,0);	
	memset (sides, 0, numsides*sizeof(side_t));
	data = W_CacheLumpNum (lump,PU_STATIC);
	
	msd = (mapsidedef_t *)data;
	sd = sides;
	for (i=0 ; i<numsides ; i++, msd++, sd++)
	{
		sd->textureoffset = SHORT(msd->textureoffset)<<FRACBITS;
		sd->rowoffset = SHORT(msd->rowoffset)<<FRACBITS;
		sd->toptexture = R_TextureNumForName(msd->toptexture);
		sd->bottomtexture = R_TextureNumForName(msd->bottomtexture);
		sd->midtexture = R_TextureNumForName(msd->midtexture);
		sd->sector = &sectors[SHORT(msd->sector)];
	}
	
	Z_Free (data);
}


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
	line_t		**linebuffer;
	int			i, j, total;
	line_t		*li;
	sector_t	*sector;
	subsector_t	*ss;
	seg_t		*seg;
	fixed_t		bbox[4];
	int			block;
	
// look up sector number for each subsector
	ss = subsectors;
	for (i=0 ; i<numsubsectors ; i++, ss++)
	{
		/*seg = &segs[ss->firstline];
		ss->sector = seg->sidedef->sector;*/

		seg = &segs[ss->firstline];
		ss->sector = NULL;
		for(j=0; j<ss->linecount; j++, seg++)
			if(seg->sidedef)
			{
				ss->sector = seg->sidedef->sector;
				break;
			}
		if(ss->sector == NULL)
			Con_Error("P_GroupLines: Subsector a part of no sector!\n");
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
static void projectVector(float a[2], float b[2], float *a_on_b)
{
	float	factor = (a[0]*b[0] + a[1]*b[1]) / (b[0]*b[0] + b[1]*b[1]);
	a_on_b[0] = factor * b[0];
	a_on_b[1] = factor * b[1];
}
#endif

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

/*	for(c=0; c<2; c++) uvecWall[c] = d[c] / len;

	// The normal.
	uvecWallNormal[VX] = uvecWall[VY];
	uvecWallNormal[VY] = -uvecWall[VX];

	// The relative position of the point.
	vecDist[VX] = FIX2FLT(x - line->v1->x);
	vecDist[VY] = FIX2FLT(y - line->v1->y);
	
	projectVector(vecDist, uvecWallNormal, vecDist);

	dist = P_ApproxDistance((fixed_t)(vecDist[VX]*FRACUNIT), (fixed_t)(vecDist[VY]*FRACUNIT));
*/
/*
            (YA-YC)(YA-YB)-(XA-XC)(XB-XA)
        r = -----------------------------
                        L**2

            (YA-YC)(XB-XA)-(XA-XC)(YB-YA)
        s = -----------------------------
                        L**2

    Let I be the point of perpendicular projection of C onto AB, the

        XI=XA+r(XB-XA)
        YI=YA+r(YB-YA)

    Distance from A to I = r*L
    Distance from C to I = s*L
*/

	if(offset) *offset = FRACUNIT * ((a[VY]-c[VY])*(a[VY]-b[VY]) - (a[VX]-c[VX])*(b[VX]-a[VX])) / len;
	return FRACUNIT * ((a[VY]-c[VY])*(b[VX]-a[VX]) - (a[VX]-c[VX])*(b[VY]-a[VY])) / len;
}

//==========================================================================
// P_MoveThingsOutOfWalls
//	Only affects torches, which are often placed inside walls in the 
//	original maps. The DOOM engine allowed these kinds of things but
//	a Z-buffer doesn't.
//==========================================================================
void P_MoveThingsOutOfWalls()
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
		// First all the things to move.
		for(k = 0, iter = sec->thinglist; iter; iter = iter->snext)
			// Wall torches are most often seen inside walls.
			if(iter->type == MT_MISC10) 
				tlist[k++] = iter;
		// Move the things out of walls.
		for(t = 0; (iter = tlist[t]) != NULL; t++)
		{
			minrad = iter->radius/2;
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
				float dx, dy, offlen = FIX2FLT(minrad - closestdist);
				float len;
				li = closestline;
				dy = -FIX2FLT(li->v2->x - li->v1->x);
				dx = FIX2FLT(li->v2->y - li->v1->y);
				len = sqrt(dx*dx + dy*dy);
				dx *= offlen / len;
				dy *= offlen / len;
				P_UnsetThingPosition(iter);
				iter->x += FRACUNIT * dx;
				iter->y += FRACUNIT * dy;
				P_SetThingPosition(iter);
			}
		}
	}
}

//==========================================================================
// P_TurnGizmosAwayFromDoors
//	Fails in some places, but works most of the time.
//==========================================================================
void P_TurnGizmosAwayFromDoors()
{
#define MAXLIST	200
	sector_t	*sec;
	mobj_t		*iter;
	int			i, k, t;
	line_t		*closestline = NULL, *li;
	fixed_t		closestdist, dist, off, linelen; //, minrad;
	mobj_t		*tlist[MAXLIST];

	for(sec = sectors, i = 0; i < numsectors; i++, sec++)
	{
		memset(tlist, 0, sizeof(tlist));
		
		// First all the things to process.
		for(k = 0, iter = sec->thinglist; k < MAXLIST-1 && iter; 
			iter = iter->snext)
		{
			if(iter->type == MT_KEYGIZMOBLUE
				|| iter->type == MT_KEYGIZMOGREEN
				|| iter->type == MT_KEYGIZMOYELLOW) 
				tlist[k++] = iter;
		}
		
		// Turn to face away from the nearest door.
		for(t = 0; (iter = tlist[t]) != NULL; t++)
		{
			closestline = NULL;
			for(k = 0; k < numlines; k++)
			{
				li = lines + k;
				// It must be a special line with a back sector.
				if(!li->backsector || (li->special != 32
					&& li->special != 33
					&& li->special != 34
					&& li->special != 26
					&& li->special != 27
					&& li->special != 28)) continue;
				linelen = P_ApproxDistance(li->v2->x - li->v1->x, li->v2->y - li->v1->y);
				dist = abs(P_PointLineDistance(li, iter->x, iter->y, &off));
				if(!closestline || dist < closestdist)
				{
					closestdist = dist;
					closestline = li;
				}
			}
			if(closestline)
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

void P_SetupLevel (int episode, int map, int playermask, skill_t skill)
{
	int		setupflags = DDSLF_POLYGONIZE | DDSLF_FIX_SKY | DDSLF_REVERB;
	int		i;
	int		parm;
	char	levelid[9];
	char	lumpname[9];
	int		lumpnum, gllumpnum;
	char	*lname, *lauthor;
	
	totalkills = totalitems = totalsecret = 0;
	for (i=0 ; i<MAXPLAYERS ; i++)
	{
		players[i].killcount = players[i].secretcount 
			= players[i].itemcount = 0;
	}
	players[consoleplayer].plr->viewz = 1; // will be set by player think
	
	//S_Start ();			// make sure all sounds are stopped before Z_FreeTags
	S_LevelChange();
	
	Z_FreeTags (PU_LEVEL, PU_PURGELEVEL-1);
	
	P_InitThinkers ();
	
//
// look for a regular (development) map first
//
	lumpname[0] = 'E';
	lumpname[1] = '0' + episode;
	lumpname[2] = 'M';
	lumpname[3] = '0' + map;
	lumpname[4] = 0;
	leveltime = 0;
	actual_leveltime = 0;
	strcpy(levelid, lumpname);

	Con_Message( "SetupLevel: %s ", lumpname);

	lumpnum = W_GetNumForName (lumpname);
// note: most of this ordering is important	
	
	P_LoadBlockMap(lumpnum+ML_BLOCKMAP);

	// Check for GL lumps.
	lumpname[0] = 'G';
	lumpname[1] = 'L';
	lumpname[2] = '_';
	lumpname[3] = 'E';
	lumpname[4] = '0' + episode;
	lumpname[5] = 'M';
	lumpname[6] = '0' + map;
	lumpname[7] = 0;
	gllumpnum = W_CheckNumForName(lumpname);
	if(gllumpnum > lumpnum)
	{
		Con_Message("(GL data found)\n");
		// We have GL nodes! Let's load them in.
		P_LoadVertexes (lumpnum+ML_VERTEXES, gllumpnum+1);				
		P_LoadSectors (lumpnum+ML_SECTORS);
		P_LoadSideDefs (lumpnum+ML_SIDEDEFS);
		P_LoadLineDefs (lumpnum+ML_LINEDEFS);
		P_LoadSubsectors (gllumpnum+3);
		P_LoadNodes (gllumpnum+4);
		P_LoadSegsGL (gllumpnum+2);
		// The subsectors in the GL nodes don't need processing.
		setupflags |= DDSLF_DONT_CLIP;
	}
	else
	{
		//
		// Begin processing map lumps
		// Note: most of this ordering is important
		//
		Con_Message("\n");
		P_LoadVertexes (lumpnum+ML_VERTEXES, -1);
		P_LoadSectors (lumpnum+ML_SECTORS);
		P_LoadSideDefs (lumpnum+ML_SIDEDEFS);
		P_LoadLineDefs (lumpnum+ML_LINEDEFS);
		P_LoadSubsectors (lumpnum+ML_SSECTORS);
		P_LoadNodes (lumpnum+ML_NODES);
		P_LoadSegs (lumpnum+ML_SEGS);		
	}
		
	// Must be called before any mobjs are spawned.
	R_SetupLevel(levelid, DDSLF_INIT_LINKS);

	P_LoadReject(lumpnum+ML_REJECT);
	P_GroupLines ();

	bodyqueslot = 0;
	deathmatch_p = deathmatchstarts;
	playerstart_p = playerstarts;

	// It's imperative that this is called!
	// - dlBlockLinks initialized
	// - necessary GL data generated
	// - sky fix
	R_SetupLevel(levelid, setupflags);

	P_InitAmbientSound();
	P_InitMonsters();
	P_OpenWeapons();
	P_LoadThings(lumpnum+ML_THINGS);
	P_CloseWeapons();

	P_DealPlayerStarts();
//
// if deathmatch, randomly spawn the active players
//
	TimerGame = 0;
	if(deathmatch)
	{
		parm = ArgCheck("-timer");
		if(parm && parm < myargc-1)
		{
			TimerGame = atoi(Argv(parm+1))*35*60;
		}
	}

	P_SpawnPlayers();

// set up world state
	P_SpawnSpecials ();

// preload graphics
	if (precache) 
	{
		R_PrecacheLevel();
		R_PrecachePSprites();
	}

	S_LevelMusic();

	// Do some fine tuning with mobj placement and orientation.
	P_MoveThingsOutOfWalls();
	P_TurnGizmosAwayFromDoors();

	lname = (char*) Get(DD_MAP_NAME);
	lauthor = (char*) Get(DD_MAP_AUTHOR);
	if(lname || lauthor)
	{
		Con_Printf("\n");
		if(lname) Con_FPrintf(CBLF_LIGHT|CBLF_BLUE, "%s\n", lname);
		if(lauthor) Con_FPrintf(CBLF_LIGHT|CBLF_BLUE, "Author: %s\n", lauthor);
		Con_Printf("\n");
	}

	R_SetupLevel(levelid, DDSLF_FINALIZE);
}

char *P_GetShortLevelName(int episode, int map)
{
	char *name = P_GetLevelName(episode, map);
	char *ptr;

	// Remove the "ExMx:" from the beginning.
	ptr = strchr(name, ':');
	if(!ptr) return name;
	name = ptr + 1;
	while(*name && isspace(*name)) name++; // Skip any number of spaces.
	return name;
}

char *P_GetLevelName(int episode, int map)
{
	char id[10];
	ddmapinfo_t info;

	// Compose the level identifier.
	sprintf(id, "E%iM%i", episode, map);

	// Get the map info definition.
	if(!Def_Get(DD_DEF_MAP_INFO, id, &info)) 
	{
		// There is no map information for this map...
		return "";
	}
	return info.name;
}

/*
=================
=
= P_Init
=
=================
*/

void P_Init (void)
{	
	P_InitSwitchList();
	P_InitPicAnims();
	P_InitTerrainTypes();
	P_InitLava();
}

