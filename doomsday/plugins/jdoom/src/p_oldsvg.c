/**
 * @file p_oldsvg.c
 * Doom ver 1.9 save game reader.
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 1993-1996 by id Software, Inc.
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

#include "dmu_lib.h"
#include "p_saveg.h"
#include "p_map.h"
#include "p_mapsetup.h"
#include "p_tick.h"
#include "p_ceiling.h"
#include "p_door.h"
#include "p_floor.h"
#include "p_plat.h"
#include "am_map.h"

#define PADSAVEP()                      savePtr += (4 - ((savePtr - saveBuffer) & 3)) & 3

// All the versions of DOOM have different savegame IDs, but 500 will be the
// savegame base from now on.
#define V19_SAVE_VERSION                500 ///< Version number associated with a recognised doom.exe game save state.
#define V19_SAVESTRINGSIZE              24
#define VERSIONSIZE                     16

#define FF_FULLBRIGHT                   0x8000 ///< Used to be a flag in thing->frame.
#define FF_FRAMEMASK                    0x7fff

#define SIZEOF_V19_THINKER_T            12
#define V19_THINKER_T_FUNC_OFFSET       8

static byte* savePtr;
static byte* saveBuffer;
static Reader* svReader;

static boolean SV_OpenFile_Dm_v19(const char* filePath);
static void    SV_CloseFile_Dm_v19(void);
static Reader* SV_NewReader_Dm_v19(void);

static void    SaveInfo_Read_Dm_v19(SaveInfo* info, Reader* reader);

static char sri8(Reader* r)
{
    if(!r) return 0;
    savePtr++;
    return *(char*) (savePtr - 1);
}

static short sri16(Reader* r)
{
    if(!r) return 0;
    savePtr += 2;
    return *(int16_t *) (savePtr - 2);
}

static int sri32(Reader* r)
{
    if(!r) return 0;
    savePtr += 4;
    return *(int32_t *) (savePtr - 4);
}

static void srd(Reader* r, char* data, int len)
{
    if(!r) return;
    if(data)
    {
        memcpy(data, savePtr, len);
    }
    savePtr += len;
}

static void SV_v19_ReadPlayer(player_t* pl)
{
    int i;

    Reader_ReadInt32(svReader);
    pl->playerState = Reader_ReadInt32(svReader);
    Reader_Read(svReader, NULL, 8);
    pl->viewZ = FIX2FLT(Reader_ReadInt32(svReader));
    pl->viewHeight = FIX2FLT(Reader_ReadInt32(svReader));
    pl->viewHeightDelta = FIX2FLT(Reader_ReadInt32(svReader));
    pl->bob = FLT2FIX(Reader_ReadInt32(svReader));
    pl->flyHeight = 0;
    pl->health = Reader_ReadInt32(svReader);
    pl->armorPoints = Reader_ReadInt32(svReader);
    pl->armorType = Reader_ReadInt32(svReader);

    memset(pl->powers, 0, sizeof(pl->powers));
    pl->powers[PT_INVULNERABILITY] = (Reader_ReadInt32(svReader)? true : false);
    pl->powers[PT_STRENGTH] = (Reader_ReadInt32(svReader)? true : false);
    pl->powers[PT_INVISIBILITY] = (Reader_ReadInt32(svReader)? true : false);
    pl->powers[PT_IRONFEET] = (Reader_ReadInt32(svReader)? true : false);
    pl->powers[PT_ALLMAP] = (Reader_ReadInt32(svReader)? true : false);
    if(pl->powers[PT_ALLMAP])
        ST_RevealAutomap(pl - players, true);
    pl->powers[PT_INFRARED] = (Reader_ReadInt32(svReader)? true : false);

    memset(pl->keys, 0, sizeof(pl->keys));
    pl->keys[KT_BLUECARD] = (Reader_ReadInt32(svReader)? true : false);
    pl->keys[KT_YELLOWCARD] = (Reader_ReadInt32(svReader)? true : false);
    pl->keys[KT_REDCARD] = (Reader_ReadInt32(svReader)? true : false);
    pl->keys[KT_BLUESKULL] = (Reader_ReadInt32(svReader)? true : false);
    pl->keys[KT_YELLOWSKULL] = (Reader_ReadInt32(svReader)? true : false);
    pl->keys[KT_REDSKULL] = (Reader_ReadInt32(svReader)? true : false);

    pl->backpack = Reader_ReadInt32(svReader);

    memset(pl->frags, 0, sizeof(pl->frags));
    pl->frags[0] = Reader_ReadInt32(svReader);
    pl->frags[1] = Reader_ReadInt32(svReader);
    pl->frags[2] = Reader_ReadInt32(svReader);
    pl->frags[3] = Reader_ReadInt32(svReader);

    pl->readyWeapon = Reader_ReadInt32(svReader);
    pl->pendingWeapon = Reader_ReadInt32(svReader);

    memset(pl->weapons, 0, sizeof(pl->weapons));
    pl->weapons[WT_FIRST].owned = (Reader_ReadInt32(svReader)? true : false);
    pl->weapons[WT_SECOND].owned = (Reader_ReadInt32(svReader)? true : false);
    pl->weapons[WT_THIRD].owned = (Reader_ReadInt32(svReader)? true : false);
    pl->weapons[WT_FOURTH].owned = (Reader_ReadInt32(svReader)? true : false);
    pl->weapons[WT_FIFTH].owned = (Reader_ReadInt32(svReader)? true : false);
    pl->weapons[WT_SIXTH].owned = (Reader_ReadInt32(svReader)? true : false);
    pl->weapons[WT_SEVENTH].owned = (Reader_ReadInt32(svReader)? true : false);
    pl->weapons[WT_EIGHTH].owned = (Reader_ReadInt32(svReader)? true : false);
    pl->weapons[WT_NINETH].owned = (Reader_ReadInt32(svReader)? true : false);

    memset(pl->ammo, 0, sizeof(pl->ammo));
    pl->ammo[AT_CLIP].owned = Reader_ReadInt32(svReader);
    pl->ammo[AT_SHELL].owned = Reader_ReadInt32(svReader);
    pl->ammo[AT_CELL].owned = Reader_ReadInt32(svReader);
    pl->ammo[AT_MISSILE].owned = Reader_ReadInt32(svReader);
    pl->ammo[AT_CLIP].max = Reader_ReadInt32(svReader);
    pl->ammo[AT_SHELL].max = Reader_ReadInt32(svReader);
    pl->ammo[AT_CELL].max = Reader_ReadInt32(svReader);
    pl->ammo[AT_MISSILE].max = Reader_ReadInt32(svReader);

    pl->attackDown = Reader_ReadInt32(svReader);
    pl->useDown = Reader_ReadInt32(svReader);

    pl->cheats = Reader_ReadInt32(svReader);
    pl->refire = Reader_ReadInt32(svReader);

    pl->killCount = Reader_ReadInt32(svReader);
    pl->itemCount = Reader_ReadInt32(svReader);
    pl->secretCount = Reader_ReadInt32(svReader);

    Reader_ReadInt32(svReader);

    pl->damageCount = Reader_ReadInt32(svReader);
    pl->bonusCount = Reader_ReadInt32(svReader);

    Reader_ReadInt32(svReader);

    pl->plr->extraLight = Reader_ReadInt32(svReader);
    pl->plr->fixedColorMap = Reader_ReadInt32(svReader);
    pl->colorMap = Reader_ReadInt32(svReader);
    for(i = 0; i < 2; ++i)
    {
        pspdef_t* psp = &pl->pSprites[i];

        psp->state = INT2PTR(state_t, Reader_ReadInt32(svReader));
        psp->pos[VX] = Reader_ReadInt32(svReader);
        psp->pos[VY] = Reader_ReadInt32(svReader);
        psp->tics = Reader_ReadInt32(svReader);
    }
    pl->didSecret = Reader_ReadInt32(svReader);
}

static void SV_v19_ReadMobj(void)
{
    float pos[3], mom[3], radius, height, floorz, ceilingz;
    angle_t angle;
    spritenum_t sprite;
    int frame, valid, type, ddflags = 0;
    mobj_t* mo;
    mobjinfo_t* info;

    // List: thinker links.
    Reader_ReadInt32(svReader);
    Reader_ReadInt32(svReader);
    Reader_ReadInt32(svReader);

    // Info for drawing: position.
    pos[VX] = FIX2FLT(Reader_ReadInt32(svReader));
    pos[VY] = FIX2FLT(Reader_ReadInt32(svReader));
    pos[VZ] = FIX2FLT(Reader_ReadInt32(svReader));

    // More list: links in sector (if needed)
    Reader_ReadInt32(svReader);
    Reader_ReadInt32(svReader);

    //More drawing info: to determine current sprite.
    angle  = Reader_ReadInt32(svReader);  // orientation
    sprite = Reader_ReadInt32(svReader); // used to find patch_t and flip value
    frame  = Reader_ReadInt32(svReader);  // might be ORed with FF_FULLBRIGHT
    if(frame & FF_FULLBRIGHT)
        frame &= FF_FRAMEMASK; // not used anymore.

    // Interaction info, by BLOCKMAP.
    // Links in blocks (if needed).
    Reader_ReadInt32(svReader);
    Reader_ReadInt32(svReader);
    Reader_ReadInt32(svReader);

    // The closest interval over all contacted Sectors.
    floorz   = FIX2FLT(Reader_ReadInt32(svReader));
    ceilingz = FIX2FLT(Reader_ReadInt32(svReader));

    // For movement checking.
    radius = FIX2FLT(Reader_ReadInt32(svReader));
    height = FIX2FLT(Reader_ReadInt32(svReader));

    // Momentums, used to update position.
    mom[MX] = FIX2FLT(Reader_ReadInt32(svReader));
    mom[MY] = FIX2FLT(Reader_ReadInt32(svReader));
    mom[MZ] = FIX2FLT(Reader_ReadInt32(svReader));

    valid = Reader_ReadInt32(svReader);
    type  = Reader_ReadInt32(svReader);
    info = &MOBJINFO[type];

    if(info->flags  & MF_SOLID)     ddflags |= DDMF_SOLID;
    if(info->flags2 & MF2_DONTDRAW) ddflags |= DDMF_DONTDRAW;

    /**
     * We now have all the information we need to create the mobj.
     */
    mo = P_MobjCreateXYZ(P_MobjThinker, pos[VX], pos[VY], pos[VZ], angle,
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

    /**
     * Continue reading the mobj data.
     */
    Reader_ReadInt32(svReader);              // &mobjinfo[mo->type]

    mo->tics   = Reader_ReadInt32(svReader);   // state tic counter
    mo->state  = INT2PTR(state_t, Reader_ReadInt32(svReader));
    mo->damage = DDMAXINT; // Use damage set in mo->info->damage
    mo->flags  = Reader_ReadInt32(svReader);
    mo->health = Reader_ReadInt32(svReader);

    // Movement direction, movement generation (zig-zagging).
    mo->moveDir   = Reader_ReadInt32(svReader);    // 0-7
    mo->moveCount = Reader_ReadInt32(svReader);  // when 0, select a new dir

    // Thing being chased/attacked (or NULL),
    // also the originator for missiles.
    Reader_ReadInt32(svReader);

    // Reaction time: if non 0, don't attack yet.
    // Used by player to freeze a bit after teleporting.
    mo->reactionTime = Reader_ReadInt32(svReader);

    // If >0, the target will be chased
    // no matter what (even if shot)
    mo->threshold = Reader_ReadInt32(svReader);

    // Additional info record for player avatars only.
    // Only valid if type == MT_PLAYER
    mo->player = INT2PTR(player_t, Reader_ReadInt32(svReader));

    // Player number last looked for.
    mo->lastLook = Reader_ReadInt32(svReader);

    // For nightmare respawn.
    mo->spawnSpot.origin[VX] = (float) Reader_ReadInt16(svReader);
    mo->spawnSpot.origin[VY] = (float) Reader_ReadInt16(svReader);
    mo->spawnSpot.origin[VZ] = 0; // Initialize with "something".
    mo->spawnSpot.angle = (angle_t) (ANG45 * ((int)Reader_ReadInt16(svReader) / 45));
    /* mo->spawnSpot.type = (int) */ Reader_ReadInt16(svReader);

    {
    int spawnFlags = ((int) Reader_ReadInt16(svReader)) & ~MASK_UNKNOWN_MSF_FLAGS;
    // Spawn on the floor by default unless the mobjtype flags override.
    spawnFlags |= MSF_Z_FLOOR;
    mo->spawnSpot.flags = spawnFlags;
    }

    // Thing being chased/attacked for tracers.
    Reader_ReadInt32(svReader);

    mo->info = info;
    SV_TranslateLegacyMobjFlags(mo, 0);

    mo->state = &STATES[PTR2INT(mo->state)];
    mo->target = NULL;
    if(mo->player)
    {
        int pnum = PTR2INT(mo->player) - 1;

        mo->player = &players[pnum];
        mo->dPlayer = mo->player->plr;
        mo->dPlayer->mo = mo;
        //mo->dPlayer->clAngle = mo->angle; /* $unifiedangles */
        mo->dPlayer->lookDir = 0; /* $unifiedangles */
    }
    P_MobjSetOrigin(mo);
    mo->floorZ   = P_GetDoublep(mo->bspLeaf, DMU_FLOOR_HEIGHT);
    mo->ceilingZ = P_GetDoublep(mo->bspLeaf, DMU_CEILING_HEIGHT);
}

