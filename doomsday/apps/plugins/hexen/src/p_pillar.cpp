/** @file p_pillar.cpp
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Activision
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

#include "jhexen.h"
#include "p_pillar.h"

#include "dmu_lib.h"
#include "p_mapspec.h"
#include "p_iterlist.h"

void T_BuildPillar(pillar_t *pillar)
{
    DE_ASSERT(pillar);

    // First, raise the floor
    result_e res1 = T_MovePlane(pillar->sector, pillar->floorSpeed, pillar->floorDest, pillar->crush, 0, pillar->direction); // floorOrCeiling, direction
    // Then, lower the ceiling
    result_e res2 = T_MovePlane(pillar->sector, pillar->ceilingSpeed, pillar->ceilingDest, pillar->crush, 1, -pillar->direction);
    if(res1 == pastdest && res2 == pastdest)
    {
        P_ToXSector(pillar->sector)->specialData = 0;
        SN_StopSequenceInSec(pillar->sector);
        P_NotifySectorFinished(P_ToXSector(pillar->sector)->tag);
        Thinker_Remove(&pillar->thinker);
    }
}

void pillar_s::write(MapStateWriter *msw) const
{
    Writer1 *writer = msw->writer();

    Writer_WriteByte(writer, 1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    Writer_WriteInt32(writer, P_ToIndex(sector));

    Writer_WriteInt32(writer, FLT2FIX(ceilingSpeed));
    Writer_WriteInt32(writer, FLT2FIX(floorSpeed));
    Writer_WriteInt32(writer, FLT2FIX(floorDest));
    Writer_WriteInt32(writer, FLT2FIX(ceilingDest));
    Writer_WriteInt32(writer, direction);
    Writer_WriteInt32(writer, crush);
}

int pillar_s::read(MapStateReader *msr)
{
    Reader1 *reader = msr->reader();
    int mapVersion = msr->mapVersion();

    if(mapVersion >= 4)
    {
        // Note: the thinker class byte has already been read.
        /*int ver =*/ Reader_ReadByte(reader); // version byte.

        // Start of used data members.
        sector          = (Sector *)P_ToPtr(DMU_SECTOR, (int) Reader_ReadInt32(reader));
        DE_ASSERT(sector != 0);

        ceilingSpeed    = FIX2FLT((fixed_t) Reader_ReadInt32(reader));
        floorSpeed      = FIX2FLT((fixed_t) Reader_ReadInt32(reader));
        floorDest       = FIX2FLT((fixed_t) Reader_ReadInt32(reader));
        ceilingDest     = FIX2FLT((fixed_t) Reader_ReadInt32(reader));
        direction       = Reader_ReadInt32(reader);
        crush           = Reader_ReadInt32(reader);
    }
    else
    {
        // Its in the old pre V4 format which serialized pillar_t
        // Padding at the start (an old thinker_t struct)
        byte junk[16]; // sizeof thinker_t
        Reader_Read(reader, junk, 16);

        // Start of used data members.
        // A 32bit pointer to sector, serialized.
        sector          = (Sector *)P_ToPtr(DMU_SECTOR, (int) Reader_ReadInt32(reader));
        DE_ASSERT(sector != 0);

        ceilingSpeed    = FIX2FLT((fixed_t) Reader_ReadInt32(reader));
        floorSpeed      = FIX2FLT((fixed_t) Reader_ReadInt32(reader));
        floorDest       = FIX2FLT((fixed_t) Reader_ReadInt32(reader));
        ceilingDest     = FIX2FLT((fixed_t) Reader_ReadInt32(reader));
        direction       = Reader_ReadInt32(reader);
        crush           = Reader_ReadInt32(reader);
    }

    thinker.function = (thinkfunc_t) T_BuildPillar;

    P_ToXSector(sector)->specialData = this;

    return true; // Add this thinker.
}

