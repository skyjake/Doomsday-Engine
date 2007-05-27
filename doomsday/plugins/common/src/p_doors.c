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

// Sounds played by the doors when changing state.
// jHexen uses sound sequences, so it's are defined as 'sfx_None'.
#if __DOOM64TC__
# define SFX_DOORCLOSE         (sfx_dorcls)
# define SFX_DOORBLAZECLOSE    (sfx_bdcls)
# define SFX_DOOROPEN          (sfx_doropn)
# define SFX_DOORBLAZEOPEN     (sfx_bdopn)
# define SFX_DOORLOCKED        (sfx_oof)
#elif __WOLFTC__
# define SFX_DOORCLOSE         (sfx_wlfdrc)
# define SFX_DOORBLAZECLOSE    (sfx_wlfpwl)
# define SFX_DOOROPEN          (sfx_wlfdro)
# define SFX_DOORBLAZEOPEN     (sfx_wlfpwl)
# define SFX_DOORLOCKED        (sfx_dorlck)
#elif __JDOOM__
# define SFX_DOORCLOSE         (sfx_dorcls)
# define SFX_DOORBLAZECLOSE    (sfx_bdcls)
# define SFX_DOOROPEN          (sfx_doropn)
# define SFX_DOORBLAZEOPEN     (sfx_bdopn)
# define SFX_DOORLOCKED        (sfx_oof)
#elif __JHERETIC__
# define SFX_DOORCLOSE         (sfx_doropn)
# define SFX_DOORBLAZECLOSE    (sfx_None)
# define SFX_DOOROPEN          (sfx_doropn)
# define SFX_DOORBLAZEOPEN     (sfx_doropn)
# define SFX_DOORLOCKED        (sfx_plroof)
#elif __JHEXEN__
# define SFX_DOORCLOSE         (SFX_NONE)
# define SFX_DOORBLAZECLOSE    (SFX_NONE)
# define SFX_DOOROPEN          (SFX_NONE)
# define SFX_DOORBLAZEOPEN     (SFX_NONE)
# define SFX_DOORLOCKED        (SFX_NONE)
#endif

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
                S_SectorSound(door->sector, SORG_CEILING, SFX_DOORBLAZECLOSE);
                break;
#endif
            case normal:
                door->direction = -1;   // time to go back down
#if __JHEXEN__
                SN_StartSequence(P_SectorSoundOrigin(door->sector),
                                 SEQ_DOOR_STONE + xsec->seqType);
#else
                S_SectorSound(door->sector, SORG_CEILING, SFX_DOORCLOSE);
#endif
                break;

            case close30ThenOpen:
                door->direction = 1;
#if !__JHEXEN__
                S_SectorSound(door->sector, SORG_CEILING, SFX_DOOROPEN);
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
            case raiseIn5Mins:
                door->direction = 1;
                door->type = normal;
#if !__JHEXEN__
                S_SectorSound(door->sector, SORG_CEILING, SFX_DOOROPEN);
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

        if(res == pastdest)
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
                S_SectorSound(door->sector, SORG_CEILING, SFX_DOORBLAZECLOSE);
                break;
#endif
            case normal:
            case close:
                xsec->specialdata = NULL;
#if __JHEXEN__
                P_TagFinished(P_XSector(door->sector)->tag);
#endif
                P_RemoveThinker(&door->thinker);    // unlink and free
#if __JHERETIC__
                S_SectorSound(door->sector, SORG_CEILING, SFX_DOORCLOSE);
