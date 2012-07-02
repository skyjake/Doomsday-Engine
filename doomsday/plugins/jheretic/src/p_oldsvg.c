/**
 * @file p_oldsvg.c
 * Heretic ver 1.3 save game reader.
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 1999 Activision
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

#include <stdio.h>
#include <string.h>

#include "jheretic.h"

#include "dmu_lib.h"
#include "p_saveg.h"
#include "p_map.h"
#include "p_mapsetup.h"
#include "p_tick.h"
#include "p_ceiling.h"
#include "p_door.h"
#include "p_plat.h"
#include "p_floor.h"
#include "am_map.h"
#include "p_inventory.h"
#include "hu_inventory.h"

// Do NOT change this:
#define SAVE_VERSION            130
#define VERSIONSIZE             16
#define SAVE_GAME_TERMINATOR    0x1d

#define V13_SAVESTRINGSIZE      24

#define FF_FRAMEMASK            0x7fff

#define SIZEOF_V13_THINKER_T    12
#define V13_THINKER_T_FUNC_OFFSET 8

static byte* savebuffer;
static byte* save_p;

static long SV_v13_ReadLong(void)
{
    save_p += 4;
    return *(int *) (save_p - 4);
}

static short SV_v13_ReadShort(void)
{
    save_p += 2;
    return *(short *) (save_p - 2);
}

static void SV_v13_Read(void* data, int len)
{
    if(data)
        memcpy(data, save_p, len);
    save_p += len;
}

static void SV_v13_ReadPlayer(player_t* pl)
{
    int             i, plrnum = pl - players;
    byte            temp[12];
    ddplayer_t*     ddpl = pl->plr;

    SV_v13_ReadLong(); // mo
    pl->playerState = SV_v13_ReadLong();
    SV_v13_Read(temp, 10); // ticcmd_t
    pl->viewZ = FIX2FLT(SV_v13_ReadLong());
    pl->viewHeight = FIX2FLT(SV_v13_ReadLong());
    pl->viewHeightDelta = FIX2FLT(SV_v13_ReadLong());
    pl->bob = FIX2FLT(SV_v13_ReadLong());
    pl->flyHeight = SV_v13_ReadLong();
    ddpl->lookDir = SV_v13_ReadLong();
    pl->centering = SV_v13_ReadLong();
    pl->health = SV_v13_ReadLong();
    pl->armorPoints = SV_v13_ReadLong();
    pl->armorType = SV_v13_ReadLong();

    P_InventoryEmpty(plrnum);
    for(i = 0; i < 14; ++i)
    {
        inventoryitemtype_t type = SV_v13_ReadLong();
        int             j, count = SV_v13_ReadLong();

        for(j = 0; j < count; ++j)
            P_InventoryGive(plrnum, type, true);
    }

    P_InventorySetReadyItem(plrnum, (inventoryitemtype_t) SV_v13_ReadLong());
    Hu_InventorySelect(plrnum, P_InventoryReadyItem(plrnum));
    SV_v13_ReadLong(); // current inventory item count?
    /*pl->inventorySlotNum =*/ SV_v13_ReadLong();

    memset(pl->powers, 0, sizeof(pl->powers));
    pl->powers[PT_INVULNERABILITY] = (SV_v13_ReadLong()? true : false);
    pl->powers[PT_INVISIBILITY] = (SV_v13_ReadLong()? true : false);
    pl->powers[PT_ALLMAP] = (SV_v13_ReadLong()? true : false);
    if(pl->powers[PT_ALLMAP])
        ST_RevealAutomap(pl - players, true);
    pl->powers[PT_INFRARED] = (SV_v13_ReadLong()? true : false);
    pl->powers[PT_WEAPONLEVEL2] = (SV_v13_ReadLong()? true : false);
    pl->powers[PT_FLIGHT] = (SV_v13_ReadLong()? true : false);
    pl->powers[PT_SHIELD] = (SV_v13_ReadLong()? true : false);
    pl->powers[PT_HEALTH2] = (SV_v13_ReadLong()? true : false);

    memset(pl->keys, 0, sizeof(pl->keys));
    pl->keys[KT_YELLOW] = (SV_v13_ReadLong()? true : false);
    pl->keys[KT_GREEN] = (SV_v13_ReadLong()? true : false);
    pl->keys[KT_BLUE] = (SV_v13_ReadLong()? true : false);

    pl->backpack = SV_v13_ReadLong();

    memset(pl->frags, 0, sizeof(pl->frags));
    pl->frags[0] = SV_v13_ReadLong();
    pl->frags[1] = SV_v13_ReadLong();
    pl->frags[2] = SV_v13_ReadLong();
    pl->frags[3] = SV_v13_ReadLong();

    pl->readyWeapon = SV_v13_ReadLong();
    pl->pendingWeapon = SV_v13_ReadLong();

    // Owned weapons.
    memset(pl->weapons, 0, sizeof(pl->weapons));
    pl->weapons[WT_FIRST].owned = (SV_v13_ReadLong()? true : false);
    pl->weapons[WT_SECOND].owned = (SV_v13_ReadLong()? true : false);
    pl->weapons[WT_THIRD].owned = (SV_v13_ReadLong()? true : false);
    pl->weapons[WT_FOURTH].owned = (SV_v13_ReadLong()? true : false);
    pl->weapons[WT_FIFTH].owned = (SV_v13_ReadLong()? true : false);
    pl->weapons[WT_SIXTH].owned = (SV_v13_ReadLong()? true : false);
    pl->weapons[WT_SEVENTH].owned = (SV_v13_ReadLong()? true : false);
    pl->weapons[WT_EIGHTH].owned = (SV_v13_ReadLong()? true : false);

    memset(pl->ammo, 0, sizeof(pl->ammo));
    pl->ammo[AT_CRYSTAL].owned = SV_v13_ReadLong();
    pl->ammo[AT_ARROW].owned = SV_v13_ReadLong();
    pl->ammo[AT_ORB].owned = SV_v13_ReadLong();
    pl->ammo[AT_RUNE].owned = SV_v13_ReadLong();
    pl->ammo[AT_FIREORB].owned = SV_v13_ReadLong();
    pl->ammo[AT_MSPHERE].owned = SV_v13_ReadLong();
    pl->ammo[AT_CRYSTAL].max = SV_v13_ReadLong();
    pl->ammo[AT_ARROW].max = SV_v13_ReadLong();
    pl->ammo[AT_ORB].max = SV_v13_ReadLong();
    pl->ammo[AT_RUNE].max = SV_v13_ReadLong();
    pl->ammo[AT_FIREORB].max = SV_v13_ReadLong();
    pl->ammo[AT_MSPHERE].max = SV_v13_ReadLong();

    pl->attackDown = SV_v13_ReadLong();
    pl->useDown = SV_v13_ReadLong();
    pl->cheats = SV_v13_ReadLong();
    pl->refire = SV_v13_ReadLong();
    pl->killCount = SV_v13_ReadLong();
    pl->itemCount = SV_v13_ReadLong();
    pl->secretCount = SV_v13_ReadLong();
    SV_v13_ReadLong(); // message, char*
    pl->damageCount = SV_v13_ReadLong();
    pl->bonusCount = SV_v13_ReadLong();
    pl->flameCount = SV_v13_ReadLong();
    SV_v13_ReadLong(); // attacker
    ddpl->extraLight = SV_v13_ReadLong();
    ddpl->fixedColorMap = SV_v13_ReadLong();
    pl->colorMap = SV_v13_ReadLong();
    for(i = 0; i < 2; ++i)
    {
        pspdef_t       *psp = &pl->pSprites[i];

        psp->state = (state_t*) SV_v13_ReadLong();
        psp->pos[VX] = SV_v13_ReadLong();
        psp->pos[VY] = SV_v13_ReadLong();
        psp->tics = SV_v13_ReadLong();
    }

    pl->didSecret = SV_v13_ReadLong();
    pl->morphTics = SV_v13_ReadLong();
    pl->chickenPeck = SV_v13_ReadLong();
    SV_v13_ReadLong(); // rain1
    SV_v13_ReadLong(); // rain2
}

