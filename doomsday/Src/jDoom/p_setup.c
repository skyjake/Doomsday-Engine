// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log$
// Revision 1.2  2003/02/27 23:14:32  skyjake
// Obsolete jDoom files removed
//
// Revision 1.1  2003/02/26 19:21:57  skyjake
// Initial checkin
//
// Revision 1.1  2002/09/29 01:11:47  Jaakko
// Added Doomsday sources
//
//
// DESCRIPTION:
//	Do all the WAD I/O, get map description,
//	set up initial state and misc. LUTs.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id$";

#include "doomdef.h"
#include "d_config.h"
#include "d_netjd.h"

#include <math.h>

#include "m_swap.h"
#include "g_game.h"
#include "p_local.h"
#include "s_sound.h"
#include "doomstat.h"
#include "m_bams.h"
#include "hu_stuff.h"
#include "d_items.h" // has GetDefInt

// External Data ------------------------------------------------------------

extern int actual_leveltime;

// Functions Prototypes -----------------------------------------------------

void	P_SpawnMapThing (mapthing_t*	mthing);

// Types --------------------------------------------------------------------

// For glBSP.
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

int firstGLvertex = 0;

// Data --------------------------------------------------------------------

// Maintain single and multi player starting spots.
mapthing_t	deathmatchstarts[MAX_DM_STARTS];
mapthing_t	*deathmatch_p;

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


//
// P_LoadVertexes
//
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
		numvertexes += W_LumpLength(gllump) / 
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


