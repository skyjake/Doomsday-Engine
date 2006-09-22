/**\file
 *\section Copyright and License Summary
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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
 */

// HEADER FILES ------------------------------------------------------------

#include "jheretic.h"

#include "p_saveg.h"
#include "p_mapsetup.h"

// MACROS ------------------------------------------------------------------

#define VERSIONSIZE 16
#define SAVE_GAME_TERMINATOR 0x1d

#define SAVESTRINGSIZE 24

#define FF_FRAMEMASK        0x7fff

// TYPES -------------------------------------------------------------------

/*
 typedef enum {
    tc_end,
    tc_mobj
} thinkerclass_t;
*/

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern void SV_UpdateReadMobjFlags(mobj_t *mo, int ver);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

byte   *savebuffer;
byte   *save_p;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static long SV_v13_ReadLong()
{
    save_p += 4;
    return *(int *) (save_p - 4);
}

static void SV_v13_Read(void *data, int len)
{
    memcpy(data, save_p, len);
    save_p += len;
}

static void SV_v13_ReadPlayer(player_t *pl)
{
    byte    temp[12];
    ddplayer_t *ddpl = pl->plr;

    SV_v13_ReadLong();          // mo
    pl->playerstate = SV_v13_ReadLong();
    SV_v13_Read(temp, 10);      // ticcmd_t
    ddpl->viewz = SV_v13_ReadLong();
    ddpl->viewheight = SV_v13_ReadLong();
    ddpl->deltaviewheight = SV_v13_ReadLong();
    pl->bob = SV_v13_ReadLong();
    pl->flyheight = SV_v13_ReadLong();
    ddpl->lookdir = SV_v13_ReadLong();
    pl->centering = SV_v13_ReadLong();
    pl->health = SV_v13_ReadLong();
    pl->armorpoints = SV_v13_ReadLong();
    pl->armortype = SV_v13_ReadLong();
    SV_v13_Read(pl->inventory, 4 * 2 * 14);
    pl->readyArtifact = SV_v13_ReadLong();
    pl->artifactCount = SV_v13_ReadLong();
    pl->inventorySlotNum = SV_v13_ReadLong();
    SV_v13_Read(pl->powers, 4 * 9);
    SV_v13_Read(pl->keys, 4 * 3);
    pl->backpack = SV_v13_ReadLong();
    SV_v13_Read(pl->frags, 4 * 4);
    pl->readyweapon = SV_v13_ReadLong();
    pl->pendingweapon = SV_v13_ReadLong();
    SV_v13_Read(pl->weaponowned, 4 * 9);
    SV_v13_Read(pl->ammo, 4 * 6);
    SV_v13_Read(pl->maxammo, 4 * 6);
    pl->attackdown = SV_v13_ReadLong();
    pl->usedown = SV_v13_ReadLong();
    pl->cheats = SV_v13_ReadLong();
    pl->refire = SV_v13_ReadLong();
    pl->killcount = SV_v13_ReadLong();
    pl->itemcount = SV_v13_ReadLong();
    pl->secretcount = SV_v13_ReadLong();
    SV_v13_ReadLong();          // message, char*
    pl->damagecount = SV_v13_ReadLong();
    pl->bonuscount = SV_v13_ReadLong();
    pl->flamecount = SV_v13_ReadLong();
    SV_v13_ReadLong();          // attacker
    ddpl->extralight = SV_v13_ReadLong();
    ddpl->fixedcolormap = SV_v13_ReadLong();
    pl->colormap = SV_v13_ReadLong();
    SV_v13_Read(pl->psprites, 16 * 2);
    pl->didsecret = SV_v13_ReadLong();
    pl->morphTics = SV_v13_ReadLong();
    pl->chickenPeck = SV_v13_ReadLong();
    SV_v13_ReadLong();          // rain1
    SV_v13_ReadLong();          // rain2
}