static void SV_v13_ReadMobj(void)
{
    angle_t angle;
    spritenum_t sprite;
    int frame, valid, type, ddflags = 0;
    coord_t pos[3], mom[3], floorz, ceilingz, radius, height;
    mobj_t* mo;
    mobjinfo_t* info;

    // The thinker was 3 ints long.
    SV_v13_ReadLong();
    SV_v13_ReadLong();
    SV_v13_ReadLong();

    pos[VX] = FIX2FLT(SV_v13_ReadLong());
    pos[VY] = FIX2FLT(SV_v13_ReadLong());
    pos[VZ] = FIX2FLT(SV_v13_ReadLong());

    // Sector links.
    SV_v13_ReadLong();
    SV_v13_ReadLong();

    angle = (angle_t) (ANG45 * (SV_v13_ReadLong() / 45));
    sprite = SV_v13_ReadLong();
    frame = SV_v13_ReadLong();
    frame &= ~FF_FRAMEMASK; // not used anymore.

    // Block links.
    SV_v13_ReadLong();
    SV_v13_ReadLong();

    // BspLeaf.
    SV_v13_ReadLong();

    floorz = FIX2FLT(SV_v13_ReadLong());
    ceilingz = FIX2FLT(SV_v13_ReadLong());
    radius = FIX2FLT(SV_v13_ReadLong());
    height = FIX2FLT(SV_v13_ReadLong());
    mom[MX] = FIX2FLT(SV_v13_ReadLong());
    mom[MY] = FIX2FLT(SV_v13_ReadLong());
    mom[MZ] = FIX2FLT(SV_v13_ReadLong());
    valid = SV_v13_ReadLong();
    type = SV_v13_ReadLong();
    info = &MOBJINFO[type];

    if(info->flags & MF_SOLID)
        ddflags |= DDMF_SOLID;
    if(info->flags2 & MF2_DONTDRAW)
        ddflags |= DDMF_DONTDRAW;

    /**
     * We now have all the information we need to create the mobj.
     */
    mo = P_MobjCreateXYZ(P_MobjThinker, pos[VX], pos[VY], pos[VZ], angle,
                      radius, height, ddflags);

    mo->sprite = sprite;
    mo->frame = frame;
    mo->floorZ = floorz;
    mo->ceilingZ = ceilingz;
    mo->mom[MX] = mom[MX];
    mo->mom[MY] = mom[MY];
    mo->mom[MZ] = mom[MZ];
    mo->valid = valid;
    mo->type = type;
    mo->moveDir = DI_NODIR;

    /**
     * Continue reading the mobj data.
     */

    SV_v13_ReadLong();          // info

    mo->tics = SV_v13_ReadLong();
    mo->state = (state_t *) SV_v13_ReadLong();
    mo->damage = SV_v13_ReadLong();
    mo->flags = SV_v13_ReadLong();
    mo->flags2 = SV_v13_ReadLong();
    mo->special1 = SV_v13_ReadLong();
    mo->special2 = SV_v13_ReadLong();
    mo->health = SV_v13_ReadLong();

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

    default:
        break;
    }

    mo->moveDir = SV_v13_ReadLong();
    mo->moveCount = SV_v13_ReadLong();
    SV_v13_ReadLong();          // target
    mo->reactionTime = SV_v13_ReadLong();
    mo->threshold = SV_v13_ReadLong();
    mo->player = (player_t *) SV_v13_ReadLong();
    mo->lastLook = SV_v13_ReadLong();

    mo->spawnSpot.origin[VX] = (coord_t) SV_v13_ReadLong();
    mo->spawnSpot.origin[VY] = (coord_t) SV_v13_ReadLong();
    mo->spawnSpot.origin[VZ] = 0; // Initialize with "something".
    mo->spawnSpot.angle = (angle_t) (ANG45 * (SV_v13_ReadLong() / 45));
    /*mo->spawnSpot.type = (int)*/ SV_v13_ReadLong();

    {
    int spawnFlags = ((int) SV_v13_ReadLong()) & ~MASK_UNKNOWN_MSF_FLAGS;
    // Spawn on the floor by default unless the mobjtype flags override.
    spawnFlags |= MSF_Z_FLOOR;
    mo->spawnSpot.flags = spawnFlags;
    }

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
    P_MobjSetOrigin(mo);
    mo->floorZ   = P_GetDoublep(mo->bspLeaf, DMU_FLOOR_HEIGHT);
    mo->ceilingZ = P_GetDoublep(mo->bspLeaf, DMU_CEILING_HEIGHT);
}