int EV_BuildPillar(Line * /*line*/, byte *args, dd_bool crush)
{
    iterlist_t *list = P_GetSectorIterListForTag((int) args[0], false);
    if(!list) return 0;

    int rtn = 0;

    IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
    IterList_RewindIterator(list);

    Sector *sec;
    while((sec = (Sector *)IterList_MoveIterator(list)))
    {
        // If already moving keep going...
        if(P_ToXSector(sec)->specialData)
            continue;

        if(FEQUAL(P_GetDoublep(sec, DMU_FLOOR_HEIGHT),
                  P_GetDoublep(sec, DMU_CEILING_HEIGHT)))
            continue; // Pillar is already closed.

        rtn = 1;
        coord_t newHeight = 0;
        if(!args[2])
        {
            newHeight = P_GetDoublep(sec, DMU_FLOOR_HEIGHT) +
                ((P_GetDoublep(sec, DMU_CEILING_HEIGHT) -
                  P_GetDoublep(sec, DMU_FLOOR_HEIGHT)) * .5f);
        }
        else
        {
            newHeight = P_GetDoublep(sec, DMU_FLOOR_HEIGHT) + (coord_t) args[2];
        }

        pillar_t *pillar = (pillar_t *)Z_Calloc(sizeof(*pillar), PU_MAP, 0);
        pillar->thinker.function = (thinkfunc_t) T_BuildPillar;
        Thinker_Add(&pillar->thinker);

        P_ToXSector(sec)->specialData = pillar;
        pillar->sector = sec;

        if(!args[2])
        {
            pillar->ceilingSpeed = pillar->floorSpeed =
                (float) args[1] * (1.0f / 8);
        }
        else if(newHeight - P_GetDoublep(sec, DMU_FLOOR_HEIGHT) >
                P_GetDoublep(sec, DMU_CEILING_HEIGHT) - newHeight)
        {
            pillar->floorSpeed = (float) args[1] * (1.0f / 8);
            pillar->ceilingSpeed =
                (P_GetDoublep(sec, DMU_CEILING_HEIGHT) - newHeight) *
                      (pillar->floorSpeed / (newHeight - P_GetDoublep(sec, DMU_FLOOR_HEIGHT)));
        }
        else
        {
            pillar->ceilingSpeed = (float) args[1] * (1.0f / 8);
            pillar->floorSpeed =
                (newHeight - P_GetDoublep(sec, DMU_FLOOR_HEIGHT)) *
                    (pillar->ceilingSpeed / (P_GetDoublep(sec, DMU_CEILING_HEIGHT) - newHeight));
        }

        pillar->floorDest = newHeight;
        pillar->ceilingDest = newHeight;
        pillar->direction = 1;
        pillar->crush = crush * (int) args[3];
        SN_StartSequence((mobj_t *)P_GetPtrp(pillar->sector, DMU_EMITTER),
                         SEQ_PLATFORM + P_ToXSector(pillar->sector)->seqType);
    }
    return rtn;
}

int EV_OpenPillar(Line * /*line*/, byte *args)
{
    iterlist_t *list = P_GetSectorIterListForTag((int) args[0], false);
    if(!list) return 0;

    int rtn = 0;

    IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
    IterList_RewindIterator(list);

    Sector *sec;
    while((sec = (Sector *)IterList_MoveIterator(list)))
    {
        // If already moving keep going...
        if(P_ToXSector(sec)->specialData)
            continue;

        if(!FEQUAL(P_GetDoublep(sec, DMU_FLOOR_HEIGHT),
                   P_GetDoublep(sec, DMU_CEILING_HEIGHT)))
            continue; // Pillar isn't closed.

        rtn = 1;

        pillar_t *pillar = (pillar_t *)Z_Calloc(sizeof(*pillar), PU_MAP, 0);
        pillar->thinker.function = (thinkfunc_t) T_BuildPillar;
        Thinker_Add(&pillar->thinker);

        P_ToXSector(sec)->specialData = pillar;
        pillar->sector = sec;
        if(!args[2])
        {
            P_FindSectorSurroundingLowestFloor(sec,
                P_GetDoublep(sec, DMU_FLOOR_HEIGHT), &pillar->floorDest);
        }
        else
        {
            pillar->floorDest =
                P_GetDoublep(sec, DMU_FLOOR_HEIGHT) - (coord_t) args[2];
        }

        if(!args[3])
        {
            P_FindSectorSurroundingHighestCeiling(sec, 0, &pillar->ceilingDest);
        }
        else
        {
            pillar->ceilingDest =
                P_GetDoublep(sec, DMU_CEILING_HEIGHT) + (coord_t) args[3];
        }

        if(P_GetDoublep(sec, DMU_FLOOR_HEIGHT) - pillar->floorDest >=
           pillar->ceilingDest - P_GetDoublep(sec, DMU_CEILING_HEIGHT))
        {
            pillar->floorSpeed = (float) args[1] * (1.0f / 8);
            pillar->ceilingSpeed =
                (P_GetDoublep(sec, DMU_CEILING_HEIGHT) - pillar->ceilingDest) *
                    (pillar->floorSpeed / (pillar->floorDest - P_GetDoublep(sec, DMU_FLOOR_HEIGHT)));
        }
        else
        {
            pillar->ceilingSpeed = (float) args[1] * (1.0f / 8);
            pillar->floorSpeed =
                (pillar->floorDest - P_GetDoublep(sec, DMU_FLOOR_HEIGHT)) *
                    (pillar->ceilingSpeed / (P_GetDoublep(sec, DMU_CEILING_HEIGHT) - pillar->ceilingDest));
        }

        pillar->direction = -1; // Open the pillar.
        SN_StartSequence((mobj_t *)P_GetPtrp(pillar->sector, DMU_EMITTER),
                         SEQ_PLATFORM + P_ToXSector(pillar->sector)->seqType);
    }

    return rtn;
}
