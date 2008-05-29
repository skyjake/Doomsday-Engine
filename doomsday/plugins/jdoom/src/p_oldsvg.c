/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 * \bug Not 64bit clean: In function 'SV_ReadMobj': cast to pointer from integer of different size
 * \bug Not 64bit clean: In function 'P_v19_UnArchivePlayers': cast from pointer to integer of different size
 * \bug Not 64bit clean: In function 'P_v19_UnArchiveThinkers': cast from pointer to integer of different size
 */

/**
 * p_oldsvg.c:
 */

// HEADER FILES ------------------------------------------------------------

#include "jdoom.h"

#include "p_map.h"
#include "p_mapsetup.h"
#include "p_tick.h"
#include "dmu_lib.h"

// MACROS ------------------------------------------------------------------

#define PADSAVEP()          savePtr += (4 - ((savePtr - saveBuffer) & 3)) & 3
#define SAVESTRINGSIZE      (24)
#define VERSIONSIZE         (16)

#define FF_FULLBRIGHT       (0x8000) // Used to be a flag in thing->frame.
#define FF_FRAMEMASK        (0x7fff)

#define SIZEOF_V19_THINKER_T 12
#define V19_THINKER_T_FUNC_OFFSET 8

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern void SV_UpdateReadMobjFlags(mobj_t *mo, int ver);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static byte   *savePtr;
static byte   *saveBuffer;

// CODE --------------------------------------------------------------------

static void SV_Read(void *data, int len)
{
    if(data)
        memcpy(data, savePtr, len);
    savePtr += len;
}

static short SV_ReadShort(void)
{
    savePtr += 2;
    return *(short *) (savePtr - 2);
}

static int SV_ReadLong(void)
{
    savePtr += 4;
    return *(int *) (savePtr - 4);
}

static void SV_ReadPlayer(player_t *pl)
{
    int                 temp[3];

    SV_ReadLong();
    pl->playerState = SV_ReadLong();
    SV_Read(temp, 8);
    pl->plr->viewZ = FIX2FLT(SV_ReadLong());
    pl->plr->viewHeight = FIX2FLT(SV_ReadLong());
    pl->plr->viewHeightDelta = FIX2FLT(SV_ReadLong());
    pl->bob = FLT2FIX(SV_ReadLong());
    pl->flyHeight = 0;
    pl->health = SV_ReadLong();
    pl->armorPoints = SV_ReadLong();
    pl->armorType = SV_ReadLong();

    SV_Read(pl->powers, 6 * 4);
    SV_Read(pl->keys, 6 * 4);
    pl->backpack = SV_ReadLong();

    SV_Read(pl->frags, 4 * 4);
    pl->readyWeapon = SV_ReadLong();
    pl->pendingWeapon = SV_ReadLong();

    SV_Read(pl->weaponOwned, 9 * 4);
    SV_Read(pl->ammo, 4 * 4);
    SV_Read(pl->maxAmmo, 4 * 4);

    pl->attackDown = SV_ReadLong();
    pl->useDown = SV_ReadLong();

    pl->cheats = SV_ReadLong();

    pl->refire = SV_ReadLong();

    pl->killCount = SV_ReadLong();
    pl->itemCount = SV_ReadLong();
    pl->secretCount = SV_ReadLong();

    SV_ReadLong();

    pl->damageCount = SV_ReadLong();
    pl->bonusCount = SV_ReadLong();

    SV_ReadLong();

    pl->plr->extraLight = SV_ReadLong();
    pl->plr->fixedColorMap = SV_ReadLong();
    pl->colorMap = SV_ReadLong();
    SV_Read(pl->pSprites, 2 * sizeof(pspdef_t));

    pl->didSecret = SV_ReadLong();
}