void P_v13_UnArchivePlayers(void)
{
    int i, j;

    for(i = 0; i < 4; ++i)
    {
        if(!players[i].plr->inGame)
            continue;

        SV_v13_ReadPlayer(players + i);
        players[i].plr->mo = NULL; // Will be set when unarc thinker.
        players[i].attacker = NULL;
        for(j = 0; j < NUMPSPRITES; ++j)
        {
            player_t* plr = &players[i];

            if(plr->pSprites[j].state)
            {
                plr->pSprites[j].state =
                    &STATES[PTR2INT(plr->pSprites[j].state)];
            }
        }
    }
}

void P_v13_UnArchiveWorld(void)
{
    uint                i, j;
    fixed_t             offx, offy;
    short*              get;
    Sector*             sec;
    xsector_t*          xsec;
    LineDef*            line;
    xline_t*            xline;

    get = (short *) save_p;

    // Do sectors.
    for(i = 0; i < numsectors; ++i)
    {
        sec = P_ToPtr(DMU_SECTOR, i);
        xsec = P_ToXSector(sec);

        P_SetDoublep(sec, DMU_FLOOR_HEIGHT,   (coord_t)*get++);
        P_SetDoublep(sec, DMU_CEILING_HEIGHT, (coord_t)*get++);
        P_SetPtrp(sec, DMU_FLOOR_MATERIAL,   P_ToPtr(DMU_MATERIAL, DD_MaterialForTextureUniqueId(TN_FLATS, *get++)));
        P_SetPtrp(sec, DMU_CEILING_MATERIAL, P_ToPtr(DMU_MATERIAL, DD_MaterialForTextureUniqueId(TN_FLATS, *get++)));
        P_SetFloatp(sec, DMU_LIGHT_LEVEL, (float) (*get++) / 255.0f);
        xsec->special = *get++; // needed?
        /*xsec->tag = **/get++; // needed?
        xsec->specialData = 0;
        xsec->soundTarget = 0;
    }

    // Do lines.
    for(i = 0; i < numlines; ++i)
    {
        line = P_ToPtr(DMU_LINEDEF, i);
        xline = P_ToXLine(line);

        xline->flags = *get++;
        xline->special = *get++;
        /*xline->tag = **/get++;

        for(j = 0; j < 2; j++)
        {
            SideDef* sdef;

            if(j == 0)
                sdef = P_GetPtrp(line, DMU_SIDEDEF0);
            else
                sdef = P_GetPtrp(line, DMU_SIDEDEF1);

            if(!sdef)
                continue;

            offx = *get++ << FRACBITS;
            offy = *get++ << FRACBITS;
            P_SetFixedp(sdef, DMU_TOP_MATERIAL_OFFSET_X, offx);
            P_SetFixedp(sdef, DMU_TOP_MATERIAL_OFFSET_Y, offy);
            P_SetFixedp(sdef, DMU_MIDDLE_MATERIAL_OFFSET_X, offx);
            P_SetFixedp(sdef, DMU_MIDDLE_MATERIAL_OFFSET_Y, offy);
            P_SetFixedp(sdef, DMU_BOTTOM_MATERIAL_OFFSET_X, offx);
            P_SetFixedp(sdef, DMU_BOTTOM_MATERIAL_OFFSET_Y, offy);
            P_SetPtrp(sdef, DMU_TOP_MATERIAL,    P_ToPtr(DMU_MATERIAL, DD_MaterialForTextureUniqueId(TN_TEXTURES, *get++)));
            P_SetPtrp(sdef, DMU_BOTTOM_MATERIAL, P_ToPtr(DMU_MATERIAL, DD_MaterialForTextureUniqueId(TN_TEXTURES, *get++)));
            P_SetPtrp(sdef, DMU_MIDDLE_MATERIAL, P_ToPtr(DMU_MATERIAL, DD_MaterialForTextureUniqueId(TN_TEXTURES, *get++)));
        }
    }
    save_p = (byte*) get;
}

