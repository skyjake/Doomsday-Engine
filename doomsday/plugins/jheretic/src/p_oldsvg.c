/**\file
 *\section License
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
 *
 * This program is covered by the HERETIC / HEXEN (LIMITED USE) source
 * code license; you can redistribute it and/or modify it under the terms
 * of the HERETIC / HEXEN source code license as published by Activision.
 *
 * THIS MATERIAL IS NOT MADE OR SUPPORTED BY ACTIVISION.
 *
 * WARRANTY INFORMATION.
 * This program is provided as is. Activision and it's affiliates make no
 * warranties of any kind, whether oral or written , express or implied,
 * including any warranty of merchantability, fitness for a particular
 * purpose or non-infringement, and no other representations or claims of
 * any kind shall be binding on or obligate Activision or it's affiliates.
 *
 * LICENSE CONDITIONS.
 * You shall not:
 *
 * 1) Exploit this Program or any of its parts commercially.
 * 2) Use this Program, or permit use of this Program, on more than one
 *    computer, computer terminal, or workstation at the same time.
 * 3) Make copies of this Program or any part thereof, or make copies of
 *    the materials accompanying this Program.
 * 4) Use the program, or permit use of this Program, in a network,
 *    multi-user arrangement or remote access arrangement, including any
 *    online use, except as otherwise explicitly provided by this Program.
 * 5) Sell, rent, lease or license any copies of this Program, without
 *    the express prior written consent of Activision.
 * 6) Remove, disable or circumvent any proprietary notices or labels
 *    contained on or within the Program.
 *
 * You should have received a copy of the HERETIC / HEXEN source code
 * license along with this program (Ravenlic.txt); if not:
 * http://www.ravensoft.com/
 *
 * \bug Not 64bit clean: In function 'P_v13_UnArchivePlayers': cast from pointer to integer of different size
 * \bug Not 64bit clean: In function 'P_v13_UnArchiveThinkers': cast from pointer to integer of different size
 */

/**
 * p_oldsvg.c:
 */

// HEADER FILES ------------------------------------------------------------

#include "jheretic.h"

#include "p_saveg.h"
#include "p_map.h"
#include "p_mapsetup.h"

// MACROS ------------------------------------------------------------------

#define VERSIONSIZE             16
#define SAVE_GAME_TERMINATOR    0x1d

#define SAVESTRINGSIZE          24

#define FF_FRAMEMASK            0x7fff

#define SIZEOF_V13_THINKER_T    12
#define V13_THINKER_T_FUNC_OFFSET 8

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern void SV_UpdateReadMobjFlags(mobj_t *mo, int ver);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

byte *savebuffer;
byte *save_p;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

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

static void SV_v13_Read(void *data, int len)
{
    if(data)
        memcpy(data, save_p, len);
    save_p += len;
}