static void P_v19_UnArchivePlayers(void)
{
    int i, j;

    for(i = 0; i < 4; ++i)
    {
        if(!players[i].plr->inGame) continue;

        PADSAVEP();

        SV_v19_ReadPlayer(players + i);

        // will be set when unarc thinker
        players[i].plr->mo = NULL;
        players[i].attacker = NULL;

        for(j = 0; j < NUMPSPRITES; ++j)
        {
            if(players[i].pSprites[j].state)
            {
                players[i].pSprites[j].state =
                    &STATES[PTR2INT(players[i].pSprites[j].state)];
            }
        }
    }
}

static void P_v19_UnArchiveWorld(void)
{
    uint i, j;
    float matOffset[2];
    Sector* sec;
    xsector_t* xsec;
    LineDef* line;
    xline_t* xline;

    // Do sectors.
    for(i = 0; i < numsectors; ++i)
    {
        sec = P_ToPtr(DMU_SECTOR, i);
        xsec = P_ToXSector(sec);

        P_SetDoublep(sec, DMU_FLOOR_HEIGHT,   (coord_t) Reader_ReadInt16(svReader));
        P_SetDoublep(sec, DMU_CEILING_HEIGHT, (coord_t) Reader_ReadInt16(svReader));
        P_SetPtrp   (sec, DMU_FLOOR_MATERIAL,   P_ToPtr(DMU_MATERIAL, DD_MaterialForTextureUniqueId(TN_FLATS, Reader_ReadInt16(svReader))));
        P_SetPtrp   (sec, DMU_CEILING_MATERIAL, P_ToPtr(DMU_MATERIAL, DD_MaterialForTextureUniqueId(TN_FLATS, Reader_ReadInt16(svReader))));

        P_SetFloatp(sec, DMU_LIGHT_LEVEL, (float) (Reader_ReadInt16(svReader)) / 255.0f);
        xsec->special = Reader_ReadInt16(svReader); // needed?
        /*xsec->tag = */Reader_ReadInt16(svReader); // needed?
        xsec->specialData = 0;
        xsec->soundTarget = 0;
    }

    // Do lines.
    for(i = 0; i < numlines; ++i)
    {
        line = P_ToPtr(DMU_LINEDEF, i);
        xline = P_ToXLine(line);

        xline->flags   = Reader_ReadInt16(svReader);
        xline->special = Reader_ReadInt16(svReader);
        /*xline->tag    =*/Reader_ReadInt16(svReader);

        for(j = 0; j < 2; ++j)
        {
            SideDef* sdef = P_GetPtrp(line, (j? DMU_SIDEDEF1:DMU_SIDEDEF0));

            if(!sdef) continue;

            matOffset[VX] = (float) (Reader_ReadInt16(svReader));
            matOffset[VY] = (float) (Reader_ReadInt16(svReader));
            P_SetFloatpv(sdef, DMU_TOP_MATERIAL_OFFSET_XY,    matOffset);
            P_SetFloatpv(sdef, DMU_MIDDLE_MATERIAL_OFFSET_XY, matOffset);
            P_SetFloatpv(sdef, DMU_BOTTOM_MATERIAL_OFFSET_XY, matOffset);

            P_SetPtrp   (sdef, DMU_TOP_MATERIAL,    P_ToPtr(DMU_MATERIAL, DD_MaterialForTextureUniqueId(TN_TEXTURES, Reader_ReadInt16(svReader))));
            P_SetPtrp   (sdef, DMU_BOTTOM_MATERIAL, P_ToPtr(DMU_MATERIAL, DD_MaterialForTextureUniqueId(TN_TEXTURES, Reader_ReadInt16(svReader))));
            P_SetPtrp   (sdef, DMU_MIDDLE_MATERIAL, P_ToPtr(DMU_MATERIAL, DD_MaterialForTextureUniqueId(TN_TEXTURES, Reader_ReadInt16(svReader))));
        }
    }
}

