/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Martin Eyre <martineyre@btinternet.com>
 *\author Copyright © 1999 by Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 *\author Copyright © 1999-2000 by Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze (PrBoom 2.2.6)
 *\author Copyright © 1993-1996 by id Software, Inc.
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

/*
 * Door animation code (opening/closing)
 */

// HEADER FILES ------------------------------------------------------------

#include "wolftc.h"

#include "dmu_lib.h"
#include "p_player.h"
#include "p_mapspec.h"

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
            case blazeRaise:
                door->direction = -1;   // time to go back down
                S_SectorSound(door->sector, SORG_CEILING, sfx_wlfpwl);
                break;

            case normal:
                door->direction = -1;   // time to go back down
                S_SectorSound(door->sector, SORG_CEILING, sfx_wlfdrc);
                break;

            case close30ThenOpen:
                door->direction = 1;
                S_SectorSound(door->sector, SORG_CEILING, sfx_wlfdro);
                break;

            default:
                break;
            }
        }
        break;

    case 2:
        //  INITIAL WAIT
        if(!--door->topcountdown)
        {
            switch (door->type)
            {
            case raiseIn5Mins:
                door->direction = 1;
                door->type = normal;
                S_SectorSound(door->sector, SORG_CEILING, sfx_wlfdro);
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
            case blazeRaise:
            case blazeClose:
                xsec->specialdata = NULL;
                P_RemoveThinker(&door->thinker);    // unlink and free

                // DOOMII BUG:
                // This is what causes blazing doors to produce two closing
                // sounds as one has already been played when the door starts
                // to close (above)
                S_SectorSound(door->sector, SORG_CEILING, sfx_wlfpwl);
                break;

            case normal:
            case close:
                xsec->specialdata = NULL;
                P_RemoveThinker(&door->thinker);    // unlink and free
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
            // DOOMII BUG:
            // The switch bellow SHOULD(?) play the blazing open sound if
            // the door type is blazing and not sfx_doropn.
            switch (door->type)
            {
            case blazeClose:
            case close:     // DO NOT GO BACK UP!
                break;

            default:
                door->direction = 1;
                S_SectorSound(door->sector, SORG_CEILING, sfx_wlfdro);
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
            case blazeRaise:
            case normal:
                door->direction = 0;    // wait at top
                door->topcountdown = door->topwait;
                break;

            case close30ThenOpen:
            case blazeOpen:
            case open:
                xsec->specialdata = NULL;
                P_RemoveThinker(&door->thinker);    // unlink and free
                break;

            default:
                break;
            }
        }
        break;
    }
}

/*
 * Move a locked door up/down
 */
int EV_DoLockedDoor(line_t *line, vldoor_e type, mobj_t *thing)
{
    xline_t *xline = P_XLine(line);
    player_t *p;

    p = thing->player;

    if(!p)
        return 0;

    switch(xline->special)
    {
    case 99:                    // Blue Lock
    case 133:
        if(!p)
            return 0;
        if(!p->keys[it_bluecard] && !p->keys[it_blueskull])
        {
            P_SetMessage(p, PD_BLUEO, false);
            S_StartSound(sfx_dorlck, p->plr->mo);
            return 0;
        }
        break;

    case 134:                   // Red Lock
    case 135:
        if(!p)
            return 0;
        if(!p->keys[it_redcard] && !p->keys[it_redskull])
        {
            P_SetMessage(p, PD_REDO, false);
            S_StartSound(sfx_dorlck, p->plr->mo);
            return 0;
        }
        break;

    case 136:                   // Yellow Lock
    case 137:
        if(!p)
            return 0;
        if(!p->keys[it_yellowcard] && !p->keys[it_yellowskull])
        {
            P_SetMessage(p, PD_YELLOWO, false);
            S_StartSound(sfx_dorlck, p->plr->mo);
            return 0;
        }
        break;
    }

    return EV_DoDoor(line, type);
}

int EV_DoDoor(line_t *line, vldoor_e type)
{
    int         rtn = 0;
    xsector_t  *xsec;
    sector_t   *sec = NULL;
    vldoor_t   *door;
    iterlist_t *list;

    list = P_GetSectorIterListForTag(P_XLine(line)->tag, false);
    if(!list)
        return rtn;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        xsec = P_XSector(sec);

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
        door->speed = VDOORSPEED;

        switch (type)
        {
        case blazeClose:
            door->topheight = P_FindLowestCeilingSurrounding(sec);
            door->topheight -= 4 * FRACUNIT;
            door->direction = -1;
            door->speed = VDOORSPEED * 4;
            S_SectorSound(door->sector, SORG_CEILING, sfx_wlfpwl);
            break;

        case close:
            door->topheight = P_FindLowestCeilingSurrounding(sec);
            door->topheight -= 4 * FRACUNIT;
            door->direction = -1;
            S_SectorSound(door->sector, SORG_CEILING, sfx_wlfdrc);
            break;

        case close30ThenOpen:
            door->topheight =
                P_GetFixedp(sec, DMU_CEILING_HEIGHT);
            door->direction = -1;
            S_SectorSound(door->sector, SORG_CEILING, sfx_wlfdrc);
            break;

        case blazeRaise:
        case blazeOpen:
            door->direction = 1;
            door->topheight = P_FindLowestCeilingSurrounding(sec);
            door->topheight -= 4 * FRACUNIT;
            door->speed = VDOORSPEED * 4;
            if(door->topheight !=
                P_GetFixedp(sec, DMU_CEILING_HEIGHT))
                S_SectorSound(door->sector, SORG_CEILING, sfx_wlfpwl);
            break;

        case normal:
        case open:
            door->direction = 1;
            door->topheight = P_FindLowestCeilingSurrounding(sec);
            door->topheight -= 4 * FRACUNIT;
            if(door->topheight !=
                P_GetFixedp(sec, DMU_CEILING_HEIGHT))
                S_SectorSound(door->sector, SORG_CEILING, sfx_wlfdro);
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

        if(!player->keys[it_bluecard] && !player->keys[it_blueskull])
        {
            P_SetMessage(player, PD_BLUEK, false);
            S_StartSound(sfx_dorlck, player->plr->mo);
            return;
        }
        break;

    case 27:
    case 34:
        // Yellow Lock
        if(!player)
            return;

        if(!player->keys[it_yellowcard] && !player->keys[it_yellowskull])
        {
            P_SetMessage(player, PD_YELLOWK, false);
            S_StartSound(sfx_dorlck, player->plr->mo);
            return;
        }
        break;

    case 28:
    case 33:
        // Red Lock
        if(!player)
            return;

        if(!player->keys[it_redcard] && !player->keys[it_redskull])
        {
            P_SetMessage(player, PD_REDK, false);
            S_StartSound(sfx_dorlck, player->plr->mo);
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
        case 117:
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
    case 117:
    case 118:
        // BLAZING DOOR RAISE/OPEN
        S_SectorSound(sec, SORG_CEILING, sfx_wlfpwl);
        break;

    case 1:
    case 31:
        // NORMAL DOOR SOUND
        S_SectorSound(sec, SORG_CEILING, sfx_wlfdro);
        break;

    default:
        // LOCKED DOOR SOUND
        S_SectorSound(sec, SORG_CEILING, sfx_wlfdro);
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

    case 117:                   // blazing door raise
        door->type = blazeRaise;
        door->speed = VDOORSPEED * 4;
        break;
    case 118:                   // blazing door open
        door->type = blazeOpen;
        xline->special = 0;
        door->speed = VDOORSPEED * 4;
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
