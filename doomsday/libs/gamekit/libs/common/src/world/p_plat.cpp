/** @file p_plat.cpp  Common playsim routines relating to moving platforms.
 *
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
#include "p_plat.h"

#include "dmu_lib.h"
#include "p_mapspec.h"
#include "p_tick.h"
#include "p_sound.h"

// Sounds played by the platforms when changing state or moving.
// jHexen uses sound sequences, so it's are defined as 'SFX_NONE'.
#if __JDOOM__
# define SFX_PLATFORMSTART      (SFX_PSTART)
# define SFX_PLATFORMMOVE       (SFX_STNMOV)
# define SFX_PLATFORMSTOP       (SFX_PSTOP)
#elif __JDOOM64__
# define SFX_PLATFORMSTART      (SFX_PSTART)
# define SFX_PLATFORMMOVE       (SFX_STNMOV)
# define SFX_PLATFORMSTOP       (SFX_PSTOP)
#elif __JHERETIC__
# define SFX_PLATFORMSTART      (SFX_PSTART)
# define SFX_PLATFORMMOVE       (SFX_STNMOV)
# define SFX_PLATFORMSTOP       (SFX_PSTOP)
#elif __JHEXEN__
# define SFX_PLATFORMSTART      (SFX_NONE)
# define SFX_PLATFORMMOVE       (SFX_NONE)
# define SFX_PLATFORMSTOP       (SFX_NONE)
#endif

/**
 * Called when a moving plat needs to be removed.
 *
 * @param plat  Ptr to the plat to remove.
 */
static void stopPlat(plat_t *plat)
{
    DE_ASSERT(plat);
    P_ToXSector(plat->sector)->specialData = nullptr;
    P_NotifySectorFinished(P_ToXSector(plat->sector)->tag);
    Thinker_Remove(&plat->thinker);
}

void T_PlatRaise(void *platThinkerPtr)
{
    plat_t *plat = (plat_t *)platThinkerPtr;
    result_e res;

    switch(plat->state)
    {
    case PS_UP:
        res = T_MovePlane(plat->sector, plat->speed, plat->high,
                          plat->crush, 0, 1);

        // Play a "while-moving" sound?
#if __JHERETIC__
        if(!(mapTime & 31))
            S_PlaneSound((Plane *)P_GetPtrp(plat->sector, DMU_FLOOR_PLANE), SFX_PLATFORMMOVE);
#endif
#if __JDOOM__ || __JDOOM64__
        if(plat->type == PT_RAISEANDCHANGE ||
           plat->type == PT_RAISETONEARESTANDCHANGE)
        {
            if(!(mapTime & 7))
                S_PlaneSound((Plane *)P_GetPtrp(plat->sector, DMU_FLOOR_PLANE), SFX_PLATFORMMOVE);
        }
#endif
        if(res == crushed && (!plat->crush))
        {
            plat->count = plat->wait;
            plat->state = PS_DOWN;
#if __JHEXEN__
            SN_StartSequenceInSec(plat->sector, SEQ_PLATFORM);
#else
# if __JDOOM64__
            if(plat->type != PT_DOWNWAITUPDOOR) // jd64 added test
# endif
                S_PlaneSound((Plane *)P_GetPtrp(plat->sector, DMU_FLOOR_PLANE), SFX_PLATFORMSTART);
#endif
        }
        else
        {
            if(res == pastdest)
            {
                plat->count = plat->wait;
                plat->state = PS_WAIT;
#if __JHEXEN__
                SN_StopSequenceInSec(plat->sector);
#else
                S_PlaneSound((Plane *)P_GetPtrp(plat->sector, DMU_FLOOR_PLANE), SFX_PLATFORMSTOP);
#endif
                switch(plat->type)
                {
                case PT_DOWNWAITUPSTAY:
#if __JHEXEN__
                case PT_DOWNBYVALUEWAITUPSTAY:
#else
# if !__JHERETIC__
                case PT_DOWNWAITUPSTAYBLAZE:
                case PT_RAISETONEARESTANDCHANGE:
# endif
# if __JDOOM64__
                case PT_DOWNWAITUPPLUS16STAYBLAZE: // jd64
                case PT_DOWNWAITUPDOOR: // jd64
# endif
                case PT_RAISEANDCHANGE:
#endif
                    stopPlat(plat);
                    break;

                default:
                    break;
                }
            }
        }
        break;

    case PS_DOWN:
        res =
            T_MovePlane(plat->sector, plat->speed, plat->low, false, 0, -1);

        if(res == pastdest)
        {
            plat->count = plat->wait;
            plat->state = PS_WAIT;

#if __JHEXEN__ || __JDOOM64__
            switch(plat->type)
            {
# if __JHEXEN__
            case PT_UPBYVALUEWAITDOWNSTAY:
# endif
            case PT_UPWAITDOWNSTAY:
                stopPlat(plat);
                break;

            default:
                break;
            }
#endif

#if __JHEXEN__
            SN_StopSequenceInSec(plat->sector);
#else
            S_PlaneSound((Plane *)P_GetPtrp(plat->sector, DMU_FLOOR_PLANE), SFX_PLATFORMSTOP);
#endif
        }
        else
        {
            // Play a "while-moving" sound?
#if __JHERETIC__
            if(!(mapTime & 31))
                S_PlaneSound((Plane *)P_GetPtrp(plat->sector, DMU_FLOOR_PLANE), SFX_PLATFORMMOVE);
#endif
        }
        break;

    case PS_WAIT:
        if(!--plat->count)
        {
            if(FEQUAL(P_GetDoublep(plat->sector, DMU_FLOOR_HEIGHT), plat->low))
                plat->state = PS_UP;
            else
                plat->state = PS_DOWN;
#if __JHEXEN__
            SN_StartSequenceInSec(plat->sector, SEQ_PLATFORM);
#else
            S_PlaneSound((Plane *)P_GetPtrp(plat->sector, DMU_FLOOR_PLANE), SFX_PLATFORMSTART);
#endif
        }
        break;

    default:
        break;
    }
}

