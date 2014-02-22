/** @file p_saveg.cpp  Common game-save state management.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "common.h"
#include "p_saveg.h"

#include "dmu_lib.h"
#include "g_common.h"
#include "p_actor.h"
#include "p_tick.h"          // mapTime
#include "p_saveio.h"
#include "mapstatereader.h"
#include "mapstatewriter.h"
#include "saveslots.h"
#include <de/NativePath>
#include <de/String>
#include <de/memory.h>
#include <cstdio>
#include <cstring>

int saveToRealPlayerNum[MAXPLAYERS];
#if __JHEXEN__
targetplraddress_t *targetPlayerAddrs;
#endif

#if !__JHEXEN__
/**
 * Compose the save game file name for the specified @a sessionId.
 *
 * @param sessionId  Unique game-session identifier.
 *
 * @return  File path to the reachable save directory. If the game-save path
 *          is unreachable then a zero-length string is returned instead.
 */
static de::String saveNameForClientSessionId(uint sessionId)
{
    // Do we have a valid path?
    if(!F_MakePath(de::NativePath(SV_ClientSavePath()).expand().toUtf8().constData()))
    {
        return "";
    }

    // Compose the full game-save path and filename.
    return de::String(CLIENTSAVEGAMENAME "%1")
                   .arg(sessionId, 8, 16, QChar('0')).toUpper();
}
#endif

#if __JHEXEN__
dd_bool SV_HxHaveMapStateForSlot(int slotNumber, uint map)
{
    if(SV_SavePath().isEmpty() || !G_SaveSlots().isKnownSlot(slotNumber))
    {
        return false;
    }
    return SV_ExistingFile(SV_SavePath() / G_SaveSlots()[slotNumber].saveInfo().fileNameForMap(map));
}

void SV_InitTargetPlayers()
{
    targetPlayerAddrs = 0;
}

void SV_ClearTargetPlayers()
{
    while(targetPlayerAddrs)
    {
        targetplraddress_t *next = targetPlayerAddrs->next;
        M_Free(targetPlayerAddrs);
        targetPlayerAddrs = next;
    }
}
#endif // __JHEXEN__

void SV_TranslateLegacyMobjFlags(mobj_t *mo, int ver)
{
#if __JDOOM64__
    // Nothing to do.
    DENG2_UNUSED(mo);
    DENG2_UNUSED(ver);
    return;
#endif

#if __JDOOM__ || __JHERETIC__
    if(ver < 6)
    {
        // mobj.flags
# if __JDOOM__
        // switched values for MF_BRIGHTSHADOW <> MF_BRIGHTEXPLODE
        if((mo->flags & MF_BRIGHTEXPLODE) != (mo->flags & MF_BRIGHTSHADOW))
        {
            if(mo->flags & MF_BRIGHTEXPLODE) // previously MF_BRIGHTSHADOW
            {
                mo->flags |= MF_BRIGHTSHADOW;
                mo->flags &= ~MF_BRIGHTEXPLODE;
            }
            else // previously MF_BRIGHTEXPLODE
            {
                mo->flags |= MF_BRIGHTEXPLODE;
                mo->flags &= ~MF_BRIGHTSHADOW;
            }
        } // else they were both on or off so it doesn't matter.
# endif
        // Remove obsoleted flags in earlier save versions.
        mo->flags &= ~MF_V6OBSOLETE;

        // mobj.flags2
# if __JDOOM__
        // jDoom only gained flags2 in ver 6 so all we can do is to
        // apply the values as set in the mobjinfo.
        // Non-persistent flags might screw things up a lot worse otherwise.
        mo->flags2 = mo->info->flags2;
# endif
    }
#endif

#if __JDOOM__ || __JHERETIC__
    if(ver < 9)
    {
        mo->spawnSpot.flags &= ~MASK_UNKNOWN_MSF_FLAGS;
        // Spawn on the floor by default unless the mobjtype flags override.
        mo->spawnSpot.flags |= MSF_Z_FLOOR;
    }
#endif

#if __JHEXEN__
    if(ver < 5)
#else
    if(ver < 7)
#endif
    {
        // flags3 was introduced in a latter version so all we can do is to
        // apply the values as set in the mobjinfo.
        // Non-persistent flags might screw things up a lot worse otherwise.
        mo->flags3 = mo->info->flags3;
    }
}

