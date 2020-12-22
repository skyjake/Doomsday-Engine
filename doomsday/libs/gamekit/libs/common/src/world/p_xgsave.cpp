/** @file p_xgsave.cpp  XG data/thinker (de)serialization.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__

#include "common.h"
#include "dmu_lib.h"
#include "mapstatereader.h"
#include "mapstatewriter.h"
#include "p_mapsetup.h"
#include "p_saveg.h"
#include "p_xg.h"

void SV_WriteXGLine(Line *li, MapStateWriter *msw)
{
    Writer1 *writer = msw->writer();
    xline_t *xline = P_ToXLine(li);

    // Version byte.
    Writer_WriteByte(writer, 1);

    /*
     * Remember, savegames are applied on top of an initialized map. No strings are saved,
     * because they are all const strings defined either in the maps's DDXGDATA lump or a
     * DED file. During loading, XL_SetLineType is called with the id in the savegame.
     */

    DE_ASSERT(xline->xg != 0);
    xgline_t *xg = xline->xg;
    linetype_t *info = &xg->info;

    Writer_WriteInt32(writer, info->id);
    Writer_WriteInt32(writer, info->actCount);

    Writer_WriteByte(writer, xg->active);
    Writer_WriteByte(writer, xg->disabled);
    Writer_WriteInt32(writer, xg->timer);
    Writer_WriteInt32(writer, xg->tickerTimer);
    Writer_WriteInt16(writer, msw->serialIdFor((mobj_t *)xg->activator));
    Writer_WriteInt32(writer, xg->idata);
    Writer_WriteFloat(writer, xg->fdata);
    Writer_WriteInt32(writer, xg->chIdx);
    Writer_WriteFloat(writer, xg->chTimer);
}

void SV_ReadXGLine(Line *li, MapStateReader *msr)
{
    Reader1 *reader = msr->reader();
    xline_t *xline = P_ToXLine(li);

    // Read version.
    Reader_ReadByte(reader);

    // This'll set all the correct string pointers and other data.
    XL_SetLineType(li, Reader_ReadInt32(reader));

    DE_ASSERT(xline->xg != 0);
    xgline_t *xg = xline->xg;

    xg->info.actCount = Reader_ReadInt32(reader);

    xg->active      = Reader_ReadByte(reader);
    xg->disabled    = Reader_ReadByte(reader);
    xg->timer       = Reader_ReadInt32(reader);
    xg->tickerTimer = Reader_ReadInt32(reader);

    // Will be updated later.
    xg->activator   = INT2PTR(void, Reader_ReadInt16(reader));

    xg->idata       = Reader_ReadInt32(reader);
    xg->fdata       = Reader_ReadFloat(reader);
    xg->chIdx       = Reader_ReadInt32(reader);
    xg->chTimer     = Reader_ReadFloat(reader);
}

/**
 * @param fn  This function must belong to XG sector @a xg.
 */
void SV_WriteXGFunction(xgsector_t * /*xg*/, function_t *fn, Writer1 *writer)
{
    // Version byte.
    Writer_WriteByte(writer, 1);

    Writer_WriteInt32(writer, fn->flags);
    Writer_WriteInt16(writer, fn->pos);
    Writer_WriteInt16(writer, fn->repeat);
    Writer_WriteInt16(writer, fn->timer);
    Writer_WriteInt16(writer, fn->maxTimer);
    Writer_WriteFloat(writer, fn->value);
    Writer_WriteFloat(writer, fn->oldValue);
}

/**
 * @param fn  This function must belong to XG sector @a xg.
 */
void SV_ReadXGFunction(xgsector_t * /*xg*/, function_t *fn, Reader1 *reader, int /*mapVersion*/)
{
    // Version byte.
    Reader_ReadByte(reader);

    fn->flags    = Reader_ReadInt32(reader);
    fn->pos      = Reader_ReadInt16(reader);
    fn->repeat   = Reader_ReadInt16(reader);
    fn->timer    = Reader_ReadInt16(reader);
    fn->maxTimer = Reader_ReadInt16(reader);
    fn->value    = Reader_ReadFloat(reader);
    fn->oldValue = Reader_ReadFloat(reader);
}

