/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
 *\section License Text
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
 */

// HEADER FILES ------------------------------------------------------------

#include "jdoom.h"

#include "p_mapsetup.h"

// MACROS ------------------------------------------------------------------

#define PADSAVEP()      save_p += (4 - ((save_p - savebuffer) & 3)) & 3
#define SAVESTRINGSIZE  24
#define VERSIONSIZE     16

#define FF_FRAMEMASK        0x7fff

// TYPES -------------------------------------------------------------------

typedef enum thinkerclass_e {
    tc_end,
    tc_mobj
} thinkerclass_t;

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
    memcpy(data, save_p, len);
    save_p += len;
}

static int SV_ReadLong()
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
    pl->plr->viewz = SV_ReadLong();
    pl->plr->viewheight = SV_ReadLong();
    pl->plr->deltaviewheight = SV_ReadLong();
    pl->bob = SV_ReadLong();
    pl->flyheight = 0;
    pl->health = SV_ReadLong();
    pl->armorpoints = SV_ReadLong();
    pl->armortype = SV_ReadLong();

    SV_Read(pl->powers, NUMPOWERS * 4);
    SV_Read(pl->keys, NUMKEYS * 4);
    pl->backpack = SV_ReadLong();

    SV_Read(pl->frags, 4 * 4);
    pl->readyweapon = SV_ReadLong();
    pl->pendingweapon = SV_ReadLong();

    SV_Read(pl->weaponowned, NUMWEAPONS * 4);
    SV_Read(pl->ammo, NUMAMMO * 4);
    SV_Read(pl->maxammo, NUMAMMO * 4);

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

    pl->plr->extralight = SV_ReadLong();
    pl->plr->fixedcolormap = SV_ReadLong();
    pl->colormap = SV_ReadLong();
    SV_Read(pl->psprites, NUMPSPRITES * sizeof(pspdef_t));

    pl->didsecret = SV_ReadLong();
}