void plat_t::write(MapStateWriter *msw) const
{
    Writer1 *writer = msw->writer();

    Writer_WriteByte(writer, 1); // Write a version byte.

    Writer_WriteByte(writer, (byte) type);

    Writer_WriteInt32(writer, P_ToIndex(sector));

    Writer_WriteInt32(writer, FLT2FIX(speed));
    Writer_WriteInt16(writer, (int)low);
    Writer_WriteInt16(writer, (int)high);

    Writer_WriteInt32(writer, wait);
    Writer_WriteInt32(writer, count);

    Writer_WriteByte(writer, (byte) state);
    Writer_WriteByte(writer, (byte) oldState);
    Writer_WriteByte(writer, (byte) crush);

    Writer_WriteInt32(writer, tag);
}

int plat_t::read(MapStateReader *msr)
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

        thinker.function = T_PlatRaise;

#if !__JHEXEN__
        // Should we put this into stasis?
        if(mapVersion == 5)
        {
            if(!Reader_ReadByte(reader))
                Thinker_SetStasis(&thinker, true);
        }
#endif

        type      = plattype_e(Reader_ReadByte(reader));
        sector    = (Sector *)P_ToPtr(DMU_SECTOR, Reader_ReadInt32(reader));
        DE_ASSERT(sector != 0);
        speed     = FIX2FLT(Reader_ReadInt32(reader));
        low       = (float) Reader_ReadInt16(reader);
        high      = (float) Reader_ReadInt16(reader);
        wait      = Reader_ReadInt32(reader);
        count     = Reader_ReadInt32(reader);
        state     = platstate_e(Reader_ReadByte(reader));
        oldState  = platstate_e(Reader_ReadByte(reader));
        crush     = (dd_bool) Reader_ReadByte(reader);
        tag       = Reader_ReadInt32(reader);
    }
    else
    {
        // Its in the old format which serialized plat_t
        // Padding at the start (an old thinker_t struct)
        int32_t junk[4]; // sizeof thinker_t
        Reader_Read(reader, (byte *)junk, 16);

        // Start of used data members.
        // A 32bit pointer to sector, serialized.
        sector    = (Sector *)P_ToPtr(DMU_SECTOR, (int) Reader_ReadInt32(reader));
        DE_ASSERT(sector != 0);

        speed     = FIX2FLT((fixed_t) Reader_ReadInt32(reader));
        low       = FIX2FLT((fixed_t) Reader_ReadInt32(reader));
        high      = FIX2FLT((fixed_t) Reader_ReadInt32(reader));

        wait      = Reader_ReadInt32(reader);
        count     = Reader_ReadInt32(reader);
        state     = platstate_e(Reader_ReadInt32(reader));
        oldState  = platstate_e(Reader_ReadInt32(reader));
        crush     = Reader_ReadInt32(reader);
        tag       = Reader_ReadInt32(reader);
        type      = plattype_e(Reader_ReadInt32(reader));

        thinker.function = T_PlatRaise;
#if !__JHEXEN__
        if(junk[2] == 0)
        {
            Thinker_SetStasis(&thinker, true);
        }
#endif
    }

    P_ToXSector(sector)->specialData = this;

    return true; // Add this thinker.
}

