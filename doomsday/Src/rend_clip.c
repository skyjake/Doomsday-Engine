
//**************************************************************************
//**
//** REND_CLIP.C
//**
//** Clipnodes and oranges.
//**
//** The idea is to keep track of occluded angles around the camera.
//** Since subsectors are rendered front-to-back, the occlusion lists
//** start a frame empty and eventually fill up to cover the whole 360 
//** degrees around the camera.
//**
//** Oranges (occlusion ranges) clip a half-space on an angle range.
//** These are produced by horizontal edges that have empty space behind.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define MAXCLIPNODES	128		// We can have this many nodes at once.
#define DEFOCCNODES		2048	// Hard-coded limits suck...

// Occlusion node flags.
#define OCNF_USED		0x1
#define OCNF_TOPHALF	0x2		// Otherwise bottom half.

// TYPES -------------------------------------------------------------------

typedef struct occnode_s {
	struct occnode_s *next, *prev;
	byte		flags;			// Used and tophalf.
	binangle_t	start, end;		// Start and end angles of the segment (s<e).
	float		normal[3];		// The normal of the occlusion plane.
} occnode_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

int		C_IsAngleVisible(binangle_t bang);
void	C_CutOcclusionRange(binangle_t startAngle, binangle_t endAngle);
static int C_SafeCheckRange(binangle_t startAngle, binangle_t endAngle);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

clipnode_t	*clipnodes;		// The list of clipnodes.
clipnode_t	*cliphead;		// The head node.
int			maxnumnodes=0;		

int			maxOccNodes = DEFOCCNODES; 
occnode_t	*occnodes;		// The list of occlusion nodes.
occnode_t	*occhead;		// The head occlusion node.
occnode_t	*orangeCursor;
int			highOccNodes;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static binangle_t anglist[MAX_POLY_SIDES];

// CODE --------------------------------------------------------------------

static void C_CountNodes()
{
	int	i;
	clipnode_t *ci;
	for(i=0, ci=cliphead; ci; i++, ci=ci->next);
	if(i > maxnumnodes) maxnumnodes = i;
}

static int C_CountUsedOranges(void)
{
	int cnt;
	occnode_t *orange;
	for(cnt = 0, orange = occhead; orange; orange = orange->next, cnt++);
	return cnt;
}

static void C_CountOranges(void)
{
	int i = C_CountUsedOranges();
	if(i > highOccNodes) highOccNodes = i;
}

//===========================================================================
// C_Init
//===========================================================================
void C_Init(void)
{
	if(ArgCheckWith("-maxor", 1)) // Maximum number of occlusion ranges.
	{
		maxOccNodes = atoi(ArgNext());
		if(maxOccNodes < DEFOCCNODES) maxOccNodes = DEFOCCNODES;
	}

	clipnodes = Z_Calloc(sizeof(clipnode_t)*MAXCLIPNODES, PU_STATIC, 0);
	occnodes = Z_Calloc(sizeof(occnode_t)*maxOccNodes, PU_STATIC, 0);
	orangeCursor = occnodes;
}

//===========================================================================
// C_ClearRanges
//===========================================================================
void C_ClearRanges(void)
{
	memset(clipnodes, 0, sizeof(clipnode_t)*MAXCLIPNODES);
	cliphead = 0;

/*#if _DEBUG
	{
		int left = C_CountUsedOranges();
		Con_Printf("max oranges=%i (%i leftovers)\n", highOccNodes, left);
		highOccNodes = 0;
	}
#endif*/

	memset(occnodes, 0, sizeof(occnode_t)*maxOccNodes);
	orangeCursor = occnodes;
	occhead = 0;
}

//===========================================================================
// C_NewRange
//	Finds the first unused clip node.
//===========================================================================
static clipnode_t *C_NewRange(binangle_t stAng, binangle_t endAng)	
{
	int			i;
	clipnode_t	*cnode;

	for(i = 0; i < MAXCLIPNODES; i++) if(!clipnodes[i].used) break;
	if(i == MAXCLIPNODES) 
		Con_Error("C_NewRange: Out of clipnodes (max %d).\n", MAXCLIPNODES);
	// Initialize the node.
	cnode = clipnodes+i;
	cnode->used = 1;
	cnode->start = stAng;
	cnode->end = endAng;
	cnode->prev = cnode->next = NULL;
	return cnode;
}

