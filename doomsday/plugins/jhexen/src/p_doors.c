/**\file
 *\section Copyright and License Summary
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2004-2006 Jaakko Keränen <skyjake@dengine.net>
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
 *
 *\attention  2006-09-10 - We should be able to use jDoom version of this as
 * a complete drop in replacement. - Yagisan
 */

#include "jhexen.h"

#include "dmu_lib.h"
#include "p_mapspec.h"

//==================================================================
//==================================================================
//
//                                                      VERTICAL DOORS
//
//==================================================================
//==================================================================

//==================================================================
//
//      T_VerticalDoor
//
//==================================================================
void T_VerticalDoor(vldoor_t * door)
{
    result_e res;

    switch (door->direction)
    {
    case 0:                 // WAITING
        if(!--door->topcountdown)
            switch (door->type)
            {
            case DREV_NORMAL:
                door->direction = -1;   // time to go back down
                SN_StartSequence(P_SectorSoundOrigin(door->sector),
                                 SEQ_DOOR_STONE + P_XSector(door->sector)->seqType);
                break;
            case DREV_CLOSE30THENOPEN:
                door->direction = 1;
                break;
            default:
                break;
            }
        break;
    case 2:                 // INITIAL WAIT
        if(!--door->topcountdown)
        {
            switch (door->type)
            {
            case DREV_RAISEIN5MINS:
                door->direction = 1;
                door->type = DREV_NORMAL;
                break;
            default:
                break;
            }
        }
        break;
    case -1:                    // DOWN
        res =
            T_MovePlane(door->sector, door->speed,
                        P_GetFixedp(door->sector, DMU_FLOOR_HEIGHT),
                        false, 1, door->direction);
        if(res == RES_PASTDEST)
        {
            SN_StopSequence(P_SectorSoundOrigin(door->sector));
            switch (door->type)
            {
            case DREV_NORMAL:
            case DREV_CLOSE:
                P_XSector(door->sector)->specialdata = NULL;
                P_TagFinished(P_XSector(door->sector)->tag);
                P_RemoveThinker(&door->thinker);    // unlink and free
                break;
            case DREV_CLOSE30THENOPEN:
                door->direction = 0;
                door->topcountdown = 35 * 30;
                break;
            default:
                break;
            }
        }
        else if(res == RES_CRUSHED)
        {
            switch (door->type)
            {
            case DREV_CLOSE:    // DON'T GO BACK UP!
                break;
            default:
                door->direction = 1;
                break;
            }
        }
        break;
    case 1:                 // UP
        res =
            T_MovePlane(door->sector, door->speed, door->topheight, false, 1,
                        door->direction);
        if(res == RES_PASTDEST)
        {
            SN_StopSequence(P_SectorSoundOrigin(door->sector));
            switch (door->type)
            {
            case DREV_NORMAL:
                door->direction = 0;    // wait at top
                door->topcountdown = door->topwait;
                break;
            case DREV_CLOSE30THENOPEN:
            case DREV_OPEN:
                P_XSector(door->sector)->specialdata = NULL;
                P_TagFinished(P_XSector(door->sector)->tag);
                P_RemoveThinker(&door->thinker);    // unlink and free
                break;
            default:
                break;
            }
        }
        break;
    }
}

/**
 * Move a door up/down
 */
