
//**************************************************************************
//**
//** R_WORLD.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_audio.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void R_PrepareSubsector(subsector_t *sub);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

char currentLevelId[64];
int leveltic;					// Restarts when a new map is set up.

sectorinfo_t *secinfo;
lineinfo_t *lineinfo;
vertexowner_t *vertexowners;
nodeindex_t *linelinks;			// indices to roots

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

#ifdef _MSC_VER
#pragma optimize("g", off)
#endif

//===========================================================================
// R_IsValidLink
//	We mustn't create links which form loops. This will start looking
//	from destlink, and if it finds startsec we're in trouble.
//===========================================================================
boolean R_IsValidLink(sector_t *startsec, sector_t *destlink,
					   boolean is_floor)
{
	sector_t *sec = destlink;
	sector_t *link;
	sectorinfo_t *sin;

	for(;;)
	{
		sin = secinfo + GET_SECTOR_IDX(sec);
		// Advance to the linked sector.
		if(is_floor)
		{
			if(!sin->linkedfloor) break;
			link = sin->linkedfloor;
		}
		else 
		{
			if(!sin->linkedceiling) break;
			link = sin->linkedceiling;
		}
		// Is there an illegal linkage?
		if(sec == link || startsec == link) return false;
		sec = link;
	}
	// No problems encountered.
	return true;
}

//===========================================================================
// R_SetSectorLinks
//	Called every frame. Sector heights may change at any time 
//	without notice.
//===========================================================================
void R_SetSectorLinks(sector_t *sec)
{
	int i = GET_SECTOR_IDX(sec), k;
	sector_t *back;
	boolean hackfloor, hackceil;
	side_t *sid, *frontsid, *backsid;
	sector_t *floorlink_candidate = 0, *ceillink_candidate = 0;

	//return; //---DEBUG---

	// Must have a valid sector!
	if(!sec	|| !sec->linecount
		|| secinfo[i].permanentlink) return; // Can't touch permanent links.

	hackfloor = (sec->floorpic != skyflatnum);
	hackceil = (sec->ceilingpic != skyflatnum);
	for(k = 0; k < sec->linecount; k++)
	{
		if(!hackfloor && !hackceil) break;
		// Most sectors will fail the test below.
		if(!sec->lines[k]->frontsector
			|| !sec->lines[k]->backsector) return;
		// Check which way the line is facing.		
		sid = SIDE_PTR(sec->lines[k]->sidenum[0]);
		if(sid->sector == sec)
		{
			frontsid = sid;
			backsid = SIDE_PTR(sec->lines[k]->sidenum[1]);
		}
		else
		{
			frontsid = SIDE_PTR(sec->lines[k]->sidenum[1]);
			backsid = sid;
		}
		back = backsid->sector;
		if(back == sec) return; 
		// Check that there is something on the other side.
		if(back->ceilingheight == back->floorheight) return;
		// Check the conditions that prevent the invis plane.
		if(back->floorheight == sec->floorheight)
			hackfloor = false;
		else
		{
			if(back->floorheight > sec->floorheight)
				sid = frontsid;
			else
				sid = backsid;
			if(sid->bottomtexture || sid->midtexture) 
				hackfloor = false;
			else if(R_IsValidLink(sec, back, true))
				floorlink_candidate = back;
		}
		if(back->ceilingheight == sec->ceilingheight)
			hackceil = false;
		else 
		{
			if(back->ceilingheight < sec->ceilingheight)
				sid = frontsid;
			else
				sid = backsid;
			if(sid->toptexture || sid->midtexture) 
				hackceil = false;
			else if(R_IsValidLink(sec, back, false))
				ceillink_candidate = back;
		}
	}
	if(hackfloor) 
	{
		secinfo[i].linkedfloor = floorlink_candidate;
		
/*		if(floorlink_candidate)
			Con_Printf("LF:%i->%i\n",
				i, GET_SECTOR_IDX(floorlink_candidate));*/
	}
	if(hackceil) 
	{
		secinfo[i].linkedceiling = ceillink_candidate;
		
/*		if(ceillink_candidate)
			Con_Printf("LC:%i->%i\n",
				i, GET_SECTOR_IDX(ceillink_candidate));*/
	}
}

#ifdef _MSC_VER
#pragma optimize("", on)
#endif