static int removeThinker(thinker_t* th, void* context)
{
    if(th->function == P_MobjThinker)
        P_MobjRemove((mobj_t *) th, true);
    else
        Z_Free(th);

    return false; // Continue iteration.
}

static void P_v19_UnArchiveThinkers(void)
{
    enum thinkerclass_e {
        TC_END,
        TC_MOBJ
    };
    byte tClass;

    // Remove all the current thinkers.
    DD_IterateThinkers(NULL, removeThinker, NULL);
    DD_InitThinkers();

    // Read in saved thinkers.
    for(;;)
    {
        tClass = Reader_ReadByte(svReader);
        switch(tClass)
        {
        case TC_END: return; // End of list.

        case TC_MOBJ:
            PADSAVEP();
            SV_v19_ReadMobj();
            break;

        default:
            Con_Error("Unknown tclass %i in savegame", tClass);
        }
    }
}

static int SV_v19_ReadCeiling(ceiling_t* ceiling)
{
/* Original DOOM format:
typedef struct {
    thinker_t thinker; // was 12 bytes
    ceilingtype_e type; // was 32bit int
    Sector *sector;
    fixed_t bottomheight;
    fixed_t topheight;
    fixed_t speed;
    boolean crush;
    int     direction;
    int     tag;
    int     olddirection;
} v19_ceiling_t;
*/
    byte temp[SIZEOF_V19_THINKER_T];

    // Padding at the start (an old thinker_t struct).
    Reader_Read(svReader, &temp, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    ceiling->type = Reader_ReadInt32(svReader);

    // A 32bit pointer to sector, serialized.
    ceiling->sector = P_ToPtr(DMU_SECTOR, Reader_ReadInt32(svReader));
    if(!ceiling->sector)
        Con_Error("tc_ceiling: bad sector number\n");

    ceiling->bottomHeight = FIX2FLT(Reader_ReadInt32(svReader));
    ceiling->topHeight    = FIX2FLT(Reader_ReadInt32(svReader));
    ceiling->speed = FIX2FLT(Reader_ReadInt32(svReader));
    ceiling->crush = Reader_ReadInt32(svReader);
    ceiling->state = (Reader_ReadInt32(svReader) == -1? CS_DOWN : CS_UP);
    ceiling->tag   = Reader_ReadInt32(svReader);
    ceiling->oldState = (Reader_ReadInt32(svReader) == -1? CS_DOWN : CS_UP);

    ceiling->thinker.function = T_MoveCeiling;
    if(!(temp + V19_THINKER_T_FUNC_OFFSET))
        DD_ThinkerSetStasis(&ceiling->thinker, true);

    P_ToXSector(ceiling->sector)->specialData = ceiling;
    return true; // Add this thinker.
}

static int SV_v19_ReadDoor(door_t *door)
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
    Reader_Read(svReader, NULL, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    door->type = Reader_ReadInt32(svReader);

    // A 32bit pointer to sector, serialized.
    door->sector = P_ToPtr(DMU_SECTOR, Reader_ReadInt32(svReader));
    if(!door->sector)
        Con_Error("tc_door: bad sector number\n");

    door->topHeight = FIX2FLT(Reader_ReadInt32(svReader));
    door->speed = FIX2FLT(Reader_ReadInt32(svReader));
    door->state = Reader_ReadInt32(svReader);
    door->topWait = Reader_ReadInt32(svReader);
    door->topCountDown = Reader_ReadInt32(svReader);

    door->thinker.function = T_Door;

    P_ToXSector(door->sector)->specialData = door;
    return true; // Add this thinker.
}

static int SV_v19_ReadFloor(floor_t *floor)
{
/* Original DOOM format:
typedef struct {
    thinker_t thinker; // was 12 bytes
    floortype_e type; // was 32bit int
    boolean crush;
    Sector *sector;
    int     direction;
    int     newspecial;
    short   texture;
    fixed_t floordestheight;
    fixed_t speed;
} v19_floormove_t;
*/
    // Padding at the start (an old thinker_t struct)
    Reader_Read(svReader, NULL, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    floor->type = Reader_ReadInt32(svReader);
    floor->crush = Reader_ReadInt32(svReader);

    // A 32bit pointer to sector, serialized.
    floor->sector = P_ToPtr(DMU_SECTOR, Reader_ReadInt32(svReader));
    if(!floor->sector)
        Con_Error("tc_floor: bad sector number\n");

    floor->state = (int) Reader_ReadInt32(svReader);
    floor->newSpecial = Reader_ReadInt32(svReader);
    floor->material = P_ToPtr(DMU_MATERIAL, DD_MaterialForTextureUniqueId(TN_FLATS, Reader_ReadInt16(svReader)));
    floor->floorDestHeight = FIX2FLT(Reader_ReadInt32(svReader));
    floor->speed = FIX2FLT(Reader_ReadInt32(svReader));

    floor->thinker.function = T_MoveFloor;

    P_ToXSector(floor->sector)->specialData = floor;
    return true; // Add this thinker.
}

static int SV_v19_ReadPlat(plat_t *plat)
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
    boolean crush;
    int     tag;
    plattype_e type; // was 32bit int
} v19_plat_t;
*/
    byte                temp[SIZEOF_V19_THINKER_T];

    // Padding at the start (an old thinker_t struct)
    Reader_Read(svReader, temp, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    // A 32bit pointer to sector, serialized.
    plat->sector = P_ToPtr(DMU_SECTOR, Reader_ReadInt32(svReader));
    if(!plat->sector)
        Con_Error("tc_plat: bad sector number\n");

    plat->speed = FIX2FLT(Reader_ReadInt32(svReader));
    plat->low = FIX2FLT(Reader_ReadInt32(svReader));
    plat->high = FIX2FLT(Reader_ReadInt32(svReader));
    plat->wait = Reader_ReadInt32(svReader);
    plat->count = Reader_ReadInt32(svReader);
    plat->state = Reader_ReadInt32(svReader);
    plat->oldState = Reader_ReadInt32(svReader);
    plat->crush = Reader_ReadInt32(svReader);
    plat->tag = Reader_ReadInt32(svReader);
    plat->type = Reader_ReadInt32(svReader);

    plat->thinker.function = T_PlatRaise;
    if(!(temp + V19_THINKER_T_FUNC_OFFSET))
        DD_ThinkerSetStasis(&plat->thinker, true);

    P_ToXSector(plat->sector)->specialData = plat;
    return true; // Add this thinker.
}

static int SV_v19_ReadFlash(lightflash_t *flash)
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
    Reader_Read(svReader, NULL, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    // A 32bit pointer to sector, serialized.
    flash->sector = P_ToPtr(DMU_SECTOR, Reader_ReadInt32(svReader));
    if(!flash->sector)
        Con_Error("tc_flash: bad sector number\n");

    flash->count = Reader_ReadInt32(svReader);
    flash->maxLight = (float) Reader_ReadInt32(svReader) / 255.0f;
    flash->minLight = (float) Reader_ReadInt32(svReader) / 255.0f;
    flash->maxTime = Reader_ReadInt32(svReader);
    flash->minTime = Reader_ReadInt32(svReader);

    flash->thinker.function = T_LightFlash;
    return true; // Add this thinker.
}

static int SV_v19_ReadStrobe(strobe_t *strobe)
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
    Reader_Read(svReader, NULL, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    // A 32bit pointer to sector, serialized.
    strobe->sector = P_ToPtr(DMU_SECTOR, Reader_ReadInt32(svReader));
    if(!strobe->sector)
        Con_Error("tc_strobe: bad sector number\n");

    strobe->count = Reader_ReadInt32(svReader);
    strobe->minLight = (float) Reader_ReadInt32(svReader) / 255.0f;
    strobe->maxLight = (float) Reader_ReadInt32(svReader) / 255.0f;
    strobe->darkTime = Reader_ReadInt32(svReader);
    strobe->brightTime = Reader_ReadInt32(svReader);

    strobe->thinker.function = T_StrobeFlash;
    return true; // Add this thinker.
}

static int SV_v19_ReadGlow(glow_t *glow)
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
    Reader_Read(svReader, NULL, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    // A 32bit pointer to sector, serialized.
    glow->sector = P_ToPtr(DMU_SECTOR, Reader_ReadInt32(svReader));
    if(!glow->sector)
        Con_Error("tc_glow: bad sector number\n");

    glow->minLight = (float) Reader_ReadInt32(svReader) / 255.0f;
    glow->maxLight = (float) Reader_ReadInt32(svReader) / 255.0f;
    glow->direction = Reader_ReadInt32(svReader);

    glow->thinker.function = T_Glow;
    return true; // Add this thinker.
}

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
static void P_v19_UnArchiveSpecials(void)
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

    byte tClass;
    ceiling_t* ceiling;
    door_t* door;
    floor_t* floor;
    plat_t* plat;
    lightflash_t* flash;
    strobe_t* strobe;
    glow_t* glow;

    // Read in saved thinkers.
    for(;;)
    {
        tClass = Reader_ReadByte(svReader);
        switch(tClass)
        {
        case tc_endspecials:
            return; // End of list.

        case tc_ceiling:
            PADSAVEP();
            ceiling = Z_Calloc(sizeof(*ceiling), PU_MAP, NULL);

            SV_v19_ReadCeiling(ceiling);

            DD_ThinkerAdd(&ceiling->thinker);
            break;

        case tc_door:
            PADSAVEP();
            door = Z_Calloc(sizeof(*door), PU_MAP, NULL);

            SV_v19_ReadDoor(door);

            DD_ThinkerAdd(&door->thinker);
            break;

        case tc_floor:
            PADSAVEP();
            floor = Z_Calloc(sizeof(*floor), PU_MAP, NULL);

            SV_v19_ReadFloor(floor);

            DD_ThinkerAdd(&floor->thinker);
            break;

        case tc_plat:
            PADSAVEP();
            plat = Z_Calloc(sizeof(*plat), PU_MAP, NULL);

            SV_v19_ReadPlat(plat);

            DD_ThinkerAdd(&plat->thinker);
            break;

        case tc_flash:
            PADSAVEP();
            flash = Z_Calloc(sizeof(*flash), PU_MAP, NULL);

            SV_v19_ReadFlash(flash);

            DD_ThinkerAdd(&flash->thinker);
            break;

        case tc_strobe:
            PADSAVEP();
            strobe = Z_Calloc(sizeof(*strobe), PU_MAP, NULL);

            SV_v19_ReadStrobe(strobe);

            DD_ThinkerAdd(&strobe->thinker);
            break;

        case tc_glow:
            PADSAVEP();
            glow = Z_Calloc(sizeof(*glow), PU_MAP, NULL);

            SV_v19_ReadGlow(glow);

            DD_ThinkerAdd(&glow->thinker);
            break;

        default:
            Con_Error("P_UnarchiveSpecials:Unknown tclass %i in savegame",
                      tClass);
        }
    }
}