#endif
                break;

            case close30ThenOpen:
                door->direction = 0;
                door->topcountdown = 30 * TICSPERSEC;
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
            switch(door->type)
            {
#if __JDOOM__ || __DOOM64TC__ || __WOLFTC__
            case blazeClose:
#endif
            case close:     // Do not go back up!
                break;

            default:
                door->direction = 1;
#if !__JHEXEN__
                S_SectorSound(door->sector, SORG_CEILING, SFX_DOOROPEN);
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

        if(res == pastdest)
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

            case normal:
                door->direction = 0;    // wait at top
                door->topcountdown = door->topwait;
                break;

#if __JDOOM__ || __DOOM64TC__ || __WOLFTC__
            case blazeOpen:
#endif
            case close30ThenOpen:
            case open:
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

static int EV_DoDoor2(int tag, float speed, int topwait, vldoor_e type)
{
    int         rtn = 0, sound;
    xsector_t  *xsec;
    sector_t   *sec = NULL;
    vldoor_t   *door;
    iterlist_t *list;

    list = P_GetSectorIterListForTag(tag, false);
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
        door->topwait = topwait;
        door->speed = speed;

#if __JHEXEN__
        sound = SEQ_DOOR_STONE + P_XSector(door->sector)->seqType;
#else
        sound = 0;
#endif

        switch(type)
        {
#if __JDOOM__ || __DOOM64TC__ || __WOLFTC__
        case blazeClose:
            door->topheight = P_FindLowestCeilingSurrounding(sec);
            door->topheight -= 4;
            door->direction = -1;
            door->speed *= 4;
            sound = SFX_DOORBLAZECLOSE;
            break;
#endif
        case close:
            door->topheight = P_FindLowestCeilingSurrounding(sec);
            door->topheight -= 4;
            door->direction = -1;
#if !__JHEXEN__
            sound = SFX_DOORCLOSE;
#endif
            break;

        case close30ThenOpen:
            door->topheight = P_GetFloatp(sec, DMU_CEILING_HEIGHT);
            door->direction = -1;
#if !__JHEXEN__
            sound = SFX_DOORCLOSE;
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
            door->speed *= 3;
# else
            door->speed *= 4;
# endif
            if(door->topheight != P_GetFloatp(sec, DMU_CEILING_HEIGHT))
                sound = SFX_DOORBLAZEOPEN;
            break;
#endif

        case normal:
        case open:
            door->direction = 1;
            door->topheight = P_FindLowestCeilingSurrounding(sec);
            door->topheight -= 4;

#if !__JHEXEN__
            if(door->topheight != P_GetFloatp(sec, DMU_CEILING_HEIGHT))
                sound = SFX_DOOROPEN;
#endif
            break;

        default:
            break;
        }

        // Play a sound?
#if __JHEXEN__
        SN_StartSequence(P_SectorSoundOrigin(door->sector), sound);
#else
        if(sound)
            S_SectorSound(door->sector, SORG_CEILING, sound);
#endif
    }
    return rtn;
}

#if __JHEXEN__
int EV_DoDoor(line_t *line, byte *args, vldoor_e type)
{
    return EV_DoDoor2((int) args[0], (float) args[1] * (1.0 / 8),
                      (int) args[2], type);
}
#else
int EV_DoDoor(line_t *line, vldoor_e type)
{
    return EV_DoDoor2(P_XLine(line)->tag, VDOORSPEED, VDOORWAIT, type);
}
#endif

/**
 * Checks whether the given linedef is a locked door.
 * If locked and the player IS ABLE to open it, return <code>true</code>.
 * If locked and the player IS NOT ABLE to open it, send an appropriate
 * message and play a sound before returning <code>false</code>.
 * Else, NOT a locked door and can be opened, return <code>true</code> 
 */