void playerheader_s::write(Writer *writer)
{
    Writer_WriteByte(writer, 2); // version byte

    numPowers       = NUM_POWER_TYPES;
    numKeys         = NUM_KEY_TYPES;
    numFrags        = MAXPLAYERS;
    numWeapons      = NUM_WEAPON_TYPES;
    numAmmoTypes    = NUM_AMMO_TYPES;
    numPSprites     = NUMPSPRITES;
#if __JHERETIC__ || __JHEXEN__ || __JDOOM64__
    numInvItemTypes = NUM_INVENTORYITEM_TYPES;
#endif
#if __JHEXEN__
    numArmorTypes   = NUMARMOR;
#endif

    Writer_WriteInt32(writer, numPowers);
    Writer_WriteInt32(writer, numKeys);
    Writer_WriteInt32(writer, numFrags);
    Writer_WriteInt32(writer, numWeapons);
    Writer_WriteInt32(writer, numAmmoTypes);
    Writer_WriteInt32(writer, numPSprites);
#if __JDOOM64__ || __JHERETIC__ || __JHEXEN__
    Writer_WriteInt32(writer, numInvItemTypes);
#endif
#if __JHEXEN__
    Writer_WriteInt32(writer, numArmorTypes);
#endif
}

void playerheader_s::read(Reader *reader, int saveVersion)
{
#if __JHEXEN__
    if(saveVersion >= 4)
#else
    if(saveVersion >= 5)
#endif
    {
        int ver = Reader_ReadByte(reader);
#if !__JHERETIC__
        DENG2_UNUSED(ver);
#endif

        numPowers      = Reader_ReadInt32(reader);
        numKeys        = Reader_ReadInt32(reader);
        numFrags       = Reader_ReadInt32(reader);
        numWeapons     = Reader_ReadInt32(reader);
        numAmmoTypes   = Reader_ReadInt32(reader);
        numPSprites    = Reader_ReadInt32(reader);
#if __JHERETIC__
        if(ver >= 2)
            numInvItemTypes = Reader_ReadInt32(reader);
        else
            numInvItemTypes = NUM_INVENTORYITEM_TYPES;
#endif
#if __JHEXEN__ || __JDOOM64__
        numInvItemTypes = Reader_ReadInt32(reader);
#endif
#if __JHEXEN__
        numArmorTypes  = Reader_ReadInt32(reader);
#endif
    }
    else // The old format didn't save the counts.
    {
#if __JHEXEN__
        numPowers       = 9;
        numKeys         = 11;
        numFrags        = 8;
        numWeapons      = 4;
        numAmmoTypes    = 2;
        numPSprites     = 2;
        numInvItemTypes = 33;
        numArmorTypes   = 4;
#elif __JDOOM__ || __JDOOM64__
        numPowers       = 6;
        numKeys         = 6;
        numFrags        = 4; // Why was this only 4?
        numWeapons      = 9;
        numAmmoTypes    = 4;
        numPSprites     = 2;
# if __JDOOM64__
        numInvItemTypes = 3;
# endif
#elif __JHERETIC__
        numPowers       = 9;
        numKeys         = 3;
        numFrags        = 4; // ?
        numWeapons      = 8;
        numInvItemTypes = 14;
        numAmmoTypes    = 6;
        numPSprites     = 2;
#endif
    }
}

enum sectorclass_t
{
    sc_normal,
    sc_ploff, ///< plane offset
#if !__JHEXEN__
    sc_xg1,
#endif
    NUM_SECTORCLASSES
};