static int removeThinker(thinker_t* th, void* context)
{
    if(th->function == P_MobjThinker)
        P_MobjRemove((mobj_t *) th, true);
    else
        Z_Free(th);

    return false; // Continue iteration.
}

void P_v13_UnArchiveThinkers(void)
{
typedef enum
{
    TC_END,
    TC_MOBJ
} thinkerclass_t;

    byte                tclass;

    // Remove all the current thinkers.
    DD_IterateThinkers(NULL, removeThinker, NULL);
    DD_InitThinkers();

    // read in saved thinkers
    for(;;)
    {
        tclass = *save_p++;
        switch(tclass)
        {
        case TC_END:
            return; // End of list.

        case TC_MOBJ:
            SV_v13_ReadMobj();
            break;

        default:
            Con_Error("Unknown tclass %i in savegame", tclass);
        }
    }
}

static int SV_ReadCeiling(ceiling_t *ceiling)
{
/* Original Heretic format:
typedef struct {
    thinker_t   thinker;        // was 12 bytes
    ceilingtype_e   type;           // was 32bit int
    Sector     *sector;
    fixed_t     bottomheight, topheight;
    fixed_t     speed;
    boolean     crush;
    int         direction;      // 1 = up, 0 = waiting, -1 = down
    int         tag;            // ID
    int         olddirection;
} v13_ceiling_t;
*/
    byte temp[SIZEOF_V13_THINKER_T];

    // Padding at the start (an old thinker_t struct)
    SV_v13_Read(&temp, SIZEOF_V13_THINKER_T);

    // Start of used data members.
    ceiling->type = SV_v13_ReadLong();

    // A 32bit pointer to sector, serialized.
    ceiling->sector = P_ToPtr(DMU_SECTOR, SV_v13_ReadLong());
    if(!ceiling->sector)
        Con_Error("tc_ceiling: bad sector number\n");

    ceiling->bottomHeight = FIX2FLT(SV_v13_ReadLong());
    ceiling->topHeight = FIX2FLT(SV_v13_ReadLong());
    ceiling->speed = FIX2FLT(SV_v13_ReadLong());
    ceiling->crush = SV_v13_ReadLong();
    ceiling->state = (SV_v13_ReadLong() == -1? CS_DOWN : CS_UP);
    ceiling->tag = SV_v13_ReadLong();
    ceiling->oldState = (SV_v13_ReadLong() == -1? CS_DOWN : CS_UP);

    ceiling->thinker.function = T_MoveCeiling;
    if(!(temp + V13_THINKER_T_FUNC_OFFSET))
        DD_ThinkerSetStasis(&ceiling->thinker, true);

    P_ToXSector(ceiling->sector)->specialData = T_MoveCeiling;
    return true; // Add this thinker.
}