static boolean tryLockedDoor(line_t *line, player_t *p)
{
    xline_t *xline = P_XLine(line);

    if(!p || !xline)
        return false;

#if __JDOOM__ || __DOOM64TC__ || __WOLFTC__
    switch(xline->special)
    {
    case 99:                    // Blue Lock
    case 133:
        if(!p->keys[KT_BLUECARD] && !p->keys[KT_BLUESKULL])
        {
            P_SetMessage(p, PD_BLUEO, false);
            S_StartSound(SFX_DOORLOCKED, p->plr->mo);
            return false;
        }
        break;

    case 134:                   // Red Lock
    case 135:
        if(!p->keys[KT_REDCARD] && !p->keys[KT_REDSKULL])
        {
            P_SetMessage(p, PD_REDO, false);
            S_StartSound(SFX_DOORLOCKED, p->plr->mo);
            return false;
        }
        break;

    case 136:                   // Yellow Lock
    case 137:
        if(!p->keys[KT_YELLOWCARD] && !p->keys[KT_YELLOWSKULL])
        {
            P_SetMessage(p, PD_YELLOWO, false);
            S_StartSound(SFX_DOORLOCKED, p->plr->mo);
            return false;
        }
        break;

# if __DOOM64TC__
    case 343: // d64tc
        if(!p->artifacts[it_laserpw1])
        {
            P_SetMessage(p, PD_OPNPOWERUP, false);
            S_StartSound(SFX_DOORLOCKED, p->plr->mo);
            return false;
        }
        break;

    case 344: // d64tc
        if(!p->artifacts[it_laserpw2])
        {
            P_SetMessage(p, PD_OPNPOWERUP, false);
            S_StartSound(SFX_DOORLOCKED, p->plr->mo);
            return false;
        }
        break;

    case 345: // d64tc
        if(!p->artifacts[it_laserpw3])
        {
            P_SetMessage(p, PD_OPNPOWERUP, false);
            S_StartSound(SFX_DOORLOCKED, p->plr->mo);
            return false;
        }
        break;
# endif
    default:
        break;
    }
#endif

    return true;
}

/**
 * Checks whether the given linedef is a locked manual door.
 * If locked and the player IS ABLE to open it, return <code>true</code>.
 * If locked and the player IS NOT ABLE to open it, send an appropriate
 * message and play a sound before returning <code>false</code>.
 * Else, NOT a locked door and can be opened, return <code>true</code> 
 */
static boolean tryLockedManualDoor(line_t *line, player_t *p)
{
    xline_t *xline = P_XLine(line);

    if(!p || !xline)
        return false;

#if !__JHEXEN__
    switch(xline->special)
    {
    case 26:
    case 32:
# if __DOOM64TC__
    case 525: // d64tc
# endif
        // Blue Lock
# if __JHERETIC__
        if(!p->keys[KT_BLUE])
        {
            P_SetMessage(p, TXT_NEEDBLUEKEY, false);
            S_ConsoleSound(SFX_DOORLOCKED, NULL, p - players);
            return false;
        }
# else
        if(!p->keys[KT_BLUECARD] && !p->keys[KT_BLUESKULL])
        {
            P_SetMessage(p, PD_BLUEK, false);
            S_StartSound(SFX_DOORLOCKED, p->plr->mo);
            return false;
        }
# endif
        break;

    case 27:
    case 34:
# if __DOOM64TC__
    case 526: // d64tc
# endif
        // Yellow Lock
# if __JHERETIC__
        if(!p->keys[KT_YELLOW])
        {
            P_SetMessage(p, TXT_NEEDYELLOWKEY, false);
            S_ConsoleSound(SFX_DOORLOCKED, NULL, p - players);
            return false;
        }
# else
        if(!p->keys[KT_YELLOWCARD] && !p->keys[KT_YELLOWSKULL])
        {
            P_SetMessage(p, PD_YELLOWK, false);
            S_StartSound(SFX_DOORLOCKED, p->plr->mo);
            return false;
        }
# endif
        break;

    case 28:
    case 33:
# if __DOOM64TC__
    case 527: // d64tc
# endif
# if __JHERETIC__
        // Green lock
        if(!p->keys[KT_GREEN])
        {
            P_SetMessage(p, TXT_NEEDGREENKEY, false);
            S_ConsoleSound(SFX_DOORLOCKED, NULL, p - players);
            return false;
        }
# else
        // Red lock
        if(!p->keys[KT_REDCARD] && !p->keys[KT_REDSKULL])
        {
            P_SetMessage(p, PD_REDK, false);
            S_StartSound(SFX_DOORLOCKED, p->plr->mo);
            return false;
        }
# endif
        break;

    default:
        break;
    }
#endif

    return true;
}

/**
 * Move a locked door up/down
 */