//===========================================================================
// C_RemoveRange
//===========================================================================
static void C_RemoveRange(clipnode_t *crange)
{
#if _DEBUG
	if(!crange->used) Con_Error("Tried to remove an unused range.\n");
#endif

	// If this is the head, move it.
	if(cliphead == crange) cliphead = crange->next;

	crange->used = 0;
	if(crange->prev) crange->prev->next = crange->next;
	if(crange->next) crange->next->prev = crange->prev;
	crange->prev = crange->next = 0;
}

//===========================================================================
// C_AddRange
//===========================================================================
static void C_AddRange(binangle_t startAngle, binangle_t endAngle)
{
	clipnode_t	*ci, *crange;

	//printf( "=== C_AddRange(%x,%x):\n",startAngle,endAngle);

	/*if(startAngle == endAngle)
	{
		//printf( "  range has no length, skipping\n");
		return;
	}*/

	// This range becomes a solid segment: cut everything away from the 
	// corresponding occlusion range.
	C_CutOcclusionRange(startAngle, endAngle);

	// If there is no head, this will be the first range.
	if(!cliphead)
	{
		cliphead = C_NewRange(startAngle, endAngle);
		//printf( "  new head node added, %x => %x\n", cliphead->start, cliphead->end);
		return;
	}

	// There are previous ranges. Check that the new range isn't contained
	// by any of them.
	for(ci = cliphead; ci; ci = ci->next)
	{
		//printf( "      %p: %4x => %4x (%d)\n",ci,ci->start,ci->end,ci->used);
		if(startAngle >= ci->start && endAngle <= ci->end)
		{
			//printf( "  range already exists\n");
			return;	// The new range already exists.
		}
		//printf( "loop1\n");
		/*if(ci == ci->next)
			Con_Error("%p linked to itself: %x => %x\n",ci,ci->start,ci->end);*/
	}

	// Now check if any of the old ranges are contained by the new one.
	for(ci = cliphead; ci;)
	{
		//printf( "loop2\n");
		if(ci->start >= startAngle && ci->end <= endAngle)
		{
			crange = ci;
			//printf( "  removing contained range %x => %x\n",crange->start,crange->end);
			// We must do this in order to keep the loop from breaking.
			ci = ci->next;	
			C_RemoveRange(crange);
			//if(!ci) ci = cliphead;
			//if(!ci) break;
			continue;
		}	
		ci = ci->next;
	}

	// Now it is possible that the new range overlaps one or two old ranges.
	// If two are overlapped, they are consecutive. First we'll try to find
	// a range that overlaps the beginning.
	for(crange = 0, ci = cliphead; ci; ci = ci->next)
	{
		// In preparation of the next stage, find a good spot for the range.
		if(ci->start < endAngle) crange = ci; // Add after this one.

		//printf( "loop3\n");
		if(ci->start >= startAngle && ci->start <= endAngle)
		{
			// New range's end and ci's beginning overlap. ci's end is outside.
			// Otherwise it would have been already removed.
			// It suffices to adjust ci.
			//printf( "  overlapping beginning with %x => %x, ",ci->start,ci->end);
			ci->start = startAngle;
			//printf( "adjusting ci to %x => %x\n",ci->start,ci->end);
			return;
		}
		// Check an overlapping end.
		if(ci->end >= startAngle && ci->end <= endAngle)
		{
			// Now it's possible that the ci->next's beginning overlaps the new
			// range's end. In that case there will be a merger.
			//printf( "  overlapping end with %x => %x:\n",ci->start,ci->end);
			crange = ci->next;
			if(!crange)
			{
				ci->end = endAngle;
				//printf( "    no next, adjusting end (now %x => %x)\n",ci->start,ci->end);
				//return;
			}
			else
			{
				if(crange->start <= endAngle)
				{
					// A fusion will commence. Ci will eat the new range 
					// *and* crange.
					ci->end = crange->end;
					//printf( "    merging with the next (%x => %x)\n",crange->start,crange->end);
					C_RemoveRange(crange);
					//return;
				}
				else
				{
					// Not overlapping.
					ci->end = endAngle;
					//printf( "    not merger w/next, ci is now %x => %x\n",ci->start,ci->end);
					//return;
				}
			}
			return;
		}
	}
	
	// Still here? Now we know for sure that the range is disconnected from
	// the others. We still need to find a good place for it. Crange will mark
	// the spot. 
	
	//printf( "  range doesn't overlap old ones:\n");
/*	crange = 0;
	for(ci=cliphead; ci; ci=ci->next)
	{
	//	//printf( "loop4\n");
		if(ci->start < endAngle) // Is this a suitable spot?
			crange = ci; // Add after this one.
		else break;	// We're done.
	}*/
	if(!crange)
	{
		//printf( "    no suitable new spot, adding to head\n");
		// We have a new head node.
		crange = cliphead;
		cliphead = C_NewRange(startAngle, endAngle);
		cliphead->next = crange;
		if(crange) crange->prev = cliphead;
		return;
	}
	//printf("  spot found, adding after %x => %x\n",crange->start,crange->end);
	// Add the new range after crange.
	ci = C_NewRange(startAngle, endAngle);
	ci->next = crange->next;
	if(ci->next) ci->next->prev = ci;
	ci->prev = crange;
	crange->next = ci;
}

