/** @file p_ceiling.cpp  Moving ceilings (lowering, crushing, raising).
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
#include "p_ceiling.h"

#include "dmu_lib.h"
#include "p_mapspec.h"
#include "p_sound.h"
#include "p_start.h"
#include "p_tick.h"

// Sounds played by the ceilings when changing state or moving.
// jHexen uses sound sequences, so it's are defined as 'SFX_NONE'.
#if __JDOOM__
# define SFX_CEILINGMOVE        (SFX_STNMOV)
# define SFX_CEILINGSTOP        (SFX_PSTOP)
#elif __JDOOM64__
# define SFX_CEILINGMOVE        (SFX_STNMOV)
# define SFX_CEILINGSTOP        (SFX_PSTOP)
#elif __JHERETIC__
# define SFX_CEILINGMOVE        (SFX_DORMOV)
# define SFX_CEILINGSTOP        (SFX_NONE)
#elif __JHEXEN__
# define SFX_CEILINGMOVE        (SFX_NONE)
# define SFX_CEILINGSTOP        (SFX_NONE)
#endif

/**
 * Called when a moving ceiling needs to be removed.
 *
 * @param ceiling       Ptr to the ceiling to be stopped.
 */
static void stopCeiling(ceiling_t *ceiling)
{
    P_ToXSector(ceiling->sector)->specialData = nullptr;
    P_NotifySectorFinished(P_ToXSector(ceiling->sector)->tag);
    Thinker_Remove(&ceiling->thinker);
}