#if __JDOOM__ || __DOOM64TC__ || __WOLFTC__
int EV_DoLockedDoor(line_t *line, vldoor_e type, mobj_t *thing)
{
    if(!tryLockedDoor(line, thing->player))
        return 0;

    return EV_DoDoor(line, type);
}
#endif

/**
 * Open a door manually, no tag value.
 */
boolean EV_VerticalDoor(line_t *line, mobj_t *thing)
{
    xline_t *xline = P_XLine(line);
    xsector_t *xsec;
    sector_t *sec;
    vldoor_t *door;

    sec = P_GetPtrp(line, DMU_BACK_SECTOR);
    if(!sec)
        return false;

    xsec = P_XSector(sec);

    if(!tryLockedManualDoor(line, thing->player))
        return false;

    // If the sector has an active thinker, use it.
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
            // Only for "raise" doors, not "open"s
            if(door->direction == -1)
                door->direction = 1; // go back up
            else
            {
                if(!thing->player)
                    return false; // JDC: bad guys never close doors

                door->direction = -1; // start going down immediately
            }
            return false;

        default:
            break;
        }
#endif
    }

    // New door thinker.
    door = Z_Malloc(sizeof(*door), PU_LEVSPEC, 0);
    P_AddThinker(&door->thinker);
    xsec->specialdata = door;
    door->thinker.function = T_VerticalDoor;
    door->sector = sec;
    door->direction = 1;

    // Play a sound?
#if __JHEXEN__
    SN_StartSequence(P_SectorSoundOrigin(door->sector),
                     SEQ_DOOR_STONE + P_XSector(door->sector)->seqType);
#else
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
        S_SectorSound(door->sector, SORG_CEILING, SFX_DOORBLAZEOPEN);
        break;
# endif

    case 1:
    case 31:
        // NORMAL DOOR SOUND
        S_SectorSound(door->sector, SORG_CEILING, SFX_DOOROPEN);
        break;

    default:
        // LOCKED DOOR SOUND
        S_SectorSound(door->sector, SORG_CEILING, SFX_DOOROPEN);
        break;
    }
#endif

    switch(xline->special)
    {
#if __JHEXEN__
    case 11:
        door->type = open;
        door->speed = (float) xline->arg2 * (1.0 / 8);
        door->topwait = (int) xline->arg3;
        xline->special = 0;
        break;
#else
    case 31:
    case 32:
    case 33:
    case 34:
        door->type = open;
        door->speed = VDOORSPEED;
        door->topwait = VDOORWAIT;
        xline->special = 0;
        break;
#endif

#if __JHEXEN__
    case 12:
    case 13:
        door->type = normal;
        door->speed = (float) xline->arg2 * (1.0 / 8);
        door->topwait = (int) xline->arg3;
        break;
#else
    case 1:
    case 26:
    case 27:
    case 28:
        door->type = normal;
        door->speed = VDOORSPEED;
        door->topwait = VDOORWAIT;
        break;
#endif

#if __JDOOM__ || __DOOM64TC__ || __WOLFTC__
    case 117:                   // blazing door raise
# if __DOOM64TC__
    case 525: // d64tc
    case 526: // d64tc
    case 527: // d64tc
# endif
        door->type = blazeRaise;
        door->speed = VDOORSPEED * 4;
        door->topwait = VDOORWAIT;
        break;

    case 118:                   // blazing door open
        door->type = blazeOpen;
        door->speed = VDOORSPEED * 4;
        door->topwait = VDOORWAIT;
        xline->special = 0;
        break;
#endif

    default:
#if __JHEXEN__
        door->type = normal;
        door->speed = (float) xline->arg2 * (1.0 / 8);
        door->topwait = (int) xline->arg3;
#else
        door->speed = VDOORSPEED;
        door->topwait = VDOORWAIT;
#endif
        break;
    }

    // find the top and bottom of the movement range
    door->topheight = P_FindLowestCeilingSurrounding(sec);
    door->topheight -= 4;
    return true;
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
    door->topcountdown = 30 * TICSPERSEC;
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
    door->topcountdown = 5 * 60 * TICSPERSEC;
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