//==========================================================================
// edgeClipper
//	Returns a pointer to the list of points. It must be used.
//==========================================================================
fvertex_t *edgeClipper
	(int *numpoints, fvertex_t *points, int numclippers, fdivline_t *clippers)
{
	unsigned char	sidelist[MAX_POLY_SIDES];
	int				i, k, num = *numpoints;

	// We'll clip the polygon with each of the divlines. The left side of
	// each divline is discarded.
	for(i=0; i<numclippers; i++)
	{
		fdivline_t *curclip = clippers+i;

		// First we'll determine the side of each vertex. Points are allowed
		// to be on the line.
		for(k=0; k<num; k++)
		{
			sidelist[k] = P_FloatPointOnLineSide(points+k, curclip);
		}
		
		for(k=0; k<num; k++)
		{
			int startIdx = k, endIdx = k+1;

			// Check the end index.
			if(endIdx == num) endIdx = 0;	// Wrap-around.

			// Clipping will happen when the ends are on different sides.
			if(sidelist[startIdx] != sidelist[endIdx])
			{
				fvertex_t newvert;
				// Find the intersection point of intersecting lines.
				P_FloatInterceptVertex(points+startIdx, points+endIdx,
					curclip, &newvert);

				// Add the new vertex. Also modify the sidelist.
				points = (fvertex_t*)realloc(points,(++num)*sizeof(fvertex_t));
				if(num >= MAX_POLY_SIDES) 
					Con_Error("Too many points in clipper.\n");

				// Make room for the new vertex.
				memmove(points+endIdx+1, points+endIdx, 
					(num - endIdx-1)*sizeof(fvertex_t));
				memcpy(points+endIdx, &newvert, sizeof(newvert));

				memmove(sidelist+endIdx+1, sidelist+endIdx, num-endIdx-1);
				sidelist[endIdx] = 1;
				
				// Skip over the new vertex.
				k++;
			}
		}
		
		// Now we must discard the points that are on the wrong side.
		for(k=0; k<num; k++)
			if(!sidelist[k])
			{
				memmove(points+k, points+k+1, (num - k-1)*sizeof(fvertex_t));
				memmove(sidelist+k, sidelist+k+1, num - k-1);
				num--;
				k--;
			}
	}
	// Screen out consecutive identical points.
	for(i=0; i<num; i++)
	{
		int previdx = i-1;
		if(previdx < 0) previdx = num - 1;
		if(points[i].x == points[previdx].x 
			&& points[i].y == points[previdx].y)
		{
			// This point (i) must be removed.
			memmove(points+i, points+i+1, sizeof(fvertex_t)*(num-i-1));
			num--;
			i--;
		}
	}
	*numpoints = num;
	return points;
}

//==========================================================================
//
// R_ConvexClipper
//
//==========================================================================

void R_ConvexClipper(subsector_t *ssec, int num, divline_t *list)
{
	int			numclippers = num+ssec->linecount;
	fdivline_t	*clippers = (fdivline_t*) Z_Malloc(numclippers*sizeof(fdivline_t), PU_STATIC, 0);
	int			i, numedgepoints;
	fvertex_t	*edgepoints;
	
	// Convert the divlines to float, in reverse order.
	for(i=0; i<numclippers; i++)
	{
		if(i<num)
		{
			clippers[i].x = FIX2FLT(list[num-i-1].x);
			clippers[i].y = FIX2FLT(list[num-i-1].y);
			clippers[i].dx = FIX2FLT(list[num-i-1].dx);
			clippers[i].dy = FIX2FLT(list[num-i-1].dy);
		}
		else
		{
			seg_t *seg = SEG_PTR(ssec->firstline+i-num);
			clippers[i].x = FIX2FLT(seg->v1->x);
			clippers[i].y = FIX2FLT(seg->v1->y);
			clippers[i].dx = FIX2FLT(seg->v2->x - seg->v1->x);
			clippers[i].dy = FIX2FLT(seg->v2->y - seg->v1->y);
		}
	}

	// Setup the 'worldwide' polygon.
	numedgepoints = 4;
	edgepoints = (fvertex_t*) malloc(numedgepoints*sizeof(fvertex_t));
	
	edgepoints[0].x = -32768;
	edgepoints[0].y = 32768;
	
	edgepoints[1].x = 32768;
	edgepoints[1].y = 32768;
	
	edgepoints[2].x = 32768;
	edgepoints[2].y = -32768;
	
	edgepoints[3].x = -32768;
	edgepoints[3].y = -32768;

	// Do some clipping, <snip> <snip>
	edgepoints = edgeClipper(&numedgepoints, edgepoints, numclippers, clippers);

	if(!numedgepoints)
	{
		printf( "All clipped away: subsector %d\n", ((byte*)ssec-subsectors)/SUBSIZE);
		ssec->numverts = 0;
		ssec->verts = 0;
		//ssec->origverts = 0;
		//ssec->diffverts = 0;
	}
	else
	{
		// We need these with dynamic lights.
		ssec->verts = Z_Malloc(sizeof(fvertex_t)*numedgepoints, PU_LEVEL, 0);
		memcpy(ssec->verts, edgepoints, sizeof(fvertex_t)*numedgepoints);
		ssec->numverts = numedgepoints;

		R_PrepareSubsector(ssec);
	}

	// We're done, free the edgepoints memory.
	free(edgepoints);
	Z_Free(clippers);
}

