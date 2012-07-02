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

#define PADSAVEP()          savePtr += (4 - ((savePtr - saveBuffer) & 3)) & 3

// All the versions of DOOM have different savegame IDs, but 500 will be the
// savegame base from now on.
#define SAVE_VERSION_BASE   500
#define SAVE_VERSION        (SAVE_VERSION_BASE + gameMode)
#define V19_SAVESTRINGSIZE  (24)
#define VERSIONSIZE         (16)

#define FF_FULLBRIGHT       (0x8000) // Used to be a flag in thing->frame.
#define FF_FRAMEMASK        (0x7fff)

#define SIZEOF_V19_THINKER_T 12
#define V19_THINKER_T_FUNC_OFFSET 8

static byte* savePtr;
static byte* saveBuffer;

static void SV_v19_Read(void* data, int len)
{
    if(data)
    {
        memcpy(data, savePtr, len);
    }
    savePtr += len;
}

static short SV_v19_ReadShort(void)
{
    savePtr += 2;
    return *(int16_t *) (savePtr - 2);
}

static int SV_v19_ReadLong(void)
{
    savePtr += 4;
    return *(int32_t *) (savePtr - 4);
}

static void SV_v19_ReadPlayer(player_t* pl)
{
    int i;

    SV_v19_ReadLong();
    pl->playerState = SV_v19_ReadLong();
    SV_v19_Read(NULL, 8);
    pl->viewZ = FIX2FLT(SV_v19_ReadLong());
    pl->viewHeight = FIX2FLT(SV_v19_ReadLong());
    pl->viewHeightDelta = FIX2FLT(SV_v19_ReadLong());
    pl->bob = FLT2FIX(SV_v19_ReadLong());
    pl->flyHeight = 0;
    pl->health = SV_v19_ReadLong();
    pl->armorPoints = SV_v19_ReadLong();
    pl->armorType = SV_v19_ReadLong();

    memset(pl->powers, 0, sizeof(pl->powers));
    pl->powers[PT_INVULNERABILITY] = (SV_v19_ReadLong()? true : false);
    pl->powers[PT_STRENGTH] = (SV_v19_ReadLong()? true : false);
    pl->powers[PT_INVISIBILITY] = (SV_v19_ReadLong()? true : false);
    pl->powers[PT_IRONFEET] = (SV_v19_ReadLong()? true : false);
    pl->powers[PT_ALLMAP] = (SV_v19_ReadLong()? true : false);
    if(pl->powers[PT_ALLMAP])
        ST_RevealAutomap(pl - players, true);
    pl->powers[PT_INFRARED] = (SV_v19_ReadLong()? true : false);

    memset(pl->keys, 0, sizeof(pl->keys));
    pl->keys[KT_BLUECARD] = (SV_v19_ReadLong()? true : false);
    pl->keys[KT_YELLOWCARD] = (SV_v19_ReadLong()? true : false);
    pl->keys[KT_REDCARD] = (SV_v19_ReadLong()? true : false);
    pl->keys[KT_BLUESKULL] = (SV_v19_ReadLong()? true : false);
    pl->keys[KT_YELLOWSKULL] = (SV_v19_ReadLong()? true : false);
    pl->keys[KT_REDSKULL] = (SV_v19_ReadLong()? true : false);

    pl->backpack = SV_v19_ReadLong();

    memset(pl->frags, 0, sizeof(pl->frags));
    pl->frags[0] = SV_v19_ReadLong();
    pl->frags[1] = SV_v19_ReadLong();
    pl->frags[2] = SV_v19_ReadLong();
    pl->frags[3] = SV_v19_ReadLong();

    pl->readyWeapon = SV_v19_ReadLong();
    pl->pendingWeapon = SV_v19_ReadLong();

    memset(pl->weapons, 0, sizeof(pl->weapons));
    pl->weapons[WT_FIRST].owned = (SV_v19_ReadLong()? true : false);
    pl->weapons[WT_SECOND].owned = (SV_v19_ReadLong()? true : false);
    pl->weapons[WT_THIRD].owned = (SV_v19_ReadLong()? true : false);
    pl->weapons[WT_FOURTH].owned = (SV_v19_ReadLong()? true : false);
    pl->weapons[WT_FIFTH].owned = (SV_v19_ReadLong()? true : false);
    pl->weapons[WT_SIXTH].owned = (SV_v19_ReadLong()? true : false);
    pl->weapons[WT_SEVENTH].owned = (SV_v19_ReadLong()? true : false);
    pl->weapons[WT_EIGHTH].owned = (SV_v19_ReadLong()? true : false);
    pl->weapons[WT_NINETH].owned = (SV_v19_ReadLong()? true : false);

    memset(pl->ammo, 0, sizeof(pl->ammo));
    pl->ammo[AT_CLIP].owned = SV_v19_ReadLong();
    pl->ammo[AT_SHELL].owned = SV_v19_ReadLong();
    pl->ammo[AT_CELL].owned = SV_v19_ReadLong();
    pl->ammo[AT_MISSILE].owned = SV_v19_ReadLong();
    pl->ammo[AT_CLIP].max = SV_v19_ReadLong();
    pl->ammo[AT_SHELL].max = SV_v19_ReadLong();
    pl->ammo[AT_CELL].max = SV_v19_ReadLong();
    pl->ammo[AT_MISSILE].max = SV_v19_ReadLong();

    pl->attackDown = SV_v19_ReadLong();
    pl->useDown = SV_v19_ReadLong();

    pl->cheats = SV_v19_ReadLong();
    pl->refire = SV_v19_ReadLong();

    pl->killCount = SV_v19_ReadLong();
    pl->itemCount = SV_v19_ReadLong();
    pl->secretCount = SV_v19_ReadLong();

    SV_v19_ReadLong();

    pl->damageCount = SV_v19_ReadLong();
    pl->bonusCount = SV_v19_ReadLong();

    SV_v19_ReadLong();

    pl->plr->extraLight = SV_v19_ReadLong();
    pl->plr->fixedColorMap = SV_v19_ReadLong();
    pl->colorMap = SV_v19_ReadLong();
    for(i = 0; i < 2; ++i)
    {
        pspdef_t* psp = &pl->pSprites[i];

        psp->state = INT2PTR(state_t, SV_v19_ReadLong());
        psp->pos[VX] = SV_v19_ReadLong();
        psp->pos[VY] = SV_v19_ReadLong();
        psp->tics = SV_v19_ReadLong();
    }
    pl->didSecret = SV_v19_ReadLong();
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
    SV_v19_ReadLong();
    SV_v19_ReadLong();
    SV_v19_ReadLong();

    // Info for drawing: position.
    pos[VX] = FIX2FLT(SV_v19_ReadLong());
    pos[VY] = FIX2FLT(SV_v19_ReadLong());
    pos[VZ] = FIX2FLT(SV_v19_ReadLong());

    // More list: links in sector (if needed)
    SV_v19_ReadLong();
    SV_v19_ReadLong();

    //More drawing info: to determine current sprite.
    angle = SV_v19_ReadLong();  // orientation
    sprite = SV_v19_ReadLong(); // used to find patch_t and flip value
    frame = SV_v19_ReadLong();  // might be ORed with FF_FULLBRIGHT
    if(frame & FF_FULLBRIGHT)
        frame &= FF_FRAMEMASK; // not used anymore.

    // Interaction info, by BLOCKMAP.
    // Links in blocks (if needed).
    SV_v19_ReadLong();
    SV_v19_ReadLong();
    SV_v19_ReadLong();

    // The closest interval over all contacted Sectors.
    floorz = FIX2FLT(SV_v19_ReadLong());
    ceilingz = FIX2FLT(SV_v19_ReadLong());

    // For movement checking.
    radius = FIX2FLT(SV_v19_ReadLong());
    height = FIX2FLT(SV_v19_ReadLong());

    // Momentums, used to update position.
    mom[MX] = FIX2FLT(SV_v19_ReadLong());
    mom[MY] = FIX2FLT(SV_v19_ReadLong());
    mom[MZ] = FIX2FLT(SV_v19_ReadLong());

    valid = SV_v19_ReadLong();
    type = SV_v19_ReadLong();
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
    SV_v19_ReadLong();              // &mobjinfo[mo->type]

    mo->tics = SV_v19_ReadLong();   // state tic counter
    mo->state = INT2PTR(state_t, SV_v19_ReadLong());
    mo->damage = DDMAXINT; // Use damage set in mo->info->damage
    mo->flags = SV_v19_ReadLong();
    mo->health = SV_v19_ReadLong();

    // Movement direction, movement generation (zig-zagging).
    mo->moveDir = SV_v19_ReadLong();    // 0-7
    mo->moveCount = SV_v19_ReadLong();  // when 0, select a new dir

    // Thing being chased/attacked (or NULL),
    // also the originator for missiles.
    SV_v19_ReadLong();

    // Reaction time: if non 0, don't attack yet.
    // Used by player to freeze a bit after teleporting.
    mo->reactionTime = SV_v19_ReadLong();

    // If >0, the target will be chased
    // no matter what (even if shot)
    mo->threshold = SV_v19_ReadLong();

    // Additional info record for player avatars only.
    // Only valid if type == MT_PLAYER
    mo->player = INT2PTR(player_t, SV_v19_ReadLong());

    // Player number last looked for.
    mo->lastLook = SV_v19_ReadLong();

    // For nightmare respawn.
    mo->spawnSpot.origin[VX] = (float) SV_v19_ReadShort();
    mo->spawnSpot.origin[VY] = (float) SV_v19_ReadShort();
    mo->spawnSpot.origin[VZ] = 0; // Initialize with "something".
    mo->spawnSpot.angle = (angle_t) (ANG45 * ((int)SV_v19_ReadShort() / 45));
    /* mo->spawnSpot.type = (int) */ SV_v19_ReadShort();

    {
    int spawnFlags = ((int) SV_v19_ReadShort()) & ~MASK_UNKNOWN_MSF_FLAGS;
    // Spawn on the floor by default unless the mobjtype flags override.
    spawnFlags |= MSF_Z_FLOOR;
    mo->spawnSpot.flags = spawnFlags;
    }

    // Thing being chased/attacked for tracers.
    SV_v19_ReadLong();

    mo->info = info;
    SV_TranslateLegacyMobjFlags(mo, 0);

    mo->state = &STATES[PTR2INT(mo->state)];
    mo->target = NULL;
    if(mo->player)
    {
        int     pnum = PTR2INT(mo->player) - 1;

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

void P_v19_UnArchivePlayers(void)
{
    int i, j;

    for(i = 0; i < 4; ++i)
    {
        if(!players[i].plr->inGame)
            continue;

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

void P_v19_UnArchiveWorld(void)
{
    uint                i, j;
    float               matOffset[2];
    short              *get;
    Sector             *sec;
    xsector_t          *xsec;
    LineDef            *line;
    xline_t            *xline;

    get = (short *) savePtr;

    // Do sectors.
    for(i = 0; i < numsectors; ++i)
    {
        sec = P_ToPtr(DMU_SECTOR, i);
        xsec = P_ToXSector(sec);

        P_SetDoublep(sec, DMU_FLOOR_HEIGHT,   (coord_t) (*get++));
        P_SetDoublep(sec, DMU_CEILING_HEIGHT, (coord_t) (*get++));
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

        for(j = 0; j < 2; ++j)
        {
            SideDef* sdef = P_GetPtrp(line, (j? DMU_SIDEDEF1:DMU_SIDEDEF0));

            if(!sdef) continue;

            matOffset[VX] = (float) (*get++);
            matOffset[VY] = (float) (*get++);
            P_SetFloatpv(sdef, DMU_TOP_MATERIAL_OFFSET_XY, matOffset);
            P_SetFloatpv(sdef, DMU_MIDDLE_MATERIAL_OFFSET_XY, matOffset);
            P_SetFloatpv(sdef, DMU_BOTTOM_MATERIAL_OFFSET_XY, matOffset);

            P_SetPtrp(sdef, DMU_TOP_MATERIAL,    P_ToPtr(DMU_MATERIAL, DD_MaterialForTextureUniqueId(TN_TEXTURES, *get++)));
            P_SetPtrp(sdef, DMU_BOTTOM_MATERIAL, P_ToPtr(DMU_MATERIAL, DD_MaterialForTextureUniqueId(TN_TEXTURES, *get++)));
            P_SetPtrp(sdef, DMU_MIDDLE_MATERIAL, P_ToPtr(DMU_MATERIAL, DD_MaterialForTextureUniqueId(TN_TEXTURES, *get++)));
        }
    }

    savePtr = (byte *) get;
}

static int removeThinker(thinker_t* th, void* context)
{
    if(th->function == P_MobjThinker)
        P_MobjRemove((mobj_t *) th, true);
    else
        Z_Free(th);

    return false; // Continue iteration.
}

void P_v19_UnArchiveThinkers(void)
{
enum thinkerclass_e {
    TC_END,
    TC_MOBJ
};

    byte                tClass;

    // Remove all the current thinkers.
    DD_IterateThinkers(NULL, removeThinker, NULL);
    DD_InitThinkers();

    // Read in saved thinkers.
    for(;;)
    {
        tClass = *savePtr++;
        switch(tClass)
        {
        case TC_END:
            return; // End of list.

        case TC_MOBJ:
            PADSAVEP();
            SV_v19_ReadMobj();
            break;

        default:
            Con_Error("Unknown tclass %i in savegame", tClass);
        }
    }
}

static int SV_v19_ReadCeiling(ceiling_t *ceiling)
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
    byte                temp[SIZEOF_V19_THINKER_T];

    // Padding at the start (an old thinker_t struct).
    SV_v19_Read(&temp, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    ceiling->type = SV_v19_ReadLong();

    // A 32bit pointer to sector, serialized.
    ceiling->sector = P_ToPtr(DMU_SECTOR, SV_v19_ReadLong());
    if(!ceiling->sector)
        Con_Error("tc_ceiling: bad sector number\n");

    ceiling->bottomHeight = FIX2FLT(SV_v19_ReadLong());
    ceiling->topHeight = FIX2FLT(SV_v19_ReadLong());
    ceiling->speed = FIX2FLT(SV_v19_ReadLong());
    ceiling->crush = SV_v19_ReadLong();
    ceiling->state = (SV_v19_ReadLong() == -1? CS_DOWN : CS_UP);
    ceiling->tag = SV_v19_ReadLong();
    ceiling->oldState = (SV_v19_ReadLong() == -1? CS_DOWN : CS_UP);

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
    SV_v19_Read(NULL, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    door->type = SV_v19_ReadLong();

    // A 32bit pointer to sector, serialized.
    door->sector = P_ToPtr(DMU_SECTOR, SV_v19_ReadLong());
    if(!door->sector)
        Con_Error("tc_door: bad sector number\n");

    door->topHeight = FIX2FLT(SV_v19_ReadLong());
    door->speed = FIX2FLT(SV_v19_ReadLong());
    door->state = SV_v19_ReadLong();
    door->topWait = SV_v19_ReadLong();
    door->topCountDown = SV_v19_ReadLong();

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
    SV_v19_Read(NULL, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    floor->type = SV_v19_ReadLong();
    floor->crush = SV_v19_ReadLong();

    // A 32bit pointer to sector, serialized.
    floor->sector = P_ToPtr(DMU_SECTOR, SV_v19_ReadLong());
    if(!floor->sector)
        Con_Error("tc_floor: bad sector number\n");

    floor->state = (int) SV_v19_ReadLong();
    floor->newSpecial = SV_v19_ReadLong();
    floor->material = P_ToPtr(DMU_MATERIAL, DD_MaterialForTextureUniqueId(TN_FLATS, SV_v19_ReadShort()));
    floor->floorDestHeight = FIX2FLT(SV_v19_ReadLong());
    floor->speed = FIX2FLT(SV_v19_ReadLong());

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
    SV_v19_Read(temp, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    // A 32bit pointer to sector, serialized.
    plat->sector = P_ToPtr(DMU_SECTOR, SV_v19_ReadLong());
    if(!plat->sector)
        Con_Error("tc_plat: bad sector number\n");

    plat->speed = FIX2FLT(SV_v19_ReadLong());
    plat->low = FIX2FLT(SV_v19_ReadLong());
    plat->high = FIX2FLT(SV_v19_ReadLong());
    plat->wait = SV_v19_ReadLong();
    plat->count = SV_v19_ReadLong();
    plat->state = SV_v19_ReadLong();
    plat->oldState = SV_v19_ReadLong();
    plat->crush = SV_v19_ReadLong();
    plat->tag = SV_v19_ReadLong();
    plat->type = SV_v19_ReadLong();

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
    SV_v19_Read(NULL, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    // A 32bit pointer to sector, serialized.
    flash->sector = P_ToPtr(DMU_SECTOR, SV_v19_ReadLong());
    if(!flash->sector)
        Con_Error("tc_flash: bad sector number\n");

    flash->count = SV_v19_ReadLong();
    flash->maxLight = (float) SV_v19_ReadLong() / 255.0f;
    flash->minLight = (float) SV_v19_ReadLong() / 255.0f;
    flash->maxTime = SV_v19_ReadLong();
    flash->minTime = SV_v19_ReadLong();

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
    SV_v19_Read(NULL, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    // A 32bit pointer to sector, serialized.
    strobe->sector = P_ToPtr(DMU_SECTOR, SV_v19_ReadLong());
    if(!strobe->sector)
        Con_Error("tc_strobe: bad sector number\n");

    strobe->count = SV_v19_ReadLong();
    strobe->minLight = (float) SV_v19_ReadLong() / 255.0f;
    strobe->maxLight = (float) SV_v19_ReadLong() / 255.0f;
    strobe->darkTime = SV_v19_ReadLong();
    strobe->brightTime = SV_v19_ReadLong();

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
    SV_v19_Read(NULL, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    // A 32bit pointer to sector, serialized.
    glow->sector = P_ToPtr(DMU_SECTOR, SV_v19_ReadLong());
    if(!glow->sector)
        Con_Error("tc_glow: bad sector number\n");

    glow->minLight = (float) SV_v19_ReadLong() / 255.0f;
    glow->maxLight = (float) SV_v19_ReadLong() / 255.0f;
    glow->direction = SV_v19_ReadLong();

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
void P_v19_UnArchiveSpecials(void)
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

    byte                tClass;
    ceiling_t          *ceiling;
    door_t           *door;
    floor_t        *floor;
    plat_t             *plat;
    lightflash_t       *flash;
    strobe_t           *strobe;
    glow_t             *glow;

    // Read in saved thinkers.
    for(;;)
    {
        tClass = *savePtr++;
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

int SV_v19_LoadGame(SaveInfo* info)
{
    const char* savename;
    int i, a, b, c;
    size_t length;
    char vcheck[VERSIONSIZE];

    if(!info) return 1;

    savename = Str_Text(SaveInfo_FilePath(info));
    if(!(length = M_ReadFile(savename, (char**)&saveBuffer)))
        return 1;

    // Skip the description field.
    savePtr = saveBuffer + V19_SAVESTRINGSIZE;

    // Check version.
    memset(vcheck, 0, sizeof(vcheck));
    sprintf(vcheck, "version %i", SAVE_VERSION);
    if(strcmp((const char*) savePtr, vcheck))
    {
        int saveVer;

        sscanf((const char*) savePtr, "version %i", &saveVer);
        if(saveVer >= SAVE_VERSION_BASE)
        {
            // Must be from the wrong game.
            Con_Message("Bad savegame version.\n");
            Z_Free(saveBuffer);
            saveBuffer = NULL;
            savePtr = NULL;
            return 1;
        }

        // Just give a warning.
        Con_Message("Savegame ID '%s': incompatible?\n", savePtr);
    }
    savePtr += VERSIONSIZE;

    gameSkill = *savePtr++;
    gameEpisode = (*savePtr++) - 1;
    gameMap = (*savePtr++) - 1;
    for(i = 0; i < 4; ++i)
    {
        players[i].plr->inGame = *savePtr++;
    }

    // Load a base map.
    G_InitNew(gameSkill, gameEpisode, gameMap);

    // Get the map time.
    a = *savePtr++;
    b = *savePtr++;
    c = *savePtr++;
    mapTime = (a << 16) + (b << 8) + c;

    // Dearchive all the modifications.
    P_v19_UnArchivePlayers();
    P_v19_UnArchiveWorld();
    P_v19_UnArchiveThinkers();
    P_v19_UnArchiveSpecials();

    if(*savePtr != 0x1d)
        Con_Error("SV_v19_LoadGame: Bad savegame (consistency test failed!)\n");

    Z_Free(saveBuffer);
    saveBuffer = NULL;

    // Spawn particle generators.
    R_SetupMap(DDSMM_AFTER_LOADING, 0);

    return 0; // Success!
}

void SaveInfo_Read_Dm_v19(SaveInfo* info)
{
    saveheader_t* hdr = &info->header;
    char nameBuffer[V19_SAVESTRINGSIZE];
    char vcheck[VERSIONSIZE];
    int i;
    assert(info);

    SV_Read(nameBuffer, V19_SAVESTRINGSIZE);
    nameBuffer[V19_SAVESTRINGSIZE - 1] = 0;
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

boolean SV_v19_Recognise(SaveInfo* info)
{
    if(!info || Str_IsEmpty(SaveInfo_FilePath(info))) return false;

    if(SV_OpenFile(Str_Text(SaveInfo_FilePath(info)), "r"))
    {
        SaveInfo_Read_Dm_v19(info);
        SV_CloseFile();
        return true;
    }
    return false;
}