static void SV_ReadMobj(mobj_t *mo)
{
    // List: thinker links.
    SV_ReadLong();
    SV_ReadLong();
    SV_ReadLong();

    // Info for drawing: position.
    mo->pos[VX] = SV_ReadLong();
    mo->pos[VY] = SV_ReadLong();
    mo->pos[VZ] = SV_ReadLong();

    // More list: links in sector (if needed)
    SV_ReadLong();
    SV_ReadLong();

    //More drawing info: to determine current sprite.
    mo->angle = SV_ReadLong();  // orientation
    mo->sprite = SV_ReadLong(); // used to find patch_t and flip value
    mo->frame = SV_ReadLong();  // might be ORed with FF_FULLBRIGHT
    mo->frame &= ~FF_FRAMEMASK;

    // Interaction info, by BLOCKMAP.
    // Links in blocks (if needed).
    SV_ReadLong();
    SV_ReadLong();
    SV_ReadLong();

    // The closest interval over all contacted Sectors.
    mo->floorz = SV_ReadLong();
    mo->ceilingz = SV_ReadLong();

    // For movement checking.
    mo->radius = SV_ReadLong();
    mo->height = SV_ReadLong();

    // Momentums, used to update position.
    mo->momx = SV_ReadLong();
    mo->momy = SV_ReadLong();
    mo->momz = SV_ReadLong();

    // If == validcount, already checked.
    mo->valid = SV_ReadLong();

    mo->type = SV_ReadLong();
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
    mo->spawninfo.pos[VX] = (fixed_t) (SV_ReadLong() << FRACBITS);
    mo->spawninfo.pos[VY] = (fixed_t) (SV_ReadLong() << FRACBITS);
    mo->spawninfo.pos[VZ] = ONFLOORZ;
    mo->spawninfo.angle = (angle_t) (ANG45 * (SV_ReadLong() / 45));
    mo->spawninfo.type = (int) SV_ReadLong();
    mo->spawninfo.options = (int) SV_ReadLong();

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
    int     i, j;
    fixed_t offx, offy;
    short  *get;
    int     firstflat = W_CheckNumForName("F_START") + 1;

    get = (short *) save_p;

    // do sectors
    for(i = 0; i < numsectors; i++)
    {
        P_SetFixed(DMU_SECTOR, i, DMU_FLOOR_HEIGHT, *get++ << FRACBITS);
        P_SetFixed(DMU_SECTOR, i, DMU_CEILING_HEIGHT, *get++ << FRACBITS);
        P_SetInt(DMU_SECTOR, i, DMU_FLOOR_TEXTURE, *get++ + firstflat);
        P_SetInt(DMU_SECTOR, i, DMU_CEILING_TEXTURE, *get++ + firstflat);
        P_SetInt(DMU_SECTOR, i, DMU_LIGHT_LEVEL, *get++);
        xsectors[i].special = *get++;  // needed?
        xsectors[i].tag = *get++;      // needed?
        xsectors[i].specialdata = 0;
        xsectors[i].soundtarget = 0;
    }

    // do lines
    for(i = 0; i < numlines; i++)
    {
        P_SetInt(DMU_LINE, i, DMU_FLAGS, *get++);
        xlines[i].special = *get++;
        xlines[i].tag = *get++;

        for(j = 0; j < 2; j++)
        {
            side_t* sdef;

            if(j == 0)
                sdef = P_GetPtr(DMU_LINE, i, DMU_SIDE0);
            else
                sdef = P_GetPtr(DMU_LINE, i, DMU_SIDE1);

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

void P_v19_UnArchiveThinkers(void)
{
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
        switch (tclass)
        {
        case tc_end:
            return;             // end of list

        case tc_mobj:
            PADSAVEP();
            mobj = Z_Malloc(sizeof(*mobj), PU_LEVEL, NULL);
            memset(mobj, 0, sizeof(*mobj));

            SV_ReadMobj(mobj);

            mobj->state = &states[(int) mobj->state];
            mobj->target = NULL;
            if(mobj->player)
            {
                int     pnum = (int) mobj->player - 1;

                mobj->player = &players[pnum];
                mobj->dplayer = mobj->player->plr;
                mobj->dplayer->mo = mobj;
                mobj->dplayer->clAngle = mobj->angle;
                mobj->dplayer->clLookDir = 0;
            }
            P_SetThingPosition(mobj);
            mobj->info = &mobjinfo[mobj->type];
            mobj->floorz =
                P_GetFixedp(mobj->subsector, DMU_FLOOR_HEIGHT);
            mobj->ceilingz =
                P_GetFixedp(mobj->subsector, DMU_CEILING_HEIGHT);
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
        switch (tclass)
        {
        case tc_endspecials:
            return;             // end of list

        case tc_ceiling:
            PADSAVEP();
            ceiling = Z_Malloc(sizeof(*ceiling), PU_LEVEL, NULL);
            memcpy(ceiling, save_p, sizeof(*ceiling));
            save_p += sizeof(*ceiling);

            ceiling->sector = P_ToPtr(DMU_SECTOR, (int) ceiling->sector);

            P_XSector(ceiling->sector)->specialdata = ceiling;

            if(ceiling->thinker.function)
                ceiling->thinker.function = T_MoveCeiling;

            P_AddThinker(&ceiling->thinker);
            P_AddActiveCeiling(ceiling);
            break;

        case tc_door:
            PADSAVEP();
            door = Z_Malloc(sizeof(*door), PU_LEVEL, NULL);
            memcpy(door, save_p, sizeof(*door));
            save_p += sizeof(*door);

            door->sector = P_ToPtr(DMU_SECTOR, (int) door->sector);

            P_XSector(door->sector)->specialdata = door;
            door->thinker.function = T_VerticalDoor;
            P_AddThinker(&door->thinker);
            break;

        case tc_floor:
            PADSAVEP();
            floor = Z_Malloc(sizeof(*floor), PU_LEVEL, NULL);
            memcpy(floor, save_p, sizeof(*floor));
            save_p += sizeof(*floor);

            floor->sector = P_ToPtr(DMU_SECTOR, (int) floor->sector);
            P_XSector(floor->sector)->specialdata = floor;
            floor->thinker.function = T_MoveFloor;

            P_AddThinker(&floor->thinker);
            break;

        case tc_plat:
            PADSAVEP();
            plat = Z_Malloc(sizeof(*plat), PU_LEVEL, NULL);
            memcpy(plat, save_p, sizeof(*plat));
            save_p += sizeof(*plat);

            plat->sector = P_ToPtr(DMU_SECTOR, (int) plat->sector);
            P_XSector(plat->sector)->specialdata = plat;

            if(plat->thinker.function)
                plat->thinker.function = T_PlatRaise;

            P_AddThinker(&plat->thinker);
            P_AddActivePlat(plat);

            break;

        case tc_flash:
            PADSAVEP();
            flash = Z_Malloc(sizeof(*flash), PU_LEVEL, NULL);
            memcpy(flash, save_p, sizeof(*flash));
            save_p += sizeof(*flash);

            flash->sector = P_ToPtr(DMU_SECTOR, (int) flash->sector);
            flash->thinker.function = T_LightFlash;

            P_AddThinker(&flash->thinker);
            break;

        case tc_strobe:
            PADSAVEP();
            strobe = Z_Malloc(sizeof(*strobe), PU_LEVEL, NULL);
            memcpy(strobe, save_p, sizeof(*strobe));
            save_p += sizeof(*strobe);

            strobe->sector = P_ToPtr(DMU_SECTOR, (int) strobe->sector);
            strobe->thinker.function = T_StrobeFlash;

            P_AddThinker(&strobe->thinker);
            break;

        case tc_glow:
            PADSAVEP();
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

void SV_v19_LoadGame(char *savename)
{
    int     length;
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
    R_SetupLevel("", DDSLF_AFTER_LOADING);
}