void R_PrepareSubsector(subsector_t *sub)
{
	int j, num = sub->numverts;

	// Find the center point. First calculate the bounding box.
	sub->bbox[0].x = sub->bbox[1].x = sub->verts[0].x;
	sub->bbox[0].y = sub->bbox[1].y = sub->verts[0].y;
	sub->midpoint.x = sub->verts[0].x;
	sub->midpoint.y = sub->verts[0].y;
	for(j=1; j<num; j++)
	{
		if(sub->verts[j].x < sub->bbox[0].x) sub->bbox[0].x = sub->verts[j].x;
		if(sub->verts[j].y < sub->bbox[0].y) sub->bbox[0].y = sub->verts[j].y;
		if(sub->verts[j].x > sub->bbox[1].x) sub->bbox[1].x = sub->verts[j].x;
		if(sub->verts[j].y > sub->bbox[1].y) sub->bbox[1].y = sub->verts[j].y;
		sub->midpoint.x += sub->verts[j].x;
		sub->midpoint.y += sub->verts[j].y;
	}
	sub->midpoint.x /= num;
	sub->midpoint.y /= num;
}

//==========================================================================
//
// R_PolygonizeWithoutCarving
//
//==========================================================================

void R_PolygonizeWithoutCarving()
{
	int			i, j;
	subsector_t *sub;
		
	for(i=0; i<numsubsectors; i++)
	{
		sub = SUBSECTOR_PTR(i);
		sub->numverts = sub->linecount;
		sub->verts = (fvertex_t*) Z_Malloc(sizeof(fvertex_t) * sub->linecount, PU_LEVEL, 0);
		for(j=0; j<sub->linecount; j++)
		{
			sub->verts[j].x = FIX2FLT(SEG_PTR(sub->firstline + j)->v1->x);
			sub->verts[j].y = FIX2FLT(SEG_PTR(sub->firstline + j)->v1->y);
		}
		R_PrepareSubsector(sub);
	}
}

//==========================================================================
// R_CreateFloorsAndCeilings
//	Recursively polygonizes all ceilings and floors.
//==========================================================================
void R_CreateFloorsAndCeilings(int bspnode, int numdivlines, divline_t *divlines)
{
	node_t		*nod;
	divline_t	*childlist, *dl;
	int			childlistsize = numdivlines+1;
	
	// If this is a subsector we are dealing with, begin carving with the
	// given list.
	if(bspnode & NF_SUBSECTOR)
	{
		// We have arrived at a subsector. The divline list contains all
		// the partition lines that carve out the subsector.
		int ssidx = bspnode & (~NF_SUBSECTOR);
		R_ConvexClipper(SUBSECTOR_PTR(ssidx), numdivlines, divlines);
		// This leaf is done.
		return;
	}

	// Get a pointer to the node.
	nod = NODE_PTR(bspnode);

	// Allocate a new list for each child.
	childlist = (divline_t*)malloc(childlistsize*sizeof(divline_t));

	// Copy the previous lines, from the parent nodes.
	if(divlines) memcpy(childlist,divlines,numdivlines*sizeof(divline_t));

	dl = childlist + numdivlines;
	dl->x = nod->x;
	dl->y = nod->y;
	// The right child gets the original line (LEFT side clipped).
	dl->dx = nod->dx;
	dl->dy = nod->dy;
	R_CreateFloorsAndCeilings(nod->children[0],childlistsize,childlist);

	// The left side. We must reverse the line, otherwise the wrong
	// side would get clipped.
	dl->dx = -nod->dx;
	dl->dy = -nod->dy;
	R_CreateFloorsAndCeilings(nod->children[1],childlistsize,childlist);

	// We are finishing with this node, free the allocated list.
	free(childlist);
}

//==========================================================================
// R_SkyFix
//	Fixing the sky means that for adjacent sky sectors the lower sky 
//	ceiling is lifted to match the upper sky. The raising only affects 
//	rendering, it has no bearing on gameplay.
//==========================================================================
void R_SkyFix(void)
{
	boolean		adjusted;
	int			i, f, b;

	// Check all things first.
	for(i = 0; i < numsectors; i++)
	{
		sector_t *sec = SECTOR_PTR(i);
		mobj_t *it;
		// Must have a sky ceiling.
		if(sec->ceilingpic != skyflatnum) continue;
		// Check that all the things in the sector fit in.
		for(it = sec->thinglist; it; it = it->snext)
		{
			b = it->height >> FRACBITS;
			f = (sec->ceilingheight >> FRACBITS) + sec->skyfix
				- (sec->floorheight >> FRACBITS);
			if(b > f)
			{
				// Must increase skyfix.
				sec->skyfix += b - f;
				if(verbose)
				{
					Con_Printf("S%i: (mo)skyfix to %i (ceil=%i)\n",
						GET_SECTOR_IDX(sec), sec->skyfix,
						(sec->ceilingheight>>FRACBITS)+sec->skyfix);
				}
			}
		}
	}

	// We'll do this as long as we must to be sure all the sectors are fixed.
	do 
	{
		adjusted = false;

		// We need to check all the linedefs.
		for(i = 0; i < numlines; i++)
		{
			line_t *line = LINE_PTR(i);
			sector_t *front = line->frontsector, *back = line->backsector;
			int fix = 0;

			// The conditions: must have two sides.
			if(!front || !back) continue;

			// Both the front and back sectors must have the sky ceiling.
			if(front->ceilingpic != skyflatnum 
				|| back->ceilingpic != skyflatnum) continue;

			f = (front->ceilingheight >> FRACBITS) + front->skyfix;
			b = (back->ceilingheight >> FRACBITS) + back->skyfix;
			if(f < b)
			{
				fix = b - (front->ceilingheight>>FRACBITS);
				if(fix > front->skyfix)
				{
					front->skyfix = fix;
					if(verbose)
					{
						Con_Printf("S%i: skyfix to %i (ceil=%i)\n",
							GET_SECTOR_IDX(front), front->skyfix,
							(front->ceilingheight>>FRACBITS)+front->skyfix);
					}
					adjusted = true;
				}
			}
			else if(f > b)
			{
				fix = f - (back->ceilingheight>>FRACBITS);
				if(fix > back->skyfix)
				{
					back->skyfix = fix;
					if(verbose)
					{
						Con_Printf("S%i: skyfix to %i (ceil=%i)\n",
							GET_SECTOR_IDX(back), back->skyfix,
							(back->ceilingheight>>FRACBITS)+back->skyfix);
					}
					adjusted = true;
				}
			}
		}
	}
	while(adjusted);
}

