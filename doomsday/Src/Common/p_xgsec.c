//**************************************************************************
//**
//** P_XGSEC.C
//**
//** Extended Generalized Sector Types.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <math.h>
#include <ctype.h>

#ifdef __JDOOM__
#include "doomdef.h"
#include "p_local.h"
#include "p_spec.h"
#include "doomstat.h"
#include "d_config.h"
#include "s_sound.h"
#include "m_random.h"
#include "p_inter.h"
#include "r_defs.h"
#endif

#ifdef __JHERETIC__
#include "Doomdef.h"
#include "settings.h"
#include "P_local.h"
#include "Soundst.h"
#endif

#include "p_xgline.h"
#include "p_xgsec.h"

// MACROS ------------------------------------------------------------------

#define PI				3.141592657

#define MAX_VALS		128

#define BL_BUILT		0x1
#define BL_WAS_BUILT	0x2
#define BL_SPREADED		0x4

#define SIGN(x)			((x)>0? 1 : (x)<0? -1 : 0)

#define ISFUNC(fn)		(fn->func && fn->func[fn->pos])
#define UPDFUNC(fn)		((ISFUNC(fn) || fn->link))

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void XS_DoChain(sector_t *sec, int ch, int activating, void *act_thing);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern mobj_t		dummything;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static byte			*builder;
static sectortype_t	sectypebuffer;

// CODE --------------------------------------------------------------------

sectortype_t *XS_GetType(int id)
{
	sectortype_t *ptr;

	// Try finding it from the DDXGDATA lump.
	ptr = XG_GetLumpSector(id);
	if(ptr) 
	{
		// Got it!
		memcpy(&sectypebuffer, ptr, sizeof(*ptr));
		return &sectypebuffer;	
	}
	if(Def_Get(DD_DEF_SECTOR_TYPE, (char*) id, &sectypebuffer))
		return &sectypebuffer; // A definition was found.
	// Sorry...	
	return NULL;
}

void XF_Init(sector_t *sec, function_t *fn, char *func, int min, int max, 
			 float scale, float offset)
{
	xgsector_t *xg = sec->xg;
	memset(fn, 0, sizeof(*fn));	

	if(!func) return;

	// Check for links.
	if(func[0] == '=')
	{
		switch(tolower(func[1]))
		{
		case 'r':
			fn->link = &xg->rgb[0];
			break;

		case 'g':
			fn->link = &xg->rgb[1];
			break;

		case 'b':
			fn->link = &xg->rgb[2];
			break;

		case 'f':
			fn->link = &xg->plane[XGSP_FLOOR];
			break;

		case 'c':
			fn->link = &xg->plane[XGSP_CEILING];
			break;

		case 'l':
			fn->link = &xg->light;
			break;

		default:
			Con_Error("XF_Init: Bad linked func (%s).\n", func);
		}
		return;
	}
	// Check for offsets to current values.
	if(func[0] == '+')
	{
		switch(func[1])
		{
		case 'r':
			offset += sec->origrgb[0];
			break;

		case 'g':
			offset += sec->origrgb[1];
			break;

		case 'b':
			offset += sec->origrgb[2];
			break;

		case 'l':
			offset += sec->origlight;
			break;

		case 'f':
			offset += FIX2FLT(sec->origfloor);
			break;

		case 'c':
			offset += FIX2FLT(sec->origceiling);
			break;

		default:
			Con_Error("XF_Init: Bad preset offset (%s).\n", func);
		}
		fn->func = func + 2;
	}
	else
	{
		fn->func = func;
	}
	fn->timer = -1;	// The first step musn't skip the first value.
	fn->maxtimer = XG_RandomInt(min, max);
	fn->mininterval = min;
	fn->maxinterval = max;
	fn->scale = scale;
	fn->offset = offset;
	// Make sure oldvalue is out of range.
	fn->oldvalue = -scale + offset;
}

int XLTrav_LineAngle(line_t *line, int data, void *context)
{
	sector_t *sec = (sector_t*) data;

	if(line->frontsector != sec && line->backsector != sec) 
		return true; // Wrong sector, keep looking.
	*(angle_t*) context = R_PointToAngle2(0, 0, line->dx, line->dy);
	return false; // Stop looking after first hit.
}

void XS_SetSectorType(struct sector_s *sec, int special)
{
	xgsector_t *xg;
	sectortype_t *info;
	int i;

	if(XS_GetType(special))
	{
		XG_Dev("XS_SetSectorType: Sector %i, type %i", sec - sectors,
			special);

		sec->special = special;

		// All right, do the init.
		if(!sec->xg) sec->xg = Z_Malloc(sizeof(xgsector_t), PU_LEVEL, 0);
		memset(sec->xg, 0, sizeof(*sec->xg));

		// Get the type info.
		memcpy(&sec->xg->info, &sectypebuffer, sizeof(sectypebuffer));
		
		// Init the state.
		xg = sec->xg;
		info = &sec->xg->info;

		// Init timer so ambient doesn't play immediately at level start.
		xg->timer = XG_RandomInt(FLT2TIC(xg->info.sound_interval[0]), 
			FLT2TIC(xg->info.sound_interval[1]));

		// Light function.
		XF_Init(sec, &xg->light, info->lightfunc, info->light_interval[0],
			info->light_interval[1], 255, 0);

		// Color functions.
		for(i=0; i<3; i++)
		{
			XF_Init(sec, &xg->rgb[i], info->colfunc[i], 
				info->col_interval[i][0], info->col_interval[i][1], 255, 0);
		}

		// Plane functions / floor.
		XF_Init(sec, &xg->plane[XGSP_FLOOR], info->floorfunc, 
			info->floor_interval[0], info->floor_interval[1], 
			info->floormul, info->flooroff);
		XF_Init(sec, &xg->plane[XGSP_CEILING], info->ceilfunc, 
			info->ceil_interval[0], info->ceil_interval[1], 
			info->ceilmul, info->ceiloff);

		// Derive texmove angle from first act-tagged line?
		if(info->flags & STF_ACT_TAG_TEXMOVE
			|| info->flags & STF_ACT_TAG_WIND)
		{
			angle_t angle = 0;
			XL_TraverseLines(0, LREF_TAGGED, info->act_tag,
				(int) sec, &angle, XLTrav_LineAngle);
			// Convert to degrees.
			if(info->flags & STF_ACT_TAG_TEXMOVE)
			{
				info->texmove_angle[0] 
					= info->texmove_angle[1]
					= angle / (float) ANGLE_MAX * 360;
			}
			if(info->flags & STF_ACT_TAG_WIND)
			{
				info->wind_angle = angle / (float) ANGLE_MAX * 360;
			}
		}
	}
	else
	{
		XG_Dev("XS_SetSectorType: Sector %i, NORMAL TYPE %i", sec - sectors,
			special);

		// Free previously allocated XG data.
		if(sec->xg) Z_Free(sec->xg);
		sec->xg = NULL;

		// Just set it, then. Must be a standard sector type...
		// Mind you, we're not going to spawn any standard flash funcs 
		// or anything.
		sec->special = special;
	}
}

void XS_Init(void)
{
	int i;
	sector_t *sec;

	// Allocate stair builder data.
	builder = Z_Malloc(numsectors, PU_LEVEL, 0);
	memset(builder, 0, numsectors);

/*	// Clients rely on the server, they don't do XG themselves.
	if(IS_CLIENT) return;*/

	for(i = 0; i < numsectors; i++)
	{
		sec = &sectors[i];
		sec->origfloor = sec->floorheight;
		sec->origceiling = sec->ceilingheight;
		sec->origlight = sec->lightlevel;
		memcpy(sec->origrgb, sec->rgb, 3);

		// Initialize the XG data for this sector.
		XS_SetSectorType(sec, sec->special);
	}
}

void XS_SectorSound(sector_t *sec, int snd)
{
	if(!snd) return;
	S_SectorSound(sec, snd);
}