static void SV_v13_ReadMobj(mobj_t *mo)
{
    // Clear everything first.
    memset(mo, 0, sizeof(*mo));

    // The thinker is 3 ints long.
    SV_v13_ReadLong();
    SV_v13_ReadLong();
    SV_v13_ReadLong();

    mo->pos[VX] = SV_v13_ReadLong();
    mo->pos[VY] = SV_v13_ReadLong();
    mo->pos[VZ] = SV_v13_ReadLong();

    // Sector links.
    SV_v13_ReadLong();
    SV_v13_ReadLong();

    mo->angle = SV_v13_ReadLong();
    mo->sprite = SV_v13_ReadLong();
    mo->frame = SV_v13_ReadLong();
    mo->frame &= ~FF_FRAMEMASK; // not used anymore.

    // Block links.
    SV_v13_ReadLong();
    SV_v13_ReadLong();

    // Subsector.
    SV_v13_ReadLong();

    mo->floorz = SV_v13_ReadLong();
    mo->ceilingz = SV_v13_ReadLong();
    mo->radius = SV_v13_ReadLong();
    mo->height = SV_v13_ReadLong();
    mo->momx = SV_v13_ReadLong();
    mo->momy = SV_v13_ReadLong();
    mo->momz = SV_v13_ReadLong();

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

    mo->spawninfo.pos[VX] = (fixed_t) (SV_v13_ReadLong() << FRACBITS);
    mo->spawninfo.pos[VY] = (fixed_t) (SV_v13_ReadLong() << FRACBITS);
    mo->spawninfo.pos[VZ] = ONFLOORZ;
    mo->spawninfo.angle = (angle_t) (ANG45 * (SV_v13_ReadLong() / 45));
    mo->spawninfo.type = (int) SV_v13_ReadLong();
    mo->spawninfo.options = (int) SV_v13_ReadLong();

    SV_UpdateReadMobjFlags(mo, 0);
}

void P_v13_UnArchivePlayers(void)
{
    int     i, j;

    for(i = 0; i < 4; i++)
    {
        if(!players[i].plr->ingame)
            continue;

        SV_v13_ReadPlayer(players + i);
        players[i].plr->mo = NULL;  // will be set when unarc thinker
        players[i].attacker = NULL;
        for(j = 0; j < NUMPSPRITES; j++)
            if(players[i].psprites[j].state)
                players[i].psprites[j].state =
                    &states[(int) players[i].psprites[j].state];
    }
}

void P_v13_UnArchiveWorld(void)
{
    int         i, j;
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
        xsec = P_XSector(sec);

        P_SetFixedp(sec, DMU_FLOOR_HEIGHT, *get++ << FRACBITS);
        P_SetFixedp(sec, DMU_CEILING_HEIGHT, *get++ << FRACBITS);
        P_SetIntp(sec, DMU_FLOOR_TEXTURE, *get++ + firstflat);
        P_SetIntp(sec, DMU_CEILING_TEXTURE, *get++ + firstflat);
        P_SetIntp(sec, DMU_LIGHT_LEVEL, *get++);
        xsec->special = *get++;  // needed?
        /*xsec->tag =*/ *get++;      // needed?
        xsec->specialdata = 0;
        xsec->soundtarget = 0;
    }

    // do lines
    for(i = 0; i < numlines; ++i)
    {
        line = P_ToPtr(DMU_LINE, i);
        xline = P_XLine(line);

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
            P_SetFixedp(sdef, DMU_TOP_TEXTURE_OFFSET_X, offx);
            P_SetFixedp(sdef, DMU_TOP_TEXTURE_OFFSET_Y, offy);
            P_SetFixedp(sdef, DMU_MIDDLE_TEXTURE_OFFSET_X, offx);
            P_SetFixedp(sdef, DMU_MIDDLE_TEXTURE_OFFSET_Y, offy);
            P_SetFixedp(sdef, DMU_BOTTOM_TEXTURE_OFFSET_X, offx);
            P_SetFixedp(sdef, DMU_BOTTOM_TEXTURE_OFFSET_Y, offy);
            P_SetIntp(sdef, DMU_TOP_TEXTURE, *get++);
            P_SetIntp(sdef, DMU_BOTTOM_TEXTURE, *get++);
            P_SetIntp(sdef, DMU_MIDDLE_TEXTURE, *get++);
        }
    }
    save_p = (byte *) get;
}