void SV_WriteSector(Sector *sec, MapStateWriter *msw)
{
    Writer *writer = msw->writer();

    int i, type;
    float flooroffx           = P_GetFloatp(sec, DMU_FLOOR_MATERIAL_OFFSET_X);
    float flooroffy           = P_GetFloatp(sec, DMU_FLOOR_MATERIAL_OFFSET_Y);
    float ceiloffx            = P_GetFloatp(sec, DMU_CEILING_MATERIAL_OFFSET_X);
    float ceiloffy            = P_GetFloatp(sec, DMU_CEILING_MATERIAL_OFFSET_Y);
    byte lightlevel           = (byte) (255.f * P_GetFloatp(sec, DMU_LIGHT_LEVEL));
    short floorheight         = (short) P_GetIntp(sec, DMU_FLOOR_HEIGHT);
    short ceilingheight       = (short) P_GetIntp(sec, DMU_CEILING_HEIGHT);
    short floorFlags          = (short) P_GetIntp(sec, DMU_FLOOR_FLAGS);
    short ceilingFlags        = (short) P_GetIntp(sec, DMU_CEILING_FLAGS);
    Material *floorMaterial   = (Material *)P_GetPtrp(sec, DMU_FLOOR_MATERIAL);
    Material *ceilingMaterial = (Material *)P_GetPtrp(sec, DMU_CEILING_MATERIAL);

    xsector_t *xsec = P_ToXSector(sec);

#if !__JHEXEN__
    // Determine type.
    if(xsec->xg)
        type = sc_xg1;
    else
#endif
        if(!FEQUAL(flooroffx, 0) || !FEQUAL(flooroffy, 0) || !FEQUAL(ceiloffx, 0) || !FEQUAL(ceiloffy, 0))
        type = sc_ploff;
    else
        type = sc_normal;

    // Type byte.
    Writer_WriteByte(writer, type);

    // Version.
    // 2: Surface colors.
    // 3: Surface flags.
    Writer_WriteByte(writer, 3); // write a version byte.

    Writer_WriteInt16(writer, floorheight);
    Writer_WriteInt16(writer, ceilingheight);
    Writer_WriteInt16(writer, msw->serialIdFor(floorMaterial));
    Writer_WriteInt16(writer, msw->serialIdFor(ceilingMaterial));
    Writer_WriteInt16(writer, floorFlags);
    Writer_WriteInt16(writer, ceilingFlags);
#if __JHEXEN__
    Writer_WriteInt16(writer, (short) lightlevel);
#else
    Writer_WriteByte(writer, lightlevel);
#endif

    float rgb[3];
    P_GetFloatpv(sec, DMU_COLOR, rgb);
    for(i = 0; i < 3; ++i)
        Writer_WriteByte(writer, (byte)(255.f * rgb[i]));

    P_GetFloatpv(sec, DMU_FLOOR_COLOR, rgb);
    for(i = 0; i < 3; ++i)
        Writer_WriteByte(writer, (byte)(255.f * rgb[i]));

    P_GetFloatpv(sec, DMU_CEILING_COLOR, rgb);
    for(i = 0; i < 3; ++i)
        Writer_WriteByte(writer, (byte)(255.f * rgb[i]));

    Writer_WriteInt16(writer, xsec->special);
    Writer_WriteInt16(writer, xsec->tag);

#if __JHEXEN__
    Writer_WriteInt16(writer, xsec->seqType);
#endif

    if(type == sc_ploff
#if !__JHEXEN__
       || type == sc_xg1
#endif
       )
    {
        Writer_WriteFloat(writer, flooroffx);
        Writer_WriteFloat(writer, flooroffy);
        Writer_WriteFloat(writer, ceiloffx);
        Writer_WriteFloat(writer, ceiloffy);
    }

#if !__JHEXEN__
    if(xsec->xg) // Extended General?
    {
        SV_WriteXGSector(sec, writer);
    }
#endif
}