void T_MoveCeiling(void *ceilingThinkerPtr)
{
    ceiling_t *ceiling = (ceiling_t *)ceilingThinkerPtr;
    result_e res;

    switch(ceiling->state)
    {
    case CS_UP: // Going up.
        res =
            T_MovePlane(ceiling->sector, ceiling->speed, ceiling->topHeight,
                        false, 1, 1);

        // Play a "while-moving" sound?
#if !__JHEXEN__
        if(!(mapTime & 7))
        {
# if __JHERETIC__
            S_PlaneSound((Plane *)P_GetPtrp(ceiling->sector, DMU_CEILING_PLANE), SFX_CEILINGMOVE);
# else
            switch(ceiling->type)
            {
            case CT_SILENTCRUSHANDRAISE:
                break;
            default:
                S_PlaneSound((Plane *)P_GetPtrp(ceiling->sector, DMU_CEILING_PLANE), SFX_CEILINGMOVE);
                break;
            }
# endif
        }
#endif

        if(res == pastdest)
        {
#if __JHEXEN__
            SN_StopSequence((mobj_t *)P_GetPtrp(ceiling->sector, DMU_EMITTER));
#endif
            switch(ceiling->type)
            {
#if !__JHEXEN__
            case CT_RAISETOHIGHEST:
# if __JDOOM64__
            case CT_CUSTOM: //jd64
# endif
                stopCeiling(ceiling);
                break;
# if !__JHERETIC__
            case CT_SILENTCRUSHANDRAISE:
                S_PlaneSound((Plane *)P_GetPtrp(ceiling->sector, DMU_CEILING_PLANE), SFX_CEILINGSTOP);
# endif
            case CT_CRUSHANDRAISEFAST:
#endif
            case CT_CRUSHANDRAISE:
                ceiling->state = CS_DOWN;
#if __JHEXEN__
                ceiling->speed *= 2;
#endif
                break;

            default:
#if __JHEXEN__
                stopCeiling(ceiling);
#endif
                break;
            }

        }
        break;

    case CS_DOWN: // Going down.
        res =
            T_MovePlane(ceiling->sector, ceiling->speed, ceiling->bottomHeight,
                        ceiling->crush, 1, -1);

        // Play a "while-moving" sound?
#if !__JHEXEN__
        if(!(mapTime & 7))
        {
# if __JHERETIC__
            S_PlaneSound((Plane *)P_GetPtrp(ceiling->sector, DMU_CEILING_PLANE), SFX_CEILINGMOVE);
# else
            switch(ceiling->type)
            {
            case CT_SILENTCRUSHANDRAISE:
                break;
            default:
                S_PlaneSound((Plane *)P_GetPtrp(ceiling->sector, DMU_CEILING_PLANE), SFX_CEILINGMOVE);
            }
# endif
        }
#endif

        if(res == pastdest)
        {
#if __JHEXEN__
            SN_StopSequence((mobj_t *)P_GetPtrp(ceiling->sector, DMU_EMITTER));
#endif
            switch(ceiling->type)
            {
#if __JDOOM__ || __JDOOM64__
            case CT_SILENTCRUSHANDRAISE:
                S_PlaneSound((Plane *)P_GetPtrp(ceiling->sector, DMU_CEILING_PLANE), SFX_CEILINGSTOP);
                ceiling->speed = CEILSPEED;
                ceiling->state = CS_UP;
                break;
#endif

            case CT_CRUSHANDRAISE:
#if __JHEXEN__
            case CT_CRUSHRAISEANDSTAY:
#endif
#if __JHEXEN__
                ceiling->speed = ceiling->speed * .5;
#else
                ceiling->speed = CEILSPEED;
#endif
                ceiling->state = CS_UP;
                break;
#if !__JHEXEN__
            case CT_CRUSHANDRAISEFAST:
                ceiling->state = CS_UP;
                break;

            case CT_LOWERANDCRUSH:
            case CT_LOWERTOFLOOR:
# if __JDOOM64__
            case CT_CUSTOM: //jd64
# endif
                stopCeiling(ceiling);
                break;
#endif

            default:
#if __JHEXEN__
                stopCeiling(ceiling);
#endif
                break;
            }
        }
        else                    // ( res != pastdest )
        {
            if(res == crushed)
            {
                switch(ceiling->type)
                {
#if __JDOOM__ || __JDOOM64__
                case CT_SILENTCRUSHANDRAISE:
#endif
                case CT_CRUSHANDRAISE:
                case CT_LOWERANDCRUSH:
#if __JHEXEN__
                case CT_CRUSHRAISEANDSTAY:
#endif
#if !__JHEXEN__
                    ceiling->speed = CEILSPEED * .125;
#endif
                    break;

                default:
                    break;
                }
            }
        }
        break;
    }
}

void ceiling_s::write(MapStateWriter *msw) const
{
    Writer1 *writer = msw->writer();

    Writer_WriteByte(writer, 2); // Write a version byte.

    Writer_WriteByte(writer, (byte) type);
    Writer_WriteInt32(writer, P_ToIndex(sector));

    Writer_WriteInt16(writer, (int)bottomHeight);
    Writer_WriteInt16(writer, (int)topHeight);
    Writer_WriteInt32(writer, FLT2FIX(speed));

    Writer_WriteByte(writer, crush);

    Writer_WriteByte(writer, (byte) state);
    Writer_WriteInt32(writer, tag);
    Writer_WriteByte(writer, (byte) oldState);
}

