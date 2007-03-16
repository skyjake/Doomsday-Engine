/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Martin Eyre <martineyre@btinternet.com>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
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
 * p_doors.c : Vertical doors (opening/closing).
 */

// HEADER FILES ------------------------------------------------------------

#if  __DOOM64TC__
#  include "doom64tc.h"
#elif __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

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

void T_VerticalDoor(vldoor_t *door)
{
    xsector_t *xsec;
    result_e res;

    xsec = P_XSector(door->sector);

    switch(door->direction)
    {
    case 0:
        // WAITING
        if(!--door->topcountdown)
        {
            switch(door->type)
            {
#if __DOOM64TC__
            case instantRaise:  //d64tc
                door->direction = -1;
                break;
#endif
#if __JDOOM__ || __DOOM64TC__ || __WOLFTC__
            case blazeRaise:
                door->direction = -1;   // time to go back down
# if __WOLFTC__
                S_SectorSound(door->sector, SORG_CEILING, sfx_wlfpwl);
# else
                S_SectorSound(door->sector, SORG_CEILING, sfx_bdcls);
# endif
                break;
#endif

#if __JHEXEN__
            case DREV_NORMAL:
#else
            case normal:
#endif
                door->direction = -1;   // time to go back down
#if __JHEXEN__
                SN_StartSequence(P_SectorSoundOrigin(door->sector),
                                 SEQ_DOOR_STONE + xsec->seqType);
#elif __WOLFTC__
                S_SectorSound(door->sector, SORG_CEILING, sfx_wlfdrc);
#elif __JHERETIC__
                S_SectorSound(door->sector, SORG_CEILING, sfx_doropn);
#else
                S_SectorSound(door->sector, SORG_CEILING, sfx_dorcls);
#endif
                break;

#if __JHEXEN__
            case DREV_CLOSE30THENOPEN:
#else
            case close30ThenOpen:
#endif
                door->direction = 1;
#if __WOLFTC__
                S_SectorSound(door->sector, SORG_CEILING, sfx_wlfdro);
#elif __JDOOM__ || __JHERETIC__ || __DOOM64TC__
                S_SectorSound(door->sector, SORG_CEILING, sfx_doropn);
#endif
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
            switch(door->type)
            {
#if __JHEXEN__
            case DREV_RAISEIN5MINS:
#else
            case raiseIn5Mins:
#endif
                door->direction = 1;
#if __JHEXEN__
                door->type = DREV_NORMAL;
#else
                door->type = normal;
#endif

#if __WOLFTC__
                S_SectorSound(door->sector, SORG_CEILING, sfx_wlfdro);
#elif __JDOOM__ || __JHERETIC__ || __DOOM64TC__
                S_SectorSound(door->sector, SORG_CEILING, sfx_doropn);
#endif
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
                        P_GetFloatp(door->sector, DMU_FLOOR_HEIGHT),
                        false, 1, door->direction);
#if __JHEXEN__
        if(res == RES_PASTDEST)
#else
        if(res == pastdest)
#endif
        {
#if __JHEXEN__
            SN_StopSequence(P_SectorSoundOrigin(door->sector));
#endif
            switch(door->type)
            {
#if __DOOM64TC__
            case instantRaise:  //d64tc
            case instantClose:  //d64tc
                P_XSector(door->sector)->specialdata = NULL;
                P_RemoveThinker(&door->thinker);  // unlink and free
                break;
#endif
#if __JDOOM__ || __DOOM64TC__ || __WOLFTC__
            case blazeRaise:
            case blazeClose:
                xsec->specialdata = NULL;
                P_RemoveThinker(&door->thinker);    // unlink and free

                // DOOMII BUG:
                // This is what causes blazing doors to produce two closing
                // sounds as one has already been played when the door starts
                // to close (above)
# if __WOLFTC__
                S_SectorSound(door->sector, SORG_CEILING, sfx_wlfpwl);
# else
                S_SectorSound(door->sector, SORG_CEILING, sfx_bdcls);
# endif
                break;
#endif
#if __JHEXEN__
            case DREV_NORMAL:
            case DREV_CLOSE:
#else
            case normal:
            case close:
#endif
                xsec->specialdata = NULL;
#if __JHEXEN__
                P_TagFinished(P_XSector(door->sector)->tag);
#endif
                P_RemoveThinker(&door->thinker);    // unlink and free
#if __JHERETIC__
                S_SectorSound(door->sector, SORG_CEILING, sfx_dorcls);
#endif
                break;

#if __JHEXEN__
            case DREV_CLOSE30THENOPEN:
#else
            case close30ThenOpen:
#endif
                door->direction = 0;
                door->topcountdown = 35 * 30;
                break;

            default:
                break;
            }
        }
#if __JHEXEN__
        else if(res == RES_CRUSHED)
#else
        else if(res == crushed)
#endif
        {
            // DOOMII BUG:
            // The switch bellow SHOULD(?) play the blazing open sound if
            // the door type is blazing and not sfx_doropn.
            switch(door->type)
            {
#if __JDOOM__ || __DOOM64TC__ || __WOLFTC__
            case blazeClose:
#endif
#if __JHEXEN__
            case DREV_CLOSE:    // DON'T GO BACK UP!
#else
            case close:     // DO NOT GO BACK UP!
#endif
                break;

            default:
                door->direction = 1;
#if __WOLFTC__
                S_SectorSound(door->sector, SORG_CEILING, sfx_wlfdro);
#elif __JDOOM__ || __JHERETIC__ || __DOOM64TC__
                S_SectorSound(door->sector, SORG_CEILING, sfx_doropn);
#endif
                break;
            }
        }
        break;

    case 1:
        // UP
        res =
            T_MovePlane(door->sector, door->speed, door->topheight, false, 1,
                        door->direction);

#if __JHEXEN__
        if(res == RES_PASTDEST)
#else
        if(res == pastdest)
#endif
        {
#if __JHEXEN__
            SN_StopSequence(P_SectorSoundOrigin(door->sector));
#endif
            switch(door->type)
            {
#if __DOOM64TC__
            case instantRaise:  //d64tc
                door->direction = 0;
                /*
                 * skip topwait and began the countdown
                 * that way there won't be a big delay when the
                 * animation starts -kaiser
                 */
                door->topcountdown = 160;
                break;
#endif
#if __JDOOM__ || __DOOM64TC__ || __WOLFTC__
            case blazeRaise:
#endif
#if __JHEXEN__
            case DREV_NORMAL:
#else
            case normal:
#endif
                door->direction = 0;    // wait at top
                door->topcountdown = door->topwait;
                break;

#if __JDOOM__ || __DOOM64TC__ || __WOLFTC__
            case blazeOpen:
#endif
#if __JHEXEN__
            case DREV_CLOSE30THENOPEN:
            case DREV_OPEN:
#else
            case close30ThenOpen:
            case open:
#endif
                xsec->specialdata = NULL;
#if __JHEXEN__
                P_TagFinished(P_XSector(door->sector)->tag);
#endif
                P_RemoveThinker(&door->thinker);    // unlink and free
#if __JHERETIC__
                S_StopSound(0, (mobj_t *) P_GetPtrp(door->sector,
                                                    DMU_CEILING_SOUND_ORIGIN));
#endif
                break;

            default:
                break;
            }
        }
        break;

    default:
        break;
    }
}

/**
 * Move a locked door up/down
 */
#if __JDOOM__ || __DOOM64TC__ || __WOLFTC__
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
        if(!p->keys[KT_BLUECARD] && !p->keys[KT_BLUESKULL])
        {
            P_SetMessage(p, PD_BLUEO, false);
# if __WOLFTC__
            S_StartSound(sfx_dorlck, p->plr->mo);
# else
            S_StartSound(sfx_oof, p->plr->mo);
#endif
            return 0;
        }
        break;

    case 134:                   // Red Lock
    case 135:
        if(!p)
            return 0;
        if(!p->keys[KT_REDCARD] && !p->keys[KT_REDSKULL])
        {
            P_SetMessage(p, PD_REDO, false);
# if __WOLFTC__
            S_StartSound(sfx_dorlck, p->plr->mo);
# else
            S_StartSound(sfx_oof, p->plr->mo);
# endif
            return 0;
        }
        break;

    case 136:                   // Yellow Lock
    case 137:
        if(!p)
            return 0;
        if(!p->keys[KT_YELLOWCARD] && !p->keys[KT_YELLOWSKULL])
        {
            P_SetMessage(p, PD_YELLOWO, false);
# if __WOLFTC__
            S_StartSound(sfx_dorlck, p->plr->mo);
# else
            S_StartSound(sfx_oof, p->plr->mo);
#endif
            return 0;
        }
        break;

# if __DOOM64TC__
    case 343: // d64tc
        if(!p)
            return 0;
        if(!p->artifacts[it_laserpw1])
        {
            P_SetMessage(p, PD_OPNPOWERUP, false);
            S_StartSound(sfx_oof, p->plr->mo);
            return 0;
        }
        break;

    case 344: // d64tc
        if(!p)
            return 0;
        if(!p->artifacts[it_laserpw2])
        {
            P_SetMessage(p, PD_OPNPOWERUP, false);
            S_StartSound(sfx_oof, p->plr->mo);
            return 0;
        }
        break;

    case 345: // d64tc
        if(!p)
            return 0;
        if(!p->artifacts[it_laserpw3])
        {
            P_SetMessage(p, PD_OPNPOWERUP, false);
            S_StartSound(sfx_oof, p->plr->mo);
            return 0;
        }
        break;
# endif
    default:
        break;
    }

    return EV_DoDoor(line, type);
}
#endif

