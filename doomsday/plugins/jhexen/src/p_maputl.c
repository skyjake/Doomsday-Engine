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

static mobj_t *PIT_MobjBlockLinks(int index, void *data)
{
    mobj_t         *mo = (mobj_t*) data;
    mobj_t         *link, *root = P_GetBlockRootIdx(index);

    // If this doesn't work, check the backed-up version!
    for(link = root->bnext; link != root; link = link->bnext)
    {
        if(isTargetable(mo, link))
            return link;
    }

    return NULL;
}

/**
 * Searches around for targetable monsters/players near mobj.
 *
 * @return              Ptr to the targeted mobj if found, ELSE @c NULL;
 */
mobj_t *P_RoughMonsterSearch(mobj_t *mo, int distance)
{
    uint        blockX, blockY;
    uint        startX, startY;
    uint        blockIndex;
    uint        firstStop;
    uint        secondStop;
    uint        thirdStop;
    uint        finalStop;
    uint        count;
    mobj_t     *target;
    uint        bmapwidth = *(uint*) DD_GetVariable(DD_BLOCKMAP_WIDTH);
    uint        bmapheight = *(uint*) DD_GetVariable(DD_BLOCKMAP_HEIGHT);

    // Convert from world units to map block units.
    distance /= 128;

    P_PointToBlock(mo->pos[VX], mo->pos[VY], &startX, &startY);

    if(startX >= 0 && startX < bmapwidth &&
       startY >= 0 && startY < bmapheight)
    {
        target = PIT_MobjBlockLinks(startY * bmapwidth + startX, mo);
        if(target)
        {   // Found a target right away!
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

        // Trace the first block section (along the top).
        for(; blockIndex <= firstStop; blockIndex++)
        {
            int         x = blockIndex % bmapwidth;
            int         y = blockIndex / bmapwidth;
            target = PIT_MobjBlockLinks(blockIndex, mo);
            if(target)
                return target;
        }

        // Trace the second block section (right edge).
        for(blockIndex += -1 + bmapwidth; blockIndex <= secondStop; blockIndex += bmapwidth)
        {
            int         x = blockIndex % bmapwidth;
            int         y = blockIndex / bmapwidth;
            target = PIT_MobjBlockLinks(blockIndex, mo);
            if(target)
                return target;
        }

        // Trace the third block section (bottom edge).
        for(blockIndex -= 1 + bmapwidth; blockIndex >= thirdStop; blockIndex--)
        {
            int         x = blockIndex % bmapwidth;
            int         y = blockIndex / bmapwidth;
            target = PIT_MobjBlockLinks(blockIndex, mo);
            if(target)
                return target;
        }

        // Trace the final block section (left edge).
        for(blockIndex += 1 - bmapwidth; blockIndex > finalStop; blockIndex -= bmapwidth)
        {
            int         x = blockIndex % bmapwidth;
            int         y = blockIndex / bmapwidth;
            target = PIT_MobjBlockLinks(blockIndex, mo);
            if(target)
                return target;
        }
    }

    return NULL;
}

static boolean isTargetable(mobj_t *mo, mobj_t *target)
{
    if(mo->player)
    {   // Minotaur looking around player.
        if((target->flags & MF_COUNTKILL) || (target->player && (target != mo)))
        {
            if(!(target->flags & MF_SHOOTABLE) ||
               (target->flags2 & MF2_DORMANT) ||
               ((target->type == MT_MINOTAUR) && (target->tracer == mo)) ||
                (IS_NETGAME && !deathmatch && target->player))
                return false;

            if(P_CheckSight(mo, target))
                return true;
        }
    }
    else if(mo->type == MT_MINOTAUR)
    {   // Looking around minotaur.
        mobj_t             *master = mo->tracer;

        if((target->flags & MF_COUNTKILL) ||
           (target->player && (target != master)))
        {
            if(!(target->flags & MF_SHOOTABLE) ||
               (target->flags2 & MF2_DORMANT) ||
               ((target->type == MT_MINOTAUR) && (target->tracer == mo->tracer)) ||
                (IS_NETGAME && !deathmatch && target->player))
                return false;

            if(P_CheckSight(mo, target))
                return true;
        }
    }
    else if(mo->type == MT_MSTAFF_FX2)
    {   // bloodscourge.
        if(((target->flags & MF_COUNTKILL) ||
            (target->player && target != mo->target)) &&
           !(target->flags2 & MF2_DORMANT))
        {
            if(!(target->flags & MF_SHOOTABLE) ||
               (IS_NETGAME && !deathmatch && target->player))
                return false;

            if(P_CheckSight(mo, target))
            {
                angle_t             angle;
                mobj_t             *master;

                master = mo->target;
                angle =
                    R_PointToAngle2(master->pos[VX], master->pos[VY],
                                    target->pos[VY], target->pos[VY]) -
                                    master->angle;
                angle >>= 24;
                if(angle > 226 || angle < 30)
                {
                    return true;
                }
            }
        }
    }
    else
    {   // spirits.
        if(((target->flags & MF_COUNTKILL) ||
            (target->player && target != mo->target)) &&
           !(target->flags2 & MF2_DORMANT))
        {
            if(!(target->flags & MF_SHOOTABLE) ||
               (IS_NETGAME && !deathmatch && target->player) ||
               target == mo->target)
                return false;

            if(P_CheckSight(mo, target))
                return true;
        }
    }

    return false;
}