void XS_MoverStopped(xgplanemover_t *mover, boolean done)
{
	XG_Dev("XS_MoverStopped: Sector %i (done=%i, origin line=%i)", 
		mover->sector - sectors, done, 
		mover->origin? mover->origin - lines : -1);

	if(done)
	{
		if(mover->flags & PMF_ACTIVATE_WHEN_DONE
			&& mover->origin)
		{
			XL_ActivateLine(true, &mover->origin->xg->info, mover->origin,
				0, &dummything);
		}			
		if(mover->flags & PMF_DEACTIVATE_WHEN_DONE
			&& mover->origin)
		{
			XL_ActivateLine(false, &mover->origin->xg->info, mover->origin,
				0, &dummything);
		}
		// Remove this thinker.
		P_RemoveThinker((thinker_t*)mover);
	}
	else
	{
		// Normally we just wait, but if...
		if(mover->flags & PMF_ACTIVATE_ON_ABORT
			&& mover->origin)
		{
			XL_ActivateLine(true, &mover->origin->xg->info, 
				mover->origin, 0, &dummything);
		}
		if(mover->flags & PMF_DEACTIVATE_ON_ABORT
			&& mover->origin)
		{
			XL_ActivateLine(false, &mover->origin->xg->info, 
				mover->origin, 0, &dummything);
		}
		if(mover->flags & (PMF_ACTIVATE_ON_ABORT | PMF_DEACTIVATE_ON_ABORT))
		{
			// Destroy this mover.
			P_RemoveThinker((thinker_t*)mover);
		}
	}
}

/*
 * A thinker function for plane movers.
 */
void XS_PlaneMover(xgplanemover_t *mover)
{
	int res, res2;
	int dir;
	int ceil = mover->sector->ceilingheight;
	int floor = mover->sector->floorheight;
	boolean docrush = (mover->flags & PMF_CRUSH) != 0;
	boolean follows = (mover->flags & PMF_OTHER_FOLLOWS) != 0;
	boolean setorig = (mover->flags & PMF_SET_ORIGINAL) != 0;

	// Play movesound when timer goes to zero.
	if(mover->timer-- <= 0)
	{
		// Clear the wait flag.
		if(mover->flags & PMF_WAIT)
		{
			mover->flags &= ~PMF_WAIT;
			// Play a sound.
			XS_SectorSound(mover->sector, mover->startsound);
		}

		mover->timer = XG_RandomInt(mover->mininterval, mover->maxinterval);
		XS_SectorSound(mover->sector, mover->movesound);
	}

	// Are we waiting?
	if(mover->flags & PMF_WAIT) return;

	// Determine move direction.
	if((mover->destination - (mover->ceiling? ceil : floor)) > 0)
		dir = 1;	// Moving up.
	else 
		dir = -1;	// Moving down.

	// Do the move.
	res = T_MovePlane(mover->sector, mover->speed, mover->destination,
		docrush, mover->ceiling, dir);
	// Should we update origheight?
	if(setorig)
	{
		if(mover->ceiling)
			mover->sector->origceiling = mover->sector->ceilingheight;
		else
			mover->sector->origfloor = mover->sector->floorheight;
	}

	if(follows)
	{
		int off = mover->ceiling? floor-ceil : ceil-floor;
		res2 = T_MovePlane(mover->sector, mover->speed, 
			mover->destination + off, docrush,
			!mover->ceiling, dir);
		// Should we update origheight?
		if(setorig)
		{
			if(!mover->ceiling)
				mover->sector->origceiling = mover->sector->ceilingheight;
			else
				mover->sector->origfloor = mover->sector->floorheight;
		}
		if(res2 == crushed) res = crushed;
	}
	
	if(res == pastdest)
	{
		// Move has finished.
		XS_MoverStopped(mover, true);

		// The move is done. Do end stuff.
		if(mover->setflat > 0) 
		{
			XS_ChangePlaneTexture(mover->sector, mover->ceiling, 
				mover->setflat);
		}
		if(mover->setsector >= 0)
		{
			XS_SetSectorType(mover->sector, mover->setsector);
		}
		// Play sound?
		XS_SectorSound(mover->sector, mover->endsound);
	}
	else if(res == crushed)
	{
		if(mover->flags & PMF_CRUSH)
		{
			// We're crushing things.
			mover->speed = mover->crushspeed;
		}
		else
		{
			// Make sure both the planes are where we started from.
			if((!mover->ceiling || follows)
				&& mover->sector->floorheight != floor)
			{
				T_MovePlane(mover->sector, mover->speed, 
					floor, docrush, false, -dir);
			}
			if((mover->ceiling || follows)
				&& mover->sector->ceilingheight != ceil)
			{
				T_MovePlane(mover->sector, mover->speed,
					ceil, docrush, true, -dir);
			}
			XS_MoverStopped(mover, false);
		}
	}
}

// Returns a new thinker for handling the specified plane.
// Removes any existing thinkers associated with the plane.
xgplanemover_t *XS_GetPlaneMover(sector_t *sector, boolean ceiling)
{
	thinker_t *th;
	xgplanemover_t *mover;

	for(th = thinkercap.next; th != &thinkercap; th = th->next)
		if(th->function == XS_PlaneMover)
		{
			mover = (xgplanemover_t*) th;
			if(mover->sector == sector 
				&& mover->ceiling == ceiling)
			{
				XS_MoverStopped(mover, false);
				P_RemoveThinker(th);	// Remove it.
			}
		}
	
	// Allocate a new thinker.
	mover = Z_Malloc(sizeof(*mover), PU_LEVEL, 0);
	memset(mover, 0, sizeof(*mover));
	mover->thinker.function = XS_PlaneMover;
	mover->sector = sector;
	mover->ceiling = ceiling;
	return mover;
}

void XS_ChangePlaneTexture(sector_t *sector, boolean ceiling, int tex)
{
	XG_Dev("XS_ChangePlaneTexture: Sector %i, %s, pic %i",
		sector - sectors, ceiling? "ceiling" : "floor", tex);

	if(ceiling) 
		sector->ceilingpic = tex; 
	else 
		sector->floorpic = tex;
}

// One plane can get listed multiple times.
int XS_AdjoiningPlanes(sector_t *sector, boolean ceiling, 
					   int *heightlist, int *piclist, int *lightlist,
					   sector_t **sectorlist)
{	
	int i, count = 0;
	line_t *lin;
	sector_t *other;

	for(i = 0; i < sector->linecount; i++)
	{
		lin = sector->Lines[i];
		// Only accept two-sided lines.
		if(!lin->frontsector || !lin->backsector) continue;
		if(lin->frontsector == sector)
			other = lin->backsector;
		else
			other = lin->frontsector;
		if(heightlist) heightlist[count] = ceiling? other->ceilingheight 
			: other->floorheight;
		if(piclist) piclist[count] = ceiling? other->ceilingpic 
			: other->floorpic;
		if(lightlist) lightlist[count] = other->lightlevel;
		if(sectorlist) sectorlist[count] = other;
		// Increment counter.
		count++;
	}
	return count;
}

int FindMaxOf(int *list, int num)
{
	int i, max = list[0], idx = 0;

	for(i=1; i<num; i++)
		if(list[i] > max) 
		{
			max = list[i];
			idx = i;
		}
	return idx;	
}

int FindMinOf(int *list, int num)
{
	int i, min = list[0], idx = 0;

	for(i=1; i<num; i++)
		if(list[i] < min)
		{
			min = list[i];
			idx = i;
		}
	return idx;
}

int FindNextOf(int *list, int num, int h)
{
	int i, min, idx = -1;

	for(i=0; i<num; i++)
	{
		if(list[i] <= h) continue;
		if(idx < 0 || list[i] < min)
		{
			idx = i;
			min = list[i];
		}
	}
	return idx;
}