#if __JHEXEN__
static int doPlat(Line * /*line*/, int tag, byte *args, plattype_e type, int /*amount*/)
#else
static int doPlat(Line *line, int tag, plattype_e type, int amount)
#endif
{
#if !__JHEXEN__
    Sector *frontSector = (Sector *)P_GetPtrp(line, DMU_FRONT_SECTOR);
#endif

    iterlist_t *list = P_GetSectorIterListForTag(tag, false);
    if(!list) return 0;

    int rtn = 0;

    IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
    IterList_RewindIterator(list);

    Sector *sec;
    while((sec = (Sector *)IterList_MoveIterator(list)))
    {
        xsector_t *xsec = P_ToXSector(sec);

        if(xsec->specialData)
            continue;

        // Find lowest & highest floors around sector
        rtn = 1;

        plat_t *plat = (plat_t *) Z_Calloc(sizeof(*plat), PU_MAP, 0);
        plat->thinker.function = T_PlatRaise;
        Thinker_Add(&plat->thinker);

        xsec->specialData = plat;

        plat->type   = type;
        plat->sector = sec;
        plat->crush  = false;
        plat->tag    = tag;
#if __JHEXEN__
        plat->speed  = (float) args[1] * (1.0 / 8);
#endif

        coord_t floorHeight = P_GetDoublep(sec, DMU_FLOOR_HEIGHT);

        switch(type)
        {
#if !__JHEXEN__
        case PT_RAISETONEARESTANDCHANGE:
            plat->speed = PLATSPEED * .5;

            P_SetPtrp(sec, DMU_FLOOR_MATERIAL,
                      P_GetPtrp(frontSector, DMU_FLOOR_MATERIAL));

            {
            coord_t nextFloor;
            if(P_FindSectorSurroundingNextHighestFloor(sec, floorHeight, &nextFloor))
                plat->high = nextFloor;
            else
                plat->high = floorHeight;
            }

            plat->wait = 0;
            plat->state = PS_UP;
            // No more damage if applicable.
            xsec->special = 0;
            S_PlaneSound((Plane *)P_GetPtrp(sec, DMU_FLOOR_PLANE), SFX_PLATFORMMOVE);
            break;

        case PT_RAISEANDCHANGE:
            plat->speed = PLATSPEED * .5;

            P_SetPtrp(sec, DMU_FLOOR_MATERIAL,
                      P_GetPtrp(frontSector, DMU_FLOOR_MATERIAL));

            plat->high = floorHeight + amount;
            plat->wait = 0;
            plat->state = PS_UP;
            S_PlaneSound((Plane *)P_GetPtrp(sec, DMU_FLOOR_PLANE), SFX_PLATFORMMOVE);
            break;
#endif
        case PT_DOWNWAITUPSTAY:
            P_FindSectorSurroundingLowestFloor(sec,
                P_GetDoublep(sec, DMU_FLOOR_HEIGHT), &plat->low);
#if __JHEXEN__
            plat->low += 8;
#else
            plat->speed = PLATSPEED * 4;
#endif
            if(plat->low > floorHeight)
                plat->low = floorHeight;

            plat->high = floorHeight;
            plat->state = PS_DOWN;
#if __JHEXEN__
            plat->wait = (int) args[2];
#else
            plat->wait = PLATWAIT * TICSPERSEC;
#endif
#if !__JHEXEN__
            S_PlaneSound((Plane *)P_GetPtrp(sec, DMU_FLOOR_PLANE), SFX_PLATFORMSTART);
#endif
            break;

#if __JDOOM64__ || __JHEXEN__
        case PT_UPWAITDOWNSTAY:
            P_FindSectorSurroundingHighestFloor(sec, -500, &plat->high);

            if(plat->high < floorHeight)
                plat->high = floorHeight;

            plat->low = floorHeight;
            plat->state = PS_UP;
# if __JHEXEN__
            plat->wait = (int) args[2];
# else
            plat->wait = PLATWAIT * TICSPERSEC;
# endif
# if __JDOOM64__
            plat->speed = PLATSPEED * 8;
            S_PlaneSound((Plane *)P_GetPtrp(sec, DMU_FLOOR_PLANE), SFX_PLATFORMSTART);
# endif
            break;
#endif
#if __JDOOM64__
        case PT_DOWNWAITUPDOOR: // jd64
            plat->speed = PLATSPEED * 8;
            P_FindSectorSurroundingLowestFloor(sec,
                P_GetDoublep(sec, DMU_FLOOR_HEIGHT), &plat->low);

            if(plat->low > floorHeight)
                plat->low = floorHeight;
            if(plat->low != floorHeight)
                plat->low += 6;

            plat->high = floorHeight;
            plat->wait = 50 * PLATWAIT;
            plat->state = PS_DOWN;
            break;
#endif
#if __JHEXEN__
       case PT_DOWNBYVALUEWAITUPSTAY:
            plat->low = floorHeight - (coord_t) args[3] * 8;
            if(plat->low > floorHeight)
                plat->low = floorHeight;
            plat->high = floorHeight;
            plat->wait = (int) args[2];
            plat->state = PS_DOWN;
            break;

        case PT_UPBYVALUEWAITDOWNSTAY:
            plat->high = floorHeight + (coord_t) args[3] * 8;
            if(plat->high < floorHeight)
                plat->high = floorHeight;
            plat->low = floorHeight;
            plat->wait = (int) args[2];
            plat->state = PS_UP;
            break;
#endif
#if __JDOOM__ || __JDOOM64__
        case PT_DOWNWAITUPSTAYBLAZE:
            plat->speed = PLATSPEED * 8;
            P_FindSectorSurroundingLowestFloor(sec,
                P_GetDoublep(sec, DMU_FLOOR_HEIGHT), &plat->low);

            if(plat->low > floorHeight)
                plat->low = floorHeight;

            plat->high = floorHeight;
            plat->wait = PLATWAIT * TICSPERSEC;
            plat->state = PS_DOWN;
            S_PlaneSound((Plane *)P_GetPtrp(sec, DMU_FLOOR_PLANE), SFX_PLATFORMSTART);
            break;
#endif
        case PT_PERPETUALRAISE:
            P_FindSectorSurroundingLowestFloor(sec,
                P_GetDoublep(sec, DMU_FLOOR_HEIGHT), &plat->low);
#if __JHEXEN__
            plat->low += 8;
#else
            plat->speed = PLATSPEED;
#endif
            if(plat->low > floorHeight)
                plat->low = floorHeight;

            P_FindSectorSurroundingHighestFloor(sec, -500, &plat->high);

            if(plat->high < floorHeight)
                plat->high = floorHeight;

            plat->state = platstate_e(P_Random() & 1);
#if __JHEXEN__
            plat->wait = (int) args[2];
#else
            plat->wait = PLATWAIT * TICSPERSEC;
#endif
#if !__JHEXEN__
            S_PlaneSound((Plane *)P_GetPtrp(sec, DMU_FLOOR_PLANE), SFX_PLATFORMSTART);
#endif
            break;

        default:
            break;
        }

#if __JHEXEN__
        SN_StartSequenceInSec(plat->sector, SEQ_PLATFORM);
#endif
    }

    return rtn;
}

