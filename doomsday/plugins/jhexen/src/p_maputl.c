/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
 * p_maputl.c: Map utility routines - jHexen specific.
 */

// HEADER FILES ------------------------------------------------------------

#include "jhexen.h"
#include "r_common.h"
#include "p_map.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct mobjtargetableparams_s {
    mobj_t         *source;
    mobj_t         *target;
} mobjtargetableparams_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

intercept_t intercepts[MAXINTERCEPTS], *intercept_p;

divline_t trace;
boolean earlyout;
int     ptflags;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

boolean PIT_MobjTargetable(mobj_t *mo, void *data)
{
    mobjtargetableparams_t *params = (mobjtargetableparams_t*) data;

    if(params->source->player)
    {   // Minotaur looking around player.
        if((mo->flags & MF_COUNTKILL) || (mo->player && (mo != params->source)))
        {
            if(!(mo->flags & MF_SHOOTABLE) ||
               (mo->flags2 & MF2_DORMANT) ||
               ((mo->type == MT_MINOTAUR) && (mo->tracer == params->source)) ||
                (IS_NETGAME && !deathmatch && mo->player))
                return true; // Continue iteration.

            if(P_CheckSight(params->source, mo))
            {
                params->target = mo;
                return false; // Stop iteration.
            }
        }
    }
    else if(params->source->type == MT_MINOTAUR)
    {   // Looking around minotaur.
        mobj_t             *master = params->source->tracer;

        if((mo->flags & MF_COUNTKILL) ||
           (mo->player && (mo != master)))
        {
            if(!(mo->flags & MF_SHOOTABLE) ||
               (mo->flags2 & MF2_DORMANT) ||
               ((mo->type == MT_MINOTAUR) && (mo->tracer == params->source->tracer)) ||
                (IS_NETGAME && !deathmatch && mo->player))
                return true; // Continue iteration.

            if(P_CheckSight(params->source, mo))
            {
                params->target = mo;
                return false; // Stop iteration.
            }
        }
    }
    else if(params->source->type == MT_MSTAFF_FX2)
    {   // bloodscourge.
        if(((mo->flags & MF_COUNTKILL) ||
            (mo->player && mo != params->source->target)) &&
           !(mo->flags2 & MF2_DORMANT))
        {
            if(!(mo->flags & MF_SHOOTABLE) ||
               (IS_NETGAME && !deathmatch && mo->player))
                return true; // Continue iteration.

            if(P_CheckSight(params->source, mo))
            {
                angle_t             angle;
                mobj_t             *master;

                master = params->source->target;
                angle =
                    R_PointToAngle2(master->pos[VX], master->pos[VY],
                                    mo->pos[VY], mo->pos[VY]) -
                                    master->angle;
                angle >>= 24;
                if(angle > 226 || angle < 30)
                {
                    params->target = mo;
                    return false; // Stop iteration.
                }
            }
        }
    }
    else
    {   // spirits.
        if(((mo->flags & MF_COUNTKILL) ||
            (mo->player && mo != params->source->target)) &&
           !(mo->flags2 & MF2_DORMANT))
        {
            if(!(mo->flags & MF_SHOOTABLE) ||
               (IS_NETGAME && !deathmatch && mo->player) ||
               mo == params->source->target)
                return true; // Continue iteration.

            if(P_CheckSight(params->source, mo))
            {
                params->target = mo;
                return false; // Stop iteration.
            }
        }
    }

    return true; // Continue iteration.
}

/**
 * Searches around for targetable monsters/players near mobj.
 *
 * @return              Ptr to the targeted mobj if found, ELSE @c NULL;
 */
mobj_t* P_RoughMonsterSearch(mobj_t *mo, int distance)
{
#define MAPBLOCKUNITS       128
#define MAPBLOCKSHIFT       (FRACBITS+7)

    int             i, block[2], startBlock[2];
    int             count;
    float           mapOrigin[2];
    float           box[4];
    mobjtargetableparams_t params;

    mapOrigin[VX] = *((float*) DD_GetVariable(DD_MAP_MIN_X));
    mapOrigin[VY] = *((float*) DD_GetVariable(DD_MAP_MIN_Y));

    // The original blockmap generator added a border of 8 units.
    mapOrigin[VX] -= 8;
    mapOrigin[VY] -= 8;

    params.source = mo;
    params.target = NULL;

    // Convert from world units to map block units.
    distance /= MAPBLOCKUNITS;

    // Determine the start block.
    startBlock[VX] = FLT2FIX(mo->pos[VX] - mapOrigin[VX]) >> MAPBLOCKSHIFT;
    startBlock[VY] = FLT2FIX(mo->pos[VY] - mapOrigin[VY]) >> MAPBLOCKSHIFT;

    box[BOXLEFT]   = mapOrigin[VX] + startBlock[VX] * MAPBLOCKUNITS;
    box[BOXRIGHT]  = box[BOXLEFT] + MAPBLOCKUNITS;
    box[BOXBOTTOM] = mapOrigin[VY] + startBlock[VY] * MAPBLOCKUNITS;
    box[BOXTOP]    = box[BOXBOTTOM] + MAPBLOCKUNITS;

    // Check the first block.
    VALIDCOUNT++;
    if(!P_MobjsBoxIterator(box, PIT_MobjTargetable, &params))
    {   // Found a target right away!
        return params.target;
    }

    for(count = 1; count <= distance; count++)
    {
        block[VX] = startBlock[VX] - count;
        block[VY] = startBlock[VY] - count;

        box[BOXLEFT]   = mapOrigin[VX] + block[VX] * MAPBLOCKUNITS;
        box[BOXRIGHT]  = box[BOXLEFT] + MAPBLOCKUNITS;
        box[BOXBOTTOM] = mapOrigin[VY] + block[VY] * MAPBLOCKUNITS;
        box[BOXTOP]    = box[BOXBOTTOM] + MAPBLOCKUNITS;

        // Trace the first block section (along the top).
        for(i = 0; i < count * 2 + 1; ++i)
        {
            if(!P_MobjsBoxIterator(box, PIT_MobjTargetable, &params))
                return params.target;

            if(i < count * 2)
            {
                box[BOXLEFT]  += MAPBLOCKUNITS;
                box[BOXRIGHT] += MAPBLOCKUNITS;
            }
        }

        // Trace the second block section (right edge).
        for(i = 0; i < count * 2; ++i)
        {
            box[BOXBOTTOM] += MAPBLOCKUNITS;
            box[BOXTOP]    += MAPBLOCKUNITS;

            if(!P_MobjsBoxIterator(box, PIT_MobjTargetable, &params))
                return params.target;
        }

        // Trace the third block section (bottom edge).
        for(i = 0; i < count * 2; ++i)
        {
            box[BOXLEFT]  -= MAPBLOCKUNITS;
            box[BOXRIGHT] -= MAPBLOCKUNITS;

            if(!P_MobjsBoxIterator(box, PIT_MobjTargetable, &params))
                return params.target;
        }

        // Trace the final block section (left edge).
        for(i = 0; i < count * 2 - 1; ++i)
        {
            box[BOXBOTTOM] -= MAPBLOCKUNITS;
            box[BOXTOP]    -= MAPBLOCKUNITS;

            if(!P_MobjsBoxIterator(box, PIT_MobjTargetable, &params))
                return params.target;
        }
    }

    return NULL;

#undef MAPBLOCKUNITS
#undef MAPBLOCKSHIFT
}