static void SV_ReadMobj(void)
{
    float               pos[3], mom[3], radius, height, floorz, ceilingz;
    angle_t             angle;
    spritenum_t         sprite;
    int                 frame, valid, type;
    mobj_t             *mo;

    // List: thinker links.
    SV_ReadLong();
    SV_ReadLong();
    SV_ReadLong();

    // Info for drawing: position.
    pos[VX] = FIX2FLT(SV_ReadLong());
    pos[VY] = FIX2FLT(SV_ReadLong());
    pos[VZ] = FIX2FLT(SV_ReadLong());

    // More list: links in sector (if needed)
    SV_ReadLong();
    SV_ReadLong();

    //More drawing info: to determine current sprite.
    angle = SV_ReadLong();  // orientation
    sprite = SV_ReadLong(); // used to find patch_t and flip value
    frame = SV_ReadLong();  // might be ORed with FF_FULLBRIGHT
    if(frame & FF_FULLBRIGHT)
        frame &= FF_FRAMEMASK; // not used anymore.

    // Interaction info, by BLOCKMAP.
    // Links in blocks (if needed).
    SV_ReadLong();
    SV_ReadLong();
    SV_ReadLong();

    // The closest interval over all contacted Sectors.
    floorz = FIX2FLT(SV_ReadLong());
    ceilingz = FIX2FLT(SV_ReadLong());

    // For movement checking.
    radius = FIX2FLT(SV_ReadLong());
    height = FIX2FLT(SV_ReadLong());

    // Momentums, used to update position.
    mom[MX] = FIX2FLT(SV_ReadLong());
    mom[MY] = FIX2FLT(SV_ReadLong());
    mom[MZ] = FIX2FLT(SV_ReadLong());

    valid = SV_ReadLong();
    type = SV_ReadLong();

    /**
     * We now have all the information we need to create the mobj.
     */
    mo = P_MobjCreate(P_MobjThinker, pos[VX], pos[VY], pos[VZ], angle,
                      radius, height, 0);

    mo->sprite = sprite;
    mo->frame = frame;
    mo->floorZ = floorz;
    mo->ceilingZ = ceilingz;
    mo->mom[MX] = mom[MX];
    mo->mom[MY] = mom[MY];
    mo->mom[MZ] = mom[MZ];
    mo->valid = valid;
    mo->type = type;

    /**
     * Continue reading the mobj data.
     */
    mo->info = &mobjInfo[mo->type];
    SV_ReadLong();              // &mobjinfo[mo->type]

    mo->tics = SV_ReadLong();   // state tic counter
    mo->state = (state_t *) SV_ReadLong();
    mo->damage = DDMAXINT; // Use damage set in mo->info->damage
    mo->flags = SV_ReadLong();
    mo->health = SV_ReadLong();

    // Movement direction, movement generation (zig-zagging).
    mo->moveDir = SV_ReadLong();    // 0-7
    mo->moveCount = SV_ReadLong();  // when 0, select a new dir

    // Thing being chased/attacked (or NULL),
    // also the originator for missiles.
    SV_ReadLong();

    // Reaction time: if non 0, don't attack yet.
    // Used by player to freeze a bit after teleporting.
    mo->reactionTime = SV_ReadLong();

    // If >0, the target will be chased
    // no matter what (even if shot)
    mo->threshold = SV_ReadLong();

    // Additional info record for player avatars only.
    // Only valid if type == MT_PLAYER
    mo->player = (player_t *) SV_ReadLong();

    // Player number last looked for.
    mo->lastLook = SV_ReadLong();

    // For nightmare respawn.
    mo->spawnSpot.pos[VX] = (float) SV_ReadShort();
    mo->spawnSpot.pos[VY] = (float) SV_ReadShort();
    mo->spawnSpot.pos[VZ] = ONFLOORZ;
    mo->spawnSpot.angle = (angle_t) (ANG45 * ((int)SV_ReadShort() / 45));
    mo->spawnSpot.type = (int) SV_ReadShort();
    mo->spawnSpot.flags = (int) SV_ReadShort();

    // Thing being chased/attacked for tracers.
    SV_ReadLong();

    SV_UpdateReadMobjFlags(mo, 0);

    mo->state = &states[(int) mo->state];
    mo->target = NULL;
    if(mo->player)
    {
        int     pnum = (int) mo->player - 1;

        mo->player = &players[pnum];
        mo->dPlayer = mo->player->plr;
        mo->dPlayer->mo = mo;
        //mo->dPlayer->clAngle = mo->angle; /* $unifiedangles */
        mo->dPlayer->lookDir = 0; /* $unifiedangles */
    }
    P_MobjSetPosition(mo);
    mo->info = &mobjInfo[mo->type];
    mo->floorZ =
        P_GetFloatp(mo->subsector, DMU_FLOOR_HEIGHT);
    mo->ceilingZ =
        P_GetFloatp(mo->subsector, DMU_CEILING_HEIGHT);
}

