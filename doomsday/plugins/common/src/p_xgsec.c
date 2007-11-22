/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * p_xgsec.c: Extended Generalized Sector Types.
 */

#if __JDOOM__ || __JHERETIC__
// HEADER FILES ------------------------------------------------------------

#include <math.h>
#include <ctype.h>
#include <stdio.h>

#if  __DOOM64TC__
#  include "doom64tc.h"
#elif __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "dmu_lib.h"
#include "p_mapsetup.h"
#include "p_xgline.h"
#include "p_xgsec.h"
#include "g_common.h"
#include "p_map.h"
#include "p_mapspec.h"

// MACROS ------------------------------------------------------------------

#define PI              3.141592657

#define MAX_VALS        128

#define BL_BUILT        0x1
#define BL_WAS_BUILT    0x2
#define BL_SPREADED     0x4

#define SIGN(x)         ((x)>0? 1 : (x)<0? -1 : 0)

#define ISFUNC(fn)      (fn->func && fn->func[fn->pos])
#define UPDFUNC(fn)     ((ISFUNC(fn) || fn->link))

#define SPREFTYPESTR(reftype) (reftype == SPREF_NONE? "NONE" \
        : reftype == SPREF_MY_FLOOR? "MY FLOOR" \
        : reftype == SPREF_MY_CEILING? "MY CEILING" \
        : reftype == SPREF_ORIGINAL_FLOOR? "ORIGINAL FLOOR" \
        : reftype == SPREF_ORIGINAL_CEILING? "ORIGINAL CEILING" \
        : reftype == SPREF_CURRENT_FLOOR? "CURRENT FLOOR" \
        : reftype == SPREF_CURRENT_CEILING? "CURRENT CEILING" \
        : reftype == SPREF_HIGHEST_FLOOR? "HIGHEST FLOOR" \
        : reftype == SPREF_HIGHEST_CEILING? "HIGHEST CEILING" \
        : reftype == SPREF_LOWEST_FLOOR? "LOWEST FLOOR" \
        : reftype == SPREF_LOWEST_CEILING? "LOWEST CEILING" \
        : reftype == SPREF_NEXT_HIGHEST_FLOOR? "NEXT HIGHEST FLOOR" \
        : reftype == SPREF_NEXT_HIGHEST_CEILING? "NEXT HIGHEST CEILING" \
        : reftype == SPREF_NEXT_LOWEST_FLOOR? "NEXT LOWEST FLOOR" \
        : reftype == SPREF_NEXT_LOWEST_CEILING? "NEXT LOWEST CEILING" \
        : reftype == SPREF_MIN_BOTTOM_TEXTURE? "MIN BOTTOM TEXTURE" \
        : reftype == SPREF_MIN_MID_TEXTURE? "MIN MIDDLE TEXTURE" \
        : reftype == SPREF_MIN_TOP_TEXTURE? "MIN TOP TEXTURE" \
        : reftype == SPREF_MAX_BOTTOM_TEXTURE? "MAX BOTTOM TEXTURE" \
        : reftype == SPREF_MAX_MID_TEXTURE? "MAX MIDDLE TEXTURE" \
        : reftype == SPREF_MAX_TOP_TEXTURE? "MAX TOP TEXTURE" \
        : reftype == SPREF_SECTOR_TAGGED_FLOOR? "SECTOR TAGGED FLOOR" \
        : reftype == SPREF_LINE_TAGGED_FLOOR? "LINE TAGGED FLOOR" \
        : reftype == SPREF_TAGGED_FLOOR? "TAGGED FLOOR" \
        : reftype == SPREF_ACT_TAGGED_FLOOR? "ACT TAGGED FLOOR" \
        : reftype == SPREF_INDEX_FLOOR? "INDEXED FLOOR" \
        : reftype == SPREF_SECTOR_TAGGED_CEILING? "SECTOR TAGGED CEILING" \
        : reftype == SPREF_LINE_TAGGED_CEILING? "LINE TAGGED CEILING" \
        : reftype == SPREF_TAGGED_CEILING? "TAGGED CEILING" \
        : reftype == SPREF_ACT_TAGGED_CEILING? "ACT TAGGED CEILING" \
        : reftype == SPREF_INDEX_CEILING? "INDEXED CEILING" \
        : reftype == SPREF_BACK_FLOOR? "BACK FLOOR" \
        : reftype == SPREF_BACK_CEILING? "BACK CEILING" \
        : reftype == SPREF_SPECIAL? "SPECIAL" \
        : reftype == SPREF_LINE_ACT_TAGGED_FLOOR? "LINE ACT TAGGED FLOOR" \
        : reftype == SPREF_LINE_ACT_TAGGED_CEILING? "LINE ACT TAGGED CEILING" : "???")

#define TO_DMU_COLOR(x) (x == 0? DMU_COLOR_RED \
        : x == 1? DMU_COLOR_GREEN \
        : DMU_COLOR_BLUE)

#define TO_DMU_CEILING_COLOR(x) (x == 0? DMU_CEILING_COLOR_RED \
        : x == 1? DMU_CEILING_COLOR_GREEN \
        : DMU_CEILING_COLOR_BLUE)

#define TO_DMU_FLOOR_COLOR(x) (x == 0? DMU_FLOOR_COLOR_RED \
        : x == 1? DMU_FLOOR_COLOR_GREEN \
        : DMU_FLOOR_COLOR_BLUE)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void XS_DoChain(sector_t *sec, int ch, int activating, void *actThing);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern mobj_t dummything;
extern boolean xgdatalumps;

extern int xgDev;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static byte *builder;
static sectortype_t sectypebuffer;

// CODE --------------------------------------------------------------------

sectortype_t *XS_GetType(int id)
{
    sectortype_t *ptr;
    char        buff[5];

    // Try finding it from the DDXGDATA lump.
    ptr = XG_GetLumpSector(id);
    if(ptr)
    {   // Got it!
        memcpy(&sectypebuffer, ptr, sizeof(*ptr));
        return &sectypebuffer;
    }

    sprintf(buff, "%i", id);
    if(Def_Get(DD_DEF_SECTOR_TYPE, buff, &sectypebuffer))
        return &sectypebuffer;  // A definition was found.

    return NULL; // None found.
}