#if __JHEXEN__
int EV_DoDoor(line_t *line, byte *args, vldoor_e type)
#else
int EV_DoDoor(line_t *line, vldoor_e type)
#endif
{
    int         rtn = 0;
#if __JHEXEN__
    float       speed = FIX2FLT(args[1]) / 8;
#endif
    xsector_t  *xsec;
    sector_t   *sec = NULL;
    vldoor_t   *door;
    iterlist_t *list;

#if __JHEXEN__
    list = P_GetSectorIterListForTag((int) args[0], false);
#else
    list = P_GetSectorIterListForTag(P_XLine(line)->tag, false);
#endif
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
#if __JDOOM__ || __JHERETIC__ || __WOLFTC__ || __DOOM64TC__
        door->type = type;
        door->topwait = VDOORWAIT;
        door->speed = VDOORSPEED;
#endif
        switch(type)
        {
#if __JDOOM__ || __DOOM64TC__ || __WOLFTC__
        case blazeClose:
            door->topheight = P_FindLowestCeilingSurrounding(sec);
            door->topheight -= 4;
            door->direction = -1;
            door->speed = VDOORSPEED * 4;
# if __WOLFTC__
            S_SectorSound(door->sector, SORG_CEILING, sfx_wlfpwl);
# else
            S_SectorSound(door->sector, SORG_CEILING, sfx_bdcls);
# endif
            break;
#endif
#if __JHEXEN__
        case DREV_CLOSE:
#else
        case close:
#endif
            door->topheight = P_FindLowestCeilingSurrounding(sec);
            door->topheight -= 4;
            door->direction = -1;
#if __WOLFTC__
            S_SectorSound(door->sector, SORG_CEILING, sfx_wlfdrc);
#elif __JHERETIC__
            S_SectorSound(door->sector, SORG_CEILING, sfx_doropn);
#elif __JDOOM__ || __DOOM64TC__
            S_SectorSound(door->sector, SORG_CEILING, sfx_dorcls);
#endif
            break;

#if __JHEXEN__
        case DREV_CLOSE30THENOPEN:
#else
        case close30ThenOpen:
#endif
            door->topheight = P_GetFloatp(sec, DMU_CEILING_HEIGHT);
            door->direction = -1;
#if __WOLFTC__
            S_SectorSound(door->sector, SORG_CEILING, sfx_wlfdrc);
#elif __JHERETIC__
            S_SectorSound(door->sector, SORG_CEILING, sfx_doropn);
#elif __JDOOM__ || __DOOM64TC__
            S_SectorSound(door->sector, SORG_CEILING, sfx_dorcls);
#endif
            break;

#if __JDOOM__ || __DOOM64TC__ || __WOLFTC__
        case blazeRaise:
#endif
#if !__JHEXEN__
        case blazeOpen:
            door->direction = 1;
            door->topheight = P_FindLowestCeilingSurrounding(sec);
            door->topheight -= 4;
# if __JHERETIC__
            door->speed = VDOORSPEED * 3;
# else
            door->speed = VDOORSPEED * 4;
# endif
            if(door->topheight != P_GetFloatp(sec, DMU_CEILING_HEIGHT))
            {
# if __WOLFTC__
                S_SectorSound(door->sector, SORG_CEILING, sfx_wlfpwl);
# elif __JHERETIC__
                S_SectorSound(door->sector, SORG_CEILING, sfx_doropn);
# else
                S_SectorSound(door->sector, SORG_CEILING, sfx_bdopn);
# endif
            }
            break;
#endif

#if __JHEXEN__
        case DREV_NORMAL:
        case DREV_OPEN:
#else
        case normal:
        case open:
#endif
            door->direction = 1;
            door->topheight = P_FindLowestCeilingSurrounding(sec);
            door->topheight -= 4;
#if __JDOOM__ || __JHERETIC__ || __DOOM64TC__ || __WOLFTC__
            if(door->topheight != P_GetFloatp(sec, DMU_CEILING_HEIGHT))
            {
# if __WOLFTC__
                S_SectorSound(door->sector, SORG_CEILING, sfx_wlfdro);
# else
                S_SectorSound(door->sector, SORG_CEILING, sfx_doropn);
# endif
            }
#endif
            break;

        default:
            break;
        }

#if __JHEXEN__
        door->type = type;
        door->speed = speed;
        door->topwait = args[2];    // line->arg3
        SN_StartSequence(P_SectorSoundOrigin(door->sector),
                         SEQ_DOOR_STONE + P_XSector(door->sector)->seqType);
#endif
    }
    return rtn;
}

