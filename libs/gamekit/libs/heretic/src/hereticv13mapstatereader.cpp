/** @file hereticv13mapstatereader.cpp  Heretic ver 1.3 save game reader.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include "jheretic.h"
#include "hereticv13mapstatereader.h"

#include <cstdio>
#include <cstring>
#include <de/arrayvalue.h>
#include <de/nativepath.h>
#include <de/numbervalue.h>
#include "dmu_lib.h"
#include "hu_inventory.h"
#include "p_ceiling.h"
#include "p_door.h"
#include "p_floor.h"
#include "p_inventory.h"
#include "p_map.h"
#include "p_mapsetup.h"
#include "p_plat.h"
#include "p_saveio.h"
#include "p_saveg.h"
#include "p_tick.h"
#include "r_common.h"       // R_UpdateConsoleView

#define SIZEOF_V13_THINKER_T            12
#define V13_THINKER_T_FUNC_OFFSET       8

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
        std::memcpy(data, savePtr, len);
    }
    savePtr += len;
}

static reader_s *SV_NewReader_Hr_v13()
{
    if(!saveBuffer) return 0;
    return Reader_NewWithCallbacks(sri8, sri16, sri32, NULL, srd);
}

static uri_s *readTextureUrn(reader_s *reader, const char *schemeName)
{
    DE_ASSERT(reader != 0 && schemeName != 0);
    return Uri_NewWithPath2(Str_Text(Str_Appendf(AutoStr_NewStd(), "urn:%s:%i", schemeName, Reader_ReadInt16(reader))), RC_NULL);
}

static void readPlayer(player_t *pl, reader_s *reader)
{
    int plrnum = pl - players;
    ddplayer_t *ddpl = pl->plr;

    Reader_ReadInt32(reader); // mo

    pl->playerState = playerstate_t(Reader_ReadInt32(reader));
    DE_ASSERT(pl->playerState >= PST_LIVE && pl->playerState <= PST_REBORN);

    byte junk[12];
    Reader_Read(reader, junk, 10); // ticcmd_t

    pl->viewZ       = FIX2FLT(Reader_ReadInt32(reader));
    pl->viewHeight  = FIX2FLT(Reader_ReadInt32(reader));
    pl->viewHeightDelta = FIX2FLT(Reader_ReadInt32(reader));
    pl->bob         = FIX2FLT(Reader_ReadInt32(reader));
    pl->flyHeight   = Reader_ReadInt32(reader);
    ddpl->lookDir   = Reader_ReadInt32(reader);
    pl->centering   = Reader_ReadInt32(reader);
    pl->health      = Reader_ReadInt32(reader);
    pl->armorPoints = Reader_ReadInt32(reader);
    pl->armorType   = Reader_ReadInt32(reader);

    P_InventoryEmpty(plrnum);
    for(int i = 0; i < 14; ++i)
    {
        inventoryitemtype_t type = inventoryitemtype_t(Reader_ReadInt32(reader));
        DE_ASSERT(type >= IIT_NONE && type < NUM_INVENTORYITEM_TYPES);
        int count = Reader_ReadInt32(reader);

        for(int k = 0; k < count; ++k)
        {
            P_InventoryGive(plrnum, type, true);
        }
    }

    P_InventorySetReadyItem(plrnum, inventoryitemtype_t(Reader_ReadInt32(reader)));
    Hu_InventorySelect(plrnum, P_InventoryReadyItem(plrnum));
    /*pl->artifactCount    =*/ Reader_ReadInt32(reader);
    /*pl->inventorySlotNum =*/ Reader_ReadInt32(reader);

    memset(pl->powers, 0, sizeof(pl->powers));
    /*pl->powers[pw_NONE]            =*/ Reader_ReadInt32(reader);
    pl->powers[PT_INVULNERABILITY] = Reader_ReadInt32(reader);
    pl->powers[PT_INVISIBILITY]    = Reader_ReadInt32(reader);
    pl->powers[PT_ALLMAP]          = Reader_ReadInt32(reader);
    if(pl->powers[PT_ALLMAP])
    {
        ST_RevealAutomap(pl - players, true);
    }
    pl->powers[PT_INFRARED]     = Reader_ReadInt32(reader);
    pl->powers[PT_WEAPONLEVEL2] = Reader_ReadInt32(reader);
    pl->powers[PT_FLIGHT]       = Reader_ReadInt32(reader);
    pl->powers[PT_SHIELD]       = Reader_ReadInt32(reader);
    pl->powers[PT_HEALTH2]      = Reader_ReadInt32(reader);

    memset(pl->keys, 0, sizeof(pl->keys));
    pl->keys[KT_YELLOW] = !!Reader_ReadInt32(reader);
    pl->keys[KT_GREEN]  = !!Reader_ReadInt32(reader);
    pl->keys[KT_BLUE]   = !!Reader_ReadInt32(reader);

    pl->backpack = Reader_ReadInt32(reader);

    memset(pl->frags, 0, sizeof(pl->frags));
    pl->frags[0] = Reader_ReadInt32(reader);
    pl->frags[1] = Reader_ReadInt32(reader);
    pl->frags[2] = Reader_ReadInt32(reader);
    pl->frags[3] = Reader_ReadInt32(reader);

    int readyWeapon = Reader_ReadInt32(reader);
    pl->readyWeapon = readyWeapon == 10? WT_NOCHANGE : weapontype_t(readyWeapon);

    int pendingWeapon = Reader_ReadInt32(reader);
    pl->pendingWeapon = pendingWeapon == 10? WT_NOCHANGE : weapontype_t(pendingWeapon);

    // Owned weapons.
    memset(pl->weapons, 0, sizeof(pl->weapons));
    pl->weapons[WT_FIRST  ].owned = !!Reader_ReadInt32(reader);
    pl->weapons[WT_SECOND ].owned = !!Reader_ReadInt32(reader);
    pl->weapons[WT_THIRD  ].owned = !!Reader_ReadInt32(reader);
    pl->weapons[WT_FOURTH ].owned = !!Reader_ReadInt32(reader);
    pl->weapons[WT_FIFTH  ].owned = !!Reader_ReadInt32(reader);
    pl->weapons[WT_SIXTH  ].owned = !!Reader_ReadInt32(reader);
    pl->weapons[WT_SEVENTH].owned = !!Reader_ReadInt32(reader);
    pl->weapons[WT_EIGHTH ].owned = !!Reader_ReadInt32(reader);
    /*pl->weapons[wp_beak   ].owned = !!*/ Reader_ReadInt32(reader);

    memset(pl->ammo, 0, sizeof(pl->ammo));
    pl->ammo[AT_CRYSTAL].owned = Reader_ReadInt32(reader);
    pl->ammo[AT_ARROW  ].owned = Reader_ReadInt32(reader);
    pl->ammo[AT_ORB    ].owned = Reader_ReadInt32(reader);
    pl->ammo[AT_RUNE   ].owned = Reader_ReadInt32(reader);
    pl->ammo[AT_FIREORB].owned = Reader_ReadInt32(reader);
    pl->ammo[AT_MSPHERE].owned = Reader_ReadInt32(reader);
    pl->ammo[AT_CRYSTAL].max = Reader_ReadInt32(reader);
    pl->ammo[AT_ARROW  ].max = Reader_ReadInt32(reader);
    pl->ammo[AT_ORB    ].max = Reader_ReadInt32(reader);
    pl->ammo[AT_RUNE   ].max = Reader_ReadInt32(reader);
    pl->ammo[AT_FIREORB].max = Reader_ReadInt32(reader);
    pl->ammo[AT_MSPHERE].max = Reader_ReadInt32(reader);

    pl->attackDown  = Reader_ReadInt32(reader);
    pl->useDown     = Reader_ReadInt32(reader);
    pl->cheats      = Reader_ReadInt32(reader);
    pl->refire      = Reader_ReadInt32(reader);
    pl->killCount   = Reader_ReadInt32(reader);
    pl->itemCount   = Reader_ReadInt32(reader);
    pl->secretCount = Reader_ReadInt32(reader);
    /*pl->message     =*/ Reader_ReadInt32(reader); // char*
    /*pl->messageTics =*/ Reader_ReadInt32(reader); // int32
    pl->damageCount = Reader_ReadInt32(reader);
    pl->bonusCount  = Reader_ReadInt32(reader);
    pl->flameCount  = Reader_ReadInt32(reader);
    /*pl->attacker    =*/ Reader_ReadInt32(reader); // mobj_t*

    ddpl->extraLight    = Reader_ReadInt32(reader);
    ddpl->fixedColorMap = Reader_ReadInt32(reader);

    pl->colorMap    = Reader_ReadInt32(reader);

    for(int i = 0; i < 2; ++i)
    {
        pspdef_t *psp = &pl->pSprites[i];

        psp->state   = INT2PTR(state_t, Reader_ReadInt32(reader));
        psp->tics    = Reader_ReadInt32(reader);
        psp->pos[VX] = FIX2FLT(Reader_ReadInt32(reader));
        psp->pos[VY] = FIX2FLT(Reader_ReadInt32(reader));
    }

    pl->didSecret   = !!Reader_ReadInt32(reader);
    pl->morphTics   = Reader_ReadInt32(reader);
    pl->chickenPeck = Reader_ReadInt32(reader);
    /*pl->rain1       =*/ Reader_ReadInt32(reader); // mobj_t*
    /*pl->rain2       =*/ Reader_ReadInt32(reader); // mobj_t*
}