int SV_LoadState_Dm_v19(SaveInfo* info)
{
    const saveheader_t* hdr;
    if(!info) return 1;
    if(!SV_OpenFile_Dm_v19(Str_Text(SaveInfo_FilePath(info))))
        return 1;

    svReader = SV_NewReader_Dm_v19();

    // Read the header again.
    /// @todo Seek past the header straight to the game state.
    {
    SaveInfo* tmp = SaveInfo_New();
    SaveInfo_Read_Dm_v19(tmp, svReader);
    SaveInfo_Delete(tmp);
    }
    hdr = SaveInfo_Header(info);

    gameSkill = hdr->skill;
    gameEpisode = hdr->episode;
    gameMap = hdr->map;
    gameMapEntryPoint = 0;

    // We don't want to see a briefing if we're loading a save game.
    briefDisabled = true;

    // Load a base map.
    G_NewGame(gameSkill, gameEpisode, gameMap, gameMapEntryPoint);
    /// @todo Necessary?
    G_SetGameAction(GA_NONE);

    // Recreate map state.
    mapTime = hdr->mapTime;
    P_v19_UnArchivePlayers();
    P_v19_UnArchiveWorld();
    P_v19_UnArchiveThinkers();
    P_v19_UnArchiveSpecials();

    if(Reader_ReadByte(svReader) != 0x1d)
    {
        Reader_Delete(svReader);
        svReader = NULL;
        SV_CloseFile_Dm_v19();

        Con_Error("SV_LoadState_Dm_v19: Bad savegame (consistency test failed!)\n");
        exit(1); // Unreachable.
    }

    Reader_Delete(svReader);
    svReader = NULL;
    SV_CloseFile_Dm_v19();

    return 0; // Success!
}