static int SV_ReadDoor(door_t *door)
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
    SV_v13_Read(NULL, SIZEOF_V13_THINKER_T);

    // Start of used data members.
    door->type = SV_v13_ReadLong();

    // A 32bit pointer to sector, serialized.
    door->sector = P_ToPtr(DMU_SECTOR, SV_v13_ReadLong());
    if(!door->sector)
        Con_Error("tc_door: bad sector number\n");

    door->topHeight = FIX2FLT(SV_v13_ReadLong());
    door->speed = FIX2FLT(SV_v13_ReadLong());
    door->state = SV_v13_ReadLong();
    door->topWait = SV_v13_ReadLong();
    door->topCountDown = SV_v13_ReadLong();

    door->thinker.function = T_Door;

    P_ToXSector(door->sector)->specialData = T_Door;
    return true; // Add this thinker.
}

static int SV_ReadFloor(floor_t *floor)
{
/* Original Heretic format:
typedef struct {
    thinker_t   thinker;        // was 12 bytes
    floortype_e type;           // was 32bit int
    boolean     crush;
    Sector     *sector;
    int         direction;
    int         newspecial;
    short       texture;
    fixed_t     floordestheight;
    fixed_t     speed;
} v13_floormove_t;
*/
    // Padding at the start (an old thinker_t struct)
    SV_v13_Read(NULL, SIZEOF_V13_THINKER_T);

    // Start of used data members.
    floor->type = SV_v13_ReadLong();
    floor->crush = SV_v13_ReadLong();

    // A 32bit pointer to sector, serialized.
    floor->sector = P_ToPtr(DMU_SECTOR, SV_v13_ReadLong());
    if(!floor->sector)
        Con_Error("tc_floor: bad sector number\n");

    floor->state = (int) SV_v13_ReadLong();
    floor->newSpecial = SV_v13_ReadLong();
    floor->material = P_ToPtr(DMU_MATERIAL, DD_MaterialForTextureUniqueId(TN_FLATS, SV_v13_ReadShort()));
    floor->floorDestHeight = FIX2FLT(SV_v13_ReadLong());
    floor->speed = FIX2FLT(SV_v13_ReadLong());

    floor->thinker.function = T_MoveFloor;

    P_ToXSector(floor->sector)->specialData = T_MoveFloor;
    return true; // Add this thinker.
}