static void SV_v13_ReadPlayer(player_t *pl)
{
    int         i;
    byte        temp[12];
    ddplayer_t *ddpl = pl->plr;

    SV_v13_ReadLong(); // mo
    pl->playerstate = SV_v13_ReadLong();
    SV_v13_Read(temp, 10); // ticcmd_t
    ddpl->viewZ = FIX2FLT(SV_v13_ReadLong());
    ddpl->viewheight = FIX2FLT(SV_v13_ReadLong());
    ddpl->deltaviewheight = FIX2FLT(SV_v13_ReadLong());
    pl->bob = FIX2FLT(SV_v13_ReadLong());
    pl->flyheight = SV_v13_ReadLong();
    ddpl->lookdir = SV_v13_ReadLong();
    pl->centering = SV_v13_ReadLong();
    pl->health = SV_v13_ReadLong();
    pl->armorpoints = SV_v13_ReadLong();
    pl->armortype = SV_v13_ReadLong();

    //// \fixme Assumes inventory size.
    SV_v13_Read(pl->inventory, 4 * 2 * 14);

    pl->readyArtifact = SV_v13_ReadLong();
    pl->artifactCount = SV_v13_ReadLong();
    pl->inventorySlotNum = SV_v13_ReadLong();

    //// \fixme Assumes powers size.
    SV_v13_Read(pl->powers, 4 * 9);

    //// \fixme Assumes keys size.
    SV_v13_Read(pl->keys, 4 * 3);

    pl->backpack = SV_v13_ReadLong();

    //// \fixme Assumes frags size.
    SV_v13_Read(pl->frags, 4 * 4);
    pl->readyweapon = SV_v13_ReadLong();
    pl->pendingweapon = SV_v13_ReadLong();

    //// \fixme Assumes owned weapon size.
    SV_v13_Read(pl->weaponowned, 4 * 9);

    //// \fixme Assumes ammo size.
    SV_v13_Read(pl->ammo, 4 * 6);

    //// \fixme Assumes maxammo size.
    SV_v13_Read(pl->maxammo, 4 * 6);
    pl->attackdown = SV_v13_ReadLong();
    pl->usedown = SV_v13_ReadLong();
    pl->cheats = SV_v13_ReadLong();
    pl->refire = SV_v13_ReadLong();
    pl->killcount = SV_v13_ReadLong();
    pl->itemcount = SV_v13_ReadLong();
    pl->secretcount = SV_v13_ReadLong();
    SV_v13_ReadLong(); // message, char*
    pl->damagecount = SV_v13_ReadLong();
    pl->bonuscount = SV_v13_ReadLong();
    pl->flamecount = SV_v13_ReadLong();
    SV_v13_ReadLong(); // attacker
    ddpl->extraLight = SV_v13_ReadLong();
    ddpl->fixedcolormap = SV_v13_ReadLong();
    pl->colormap = SV_v13_ReadLong();
    for(i = 0; i < 2; ++i)
    {
        pspdef_t       *psp = &pl->psprites[i];

        psp->state = (state_t*) SV_v13_ReadLong();
        psp->pos[VX] = SV_v13_ReadLong();
        psp->pos[VY] = SV_v13_ReadLong();
        psp->tics = SV_v13_ReadLong();
    }

    pl->didsecret = SV_v13_ReadLong();
    pl->morphTics = SV_v13_ReadLong();
    pl->chickenPeck = SV_v13_ReadLong();
    SV_v13_ReadLong(); // rain1
    SV_v13_ReadLong(); // rain2
}

static void SV_v13_ReadMobj(mobj_t *mo)
{
    // Clear everything first.
    memset(mo, 0, sizeof(*mo));

    // The thinker was 3 ints long.
    SV_v13_ReadLong();
    SV_v13_ReadLong();
    SV_v13_ReadLong();

    mo->pos[VX] = FIX2FLT(SV_v13_ReadLong());
    mo->pos[VY] = FIX2FLT(SV_v13_ReadLong());
    mo->pos[VZ] = FIX2FLT(SV_v13_ReadLong());

    // Sector links.
    SV_v13_ReadLong();
    SV_v13_ReadLong();

    mo->angle = (angle_t) (ANG45 * (SV_v13_ReadLong() / 45));
    mo->sprite = SV_v13_ReadLong();
    mo->frame = SV_v13_ReadLong();
    mo->frame &= ~FF_FRAMEMASK; // not used anymore.

    // Block links.
    SV_v13_ReadLong();
    SV_v13_ReadLong();

    // Subsector.
    SV_v13_ReadLong();

    mo->floorz = FIX2FLT(SV_v13_ReadLong());
    mo->ceilingz = FIX2FLT(SV_v13_ReadLong());
    mo->radius = FIX2FLT(SV_v13_ReadLong());
    mo->height = FIX2FLT(SV_v13_ReadLong());
    mo->mom[MX] = FIX2FLT(SV_v13_ReadLong());
    mo->mom[MY] = FIX2FLT(SV_v13_ReadLong());
    mo->mom[MZ] = FIX2FLT(SV_v13_ReadLong());

    mo->valid = SV_v13_ReadLong();

    mo->type = SV_v13_ReadLong();
    SV_v13_ReadLong();          // info
    mo->tics = SV_v13_ReadLong();
    mo->state = (state_t *) SV_v13_ReadLong();
    mo->damage = SV_v13_ReadLong();
    mo->flags = SV_v13_ReadLong();
    mo->flags2 = SV_v13_ReadLong();
    mo->special1 = SV_v13_ReadLong();
    mo->special2 = SV_v13_ReadLong();
    mo->health = SV_v13_ReadLong();
    mo->movedir = SV_v13_ReadLong();
    mo->movecount = SV_v13_ReadLong();
    SV_v13_ReadLong();          // target
    mo->reactiontime = SV_v13_ReadLong();
    mo->threshold = SV_v13_ReadLong();
    mo->player = (player_t *) SV_v13_ReadLong();
    mo->lastlook = SV_v13_ReadLong();

    mo->spawnspot.pos[VX] = (float) SV_v13_ReadLong();
    mo->spawnspot.pos[VY] = (float) SV_v13_ReadLong();
    mo->spawnspot.pos[VZ] = ONFLOORZ;
    mo->spawnspot.angle = (angle_t) (ANG45 * (SV_v13_ReadLong() / 45));
    mo->spawnspot.type = (int) SV_v13_ReadLong();
    mo->spawnspot.flags = (int) SV_v13_ReadLong();

    SV_UpdateReadMobjFlags(mo, 0);
}

