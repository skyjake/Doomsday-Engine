/**
 * @file p_xgsave.c: Extended Generalized Line Types.
 *
 * Implements: Saving and loading routines for the XG data/thinkers.
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
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
#include "p_mapsetup.h"
#include "p_saveg.h"
#include "p_xg.h"

void SV_WriteXGLine(LineDef* li)
{
    xgline_t* xg;
    linetype_t* info;

    xg = P_ToXLine(li)->xg;
    info = &xg->info;

    // Version byte.
    SV_WriteByte(1);

    /**
     * Remember, savegames are applied on top of an initialized map.
     * No strings are saved, because they are all const strings
     * defined either in the maps's DDXGDATA lump or a DED file.
     * During loading, XL_SetLineType is called with the id in the savegame.
     */

    SV_WriteLong(info->id);
    SV_WriteLong(info->actCount);

    SV_WriteByte(xg->active);
    SV_WriteByte(xg->disabled);
    SV_WriteLong(xg->timer);
    SV_WriteLong(xg->tickerTimer);
    SV_WriteShort(SV_ThingArchiveNum(xg->activator));
    SV_WriteLong(xg->idata);
    SV_WriteFloat(xg->fdata);
    SV_WriteLong(xg->chIdx);
    SV_WriteFloat(xg->chTimer);
}

void SV_ReadXGLine(LineDef* li)
{
    xgline_t* xg;
    xline_t* xline = P_ToXLine(li);

    // Read version.
    SV_ReadByte();

    // This'll set all the correct string pointers and other data.
    XL_SetLineType(li, SV_ReadLong());

    if(!xline || !xline->xg)
        Con_Error("SV_ReadXGLine: Bad XG line!\n");

    xg = xline->xg;

    xg->info.actCount = SV_ReadLong();
    xg->active = SV_ReadByte();
    xg->disabled = SV_ReadByte();
    xg->timer = SV_ReadLong();
    xg->tickerTimer = SV_ReadLong();

    // Will be updated later.
    xg->activator = INT2PTR(void, SV_ReadShort());

    xg->idata = SV_ReadLong();
    xg->fdata = SV_ReadFloat();
    xg->chIdx = SV_ReadLong();
    xg->chTimer = SV_ReadFloat();
}

/**
 * @param fn  This function must belong to XG sector @a xg.
 */
void SV_WriteXGFunction(xgsector_t* xg, function_t* fn)
{
    // Version byte.
    SV_WriteByte(1);

    SV_WriteLong(fn->flags);
    SV_WriteShort(fn->pos);
    SV_WriteShort(fn->repeat);
    SV_WriteShort(fn->timer);
    SV_WriteShort(fn->maxTimer);
    SV_WriteFloat(fn->value);
    SV_WriteFloat(fn->oldValue);
}

/**
 * @param fn  This function must belong to XG sector @a xg.
 */
void SV_ReadXGFunction(xgsector_t* xg, function_t* fn)
{
    // Version byte.
    SV_ReadByte();

    fn->flags = SV_ReadLong();
    fn->pos = SV_ReadShort();
    fn->repeat = SV_ReadShort();
    fn->timer = SV_ReadShort();
    fn->maxTimer = SV_ReadShort();
    fn->value = SV_ReadFloat();
    fn->oldValue = SV_ReadFloat();
}

void SV_WriteXGSector(struct sector_s* sec)
{
    xgsector_t* xg;
    sectortype_t* info;
    xsector_t* xsec = P_ToXSector(sec);
    int i;

    xg = xsec->xg;
    info = &xg->info;

    // Version byte.
    SV_WriteByte(1);

    SV_WriteLong(info->id);
    SV_Write(info->count, sizeof(info->count));
    SV_Write(xg->chainTimer, sizeof(xg->chainTimer));
    SV_WriteLong(xg->timer);
    SV_WriteByte(xg->disabled);
    for(i = 0; i < 3; ++i)
        SV_WriteXGFunction(xg, &xg->rgb[i]);
    for(i = 0; i < 2; ++i)
        SV_WriteXGFunction(xg, &xg->plane[i]);
    SV_WriteXGFunction(xg, &xg->light);
}

