/**\file
 *\section Copyright and License Summary
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
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

/*
 * Handle jHexen specific map utility routines.
 */

// HEADER FILES ------------------------------------------------------------

#include "jhexen.h"
#include "r_common.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

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

static mobj_t *RoughBlockCheck(mobj_t *mo, int index);

// CODE --------------------------------------------------------------------

/**
 * Unlinks a thing from block map and sectors
 */
void P_UnsetThingPosition(mobj_t *thing)
{
    P_UnlinkThing(thing);
}

/**
 * Links a thing into both a block and a subsector based on it's x y
 * Sets thing->subsector properly
 *
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

/**
 * Searches though the surrounding mapblocks for monsters/players within the
 * specified distance (in MAPBLOCKUNITS).
 */
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

static mobj_t *RoughBlockCheck(mobj_t *mo, int index)
{
    mobj_t *link, *root = P_GetBlockRootIdx(index);
    mobj_t *master;
    angle_t angle;

    // If this doesn't work, check the backed-up version!
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
        }
    }
    return NULL;
}