void P_v13_UnArchivePlayers(void)
{
    int         i, j;

    for(i = 0; i < 4; ++i)
    {
        if(!players[i].plr->ingame)
            continue;

        SV_v13_ReadPlayer(players + i);
        players[i].plr->mo = NULL; // Will be set when unarc thinker.
        players[i].attacker = NULL;
        for(j = 0; j < NUMPSPRITES; ++j)
        {
            if(players[i].psprites[j].state)
            {
                players[i].psprites[j].state =
                    &states[(int) players[i].psprites[j].state];
            }
        }
    }
}

void P_v13_UnArchiveWorld(void)
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

    // Do sectors.
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

    // Do lines.
    for(i = 0; i < numlines; ++i)
    {
        line = P_ToPtr(DMU_LINE, i);
        xline = P_ToXLine(line);

        P_SetIntp(line, DMU_FLAGS, *get++);
        xline->special = *get++;
        /*xline->tag =*/ *get++;

        for(j = 0; j < 2; j++)
        {
            side_t* sdef;

            if(j == 0)
                sdef = P_GetPtrp(line, DMU_SIDE0);
            else
                sdef = P_GetPtrp(line, DMU_SIDE1);

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

void P_v13_UnArchiveThinkers(void)
{
typedef enum
{
	TC_END,
	TC_MOBJ
} thinkerclass_t;

    byte    tclass;
    thinker_t *currentthinker, *next;
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
        switch (tclass)
        {
        case TC_END:
            return;             // end of list

        case TC_MOBJ:
            mobj = Z_Malloc(sizeof(*mobj), PU_LEVEL, NULL);

            SV_v13_ReadMobj(mobj);
            mobj->state = &states[(int) mobj->state];
            mobj->target = NULL;
            if(mobj->player)
            {
                mobj->player = &players[(int) mobj->player - 1];
                mobj->player->plr->mo = mobj;
                mobj->player->plr->mo->dplayer = mobj->player->plr;
                //mobj->player->plr->clAngle = mobj->angle; /* $unifiedangles */
                //mobj->player->plr->clLookDir = mobj->player->plr->lookdir; /* $unifiedangles */
            }
            P_SetMobjPosition(mobj);
            mobj->info = &mobjinfo[mobj->type];
            mobj->floorz = P_GetFloatp(mobj->subsector, DMU_FLOOR_HEIGHT);
            mobj->ceilingz = P_GetFloatp(mobj->subsector, DMU_CEILING_HEIGHT);
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
/* Original Heretic format:
typedef struct {
	thinker_t	thinker;        // was 12 bytes
	ceiling_e	type;           // was 32bit int
	sector_t	*sector;
	fixed_t		bottomheight, topheight;
	fixed_t		speed;
	boolean		crush;
	int			direction;		// 1 = up, 0 = waiting, -1 = down
	int			tag;			// ID
	int			olddirection;
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

    ceiling->bottomheight = FIX2FLT(SV_v13_ReadLong());
    ceiling->topheight = FIX2FLT(SV_v13_ReadLong());
    ceiling->speed = FIX2FLT(SV_v13_ReadLong());
    ceiling->crush = SV_v13_ReadLong();
    ceiling->direction = SV_v13_ReadLong();
    ceiling->tag = SV_v13_ReadLong();
    ceiling->olddirection = SV_v13_ReadLong();

    if(temp + V13_THINKER_T_FUNC_OFFSET)
        ceiling->thinker.function = T_MoveCeiling;

    P_ToXSector(ceiling->sector)->specialdata = T_MoveCeiling;
    return true; // Add this thinker.
}

static int SV_ReadDoor(vldoor_t *door)
{
/* Original Heretic format:
typedef struct {
	thinker_t	thinker;        // was 12 bytes
	vldoor_e	type;           // was 32bit int
	sector_t	*sector;
	fixed_t		topheight;
	fixed_t		speed;
	int			direction;		// 1 = up, 0 = waiting at top, -1 = down
	int			topwait;		// tics to wait at the top
								// (keep in case a door going down is reset)
	int			topcountdown;	// when it reaches 0, start going down
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

    door->topheight = FIX2FLT(SV_v13_ReadLong());
    door->speed = FIX2FLT(SV_v13_ReadLong());
    door->direction = SV_v13_ReadLong();
    door->topwait = SV_v13_ReadLong();
    door->topcountdown = SV_v13_ReadLong();

    door->thinker.function = T_VerticalDoor;

    P_ToXSector(door->sector)->specialdata = T_VerticalDoor;
    return true; // Add this thinker.
}

static int SV_ReadFloor(floormove_t *floor)
{
/* Original Heretic format:
typedef struct {
	thinker_t	thinker;        // was 12 bytes
	floor_e		type;           // was 32bit int
	boolean		crush;
	sector_t	*sector;
	int			direction;
	int			newspecial;
	short		texture;
	fixed_t		floordestheight;
	fixed_t		speed;
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

    floor->direction = SV_v13_ReadLong();
    floor->newspecial = SV_v13_ReadLong();
    floor->texture = SV_v13_ReadShort();
    floor->floordestheight = FIX2FLT(SV_v13_ReadLong());
    floor->speed = FIX2FLT(SV_v13_ReadLong());

    floor->thinker.function = T_MoveFloor;

    P_ToXSector(floor->sector)->specialdata = T_MoveFloor;
    return true; // Add this thinker.
}

static int SV_ReadPlat(plat_t *plat)
{
/* Original Heretic format:
typedef struct {
	thinker_t	thinker;        // was 12 bytes
	sector_t	*sector;
	fixed_t		speed;
	fixed_t		low;
	fixed_t		high;
	int			wait;
	int			count;
	plat_e		status;         // was 32bit int
	plat_e		oldstatus;      // was 32bit int
	boolean		crush;
	int			tag;
	plattype_e	type;           // was 32bit int
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
    plat->status = SV_v13_ReadLong();
    plat->oldstatus = SV_v13_ReadLong();
    plat->crush = SV_v13_ReadLong();
    plat->tag = SV_v13_ReadLong();
    plat->type = SV_v13_ReadLong();

    if(temp + V13_THINKER_T_FUNC_OFFSET)
        plat->thinker.function = T_PlatRaise;

    P_ToXSector(plat->sector)->specialdata = T_PlatRaise;
    return true; // Add this thinker.
}

static int SV_ReadFlash(lightflash_t *flash)
{
/* Original Heretic format:
typedef struct {
	thinker_t	thinker;        // was 12 bytes
	sector_t	*sector;
	int			count;
	int			maxlight;
	int			minlight;
	int			maxtime;
	int			mintime;
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
    flash->maxlight = (float) SV_v13_ReadLong() / 255.0f;
    flash->minlight = (float) SV_v13_ReadLong() / 255.0f;
    flash->maxtime = SV_v13_ReadLong();
    flash->mintime = SV_v13_ReadLong();

    flash->thinker.function = T_LightFlash;
    return true; // Add this thinker.
}

static int SV_ReadStrobe(strobe_t *strobe)
{
/* Original Heretic format:
typedef struct {
	thinker_t	thinker;        // was 12 bytes
	sector_t	*sector;
	int			count;
	int			minlight;
	int			maxlight;
	int			darktime;
	int			brighttime;
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
    strobe->minlight = (float) SV_v13_ReadLong() / 255.0f;
    strobe->maxlight = (float) SV_v13_ReadLong() / 255.0f;
    strobe->darktime = SV_v13_ReadLong();
    strobe->brighttime = SV_v13_ReadLong();

    strobe->thinker.function = T_StrobeFlash;
    return true; // Add this thinker.
}

static int SV_ReadGlow(glow_t *glow)
{
/* Original Heretic format:
typedef struct {
	thinker_t	thinker;        // was 12 bytes
	sector_t	*sector;
	int			minlight;
	int			maxlight;
	int			direction;
} v13_glow_t;
*/
    // Padding at the start (an old thinker_t struct)
    SV_v13_Read(NULL, SIZEOF_V13_THINKER_T);

    // Start of used data members.
    // A 32bit pointer to sector, serialized.
    glow->sector = P_ToPtr(DMU_SECTOR, SV_v13_ReadLong());
    if(!glow->sector)
        Con_Error("tc_glow: bad sector number\n");

    glow->minlight = (float) SV_v13_ReadLong() / 255.0f;
    glow->maxlight = (float) SV_v13_ReadLong() / 255.0f;
    glow->direction = SV_v13_ReadLong();

    glow->thinker.function = T_Glow;
    return true; // Add this thinker.
}

/**
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
    vldoor_t   *door;
    floormove_t *floor;
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
            ceiling = Z_Malloc(sizeof(*ceiling), PU_LEVEL, NULL);

            SV_ReadCeiling(ceiling);

            P_AddThinker(&ceiling->thinker);
            P_AddActiveCeiling(ceiling);
            break;

        case tc_door:
            door = Z_Malloc(sizeof(*door), PU_LEVEL, NULL);

            SV_ReadDoor(door);

            P_AddThinker(&door->thinker);
            break;

        case tc_floor:
            floor = Z_Malloc(sizeof(*floor), PU_LEVEL, NULL);

            SV_ReadFloor(floor);

            P_AddThinker(&floor->thinker);
            break;

        case tc_plat:
            plat = Z_Malloc(sizeof(*plat), PU_LEVEL, NULL);

            SV_ReadPlat(plat);

            P_AddThinker(&plat->thinker);
            P_AddActivePlat(plat);
            break;

        case tc_flash:
            flash = Z_Malloc(sizeof(*flash), PU_LEVEL, NULL);

            SV_ReadFlash(flash);

            P_AddThinker(&flash->thinker);
            break;

        case tc_strobe:
            strobe = Z_Malloc(sizeof(*strobe), PU_LEVEL, NULL);

            SV_ReadStrobe(strobe);

            P_AddThinker(&strobe->thinker);
            break;

        case tc_glow:
            glow = Z_Malloc(sizeof(*glow), PU_LEVEL, NULL);

            SV_ReadGlow(glow);

            P_AddThinker(&glow->thinker);
            break;

        default:
            Con_Error("P_UnarchiveSpecials:Unknown tclass %i " "in savegame",
                      tclass);
        }
    }
}

void SV_v13_LoadGame(char *savename)
{
    size_t      length;
    int         i, a, b, c;
    char        vcheck[VERSIONSIZE];

    length = M_ReadFile(savename, &savebuffer);
    save_p = savebuffer + SAVESTRINGSIZE;

    // Skip the description field
    memset(vcheck, 0, sizeof(vcheck));
    sprintf(vcheck, "version %i", SAVE_VERSION);
    if(strcmp(save_p, vcheck))
    {                           // Bad version
        Con_Message("Savegame ID '%s': incompatible?\n", save_p);
    }
    save_p += VERSIONSIZE;
    gameskill = *save_p++;
    gameepisode = *save_p++;
    gamemap = *save_p++;

    for(i = 0; i < 4; ++i)
    {
        players[i].plr->ingame = *save_p++;
    }

    // Load a base level
    G_InitNew(gameskill, gameepisode, gamemap);

    // Create leveltime
    a = *save_p++;
    b = *save_p++;
    c = *save_p++;
    leveltime = (a << 16) + (b << 8) + c;

    // De-archive all the modifications
    P_v13_UnArchivePlayers();
    P_v13_UnArchiveWorld();
    P_v13_UnArchiveThinkers();
    P_v13_UnArchiveSpecials();

    if(*save_p != SAVE_GAME_TERMINATOR)
        Con_Error("Bad savegame"); // Missing savegame termination marker

    Z_Free(savebuffer);

    // Spawn particle generators.
    R_SetupLevel(DDSLM_AFTER_LOADING, 0);
}