static float TriangleArea(fvertex_t *o, fvertex_t *s, fvertex_t *t)
{
	fvertex_t a = { s->x - o->x, s->y - o->y }, 
		b = { t->x - o->x, t->y - o->y };
	float area = (a.x*b.y - b.x*a.y) / 2;
	if(area < 0) area = -area;
	return area;
}

//===========================================================================
// R_TestTriFan
//	Returns true if 'base' is a good tri-fan base.
//===========================================================================
int R_TestTriFan(subsector_t *sub, int base)
{
#define TRIFAN_LIMIT	0.1
	int i, a, b;

	if(sub->numverts == 3) return true;	// They're all valid.
	// Higher vertex counts need checking.
	for(i=0; i<sub->numverts-2; i++)
	{
		a = base + 1 + i;
		b = a + 1;
		if(a >= sub->numverts) a -= sub->numverts;
		if(b >= sub->numverts) b -= sub->numverts;
		if(TriangleArea(sub->verts + base, sub->verts + a, 
			sub->verts + b) <= TRIFAN_LIMIT) return false;
	}
	// Whole triangle fan checked out OK, must be good.
	return true;
}

//===========================================================================
// R_SubsectorPlanes
//===========================================================================
void R_SubsectorPlanes(void)
{
	int	i, k, num;
	subsector_t *sub;
	fvertex_t buf[RL_MAX_POLY_SIDES];
	
	for(i=0; i<numsubsectors; i++)
	{
		sub = SUBSECTOR_PTR(i);
		num = sub->numverts;
		// We need to find a good tri-fan base vertex. 
		// (One that doesn't generate zero-area triangles).
		// We'll test each one and pick the first good one.
		for(k=0; k<num; k++)
		{
			if(R_TestTriFan(sub, k))
			{
				// Yes! This'll do nicely. Change the order of the
				// vertices so that k comes first.
				if(k)	// Need to change?
				{
					memcpy(buf, sub->verts, sizeof(fvertex_t) * num);
					memcpy(sub->verts, buf + k, sizeof(fvertex_t) * (num-k));
					memcpy(sub->verts + (num-k), buf, sizeof(fvertex_t) * k);
				}
				goto ddSP_NextSubSctr;
			}
		}
		// There was no match. Bugger. We need to use the subsector
		// midpoint as the base. It's always valid.
		sub->flags |= DDSUBF_MIDPOINT;
		//Con_Message("Using midpoint for subsctr %i.\n", i);

ddSP_NextSubSctr:;
	}
}

void R_SetVertexOwner(int idx, sector_t *secptr)
{
	int i;
	vertexowner_t *own;
	unsigned short *list;
	int sector;

	if(!secptr) return;
	sector = GET_SECTOR_IDX(secptr);
	own = vertexowners + idx;
	// Has this sector been already registered?
	for(i=0; i<own->num; i++)
		if(own->list[i] == sector) return;	// Yes, we can exit.
	// Add a new owner.
	own->num++;
	// Allocate a new list.
	list = (unsigned short*) Z_Malloc(sizeof(unsigned short) * own->num, 
		PU_LEVEL, 0);
	// If there are previous references, copy them.
	if(own->num > 1)
	{
		memcpy(list, own->list, sizeof(unsigned short) * (own->num-1));
		// Free the old list.
		Z_Free(own->list);
	}
	own->list = list;	
	own->list[own->num - 1] = sector;
}