int FindPrevOf(int *list, int num, int h)
{
	int i, max, idx = -1;

	for(i=0; i<num; i++)
	{
		if(list[i] >= h) continue;
		if(idx < 0 || list[i] > max)
		{
			idx = i;
			max = list[i];
		}
	}
	return idx;
}

int XS_GetTexH(int tex)
{
	Set(DD_TEXTURE_HEIGHT_QUERY, tex);
	return Get(DD_QUERY_RESULT);
}

// Really an XL_* function!
// 0=top, 1=mid, 2=bottom. Returns MAXINT if not height n/a.
int XS_TextureHeight(line_t *line, int part)
{
	side_t *side;
	int snum = 0;
	int minfloor, maxfloor, maxceil;
	sector_t *front = line->frontsector, *back = line->backsector;
	boolean twosided = front && back;

	if(part != 1 && !twosided) return DDMAXINT;
	if(twosided)
	{
		minfloor = front->floorheight;
		maxfloor = back->floorheight;
		if(part == 2) snum = 0;
		if(back->floorheight < minfloor)
		{
			minfloor = back->floorheight;
			maxfloor = front->floorheight;
			if(part == 2) snum = 1;
		}		
		maxceil = front->ceilingheight;
		if(part == 0) snum = 0;
		if(back->ceilingheight > maxceil)
		{
			maxceil = back->ceilingheight;
			if(part == 0) snum = 1;
		}
	}
	else
	{
		if(line->sidenum[0] >= 0) snum = 0; else snum = 1;
	}
	
	side = sides + line->sidenum[snum];

	if(part == 0)
	{
		if(!side->toptexture) return DDMAXINT;
		return maxceil - XS_GetTexH(side->toptexture);
	}
	if(part == 1)
	{
		if(!side->midtexture) return DDMAXINT;
		return maxfloor + XS_GetTexH(side->midtexture);
	}
	if(part == 2)
	{
		if(!side->bottomtexture) return DDMAXINT;
		return minfloor + XS_GetTexH(side->bottomtexture);
	}
	return DDMAXINT;
}

/*
 * Returns a pointer to the first sector with the tag.
 */
sector_t *XS_FindTagged(int tag)
{
	int k;

	for(k = 0; k < numsectors; k++)
	{
		if(sectors[k].tag == tag) return sectors + k;
	}
	return NULL;
}

boolean XS_GetPlane(line_t *actline, sector_t *sector, int ref, int refdata,
					int *height, int *pic, sector_t **planesector)
{
	int i = 0, k, num;
	int heights[MAX_VALS], pics[MAX_VALS];
	sector_t *sectorlist[MAX_VALS];
	boolean ceiling;
	sector_t *iter;

	XG_Dev("XS_GetPlane: Line %i, sector %i, ref (%i, %i)", 
		actline? actline - lines : -1, sector - sectors, ref, refdata);

	if(ref == SPREF_NONE) 
	{
		// No reference to anywhere.
		return false;
	}
	// Init the values to the current sector's floor.
	if(height) *height = sector->floorheight;
	if(pic) *pic = sector->floorpic;
	if(planesector) *planesector = sector;

	// First try the non-comparative, iterative sprefs.
	iter = NULL;
	switch(ref)
	{
	case SPREF_SECTOR_TAGGED_FLOOR:
	case SPREF_SECTOR_TAGGED_CEILING:
		if(!(iter = XS_FindTagged(sector->tag))) return false;
		break;

	case SPREF_LINE_TAGGED_FLOOR:
	case SPREF_LINE_TAGGED_CEILING:
		if(!actline) return false;
		if(!(iter = XS_FindTagged(actline->tag))) return false;
		break;

	case SPREF_TAGGED_FLOOR:
	case SPREF_TAGGED_CEILING:
	case SPREF_ACT_TAGGED_FLOOR:
	case SPREF_ACT_TAGGED_CEILING:
		if(!(iter = XS_FindTagged(refdata))) return false;
		break;

	case SPREF_INDEX_FLOOR:
	case SPREF_INDEX_CEILING:
		if(refdata < 0 || refdata >= numsectors) return false;
		iter = sectors + refdata;
		break;

	default:
		// No iteration.
		break;
	}

	// Did we find the plane through iteration?
	if(iter)
	{
		if(planesector) *planesector = iter;
		if(ref >= SPREF_SECTOR_TAGGED_FLOOR
			&& ref <= SPREF_INDEX_FLOOR)
		{
			if(height) *height = iter->floorheight;
			if(pic) *pic = iter->floorpic;
		}
		else
		{
			if(height) *height = iter->ceilingheight;
			if(pic) *pic = iter->ceilingpic;
		}
		return true;
	}

	if(ref == SPREF_MY_FLOOR)
	{
		if(!actline || !actline->frontsector) return false;
		// Actline's front floor.
		if(height) *height = actline->frontsector->floorheight;
		if(pic) *pic = actline->frontsector->floorpic;
		if(planesector) *planesector = actline->frontsector;
		return true;
	}
	if(ref == SPREF_MY_CEILING) 
	{
		if(!actline || !actline->frontsector) return false;
		// Actline's front ceiling.
		if(height) *height = actline->frontsector->ceilingheight;
		if(pic) *pic = actline->frontsector->ceilingpic;
		if(planesector) *planesector = actline->frontsector;
		return true;
	}
	if(ref == SPREF_ORIGINAL_FLOOR)
	{
		if(height) *height = sector->origfloor;
		if(pic) *pic = sector->floorpic;
		return true;
	}
	if(ref == SPREF_ORIGINAL_CEILING)
	{
		if(height) *height = sector->origceiling;
		if(pic) *pic = sector->ceilingpic;
		return true;
	}
	if(ref == SPREF_CURRENT_FLOOR)
	{
		if(height) *height = sector->floorheight;
		if(pic) *pic = sector->floorpic;
		return true;
	}
	if(ref == SPREF_CURRENT_CEILING)
	{
		if(height) *height = sector->ceilingheight;
		if(pic) *pic = sector->ceilingpic;
		return true;
	}
	// Texture targets?
	if(ref >= SPREF_MIN_BOTTOM_TEXTURE
		&& ref <= SPREF_MAX_TOP_TEXTURE)
	{
		int part;
		num = 0;
		// Which part of the wall are we looking at?
		if(ref == SPREF_MIN_MID_TEXTURE || ref == SPREF_MAX_MID_TEXTURE)
			part = 1;
		else if(ref == SPREF_MIN_TOP_TEXTURE || ref == SPREF_MAX_TOP_TEXTURE)
			part = 0;
		else // Then it's the bottom.
			part = 2;
		// Get the heights of the sector's textures.
		// The heights are in real world coordinates.
		for(i=0, k=0; i<sector->linecount; i++)
		{
			k = XS_TextureHeight(sector->Lines[i], part);
			if(k != DDMAXINT) heights[num++] = k;
		}
		if(!num) return true;
		// Are we looking at the min or max heights?
		if(ref >= SPREF_MIN_BOTTOM_TEXTURE 
			&& ref <= SPREF_MIN_TOP_TEXTURE)
		{
			i = FindMinOf(heights, num);
			if(height) *height = heights[i];
			return true;
		}
		i = FindMaxOf(heights, num);
		if(height) *height = heights[i];
		return true;
	}
	// We need to figure out the heights of the adjoining sectors.
	// The results will be stored in the heights array.
	ceiling = (ref == SPREF_HIGHEST_CEILING
		|| ref == SPREF_LOWEST_CEILING 
		|| ref == SPREF_NEXT_HIGHEST_CEILING
		|| ref == SPREF_NEXT_LOWEST_CEILING);
	num = XS_AdjoiningPlanes(sector, ceiling, heights, pics, NULL, sectorlist);

	if(!num)
	{
		// Add self.
		heights[0] = ceiling? sector->ceilingheight : sector->floorheight;
		pics[0] = ceiling? sector->ceilingpic : sector->floorpic;
		sectorlist[0] = sector;
		num = 1;
	}

	// Get the right height and pic.
	if(ref == SPREF_HIGHEST_CEILING || ref == SPREF_HIGHEST_FLOOR)
		i = FindMaxOf(heights, num);

	if(ref == SPREF_LOWEST_CEILING || ref == SPREF_LOWEST_FLOOR)
		i = FindMinOf(heights, num);

	if(ref == SPREF_NEXT_HIGHEST_CEILING)
		i = FindNextOf(heights, num, sector->ceilingheight);

	if(ref == SPREF_NEXT_HIGHEST_FLOOR)
		i = FindNextOf(heights, num, sector->floorheight);

	if(ref == SPREF_NEXT_LOWEST_CEILING)
		i = FindPrevOf(heights, num, sector->ceilingheight);

	if(ref == SPREF_NEXT_LOWEST_FLOOR)
		i = FindPrevOf(heights, num, sector->floorheight);

	// The requested plane was not found.
	if(i == -1) return false;

	// Set the values.
	if(height) *height = heights[i];
	if(pic) *pic = pics[i];
	if(planesector) *planesector = sectorlist[i];
	return true;
}

