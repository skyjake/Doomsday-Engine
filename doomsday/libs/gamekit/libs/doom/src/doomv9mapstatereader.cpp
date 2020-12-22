/** @file doomv9mapstatereader.cpp  Doom ver 1.9 save game reader.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include "jdoom.h"
#include "doomv9mapstatereader.h"

#include <de/arrayvalue.h>
#include <de/nativepath.h>
#include <de/numbervalue.h>
#include "dmu_lib.h"
#include "g_game.h"
#include "p_ceiling.h"
#include "p_door.h"
#include "p_floor.h"
#include "p_map.h"
#include "p_mapsetup.h"
#include "p_plat.h"
#include "p_saveio.h"
#include "p_saveg.h"
#include "p_tick.h"
#include "r_common.h"    // R_UpdateConsoleView

#define PADSAVEP()                      savePtr += (4 - ((savePtr - saveBuffer) & 3)) & 3

#define SIZEOF_V19_THINKER_T            12
#define V19_THINKER_T_FUNC_OFFSET       8

using namespace de;

static byte *savePtr;
static byte *saveBuffer;

static char sri8(reader_s *r)
{
    if(!r) return 0;
    savePtr++;
    return *(char *) (savePtr - 1);
}

static short sri16(reader_s *r)
{
    if(!r) return 0;
    savePtr += 2;
    return *(int16_t *) (savePtr - 2);
}

static int sri32(reader_s *r)
{
    if(!r) return 0;
    savePtr += 4;
    return *(int32_t *) (savePtr - 4);
}

static void srd(reader_s *r, char *data, int len)
{
    if(!r) return;
    if(data)
    {
        memcpy(data, savePtr, len);
    }
    savePtr += len;
}

static reader_s *SV_NewReader_Dm_v19()
{
    if(!saveBuffer) return 0;
    return Reader_NewWithCallbacks(sri8, sri16, sri32, 0, srd);
}

static uri_s *readTextureUrn(reader_s *reader, const char *schemeName)
{
    DE_ASSERT(reader != 0 && schemeName != 0);
    return Uri_NewWithPath2(Str_Text(Str_Appendf(AutoStr_NewStd(), "urn:%s:%i", schemeName, Reader_ReadInt16(reader))), RC_NULL);
}

static void readPlayer(player_t *pl, reader_s *reader)
{
    Reader_ReadInt32(reader);

    pl->playerState     = playerstate_t(Reader_ReadInt32(reader));

    Reader_Read(reader, NULL, 8);

    pl->viewZ           = FIX2FLT(Reader_ReadInt32(reader));
    pl->viewHeight      = FIX2FLT(Reader_ReadInt32(reader));
    pl->viewHeightDelta = FIX2FLT(Reader_ReadInt32(reader));
    pl->bob             = FLT2FIX(Reader_ReadInt32(reader));
    pl->flyHeight       = 0;
    pl->health          = Reader_ReadInt32(reader);
    pl->armorPoints     = Reader_ReadInt32(reader);
    pl->armorType       = Reader_ReadInt32(reader);

    de::zap(pl->powers);
    pl->powers[PT_INVULNERABILITY] = Reader_ReadInt32(reader);
    pl->powers[PT_STRENGTH]        = Reader_ReadInt32(reader);
    pl->powers[PT_INVISIBILITY]    = Reader_ReadInt32(reader);
    pl->powers[PT_IRONFEET]        = Reader_ReadInt32(reader);
    pl->powers[PT_ALLMAP]          = Reader_ReadInt32(reader);
    if(pl->powers[PT_ALLMAP])
    {
        ST_RevealAutomap(pl - players, true);
    }
    pl->powers[PT_INFRARED]        = Reader_ReadInt32(reader);

    de::zap(pl->keys);
    pl->keys[KT_BLUECARD]    = !!Reader_ReadInt32(reader);
    pl->keys[KT_YELLOWCARD]  = !!Reader_ReadInt32(reader);
    pl->keys[KT_REDCARD]     = !!Reader_ReadInt32(reader);
    pl->keys[KT_BLUESKULL]   = !!Reader_ReadInt32(reader);
    pl->keys[KT_YELLOWSKULL] = !!Reader_ReadInt32(reader);
    pl->keys[KT_REDSKULL]    = !!Reader_ReadInt32(reader);

    pl->backpack = Reader_ReadInt32(reader);

    de::zap(pl->frags);
    pl->frags[0] = Reader_ReadInt32(reader);
    pl->frags[1] = Reader_ReadInt32(reader);
    pl->frags[2] = Reader_ReadInt32(reader);
    pl->frags[3] = Reader_ReadInt32(reader);

    pl->readyWeapon   = weapontype_t(Reader_ReadInt32(reader));
    pl->pendingWeapon = weapontype_t(Reader_ReadInt32(reader));

    de::zap(pl->weapons);
    pl->weapons[WT_FIRST].owned   = !!Reader_ReadInt32(reader);
    pl->weapons[WT_SECOND].owned  = !!Reader_ReadInt32(reader);
    pl->weapons[WT_THIRD].owned   = !!Reader_ReadInt32(reader);
    pl->weapons[WT_FOURTH].owned  = !!Reader_ReadInt32(reader);
    pl->weapons[WT_FIFTH].owned   = !!Reader_ReadInt32(reader);
    pl->weapons[WT_SIXTH].owned   = !!Reader_ReadInt32(reader);
    pl->weapons[WT_SEVENTH].owned = !!Reader_ReadInt32(reader);
    pl->weapons[WT_EIGHTH].owned  = !!Reader_ReadInt32(reader);
    pl->weapons[WT_NINETH].owned  = !!Reader_ReadInt32(reader);

    de::zap(pl->ammo);
    pl->ammo[AT_CLIP].owned    = Reader_ReadInt32(reader);
    pl->ammo[AT_SHELL].owned   = Reader_ReadInt32(reader);
    pl->ammo[AT_CELL].owned    = Reader_ReadInt32(reader);
    pl->ammo[AT_MISSILE].owned = Reader_ReadInt32(reader);

    pl->ammo[AT_CLIP].max    = Reader_ReadInt32(reader);
    pl->ammo[AT_SHELL].max   = Reader_ReadInt32(reader);
    pl->ammo[AT_CELL].max    = Reader_ReadInt32(reader);
    pl->ammo[AT_MISSILE].max = Reader_ReadInt32(reader);

    pl->attackDown  = Reader_ReadInt32(reader);
    pl->useDown     = Reader_ReadInt32(reader);

    pl->cheats      = Reader_ReadInt32(reader);
    pl->refire      = Reader_ReadInt32(reader);

    pl->killCount   = Reader_ReadInt32(reader);
    pl->itemCount   = Reader_ReadInt32(reader);
    pl->secretCount = Reader_ReadInt32(reader);

    Reader_ReadInt32(reader);

    pl->damageCount = Reader_ReadInt32(reader);
    pl->bonusCount  = Reader_ReadInt32(reader);

    Reader_ReadInt32(reader);

    pl->plr->extraLight    = Reader_ReadInt32(reader);
    pl->plr->fixedColorMap = Reader_ReadInt32(reader);

    pl->colorMap = Reader_ReadInt32(reader);

    for(int i = 0; i < 2; ++i)
    {
        pspdef_t *psp = &pl->pSprites[i];

        psp->state   = INT2PTR(state_t, Reader_ReadInt32(reader));
        psp->tics    = Reader_ReadInt32(reader);
        psp->pos[VX] = FIX2FLT(Reader_ReadInt32(reader));
        psp->pos[VY] = FIX2FLT(Reader_ReadInt32(reader));
    }

    pl->didSecret  = !!Reader_ReadInt32(reader);
}

static void readMobj(reader_s *reader)
{
#define FF_FULLBRIGHT  0x8000 ///< Used to be a flag in thing->frame.
#define FF_FRAMEMASK   0x7fff

    // List: thinker links.
    Reader_ReadInt32(reader);
    Reader_ReadInt32(reader);
    Reader_ReadInt32(reader);

    // Info for drawing: position.
    coord_t pos[3];
    pos[VX] = FIX2FLT(Reader_ReadInt32(reader));
    pos[VY] = FIX2FLT(Reader_ReadInt32(reader));
    pos[VZ] = FIX2FLT(Reader_ReadInt32(reader));

    // More list: links in sector (if needed)
    Reader_ReadInt32(reader);
    Reader_ReadInt32(reader);

    //More drawing info: to determine current sprite.
    angle_t angle      = Reader_ReadInt32(reader);  // orientation
    spritenum_t sprite = Reader_ReadInt32(reader); // used to find patch_t and flip value

    int frame = Reader_ReadInt32(reader);  // might be ORed with FF_FULLBRIGHT
    if(frame & FF_FULLBRIGHT)
    {
        frame &= FF_FRAMEMASK; // not used anymore.
    }

    // Interaction info, by BLOCKMAP.
    // Links in blocks (if needed).
    Reader_ReadInt32(reader);
    Reader_ReadInt32(reader);
    Reader_ReadInt32(reader);

    // The closest interval over all contacted Sectors.
    coord_t floorz   = FIX2FLT(Reader_ReadInt32(reader));
    coord_t ceilingz = FIX2FLT(Reader_ReadInt32(reader));

    // For movement checking.
    coord_t radius = FIX2FLT(Reader_ReadInt32(reader));
    coord_t height = FIX2FLT(Reader_ReadInt32(reader));

    // Momentums, used to update position.
    coord_t mom[3];
    mom[MX] = FIX2FLT(Reader_ReadInt32(reader));
    mom[MY] = FIX2FLT(Reader_ReadInt32(reader));
    mom[MZ] = FIX2FLT(Reader_ReadInt32(reader));

    int valid = Reader_ReadInt32(reader);
    int type  = Reader_ReadInt32(reader);
    mobjinfo_t *info = &MOBJINFO[type];

    int ddflags = 0;
    if(info->flags  & MF_SOLID)     ddflags |= DDMF_SOLID;
    if(info->flags2 & MF2_DONTDRAW) ddflags |= DDMF_DONTDRAW;

    /*
     * We now have all the information we need to create the mobj.
     */
    mobj_t *mo = Mobj_CreateXYZ(P_MobjThinker, pos[VX], pos[VY], pos[VZ], angle,
                                radius, height, ddflags);

    mo->sprite       = sprite;
    mo->frame        = frame;
    mo->floorZ       = floorz;
    mo->ceilingZ     = ceilingz;
    mo->mom[MX]      = mom[MX];
    mo->mom[MY]      = mom[MY];
    mo->mom[MZ]      = mom[MZ];
    mo->valid        = valid;
    mo->type         = type;
    mo->moveDir      = DI_NODIR;

    Reader_ReadInt32(reader); // &mobjinfo[mo->type]

    mo->tics         = Reader_ReadInt32(reader); // state tic counter
    mo->state        = INT2PTR(state_t, Reader_ReadInt32(reader));
    mo->damage       = DDMAXINT; // Use damage set in mo->info->damage
    mo->flags        = Reader_ReadInt32(reader);
    mo->health       = Reader_ReadInt32(reader);

    // Movement direction, movement generation (zig-zagging).
    mo->moveDir      = Reader_ReadInt32(reader); // 0-7
    mo->moveCount    = Reader_ReadInt32(reader); // when 0, select a new dir

    // Thing being chased/attacked (or NULL),
    // also the originator for missiles.
    Reader_ReadInt32(reader);

    // Reaction time: if non 0, don't attack yet.
    // Used by player to freeze a bit after teleporting.
    mo->reactionTime = Reader_ReadInt32(reader);

    // If >0, the target will be chased
    // no matter what (even if shot)
    mo->threshold    = Reader_ReadInt32(reader);

    // Additional info record for player avatars only.
    // Only valid if type == MT_PLAYER
    mo->player       = INT2PTR(player_t, Reader_ReadInt32(reader));

    // Player number last looked for.
    mo->lastLook     = Reader_ReadInt32(reader);

    // For nightmare respawn.
    mo->spawnSpot.origin[VX] = (float) Reader_ReadInt16(reader);
    mo->spawnSpot.origin[VY] = (float) Reader_ReadInt16(reader);
    mo->spawnSpot.origin[VZ] = 0; // Initialize with "something".
    mo->spawnSpot.angle      = (angle_t) (ANG45 * ((int)Reader_ReadInt16(reader) / 45));
    /* mo->spawnSpot.type      = (int) */ Reader_ReadInt16(reader);

    int spawnFlags = ((int) Reader_ReadInt16(reader)) & ~MASK_UNKNOWN_MSF_FLAGS;
    // Spawn on the floor by default unless the mobjtype flags override.
    spawnFlags |= MSF_Z_FLOOR;
    mo->spawnSpot.flags      = spawnFlags;

    // Thing being chased/attacked for tracers.
    Reader_ReadInt32(reader);

    mo->info = info;
    SV_TranslateLegacyMobjFlags(mo, 0);

    mo->state  = &STATES[PTR2INT(mo->state)];
    mo->target = 0;
    if(mo->player)
    {
        int pnum = PTR2INT(mo->player) - 1;

        mo->player  = &players[pnum];
        mo->dPlayer = mo->player->plr;

        mo->dPlayer->mo      = mo;
        //mo->dPlayer->clAngle = mo->angle; /* $unifiedangles */
        mo->dPlayer->lookDir = 0; /* $unifiedangles */
    }
    P_MobjLink(mo);
    mo->floorZ   = P_GetDoublep(Mobj_Sector(mo), DMU_FLOOR_HEIGHT);
    mo->ceilingZ = P_GetDoublep(Mobj_Sector(mo), DMU_CEILING_HEIGHT);