void P_v19_UnArchivePlayers(void)
{
    int                 i, j;

    for(i = 0; i < 4; ++i)
    {
        if(!players[i].plr->inGame)
            continue;

        PADSAVEP();

        SV_ReadPlayer(players + i);

        // will be set when unarc thinker
        players[i].plr->mo = NULL;
        players[i].attacker = NULL;

        for(j = 0; j < NUMPSPRITES; ++j)
        {
            if(players[i].pSprites[j].state)
            {
                players[i].pSprites[j].state =
                    &states[(int) players[i].pSprites[j].state];
            }
        }
    }
}

void P_v19_UnArchiveWorld(void)
{
    uint                i, j;
    float               matOffset[2];
    short              *get;
    int                 firstflat = W_CheckNumForName("F_START") + 1;
    sector_t           *sec;
    xsector_t          *xsec;
    linedef_t          *line;
    xline_t            *xline;

    get = (short *) savePtr;

    // Do sectors.
    for(i = 0; i < numsectors; ++i)
    {
        sec = P_ToPtr(DMU_SECTOR, i);
        xsec = P_ToXSector(sec);

        P_SetFloatp(sec, DMU_FLOOR_HEIGHT, (float) (*get++));
        P_SetFloatp(sec, DMU_CEILING_HEIGHT, (float) (*get++));
        P_SetIntp(sec, DMU_FLOOR_MATERIAL, *get++ + firstflat);
        P_SetIntp(sec, DMU_CEILING_MATERIAL, *get++ + firstflat);
        P_SetFloatp(sec, DMU_LIGHT_LEVEL, (float) (*get++) / 255.0f);
        xsec->special = *get++; // needed?
        /*xsec->tag =*/ *get++; // needed?
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
        /*xline->tag =*/ *get++;

        for(j = 0; j < 2; ++j)
        {
            sidedef_t* sdef = P_GetPtrp(line, (j? DMU_SIDEDEF1:DMU_SIDEDEF0));

            if(!sdef)
                continue;

            matOffset[VX] = (float) (*get++);
            matOffset[VY] = (float) (*get++);
            P_SetFloatpv(sdef, DMU_TOP_MATERIAL_OFFSET_XY, matOffset);
            P_SetFloatpv(sdef, DMU_MIDDLE_MATERIAL_OFFSET_XY, matOffset);
            P_SetFloatpv(sdef, DMU_BOTTOM_MATERIAL_OFFSET_XY, matOffset);

            P_SetIntp(sdef, DMU_TOP_MATERIAL, *get++);
            P_SetIntp(sdef, DMU_BOTTOM_MATERIAL, *get++);
            P_SetIntp(sdef, DMU_MIDDLE_MATERIAL, *get++);
        }
    }

    savePtr = (byte *) get;
}

static boolean removeThinker(thinker_t* th, void* context)
{
    if(th->function == P_MobjThinker)
        P_MobjRemove((mobj_t *) th, true);
    else
        Z_Free(th);

    return true; // Continue iteration.
}

void P_v19_UnArchiveThinkers(void)
{
enum thinkerclass_e {
    TC_END,
    TC_MOBJ
};

    byte                tClass;

    // Remove all the current thinkers.
    P_IterateThinkers(NULL, removeThinker, NULL);
    P_InitThinkers();

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
            SV_ReadMobj();
            break;

        default:
            Con_Error("Unknown tclass %i in savegame", tClass);
        }
    }
}

