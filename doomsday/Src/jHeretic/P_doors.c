/* $Id$
 *
 * Copyright (C) 1993-1996 by id Software, Inc.
 *
 * This source is available for distribution and/or modification
 * only under the terms of the DOOM Source Code License as
 * published by id Software. All rights reserved.
 *
 * The source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
 * for more details.
 */

/*
 * Door animation code (opening/closing)
 *  2006/01/17 DJS - Recreated using jDoom's p_doors.c as a base.
 */

// HEADER FILES ------------------------------------------------------------

#include "jHeretic/Doomdef.h"
#include "jHeretic/P_local.h"
#include "jHeretic/Soundst.h"
#include "jHeretic/h_stat.h"
#include "jHeretic/Dstrings.h"

#include "Common/dmu_lib.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void T_VerticalDoor(vldoor_t * door)
{
    xsector_t *xsec;
    result_e res;

    xsec = P_XSector(door->sector);

    switch (door->direction)
    {
    case 0:
        // WAITING
        if(!--door->topcountdown)
        {
            switch (door->type)
            {
            case normal:
                door->direction = -1;   // time to go back down
                S_SectorSound(door->sector, SORG_CEILING, sfx_doropn);
                break;

            case close30ThenOpen:
                door->direction = 1;
                S_SectorSound(door->sector, SORG_CEILING, sfx_doropn);
                break;

            default:
                break;
            }
        }
        break;

    case 2:
        // INITIAL WAIT
        if(!--door->topcountdown)
        {
            switch (door->type)
            {
            case raiseIn5Mins:
                door->direction = 1;
                door->type = normal;
                S_SectorSound(door->sector, SORG_CEILING, sfx_doropn);
                break;

            default:
                break;
            }
        }
        break;

    case -1:
        // DOWN
        res =
            T_MovePlane(door->sector, door->speed,
                        P_GetFixedp(door->sector, DMU_FLOOR_HEIGHT),
                        false, 1, door->direction);
        if(res == pastdest)
        {
            switch (door->type)
            {
            case normal:
            case close:
                xsec->specialdata = NULL;
                P_RemoveThinker(&door->thinker);    // unlink and free
                S_SectorSound(door->sector, SORG_CEILING, sfx_dorcls);
                break;

            case close30ThenOpen:
                door->direction = 0;
                door->topcountdown = 35 * 30;
                break;

            default:
                break;
            }
        }
        else if(res == crushed)
        {
            switch (door->type)
            {
            case close:      // DON'T GO BACK UP!
                break;

            default:
                door->direction = 1;
                S_SectorSound(door->sector, SORG_CEILING, sfx_doropn);
                break;
            }
        }
        break;

    case 1:
        // UP
        res =
            T_MovePlane(door->sector, door->speed, door->topheight, false, 1,
                        door->direction);

        if(res == pastdest)
        {
            switch (door->type)
            {
            case normal:
                door->direction = 0;    // wait at top
                door->topcountdown = door->topwait;
                break;

            case close30ThenOpen:
            case open:
                xsec->specialdata = NULL;
                P_RemoveThinker(&door->thinker);    // unlink and free
                S_StopSound(0, (mobj_t *) P_GetPtrp(door->sector,
                                                    DMU_CEILING_SOUND_ORIGIN));
                break;

            default:
                break;
            }
        }
        break;
    }
}

int EV_DoDoor(line_t *line, vldoor_e type)
{
    int     secnum, rtn;
    xsector_t *xsec;
    sector_t *sec;
    vldoor_t *door;

    secnum = -1;
    rtn = 0;

    while((secnum = P_FindSectorFromLineTag(line, secnum)) >= 0)
    {
        sec = P_ToPtr(DMU_SECTOR, secnum);
        xsec = &xsectors[secnum];

        if(xsec->specialdata)
            continue;

        // new door thinker
        rtn = 1;
        door = Z_Malloc(sizeof(*door), PU_LEVSPEC, 0);
        P_AddThinker(&door->thinker);
        xsec->specialdata = door;

        door->thinker.function = T_VerticalDoor;
        door->sector = sec;
        door->type = type;
        door->topwait = VDOORWAIT;

        switch (type)
        {
        case close:
            door->topheight = P_FindLowestCeilingSurrounding(sec);
            door->topheight -= 4 * FRACUNIT;
            door->direction = -1;
            door->speed = VDOORSPEED;
            S_SectorSound(door->sector, SORG_CEILING, sfx_doropn);
            break;

        case close30ThenOpen:
            door->topheight =
                P_GetFixedp(sec, DMU_CEILING_HEIGHT);
            door->direction = -1;
            door->speed = VDOORSPEED;
            S_SectorSound(door->sector, SORG_CEILING, sfx_doropn);
            break;

        case blazeOpen:
            door->direction = 1;
            door->topheight = P_FindLowestCeilingSurrounding(sec);
            door->topheight -= 4 * FRACUNIT;
            door->speed = VDOORSPEED * 3;
            if(door->topheight !=
                P_GetFixedp(sec, DMU_CEILING_HEIGHT))
                S_SectorSound(door->sector, SORG_CEILING, sfx_doropn);
            break;

        case normal:
        case open:
            door->direction = 1;
            door->topheight = P_FindLowestCeilingSurrounding(sec);
            door->topheight -= 4 * FRACUNIT;
            door->speed = VDOORSPEED;
            if(door->topheight !=
                P_GetFixedp(sec, DMU_CEILING_HEIGHT))
                S_SectorSound(door->sector, SORG_CEILING, sfx_doropn);
            break;

        default:
            break;
        }

    }
    return rtn;
}