//==========================================================================
// R_InitVertexOwners
//	Generates an array of sector references for each vertex. The list 
//	includes all the sectors the vertex belongs to.
//==========================================================================
void R_InitVertexOwners(void)
{
	int i, k, p, v[2];
	byte *ptr;
	sector_t *sec;
		
	// Allocate enough memory.
	vertexowners = (vertexowner_t*) Z_Malloc(sizeof(vertexowner_t) *
		numvertexes, PU_LEVEL, 0);
	memset(vertexowners, 0, sizeof(vertexowner_t) * numvertexes);

	for(i=0, ptr=sectors; i<numsectors; i++, ptr += SECTSIZE)
	{
		sec = (sector_t*) ptr;
		// Traversing the line list will do fine.
		for(k=0; k<sec->linecount; k++)
		{
			v[0] = ( (byte*) sec->lines[k]->v1 - vertexes) / VTXSIZE;
			v[1] = ( (byte*) sec->lines[k]->v2 - vertexes) / VTXSIZE;
			for(p=0; p<2; p++)
			{
				R_SetVertexOwner(v[p], sec->lines[k]->frontsector);
				R_SetVertexOwner(v[p], sec->lines[k]->backsector);
			}
		}
	}
}

/*boolean DD_SubContainTest(sector_t *innersec, sector_t *outersec)
{
	int i, k;
	boolean contained;
	float in[4], out[4];
	subsector_t *isub, *osub;

	// Test each subsector of innersec against all subsectors of outersec.
	for(i=0; i<numsubsectors; i++)
	{
		isub = SUBSECTOR_PTR(i);
		contained = false;
		// Only accept innersec's subsectors.
		if(isub->sector != innersec) continue;
		for(k=0; k<numsubsectors && !contained; k++)
		{
			osub = SUBSECTOR_PTR(i);
			// Only accept outersec's subsectors.
			if(osub->sector != outersec) continue;
			// Test if isub is inside osub.
			if(isub->bbox[BLEFT] >= osub->bbox[BLEFT]
				&& isub->bbox[BRIGHT] <= osub->bbox[BRIGHT]
				&& isub->bbox[BTOP] >= osub->bbox[BTOP]
				&& isub->bbox[BBOTTOM] <= osub->bbox[BBOTTOM])
			{
				// This is contained.
				contained = true;
			}
		}
		if(!contained) return false;
	}
	// All tested subsectors were contained!
	return true;
}*/

//===========================================================================
// R_GetContainingSectorOf
//	The test is done on subsectors.
//===========================================================================
sector_t *R_GetContainingSectorOf(sector_t *sec)
{
	int i;
	float cdiff = -1, diff;
	float inner[4], outer[4];
	sector_t *other, *closest = NULL;
	
	memcpy(inner, secinfo[GET_SECTOR_IDX(sec)].bounds, sizeof(inner));

	// Try all sectors that fit in the bounding box.
	for(i = 0; i < numsectors; i++)
	{
		other = SECTOR_PTR(i);
		if(!other->linecount) continue;
		if(other == sec) continue;	// Don't try on self!
		memcpy(outer, secinfo[i].bounds, sizeof(outer));
		if(inner[BLEFT] >= outer[BLEFT]
			&& inner[BRIGHT] <= outer[BRIGHT]
			&& inner[BTOP] >= outer[BTOP]
			&& inner[BBOTTOM] <= outer[BBOTTOM])
		{
			// Inside! Now we must test each of the subsectors. Otherwise
			// we can't be sure...
			/*if(DD_SubContainTest(sec, other))
			{*/
				// Sec is totally and completely inside other!
				diff = M_BoundingBoxDiff(inner, outer);
				if(cdiff < 0 || diff <= cdiff)
				{
					closest = other;
					cdiff = diff;
				}
			//}
		}
	}
	return closest;
}

//===========================================================================
// R_InitSectorInfo
//===========================================================================
void R_InitSectorInfo(void)
{
	int i, k, len = sizeof(sectorinfo_t) * numsectors;
	sector_t *sec;
	line_t *lin;
	boolean dohack;

	secinfo = Z_Malloc(len, PU_LEVEL, 0);
	memset(secinfo, 0, len);

	// Calculate bounding boxes for all sectors.
	for(i = 0; i < numsectors; i++)
		P_SectorBoundingBox(SECTOR_PTR(i), secinfo[i].bounds);

	for(i = 0; i < numsectors; i++)
	{
		sec = SECTOR_PTR(i);
		if(!sec->linecount) continue;
		dohack = true;
		for(k = 0; k < sec->linecount; k++)
		{
			lin = sec->lines[k];
			if(!lin->frontsector 
				|| !lin->backsector
				|| lin->frontsector != lin->backsector)
			{
				dohack = false;
				break;
			}			
		}
		if(dohack)
		{
			// Link permanently.
			secinfo[i].permanentlink = true;
			secinfo[i].linkedceiling 
				= secinfo[i].linkedfloor
				= R_GetContainingSectorOf(sec);
			if(secinfo[i].linkedceiling)
			{
				Con_Printf("Linking S%i planes permanently to S%i\n",
					i, GET_SECTOR_IDX(secinfo[i].linkedceiling));
			}
		}
	}
}

