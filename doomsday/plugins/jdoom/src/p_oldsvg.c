/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
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

#include "p_mapsetup.h"

// MACROS ------------------------------------------------------------------

#define PADSAVEP()      save_p += (4 - ((save_p - savebuffer) & 3)) & 3
#define SAVESTRINGSIZE  24
#define VERSIONSIZE     16

#define FF_FULLBRIGHT       0x8000 // used to be flag in thing->frame
#define FF_FRAMEMASK        0x7fff

#define SIZEOF_V19_THINKER_T 12
#define V19_THINKER_T_FUNC_OFFSET 8

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern void SV_UpdateReadMobjFlags(mobj_t *mo, int ver);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

byte   *save_p;
byte   *savebuffer;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static void SV_Read(void *data, int len)
{
    if(data)
        memcpy(data, save_p, len);
    save_p += len;
}

static short SV_ReadShort(void)
{
    save_p += 2;
    return *(short *) (save_p - 2);
}

static int SV_ReadLong(void)
{
    save_p += 4;
    return *(int *) (save_p - 4);
}

static void SV_ReadPlayer(player_t *pl)
{
    int     temp[3];

    SV_ReadLong();
    pl->playerstate = SV_ReadLong();
    SV_Read(temp, 8);
    pl->plr->viewZ = FIX2FLT(SV_ReadLong());
    pl->plr->viewheight = FIX2FLT(SV_ReadLong());
    pl->plr->deltaviewheight = FIX2FLT(SV_ReadLong());
    pl->bob = FLT2FIX(SV_ReadLong());
    pl->flyheight = 0;
    pl->health = SV_ReadLong();
    pl->armorpoints = SV_ReadLong();
    pl->armortype = SV_ReadLong();

    SV_Read(pl->powers, 6 * 4);
    SV_Read(pl->keys, 6 * 4);
    pl->backpack = SV_ReadLong();

    SV_Read(pl->frags, 4 * 4);
    pl->readyweapon = SV_ReadLong();
    pl->pendingweapon = SV_ReadLong();

    SV_Read(pl->weaponowned, 9 * 4);
    SV_Read(pl->ammo, 4 * 4);
    SV_Read(pl->maxammo, 4 * 4);

    pl->attackdown = SV_ReadLong();
    pl->usedown = SV_ReadLong();

    pl->cheats = SV_ReadLong();

    pl->refire = SV_ReadLong();

    pl->killcount = SV_ReadLong();
    pl->itemcount = SV_ReadLong();
    pl->secretcount = SV_ReadLong();

    SV_ReadLong();

    pl->damagecount = SV_ReadLong();
    pl->bonuscount = SV_ReadLong();

    SV_ReadLong();

    pl->plr->extraLight = SV_ReadLong();
    pl->plr->fixedcolormap = SV_ReadLong();
    pl->colormap = SV_ReadLong();
    SV_Read(pl->psprites, 2 * sizeof(pspdef_t));

    pl->didsecret = SV_ReadLong();
}