//===========================================================================
// C_NewOcclusionRange
//===========================================================================
occnode_t *C_NewOcclusionRange
	(binangle_t stAng, binangle_t endAng, float *normal, boolean tophalf)
{
	occnode_t *startNode = orangeCursor;
	occnode_t *last = occnodes + maxOccNodes-1;

	do 
	{
		if(++orangeCursor > last) 
			orangeCursor = occnodes;
		if(orangeCursor == startNode) // Full circle?
		{
			Con_Error("C_NewOcclusionRange: Out of oranges (max %i).\n",
				maxOccNodes);
		}
	}
	while(orangeCursor->flags & OCNF_USED);

	// Initialize the node.
	orangeCursor->flags = OCNF_USED;	// Clear other flags.
	if(tophalf) orangeCursor->flags |= OCNF_TOPHALF;
	orangeCursor->start = stAng;
	orangeCursor->end = endAng;
	memcpy(orangeCursor->normal, normal, sizeof(orangeCursor->normal));
	return orangeCursor;
}

//===========================================================================
// C_RemoveOcclusionRange
//===========================================================================
void C_RemoveOcclusionRange(occnode_t *orange)
{
#if _DEBUG
	if(!(orange->flags & OCNF_USED)) 
		Con_Error("Tried to remove an unused orange.\n");
#endif

	// If this is the head, move it to the next one.
	if(occhead == orange) occhead = orange->next;

	orange->flags = 0;
	if(orange->prev) orange->prev->next = orange->next;
	if(orange->next) orange->next->prev = orange->prev;
}

//===========================================================================
// C_AddOcclusionRange
//	The given range must be safe.
//===========================================================================
void C_AddOcclusionRange
	(binangle_t start, binangle_t end, float *normal, boolean tophalf)
{
	occnode_t *orange, *newor, *last;

	// Is the range valid?
	if(start > end) return;

	// A new range will be added.
	newor = C_NewOcclusionRange(start, end, normal, tophalf);

#if _DEBUG
	C_CountOranges();
#endif

	// Are there any previous occlusion nodes?
	if(!occhead)
	{
		// No; this is the first.
		occhead = newor;
		occhead->next = occhead->prev = NULL;
		return;
	}

	// FIXME: An optimization: remove existing oranges that are fully 
	// contained by the new orange. But how to do the check efficiently?

	// Add the new occlusion range to the appropriate position.
	for(orange = occhead; orange; last = orange, orange = orange->next)
		// The list of oranges is sorted by the start angle.
		// Find the first range whose start is greater than the new one.
		if(orange->start > start) 
		{
			// Add before this one.
			newor->next = orange;
			newor->prev = orange->prev;
			orange->prev = newor;
			if(newor->prev) 
				newor->prev->next = newor;
			else
				occhead = newor; // We have a new head.
			return;
		}

	// All right, add the new range to the end of the list.
	last->next = newor;
	newor->prev = last;
	newor->next = NULL;
}

#if _DEBUG
//===========================================================================
// C_OrangeRanger
//===========================================================================
void C_OrangeRanger(int mark)
{
	occnode_t *orange;

	for(orange = occhead; orange; orange = orange->next)
		if(orange->prev && orange->prev->start > orange->start)
			Con_Error("C_OrangeRanger(%i): Orange order has failed.\n", mark);
}

//===========================================================================
// C_OcclusionLister
//===========================================================================
void C_OcclusionLister(char *title)
{
	occnode_t *orange;

	printf("*** %s:\n", title);
	for(orange = occhead; orange; orange = orange->next)
		printf("  %04x-%04x (%i)\n",
		orange->start, orange->end, (orange->flags & OCNF_TOPHALF)!=0);
}
#endif