int EV_DoDoor(line_t *line, byte *args, vldoor_e type)
{
    int         rtn = 0;
    fixed_t     speed;
    sector_t   *sec = NULL;
    vldoor_t   *door;

    speed = args[1] * FRACUNIT / 8;
    while((sec = P_IterateTaggedSectors((int) args[0], sec)) != NULL)
    {
        if(P_XSector(sec)->specialdata)
            continue;

        rtn = 1;
        door = Z_Malloc(sizeof(*door), PU_LEVSPEC, 0);
        P_AddThinker(&door->thinker);
        P_XSector(sec)->specialdata = door;
        door->thinker.function = T_VerticalDoor;
        door->sector = sec;
        switch (type)
        {
        case DREV_CLOSE:
            door->topheight = P_FindLowestCeilingSurrounding(sec);
            door->topheight -= 4 * FRACUNIT;
            door->direction = -1;
            break;

        case DREV_CLOSE30THENOPEN:
            door->topheight = P_GetFixedp(sec, DMU_CEILING_HEIGHT);
            door->direction = -1;
            break;

        case DREV_NORMAL:
        case DREV_OPEN:
            door->direction = 1;
            door->topheight = P_FindLowestCeilingSurrounding(sec);
            door->topheight -= 4 * FRACUNIT;
            break;

        default:
            break;
        }

        door->type = type;
        door->speed = speed;
        door->topwait = args[2];    // line->arg3
        SN_StartSequence(P_SectorSoundOrigin(door->sector),
                         SEQ_DOOR_STONE + P_XSector(door->sector)->seqType);
    }

    return rtn;
}

//==================================================================
//
//      EV_VerticalDoor : open a door manually, no tag value
//
//==================================================================
boolean EV_VerticalDoor(line_t *line, mobj_t *thing)
{
    sector_t *sec;
    vldoor_t *door;
    //int     side;

    //side = 0;                 // only front sides can be used
    // if the sector has an active thinker, use it
    //sec = sides[line->sidenum[side ^ 1]].sector;

    sec = P_GetPtrp( P_GetPtrp(line, DMU_SIDE1), DMU_SECTOR );
    if(P_XSector(sec)->specialdata)
    {
        return false;
    }

    //
    // new door thinker
    //
    door = Z_Malloc(sizeof(*door), PU_LEVSPEC, 0);
    P_AddThinker(&door->thinker);
    P_XSector(sec)->specialdata = door;
    door->thinker.function = T_VerticalDoor;
    door->sector = sec;
    door->direction = 1;
    switch (P_XLine(line)->special)
    {
    case 11:
        door->type = DREV_OPEN;
        P_XLine(line)->special = 0;
        break;
    case 12:
    case 13:
        door->type = DREV_NORMAL;
        break;
    default:
        door->type = DREV_NORMAL;
        break;
    }
    door->speed = P_XLine(line)->arg2 * (FRACUNIT / 8);
    door->topwait = P_XLine(line)->arg3;

    //
    // find the top and bottom of the movement range
    //
    door->topheight = P_FindLowestCeilingSurrounding(sec);
    door->topheight -= 4 * FRACUNIT;
    SN_StartSequence(P_SectorSoundOrigin(door->sector),
                     SEQ_DOOR_STONE + P_XSector(door->sector)->seqType);
    return true;
}

//==================================================================
//
//      Spawn a door that closes after 30 seconds
//
//==================================================================

/*
   void P_SpawnDoorCloseIn30(sector_t *sec)
   {
   vldoor_t *door;

   door = Z_Malloc(sizeof(*door), PU_LEVSPEC, 0);
   P_AddThinker(&door->thinker);
   sec->specialdata = door;
   sec->special = 0;
   door->thinker.function = T_VerticalDoor;
   door->sector = sec;
   door->direction = 0;
   door->type = DREV_NORMAL;
   door->speed = VDOORSPEED;
   door->topcountdown = 30*35;
   }
 */

//==================================================================
//
//      Spawn a door that opens after 5 minutes
//
//==================================================================

/*
   void P_SpawnDoorRaiseIn5Mins(sector_t *sec, int secnum)
   {
   vldoor_t *door;

   door = Z_Malloc(sizeof(*door), PU_LEVSPEC, 0);
   P_AddThinker(&door->thinker);
   sec->specialdata = door;
   sec->special = 0;
   door->thinker.function = T_VerticalDoor;
   door->sector = sec;
   door->direction = 2;
   door->type = DREV_RAISEIN5MINS;
   door->speed = VDOORSPEED;
   door->topheight = P_FindLowestCeilingSurrounding(sec);
   door->topheight -= 4*FRACUNIT;
   door->topwait = VDOORWAIT;
   door->topcountdown = 5*60*35;
   }
 */