static void SV_ReadMobj(mobj_t *mo)
{
    // List: thinker links.
    SV_ReadLong();
    SV_ReadLong();
    SV_ReadLong();

    // Info for drawing: position.
    mo->pos[VX] = FIX2FLT(SV_ReadLong());
    mo->pos[VY] = FIX2FLT(SV_ReadLong());
    mo->pos[VZ] = FIX2FLT(SV_ReadLong());

    // More list: links in sector (if needed)
    SV_ReadLong();
    SV_ReadLong();

    //More drawing info: to determine current sprite.
    mo->angle = SV_ReadLong();  // orientation
    mo->sprite = SV_ReadLong(); // used to find patch_t and flip value
    mo->frame = SV_ReadLong();  // might be ORed with FF_FULLBRIGHT
    if(mo->frame & FF_FULLBRIGHT)
        mo->frame &= FF_FRAMEMASK; // not used anymore.

    // Interaction info, by BLOCKMAP.
    // Links in blocks (if needed).
    SV_ReadLong();
    SV_ReadLong();
    SV_ReadLong();

    // The closest interval over all contacted Sectors.
    mo->floorz = FIX2FLT(SV_ReadLong());
    mo->ceilingz = FIX2FLT(SV_ReadLong());

    // For movement checking.
    mo->radius = FIX2FLT(SV_ReadLong());
    mo->height = FIX2FLT(SV_ReadLong());

    // Momentums, used to update position.
    mo->mom[MX] = FIX2FLT(SV_ReadLong());
    mo->mom[MY] = FIX2FLT(SV_ReadLong());
    mo->mom[MZ] = FIX2FLT(SV_ReadLong());

    // If == validcount, already checked.
    mo->valid = SV_ReadLong();

    mo->type = SV_ReadLong();
    mo->info = &mobjinfo[mo->type];
    SV_ReadLong();              // &mobjinfo[mobj->type]

    mo->tics = SV_ReadLong();   // state tic counter
    mo->state = (state_t *) SV_ReadLong();
    mo->damage = DDMAXINT; // Use damage set in mo->info->damage
    mo->flags = SV_ReadLong();
    mo->health = SV_ReadLong();

    // Movement direction, movement generation (zig-zagging).
    mo->movedir = SV_ReadLong();    // 0-7
    mo->movecount = SV_ReadLong();  // when 0, select a new dir

    // Thing being chased/attacked (or NULL),
    // also the originator for missiles.
    SV_ReadLong();

    // Reaction time: if non 0, don't attack yet.
    // Used by player to freeze a bit after teleporting.
    mo->reactiontime = SV_ReadLong();

    // If >0, the target will be chased
    // no matter what (even if shot)
    mo->threshold = SV_ReadLong();

    // Additional info record for player avatars only.
    // Only valid if type == MT_PLAYER
    mo->player = (player_t *) SV_ReadLong();

    // Player number last looked for.
    mo->lastlook = SV_ReadLong();

    // For nightmare respawn.
    mo->spawnspot.pos[VX] = (float) SV_ReadShort();
    mo->spawnspot.pos[VY] = (float) SV_ReadShort();
    mo->spawnspot.pos[VZ] = ONFLOORZ;
    mo->spawnspot.angle = (angle_t) (ANG45 * ((int)SV_ReadShort() / 45));
    mo->spawnspot.type = (int) SV_ReadShort();
    mo->spawnspot.flags = (int) SV_ReadShort();

    // Thing being chased/attacked for tracers.
    SV_ReadLong();

    SV_UpdateReadMobjFlags(mo, 0);
}

void P_v19_UnArchivePlayers(void)
{
    int     i;
    int     j;

    for(i = 0; i < 4; i++)
    {
        if(!players[i].plr->ingame)
            continue;

        PADSAVEP();

        //memcpy (&players[i],save_p, sizeof(player_t));

        //P_PlayerConverter(&players[i], (saveplayer_t*) save_p, false);
        SV_ReadPlayer(players + i);
        //save_p += sizeof(saveplayer_t);

        // will be set when unarc thinker
        players[i].plr->mo = NULL;
        players[i].attacker = NULL;

        for(j = 0; j < NUMPSPRITES; j++)
        {
            if(players[i].psprites[j].state)
            {
                players[i].psprites[j].state =
                    &states[(int) players[i].psprites[j].state];
            }
        }
    }
}