void SV_ReadSector(Sector *sec, MapStateReader *msr)
{
    xsector_t *xsec = P_ToXSector(sec);
    Reader *reader  = msr->reader();
    int mapVersion  = msr->mapVersion();

    // A type byte?
    int type = 0;
#if __JHEXEN__
    if(mapVersion < 4)
        type = sc_ploff;
    else
#else
    if(mapVersion <= 1)
        type = sc_normal;
    else
#endif
        type = Reader_ReadByte(reader);

    // A version byte?
    int ver = 1;
#if __JHEXEN__
    if(mapVersion > 2)
#else
    if(mapVersion > 4)
#endif
    {
        ver = Reader_ReadByte(reader);
    }

    int fh = Reader_ReadInt16(reader);
    int ch = Reader_ReadInt16(reader);

    P_SetIntp(sec, DMU_FLOOR_HEIGHT, fh);
    P_SetIntp(sec, DMU_CEILING_HEIGHT, ch);
#if __JHEXEN__
    // Update the "target heights" of the planes.
    P_SetIntp(sec, DMU_FLOOR_TARGET_HEIGHT, fh);
    P_SetIntp(sec, DMU_CEILING_TARGET_HEIGHT, ch);
    // The move speed is not saved; can cause minor problems.
    P_SetIntp(sec, DMU_FLOOR_SPEED, 0);
    P_SetIntp(sec, DMU_CEILING_SPEED, 0);
#endif

    Material *floorMaterial = 0, *ceilingMaterial = 0;
#if !__JHEXEN__
    if(mapVersion == 1)
    {
        // The flat numbers are absolute lump indices.
        Uri *uri = Uri_NewWithPath2("Flats:", RC_NULL);
        Uri_SetPath(uri, Str_Text(W_LumpName(Reader_ReadInt16(reader))));
        floorMaterial = (Material *)P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));

        Uri_SetPath(uri, Str_Text(W_LumpName(Reader_ReadInt16(reader))));
        ceilingMaterial = (Material *)P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));
        Uri_Delete(uri);
    }
    else if(mapVersion >= 4)
#endif
    {
        // The flat numbers are actually archive numbers.
        floorMaterial   = msr->material(Reader_ReadInt16(reader), 0);
        ceilingMaterial = msr->material(Reader_ReadInt16(reader), 0);
    }

    P_SetPtrp(sec, DMU_FLOOR_MATERIAL,   floorMaterial);
    P_SetPtrp(sec, DMU_CEILING_MATERIAL, ceilingMaterial);

    if(ver >= 3)
    {
        P_SetIntp(sec, DMU_FLOOR_FLAGS,   Reader_ReadInt16(reader));
        P_SetIntp(sec, DMU_CEILING_FLAGS, Reader_ReadInt16(reader));
    }

    byte lightlevel;
#if __JHEXEN__
    lightlevel = (byte) Reader_ReadInt16(reader);
#else
    // In Ver1 the light level is a short
    if(mapVersion == 1)
    {
        lightlevel = (byte) Reader_ReadInt16(reader);
    }
    else
    {
        lightlevel = Reader_ReadByte(reader);
    }
#endif
    P_SetFloatp(sec, DMU_LIGHT_LEVEL, (float) lightlevel / 255.f);

#if !__JHEXEN__
    if(mapVersion > 1)
#endif
    {
        byte rgb[3];
        Reader_Read(reader, rgb, 3);
        for(int i = 0; i < 3; ++i)
            P_SetFloatp(sec, DMU_COLOR_RED + i, rgb[i] / 255.f);
    }

    // Ver 2 includes surface colours
    if(ver >= 2)
    {
        byte rgb[3];
        Reader_Read(reader, rgb, 3);
        for(int i = 0; i < 3; ++i)
            P_SetFloatp(sec, DMU_FLOOR_COLOR_RED + i, rgb[i] / 255.f);

        Reader_Read(reader, rgb, 3);
        for(int i = 0; i < 3; ++i)
            P_SetFloatp(sec, DMU_CEILING_COLOR_RED + i, rgb[i] / 255.f);
    }

    xsec->special = Reader_ReadInt16(reader);
    /*xsec->tag =*/ Reader_ReadInt16(reader);

#if __JHEXEN__
    xsec->seqType = seqtype_t(Reader_ReadInt16(reader));
#endif

    if(type == sc_ploff
#if !__JHEXEN__
       || type == sc_xg1
#endif
       )
    {
        P_SetFloatp(sec, DMU_FLOOR_MATERIAL_OFFSET_X, Reader_ReadFloat(reader));
        P_SetFloatp(sec, DMU_FLOOR_MATERIAL_OFFSET_Y, Reader_ReadFloat(reader));
        P_SetFloatp(sec, DMU_CEILING_MATERIAL_OFFSET_X, Reader_ReadFloat(reader));
        P_SetFloatp(sec, DMU_CEILING_MATERIAL_OFFSET_Y, Reader_ReadFloat(reader));
    }

