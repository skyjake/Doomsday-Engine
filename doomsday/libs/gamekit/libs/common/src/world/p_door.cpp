/** @file p_door.cpp  Vertical door (opening/closing) thinker.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Martin Eyre <martineyre@btinternet.com>
 * @authors Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
 * @authors Copyright © 1999 Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 * @authors Copyright © 1999-2000 Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze (PrBoom 2.2.6)
 * @authors Copyright © 1993-1996 id Software, Inc.
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "common.h"
#include "p_door.h"

#include "dmu_lib.h"
#include "player.h"
#include "p_mapspec.h"
#if __JDOOM64__
#  include "p_ceiling.h"
#  include "p_floor.h"
#  include "p_inventory.h"
#endif
#include "p_sound.h"
#include <cstring>

// Sounds played by the doors when changing state.
// jHexen uses sound sequences, so it's are defined as 'SFX_NONE'.
#if __JDOOM64__
# define SFX_DOORCLOSING       (SFX_DORCLS)
# define SFX_DOORCLOSED        (SFX_DORCLS)
# define SFX_DOORBLAZECLOSE    (SFX_BDCLS)
# define SFX_DOOROPEN          (SFX_DOROPN)
# define SFX_DOORBLAZEOPEN     (SFX_BDOPN)
# define SFX_DOORLOCKED        (SFX_OOF)
#elif __JDOOM__
# define SFX_DOORCLOSING       (SFX_DORCLS)
# define SFX_DOORCLOSED        (SFX_DORCLS)
# define SFX_DOORBLAZECLOSE    (SFX_BDCLS)
# define SFX_DOOROPEN          (SFX_DOROPN)
# define SFX_DOORBLAZEOPEN     (SFX_BDOPN)
# define SFX_DOORLOCKED        (SFX_OOF)
#elif __JHERETIC__
# define SFX_DOORCLOSING       (SFX_DOROPN)
# define SFX_DOORCLOSED        (SFX_DORCLS)
# define SFX_DOORBLAZECLOSE    (SFX_NONE)
# define SFX_DOOROPEN          (SFX_DOROPN)
# define SFX_DOORBLAZEOPEN     (SFX_DOROPN)
# define SFX_DOORLOCKED        (SFX_PLROOF)
#elif __JHEXEN__
# define SFX_DOORCLOSING       (SFX_NONE)
# define SFX_DOORCLOSED        (SFX_NONE)
# define SFX_DOORBLAZECLOSE    (SFX_NONE)
# define SFX_DOOROPEN          (SFX_NONE)
# define SFX_DOORBLAZEOPEN     (SFX_NONE)
# define SFX_DOORLOCKED        (SFX_NONE)
#endif

void T_Door(void *doorThinkerPtr)
{
    door_t *door = (door_t *)doorThinkerPtr;
    DE_ASSERT(door != 0);

    xsector_t *xsec = P_ToXSector(door->sector);

    result_e res;

    switch(door->state)
    {
    case DS_WAIT:
        if(!--door->topCountDown)
        {
            switch(door->type)
            {
#if __JDOOM64__
            case DT_INSTANTRAISE:  //jd64
                door->state = DS_DOWN;
                break;
#endif
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
# if __JHERETIC__
            case DT_BLAZEOPEN:
# else
            case DT_BLAZERAISE:
# endif
                door->state = DS_DOWN; // Time to go back down.
                S_PlaneSound((Plane *)P_GetPtrp(door->sector, DMU_CEILING_PLANE), SFX_DOORBLAZECLOSE);
                break;
#endif
            case DT_NORMAL:
                door->state = DS_DOWN; // Time to go back down.
#if __JHEXEN__
                SN_StartSequence((mobj_t *)P_GetPtrp(door->sector, DMU_EMITTER),
                                 SEQ_DOOR_STONE + xsec->seqType);
#else
                S_PlaneSound((Plane *)P_GetPtrp(door->sector, DMU_CEILING_PLANE), SFX_DOORCLOSING);
#endif
                break;

            case DT_CLOSE30THENOPEN:
                door->state = DS_UP;
#if !__JHEXEN__
                S_PlaneSound((Plane *)P_GetPtrp(door->sector, DMU_CEILING_PLANE), SFX_DOOROPEN);
#endif
                break;

            default:
                break;
            }
        }
        break;

    case DS_INITIALWAIT:
        if(!--door->topCountDown)
        {
            switch(door->type)
            {
            case DT_RAISEIN5MINS:
                door->state = DS_UP;
                door->type = DT_NORMAL;
#if !__JHEXEN__
                S_PlaneSound((Plane *)P_GetPtrp(door->sector, DMU_CEILING_PLANE), SFX_DOOROPEN);
#endif
                break;

            default:
                break;
            }
        }
        break;

    case DS_DOWN:
        res =
            T_MovePlane(door->sector, door->speed,
                        P_GetDoublep(door->sector, DMU_FLOOR_HEIGHT),
                        false, 1, -1);

        if(res == pastdest)
        {
#if __JHEXEN__
            SN_StopSequence((mobj_t *)P_GetPtrp(door->sector, DMU_EMITTER));
#endif
            switch(door->type)
            {
#if __JDOOM64__
            case DT_INSTANTRAISE:  //jd64
            case DT_INSTANTCLOSE:  //jd64
                P_ToXSector(door->sector)->specialData = 0;
                Thinker_Remove(&door->thinker); // Unlink and free.
                break;
#endif
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
# if __JHERETIC__
            case DT_BLAZEOPEN:
# else
            case DT_BLAZERAISE:
            case DT_BLAZECLOSE:
# endif
                xsec->specialData = 0;
                Thinker_Remove(&door->thinker); // Unlink and free.

                // DOOMII BUG:
                // This is what causes blazing doors to produce two closing
                // sounds as one has already been played when the door starts
                // to close (above)
                S_PlaneSound((Plane *)P_GetPtrp(door->sector, DMU_CEILING_PLANE), SFX_DOORBLAZECLOSE);
                break;
#endif
            case DT_NORMAL:
            case DT_CLOSE:
                xsec->specialData = nullptr;
                P_NotifySectorFinished(P_ToXSector(door->sector)->tag);
                Thinker_Remove(&door->thinker); // Unlink and free.
#if __JHERETIC__
                S_PlaneSound((Plane *)P_GetPtrp(door->sector, DMU_CEILING_PLANE), SFX_DOORCLOSED);
#endif
                break;

            case DT_CLOSE30THENOPEN:
                door->state = DS_WAIT;
                door->topCountDown = 30 * TICSPERSEC;
                break;

            default:
                break;
            }
        }
        else if(res == crushed)
        {
            // DOOMII BUG:
            // The switch below SHOULD(?) play the blazing open sound if
            // the door type is blazing and not SFX_DOROPN.
            switch(door->type)
            {
#if __JDOOM__ || __JDOOM64__
            case DT_BLAZECLOSE:
#endif
            case DT_CLOSE: // Do not go back up!
                break;

            default:
                door->state = DS_UP;
#if !__JHEXEN__
                S_PlaneSound((Plane *)P_GetPtrp(door->sector, DMU_CEILING_PLANE), SFX_DOOROPEN);
#endif
                break;
            }
        }
        break;

    case DS_UP:
        res =
            T_MovePlane(door->sector, door->speed, door->topHeight, false, 1,
                        1);

        if(res == pastdest)
        {
#if __JHEXEN__
            SN_StopSequence((mobj_t *)P_GetPtrp(door->sector, DMU_EMITTER));
#endif
            switch(door->type)
            {
#if __JDOOM64__
            case DT_INSTANTRAISE:  //jd64
                door->state = DS_WAIT;
                /*
                 * skip topwait and began the countdown
                 * that way there won't be a big delay when the
                 * animation starts -kaiser
                 */
                door->topCountDown = 160;
                break;