//==========================================================================
// R_InitPolyBlockMap
//	Allocates and clears the polyobj blockmap.
//	Normal blockmap must already be initialized when this is called.
//==========================================================================
void R_InitPolyBlockMap(void)
{
	if(verbose)
	{
		Con_Message("R_InitPolyBlockMap: w=%i h=%i\n", bmapwidth, bmapheight);
	}

	polyblockmap = Z_Malloc(bmapwidth*bmapheight*sizeof(polyblock_t*),
		PU_LEVEL, 0);
	memset(polyblockmap, 0, bmapwidth*bmapheight*sizeof(polyblock_t*));
}

/*
//===========================================================================
// R_BuildSubsectorContacts
//	For each subsector, build a list of contacted segs that belong in other 
//	subsectors. This information is needed when creating occlusion planes.
//===========================================================================
void R_BuildSubsectorContacts(void)
{
	subsector_t	*sub;
	fvertex_t	*v1, *v2, segv1, segv2;
	float		edgebox[4], unitline[2], length;
	int			i, k, t;
	seg_t		*seg;
	double		starttime = Sys_GetSeconds();

	for(i = 0; i < numsubsectors; i++)
	{
		sub = SUBSECTOR_PTR(i);
		sub->contactcount = 0;
		sub->contacts = NULL;

		// Let's build a list of contacted segs. Try contacting with each
		// of the subsector's edges.
		for(k = 0; k < sub->numverts; k++)
		{
			v1 = sub->verts + k;
			v2 = sub->verts + ((k+1) % sub->numverts);
			ORDER(v1->x, v2->x, edgebox[BLEFT], edgebox[BRIGHT]);
			ORDER(v1->y, v2->y, edgebox[BTOP], edgebox[BBOTTOM]);
			length = M_ApproxDistancef(v1->x - v2->x, v1->y - v2->y);
			if(!length) continue;
			unitline[VX] = v1->x + (v2->x - v1->x)/length;
			unitline[VY] = v1->y + (v2->y - v1->y)/length;
									
			// Find all segs that overlap the line v1 -> v2. 
			for(t = 0; t < numsegs; t++)
			{
				seg = SEG_PTR(t);
				if(!seg->linedef) continue;
				if(seg->frontsector == sub->sector) continue;
				if(seg->backsector != sub->sector) continue;

				// 1) Test the bounding box of the edge line.
				segv1.x = FIX2FLT(seg->v1->x);
				segv1.y = FIX2FLT(seg->v1->y);
				segv2.x = FIX2FLT(seg->v2->x);
				segv2.y = FIX2FLT(seg->v2->y);

				if(segv1.x < edgebox[BLEFT]
					|| segv1.y < edgebox[BTOP]
					|| segv1.x > edgebox[BRIGHT]
					|| segv1.y > edgebox[BBOTTOM]) continue;
				if(segv2.x < edgebox[BLEFT]
					|| segv2.y < edgebox[BTOP]
					|| segv2.x > edgebox[BRIGHT]
					|| segv2.y > edgebox[BBOTTOM]) continue;
							
				// 2) Test the distance to the line from each point.
				if(M_PointUnitLineDistance(&v1->x, unitline, &segv1.x)
					> 0.1) continue;
				if(M_PointUnitLineDistance(&v1->x, unitline, &segv2.x)
					> 0.1) continue;

				// This is a contact.
				sub->contacts = Z_Realloc(sub->contacts, 
					sizeof(seg_t*) * ++sub->contactcount, PU_LEVEL);
				sub->contacts[sub->contactcount - 1] = seg;
			}
		}
	}
	Con_Printf("R_BuildSubsectorContacts: %lf seconds (%i subs, %i segs)\n",
		Sys_GetSeconds() - starttime, numsubsectors, numsegs);
}
*/

//===========================================================================
// R_SetupFog
//	Mapinfo must be set.
//===========================================================================
void R_SetupFog(void)
{
	int flags;

	if(!mapinfo) 
	{
		// Go with the defaults.
		Con_Execute("fog off", true);
		return;
	}

	// Check the flags.
	flags = Def_EvalFlags(mapinfo->flags);
	if(flags & MIF_FOG)
	{
		// Setup fog.
		Con_Execute("fog on", true);
		Con_Executef(true, "fog start %f", mapinfo->fog_start);
		Con_Executef(true, "fog end %f", mapinfo->fog_end);
		Con_Executef(true, "fog density %f", mapinfo->fog_density);
		Con_Executef(true, "fog color %.0f %.0f %.0f", 
			mapinfo->fog_color[0]*255,
			mapinfo->fog_color[1]*255, 
			mapinfo->fog_color[2]*255);
	}
	else
	{
		Con_Execute("fog off", true);
	}
}