int ceiling_s::read(MapStateReader *msr)
{
    Reader1 *reader = msr->reader();
    int mapVersion = msr->mapVersion();

#if __JHEXEN__
    if(mapVersion >= 4)
#else
    if(mapVersion >= 5)
#endif
    {
        // Note: the thinker class byte has already been read.
        int ver = Reader_ReadByte(reader); // version byte.

        thinker.function = T_MoveCeiling;

#if !__JHEXEN__
        // Should we put this into stasis?
        if(mapVersion == 5)
        {
            if(!Reader_ReadByte(reader))
                Thinker_SetStasis(&thinker, true);
        }
#endif

        type         = (ceilingtype_e) Reader_ReadByte(reader);

        sector       = (Sector *)P_ToPtr(DMU_SECTOR, Reader_ReadInt32(reader));
        DE_ASSERT(sector != 0);

        bottomHeight = (float) Reader_ReadInt16(reader);
        topHeight    = (float) Reader_ReadInt16(reader);
        speed        = FIX2FLT((fixed_t) Reader_ReadInt32(reader));

        crush        = Reader_ReadByte(reader);

        if(ver == 2)
            state    = ceilingstate_e(Reader_ReadByte(reader));
        else
            state    = ceilingstate_e(Reader_ReadInt32(reader) == -1? CS_DOWN : CS_UP);

        tag          = Reader_ReadInt32(reader);

        if(ver == 2)
            oldState = ceilingstate_e(Reader_ReadByte(reader));
        else
            state    = (Reader_ReadInt32(reader) == -1? CS_DOWN : CS_UP);
    }
    else
    {
        // Its in the old format which serialized ceiling_t
        // Padding at the start (an old thinker_t struct)
        int32_t junk[4]; // sizeof thinker_t
        Reader_Read(reader, (byte *)junk, 16);

        // Start of used data members.
#if __JHEXEN__
        // A 32bit pointer to sector, serialized.
        sector       = (Sector *)P_ToPtr(DMU_SECTOR, Reader_ReadInt32(reader));
        DE_ASSERT(sector != 0);

        type         = ceilingtype_e(Reader_ReadInt32(reader));
#else
        type         = ceilingtype_e(Reader_ReadInt32(reader));

        // A 32bit pointer to sector, serialized.
        sector       = (Sector *)P_ToPtr(DMU_SECTOR, Reader_ReadInt32(reader));
        DE_ASSERT(sector != 0);
#endif

        bottomHeight = FIX2FLT((fixed_t) Reader_ReadInt32(reader));
        topHeight    = FIX2FLT((fixed_t) Reader_ReadInt32(reader));
        speed        = FIX2FLT((fixed_t) Reader_ReadInt32(reader));

        crush        = Reader_ReadInt32(reader);
        state        = (Reader_ReadInt32(reader) == -1? CS_DOWN : CS_UP);
        tag          = Reader_ReadInt32(reader);
        oldState     = (Reader_ReadInt32(reader) == -1? CS_DOWN : CS_UP);

        thinker.function = T_MoveCeiling;
#if !__JHEXEN__
        if(junk[2] == 0) // thinker_t::function
        {
            Thinker_SetStasis(&thinker, true);
        }
#endif
    }

    P_ToXSector(sector)->specialData = this;

    return true; // Add this thinker.
}