#endif
#if __JDOOM__ || __JDOOM64__
            case DT_BLAZERAISE:
#endif
#if __JHERETIC__
            case DT_BLAZEOPEN:
#endif
            case DT_NORMAL:
                door->state        = DS_WAIT; // Wait at top.
                door->topCountDown = door->topWait;
                break;

#if __JDOOM__ || __JDOOM64__
            case DT_BLAZEOPEN:
#endif
            case DT_CLOSE30THENOPEN:
            case DT_OPEN:
                xsec->specialData = nullptr;
                P_NotifySectorFinished(P_ToXSector(door->sector)->tag);
                Thinker_Remove(&door->thinker); // Unlink and free.
#if __JHERETIC__
                S_StopSound(0, (mobj_t *) P_GetPtrp(door->sector, DMU_CEILING_EMITTER));
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

void door_s::write(MapStateWriter *msw) const
{
    Writer1 *writer = msw->writer();

    Writer_WriteByte(writer, 1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    Writer_WriteByte(writer, (byte) type);

    Writer_WriteInt32(writer, P_ToIndex(sector));

    Writer_WriteInt16(writer, (int)topHeight);
    Writer_WriteInt32(writer, FLT2FIX(speed));

    Writer_WriteInt32(writer, state);
    Writer_WriteInt32(writer, topWait);
    Writer_WriteInt32(writer, topCountDown);
}

int door_s::read(MapStateReader *msr)
{
    Reader1 *reader = msr->reader();
    int mapVersion = msr->mapVersion();

#if __JHEXEN__
    if(mapVersion >= 4)
#else
    if(mapVersion >= 5)
#endif
    {   // Note: the thinker class byte has already been read.
        /*int ver =*/ Reader_ReadByte(reader); // version byte.

        type          = doortype_e(Reader_ReadByte(reader));
        sector        = (Sector *)P_ToPtr(DMU_SECTOR, Reader_ReadInt32(reader));
        DE_ASSERT(sector != 0);

        topHeight     = (float) Reader_ReadInt16(reader);
        speed         = FIX2FLT((fixed_t) Reader_ReadInt32(reader));

        state         = doorstate_e(Reader_ReadInt32(reader));
        topWait       = Reader_ReadInt32(reader);
        topCountDown  = Reader_ReadInt32(reader);
    }
    else
    {
        // Its in the old format which serialized door_t
        // Padding at the start (an old thinker_t struct)
        byte junk[16]; // sizeof thinker_t
        Reader_Read(reader, junk, 16);

        // Start of used data members.
#if __JHEXEN__
        // A 32bit pointer to sector, serialized.
        sector        = (Sector *)P_ToPtr(DMU_SECTOR, Reader_ReadInt32(reader));
        type          = doortype_e(Reader_ReadInt32(reader));
#else
        type          = doortype_e(Reader_ReadInt32(reader));

        // A 32bit pointer to sector, serialized.
        sector        = (Sector *)P_ToPtr(DMU_SECTOR, Reader_ReadInt32(reader));
#endif
        topHeight     = FIX2FLT((fixed_t) Reader_ReadInt32(reader));
        speed         = FIX2FLT((fixed_t) Reader_ReadInt32(reader));

        state         = doorstate_e(Reader_ReadInt32(reader));
        topWait       = Reader_ReadInt32(reader);
        topCountDown  = Reader_ReadInt32(reader);
    }

    P_ToXSector(sector)->specialData = this;
    thinker.function = T_Door;

    return true; // Add this thinker.
}

static int EV_DoDoor2(int tag, float speed, int topwait, doortype_e type)
{
    iterlist_t *list = P_GetSectorIterListForTag(tag, false);
    if(!list) return 0;

    int rtn = 0;

    IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
    IterList_RewindIterator(list);

    Sector *sec;
    while((sec = (Sector *) IterList_MoveIterator(list)))
    {
        xsector_t *xsec = P_ToXSector(sec);

        if(xsec->specialData)
            continue;

        // new door thinker
        rtn = 1;
        door_t *door = (door_t *)Z_Calloc(sizeof(*door), PU_MAP, 0);
        door->thinker.function = T_Door;
        Thinker_Add(&door->thinker);
        xsec->specialData = door;

        door->sector  = sec;
        door->type    = type;
        door->topWait = topwait;
        door->speed   = speed;

#if __JHEXEN__
        int sound = SEQ_DOOR_STONE + P_ToXSector(door->sector)->seqType;
#else
        int sound = 0;
#endif

        switch(type)
        {
#if __JDOOM__ || __JDOOM64__
        case DT_BLAZECLOSE:
            P_FindSectorSurroundingLowestCeiling(sec, (coord_t) MAXINT, &door->topHeight);
            door->topHeight -= 4;
            door->state = DS_DOWN;
            door->speed *= 4;
            sound = SFX_DOORBLAZECLOSE;
            break;
#endif
        case DT_CLOSE:
            P_FindSectorSurroundingLowestCeiling(sec, (coord_t) MAXINT, &door->topHeight);
            door->topHeight -= 4;
            door->state = DS_DOWN;
#if !__JHEXEN__
            sound = SFX_DOORCLOSING;
#endif
            break;

        case DT_CLOSE30THENOPEN:
            door->topHeight = P_GetDoublep(sec, DMU_CEILING_HEIGHT);
            door->state = DS_DOWN;
#if !__JHEXEN__
            sound = SFX_DOORCLOSING;
#endif
            break;

#if __JDOOM__ || __JDOOM64__
        case DT_BLAZERAISE:
#endif
#if !__JHEXEN__
        case DT_BLAZEOPEN:
            door->state = DS_UP;
            P_FindSectorSurroundingLowestCeiling(sec, (coord_t) MAXINT, &door->topHeight);
            door->topHeight -= 4;
# if __JHERETIC__
            door->speed *= 3;
# else
            door->speed *= 4;
# endif
            if(!FEQUAL(door->topHeight, P_GetDoublep(sec, DMU_CEILING_HEIGHT)))
                sound = SFX_DOORBLAZEOPEN;
            break;
#endif

        case DT_NORMAL:
        case DT_OPEN:
            door->state = DS_UP;
            P_FindSectorSurroundingLowestCeiling(sec, (coord_t) MAXINT, &door->topHeight);
            door->topHeight -= 4;

#if !__JHEXEN__
            if(!FEQUAL(door->topHeight, P_GetDoublep(sec, DMU_CEILING_HEIGHT)))
                sound = SFX_DOOROPEN;
#endif
            break;

        default:
            break;
        }

        // Play a sound?
#if __JHEXEN__
        SN_StartSequence((mobj_t *)P_GetPtrp(door->sector, DMU_EMITTER), sound);
#else
        if(sound)
            S_PlaneSound((Plane *)P_GetPtrp(door->sector, DMU_CEILING_PLANE), sound);
#endif
    }
    return rtn;
}

#if __JHEXEN__
int EV_DoDoor(Line * /*line*/, byte *args, doortype_e type)
{
    return EV_DoDoor2((int) args[0], (float) args[1] * (1.0 / 8),
                      (int) args[2], type);
}
#else
int EV_DoDoor(Line *line, doortype_e type)
{
    return EV_DoDoor2(P_ToXLine(line)->tag, DOORSPEED, DOORWAIT, type);
}
#endif

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
static void sendNeedKeyMessage(player_t *p, textenum_t msgTxt, int keyNum)
{
    char buf[160], tmp[2];
    const char *in;

    buf[0] = 0;
    tmp[1] = 0;

    // Get the message template.
    in = GET_TXT(msgTxt);

    for(; *in; in++)
    {
        if(in[0] == '%')
        {
            if(in[1] == '1')
            {
                strcat(buf, GET_TXT(TXT_KEY1 + keyNum));
                in++;
                continue;
            }

            if(in[1] == '%')
                in++;
        }
        tmp[0] = *in;
        strcat(buf, tmp);
    }

    P_SetMessage(p, buf);
}
#endif

#if __JDOOM__ || __JDOOM64__
/**
 * Checks whether the given line is a locked door.
 * If locked and the player IS ABLE to open it, return @c true.
 * If locked and the player IS NOT ABLE to open it, send an appropriate
 * message and play a sound before returning @c false.
 * Else, NOT a locked door and can be opened, return @c true.
 */
static dd_bool tryLockedDoor(Line *line, player_t *p)
{
    xline_t *xline = P_ToXLine(line);

    if(!p || !xline)
        return false;

    switch(xline->special)
    {
    case 99:                    // Blue Lock
    case 133:
        if(!p->keys[KT_BLUECARD] && !p->keys[KT_BLUESKULL])
        {
            sendNeedKeyMessage(p, TXT_PD_BLUEO, 0);
            S_StartSound(SFX_DOORLOCKED, p->plr->mo);
            return false;
        }
        break;

    case 134:                   // Red Lock
    case 135:
        if(!p->keys[KT_REDCARD] && !p->keys[KT_REDSKULL])
        {
            sendNeedKeyMessage(p, TXT_PD_REDO, 2);
            S_StartSound(SFX_DOORLOCKED, p->plr->mo);
            return false;
        }
        break;

    case 136:                   // Yellow Lock
    case 137:
        if(!p->keys[KT_YELLOWCARD] && !p->keys[KT_YELLOWSKULL])
        {
            sendNeedKeyMessage(p, TXT_PD_YELLOWO, 1);
            S_StartSound(SFX_DOORLOCKED, p->plr->mo);
            return false;
        }
        break;

# if __JDOOM64__
    case 343:
        if(!P_InventoryCount(p - players, IIT_DEMONKEY1))
        {
            P_SetMessage(p, PD_OPNPOWERUP);
            S_StartSound(SFX_DOORLOCKED, p->plr->mo);
            return false;
        }
        break;

    case 344:
        if(!P_InventoryCount(p - players, IIT_DEMONKEY2))
        {
            P_SetMessage(p, PD_OPNPOWERUP);
            S_StartSound(SFX_DOORLOCKED, p->plr->mo);
            return false;
        }
        break;

    case 345:
        if(!P_InventoryCount(p - players, IIT_DEMONKEY3))
        {
            P_SetMessage(p, PD_OPNPOWERUP);
            S_StartSound(SFX_DOORLOCKED, p->plr->mo);
            return false;
        }
        break;
# endif
    default:
        break;
    }

    return true;
}
#endif

/**
 * Checks whether the given line is a locked manual door.
 * If locked and the player IS ABLE to open it, return @c true.
 * If locked and the player IS NOT ABLE to open it, send an appropriate
 * message and play a sound before returning @c false.
 * Else, NOT a locked door and can be opened, return @c true.
 */
static dd_bool tryLockedManualDoor(Line *line, mobj_t *mo)
{
    xline_t *xline = P_ToXLine(line);
#if !__JHEXEN__
    int keyNum = -1;
    textenum_t msgTxt = NUMTEXT;
    sfxenum_t sfxNum = SFX_NONE;
#endif

    if(!mo || !xline)
        return false;

#if !__JHEXEN__
    player_t *p = mo->player;

    switch(xline->special)
    {
    case 26:
    case 32:
# if __JDOOM64__
    case 525: // jd64
# endif
        if(!p)
            return false;

        // Blue Lock
# if __JHERETIC__
        if(!p->keys[KT_BLUE])
        {
            msgTxt = TXT_TXT_NEEDBLUEKEY;
            keyNum = 2;
            sfxNum = SFX_DOORLOCKED;
        }
# else
        if(!p->keys[KT_BLUECARD] && !p->keys[KT_BLUESKULL])
        {
            msgTxt = TXT_PD_BLUEK;
            keyNum = 0;
            sfxNum = SFX_DOORLOCKED;
        }
# endif
        break;

    case 27:
    case 34:
# if __JDOOM64__
    case 526: // jd64
# endif
        if(!p)
            return false;

        // Yellow Lock
# if __JHERETIC__
        if(!p->keys[KT_YELLOW])
        {
            msgTxt = TXT_TXT_NEEDYELLOWKEY;
            keyNum = 0;
            sfxNum = SFX_DOORLOCKED;
        }
# else
        if(!p->keys[KT_YELLOWCARD] && !p->keys[KT_YELLOWSKULL])
        {
            msgTxt = TXT_PD_YELLOWK;
            keyNum = 1;
            sfxNum = SFX_DOORLOCKED;
        }
# endif
        break;

    case 28:
    case 33:
# if __JDOOM64__
    case 527: // jd64
# endif
        if(!p)
            return false;

# if __JHERETIC__
        // Green lock
        if(!p->keys[KT_GREEN])
        {
            msgTxt = TXT_TXT_NEEDGREENKEY;
            keyNum = 1;
            sfxNum = SFX_DOORLOCKED;
        }
# else
        // Red lock
        if(!p->keys[KT_REDCARD] && !p->keys[KT_REDSKULL])
        {
            msgTxt = TXT_PD_REDK;
            keyNum = 2;
            sfxNum = SFX_DOORLOCKED;
        }
# endif
        break;

    default:
        break;
    }

    if(keyNum != -1)
    {   // A key is required.
        sendNeedKeyMessage(p, msgTxt, keyNum);
        S_StartSound(sfxNum, p->plr->mo);
        return false;
    }
#endif

    return true;
}

/**
 * Move a locked door up/down.
 */
#if __JDOOM__ || __JDOOM64__
int EV_DoLockedDoor(Line *line, doortype_e type, mobj_t *thing)
{
    if(!tryLockedDoor(line, thing->player))
        return 0;

    return EV_DoDoor(line, type);
}
#endif

/**
 * Open a door manually, no tag value.
 */
dd_bool EV_VerticalDoor(Line *line, mobj_t *mo)
{
    Sector *sec = (Sector *)P_GetPtrp(line, DMU_BACK_SECTOR);
    if(!sec) return false;

    if(!tryLockedManualDoor(line, mo))
        return false; // Mobj cannot open this door.

    xsector_t *xsec = P_ToXSector(sec);
    xline_t *xline  = P_ToXLine(line);

    // If the sector has an active thinker, use it.
    if(xsec->specialData)
    {
#if __JHEXEN__
        return false;
#else
        door_t *door = (door_t *)xsec->specialData;
        switch(xline->special)
        {
        case 1:
        case 26:
        case 27:
        case 28:
# if __JDOOM__ || __JDOOM64__
        case 117:
# endif
# if __JDOOM64__
        case 525: // jd64
        case 526: // jd64
        case 527: // jd64
# endif
            // Only for "raise" doors, not "open"s.
            if(door->state == DS_DOWN)
                door->state = DS_UP; // Go back up.
            else
            {
                if(!mo->player)
                    return false; // Bad guys never close doors.

                door->state = DS_DOWN; // Start going down immediately.
            }
            return false;

        default:
            break;
        }
#endif
    }

    // New door thinker.
    door_t *door = (door_t *) Z_Calloc(sizeof(*door), PU_MAP, 0);
    door->thinker.function = T_Door;
    Thinker_Add(&door->thinker);

    xsec->specialData = door;
    door->sector      = sec;
    door->state       = DS_UP;

    // Play a sound?
#if __JHEXEN__
    SN_StartSequence((mobj_t *)P_GetPtrp(door->sector, DMU_EMITTER),
                     SEQ_DOOR_STONE + P_ToXSector(door->sector)->seqType);
#else
    switch(xline->special)
    {
# if __JDOOM__ || __JDOOM64__
    case 117:
    case 118:
#  if __JDOOM64__
    case 525: // jd64
    case 526: // jd64
    case 527: // jd64
#  endif
        // BLAZING DOOR RAISE/OPEN
        S_PlaneSound((Plane *)P_GetPtrp(door->sector, DMU_CEILING_PLANE), SFX_DOORBLAZEOPEN);
        break;
# endif

    case 1:
    case 31:
        // NORMAL DOOR SOUND
        S_PlaneSound((Plane *)P_GetPtrp(door->sector, DMU_CEILING_PLANE), SFX_DOOROPEN);
        break;

    default:
        // LOCKED DOOR SOUND
        S_PlaneSound((Plane *)P_GetPtrp(door->sector, DMU_CEILING_PLANE), SFX_DOOROPEN);
        break;
    }
#endif

    switch(xline->special)
    {
#if __JHEXEN__
    case 11:
        door->type = DT_OPEN;
        door->speed = (float) xline->arg2 * (1.0 / 8);
        door->topWait = (int) xline->arg3;
        xline->special = 0;
        break;
#else
    case 31:
    case 32:
    case 33:
    case 34:
        door->type = DT_OPEN;
        door->speed = DOORSPEED;
        door->topWait = DOORWAIT;
        xline->special = 0;
        break;
#endif

#if __JHEXEN__
    case 12:
    case 13:
        door->type = DT_NORMAL;
        door->speed = (float) xline->arg2 * (1.0 / 8);
        door->topWait = (int) xline->arg3;
        break;
#else
    case 1:
    case 26:
    case 27:
    case 28:
        door->type = DT_NORMAL;
        door->speed = DOORSPEED;
        door->topWait = DOORWAIT;
        break;
#endif

#if __JDOOM__ || __JDOOM64__
    case 117:                   // blazing door raise
# if __JDOOM64__
    case 525: // jd64
    case 526: // jd64
    case 527: // jd64
# endif
        door->type = DT_BLAZERAISE;
        door->speed = DOORSPEED * 4;
        door->topWait = DOORWAIT;
        break;

    case 118:                   // blazing door open
        door->type = DT_BLAZEOPEN;
        door->speed = DOORSPEED * 4;
        door->topWait = DOORWAIT;
        xline->special = 0;
        break;
#endif

    default:
#if __JHEXEN__
        door->type = DT_NORMAL;
        door->speed = (float) xline->arg2 * (1.0 / 8);
        door->topWait = (int) xline->arg3;
#else
        door->speed = DOORSPEED;
        door->topWait = DOORWAIT;
#endif
        break;
    }

    // find the top and bottom of the movement range
    P_FindSectorSurroundingLowestCeiling(sec, (coord_t) MAXINT, &door->topHeight);
    door->topHeight -= 4;
    return true;
}

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
void P_SpawnDoorCloseIn30(Sector *sec)
{
    door_t *door = (door_t *)Z_Calloc(sizeof(*door), PU_MAP, 0);
    door->thinker.function = T_Door;
    Thinker_Add(&door->thinker);

    P_ToXSector(sec)->specialData = door;
    P_ToXSector(sec)->special = 0;

    door->sector       = sec;
    door->state        = DS_WAIT;
    door->type         = DT_NORMAL;
    door->speed        = DOORSPEED;
    door->topCountDown = 30 * TICSPERSEC;
    door->topHeight    = P_GetDoublep(sec, DMU_CEILING_HEIGHT);
}

void P_SpawnDoorRaiseIn5Mins(Sector *sec)
{
    door_t *door = (door_t *)Z_Calloc(sizeof(*door), PU_MAP, 0);
    door->thinker.function = T_Door;
    Thinker_Add(&door->thinker);

    P_ToXSector(sec)->specialData = door;
    P_ToXSector(sec)->special = 0;

    door->sector       = sec;
    door->state        = DS_INITIALWAIT;
    door->type         = DT_RAISEIN5MINS;
    door->speed        = DOORSPEED;
    P_FindSectorSurroundingLowestCeiling(sec, (coord_t) MAXINT, &door->topHeight);
    door->topHeight   -= 4;
    door->topWait      = DOORWAIT;
    door->topCountDown = 5 * 60 * TICSPERSEC;
}
#endif