#if !__JHEXEN__
    if(type == sc_xg1)
    {
        SV_ReadXGSector(sec, reader, mapVersion);
    }
#endif

#if !__JHEXEN__
    if(mapVersion <= 1)
#endif
    {
        xsec->specialData = 0;
    }

    // We'll restore the sound targets latter on
    xsec->soundTarget = 0;
}

void SV_WriteLine(Line *li, MapStateWriter *msw)
{
    xline_t *xli   = P_ToXLine(li);
    Writer *writer = msw->writer();

#if !__JHEXEN__
    Writer_WriteByte(writer, xli->xg? 1 : 0); /// @c 1= XG data will follow.
#else
    Writer_WriteByte(writer, 0);
#endif

    // Version.
    // 2: Per surface texture offsets.
    // 2: Surface colors.
    // 3: "Mapped by player" values.
    // 3: Surface flags.
    // 4: Engine-side line flags.
    Writer_WriteByte(writer, 4); // Write a version byte

    Writer_WriteInt16(writer, P_GetIntp(li, DMU_FLAGS));
    Writer_WriteInt16(writer, xli->flags);

    for(int i = 0; i < MAXPLAYERS; ++i)
        Writer_WriteByte(writer, xli->mapped[i]);

#if __JHEXEN__
    Writer_WriteByte(writer, xli->special);
    Writer_WriteByte(writer, xli->arg1);
    Writer_WriteByte(writer, xli->arg2);
    Writer_WriteByte(writer, xli->arg3);
    Writer_WriteByte(writer, xli->arg4);
    Writer_WriteByte(writer, xli->arg5);
#else
    Writer_WriteInt16(writer, xli->special);
    Writer_WriteInt16(writer, xli->tag);
#endif

    // For each side
    float rgba[4];
    for(int i = 0; i < 2; ++i)
    {
        Side *si = (Side *)P_GetPtrp(li, (i? DMU_BACK:DMU_FRONT));
        if(!si) continue;

        Writer_WriteInt16(writer, P_GetIntp(si, DMU_TOP_MATERIAL_OFFSET_X));
        Writer_WriteInt16(writer, P_GetIntp(si, DMU_TOP_MATERIAL_OFFSET_Y));
        Writer_WriteInt16(writer, P_GetIntp(si, DMU_MIDDLE_MATERIAL_OFFSET_X));
        Writer_WriteInt16(writer, P_GetIntp(si, DMU_MIDDLE_MATERIAL_OFFSET_Y));
        Writer_WriteInt16(writer, P_GetIntp(si, DMU_BOTTOM_MATERIAL_OFFSET_X));
        Writer_WriteInt16(writer, P_GetIntp(si, DMU_BOTTOM_MATERIAL_OFFSET_Y));

        Writer_WriteInt16(writer, P_GetIntp(si, DMU_TOP_FLAGS));
        Writer_WriteInt16(writer, P_GetIntp(si, DMU_MIDDLE_FLAGS));
        Writer_WriteInt16(writer, P_GetIntp(si, DMU_BOTTOM_FLAGS));

        Writer_WriteInt16(writer, msw->serialIdFor((Material *)P_GetPtrp(si, DMU_TOP_MATERIAL)));
        Writer_WriteInt16(writer, msw->serialIdFor((Material *)P_GetPtrp(si, DMU_BOTTOM_MATERIAL)));
        Writer_WriteInt16(writer, msw->serialIdFor((Material *)P_GetPtrp(si, DMU_MIDDLE_MATERIAL)));

        P_GetFloatpv(si, DMU_TOP_COLOR, rgba);
        for(int k = 0; k < 3; ++k)
            Writer_WriteByte(writer, (byte)(255 * rgba[k]));

        P_GetFloatpv(si, DMU_BOTTOM_COLOR, rgba);
        for(int k = 0; k < 3; ++k)
            Writer_WriteByte(writer, (byte)(255 * rgba[k]));

        P_GetFloatpv(si, DMU_MIDDLE_COLOR, rgba);
        for(int k = 0; k < 4; ++k)
            Writer_WriteByte(writer, (byte)(255 * rgba[k]));

        Writer_WriteInt32(writer, P_GetIntp(si, DMU_MIDDLE_BLENDMODE));
        Writer_WriteInt16(writer, P_GetIntp(si, DMU_FLAGS));
    }

#if !__JHEXEN__
    // Extended General?
    if(xli->xg)
    {
        SV_WriteXGLine(li, msw);
    }
#endif
}