//
// P_LoadSegs
//
void P_LoadSegs (int lump)
{
    byte*		data;
    int			i;
    mapseg_t*	ml;
    seg_t*		li;
    line_t*		ldef;
    int			linedef;
    int			side;
	
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
				
		li->length = AccurateDistance(li->v2->x - li->v1->x, li->v2->y - li->v1->y);	
		if(li->length == 0) li->length = 0.01f;	// Hmm...
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


//
// P_LoadSubsectors
//
void P_LoadSubsectors (int lump)
{
    byte*		data;
    int			i;
    mapsubsector_t*	ms;
    subsector_t*	ss;
	
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



//
// P_LoadSectors
//
void P_LoadSectors (int lump)
{
    byte*		data;
    int			i;
    mapsector_t*	ms;
    sector_t*		ss;
	
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


//
// P_LoadNodes
//
void P_LoadNodes (int lump)
{
    byte*	data;
    int		i;
    int		j;
    int		k;
    mapnode_t*	mn;
    node_t*	no;
	
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


//
// P_LoadThings
//
void P_LoadThings (int lump)
{
    byte*		data;
    int			i;
    mapthing_t*		mt;
    int			numthings;
    boolean		spawn;
	
    data = W_CacheLumpNum (lump,PU_STATIC);
    numthings = W_LumpLength (lump) / sizeof(mapthing_t);
	
    mt = (mapthing_t *)data;
    for (i=0 ; i<numthings ; i++, mt++)
    {
	spawn = true;

	// Do not spawn cool, new monsters if !commercial
	if ( gamemode != commercial)
	{
	    switch(mt->type)
	    {
	      case 68:	// Arachnotron
	      case 64:	// Archvile
	      case 88:	// Boss Brain
	      case 89:	// Boss Shooter
	      case 69:	// Hell Knight
	      case 67:	// Mancubus
	      case 71:	// Pain Elemental
	      case 65:	// Former Human Commando
	      case 66:	// Revenant
	      case 84:	// Wolf SS
		spawn = false;
		break;
	    }
	}
	if (spawn == false)
	    break;

	// Do spawn all other stuff. 
	mt->x = SHORT(mt->x);
	mt->y = SHORT(mt->y);
	mt->angle = SHORT(mt->angle);
	mt->type = SHORT(mt->type);
	mt->options = SHORT(mt->options);
	
	P_SpawnMapThing (mt);
    }
	
    Z_Free (data);
}


//
// P_LoadLineDefs
// Also counts secret lines for intermissions.
//
void P_LoadLineDefs (int lump)
{
    byte*		data;
    int			i;
    maplinedef_t*	mld;
    line_t*		ld;
    vertex_t*		v1;
    vertex_t*		v2;
	
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


//
// P_LoadSideDefs
//
void P_LoadSideDefs (int lump)
{
    byte*		data;
    int			i;
    mapsidedef_t*	msd;
    side_t*		sd;
	
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


//
// P_LoadBlockMap
//
/*void P_LoadBlockMap (int lump)
{
    int		i;
    int		count;
	
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
}
*/


//
// P_GroupLines
// Builds sector line lists and subsector sector numbers.
// Finds block bounding boxes for sectors.
//
void P_GroupLines (void)
{
    line_t**		linebuffer;
    int				i;
    int				j;
    int				total;
    line_t*			li;
    sector_t*		sector;
    subsector_t*	ss;
    seg_t*			seg;
    fixed_t			bbox[4];
    int				block;
	
    // look up sector number for each subsector
    ss = subsectors;
    for (i=0 ; i<numsubsectors ; i++, ss++)
    {
/*		seg = &segs[ss->firstline];
		ss->sector = seg->sidedef->sector;*/

		seg = &segs[ss->firstline];
		ss->sector = NULL;
		for(j = 0; j < ss->linecount; j++, seg++)
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
    linebuffer = Z_Malloc(total*4, PU_LEVEL, 0);
    sector = sectors;
    for(i = 0; i < numsectors; i++, sector++)
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

//===========================================================================
// P_SpawnStaticLights
//===========================================================================
void P_SpawnStaticLights(void)
{
	int i, h, v;
	vertex_t *v1, *v2;
	float off[2];
	side_t *sid;
	mobj_t *mo;

	if(ArgCheck("-nostatic")) return;
	if(gamemode != commercial) return;

	// Check for BSTONE3 or BRICKLIT and spawn some lights.
	// - double-sided walls are skipped
	// - lights spawned only on middle textures
	// - horizontal scrolling not taken into account
	// - mobj->movedir is additional offset (many lights at the same (x,y))
	// - each tic, the Z positions are recalculated (texoff + fixed height)
	if(gamemode == commercial)
	{
		int tx1 = R_TextureNumForName("BSTONE3"),
			tx2 = R_TextureNumForName("BRICKLIT");
		fixed_t len;
		int ilen, lh;
		for(i=0; i<numlines; i++)
		{
			// We won't process fake walls.
			if(lines[i].frontsector && lines[i].backsector) continue;

			v1 = lines[i].v1;
			v2 = lines[i].v2;
			len = P_ApproxDistance(v2->x - v1->x, v2->y - v1->y);
			ilen = len >> FRACBITS;
			if(!len) continue;
			off[0] = FixedDiv(v2->y - v1->y, len);
			off[1] = -FixedDiv(v2->x - v1->x, len);

			// Let's see which sidedef is present.
			if(lines[i].sidenum[0] >= 0)
				sid = sides + lines[i].sidenum[0];
			else 
			{
				// There MUST be one sdef...
				sid = sides + lines[i].sidenum[1];
				// Flip vertices, this is the backside.
				v1 = lines[i].v2;
				v2 = lines[i].v1;
			}

			// Either will do; the lights are at the same height in both.			
			if(sid->midtexture == tx1 || sid->midtexture == tx2)
			{
				lh = (sid->sector->ceilingheight - sid->sector->floorheight)
					>> FRACBITS;
				if(!lh) continue;
				// Let's see where the bottom left light is.
				h = 33-(sid->textureoffset>>FRACBITS);
				while(h < 0) h += 64;
				while(h > 64) h -= 64;
				for(; h < ilen; h += 64)
				{
					v = 40-(sid->rowoffset>>FRACBITS);
					if(lines[i].flags & ML_DONTPEGBOTTOM) v -= 128 - lh;
					while(v < 0) v += 128;
					while(v > 128) v -= 128;
					for(; v <= lh; v += 128)
					{
						// Let there be light.
						mo = P_SpawnMobj(v1->x + off[0] +
							FixedMul(v2->x - v1->x, FixedDiv(h*FRACUNIT, len)),
							v1->y + off[1] +
							FixedMul(v2->y - v1->y, FixedDiv(h*FRACUNIT, len)),
							0,
							MT_LIGHTSOURCE);
						mo->z = sid->sector->ceilingheight - v*FRACUNIT;
						// The light must know where it's attached to.
						// Positive when based on the floor.
						if(lines[i].flags & ML_DONTPEGBOTTOM)
							mo->movedir = mo->z - sid->sector->floorheight;
						else
							mo->movedir = -v*FRACUNIT;
					}				
				}
			}
		}
	}
}

//===========================================================================
// P_SetupLevel
//===========================================================================
void P_SetupLevel(int episode, int map, int playermask, skill_t skill)
{
    int		i;
    char	lumpname[9], gl_lumpname[9], *lname, *lauthor;
    int		lumpnum, gl_lumpnum;
	int		setupflags = DDSLF_POLYGONIZE | DDSLF_FIX_SKY | DDSLF_REVERB;
	
    totalkills = totalitems = totalsecret = wminfo.maxfrags = 0;
    wminfo.partime = 180;
    for (i=0 ; i<MAXPLAYERS ; i++)
    {
		players[i].killcount = players[i].secretcount 
			= players[i].itemcount = 0;
    }
	
    // Initial height of PointOfView
    // will be set by player think.
    players[consoleplayer].plr->viewz = 1; 
	
    S_LevelChange();

	Z_FreeTags(PU_LEVEL, PU_PURGELEVEL-1);
	
    P_InitThinkers();
	
	// Clear brain data. This is only used in Doom II map30, though.
	memset(braintargets, 0, sizeof(braintargets));
	numbraintargets = 0;
	braintargeton = 0;

    // if working with a devlopment map, reload it
	//    W_Reload ();			
	   
    // find map name
    if ( gamemode == commercial)
    {
		if (map<10)
		{
			sprintf(lumpname, "map0%i", map);
			sprintf(gl_lumpname, "gl_map0%i", map);
		}
		else
		{
			sprintf (lumpname,"map%i", map);
			sprintf (gl_lumpname, "gl_map%i", map);
		}
    }
    else
    {
		lumpname[0] = 'E';
		lumpname[1] = '0' + episode;
		lumpname[2] = 'M';
		lumpname[3] = '0' + map;
		lumpname[4] = 0;
		sprintf(gl_lumpname, "GL_E%iM%i", episode, map);
    }
	Con_Message("SetupLevel: %s", lumpname);
    
	leveltime = 0;
	actual_leveltime = 0;
	
    lumpnum = W_GetNumForName (lumpname);
	gl_lumpnum = W_CheckNumForName(gl_lumpname);

	P_LoadBlockMap(lumpnum + ML_BLOCKMAP);

    // note: most of this ordering is important	
	if(gl_lumpnum > lumpnum)
	{
		Con_Message(" (GL data found)\n");
		setupflags |= DDSLF_DONT_CLIP;
		// We have GL nodes! Let's load them in.
		P_LoadVertexes (lumpnum+ML_VERTEXES, gl_lumpnum+1);				
		P_LoadSectors (lumpnum+ML_SECTORS);
		P_LoadSideDefs (lumpnum+ML_SIDEDEFS);
		P_LoadLineDefs (lumpnum+ML_LINEDEFS);
		P_LoadSubsectors (gl_lumpnum+3);
		P_LoadNodes (gl_lumpnum+4);
		P_LoadSegsGL (gl_lumpnum+2);
	}
	else
	{
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
	R_SetupLevel(lumpname, DDSLF_INIT_LINKS);

	P_LoadReject(lumpnum + ML_REJECT);
    P_GroupLines();

	// It's imperative that this is called!
	// - dlBlockLinks initialized
	// - necessary GL data generated
	// - sky fix
	// - map info setup
	R_SetupLevel(lumpname, setupflags);

    bodyqueslot = 0;
    deathmatch_p = deathmatchstarts;
	playerstart_p = playerstarts;
    P_LoadThings(lumpnum + ML_THINGS);

	P_DealPlayerStarts();
	P_SpawnPlayers();
	
    // clear special respawning que
    iquehead = iquetail = 0;		
	
    // set up world state
    P_SpawnSpecials ();
	
    // build subsector connect matrix
    //	UNUSED P_ConnectSubsectors ();
	
    // preload graphics
    if(precache) R_PrecacheLevel();
	
	// Print some info. These also appear on the screen.
	lname = (char*) Get(DD_MAP_NAME);
	lauthor = (char*) Get(DD_MAP_AUTHOR);
	// Plutonia and TNT are special cases.
	if(gamemission == pack_plut)
	{
		lname = mapnamesp[map - 1];
		lauthor = PLUT_AUTHOR;
	}
	else if(gamemission == pack_tnt)
	{
		lname = mapnamest[map - 1];
		lauthor = TNT_AUTHOR;
	}
	if(lname || lauthor)
	{
		Con_Printf("\n");
		if(lname) Con_FPrintf(CBLF_LIGHT|CBLF_BLUE, "%s\n", lname);
		if(lauthor) Con_FPrintf(CBLF_LIGHT|CBLF_BLUE, "Author: %s\n", lauthor);
		Con_Printf("\n");
	}

	// How about some music? 
	S_LevelMusic();

	// Spawn some static (dynamic) lights (based on textures).
	P_SpawnStaticLights();
	
	// Adjust slime lower wall textures (a hack!).
	// This will hide the ugly green bright line that would otherwise be
	// visible due to texture repeating and interpolation.
	for(i = 0; i < numlines; i++)
	{
		int k;
		lumpnum = R_TextureNumForName("NUKE24");
		for(k = 0; k < 2; k++)
			if(lines[i].sidenum[k] >= 0)
			{
				side_t *sdef = &sides[lines[i].sidenum[k]];
				if(sdef->bottomtexture == lumpnum && !sdef->midtexture)
					sdef->rowoffset += FRACUNIT;					
			}
	}

	// Someone may want to do something special now that the level has been
	// fully set up.
	R_SetupLevel(lumpname, DDSLF_FINALIZE);
}

//===========================================================================
// P_Init
//===========================================================================
void P_Init (void)
{
    P_InitSwitchList ();
    P_InitPicAnims ();

	// Maximum health and armor points.
	maxhealth = 100;
	healthlimit = 200;
	armorpoints[0] = 100;
	armorpoints[1] = 200;
	GetDefInt("Player|Max Health", &maxhealth);
	GetDefInt("Player|Health Limit", &healthlimit);
	GetDefInt("Player|Green Armor", &armorpoints[0]);
	GetDefInt("Player|Blue Armor", &armorpoints[1]);
}