static void readMobj(reader_s *reader)
{
#define FF_FRAMEMASK 0x7fff

    // The thinker was 3 ints long.
    Reader_ReadInt32(reader);
    Reader_ReadInt32(reader);
    Reader_ReadInt32(reader);

    coord_t pos[3];
    pos[VX] = FIX2FLT(Reader_ReadInt32(reader));
    pos[VY] = FIX2FLT(Reader_ReadInt32(reader));
    pos[VZ] = FIX2FLT(Reader_ReadInt32(reader));

    // Sector links.
    Reader_ReadInt32(reader);
    Reader_ReadInt32(reader);

    angle_t angle = (angle_t) (ANG45 * (Reader_ReadInt32(reader) / 45));
    spritenum_t sprite = Reader_ReadInt32(reader);

    int frame = Reader_ReadInt32(reader);
    frame &= ~FF_FRAMEMASK; // not used anymore.

    // Block links.
    Reader_ReadInt32(reader);
    Reader_ReadInt32(reader);

    // BSP leaf.
    Reader_ReadInt32(reader);

    coord_t floorz   = FIX2FLT(Reader_ReadInt32(reader));
    coord_t ceilingz = FIX2FLT(Reader_ReadInt32(reader));
    coord_t radius   = FIX2FLT(Reader_ReadInt32(reader));
    coord_t height   = FIX2FLT(Reader_ReadInt32(reader));

    coord_t mom[3];
    mom[MX]  = FIX2FLT(Reader_ReadInt32(reader));
    mom[MY]  = FIX2FLT(Reader_ReadInt32(reader));
    mom[MZ]  = FIX2FLT(Reader_ReadInt32(reader));

    int valid = Reader_ReadInt32(reader);
    int type  = Reader_ReadInt32(reader);

    DE_ASSERT(type >= 0 && type < Get(DD_NUMMOBJTYPES));
    mobjinfo_t *info = &MOBJINFO[type];

    int ddflags = 0;
    if(info->flags & MF_SOLID)      ddflags |= DDMF_SOLID;
    if(info->flags2 & MF2_DONTDRAW) ddflags |= DDMF_DONTDRAW;

    /*
     * We now have all the information we need to create the mobj.
     */
    mobj_t *mo = Mobj_CreateXYZ(P_MobjThinker, pos[VX], pos[VY], pos[VZ], angle,
                                radius, height, ddflags);

    mo->sprite   = sprite;
    mo->frame    = frame;
    mo->floorZ   = floorz;
    mo->ceilingZ = ceilingz;
    mo->mom[MX]  = mom[MX];
    mo->mom[MY]  = mom[MY];
    mo->mom[MZ]  = mom[MZ];
    mo->valid    = valid;
    mo->type     = type;
    mo->moveDir  = DI_NODIR;

    /*
     * Continue reading the mobj data.
     */
    Reader_ReadInt32(reader); // info

    mo->tics     = Reader_ReadInt32(reader);
    mo->state    = INT2PTR(state_t, Reader_ReadInt32(reader));
    mo->damage   = Reader_ReadInt32(reader);
    mo->flags    = Reader_ReadInt32(reader);
    mo->flags2   = Reader_ReadInt32(reader);
    mo->special1 = Reader_ReadInt32(reader);
    mo->special2 = Reader_ReadInt32(reader);
    mo->health   = Reader_ReadInt32(reader);

    // Fix a bunch of kludges in the original Heretic.
    switch(mo->type)
    {
    case MT_MACEFX1:
    case MT_MACEFX2:
    case MT_MACEFX3:
    case MT_HORNRODFX2:
    case MT_HEADFX3:
    case MT_WHIRLWIND:
    case MT_TELEGLITTER:
    case MT_TELEGLITTER2:
        mo->special3 = mo->health;
        mo->health = info->spawnHealth;
        break;

    default: break;
    }

    mo->moveDir      = Reader_ReadInt32(reader);
    mo->moveCount    = Reader_ReadInt32(reader);
    Reader_ReadInt32(reader); // target
    mo->reactionTime = Reader_ReadInt32(reader);
    mo->threshold    = Reader_ReadInt32(reader);
    mo->player       = INT2PTR(player_t, Reader_ReadInt32(reader));
    mo->lastLook     = Reader_ReadInt32(reader);

    mo->spawnSpot.origin[VX] = (coord_t) Reader_ReadInt16(reader);
    mo->spawnSpot.origin[VY] = (coord_t) Reader_ReadInt16(reader);
    mo->spawnSpot.origin[VZ] = 0; // Initialize with "something".
    mo->spawnSpot.angle      = (angle_t) (ANG45 * (Reader_ReadInt16(reader) / 45));
    /*mo->spawnSpot.type       = (int)*/ Reader_ReadInt16(reader);

    int spawnFlags = ((int) Reader_ReadInt16(reader)) & ~MASK_UNKNOWN_MSF_FLAGS;
    // Spawn on the floor by default unless the mobjtype flags override.
    spawnFlags |= MSF_Z_FLOOR;
    mo->spawnSpot.flags = spawnFlags;

    mo->info = info;
    SV_TranslateLegacyMobjFlags(mo, 0);

    mo->state = &STATES[PTR2INT(mo->state)];
    mo->target = NULL;
    if(mo->player)
    {
        mo->player = &players[PTR2INT(mo->player) - 1];
        mo->player->plr->mo = mo;
        mo->player->plr->mo->dPlayer = mo->player->plr;
    }
    P_MobjLink(mo);
    mo->floorZ   = P_GetDoublep(Mobj_Sector(mo), DMU_FLOOR_HEIGHT);
    mo->ceilingZ = P_GetDoublep(Mobj_Sector(mo), DMU_CEILING_HEIGHT);

#undef FF_FRAMEMASK
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
/* Original Heretic format:
typedef struct {
    thinker_t thinker; ///< 12 bytes
    ceilingtype_e type; ///< 32bit int
    Sector* sector;
    fixed_t bottomheight, topheight;
    fixed_t speed;
    dd_bool crush;
    int direction; /// 1= up, 0= waiting, -1= down
    int tag;
    int olddirection;
} v13_ceiling_t;
*/

    // Padding at the start (an old thinker_t struct)
    byte temp[SIZEOF_V13_THINKER_T];
    Reader_Read(reader, &temp, SIZEOF_V13_THINKER_T);

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
    if(!(temp + V13_THINKER_T_FUNC_OFFSET))
        Thinker_SetStasis(&ceiling->thinker, true);

    P_ToXSector(ceiling->sector)->specialData = ceiling;
    return true; // Add this thinker.
}