int XSTrav_HighestSectorType(sector_t *sec, boolean ceiling, int data, 
							 void *context)
{
	int *type = context;
	if(sec->special > *type) *type = sec->special;
	return true; // Keep looking...
}

int XSTrav_MovePlane(sector_t *sector, boolean ceiling, 
					 int data, void *context)
{
	line_t *line = (line_t*) data;
	linetype_t *info = (linetype_t*) context;
	xgplanemover_t *mover;
	int flat, st;
	boolean playsound = line->xg->idata;
	
	XG_Dev("XSTrav_MovePlane: Sector %i (by line %i of type %i)", 
		sector - sectors, line - lines, info->id);

	// i2: destination type (zero, relative to current, surrounding 
	//     highest/lowest floor/ceiling)
	// i3: flags (PMF_*)
	// i4: start sound
	// i5: end sound
	// i6: move sound
	// i7: start texture origin (uses same ids as i2)
	// i8: start texture index (used with PMD_ZERO).
	// i9: end texture origin (uses same ids as i2)
	// i10: end texture (used with PMD_ZERO)
	// f0: move speed (units per tic).
	// f1: crush speed (units per tic).
	// f2: destination offset
	// f3: move sound min interval (seconds)
	// f4: move sound max interval (seconds)
	// f5: time to wait before starting the move
	// f6: wait increment for each plane that gets moved

	mover = XS_GetPlaneMover(sector, ceiling);				
	mover->origin = line;
		
	// Setup the thinker and add it to the list.
	XS_GetPlane(line, sector, info->iparm[2], 0, &mover->destination, 0, 0);
	mover->destination += FRACUNIT * info->fparm[2];
	mover->speed = FRACUNIT * info->fparm[0];
	mover->crushspeed = FRACUNIT * info->fparm[1];
	mover->mininterval = FLT2TIC(info->fparm[3]);
	mover->maxinterval = FLT2TIC(info->fparm[4]);
	mover->flags = info->iparm[3];
	mover->endsound = playsound? info->iparm[5] : 0;
	mover->movesound = playsound? info->iparm[6] : 0;
	if(!XS_GetPlane(line, sector, info->iparm[9], 0, 0, &mover->setflat, 0))
		mover->setflat = info->iparm[10];

	// Init timer.
	mover->timer = XG_RandomInt(mover->mininterval, mover->maxinterval);

	// Do we need to wait before starting the move?
	if(line->xg->fdata > 0)
	{
		mover->timer = FLT2TIC(line->xg->fdata);				
		mover->flags |= PMF_WAIT;
	}
	// Increment wait time.
	line->xg->fdata += info->fparm[6];

	// Add the thinker if necessary.
	P_AddThinker(&mover->thinker);

	// Do start stuff. Play sound?
	if(playsound)
		XS_SectorSound(sector, info->iparm[4]);

	// Change texture?
	if(!XS_GetPlane(line, sector, info->iparm[7], 0, 0, &flat, 0))
		flat = info->iparm[8];
	if(flat > 0) XS_ChangePlaneTexture(sector, ceiling, flat);

	// Should we play no more sounds?
	if(info->iparm[3] & PMF_ONE_SOUND_ONLY)
	{
		// Sound was played only for the first plane.
		line->xg->idata = false;
	}

	// i11 + i12: (plane ref) start sector type
	// i13 + i14: (plane ref) end sector type

	// Change sector type right now?
	st = info->iparm[12];
	if(XL_TraversePlanes(line, info->iparm[11], info->iparm[12], 0, &st,
		XSTrav_HighestSectorType))
	{
		XS_SetSectorType(sector, st);
	}
	else
	{
		XG_Dev("XSTrav_MovePlane: SECTOR TYPE NOT SET (nothing referenced)");
	}

	// Change sector type in the end of move?
	st = info->iparm[14];
	if(XL_TraversePlanes(line, info->iparm[13], info->iparm[14], 0, &st,
		XSTrav_HighestSectorType))
	{
		// OK, found one or more.
		mover->setsector = st;
	}
	else
	{
		XG_Dev("XSTrav_MovePlane: SECTOR TYPE WON'T BE SET (nothing "
			"referenced)");
	}

	return true; // Keep looking.
}

void XS_InitStairBuilder(void)
{
	memset(builder, 0, numsectors);
}

boolean XS_DoBuild(sector_t *sector, boolean ceiling, line_t *origin,
				   linetype_t *info, int stepcount)
{
	static int firstheight;
	int secnum = sector - sectors;
	xgplanemover_t *mover;
	float waittime;

	// Make sure each sector is only processed once.
	if(builder[secnum] & BL_BUILT) return false; // Already built this one!
	builder[secnum] |= BL_WAS_BUILT;

	// Create a new mover for the plane.
	mover = XS_GetPlaneMover(sector, ceiling);
	mover->origin = origin;

	// Setup the mover. 
	if(!stepcount)
		firstheight = ceiling? sector->ceilingheight : sector->floorheight;
	
	mover->destination = firstheight + (stepcount+1)*info->fparm[1]*FRACUNIT;
	mover->speed = FRACUNIT * (info->fparm[0] + stepcount*info->fparm[6]);
	if(mover->speed <= 0) mover->speed = FRACUNIT/1000;
	mover->mininterval = FLT2TIC(info->fparm[4]);
	mover->maxinterval = FLT2TIC(info->fparm[5]);
	if(info->iparm[8]) mover->flags = PMF_CRUSH;
	mover->endsound = info->iparm[6];
	mover->movesound = info->iparm[7];

	// Wait before starting?
	waittime = info->fparm[2] + info->fparm[3] * stepcount;
	if(waittime > 0)
	{
		mover->timer = FLT2TIC(waittime);
		mover->flags |= PMF_WAIT;
		// Play start sound when waiting ends.
		mover->startsound = info->iparm[5];
	}
	else
	{
		mover->timer = XG_RandomInt(mover->mininterval, mover->maxinterval);
		// Play step start sound immediately.
		XS_SectorSound(sector, info->iparm[5]);
	}

	// Do start stuff. Play sound?
	if(!stepcount)
	{
		// Start building start sound.
		XS_SectorSound(sector, info->iparm[4]);
	}

	P_AddThinker(&mover->thinker);

	// Building has begun!
	return true;
}