#if __JDOOM64__
static int EV_DoCeiling2(Line *line, int tag, float basespeed, ceilingtype_e type)
#elif __JHEXEN__
static int EV_DoCeiling2(byte *arg, int tag, float basespeed, ceilingtype_e type)
#else
static int EV_DoCeiling2(int tag, float basespeed, ceilingtype_e type)
#endif
{
    iterlist_t *list = P_GetSectorIterListForTag(tag, false);
    if(!list) return 0;

    int rtn = 0;

    IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
    IterList_RewindIterator(list);

    Sector *sec = 0;
    while((sec = (Sector *)IterList_MoveIterator(list)))
    {
        xsector_t *xsec = P_ToXSector(sec);

        if(xsec->specialData)
            continue;

        // new door thinker
        rtn = 1;
        ceiling_t *ceiling = (ceiling_t *)Z_Calloc(sizeof(*ceiling), PU_MAP, 0);

        ceiling->thinker.function = T_MoveCeiling;
        Thinker_Add(&ceiling->thinker);

        xsec->specialData = ceiling;
        ceiling->sector = sec;
        ceiling->crush = false;
        ceiling->speed = basespeed;

        switch(type)
        {
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
        case CT_CRUSHANDRAISEFAST:
            ceiling->crush = true;
            ceiling->topHeight = P_GetDoublep(sec, DMU_CEILING_HEIGHT);
            ceiling->bottomHeight = P_GetDoublep(sec, DMU_FLOOR_HEIGHT) + 8;

            ceiling->state = CS_DOWN;
            ceiling->speed *= 2;
            break;
#endif
#if __JHEXEN__
        case CT_CRUSHRAISEANDSTAY:
            ceiling->crush = (int) arg[2];    // arg[2] = crushing value
            ceiling->topHeight = P_GetDoublep(sec, DMU_CEILING_HEIGHT);
            ceiling->bottomHeight = P_GetDoublep(sec, DMU_FLOOR_HEIGHT) + 8;
            ceiling->state = CS_DOWN;
            break;
#endif
#if __JDOOM__ || __JDOOM64__
        case CT_SILENTCRUSHANDRAISE:
#endif
        case CT_CRUSHANDRAISE:
#if !__JHEXEN__
            ceiling->crush = true;
#endif
            ceiling->topHeight = P_GetDoublep(sec, DMU_CEILING_HEIGHT);

        case CT_LOWERANDCRUSH:
#if __JHEXEN__
            ceiling->crush = (int) arg[2];    // arg[2] = crushing value
#endif
        case CT_LOWERTOFLOOR:
            ceiling->bottomHeight = P_GetDoublep(sec, DMU_FLOOR_HEIGHT);

            if(type != CT_LOWERTOFLOOR)
                ceiling->bottomHeight += 8;
            ceiling->state = CS_DOWN;
#if __JDOOM64__
            ceiling->speed *= 8; // jd64
#endif
            break;

        case CT_RAISETOHIGHEST:
            P_FindSectorSurroundingHighestCeiling(sec, 0, &ceiling->topHeight);
#if __JDOOM64__
            ceiling->topHeight -= 8;   // jd64
#endif
            ceiling->state = CS_UP;
            break;
#if __JDOOM64__
        case CT_CUSTOM: // jd64
            {
            //bitmip? wha?
            Side *front = (Side *)P_GetPtrp(line, DMU_FRONT);
            Side *back = (Side *)P_GetPtrp(line, DMU_BACK);
            coord_t bitmipL = 0, bitmipR = 0;

            bitmipL = P_GetDoublep(front, DMU_MIDDLE_MATERIAL_OFFSET_X);
            if(back)
                bitmipR = P_GetDoublep(back, DMU_MIDDLE_MATERIAL_OFFSET_X);

            if(bitmipR > 0)
            {
                P_FindSectorSurroundingHighestCeiling(sec, 0, &ceiling->topHeight);
                ceiling->state = CS_UP;
                ceiling->speed *= bitmipL;
                ceiling->topHeight -= bitmipR;
            }
            else
            {
                ceiling->bottomHeight = P_GetDoublep(sec, DMU_FLOOR_HEIGHT);
                ceiling->bottomHeight -= bitmipR;
                ceiling->state = CS_DOWN;
                ceiling->speed *= bitmipL;
            }
            }
#endif
#if __JHEXEN__
        case CT_LOWERBYVALUE:
            ceiling->bottomHeight =
                P_GetDoublep(sec, DMU_CEILING_HEIGHT) - (coord_t) arg[2];
            ceiling->state = CS_DOWN;
            break;

        case CT_RAISEBYVALUE:
            ceiling->topHeight =
                P_GetDoublep(sec, DMU_CEILING_HEIGHT) + (coord_t) arg[2];
            ceiling->state = CS_UP;
            break;

        case CT_MOVETOVALUEMUL8:
            {
            coord_t destHeight = (coord_t) arg[2] * 8;

            if(arg[3]) // Going down?
                destHeight = -destHeight;

            if(P_GetDoublep(sec, DMU_CEILING_HEIGHT) <= destHeight)
            {
                ceiling->state = CS_UP;
                ceiling->topHeight = destHeight;
                if(FEQUAL(P_GetDoublep(sec, DMU_CEILING_HEIGHT), destHeight))
                    rtn = 0;
            }
            else if(P_GetDoublep(sec, DMU_CEILING_HEIGHT) > destHeight)
            {
                ceiling->state = CS_DOWN;
                ceiling->bottomHeight = destHeight;
            }
            break;
            }
#endif
        default:
#if __JHEXEN__
            rtn = 0;
#endif
            break;
        }

        ceiling->tag = xsec->tag;
        ceiling->type = type;

#if __JHEXEN__
        if(rtn)
        {
            SN_StartSequence((mobj_t *)P_GetPtrp(ceiling->sector, DMU_EMITTER),
                             SEQ_PLATFORM + P_ToXSector(ceiling->sector)->seqType);
        }
#endif
    }
    return rtn;
}