void P_v13_UnArchiveThinkers(void)
{
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
        case tc_end:
            return;             // end of list

        case tc_mobj:
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
            P_SetThingPosition(mobj);
            mobj->info = &mobjinfo[mobj->type];
            mobj->floorz = P_GetFixedp(mobj->subsector, DMU_FLOOR_HEIGHT);
            mobj->ceilingz = P_GetFixedp(mobj->subsector, DMU_CEILING_HEIGHT);
            mobj->thinker.function = P_MobjThinker;
            P_AddThinker(&mobj->thinker);
            break;

        default:
            Con_Error("Unknown tclass %i in savegame", tclass);
        }

    }

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
        switch (tclass)
        {
        case tc_endspecials:
            return;             // end of list

        case tc_ceiling:
            ceiling = Z_Malloc(sizeof(*ceiling), PU_LEVEL, NULL);
            memcpy(ceiling, save_p, sizeof(*ceiling));
            save_p += sizeof(*ceiling);

            ceiling->sector = P_ToPtr(DMU_SECTOR, (int) ceiling->sector);

            P_XSector(ceiling->sector)->specialdata = T_MoveCeiling;

            if(ceiling->thinker.function)
                ceiling->thinker.function = T_MoveCeiling;

            P_AddThinker(&ceiling->thinker);
            P_AddActiveCeiling(ceiling);
            break;

        case tc_door:
            door = Z_Malloc(sizeof(*door), PU_LEVEL, NULL);
            memcpy(door, save_p, sizeof(*door));
            save_p += sizeof(*door);

            door->sector = P_ToPtr(DMU_SECTOR, (int) door->sector);

            P_XSector(door->sector)->specialdata = T_VerticalDoor;
            door->thinker.function = T_VerticalDoor;
            P_AddThinker(&door->thinker);
            break;

        case tc_floor:
            floor = Z_Malloc(sizeof(*floor), PU_LEVEL, NULL);
            memcpy(floor, save_p, sizeof(*floor));
            save_p += sizeof(*floor);

            floor->sector = P_ToPtr(DMU_SECTOR, (int) floor->sector);
            P_XSector(floor->sector)->specialdata = T_MoveFloor;
            floor->thinker.function = T_MoveFloor;

            P_AddThinker(&floor->thinker);
            break;

        case tc_plat:
            plat = Z_Malloc(sizeof(*plat), PU_LEVEL, NULL);
            memcpy(plat, save_p, sizeof(*plat));
            save_p += sizeof(*plat);

            plat->sector = P_ToPtr(DMU_SECTOR, (int) plat->sector);
            P_XSector(plat->sector)->specialdata = T_PlatRaise;

            if(plat->thinker.function)
                plat->thinker.function = T_PlatRaise;

            P_AddThinker(&plat->thinker);
            P_AddActivePlat(plat);
            break;

        case tc_flash:
            flash = Z_Malloc(sizeof(*flash), PU_LEVEL, NULL);
            memcpy(flash, save_p, sizeof(*flash));
            save_p += sizeof(*flash);

            flash->sector = P_ToPtr(DMU_SECTOR, (int) flash->sector);
            flash->thinker.function = T_LightFlash;

            P_AddThinker(&flash->thinker);
            break;

        case tc_strobe:
            strobe = Z_Malloc(sizeof(*strobe), PU_LEVEL, NULL);
            memcpy(strobe, save_p, sizeof(*strobe));
            save_p += sizeof(*strobe);

            strobe->sector = P_ToPtr(DMU_SECTOR, (int) strobe->sector);
            strobe->thinker.function = T_StrobeFlash;

            P_AddThinker(&strobe->thinker);
            break;

        case tc_glow:
            glow = Z_Malloc(sizeof(*glow), PU_LEVEL, NULL);
            memcpy(glow, save_p, sizeof(*glow));
            save_p += sizeof(*glow);

            glow->sector = P_ToPtr(DMU_SECTOR, (int) glow->sector);
            glow->thinker.function = T_Glow;

            P_AddThinker(&glow->thinker);
            break;

        default:
            Con_Error("P_UnarchiveSpecials:Unknown tclass %i " "in savegame",
                      tclass);
        }
    }
}

/*
 * In Heretic's case this should actually be v13...
 */
void SV_v13_LoadGame(char *savename)
{
    int     length;
    int     i;
    int     a, b, c;
    char    vcheck[VERSIONSIZE];

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
    for(i = 0; i < 4; i++)
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
    {                           // Missing savegame termination marker
        Con_Error("Bad savegame");
    }
    Z_Free(savebuffer);

    // Spawn particle generators.
    R_SetupLevel("", DDSLF_AFTER_LOADING);
}