//===========================================================================
// C_MergeOccludes
//	Try to merge oranges with matching ranges. (Quite a number may be
//	produced as a result of the cuts.)
//===========================================================================
void C_MergeOccludes(void)
{	
	occnode_t *orange, *next, *other;
	binangle_t crossAngle;
	float cross[3];

	for(orange = occhead; orange && orange->next; orange = next)
	{
		next = orange->next;

		// Find a good one to test with.
		for(other = next; other; other = other->next)
		{
			if(orange->start != other->start) break; 
			if(orange->end != other->end
				|| (other->flags & OCNF_TOPHALF) !=
					(orange->flags & OCNF_TOPHALF)) continue;
			goto try_merge;
		}
		continue;

try_merge:
		// We can't test this steep planes.
		if(!orange->normal[VZ]) continue;

		// Where do they cross?
		M_CrossProduct(orange->normal, other->normal, cross);
		if(!cross[VX] && !cross[VY] && !cross[VZ])
		{
			// These two planes are exactly the same! Remove one.
			C_RemoveOcclusionRange(orange);
			continue;
		}

		// The cross angle must be outside the range.
		crossAngle = bamsAtan2((int)cross[VY], (int)cross[VX]);
		if(crossAngle >= orange->start && crossAngle <= orange->end)
			continue; // Crosses inside the range, can't do a thing.
		// FIXME: Isn't it possible to consistently determine which 
		// direction the cross vector is pointing to?
		crossAngle += BANG_180;
		if(crossAngle >= orange->start && crossAngle <= orange->end)
			continue; // Crosses inside the range, can't do a thing.

		// Now we must determine which plane occludes which.
		// Pick a point in the middle of the range.
		crossAngle = (orange->start + orange->end) >> (1 + BAMS_BITS - 13);
		cross[VX] = 100 * FIX2FLT( finecosine[crossAngle] );
		cross[VY] = 100 * FIX2FLT( finesine[crossAngle] );
		// z = -(A*x+B*y)/C
		cross[VZ] = -(orange->normal[VX]*cross[VX] 
			+ orange->normal[VY]*cross[VY]) / orange->normal[VZ];

		// Is orange occluded by the other one?
		if(M_DotProduct(cross, other->normal) < 0)
		{
			// No; then the other one is occluded by us. 
			// Remove it instead.
			if(next == other) next = next->next;
			C_RemoveOcclusionRange(other);
		}
		else
		{
			C_RemoveOcclusionRange(orange);
		}
	}
}

//===========================================================================
// C_CutOcclusionRange
//	Everything in the given range is removed from the occlusion nodes.
//===========================================================================
void C_CutOcclusionRange(binangle_t startAngle, binangle_t endAngle)
{
	occnode_t *orange, *next, *after, *part;

/*#if _DEBUG
	if(highOccNodes >= 20)
	{
		char buf[40];
		sprintf(buf, "BEFORE CUT %04x -> %04x", startAngle, endAngle);
		C_OcclusionLister(buf);
	}
#endif*/
#if _DEBUG
	C_OrangeRanger(1);
#endif

	// Find the node after which it's OK to add oranges cut in half.
	// (Must preserve the ascending order of the start angles.)
	after = NULL;
	for(orange = occhead; orange; orange = orange->next)
	{
		// We want the orange with the smallest start angle, but one that 
		// starts after the cut range has ended.
		if(orange->start < endAngle) after = orange; else break;
	}

	for(orange = occhead; orange; orange = next)
	{
		// In case orange is removed, take a copy of the next one.
		next = orange->next;

		// Does the cut range miss this orange?
		if(startAngle > orange->end) continue;
		if(orange->start >= endAngle) break; // No more possible cuts.

		// Check if the cut range completely includes this orange.
		if(orange->start >= startAngle && orange->end <= endAngle)
		{
			// Fully contained; this orange will be removed.
			C_RemoveOcclusionRange(orange);
			continue;
		}

		// Three other options:
		// 1) The cut range contains the beginning of the orange.
		if(orange->start >= startAngle && orange->start < endAngle)
		{
			// Cut away the beginning of this orange.
			orange->start = endAngle;
			// Even thought the start angle is modified, we don't need to
			// move this orange anywhere. This is because after the cut there
			// will be no oranges beginning inside the cut range.
			continue;
		}
		
		// 2) The cut range contains the end of the orange.
		if(orange->end > startAngle && orange->end <= endAngle)
		{
			// Cut away the end of this orange.
			orange->end = startAngle;
			continue;
		}

		// 3) The orange contains the whole cut range.
		if(startAngle > orange->start && endAngle < orange->end)
		{
			// The orange gets cut in two parts. Create a new orange that
			// represents the end, and add it after the 'after' node, or to
			// the head of the list.
			part = C_NewOcclusionRange(endAngle, orange->end,
				orange->normal, (orange->flags & OCNF_TOPHALF) != 0);
			part->prev = after;
			if(after)
			{
				part->next = after->next;
				after->next = part;
			}
			else
			{
				// Add to the head.
				part->next = occhead;
				occhead = part;
			}
			if(part->next) part->next->prev = part;
			// Modify the start part.
			orange->end = startAngle;
		}
	}

#if _DEBUG
	C_OrangeRanger(2);
#endif

	C_MergeOccludes();
		
#if _DEBUG
	C_OrangeRanger(6);
#endif

/*#if _DEBUG
	if(highOccNodes >= 20)
	{
		char buf[40];
		sprintf(buf, "AFTER CUT (%04x -> %04x)", startAngle, endAngle);
		C_OcclusionLister(buf);
	}
#endif*/
}