/**
 * open a door manually, no tag value
 */
#if __JHEXEN__
boolean EV_VerticalDoor(line_t *line, mobj_t *thing)
#else
void EV_VerticalDoor(line_t *line, mobj_t *thing)
#endif
{
#if !__JHEXEN__
    player_t *player;
#endif
    xline_t *xline = P_XLine(line);
    xsector_t *xsec;
    sector_t *sec;
    vldoor_t *door;

    sec = P_GetPtrp(line, DMU_BACK_SECTOR);
    if(!sec)
    {
#if __JHEXEN__
        return false;
#else
        return;
#endif
    }
    xsec = P_XSector(sec);

#if !__JHEXEN__
    //  Check for locks
    player = thing->player;

    switch(xline->special)
    {
    case 26:
    case 32:
# if __DOOM64TC__
    case 525: // d64tc
# endif
        // Blue Lock
        if(!player)
            return;

# if __JHERETIC__
        if(!player->keys[KT_BLUE])
        {
            P_SetMessage(player, TXT_NEEDBLUEKEY, false);
            S_ConsoleSound(sfx_plroof, NULL, player - players);
            return;
        }
# else
        if(!player->keys[KT_BLUECARD] && !player->keys[KT_BLUESKULL])
        {
            P_SetMessage(player, PD_BLUEK, false);
#  if __WOLFTC__
            S_StartSound(sfx_dorlck, player->plr->mo);
#  else
            S_StartSound(sfx_oof, player->plr->mo);
#  endif
            return;
        }
# endif
        break;

    case 27:
    case 34:
# if __DOOM64TC__
    case 526: // d64tc
# endif
        // Yellow Lock
        if(!player)
            return;

# if __JHERETIC__
        if(!player->keys[KT_YELLOW])
        {
            P_SetMessage(player, TXT_NEEDYELLOWKEY, false);
            S_ConsoleSound(sfx_plroof, NULL, player - players);
            return;
        }
# else
        if(!player->keys[KT_YELLOWCARD] && !player->keys[KT_YELLOWSKULL])
        {
            P_SetMessage(player, PD_YELLOWK, false);
#  if __WOLFTC__
            S_StartSound(sfx_dorlck, player->plr->mo);
#  else
            S_StartSound(sfx_oof, player->plr->mo);
#  endif
            return;
        }
# endif
        break;

    case 28:
    case 33:
# if __DOOM64TC__
    case 527: // d64tc
# endif
        if(!player)
            return;

# if __JHERETIC__
        // Green lock
        if(!player->keys[KT_GREEN])
        {
            P_SetMessage(player, TXT_NEEDGREENKEY, false);
            S_ConsoleSound(sfx_plroof, NULL, player - players);
            return;
        }
# else
        // Red lock
        if(!player->keys[KT_REDCARD] && !player->keys[KT_REDSKULL])
        {
            P_SetMessage(player, PD_REDK, false);
#  if __WOLFTC__
            S_StartSound(sfx_dorlck, player->plr->mo);
#  else
            S_StartSound(sfx_oof, player->plr->mo);
#  endif
            return;
        }
# endif
        break;

    default:
        break;
    }
#endif

    // if the sector has an active thinker, use it
    if(xsec->specialdata)
    {
#if __JHEXEN__
        return false;
#else
        door = xsec->specialdata;
        switch(xline->special)
        {
        case 1:
        case 26:
        case 27:
        case 28:
# if __JDOOM__ || __DOOM64TC__ || __WOLFTC__
        case 117:
# endif
# if __DOOM64TC__
        case 525: // d64tc
        case 526: // d64tc
        case 527: // d64tc
# endif
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

        default:
            break;
        }
#endif
    }

#if !__JHEXEN__
    // for proper sound
    switch(xline->special)
    {
# if __JDOOM__ || __DOOM64TC__ || __WOLFTC__
    case 117:
    case 118:
#  if __DOOM64TC__
    case 525: // d64tc
    case 526: // d64tc
    case 527: // d64tc
#  endif
        // BLAZING DOOR RAISE/OPEN
#  if __WOLFTC__
        S_SectorSound(sec, SORG_CEILING, sfx_wlfpwl);
#  else
        S_SectorSound(sec, SORG_CEILING, sfx_bdopn);
#  endif
        break;
# endif
    case 1:
    case 31:
        // NORMAL DOOR SOUND
# if __WOLFTC__
        S_SectorSound(sec, SORG_CEILING, sfx_wlfdro);
# else
        S_SectorSound(sec, SORG_CEILING, sfx_doropn);
# endif
        break;

    default:
        // LOCKED DOOR SOUND
# if __WOLFTC__
        S_SectorSound(sec, SORG_CEILING, sfx_wlfdro);
# else
        S_SectorSound(sec, SORG_CEILING, sfx_doropn);
# endif
        break;
    }
# endif

    // new door thinker
    door = Z_Malloc(sizeof(*door), PU_LEVSPEC, 0);
    P_AddThinker(&door->thinker);
    xsec->specialdata = door;
    door->thinker.function = T_VerticalDoor;
    door->sector = sec;
    door->direction = 1;
#if __JHEXEN__
    door->speed = FIX2FLT(P_XLine(line)->arg2 * (FRACUNIT / 8));
    door->topwait = P_XLine(line)->arg3;
#else
    door->speed = VDOORSPEED;
    door->topwait = VDOORWAIT;
#endif
    switch(xline->special)
    {
#if __JHEXEN__
    case 11:
        door->type = DREV_OPEN;
        xline->special = 0;
        break;

    case 12:
    case 13:
        door->type = DREV_NORMAL;
        break;

    default:
        door->type = DREV_NORMAL;
        break;
#else
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
# if __JDOOM__ || __DOOM64TC__ || __WOLFTC__
    case 117:                   // blazing door raise
#  if __DOOM64TC__
    case 525: // d64tc
    case 526: // d64tc
    case 527: // d64tc
#  endif
        door->type = blazeRaise;
        door->speed = VDOORSPEED * 4;
        break;
    case 118:                   // blazing door open
        door->type = blazeOpen;
        xline->special = 0;
        door->speed = VDOORSPEED * 4;
        break;
# endif
    default:
        break;
#endif
    }

    // find the top and bottom of the movement range
    door->topheight = P_FindLowestCeilingSurrounding(sec);
    door->topheight -= 4;
#if __JHEXEN__
    SN_StartSequence(P_SectorSoundOrigin(door->sector),
                     SEQ_DOOR_STONE + P_XSector(door->sector)->seqType);
    return true;
#endif
}

#if __JDOOM__ || __JHERETIC__ || __DOOM64TC__ || __WOLFTC__
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

void P_SpawnDoorRaiseIn5Mins(sector_t *sec)
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
    door->topheight -= 4;
    door->topwait = VDOORWAIT;
    door->topcountdown = 5 * 60 * 35;
}
#endif

/**
 * kaiser - Implemented for doom64tc.
 */
#if __DOOM64TC__
int EV_DoSplitDoor(line_t *line, int ftype, int ctype)
{
    boolean     floor, ceiling;
    sector_t   *sec = NULL;
    iterlist_t *list;

    floor = EV_DoFloor(line, ftype);

    list = P_GetSectorIterListForTag(P_XLine(line)->tag, false);
    if(!list)
        return 0;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        P_XSector(sec)->specialdata = 0;
    }

    ceiling = EV_DoCeiling(line, ctype);
    return (floor || ceiling);
}
#endif