/**
 * Reads all versions of archived lines.
 * Including the old Ver1.
 */
void SV_ReadLine(Line *li, MapStateReader *msr)
{
    xline_t *xli   = P_ToXLine(li);
    Reader *reader = msr->reader();
    int mapVersion = msr->mapVersion();

    bool xgDataFollows = false;
#if __JHEXEN__
    if(mapVersion >= 4)
#else
    if(mapVersion >= 2)
#endif
    {
        xgDataFollows = Reader_ReadByte(reader) == 1;
    }
#ifdef __JHEXEN__
    DENG2_UNUSED(xgDataFollows);
#endif

    // A version byte?
    int ver = 1;
#if __JHEXEN__
    if(mapVersion >= 3)
#else
    if(mapVersion >= 5)
#endif
    {
        ver = (int) Reader_ReadByte(reader);
    }

    if(ver >= 4)
    {
        P_SetIntp(li, DMU_FLAGS, Reader_ReadInt16(reader));
    }

    int flags = Reader_ReadInt16(reader);
    if(xli->flags & ML_TWOSIDED)
    {
        flags |= ML_TWOSIDED;
    }

    if(ver < 4)
    {
        // Translate old line flags.
        int ddLineFlags = 0;

        if(flags & 0x0001) // old ML_BLOCKING flag
        {
            ddLineFlags |= DDLF_BLOCKING;
            flags &= ~0x0001;
        }

        if(flags & 0x0008) // old ML_DONTPEGTOP flag
        {
            ddLineFlags |= DDLF_DONTPEGTOP;
            flags &= ~0x0008;
        }

        if(flags & 0x0010) // old ML_DONTPEGBOTTOM flag
        {
            ddLineFlags |= DDLF_DONTPEGBOTTOM;
            flags &= ~0x0010;
        }

        P_SetIntp(li, DMU_FLAGS, ddLineFlags);
    }

    if(ver < 3)
    {
        if(flags & ML_MAPPED)
        {
            int lineIDX = P_ToIndex(li);

            // Set line as having been seen by all players..
            de::zap(xli->mapped);
            for(int i = 0; i < MAXPLAYERS; ++i)
            {
                P_SetLineAutomapVisibility(i, lineIDX, true);
            }
        }
    }

    xli->flags = flags;

    if(ver >= 3)
    {
        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            xli->mapped[i] = Reader_ReadByte(reader);
        }
    }

#if __JHEXEN__
    xli->special = Reader_ReadByte(reader);
    xli->arg1    = Reader_ReadByte(reader);
    xli->arg2    = Reader_ReadByte(reader);
    xli->arg3    = Reader_ReadByte(reader);
    xli->arg4    = Reader_ReadByte(reader);
    xli->arg5    = Reader_ReadByte(reader);
#else
    xli->special = Reader_ReadInt16(reader);
    /*xli->tag     =*/ Reader_ReadInt16(reader);