//===========================================================================
// C_Ranger
//	A debugging aid: checks if clipnode links are valid.
//===========================================================================
void C_Ranger(void)
{
	clipnode_t *ci;

	//printf("Ranger:\n");
	for(ci=cliphead; ci; ci=ci->next)
	{
		if(ci==cliphead)
		{
			if(ci->prev != 0)
				Con_Error("Cliphead->prev != 0.\n");
		}
		// Confirm that the links to prev and next are OK.
		if(ci->prev)
		{
			if(ci->prev->next != ci)
				Con_Error("Prev->next != this\n");
		}
		else if(ci != cliphead) Con_Error("prev == null, this isn't cliphead.\n");

		if(ci->next)
		{
			if(ci->next->prev != ci)
				Con_Error("Next->prev != this\n");
		}

		/*printf( "  %p: %04x => %04x ", ci, ci->start, ci->end);
		if(ci->prev)
			printf( "(gap: %d)\n", ci->start-ci->prev->end);
		else
			printf( "\n");*/
	}
}

//===========================================================================
// C_SafeAddRange
//===========================================================================
int C_SafeAddRange(binangle_t startAngle, binangle_t endAngle)
{
	// The range may wrap around.
	if(startAngle > endAngle)
	{
		// The range has to added in two parts.
		C_AddRange(startAngle, BANG_MAX);
		C_AddRange(0, endAngle);
	}
	else
	{
		// Add the range as usual.
		C_AddRange(startAngle, endAngle);
	}
	return true;
}

//===========================================================================
// C_AddViewRelSeg
//	Add a segment relative to the current viewpoint.
//===========================================================================
void C_AddViewRelSeg(float x1, float y1, float x2, float y2)
{
	float dx1 = x1-vx, dy1 = y1-vz, dx2 = x2-vx, dy2 = y2-vz;
	C_SafeAddRange(bamsAtan2((int)(dy2*100), (int)(dx2*100)), 
		bamsAtan2((int)(dy1*100), (int)(dx1*100)));
}

//===========================================================================
// C_SafeAddOcclusionRange
//	If necessary, cut the given range in two.
//===========================================================================
void C_SafeAddOcclusionRange
	(binangle_t startAngle, binangle_t endAngle, float *normal, boolean tophalf)
{
	// Is this range already clipped?
	if(!C_SafeCheckRange(startAngle, endAngle)) return; // Already clipped.

	if(startAngle > endAngle)
	{
		// The range has to added in two parts.
		C_AddOcclusionRange(startAngle, BANG_MAX, normal, tophalf);
#if _DEBUG
		C_OrangeRanger(3);
#endif
		C_AddOcclusionRange(0, endAngle, normal, tophalf);
#if _DEBUG
		C_OrangeRanger(4);
#endif

	}
	else
	{
		// Add the range as usual.
		C_AddOcclusionRange(startAngle, endAngle, normal, tophalf);
#if _DEBUG
		C_OrangeRanger(5);
#endif

	}
/*#if _DEBUG
	{
		char buf[30];
		sprintf(buf, "ADDED <%04x-%04x>", startAngle, endAngle);
		C_OcclusionLister(buf);
	}
#endif*/
}