static int readDoor(door_t *door, reader_s *reader)
{
/* Original Heretic format:
typedef struct {
    thinker_t   thinker;        // was 12 bytes
    doortype_e  type;           // was 32bit int
    Sector     *sector;
    fixed_t     topheight;
    fixed_t     speed;
    int         direction;      // 1 = up, 0 = waiting at top, -1 = down
    int         topwait;        // tics to wait at the top
                                // (keep in case a door going down is reset)
    int         topcountdown;   // when it reaches 0, start going down
} v13_vldoor_t;
*/
    // Padding at the start (an old thinker_t struct)
    Reader_Read(reader, NULL, SIZEOF_V13_THINKER_T);

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
/* Original Heretic format:
typedef struct {
    thinker_t   thinker;        // was 12 bytes
    floortype_e type;           // was 32bit int
    dd_bool     crush;
    Sector     *sector;
    int         direction;
    int         newspecial;
    short       texture;
    fixed_t     floordestheight;
    fixed_t     speed;
} v13_floormove_t;
*/

    // Padding at the start (an old thinker_t struct)
    Reader_Read(reader, NULL, SIZEOF_V13_THINKER_T);

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
/* Original Heretic format:
typedef struct {
    thinker_t   thinker; // was 12 bytes
    Sector     *sector;
    fixed_t     speed;
    fixed_t     low;
    fixed_t     high;
    int         wait;
    int         count;
    platstate_e status; // was 32bit int
    platstate_e oldStatus; // was 32bit int
    dd_bool     crush;
    int         tag;
    plattype_e  type; // was 32bit int
} v13_plat_t;
*/
    byte temp[SIZEOF_V13_THINKER_T];
    // Padding at the start (an old thinker_t struct)
    Reader_Read(reader, &temp, SIZEOF_V13_THINKER_T);

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
    if(!(temp + V13_THINKER_T_FUNC_OFFSET))
        Thinker_SetStasis(&plat->thinker, true);

    P_ToXSector(plat->sector)->specialData = plat;
    return true; // Add this thinker.
}