int XSTrav_BuildStairs(sector_t *sector, boolean ceiling, 
					 int data, void *context)
{
	boolean found = true;
	int i, k, lowest, stepcount = 0;
	line_t *line;
	line_t *origin = (line_t*) data;
	linetype_t *info = context;
	boolean picstop = info->iparm[2] != 0;
	boolean spread = info->iparm[3] != 0;
	int mypic;

	XG_Dev("XSTrav_BuildStairs: Sector %i, %s", sector - sectors,
		ceiling? "ceiling" : "floor");

	// i2: (true/false) stop when texture changes
	// i3: (true/false) spread build? 

	mypic = ceiling? sector->ceilingpic : sector->floorpic;

	// Apply to first step.
	XS_DoBuild(sector, ceiling, origin, info, stepcount);

	while(found)
	{
		stepcount++;
		
		// Mark the sectors of the last step as processed.
		for(i=0; i<numsectors; i++)			
			if(builder[i] & BL_WAS_BUILT) 
			{
				builder[i] &= ~BL_WAS_BUILT;
				builder[i] |= BL_BUILT;
			}
			
		// Scan the sectors for the next ones to spread to.
		found = false;
		lowest = numlines;
		for(i=0; i<numsectors; i++)
		{
			// Only spread from built sectors (spread only once!).
			if(!(builder[i] & BL_BUILT)
				|| builder[i] & BL_SPREADED) continue;

			builder[i] |= BL_SPREADED;
			
			// Any 2-sided lines facing the right way?
			for(k=0; k<sectors[i].linecount; k++)
			{
				line = sectors[i].Lines[k];
				if(!line->frontsector || !line->backsector) continue;
				if(line->frontsector != sectors + i) continue;
				if(picstop)
				{
					// Planepic must match.
					if((ceiling? sectors[i].ceilingpic 
						: sectors[i].floorpic) != mypic) continue;
				}
				// Don't spread to sectors which have already spreaded.
				if(builder[line->backsector - sectors] & BL_SPREADED)
					continue;
				// Build backsector.
				found = true;
				if(spread)
				{
					XS_DoBuild(line->backsector, ceiling, origin, info, 
						stepcount);
				}
				else
				{
					// We need the lowest line number.
					if(line - lines < lowest)
						lowest = line - lines;
				}
			}
		}
		if(!spread && found)
		{
			XS_DoBuild(lines[lowest].backsector, ceiling, origin, info,
				stepcount);
		}
	}
	return true; // Continue searching for planes.	
}

int XSTrav_SectorSound(struct sector_s *sec, boolean ceiling, int data,
					   void *context)
{
	XS_SectorSound(sec, data);
	return true;
}

int XSTrav_PlaneTexture(struct sector_s *sec, boolean ceiling, int data,
						void *context)
{
	line_t *line = (line_t*) data;
	linetype_t *info = context;
	int pic;

	// i2: (spref) texture origin
	// i3: texture number (flat), used with SPREF_NONE

	if(!XS_GetPlane(line, sec, info->iparm[2], 0, 0, &pic, 0))
		pic = info->iparm[3];
	else
		XG_Dev("XSTrav_PlaneTexture: Sector %i, couldn't find suitable",
			sec - sectors);

	// Set the texture.
	XS_ChangePlaneTexture(sec, ceiling, pic);

	return true;
}

int XSTrav_SectorType(struct sector_s *sec, boolean ceiling, int data,
					  void *context)
{
	XS_SetSectorType(sec, data);
	return true;
}

int XSTrav_SectorLight(sector_t *sector, boolean ceiling, int data,
					   void *context)
{
	line_t *line = (line_t*) data;
	linetype_t *info = context;
	int num, levels[MAX_VALS], i;
	int uselevel = sector->lightlevel;
	byte usergb[3];

	// i2: (true/false) set level
	// i3: (true/false) set RGB
	// i4: source of light level (SSREF*)
	// i5: offset
	// i6: source of RGB (none, my, original)
	// i7: red offset
	// i8: green offset
	// i9: blue offset

	if(info->iparm[2])
	{
		switch(info->iparm[4])
		{
		case LIGHTREF_NONE:
			uselevel = 0;
			break;
			
		case LIGHTREF_MY:
			uselevel = line->frontsector->lightlevel;
			break;
			
		case LIGHTREF_ORIGINAL:
			uselevel = sector->origlight;
			break;
			
		case LIGHTREF_HIGHEST:
		case LIGHTREF_LOWEST:
		case LIGHTREF_NEXT_HIGHEST:
		case LIGHTREF_NEXT_LOWEST:
			num = XS_AdjoiningPlanes(sector, ceiling, NULL, NULL, levels, 
				NULL);
			
			// Were there adjoining sectors?
			if(!num) break;
			
			switch(info->iparm[4])
			{
			case LIGHTREF_HIGHEST:
				i = FindMaxOf(levels, num);
				break;
				
			case LIGHTREF_LOWEST:
				i = FindMinOf(levels, num);
				break;
				
			case LIGHTREF_NEXT_HIGHEST:
				i = FindNextOf(levels, num, uselevel);
				break;
				
			case LIGHTREF_NEXT_LOWEST:
				i = FindPrevOf(levels, num, uselevel);
				break;
			}
			if(i >= 0) uselevel = levels[i];
			break;
			
		default:
			break;
		}
		
		// Set the level (plus offset).
		sector->lightlevel = uselevel + info->iparm[5];
		if(sector->lightlevel < 0) sector->lightlevel = 0;
		if(sector->lightlevel > 255) sector->lightlevel = 255;
	}
	if(info->iparm[3])
	{
		switch(info->iparm[6])
		{
		case LIGHTREF_MY:
			memcpy(usergb, line->frontsector->rgb, 3);
			break;

		case LIGHTREF_ORIGINAL:
			memcpy(usergb, sector->origrgb, 3);
			break;

		default:
			memset(usergb, 0, 3);
			break;
		}
		for(num = 0; num < 3; num++)
		{
			i = usergb[num] + info->iparm[7+num];
			if(i < 0) i = 0; 
			if(i > 255) i = 255;
			sector->rgb[num] = i;
		}
	}
	return true;
}

int XSTrav_MimicSector
	(sector_t *sector, boolean ceiling, int data, void *context)
{
	line_t *line = (line_t*) data;
	linetype_t *info = context;
	sector_t *from = NULL;
	int refdata;

	// Set the spref data parameter (tag or index).
	switch(info->iparm[2])
	{
	case SPREF_TAGGED_FLOOR:
	case SPREF_TAGGED_CEILING:
	case SPREF_INDEX_FLOOR:
	case SPREF_INDEX_CEILING:
		refdata = info->iparm[3];
		break;

	case SPREF_ACT_TAGGED_FLOOR:
	case SPREF_ACT_TAGGED_CEILING:
		refdata = info->act_tag;
		break;

	default:
		refdata = 0;
		break;
	}
	
	// If can't apply to a sector, just skip it.
	if(!XS_GetPlane(line, sector, info->iparm[2], refdata, 0, 0, &from))
	{
		XG_Dev("XSTrav_MimicSector: No suitable neighbor "
			"for %i.\n", sector - sectors);
		return true;
	}

	// Mimicing itself is pointless.
	if(from == sector) return true;

	XG_Dev("XSTrav_MimicSector: Sector %i mimicking sector %i",
		sector - sectors, from - sectors);

	// Copy the properties of the target sector.
	sector->lightlevel = from->lightlevel;
	memcpy(sector->rgb, from->rgb, sizeof(from->rgb));
	memcpy(sector->reverb, from->reverb, sizeof(from->reverb));
	memcpy(sector->planes, from->planes, sizeof(from->planes));
	sector->ceilingpic = from->ceilingpic;
	sector->floorpic = from->floorpic;
	sector->ceilingheight = from->ceilingheight;
	sector->floorheight = from->floorheight;
	sector->flooroffx = from->flooroffx;
	sector->flooroffy = from->flooroffy;
	sector->ceiloffx = from->ceiloffx;
	sector->ceiloffy = from->ceiloffy;
	P_ChangeSector(sector, false);

	// Copy type as well.
	XS_SetSectorType(sector, from->special);
	if(from->xg)
	{
		memcpy(sector->xg, from->xg, sizeof(xgsector_t));
	}
	
	return true;
}