//===========================================================================
// C_PointToAngle
//	The point should be view-relative!
//===========================================================================
binangle_t C_PointToAngle(float *point)
{
	return bamsAtan2((int)(point[VY]*100), (int)(point[VX]*100));
}

//===========================================================================
// C_AddViewRelOcclusion
//	Add an occlusion segment relative to the current viewpoint.
//===========================================================================
void C_AddViewRelOcclusion
	(float *v1, float *v2, float height, boolean tophalf)
{
	float viewtov1[3], viewtov2[3];
	float normal[3];

	// FIXME: Optimization? Check if the given line is already occluded.
	
	// Calculate the occlusion plane normal. We'll use the game's coordinate
	// system (left-handed, but Y and Z are swapped).
	viewtov1[VX] = v1[VX] - vx;
	viewtov1[VY] = v1[VY] - vz;
	viewtov2[VX] = v2[VX] - vx;
	viewtov2[VY] = v2[VY] - vz;
	viewtov1[VZ] = viewtov2[VZ] = height - vy;
	// The normal points to the half we want to occlude.
	M_CrossProduct(
		tophalf? viewtov2 : viewtov1, 
		tophalf? viewtov1 : viewtov2, normal);

#if _DEBUG
	{
		float testPos[3] = { 0, 0, tophalf? 1000 : -1000 };
		float dot = M_DotProduct(testPos, normal);
		if(dot < 0) 
		{
			Con_Error("C_AddViewRelOcclusion: wrong side!\n");
		}
	}	
#endif

	C_SafeAddOcclusionRange(C_PointToAngle(viewtov2), 
		C_PointToAngle(viewtov1), normal, tophalf);
}

//===========================================================================
// C_IsPointOccluded
//	Returns true if the view relative point is occluded by an occlusion 
//	range.
//===========================================================================
boolean C_IsPointOccluded(float *viewrelpoint)
{
	binangle_t angle = C_PointToAngle(viewrelpoint);
	occnode_t *orange;

	for(orange = occhead; orange; orange = orange->next)
	{
		if(angle < orange->start || angle > orange->end) continue;
		if(orange->start > angle) return false; // No more possibilities.

		// On which side of the occlusion plane is it? The positive side
		// is the occluded one.
		if(M_DotProduct(viewrelpoint, orange->normal) > 0)
			return true; // Occluded!
	}
		
	// No orange occluded the point.
	return false;
}

//===========================================================================
// C_IsPointVisible
//	Returns true if the point is visible after checking both the clipnodes
//	and the occlusion planes. Note that this test can only be done with
//	points that reside in subsectors that have not yet been rendered.
//===========================================================================
boolean C_IsPointVisible(float x, float y, float height)
{
	float point[3] = { x - vx, y - vz, height - vy };
	binangle_t angle = C_PointToAngle(point);

	if(!C_IsAngleVisible(angle)) return false;

	// The point was not clipped by the clipnodes. Perhaps it's occluded
	// by an orange.
	return !C_IsPointOccluded(point);
}