#if __JHEXEN__
int EV_DoPlat(Line *line, byte *args, plattype_e type, int amount)
#else
int EV_DoPlat(Line *line, plattype_e type, int amount)
#endif
{
#if __JHEXEN__
    return doPlat(line, (int) args[0], args, type, amount);
#else
    int rtn = 0;
    xline_t *xline = P_ToXLine(line);

    // Activate all <type> plats that are in stasis.
    switch(type)
    {
    case PT_PERPETUALRAISE:
        rtn = P_PlatActivate(xline->tag);
        break;

    default:
        break;
    }

    return doPlat(line, xline->tag, type, amount) || rtn;
#endif
}

#if !__JHEXEN__
struct activateplatparams_t
{
    short tag;
    int count;
};

static int activatePlat(thinker_t *th, void *context)
{
    plat_t *plat = (plat_t *) th;
    activateplatparams_t *params = (activateplatparams_t *) context;

    if(plat->tag == (int) params->tag && Thinker_InStasis(&plat->thinker))
    {
        plat->state = plat->oldState;
        Thinker_SetStasis(&plat->thinker, false);
        params->count++;
    }

    return false; // Contiue iteration.
}

int P_PlatActivate(short tag)
{
    activateplatparams_t parm;

    parm.tag   = tag;
    parm.count = 0;
    Thinker_Iterate(T_PlatRaise, activatePlat, &parm);

    return parm.count;
}
#endif

struct deactivateplatparams_t
{
    short tag;
    int count;
};

static int deactivatePlat(thinker_t *th, void *context)
{
    plat_t *plat = (plat_t *) th;
    deactivateplatparams_t *params = (deactivateplatparams_t *) context;

#if __JHEXEN__
    // For THE one with the tag.
    if(plat->tag == params->tag)
    {
        // Destroy it.
        stopPlat(plat);
        params->count++;
        return true; // Stop iteration.
    }
#else
    // For one with the tag and not in stasis.
    if(plat->tag == (int) params->tag && !Thinker_InStasis(&plat->thinker))
    {
        // Put it in stasis.
        plat->oldState = plat->state;
        Thinker_SetStasis(&plat->thinker, true);
        params->count++;
    }
#endif

    return false; // Continue iteration.
}

int P_PlatDeactivate(short tag)
{
    deactivateplatparams_t parm;

    parm.tag   = tag;
    parm.count = 0;
    Thinker_Iterate(T_PlatRaise, deactivatePlat, &parm);

    return parm.count;
}