int XF_FindRewindMarker(char *func, int pos)
{
	while(pos > 0 && func[pos] != '>') pos--;
	if(func[pos] == '>') pos++;
	return pos;
}

int XF_GetCount(function_t *fn, int *pos)
{
	char *end;
	int count;

	count = strtol(fn->func + *pos, &end, 10);
	*pos = end - fn->func;
	return count;
}

float XF_GetValue(function_t *fn, int pos)
{
	int ch;
		
	if(fn->func[pos] == '/' || fn->func[pos] == '%')
	{
		// Exact value.
		return strtod(fn->func + pos+1, 0);
	}

	ch = tolower(fn->func[pos]);
	// A=0, Z=25.
	return (ch-'a') / 25.0f;
}

/*
 * XF_FindNextPos
 *	Returns the position of the next value.
 *	Repeat counting is handled here.
 *	Poke should be true only if fn->pos is really about to move.
 */
int XF_FindNextPos(function_t *fn, int pos, boolean poke, sector_t *sec)
{
	int startpos = pos;
	int c;
	char *ptr;

	if(fn->repeat > 0)
	{
		if(poke) fn->repeat--;
		return pos;
	}

	// Skip current.
	if(fn->func[pos] == '/' || fn->func[pos] == '%')
	{
		strtod(fn->func + pos+1, &ptr);
		pos = ptr - fn->func; // Go to the end.
	}
	else // It's just a normal character [a-z,A-Z].		
		pos++;

	while(pos != startpos && fn->func[pos])
	{
		// Check for various special characters.
		if(isdigit(fn->func[pos]))
		{
			// A repeat! Move pos to the value to be repeated and set
			// repeat counter.
			c = XF_GetCount(fn, &pos) - 1;
			if(poke) fn->repeat = c;
			return pos;
		}
		if(fn->func[pos] == '!')	// Chain event.
		{
			pos++;
			c = XF_GetCount(fn, &pos);
			if(poke)
			{
				XS_DoChain(sec, XSCE_FUNCTION, c, &dummything);
			}
			continue;
		}
		if(fn->func[pos] == '#')	// Set timer.
		{
			pos++;
			c = XF_GetCount(fn, &pos);
			if(poke)
			{
				fn->timer = 0;
				fn->maxtimer = c; 
			}
			continue;
		}
		if(fn->func[pos] == '?')	// Random timer.
		{
			pos++;
			c = XF_GetCount(fn, &pos);
			if(poke)
			{
				fn->timer = 0;
				fn->maxtimer = XG_RandomInt(0, c);
			}
			continue;
		}
		if(fn->func[pos] == '<')	// Rewind.
		{
			pos = XF_FindRewindMarker(fn->func, pos);
			continue;
		}
		if(poke)
		{
			if(islower(fn->func[pos]) || fn->func[pos] == '/')
			{
				int next = XF_FindNextPos(fn, pos, false, sec);
				if(fn->func[next] == '.')
				{
					pos++;
					continue;
				}
				break;
			}
		}
		else if(fn->func[pos] == '.') break;
		// Is it a value, then?
		if(isalpha(fn->func[pos]) 
			|| fn->func[pos] == '/'	
			|| fn->func[pos] == '%') break;
		// A bad character, skip it.
		pos++;
	}

	return pos;
}

// Tick the function, update value.
void XF_Ticker(function_t *fn, sector_t *sec)
{
	int next;
	float inter;

	// Store the previous value of the function.
	fn->oldvalue = fn->value;

	// Is there a function?
	if(!ISFUNC(fn) || fn->link) return;

	// Increment time.
	if(fn->timer++ >= fn->maxtimer)
	{
		fn->timer = 0;
		fn->maxtimer = XG_RandomInt(fn->mininterval, fn->maxinterval);

		// Advance to next pos.
		fn->pos = XF_FindNextPos(fn, fn->pos, true, sec);
	}

	// abcdefghijlkmnopqrstuvwxyz (26)

	// az.<	(fade from 0 to 1, break interpolation and repeat)
	// [note that AZ.AZ is the same as AZAZ]
	// [also note that a.z is the same as z]
	// az.>mz< (fade from 0 to 1, break, repeat fade from
	//   0.5 to 1 to 0.5)
	// 10a10z is the same as aaaaaaaaaazzzzzzzzzz
	// aB or Ba do not interpolate
	// zaN (interpolate from 1 to 0, wait at 0, stay at N)
	// za.N (interpolate from 1 to 0, skip to N)
	// 1A is the same as A

	// Stop?
	if(!fn->func[fn->pos]) return;

	if(isupper(fn->func[fn->pos]) || fn->func[fn->pos] == '%')
	{
		// No interpolation.
		fn->value = XF_GetValue(fn, fn->pos);
	}
	else
	{
		inter = 0;
		next = XF_FindNextPos(fn, fn->pos, false, sec);
		if(islower(fn->func[next]) || fn->func[next] == '/')
		{
			if(fn->maxtimer) inter = fn->timer / (float) fn->maxtimer;
		}
		fn->value = (1-inter) * XF_GetValue(fn, fn->pos)
			+ inter * XF_GetValue(fn, next);
	}	

	// Scale and offset.
	fn->value = fn->value*fn->scale + fn->offset;
}

void XS_UpdatePlanes(sector_t *sec)
{
	xgsector_t *xg = sec->xg;
	function_t *fn;
	int i;
	boolean docrush = (xg->info.flags & STF_CRUSH) != 0;

	// Update floor.
	fn = &xg->plane[XGSP_FLOOR];
	if(UPDFUNC(fn))	// Changed?
	{
		// How much change?
		i = FRACUNIT * fn->value - sec->floorheight;
		if(i)
			// Move the floor plane accordingly.
			T_MovePlane(sec, abs(i), FRACUNIT * fn->value, docrush, 0, SIGN(i));
	}

	// Update celing.
	fn = &xg->plane[XGSP_CEILING];
	if(UPDFUNC(fn))	// Changed?
	{
		// How different?
		i = FRACUNIT * fn->value - sec->ceilingheight;
		if(i)
			// Move the ceiling accordingly.
			T_MovePlane(sec, abs(i), FRACUNIT * fn->value, docrush, 1, SIGN(i));
	}
}

void XS_UpdateLight(sector_t *sec)
{
	xgsector_t *xg = sec->xg;
	function_t *fn;
	int i, c;

	// Light intensity.
	fn = &xg->light;
	if(UPDFUNC(fn))
	{
		sec->lightlevel = fn->value;
		if(sec->lightlevel < 0) sec->lightlevel = 0;
		if(sec->lightlevel > 255) sec->lightlevel = 255;
	}

	// Red, green and blue.
	for(i = 0; i < 3; i++)
	{
		fn = &xg->rgb[i];
		if(!UPDFUNC(fn)) continue;
		c = fn->value;
		if(c < 0) c = 0;
		if(c > 255) c = 255;
		sec->rgb[i] = c;
	}
}