#if __JHEXEN__
int EV_DoCeiling(Line * /*line*/, byte *args, ceilingtype_e type)
#else
int EV_DoCeiling(Line *line, ceilingtype_e type)
#endif
{
#if __JHEXEN__
    return EV_DoCeiling2(args, (int) args[0], (float) args[1] * (1.0 / 8),
                         type);
#else
    int rtn = 0;
    // Reactivate in-stasis ceilings...for certain types.
    switch(type)
    {
    case CT_CRUSHANDRAISEFAST:
# if __JDOOM__ || __JDOOM64__
    case CT_SILENTCRUSHANDRAISE:
# endif
    case CT_CRUSHANDRAISE:
        rtn = P_CeilingActivate(P_ToXLine(line)->tag);
        break;

    default:
        break;
    }
# if __JDOOM64__
    return EV_DoCeiling2(line, P_ToXLine(line)->tag, CEILSPEED, type) || rtn;
# else
    return EV_DoCeiling2(P_ToXLine(line)->tag, CEILSPEED, type) || rtn;
# endif
#endif
}

#if !__JHEXEN__
struct activateceilingparams_t
{
    short tag;
    int count;
};

static int activateCeiling(thinker_t *th, void *context)
{
    ceiling_t *ceiling = (ceiling_t *) th;
    activateceilingparams_t *params = (activateceilingparams_t *) context;

    if(ceiling->tag == (int) params->tag && Thinker_InStasis(&ceiling->thinker))
    {
        ceiling->state = ceiling->oldState;
        Thinker_SetStasis(&ceiling->thinker, false);
        params->count++;
    }

    return false; // Continue iteration.
}

int P_CeilingActivate(short tag)
{
    activateceilingparams_t params;

    params.tag = tag;
    params.count = 0;
    Thinker_Iterate(T_MoveCeiling, activateCeiling, &params);

    return params.count;
}
#endif

struct deactivateceilingparams_t
{
    short tag;
    int count;
};

static int deactivateCeiling(thinker_t *th, void *context)
{
    ceiling_t *ceiling = (ceiling_t *) th;
    deactivateceilingparams_t *params = (deactivateceilingparams_t *) context;

#if __JHEXEN__
    if(ceiling->tag == (int) params->tag)
    {
        // Destroy it.
        SN_StopSequence((mobj_t *)P_GetPtrp(ceiling->sector, DMU_EMITTER));
        stopCeiling(ceiling);
        params->count++;
        return true; // Stop iteration.
    }
#else
    if(!Thinker_InStasis(&ceiling->thinker) && ceiling->tag == (int) params->tag)
    {
        // Put it into stasis.
        ceiling->oldState = ceiling->state;
        Thinker_SetStasis(&ceiling->thinker, true);
        params->count++;
    }
#endif
    return false; // Continue iteration.
}

int P_CeilingDeactivate(short tag)
{
    deactivateceilingparams_t params;

    params.tag = tag;
    params.count = 0;
    Thinker_Iterate(T_MoveCeiling, deactivateCeiling, &params);

    return params.count;
}