static int readFlash(lightflash_t *flash, reader_s *reader)
{
/* Original Heretic format:
typedef struct {
    thinker_t   thinker; // was 12 bytes
    Sector     *sector;
    int         count;
    int         maxLight;
    int         minLight;
    int         maxTime;
    int         minTime;
} v13_lightflash_t;
*/
    // Padding at the start (an old thinker_t struct)
    Reader_Read(reader, NULL, SIZEOF_V13_THINKER_T);

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
/* Original Heretic format:
typedef struct {
    thinker_t   thinker; // was 12 bytes
    Sector     *sector;
    int         count;
    int         minLight;
    int         maxLight;
    int         darkTime;
    int         brightTime;
} v13_strobe_t;
*/
    // Padding at the start (an old thinker_t struct)
    Reader_Read(reader, NULL, SIZEOF_V13_THINKER_T);

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
/* Original Heretic format:
typedef struct {
    thinker_t   thinker; // was 12 bytes
    Sector     *sector;
    int         minLight;
    int         maxLight;
    int         direction;
} v13_glow_t;
*/
    // Padding at the start (an old thinker_t struct)
    Reader_Read(reader, NULL, SIZEOF_V13_THINKER_T);

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
static bool SV_OpenFile_Hr_v13(Path filePath)
{
    DE_ASSERT(saveBuffer == 0);
    if(!M_ReadFile(NativePath(filePath).expand(), (char **)&saveBuffer))
    {
        return false;
    }
    savePtr = saveBuffer;
    return true;
}

static void SV_CloseFile_Hr_v13()
{
    if(!saveBuffer) return;
    Z_Free(saveBuffer);
    saveBuffer = savePtr = 0;
}
#endif

DE_PIMPL(HereticV13MapStateReader)
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

            readPlayer(players + i, reader);
            players[i].plr->mo  = 0; // Will be set when unarc thinker.
            players[i].attacker = 0;
            for(int k = 0; k < NUMPSPRITES; ++k)
            {
                player_t *plr = &players[i];

                if(plr->pSprites[k].state)
                {
                    plr->pSprites[k].state = &STATES[PTR2INT(plr->pSprites[k].state)];
                }
            }
        }
    }

    void readThinkers()
    {
        // Remove all the current thinkers.
        Thinker_Iterate(NULL, removeThinker, NULL);
        Thinker_Init();

        // read in saved thinkers
        byte tclass;
        while((tclass = Reader_ReadByte(reader)) != 0/*TC_END*/)
        {
            switch(tclass)
            {
            case 1: // TC_MOBJ
                readMobj(reader);
                break;

            default:
                throw ReadError("HereticV13MapStateReader", "Unknown tclass #" + String::asText(tclass) + "in savegame");
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

    /**
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
        // Read in saved thinkers.
        byte tclass;
        while((tclass = Reader_ReadByte(reader)) != tc_endspecials)
        {
            switch(tclass)
            {
            case tc_ceiling: {
                ceiling_t *ceiling = (ceiling_t *)Z_Calloc(sizeof(*ceiling), PU_MAP, NULL);

                readCeiling(ceiling, reader);

                Thinker_Add(&ceiling->thinker);
                break; }

            case tc_door: {
                door_t *door = (door_t *)Z_Calloc(sizeof(*door), PU_MAP, NULL);

                readDoor(door, reader);

                Thinker_Add(&door->thinker);
                break; }

            case tc_floor: {
                floor_t *floor = (floor_t *)Z_Calloc(sizeof(*floor), PU_MAP, NULL);

                readFloor(floor, reader);

                Thinker_Add(&floor->thinker);
                break; }

            case tc_plat: {
                plat_t *plat = (plat_t *)Z_Calloc(sizeof(*plat), PU_MAP, NULL);

                readPlat(plat, reader);

                Thinker_Add(&plat->thinker);
                break; }

            case tc_flash: {
                lightflash_t *flash = (lightflash_t *)Z_Calloc(sizeof(*flash), PU_MAP, NULL);

                readFlash(flash, reader);

                Thinker_Add(&flash->thinker);
                break; }

            case tc_strobe: {
                strobe_t *strobe = (strobe_t *)Z_Calloc(sizeof(*strobe), PU_MAP, NULL);

                readStrobe(strobe, reader);

                Thinker_Add(&strobe->thinker);
                break; }

            case tc_glow: {
                glow_t *glow = (glow_t *)Z_Calloc(sizeof(*glow), PU_MAP, NULL);

                readGlow(glow, reader);

                Thinker_Add(&glow->thinker);
                break; }

            default:
                throw ReadError("HereticV13MapStateReader", "Unknown tclass #" + String::asText(tclass) + "in savegame");
            }
        }
    }
};

HereticV13MapStateReader::HereticV13MapStateReader(const GameStateFolder &session)
    : GameStateFolder::MapStateReader(session)
    , d(new Impl(this))
{}

HereticV13MapStateReader::~HereticV13MapStateReader()
{}

void HereticV13MapStateReader::read(const String & /*mapUriStr*/)
{
    d->reader = SV_NewReader_Hr_v13();

    d->readPlayers();

    // Do sectors.
    for(int i = 0; i < numsectors; ++i)
    {
        Sector *sec     = (Sector *)P_ToPtr(DMU_SECTOR, i);
        xsector_t *xsec = P_ToXSector(sec);

        P_SetDoublep(sec, DMU_FLOOR_HEIGHT,     (coord_t)Reader_ReadInt16(d->reader));
        P_SetDoublep(sec, DMU_CEILING_HEIGHT,   (coord_t)Reader_ReadInt16(d->reader));

        uri_s *floorTextureUrn = readTextureUrn(d->reader, "Flats");
        P_SetPtrp(sec, DMU_FLOOR_MATERIAL,   DD_MaterialForTextureUri(floorTextureUrn));
        Uri_Delete(floorTextureUrn);

        uri_s *ceilingTextureUrn = readTextureUrn(d->reader, "Flats");
        P_SetPtrp(sec, DMU_CEILING_MATERIAL, DD_MaterialForTextureUri(ceilingTextureUrn));
        Uri_Delete(ceilingTextureUrn);

        P_SetFloatp(sec, DMU_LIGHT_LEVEL,      (float) (Reader_ReadInt16(d->reader)) / 255.0f);

        xsec->special = Reader_ReadInt16(d->reader); // needed?
        /*xsec->tag = **/Reader_ReadInt16(d->reader); // needed?
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
            Side *sdef = (Side *)P_GetPtrp(line, k == 0? DMU_FRONT : DMU_BACK);
            if(!sdef) continue;

            fixed_t offx = Reader_ReadInt16(d->reader) << FRACBITS;
            fixed_t offy = Reader_ReadInt16(d->reader) << FRACBITS;
            P_SetFixedp(sdef, DMU_TOP_MATERIAL_OFFSET_X,    offx);
            P_SetFixedp(sdef, DMU_TOP_MATERIAL_OFFSET_Y,    offy);
            P_SetFixedp(sdef, DMU_MIDDLE_MATERIAL_OFFSET_X, offx);
            P_SetFixedp(sdef, DMU_MIDDLE_MATERIAL_OFFSET_Y, offy);
            P_SetFixedp(sdef, DMU_BOTTOM_MATERIAL_OFFSET_X, offx);
            P_SetFixedp(sdef, DMU_BOTTOM_MATERIAL_OFFSET_Y, offy);

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
        throw ReadError("HereticV13MapStateReader", "Bad savegame (consistency test failed!)");
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

thinker_t *HereticV13MapStateReader::thinkerForPrivateId(Id::Type) const
{
    // Private identifiers not supported.
    return nullptr;
}