void XS_DoChain(sector_t *sec, int ch, int activating, void *act_thing)
{
	xgsector_t *xg = sec->xg;
	sectortype_t *info = &xg->info;
	float flevtime = TIC2FLT(leveltime);
	line_t line;
	xgline_t xgline;
	linetype_t *ltype;

/*
	XG_Dev("XS_DoChain: %s, sector %i (activating=%i)",
		  ch == XSCE_FLOOR? "FLOOR" 
		: ch == XSCE_CEILING? "CEILING" 
		: ch == XSCE_INSIDE? "INSIDE" 
		: ch == XSCE_TICKER? "TICKER" 
		: ch == XSCE_FUNCTION? "FUNCTION" 
		: "???", sec - sectors, activating);
*/

	if(ch < XSCE_NUM_CHAINS)
	{
		// How's the counter?
		if(!info->count[ch]) return;

		// How's the time?
		if(flevtime < info->start[ch] ||
			(info->end[ch] > 0 && flevtime > info->end[ch])) 
			return; // Not operating at this time.

		// Time to try the chain. Reset timer.
		xg->chain_timer[ch] = XG_RandomInt(FLT2TIC(info->interval[ch][0]),
			FLT2TIC(info->interval[ch][1]));
	}

	// Prepare the dummy line to use for the event.
	memset(&line, 0, sizeof(line));
	memset(&xgline, 0, sizeof(xgline));

	line.frontsector = sec;
	line.sidenum[0] = -1;
	line.sidenum[1] = -1;
	line.special = (ch == XSCE_FUNCTION? activating : info->chain[ch]);
	line.tag = sec->tag;
	line.xg = &xgline;
	ltype = XL_GetType(line.special);
	if(!ltype)
	{
		// What is this? There is no such XG line type.
		XG_Dev("XS_DoChain: Unknown XG line type %i", line.special);
		return;
	}
	memcpy(&line.xg->info, ltype, sizeof(*ltype));
	xgline.activator = act_thing;
	xgline.active = (ch == XSCE_FUNCTION? false : !activating);

	// Send the event.
	if(XL_LineEvent(XLE_CHAIN, 0, &line, 0, act_thing))
	{
		// A success! 
		if(ch < XSCE_NUM_CHAINS)
		{
			// Decrease counter.
			if(info->count[ch] > 0) 
			{
				info->count[ch]--;		

				XG_Dev("XS_DoChain: %s, sector %i (activating=%i): Counter now at %i",
					  ch == XSCE_FLOOR? "FLOOR" 
					: ch == XSCE_CEILING? "CEILING" 
					: ch == XSCE_INSIDE? "INSIDE" 
					: ch == XSCE_TICKER? "TICKER" 
					: ch == XSCE_FUNCTION? "FUNCTION" 
					: "???", sec - sectors, activating, info->count[ch]);
			}
		}
	}
}

int XSTrav_SectorChain(sector_t *sec, mobj_t *mo, int ch)
{
	xgsector_t *xg = sec->xg;
	sectortype_t *info = &xg->info;
	player_t *player = mo->player;
	int flags = info->chain_flags[ch];
	boolean activating;

	// Check mobj type.
	if(flags & (SCEF_ANY_A | SCEF_ANY_D 
		| SCEF_TICKER_A | SCEF_TICKER_D)) goto type_passes;
	if(flags & (SCEF_PLAYER_A | SCEF_PLAYER_D)
		&& player) goto type_passes;
	if(flags & (SCEF_OTHER_A | SCEF_OTHER_D)
		&& !player) goto type_passes;
	if(flags & (SCEF_MONSTER_A | SCEF_MONSTER_D)
		&& mo->flags & MF_COUNTKILL) goto type_passes;
	if(flags & (SCEF_MISSILE_A | SCEF_MISSILE_D)
		&& mo->flags & MF_MISSILE) 
	{
		goto type_passes;
	}

	// Wrong type, continue looking.
	return true;

type_passes:

	// Are we looking for an activation effect?
	if(player)
		activating = !(flags & SCEF_PLAYER_D);
	else if(mo->flags & MF_COUNTKILL)
		activating = !(flags & SCEF_MONSTER_D);
	else if(mo->flags & MF_MISSILE)
		activating = !(flags & SCEF_MISSILE_D);
	else if(flags & (SCEF_ANY_A | SCEF_ANY_D))
		activating = !(flags & SCEF_ANY_D);
	else 
		activating = !(flags & SCEF_OTHER_D);

	// Check for extra requirements (touching).
	switch(ch)
	{
	case XSCE_FLOOR:
		// Is it touching the floor?
		if(mo->z > sec->floorheight) return true; 		
		break;

	case XSCE_CEILING:
		// Is it touching the ceiling?
		if(mo->z + mo->height < sec->ceilingheight) return true;
		break;

	default:
		break;
	}

	XS_DoChain(sec, ch, activating, mo);

	// Continue looking.
	return true;
}

int XSTrav_Wind(sector_t *sec, mobj_t *mo, int data)
{
	sectortype_t *info = &sec->xg->info;
	float ang = PI * info->wind_angle / 180;

	if(IS_CLIENT)
	{
		// Clientside wind only affects the local player.
		if(!mo->player || mo->player != &players[consoleplayer])
			return true;
	}

	// Does wind affect this sort of things?
	if((info->flags & STF_PLAYER_WIND && mo->player)
	   || (info->flags & STF_OTHER_WIND && !mo->player)
	   || (info->flags & STF_MONSTER_WIND && mo->flags & MF_COUNTKILL)
	   || (info->flags & STF_MISSILE_WIND && mo->flags & MF_MISSILE))
	{
		if(!(info->flags & (STF_FLOOR_WIND | STF_CEILING_WIND))
		   || (info->flags & STF_FLOOR_WIND && mo->z <= mo->floorz)
		   || (info->flags & STF_CEILING_WIND &&
			   mo->z + mo->height >= mo->ceilingz))
		{
			// Apply vertical wind.
			mo->momz += FRACUNIT * info->vertical_wind;
			
			// Horizontal wind.
			mo->momx += FRACUNIT * cos(ang) * info->wind_speed;
			mo->momy += FRACUNIT * sin(ang) * info->wind_speed;
		}
	}

	// Continue applying wind.
	return true;
}

// Returns true if true was returned for each mobj.
int XS_TraverseMobjs(sector_t *sec, int data, int (*func)(sector_t *sec,
					 mobj_t *mo, int data))
{
	mobj_t *mo;

	// Only traverse sectorlinked things.
	for(mo = sec->thinglist; mo; mo = mo->snext)
		if(!func(sec, mo, data)) return false;
	return true;
}

/*
 * Makes sure the offset is in the range 0..64.
 */
void XS_ConstrainPlaneOffset(float *offset)
{
	if(*offset > 64) *offset -= 64;
	if(*offset < 0) *offset += 64;
}

/*
 * XS_Think
 *	Called for Extended Generalized sectors.
 */
void XS_Think(sector_t *sector)
{
	xgsector_t *xg = sector->xg;
	sectortype_t *info = &xg->info;
	int i;
	float ang;
	
	if(xg->disabled) return; // This sector is disabled.

	if(!IS_CLIENT)
	{
		// Function tickers.
		for(i = 0; i < 2; i++) XF_Ticker(&xg->plane[i], sector);
		XF_Ticker(&xg->light, sector);
		for(i = 0; i < 3; i++) XF_Ticker(&xg->rgb[i], sector);

		// Update linked functions.
		for(i = 0; i < 3; i++)
		{
			if(i < 2 && xg->plane[i].link)
				xg->plane[i].value = xg->plane[i].link->value;
			if(xg->rgb[i].link)
				xg->rgb[i].value = xg->rgb[i].link->value;
		}
		if(xg->light.link)
			xg->light.value = xg->light.link->value;

		// Update planes.
		XS_UpdatePlanes(sector);

		// Update sector light.
		XS_UpdateLight(sector);

		// Decrement chain timers.
		for(i = 0; i < XSCE_NUM_CHAINS; i++) xg->chain_timer[i]--;

		// Floor chain. Check any mobjs that are touching the floor of the sector.
		if(info->chain[XSCE_FLOOR] 
			&& xg->chain_timer[XSCE_FLOOR] <= 0)
		{
			XS_TraverseMobjs(sector, XSCE_FLOOR, XSTrav_SectorChain);
		}

		// Ceiling chain. Check any mobjs that are touching the ceiling.
		if(info->chain[XSCE_CEILING]
			&& xg->chain_timer[XSCE_CEILING] <= 0)
		{
			XS_TraverseMobjs(sector, XSCE_CEILING, XSTrav_SectorChain);
		}

		// Inside chain. Check any sectorlinked mobjs.
		if(info->chain[XSCE_INSIDE]
			&& xg->chain_timer[XSCE_INSIDE] <= 0)
		{
			XS_TraverseMobjs(sector, XSCE_INSIDE, XSTrav_SectorChain);
		}

		// Ticker chain. Send an activate event if TICKER_D flag is not set.
		if(info->chain[XSCE_TICKER]
			&& xg->chain_timer[XSCE_TICKER] <= 0)
		{
			XS_DoChain(sector, XSCE_TICKER, 
				!(info->chain_flags[XSCE_TICKER] & SCEF_TICKER_D),
				&dummything);
		}

		// Play ambient sounds.
		if(xg->info.ambient_sound)
		{
			if(xg->timer-- < 0)
			{
				xg->timer = XG_RandomInt(FLT2TIC(xg->info.sound_interval[0]), 
					FLT2TIC(xg->info.sound_interval[1]));
				S_SectorSound(sector, xg->info.ambient_sound);
			}
		}
	}

	// Texture movement (floor & ceiling).
	ang = PI * xg->info.texmove_angle[0] / 180;
	sector->flooroffx -= cos(ang) * xg->info.texmove_speed[0];
	sector->flooroffy -= sin(ang) * xg->info.texmove_speed[0];
	
	ang = PI * xg->info.texmove_angle[1] / 180;
	sector->ceiloffx -= cos(ang) * xg->info.texmove_speed[1];
	sector->ceiloffy -= sin(ang) * xg->info.texmove_speed[1];

	// Wind for all sectorlinked mobjs.
	if(xg->info.wind_speed || xg->info.vertical_wind)
	{
		XS_TraverseMobjs(sector, 0, XSTrav_Wind);
	}
}