//===========================================================================
// C_IsSegOccluded
//	NOTE: Unused and untested. Almost certainly doesn't work correctly.
//===========================================================================
boolean C_IsSegOccluded
	(float relv1[3], float relv2[3], float reltop, float relbottom, 
	 binangle_t startAngle, binangle_t endAngle)
{
	// The segment is always fully occluded from startAngle to occAngle.
	float cross[3], testNormal[3];
	binangle_t occAngle, crossAngle, trueStart, trueEnd;
	occnode_t *orange;
	clipnode_t *ci;
	boolean side1, side2, isSafe;

	// See if the given actual test range is safe. (startAngle and endAngle
	// always are.)
	trueStart = C_PointToAngle(relv2);
	trueEnd = C_PointToAngle(relv1);
	isSafe = (trueStart < trueEnd);

	// startAngle and endAngle are the real, safe range. It's first clipped
	// by any available clipnodes. We already know that no clipnode fully 
	// contains the test range.
	for(ci = cliphead; ci; ci = ci->next)
	{
		// Does this clipnode partially overlap the test range?
		if(startAngle >= ci->start && startAngle <= ci->end)
		{
			// Start of the test range gets clipped.
			startAngle = ci->end;
		}
		if(endAngle >= ci->start && endAngle <= ci->end)
		{
			// End of the test range gets clipped.
			endAngle = ci->start;
		}
	}

	// In the beginning we have occluded nothing, i.e. up to the start angle.
	occAngle = startAngle;

	for(orange = occhead; orange; orange = orange->next)
	{
		if(occAngle >= endAngle) return true; // Fully occluded.
		
		// This is the quickest way out of there: if we come across an
		// occlusion range that begins AFTER the occAngle, the portion
		// in between obviously won't be occluded by anybody (since oranges
		// are sorted by ascending start angles).
		if(orange->start > occAngle) return false;
		if(orange->end < occAngle) continue; // Useless...

/*		if(orange->end < startAngle) continue; // Doesn't overlap.
		if(orange->start > endAngle) break; // The rest are past the end.*/
			
		// This orange overlaps the test range. Let's determine the test
		// plane we will be using.
		if(orange->flags & OCNF_TOPHALF)
		{
			// Tophalf is occluded, so test with the bottom of the seg.
			relv1[VZ] = relv2[VZ] = relbottom;
		}
		else
		{
			// Bottomhalf is occluded, so test with the top of the seg.
			relv1[VZ] = relv2[VZ] = reltop;
		}
		// Side=true means 'occluded'. Note that side2's angle is smaller
		// than side1's.
		side1 = M_DotProduct(relv1, orange->normal) > 0;
		side2 = M_DotProduct(relv2, orange->normal) > 0;
		if(side1 && side2)
		{
			// Does the orange fully contain the remaining portion of the seg?
			if(occAngle >= orange->start && endAngle <= orange->end)
				return true; // Fully occluded by this orange!

			// Both the start and end vertex of the seg are occluded by
			// this orange, but the orange doesn't cover the whole seg. 
			if(orange->end > occAngle) occAngle = orange->end;
			// Now we know that the seg has been occluded from the beginning 
			// to occAngle.
			continue; // Find more juicy oranges.
		}
		if(!side1 && !side2)
		{
			// This orange does not fully overlap the seg. Let's hope that 
			// some other orange will.
			continue;
		}
		// The test plane crosses the occlusion plane somewhere inside the
		// seg (because the endpoints of the test seg were at different sides
		// of the occlusion plane). Calculate the normal of the test plane.
		M_CrossProduct(relv1, relv2, testNormal);
		
		// Calculate the angle of the cross line.
		M_CrossProduct(testNormal, orange->normal, cross);
		crossAngle = C_PointToAngle(cross);
		if(isSafe)
		{
			if(crossAngle < trueStart || crossAngle > trueEnd) 
			{
				crossAngle += BANG_180; // Flip over.
				if(crossAngle < trueStart || crossAngle > trueEnd)
					Con_Error("Cross line behaves strangely(1) "
					"(st=%x end=%x crs=%x).\n", trueStart, trueEnd, crossAngle);
			}
		}
		else
		{
			if(crossAngle < trueStart && crossAngle > trueEnd)
			{
				crossAngle += BANG_180;
				if(crossAngle < trueStart && crossAngle > trueEnd)
					Con_Error("Cross line behaves strangely(2) "
					"(st=%x end=%x crs=%x).\n", trueStart, trueEnd, crossAngle);
			}
		}
		
		// Remember, side2 has a smaller angle.

		// FIXME: What about trueStart/trueEnd!!! and isSafe! it must have
		// an effect on this...

		if(side2)
		{
			// We have an occlusion up to the cross point.
			if(crossAngle > occAngle) occAngle = crossAngle;
		}
		else // if(side1)
		{
			// We have an occlusion starting from crossAngle.
			if(crossAngle <= occAngle) 
				// The occlusion begins before or at the currently 
				// occluded portion.
				if(orange->end > occAngle) occAngle = orange->end;
		}
	}
	// If the occlusion happened up to the end angle, we're OK.
	return (occAngle >= endAngle); 
}

