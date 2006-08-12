
//**************************************************************************
//**
//** p_maputl.c : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile$
//** $Revision$
//** $Date$
//** $Author$
//**
//**************************************************************************

#include "jhexen.h"
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

typedef struct mapInfo_s mapInfo_t;
struct mapInfo_s {
    short   cluster;
    short   warpTrans;
    short   nextMap;
    short   cdTrack;
    char    name[32];
    short   sky1Texture;
    short   sky2Texture;
    fixed_t sky1ScrollDelta;
    fixed_t sky2ScrollDelta;
    boolean doubleSky;
    boolean lightning;
    int     fadetable;
    char    songLump[10];
};

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static int QualifyMap(int map);

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     MapCount;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static mobj_t *RoughBlockCheck(mobj_t *mo, int index);

static mapInfo_t MapInfo[99];
static char *MapCmdNames[] = {
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
static int MapCmdIDs[] = {
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

static int cd_NonLevelTracks[6];    // Non-level specific song cd track numbers

static char *cd_SongDefIDs[] =  // Music defs that correspond the above.
{
    "startup",
    "hall",
    "orb",
    "chess",
    "hub",
    "hexen"
};

/*
   ===================
   =
   = P_AproxDistance
   =
   = Gives an estimation of distance (not exact)
   =
   ===================
 */

/*fixed_t P_AproxDistance (fixed_t dx, fixed_t dy)
   {
   dx = abs(dx);
   dy = abs(dy);
   if (dx < dy)
   return dx+dy-(dx>>1);
   return dx+dy-(dy>>1);
   } */

/*
   ==================
   =
   = P_PointOnLineSide
   =
   = Returns 0 or 1
   ==================
 */

/*int P_PointOnLineSide (fixed_t x, fixed_t y, line_t *line)
   {
   fixed_t dx,dy;
   fixed_t left, right;

   if (!line->dx)
   {
   if (x <= line->v1->x)
   return line->dy > 0;
   return line->dy < 0;
   }
   if (!line->dy)
   {
   if (y <= line->v1->y)
   return line->dx < 0;
   return line->dx > 0;
   }

   dx = (x - line->v1->x);
   dy = (y - line->v1->y);

   left = FixedMul ( line->dy>>FRACBITS , dx );
   right = FixedMul ( dy , line->dx>>FRACBITS );

   if (right < left)
   return 0;               // front side
   return 1;                       // back side
   } */

/*
   =================
   =
   = P_BoxOnLineSide
   =
   = Considers the line to be infinite
   = Returns side 0 or 1, -1 if box crosses the line
   =================
 */

/*int P_BoxOnLineSide (fixed_t *tmbox, line_t *ld)
   {
   int             p1, p2;

   switch (ld->slopetype)
   {
   case ST_HORIZONTAL:
   p1 = tmbox[BOXTOP] > ld->v1->y;
   p2 = tmbox[BOXBOTTOM] > ld->v1->y;
   if (ld->dx < 0)
   {
   p1 ^= 1;
   p2 ^= 1;
   }
   break;
   case ST_VERTICAL:
   p1 = tmbox[BOXRIGHT] < ld->v1->x;
   p2 = tmbox[BOXLEFT] < ld->v1->x;
   if (ld->dy < 0)
   {
   p1 ^= 1;
   p2 ^= 1;
   }
   break;
   case ST_POSITIVE:
   p1 = P_PointOnLineSide (tmbox[BOXLEFT], tmbox[BOXTOP], ld);
   p2 = P_PointOnLineSide (tmbox[BOXRIGHT], tmbox[BOXBOTTOM], ld);
   break;
   case ST_NEGATIVE:
   p1 = P_PointOnLineSide (tmbox[BOXRIGHT], tmbox[BOXTOP], ld);
   p2 = P_PointOnLineSide (tmbox[BOXLEFT], tmbox[BOXBOTTOM], ld);
   break;
   }

   if (p1 == p2)
   return p1;
   return -1;
   } */

/*
   ==================
   =
   = P_PointOnDivlineSide
   =
   = Returns 0 or 1
   ==================
 */

/*int P_PointOnDivlineSide (fixed_t x, fixed_t y, divline_t *line)
   {
   fixed_t dx,dy;
   fixed_t left, right;

   if (!line->dx)
   {
   if (x <= line->x)
   return line->dy > 0;
   return line->dy < 0;
   }
   if (!line->dy)
   {
   if (y <= line->y)
   return line->dx < 0;
   return line->dx > 0;
   }

   dx = (x - line->x);
   dy = (y - line->y);

   // try to quickly decide by looking at sign bits
   if ( (line->dy ^ line->dx ^ dx ^ dy)&0x80000000 )
   {
   if ( (line->dy ^ dx) & 0x80000000 )
   return 1;       // (left is negative)
   return 0;
   }

   left = FixedMul ( line->dy>>8, dx>>8 );
   right = FixedMul ( dy>>8 , line->dx>>8 );

   if (right < left)
   return 0;               // front side
   return 1;                       // back side
   } */

/*
   ==============
   =
   = P_MakeDivline
   =
   ==============
 */

/*void P_MakeDivline (line_t *li, divline_t *dl)
   {
   dl->x = li->v1->x;
   dl->y = li->v1->y;
   dl->dx = li->dx;
   dl->dy = li->dy;
   } */

/*
   ===============
   =
   = P_InterceptVector
   =
   = Returns the fractional intercept point along the first divline
   =
   = This is only called by the addthings and addlines traversers
   ===============
 */

/*fixed_t P_InterceptVector (divline_t *v2, divline_t *v1)
   {
   #if 1
   fixed_t frac, num, den;

   den = FixedMul (v1->dy>>8,v2->dx) - FixedMul(v1->dx>>8,v2->dy);
   if (den == 0)
   return 0;
   //              I_Error ("P_InterceptVector: parallel");
   num = FixedMul ( (v1->x - v2->x)>>8 ,v1->dy) +
   FixedMul ( (v2->y - v1->y)>>8 , v1->dx);
   frac = FixedDiv (num , den);

   return frac;
   #else
   float   frac, num, den, v1x,v1y,v1dx,v1dy,v2x,v2y,v2dx,v2dy;

   v1x = (float)v1->x/FRACUNIT;
   v1y = (float)v1->y/FRACUNIT;
   v1dx = (float)v1->dx/FRACUNIT;
   v1dy = (float)v1->dy/FRACUNIT;
   v2x = (float)v2->x/FRACUNIT;
   v2y = (float)v2->y/FRACUNIT;
   v2dx = (float)v2->dx/FRACUNIT;
   v2dy = (float)v2->dy/FRACUNIT;

   den = v1dy*v2dx - v1dx*v2dy;
   if (den == 0)
   return 0;       // parallel
   num = (v1x - v2x)*v1dy + (v2y - v1y)*v1dx;
   frac = num / den;

   return frac*FRACUNIT;
   #endif
   } */

/*
   ==================
   =
   = P_LineOpening
   =
   = Sets opentop and openbottom to the window through a two sided line
   = OPTIMIZE: keep this precalculated
   ==================
 */

/*fixed_t opentop, openbottom, openrange;
   fixed_t lowfloor;

   void P_LineOpening (line_t *linedef)
   {
   sector_t        *front, *back;

   if (linedef->sidenum[1] == -1)
   {       // single sided line
   openrange = 0;
   return;
   }

   front = linedef->frontsector;
   back = linedef->backsector;

   if (front->ceilingheight < back->ceilingheight)
   opentop = front->ceilingheight;
   else
   opentop = back->ceilingheight;
   if (front->floorheight > back->floorheight)
   {
   openbottom = front->floorheight;
   lowfloor = back->floorheight;
   tmfloorpic = front->floorpic;
   }
   else
   {
   openbottom = back->floorheight;
   lowfloor = front->floorheight;
   tmfloorpic = back->floorpic;
   }

   openrange = opentop - openbottom;
   } */

/*
   ===============================================================================

   THING POSITION SETTING

   ===============================================================================
 */

/*
   ===================
   =
   = P_UnsetThingPosition
   =
   = Unlinks a thing from block map and sectors
   =
   ===================
 */

void P_UnsetThingPosition(mobj_t *thing)
{
    P_UnlinkThing(thing);
}

/*
   ===================
   =
   = P_SetThingPosition
   =
   = Links a thing into both a block and a subsector based on it's x y
   = Sets thing->subsector properly
   =
   ===================
 */

void P_SetThingPosition(mobj_t *thing)
{
    P_LinkThing(thing,
                (!(thing->
                   flags & MF_NOSECTOR) ? DDLINK_SECTOR : 0) | (!(thing->
                                                                  flags &
                                                                  MF_NOBLOCKMAP)
                                                                ?
                                                                DDLINK_BLOCKMAP
                                                                : 0));
}

/*
   ===============================================================================

   BLOCK MAP ITERATORS

   For each line/thing in the given mapblock, call the passed function.
   If the function returns false, exit with false without checking anything else.

   ===============================================================================
 */

/*
   ==================
   =
   = P_BlockLinesIterator
   =
   = The Validcount flags are used to avoid checking lines
   = that are marked in multiple mapblocks, so increment Validcount before
   = the first call to P_BlockLinesIterator, then make one or more calls to it
   ===================
 */

/*boolean P_BlockLinesIterator (int x, int y, boolean(*func)(line_t*) )
   {
   int                     offset;
   //   short           *list;
   //   line_t          *ld;

   int i;
   polyblock_t *polyLink;
   seg_t **tempSeg;
   //   extern polyblock_t **PolyBlockMap;

   if (x < 0 || y<0 || x>=bmapwidth || y>=bmapheight)
   return true;
   offset = y*bmapwidth+x;

   polyLink = PolyBlockMap[offset];
   while(polyLink)
   {
   if(polyLink->polyobj)
   {
   if(polyLink->polyobj->validcount != Validcount)
   {
   polyLink->polyobj->validcount = Validcount;
   tempSeg = polyLink->polyobj->Segs;
   for(i = 0; i < polyLink->polyobj->numSegs; i++, tempSeg++)
   {
   if((*tempSeg)->linedef->validcount == Validcount)
   {
   continue;
   }
   (*tempSeg)->linedef->validcount = Validcount;
   if(!func((*tempSeg)->linedef))
   {
   return false;
   }
   }
   }
   }
   polyLink = polyLink->next;
   }

   offset = *(blockmap+offset);

   for ( list = blockmaplump+offset ; *list != -1 ; list++)
   {
   ld = &lines[*list];
   if (ld->validcount == Validcount)
   continue;               // line has already been checked
   ld->validcount = Validcount;

   if ( !func(ld) )
   return false;
   }

   return gi.BlockLinesIterator(x, y, func);

   //return true;            // everything was checked
   } */

/*
   ==================
   =
   = P_BlockThingsIterator
   =
   ==================
 */

/*boolean P_BlockThingsIterator (int x, int y, boolean(*func)(mobj_t*) )
   {
   mobj_t          *mobj;

   if (x<0 || y<0 || x>=bmapwidth || y>=bmapheight)
   return true;

   for (mobj = blocklinks[y*bmapwidth+x] ; mobj ; mobj = mobj->bnext)
   if (!func( mobj ) )
   return false;

   return true;
   } */

/*
   ===============================================================================

   INTERCEPT ROUTINES

   ===============================================================================
 */

intercept_t intercepts[MAXINTERCEPTS], *intercept_p;

divline_t trace;
boolean earlyout;
int     ptflags;

/*
   ==================
   =
   = PIT_AddLineIntercepts
   =
   = Looks for lines in the given block that intercept the given trace
   = to add to the intercepts list
   = A line is crossed if its endpoints are on opposite sides of the trace
   = Returns true if earlyout and a solid line hit
   ==================
 */

/*boolean PIT_AddLineIntercepts (line_t *ld)
   {
   int                     s1, s2;
   fixed_t         frac;
   divline_t       dl;

   // avoid precision problems with two routines
   if ( trace.dx > FRACUNIT*16 || trace.dy > FRACUNIT*16
   || trace.dx < -FRACUNIT*16 || trace.dy < -FRACUNIT*16)
   {
   s1 = P_PointOnDivlineSide (ld->v1->x, ld->v1->y, &trace);
   s2 = P_PointOnDivlineSide (ld->v2->x, ld->v2->y, &trace);
   }
   else
   {
   s1 = P_PointOnLineSide (trace.x, trace.y, ld);
   s2 = P_PointOnLineSide (trace.x+trace.dx, trace.y+trace.dy, ld);
   }
   if (s1 == s2)
   return true;            // line isn't crossed

   //
   // hit the line
   //
   P_MakeDivline (ld, &dl);
   frac = P_InterceptVector (&trace, &dl);
   if (frac < 0)
   return true;            // behind source

   // try to early out the check
   if (earlyout && frac < FRACUNIT && !ld->backsector)
   return false;   // stop checking

   intercept_p->frac = frac;
   intercept_p->isaline = true;
   intercept_p->d.line = ld;
   intercept_p++;

   return true;            // continue
   }

 */

/*
   ==================
   =
   = PIT_AddThingIntercepts
   =
   ==================
 */
/*
   boolean PIT_AddThingIntercepts (mobj_t  *thing)
   {
   fixed_t         x1,y1, x2,y2;
   int                     s1, s2;
   boolean         tracepositive;
   divline_t       dl;
   fixed_t         frac;

   tracepositive = (trace.dx ^ trace.dy)>0;

   // check a corner to corner crossection for hit

   if (tracepositive)
   {
   x1 = thing->x - thing->radius;
   y1 = thing->y + thing->radius;

   x2 = thing->x + thing->radius;
   y2 = thing->y - thing->radius;
   }
   else
   {
   x1 = thing->x - thing->radius;
   y1 = thing->y - thing->radius;

   x2 = thing->x + thing->radius;
   y2 = thing->y + thing->radius;
   }
   s1 = P_PointOnDivlineSide (x1, y1, &trace);
   s2 = P_PointOnDivlineSide (x2, y2, &trace);
   if (s1 == s2)
   return true;    // line isn't crossed

   dl.x = x1;
   dl.y = y1;
   dl.dx = x2-x1;
   dl.dy = y2-y1;
   frac = P_InterceptVector (&trace, &dl);
   if (frac < 0)
   return true;            // behind source
   intercept_p->frac = frac;
   intercept_p->isaline = false;
   intercept_p->d.thing = thing;
   intercept_p++;

   return true;                    // keep going
   }
 */

/*
   ====================
   =
   = P_TraverseIntercepts
   =
   = Returns true if the traverser function returns true for all lines
   ====================
 */
/*
   boolean P_TraverseIntercepts ( traverser_t func, fixed_t maxfrac )
   {
   int                             count;
   fixed_t                 dist;
   intercept_t             *scan, *in;

   count = intercept_p - intercepts;
   in = 0;                 // shut up compiler warning

   while (count--)
   {
   dist = DDMAXINT;
   for (scan = intercepts ; scan<intercept_p ; scan++)
   if (scan->frac < dist)
   {
   dist = scan->frac;
   in = scan;
   }

   if (dist > maxfrac)
   return true;                    // checked everything in range
   #if 0
   {       // don't check these yet, ther may be others inserted
   in = scan = intercepts;
   for ( scan = intercepts ; scan<intercept_p ; scan++)
   if (scan->frac > maxfrac)
   *in++ = *scan;
   intercept_p = in;
   return false;
   }
   #endif

   if ( !func (in) )
   return false;                   // don't bother going farther
   in->frac = DDMAXINT;
   }

   return true;            // everything was traversed
   }
 */

/*
   ==================
   =
   = P_PathTraverse
   =
   = Traces a line from x1,y1 to x2,y2, calling the traverser function for each
   = Returns true if the traverser function returns true for all lines
   ==================
 */

/*boolean P_PathTraverse (fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2,
   int flags, boolean (*trav) (intercept_t *))
   {
   fixed_t xt1,yt1,xt2,yt2;
   fixed_t xstep,ystep;
   fixed_t partial;
   fixed_t xintercept, yintercept;
   int             mapx, mapy, mapxstep, mapystep;
   int             count;

   earlyout = flags & PT_EARLYOUT;

   Validcount++;
   intercept_p = intercepts;

   if ( ((x1-bmaporgx)&(MAPBLOCKSIZE-1)) == 0)
   x1 += FRACUNIT;                         // don't side exactly on a line
   if ( ((y1-bmaporgy)&(MAPBLOCKSIZE-1)) == 0)
   y1 += FRACUNIT;                         // don't side exactly on a line
   trace.x = x1;
   trace.y = y1;
   trace.dx = x2 - x1;
   trace.dy = y2 - y1;

   x1 -= bmaporgx;
   y1 -= bmaporgy;
   xt1 = x1>>MAPBLOCKSHIFT;
   yt1 = y1>>MAPBLOCKSHIFT;

   x2 -= bmaporgx;
   y2 -= bmaporgy;
   xt2 = x2>>MAPBLOCKSHIFT;
   yt2 = y2>>MAPBLOCKSHIFT;

   if (xt2 > xt1)
   {
   mapxstep = 1;
   partial = FRACUNIT - ((x1>>MAPBTOFRAC)&(FRACUNIT-1));
   ystep = FixedDiv (y2-y1,abs(x2-x1));
   }
   else if (xt2 < xt1)
   {
   mapxstep = -1;
   partial = (x1>>MAPBTOFRAC)&(FRACUNIT-1);
   ystep = FixedDiv (y2-y1,abs(x2-x1));
   }
   else
   {
   mapxstep = 0;
   partial = FRACUNIT;
   ystep = 256*FRACUNIT;
   }
   yintercept = (y1>>MAPBTOFRAC) + FixedMul (partial, ystep);

   if (yt2 > yt1)
   {
   mapystep = 1;
   partial = FRACUNIT - ((y1>>MAPBTOFRAC)&(FRACUNIT-1));
   xstep = FixedDiv (x2-x1,abs(y2-y1));
   }
   else if (yt2 < yt1)
   {
   mapystep = -1;
   partial = (y1>>MAPBTOFRAC)&(FRACUNIT-1);
   xstep = FixedDiv (x2-x1,abs(y2-y1));
   }
   else
   {
   mapystep = 0;
   partial = FRACUNIT;
   xstep = 256*FRACUNIT;
   }
   xintercept = (x1>>MAPBTOFRAC) + FixedMul (partial, xstep);

   //
   // step through map blocks
   // Count is present to prevent a round off error from skipping the break
   mapx = xt1;
   mapy = yt1;

   for (count = 0 ; count < 64 ; count++)
   {
   if (flags & PT_ADDLINES)
   {
   if (!P_BlockLinesIterator (mapx, mapy,PIT_AddLineIntercepts))
   return false;   // early out
   }
   if (flags & PT_ADDTHINGS)
   {
   if (!P_BlockThingsIterator (mapx, mapy,PIT_AddThingIntercepts))
   return false;   // early out
   }

   if (mapx == xt2 && mapy == yt2)
   break;

   if ( (yintercept >> FRACBITS) == mapy)
   {
   yintercept += ystep;
   mapx += mapxstep;
   }
   else if ( (xintercept >> FRACBITS) == mapx)
   {
   xintercept += xstep;
   mapy += mapystep;
   }

   }

   //
   // go through the sorted list
   //
   return P_TraverseIntercepts ( trav, FRACUNIT );
   } */

//===========================================================================
//
// P_RoughMonsterSearch
//
// Searches though the surrounding mapblocks for monsters/players
//      distance is in MAPBLOCKUNITS
//===========================================================================

mobj_t *P_RoughMonsterSearch(mobj_t *mo, int distance)
{
    int     blockX;
    int     blockY;
    int     startX, startY;
    int     blockIndex;
    int     firstStop;
    int     secondStop;
    int     thirdStop;
    int     finalStop;
    int     count;
    mobj_t *target;
    int     bmapwidth = DD_GetInteger(DD_BLOCKMAP_WIDTH);
    int     bmapheight = DD_GetInteger(DD_BLOCKMAP_HEIGHT);

    P_PointToBlock(mo->pos[VX], mo->pos[VY], &startX, &startY);

    if(startX >= 0 && startX < bmapwidth &&
       startY >= 0 && startY < bmapheight)
    {
        if((target = RoughBlockCheck(mo, startY * bmapwidth + startX)) != NULL)
        {                       // found a target right away
            return target;
        }
    }
    for(count = 1; count <= distance; count++)
    {
        blockX = startX - count;
        blockY = startY - count;

        if(blockY < 0)
        {
            blockY = 0;
        }
        else if(blockY >= bmapheight)
        {
            blockY = bmapheight - 1;
        }
        if(blockX < 0)
        {
            blockX = 0;
        }
        else if(blockX >= bmapwidth)
        {
            blockX = bmapwidth - 1;
        }
        blockIndex = blockY * bmapwidth + blockX;
        firstStop = startX + count;
        if(firstStop < 0)
        {
            continue;
        }
        if(firstStop >= bmapwidth)
        {
            firstStop = bmapwidth - 1;
        }
        secondStop = startY + count;
        if(secondStop < 0)
        {
            continue;
        }
        if(secondStop >= bmapheight)
        {
            secondStop = bmapheight - 1;
        }
        thirdStop = secondStop * bmapwidth + blockX;
        secondStop = secondStop * bmapwidth + firstStop;
        firstStop += blockY * bmapwidth;
        finalStop = blockIndex;

        // Trace the first block section (along the top)
        for(; blockIndex <= firstStop; blockIndex++)
        {
            target = RoughBlockCheck(mo, blockIndex);
            if(target)
                return target;
        }
        // Trace the second block section (right edge)
        for(blockIndex--; blockIndex <= secondStop; blockIndex += bmapwidth)
        {
            target = RoughBlockCheck(mo, blockIndex);
            if(target)
                return target;
        }
        // Trace the third block section (bottom edge)
        for(blockIndex -= bmapwidth; blockIndex >= thirdStop; blockIndex--)
        {
            target = RoughBlockCheck(mo, blockIndex);
            if(target)
                return target;
        }
        // Trace the final block section (left edge)
        for(blockIndex++; blockIndex > finalStop; blockIndex -= bmapwidth)
        {
            target = RoughBlockCheck(mo, blockIndex);
            if(target)
                return target;
        }
    }
    return NULL;
}

//===========================================================================
// RoughBlockCheck
//===========================================================================
static mobj_t *RoughBlockCheck(mobj_t *mo, int index)
{
    mobj_t *link, *root = P_GetBlockRootIdx(index);
    mobj_t *master;
    angle_t angle;

    //
    // If this doesn't work, check the backed-up version!
    //
    for(link = root->bnext; link != root; link = link->bnext)
    {
        if(mo->player)          // Minotaur looking around player
        {
            if((link->flags & MF_COUNTKILL) || (link->player && (link != mo)))
            {
                if(!(link->flags & MF_SHOOTABLE) || link->flags2 & MF2_DORMANT
                   || ((link->type == MT_MINOTAUR) &&
                       (link->tracer == mo)) || (IS_NETGAME && !deathmatch &&
                                                                link->player))
                    continue;

                if(P_CheckSight(mo, link))
                    return link;
            }
            //link = link->bnext;
        }
        else if(mo->type == MT_MINOTAUR)    // looking around minotaur
        {
            master = mo->tracer;
            if((link->flags & MF_COUNTKILL) ||
               (link->player && (link != master)))
            {
                if(!(link->flags & MF_SHOOTABLE) || link->flags2 & MF2_DORMANT
                   || ((link->type == MT_MINOTAUR) &&
                       (link->tracer == mo->tracer)) || (IS_NETGAME &&
                                                             !deathmatch &&
                                                             link->player))
                    continue;

                if(P_CheckSight(mo, link))
                    return link;
            }
            //link = link->bnext;
        }
        else if(mo->type == MT_MSTAFF_FX2)  // bloodscourge
        {
            if((link->flags & MF_COUNTKILL ||
                (link->player && link != mo->target)) &&
               !(link->flags2 & MF2_DORMANT))
            {
                if(!(link->flags & MF_SHOOTABLE) || (IS_NETGAME && !deathmatch &&
                   link->player))
                    continue;

                if(P_CheckSight(mo, link))
                {
                    master = mo->target;
                    angle =
                        R_PointToAngle2(master->pos[VX], master->pos[VY],
                                        link->pos[VY], link->pos[VY]) - master->angle;
                    angle >>= 24;
                    if(angle > 226 || angle < 30)
                    {
                        return link;
                    }
                }
            }
            //link = link->bnext;
        }
        else                    // spirits
        {
            if((link->flags & MF_COUNTKILL ||
                (link->player && link != mo->target)) &&
               !(link->flags2 & MF2_DORMANT))
            {
                if(!(link->flags & MF_SHOOTABLE) || (IS_NETGAME && !deathmatch &&
                   link->player) || link == mo->target)
                    continue;

                if(P_CheckSight(mo, link))
                    return link;
            }
            //link = link->bnext;
        }
    }
    return NULL;
}

// Bellow stuff is for MAPINFO

//===========================================================================
// P_SetSongCDTrack
//===========================================================================
void P_SetSongCDTrack(int index, int track)
{
    int cdTrack = track;

    // Set the internal array.
    cd_NonLevelTracks[index] = sc_Number;

    // Update the corresponding Doomsday definition.
    Def_Set(DD_DEF_MUSIC, Def_Get(DD_DEF_MUSIC, cd_SongDefIDs[index], 0),
            DD_CD_TRACK, &cdTrack);
}

//==========================================================================
// InitMapInfo
//==========================================================================
void InitMapInfo(void)
{
    int     map;
    int     mapMax;
    int     mcmdValue;
    mapInfo_t *info;
    char    songMulch[10];

    mapMax = 1;

    // Put defaults into MapInfo[0]
    info = MapInfo;
    info->cluster = 0;
    info->warpTrans = 0;
    info->nextMap = 1;          // Always go to map 1 if not specified
    info->cdTrack = 1;
    info->sky1Texture =
        R_TextureNumForName(shareware ? "SKY2" : DEFAULT_SKY_NAME);
    info->sky2Texture = info->sky1Texture;
    info->sky1ScrollDelta = 0;
    info->sky2ScrollDelta = 0;
    info->doubleSky = false;
    info->lightning = false;
    info->fadetable = W_GetNumForName(DEFAULT_FADE_TABLE);
    strcpy(info->name, UNKNOWN_MAP_NAME);

    for(map = 0; map < 99; map++)
        MapInfo[map].warpTrans = 0;

    //  strcpy(info->songLump, DEFAULT_SONG_LUMP);
    SC_Open(MAPINFO_SCRIPT_NAME);
    while(SC_GetString())
    {
        if(SC_Compare("MAP") == false)
        {
            SC_ScriptError(NULL);
        }
        SC_MustGetNumber();
        if(sc_Number < 1 || sc_Number > 99)
        {                       //
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
            {                   // Start next map definition
                SC_UnGet();
                break;
            }
            mcmdValue = MapCmdIDs[SC_MustMatchString(MapCmdNames)];
            switch (mcmdValue)
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
                info->sky1ScrollDelta = sc_Number << 8;
                break;
            case MCMD_SKY2:
                SC_MustGetString();
                info->sky2Texture = R_TextureNumForName(sc_String);
                SC_MustGetNumber();
                info->sky2ScrollDelta = sc_Number << 8;
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
                P_SetSongCDTrack(mcmdValue - MCMD_CD_STARTTRACK, sc_Number);
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
    int     i;

    /*  ST_Message("P_TranslateMap(%d):\n", map);
       for(i = 1; i < 99; i++) // Make this a macro
       ST_Message("- %d: warp to %d\n", i, MapInfo[i].warpTrans); */

    for(i = 1; i < 99; i++)     // Make this a macro (?)
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

char   *P_GetMapName(int map)
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

char   *P_GetMapSongLump(int map)
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
    return cd_NonLevelTracks[MCMD_CD_STARTTRACK - MCMD_CD_STARTTRACK];
}

//==========================================================================
//
// P_GetCDEnd1Track
//
//==========================================================================

int P_GetCDEnd1Track(void)
{
    return cd_NonLevelTracks[MCMD_CD_END1TRACK - MCMD_CD_STARTTRACK];
}

//==========================================================================
//
// P_GetCDEnd2Track
//
//==========================================================================

int P_GetCDEnd2Track(void)
{
    return cd_NonLevelTracks[MCMD_CD_END2TRACK - MCMD_CD_STARTTRACK];
}

//==========================================================================
//
// P_GetCDEnd3Track
//
//==========================================================================

int P_GetCDEnd3Track(void)
{
    return cd_NonLevelTracks[MCMD_CD_END3TRACK - MCMD_CD_STARTTRACK];
}

//==========================================================================
//
// P_GetCDIntermissionTrack
//
//==========================================================================

int P_GetCDIntermissionTrack(void)
{
    return cd_NonLevelTracks[MCMD_CD_INTERTRACK - MCMD_CD_STARTTRACK];
}

//==========================================================================
//
// P_GetCDTitleTrack
//
//==========================================================================

int P_GetCDTitleTrack(void)
{
    return cd_NonLevelTracks[MCMD_CD_TITLETRACK - MCMD_CD_STARTTRACK];
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

// Special early initializer needed to start sound before R_Init()
void InitMapMusicInfo(void)
{
    int     i;

    for(i = 0; i < 99; i++)
    {
        strcpy(MapInfo[i].songLump, DEFAULT_SONG_LUMP);
    }
    MapCount = 98;
}