#endif

    // For each side
    for(int i = 0; i < 2; ++i)
    {
        Side *si = (Side *)P_GetPtrp(li, (i? DMU_BACK:DMU_FRONT));
        if(!si) continue;

        // Versions latter than 2 store per surface texture offsets.
        if(ver >= 2)
        {
            float offset[2];

            offset[VX] = (float) Reader_ReadInt16(reader);
            offset[VY] = (float) Reader_ReadInt16(reader);
            P_SetFloatpv(si, DMU_TOP_MATERIAL_OFFSET_XY, offset);

            offset[VX] = (float) Reader_ReadInt16(reader);
            offset[VY] = (float) Reader_ReadInt16(reader);
            P_SetFloatpv(si, DMU_MIDDLE_MATERIAL_OFFSET_XY, offset);

            offset[VX] = (float) Reader_ReadInt16(reader);
            offset[VY] = (float) Reader_ReadInt16(reader);
            P_SetFloatpv(si, DMU_BOTTOM_MATERIAL_OFFSET_XY, offset);
        }
        else
        {
            float offset[2];

            offset[VX] = (float) Reader_ReadInt16(reader);
            offset[VY] = (float) Reader_ReadInt16(reader);

            P_SetFloatpv(si, DMU_TOP_MATERIAL_OFFSET_XY,    offset);
            P_SetFloatpv(si, DMU_MIDDLE_MATERIAL_OFFSET_XY, offset);
            P_SetFloatpv(si, DMU_BOTTOM_MATERIAL_OFFSET_XY, offset);
        }

        if(ver >= 3)
        {
            P_SetIntp(si, DMU_TOP_FLAGS,    Reader_ReadInt16(reader));
            P_SetIntp(si, DMU_MIDDLE_FLAGS, Reader_ReadInt16(reader));
            P_SetIntp(si, DMU_BOTTOM_FLAGS, Reader_ReadInt16(reader));
        }

        Material *topMaterial = 0, *bottomMaterial = 0, *middleMaterial = 0;
#if !__JHEXEN__
        if(mapVersion >= 4)
#endif
        {
            topMaterial    = msr->material(Reader_ReadInt16(reader), 1);
            bottomMaterial = msr->material(Reader_ReadInt16(reader), 1);
            middleMaterial = msr->material(Reader_ReadInt16(reader), 1);
        }

        P_SetPtrp(si, DMU_TOP_MATERIAL,    topMaterial);
        P_SetPtrp(si, DMU_BOTTOM_MATERIAL, bottomMaterial);
        P_SetPtrp(si, DMU_MIDDLE_MATERIAL, middleMaterial);

        // Ver2 includes surface colours
        if(ver >= 2)
        {
            float rgba[4];
            int flags;

            for(int k = 0; k < 3; ++k)
                rgba[k] = (float) Reader_ReadByte(reader) / 255.f;
            rgba[3] = 1;
            P_SetFloatpv(si, DMU_TOP_COLOR, rgba);

            for(int k = 0; k < 3; ++k)
                rgba[k] = (float) Reader_ReadByte(reader) / 255.f;
            rgba[3] = 1;
            P_SetFloatpv(si, DMU_BOTTOM_COLOR, rgba);

            for(int k = 0; k < 4; ++k)
                rgba[k] = (float) Reader_ReadByte(reader) / 255.f;
            P_SetFloatpv(si, DMU_MIDDLE_COLOR, rgba);

            P_SetIntp(si, DMU_MIDDLE_BLENDMODE, Reader_ReadInt32(reader));

            flags = Reader_ReadInt16(reader);

            if(mapVersion < 12)
            {
                if(P_GetIntp(si, DMU_FLAGS) & SDF_SUPPRESS_BACK_SECTOR)
                    flags |= SDF_SUPPRESS_BACK_SECTOR;
            }
            P_SetIntp(si, DMU_FLAGS, flags);
        }
    }

#if !__JHEXEN__
    if(xgDataFollows)
    {
        SV_ReadXGLine(li, msr);
    }
#endif
}