void SV_WriteXGSector(Sector *sec, Writer1 *writer)
{
    xsector_t *xsec = P_ToXSector(sec);

    xgsector_t *xg     = xsec->xg;
    sectortype_t *info = &xg->info;

    // Version byte.
    Writer_WriteByte(writer, 1);

    Writer_WriteInt32(writer, info->id);
    Writer_Write(writer, info->count, sizeof(info->count));
    Writer_Write(writer, xg->chainTimer, sizeof(xg->chainTimer));
    Writer_WriteInt32(writer, xg->timer);
    Writer_WriteByte(writer, xg->disabled);
    for(int i = 0; i < 3; ++i)
    {
        SV_WriteXGFunction(xg, &xg->rgb[i], writer);
    }
    for(int i = 0; i < 2; ++i)
    {
        SV_WriteXGFunction(xg, &xg->plane[i], writer);
    }
    SV_WriteXGFunction(xg, &xg->light, writer);
}

void SV_ReadXGSector(Sector *sec, Reader1 *reader, int mapVersion)
{
    xsector_t *xsec = P_ToXSector(sec);

    // Version byte.
    Reader_ReadByte(reader);

    // This'll init all the data.
    XS_SetSectorType(sec, Reader_ReadInt32(reader));

    xgsector_t *xg = xsec->xg;
    Reader_Read(reader, xg->info.count, sizeof(xg->info.count));
    Reader_Read(reader, xg->chainTimer, sizeof(xg->chainTimer));
    xg->timer    = Reader_ReadInt32(reader);
    xg->disabled = Reader_ReadByte(reader);
    for(int i = 0; i < 3; ++i)
    {
        SV_ReadXGFunction(xg, &xg->rgb[i], reader, mapVersion);
    }
    for(int i = 0; i < 2; ++i)
    {
        SV_ReadXGFunction(xg, &xg->plane[i], reader, mapVersion);
    }
    SV_ReadXGFunction(xg, &xg->light, reader, mapVersion);
}

void xgplanemover_s::write(MapStateWriter *msw) const
{
    Writer1 *writer = msw->writer();

    Writer_WriteByte(writer, 3); // Version.

    Writer_WriteInt32(writer, P_ToIndex(sector));
    Writer_WriteByte(writer, ceiling);
    Writer_WriteInt32(writer, flags);

    int i = P_ToIndex(origin);
    if(i >= 0 && i < numlines) // Is it a real line?
        i++;
    else // No.
        i = 0;

    Writer_WriteInt32(writer, i); // Zero means there is no origin.

    Writer_WriteInt32(writer, FLT2FIX(destination));
    Writer_WriteInt32(writer, FLT2FIX(speed));
    Writer_WriteInt32(writer, FLT2FIX(crushSpeed));
    Writer_WriteInt32(writer, msw->serialIdFor(setMaterial));
    Writer_WriteInt32(writer, setSectorType);
    Writer_WriteInt32(writer, startSound);
    Writer_WriteInt32(writer, endSound);
    Writer_WriteInt32(writer, moveSound);
    Writer_WriteInt32(writer, minInterval);
    Writer_WriteInt32(writer, maxInterval);
    Writer_WriteInt32(writer, timer);
}

int xgplanemover_s::read(MapStateReader *msr)
{
    Reader1 *reader = msr->reader();

    byte ver = Reader_ReadByte(reader); // Version.

    sector      = (Sector *)P_ToPtr(DMU_SECTOR, Reader_ReadInt32(reader));
    ceiling     = Reader_ReadByte(reader);
    flags       = Reader_ReadInt32(reader);

    int lineIndex = Reader_ReadInt32(reader);
    if(lineIndex > 0)
        origin  = (Line *)P_ToPtr(DMU_LINE, lineIndex - 1);

    destination = FIX2FLT(Reader_ReadInt32(reader));
    speed       = FIX2FLT(Reader_ReadInt32(reader));
    crushSpeed  = FIX2FLT(Reader_ReadInt32(reader));

    if(ver >= 3)
    {
        setMaterial = msr->material(Reader_ReadInt32(reader), 0);
    }
    else
    {
        // Flat number is an absolute lump index.
        res::Uri uri("Flats:", CentralLumpIndex()[Reader_ReadInt32(reader)].name().fileNameWithoutExtension());
        setMaterial = (world_Material *)P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(reinterpret_cast<uri_s *>(&uri)));
    }

    setSectorType = Reader_ReadInt32(reader);
    startSound    = Reader_ReadInt32(reader);
    endSound      = Reader_ReadInt32(reader);
    moveSound     = Reader_ReadInt32(reader);
    minInterval   = Reader_ReadInt32(reader);
    maxInterval   = Reader_ReadInt32(reader);
    timer         = Reader_ReadInt32(reader);

    thinker.function = (thinkfunc_t) XS_PlaneMover;

    return true; // Add this thinker.
}

#endif