static void SaveInfo_Read_Dm_v19(SaveInfo* info, Reader* reader)
{
    saveheader_t* hdr = &info->header;
    char nameBuffer[V19_SAVESTRINGSIZE];
    char vcheck[VERSIONSIZE];
    int i;
    assert(info);

    Reader_Read(reader, nameBuffer, V19_SAVESTRINGSIZE);
    nameBuffer[V19_SAVESTRINGSIZE - 1] = 0;
    Str_Set(&info->name, nameBuffer);

    Reader_Read(reader, vcheck, VERSIONSIZE);
    //assert(!strncmp(vcheck, "version ", 8)); // Ensure save state format has been recognised by now.
    hdr->version = atoi(&vcheck[8]);

    hdr->skill = Reader_ReadByte(reader);
    hdr->episode = Reader_ReadByte(reader)-1;
    hdr->map = Reader_ReadByte(reader)-1;
    for(i = 0; i < 4; ++i)
    {
        hdr->players[i] = Reader_ReadByte(reader);
    }
    memset(&hdr->players[4], 0, sizeof(*hdr->players) * (MAXPLAYERS-4));

    // Get the map time.
    { int a = Reader_ReadByte(reader);
    int b = Reader_ReadByte(reader);
    int c = Reader_ReadByte(reader);
    hdr->mapTime = (a << 16) + (b << 8) + c;
    }

    hdr->magic = 0; // Initialize with *something*.

    /// @note Older formats do not contain all needed values:
    hdr->gameMode = gameMode; // Assume the current mode.
    hdr->deathmatch = 0;
    hdr->noMonsters = 0;
    hdr->respawnMonsters = 0;

    info->gameId  = 0; // None.
}