static int SV_ReadCeiling(ceiling_t *ceiling)
{
/* Original DOOM format:
typedef struct {
    thinker_t thinker; // was 12 bytes
    ceiling_e type; // was 32bit int
    sector_t *sector;
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
    SV_Read(&temp, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    ceiling->type = SV_ReadLong();

    // A 32bit pointer to sector, serialized.
    ceiling->sector = P_ToPtr(DMU_SECTOR, SV_ReadLong());
    if(!ceiling->sector)
        Con_Error("tc_ceiling: bad sector number\n");

    ceiling->bottomHeight = FIX2FLT(SV_ReadLong());
    ceiling->topHeight = FIX2FLT(SV_ReadLong());
    ceiling->speed = FIX2FLT(SV_ReadLong());
    ceiling->crush = SV_ReadLong();
    ceiling->direction = SV_ReadLong();
    ceiling->tag = SV_ReadLong();
    ceiling->oldDirection = SV_ReadLong();

    if(temp + V19_THINKER_T_FUNC_OFFSET)
        ceiling->thinker.function = T_MoveCeiling;

    P_ToXSector(ceiling->sector)->specialData = ceiling;
    return true; // Add this thinker.
}

static int SV_ReadDoor(vldoor_t *door)
{
/* Original DOOM format:
typedef struct {
    thinker_t thinker; // was 12 bytes
    vldoor_e type; // was 32bit int
    sector_t *sector;
    fixed_t topheight;
    fixed_t speed;
    int     direction;
    int     topwait;
    int     topcountdown;
} v19_vldoor_t;
*/
    // Padding at the start (an old thinker_t struct)
    SV_Read(NULL, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    door->type = SV_ReadLong();

    // A 32bit pointer to sector, serialized.
    door->sector = P_ToPtr(DMU_SECTOR, SV_ReadLong());
    if(!door->sector)
        Con_Error("tc_door: bad sector number\n");

    door->topHeight = FIX2FLT(SV_ReadLong());
    door->speed = FIX2FLT(SV_ReadLong());
    door->direction = SV_ReadLong();
    door->topWait = SV_ReadLong();
    door->topCountDown = SV_ReadLong();

    door->thinker.function = T_VerticalDoor;

    P_ToXSector(door->sector)->specialData = door;
    return true; // Add this thinker.
}

static int SV_ReadFloor(floormove_t *floor)
{
/* Original DOOM format:
typedef struct {
    thinker_t thinker; // was 12 bytes
    floor_e	type; // was 32bit int
    boolean	crush;
    sector_t *sector;
    int		direction;
    int		newspecial;
    short	texture;
    fixed_t	floordestheight;
    fixed_t	speed;
} v19_floormove_t;
*/
    // Padding at the start (an old thinker_t struct)
    SV_Read(NULL, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    floor->type = SV_ReadLong();
    floor->crush = SV_ReadLong();

    // A 32bit pointer to sector, serialized.
    floor->sector = P_ToPtr(DMU_SECTOR, SV_ReadLong());
    if(!floor->sector)
        Con_Error("tc_floor: bad sector number\n");

    floor->direction = SV_ReadLong();
    floor->newSpecial = SV_ReadLong();
    floor->texture = SV_ReadShort();
    floor->floorDestHeight = FIX2FLT(SV_ReadLong());
    floor->speed = FIX2FLT(SV_ReadLong());

    floor->thinker.function = T_MoveFloor;

    P_ToXSector(floor->sector)->specialData = floor;
    return true; // Add this thinker.
}

static int SV_ReadPlat(plat_t *plat)
{
/* Original DOOM format:
typedef struct {
    thinker_t thinker; // was 12 bytes
    sector_t *sector;
    fixed_t speed;
    fixed_t low;
    fixed_t high;
    int     wait;
    int     count;
    plat_e  status; // was 32bit int
    plat_e  oldstatus; // was 32bit int
    boolean crush;
    int     tag;
    plattype_e type; // was 32bit int
} v19_plat_t;
*/
    byte                temp[SIZEOF_V19_THINKER_T];

    // Padding at the start (an old thinker_t struct)
    SV_Read(temp, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    // A 32bit pointer to sector, serialized.
    plat->sector = P_ToPtr(DMU_SECTOR, SV_ReadLong());
    if(!plat->sector)
        Con_Error("tc_plat: bad sector number\n");

    plat->speed = FIX2FLT(SV_ReadLong());
    plat->low = FIX2FLT(SV_ReadLong());
    plat->high = FIX2FLT(SV_ReadLong());
    plat->wait = SV_ReadLong();
    plat->count = SV_ReadLong();
    plat->status = SV_ReadLong();
    plat->oldStatus = SV_ReadLong();
    plat->crush = SV_ReadLong();
    plat->tag = SV_ReadLong();
    plat->type = SV_ReadLong();

    if(temp + V19_THINKER_T_FUNC_OFFSET)
        plat->thinker.function = T_PlatRaise;

    P_ToXSector(plat->sector)->specialData = plat;
    return true; // Add this thinker.
}

static int SV_ReadFlash(lightflash_t *flash)
{
/* Original DOOM format:
typedef struct {
    thinker_t thinker; // was 12 bytes
    sector_t *sector;
    int     count;
    int     maxlight;
    int     minlight;
    int     maxtime;
    int     mintime;
} v19_lightflash_t;
*/
    // Padding at the start (an old thinker_t struct)
    SV_Read(NULL, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    // A 32bit pointer to sector, serialized.
    flash->sector = P_ToPtr(DMU_SECTOR, SV_ReadLong());
    if(!flash->sector)
        Con_Error("tc_flash: bad sector number\n");

    flash->count = SV_ReadLong();
    flash->maxLight = (float) SV_ReadLong() / 255.0f;
    flash->minLight = (float) SV_ReadLong() / 255.0f;
    flash->maxTime = SV_ReadLong();
    flash->minTime = SV_ReadLong();

    flash->thinker.function = T_LightFlash;
    return true; // Add this thinker.
}

static int SV_ReadStrobe(strobe_t *strobe)
{
/* Original DOOM format:
typedef struct {
    thinker_t thinker; // was 12 bytes
    sector_t *sector;
    int     count;
    int     minlight;
    int     maxlight;
    int     darktime;
    int     brighttime;
} v19_strobe_t;
*/
    // Padding at the start (an old thinker_t struct)
    SV_Read(NULL, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    // A 32bit pointer to sector, serialized.
    strobe->sector = P_ToPtr(DMU_SECTOR, SV_ReadLong());
    if(!strobe->sector)
        Con_Error("tc_strobe: bad sector number\n");

    strobe->count = SV_ReadLong();
    strobe->minLight = (float) SV_ReadLong() / 255.0f;
    strobe->maxLight = (float) SV_ReadLong() / 255.0f;
    strobe->darkTime = SV_ReadLong();
    strobe->brightTime = SV_ReadLong();

    strobe->thinker.function = T_StrobeFlash;
    return true; // Add this thinker.
}

static int SV_ReadGlow(glow_t *glow)
{
/* Original DOOM format:
typedef struct {
    thinker_t thinker; // was 12 bytes
    sector_t *sector;
    int     minlight;
    int     maxlight;
    int     direction;
} v19_glow_t;
*/
    // Padding at the start (an old thinker_t struct)
    SV_Read(NULL, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    // A 32bit pointer to sector, serialized.
    glow->sector = P_ToPtr(DMU_SECTOR, SV_ReadLong());
    if(!glow->sector)
        Con_Error("tc_glow: bad sector number\n");

    glow->minLight = (float) SV_ReadLong() / 255.0f;
    glow->maxLight = (float) SV_ReadLong() / 255.0f;
    glow->direction = SV_ReadLong();

    glow->thinker.function = T_Glow;
    return true; // Add this thinker.
}

/*
 * Things to handle:
 *
 * T_MoveCeiling, (ceiling_t: sector_t * swizzle), - active list
 * T_VerticalDoor, (vldoor_t: sector_t * swizzle),
 * T_MoveFloor, (floormove_t: sector_t * swizzle),
 * T_LightFlash, (lightflash_t: sector_t * swizzle),
 * T_StrobeFlash, (strobe_t: sector_t *),
 * T_Glow, (glow_t: sector_t *),
 * T_PlatRaise, (plat_t: sector_t *), - active list
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
    vldoor_t           *door;
    floormove_t        *floor;
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
            ceiling = Z_Calloc(sizeof(*ceiling), PU_LEVSPEC, NULL);

            SV_ReadCeiling(ceiling);

            P_AddThinker(&ceiling->thinker);
            P_AddActiveCeiling(ceiling);
            break;

        case tc_door:
            PADSAVEP();
            door = Z_Calloc(sizeof(*door), PU_LEVSPEC, NULL);

            SV_ReadDoor(door);

            P_AddThinker(&door->thinker);
            break;

        case tc_floor:
            PADSAVEP();
            floor = Z_Calloc(sizeof(*floor), PU_LEVSPEC, NULL);

            SV_ReadFloor(floor);

            P_AddThinker(&floor->thinker);
            break;

        case tc_plat:
            PADSAVEP();
            plat = Z_Calloc(sizeof(*plat), PU_LEVSPEC, NULL);

            SV_ReadPlat(plat);

            P_AddThinker(&plat->thinker);
            P_AddActivePlat(plat);
            break;

        case tc_flash:
            PADSAVEP();
            flash = Z_Calloc(sizeof(*flash), PU_LEVSPEC, NULL);

            SV_ReadFlash(flash);

            P_AddThinker(&flash->thinker);
            break;

        case tc_strobe:
            PADSAVEP();
            strobe = Z_Calloc(sizeof(*strobe), PU_LEVSPEC, NULL);

            SV_ReadStrobe(strobe);

            P_AddThinker(&strobe->thinker);
            break;

        case tc_glow:
            PADSAVEP();
            glow = Z_Calloc(sizeof(*glow), PU_LEVSPEC, NULL);

            SV_ReadGlow(glow);

            P_AddThinker(&glow->thinker);
            break;

        default:
            Con_Error("P_UnarchiveSpecials:Unknown tclass %i in savegame",
                      tClass);
        }
    }
}

void SV_v19_LoadGame(char *savename)
{
    int                 i;
    int                 a, b, c;
    size_t              length;
    char                vcheck[VERSIONSIZE];

    length = M_ReadFile(savename, &saveBuffer);
    // Skip the description field.
    savePtr = saveBuffer + SAVESTRINGSIZE;

    // Check version.
    memset(vcheck, 0, sizeof(vcheck));
    sprintf(vcheck, "version %i", SAVE_VERSION);
    if(strcmp(savePtr, vcheck))
    {
        int                 saveVer;

        sscanf(savePtr, "version %i", &saveVer);
        if(saveVer >= SAVE_VERSION_BASE)
        {
            // Must be from the wrong game.
            Con_Message("Bad savegame version.\n");
            return;
        }

        // Just give a warning.
        Con_Message("Savegame ID '%s': incompatible?\n", savePtr);
    }
    savePtr += VERSIONSIZE;

    gameSkill = *savePtr++;
    gameEpisode = *savePtr++;
    gameMap = *savePtr++;
    for(i = 0; i < 4; ++i)
        players[i].plr->inGame = *savePtr++;

    // Load a base level.
    G_InitNew(gameSkill, gameEpisode, gameMap);

    // Get the level time.
    a = *savePtr++;
    b = *savePtr++;
    c = *savePtr++;
    levelTime = (a << 16) + (b << 8) + c;

    // Dearchive all the modifications.
    P_v19_UnArchivePlayers();
    P_v19_UnArchiveWorld();
    P_v19_UnArchiveThinkers();
    P_v19_UnArchiveSpecials();

    if(*savePtr != 0x1d)
        Con_Error
            ("SV_v19_LoadGame: Bad savegame (consistency test failed!)\n");

    // Success!
    Z_Free(saveBuffer);
    saveBuffer = NULL;

    // Spawn particle generators.
    R_SetupLevel(DDSLM_AFTER_LOADING, 0);
}