void SV_SaveGameClient(uint sessionId)
{
#if !__JHEXEN__ // unsupported in libhexen
    player_t *pl = &players[CONSOLEPLAYER];
    mobj_t *mo   = pl->plr->mo;

    if(!IS_CLIENT || !mo)
        return;

    if(SV_ClientSavePath().isEmpty())
    {
        return;
    }

    // Prepare new save info.
    SaveInfo *info = SaveInfo::newWithCurrentSessionMetadata(saveNameForClientSessionId(sessionId));
    info->setSessionId(sessionId);

    de::Path path = SV_ClientSavePath() / info->fileName();
    if(!SV_OpenFile(path, "wp"))
    {
        App_Log(DE2_RES_WARNING, "SV_SaveGameClient: Failed opening \"%s\" for writing",
                                 de::NativePath(path).pretty().toLatin1().constData());

        // Discard the useless save info.
        delete info;
        return;
    }

    Writer *writer = SV_NewWriter();
    info->write(writer);

    // Some important information.
    // Our position and look angles.
    Writer_WriteInt32(writer, FLT2FIX(mo->origin[VX]));
    Writer_WriteInt32(writer, FLT2FIX(mo->origin[VY]));
    Writer_WriteInt32(writer, FLT2FIX(mo->origin[VZ]));
    Writer_WriteInt32(writer, FLT2FIX(mo->floorZ));
    Writer_WriteInt32(writer, FLT2FIX(mo->ceilingZ));
    Writer_WriteInt32(writer, mo->angle); /* $unifiedangles */

    Writer_WriteFloat(writer, pl->plr->lookDir); /* $unifiedangles */

    SV_BeginSegment(ASEG_PLAYER_HEADER);
    playerheader_t plrHdr;
    plrHdr.write(writer);

    players[CONSOLEPLAYER].write(writer, plrHdr);

    ThingArchive thingArchive;
    MapStateWriter(thingArchive).write(writer);
    /// @todo No consistency bytes in client saves?

    SV_CloseFile();
    Writer_Delete(writer);
    delete info;
#else
    DENG2_UNUSED(sessionId);
#endif
}

void SV_LoadGameClient(uint sessionId)
{
#if !__JHEXEN__ // unsupported in libhexen
    player_t *cpl = players + CONSOLEPLAYER;
    mobj_t *mo    = cpl->plr->mo;

    if(!IS_CLIENT || !mo)
        return;

    Reader *reader = SV_NewReader();
    SaveInfo *info = SaveInfo::fromReader(reader);
    info->setFileName(saveNameForClientSessionId(sessionId));

    de::Path path = SV_ClientSavePath() / info->fileName();
    if(!SV_OpenFile(path, "rp"))
    {
        Reader_Delete(reader);
        delete info;
        App_Log(DE2_RES_WARNING, "SV_LoadGameClient: Failed opening \"%s\" for reading",
                                 de::NativePath(path).pretty().toLatin1().constData());
        return;
    }

    if(info->magic() != MY_CLIENT_SAVE_MAGIC)
    {
        SV_CloseFile();
        Reader_Delete(reader);
        delete info;
        App_Log(DE2_RES_ERROR, "Client save file format not recognized");
        return;
    }

    // Do we need to change the map?
    if(!Uri_Equality(gameMapUri, info->mapUri()))
    {
        G_NewGame(info->mapUri(), 0/*default*/, &info->gameRules());
        G_SetGameAction(GA_NONE); /// @todo Necessary?
    }
    else
    {
        /// @todo Necessary?
        gameRules = info->gameRules();
    }
    mapTime = info->mapTime();

    P_MobjUnlink(mo);
    mo->origin[VX] = FIX2FLT(Reader_ReadInt32(reader));
    mo->origin[VY] = FIX2FLT(Reader_ReadInt32(reader));
    mo->origin[VZ] = FIX2FLT(Reader_ReadInt32(reader));
    P_MobjLink(mo);
    mo->floorZ     = FIX2FLT(Reader_ReadInt32(reader));
    mo->ceilingZ   = FIX2FLT(Reader_ReadInt32(reader));
    mo->angle      = Reader_ReadInt32(reader); /* $unifiedangles */

    cpl->plr->lookDir = Reader_ReadFloat(reader); /* $unifiedangles */

#if __JHEXEN__
    if(info->version() >= 4)
#else
    if(info->version() >= 5)
#endif
    {
        SV_AssertSegment(ASEG_PLAYER_HEADER);
    }
    playerheader_t plrHdr;
    plrHdr.read(reader, info->version());

    cpl->read(reader, plrHdr);

    MapStateReader(info->version()).read(reader);

    SV_CloseFile();
    Reader_Delete(reader);
    delete info;
#else
    DENG2_UNUSED(sessionId);
#endif
}