void SV_ReadXGSector(struct sector_s *sec)
{
    int         i;
    xgsector_t *xg;
    xsector_t  *xsec = P_ToXSector(sec);

    // Version byte.
    SV_ReadByte();

    // This'll init all the data.
    XS_SetSectorType(sec, SV_ReadLong());
    xg = xsec->xg;
    SV_Read(xg->info.count, sizeof(xg->info.count));
    SV_Read(xg->chainTimer, sizeof(xg->chainTimer));
    xg->timer = SV_ReadLong();
    xg->disabled = SV_ReadByte();
    for(i = 0; i < 3; ++i)
        SV_ReadXGFunction(xg, &xg->rgb[i]);
    for(i = 0; i < 2; ++i)
        SV_ReadXGFunction(xg, &xg->plane[i]);
    SV_ReadXGFunction(xg, &xg->light);
}

void SV_WriteXGPlaneMover(thinker_t* th)
{
    xgplanemover_t* mov = (xgplanemover_t*) th;
    uint i;

    SV_WriteByte(3); // Version.

    SV_WriteLong(P_ToIndex(mov->sector));
    SV_WriteByte(mov->ceiling);
    SV_WriteLong(mov->flags);

    i = P_ToIndex(mov->origin);
    if(i < numlines) // Is it a real line?
        i++;
    else // No.
        i = 0;

    SV_WriteLong(i); // Zero means there is no origin.

    SV_WriteLong(FLT2FIX(mov->destination));
    SV_WriteLong(FLT2FIX(mov->speed));
    SV_WriteLong(FLT2FIX(mov->crushSpeed));
    SV_WriteLong(MaterialArchive_FindUniqueSerialId(SV_MaterialArchive(), mov->setMaterial));
    SV_WriteLong(mov->setSectorType);
    SV_WriteLong(mov->startSound);
    SV_WriteLong(mov->endSound);
    SV_WriteLong(mov->moveSound);
    SV_WriteLong(mov->minInterval);
    SV_WriteLong(mov->maxInterval);
    SV_WriteLong(mov->timer);
}

/**
 * Reads the plane mover thinker.
 */
int SV_ReadXGPlaneMover(xgplanemover_t* mov)
{
    byte ver = SV_ReadByte(); // Version.

    mov->sector = P_ToPtr(DMU_SECTOR, SV_ReadLong());
    mov->ceiling = SV_ReadByte();
    mov->flags = SV_ReadLong();

    {
    int lineDefIndex = SV_ReadLong();
    if(lineDefIndex > 0)
        mov->origin = P_ToPtr(DMU_LINEDEF, lineDefIndex - 1);
    }

    mov->destination = FIX2FLT(SV_ReadLong());
    mov->speed = FIX2FLT(SV_ReadLong());
    mov->crushSpeed = FIX2FLT(SV_ReadLong());

    if(ver >= 3)
    {
        mov->setMaterial = SV_GetArchiveMaterial(SV_ReadLong(), 0);
    }
    else
    {
        // Flat number is an absolute lump index.
        Uri* uri = Uri_NewWithPath2("Flats:", RC_NULL);
        ddstring_t name;
        Str_Init(&name);
        F_FileName(&name, Str_Text(W_LumpName(SV_ReadLong())));
        Uri_SetPath(uri, Str_Text(&name));
        mov->setMaterial = P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));
        Uri_Delete(uri);
        Str_Free(&name);
    }

    mov->setSectorType = SV_ReadLong();
    mov->startSound = SV_ReadLong();
    mov->endSound = SV_ReadLong();
    mov->moveSound = SV_ReadLong();
    mov->minInterval = SV_ReadLong();
    mov->maxInterval = SV_ReadLong();
    mov->timer = SV_ReadLong();

    mov->thinker.function = XS_PlaneMover;
    return true; // Add this thinker.
}

#endif