static int SV_ReadPlat(plat_t *plat)
{
/* Original Heretic format:
typedef struct {
    thinker_t   thinker;        // was 12 bytes
    Sector     *sector;
    fixed_t     speed;
    fixed_t     low;
    fixed_t     high;
    int         wait;
    int         count;
    platstate_e     status;         // was 32bit int
    platstate_e     oldStatus;      // was 32bit int
    boolean     crush;
    int         tag;
    plattype_e  type;           // was 32bit int
} v13_plat_t;
*/
    byte temp[SIZEOF_V13_THINKER_T];
    // Padding at the start (an old thinker_t struct)
    SV_v13_Read(&temp, SIZEOF_V13_THINKER_T);

    // Start of used data members.
    // A 32bit pointer to sector, serialized.
    plat->sector = P_ToPtr(DMU_SECTOR, SV_v13_ReadLong());
    if(!plat->sector)
        Con_Error("tc_plat: bad sector number\n");

    plat->speed = FIX2FLT(SV_v13_ReadLong());
    plat->low = FIX2FLT(SV_v13_ReadLong());
    plat->high = FIX2FLT(SV_v13_ReadLong());
    plat->wait = SV_v13_ReadLong();
    plat->count = SV_v13_ReadLong();
    plat->state = SV_v13_ReadLong();
    plat->oldState = SV_v13_ReadLong();
    plat->crush = SV_v13_ReadLong();
    plat->tag = SV_v13_ReadLong();
    plat->type = SV_v13_ReadLong();

    plat->thinker.function = T_PlatRaise;
    if(!(temp + V13_THINKER_T_FUNC_OFFSET))
        DD_ThinkerSetStasis(&plat->thinker, true);

    P_ToXSector(plat->sector)->specialData = T_PlatRaise;
    return true; // Add this thinker.
}

static int SV_ReadFlash(lightflash_t *flash)
{
/* Original Heretic format:
typedef struct {
    thinker_t   thinker;        // was 12 bytes
    Sector     *sector;
    int         count;
    int         maxLight;
    int         minLight;
    int         maxTime;
    int         minTime;
} v13_lightflash_t;
*/
    // Padding at the start (an old thinker_t struct)
    SV_v13_Read(NULL, SIZEOF_V13_THINKER_T);

    // Start of used data members.
    // A 32bit pointer to sector, serialized.
    flash->sector = P_ToPtr(DMU_SECTOR, SV_v13_ReadLong());
    if(!flash->sector)
        Con_Error("tc_flash: bad sector number\n");

    flash->count = SV_v13_ReadLong();
    flash->maxLight = (float) SV_v13_ReadLong() / 255.0f;
    flash->minLight = (float) SV_v13_ReadLong() / 255.0f;
    flash->maxTime = SV_v13_ReadLong();
    flash->minTime = SV_v13_ReadLong();

    flash->thinker.function = T_LightFlash;
    return true; // Add this thinker.
}