//===========================================================================
// C_CheckSeg
//	Returns true if the segment is visible according to the current 
//	clipnode and occlusion information.
//===========================================================================
boolean C_CheckSeg(float *v1, float *v2, float top, float bottom)
{
	float relv1[3] = { v1[VX] - vx, v1[VY] - vz, 0 };
	float relv2[3] = { v2[VX] - vx, v2[VY] - vz, 0 };
	float reltop = top - vy, relbottom = bottom - vy;
	binangle_t start, end;

	// Determine the range.
	start = C_PointToAngle(relv2);
	end = C_PointToAngle(relv1);
	if(start == end) return true; // Might as well be visible...
	if(!C_SafeCheckRange(start, end)) return false; // Entirely clipped.

	// Now the more difficult part... The range may be occluded by a number
	// of occlusion ranges, but we must determine whether these occlude the
	// segment fully, in 3D.
	if(start < end)
	{
		// The range doesn't wrap around.
		return !C_IsSegOccluded(relv1, relv2, reltop, relbottom, start, end);
	}
	// The range wraps around.
	return !C_IsSegOccluded(relv1, relv2, reltop, relbottom, start, BANG_MAX)
		|| !C_IsSegOccluded(relv1, relv2, reltop, relbottom, 0, end);
}

//===========================================================================
// C_IsRangeVisible
//	The specified range must be safe!
//===========================================================================
static int C_IsRangeVisible(binangle_t startAngle, binangle_t endAngle)
{
	clipnode_t	*ci;
	
	for(ci=cliphead; ci; ci=ci->next)
		if(startAngle >= ci->start && endAngle <= ci->end)
			return false;
	// No node fully contained the specified range.
	return true;
}

//===========================================================================
// C_SafeCheckRange
//	Returns 1 if the range is not entirely clipped. 
//===========================================================================
static int C_SafeCheckRange(binangle_t startAngle, binangle_t endAngle)
{
/*	binangle_t angLen = endAngle - startAngle;

	// Check that the range is valid.
	if(!angLen) return C_IsAngleVisible(startAngle);
	if(angLen >= BANG_180) return 0;
	if((int)startAngle+(int)angLen > BANG_MAX)*/
	//if(startAngle == endAngle) return C_IsAngleVisible(startAngle);
	
	if(startAngle > endAngle)
	{
		// The range wraps around.
		return (C_IsRangeVisible(startAngle, BANG_MAX) 
			|| C_IsRangeVisible(0, endAngle));
	}
	return C_IsRangeVisible(startAngle, endAngle);
}

int C_CheckViewRelSeg(float x1, float y1, float x2, float y2)
{
	float dx1 = x1-vx, dy1 = y1-vz, dx2 = x2-vx, dy2 = y2-vz;
	return C_SafeCheckRange(bamsAtan2((int)(dy2*100), (int)(dx2*100)), 
		bamsAtan2((int)(dy1*100), (int)(dx1*100)));
}

//===========================================================================
// C_IsAngleVisible
//	Returns true if the specified angle is visible.
//===========================================================================
int C_IsAngleVisible(binangle_t bang)
{
	clipnode_t	*ci;

	for(ci=cliphead; ci; ci=ci->next)
		if(bang > ci->start && bang < ci->end) return false;
	// No one clipped this angle.
	return true;
}

clipnode_t *C_AngleClippedBy(binangle_t bang)
{
	clipnode_t	*ci;

	for(ci=cliphead; ci; ci=ci->next)
		if(bang > ci->start && bang < ci->end) return ci;
	// No one clipped this angle.
	return 0;
}

// Returns 1 if the subsector might be visible.
int C_CheckSubsector(subsector_t *ssec)
{
	int			i;
	clipnode_t	*cnode=0;

	for(i = 0; i < ssec->numverts; i++)	// Angles to all corners.
	{
		fvertex_t *vtx = ssec->verts + i;
		// Shift for more accuracy.
		anglist[i] = bamsAtan2((int)((vtx->y - vz)*100), (int)((vtx->x - vx)*100));
	}
	// Check each of the ranges defined by the edges.
	for(i = 0; i < ssec->numverts-1; i++)
	{
		int end = i+1;
		binangle_t angLen;

		// The last edge won't be checked. This is because the edges
		// define a closed, convex polygon and the last edge's range is 
		// always already covered by the previous edges. (Right?)

		// If even one of the edges is not contained by a clipnode, 
		// the subsector is at least partially visible.
		angLen = anglist[end] - anglist[i];

		// The viewer is on an edge, the subsector should be visible.
		if(angLen == BANG_180) return 1;

		// Choose the start and end points so that length is < 180.
		if(angLen < BANG_180)
		{
			if(C_SafeCheckRange(anglist[i], anglist[end])) return 1;
		}
		else
		{
			if(C_SafeCheckRange(anglist[end], anglist[i])) return 1;
		}
	}
	// All clipped away, the subsector cannot be seen.
	return 0;
}