/*
 * open a door manually, no tag value
 */
void EV_VerticalDoor(line_t *line, mobj_t *thing)
{
    player_t *player;
    xline_t *xline = P_XLine(line);
    sector_t *sec;
    xsector_t *xsec;
    vldoor_t *door;

    sec = P_GetPtrp(line, DMU_BACK_SECTOR);
    if(!sec)
        return;

    xsec = P_XSector(sec);

    //  Check for locks
    player = thing->player;

    switch (xline->special)
    {
    case 26:
    case 32:
        // Blue Lock
        if(!player)
            return;

        if(!player->keys[key_blue])
        {
            P_SetMessage(player, TXT_NEEDBLUEKEY);
            S_ConsoleSound(sfx_plroof, NULL, player - players);
            return;
        }
        break;

    case 27:
    case 34:
        // Yellow Lock
        if(!player)
            return;

        if(!player->keys[key_yellow])
        {
            P_SetMessage(player, TXT_NEEDYELLOWKEY);
            S_ConsoleSound(sfx_plroof, NULL, player - players);
            return;
        }
        break;

    case 28:
    case 33:
        // Green Lock
        if(!player)
            return;

        if(!player->keys[key_green])
        {
            P_SetMessage(player, TXT_NEEDGREENKEY);
            S_ConsoleSound(sfx_plroof, NULL, player - players);
            return;
        }
        break;
    }

    // if the sector has an active thinker, use it
    if(xsec->specialdata)
    {
        door = xsec->specialdata;
        switch (xline->special)
        {
        case 1:
        case 26:
        case 27:
        case 28:
            // ONLY FOR "RAISE" DOORS, NOT "OPEN"s
            if(door->direction == -1)
                door->direction = 1;    // go back up
            else
            {
                if(!thing->player)
                    return;     // JDC: bad guys never close doors

                door->direction = -1;   // start going down immediately
            }
            return;
        }
    }

    // for proper sound
    switch(xline->special)
    {
    case 1:
    case 31:
        // NORMAL DOOR SOUND
        S_SectorSound(sec, SORG_CEILING, sfx_doropn);
        break;

    default:
        // LOCKED DOOR SOUND
        S_SectorSound(sec, SORG_CEILING, sfx_doropn);
        break;
    }

    // new door thinker
    door = Z_Malloc(sizeof(*door), PU_LEVSPEC, 0);
    P_AddThinker(&door->thinker);
    xsec->specialdata = door;
    door->thinker.function = T_VerticalDoor;
    door->sector = sec;
    door->direction = 1;
    door->speed = VDOORSPEED;
    door->topwait = VDOORWAIT;

    switch(xline->special)
    {
    case 1:
    case 26:
    case 27:
    case 28:
        door->type = normal;
        break;

    case 31:
    case 32:
    case 33:
    case 34:
        door->type = open;
        xline->special = 0;
        break;
    }

    // find the top and bottom of the movement range
    door->topheight = P_FindLowestCeilingSurrounding(sec);
    door->topheight -= 4 * FRACUNIT;
}

void P_SpawnDoorCloseIn30(sector_t *sec)
{
    vldoor_t *door;

    door = Z_Malloc(sizeof(*door), PU_LEVSPEC, 0);

    P_AddThinker(&door->thinker);

    P_XSector(sec)->specialdata = door;
    P_XSector(sec)->special = 0;

    door->thinker.function = T_VerticalDoor;
    door->sector = sec;
    door->direction = 0;
    door->type = normal;
    door->speed = VDOORSPEED;
    door->topcountdown = 30 * 35;
}

void P_SpawnDoorRaiseIn5Mins(sector_t *sec, int secnum)
{
    vldoor_t *door;

    door = Z_Malloc(sizeof(*door), PU_LEVSPEC, 0);

    P_AddThinker(&door->thinker);

    P_XSector(sec)->specialdata = door;
    P_XSector(sec)->special = 0;

    door->thinker.function = T_VerticalDoor;
    door->sector = sec;
    door->direction = 2;
    door->type = raiseIn5Mins;
    door->speed = VDOORSPEED;
    door->topheight = P_FindLowestCeilingSurrounding(sec);
    door->topheight -= 4 * FRACUNIT;
    door->topwait = VDOORWAIT;
    door->topcountdown = 5 * 60 * 35;
}