static int SV_ReadStrobe(strobe_t *strobe)
{
/* Original Heretic format:
typedef struct {
    thinker_t   thinker;        // was 12 bytes
    Sector     *sector;
    int         count;
    int         minLight;
    int         maxLight;
    int         darkTime;
    int         brightTime;
} v13_strobe_t;
*/
    // Padding at the start (an old thinker_t struct)
    SV_v13_Read(NULL, SIZEOF_V13_THINKER_T);

    // Start of used data members.
    // A 32bit pointer to sector, serialized.
    strobe->sector = P_ToPtr(DMU_SECTOR, SV_v13_ReadLong());
    if(!strobe->sector)
        Con_Error("tc_strobe: bad sector number\n");

    strobe->count = SV_v13_ReadLong();
    strobe->minLight = (float) SV_v13_ReadLong() / 255.0f;
    strobe->maxLight = (float) SV_v13_ReadLong() / 255.0f;
    strobe->darkTime = SV_v13_ReadLong();
    strobe->brightTime = SV_v13_ReadLong();

    strobe->thinker.function = T_StrobeFlash;
    return true; // Add this thinker.
}

static int SV_ReadGlow(glow_t *glow)
{
/* Original Heretic format:
typedef struct {
    thinker_t   thinker;        // was 12 bytes
    Sector     *sector;
    int         minLight;
    int         maxLight;
    int         direction;
} v13_glow_t;
*/
    // Padding at the start (an old thinker_t struct)
    SV_v13_Read(NULL, SIZEOF_V13_THINKER_T);

    // Start of used data members.
    // A 32bit pointer to sector, serialized.
    glow->sector = P_ToPtr(DMU_SECTOR, SV_v13_ReadLong());
    if(!glow->sector)
        Con_Error("tc_glow: bad sector number\n");

    glow->minLight = (float) SV_v13_ReadLong() / 255.0f;
    glow->maxLight = (float) SV_v13_ReadLong() / 255.0f;
    glow->direction = SV_v13_ReadLong();

    glow->thinker.function = T_Glow;
    return true; // Add this thinker.
}

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
void P_v13_UnArchiveSpecials(void)
{
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

    byte        tclass;
    ceiling_t  *ceiling;
    door_t   *door;
    floor_t *floor;
    plat_t     *plat;
    lightflash_t *flash;
    strobe_t   *strobe;
    glow_t     *glow;

    // read in saved thinkers
    for(;;)
    {
        tclass = *save_p++;
        switch(tclass)
        {
        case tc_endspecials:
            return;             // end of list

        case tc_ceiling:
            ceiling = Z_Calloc(sizeof(*ceiling), PU_MAP, NULL);

            SV_ReadCeiling(ceiling);

            DD_ThinkerAdd(&ceiling->thinker);
            break;

        case tc_door:
            door = Z_Calloc(sizeof(*door), PU_MAP, NULL);

            SV_ReadDoor(door);

            DD_ThinkerAdd(&door->thinker);
            break;

        case tc_floor:
            floor = Z_Calloc(sizeof(*floor), PU_MAP, NULL);

            SV_ReadFloor(floor);

            DD_ThinkerAdd(&floor->thinker);
            break;

        case tc_plat:
            plat = Z_Calloc(sizeof(*plat), PU_MAP, NULL);

            SV_ReadPlat(plat);

            DD_ThinkerAdd(&plat->thinker);
            break;

        case tc_flash:
            flash = Z_Calloc(sizeof(*flash), PU_MAP, NULL);

            SV_ReadFlash(flash);

            DD_ThinkerAdd(&flash->thinker);
            break;

        case tc_strobe:
            strobe = Z_Calloc(sizeof(*strobe), PU_MAP, NULL);

            SV_ReadStrobe(strobe);

            DD_ThinkerAdd(&strobe->thinker);
            break;

        case tc_glow:
            glow = Z_Calloc(sizeof(*glow), PU_MAP, NULL);

            SV_ReadGlow(glow);

            DD_ThinkerAdd(&glow->thinker);
            break;

        default:
            Con_Error("P_UnarchiveSpecials:Unknown tclass %i " "in savegame",
                      tclass);
        }
    }
}