void XS_Ticker(void)
{
	int i;

	for(i = 0; i < numsectors; i++)
	{
		if(!sectors[i].xg) continue; // Normal sector.
		XS_Think(sectors + i);
	}
}

int XS_Gravity(struct sector_s *sector)
{
	if(!sector->xg 
		|| !(sector->xg->info.flags & STF_GRAVITY)) 
		return GRAVITY; // Normal gravity.
	
	return FRACUNIT * sector->xg->info.gravity;
}

int XS_Friction(struct sector_s *sector)
{
	if(!sector->xg 
		|| !(sector->xg->info.flags & STF_FRICTION))
		return 0xe800;	// Normal friction.

	return FRACUNIT * sector->xg->info.friction;
}

// Returns the thrust multiplier caused by friction.
int XS_ThrustMul(struct sector_s *sector)
{
	fixed_t fric = XS_Friction(sector);
	float x;

	if(fric <= 0xe800) return FRACUNIT; // Normal friction.
	if(fric > 0xffff) return 0;			// There's nothing to thrust from!
	x = FIX2FLT(fric);
	// {c = -93.31092643, b = 208.0448223, a = -114.7338958}
	return FRACUNIT * (-114.7338958*x*x + 208.0448223*x - 93.31092643);
}

/*
 * During update, definitions are re-read, so the pointers need to be
 * updated. However, this is a bit messy operation, prone to errors.
 * Instead, we just disable XG.
 */
void XS_Update(void)
{
	int i;

	// It's all PU_LEVEL memory, so we can just lose it.
	for(i = 0; i < numsectors; i++)
		if(sectors[i].xg)
		{
			sectors[i].xg = NULL;
			sectors[i].special = 0;
		}
}

/*
 * Write XG types into a binary file.
 */
int CCmdDumpXG(int argc, char **argv)
{
	FILE *file;

	if(argc != 2)
	{
		Con_Printf("Usage: %s (file)\n", argv[0]);
		Con_Printf("Writes XG line and sector types to the file.\n");
		return true;
	}

	file = fopen(argv[1], "wb");
	if(!file)
	{
		Con_Printf("Can't open \"%s\" for writing.\n", argv[1]);
		return false;
	}
	XG_WriteTypes(file);	
	fclose(file);
	return true;
}

/*
 * $moveplane: Command line interface to the plane mover. 
 */
int CCmdMovePlane(int argc, char **argv)
{
	boolean isCeiling = !stricmp(argv[0], "moveceil");
	boolean isBoth = !stricmp(argv[0], "movesec");
	boolean isOffset = false, isCrusher = false;
	sector_t *sector = NULL;
	fixed_t units = 0, speed = FRACUNIT;
	int i, p;
	xgplanemover_t *mover;
	
	if(argc < 2)
	{
		Con_Printf("Usage: %s (opts)\n", argv[0]);
		Con_Printf("Opts can be:\n");
		Con_Printf("  here [crush] [off] (z/units) [speed]\n");
		Con_Printf("  at (x) (y) [crush] [off] (z/units) [speed]\n");
		Con_Printf("  tag (sector-tag) [crush] [off] (z/units) [speed]\n");
		return true; 		
	}

	if(IS_CLIENT)
	{
		Con_Printf("Clients can't move planes.\n");
		return false;
	}

	// Which mode?
	if(!stricmp(argv[1], "here"))
	{
		p = 2;
		if(!players[consoleplayer].plr->mo) return false;
		sector = players[consoleplayer].plr->mo->subsector->sector;
	}
	else if(!stricmp(argv[1], "at") && argc >= 4)
	{
		p = 4;
		sector = R_PointInSubsector(strtol(argv[2], 0, 0) << FRACBITS,
			strtol(argv[3], 0, 0) << FRACBITS)->sector;
	}
	else if(!stricmp(argv[1], "tag") && argc >= 3)
	{
		p = 3;
		// Find the first sector with the tag.
		for(i = 0; i < numsectors; i++)
			if(sectors[i].tag == (short) strtol(argv[2], 0, 0))
			{
				sector = &sectors[i];
				break;
			}
	}

	// No more arguments?
	if(argc == p)
	{
		Con_Printf("Ceiling = %i\nFloor = %i\n", 
			sector->ceilingheight >> FRACBITS,
			sector->floorheight >> FRACBITS);
		return true;
	}

	// Check for the optional 'crush' parameter.
	if(argc >= p + 1 && !stricmp(argv[p], "crush"))
	{
		isCrusher = true;
		++p;
	}

	// Check for the optional 'off' parameter.
	if(argc >= p + 1 && !stricmp(argv[p], "off"))
	{
		isOffset = true;
		++p;
	}

	// The amount to move.
	if(argc >= p + 1) 
	{
		units = FRACUNIT * strtod(argv[p++], 0);
	}
	else 
	{
		Con_Printf("You must specify Z-units.\n");
		return false; // Required parameter missing.
	}

	// The optional speed parameter.
	if(argc >= p + 1)
	{
		speed = FRACUNIT * strtod(argv[p++], 0);
		// The speed is always positive.
		if(speed < 0) speed = -speed;
	}

	// We must now have found the sector to operate on.
	if(!sector) return false;

	mover = XS_GetPlaneMover(sector, isCeiling);				
		
	// Setup the thinker and add it to the list.
	mover->destination = units + (isOffset?
		 (isCeiling? sector->ceilingheight : sector->floorheight) : 0);

	// Check that the destination is valid.
	if(!isBoth)
	{
		if(isCeiling && mover->destination < sector->floorheight + 4*FRACUNIT)
			mover->destination = sector->floorheight + 4*FRACUNIT;
		if(!isCeiling && mover->destination > sector->ceilingheight - 4*FRACUNIT)
			mover->destination = sector->ceilingheight - 4*FRACUNIT;
	}

	mover->speed = speed;
	if(isCrusher) 
	{
		mover->crushspeed = speed/2; // Crush at half speed.
		mover->flags |= PMF_CRUSH;
	}
	if(isBoth) mover->flags |= PMF_OTHER_FOLLOWS;

	P_AddThinker(&mover->thinker);
	return true;
}