void P_v19_UnArchiveWorld(void)
{
    uint        i, j;
    fixed_t     offx, offy;
    short      *get;
    int         firstflat = W_CheckNumForName("F_START") + 1;
    sector_t   *sec;
    xsector_t  *xsec;
    line_t     *line;
    xline_t    *xline;

    get = (short *) save_p;

    // do sectors
    for(i = 0; i < numsectors; ++i)
    {
        sec = P_ToPtr(DMU_SECTOR, i);
        xsec = P_ToXSector(sec);

        P_SetFixedp(sec, DMU_FLOOR_HEIGHT, *get++ << FRACBITS);
        P_SetFixedp(sec, DMU_CEILING_HEIGHT, *get++ << FRACBITS);
        P_SetIntp(sec, DMU_FLOOR_MATERIAL, *get++ + firstflat);
        P_SetIntp(sec, DMU_CEILING_MATERIAL, *get++ + firstflat);
        P_SetFloatp(sec, DMU_LIGHT_LEVEL, (float) (*get++) / 255.0f);
        xsec->special = *get++;  // needed?
        /*xsec->tag =*/ *get++;      // needed?
        xsec->specialdata = 0;
        xsec->soundtarget = 0;
    }

    // do lines
    for(i = 0; i < numlines; ++i)
    {
        line = P_ToPtr(DMU_LINE, i);
        xline = P_ToXLine(line);

        P_SetIntp(line, DMU_FLAGS, *get++);
        xline->special = *get++;
        /*xline->tag =*/ *get++;

        for(j = 0; j < 2; ++j)
        {
            side_t* sdef = P_GetPtrp(line, (j? DMU_SIDE1:DMU_SIDE0));

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
            P_SetIntp(sdef, DMU_TOP_MATERIAL, *get++);
            P_SetIntp(sdef, DMU_BOTTOM_MATERIAL, *get++);
            P_SetIntp(sdef, DMU_MIDDLE_MATERIAL, *get++);
        }
    }
    save_p = (byte *) get;
}