//===========================================================================
// R_SetupSky
//	Mapinfo must be set.
//===========================================================================
void R_SetupSky(void)
{
	int i, k;

	if(!mapinfo)
	{
		// Use the defaults.
		Rend_SkyParams(DD_SKY, DD_HEIGHT, .666667f);
		Rend_SkyParams(DD_SKY, DD_HORIZON, 0);
		Rend_SkyParams(0, DD_ENABLE, 0);
		Rend_SkyParams(0, DD_TEXTURE, R_TextureNumForName("SKY1"));
		Rend_SkyParams(0, DD_MASK, DD_NO);
		Rend_SkyParams(0, DD_OFFSET, 0);
		Rend_SkyParams(1, DD_DISABLE, 0);
		return;
	}

	Rend_SkyParams(DD_SKY, DD_HEIGHT, mapinfo->sky_height);
	Rend_SkyParams(DD_SKY, DD_HORIZON, mapinfo->horizon_offset);
	for(i = 0; i < 2; i++)
	{
		k = Def_EvalFlags(mapinfo->sky_layers[i].flags);
		if(k & SLF_ENABLED)
		{
			Rend_SkyParams(i, DD_ENABLE, 0);
			Rend_SkyParams(i, DD_TEXTURE, R_TextureNumForName(mapinfo->sky_layers[i].texture));
			Rend_SkyParams(i, DD_MASK, k & SLF_MASKED? DD_YES : DD_NO);
			Rend_SkyParams(i, DD_OFFSET, mapinfo->sky_layers[i].offset);
			Rend_SkyParams(i, DD_COLOR_LIMIT, mapinfo->sky_layers[i].color_limit);
		}
		else
		{
			Rend_SkyParams(i, DD_DISABLE, 0);
		}
	}
}

//===========================================================================
// R_InitLines
//	Calculate accurate lengths for all lines.
//===========================================================================
void R_InitLineInfo(void)
{
	line_t *line;
	int	i;

	// Allocate memory for the line info.
	lineinfo = Z_Malloc(sizeof(lineinfo_t) * numlines, PU_LEVEL, NULL);

	for(i = 0; i < numlines; i++)
	{
		line = LINE_PTR(i);
		lineinfo[i].length = P_AccurateDistance(line->dx, line->dy);
	}
}

//==========================================================================
// R_SetupLevel
//	This routine is called from the game dll to polygonize the current level.
//	Creates floors and ceilings and fixes the adjacent sky sector heights.
//	Creates a big enough dlBlockLinks.
//	Reads mapinfo and does the necessary setup.
//==========================================================================
void R_SetupLevel(char *level_id, int flags)
{
	int i;

	// First check for some special tasks.
	if(flags & DDSLF_INIT_LINKS)
	{
		// Init polyobj blockmap.
		R_InitPolyBlockMap();

		// Initialize node piles and line rings.
		NP_Init(&thingnodes, 256); // Allocate a small pile.
		NP_Init(&linenodes, numlines + 1000);
		// Allocate the rings.
		linelinks = Z_Malloc(sizeof(*linelinks) * numlines, PU_LEVEL, 0);
		for(i = 0; i < numlines; i++)
			linelinks[i] = NP_New(&linenodes, NP_ROOT_NODE);
		return;	// Do nothing else.
	}
	if(flags & DDSLF_SERVER_ONLY)
	{
		// Only init server data.
		Sv_InitPools();
		return;
	}
	if(flags & DDSLF_AFTER_LOADING)
	{
		// Loading a game usually destroys all thinkers. Until a proper
		// savegame system handled by the engine is introduced we'll have
		// to resort to re-initializing the most important stuff.
		P_SpawnTypeParticleGens();
		return;
	}
	if(flags & DDSLF_FINALIZE)
	{
		// The level setup has been completed.
		// Run the special level setup command, which the user may alias to 
		// do something useful.
		if(level_id && level_id[0])
		{
			char cmd[80];
			sprintf(cmd, "init-%s", level_id);
			if(Con_IsValidCommand(cmd))
			{
				Con_Executef(false, cmd);
			}
		}
		
		// Now that the setup is done, let's reset the tictimer so it'll
		// appear that no time has passed during the setup.
		Net_ResetTimer();

		// Kill all local commands.
		for(i = 0; i < MAXPLAYERS; i++)
		{
			clients[i].numtics = 0;
		}

		// Reset the level tick timer.
		leveltic = 0;
		return;
	}

	if(isServer)
	{
		// Whenever the map changes, remote players must tell us when 
		// they're ready to begin receiving frames.
		for(i = 0; i < MAXPLAYERS; i++)
		{
			if(!(players[i].flags & DDPF_LOCAL) && clients[i].connected)
			{
#ifdef _DEBUG
				Con_Printf("Cl%i NOT READY ANY MORE!\n", i);
#endif
				clients[i].ready = false;
			}
		}
	}

	Con_InitProgress("Setting up level...", 100);
	strcpy(currentLevelId, level_id);

	// First compose the vertex owners array.
	R_InitVertexOwners();
	Con_Progress(10, 0);

	// Polygonize.
	if(flags & DDSLF_POLYGONIZE)
	{
		if(flags & DDSLF_DONT_CLIP)
			R_PolygonizeWithoutCarving();
		else
			R_CreateFloorsAndCeilings(numnodes-1, 0, NULL);
	}
	Con_Progress(10, 0);

	// Init Particle Generator links.
	PG_InitForLevel();

	// Make sure subsector floors and ceilings will be rendered correctly.
	R_SubsectorPlanes();
	R_InitSectorInfo();
	R_InitLineInfo();
	Con_Progress(10, 0);

	if(flags & DDSLF_FIX_SKY) R_SkyFix();
	if(flags & DDSLF_REVERB) S_CalcSectorReverbs();
	
	DL_InitBlockMap();	
	Cl_Reset();
	RL_DeleteLists();
	GL_DeleteRawImages();
	Con_Progress(10, 0);

	// See what mapinfo says about this level.
	mapinfo = Def_GetMapInfo(level_id);
	if(!mapinfo) mapinfo = Def_GetMapInfo("*");
	// Setup accordingly.
	R_SetupFog();
	R_SetupSky();
	if(mapinfo)
	{
		mapgravity = mapinfo->gravity * FRACUNIT;
		r_ambient = mapinfo->ambient * 255;
	}
	else
	{
		// No map info found, set some basic stuff.
		mapgravity = FRACUNIT;
		r_ambient = 0;
	}

	// Invalidate old cmds.
	if(isServer)
	{
		for(i = 0; i < MAXPLAYERS; i++)
			if(players[i].ingame) clients[i].runTime = gametic;
	}

	// Set target heights of all planes.
	for(i = 0; i < numsectors; i++)
	{
		sector_t *sec = SECTOR_PTR(i);
		sec->planes[PLN_FLOOR].target = sec->floorheight;
		sec->planes[PLN_CEILING].target = sec->ceilingheight;
	}

	// Spawn all type-triggered particle generators.
	// Let's hope there aren't too many...
	P_SpawnTypeParticleGens();
	P_SpawnMapParticleGens(level_id);

	// Make sure that the next frame doesn't use a filtered viewer.
	R_ResetViewer();

	// Do network init: Initialize Delta Pools.
	if(!(flags & DDSLF_NO_SERVER)) 
	{
		Sv_InitPools();
	}

	Con_Progress(10, 0); // 50%.
}

