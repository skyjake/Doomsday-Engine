/** @file p_waggle.cpp
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
#include "p_waggle.h"

#include "dmu_lib.h"
#include "p_map.h"
#include "p_mapspec.h"
#include "p_iterlist.h"

void T_FloorWaggle(waggle_t *waggle)
{
    DE_ASSERT(waggle != 0);

    switch(waggle->state)
    {
    default:
    case WS_STABLE:
        if(waggle->ticker != -1)
        {
            if(!--waggle->ticker)
            {
                waggle->state = WS_REDUCE;
            }
        }
        break;

    case WS_EXPAND:
        if((waggle->scale += waggle->scaleDelta) >= waggle->targetScale)
        {
            waggle->scale = waggle->targetScale;
            waggle->state = WS_STABLE;
        }
        break;

    case WS_REDUCE:
        if((waggle->scale -= waggle->scaleDelta) <= 0)
        {
            // Remove.
            P_SetDoublep(waggle->sector, DMU_FLOOR_HEIGHT, waggle->originalHeight);
            P_ChangeSector(waggle->sector, 1 /*crush damage*/);
            P_ToXSector(waggle->sector)->specialData = nullptr;
            P_NotifySectorFinished(P_ToXSector(waggle->sector)->tag);
            Thinker_Remove(&waggle->thinker);
            return;
        }
        break;
    }

    waggle->accumulator += waggle->accDelta;
    coord_t fh = waggle->originalHeight +
        FLOATBOBOFFSET(((int) waggle->accumulator) & 63) * waggle->scale;
    P_SetDoublep(waggle->sector, DMU_FLOOR_HEIGHT, fh);
    P_SetDoublep(waggle->sector, DMU_FLOOR_TARGET_HEIGHT, fh);
    P_SetFloatp(waggle->sector, DMU_FLOOR_SPEED, 0);
    P_ChangeSector(waggle->sector, 1 /*crush damage*/);
}

void waggle_s::write(MapStateWriter *msw) const
{
    Writer1 *writer = msw->writer();

    Writer_WriteByte(writer, 1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    Writer_WriteInt32(writer, P_ToIndex(sector));

    Writer_WriteInt32(writer, FLT2FIX(originalHeight));
    Writer_WriteInt32(writer, FLT2FIX(accumulator));
    Writer_WriteInt32(writer, FLT2FIX(accDelta));
    Writer_WriteInt32(writer, FLT2FIX(targetScale));
    Writer_WriteInt32(writer, FLT2FIX(scale));
    Writer_WriteInt32(writer, FLT2FIX(scaleDelta));
    Writer_WriteInt32(writer, ticker);
    Writer_WriteInt32(writer, state);
}

int waggle_s::read(MapStateReader *msr)
{
    Reader1 *reader = msr->reader();
    int mapVersion = msr->mapVersion();

    if(mapVersion >= 4)
    {
        /*int ver =*/ Reader_ReadByte(reader); // version byte.

        // Start of used data members.
        sector          = (Sector *)P_ToPtr(DMU_SECTOR, (int) Reader_ReadInt32(reader));
        DE_ASSERT(sector != 0);

        originalHeight  = FIX2FLT((fixed_t) Reader_ReadInt32(reader));
        accumulator     = FIX2FLT((fixed_t) Reader_ReadInt32(reader));
        accDelta        = FIX2FLT((fixed_t) Reader_ReadInt32(reader));
        targetScale     = FIX2FLT((fixed_t) Reader_ReadInt32(reader));
        scale           = FIX2FLT((fixed_t) Reader_ReadInt32(reader));
        scaleDelta      = FIX2FLT((fixed_t) Reader_ReadInt32(reader));
        ticker          = Reader_ReadInt32(reader);
        state           = wagglestate_e(Reader_ReadInt32(reader));
    }
    else
    {
        // Its in the old pre V4 format which serialized waggle_t
        // Padding at the start (an old thinker_t struct)
        byte junk[16]; // sizeof thinker_t
        Reader_Read(reader, junk, 16);

        // Start of used data members.
        // A 32bit pointer to sector, serialized.
        sector          = (Sector *)P_ToPtr(DMU_SECTOR, (int) Reader_ReadInt32(reader));
        DE_ASSERT(sector != 0);

        originalHeight  = FIX2FLT((fixed_t) Reader_ReadInt32(reader));
        accumulator     = FIX2FLT((fixed_t) Reader_ReadInt32(reader));
        accDelta        = FIX2FLT((fixed_t) Reader_ReadInt32(reader));
        targetScale     = FIX2FLT((fixed_t) Reader_ReadInt32(reader));
        scale           = FIX2FLT((fixed_t) Reader_ReadInt32(reader));
        scaleDelta      = FIX2FLT((fixed_t) Reader_ReadInt32(reader));
        ticker          = Reader_ReadInt32(reader);
        state           = wagglestate_e(Reader_ReadInt32(reader));
    }

    thinker.function = (thinkfunc_t) T_FloorWaggle;

    P_ToXSector(sector)->specialData = this;

    return true; // Add this thinker.
}

dd_bool EV_StartFloorWaggle(int tag, int height, int speed, int offset, int timer)
{
    iterlist_t *list = P_GetSectorIterListForTag(tag, false);
    if(!list) return false;

    dd_bool retCode = false;

    IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
    IterList_RewindIterator(list);

    Sector *sec;
    while((sec = (Sector *)IterList_MoveIterator(list)))
    {
        if(P_ToXSector(sec)->specialData)
            continue; // Already moving, so keep going...

        retCode = true;

        waggle_t *waggle = (waggle_t *)Z_Calloc(sizeof(*waggle), PU_MAP, 0);
        waggle->thinker.function = (thinkfunc_t) T_FloorWaggle;
        Thinker_Add(&waggle->thinker);

        P_ToXSector(sec)->specialData = waggle;
        waggle->sector         = sec;
        waggle->originalHeight = P_GetDoublep(sec, DMU_FLOOR_HEIGHT);
        waggle->accumulator    = offset;
        waggle->accDelta       = FIX2FLT(speed << 10);
        waggle->scale          = 0;
        waggle->targetScale    = FIX2FLT(height << 10);
        waggle->scaleDelta     =
            FIX2FLT(FLT2FIX(waggle->targetScale) /
                    (TICSPERSEC + ((3 * TICSPERSEC) * height) / 255));
        waggle->ticker         = timer ? timer * 35 : -1;
        waggle->state          = WS_EXPAND;
    }

    return retCode;
}