void XF_Init(sector_t *sec, function_t *fn, char *func, int min, int max,
             float scale, float offset)
{
    xsector_t *xsec = P_ToXSector(sec);

    memset(fn, 0, sizeof(*fn));

    if(!func)
        return;

    // Check for links.
    if(func[0] == '=')
    {
        switch(tolower(func[1]))
        {
        case 'r':
            fn->link = &xsec->xg->rgb[0];
            break;

        case 'g':
            fn->link = &xsec->xg->rgb[1];
            break;

        case 'b':
            fn->link = &xsec->xg->rgb[2];
            break;

        case 'f':
            fn->link = &xsec->xg->plane[XGSP_FLOOR];
            break;

        case 'c':
            fn->link = &xsec->xg->plane[XGSP_CEILING];
            break;

        case 'l':
            fn->link = &xsec->xg->light;
            break;

        default:
            Con_Error("XF_Init: Bad linked func (%s).\n", func);
        }
        return;
    }

    // Check for offsets to current values.
    if(func[0] == '+')
    {
        /**
         * \important
         * The original value ranges must be maintained due to the cross linking
         * between sector function types i.e:
         * RGB = 0 > 254
         * light = 0 > 254
         * planeheight = -32768 > 32768
         */
        switch(func[1])
        {
        case 'r':
            offset += 255.f * xsec->origrgb[0];
            break;

        case 'g':
            offset += 255.f * xsec->origrgb[1];
            break;

        case 'b':
            offset += 255.f * xsec->origrgb[2];
            break;

        case 'l':
            offset += 255.0f * xsec->origlight;
            break;

        case 'f':
            offset += xsec->SP_floororigheight * FRACUNIT;
            break;

        case 'c':
            offset += xsec->SP_ceilorigheight * FRACUNIT;
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

    fn->timer = -1; // The first step musn't skip the first value.
    fn->maxtimer = XG_RandomInt(min, max);
    fn->mininterval = min;
    fn->maxinterval = max;
    fn->scale = scale;
    fn->offset = offset;
    // Make sure oldvalue is out of range.
    fn->oldvalue = -scale + offset;
}

int C_DECL XLTrav_LineAngle(line_t *line, boolean dummy, void *context,
                            void *context2, mobj_t *activator)
{
    sector_t* sec = (sector_t *) context;

    if(P_GetPtrp(line, DMU_FRONT_SECTOR) != sec &&
       P_GetPtrp(line, DMU_BACK_SECTOR) != sec)
        return true; // Wrong sector, keep looking.

    *(angle_t *) context2 = R_PointToAngle2(0, 0, P_GetFixedp(line, DMU_DX),
                                            P_GetFixedp(line, DMU_DY));

    return false; // Stop looking after first hit.
}

void XS_SetSectorType(struct sector_s *sec, int special)
{
    int         i;
    xsector_t *xsec = P_ToXSector(sec);
    xgsector_t *xg;
    sectortype_t *info;

    if(XS_GetType(special))
    {
        XG_Dev("XS_SetSectorType: Sector %i, type %i", P_ToIndex(sec),
               special);

        xsec->special = special;

        // All right, do the init.
        if(!xsec->xg)
            xsec->xg = Z_Malloc(sizeof(xgsector_t), PU_LEVEL, 0);
        memset(xsec->xg, 0, sizeof(*xsec->xg));

        // Get the type info.
        memcpy(&xsec->xg->info, &sectypebuffer, sizeof(sectypebuffer));

        // Init the state.
        xg = xsec->xg;
        info = &xsec->xg->info;

        // Init timer so ambient doesn't play immediately at level start.
        xg->timer =
            XG_RandomInt(FLT2TIC(xg->info.sound_interval[0]),
                         FLT2TIC(xg->info.sound_interval[1]));

        // Light function.
        XF_Init(sec, &xg->light, info->lightfunc, info->light_interval[0],
                info->light_interval[1], 255, 0);

        // Color functions.
        for(i = 0; i < 3; ++i)
        {
            XF_Init(sec, &xg->rgb[i], info->colfunc[i],
                    info->col_interval[i][0], info->col_interval[i][1], 255,
                    0);
        }

        // Plane functions / floor.
        XF_Init(sec, &xg->plane[XGSP_FLOOR], info->floorfunc,
                info->floor_interval[0], info->floor_interval[1],
                info->floormul, info->flooroff);
        XF_Init(sec, &xg->plane[XGSP_CEILING], info->ceilfunc,
                info->ceil_interval[0], info->ceil_interval[1],
                info->ceilmul, info->ceiloff);

        // Derive texmove angle from first act-tagged line?
        if((info->flags & STF_ACT_TAG_TEXMOVE) ||
           (info->flags & STF_ACT_TAG_WIND))
        {
            angle_t angle = 0;

            // -1 to support binary XG data with old flag values.
            XL_TraverseLines(0, (xgdatalumps? LREF_TAGGED -1: LREF_TAGGED),
                             info->act_tag, sec, &angle,
                             NULL, XLTrav_LineAngle);

            // Convert to degrees.
            if(info->flags & STF_ACT_TAG_TEXMOVE)
            {
                info->texmove_angle[0] = info->texmove_angle[1] =
                    angle / (float) ANGLE_MAX *360;
            }

            if(info->flags & STF_ACT_TAG_WIND)
            {
                info->wind_angle = angle / (float) ANGLE_MAX *360;
            }
        }
    }
    else
    {
        XG_Dev("XS_SetSectorType: Sector %i, NORMAL TYPE %i", P_ToIndex(sec),
               special);

        // Free previously allocated XG data.
        if(xsec->xg)
            Z_Free(xsec->xg);
        xsec->xg = NULL;

        // Just set it, then. Must be a standard sector type...
        // Mind you, we're not going to spawn any standard flash funcs
        // or anything.
        xsec->special = special;
    }
}

void XS_Init(void)
{
    int         i;
    int         count = DD_GetInteger(DD_SECTOR_COUNT);
    sector_t   *sec;
    xsector_t  *xsec;

    // Allocate stair builder data.

    builder = Z_Malloc(count, PU_LEVEL, 0);
    memset(builder, 0, count);

    /*  // Clients rely on the server, they don't do XG themselves.
       if(IS_CLIENT) return; */

    for(i = 0; i < count; ++i)
    {
        sec = P_ToPtr(DMU_SECTOR, i);
        xsec = P_ToXSector(sec);

        P_GetFloatpv(sec, DMU_COLOR, xsec->origrgb);

        xsec->SP_floororigheight = P_GetFloatp(sec, DMU_FLOOR_HEIGHT);
        xsec->SP_ceilorigheight = P_GetFloatp(sec, DMU_CEILING_HEIGHT);
        xsec->origlight = P_GetFloatp(sec, DMU_LIGHT_LEVEL);

        // Initialize the XG data for this sector.
        XS_SetSectorType(sec, xsec->special);
    }
}

void XS_SectorSound(sector_t *sec, int origin, int snd)
{
    if(!snd)
        return;

    XG_Dev("XS_SectorSound: Play Sound ID (%i) in Sector ID (%i)",
            snd, P_ToIndex(sec));
    S_SectorSound(sec, origin, snd);
}

void XS_MoverStopped(xgplanemover_t *mover, boolean done)
{
    xline_t    *origin = P_ToXLine(mover->origin);

    XG_Dev("XS_MoverStopped: Sector %i (done=%i, origin line=%i)",
           P_ToIndex(mover->sector), done,
           mover->origin ? P_ToIndex(mover->origin) : -1);

    if(done)
    {
        if((mover->flags & PMF_ACTIVATE_WHEN_DONE) && mover->origin)
        {
            XL_ActivateLine(true, &origin->xg->info, mover->origin, 0,
                            &dummything, XLE_AUTO);
        }

        if((mover->flags & PMF_DEACTIVATE_WHEN_DONE) && mover->origin)
        {
            XL_ActivateLine(false, &origin->xg->info, mover->origin, 0,
                            &dummything, XLE_AUTO);
        }

        // Remove this thinker.
        P_RemoveThinker((thinker_t *) mover);
    }
    else
    {
        // Normally we just wait, but if...
        if((mover->flags & PMF_ACTIVATE_ON_ABORT) && mover->origin)
        {
            XL_ActivateLine(true, &origin->xg->info, mover->origin, 0,
                            &dummything, XLE_AUTO);
        }

        if((mover->flags & PMF_DEACTIVATE_ON_ABORT) && mover->origin)
        {
            XL_ActivateLine(false, &origin->xg->info, mover->origin, 0,
                            &dummything, XLE_AUTO);
        }

        if(mover->flags & (PMF_ACTIVATE_ON_ABORT | PMF_DEACTIVATE_ON_ABORT))
        {
            // Destroy this mover.
            P_RemoveThinker((thinker_t *) mover);
        }
    }
}

/**
 * A thinker function for plane movers.
 */
void XS_PlaneMover(xgplanemover_t *mover)
{
    int         res, res2;
    int         dir;
    float       ceil = P_GetFloatp(mover->sector, DMU_CEILING_HEIGHT);
    float       floor = P_GetFloatp(mover->sector, DMU_FLOOR_HEIGHT);
    xsector_t  *xsec = P_ToXSector(mover->sector);
    boolean     docrush = (mover->flags & PMF_CRUSH) != 0;
    boolean     follows = (mover->flags & PMF_OTHER_FOLLOWS) != 0;
    boolean     setorig = (mover->flags & PMF_SET_ORIGINAL) != 0;

    // Play movesound when timer goes to zero.
    if(mover->timer-- <= 0)
    {
        // Clear the wait flag.
        if(mover->flags & PMF_WAIT)
        {
            mover->flags &= ~PMF_WAIT;
            // Play a sound.
            XS_SectorSound(mover->sector, SORG_FLOOR + mover->ceiling,
                           mover->startsound);
        }

        mover->timer = XG_RandomInt(mover->mininterval, mover->maxinterval);
        XS_SectorSound(mover->sector, SORG_FLOOR + mover->ceiling,
                       mover->movesound);
    }

    // Are we waiting?
    if(mover->flags & PMF_WAIT)
        return;

    // Determine move direction.
    if((mover->destination - (mover->ceiling ? ceil : floor)) > 0)
        dir = 1; // Moving up.
    else
        dir = -1; // Moving down.

    // Do the move.
    res = T_MovePlane(mover->sector, mover->speed, mover->destination,
                      docrush, mover->ceiling, dir);

    // Should we update origheight?
    if(setorig)
    {
        xsec->planes[mover->ceiling? PLN_CEILING:PLN_FLOOR].origheight =
            P_GetFloatp(mover->sector,
                        mover->ceiling? DMU_CEILING_HEIGHT:DMU_FLOOR_HEIGHT);
    }

    if(follows)
    {
        float       off = (mover->ceiling? floor - ceil : ceil - floor);

        res2 = T_MovePlane(mover->sector, mover->speed,
                           mover->destination + off, docrush,
                           !mover->ceiling, dir);

        // Should we update origheight?
        if(setorig)
        {
            xsec->planes[(!mover->ceiling)? PLN_CEILING:PLN_FLOOR].origheight =
                P_GetFloatp(mover->sector,
                            (!mover->ceiling)? DMU_CEILING_HEIGHT:DMU_FLOOR_HEIGHT);
        }

        if(res2 == crushed)
            res = crushed;
    }

    if(res == pastdest)
    {
        // Move has finished.
        XS_MoverStopped(mover, true);

        // The move is done. Do end stuff.
        if(mover->setflat > 0)
        {
            XS_ChangePlaneTexture(mover->sector, mover->ceiling,
                                  mover->setflat, NULL);
        }

        if(mover->setsector >= 0)
        {
            XS_SetSectorType(mover->sector, mover->setsector);
        }

        // Play sound?
        XS_SectorSound(mover->sector, SORG_FLOOR + mover->ceiling,
                       mover->endsound);
    }
    else if(res == crushed)
    {
        if(mover->flags & PMF_CRUSH)
        {   // We're crushing things.
            mover->speed = mover->crushspeed;
        }
        else
        {
            // Make sure both the planes are where we started from.
            if((!mover->ceiling || follows) &&
               P_GetFloatp(mover->sector, DMU_FLOOR_HEIGHT) != floor)
            {
                T_MovePlane(mover->sector, mover->speed, floor, docrush,
                            false, -dir);
            }

            if((mover->ceiling || follows) &&
               P_GetFloatp(mover->sector, DMU_CEILING_HEIGHT) != ceil)
            {
                T_MovePlane(mover->sector, mover->speed, ceil, docrush,
                            true, -dir);
            }

            XS_MoverStopped(mover, false);
        }
    }
}

/**
 * Returns a new thinker for handling the specified plane. Removes any
 * existing thinkers associated with the plane.
 */
xgplanemover_t *XS_GetPlaneMover(sector_t *sector, boolean ceiling)
{
    thinker_t  *th;
    xgplanemover_t *mover;

    for(th = thinkercap.next; th != &thinkercap; th = th->next)
        if(th->function == XS_PlaneMover)
        {
            mover = (xgplanemover_t *) th;
            if(mover->sector == sector && mover->ceiling == ceiling)
            {
                XS_MoverStopped(mover, false);
                P_RemoveThinker(th); // Remove it.
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

void XS_ChangePlaneTexture(sector_t *sector, boolean ceiling, int tex,
                           float *rgb)
{
    XG_Dev("XS_ChangePlaneTexture: Sector %i, %s, texture %i",
           P_ToIndex(sector), (ceiling ? "ceiling" : "floor"), tex);
    if(rgb)
        XG_Dev("red %g, green %g, blue %g", rgb[0], rgb[1], rgb[2]);

    if(ceiling)
    {
        if(rgb)
            P_SetFloatpv(sector, DMU_CEILING_COLOR, rgb);

        if(tex)
            P_SetIntp(sector, DMU_CEILING_MATERIAL, tex);
    }
    else
    {
        if(rgb)
            P_SetFloatpv(sector, DMU_FLOOR_COLOR, rgb);

        if(tex)
            P_SetIntp(sector, DMU_FLOOR_MATERIAL, tex);
    }
}

/**
 * \note One plane can get listed multiple times.
 */
int XS_AdjoiningPlanes(sector_t *sector, boolean ceiling, int *heightlist,
                       int *piclist, int *lightlist, sector_t **sectorlist)
{
    int         i, count = 0;
    int         sectorLineCount;
    line_t     *lin;
    sector_t   *other;

    if(!sector)
        return 0;

    sectorLineCount = P_GetIntp(sector, DMU_LINE_COUNT);
    for(i = 0; i < sectorLineCount; ++i)
    {
        lin = P_GetPtrp(sector, DMU_LINE_OF_SECTOR | i);

        // Only accept two-sided lines.
        if(!P_GetPtrp(lin, DMU_BACK_SECTOR) ||
           !P_GetPtrp(lin, DMU_FRONT_SECTOR))
            continue;

        if(P_GetPtrp(lin, DMU_FRONT_SECTOR) == sector)
            other = P_GetPtrp(lin, DMU_BACK_SECTOR);
        else
            other = P_GetPtrp(lin, DMU_FRONT_SECTOR);

        if(heightlist)
        {
            if(ceiling)
                heightlist[count] = P_GetFixedp(other, DMU_CEILING_HEIGHT);
            else
                heightlist[count] = P_GetFixedp(other, DMU_FLOOR_HEIGHT);
        }

        if(piclist)
        {
            if(ceiling)
                piclist[count] = P_GetIntp(other, DMU_CEILING_MATERIAL);
            else
                piclist[count] = P_GetIntp(other, DMU_FLOOR_MATERIAL);
        }

        if(lightlist)
            lightlist[count] = 255.0f * P_GetFloatp(other, DMU_LIGHT_LEVEL);

        if(sectorlist)
            sectorlist[count] = other;

        // Increment counter.
        count++;
    }

    return count;
}

uint FindMaxOf(int *list, uint num)
{
    uint        i, idx = 0;
    int         max = list[0];

    for(i = 1; i < num; ++i)
    {
        if(list[i] > max)
        {
            max = list[i];
            idx = i;
        }
    }

    return idx;
}

uint FindMinOf(int *list, uint num)
{
    uint        i, idx = 0;
    int         min = list[0];

    for(i = 1; i < num; ++i)
    {
        if(list[i] < min)
        {
            min = list[i];
            idx = i;
        }
    }

    return idx;
}

int FindNextOf(int *list, int num, int h)
{
    int         i, min = 0, idx = -1;

    for(i = 0; i < num; ++i)
    {
        if(list[i] <= h)
            continue;

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
    int         i, max = 0, idx = -1;

    for(i = 0; i < num; ++i)
    {
        if(list[i] >= h)
            continue;

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

/**
 * Really an XL_* function!
 *
 * @param part              1=mid, 2=top, 3=bottom.
 *
 * @return                  @c MAXINT if not height n/a.
 */
int XS_TextureHeight(line_t *line, int part)
{
    side_t     *side;
    int         snum = 0;
    int         minfloor = 0, maxfloor = 0, maxceil = 0;
    sector_t   *front = P_GetPtrp(line, DMU_FRONT_SECTOR);
    sector_t   *back = P_GetPtrp(line, DMU_BACK_SECTOR);
    boolean     twosided = front && back;

    if(part != LWS_MID && !twosided)
        return DDMAXINT;

    if(twosided)
    {
        int         ffloor = P_GetIntp(front, DMU_FLOOR_HEIGHT);
        int         fceil = P_GetIntp(front, DMU_CEILING_HEIGHT);
        int         bfloor = P_GetIntp(back, DMU_FLOOR_HEIGHT);
        int         bceil = P_GetIntp(back, DMU_CEILING_HEIGHT);

        minfloor = ffloor;
        maxfloor = bfloor;
        if(part == LWS_LOWER)
            snum = 0;
        if(bfloor < minfloor)
        {
            minfloor = bfloor;
            maxfloor = ffloor;
            if(part == LWS_LOWER)
                snum = 1;
        }
        maxceil = fceil;
        if(part == LWS_UPPER)
            snum = 0;
        if(bceil > maxceil)
        {
            maxceil = bceil;
            if(part == LWS_UPPER)
                snum = 1;
        }
    }
    else
    {
        if(P_GetPtrp(line, DMU_SIDE0))
            snum = 0;
        else
            snum = 1;
    }

    // Which side are we working with?
    if(snum == 0)
        side = P_GetPtrp(line, DMU_SIDE0);
    else
        side = P_GetPtrp(line, DMU_SIDE1);

    // Which section of the wall?
    if(part == LWS_UPPER)
    {
        int texid = P_GetIntp(side, DMU_TOP_MATERIAL);

        if(!texid)
            return DDMAXINT;

        return maxceil - XS_GetTexH(texid);
    }
    else if(part == LWS_MID)
    {
        int texid = P_GetIntp(side, DMU_MIDDLE_MATERIAL);

        if(!texid)
            return DDMAXINT;

        return maxfloor + XS_GetTexH(texid);
    }
    else if(part == LWS_LOWER)
    {
        int texid = P_GetIntp(side, DMU_BOTTOM_MATERIAL);

        if(!texid)
            return DDMAXINT;

        return minfloor + XS_GetTexH(texid);
    }

    return DDMAXINT;
}

/**
 * Returns a pointer to the first sector with the tag.
 *
 * NOTE: We cannot use the tagged sector lists here as this can be called
 * during an iteration at a higher level. Doing so would change the position
 * of the rover which would affect the other iteration.
 *
 * NOTE2: Re-above, obviously that is bad design and should be addressed.
 */
sector_t *XS_FindTagged(int tag)
{
    uint        k;
    uint        foundcount = 0;
    uint        retsectorid = 0;
    sector_t   *sec, *retsector;

    retsector = NULL;

    for(k = 0; k < numsectors; ++k)
    {
        sec = P_ToPtr(DMU_SECTOR, k);
        if(P_ToXSector(sec)->tag == tag)
        {
            if(xgDev)
            {
                if(foundcount == 0)
                {
                    retsector = sec;
                    retsectorid = k;
                }
            }
            else
                return sec;

            foundcount++;
        }
    }

    if(xgDev)
    {
        if(foundcount > 1)
        {
            XG_Dev("XS_FindTagged: More than one sector exists with this tag (%i)!",tag);
            XG_Dev("  The sector with the lowest ID (%i) will be used.", retsectorid);
        }

        if(retsector)
            return retsector;
    }

    return NULL;
}

/**
 * Returns a pointer to the first sector with the specified act tag.
 */
sector_t *XS_FindActTagged(int tag)
{
    uint        k;
    uint        foundcount = 0;
    uint        retsectorid = 0;
    sector_t   *sec, *retsector;
    xsector_t  *xsec;

    retsector = NULL;

    for(k = 0; k < numsectors; ++k)
    {
        sec = P_ToPtr(DMU_SECTOR, k);
        xsec = P_ToXSector(sec);
        if(xsec->xg)
        {
            if(xsec->xg->info.act_tag == tag)
            {
                if(xgDev)
                {
                    if(foundcount == 0)
                    {
                        retsector = sec;
                        retsectorid = k;
                    }
                }
                else
                {
                    return sec;
                }

                foundcount++;
            }
        }
    }

    if(xgDev)
    {
        if(foundcount > 1)
        {
            XG_Dev("XS_FindActTagged: More than one sector exists with this ACT tag (%i)!",tag);
            XG_Dev("  The sector with the lowest ID (%i) will be used.", retsectorid);
        }

        if(retsector)
            return retsector;
    }

    return NULL;
}

boolean XS_GetPlane(line_t *actline, sector_t *sector, int ref,
                    uint *refdata, int *height, int *pic,
                    sector_t **planesector)
{
    int         idx = 0;
    uint        num;
    int         heights[MAX_VALS], pics[MAX_VALS];
    sector_t   *sectorlist[MAX_VALS];
    boolean     ceiling;
    sector_t   *iter;
    xline_t    *xline;
    char        buff[50];

    if(refdata)
        sprintf(buff, " : %i", *refdata);

    XG_Dev("XS_GetPlane: Line %i, sector %i, ref (%s(%i)%s)",
           actline ? P_ToIndex(actline) : -1, P_ToIndex(sector),
           SPREFTYPESTR(ref), ref, refdata? buff : "" );

    if(ref == SPREF_NONE || ref == SPREF_SPECIAL)
    {
        // No reference to anywhere.
        return false;
    }

    // Init the values to the current sector's floor.
    if(height)
        *height = P_GetFixedp(sector, DMU_FLOOR_HEIGHT);
    if(pic)
        *pic = P_GetIntp(sector, DMU_FLOOR_MATERIAL);
    if(planesector)
        *planesector = sector;

    // First try the non-comparative, iterative sprefs.
    iter = NULL;
    switch (ref)
    {
    case SPREF_SECTOR_TAGGED_FLOOR:
    case SPREF_SECTOR_TAGGED_CEILING:
        iter = XS_FindTagged(P_ToXSector(sector)->tag);
        if(!iter)
            return false;
        break;

    case SPREF_LINE_TAGGED_FLOOR:
    case SPREF_LINE_TAGGED_CEILING:
        if(!actline)
            return false;

        iter = XS_FindTagged(P_ToXLine(actline)->tag);
        if(!iter)
            return false;
        break;

    case SPREF_TAGGED_FLOOR:
    case SPREF_TAGGED_CEILING:
        if(!refdata)
        {
            XG_Dev("  %s IS NOT VALID FOR THIS CLASS PARAMETER!", SPREFTYPESTR(ref));
            return false;
        }

        iter = XS_FindTagged(*refdata);
        if(!iter)
            return false;
        break;

    case SPREF_LINE_ACT_TAGGED_FLOOR:
    case SPREF_LINE_ACT_TAGGED_CEILING:
        xline = P_ToXLine(actline);

        if(!xline)
            return false;

        if(!xline->xg)
        {
            XG_Dev("  ACT LINE IS NOT AN XG LINE!");
            return false;
        }
        if(!xline->xg->info.act_tag)
        {
            XG_Dev("  ACT LINE DOES NOT HAVE AN ACT TAG!");
            return false;
        }

        iter = XS_FindActTagged(xline->xg->info.act_tag);
        if(!iter)
            return false;
        break;

    case SPREF_ACT_TAGGED_FLOOR:
    case SPREF_ACT_TAGGED_CEILING:
        if(!refdata)
        {
            XG_Dev("  %s IS NOT VALID FOR THIS CLASS PARAMETER!", SPREFTYPESTR(ref));
            return false;
        }

        iter = XS_FindActTagged(*refdata);
        if(!iter)
            return false;
        break;

    case SPREF_INDEX_FLOOR:
    case SPREF_INDEX_CEILING:
        if(!refdata || *refdata >= numsectors)
            return false;
        iter = P_ToPtr(DMU_SECTOR, *refdata);
        break;

    default:
        // No iteration.
        break;
    }

    // Did we find the plane through iteration?
    if(iter)
    {
        if(planesector)
            *planesector = iter;
        if((ref >= SPREF_SECTOR_TAGGED_FLOOR && ref <= SPREF_INDEX_FLOOR) ||
            ref == SPREF_LINE_ACT_TAGGED_FLOOR)
        {
            if(height)
                *height = P_GetFixedp(iter, DMU_FLOOR_HEIGHT);
            if(pic)
                *pic = P_GetIntp(iter, DMU_FLOOR_MATERIAL);
        }
        else
        {
            if(height)
                *height = P_GetFixedp(iter, DMU_CEILING_HEIGHT);
            if(pic)
                *pic = P_GetIntp(iter, DMU_CEILING_MATERIAL);
        }
        return true;
    }

    if(ref == SPREF_MY_FLOOR)
    {
        sector_t *frontsector;

        if(!actline)
            return false;

        frontsector = P_GetPtrp(actline, DMU_FRONT_SECTOR);

        if(!frontsector)
            return false;

        // Actline's front floor.
        if(height)
            *height = P_GetFixedp(frontsector, DMU_FLOOR_HEIGHT);
        if(pic)
            *pic = P_GetIntp(frontsector, DMU_FLOOR_MATERIAL);
        if(planesector)
            *planesector = frontsector;
        return true;
    }

    if(ref == SPREF_BACK_FLOOR)
    {
        sector_t *backsector;

        if(!actline)
            return false;

        backsector = P_GetPtrp(actline, DMU_BACK_SECTOR);

        if(!backsector)
            return false;

        // Actline's back floor.
        if(height)
            *height = P_GetFixedp(backsector, DMU_FLOOR_HEIGHT);
        if(pic)
            *pic = P_GetIntp(backsector, DMU_FLOOR_MATERIAL);
        if(planesector)
            *planesector = backsector;
        return true;
    }

    if(ref == SPREF_MY_CEILING)
    {
        sector_t *frontsector;

        if(!actline)
            return false;

        frontsector = P_GetPtrp(actline, DMU_FRONT_SECTOR);

        if(!frontsector)
            return false;

        // Actline's front ceiling.
        if(height)
            *height = P_GetFixedp(frontsector, DMU_CEILING_HEIGHT);
        if(pic)
            *pic = P_GetIntp(frontsector, DMU_CEILING_MATERIAL);
        if(planesector)
            *planesector = frontsector;
        return true;
    }

    if(ref == SPREF_BACK_CEILING)
    {
        sector_t *backsector;

        if(!actline)
            return false;

        backsector = P_GetPtrp(actline, DMU_BACK_SECTOR);

        if(!backsector)
            return false;

        // Actline's back ceiling.
        if(height)
            *height = P_GetFixedp(backsector, DMU_CEILING_HEIGHT);
        if(pic)
            *pic = P_GetIntp(backsector, DMU_CEILING_MATERIAL);
        if(planesector)
            *planesector = backsector;
        return true;
    }

    if(ref == SPREF_ORIGINAL_FLOOR)
    {
        if(height)
            *height = FLT2FIX(P_ToXSector(sector)->SP_floororigheight);
        if(pic)
            *pic = P_GetIntp(sector, DMU_FLOOR_MATERIAL);
        return true;
    }

    if(ref == SPREF_ORIGINAL_CEILING)
    {
        if(height)
            *height = FLT2FIX(P_ToXSector(sector)->SP_ceilorigheight);
        if(pic)
            *pic = P_GetIntp(sector, DMU_CEILING_MATERIAL);
        return true;
    }

    if(ref == SPREF_CURRENT_FLOOR)
    {
        if(height)
            *height = P_GetFixedp(sector, DMU_FLOOR_HEIGHT);
        if(pic)
            *pic = P_GetIntp(sector, DMU_FLOOR_MATERIAL);
        return true;
    }

    if(ref == SPREF_CURRENT_CEILING)
    {
        if(height)
            *height = P_GetFixedp(sector, DMU_CEILING_HEIGHT);
        if(pic)
            *pic = P_GetIntp(sector, DMU_CEILING_MATERIAL);
        return true;
    }

    // Texture targets?
    if(ref >= SPREF_MIN_BOTTOM_TEXTURE && ref <= SPREF_MAX_TOP_TEXTURE)
    {
        int     i, k;
        int     part;

        num = 0;
        // Which part of the wall are we looking at?
        if(ref == SPREF_MIN_MID_TEXTURE || ref == SPREF_MAX_MID_TEXTURE)
            part = LWS_MID;
        else if(ref == SPREF_MIN_TOP_TEXTURE || ref == SPREF_MAX_TOP_TEXTURE)
            part = LWS_UPPER;
        else // Then it's the bottom.
            part = LWS_LOWER;

        // Get the heights of the sector's textures.
        // The heights are in real world coordinates.
        for(i = 0, k = 0; i < P_GetIntp(sector, DMU_LINE_COUNT); ++i)
        {
            k = XS_TextureHeight(P_GetPtrp(sector, DMU_LINE_OF_SECTOR | i), part);
            if(k != DDMAXINT)
                heights[num++] = k << FRACBITS;
        }

        if(!num)
            return true;

        // Are we looking at the min or max heights?
        if(ref >= SPREF_MIN_BOTTOM_TEXTURE && ref <= SPREF_MIN_TOP_TEXTURE)
        {
            idx = FindMinOf(heights, num);
            if(height)
                *height = heights[idx];
            return true;
        }

        idx = FindMaxOf(heights, num);
        if(height)
            *height = heights[idx];

        return true;
    }

    // We need to figure out the heights of the adjoining sectors.
    // The results will be stored in the heights array.
    ceiling = (ref == SPREF_HIGHEST_CEILING || ref == SPREF_LOWEST_CEILING ||
               ref == SPREF_NEXT_HIGHEST_CEILING ||
               ref == SPREF_NEXT_LOWEST_CEILING);
    num = XS_AdjoiningPlanes(sector, ceiling, heights, pics, NULL, sectorlist);

    if(!num)
    {
        // Add self.
        heights[0] = ceiling ? P_GetFixedp(sector, DMU_CEILING_HEIGHT) :
                               P_GetFixedp(sector, DMU_FLOOR_HEIGHT);

        pics[0] = ceiling ? P_GetIntp(sector, DMU_CEILING_MATERIAL) :
                            P_GetIntp(sector, DMU_FLOOR_MATERIAL);
        sectorlist[0] = sector;
        num = 1;
    }

    // Get the right height and pic.
    if(ref == SPREF_HIGHEST_CEILING || ref == SPREF_HIGHEST_FLOOR)
        idx = FindMaxOf(heights, num);
    else if(ref == SPREF_LOWEST_CEILING || ref == SPREF_LOWEST_FLOOR)
        idx = FindMinOf(heights, num);
    else if(ref == SPREF_NEXT_HIGHEST_CEILING)
        idx = FindNextOf(heights, num, P_GetFixedp(sector, DMU_CEILING_HEIGHT));
    else if(ref == SPREF_NEXT_HIGHEST_FLOOR)
        idx = FindNextOf(heights, num, P_GetFixedp(sector, DMU_FLOOR_HEIGHT));
    else if(ref == SPREF_NEXT_LOWEST_CEILING)
        idx = FindPrevOf(heights, num, P_GetFixedp(sector, DMU_CEILING_HEIGHT));
    else if(ref == SPREF_NEXT_LOWEST_FLOOR)
        idx = FindPrevOf(heights, num, P_GetFixedp(sector, DMU_FLOOR_HEIGHT));

    // The requested plane was not found.
    if(idx == -1)
        return false;

    // Set the values.
    if(height)
        *height = heights[idx];
    if(pic)
        *pic = pics[idx];
    if(planesector)
        *planesector = sectorlist[idx];
    return true;
}

/**
 * DJS - Why find the highest??? Surely unlogical to mod authors.
 * IMO if a user references multiple sectors, the one with the lowest ID
 * should be chosen (the same way it works for FIND(act)TAGGED). If that
 * happens to be zero - so be it.
 */
int C_DECL XSTrav_HighestSectorType(sector_t *sec, boolean ceiling,
                                    void *context, void *context2,
                                    mobj_t *activator)
{
    int        *type = context2;
    xsector_t  *xsec = P_ToXSector(sec);

    if(xsec->special > *type)
        *type = xsec->special;

    return true; // Keep looking...
}

void XS_InitMovePlane(line_t *line)
{
    xline_t *xline = P_ToXLine(line);

    // fdata keeps track of wait time.
    xline->xg->fdata = xline->xg->info.fparm[5];
    xline->xg->idata = true; // Play sound.
};

int C_DECL XSTrav_MovePlane(sector_t *sector, boolean ceiling, void *context,
                            void *context2, mobj_t *activator)
{
    line_t     *line = (line_t *) context;
    linetype_t *info = (linetype_t *) context2;
    xgplanemover_t *mover;
    int         flat, st;
    xline_t     *xline = P_ToXLine(line);
    boolean     playsound;

    playsound = xline->xg->idata;

    XG_Dev("XSTrav_MovePlane: Sector %i (by line %i of type %i)",
           P_ToIndex(sector), P_ToIndex(line), info->id);

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
    // i11 + i12: (plane ref) start sector type
    // i13 + i14: (plane ref) end sector type
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
    {
    int temp = FLT2FIX(mover->destination);
    XS_GetPlane(line, sector, info->iparm[2], NULL, &temp, 0, 0);
    mover->destination = FIX2FLT(temp) + info->fparm[2];
    }
    mover->speed = info->fparm[0];
    mover->crushspeed = info->fparm[1];
    mover->mininterval = FLT2TIC(info->fparm[3]);
    mover->maxinterval = FLT2TIC(info->fparm[4]);
    mover->flags = info->iparm[3];
    mover->endsound = playsound ? info->iparm[5] : 0;
    mover->movesound = playsound ? info->iparm[6] : 0;

    // Change texture at end?
    if(info->iparm[9] == SPREF_NONE || info->iparm[9] == SPREF_SPECIAL)
        mover->setflat = info->iparm[10];
    else
    {
        if(!XS_GetPlane(line, sector, info->iparm[9], NULL, 0, &mover->setflat, 0))
            XG_Dev("  Couldn't find suitable texture to set when move ends!");
    }

    // Init timer.
    mover->timer = XG_RandomInt(mover->mininterval, mover->maxinterval);

    // Do we need to wait before starting the move?
    if(xline->xg->fdata > 0)
    {
        mover->timer = FLT2TIC(xline->xg->fdata);
        mover->flags |= PMF_WAIT;
    }

    // Increment wait time.
    xline->xg->fdata += info->fparm[6];

    // Add the thinker if necessary.
    P_AddThinker(&mover->thinker);

    // Do start stuff. Play sound?
    if(playsound)
        XS_SectorSound(sector, SORG_FLOOR + ceiling, info->iparm[4]);

    // Change texture at start?
    if(info->iparm[7] == SPREF_NONE || info->iparm[7] == SPREF_SPECIAL)
        flat = info->iparm[8];
    else
    {
        if(!XS_GetPlane(line, sector, info->iparm[7], NULL, 0, &flat, 0))
            XG_Dev("  Couldn't find suitable texture to set when move starts!");
    }
    if(flat > 0)
        XS_ChangePlaneTexture(sector, ceiling, flat, NULL);

    // Should we play no more sounds?
    if(info->iparm[3] & PMF_ONE_SOUND_ONLY)
    {
        // Sound was played only for the first plane.
        xline->xg->idata = false;
    }

    // Change sector type right now?
    st = info->iparm[12];
    if(info->iparm[11] != LPREF_NONE)
    {
        if(XL_TraversePlanes
           (line, info->iparm[11], info->iparm[12], 0, &st, false, 0,
            XSTrav_HighestSectorType))
        {
            XS_SetSectorType(sector, st);
        }
        else
            XG_Dev("  SECTOR TYPE NOT SET (nothing referenced)");
    }

    // Change sector type in the end of move?
    st = info->iparm[14];
    if(info->iparm[13] != LPREF_NONE)
    {
        if(XL_TraversePlanes
           (line, info->iparm[13], info->iparm[14], 0, &st, false, 0,
            XSTrav_HighestSectorType))
        {   // OK, found one or more.
            mover->setsector = st;
        }
        else
        {
            XG_Dev("  SECTOR TYPE WON'T BE CHANGED AT END (nothing referenced)");
            mover->setsector = -1;
        }
    }
    else
        mover->setsector = -1;

    return true; // Keep looking...
}

void XS_InitStairBuilder(line_t *line)
{
    memset(builder, 0, numsectors);
}

boolean XS_DoBuild(sector_t *sector, boolean ceiling, line_t *origin,
                   linetype_t *info, uint stepcount)
{
    static float firstheight;
    uint        secnum = P_ToIndex(sector);
    float       waittime;
    xgplanemover_t *mover;

    // Make sure each sector is only processed once.
    if(builder[secnum] & BL_BUILT)
        return false; // Already built this one!
    builder[secnum] |= BL_WAS_BUILT;

    // Create a new mover for the plane.
    mover = XS_GetPlaneMover(sector, ceiling);
    mover->origin = origin;

    // Setup the mover.
    if(stepcount != 0)
        firstheight =
        P_GetFloatp(sector, (ceiling? DMU_CEILING_HEIGHT:DMU_FLOOR_HEIGHT));

    mover->destination =
        firstheight + (stepcount + 1) * info->fparm[1];

    mover->speed = (info->fparm[0] + stepcount * info->fparm[6]);
    if(mover->speed <= 0)
        mover->speed = 1 / 1000;

    mover->mininterval = FLT2TIC(info->fparm[4]);
    mover->maxinterval = FLT2TIC(info->fparm[5]);

    if(info->iparm[8])
        mover->flags = PMF_CRUSH;

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
        XS_SectorSound(sector, SORG_FLOOR + ceiling, info->iparm[5]);
    }

    // Do start stuff. Play sound?
    if(stepcount != 0)
    {
        // Start building start sound.
        XS_SectorSound(sector, SORG_FLOOR + ceiling, info->iparm[4]);
    }

    P_AddThinker(&mover->thinker);

    return true; // Building has begun!
}

int C_DECL XSTrav_BuildStairs(sector_t *sector, boolean ceiling,
                              void *context, void *context2,
                              mobj_t *activator)
{
    boolean     found = true;
    int         k;
    uint        i, lowest, stepcount = 0;
    line_t     *line;
    line_t     *origin = (line_t *) context;
    linetype_t *info = context2;
    sector_t   *sec;
    boolean     picstop = info->iparm[2] != 0;
    boolean     spread = info->iparm[3] != 0;
    int         mypic;

    XG_Dev("XSTrav_BuildStairs: Sector %i, %s", P_ToIndex(sector),
           ceiling ? "ceiling" : "floor");

    // i2: (true/false) stop when texture changes
    // i3: (true/false) spread build?

    mypic = ceiling ? P_GetIntp(sector, DMU_CEILING_MATERIAL) :
                      P_GetIntp(sector, DMU_FLOOR_MATERIAL);

    // Apply to first step.
    XS_DoBuild(sector, ceiling, origin, info, stepcount);

    while(found)
    {
        stepcount++;

        // Mark the sectors of the last step as processed.
        for(i = 0; i < numsectors; ++i)
        {
            if(builder[i] & BL_WAS_BUILT)
            {
                builder[i] &= ~BL_WAS_BUILT;
                builder[i] |= BL_BUILT;
            }
        }

        // Scan the sectors for the next ones to spread to.
        found = false;
        lowest = numlines;
        for(i = 0; i < numsectors; ++i)
        {
            // Only spread from built sectors (spread only once!).
            if(!(builder[i] & BL_BUILT) || builder[i] & BL_SPREADED)
                continue;

            builder[i] |= BL_SPREADED;

            // Any 2-sided lines facing the right way?
            sec = P_ToPtr(DMU_SECTOR, i);

            for(k = 0; k < P_GetIntp(sec, DMU_LINE_COUNT); ++k)
            {
                line = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | k);

                if(!P_GetPtrp(line, DMU_FRONT_SECTOR) ||
                   !P_GetPtrp(line, DMU_BACK_SECTOR))
                    continue;

                if(P_GetPtrp(line, DMU_FRONT_SECTOR) !=
                   P_ToPtr(DMU_SECTOR, i))
                    continue;

                if(picstop)
                {   // Planepic must match.
                    if(ceiling)
                    {
                        if(P_GetIntp(sec, DMU_CEILING_MATERIAL) != mypic)
                            continue;
                    }
                    else
                    {
                        if(P_GetIntp(sec, DMU_FLOOR_MATERIAL) != mypic)
                            continue;
                    }
                }

                // Don't spread to sectors which have already spreaded.
                if(builder[P_GetIntp(line, DMU_BACK_SECTOR)] & BL_SPREADED)
                    continue;

                // Build backsector.
                found = true;
                if(spread)
                {
                    XS_DoBuild(P_GetPtrp(line, DMU_BACK_SECTOR), ceiling,
                               origin, info, stepcount);
                }
                else
                {
                    // We need the lowest line number.
                    if(P_ToIndex(line) < lowest)
                        lowest = P_ToIndex(line);
                }
            }
        }

        if(!spread && found)
        {
            XS_DoBuild(P_GetPtr(DMU_LINE, lowest, DMU_BACK_SECTOR), ceiling, origin, info,
                       stepcount);
        }
    }

    return true; // Continue searching for planes...
}

int C_DECL XSTrav_SectorSound(struct sector_s *sec, boolean ceiling,
                              void *context, void *context2,
                              mobj_t *activator)
{
    int         originid;
    linetype_t *info = context2;

    if(info->iparm[3])
        originid = info->iparm[3];
    else
        originid = SORG_CENTER;

    XS_SectorSound(sec, originid, info->iparm[2]);
    return true;
}

int C_DECL XSTrav_PlaneTexture(struct sector_s *sec, boolean ceiling,
                               void *context, void *context2,
                               mobj_t *activator)
{
    line_t     *line = (line_t *) context;
    linetype_t *info = context2;
    int         pic;
    float       rgb[3];

    // i2: (spref) texture origin
    // i3: texture number (flat), used with SPREF_NONE
    // i4: red
    // i5: green
    // i6: blue
    if(info->iparm[2] == SPREF_NONE)
    {
        pic = info->iparm[3];
    }
    else
    {
        if(!XS_GetPlane(line, sec, info->iparm[2], NULL, 0, &pic, 0))
            XG_Dev("XSTrav_PlaneTexture: Sector %i, couldn't find suitable texture!",
                   P_ToIndex(sec));
    }

    rgb[0] = info->iparm[4] / 255.f;
    CLAMP(rgb[0], 0, 1);
    rgb[1] = info->iparm[5] / 255.f;
    CLAMP(rgb[1], 0, 1);
    rgb[2] = info->iparm[6] / 255.f;
    CLAMP(rgb[2], 0, 1);

    // Set the texture.
    XS_ChangePlaneTexture(sec, ceiling, pic, rgb);

    return true;
}

int C_DECL XSTrav_SectorType(struct sector_s *sec, boolean ceiling,
                             void *context, void *context2,
                             mobj_t *activator)
{
    linetype_t *info = context2;

    XS_SetSectorType(sec, info->iparm[2]);
    return true;
}

int C_DECL XSTrav_SectorLight(sector_t *sector, boolean ceiling,
                              void *context, void *context2,
                              mobj_t *activator)
{
    line_t     *line = (line_t *) context;
    linetype_t *info = context2;
    int         num, levels[MAX_VALS], i = 0;
    float       uselevel = P_GetFloatp(sector, DMU_LIGHT_LEVEL);
    sector_t   *frontsector, *backsector;
    float       usergb[3];

    frontsector = P_GetPtrp(line, DMU_FRONT_SECTOR);
    backsector = P_GetPtrp(line, DMU_BACK_SECTOR);

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
            uselevel = P_GetFloatp(frontsector, DMU_LIGHT_LEVEL);
            break;

        case LIGHTREF_BACK:
            if(backsector)
                uselevel = P_GetFloatp(backsector, DMU_LIGHT_LEVEL);
            break;

        case LIGHTREF_ORIGINAL:
            uselevel = P_ToXSector(sector)->origlight;
            break;

        case LIGHTREF_HIGHEST:
        case LIGHTREF_LOWEST:
        case LIGHTREF_NEXT_HIGHEST:
        case LIGHTREF_NEXT_LOWEST:
            num =
                XS_AdjoiningPlanes(sector, ceiling, NULL, NULL, levels, NULL);

            // Were there adjoining sectors?
            if(!num)
                break;

            switch(info->iparm[4])
            {
            case LIGHTREF_HIGHEST:
                i = FindMaxOf(levels, num);
                break;

            case LIGHTREF_LOWEST:
                i = FindMinOf(levels, num);
                break;

            case LIGHTREF_NEXT_HIGHEST:
                i = FindNextOf(levels, num, 255.f * uselevel);
                break;

            case LIGHTREF_NEXT_LOWEST:
                i = FindPrevOf(levels, num, 255.f * uselevel);
                break;
            }

            if(i >= 0)
                uselevel = (float) levels[i] / 255.f;
            break;

        default:
            break;
        }

        // Add the offset.
        uselevel += (float) info->iparm[5] / 255.f;

        // Clamp the result.
        if(uselevel < 0)
            uselevel = 0;
        if(uselevel > 1)
            uselevel = 1;

        // Set the value.
        P_SetFloatp(sector, DMU_LIGHT_LEVEL, uselevel);
    }

    if(info->iparm[3])
    {
        switch(info->iparm[6])
        {
        case LIGHTREF_MY:
            {
            sector_t* sector = P_GetPtrp(line, DMU_FRONT_SECTOR);

            P_GetFloatpv(sector, DMU_COLOR, usergb);
            break;
            }
        case LIGHTREF_BACK:
            {
            sector_t* sector = P_GetPtrp(line, DMU_BACK_SECTOR);

            if(sector)
                P_GetFloatpv(sector, DMU_COLOR, usergb);
            break;
            }
        case LIGHTREF_ORIGINAL:
            memcpy(usergb, P_ToXSector(sector)->origrgb, sizeof(float) * 3);
            break;

        default:
            memset(usergb, 0, sizeof(usergb));
            break;
        }

        for(num = 0; num < 3; ++num)
        {
            float f = usergb[num] + (float) info->iparm[7 + num] / 255.f;
            if(f < 0)
                f = 0;
            if(f > 1)
                f = 1;
            P_SetFloatp(sector, TO_DMU_COLOR(num), f);
        }
    }

    return true;
}

int C_DECL XSTrav_MimicSector(sector_t *sector, boolean ceiling,
                              void *context, void *context2,
                              mobj_t *activator)
{
    line_t     *line = (line_t *) context;
    linetype_t *info = context2;
    sector_t   *from = NULL;
    uint        refdata;

    // Set the spref data parameter (tag or index).
    switch(info->iparm[2])
    {
    case SPREF_TAGGED_FLOOR:
    case SPREF_TAGGED_CEILING:
    case SPREF_INDEX_FLOOR:
    case SPREF_INDEX_CEILING:
    case SPREF_ACT_TAGGED_FLOOR:
    case SPREF_ACT_TAGGED_CEILING:
        if(info->iparm[3] >= 0)
            refdata = info->iparm[3];
        break;

    case SPREF_LINE_ACT_TAGGED_FLOOR:
    case SPREF_LINE_ACT_TAGGED_CEILING:
        if(info->act_tag >= 0);
            refdata = info->act_tag;
        break;

    default:
        refdata = 0;
        break;
    }

    // If can't apply to a sector, just skip it.
    if(!XS_GetPlane(line, sector, info->iparm[2], &refdata, 0, 0, &from))
    {
        XG_Dev("XSTrav_MimicSector: No suitable neighbor " "for %i.\n",
               P_ToIndex(sector));
        return true;
    }

    // Mimicing itself is pointless.
    if(from == sector)
        return true;

    XG_Dev("XSTrav_MimicSector: Sector %i mimicking sector %i",
           P_ToIndex(sector), P_ToIndex(from));

    // Copy the properties of the target sector.
    P_CopySector(from, sector);

    P_ChangeSector(sector, false);

    // Copy type as well.
    XS_SetSectorType(sector, P_ToXSector(from)->special);

    if(P_ToXSector(from)->xg)
        memcpy(P_ToXSector(sector)->xg, P_ToXSector(from)->xg, sizeof(xgsector_t));

    return true;
}

int C_DECL XSTrav_Teleport(sector_t *sector, boolean ceiling, void *context,
                           void *context2, mobj_t *thing)
{
    mobj_t     *mo = NULL;
    boolean     ok = false;
    linetype_t *info = context2;

    // Don't teleport things marked noteleport!
    if(thing->flags2 & MF2_NOTELEPORT)
    {
        XG_Dev("XSTrav_Teleport: Activator is unteleportable (THING type %i)",
                thing->type);
        return false;
    }

    for(mo = P_GetPtrp(sector, DMT_MOBJS); mo; mo = mo->snext)
    {
        thinker_t *th = (thinker_t*) mo;

        // Not a mobj.
        if(th->function != P_MobjThinker)
            continue;

        // Not a teleportman.
        if(mo->type != MT_TELEPORTMAN)
            continue;

        ok = true;
        break;
    }

    if(ok)
    {   // We can teleport.
        mobj_t     *flash;
        unsigned    an;
        float       oldpos[3];
        float       thfloorz, thceilz;
        float       aboveFloor, fogDelta = 0;

        XG_Dev("XSTrav_Teleport: Sector %i, %s, %s%s", P_ToIndex(sector),
                info->iparm[2]? "No Flash":"", info->iparm[3]? "Play Sound":"Silent",
                info->iparm[4]? " Stomp" : "");

        if(!P_TeleportMove(thing, mo->pos[VX], mo->pos[VY], (info->iparm[4] > 0? 1 : 0)))
        {
            XG_Dev("XSTrav_Teleport: No free space at teleport exit. Aborting teleport...");
            return false;
        }

        memcpy(oldpos, thing->pos, sizeof(thing->pos));
        thfloorz = P_GetFloatp(thing->subsector, DMU_FLOOR_HEIGHT);
        thceilz  = P_GetFloatp(thing->subsector, DMU_CEILING_HEIGHT);
        aboveFloor = thing->pos[VZ] - thfloorz;

        // Players get special consideration
        if(thing->player)
        {
            if((thing->player->plr->mo->flags2 & MF2_FLY) && aboveFloor)
            {
                thing->pos[VZ] = thfloorz + aboveFloor;
                if(thing->pos[VZ] + thing->height > thceilz)
                {
                    thing->pos[VZ] = thceilz - thing->height;
                }
                thing->dplayer->viewZ =
                    thing->pos[VZ] + thing->dplayer->viewheight;
            }
            else
            {
                thing->pos[VZ] = thfloorz;
                thing->dplayer->viewZ =
                    thing->pos[VZ] + thing->dplayer->viewheight;
                thing->dplayer->lookdir = 0; /* $unifiedangles */
            }
#if __JHERETIC__
            if(!thing->player->powers[PT_WEAPONLEVEL2])
#endif
            {
                // Freeze player for about .5 sec
                thing->reactiontime = 18;
            }

            //thing->dplayer->clAngle = thing->angle; /* $unifiedangles */
            thing->dplayer->flags |= DDPF_FIXANGLES | DDPF_FIXPOS | DDPF_FIXMOM;
        }
#if __JHERETIC__
        else if(thing->flags & MF_MISSILE)
        {
            thing->pos[VZ] = thfloorz + aboveFloor;
            if(thing->pos[VZ] + thing->height > thceilz)
            {
                thing->pos[VZ] = thceilz - thing->height;
            }
        }
#endif
        else
        {
            thing->pos[VZ] = thfloorz;
        }

        // Spawn flash at the old position?
        if(!info->iparm[2])
        {
            // Old position
#if __JHERETIC__
            fogDelta = ((thing->flags & MF_MISSILE)? 0 : TELEFOGHEIGHT);
#endif
            flash = P_SpawnMobj3f(MT_TFOG, oldpos[VX], oldpos[VY],
                                  oldpos[VZ] + fogDelta);
            // Play a sound?
            if(info->iparm[3])
                S_StartSound(info->iparm[3], flash);
        }

        an = mo->angle >> ANGLETOFINESHIFT;

        // Spawn flash at the new position?
        if(!info->iparm[2])
        {
            // New position
            flash = P_SpawnMobj3f(MT_TFOG,
                                  mo->pos[VX] + 20 * FIX2FLT(finecosine[an]),
                                  mo->pos[VY] + 20 * FIX2FLT(finesine[an]),
                                  mo->pos[VZ] + fogDelta);
            // Play a sound?
            if(info->iparm[3])
                S_StartSound(info->iparm[3], flash);
        }

        // Adjust the angle to match that of the teleporter exit
        thing->angle = mo->angle;

        // Have we teleported from/to a sector with a non-solid floor?
        if(thing->flags2 & MF2_FLOORCLIP)
        {
            if(thing->pos[VZ] ==
               P_GetFloatp(thing->subsector,
                           DMU_SECTOR_OF_SUBSECTOR | DMU_FLOOR_HEIGHT) &&
               P_GetMobjFloorType(thing) >= FLOOR_LIQUID)
            {
                thing->floorclip = 10;
            }
            else
            {
                thing->floorclip = 0;
            }
        }

        if(thing->flags & MF_MISSILE)
        {
            an >>= ANGLETOFINESHIFT;
            thing->mom[MX] = thing->info->speed * FIX2FLT(finecosine[an]);
            thing->mom[MY] = thing->info->speed * FIX2FLT(finesine[an]);
        }
        else
        {
            thing->mom[MX] = thing->mom[MY] = thing->mom[MZ] = 0;
        }
    }
    else
    {   // Keep looking, there may be another referenced sector we could
        // teleport to...
        XG_Dev("XSTrav_Teleport: No teleport exit in referenced sector (ID %i)."
               " Continuing search...", P_ToIndex(sector));
        return true;
    }

    return false; // Only do this once.
}

int XF_FindRewindMarker(char *func, int pos)
{
    while(pos > 0 && func[pos] != '>')
        pos--;
    if(func[pos] == '>')
        pos++;

    return pos;
}

int XF_GetCount(function_t * fn, int *pos)
{
    char       *end;
    int         count;

    count = strtol(fn->func + *pos, &end, 10);
    *pos = end - fn->func;

    return count;
}

float XF_GetValue(function_t * fn, int pos)
{
    int         ch;

    if(fn->func[pos] == '/' || fn->func[pos] == '%')
    {
        // Exact value.
        return strtod(fn->func + pos + 1, 0);
    }

    ch = tolower(fn->func[pos]);
    // A=0, Z=25.
    return (ch - 'a') / 25.0f;
}

/**
 * Returns the position of the next value.
 * Repeat counting is handled here.
 * Poke should be true only if fn->pos is really about to move.
 */
int XF_FindNextPos(function_t *fn, int pos, boolean poke, sector_t *sec)
{
    int         startpos = pos;
    int         c;
    char       *ptr;

    if(fn->repeat > 0)
    {
        if(poke)
            fn->repeat--;
        return pos;
    }

    // Skip current.
    if(fn->func[pos] == '/' || fn->func[pos] == '%')
    {
        strtod(fn->func + pos + 1, &ptr);
        pos = ptr - fn->func; // Go to the end.
    }
    else
    {   // It's just a normal character [a-z,A-Z].
        pos++;
    }

    while(pos != startpos && fn->func[pos])
    {
        // Check for various special characters.
        if(isdigit(fn->func[pos]))
        {   // A repeat!
            // Move pos to the value to be repeated and set repeat counter.
            c = XF_GetCount(fn, &pos) - 1;
            if(poke)
                fn->repeat = c;
            return pos;
        }

        if(fn->func[pos] == '!')
        {   // Chain event.
            pos++;
            c = XF_GetCount(fn, &pos);
            if(poke)
            {
                // Sector funcs don't have activators.
                XS_DoChain(sec, XSCE_FUNCTION, c, NULL);
            }
            continue;
        }

        if(fn->func[pos] == '#')
        {   // Set timer.
            pos++;
            c = XF_GetCount(fn, &pos);
            if(poke)
            {
                fn->timer = 0;
                fn->maxtimer = c;
            }
            continue;
        }

        if(fn->func[pos] == '?')
        {   // Random timer.
            pos++;
            c = XF_GetCount(fn, &pos);
            if(poke)
            {
                fn->timer = 0;
                fn->maxtimer = XG_RandomInt(0, c);
            }
            continue;
        }

        if(fn->func[pos] == '<')
        {   // Rewind.
            pos = XF_FindRewindMarker(fn->func, pos);
            continue;
        }

        if(poke)
        {
            if(islower(fn->func[pos]) || fn->func[pos] == '/')
            {
                int     next = XF_FindNextPos(fn, pos, false, sec);

                if(fn->func[next] == '.')
                {
                    pos++;
                    continue;
                }
                break;
            }
        }
        else if(fn->func[pos] == '.')
        {
            break;
        }

        // Is it a value, then?
        if(isalpha(fn->func[pos]) || fn->func[pos] == '/' ||
           fn->func[pos] == '%')
            break;

        // A bad character, skip it.
        pos++;
    }

    return pos;
}

/**
 * Tick the function, update value.
 */
void XF_Ticker(function_t *fn, sector_t *sec)
{
    int         next;
    float       inter;

    // Store the previous value of the function.
    fn->oldvalue = fn->value;

    // Is there a function?
    if(!ISFUNC(fn) || fn->link)
        return;

    // Increment time.
    if(fn->timer++ >= fn->maxtimer)
    {
        fn->timer = 0;
        fn->maxtimer = XG_RandomInt(fn->mininterval, fn->maxinterval);

        // Advance to next pos.
        fn->pos = XF_FindNextPos(fn, fn->pos, true, sec);
    }

    /**
     * Syntax:
     *
     * abcdefghijlkmnopqrstuvwxyz (26)
     *
     * az.< (fade from 0 to 1, break interpolation and repeat)
     * [note that AZ.AZ is the same as AZAZ]
     * [also note that a.z is the same as z]
     * az.>mz< (fade from 0 to 1, break, repeat fade from 0.5 to 1 to 0.5)
     * 10a10z is the same as aaaaaaaaaazzzzzzzzzz
     * aB or Ba do not interpolate
     * zaN (interpolate from 1 to 0, wait at 0, stay at N)
     * za.N (interpolate from 1 to 0, skip to N)
     * 1A is the same as A
     */

    // Stop?
    if(!fn->func[fn->pos])
        return;

    if(isupper(fn->func[fn->pos]) || fn->func[fn->pos] == '%')
    {   // No interpolation.
        fn->value = XF_GetValue(fn, fn->pos);
    }
    else
    {
        inter = 0;
        next = XF_FindNextPos(fn, fn->pos, false, sec);
        if(islower(fn->func[next]) || fn->func[next] == '/')
        {
            if(fn->maxtimer)
                inter = fn->timer / (float) fn->maxtimer;
        }

        fn->value = (1 - inter) * XF_GetValue(fn, fn->pos) +
            inter * XF_GetValue(fn, next);
    }

    // Scale and offset.
    fn->value = fn->value * fn->scale + fn->offset;
}

void XS_UpdatePlanes(sector_t *sec)
{
    int         i;
    xgsector_t *xg;
    function_t *fn;
    boolean     docrush;

    xg = P_ToXSector(sec)->xg;
    docrush = (xg->info.flags & STF_CRUSH) != 0;

    // Update floor.
    fn = &xg->plane[XGSP_FLOOR];
    if(UPDFUNC(fn))
    {   // Changed.
        // How different?
        i = fn->value - P_GetFloatp(sec, DMU_FLOOR_HEIGHT);
        if(i)
        {
            // Move the floor plane accordingly.
            T_MovePlane(sec, abs(i), fn->value, docrush, 0, SIGN(i));
        }
    }

    // Update celing.
    fn = &xg->plane[XGSP_CEILING];
    if(UPDFUNC(fn))
    {   // Changed.
        // How different?
        i = fn->value - P_GetFloatp(sec, DMU_CEILING_HEIGHT);
        if(i)
        {
            // Move the ceiling accordingly.
            T_MovePlane(sec, abs(i), fn->value, docrush, 1, SIGN(i));
        }
    }
}

void XS_UpdateLight(sector_t *sec)
{
    int         i;
    float       c, lightlevel;
    xgsector_t *xg;
    function_t *fn;

    xg = P_ToXSector(sec)->xg;

    // Light intensity.
    fn = &xg->light;
    if(UPDFUNC(fn))
    {   // Changed.
        lightlevel = fn->value / 255.f;

        if(lightlevel < 0)
            lightlevel = 0;
        if(lightlevel > 1)
            lightlevel = 1;

        P_SetFloatp(sec, DMU_LIGHT_LEVEL, lightlevel);
    }

    // Red, green and blue.
    for(i = 0; i < 3; ++i)
    {
        fn = &xg->rgb[i];
        if(!UPDFUNC(fn))
            continue;

        // Changed.
        c = fn->value / 255.f;
        if(c < 0)
            c = 0;
        if(c > 1)
            c = 1;
        P_SetFloatp(sec, TO_DMU_COLOR(i), c);
    }
}

void XS_DoChain(sector_t *sec, int ch, int activating, void *act_thing)
{
    xgsector_t *xg;
    sectortype_t *info;
    float       flevtime = TIC2FLT(leveltime);
    line_t     *dummyLine;
    xline_t    *xdummyLine;
    linetype_t *ltype;

    xg = P_ToXSector(sec)->xg;
    info = &xg->info;

    if(ch < XSCE_NUM_CHAINS)
    {
        // How's the counter?
        if(!info->count[ch])
            return;

        // How's the time?
        if(flevtime < info->start[ch] ||
           (info->end[ch] > 0 && flevtime > info->end[ch]))
            return; // Not operating at this time.

        // Time to try the chain. Reset timer.
        xg->chain_timer[ch] =
            XG_RandomInt(FLT2TIC(info->interval[ch][0]),
                         FLT2TIC(info->interval[ch][1]));
    }

    // Prepare the dummy line to use for the event.
    dummyLine = P_AllocDummyLine();
    xdummyLine = P_ToXLine(dummyLine);
    xdummyLine->xg = Z_Calloc(sizeof(xgline_t), PU_LEVEL, 0);

    P_SetPtrp(dummyLine, DMU_FRONT_SECTOR, sec);

    xdummyLine->special =
        (ch == XSCE_FUNCTION ? activating : info->chain[ch]);

    xdummyLine->tag = P_ToXSector(sec)->tag;

    ltype = XL_GetType(xdummyLine->special);
    if(!ltype)
    {
        // What is this? There is no such XG line type.
        XG_Dev("XS_DoChain: Unknown XG line type %i", xdummyLine->special);
        // We're done, free the dummy.
        Z_Free(xdummyLine->xg);
        P_FreeDummyLine(dummyLine);
        return;
    }

    memcpy(&xdummyLine->xg->info, ltype, sizeof(*ltype));

    if(act_thing)
        xdummyLine->xg->activator = act_thing;
    else
        xdummyLine->xg->activator = NULL;

    xdummyLine->xg->active = (ch == XSCE_FUNCTION ? false : !activating);

    XG_Dev("XS_DoChain: Dummy line will show up as %i", P_ToIndex(dummyLine));

    // Send the event.
    if(XL_LineEvent((ch == XSCE_FUNCTION ? XLE_FUNC : XLE_CHAIN), 0,
                    dummyLine, 0, act_thing))
    {   // Success!
        if(ch < XSCE_NUM_CHAINS)
        {
            // Decrease counter.
            if(info->count[ch] > 0)
            {
                info->count[ch]--;

                XG_Dev
                    ("XS_DoChain: %s, sector %i (activating=%i): Counter now at %i",
                     ch == XSCE_FLOOR ? "FLOOR" : ch ==
                     XSCE_CEILING ? "CEILING" : ch ==
                     XSCE_INSIDE ? "INSIDE" : ch ==
                     XSCE_TICKER ? "TICKER" : ch ==
                     XSCE_FUNCTION ? "FUNCTION" : "???", P_ToIndex(sec),
                     activating, info->count[ch]);
            }
        }
    }

    // We're done, free the dummy.
    Z_Free(xdummyLine->xg);
    P_FreeDummyLine(dummyLine);
}

int XSTrav_SectorChain(sector_t *sec, mobj_t *mo, int ch)
{
    xgsector_t *xg;
    sectortype_t *info;
    player_t   *player = mo->player;
    int         flags;
    boolean     activating;

    xg = P_ToXSector(sec)->xg;
    info = &xg->info;
    flags = info->chain_flags[ch];

    // Check mobj type.
    if(flags & (SCEF_ANY_A | SCEF_ANY_D | SCEF_TICKER_A | SCEF_TICKER_D))
        goto type_passes;
    if((flags & (SCEF_PLAYER_A | SCEF_PLAYER_D)) && player)
        goto type_passes;
    if((flags & (SCEF_OTHER_A | SCEF_OTHER_D)) && !player)
        goto type_passes;
    if((flags & (SCEF_MONSTER_A | SCEF_MONSTER_D)) && (mo->flags & MF_COUNTKILL))
        goto type_passes;
    if((flags & (SCEF_MISSILE_A | SCEF_MISSILE_D)) && (mo->flags & MF_MISSILE))
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
        if(mo->pos[VZ] > P_GetFloatp(sec, DMU_FLOOR_HEIGHT))
            return true;
        break;

    case XSCE_CEILING:
        // Is it touching the ceiling?
        if(mo->pos[VZ] + mo->height < P_GetFloatp(sec, DMU_CEILING_HEIGHT))
            return true;
        break;

    default:
        break;
    }

    XS_DoChain(sec, ch, activating, mo);

    return true; // Continue looking...
}

int XSTrav_Wind(sector_t *sec, mobj_t *mo, int data)
{
    sectortype_t *info;
    float       ang;

    if(mo->player && (mo->player->plr->flags & DDPF_CAMERA))
        return true; // Wind does not affect cameras.

    info = &(P_ToXSector(sec)->xg->info);
    ang = PI * info->wind_angle / 180;

    if(IS_CLIENT)
    {
        // Clientside wind only affects the local player.
        if(!mo->player || mo->player != &players[consoleplayer])
            return true;
    }

    // Does wind affect this sort of things?
    if(((info->flags & STF_PLAYER_WIND) && mo->player) ||
       ((info->flags & STF_OTHER_WIND) && !mo->player) ||
       ((info->flags & STF_MONSTER_WIND) && (mo->flags & MF_COUNTKILL)) ||
       ((info->flags & STF_MISSILE_WIND) && (mo->flags & MF_MISSILE)))
    {
        float       thfloorz = P_GetFloatp(mo->subsector, DMU_FLOOR_HEIGHT);
        float       thceilz  = P_GetFloatp(mo->subsector, DMU_CEILING_HEIGHT);

        if(!(info->flags & (STF_FLOOR_WIND | STF_CEILING_WIND)) ||
           ((info->flags & STF_FLOOR_WIND) && mo->pos[VZ] <= thfloorz) ||
           ((info->flags & STF_CEILING_WIND) &&
            mo->pos[VZ] + mo->height >= thceilz))
        {
            // Apply vertical wind.
            mo->mom[MZ] += info->vertical_wind;

            // Horizontal wind.
            mo->mom[MX] += cos(ang) * info->wind_speed;
            mo->mom[MY] += sin(ang) * info->wind_speed;
        }
    }

    return true; // Continue applying wind.
}

/**
 * @return              @c true, if all callbacks returned @c true.
 */
int XS_TraverseMobjs(sector_t *sec, int data,
                     int (*func) (sector_t *sec, mobj_t *mo, int data))
{
    mobj_t     *mo;
    thinker_t  *th;

    // Only traverse sectorlinked things.
    //for(mo = sec->thinglist; mo; mo = mo->snext)
    //{

    th = thinkercap.next;
    for(th = thinkercap.next; th != &thinkercap; th = th->next)
    {
        // Not a mobj.
        if(th->function != P_MobjThinker)
            continue;

        mo = (mobj_t *) th;

        // Wrong sector.
        if(sec != P_GetPtrp(mo->subsector, DMU_SECTOR))
            continue;

        if(!func(sec, mo, data))
            return false;
    }

    return true;
}

/**
 * Makes sure the offset is in the range 0..64.
 */
void XS_ConstrainPlaneOffset(float *offset)
{
    if(*offset > 64)
        *offset -= 64;
    if(*offset < 0)
        *offset += 64;
}

/**
 * Called for Extended Generalized sectors.
 */
void XS_Think(uint idx)
{
    int         i;
    float       ang;
    float       floorOffset[2], ceilOffset[2];
    sector_t   *sector;
    xsector_t  *xsector = P_GetXSector(idx);
    xgsector_t *xg;
    sectortype_t *info;

    if(!xsector)
    {
#if _DEBUG
Con_Message("XS_Think: Index (%i) not xsector!\n", idx);
#endif
        return; // Not an xsector? Perhaps we were passed a dummy index??
    }

    xg = xsector->xg;
    if(!xg)
        return; // Not an extended sector.

    if(xg->disabled)
        return; // This sector is disabled.

    sector = P_ToPtr(DMU_SECTOR, idx);
    info = &xg->info;

    if(!IS_CLIENT)
    {
        // Function tickers.
        for(i = 0; i < 2; ++i)
            XF_Ticker(&xg->plane[i], sector);
        XF_Ticker(&xg->light, sector);
        for(i = 0; i < 3; ++i)
            XF_Ticker(&xg->rgb[i], sector);

        // Update linked functions.
        for(i = 0; i < 3; ++i)
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
        for(i = 0; i < XSCE_NUM_CHAINS; ++i)
            xg->chain_timer[i]--;

        // Floor chain. Check any mobjs that are touching the floor of the
        // sector.
        if(info->chain[XSCE_FLOOR] && xg->chain_timer[XSCE_FLOOR] <= 0)
        {
            XS_TraverseMobjs(sector, XSCE_FLOOR, XSTrav_SectorChain);
        }

        // Ceiling chain. Check any mobjs that are touching the ceiling.
        if(info->chain[XSCE_CEILING] && xg->chain_timer[XSCE_CEILING] <= 0)
        {
            XS_TraverseMobjs(sector, XSCE_CEILING, XSTrav_SectorChain);
        }

        // Inside chain. Check any sectorlinked mobjs.
        if(info->chain[XSCE_INSIDE] && xg->chain_timer[XSCE_INSIDE] <= 0)
        {
            XS_TraverseMobjs(sector, XSCE_INSIDE, XSTrav_SectorChain);
        }

        // Ticker chain. Send an activate event if TICKER_D flag is not set.
        if(info->chain[XSCE_TICKER] && xg->chain_timer[XSCE_TICKER] <= 0)
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
                xg->timer =
                    XG_RandomInt(FLT2TIC(xg->info.sound_interval[0]),
                                 FLT2TIC(xg->info.sound_interval[1]));
                S_SectorSound(sector, SORG_CENTER, xg->info.ambient_sound);
            }
        }
    }

    // Floor Texture movement
    if(xg->info.texmove_angle[0] != 0)
    {
        ang = PI * xg->info.texmove_angle[0] / 180;
        // Get current values
        P_GetFloatpv(sector, DMU_FLOOR_MATERIAL_OFFSET_XY, floorOffset);
        // Apply the offsets
        floorOffset[VX] -= cos(ang) * xg->info.texmove_speed[0];
        floorOffset[VY] -= sin(ang) * xg->info.texmove_speed[0];
        // Set the results
        P_SetFloatpv(sector, DMU_FLOOR_MATERIAL_OFFSET_XY, floorOffset);
    }

    // Ceiling Texture movement
    if(xg->info.texmove_angle[1] != 0)
    {
        ang = PI * xg->info.texmove_angle[1] / 180;
        // Get current values
        P_GetFloatpv(sector, DMU_CEILING_MATERIAL_OFFSET_XY, ceilOffset);
        // Apply the offsets
        ceilOffset[VX] -= cos(ang) * xg->info.texmove_speed[1];
        ceilOffset[VY] -= sin(ang) * xg->info.texmove_speed[1];
        // Set the results
        P_SetFloatpv(sector, DMU_CEILING_MATERIAL_OFFSET_XY, ceilOffset);
    }

    // Wind for all sectorlinked mobjs.
    if(xg->info.wind_speed || xg->info.vertical_wind)
    {
        XS_TraverseMobjs(sector, 0, XSTrav_Wind);
    }
}

void XS_Ticker(void)
{
    uint        i;

    for(i = 0; i < numsectors; ++i)
    {
        XS_Think(i);
    }
}

float XS_Gravity(struct sector_s *sector)
{
    if(!P_ToXSector(sector)->xg ||
       !(P_ToXSector(sector)->xg->info.flags & STF_GRAVITY))
    {
        return P_GetGravity(); // World gravity.
    }
    else
    {   // Sector-specific gravity.
        float       gravity = P_ToXSector(sector)->xg->info.gravity;

        // Apply gravity modifier.
        if(IS_NETGAME && cfg.netGravity != -1)
            gravity *= (float) cfg.netGravity / 100;

        return gravity;
    }
}

float XS_Friction(struct sector_s *sector)
{
    if(!P_ToXSector(sector)->xg || !(P_ToXSector(sector)->xg->info.flags & STF_FRICTION))
        return FRICTION_NORMAL; // Normal friction.

    return P_ToXSector(sector)->xg->info.friction;
}

/**
 * @return              The thrust multiplier caused by friction.
 */
float XS_ThrustMul(struct sector_s *sector)
{
    float       x = XS_Friction(sector);

    if(x <= FRICTION_NORMAL)
        return 1; // Normal friction.

    if(x > 1)
        return 0; // There's nothing to thrust from!

    // {c = -93.31092643, b = 208.0448223, a = -114.7338958}
    return (-114.7338958 * x * x + 208.0448223 * x - 93.31092643);
}

/**
 * During update, definitions are re-read, so the pointers need to be
 * updated. However, this is a bit messy operation, prone to errors.
 * Instead, we just disable XG...
 */
void XS_Update(void)
{
    uint        i;
    xsector_t  *xsec;

    // It's all PU_LEVEL memory, so we can just lose it.
    for(i = 0; i < numsectors; ++i)
    {
        xsec = P_ToXSector(P_ToPtr(DMU_SECTOR, i));
        if(xsec->xg)
        {
            xsec->xg = NULL;
            xsec->special = 0;
        }
    }
}

#if 0 // no longer supported in 1.8.7
/*
 * Write XG types into a binary file.
 */
DEFCC(CCmdDumpXG)
{
    FILE   *file;

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
#endif

/**
 * $moveplane: Command line interface to the plane mover.
 */
DEFCC(CCmdMovePlane)
{
    boolean     isCeiling = !stricmp(argv[0], "moveceil");
    boolean     isBoth = !stricmp(argv[0], "movesec");
    boolean     isOffset = false, isCrusher = false;
    sector_t   *sector = NULL;
    float       units = 0, speed = FRACUNIT;
    int         p = 0;
    float       floorheight, ceilingheight;
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
        if(!players[consoleplayer].plr->mo)
            return false;
        sector =
            P_GetPtrp(players[consoleplayer].plr->mo->subsector, DMU_SECTOR);
    }
    else if(!stricmp(argv[1], "at") && argc >= 4)
    {
        p = 4;
        sector =
            P_GetPtrp(R_PointInSubsector((float) strtol(argv[2], 0, 0),
                                         (float) strtol(argv[3], 0, 0)),
                      DMU_SECTOR);
    }
    else if(!stricmp(argv[1], "tag") && argc >= 3)
    {
        int         tag = (short) strtol(argv[2], 0, 0);
        sector_t   *sec = NULL;
        iterlist_t *list;

        p = 3;

        list = P_GetSectorIterListForTag(tag, false);
        if(list)
        {   // Find the first sector with the tag.
            P_IterListResetIterator(list, true);
            while((sec = P_IterListIterator(list)) != NULL)
            {
                sector = sec;
                break;
            }
        }
    }
    else
    {   // Unknown mode.
        Con_Printf("Unknown mode.\n");
        return false;
    }

    floorheight = P_GetFloatp(sector, DMU_FLOOR_HEIGHT);
    ceilingheight = P_GetFloatp(sector, DMU_CEILING_HEIGHT);

    // No more arguments?
    if(argc == p)
    {
        Con_Printf("Ceiling = %g\nFloor = %g\n", ceilingheight,
                   floorheight);
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
        units = strtod(argv[p++], 0);
    }
    else
    {
        Con_Printf("You must specify Z-units.\n");
        return false; // Required parameter missing.
    }

    // The optional speed parameter.
    if(argc >= p + 1)
    {
        speed = strtod(argv[p++], 0);
        // The speed is always positive.
        if(speed < 0)
            speed = -speed;
    }

    // We must now have found the sector to operate on.
    if(!sector)
        return false;

    mover = XS_GetPlaneMover(sector, isCeiling);

    // Setup the thinker and add it to the list.
    mover->destination =
        units +
        (isOffset ? (isCeiling ? ceilingheight : floorheight) : 0);

    // Check that the destination is valid.
    if(!isBoth)
    {
        if(isCeiling &&
           mover->destination < floorheight + 4)
            mover->destination = floorheight + 4;
        if(!isCeiling &&
           mover->destination > ceilingheight - 4)
            mover->destination = ceilingheight - 4;
    }

    mover->speed = speed;
    if(isCrusher)
    {
        mover->crushspeed = speed * .5;  // Crush at half speed.
        mover->flags |= PMF_CRUSH;
    }
    if(isBoth)
        mover->flags |= PMF_OTHER_FOLLOWS;

    P_AddThinker(&mover->thinker);
    return true;
}

#endif