int SV_v13_LoadGame(SaveInfo* info)
{
    const char* savename;
    size_t length;
    int i, a, b, c;
    char vcheck[VERSIONSIZE];

    if(!info) return 1;

    savename = Str_Text(SaveInfo_FilePath(info));
    if(!(length = M_ReadFile(savename, (char**)&savebuffer)))
        return 1;

    save_p = savebuffer + V13_SAVESTRINGSIZE;

    // Skip the description field
    memset(vcheck, 0, sizeof(vcheck));
    sprintf(vcheck, "version %i", SAVE_VERSION);
    if(strcmp((const char*)save_p, vcheck))
    {
        // Bad version!
        Con_Message("Savegame ID '%s': incompatible?\n", save_p);
        return 1;
    }
    save_p += VERSIONSIZE;

    gameSkill = *save_p++;
    gameEpisode = (*save_p++) - 1;
    gameMap = (*save_p++) - 1;

    for(i = 0; i < 4; ++i)
    {
        players[i].plr->inGame = *save_p++;
    }

    // Load a base map.
    G_InitNew(gameSkill, gameEpisode, gameMap);

    // Create map time.
    a = *save_p++;
    b = *save_p++;
    c = *save_p++;
    mapTime = (a << 16) + (b << 8) + c;

    // De-archive all the modifications.
    P_v13_UnArchivePlayers();
    P_v13_UnArchiveWorld();
    P_v13_UnArchiveThinkers();
    P_v13_UnArchiveSpecials();

    if(*save_p != SAVE_GAME_TERMINATOR)
        Con_Error("Bad savegame"); // Missing savegame termination marker.

    Z_Free(savebuffer);

    // Spawn particle generators.
    R_SetupMap(DDSMM_AFTER_LOADING, 0);

    return 0; // Success!
}

void SaveInfo_Read_Hr_v13(SaveInfo* info)
{
    saveheader_t* hdr = &info->header;
    char nameBuffer[V13_SAVESTRINGSIZE];
    char vcheck[VERSIONSIZE];
    int i;
    assert(info);

    SV_Read(nameBuffer, V13_SAVESTRINGSIZE);
    nameBuffer[V13_SAVESTRINGSIZE - 1] = 0;
    Str_Set(&info->name, nameBuffer);

    // Check version.
    SV_Read(vcheck, VERSIONSIZE);
    hdr->version = atoi(&vcheck[8]);
    hdr->version = MY_SAVE_VERSION; /// @todo Remove me.

    hdr->skill = SV_ReadByte();
    hdr->episode = SV_ReadByte()-1;
    hdr->map = SV_ReadByte()-1;
    for(i = 0; i < 4; ++i)
    {
        hdr->players[i] = SV_ReadByte();
    }
    memset(&hdr->players[4], 0, sizeof(*hdr->players) * (MAXPLAYERS-4));

    // Get the map time.
    { int a = SV_ReadByte();
    int b = SV_ReadByte();
    int c = SV_ReadByte();
    hdr->mapTime = (a << 16) + (b << 8) + c;
    }

    hdr->magic = MY_SAVE_MAGIC; // Lets pretend...

    /// @note Older formats do not contain all needed values:
    hdr->gameMode = gameMode; // Assume the current mode.
    hdr->deathmatch = 0;
    hdr->noMonsters = 0;
    hdr->respawnMonsters = 0;

    info->gameId  = 0; // None.
}

boolean SV_v13_Recognise(SaveInfo* info)
{
    if(!info || Str_IsEmpty(SaveInfo_FilePath(info))) return false;

    if(SV_OpenFile(Str_Text(SaveInfo_FilePath(info)), "r"))
    {
        SaveInfo_Read_Hr_v13(info);
        SV_CloseFile();
        return true;
    }
    return false;
}
