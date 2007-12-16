/**\file
 *\section License
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
 *
 * This program is covered by the HERETIC / HEXEN (LIMITED USE) source
 * code license; you can redistribute it and/or modify it under the terms
 * of the HERETIC / HEXEN source code license as published by Activision.
 *
 * THIS MATERIAL IS NOT MADE OR SUPPORTED BY ACTIVISION.
 *
 * WARRANTY INFORMATION.
 * This program is provided as is. Activision and it's affiliates make no
 * warranties of any kind, whether oral or written , express or implied,
 * including any warranty of merchantability, fitness for a particular
 * purpose or non-infringement, and no other representations or claims of
 * any kind shall be binding on or obligate Activision or it's affiliates.
 *
 * LICENSE CONDITIONS.
 * You shall not:
 *
 * 1) Exploit this Program or any of its parts commercially.
 * 2) Use this Program, or permit use of this Program, on more than one
 *    computer, computer terminal, or workstation at the same time.
 * 3) Make copies of this Program or any part thereof, or make copies of
 *    the materials accompanying this Program.
 * 4) Use the program, or permit use of this Program, in a network,
 *    multi-user arrangement or remote access arrangement, including any
 *    online use, except as otherwise explicitly provided by this Program.
 * 5) Sell, rent, lease or license any copies of this Program, without
 *    the express prior written consent of Activision.
 * 6) Remove, disable or circumvent any proprietary notices or labels
 *    contained on or within the Program.
 *
 * You should have received a copy of the HERETIC / HEXEN source code
 * license along with this program (Ravenlic.txt); if not:
 * http://www.ravensoft.com/
 */

/**
 * p_maputl.c: Map utility routines - jHexen specific.
 */

// HEADER FILES ------------------------------------------------------------

#include "jhexen.h"
#include "r_common.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct mobjtargetableparams_s {
    mobj_t         *source;
    mobj_t         *target;
} mobjtargetableparams_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static boolean isTargetable(mobj_t *mo, mobj_t *target);

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
mobj_t *P_RoughMonsterSearch(mobj_t *mo, int distance)
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