void P_v19_UnArchiveThinkers(void)
{
enum thinkerclass_e {
    TC_END,
    TC_MOBJ
};

    byte    tclass;
    thinker_t *currentthinker;
    thinker_t *next;
    mobj_t *mobj;

    // remove all the current thinkers
    currentthinker = thinkercap.next;
    while(currentthinker != &thinkercap)
    {
        next = currentthinker->next;

        if(currentthinker->function == P_MobjThinker)
            P_RemoveMobj((mobj_t *) currentthinker);
        else
            Z_Free(currentthinker);

        currentthinker = next;
    }
    P_InitThinkers();

    // read in saved thinkers
    for(;;)
    {
        tclass = *save_p++;
        switch(tclass)
        {
        case TC_END:
            return;             // end of list

        case TC_MOBJ:
            PADSAVEP();
            mobj = Z_Calloc(sizeof(*mobj), PU_LEVEL, NULL);

            SV_ReadMobj(mobj);

            mobj->state = &states[(int) mobj->state];
            mobj->target = NULL;
            if(mobj->player)
            {
                int     pnum = (int) mobj->player - 1;

                mobj->player = &players[pnum];
                mobj->dplayer = mobj->player->plr;
                mobj->dplayer->mo = mobj;
                //mobj->dplayer->clAngle = mobj->angle; /* $unifiedangles */
                mobj->dplayer->lookdir = 0; /* $unifiedangles */
            }
            P_SetMobjPosition(mobj);
            mobj->info = &mobjinfo[mobj->type];
            mobj->floorz =
                P_GetFloatp(mobj->subsector, DMU_FLOOR_HEIGHT);
            mobj->ceilingz =
                P_GetFloatp(mobj->subsector, DMU_CEILING_HEIGHT);
            mobj->thinker.function = P_MobjThinker;
            P_AddThinker(&mobj->thinker);
            break;

        default:
            Con_Error("Unknown tclass %i in savegame", tclass);
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
    byte temp[SIZEOF_V19_THINKER_T];

    // Padding at the start (an old thinker_t struct)
    SV_Read(&temp, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    ceiling->type = SV_ReadLong();

    // A 32bit pointer to sector, serialized.
    ceiling->sector = P_ToPtr(DMU_SECTOR, SV_ReadLong());
    if(!ceiling->sector)
        Con_Error("tc_ceiling: bad sector number\n");

    ceiling->bottomheight = FIX2FLT(SV_ReadLong());
    ceiling->topheight = FIX2FLT(SV_ReadLong());
    ceiling->speed = FIX2FLT(SV_ReadLong());
    ceiling->crush = SV_ReadLong();
    ceiling->direction = SV_ReadLong();
    ceiling->tag = SV_ReadLong();
    ceiling->olddirection = SV_ReadLong();

    if(temp + V19_THINKER_T_FUNC_OFFSET)
        ceiling->thinker.function = T_MoveCeiling;

    P_ToXSector(ceiling->sector)->specialdata = ceiling;
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

    door->topheight = FIX2FLT(SV_ReadLong());
    door->speed = FIX2FLT(SV_ReadLong());
    door->direction = SV_ReadLong();
    door->topwait = SV_ReadLong();
    door->topcountdown = SV_ReadLong();

    door->thinker.function = T_VerticalDoor;

    P_ToXSector(door->sector)->specialdata = door;
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
    floor->newspecial = SV_ReadLong();
    floor->texture = SV_ReadShort();
    floor->floordestheight = FIX2FLT(SV_ReadLong());
    floor->speed = FIX2FLT(SV_ReadLong());

    floor->thinker.function = T_MoveFloor;

    P_ToXSector(floor->sector)->specialdata = floor;
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
    byte temp[SIZEOF_V19_THINKER_T];

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
    plat->oldstatus = SV_ReadLong();
    plat->crush = SV_ReadLong();
    plat->tag = SV_ReadLong();
    plat->type = SV_ReadLong();

    if(temp + V19_THINKER_T_FUNC_OFFSET)
        plat->thinker.function = T_PlatRaise;

    P_ToXSector(plat->sector)->specialdata = plat;
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
    flash->maxlight = (float) SV_ReadLong() / 255.0f;
    flash->minlight = (float) SV_ReadLong() / 255.0f;
    flash->maxtime = SV_ReadLong();
    flash->mintime = SV_ReadLong();

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
    strobe->minlight = (float) SV_ReadLong() / 255.0f;
    strobe->maxlight = (float) SV_ReadLong() / 255.0f;
    strobe->darktime = SV_ReadLong();
    strobe->brighttime = SV_ReadLong();

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

    glow->minlight = (float) SV_ReadLong() / 255.0f;
    glow->maxlight = (float) SV_ReadLong() / 255.0f;
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

    byte    tclass;
    ceiling_t *ceiling;
    vldoor_t *door;
    floormove_t *floor;
    plat_t *plat;
    lightflash_t *flash;
    strobe_t *strobe;
    glow_t *glow;

    // read in saved thinkers
    for(;;)
    {
        tclass = *save_p++;
        switch(tclass)
        {
        case tc_endspecials:
            return;             // end of list

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
                      tclass);
        }
    }
}

void SV_v19_LoadGame(char *savename)
{
    size_t  length;
    int     i;
    int     a, b, c;
    char    vcheck[VERSIONSIZE];

    length = M_ReadFile(savename, &savebuffer);
    // Skip the description field.
    save_p = savebuffer + SAVESTRINGSIZE;
    // Check version.
    memset(vcheck, 0, sizeof(vcheck));
    sprintf(vcheck, "version %i", SAVE_VERSION);
    if(strcmp(save_p, vcheck))
    {
        int     saveVer;

        sscanf(save_p, "version %i", &saveVer);
        if(saveVer >= SAVE_VERSION_BASE)
        {
            // Must be from the wrong game.
            Con_Message("Bad savegame version.\n");
            return;
        }
        // Just give a warning.
        Con_Message("Savegame ID '%s': incompatible?\n", save_p);
    }
    save_p += VERSIONSIZE;

    gameskill = *save_p++;
    gameepisode = *save_p++;
    gamemap = *save_p++;
    for(i = 0; i < 4; i++)
        players[i].plr->ingame = *save_p++;

    // Load a base level.
    G_InitNew(gameskill, gameepisode, gamemap);

    // get the times
    a = *save_p++;
    b = *save_p++;
    c = *save_p++;
    leveltime = (a << 16) + (b << 8) + c;

    // dearchive all the modifications
    P_v19_UnArchivePlayers();
    P_v19_UnArchiveWorld();
    P_v19_UnArchiveThinkers();
    P_v19_UnArchiveSpecials();

    if(*save_p != 0x1d)
        Con_Error
            ("SV_v19_LoadGame: Bad savegame (consistency test failed!)\n");

    // done
    Z_Free(savebuffer);
    savebuffer = NULL;

    // Spawn particle generators.
    R_SetupLevel(DDSLM_AFTER_LOADING, 0);
}