//===========================================================================
// R_ClearSectorFlags
//===========================================================================
void R_ClearSectorFlags(void)
{
	int	i;
	sectorinfo_t *sinf;

	for(i = 0, sinf = secinfo; i < numsectors; i++, sinf++)
	{
		// Clear all flags that can be cleared before each frame.
		sinf->flags &= ~SIF_FRAME_CLEAR;
	}
}

//===========================================================================
// R_GetLinkedSector
//===========================================================================
sector_t *R_GetLinkedSector(sector_t *startsec, boolean getfloor)
{
	sector_t *sec = startsec;
	sector_t *link;
	sectorinfo_t *sin;

	for(;;)
	{
		sin = secinfo + GET_SECTOR_IDX(sec);
		if(getfloor)
		{
			if(!sin->linkedfloor) return sec;
			link = sin->linkedfloor;
		}
		else 
		{
			if(!sin->linkedceiling) return sec;
			link = sin->linkedceiling;
		}
#if _DEBUG
		if(sec == link || startsec == link) 
		{
			Con_Error("R_GetLinkedSector: linked to self! (%s)\n", getfloor? "flr" : "ceil");
			return startsec;
		}
#endif
		sec = link;
	}
}

//===========================================================================
// R_UpdatePlanes
//	All links will be updated every frame (sectorheights may change
//	at any time without notice).
//===========================================================================
void R_UpdatePlanes(void)
{
	int i;
	sector_t *sec;
	sectorinfo_t *sin;

	// Clear all non-permanent sector links.
	for(i = 0, sin = secinfo; i < numsectors; i++, sin++)
	{
		if(sin->permanentlink) continue;
		sin->linkedfloor = sin->linkedceiling = NULL;
	}

	// Assign new links.
	for(i = 0, sin = secinfo; i < numsectors; i++, sin++)
	{
		sec = SECTOR_PTR(i);
		R_SetSectorLinks(sec);
		// Floor height.
		if(!sin->linkedfloor) 
		{
			sin->visfloor = FIX2FLT(sec->floorheight)
				+ sin->visflooroffset;
		}
		else
		{
			sin->visfloor = FIX2FLT(R_GetLinkedSector
				(sin->linkedfloor, true)->floorheight);
		}
		

		// Ceiling height.
		if(!sin->linkedceiling)
		{
			sin->visceiling = FIX2FLT(sec->ceilingheight)
				+ sin->visceilingoffset;
		}
		else
		{
			sin->visceiling = FIX2FLT(R_GetLinkedSector
				(sin->linkedceiling, false)->ceilingheight);
		}		
	}
}

//===========================================================================
// R_GetCurrentLevelID
//===========================================================================
const char *R_GetCurrentLevelID(void)
{
	return currentLevelId;
}