static boolean SV_OpenFile_Dm_v19(const char* filePath)
{
    boolean fileOpened;
#if _DEBUG
    if(saveBuffer)
        Con_Error("SV_OpenFile_Dm_v19: A save state file has already been opened!");
#endif
    fileOpened = 0 != M_ReadFile(filePath, (char**)&saveBuffer);
    if(!fileOpened) return false;
    savePtr = saveBuffer;
    return true;
}

static void SV_CloseFile_Dm_v19(void)
{
    if(!saveBuffer) return;
    Z_Free(saveBuffer);
    saveBuffer = savePtr = NULL;
}

static Reader* SV_NewReader_Dm_v19(void)
{
    if(!saveBuffer) return NULL;
    return Reader_NewWithCallbacks(sri8, sri16, sri32, NULL, srd);
}

boolean SV_RecogniseState_Dm_v19(SaveInfo* info)
{
    if(!info) return false;
    if(!SV_ExistingFile(Str_Text(SaveInfo_FilePath(info)))) return false;

    if(SV_OpenFile_Dm_v19(Str_Text(SaveInfo_FilePath(info))))
    {
        Reader* svReader = SV_NewReader_Dm_v19();
        boolean result = false;

        /// @todo Use the 'version' string as the "magic" identifier.
        /*char vcheck[VERSIONSIZE];
        memset(vcheck, 0, sizeof(vcheck));
        Reader_Read(svReader, vcheck, sizeof(vcheck));

        if(strncmp(vcheck, "version ", 8))*/
        {
            SaveInfo_Read_Dm_v19(info, svReader);
            result = (SaveInfo_Header(info)->version <= V19_SAVE_VERSION);
        }

        Reader_Delete(svReader);
        svReader = NULL;
        SV_CloseFile_Dm_v19();

        return result;
    }
    return false;
}