#undef FF_FRAMEMASK
#undef FF_FULLBRIGHT
}

static int removeThinker(thinker_t *th, void * /*context*/)
{
    if(th->function == (thinkfunc_t) P_MobjThinker)
    {
        P_MobjRemove((mobj_t *) th, true);
    }
    else
    {
        Z_Free(th);
    }

    return false; // Continue iteration.
}

static int readCeiling(ceiling_t *ceiling, reader_s *reader)
{
/* Original DOOM format:
typedef struct {
    thinker_t thinker; // was 12 bytes
    ceilingtype_e type; // was 32bit int
    Sector *sector;
    fixed_t bottomheight;
    fixed_t topheight;
    fixed_t speed;
    dd_bool crush;
    int     direction;
    int     tag;
    int     olddirection;
} v19_ceiling_t;
*/
    byte temp[SIZEOF_V19_THINKER_T];

    // Padding at the start (an old thinker_t struct).
    Reader_Read(reader, &temp, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    ceiling->type         = ceilingtype_e(Reader_ReadInt32(reader));

    // A 32bit pointer to sector, serialized.
    ceiling->sector       = (Sector *)P_ToPtr(DMU_SECTOR, Reader_ReadInt32(reader));
    DE_ASSERT(ceiling->sector != 0);

    ceiling->bottomHeight = FIX2FLT(Reader_ReadInt32(reader));
    ceiling->topHeight    = FIX2FLT(Reader_ReadInt32(reader));
    ceiling->speed        = FIX2FLT(Reader_ReadInt32(reader));
    ceiling->crush        = Reader_ReadInt32(reader);
    ceiling->state        = (Reader_ReadInt32(reader) == -1? CS_DOWN : CS_UP);
    ceiling->tag          = Reader_ReadInt32(reader);
    ceiling->oldState     = (Reader_ReadInt32(reader) == -1? CS_DOWN : CS_UP);

    ceiling->thinker.function = T_MoveCeiling;
    if(!(temp + V19_THINKER_T_FUNC_OFFSET))
    {
        Thinker_SetStasis(&ceiling->thinker, true);
    }

    P_ToXSector(ceiling->sector)->specialData = ceiling;
    return true; // Add this thinker.
}

static int readDoor(door_t *door, reader_s *reader)
{
/* Original DOOM format:
typedef struct {
    thinker_t thinker; // was 12 bytes
    doortype_e type; // was 32bit int
    Sector *sector;
    fixed_t topheight;
    fixed_t speed;
    int     direction;
    int     topwait;
    int     topcountdown;
} v19_vldoor_t;
*/
    // Padding at the start (an old thinker_t struct)
    Reader_Read(reader, NULL, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    door->type         = doortype_e(Reader_ReadInt32(reader));

    // A 32bit pointer to sector, serialized.
    door->sector       = (Sector *)P_ToPtr(DMU_SECTOR, Reader_ReadInt32(reader));
    DE_ASSERT(door->sector != 0);

    door->topHeight    = FIX2FLT(Reader_ReadInt32(reader));
    door->speed        = FIX2FLT(Reader_ReadInt32(reader));
    door->state        = doorstate_e(Reader_ReadInt32(reader));
    door->topWait      = Reader_ReadInt32(reader);
    door->topCountDown = Reader_ReadInt32(reader);

    door->thinker.function = T_Door;

    P_ToXSector(door->sector)->specialData = door;
    return true; // Add this thinker.
}

static int readFloor(floor_t *floor, reader_s *reader)
{
/* Original DOOM format:
typedef struct {
    thinker_t thinker; // was 12 bytes
    floortype_e type; // was 32bit int
    dd_bool crush;
    Sector *sector;
    int     direction;
    int     newspecial;
    short   texture;
    fixed_t floordestheight;
    fixed_t speed;
} v19_floormove_t;
*/

    // Padding at the start (an old thinker_t struct)
    Reader_Read(reader, NULL, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    floor->type            = floortype_e(Reader_ReadInt32(reader));
    floor->crush           = Reader_ReadInt32(reader);

    // A 32bit pointer to sector, serialized.
    floor->sector          = (Sector *)P_ToPtr(DMU_SECTOR, Reader_ReadInt32(reader));
    DE_ASSERT(floor->sector != 0);

    floor->state           = floorstate_e(Reader_ReadInt32(reader));
    floor->newSpecial      = Reader_ReadInt32(reader);

    uri_s *newTextureUrn = readTextureUrn(reader, "Flats");
    floor->material        = DD_MaterialForTextureUri(newTextureUrn);
    Uri_Delete(newTextureUrn);

    floor->floorDestHeight = FIX2FLT(Reader_ReadInt32(reader));
    floor->speed           = FIX2FLT(Reader_ReadInt32(reader));

    floor->thinker.function = T_MoveFloor;

    P_ToXSector(floor->sector)->specialData = floor;
    return true; // Add this thinker.
}

static int readPlat(plat_t *plat, reader_s *reader)
{
/* Original DOOM format:
typedef struct {
    thinker_t thinker; // was 12 bytes
    Sector *sector;
    fixed_t speed;
    fixed_t low;
    fixed_t high;
    int     wait;
    int     count;
    platstate_e  status; // was 32bit int
    platstate_e  oldstatus; // was 32bit int
    dd_bool crush;
    int     tag;
    plattype_e type; // was 32bit int
} v19_plat_t;
*/
    byte temp[SIZEOF_V19_THINKER_T];

    // Padding at the start (an old thinker_t struct)
    Reader_Read(reader, temp, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    // A 32bit pointer to sector, serialized.
    plat->sector   = (Sector *)P_ToPtr(DMU_SECTOR, Reader_ReadInt32(reader));
    DE_ASSERT(plat->sector != 0);

    plat->speed    = FIX2FLT(Reader_ReadInt32(reader));
    plat->low      = FIX2FLT(Reader_ReadInt32(reader));
    plat->high     = FIX2FLT(Reader_ReadInt32(reader));
    plat->wait     = Reader_ReadInt32(reader);
    plat->count    = Reader_ReadInt32(reader);
    plat->state    = platstate_e(Reader_ReadInt32(reader));
    plat->oldState = platstate_e(Reader_ReadInt32(reader));
    plat->crush    = Reader_ReadInt32(reader);
    plat->tag      = Reader_ReadInt32(reader);
    plat->type     = plattype_e(Reader_ReadInt32(reader));

    plat->thinker.function = T_PlatRaise;
    if(!(temp + V19_THINKER_T_FUNC_OFFSET))
    {
        Thinker_SetStasis(&plat->thinker, true);
    }

    P_ToXSector(plat->sector)->specialData = plat;
    return true; // Add this thinker.
}

static int readFlash(lightflash_t *flash, reader_s *reader)
{
/* Original DOOM format:
typedef struct {
    thinker_t thinker; // was 12 bytes
    Sector *sector;
    int     count;
    int     maxlight;
    int     minlight;
    int     maxtime;
    int     mintime;
} v19_lightflash_t;
*/
    // Padding at the start (an old thinker_t struct)
    Reader_Read(reader, NULL, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    // A 32bit pointer to sector, serialized.
    flash->sector   = (Sector *)P_ToPtr(DMU_SECTOR, Reader_ReadInt32(reader));
    DE_ASSERT(flash->sector != 0);

    flash->count    = Reader_ReadInt32(reader);
    flash->maxLight = (float) Reader_ReadInt32(reader) / 255.0f;
    flash->minLight = (float) Reader_ReadInt32(reader) / 255.0f;
    flash->maxTime  = Reader_ReadInt32(reader);
    flash->minTime  = Reader_ReadInt32(reader);

    flash->thinker.function = (thinkfunc_t) T_LightFlash;
    return true; // Add this thinker.
}

static int readStrobe(strobe_t *strobe, reader_s *reader)
{
/* Original DOOM format:
typedef struct {
    thinker_t thinker; // was 12 bytes
    Sector *sector;
    int     count;
    int     minlight;
    int     maxlight;
    int     darktime;
    int     brighttime;
} v19_strobe_t;
*/
    // Padding at the start (an old thinker_t struct)
    Reader_Read(reader, NULL, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    // A 32bit pointer to sector, serialized.
    strobe->sector     = (Sector *)P_ToPtr(DMU_SECTOR, Reader_ReadInt32(reader));
    DE_ASSERT(strobe->sector != 0);

    strobe->count      = Reader_ReadInt32(reader);
    strobe->minLight   = (float) Reader_ReadInt32(reader) / 255.0f;
    strobe->maxLight   = (float) Reader_ReadInt32(reader) / 255.0f;
    strobe->darkTime   = Reader_ReadInt32(reader);
    strobe->brightTime = Reader_ReadInt32(reader);

    strobe->thinker.function = (thinkfunc_t) T_StrobeFlash;
    return true; // Add this thinker.
}

static int readGlow(glow_t *glow, reader_s *reader)
{
/* Original DOOM format:
typedef struct {
    thinker_t thinker; // was 12 bytes
    Sector *sector;
    int     minlight;
    int     maxlight;
    int     direction;
} v19_glow_t;
*/
    // Padding at the start (an old thinker_t struct)
    Reader_Read(reader, NULL, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    // A 32bit pointer to sector, serialized.
    glow->sector    = (Sector *)P_ToPtr(DMU_SECTOR, Reader_ReadInt32(reader));
    DE_ASSERT(glow->sector != 0);

    glow->minLight  = (float) Reader_ReadInt32(reader) / 255.0f;
    glow->maxLight  = (float) Reader_ReadInt32(reader) / 255.0f;
    glow->direction = Reader_ReadInt32(reader);

    glow->thinker.function = (thinkfunc_t) T_Glow;
    return true; // Add this thinker.
}

#if 0
static bool SV_OpenFile_Dm_v19(Path path)
{
    DE_ASSERT(saveBuffer == 0);
    if(!M_ReadFile(NativePath(path).expand(), (char **)&saveBuffer))
    {
        return false;
    }
    savePtr = saveBuffer;
    return true;
}

static void SV_CloseFile_Dm_v19()
{
    if(!saveBuffer) return;
    Z_Free(saveBuffer);
    saveBuffer = savePtr = 0;
}
#endif

DE_PIMPL(DoomV9MapStateReader)
{
    reader_s *reader;

    Impl(Public *i)
        : Base(i)
        , reader(0)
    {}

    ~Impl()
    {
        Reader_Delete(reader);
    }

    void readPlayers()
    {
        for(int i = 0; i < 4; ++i)
        {
            if(!players[i].plr->inGame) continue;

            PADSAVEP();

            readPlayer(players + i, reader);

            // will be set when unarc thinker
            players[i].plr->mo  = 0;
            players[i].attacker = 0;

            for(int k = 0; k < NUMPSPRITES; ++k)
            {
                if(players[i].pSprites[k].state)
                {
                    players[i].pSprites[k].state =
                        &STATES[PTR2INT(players[i].pSprites[k].state)];
                }
            }
        }
    }

    void readThinkers()
    {
        // Remove all the current thinkers.
        Thinker_Iterate(NULL, removeThinker, NULL);
        Thinker_Init();

        // Read in saved thinkers.
        byte tClass;
        while((tClass = Reader_ReadByte(reader)) != 0 /*TC_END*/)
        {
            switch(tClass)
            {
            case 1: //TC_MOBJ
                PADSAVEP();
                readMobj(reader);
                break;

            default:
                throw ReadError("DoomV9MapStateReader",
                                "Unknown tclass #" + String::asText(tClass) + "in savegame");
            }
        }
    }

    enum {
        tc_ceiling,
        tc_door,
        tc_floor,
        tc_plat,
        tc_flash,
        tc_strobe,
        tc_glow,
        tc_endspecials
    };

    /*
     * Things to handle:
     *
     * T_MoveCeiling, (ceiling_t: Sector * swizzle), - active list
     * T_Door, (door_t: Sector * swizzle),
     * T_MoveFloor, (floor_t: Sector * swizzle),
     * T_LightFlash, (lightflash_t: Sector * swizzle),
     * T_StrobeFlash, (strobe_t: Sector *),
     * T_Glow, (glow_t: Sector *),
     * T_PlatRaise, (plat_t: Sector *), - active list
     */
    void readSpecials()
    {
        byte tClass;
        while((tClass = Reader_ReadByte(reader)) != tc_endspecials)
        {
            switch(tClass)
            {
            case tc_ceiling: {
                PADSAVEP();
                ceiling_t *ceiling = (ceiling_t *)Z_Calloc(sizeof(*ceiling), PU_MAP, NULL);

                readCeiling(ceiling, reader);

                Thinker_Add(&ceiling->thinker);
                break; }

            case tc_door: {
                PADSAVEP();
                door_t *door = (door_t *)Z_Calloc(sizeof(*door), PU_MAP, NULL);

                readDoor(door, reader);

                Thinker_Add(&door->thinker);
                break; }

            case tc_floor: {
                PADSAVEP();
                floor_t *floor = (floor_t *)Z_Calloc(sizeof(*floor), PU_MAP, NULL);

                readFloor(floor, reader);

                Thinker_Add(&floor->thinker);
                break; }

            case tc_plat: {
                PADSAVEP();
                plat_t *plat = (plat_t *)Z_Calloc(sizeof(*plat), PU_MAP, NULL);

                readPlat(plat, reader);

                Thinker_Add(&plat->thinker);
                break; }

            case tc_flash: {
                PADSAVEP();
                lightflash_t *flash = (lightflash_t *)Z_Calloc(sizeof(*flash), PU_MAP, NULL);

                readFlash(flash, reader);

                Thinker_Add(&flash->thinker);
                break; }

            case tc_strobe: {
                PADSAVEP();
                strobe_t *strobe = (strobe_t *)Z_Calloc(sizeof(*strobe), PU_MAP, NULL);

                readStrobe(strobe, reader);

                Thinker_Add(&strobe->thinker);
                break; }

            case tc_glow: {
                PADSAVEP();
                glow_t *glow = (glow_t *)Z_Calloc(sizeof(*glow), PU_MAP, NULL);

                readGlow(glow, reader);

                Thinker_Add(&glow->thinker);
                break; }

            default:
                throw ReadError("DoomV9MapStateReader",
                                "Unknown tclass #" + String::asText(tClass) + "in savegame");
            }
        }
    }
};

DoomV9MapStateReader::DoomV9MapStateReader(const GameStateFolder &session)
    : GameStateFolder::MapStateReader(session)
    , d(new Impl(this))
{}

DoomV9MapStateReader::~DoomV9MapStateReader()
{}

void DoomV9MapStateReader::read(const String & /*mapUriStr*/)
{
    d->reader = SV_NewReader_Dm_v19();

    d->readPlayers();

    // Do sectors.
    for(int i = 0; i < numsectors; ++i)
    {
        Sector *sec     = (Sector *)P_ToPtr(DMU_SECTOR, i);
        xsector_t *xsec = P_ToXSector(sec);

        P_SetDoublep(sec, DMU_FLOOR_HEIGHT,   (coord_t) Reader_ReadInt16(d->reader));
        P_SetDoublep(sec, DMU_CEILING_HEIGHT, (coord_t) Reader_ReadInt16(d->reader));

        uri_s *floorTextureUrn = readTextureUrn(d->reader, "Flats");
        P_SetPtrp   (sec, DMU_FLOOR_MATERIAL,   DD_MaterialForTextureUri(floorTextureUrn));
        Uri_Delete(floorTextureUrn);

        uri_s *ceilingTextureUrn = readTextureUrn(d->reader, "Flats");
        P_SetPtrp   (sec, DMU_CEILING_MATERIAL, DD_MaterialForTextureUri(ceilingTextureUrn));
        Uri_Delete(ceilingTextureUrn);

        P_SetFloatp(sec, DMU_LIGHT_LEVEL, (float) (Reader_ReadInt16(d->reader)) / 255.0f);
        xsec->special = Reader_ReadInt16(d->reader); // needed?
        /*xsec->tag = */Reader_ReadInt16(d->reader); // needed?
        xsec->specialData = 0;
        xsec->soundTarget = 0;
    }

    // Do lines.
    for(int i = 0; i < numlines; ++i)
    {
        Line *line     = (Line *)P_ToPtr(DMU_LINE, i);
        xline_t *xline = P_ToXLine(line);

        xline->flags   = Reader_ReadInt16(d->reader);
        xline->special = Reader_ReadInt16(d->reader);
        /*xline->tag    =*/Reader_ReadInt16(d->reader);

        for(int k = 0; k < 2; ++k)
        {
            Side *sdef = (Side *)P_GetPtrp(line, (k? DMU_BACK:DMU_FRONT));
            if(!sdef) continue;

            float matOffset[2];
            matOffset[VX] = (float) (Reader_ReadInt16(d->reader));
            matOffset[VY] = (float) (Reader_ReadInt16(d->reader));
            P_SetFloatpv(sdef, DMU_TOP_MATERIAL_OFFSET_XY,    matOffset);
            P_SetFloatpv(sdef, DMU_MIDDLE_MATERIAL_OFFSET_XY, matOffset);
            P_SetFloatpv(sdef, DMU_BOTTOM_MATERIAL_OFFSET_XY, matOffset);

            uri_s *topTextureUrn = readTextureUrn(d->reader, "Textures");
            P_SetPtrp(sdef, DMU_TOP_MATERIAL,    DD_MaterialForTextureUri(topTextureUrn));
            Uri_Delete(topTextureUrn);

            uri_s *bottomTextureUrn = readTextureUrn(d->reader, "Textures");
            P_SetPtrp(sdef, DMU_BOTTOM_MATERIAL, DD_MaterialForTextureUri(bottomTextureUrn));
            Uri_Delete(bottomTextureUrn);

            uri_s *middleTextureUrn = readTextureUrn(d->reader, "Textures");
            P_SetPtrp(sdef, DMU_MIDDLE_MATERIAL, DD_MaterialForTextureUri(middleTextureUrn));
            Uri_Delete(middleTextureUrn);
        }
    }

    d->readThinkers();
    d->readSpecials();

    const byte consistency = Reader_ReadByte(d->reader);
    Reader_Delete(d->reader); d->reader = 0;

    if(consistency != 0x1d)
    {
        throw ReadError("DoomV9MapStateReader", "Bad savegame (consistency test failed!)");
    }

    // Material scrollers must be spawned.
    P_SpawnAllMaterialOriginScrollers();

    // Let the engine know where the local players are now.
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        R_UpdateConsoleView(i);
    }

    // Inform the engine that map setup must be performed once more.
    R_SetupMap(0, 0);
}

thinker_t *DoomV9MapStateReader::thinkerForPrivateId(Id::Type) const
{
    // Private identifiers not supported.
    return nullptr;
}
